/**
 * @file CUltimaLive.h
 * UltimaLive map streaming (ClassicUO / Igrping.dll protocol).
 * Portions adapted from SaschaKP/UltimaLive (MIT License).
 */

#ifndef _INC_CULTIMALIVE_H
#define _INC_CULTIMALIVE_H

#include "../../common/common.h"
#include "../uo_files/CUOMapList.h"
#include "../uo_files/CUOStaticItemRec.h"
#include "../CWorldMap.h"
#include <vector>

class CClient;
class CChar;
class CScript;

struct UltimaLiveMapDef
{
	int iMapIndex = -1;
	int iFileIndex = 0;
	uint16 uiWidth = 0;
	uint16 uiHeight = 0;
	uint16 uiWrapX = 0;
	uint16 uiWrapY = 0;
	bool fRegistered = false;
};

enum UltimaLiveLocalUpdate : byte
{
	ULTIMALIVE_UPDATE_NONE = 0,
	ULTIMALIVE_UPDATE_TERRAIN = 1,
	ULTIMALIVE_UPDATE_STATICS = 2,
};

class CUltimaLive
{
	friend class CUltimaLiveHarvest;
	friend class CUltimaLiveMining;

public:
	static const char *m_sClassName;

	static CUltimaLive &Get();

	bool IsEnabled() const noexcept { return m_fEnabled; }
	bool IsStreaming() const noexcept { return m_fEnabled && m_fStreaming; }
	bool IsDiscoveryEnabled() const noexcept { return m_fEnabled && m_fDiscovery; }
	int GetDiscoveryViewBlocks() const noexcept { return m_iDiscoveryViewBlocks; }
	bool IsDiscoveryRevealOnLogin() const noexcept { return m_fDiscoveryRevealOnLogin; }
	bool IsHarvestEnabled() const noexcept { return m_fEnabled && m_fHarvest; }
	int GetHarvestRegrowthMinutes() const noexcept { return m_iHarvestRegrowthMinutes; }
	bool IsMiningEnabled() const noexcept { return m_fEnabled && m_fMining; }
	int GetMiningRegrowthMinutes() const noexcept { return m_iMiningRegrowthMinutes; }
	lpctstr GetShardIdentifier() const noexcept { return m_sShardIdentifier; }

	bool LoadKey(CScript & s);
	void OnStartupCheck();
	void OnWorldLoad();
	void OnWorldSave();

	void OnServerList(CClient * pClient) const;
	void OnPlayerStart(CClient * pClient);
	void OnCharMove(CChar * pChar);
	void OnCharMapChange(CChar * pChar);

	void OnClientDisconnect(CClient * pClient);
	void OnUltimaLiveVersion(CClient * pClient, word wMajor, word wMinor);

	void OnBlockQueryReply(CClient * pClient, dword dwCenterBlock, int iMapID, const word * pCRCs, size_t iCRCCount);

	bool ParseStaffCommand(CClient * pClient, lpctstr pszCommand);

	void InvalidateBlockCRC(int iMap, dword dwBlockId);
	void SendLocalUpdates(int iMap, int iBlockX, int iBlockY, UltimaLiveLocalUpdate flags);

	const UltimaLiveMapDef * GetMapDef(int iMap) const;
	bool IsMapRegistered(int iMap) const;

	// Block helpers (used by overlay / lumber / commands)
	static dword GetBlockId(int iMap, int iBlockX, int iBlockY);
	static void GetBlockXY(int iMap, dword dwBlockId, int & iBlockX, int & iBlockY);
	int GetBlockWidthInBlocks(int iMap) const;
	int GetBlockHeightInBlocks(int iMap) const;

	word GetBlockCRC(int iMap, dword dwBlockId, std::vector<byte> * pLandOut = nullptr, std::vector<byte> * pStaticsOut = nullptr);
	bool GetLandData(int iMap, int iBlockX, int iBlockY, byte land[192]);
	bool GetStaticsData(int iMap, int iBlockX, int iBlockY, std::vector<byte> & staticsOut);

	bool SetLandTile(int iMap, int x, int y, word wTerrainId, char z);
	bool SetStaticAt(int iMap, int x, int y, const struct CUOStaticItemRec & tile, bool fAdd);
	bool DeleteStaticAt(int iMap, int x, int y, word wTileId, char z);
	void SetBlockNotifySuppressed(bool fSuppress) { m_fSuppressBlockNotify = fSuppress; }
	void OnLumberjackSuccess(CChar * pChar, int x, int y, int z);
	void OnTreeHarvest(CChar * pChar, int x, int y, int z);
	bool ResolveLumberjackResourcePoint(const CPointMap & ptChop, CPointMap & ptResource) const;
	bool CanGraphicHarvestAt(const CPointMap & ptChop) const;
	bool TryGraphicHarvest(CChar * pChar, int chopX, int chopY);
	bool IsHarvestGraphicAt(const CPointMap & pt, ITEMID_TYPE id) const;
	void DeferTreeHarvest(CChar * pChar, int x, int y, int z, bool fGraphicOnly);
	void FlushDeferredTreeHarvest(CChar * pChar);

	bool CanMiningExcavateAt(const CPointMap & ptMine) const;
	bool TryMiningHarvest(CChar * pChar, int mineX, int mineY, int mineZ);
	bool IsMiningGraphicAt(const CPointMap & pt, ITEMID_TYPE id) const;

	CUltimaLive();

private:

	bool RemoveStaticFromBlock(int iMap, int x, int y, word wTileId, char z);
	void PushBlockToClient(CClient * pClient, int iMap, int iBlockX, int iBlockY);
	void NotifyBlockChange(int iMap, int iBlockX, int iBlockY, CChar * pOrigin);

	bool m_fEnabled = false;
	bool m_fStreaming = true;
	bool m_fDiscovery = false;
	bool m_fDiscoveryRevealOnLogin = true;
	bool m_fHarvest = true;
	bool m_fHarvestRegrowth = true;
	bool m_fMining = true;
	bool m_fMiningRegrowth = true;
	bool m_fSuppressBlockNotify = false;
	int m_iHarvestRegrowthMinutes = 1440;
	int m_iMiningRegrowthMinutes = 10080;
	int m_iMiningStrokes = 3;
	int m_iDiscoveryViewBlocks = 3;
	CSString m_sShardIdentifier;
	CSString m_sRootPath;
	CSString m_sClientFilesPath;
	CSString m_sLumberHarvestPath;
	CSString m_sMiningHarvestPath;

	UltimaLiveMapDef m_aMapDefs[MAP_SUPPORTED_QTY];

	struct BlockCRCRow;
	BlockCRCRow * m_apCRC[MAP_SUPPORTED_QTY];

	class CUltimaLiveOverlay * m_pOverlay;
	class CUltimaLiveLumber * m_pLumber;
	class CUltimaLiveHarvest * m_pHarvest;
	class CUltimaLiveMining * m_pMining;

	struct DeferredTreeHarvest
	{
		CUID uidChar;
		short x = 0;
		short y = 0;
		char z = 0;
		uchar iMap = 0;
		bool fGraphicOnly = false;
	};
	std::vector<DeferredTreeHarvest> m_DeferredHarvests;

	void ApplyDeferredTreeHarvest(CChar * pChar, int x, int y, int z, bool fGraphicOnly);

	void PushBlockUpdates(CClient * pClient, dword dwCenterBlock, int iMapID, const word * pReceivedCRCs);
	void QueryMobile(CChar * pChar, bool fForce = false);
	void EnsureCRCRow(int iMap);
	void ApplyDefaults();
	void EnsureDataDirectories() const;
	CSString GetClientFilesPath() const;
	CSString GetLumberHarvestPath() const;
	CSString GetMiningHarvestPath() const;
	CSString GetDiscoveryPath() const;
	void SendDiscoverySnapshot(CClient * pClient) const;
};

extern CUltimaLive g_UltimaLive;

#endif // _INC_CULTIMALIVE_H
