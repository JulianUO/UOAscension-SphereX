/**
 * @file CUltimaLiveMining.cpp
 */

#include "CUltimaLiveMining.h"
#include "CUltimaLive.h"
#include "CUltimaLiveOverlay.h"
#include "../../common/CLog.h"
#include "../../common/CScript.h"
#include "../../common/CServerMap.h"
#include "../../common/sphere_library/CSFile.h"
#include "../../common/sphere_library/CSFileList.h"
#include "../CWorldGameTime.h"
#include "../chars/CChar.h"
#include "../items/CItem.h"
#include "../items/CItemBase.h"
#include "../CWorldMap.h"
#include "../CWorldSearch.h"
#include "../uo_files/CUOMapList.h"
#include <algorithm>
#include <cstdio>
#include <unordered_set>

namespace
{
	word NormalizeTileId(word wTileId)
	{
		return static_cast<word>(wTileId & 0x3FFF);
	}

	dword MakeTileKey(int iMap, int x, int y)
	{
		return (static_cast<dword>(iMap & 0xFF) << 24) |
		       (static_cast<dword>(x & 0xFFF) << 12) |
		       static_cast<dword>(y & 0xFFF);
	}
}

void CUltimaLiveMining::Clear()
{
	m_Templates.clear();
	m_TileToTemplate.clear();
	m_StrokeCounts.clear();
	m_Regrowth.clear();
}

void CUltimaLiveMining::RebuildTileLookup()
{
	m_TileToTemplate.clear();
	for (size_t i = 0; i < m_Templates.size(); ++i)
	{
		const MiningWallTemplate & tmpl = m_Templates[i];
		m_TileToTemplate[tmpl.wWallTileId] = i;
		for (const MiningGraphicDef & g : tmpl.walls)
			m_TileToTemplate[g.wTileId] = i;
		for (const MiningGraphicDef & g : tmpl.rubble)
			m_TileToTemplate[g.wTileId] = i;
	}
}

void CUltimaLiveMining::RegisterBuiltinWalls()
{
	auto addWall = [this](word wWallId, word wFloorId = 0x0244, word wRubbleId = 0x1363)
	{
		MiningWallTemplate tmpl;
		tmpl.wWallTileId = wWallId;
		tmpl.wFloorTileId = wFloorId;
		tmpl.wRubbleTileId = wRubbleId;

		MiningGraphicDef g;
		g.wTileId = wWallId;
		g.iDx = 0;
		g.iDy = 0;
		tmpl.walls.push_back(g);

		MiningGraphicDef r;
		r.wTileId = wRubbleId;
		r.iDx = 0;
		r.iDy = 0;
		tmpl.rubble.push_back(r);

		m_Templates.push_back(tmpl);
	};

	// Standard UO cave wall statics
	for (word id = 0x0220; id <= 0x028F; ++id)
		addWall(id);

	// Dungeon rock / cave entrance statics
	for (word id = 0x0320; id <= 0x037F; ++id)
		addWall(id);

	// Standard UO mountain rocks / rock face statics
	for (word id = 0x08C0; id <= 0x08DF; ++id)
		addWall(id);
	for (word id = 0x0D80; id <= 0x0DAF; ++id)
		addWall(id);
	for (word id = 0x0E50; id <= 0x0E6F; ++id)
		addWall(id);
	for (word id = 0x1360; id <= 0x137F; ++id)
		addWall(id);
	for (word id = 0x1770; id <= 0x1785; ++id)
		addWall(id);
	for (word id = 0x1B00; id <= 0x1B1F; ++id)
		addWall(id);
	for (word id = 0x3200; id <= 0x32FF; ++id)
		addWall(id);
}

void CUltimaLiveMining::LoadTunnelsIni(lpctstr pszDirectory)
{
	if (!pszDirectory || !*pszDirectory)
		return;

	CSString sPath = CSFile::GetMergedFileName(pszDirectory, "tunnels.ini");
	CSFile file;
	if (!file.Open(sPath, OF_READ | OF_TEXT))
		return;

	// Load custom tunnel definitions from tunnels.ini if present
	// Format: [WALL id] FLOOR=0x244 RUBBLE=0x1363
	CScript s;
	if (!s.Open(sPath))
		return;

	while (s.FindNextSection())
	{
		if (!s.IsSectionType("WALL") && !s.IsSectionType("TUNNEL"))
			continue;

		lpctstr pszArg = s.GetArgStr();
		word wWallId = static_cast<word>(Exp_GetVal(pszArg));
		if (!wWallId)
			continue;

		word wFloorId = 0x0244;
		word wRubbleId = 0x1363;

		while (s.ReadKeyParse())
		{
			if (s.IsKey("FLOOR"))
			{
				lpctstr pFloor = s.GetArgStr();
				wFloorId = static_cast<word>(Exp_GetVal(pFloor));
			}
			else if (s.IsKey("RUBBLE"))
			{
				lpctstr pRubble = s.GetArgStr();
				wRubbleId = static_cast<word>(Exp_GetVal(pRubble));
			}
		}

		MiningWallTemplate tmpl;
		tmpl.wWallTileId = wWallId;
		tmpl.wFloorTileId = wFloorId;
		tmpl.wRubbleTileId = wRubbleId;

		MiningGraphicDef g;
		g.wTileId = wWallId;
		tmpl.walls.push_back(g);

		MiningGraphicDef r;
		r.wTileId = wRubbleId;
		tmpl.rubble.push_back(r);

		m_Templates.push_back(tmpl);
	}
}

void CUltimaLiveMining::LoadDefinitions(lpctstr pszDirectory)
{
	Clear();
	RegisterBuiltinWalls();
	LoadTunnelsIni(pszDirectory);
	RebuildTileLookup();
}

const MiningWallTemplate * CUltimaLiveMining::LookupTemplate(word wTileId) const
{
	const word norm = NormalizeTileId(wTileId);
	auto it = m_TileToTemplate.find(norm);
	if (it == m_TileToTemplate.end() || it->second >= m_Templates.size())
		return nullptr;
	return &m_Templates[it->second];
}

void CUltimaLiveMining::CollectStaticsAt(CUltimaLive & live, int iMap, int x, int y, std::vector<CUOStaticItemRec> & out) const
{
	out.clear();
	if (iMap < 0 || iMap >= MAP_SUPPORTED_QTY || !g_MapList.IsMapSupported(iMap))
		return;

	const int bx = x / UO_BLOCK_SIZE;
	const int by = y / UO_BLOCK_SIZE;
	const dword dwBlockId = static_cast<dword>((bx * (g_MapList.GetMapSizeY(iMap) / UO_BLOCK_SIZE)) + by);

	if (live.m_pOverlay && live.m_pOverlay->HasStatics(iMap, dwBlockId))
	{
		live.m_pOverlay->GetStaticsAt(iMap, x, y, out);
		return;
	}

	const CPointMap pt(static_cast<short>(x), static_cast<short>(y), 0, static_cast<uchar>(iMap));
	const CServerMapBlock * pBlock = CWorldMap::GetMapBlock(pt);
	if (!pBlock)
		return;

	const int lx = x & 7;
	const int ly = y & 7;
	const uint qty = pBlock->m_Statics.GetStaticQty();
	for (uint i = 0; i < qty; ++i)
	{
		if (!pBlock->m_Statics.IsStaticPoint(i, lx, ly))
			continue;
		const CUOStaticItemRec * pSt = pBlock->m_Statics.GetStatic(i);
		if (pSt)
			out.push_back(*pSt);
	}
}

static inline bool IsCaveFloorEdgeTile(word id)
{
	id &= 0x3FFF;
	return (id >= 0x0540 && id <= 0x054F);
}

static inline bool IsCaveFloorTile(word id)
{
	id &= 0x3FFF;
	return (id >= 0x053B && id <= 0x054F) || (id >= 0x0551 && id <= 0x0553) || (id == 0x056A);
}

bool CUltimaLiveMining::CanMiningExcavateAt(CUltimaLive & live, const CPointMap & ptMine) const
{
	if (!m_fEnabled || !live.IsEnabled())
		return false;

	std::vector<CUOStaticItemRec> statics;
	CollectStaticsAt(live, ptMine.m_map, ptMine.m_x, ptMine.m_y, statics);
	for (const CUOStaticItemRec & s : statics)
	{
		if (IsCaveFloorEdgeTile(s.m_wTileID))
			return true;
		if (!IsCaveFloorTile(s.m_wTileID) && LookupTemplate(s.m_wTileID))
			return true;
	}

	const CUOMapMeter * pMeter = CWorldMap::GetMapMeter(ptMine);
	if (pMeter && CWorldMap::GetTerrainItemType(pMeter->m_wTerrainIndex) == IT_ROCK)
		return true;

	return false;
}

bool CUltimaLiveMining::IsMiningGraphicAt(CUltimaLive & live, const CPointMap & pt, ITEMID_TYPE id) const
{
	if (!m_fEnabled || !live.IsEnabled())
		return false;

	const word norm = NormalizeTileId(static_cast<word>(id));
	if (IsCaveFloorEdgeTile(norm))
		return true;
	if (IsCaveFloorTile(norm))
		return false;
	if (!LookupTemplate(norm))
		return false;

	std::vector<CUOStaticItemRec> statics;
	CollectStaticsAt(live, pt.m_map, pt.m_x, pt.m_y, statics);
	for (const CUOStaticItemRec & s : statics)
	{
		if (NormalizeTileId(s.m_wTileID) == norm)
			return true;
	}
	return false;
}

void CUltimaLiveMining::ApplyCaveExpansionTile(CUltimaLive & live, int iMap, int x, int y, word wTileToAdd, char z) const
{
	std::vector<CUOStaticItemRec> statics;
	CollectStaticsAt(live, iMap, x, y, statics);

	bool hasFull = false;
	bool hasH1 = false; // 0x0541 (S)
	bool hasH2 = false; // 0x054a (W)
	bool hasH3 = false; // 0x054d (N)
	bool hasH4 = false; // 0x0544 (E)
	bool hasQ1 = false; // 0x0540 (SE)
	bool hasQ2 = false; // 0x0547 (SW)
	bool hasQ3 = false; // 0x0548 (NE)
	bool hasQ4 = false; // 0x0549 (NW)

	char tileZ = z;
	for (const auto & s : statics)
	{
		if (IsCaveFloorTile(s.m_wTileID))
		{
			tileZ = s.m_z;
			const word id = s.m_wTileID & 0x3FFF;
			if (id >= 0x053B && id <= 0x053F) hasFull = true;
			else if (id >= 0x0551 && id <= 0x0553) hasFull = true;
			else if (id == 0x056A) hasFull = true;
			else if (id >= 0x0541 && id <= 0x0543) hasH1 = true;
			else if (id >= 0x054A && id <= 0x054C) hasH2 = true;
			else if (id >= 0x054D && id <= 0x054F) hasH3 = true;
			else if (id >= 0x0544 && id <= 0x0546) hasH4 = true;
			else if (id == 0x0540) hasQ1 = true;
			else if (id == 0x0547) hasQ2 = true;
			else if (id == 0x0548) hasQ3 = true;
			else if (id == 0x0549) hasQ4 = true;
		}
	}

	const word addId = wTileToAdd & 0x3FFF;
	if (addId >= 0x053B && addId <= 0x053F) hasFull = true;
	else if (addId >= 0x0551 && addId <= 0x0553) hasFull = true;
	else if (addId == 0x056A) hasFull = true;
	else if (addId >= 0x0541 && addId <= 0x0543) hasH1 = true;
	else if (addId >= 0x054A && addId <= 0x054C) hasH2 = true;
	else if (addId >= 0x054D && addId <= 0x054F) hasH3 = true;
	else if (addId >= 0x0544 && addId <= 0x0546) hasH4 = true;
	else if (addId == 0x0540) hasQ1 = true;
	else if (addId == 0x0547) hasQ2 = true;
	else if (addId == 0x0548) hasQ3 = true;
	else if (addId == 0x0549) hasQ4 = true;

	// Merge quarters into halves
	if (hasQ1 && hasQ2) { hasH1 = true; hasQ1 = false; hasQ2 = false; }
	if (hasQ1 && hasQ3) { hasH4 = true; hasQ1 = false; hasQ3 = false; }
	if (hasQ2 && hasQ4) { hasH2 = true; hasQ2 = false; hasQ4 = false; }
	if (hasQ3 && hasQ4) { hasH3 = true; hasQ3 = false; hasQ4 = false; }

	// Subsume quarters under halves
	if (hasH1) { hasQ1 = false; hasQ2 = false; }
	if (hasH2) { hasQ2 = false; hasQ4 = false; }
	if (hasH3) { hasQ3 = false; hasQ4 = false; }
	if (hasH4) { hasQ1 = false; hasQ3 = false; }

	// Merge opposite halves into full
	if ((hasH1 && hasH3) || (hasH2 && hasH4)) { hasFull = true; }

	// Delete existing cave floor statics at this (x, y)
	for (const auto & s : statics)
	{
		if (IsCaveFloorTile(s.m_wTileID))
		{
			live.DeleteStaticAt(iMap, x, y, s.m_wTileID, s.m_z);
		}
	}

	std::vector<word> toPlace;
	if (hasFull)
	{
		toPlace.push_back(0x053B);
	}
	else
	{
		if (hasH1) toPlace.push_back(0x0541);
		if (hasH2) toPlace.push_back(0x054A);
		if (hasH3) toPlace.push_back(0x054D);
		if (hasH4) toPlace.push_back(0x0544);
		if (hasQ1) toPlace.push_back(0x0540);
		if (hasQ2) toPlace.push_back(0x0547);
		if (hasQ3) toPlace.push_back(0x0548);
		if (hasQ4) toPlace.push_back(0x0549);
	}

	const byte lx = static_cast<byte>(x & 7);
	const byte ly = static_cast<byte>(y & 7);

	for (word tileId : toPlace)
	{
		CUOStaticItemRec rec;
		rec.m_x = lx;
		rec.m_y = ly;
		rec.m_z = tileZ;
		rec.m_wTileID = tileId;
		rec.m_wHue = 0;
		live.SetStaticAt(iMap, x, y, rec, true);
	}
}

bool CUltimaLiveMining::TryMiningHarvest(CUltimaLive & live, CChar * pChar, int mineX, int mineY, int mineZ)
{
	if (!m_fEnabled || !live.IsEnabled() || !pChar)
		return false;

	const int iMap = pChar->GetTopMap();
	std::vector<CUOStaticItemRec> statics;
	CollectStaticsAt(live, iMap, mineX, mineY, statics);

	const CUOStaticItemRec * pTargetStatic = nullptr;
	bool fHasFloorEdge = false;

	for (const CUOStaticItemRec & s : statics)
	{
		if (IsCaveFloorEdgeTile(s.m_wTileID))
		{
			fHasFloorEdge = true;
		}
		else if (!IsCaveFloorTile(s.m_wTileID))
		{
			const MiningWallTemplate * t = LookupTemplate(s.m_wTileID);
			if (t && !pTargetStatic)
			{
				pTargetStatic = &s;
			}
		}
	}

	const CPointMap ptMine(static_cast<short>(mineX), static_cast<short>(mineY), static_cast<char>(mineZ), static_cast<uchar>(iMap));
	const CUOMapMeter * pMeter = CWorldMap::GetMapMeter(ptMine);
	const bool fIsRockTerrain = (pMeter && CWorldMap::GetTerrainItemType(pMeter->m_wTerrainIndex) == IT_ROCK);

	if (!pTargetStatic && !fIsRockTerrain && !fHasFloorEdge)
		return false;

	const dword dwKey = MakeTileKey(iMap, mineX, mineY);
	int & iStrokes = m_StrokeCounts[dwKey];
	iStrokes++;

	if (iStrokes < m_iRequiredStrokes)
	{
		pChar->SysMessage("Picas la roca para abrir paso en la cueva...");
		return true;
	}

	// Reached required strokes: excavate
	m_StrokeCounts.erase(dwKey);

	const word wWallTileId = pTargetStatic ? pTargetStatic->m_wTileID : 0;
	const char z = pChar->GetTopPoint().m_z;

	// If there was a wall static (NOT floor) at the mined point, delete it
	if (pTargetStatic)
	{
		live.DeleteStaticAt(iMap, mineX, mineY, pTargetStatic->m_wTileID, pTargetStatic->m_z);
	}

	// 3x3 surrounding tiles (MinableTunnels matrix):
	ApplyCaveExpansionTile(live, iMap, mineX,     mineY,     0x053B, z); // Center Full
	ApplyCaveExpansionTile(live, iMap, mineX - 1, mineY - 1, 0x0549, z); // NW
	ApplyCaveExpansionTile(live, iMap, mineX,     mineY - 1, 0x054D, z); // N
	ApplyCaveExpansionTile(live, iMap, mineX + 1, mineY - 1, 0x0548, z); // NE
	ApplyCaveExpansionTile(live, iMap, mineX + 1, mineY,     0x0544, z); // E
	ApplyCaveExpansionTile(live, iMap, mineX + 1, mineY + 1, 0x0540, z); // SE
	ApplyCaveExpansionTile(live, iMap, mineX,     mineY + 1, 0x0541, z); // S
	ApplyCaveExpansionTile(live, iMap, mineX - 1, mineY + 1, 0x0547, z); // SW
	ApplyCaveExpansionTile(live, iMap, mineX - 1, mineY,     0x054A, z); // W

	// Record for slow regrowth if it was a wall
	if (wWallTileId)
		RecordRegrowth(iMap, static_cast<short>(mineX), static_cast<short>(mineY), z, wWallTileId, 0x053B);

	// Notify all affected blocks to clients
	std::unordered_set<dword> affectedBlocks;
	for (int dx = -1; dx <= 1; ++dx)
	{
		for (int dy = -1; dy <= 1; ++dy)
		{
			const int bx = (mineX + dx) / UO_BLOCK_SIZE;
			const int by = (mineY + dy) / UO_BLOCK_SIZE;
			affectedBlocks.insert(CUltimaLive::GetBlockId(iMap, bx, by));
		}
	}
	for (dword bId : affectedBlocks)
	{
		int bx, by;
		CUltimaLive::GetBlockXY(iMap, bId, bx, by);
		live.NotifyBlockChange(iMap, bx, by, pChar);
	}

	pChar->SysMessage("¡Excavas la roca y abres camino en la cueva!");
	return true;
}

bool CUltimaLiveMining::IsRegrowthLocation(int iMap, short ox, short oy, char oz) const
{
	auto it = m_Regrowth.find(iMap);
	if (it == m_Regrowth.end())
		return false;
	for (const MiningRegrowthEntry & entry : it->second)
	{
		if (entry.iX == ox && entry.iY == oy && abs(entry.iZ - oz) <= 5)
			return true;
	}
	return false;
}

void CUltimaLiveMining::RecordRegrowth(int iMap, short ox, short oy, char oz, word wOriginalWallTileId, word wFloorTileId)
{
	RemoveRegrowthAt(iMap, ox, oy, oz);
	MiningRegrowthEntry entry;
	entry.iMap = iMap;
	entry.iX = ox;
	entry.iY = oy;
	entry.iZ = oz;
	entry.wOriginalWallTileId = wOriginalWallTileId;
	entry.wFloorTileId = wFloorTileId;
	entry.iTimeExcavated = CWorldGameTime::GetCurrentTime().GetTimeRaw();
	m_Regrowth[iMap].push_back(entry);
}

void CUltimaLiveMining::RemoveRegrowthAt(int iMap, short ox, short oy, char oz)
{
	auto it = m_Regrowth.find(iMap);
	if (it == m_Regrowth.end())
		return;
	auto & vec = it->second;
	vec.erase(std::remove_if(vec.begin(), vec.end(), [ox, oy, oz](const MiningRegrowthEntry & e)
	{
		return e.iX == ox && e.iY == oy && abs(e.iZ - oz) <= 5;
	}), vec.end());
}

bool CUltimaLiveMining::RestoreWall(CUltimaLive & live, MiningRegrowthEntry & entry)
{
	live.DeleteStaticAt(entry.iMap, entry.iX, entry.iY, 0x0540, entry.iZ);
	live.DeleteStaticAt(entry.iMap, entry.iX, entry.iY, 0x0541, entry.iZ);
	live.DeleteStaticAt(entry.iMap, entry.iX, entry.iY, 0x0542, entry.iZ);
	live.DeleteStaticAt(entry.iMap, entry.iX, entry.iY, 0x0543, entry.iZ);
	live.DeleteStaticAt(entry.iMap, entry.iX, entry.iY, 0x054b, entry.iZ);

	CUOStaticItemRec rec;
	rec.m_x = static_cast<byte>(entry.iX & 7);
	rec.m_y = static_cast<byte>(entry.iY & 7);
	rec.m_z = entry.iZ;
	rec.m_wHue = 0;
	rec.m_wTileID = entry.wOriginalWallTileId;

	live.SetStaticAt(entry.iMap, entry.iX, entry.iY, rec, true);

	const int blockX = entry.iX >> 3;
	const int blockY = entry.iY >> 3;
	live.NotifyBlockChange(entry.iMap, blockX, blockY, nullptr);
	return true;
}

void CUltimaLiveMining::ProcessRegrowth(CUltimaLive & live)
{
	if (!m_fEnabled || !m_fRegrowth || !live.IsEnabled())
		return;

	const int64 now = CWorldGameTime::GetCurrentTime().GetTimeRaw();
	const int64 thresholdSec = static_cast<int64>(m_iRegrowthMinutes) * 60;

	for (auto & kv : m_Regrowth)
	{
		std::vector<MiningRegrowthEntry> toRestore;
		for (const MiningRegrowthEntry & entry : kv.second)
		{
			if ((now - entry.iTimeExcavated) >= thresholdSec)
				toRestore.push_back(entry);
		}

		for (MiningRegrowthEntry & entry : toRestore)
		{
			RestoreWall(live, entry);
			RemoveRegrowthAt(kv.first, entry.iX, entry.iY, entry.iZ);
		}
	}

	m_iLastRegrowthPass = now;
}

void CUltimaLiveMining::LoadRegrowth(lpctstr pszDirectory, CUltimaLiveOverlay & overlay, CUltimaLive & live)
{
	(void)overlay;
	m_Regrowth.clear();
	if (!pszDirectory || !*pszDirectory)
		return;

	for (int m = 0; m < MAP_SUPPORTED_QTY; ++m)
	{
		if (!g_MapList.IsMapSupported(m))
			continue;

		CSString sFile;
		sFile.Format("TunnelLocations.%d", m);
		CSString sFull = CSFile::GetMergedFileName(pszDirectory, sFile);

		CSFile file;
		if (!file.Open(sFull, OF_READ | OF_BINARY))
			continue;

		dword magic = 0;
		if (file.Read(&magic, sizeof(magic)) <= 0 || magic != kTunnelLocMagic)
			continue;

		word version = 0;
		dword count = 0;
		if (file.Read(&version, sizeof(version)) <= 0 || file.Read(&count, sizeof(count)) <= 0)
			continue;

		for (dword i = 0; i < count; ++i)
		{
			MiningRegrowthEntry entry;
			entry.iMap = m;
			if (file.Read(&entry.iX, sizeof(entry.iX)) <= 0)
				break;
			if (file.Read(&entry.iY, sizeof(entry.iY)) <= 0)
				break;
			if (file.Read(&entry.iZ, sizeof(entry.iZ)) <= 0)
				break;
			if (file.Read(&entry.wOriginalWallTileId, sizeof(entry.wOriginalWallTileId)) <= 0)
				break;
			if (file.Read(&entry.wFloorTileId, sizeof(entry.wFloorTileId)) <= 0)
				break;
			if (file.Read(&entry.iTimeExcavated, sizeof(entry.iTimeExcavated)) <= 0)
				entry.iTimeExcavated = CWorldGameTime::GetCurrentTime().GetTimeRaw();

			m_Regrowth[m].push_back(entry);
			// Apply excavated state to live map overlay
			live.DeleteStaticAt(m, entry.iX, entry.iY, entry.wOriginalWallTileId, entry.iZ);
			if (entry.wFloorTileId)
				live.SetLandTile(m, entry.iX, entry.iY, entry.wFloorTileId, entry.iZ);
		}
	}
}

void CUltimaLiveMining::SaveRegrowth(lpctstr pszDirectory) const
{
	if (!pszDirectory || !*pszDirectory)
		return;

	for (const auto & kv : m_Regrowth)
	{
		if (kv.second.empty())
			continue;

		CSString sFile;
		sFile.Format("TunnelLocations.%d", kv.first);
		CSString sFull = CSFile::GetMergedFileName(pszDirectory, sFile);

		CSFile file;
		if (!file.Open(sFull, OF_WRITE | OF_BINARY))
			continue;

		const dword magic = kTunnelLocMagic;
		const word version = kTunnelLocVersion;
		const dword count = static_cast<dword>(kv.second.size());

		file.Write(&magic, sizeof(magic));
		file.Write(&version, sizeof(version));
		file.Write(&count, sizeof(count));

		for (const MiningRegrowthEntry & entry : kv.second)
		{
			file.Write(&entry.iX, sizeof(entry.iX));
			file.Write(&entry.iY, sizeof(entry.iY));
			file.Write(&entry.iZ, sizeof(entry.iZ));
			file.Write(&entry.wOriginalWallTileId, sizeof(entry.wOriginalWallTileId));
			file.Write(&entry.wFloorTileId, sizeof(entry.wFloorTileId));
			file.Write(&entry.iTimeExcavated, sizeof(entry.iTimeExcavated));
		}
	}
}
