/**
 * @file CUltimaLiveMining.h
 * Graphic-based mining and tunnel excavation (UltimaLive dynamic cave system).
 */

#ifndef _INC_CULTIMALIVEMINING_H
#define _INC_CULTIMALIVEMINING_H

#include "../../common/common.h"
#include "../CWorldMap.h"
#include "../uo_files/CUOStaticItemRec.h"
#include <unordered_map>
#include <vector>

class CChar;
class CUltimaLive;
class CUltimaLiveOverlay;

struct MiningGraphicDef
{
	word wTileId = 0;
	char iDx = 0;
	char iDy = 0;
};

struct MiningWallTemplate
{
	word wWallTileId = 0;
	word wFloorTileId = 0x0244; // Default cave floor
	word wRubbleTileId = 0x1363; // Rubble
	std::vector<MiningGraphicDef> walls;
	std::vector<MiningGraphicDef> rubble;
};

struct MiningRegrowthEntry
{
	int iMap = 0;
	short iX = 0;
	short iY = 0;
	char iZ = 0;
	word wOriginalWallTileId = 0;
	word wFloorTileId = 0x0244;
	int64 iTimeExcavated = 0;
};

class CUltimaLiveMining
{
public:
	void Clear();

	bool IsEnabled() const noexcept { return m_fEnabled; }
	void SetEnabled(bool fEnabled) { m_fEnabled = fEnabled; }

	bool IsRegrowthEnabled() const noexcept { return m_fRegrowth; }
	void SetRegrowthEnabled(bool fEnabled) { m_fRegrowth = fEnabled; }

	int GetRegrowthMinutes() const noexcept { return m_iRegrowthMinutes; }
	void SetRegrowthMinutes(int iMinutes) { m_iRegrowthMinutes = (iMinutes > 0) ? iMinutes : 10080; } // 7 days default

	int GetRequiredStrokes() const noexcept { return m_iRequiredStrokes; }
	void SetRequiredStrokes(int iStrokes) { m_iRequiredStrokes = (iStrokes > 0) ? iStrokes : 3; }

	void LoadDefinitions(lpctstr pszDirectory);
	void LoadRegrowth(lpctstr pszDirectory, CUltimaLiveOverlay & overlay, CUltimaLive & live);
	void SaveRegrowth(lpctstr pszDirectory) const;

	bool CanMiningExcavateAt(CUltimaLive & live, const CPointMap & ptMine) const;
	bool TryMiningHarvest(CUltimaLive & live, CChar * pChar, int mineX, int mineY, int mineZ);
	void ProcessRegrowth(CUltimaLive & live);

	bool IsMiningGraphicAt(CUltimaLive & live, const CPointMap & pt, ITEMID_TYPE id) const;

private:
	static constexpr dword kTunnelLocMagic = 0x544E4C43; // 'TNLC' TunnelLocations
	static constexpr word kTunnelLocVersion = 1;

	bool m_fEnabled = true;
	bool m_fRegrowth = true;
	int m_iRegrowthMinutes = 10080; // 7 days default
	int m_iRequiredStrokes = 3;
	int64 m_iLastRegrowthPass = 0;

	std::vector<MiningWallTemplate> m_Templates;
	std::unordered_map<word, size_t> m_TileToTemplate;

	// Track hit progress per tile (hash of map+x+y)
	std::unordered_map<dword, int> m_StrokeCounts;

	// MapId -> vector of active excavated tunnel entries
	std::unordered_map<int, std::vector<MiningRegrowthEntry>> m_Regrowth;

	void RegisterBuiltinWalls();
	void LoadTunnelsIni(lpctstr pszDirectory);
	void RebuildTileLookup();
	const MiningWallTemplate * LookupTemplate(word wTileId) const;

	void CollectStaticsAt(CUltimaLive & live, int iMap, int x, int y, std::vector<CUOStaticItemRec> & out) const;
	void ApplyCaveExpansionTile(CUltimaLive & live, int iMap, int x, int y, word wTileToAdd, char z) const;
	bool IsRegrowthLocation(int iMap, short ox, short oy, char oz) const;
	void RecordRegrowth(int iMap, short ox, short oy, char oz, word wOriginalWallTileId, word wFloorTileId);
	void RemoveRegrowthAt(int iMap, short ox, short oy, char oz);

	bool ApplyExcavatedState(CUltimaLive & live, const MiningRegrowthEntry & entry);
	bool RestoreWall(CUltimaLive & live, MiningRegrowthEntry & entry);
};

#endif // _INC_CULTIMALIVEMINING_H
