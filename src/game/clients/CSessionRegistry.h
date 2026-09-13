/**
 * @file CSessionRegistry.h
 * @brief Short-lived login sessions registered by an external login server.
 */

#ifndef _INC_CSESSIONREGISTRY_H
#define _INC_CSESSIONREGISTRY_H

#include "../../common/sphere_library/CSString.h"
#include <map>
#include <mutex>

struct LoginSessionEntry
{
	CSString m_sAccount;
	dword m_dwClientVersion;
	dword m_dwReportedVersion;
	int64 m_iExpiresAtMs;
};

class CSessionRegistry
{
public:
	static CSessionRegistry& get() noexcept;

	bool RegisterSession(dword authId, lpctstr account, dword clientVersion, dword reportedVersion, int ttlSeconds);
	bool ConsumeSession(dword authId, lpctstr account, dword& clientVersion, dword& reportedVersion);
	void PurgeExpired();

private:
	CSessionRegistry() = default;

	std::mutex m_mutex;
	std::map<dword, LoginSessionEntry> m_sessions;
};

#endif // _INC_CSESSIONREGISTRY_H
