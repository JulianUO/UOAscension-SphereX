/**
 * @file CUltimaLiveOverlay.cpp
 */

#include "CUltimaLiveOverlay.h"
#include "../../common/CLog.h"
#include "../../common/CServerMap.h"
#include "../../common/sphere_library/CSFileList.h"
#include "../CWorldGameTime.h"
#include "../uo_files/CUOMapMeter.h"
#include <cstdio>
#include <ctime>

void CUltimaLiveOverlay::Clear()
{
	for (int m = 0; m < MAP_SUPPORTED_QTY; ++m)
		m_Blocks[m].clear();
}

UltimaLiveBlockOverlay * CUltimaLiveOverlay::GetOrCreate(int iMap, dword dwBlockId)
{
	if (iMap < 0 || iMap >= MAP_SUPPORTED_QTY)
		return nullptr;
	return &m_Blocks[iMap][dwBlockId];
}

bool CUltimaLiveOverlay::HasLand(int iMap, dword dwBlockId) const
{
	if (iMap < 0 || iMap >= MAP_SUPPORTED_QTY)
		return false;
	const auto it = m_Blocks[iMap].find(dwBlockId);
	return it != m_Blocks[iMap].end() && it->second.fHasLand;
}

bool CUltimaLiveOverlay::HasStatics(int iMap, dword dwBlockId) const
{
	if (iMap < 0 || iMap >= MAP_SUPPORTED_QTY)
		return false;
	const auto it = m_Blocks[iMap].find(dwBlockId);
	return it != m_Blocks[iMap].end() && it->second.fHasStatics;
}

bool CUltimaLiveOverlay::GetLand(int iMap, dword dwBlockId, byte land[192]) const
{
	if (!HasLand(iMap, dwBlockId))
		return false;
	memcpy(land, m_Blocks[iMap].at(dwBlockId).m_land, 192);
	return true;
}

bool CUltimaLiveOverlay::GetStatics(int iMap, dword dwBlockId, std::vector<CUOStaticItemRec> & out) const
{
	if (!HasStatics(iMap, dwBlockId))
		return false;
	out = m_Blocks[iMap].at(dwBlockId).m_statics;
	return true;
}

void CUltimaLiveOverlay::SetLandBlock(int iMap, int iBlockX, int iBlockY, const byte land[192])
{
	if (iMap < 0 || iMap >= MAP_SUPPORTED_QTY)
		return;
	const dword dwBlockId = static_cast<dword>((iBlockX * (g_MapList.GetMapSizeY(iMap) / UO_BLOCK_SIZE)) + iBlockY);
	UltimaLiveBlockOverlay & blk = m_Blocks[iMap][dwBlockId];
	blk.fHasLand = true;
	memcpy(blk.m_land, land, 192);
}

void CUltimaLiveOverlay::SetStaticsBlock(int iMap, int iBlockX, int iBlockY, const std::vector<CUOStaticItemRec> & statics)
{
	if (iMap < 0 || iMap >= MAP_SUPPORTED_QTY)
		return;
	const dword dwBlockId = static_cast<dword>((iBlockX * (g_MapList.GetMapSizeY(iMap) / UO_BLOCK_SIZE)) + iBlockY);
	UltimaLiveBlockOverlay & blk = m_Blocks[iMap][dwBlockId];
	blk.fHasStatics = true;
	blk.m_statics = statics;
}

bool CUltimaLiveOverlay::GetLandTile(int iMap, int x, int y, word & wId, char & z) const
{
	const int bx = x / UO_BLOCK_SIZE;
	const int by = y / UO_BLOCK_SIZE;
	const dword dwBlockId = static_cast<dword>((bx * (g_MapList.GetMapSizeY(iMap) / UO_BLOCK_SIZE)) + by);
	byte land[192];
	if (!GetLand(iMap, dwBlockId, land))
		return false;
	const int idx = ((y & 7) * 8 + (x & 7)) * 3;
	wId = static_cast<word>(land[idx] | (land[idx + 1] << 8));
	z = static_cast<char>(land[idx + 2]);
	return true;
}

bool CUltimaLiveOverlay::SetLandTile(int iMap, int x, int y, word wId, char z)
{
	const int bx = x / UO_BLOCK_SIZE;
	const int by = y / UO_BLOCK_SIZE;
	const dword dwBlockId = static_cast<dword>((bx * (g_MapList.GetMapSizeY(iMap) / UO_BLOCK_SIZE)) + by);

	byte land[192]{};
	if (!GetLand(iMap, dwBlockId, land))
	{
		try
		{
			CServerMapBlock mb(bx, by, iMap);
			const CUOMapBlock * pTer = mb.GetTerrainBlock();
			for (int i = 0; i < 64; ++i)
			{
				land[i * 3] = static_cast<byte>(pTer->m_Meter[i].m_wTerrainIndex & 0xFF);
				land[i * 3 + 1] = static_cast<byte>((pTer->m_Meter[i].m_wTerrainIndex >> 8) & 0xFF);
				land[i * 3 + 2] = static_cast<byte>(pTer->m_Meter[i].m_z);
			}
		}
		catch (...)
		{
			return false;
		}
	}

	const int idx = ((y & 7) * 8 + (x & 7)) * 3;
	land[idx] = static_cast<byte>(wId & 0xFF);
	land[idx + 1] = static_cast<byte>((wId >> 8) & 0xFF);
	land[idx + 2] = static_cast<byte>(z);
	SetLandBlock(iMap, bx, by, land);
	return true;
}

bool CUltimaLiveOverlay::GetStaticsAt(int iMap, int x, int y, std::vector<CUOStaticItemRec> & out) const
{
	const int bx = x / UO_BLOCK_SIZE;
	const int by = y / UO_BLOCK_SIZE;
	const dword dwBlockId = static_cast<dword>((bx * (g_MapList.GetMapSizeY(iMap) / UO_BLOCK_SIZE)) + by);

	std::vector<CUOStaticItemRec> block;
	if (GetStatics(iMap, dwBlockId, block))
	{
		for (const CUOStaticItemRec & st : block)
		{
			if (st.m_x == (x & 7) && st.m_y == (y & 7))
				out.push_back(st);
		}
		return !out.empty();
	}

	try
	{
		CServerMapBlock mb(bx, by, iMap);
		const uint qty = mb.m_Statics.GetStaticQty();
		for (uint i = 0; i < qty; ++i)
		{
			const CUOStaticItemRec * pSt = mb.m_Statics.GetStatic(i);
			if (pSt && pSt->m_x == (x & 7) && pSt->m_y == (y & 7))
				out.push_back(*pSt);
		}
	}
	catch (...)
	{
		return false;
	}
	return !out.empty();
}

bool CUltimaLiveOverlay::SetStaticsAt(int iMap, int x, int y, const std::vector<CUOStaticItemRec> & tiles)
{
	const int bx = x / UO_BLOCK_SIZE;
	const int by = y / UO_BLOCK_SIZE;
	const dword dwBlockId = static_cast<dword>((bx * (g_MapList.GetMapSizeY(iMap) / UO_BLOCK_SIZE)) + by);

	std::vector<CUOStaticItemRec> block;
	if (!GetStatics(iMap, dwBlockId, block))
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

	std::vector<CUOStaticItemRec> kept;
	for (const CUOStaticItemRec & st : block)
	{
		if (st.m_x != (x & 7) || st.m_y != (y & 7))
			kept.push_back(st);
	}
	for (const CUOStaticItemRec & st : tiles)
		kept.push_back(st);

	SetStaticsBlock(iMap, bx, by, kept);
	return true;
}

void CUltimaLiveOverlay::LoadLiveFiles(lpctstr pszDirectory)
{
	// Land: map{N}-*.live
	// Statics: statics{N}-*.live
	// Minimal loader: read most recent files per map index on startup.
	Clear();
	if (!pszDirectory || !*pszDirectory)
		return;

	CSFileList files;
	CSString sPattern = CSFile::GetMergedFileName(pszDirectory, "*.live");
	if (files.ReadDir(sPattern, false) < 0)
		return;

	for (CSStringListRec * psFile = files.GetHead(); psFile; psFile = psFile->GetNext())
	{
		const lpctstr pszName = *psFile;
		if (!pszName)
			continue;

		CSString sLower(pszName);
		sLower.MakeLower();
		if (!strstr(sLower, ".live"))
			continue;

		const bool fLand = (strstr(sLower, "map") != nullptr);
		const bool fStatics = (strstr(sLower, "statics") != nullptr);
		if (!fLand && !fStatics)
			continue;

		CSFile file;
		CSString sFull = CSFile::GetMergedFileName(pszDirectory, pszName);
		if (!file.Open(sFull, OF_READ | OF_BINARY))
			continue;

		const size_t len = file.GetLength();
		if (len < 2)
			continue;

		std::vector<byte> buf(len);
		if (file.Read(buf.data(), static_cast<int>(len)) <= 0)
			continue;

		const int iMap = buf[0] | (buf[1] << 8);
		if (iMap < 0 || iMap >= MAP_SUPPORTED_QTY)
			continue;

		size_t pos = 2;
		if (fLand)
		{
			while (pos + 4 + 192 <= len)
			{
				const int bx = buf[pos] | (buf[pos + 1] << 8);
				const int by = buf[pos + 2] | (buf[pos + 3] << 8);
				pos += 4;
				SetLandBlock(iMap, bx, by, &buf[pos]);
				pos += 192;
			}
		}
		else if (fStatics)
		{
			while (pos + 8 <= len)
			{
				const int bx = buf[pos] | (buf[pos + 1] << 8);
				const int by = buf[pos + 2] | (buf[pos + 3] << 8);
				const int count = static_cast<int>(buf[pos + 4] | (buf[pos + 5] << 8) | (buf[pos + 6] << 16) | (buf[pos + 7] << 24));
				pos += 8;
				if (count < 0 || pos + static_cast<size_t>(count) * 7 > len)
					break;
				std::vector<CUOStaticItemRec> tiles(static_cast<size_t>(count));
				for (int j = 0; j < count; ++j)
				{
					CUOStaticItemRec & st = tiles[static_cast<size_t>(j)];
					st.m_wTileID = static_cast<word>(buf[pos] | (buf[pos + 1] << 8));
					st.m_x = buf[pos + 2];
					st.m_y = buf[pos + 3];
					st.m_z = static_cast<char>(buf[pos + 4]);
					st.m_wHue = static_cast<word>(buf[pos + 5] | (buf[pos + 6] << 8));
					pos += 7;
				}
				SetStaticsBlock(iMap, bx, by, tiles);
			}
		}
	}
}

void CUltimaLiveOverlay::SaveLiveFiles(lpctstr pszDirectory) const
{
	if (!pszDirectory || !*pszDirectory)
		return;

	tchar szStamp[32];
	snprintf(szStamp, sizeof(szStamp), "%" PRIu64, static_cast<uint64>(CWorldGameTime::GetCurrentTime().GetTimeRaw()));

	for (int iMap = 0; iMap < MAP_SUPPORTED_QTY; ++iMap)
	{
		if (m_Blocks[iMap].empty())
			continue;

		// Land save
		{
			CSString sFile;
			sFile.Format("map%d-%s.live", iMap, szStamp);
			CSString sFull = CSFile::GetMergedFileName(pszDirectory, sFile);
			CSFile file;
			if (file.Open(sFull, OF_WRITE | OF_BINARY))
			{
				word wMap = static_cast<word>(iMap);
				file.Write(&wMap, sizeof(wMap));
				for (const auto & kv : m_Blocks[iMap])
				{
					if (!kv.second.fHasLand)
						continue;
					int bx = 0, by = 0;
					const int bh = g_MapList.GetMapSizeY(iMap) / UO_BLOCK_SIZE;
					if (bh > 0)
					{
						bx = static_cast<int>(kv.first / bh);
						by = static_cast<int>(kv.first % bh);
					}
					word wx = static_cast<word>(bx);
					word wy = static_cast<word>(by);
					file.Write(&wx, sizeof(wx));
					file.Write(&wy, sizeof(wy));
					file.Write(kv.second.m_land, 192);
				}
				file.Close();
			}
		}

		// Statics save
		{
			CSString sFile;
			sFile.Format("statics%d-%s.live", iMap, szStamp);
			CSString sFull = CSFile::GetMergedFileName(pszDirectory, sFile);
			CSFile file;
			if (file.Open(sFull, OF_WRITE | OF_BINARY))
			{
				word wMap = static_cast<word>(iMap);
				file.Write(&wMap, sizeof(wMap));
				for (const auto & kv : m_Blocks[iMap])
				{
					if (!kv.second.fHasStatics)
						continue;
					int bx = 0, by = 0;
					const int bh = g_MapList.GetMapSizeY(iMap) / UO_BLOCK_SIZE;
					if (bh > 0)
					{
						bx = static_cast<int>(kv.first / bh);
						by = static_cast<int>(kv.first % bh);
					}
					word wx = static_cast<word>(bx);
					word wy = static_cast<word>(by);
					const int count = static_cast<int>(kv.second.m_statics.size());
					file.Write(&wx, sizeof(wx));
					file.Write(&wy, sizeof(wy));
					file.Write(&count, sizeof(count));
					for (const CUOStaticItemRec & st : kv.second.m_statics)
					{
						file.Write(&st.m_wTileID, sizeof(st.m_wTileID));
						file.Write(&st.m_x, sizeof(st.m_x));
						file.Write(&st.m_y, sizeof(st.m_y));
						file.Write(&st.m_z, sizeof(st.m_z));
						file.Write(&st.m_wHue, sizeof(st.m_wHue));
					}
				}
				file.Close();
			}
		}
	}
}
