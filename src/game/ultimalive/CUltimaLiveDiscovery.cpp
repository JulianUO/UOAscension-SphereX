/**
 * @file CUltimaLiveDiscovery.cpp
 */

#include "CUltimaLiveDiscovery.h"
#include "../../common/sphere_library/CSFile.h"
#include "../uo_files/CUOMapList.h"
#include <algorithm>
#include <cstdio>

namespace
{
	constexpr dword kDiscoveryMagic = 0x53444C55; // 'ULDS'
	constexpr word kDiscoveryVersion = 1;
}

void CUltimaLiveDiscovery::Clear()
{
	m_Blocks.clear();
}

bool CUltimaLiveDiscovery::IsDiscovered(int iMap, dword dwBlockId) const
{
	if (!m_fEnabled)
		return true;

	const auto itMap = m_Blocks.find(iMap);
	if (itMap == m_Blocks.end())
		return false;
	return itMap->second.find(dwBlockId) != itMap->second.end();
}

void CUltimaLiveDiscovery::MarkDiscovered(int iMap, dword dwBlockId)
{
	if (!m_fEnabled)
		return;
	m_Blocks[iMap].insert(dwBlockId);
}

bool CUltimaLiveDiscovery::IsBlockInRevealRange(int iMap, int iBlockX, int iBlockY, const CPointMap & ptChar) const
{
	if (!m_fEnabled)
		return true;
	if (ptChar.m_map != iMap)
		return false;

	const int charBX = ptChar.m_x / UO_BLOCK_SIZE;
	const int charBY = ptChar.m_y / UO_BLOCK_SIZE;
	const int distX = abs(iBlockX - charBX);
	const int distY = abs(iBlockY - charBY);
	const int dist = (distX > distY) ? distX : distY;
	return dist <= m_iViewBlocks;
}

void CUltimaLiveDiscovery::CollectBlocksForMap(int iMap, std::vector<dword> & out) const
{
	out.clear();
	const auto itMap = m_Blocks.find(iMap);
	if (itMap == m_Blocks.end())
		return;
	out.reserve(itMap->second.size());
	for (dword bid : itMap->second)
		out.push_back(bid);
}

void CUltimaLiveDiscovery::CollectBlocksNear(int iMap, int iBlockX, int iBlockY, int iRadiusBlocks, std::vector<dword> & out) const
{
	out.clear();
	const auto itMap = m_Blocks.find(iMap);
	if (itMap == m_Blocks.end())
		return;

	for (dword bid : itMap->second)
	{
		if (!g_MapList.IsMapSupported(iMap))
			continue;
		const int bh = g_MapList.GetMapSizeY(iMap) / UO_BLOCK_SIZE;
		if (bh <= 0)
			continue;
		const int bx = static_cast<int>(bid / bh);
		const int by = static_cast<int>(bid % bh);
		const int distX = abs(bx - iBlockX);
		const int distY = abs(by - iBlockY);
		const int dist = (distX > distY) ? distX : distY;
		if (dist <= iRadiusBlocks)
			out.push_back(bid);
	}
}

bool CUltimaLiveDiscovery::LoadForChar(dword dwCharUid, lpctstr pszDirectory)
{
	Clear();
	if (!pszDirectory || !*pszDirectory || dwCharUid == 0)
		return false;

	CSString sFile;
	sFile.Format("0x%x.bin", dwCharUid);
	CSString sFull = CSFile::GetMergedFileName(pszDirectory, sFile);

	CSFile file;
	if (!file.Open(sFull, OF_READ | OF_BINARY))
		return false;

	dword magic = 0;
	word version = 0;
	dword count = 0;
	if (file.Read(&magic, sizeof(magic)) <= 0)
		return false;
	if (file.Read(&version, sizeof(version)) <= 0)
		return false;
	if (file.Read(&count, sizeof(count)) <= 0)
		return false;

	if (magic != kDiscoveryMagic || version != kDiscoveryVersion)
		return false;

	for (dword i = 0; i < count; ++i)
	{
		byte bMap = 0;
		dword dwBlockId = 0;
		if (file.Read(&bMap, sizeof(bMap)) <= 0)
			break;
		if (file.Read(&dwBlockId, sizeof(dwBlockId)) <= 0)
			break;
		m_Blocks[bMap].insert(dwBlockId);
	}
	return true;
}

bool CUltimaLiveDiscovery::SaveForChar(dword dwCharUid, lpctstr pszDirectory) const
{
	if (!pszDirectory || !*pszDirectory || dwCharUid == 0 || m_Blocks.empty())
		return false;

	CSString sFile;
	sFile.Format("0x%x.bin", dwCharUid);
	CSString sFull = CSFile::GetMergedFileName(pszDirectory, sFile);

	CSFile file;
	if (!file.Open(sFull, OF_WRITE | OF_BINARY))
		return false;

	dword count = 0;
	for (const auto & kv : m_Blocks)
		count += static_cast<dword>(kv.second.size());

	const dword magic = kDiscoveryMagic;
	const word version = kDiscoveryVersion;
	file.Write(&magic, sizeof(magic));
	file.Write(&version, sizeof(version));
	file.Write(&count, sizeof(count));

	for (const auto & kv : m_Blocks)
	{
		const byte bMap = static_cast<byte>(kv.first);
		for (dword bid : kv.second)
		{
			file.Write(&bMap, sizeof(bMap));
			file.Write(&bid, sizeof(bid));
		}
	}
	file.Close();
	return true;
}
