/**
 * @file CUltimaLiveHarvest.h
 * Graphic-based lumber harvest (UltimaLive reference parity).
 */

#ifndef _INC_CULTIMALIVEHARVEST_H
#define _INC_CULTIMALIVEHARVEST_H

#include "../../common/common.h"
#include "../CWorldMap.h"
#include "../uo_files/CUOStaticItemRec.h"
#include <unordered_map>
#include <vector>

class CChar;
class CUltimaLive;
class CUltimaLiveOverlay;

struct HarvestGraphicDef
{
	word wTileId = 0;
	char iDx = 0;
	char iDy = 0;
};

struct HarvestTreeTemplate
{
	word wMatureTrunkId = 0;
	word wStumpId = 0xE57;
	std::vector<HarvestGraphicDef> mature;
	std::vector<HarvestGraphicDef> fallen;
	std::vector<HarvestGraphicDef> sapling;
};

struct HarvestRegrowthEntry
{
	int iMap = 0;
	short iX = 0;
	short iY = 0;
	char iZ = 0;
	word wMatureTileId = 0;
	int64 iTimeFelled = 0;
};

class CUltimaLiveHarvest
{
public:
	void Clear();

	bool IsEnabled() const noexcept { return m_fEnabled; }
	void SetEnabled(bool fEnabled) { m_fEnabled = fEnabled; }

	bool IsRegrowthEnabled() const noexcept { return m_fRegrowth; }
	void SetRegrowthEnabled(bool fEnabled) { m_fRegrowth = fEnabled; }

	int GetRegrowthMinutes() const noexcept { return m_iRegrowthMinutes; }
	void SetRegrowthMinutes(int iMinutes) { m_iRegrowthMinutes = (iMinutes > 0) ? iMinutes : 1440; }

	void LoadDefinitions(lpctstr pszDirectory);
	void LoadRegrowth(lpctstr pszDirectory, CUltimaLiveOverlay & overlay, CUltimaLive & live);
	void SaveRegrowth(lpctstr pszDirectory) const;

	bool HarvestTree(CUltimaLive & live, CChar * pChar, int x, int y, int z);
	void ProcessRegrowth(CUltimaLive & live);

	// Map a chop target (trunk, fallen log, or stump) to the resource-tracker tile.
	bool ResolveLumberjackResourcePoint(CUltimaLive & live, const CPointMap & ptChop, CPointMap & ptResource) const;
	// Fallen log or stump at the exact chop tile on a felled tree (not tied to world-gem amount).
	bool CanGraphicHarvestAt(CUltimaLive & live, const CPointMap & ptChop) const;
	bool TryGraphicHarvestAt(CUltimaLive & live, CChar * pChar, int chopX, int chopY);
	// After lumberjack completes: clear stump when fallen logs are gone and the gem is empty.
	bool TryRemoveStumpAfterChop(CUltimaLive & live, CChar * pChar, int chopX, int chopY);
	// Overlay harvest graphics (stump, fallen, mature) are not in the MUL; used by CanTouchStatic.
	bool IsHarvestGraphicAt(CUltimaLive & live, const CPointMap & pt, ITEMID_TYPE id) const;

private:
	static constexpr int kZTolerance = 5;
	static constexpr dword kTreeLocMagic = 0x544C4F43; // 'COLT' TreeLocations
	static constexpr word kTreeLocVersion = 2;

	bool m_fEnabled = true;
	bool m_fRegrowth = true;
	int m_iRegrowthMinutes = 1440;
	int64 m_iLastRegrowthPass = 0;

	std::vector<HarvestTreeTemplate> m_Templates;
	std::unordered_map<word, size_t> m_TileToTemplate;

	std::unordered_map<int, std::vector<HarvestRegrowthEntry>> m_Regrowth;

	void RegisterBuiltinTrees();
	void LoadTreesIni(lpctstr pszDirectory);
	void RebuildTileLookup();
	const HarvestTreeTemplate * LookupTemplate(word wTileId) const;
	bool FindTrunkOrigin(int x, int y, char hitZ, word wHitTile, const HarvestTreeTemplate & tmpl, short & ox, short & oy, char & oz) const;
	void ResolveTrunkZ(CUltimaLive & live, int iMap, short ox, short oy, const HarvestTreeTemplate & tmpl, char & oz) const;
	bool TryHarvestAtTile(CUltimaLive & live, CChar * pChar, int iMap, int x, int y);
	bool FindRegisteredTreeNear(CUltimaLive & live, int iMap, int cx, int cy, int iRadius, int & outX, int & outY) const;
	void CollectStaticsAt(CUltimaLive & live, int iMap, int x, int y, std::vector<CUOStaticItemRec> & out) const;
	bool IsRegrowthLocation(int iMap, short ox, short oy, char oz) const;
	void RecordRegrowth(int iMap, short ox, short oy, char oz, word wMatureTileId);
	void RemoveRegrowthAt(int iMap, short ox, short oy, char oz);

	static bool IsFallenGraphic(const HarvestTreeTemplate & tmpl, word wTileId);
	static bool IsStumpGraphic(const HarvestTreeTemplate & tmpl, word wTileId);
	bool HasMatureGraphicAt(CUltimaLive & live, int iMap, int x, int y) const;
	bool HasFallenGraphicsAtTrunk(CUltimaLive & live, int iMap, short ox, short oy, char oz, const HarvestTreeTemplate & tmpl) const;

	bool RemoveGraphics(CUltimaLive & live, int iMap, short ox, short oy, char oz, const std::vector<HarvestGraphicDef> & graphics);
	bool AddGraphics(CUltimaLive & live, int iMap, short ox, short oy, char oz, const std::vector<HarvestGraphicDef> & graphics, word wHue = 0);
	void NotifyGraphicsChange(CUltimaLive & live, int iMap, short ox, short oy, const HarvestTreeTemplate & tmpl, CChar * pChar);

	bool ApplyFallenState(CUltimaLive & live, const HarvestRegrowthEntry & entry);
	bool RestoreMatureTree(CUltimaLive & live, HarvestRegrowthEntry & entry);
};

#endif // _INC_CULTIMALIVEHARVEST_H
