/**
 * @file CUltimaLiveOverlay.h
 * Mutable UltimaLive map block overrides and .live persistence.
 */

#ifndef _INC_CULTIMALIVEOVERLAY_H
#define _INC_CULTIMALIVEOVERLAY_H

#include "../../common/common.h"
#include "../uo_files/CUOStaticItemRec.h"
#include "../uo_files/CUOMapList.h"
#include <unordered_map>
#include <vector>

struct UltimaLiveBlockOverlay
{
	bool fHasLand = false;
	bool fHasStatics = false;
	byte m_land[192]{};
	std::vector<CUOStaticItemRec> m_statics;
};

class CUltimaLiveOverlay
{
public:
	void Clear();
	void LoadLiveFiles(lpctstr pszDirectory);
	void SaveLiveFiles(lpctstr pszDirectory) const;

	bool HasLand(int iMap, dword dwBlockId) const;
	bool HasStatics(int iMap, dword dwBlockId) const;

	bool GetLand(int iMap, dword dwBlockId, byte land[192]) const;
	bool GetStatics(int iMap, dword dwBlockId, std::vector<CUOStaticItemRec> & out) const;

	void SetLandBlock(int iMap, int iBlockX, int iBlockY, const byte land[192]);
	void SetStaticsBlock(int iMap, int iBlockX, int iBlockY, const std::vector<CUOStaticItemRec> & statics);

	bool GetLandTile(int iMap, int x, int y, word & wId, char & z) const;
	bool SetLandTile(int iMap, int x, int y, word wId, char z);

	bool GetStaticsAt(int iMap, int x, int y, std::vector<CUOStaticItemRec> & out) const;
	bool SetStaticsAt(int iMap, int x, int y, const std::vector<CUOStaticItemRec> & tiles);

private:
	UltimaLiveBlockOverlay * GetOrCreate(int iMap, dword dwBlockId);

	std::unordered_map<dword, UltimaLiveBlockOverlay> m_Blocks[MAP_SUPPORTED_QTY];
};

#endif // _INC_CULTIMALIVEOVERLAY_H
