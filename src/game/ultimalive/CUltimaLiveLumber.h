/**
 * @file CUltimaLiveLumber.h
 * UltimaLive lumber harvest persistence (server-side companion files).
 */

#ifndef _INC_CULTIMALIVELUMBER_H
#define _INC_CULTIMALIVELUMBER_H

#include "../../common/common.h"

class CUltimaLiveOverlay;

class CUltimaLiveLumber
{
public:
	void Load(lpctstr pszDirectory, CUltimaLiveOverlay & overlay);
	void Save(lpctstr pszDirectory) const;

	void RecordTreeFelled(int iMap, int x, int y, int z, word wTileId);
	void ApplyToOverlay(CUltimaLiveOverlay & overlay) const;

private:
	struct LumberRecord
	{
		int iMap = 0;
		int x = 0;
		int y = 0;
		int z = 0;
		word wTileId = 0;
	};

	std::vector<LumberRecord> m_records;
};

#endif // _INC_CULTIMALIVELUMBER_H
