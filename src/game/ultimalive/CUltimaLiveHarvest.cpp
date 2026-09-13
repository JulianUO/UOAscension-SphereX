/**
 * @file CUltimaLiveHarvest.cpp
 */

#include "CUltimaLiveHarvest.h"
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

	CItem * FindLumberResourceGem(const CPointMap & pt)
	{
		auto Area = CWorldSearchHolder::GetInstance(pt);
		for (;;)
		{
			CItem * pItem = Area->GetItem();
			if (!pItem)
				return nullptr;
			if (pItem->IsType(IT_TREE) && pItem->GetID() == ITEMID_WorldGem)
				return pItem;
		}
	}

	void ReplenishFallenResourceAtTrunk(int iMap, short ox, short oy, char oz, const HarvestTreeTemplate & tmpl)
	{
		CPointMap pt(ox, oy, oz, static_cast<uchar>(iMap));
		CItem * pGem = FindLumberResourceGem(pt);
		if (!pGem || tmpl.fallen.empty())
			return;
		const word wWant = static_cast<word>(tmpl.fallen.size());
		if (pGem->GetAmount() < wWant)
			pGem->SetAmount(wWant);
	}
}

namespace
{
	void AddGraphic(std::vector<HarvestGraphicDef> & list, word wTileId, char dx, char dy)
	{
		list.emplace_back(wTileId, dx, dy);
	}

	void AddFallenEastWest(std::vector<HarvestGraphicDef> & fallen)
	{
		AddGraphic(fallen, 0xCF5, -3, 0);
		AddGraphic(fallen, 0xCF6, -2, 0);
		AddGraphic(fallen, 0xCF7, -1, 0);
	}

	bool ParseGraphicLine(lpctstr pszLine, HarvestGraphicDef & out)
	{
		if (!pszLine || !*pszLine)
			return false;

		tchar szCopy[128];
		Str_CopyLimitNull(szCopy, pszLine, sizeof(szCopy));

		tchar * ppTok[3];
		if (Str_ParseCmds(szCopy, ppTok, ARRAY_COUNT(ppTok), ",") < 1)
			return false;

		out.wTileId = static_cast<word>(strtoul(ppTok[0], nullptr, 0));
		out.iDx = (ppTok[1] && *ppTok[1]) ? static_cast<char>(atoi(ppTok[1])) : 0;
		out.iDy = (ppTok[2] && *ppTok[2]) ? static_cast<char>(atoi(ppTok[2])) : 0;
		return out.wTileId != 0;
	}
}

void CUltimaLiveHarvest::Clear()
{
	m_Templates.clear();
	m_TileToTemplate.clear();
	m_Regrowth.clear();
}

void CUltimaLiveHarvest::RebuildTileLookup()
{
	m_TileToTemplate.clear();
	for (size_t i = 0; i < m_Templates.size(); ++i)
	{
		const HarvestTreeTemplate & tmpl = m_Templates[i];
		for (const HarvestGraphicDef & g : tmpl.mature)
			m_TileToTemplate[g.wTileId] = i;
		for (const HarvestGraphicDef & g : tmpl.fallen)
			m_TileToTemplate[g.wTileId] = i;
		for (const HarvestGraphicDef & g : tmpl.sapling)
			m_TileToTemplate[g.wTileId] = i;
		if (tmpl.wStumpId)
			m_TileToTemplate[tmpl.wStumpId] = i;
	}
}

void CUltimaLiveHarvest::RegisterBuiltinTrees()
{
	// Stock UO tree graphics from UltimaLive HarvestableTrees.cs (CUSTOM_TREE_GRAPHICS off).
	auto addStockTree = [this](word wTrunk, std::initializer_list<word> leaves, word wStump = 0xE57)
	{
		HarvestTreeTemplate tmpl;
		tmpl.wMatureTrunkId = wTrunk;
		tmpl.wStumpId = wStump;
		AddGraphic(tmpl.mature, wTrunk, 0, 0);
		for (word wLeaf : leaves)
			AddGraphic(tmpl.mature, wLeaf, 0, 0);
		AddFallenEastWest(tmpl.fallen);
		AddGraphic(tmpl.sapling, 0x0CEA, 0, 0);
		m_Templates.push_back(tmpl);
	};

	// Leafless
	addStockTree(0x0CCA, {});
	addStockTree(0x0CCB, {});
	addStockTree(0x0CCC, {});
	addStockTree(0x0C9E, {}); // Ohii

	// Generic / forest stock trees (trunk + leaf variants on same tile)
	addStockTree(0x0CCD, { 0x0CCE, 0x0CCF });
	addStockTree(0x0CD0, { 0x0CD1, 0x0CD2 });
	addStockTree(0x0CD3, { 0x0CD4, 0x0CD5 });
	addStockTree(0x0CD6, { 0x0CD7 });
	addStockTree(0x0CD8, { 0x0CD9 });
	addStockTree(0x0CDA, { 0x0CDB, 0x0CDC });
	addStockTree(0x0CDD, { 0x0CDE, 0x0CDF });
	addStockTree(0x0CE0, { 0x0CE1, 0x0CE2 }); // Walnut
	addStockTree(0x0CE3, { 0x0CE4, 0x0CE5 }); // Walnut2
	addStockTree(0x0CE6, { 0x0CE7, 0x0CE8 }); // Willow
	addStockTree(0x0CF8, { 0x0CF9, 0x0CFA }); // Cypress
	addStockTree(0x0CFB, { 0x0CFC, 0x0CFD }); // Cypress2
	addStockTree(0x0CFE, { 0x0CFF, 0x0D00 }); // Cypress3
	addStockTree(0x0D01, { 0x0D02, 0x0D03 }); // Cypress4

	// Yucca (custom stump)
	addStockTree(0x0D37, {}, 0x0DAC);
	addStockTree(0x0D38, {}, 0x0DAC);

	// Tuscany pine (leafless)
	addStockTree(0x1B7E, {}); // 7038

	// Fruit trees
	addStockTree(0x0D94, { 0x0D95, 0x0D96, 0x0D97 }); // Apple 3476
	addStockTree(0x0D98, { 0x0D99, 0x0D9A, 0x0D9B }); // Apple2 3480
	addStockTree(0x0D9C, { 0x0D9D, 0x0D9E, 0x0D9F }); // Peach 3484
	addStockTree(0x0DA0, { 0x0DA1, 0x0DA2, 0x0DA3 }); // Peach2 3488
	addStockTree(0x0DA4, { 0x0DA5, 0x0DA6, 0x0DA7 }); // Pear 3492
	addStockTree(0x0DA8, { 0x0DA9, 0x0DAA, 0x0DAB }); // Pear2 3496

	RebuildTileLookup();
	g_Log.Event(LOGM_INIT, "UltimaLive: registered %u stock tree templates.\n", static_cast<uint>(m_Templates.size()));
}

void CUltimaLiveHarvest::LoadTreesIni(lpctstr pszDirectory)
{
	if (!pszDirectory || !*pszDirectory)
		return;

	const CSString sFile = CSFile::GetMergedFileName(pszDirectory, "trees.ini");
	CScript s;
	if (!s.Open(sFile, OF_READ | OF_TEXT))
		return;

	while (s.FindNextSection())
	{
		lpctstr pszName = s.GetKey();
		if (!pszName || strnicmp(pszName, "TREE_", 5))
			continue;

		const word wTrunk = static_cast<word>(strtoul(pszName + 5, nullptr, 0));
		if (!wTrunk)
			continue;

		HarvestTreeTemplate * pTmpl = nullptr;
		for (HarvestTreeTemplate & tmpl : m_Templates)
		{
			if (tmpl.wMatureTrunkId == wTrunk)
			{
				pTmpl = &tmpl;
				break;
			}
		}
		if (!pTmpl)
		{
			HarvestTreeTemplate tmpl;
			tmpl.wMatureTrunkId = wTrunk;
			m_Templates.push_back(tmpl);
			pTmpl = &m_Templates.back();
		}

		while (s.ReadKey(false))
		{
			if (s.IsKeyHead("[", 1))
				break;

			if (s.IsKeyHead("STUMP", 5))
			{
				pTmpl->wStumpId = static_cast<word>(s.GetArgVal());
			}
			else if (s.IsKeyHead("MATURE", 6))
			{
				HarvestGraphicDef g;
				if (ParseGraphicLine(s.GetArgStr(), g))
					pTmpl->mature.push_back(g);
			}
			else if (s.IsKeyHead("FALLEN", 6))
			{
				HarvestGraphicDef g;
				if (ParseGraphicLine(s.GetArgStr(), g))
					pTmpl->fallen.push_back(g);
			}
			else if (s.IsKeyHead("SAPLING", 7))
			{
				HarvestGraphicDef g;
				if (ParseGraphicLine(s.GetArgStr(), g))
					pTmpl->sapling.push_back(g);
			}
		}
	}

	RebuildTileLookup();
	g_Log.Event(LOGM_INIT, "UltimaLive: loaded tree definitions from '%s' (%u templates).\n",
		static_cast<lpctstr>(sFile), static_cast<uint>(m_Templates.size()));
}

void CUltimaLiveHarvest::LoadDefinitions(lpctstr pszDirectory)
{
	RegisterBuiltinTrees();
	LoadTreesIni(pszDirectory);
}

const HarvestTreeTemplate * CUltimaLiveHarvest::LookupTemplate(word wTileId) const
{
	wTileId = NormalizeTileId(wTileId);
	const auto it = m_TileToTemplate.find(wTileId);
	if (it == m_TileToTemplate.end())
		return nullptr;
	if (it->second >= m_Templates.size())
		return nullptr;
	return &m_Templates[it->second];
}

bool CUltimaLiveHarvest::FindTrunkOrigin(int x, int y, char hitZ, word wHitTile, const HarvestTreeTemplate & tmpl, short & ox, short & oy, char & oz) const
{
	for (const HarvestGraphicDef & g : tmpl.mature)
	{
		if (g.wTileId == wHitTile)
		{
			ox = static_cast<short>(x - g.iDx);
			oy = static_cast<short>(y - g.iDy);
			oz = hitZ;
			return true;
		}
	}
	for (const HarvestGraphicDef & g : tmpl.fallen)
	{
		if (g.wTileId == wHitTile)
		{
			ox = static_cast<short>(x - g.iDx);
			oy = static_cast<short>(y - g.iDy);
			oz = hitZ;
			return true;
		}
	}
	if (tmpl.wStumpId && NormalizeTileId(tmpl.wStumpId) == wHitTile)
	{
		ox = static_cast<short>(x);
		oy = static_cast<short>(y);
		oz = hitZ;
		return true;
	}
	if (tmpl.wMatureTrunkId == wHitTile)
	{
		ox = static_cast<short>(x);
		oy = static_cast<short>(y);
		oz = hitZ;
		return true;
	}
	return false;
}

void CUltimaLiveHarvest::ResolveTrunkZ(CUltimaLive & live, int iMap, short ox, short oy, const HarvestTreeTemplate & tmpl, char & oz) const
{
	std::vector<CUOStaticItemRec> at;
	CollectStaticsAt(live, iMap, ox, oy, at);
	const word wTrunk = NormalizeTileId(tmpl.wMatureTrunkId);
	for (const CUOStaticItemRec & st : at)
	{
		if (NormalizeTileId(st.m_wTileID) == wTrunk)
		{
			oz = st.m_z;
			return;
		}
	}
}

bool CUltimaLiveHarvest::IsFallenGraphic(const HarvestTreeTemplate & tmpl, word wTileId)
{
	const word wNorm = static_cast<word>(wTileId & 0x3FFF);
	for (const HarvestGraphicDef & g : tmpl.fallen)
	{
		if ((g.wTileId & 0x3FFF) == wNorm)
			return true;
	}
	return false;
}

bool CUltimaLiveHarvest::IsStumpGraphic(const HarvestTreeTemplate & tmpl, word wTileId)
{
	return tmpl.wStumpId && ((wTileId & 0x3FFF) == (tmpl.wStumpId & 0x3FFF));
}

bool CUltimaLiveHarvest::HasMatureGraphicAt(CUltimaLive & live, int iMap, int x, int y) const
{
	std::vector<CUOStaticItemRec> at;
	CollectStaticsAt(live, iMap, x, y, at);
	for (const CUOStaticItemRec & hit : at)
	{
		const word wTileId = NormalizeTileId(hit.m_wTileID);
		const HarvestTreeTemplate * pTmpl = LookupTemplate(wTileId);
		if (!pTmpl)
			continue;
		for (const HarvestGraphicDef & g : pTmpl->mature)
		{
			if (g.wTileId == wTileId)
				return true;
		}
	}
	return false;
}

bool CUltimaLiveHarvest::HasFallenGraphicsAtTrunk(CUltimaLive & live, int iMap, short ox, short oy, char oz, const HarvestTreeTemplate & tmpl) const
{
	(void)oz;
	for (const HarvestGraphicDef & g : tmpl.fallen)
	{
		std::vector<CUOStaticItemRec> at;
		CollectStaticsAt(live, iMap, ox + g.iDx, oy + g.iDy, at);
		for (const CUOStaticItemRec & st : at)
		{
			if (IsFallenGraphic(tmpl, st.m_wTileID))
				return true;
		}
	}
	return false;
}

bool CUltimaLiveHarvest::TryRemoveStumpAfterChop(CUltimaLive & live, CChar * pChar, int chopX, int chopY)
{
	if (!pChar)
		return false;

	const int iMap = pChar->GetTopMap();
	std::vector<CUOStaticItemRec> at;
	CollectStaticsAt(live, iMap, chopX, chopY, at);
	for (const CUOStaticItemRec & hit : at)
	{
		const word wTileId = NormalizeTileId(hit.m_wTileID);
		const HarvestTreeTemplate * pTmpl = LookupTemplate(wTileId);
		if (!pTmpl || !IsStumpGraphic(*pTmpl, wTileId))
			continue;

		short ox = 0, oy = 0;
		char oz = hit.m_z;
		if (!FindTrunkOrigin(chopX, chopY, hit.m_z, wTileId, *pTmpl, ox, oy, oz))
			continue;
		ResolveTrunkZ(live, iMap, ox, oy, *pTmpl, oz);
		if (!IsRegrowthLocation(iMap, ox, oy, oz))
			continue;
		if (HasFallenGraphicsAtTrunk(live, iMap, ox, oy, oz, *pTmpl))
			continue;

		CPointMap pt(ox, oy, oz, static_cast<uchar>(iMap));
		CItem * pGem = FindLumberResourceGem(pt);
		if (pGem && pGem->GetAmount() > 0)
			continue;

		HarvestGraphicDef one;
		one.wTileId = pTmpl->wStumpId;
		one.iDx = 0;
		one.iDy = 0;
		std::vector<HarvestGraphicDef> piece = { one };
		live.SetBlockNotifySuppressed(true);
		RemoveGraphics(live, iMap, ox, oy, oz, piece);
		live.SetBlockNotifySuppressed(false);
		live.NotifyBlockChange(iMap, ox / UO_BLOCK_SIZE, oy / UO_BLOCK_SIZE, pChar);
		return true;
	}
	return false;
}

bool CUltimaLiveHarvest::CanGraphicHarvestAt(CUltimaLive & live, const CPointMap & ptChop) const
{
	if (!ptChop.IsValidPoint())
		return false;

	const int iMap = ptChop.m_map;
	std::vector<CUOStaticItemRec> at;
	CollectStaticsAt(live, iMap, ptChop.m_x, ptChop.m_y, at);
	for (const CUOStaticItemRec & hit : at)
	{
		const word wTileId = NormalizeTileId(hit.m_wTileID);
		const HarvestTreeTemplate * pTmpl = LookupTemplate(wTileId);
		if (!pTmpl)
			continue;

		short ox = 0, oy = 0;
		char oz = hit.m_z;
		if (!FindTrunkOrigin(ptChop.m_x, ptChop.m_y, hit.m_z, wTileId, *pTmpl, ox, oy, oz))
			continue;
		ResolveTrunkZ(live, iMap, ox, oy, *pTmpl, oz);
		if (!IsRegrowthLocation(iMap, ox, oy, oz))
			continue;

		if (IsFallenGraphic(*pTmpl, wTileId))
			return true;
	}
	return false;
}

bool CUltimaLiveHarvest::TryGraphicHarvestAt(CUltimaLive & live, CChar * pChar, int chopX, int chopY)
{
	if (!pChar)
		return false;

	const int iMap = pChar->GetTopMap();
	std::vector<CUOStaticItemRec> at;
	CollectStaticsAt(live, iMap, chopX, chopY, at);
	for (const CUOStaticItemRec & hit : at)
	{
		const word wTileId = NormalizeTileId(hit.m_wTileID);
		const HarvestTreeTemplate * pTmpl = LookupTemplate(wTileId);
		if (!pTmpl)
			continue;

		short ox = 0, oy = 0;
		char oz = hit.m_z;
		if (!FindTrunkOrigin(chopX, chopY, hit.m_z, wTileId, *pTmpl, ox, oy, oz))
			continue;
		ResolveTrunkZ(live, iMap, ox, oy, *pTmpl, oz);
		if (!IsRegrowthLocation(iMap, ox, oy, oz))
			continue;

		const bool fFallen = IsFallenGraphic(*pTmpl, wTileId);
		if (!fFallen)
			continue;

		HarvestGraphicDef one;
		one.wTileId = wTileId;
		one.iDx = static_cast<char>(chopX - ox);
		one.iDy = static_cast<char>(chopY - oy);
		std::vector<HarvestGraphicDef> piece = { one };

		live.SetBlockNotifySuppressed(true);
		RemoveGraphics(live, iMap, ox, oy, oz, piece);
		live.SetBlockNotifySuppressed(false);
		live.NotifyBlockChange(iMap, chopX / UO_BLOCK_SIZE, chopY / UO_BLOCK_SIZE, pChar);
		return true;
	}
	return false;
}

bool CUltimaLiveHarvest::IsHarvestGraphicAt(CUltimaLive & live, const CPointMap & pt, ITEMID_TYPE id) const
{
	if (!pt.IsValidPoint() || !id)
		return false;

	const word wTarget = NormalizeTileId(static_cast<word>(id));
	std::vector<CUOStaticItemRec> at;
	CollectStaticsAt(live, pt.m_map, pt.m_x, pt.m_y, at);
	for (const CUOStaticItemRec & hit : at)
	{
		if (NormalizeTileId(hit.m_wTileID) != wTarget)
			continue;
		if (LookupTemplate(hit.m_wTileID))
			return true;
	}
	return false;
}

bool CUltimaLiveHarvest::IsRegrowthLocation(int iMap, short ox, short oy, char oz) const
{
	const auto it = m_Regrowth.find(iMap);
	if (it == m_Regrowth.end())
		return false;
	for (const HarvestRegrowthEntry & e : it->second)
	{
		if (e.iX == ox && e.iY == oy && e.iZ == oz)
			return true;
	}
	return false;
}

void CUltimaLiveHarvest::RecordRegrowth(int iMap, short ox, short oy, char oz, word wMatureTileId)
{
	HarvestRegrowthEntry entry;
	entry.iMap = iMap;
	entry.iX = ox;
	entry.iY = oy;
	entry.iZ = oz;
	entry.wMatureTileId = wMatureTileId;
	entry.iTimeFelled = CWorldGameTime::GetCurrentTime().GetTimeRaw();

	std::vector<HarvestRegrowthEntry> & list = m_Regrowth[iMap];
	for (HarvestRegrowthEntry & e : list)
	{
		if (e.iX == ox && e.iY == oy && e.iZ == oz)
		{
			e = entry;
			return;
		}
	}
	list.push_back(entry);
}

void CUltimaLiveHarvest::RemoveRegrowthAt(int iMap, short ox, short oy, char oz)
{
	const auto it = m_Regrowth.find(iMap);
	if (it == m_Regrowth.end())
		return;
	auto & list = it->second;
	list.erase(
		std::remove_if(list.begin(), list.end(), [&](const HarvestRegrowthEntry & e)
		{
			return e.iX == ox && e.iY == oy && e.iZ == oz;
		}),
		list.end());
}

bool CUltimaLiveHarvest::RemoveGraphics(CUltimaLive & live, int iMap, short ox, short oy, char oz, const std::vector<HarvestGraphicDef> & graphics)
{
	(void)oz;
	bool fChanged = false;
	for (const HarvestGraphicDef & g : graphics)
	{
		const int tx = ox + g.iDx;
		const int ty = oy + g.iDy;
		const word wTarget = NormalizeTileId(g.wTileId);

		std::vector<CUOStaticItemRec> at;
		CollectStaticsAt(live, iMap, tx, ty, at);
		for (const CUOStaticItemRec & st : at)
		{
			if (NormalizeTileId(st.m_wTileID) != wTarget)
				continue;
			// Trunk and foliage on the same tile often use different Z; match by graphic id + tile.
			if (live.DeleteStaticAt(iMap, tx, ty, st.m_wTileID, st.m_z))
				fChanged = true;
		}
	}
	return fChanged;
}

bool CUltimaLiveHarvest::AddGraphics(CUltimaLive & live, int iMap, short ox, short oy, char oz, const std::vector<HarvestGraphicDef> & graphics, word wHue)
{
	bool fChanged = false;
	for (const HarvestGraphicDef & g : graphics)
	{
		const int tx = ox + g.iDx;
		const int ty = oy + g.iDy;

		CUOStaticItemRec st;
		st.m_wTileID = g.wTileId;
		st.m_x = static_cast<byte>(tx & 7);
		st.m_y = static_cast<byte>(ty & 7);
		st.m_z = oz;
		st.m_wHue = wHue;

		if (live.SetStaticAt(iMap, tx, ty, st, true))
			fChanged = true;
	}
	return fChanged;
}

void CUltimaLiveHarvest::NotifyGraphicsChange(CUltimaLive & live, int iMap, short ox, short oy, const HarvestTreeTemplate & tmpl, CChar * pChar)
{
	std::unordered_set<dword> blocks;
	auto markBlock = [&](int tx, int ty)
	{
		blocks.insert(live.GetBlockId(iMap, tx / UO_BLOCK_SIZE, ty / UO_BLOCK_SIZE));
	};

	for (const HarvestGraphicDef & g : tmpl.mature)
		markBlock(ox + g.iDx, oy + g.iDy);
	for (const HarvestGraphicDef & g : tmpl.fallen)
		markBlock(ox + g.iDx, oy + g.iDy);
	if (tmpl.wStumpId)
		markBlock(ox, oy);

	for (const dword dwBlock : blocks)
	{
		int bx = 0, by = 0;
		live.GetBlockXY(iMap, dwBlock, bx, by);
		live.NotifyBlockChange(iMap, bx, by, pChar);
	}
}

bool CUltimaLiveHarvest::ApplyFallenState(CUltimaLive & live, const HarvestRegrowthEntry & entry)
{
	const HarvestTreeTemplate * pTmpl = LookupTemplate(entry.wMatureTileId);
	if (!pTmpl)
		return false;

	live.SetBlockNotifySuppressed(true);
	RemoveGraphics(live, entry.iMap, entry.iX, entry.iY, entry.iZ, pTmpl->mature);
	AddGraphics(live, entry.iMap, entry.iX, entry.iY, entry.iZ, pTmpl->fallen);

	if (pTmpl->wStumpId)
	{
		CUOStaticItemRec stump;
		stump.m_wTileID = pTmpl->wStumpId;
		stump.m_x = static_cast<byte>(entry.iX & 7);
		stump.m_y = static_cast<byte>(entry.iY & 7);
		stump.m_z = entry.iZ;
		stump.m_wHue = 0;
		live.SetStaticAt(entry.iMap, entry.iX, entry.iY, stump, true);
	}

	live.SetBlockNotifySuppressed(false);
	NotifyGraphicsChange(live, entry.iMap, entry.iX, entry.iY, *pTmpl, nullptr);
	return true;
}

bool CUltimaLiveHarvest::RestoreMatureTree(CUltimaLive & live, HarvestRegrowthEntry & entry)
{
	const HarvestTreeTemplate * pTmpl = LookupTemplate(entry.wMatureTileId);
	if (!pTmpl)
		return false;

	live.SetBlockNotifySuppressed(true);
	RemoveGraphics(live, entry.iMap, entry.iX, entry.iY, entry.iZ, pTmpl->fallen);
	if (pTmpl->wStumpId)
		live.DeleteStaticAt(entry.iMap, entry.iX, entry.iY, pTmpl->wStumpId, entry.iZ);

	const std::vector<HarvestGraphicDef> & restore = !pTmpl->sapling.empty() ? pTmpl->sapling : pTmpl->mature;
	AddGraphics(live, entry.iMap, entry.iX, entry.iY, entry.iZ, restore);
	live.SetBlockNotifySuppressed(false);
	NotifyGraphicsChange(live, entry.iMap, entry.iX, entry.iY, *pTmpl, nullptr);
	return true;
}

void CUltimaLiveHarvest::CollectStaticsAt(CUltimaLive & live, int iMap, int x, int y, std::vector<CUOStaticItemRec> & out) const
{
	out.clear();

	const int bx = x / UO_BLOCK_SIZE;
	const int by = y / UO_BLOCK_SIZE;
	const dword dwBlockId = static_cast<dword>((bx * (g_MapList.GetMapSizeY(iMap) / UO_BLOCK_SIZE)) + by);

	if (live.m_pOverlay->HasStatics(iMap, dwBlockId))
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

bool CUltimaLiveHarvest::FindRegisteredTreeNear(CUltimaLive & live, int iMap, int cx, int cy, int iRadius, int & outX, int & outY) const
{
	int iBest = iRadius + 1;
	bool fFound = false;

	for (int dx = -iRadius; dx <= iRadius; ++dx)
	{
		for (int dy = -iRadius; dy <= iRadius; ++dy)
		{
			const int tx = cx + dx;
			const int ty = cy + dy;
			std::vector<CUOStaticItemRec> at;
			CollectStaticsAt(live, iMap, tx, ty, at);
			for (const CUOStaticItemRec & st : at)
			{
				if (!LookupTemplate(st.m_wTileID))
					continue;
				const int dist = (abs(dx) > abs(dy)) ? abs(dx) : abs(dy);
				if (dist < iBest)
				{
					iBest = dist;
					outX = tx;
					outY = ty;
					fFound = true;
				}
			}
		}
	}
	return fFound;
}

bool CUltimaLiveHarvest::TryHarvestAtTile(CUltimaLive & live, CChar * pChar, int iMap, int x, int y)
{
	std::vector<CUOStaticItemRec> at;
	CollectStaticsAt(live, iMap, x, y, at);

	if (at.empty())
		return false;

	for (const CUOStaticItemRec & hit : at)
	{
		const word wTileId = NormalizeTileId(hit.m_wTileID);
		const HarvestTreeTemplate * pTmpl = LookupTemplate(wTileId);
		if (!pTmpl)
			continue;

		short ox = 0, oy = 0;
		char oz = 0;
		if (!FindTrunkOrigin(x, y, hit.m_z, wTileId, *pTmpl, ox, oy, oz))
			continue;

		ResolveTrunkZ(live, iMap, ox, oy, *pTmpl, oz);

		bool fIsMature = false;
		for (const HarvestGraphicDef & g : pTmpl->mature)
		{
			if (g.wTileId == wTileId)
			{
				fIsMature = true;
				break;
			}
		}

		if (IsRegrowthLocation(iMap, ox, oy, oz))
		{
			if (fIsMature)
				return true;

			// Logs from the trunk tile must not strip fallen logs or the stump.
			if (!IsFallenGraphic(*pTmpl, wTileId))
				return true;

			HarvestGraphicDef one;
			one.wTileId = wTileId;
			one.iDx = static_cast<char>(x - ox);
			one.iDy = static_cast<char>(y - oy);
			std::vector<HarvestGraphicDef> piece = { one };
			live.SetBlockNotifySuppressed(true);
			RemoveGraphics(live, iMap, ox, oy, oz, piece);
			live.SetBlockNotifySuppressed(false);
			live.NotifyBlockChange(iMap, x / UO_BLOCK_SIZE, y / UO_BLOCK_SIZE, pChar);
			return true;
		}

		if (!fIsMature)
			continue;

		live.SetBlockNotifySuppressed(true);
		if (!RemoveGraphics(live, iMap, ox, oy, oz, pTmpl->mature))
		{
			live.SetBlockNotifySuppressed(false);
			g_Log.EventWarn("UltimaLive: failed to remove mature tree graphics at %d,%d,%d map %d\n",
				ox, oy, static_cast<int>(oz), iMap);
			return false;
		}
		AddGraphics(live, iMap, ox, oy, oz, pTmpl->fallen);

		if (pTmpl->wStumpId)
		{
			CUOStaticItemRec stump;
			stump.m_wTileID = pTmpl->wStumpId;
			stump.m_x = static_cast<byte>(ox & 7);
			stump.m_y = static_cast<byte>(oy & 7);
			stump.m_z = oz;
			stump.m_wHue = 0;
			live.SetStaticAt(iMap, ox, oy, stump, false);
		}

		RecordRegrowth(iMap, ox, oy, oz, pTmpl->wMatureTrunkId);
		ReplenishFallenResourceAtTrunk(iMap, ox, oy, oz, *pTmpl);
		live.SetBlockNotifySuppressed(false);
		NotifyGraphicsChange(live, iMap, ox, oy, *pTmpl, pChar);

		std::vector<CUOStaticItemRec> trunkAfter;
		CollectStaticsAt(live, iMap, ox, oy, trunkAfter);
		CSString sIds;
		for (size_t i = 0; i < trunkAfter.size(); ++i)
		{
			if (i > 0)
				sIds += ",";
			CSString sOne;
			sOne.Format("0x%x", NormalizeTileId(trunkAfter[i].m_wTileID));
			sIds += sOne;
		}
		g_Log.Event(LOGM_CLIENTS_LOG, "UltimaLive: harvested tree 0x%x -> fallen at %d,%d,%d map %d (trunk statics now:%s)\n",
			pTmpl->wMatureTrunkId, ox, oy, static_cast<int>(oz), iMap,
			trunkAfter.empty() ? "none" : static_cast<lpctstr>(sIds));
		return true;
	}

	return false;
}

bool CUltimaLiveHarvest::ResolveLumberjackResourcePoint(CUltimaLive & live, const CPointMap & ptChop, CPointMap & ptResource) const
{
	if (!ptChop.IsValidPoint())
		return false;

	const int iMap = ptChop.m_map;
	auto tryTile = [&](int x, int y) -> bool
	{
		std::vector<CUOStaticItemRec> at;
		CollectStaticsAt(live, iMap, x, y, at);
		for (const CUOStaticItemRec & hit : at)
		{
			const word wTileId = NormalizeTileId(hit.m_wTileID);
			const HarvestTreeTemplate * pTmpl = LookupTemplate(wTileId);
			if (!pTmpl)
				continue;

			for (const HarvestGraphicDef & g : pTmpl->mature)
			{
				if (g.wTileId == wTileId)
				{
					ptResource = CPointMap(static_cast<short>(x), static_cast<short>(y), hit.m_z, static_cast<uchar>(iMap));
					return true;
				}
			}

			short ox = 0, oy = 0;
			char oz = hit.m_z;
			if (!FindTrunkOrigin(x, y, hit.m_z, wTileId, *pTmpl, ox, oy, oz))
				continue;
			ResolveTrunkZ(live, iMap, ox, oy, *pTmpl, oz);
			if (!IsRegrowthLocation(iMap, ox, oy, oz))
				continue;

			ptResource = CPointMap(ox, oy, oz, static_cast<uchar>(iMap));
			return true;
		}
		return false;
	};

	if (tryTile(ptChop.m_x, ptChop.m_y))
		return true;

	int nearX = 0, nearY = 0;
	if (FindRegisteredTreeNear(live, iMap, ptChop.m_x, ptChop.m_y, 4, nearX, nearY))
		return tryTile(nearX, nearY);

	return false;
}

bool CUltimaLiveHarvest::HarvestTree(CUltimaLive & live, CChar * pChar, int x, int y, int z)
{
	if (!m_fEnabled || !pChar)
		return false;

	const int iMap = pChar->GetTopMap();

	if (TryHarvestAtTile(live, pChar, iMap, x, y))
		return true;

	// Pre-fell: allow targeting a tile near a mature tree (not fallen/stump tiles).
	int treeX = 0, treeY = 0;
	if (FindRegisteredTreeNear(live, iMap, x, y, 2, treeX, treeY))
	{
		if ((treeX != x || treeY != y) && HasMatureGraphicAt(live, iMap, treeX, treeY))
		{
			if (TryHarvestAtTile(live, pChar, iMap, treeX, treeY))
				return true;
		}
	}

	std::vector<CUOStaticItemRec> at;
	CollectStaticsAt(live, iMap, x, y, at);
	if (at.empty())
	{
		g_Log.Event(LOGM_CLIENTS_LOG,
			"UltimaLive: lumberjack at %d,%d,%d map %d - no map statics (chopped terrain/resource only?)\n",
			x, y, z, iMap);
	}
	else
	{
		CSString sIds;
		for (const CUOStaticItemRec & st : at)
		{
			CSString sOne;
			sOne.Format(" 0x%x", NormalizeTileId(st.m_wTileID));
			sIds += sOne;
		}
		g_Log.Event(LOGM_CLIENTS_LOG,
			"UltimaLive: lumberjack at %d,%d,%d map %d - statics%s not in harvest registry (templates=%u)\n",
			x, y, z, iMap, static_cast<lpctstr>(sIds), static_cast<uint>(m_Templates.size()));
	}
	return false;
}

void CUltimaLiveHarvest::ProcessRegrowth(CUltimaLive & live)
{
	if (!m_fEnabled || !m_fRegrowth)
		return;

	const int64 now = CWorldGameTime::GetCurrentTime().GetTimeRaw();
	const int64 regrowthMs = static_cast<int64>(m_iRegrowthMinutes) * 60LL * 1000LL;

	for (auto & kv : m_Regrowth)
	{
		std::vector<HarvestRegrowthEntry> grown;
		for (HarvestRegrowthEntry & entry : kv.second)
		{
			if ((now - entry.iTimeFelled) < regrowthMs)
				continue;
			if (RestoreMatureTree(live, entry))
				grown.push_back(entry);
		}

		for (const HarvestRegrowthEntry & entry : grown)
			RemoveRegrowthAt(kv.first, entry.iX, entry.iY, entry.iZ);
	}

	m_iLastRegrowthPass = now;
}

void CUltimaLiveHarvest::LoadRegrowth(lpctstr pszDirectory, CUltimaLiveOverlay & overlay, CUltimaLive & live)
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
		sFile.Format("TreeLocations.%d", m);
		CSString sFull = CSFile::GetMergedFileName(pszDirectory, sFile);

		CSFile file;
		if (!file.Open(sFull, OF_READ | OF_BINARY))
			continue;

		dword magic = 0;
		if (file.Read(&magic, sizeof(magic)) <= 0)
			continue;
		if (magic != kTreeLocMagic)
			continue;

		word version = 0;
		dword count = 0;
		if (file.Read(&version, sizeof(version)) <= 0)
			continue;
		if (file.Read(&count, sizeof(count)) <= 0)
			continue;

		for (dword i = 0; i < count; ++i)
		{
			HarvestRegrowthEntry entry;
			entry.iMap = m;
			if (file.Read(&entry.iX, sizeof(entry.iX)) <= 0)
				break;
			if (file.Read(&entry.iY, sizeof(entry.iY)) <= 0)
				break;
			if (file.Read(&entry.iZ, sizeof(entry.iZ)) <= 0)
				break;
			if (file.Read(&entry.wMatureTileId, sizeof(entry.wMatureTileId)) <= 0)
				break;
			if (version >= 2)
			{
				if (file.Read(&entry.iTimeFelled, sizeof(entry.iTimeFelled)) <= 0)
					entry.iTimeFelled = CWorldGameTime::GetCurrentTime().GetTimeRaw();
			}
			else
			{
				entry.iTimeFelled = CWorldGameTime::GetCurrentTime().GetTimeRaw();
			}

			m_Regrowth[m].push_back(entry);
			ApplyFallenState(live, entry);
		}
	}

	CSString sLegacy = CSFile::GetMergedFileName(pszDirectory, "harvest.lumber");
	CSFile legacy;
	if (!legacy.Open(sLegacy, OF_READ | OF_BINARY))
		return;

	const size_t len = legacy.GetLength();
	if (len < 11 || (len % 11) != 0)
		return;

	for (size_t pos = 0; pos + 11 <= len; pos += 11)
	{
		byte buf[11];
		if (legacy.Read(buf, 11) <= 0)
			break;

		HarvestRegrowthEntry entry;
		entry.iMap = buf[0];
		entry.iX = static_cast<short>(buf[1] | (buf[2] << 8));
		entry.iY = static_cast<short>(buf[3] | (buf[4] << 8));
		entry.iZ = static_cast<char>(buf[5]);
		entry.wMatureTileId = static_cast<word>(buf[6] | (buf[7] << 8));
		entry.iTimeFelled = CWorldGameTime::GetCurrentTime().GetTimeRaw();

		if (IsRegrowthLocation(entry.iMap, entry.iX, entry.iY, entry.iZ))
			continue;

		m_Regrowth[entry.iMap].push_back(entry);
		ApplyFallenState(live, entry);
	}
}

void CUltimaLiveHarvest::SaveRegrowth(lpctstr pszDirectory) const
{
	if (!pszDirectory || !*pszDirectory)
		return;

	for (const auto & kv : m_Regrowth)
	{
		if (kv.second.empty())
			continue;

		CSString sFile;
		sFile.Format("TreeLocations.%d", kv.first);
		CSString sFull = CSFile::GetMergedFileName(pszDirectory, sFile);

		CSFile file;
		if (!file.Open(sFull, OF_WRITE | OF_BINARY))
			continue;

		const dword magic = kTreeLocMagic;
		const word version = kTreeLocVersion;
		const dword count = static_cast<dword>(kv.second.size());
		file.Write(&magic, sizeof(magic));
		file.Write(&version, sizeof(version));
		file.Write(&count, sizeof(count));

		for (const HarvestRegrowthEntry & entry : kv.second)
		{
			file.Write(&entry.iX, sizeof(entry.iX));
			file.Write(&entry.iY, sizeof(entry.iY));
			file.Write(&entry.iZ, sizeof(entry.iZ));
			file.Write(&entry.wMatureTileId, sizeof(entry.wMatureTileId));
			file.Write(&entry.iTimeFelled, sizeof(entry.iTimeFelled));
		}
		file.Close();
	}
}
