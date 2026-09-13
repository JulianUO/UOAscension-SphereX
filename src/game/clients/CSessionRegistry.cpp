#include "../../common/sphere_library/CSTime.h"
#include "CSessionRegistry.h"

CSessionRegistry& CSessionRegistry::get() noexcept
{
	static CSessionRegistry registry;
	return registry;
}

void CSessionRegistry::PurgeExpired()
{
	const int64 iNow = CSTime::GetMonotonicSysTimeMilli();
	std::lock_guard lock(m_mutex);
	for (auto it = m_sessions.begin(); it != m_sessions.end(); )
	{
		if (it->second.m_iExpiresAtMs <= iNow)
			it = m_sessions.erase(it);
		else
			++it;
	}
}

bool CSessionRegistry::RegisterSession(dword authId, lpctstr account, dword clientVersion, dword reportedVersion, int ttlSeconds)
{
	if (!account || !account[0] || authId == 0)
		return false;

	if (ttlSeconds <= 0)
		ttlSeconds = 30;
	if (ttlSeconds > 300)
		ttlSeconds = 300;

	PurgeExpired();

	LoginSessionEntry entry;
	entry.m_sAccount = account;
	entry.m_dwClientVersion = clientVersion;
	entry.m_dwReportedVersion = reportedVersion;
	entry.m_iExpiresAtMs = CSTime::GetMonotonicSysTimeMilli() + (static_cast<int64>(ttlSeconds) * 1000);

	std::lock_guard lock(m_mutex);
	m_sessions[authId] = std::move(entry);
	return true;
}

bool CSessionRegistry::ConsumeSession(dword authId, lpctstr account, dword& clientVersion, dword& reportedVersion)
{
	if (!account || !account[0] || authId == 0)
		return false;

	PurgeExpired();

	std::lock_guard lock(m_mutex);
	const auto it = m_sessions.find(authId);
	if (it == m_sessions.end())
		return false;

	if (it->second.m_sAccount.CompareNoCase(account) != 0)
		return false;

	clientVersion = it->second.m_dwClientVersion;
	reportedVersion = it->second.m_dwReportedVersion;
	m_sessions.erase(it);
	return true;
}
