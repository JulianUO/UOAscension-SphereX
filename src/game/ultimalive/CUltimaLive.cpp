/**
 * @file CUltimaLive.cpp
 * Portions adapted from SaschaKP/UltimaLive (MIT License).
 */

#include "CUltimaLive.h"
#include "CUltimaLiveOverlay.h"
#include "CUltimaLiveLumber.h"
#include "CUltimaLiveHarvest.h"
#include "CUltimaLiveMining.h"
#include "../../common/CScript.h"
#include "../../common/CServerMap.h"
#include "../../common/CLog.h"
#include "../../common/CUOInstall.h"
#include "../../network/send.h"
#include "../../network/CClientIterator.h"
#include "../CServerConfig.h"
#include "../clients/CClient.h"
#include "../chars/CChar.h"
#include "../CWorldMap.h"
#include "../items/CItemBase.h"
#include <algorithm>
#include "../uo_files/CUOMapBlock.h"
#include <cerrno>
#include <cstring>
#ifdef _WIN32
	#include <direct.h>
	#include <sys/stat.h>
#else
	#include <sys/stat.h>
	#include <sys/types.h>
#endif

const char * CUltimaLive::m_sClassName = "UltimaLive";
CUltimaLive g_UltimaLive;

namespace
{
	constexpr word kCRCInvalid = 0xFFFF;

	bool EnsureDirectoryExists(lpctstr pszPath)
	{
		if (!pszPath || !*pszPath)
			return false;

		tchar szPath[SPHERE_MAX_PATH];
		Str_CopyLimitNull(szPath, pszPath, SPHERE_MAX_PATH);

		size_t len = strlen(szPath);
		while (len > 0 && (szPath[len - 1] == '\\' || szPath[len - 1] == '/'))
			szPath[--len] = '\0';

#ifdef _WIN32
		struct _stat st;
		if (_stat(szPath, &st) == 0 && (st.st_mode & _S_IFDIR))
			return true;
#else
		struct stat st;
		if (stat(szPath, &st) == 0 && S_ISDIR(st.st_mode))
			return true;
#endif

		for (size_t i = 1; i < len; ++i)
		{
			if (szPath[i] != '\\' && szPath[i] != '/')
				continue;

			const tchar ch = szPath[i];
			szPath[i] = '\0';
			if (!EnsureDirectoryExists(szPath))
			{
				szPath[i] = ch;
				return false;
			}
			szPath[i] = ch;
		}

#ifdef _WIN32
		return (_mkdir(szPath) == 0 || errno == EEXIST);
#else
		return (mkdir(szPath, 0755) == 0 || errno == EEXIST);
#endif
	}

	word Fletcher16(const byte * data, size_t len)
	{
		word sum1 = 0;
		word sum2 = 0;
		for (size_t i = 0; i < len; ++i)
		{
			sum1 = static_cast<word>((sum1 + data[i]) % 255);
			sum2 = static_cast<word>((sum2 + sum1) % 255);
		}
		return static_cast<word>((sum2 << 8) | sum1);
	}

	void EncodeLandFromBlock(const CUOMapBlock & ter, byte land[192])
	{
		// Must match ClassicUO map file bytes at block*196+4 (packed CUOMapMeter[64]).
		memcpy(land, ter.m_Meter, 192);
	}

	bool GetRawLandBytesForCrc(int iMap, dword dwBlockId, byte land[192])
	{
		if (!g_MapList.IsMapSupported(iMap))
			return false;

		const int iMapNumber = g_MapList.GetMapFileNum(iMap);
		CSFile * pFile = &(g_Install.m_Maps[iMapNumber]);
		if (!pFile->IsFileOpen())
			return false;

		dword fileOffset = dwBlockId * sizeof(CUOMapBlock);
		if (g_Install.m_IsMapUopFormat[iMapNumber])
		{
			for (int i = 0; i < MAP_SUPPORTED_QTY; ++i)
			{
				const MapAddress & addr = g_Install.m_UopMapAddress[iMapNumber][i];
				if (dwBlockId <= addr.dwLastBlock && dwBlockId >= addr.dwFirstBlock)
				{
					fileOffset = static_cast<dword>(addr.qwAdress + ((dwBlockId - addr.dwFirstBlock) * 196LL));
					break;
				}
			}
		}

		if (static_cast<uint>(pFile->Seek(fileOffset, SEEK_SET)) != fileOffset)
			return false;

		// ClassicUO reads land at block*196+4 (skip 4-byte block header).
		word wHeader[2];
		if (pFile->Read(wHeader, sizeof(wHeader)) <= 0)
			return false;
		if (pFile->Read(land, 192) <= 0)
			return false;
		return true;
	}
	bool GetRawStaticsBytesForCrc(int iMap, dword dwBlockId, std::vector<byte> & out)
	{
		out.clear();
		if (!g_MapList.IsMapSupported(iMap))
			return false;

		CUOIndexRec index;
		if (!g_Install.ReadMulIndex(g_Install.m_Staidx[g_MapList.GetMapFileNum(iMap)], dwBlockId, index))
			return true;

		const uint len = index.GetBlockLength();
		if (len == 0)
			return true;

		out.resize(len);
		if (!g_Install.ReadMulData(g_Install.m_Statics[g_MapList.GetMapFileNum(iMap)], index, out.data()))
			return false;
		return true;
	}

	void EncodeStaticsFromTiles(const std::vector<CUOStaticItemRec> & tiles, std::vector<byte> & out)
	{
		std::vector<CUOStaticItemRec> sorted = tiles;
		std::sort(sorted.begin(), sorted.end(),
			[](const CUOStaticItemRec & a, const CUOStaticItemRec & b)
			{
				if (a.m_z != b.m_z)
					return a.m_z < b.m_z;
				if (a.m_y != b.m_y)
					return a.m_y < b.m_y;
				return a.m_x < b.m_x;
			});

		out.resize(sorted.size() * 7);
		size_t off = 0;
		for (const CUOStaticItemRec & st : sorted)
		{
			out[off + 0] = static_cast<byte>(st.m_wTileID & 0xFF);
			out[off + 1] = static_cast<byte>((st.m_wTileID >> 8) & 0xFF);
			out[off + 2] = st.m_x;
			out[off + 3] = st.m_y;
			out[off + 4] = static_cast<byte>(st.m_z);
			out[off + 5] = static_cast<byte>(st.m_wHue & 0xFF);
			out[off + 6] = static_cast<byte>((st.m_wHue >> 8) & 0xFF);
			off += 7;
		}
	}

	void EncodeStaticsFromBlock(const CServerStaticsBlock & statics, std::vector<byte> & out)
	{
		const uint qty = statics.GetStaticQty();
		out.resize(static_cast<size_t>(qty) * 7);
		size_t off = 0;
		for (uint i = 0; i < qty; ++i)
		{
			const CUOStaticItemRec * pSt = statics.GetStatic(i);
			if (!pSt)
				continue;
			out[off + 0] = static_cast<byte>(pSt->m_wTileID & 0xFF);
			out[off + 1] = static_cast<byte>((pSt->m_wTileID >> 8) & 0xFF);
			out[off + 2] = pSt->m_x;
			out[off + 3] = pSt->m_y;
			out[off + 4] = static_cast<byte>(pSt->m_z);
			out[off + 5] = static_cast<byte>(pSt->m_wHue & 0xFF);
			out[off + 6] = static_cast<byte>((pSt->m_wHue >> 8) & 0xFF);
			off += 7;
		}
		if (off != out.size())
			out.resize(off);
	}
}

struct CUltimaLive::BlockCRCRow
{
	std::vector<word> m_CRC;
};

CUltimaLive & CUltimaLive::Get()
{
	return g_UltimaLive;
}

CUltimaLive::CUltimaLive()
{
	memset(m_apCRC, 0, sizeof(m_apCRC));
	m_pOverlay = new CUltimaLiveOverlay();
	m_pLumber = new CUltimaLiveLumber();
	m_pHarvest = new CUltimaLiveHarvest();
	m_pMining = new CUltimaLiveMining();
}

bool CUltimaLive::LoadKey(CScript & s)
{
	if (s.IsKey("ULTIMALIVEENABLED"))
	{
		m_fEnabled = (s.GetArgVal() != 0);
		if (m_fEnabled)
			ApplyDefaults();
		return true;
	}
	if (s.IsKey("ULTIMALIVESTREAMING"))
	{
		m_fStreaming = (s.GetArgVal() != 0);
		return true;
	}
	if (s.IsKey("ULTIMALIVEDISCOVERY"))
	{
		m_fDiscovery = (s.GetArgVal() != 0);
		return true;
	}
	if (s.IsKey("ULTIMALIVEDISCOVERYVIEWBLOCKS"))
	{
		m_iDiscoveryViewBlocks = s.GetArgVal();
		if (m_iDiscoveryViewBlocks <= 0)
			m_iDiscoveryViewBlocks = 3;
		return true;
	}
	if (s.IsKey("ULTIMALIVEDISCOVERYREVEALONLOGIN"))
	{
		m_fDiscoveryRevealOnLogin = (s.GetArgVal() != 0);
		return true;
	}
	if (s.IsKey("ULTIMALIVEHARVEST"))
	{
		m_fHarvest = (s.GetArgVal() != 0);
		return true;
	}
	if (s.IsKey("ULTIMALIVEHARVESTREGROWTH"))
	{
		m_fHarvestRegrowth = (s.GetArgVal() != 0);
		return true;
	}
	if (s.IsKey("ULTIMALIVEHARVESTREGROWTHMINUTES"))
	{
		m_iHarvestRegrowthMinutes = s.GetArgVal();
		if (m_iHarvestRegrowthMinutes <= 0)
			m_iHarvestRegrowthMinutes = 1440;
		return true;
	}
	if (s.IsKey("ULTIMALIVEMINING"))
	{
		m_fMining = (s.GetArgVal() != 0);
		return true;
	}
	if (s.IsKey("ULTIMALIVEMININGREGROWTH"))
	{
		m_fMiningRegrowth = (s.GetArgVal() != 0);
		return true;
	}
	if (s.IsKey("ULTIMALIVEMININGREGROWTHMINUTES"))
	{
		m_iMiningRegrowthMinutes = s.GetArgVal();
		if (m_iMiningRegrowthMinutes <= 0)
			m_iMiningRegrowthMinutes = 10080;
		return true;
	}
	if (s.IsKey("ULTIMALIVEMININGSTROKES"))
	{
		m_iMiningStrokes = s.GetArgVal();
		if (m_iMiningStrokes <= 0)
			m_iMiningStrokes = 3;
		return true;
	}
	if (s.IsKey("ULTIMALIVESHARDIDENTIFIER"))
	{
		m_sShardIdentifier = s.GetArgStr();
		if (m_sShardIdentifier.GetLength() > 28)
			m_sShardIdentifier.Resize(28);
		return true;
	}
	if (s.IsKey("ULTIMALIVEROOTPATH"))
	{
		m_sRootPath = s.GetArgStr();
		return true;
	}
	if (s.IsKey("ULTIMALIVECLIENTFILESPATH"))
	{
		m_sClientFilesPath = s.GetArgStr();
		return true;
	}
	if (s.IsKey("ULTIMALIVELUMBERHARVESTPATH"))
	{
		m_sLumberHarvestPath = s.GetArgStr();
		return true;
	}
	if (s.IsKey("ULTIMALIVEMININGHARVESTPATH"))
	{
		m_sMiningHarvestPath = s.GetArgStr();
		return true;
	}
	if (s.IsKeyHead("ULTIMALIVEMAP", 13))
	{
		const char * psz = s.GetKey() + 13;
		while (*psz && !IsDigit(*psz))
			++psz;
		const int iMap = atoi(psz);
		if (iMap < 0 || iMap >= MAP_SUPPORTED_QTY)
			return false;

		tchar * ppCmd[6];
		const size_t iCount = Str_ParseCmds(s.GetArgRaw(), ppCmd, ARRAY_COUNT(ppCmd), ",");
		if (iCount < 3)
			return false;

		UltimaLiveMapDef & def = m_aMapDefs[iMap];
		def.iMapIndex = iMap;
		def.iFileIndex = atoi(ppCmd[0]);
		def.uiWidth = static_cast<uint16>(atoi(ppCmd[1]));
		def.uiHeight = static_cast<uint16>(atoi(ppCmd[2]));
		def.uiWrapX = (iCount > 3) ? static_cast<uint16>(atoi(ppCmd[3])) : def.uiWidth;
		def.uiWrapY = (iCount > 4) ? static_cast<uint16>(atoi(ppCmd[4])) : def.uiHeight;
		def.fRegistered = true;
		return true;
	}
	return false;
}

void CUltimaLive::ApplyDefaults()
{
	if (m_sRootPath.IsEmpty())
		m_sRootPath = "ultimalive/";
	if (m_sClientFilesPath.IsEmpty())
		m_sClientFilesPath = "ClientFiles";
	if (m_sLumberHarvestPath.IsEmpty())
		m_sLumberHarvestPath = "LumberHarvest";
	if (m_sMiningHarvestPath.IsEmpty())
		m_sMiningHarvestPath = "MiningHarvest";
	if (m_sShardIdentifier.IsEmpty())
		m_sShardIdentifier = "MyShard";
}

void CUltimaLive::EnsureDataDirectories() const
{
	if (m_sRootPath.IsEmpty())
		return;

	if (!EnsureDirectoryExists(m_sRootPath))
	{
		g_Log.EventWarn("UltimaLive: unable to create root directory '%s'.\n", static_cast<lpctstr>(m_sRootPath));
		return;
	}

	const CSString sClientFiles = GetClientFilesPath();
	const CSString sLumber = GetLumberHarvestPath();
	const CSString sMining = GetMiningHarvestPath();
	if (!EnsureDirectoryExists(sClientFiles))
		g_Log.EventWarn("UltimaLive: unable to create ClientFiles directory '%s'.\n", static_cast<lpctstr>(sClientFiles));
	if (!EnsureDirectoryExists(sLumber))
		g_Log.EventWarn("UltimaLive: unable to create LumberHarvest directory '%s'.\n", static_cast<lpctstr>(sLumber));
	if (!EnsureDirectoryExists(sMining))
		g_Log.EventWarn("UltimaLive: unable to create MiningHarvest directory '%s'.\n", static_cast<lpctstr>(sMining));

	const CSString sDiscovery = GetDiscoveryPath();
	if (!EnsureDirectoryExists(sDiscovery))
		g_Log.EventWarn("UltimaLive: unable to create discovery directory '%s'.\n", static_cast<lpctstr>(sDiscovery));
}

void CUltimaLive::OnStartupCheck()
{
	if (!m_fEnabled)
		return;

	ApplyDefaults();

	int iMaps = 0;
	for (int m = 0; m < MAP_SUPPORTED_QTY; ++m)
	{
		if (m_aMapDefs[m].fRegistered)
			++iMaps;
	}

	g_Log.Event(LOGM_INIT,
		"UltimaLive enabled (shard='%s', root='%s', streaming=%d, discovery=%d, harvest=%d, mining=%d, regrowth=%dm, maps=%d).\n",
		static_cast<lpctstr>(m_sShardIdentifier),
		static_cast<lpctstr>(m_sRootPath),
		m_fStreaming ? 1 : 0,
		m_fDiscovery ? 1 : 0,
		m_fHarvest ? 1 : 0,
		m_fMining ? 1 : 0,
		m_fHarvestRegrowth ? m_iHarvestRegrowthMinutes : 0,
		iMaps);

	if (g_Cfg.m_fUseMapDiffs)
	{
		g_Log.EventWarn("UltimaLive is enabled but UseMapDiffs=1. UltimaLive clients ignore mapdiff packets; consider UseMapDiffs=0.\n");
	}

	if (!g_Install.m_Maps[0].IsFileOpen())
	{
		g_Log.EventWarn("UltimaLive: map0.mul is not open. Set MulFiles= to the same client folder (e.g. C:\\Games\\Ascension UO\\).\n");
	}
	else
	{
		g_Log.Event(LOGM_INIT, "UltimaLive: reading MUL files from '%s'.\n", static_cast<lpctstr>(g_Install.GetPreferPath("")));
	}

	for (int m = 0; m < MAP_SUPPORTED_QTY; ++m)
	{
		if (!g_MapList.IsMapSupported(m))
			continue;
		g_Log.Event(LOGM_INIT, "UltimaLive map %d: %ux%u (file map%d).\n",
			m, g_MapList.GetMapSizeX(m), g_MapList.GetMapSizeY(m), g_MapList.GetMapFileNum(m));
	}
}

void CUltimaLive::OnWorldLoad()
{
	if (!m_fEnabled)
		return;

	ApplyDefaults();
	EnsureDataDirectories();

	for (int m = 0; m < MAP_SUPPORTED_QTY; ++m)
	{
		if (!g_MapList.IsMapSupported(m))
			continue;
		if (!m_aMapDefs[m].fRegistered)
		{
			UltimaLiveMapDef & def = m_aMapDefs[m];
			def.iMapIndex = m;
			def.iFileIndex = g_MapList.GetMapFileNum(m);
			def.uiWidth = g_MapList.GetMapSizeX(m);
			def.uiHeight = g_MapList.GetMapSizeY(m);
			def.uiWrapX = def.uiWidth;
			def.uiWrapY = def.uiHeight;
			def.fRegistered = (def.uiWidth > 0 && def.uiHeight > 0);
		}
		else
		{
			// Width/height sent to clients must match loaded MUL dimensions.
			UltimaLiveMapDef & def = m_aMapDefs[m];
			def.iFileIndex = g_MapList.GetMapFileNum(m);
			def.uiWidth = g_MapList.GetMapSizeX(m);
			def.uiHeight = g_MapList.GetMapSizeY(m);
			if (def.uiWrapX == 0 || def.uiWrapX > def.uiWidth)
				def.uiWrapX = def.uiWidth;
			if (def.uiWrapY == 0 || def.uiWrapY > def.uiHeight)
				def.uiWrapY = def.uiHeight;
		}
		EnsureCRCRow(m);
	}

	m_pOverlay->LoadLiveFiles(GetClientFilesPath());
	if (m_pHarvest)
	{
		m_pHarvest->SetEnabled(m_fHarvest);
		m_pHarvest->SetRegrowthEnabled(m_fHarvestRegrowth);
		m_pHarvest->SetRegrowthMinutes(m_iHarvestRegrowthMinutes);
		m_pHarvest->LoadDefinitions(GetLumberHarvestPath());
		m_pHarvest->LoadRegrowth(GetLumberHarvestPath(), *m_pOverlay, *this);
	}
	else
		m_pLumber->Load(GetLumberHarvestPath(), *m_pOverlay);

	if (m_pMining)
	{
		m_pMining->SetEnabled(m_fMining);
		m_pMining->SetRegrowthEnabled(m_fMiningRegrowth);
		m_pMining->SetRegrowthMinutes(m_iMiningRegrowthMinutes);
		m_pMining->SetRequiredStrokes(m_iMiningStrokes);
		m_pMining->LoadDefinitions(GetMiningHarvestPath());
		m_pMining->LoadRegrowth(GetMiningHarvestPath(), *m_pOverlay, *this);
	}
}

void CUltimaLive::OnWorldSave()
{
	if (!m_fEnabled)
		return;
	EnsureDataDirectories();
	m_pOverlay->SaveLiveFiles(GetClientFilesPath());
	if (m_pHarvest)
	{
		m_pHarvest->ProcessRegrowth(*this);
		m_pHarvest->SaveRegrowth(GetLumberHarvestPath());
	}
	else
		m_pLumber->Save(GetLumberHarvestPath());

	if (m_pMining)
	{
		m_pMining->ProcessRegrowth(*this);
		m_pMining->SaveRegrowth(GetMiningHarvestPath());
	}
}

CSString CUltimaLive::GetClientFilesPath() const
{
	if (m_sClientFilesPath.IsEmpty())
		return CSFile::GetMergedFileName(m_sRootPath, "ClientFiles");
	return CSFile::GetMergedFileName(m_sRootPath, m_sClientFilesPath);
}

CSString CUltimaLive::GetLumberHarvestPath() const
{
	if (m_sLumberHarvestPath.IsEmpty())
		return CSFile::GetMergedFileName(m_sRootPath, "LumberHarvest");
	return CSFile::GetMergedFileName(m_sRootPath, m_sLumberHarvestPath);
}

CSString CUltimaLive::GetMiningHarvestPath() const
{
	if (m_sMiningHarvestPath.IsEmpty())
		return CSFile::GetMergedFileName(m_sRootPath, "MiningHarvest");
	return CSFile::GetMergedFileName(m_sRootPath, m_sMiningHarvestPath);
}

CSString CUltimaLive::GetDiscoveryPath() const
{
	return CSFile::GetMergedFileName(m_sRootPath, "discovery");
}

void CUltimaLive::SendDiscoverySnapshot(CClient * pClient) const
{
	if (!pClient || !m_fDiscovery)
		return;

	const CUltimaLiveDiscovery & disc = pClient->m_UltimaLiveDiscovery;
	if (!disc.IsEnabled())
		return;

	for (int m = 0; m < MAP_SUPPORTED_QTY; ++m)
	{
		if (!IsMapRegistered(m))
			continue;

		std::vector<dword> blocks;
		disc.CollectBlocksForMap(m, blocks);
		if (blocks.empty())
			continue;

		new PacketUltimaLiveDiscoverySnapshot(pClient, static_cast<byte>(m), blocks.data(), blocks.size());
	}
}

const UltimaLiveMapDef * CUltimaLive::GetMapDef(int iMap) const
{
	if (iMap < 0 || iMap >= MAP_SUPPORTED_QTY)
		return nullptr;
	if (!m_aMapDefs[iMap].fRegistered)
		return nullptr;
	return &m_aMapDefs[iMap];
}

bool CUltimaLive::IsMapRegistered(int iMap) const
{
	return GetMapDef(iMap) != nullptr;
}

dword CUltimaLive::GetBlockId(int iMap, int iBlockX, int iBlockY)
{
	if (!g_MapList.IsMapSupported(iMap))
		return 0;
	const int bh = g_MapList.GetMapSizeY(iMap) / UO_BLOCK_SIZE;
	if (bh <= 0)
		return 0;
	return static_cast<dword>((iBlockX * bh) + iBlockY);
}

void CUltimaLive::GetBlockXY(int iMap, dword dwBlockId, int & iBlockX, int & iBlockY)
{
	if (!g_MapList.IsMapSupported(iMap))
	{
		iBlockX = 0;
		iBlockY = 0;
		return;
	}
	const int bh = g_MapList.GetMapSizeY(iMap) / UO_BLOCK_SIZE;
	if (bh <= 0)
	{
		iBlockX = 0;
		iBlockY = 0;
		return;
	}
	iBlockX = static_cast<int>(dwBlockId / bh);
	iBlockY = static_cast<int>(dwBlockId % bh);
}

int CUltimaLive::GetBlockWidthInBlocks(int iMap) const
{
	if (!g_MapList.IsMapSupported(iMap))
		return 0;
	return g_MapList.GetMapSizeX(iMap) / UO_BLOCK_SIZE;
}

int CUltimaLive::GetBlockHeightInBlocks(int iMap) const
{
	if (!g_MapList.IsMapSupported(iMap))
		return 0;
	return g_MapList.GetMapSizeY(iMap) / UO_BLOCK_SIZE;
}

void CUltimaLive::EnsureCRCRow(int iMap)
{
	if (m_apCRC[iMap])
		return;
	const int blocks = GetBlockWidthInBlocks(iMap) * GetBlockHeightInBlocks(iMap);
	if (blocks <= 0)
		return;
	BlockCRCRow * pRow = new BlockCRCRow();
	pRow->m_CRC.assign(static_cast<size_t>(blocks), kCRCInvalid);
	m_apCRC[iMap] = pRow;
}

void CUltimaLive::InvalidateBlockCRC(int iMap, dword dwBlockId)
{
	EnsureCRCRow(iMap);
	if (!m_apCRC[iMap])
		return;
	if (dwBlockId < m_apCRC[iMap]->m_CRC.size())
		m_apCRC[iMap]->m_CRC[dwBlockId] = kCRCInvalid;
}

bool CUltimaLive::GetLandData(int iMap, int iBlockX, int iBlockY, byte land[192])
{
	const dword dwBlockId = GetBlockId(iMap, iBlockX, iBlockY);
	if (m_pOverlay->GetLand(iMap, dwBlockId, land))
		return true;

	if (GetRawLandBytesForCrc(iMap, dwBlockId, land))
		return true;

	try
	{
		CServerMapBlock mb(iBlockX, iBlockY, iMap);
		EncodeLandFromBlock(*mb.GetTerrainBlock(), land);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool CUltimaLive::GetStaticsData(int iMap, int iBlockX, int iBlockY, std::vector<byte> & staticsOut)
{
	const dword dwBlockId = GetBlockId(iMap, iBlockX, iBlockY);
	std::vector<CUOStaticItemRec> tiles;
	if (m_pOverlay->GetStatics(iMap, dwBlockId, tiles))
	{
		EncodeStaticsFromTiles(tiles, staticsOut);
		return true;
	}

	try
	{
		CServerMapBlock mb(iBlockX, iBlockY, iMap);
		EncodeStaticsFromBlock(mb.m_Statics, staticsOut);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

word CUltimaLive::GetBlockCRC(int iMap, dword dwBlockId, std::vector<byte> * pLandOut, std::vector<byte> * pStaticsOut)
{
	EnsureCRCRow(iMap);
	if (m_apCRC[iMap] && dwBlockId < m_apCRC[iMap]->m_CRC.size())
	{
		const word cached = m_apCRC[iMap]->m_CRC[dwBlockId];
		if (cached != kCRCInvalid)
			return cached;
	}

	int bx = 0, by = 0;
	GetBlockXY(iMap, dwBlockId, bx, by);

	byte land[192];
	if (m_pOverlay->HasLand(iMap, dwBlockId))
	{
		if (!GetLandData(iMap, bx, by, land))
			return 0;
	}
	else if (!GetRawLandBytesForCrc(iMap, dwBlockId, land))
	{
		return 0;
	}

	std::vector<byte> statics;
	if (m_pOverlay->HasStatics(iMap, dwBlockId))
	{
		GetStaticsData(iMap, bx, by, statics);
	}
	else if (!GetRawStaticsBytesForCrc(iMap, dwBlockId, statics))
	{
		return 0;
	}

	std::vector<byte> combined;
	combined.resize(192 + statics.size());
	memcpy(combined.data(), land, 192);
	if (!statics.empty())
		memcpy(combined.data() + 192, statics.data(), statics.size());

	const word crc = Fletcher16(combined.data(), combined.size());
	if (m_apCRC[iMap] && dwBlockId < m_apCRC[iMap]->m_CRC.size())
		m_apCRC[iMap]->m_CRC[dwBlockId] = crc;

	if (pLandOut)
	{
		pLandOut->assign(land, land + 192);
	}
	if (pStaticsOut)
		*pStaticsOut = statics;

	return crc;
}

void CUltimaLive::OnServerList(CClient * pClient) const
{
	if (!m_fEnabled || !pClient)
		return;
	new PacketUltimaLiveLoginComplete(pClient);
	new PacketUltimaLiveMapDefinitions(pClient);
}

void CUltimaLive::OnPlayerStart(CClient * pClient)
{
	if (!m_fEnabled || !pClient)
		return;

	pClient->m_UltimaLiveDiscovery.Clear();
	pClient->m_UltimaLiveDiscovery.SetEnabled(m_fDiscovery);
	pClient->m_UltimaLiveDiscovery.SetViewBlocks(m_iDiscoveryViewBlocks);
	pClient->m_UltimaLiveDiscovery.SetRevealOnLogin(m_fDiscoveryRevealOnLogin);

	CChar * pChar = pClient->GetChar();
	if (m_fDiscovery && pChar)
		pClient->m_UltimaLiveDiscovery.LoadForChar(pChar->GetUID(), GetDiscoveryPath());

	pClient->m_fUltimaLiveClient = true;
	new PacketUltimaLiveLoginComplete(pClient);
	new PacketUltimaLiveMapDefinitions(pClient);

	g_Log.Event(LOGM_CLIENTS_LOG, "UltimaLive: login handshake for '%s' (discovery=%d, harvest=%d, streaming=%d)\n",
		static_cast<lpctstr>(GetShardIdentifier()),
		IsDiscoveryEnabled() ? 1 : 0,
		m_fHarvest ? 1 : 0,
		m_fStreaming ? 1 : 0);

	if (m_fDiscovery && m_fDiscoveryRevealOnLogin)
		SendDiscoverySnapshot(pClient);

	if (m_fStreaming && pChar)
	{
		const CPointMap pt = pChar->GetTopPoint();
		const dword dwBlock = GetBlockId(pt.m_map, pt.m_x / UO_BLOCK_SIZE, pt.m_y / UO_BLOCK_SIZE);
		new PacketUltimaLiveQueryHash(pClient, dwBlock, static_cast<byte>(pt.m_map));
	}
}

void CUltimaLive::QueryMobile(CChar * pChar, bool fForce)
{
	if (!IsStreaming() || !pChar || !pChar->m_pPlayer)
		return;

	CClient * pClient = pChar->GetClientActive();
	if (!pClient || !pClient->m_fUltimaLiveClient)
		return;

	const CPointMap pt = pChar->GetTopPoint();
	const dword dwBlock = GetBlockId(pt.m_map, pt.m_x / UO_BLOCK_SIZE, pt.m_y / UO_BLOCK_SIZE);

	if (!fForce && pClient->m_iUltimaLivePrevBlock == static_cast<int>(dwBlock) && pClient->m_iUltimaLivePrevMap == pt.m_map)
		return;

	pClient->m_iUltimaLivePrevBlock = static_cast<int>(dwBlock);
	pClient->m_iUltimaLivePrevMap = pt.m_map;
	new PacketUltimaLiveQueryHash(pClient, dwBlock, static_cast<byte>(pt.m_map));
}

void CUltimaLive::OnCharMove(CChar * pChar)
{
	QueryMobile(pChar, false);
}

void CUltimaLive::OnCharMapChange(CChar * pChar)
{
	if (!pChar)
		return;
	CClient * pClient = pChar->GetClientActive();
	if (pClient)
	{
		pClient->m_iUltimaLivePrevBlock = -1;
		pClient->m_iUltimaLivePrevMap = -1;
	}
	QueryMobile(pChar, true);
	if (IsStreaming() && pClient)
		new PacketUltimaLiveRefreshView(pClient);
}

void CUltimaLive::OnClientDisconnect(CClient * pClient)
{
	if (!pClient)
		return;

	if (m_fDiscovery)
	{
		CChar * pChar = pClient->GetChar();
		if (pChar)
			pClient->m_UltimaLiveDiscovery.SaveForChar(pChar->GetUID(), GetDiscoveryPath());
	}

	pClient->m_iUltimaLivePrevBlock = -1;
	pClient->m_iUltimaLivePrevMap = -1;
	pClient->m_wUltimaLiveMajor = 0;
	pClient->m_wUltimaLiveMinor = 0;
	pClient->m_UltimaLiveDiscovery.Clear();
}

void CUltimaLive::OnUltimaLiveVersion(CClient * pClient, word wMajor, word wMinor)
{
	if (!pClient)
		return;
	pClient->m_wUltimaLiveMajor = wMajor;
	pClient->m_wUltimaLiveMinor = wMinor;
	pClient->m_fUltimaLiveClient = true;
}

void CUltimaLive::OnBlockQueryReply(CClient * pClient, dword dwCenterBlock, int iMapID, const word * pCRCs, size_t iCRCCount)
{
	if (!IsStreaming() || !pClient || !pCRCs || iCRCCount < 25)
		return;

	const CChar * pChar = pClient->GetChar();
	if (!pChar || pChar->GetTopMap() != iMapID)
		return;

	PushBlockUpdates(pClient, dwCenterBlock, iMapID, pCRCs);
}

void CUltimaLive::PushBlockUpdates(CClient * pClient, dword dwCenterBlock, int iMapID, const word * pReceivedCRCs)
{
	const UltimaLiveMapDef * pDef = GetMapDef(iMapID);
	if (!pDef)
		return;

	const CChar * pChar = pClient ? pClient->GetChar() : nullptr;
	const CPointMap ptChar = pChar ? pChar->GetTopPoint() : CPointMap();
	CUltimaLiveDiscovery & disc = pClient->m_UltimaLiveDiscovery;
	const bool fDiscovery = disc.IsEnabled();

	const int mapWidthBlocks = GetBlockWidthInBlocks(iMapID);
	const int mapHeightBlocks = GetBlockHeightInBlocks(iMapID);
	const int wrapWidthBlocks = static_cast<int>(pDef->uiWrapX / UO_BLOCK_SIZE);
	const int wrapHeightBlocks = static_cast<int>(pDef->uiWrapY / UO_BLOCK_SIZE);

	if (mapWidthBlocks <= 0 || mapHeightBlocks <= 0)
		return;

	int blockX = 0, blockY = 0;
	GetBlockXY(iMapID, dwCenterBlock, blockX, blockY);

	std::vector<dword> newlyRevealed;
	if (fDiscovery)
		newlyRevealed.reserve(25);

	for (int x = -2; x <= 2; ++x)
	{
		const int xModWidth = (blockX < wrapWidthBlocks) ? wrapWidthBlocks : mapWidthBlocks;
		int xBlockItr = (blockX + x) % xModWidth;
		if (xBlockItr < 0 && xBlockItr > -3)
			xBlockItr += xModWidth;

		for (int y = -2; y <= 2; ++y)
		{
			const int yModHeight = (blockY < wrapHeightBlocks) ? wrapHeightBlocks : mapHeightBlocks;
			int yBlockItr = (blockY + y) % yModHeight;
			if (yBlockItr < 0)
				yBlockItr += yModHeight;

			const dword blocknum = GetBlockId(iMapID, xBlockItr, yBlockItr);
			const int crcIndex = ((x + 2) * 5) + (y + 2);

			const bool fInRange = !fDiscovery || disc.IsBlockInRevealRange(iMapID, xBlockItr, yBlockItr, ptChar);
			const bool fDiscovered = !fDiscovery || disc.IsDiscovered(iMapID, blocknum);

			if (fDiscovery && fInRange && !fDiscovered)
			{
				disc.MarkDiscovered(iMapID, blocknum);
				newlyRevealed.push_back(blocknum);
			}

			std::vector<byte> land;
			std::vector<byte> statics;
			const word crc = GetBlockCRC(iMapID, blocknum, &land, &statics);

			if (crc == pReceivedCRCs[crcIndex])
				continue;



			byte landBuf[192];
			if (land.size() < 192)
			{
				GetLandData(iMapID, xBlockItr, yBlockItr, landBuf);
				new PacketUltimaLiveTerrain(pClient, landBuf, blocknum, static_cast<byte>(iMapID));
			}
			else
				new PacketUltimaLiveTerrain(pClient, land.data(), blocknum, static_cast<byte>(iMapID));

			std::vector<byte> wireStatics;
			if (m_pOverlay->HasStatics(iMapID, blocknum))
			{
				GetStaticsData(iMapID, xBlockItr, yBlockItr, wireStatics);
			}
			else if (GetRawStaticsBytesForCrc(iMapID, blocknum, wireStatics))
			{
				// use raw MUL bytes for wire format (matches ClassicUO CRC)
			}
			else
			{
				wireStatics.clear();
			}

			if (!wireStatics.empty())
				new PacketUltimaLiveStatics(pClient, wireStatics.data(), static_cast<uint>(wireStatics.size()), blocknum, static_cast<byte>(iMapID));
			else if (m_pOverlay->HasStatics(iMapID, blocknum))
				new PacketUltimaLiveStatics(pClient, nullptr, 0, blocknum, static_cast<byte>(iMapID));
		}
	}

	if (fDiscovery && !newlyRevealed.empty())
		new PacketUltimaLiveDiscoveryBlock(pClient, static_cast<byte>(iMapID), newlyRevealed.data(), newlyRevealed.size());
}

void CUltimaLive::PushBlockToClient(CClient * pClient, int iMap, int iBlockX, int iBlockY)
{
	if (!pClient)
		return;

	const dword blocknum = GetBlockId(iMap, iBlockX, iBlockY);

	// Harvest edits statics only; skip terrain to avoid ClassicUO reloading the chunk
	// with stale static indices before the statics packet is applied.
	if (m_pOverlay->HasLand(iMap, blocknum))
	{
		byte land[192];
		if (GetLandData(iMap, iBlockX, iBlockY, land))
			new PacketUltimaLiveTerrain(pClient, land, blocknum, static_cast<byte>(iMap));
	}

	std::vector<byte> wireStatics;
	if (m_pOverlay->HasStatics(iMap, blocknum))
		GetStaticsData(iMap, iBlockX, iBlockY, wireStatics);
	else
		GetRawStaticsBytesForCrc(iMap, blocknum, wireStatics);

	if (wireStatics.empty() && m_pOverlay->HasStatics(iMap, blocknum))
		new PacketUltimaLiveStatics(pClient, nullptr, 0, blocknum, static_cast<byte>(iMap));
	else if (!wireStatics.empty())
		new PacketUltimaLiveStatics(pClient, wireStatics.data(), static_cast<uint>(wireStatics.size()), blocknum, static_cast<byte>(iMap));
}

void CUltimaLive::NotifyBlockChange(int iMap, int iBlockX, int iBlockY, CChar * pOrigin)
{
	const int cx = iBlockX * UO_BLOCK_SIZE + 4;
	const int cy = iBlockY * UO_BLOCK_SIZE + 4;

	ClientIterator it;
	for (CClient * pClient = it.next(); pClient != nullptr; pClient = it.next())
	{
		CChar * pChar = pClient->GetChar();
		if (!pChar || !pChar->m_pPlayer || !pClient->m_fUltimaLiveClient)
			continue;
		if (pChar->GetTopMap() != iMap)
			continue;

		const CPointMap pt = pChar->GetTopPoint();
		if (abs(pt.m_x - cx) > 40 || abs(pt.m_y - cy) > 40)
			continue;

		CUltimaLiveDiscovery & disc = pClient->m_UltimaLiveDiscovery;
		const dword blocknum = GetBlockId(iMap, iBlockX, iBlockY);
		if (disc.IsEnabled() && !disc.IsDiscovered(iMap, blocknum))
		{
			if (!disc.IsBlockInRevealRange(iMap, iBlockX, iBlockY, pt))
				continue;
			disc.MarkDiscovered(iMap, blocknum);
			new PacketUltimaLiveDiscoveryBlock(pClient, static_cast<byte>(iMap), &blocknum, 1);
		}

		PushBlockToClient(pClient, iMap, iBlockX, iBlockY);
	}

	if (pOrigin && IsStreaming())
		QueryMobile(pOrigin, true);
}

void CUltimaLive::SendLocalUpdates(int iMap, int iBlockX, int iBlockY, UltimaLiveLocalUpdate flags)
{
	(void)flags;
	NotifyBlockChange(iMap, iBlockX, iBlockY, nullptr);
}

bool CUltimaLive::RemoveStaticFromBlock(int iMap, int x, int y, word wTileId, char z)
{
	const int bx = x / UO_BLOCK_SIZE;
	const int by = y / UO_BLOCK_SIZE;
	const dword dwBlockId = GetBlockId(iMap, bx, by);
	const byte lx = static_cast<byte>(x & 7);
	const byte ly = static_cast<byte>(y & 7);

	std::vector<CUOStaticItemRec> block;
	if (!m_pOverlay->GetStatics(iMap, dwBlockId, block))
	{
		try
		{
			CServerMapBlock mb(bx, by, iMap);
			const uint qty = mb.m_Statics.GetStaticQty();
			for (uint i = 0; i < qty; ++i)
			{
				const CUOStaticItemRec * pSt = mb.m_Statics.GetStatic(i);
				if (pSt)
					block.push_back(*pSt);
			}
		}
		catch (...)
		{
			return false;
		}
	}

	const size_t before = block.size();
	std::vector<CUOStaticItemRec> kept;
	kept.reserve(block.size());
	for (const CUOStaticItemRec & st : block)
	{
		if (st.m_x == lx && st.m_y == ly && (st.m_wTileID & 0x3FFF) == (wTileId & 0x3FFF) && st.m_z == z)
			continue;
		kept.push_back(st);
	}
	if (kept.size() == before)
		return false;

	m_pOverlay->SetStaticsBlock(iMap, bx, by, kept);
	return true;
}

bool CUltimaLive::SetLandTile(int iMap, int x, int y, word wTerrainId, char z)
{
	if (!m_pOverlay->SetLandTile(iMap, x, y, wTerrainId, z))
		return false;
	const dword bid = GetBlockId(iMap, x / UO_BLOCK_SIZE, y / UO_BLOCK_SIZE);
	InvalidateBlockCRC(iMap, bid);
	SendLocalUpdates(iMap, x / UO_BLOCK_SIZE, y / UO_BLOCK_SIZE, ULTIMALIVE_UPDATE_TERRAIN);
	return true;
}

bool CUltimaLive::SetStaticAt(int iMap, int x, int y, const CUOStaticItemRec & tile, bool fAdd)
{
	std::vector<CUOStaticItemRec> tiles;
	if (fAdd)
	{
		m_pOverlay->GetStaticsAt(iMap, x, y, tiles);
		tiles.push_back(tile);
	}
	else
	{
		tiles.push_back(tile);
	}
	if (!m_pOverlay->SetStaticsAt(iMap, x, y, tiles))
		return false;
	InvalidateBlockCRC(iMap, GetBlockId(iMap, x / UO_BLOCK_SIZE, y / UO_BLOCK_SIZE));
	if (!m_fSuppressBlockNotify)
		SendLocalUpdates(iMap, x / UO_BLOCK_SIZE, y / UO_BLOCK_SIZE, ULTIMALIVE_UPDATE_STATICS);
	return true;
}

bool CUltimaLive::DeleteStaticAt(int iMap, int x, int y, word wTileId, char z)
{
	if (!RemoveStaticFromBlock(iMap, x, y, wTileId, z))
		return false;

	InvalidateBlockCRC(iMap, GetBlockId(iMap, x / UO_BLOCK_SIZE, y / UO_BLOCK_SIZE));
	if (!m_fSuppressBlockNotify)
		NotifyBlockChange(iMap, x / UO_BLOCK_SIZE, y / UO_BLOCK_SIZE, nullptr);
	return true;
}

void CUltimaLive::OnTreeHarvest(CChar * pChar, int x, int y, int z)
{
	if (!m_fEnabled || !pChar || !m_pHarvest || !m_fHarvest)
		return;
	m_pHarvest->HarvestTree(*this, pChar, x, y, z);
}

bool CUltimaLive::ResolveLumberjackResourcePoint(const CPointMap & ptChop, CPointMap & ptResource) const
{
	if (!m_fEnabled || !m_pHarvest || !m_fHarvest)
		return false;
	return m_pHarvest->ResolveLumberjackResourcePoint(const_cast<CUltimaLive &>(*this), ptChop, ptResource);
}

bool CUltimaLive::CanGraphicHarvestAt(const CPointMap & ptChop) const
{
	if (!m_fEnabled || !m_pHarvest || !m_fHarvest)
		return false;
	return m_pHarvest->CanGraphicHarvestAt(const_cast<CUltimaLive &>(*this), ptChop);
}

bool CUltimaLive::TryGraphicHarvest(CChar * pChar, int chopX, int chopY)
{
	if (!m_fEnabled || !pChar || !m_pHarvest || !m_fHarvest)
		return false;
	return m_pHarvest->TryGraphicHarvestAt(*this, pChar, chopX, chopY);
}

bool CUltimaLive::IsHarvestGraphicAt(const CPointMap & pt, ITEMID_TYPE id) const
{
	if (!m_fEnabled || !m_pHarvest || !m_fHarvest)
		return false;
	return m_pHarvest->IsHarvestGraphicAt(const_cast<CUltimaLive &>(*this), pt, id);
}

void CUltimaLive::ApplyDeferredTreeHarvest(CChar * pChar, int x, int y, int z, bool fGraphicOnly)
{
	if (!m_fEnabled || !pChar)
		return;

	if (fGraphicOnly)
	{
		TryGraphicHarvest(pChar, x, y);
		return;
	}

	if (m_pHarvest && m_fHarvest)
	{
		OnTreeHarvest(pChar, x, y, z);
		m_pHarvest->TryRemoveStumpAfterChop(*this, pChar, x, y);
	}
	else
		OnLumberjackSuccess(pChar, x, y, z);
}

void CUltimaLive::DeferTreeHarvest(CChar * pChar, int x, int y, int z, bool fGraphicOnly)
{
	if (!m_fEnabled || !pChar)
		return;

	DeferredTreeHarvest entry;
	entry.uidChar = pChar->GetUID();
	entry.x = static_cast<short>(x);
	entry.y = static_cast<short>(y);
	entry.z = static_cast<char>(z);
	entry.iMap = static_cast<uchar>(pChar->GetTopMap());
	entry.fGraphicOnly = fGraphicOnly;
	m_DeferredHarvests.push_back(entry);
}

void CUltimaLive::FlushDeferredTreeHarvest(CChar * pChar)
{
	if (!pChar || m_DeferredHarvests.empty())
		return;

	const CUID uid = pChar->GetUID();
	for (size_t i = 0; i < m_DeferredHarvests.size(); )
	{
		const DeferredTreeHarvest & entry = m_DeferredHarvests[i];
		if (entry.uidChar != uid)
		{
			++i;
			continue;
		}
		ApplyDeferredTreeHarvest(pChar, entry.x, entry.y, entry.z, entry.fGraphicOnly);
		m_DeferredHarvests.erase(m_DeferredHarvests.begin() + static_cast<std::ptrdiff_t>(i));
	}
}

void CUltimaLive::OnLumberjackSuccess(CChar * pChar, int x, int y, int z)
{
	if (!m_fEnabled || !pChar)
		return;

	if (m_pHarvest && m_fHarvest)
	{
		OnTreeHarvest(pChar, x, y, z);
		return;
	}

	const int iMap = pChar->GetTopMap();
	CPointMap pt(static_cast<short>(x), static_cast<short>(y), static_cast<char>(z), static_cast<uchar>(iMap));
	const CPointMap ptTree = CWorldMap::FindItemTypeNearby(pt, IT_TREE, 2, false, false);
	if (!ptTree.IsValidPoint())
	{
		g_Log.EventWarn("UltimaLive: lumberjack - no IT_TREE near %d,%d,%d on map %d\n", x, y, z, iMap);
		return;
	}

	std::vector<CUOStaticItemRec> at;
	if (!m_pOverlay->GetStaticsAt(iMap, ptTree.m_x, ptTree.m_y, at))
	{
		g_Log.EventWarn("UltimaLive: lumberjack - no map statics at tree tile %d,%d on map %d\n", ptTree.m_x, ptTree.m_y, iMap);
		return;
	}

	for (const CUOStaticItemRec & st : at)
	{
		const CItemBase * pDef = CItemBase::FindItemBase(st.GetDispID());
		if (pDef && !pDef->IsType(IT_TREE))
			continue;

		if (!RemoveStaticFromBlock(iMap, ptTree.m_x, ptTree.m_y, st.m_wTileID, st.m_z))
		{
			g_Log.EventWarn("UltimaLive: lumberjack - failed to remove static 0x%x at %d,%d z=%d\n",
				st.m_wTileID, ptTree.m_x, ptTree.m_y, static_cast<int>(st.m_z));
			continue;
		}

		m_pLumber->RecordTreeFelled(iMap, ptTree.m_x, ptTree.m_y, st.m_z, st.m_wTileID);

		const int bx = ptTree.m_x / UO_BLOCK_SIZE;
		const int by = ptTree.m_y / UO_BLOCK_SIZE;
		InvalidateBlockCRC(iMap, GetBlockId(iMap, bx, by));
		NotifyBlockChange(iMap, bx, by, pChar);

		g_Log.Event(LOGM_CLIENTS_LOG, "UltimaLive: felled tree 0x%x at %d,%d,%d map %d\n",
			st.m_wTileID, ptTree.m_x, ptTree.m_y, static_cast<int>(st.m_z), iMap);
		return;
	}

	g_Log.EventWarn("UltimaLive: lumberjack - statics at %d,%d are not IT_TREE on map %d\n", ptTree.m_x, ptTree.m_y, iMap);
}

bool CUltimaLive::ParseStaffCommand(CClient * pClient, lpctstr pszCommand)
{
	if (!m_fEnabled || !pClient || !pszCommand || !*pszCommand)
		return false;
	if (pClient->GetPrivLevel() < PLEVEL_GM)
		return false;

	CChar * pChar = pClient->GetChar();
	if (!pChar)
		return false;

	tchar szCmd[MAX_TALK_BUFFER];
	Str_CopyLimitNull(szCmd, pszCommand, sizeof(szCmd));

	tchar * ppCmd[4];
	const size_t iQty = Str_ParseCmds(szCmd, ppCmd, ARRAY_COUNT(ppCmd), " ");
	if (iQty < 1)
		return false;

	const CPointMap pt = pChar->GetTopPoint();
	const int iMap = pt.m_map;

	if (!strnicmp(ppCmd[0], "updateblock", 11))
	{
		pClient->m_iUltimaLivePrevBlock = -1;
		QueryMobile(pChar, true);
		pClient->SysMessage("UltimaLive: block update requested.");
		return true;
	}
	if (!strnicmp(ppCmd[0], "queryclienthash", 15))
	{
		QueryMobile(pChar, true);
		pClient->SysMessage("UltimaLive: client hash query sent.");
		return true;
	}
	if (!strnicmp(ppCmd[0], "getblocknumber", 14))
	{
		const dword bid = GetBlockId(iMap, pt.m_x / UO_BLOCK_SIZE, pt.m_y / UO_BLOCK_SIZE);
		pClient->SysMessagef("UltimaLive block: %u (map %d)", bid, iMap);
		return true;
	}
	if (!strnicmp(ppCmd[0], "addstatic", 9) && iQty >= 2)
	{
		CUOStaticItemRec st{};
		st.m_wTileID = static_cast<word>(atoi(ppCmd[1]));
		st.m_x = static_cast<byte>(pt.m_x & 7);
		st.m_y = static_cast<byte>(pt.m_y & 7);
		st.m_z = static_cast<char>(pt.m_z);
		if (iQty >= 3)
			st.m_wHue = static_cast<word>(atoi(ppCmd[2]));
		SetStaticAt(iMap, pt.m_x, pt.m_y, st, true);
		pClient->SysMessage("UltimaLive: static added.");
		return true;
	}
	if (!strnicmp(ppCmd[0], "delstatic", 9))
	{
		word id = (iQty >= 2) ? static_cast<word>(atoi(ppCmd[1])) : 0;
		char z = (iQty >= 3) ? static_cast<char>(atoi(ppCmd[2])) : static_cast<char>(pt.m_z);
		std::vector<CUOStaticItemRec> at;
		if (m_pOverlay->GetStaticsAt(iMap, pt.m_x, pt.m_y, at))
		{
			for (const CUOStaticItemRec & st : at)
			{
				if ((id == 0 || st.m_wTileID == id) && (iQty < 3 || st.m_z == z))
					DeleteStaticAt(iMap, pt.m_x, pt.m_y, st.m_wTileID, st.m_z);
			}
		}
		pClient->SysMessage("UltimaLive: static deleted.");
		return true;
	}
	if (!strnicmp(ppCmd[0], "setlandid", 9) && iQty >= 2)
	{
		SetLandTile(iMap, pt.m_x, pt.m_y, static_cast<word>(atoi(ppCmd[1])), static_cast<char>(pt.m_z));
		pClient->SysMessage("UltimaLive: land id set.");
		return true;
	}
	if (!strnicmp(ppCmd[0], "setlandalt", 10) && iQty >= 2)
	{
		word wId = 0;
		char z = static_cast<char>(atoi(ppCmd[1]));
		m_pOverlay->GetLandTile(iMap, pt.m_x, pt.m_y, wId, z);
		SetLandTile(iMap, pt.m_x, pt.m_y, wId, z);
		pClient->SysMessage("UltimaLive: land altitude set.");
		return true;
	}
	if (!strnicmp(ppCmd[0], "inclandalt", 10) && iQty >= 2)
	{
		word wId = 0;
		char z = 0;
		m_pOverlay->GetLandTile(iMap, pt.m_x, pt.m_y, wId, z);
		z = static_cast<char>(z + atoi(ppCmd[1]));
		SetLandTile(iMap, pt.m_x, pt.m_y, wId, z);
		pClient->SysMessage("UltimaLive: land altitude adjusted.");
		return true;
	}
	if (!strnicmp(ppCmd[0], "refreshclientview", 17))
	{
		new PacketUltimaLiveRefreshView(pClient);
		pClient->SysMessage("UltimaLive: refresh view sent.");
		return true;
	}
	if (!strnicmp(ppCmd[0], "addtunnel", 9))
	{
		if (TryMiningHarvest(pChar, pt.m_x, pt.m_y, pt.m_z))
			pClient->SysMessage("UltimaLive: tunnel excavated at location.");
		else
			pClient->SysMessage("UltimaLive: no minable wall/rock at location.");
		return true;
	}
	if (!strnicmp(ppCmd[0], "deltunnel", 9))
	{
		CUOStaticItemRec st{};
		st.m_wTileID = 0x0224; // Default cave wall
		st.m_x = static_cast<byte>(pt.m_x & 7);
		st.m_y = static_cast<byte>(pt.m_y & 7);
		st.m_z = static_cast<char>(pt.m_z);
		SetStaticAt(iMap, pt.m_x, pt.m_y, st, true);
		pClient->SysMessage("UltimaLive: tunnel wall placed.");
		return true;
	}

	return false;
}

bool CUltimaLive::CanMiningExcavateAt(const CPointMap & ptMine) const
{
	if (!m_fEnabled || !m_fMining || !m_pMining)
		return false;
	return m_pMining->CanMiningExcavateAt(const_cast<CUltimaLive &>(*this), ptMine);
}

bool CUltimaLive::TryMiningHarvest(CChar * pChar, int mineX, int mineY, int mineZ)
{
	if (!m_fEnabled || !m_fMining || !m_pMining || !pChar)
		return false;
	return m_pMining->TryMiningHarvest(*this, pChar, mineX, mineY, mineZ);
}

bool CUltimaLive::IsMiningGraphicAt(const CPointMap & pt, ITEMID_TYPE id) const
{
	if (!m_fEnabled || !m_fMining || !m_pMining)
		return false;
	return m_pMining->IsMiningGraphicAt(const_cast<CUltimaLive &>(*this), pt, id);
}

