/**
 * @file CUltimaLiveDiscovery.h
 * Per-character UltimaLive map block discovery (fog-of-war streaming).
 */

#ifndef _INC_CULTIMALIVEDISCOVERY_H
#define _INC_CULTIMALIVEDISCOVERY_H

#include "../../common/common.h"
#include "../../common/CPointBase.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

class CUltimaLiveDiscovery
{
public:
	void Clear();

	bool IsEnabled() const noexcept { return m_fEnabled; }
	void SetEnabled(bool fEnabled) { m_fEnabled = fEnabled; }

	int GetViewBlocks() const noexcept { return m_iViewBlocks; }
	void SetViewBlocks(int iBlocks) { m_iViewBlocks = (iBlocks > 0) ? iBlocks : 3; }

	bool IsRevealOnLogin() const noexcept { return m_fRevealOnLogin; }
	void SetRevealOnLogin(bool fReveal) { m_fRevealOnLogin = fReveal; }

	bool IsDiscovered(int iMap, dword dwBlockId) const;
	void MarkDiscovered(int iMap, dword dwBlockId);

	bool IsBlockInRevealRange(int iMap, int iBlockX, int iBlockY, const CPointMap & ptChar) const;

	void CollectBlocksForMap(int iMap, std::vector<dword> & out) const;
	void CollectBlocksNear(int iMap, int iBlockX, int iBlockY, int iRadiusBlocks, std::vector<dword> & out) const;

	bool LoadForChar(dword dwCharUid, lpctstr pszDirectory);
	bool SaveForChar(dword dwCharUid, lpctstr pszDirectory) const;

private:
	bool m_fEnabled = false;
	bool m_fRevealOnLogin = true;
	int m_iViewBlocks = 3;

	std::unordered_map<int, std::unordered_set<dword>> m_Blocks;
};

#endif // _INC_CULTIMALIVEDISCOVERY_H
