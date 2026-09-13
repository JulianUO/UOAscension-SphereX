/**
 * @file CUltimaLiveLumber.cpp
 */

#include "CUltimaLiveLumber.h"
#include "CUltimaLiveOverlay.h"
#include "../../common/CLog.h"
#include "../../common/sphere_library/CSFileList.h"
#include <cstdio>

void CUltimaLiveLumber::Load(lpctstr pszDirectory, CUltimaLiveOverlay & overlay)
{
	m_records.clear();
	if (!pszDirectory || !*pszDirectory)
		return;

	CSFileList files;
	CSString sPattern = CSFile::GetMergedFileName(pszDirectory, "*.lumber");
	if (files.ReadDir(sPattern, false) < 0)
		return;

	for (CSStringListRec * psFile = files.GetHead(); psFile; psFile = psFile->GetNext())
	{
		CSString sFull = CSFile::GetMergedFileName(pszDirectory, *psFile);
		CSFile file;
		if (!file.Open(sFull, OF_READ | OF_BINARY))
			continue;

		const size_t len = file.GetLength();
		if (len < 11 || (len % 11) != 0)
			continue;

		std::vector<byte> buf(len);
		if (file.Read(buf.data(), static_cast<int>(len)) <= 0)
			continue;

		for (size_t pos = 0; pos + 11 <= len; pos += 11)
		{
			LumberRecord rec;
			rec.iMap = buf[pos];
			rec.x = static_cast<int>(buf[pos + 1] | (buf[pos + 2] << 8));
			rec.y = static_cast<int>(buf[pos + 3] | (buf[pos + 4] << 8));
			rec.z = static_cast<int>(static_cast<signed char>(buf[pos + 5]));
			rec.wTileId = static_cast<word>(buf[pos + 6] | (buf[pos + 7] << 8));
			m_records.push_back(rec);
		}
	}

	ApplyToOverlay(overlay);
}

void CUltimaLiveLumber::Save(lpctstr pszDirectory) const
{
	if (!pszDirectory || !*pszDirectory || m_records.empty())
		return;

	CSString sFull = CSFile::GetMergedFileName(pszDirectory, "harvest.lumber");
	CSFile file;
	if (!file.Open(sFull, OF_WRITE | OF_BINARY))
		return;

	for (const LumberRecord & rec : m_records)
	{
		byte buf[11];
		buf[0] = static_cast<byte>(rec.iMap);
		buf[1] = static_cast<byte>(rec.x & 0xFF);
		buf[2] = static_cast<byte>((rec.x >> 8) & 0xFF);
		buf[3] = static_cast<byte>(rec.y & 0xFF);
		buf[4] = static_cast<byte>((rec.y >> 8) & 0xFF);
		buf[5] = static_cast<byte>(rec.z);
		buf[6] = static_cast<byte>(rec.wTileId & 0xFF);
		buf[7] = static_cast<byte>((rec.wTileId >> 8) & 0xFF);
		buf[8] = 0;
		buf[9] = 0;
		buf[10] = 0;
		file.Write(buf, sizeof(buf));
	}
	file.Close();
}

void CUltimaLiveLumber::RecordTreeFelled(int iMap, int x, int y, int z, word wTileId)
{
	for (const LumberRecord & rec : m_records)
	{
		if (rec.iMap == iMap && rec.x == x && rec.y == y && rec.z == z)
			return;
	}
	LumberRecord rec;
	rec.iMap = iMap;
	rec.x = x;
	rec.y = y;
	rec.z = z;
	rec.wTileId = wTileId;
	m_records.push_back(rec);
}

void CUltimaLiveLumber::ApplyToOverlay(CUltimaLiveOverlay & overlay) const
{
	for (const LumberRecord & rec : m_records)
	{
		std::vector<CUOStaticItemRec> at;
		if (!overlay.GetStaticsAt(rec.iMap, rec.x, rec.y, at))
			continue;

		std::vector<CUOStaticItemRec> kept;
		for (const CUOStaticItemRec & st : at)
		{
			if (st.m_wTileID == rec.wTileId && st.m_z == static_cast<char>(rec.z))
				continue;
			kept.push_back(st);
		}
		overlay.SetStaticsAt(rec.iMap, rec.x, rec.y, kept);
	}
}
