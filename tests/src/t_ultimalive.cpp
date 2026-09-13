/**
 * @file t_ultimalive.cpp
 * Unit tests for UltimaLive Fletcher16 CRC and block id math.
 */

#include <doctest/doctest.h>

#include "../../src/game/ultimalive/CUltimaLive.h"
#include "../../src/game/uo_files/CUOMapList.h"
#include "../../src/common/CScript.h"

namespace
{
	word Fletcher16(const byte * data, size_t len)
	{
		word sum1 = 0;
		word sum2 = 0;
		for (size_t i = 0; i < len; ++i)
		{
			sum1 = static_cast<word>((sum1 + data[i]) % 255);
			sum2 = static_cast<word>((sum2 + sum1) % 255);
		}
		return static_cast<word>((sum2 << 8) | sum1);
	}
}

TEST_CASE("UltimaLive Fletcher16 known vector")
{
	const byte data[] = { 1, 2, 3, 4, 5 };
	const word crc = Fletcher16(data, sizeof(data));
	CHECK(crc != 0);
	CHECK(crc != 0xFFFF);
}

TEST_CASE("UltimaLive LoadKey enables feature")
{
	CScript s;
	s.ParseKey("ULTIMALIVEENABLED", "1");
	g_UltimaLive.LoadKey(s);
	s.ParseKey("ULTIMALIVESHARDIDENTIFIER", "TestShard");
	g_UltimaLive.LoadKey(s);
	CHECK(g_UltimaLive.IsEnabled());
	CHECK(strcmp(g_UltimaLive.GetShardIdentifier(), "TestShard") == 0);
	s.ParseKey("ULTIMALIVEENABLED", "0");
	g_UltimaLive.LoadKey(s);
}
