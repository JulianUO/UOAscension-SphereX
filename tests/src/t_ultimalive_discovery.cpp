/**
 * @file t_ultimalive_discovery.cpp
 * Unit tests for UltimaLive per-character map discovery.
 */

#include <catch2/catch_test_macros.hpp>

#include "../../src/game/ultimalive/CUltimaLive.h"
#include "../../src/game/ultimalive/CUltimaLiveDiscovery.h"
#include "../../src/common/CPointBase.h"

TEST_CASE("UltimaLive discovery reveal range", "[ultimalive][discovery]")
{
	CUltimaLiveDiscovery disc;
	disc.SetEnabled(true);
	disc.SetViewBlocks(3);

	CPointMap pt;
	pt.m_map = 0;
	pt.m_x = 100;
	pt.m_y = 100;

	const int charBX = pt.m_x / 8;
	const int charBY = pt.m_y / 8;

	CHECK(disc.IsBlockInRevealRange(0, charBX, charBY, pt));
	CHECK(disc.IsBlockInRevealRange(0, charBX + 3, charBY, pt));
	CHECK_FALSE(disc.IsBlockInRevealRange(0, charBX + 4, charBY, pt));
	CHECK_FALSE(disc.IsBlockInRevealRange(0, charBX, charBY + 4, pt));
	CHECK_FALSE(disc.IsBlockInRevealRange(1, charBX, charBY, pt));
}

TEST_CASE("UltimaLive discovery mark and query", "[ultimalive][discovery]")
{
	CUltimaLiveDiscovery disc;
	disc.SetEnabled(true);

	CHECK_FALSE(disc.IsDiscovered(0, 42));
	disc.MarkDiscovered(0, 42);
	CHECK(disc.IsDiscovered(0, 42));
	CHECK_FALSE(disc.IsDiscovered(0, 43));
	CHECK_FALSE(disc.IsDiscovered(1, 42));

	std::vector<dword> blocks;
	disc.CollectBlocksForMap(0, blocks);
	REQUIRE(blocks.size() == 1);
	CHECK(blocks[0] == 42);
}

TEST_CASE("UltimaLive discovery disabled passes through", "[ultimalive][discovery]")
{
	CUltimaLiveDiscovery disc;
	disc.SetEnabled(false);

	CPointMap pt;
	pt.m_map = 0;
	pt.m_x = 0;
	pt.m_y = 0;

	CHECK(disc.IsDiscovered(0, 999));
	CHECK(disc.IsBlockInRevealRange(0, 100, 100, pt));
}

TEST_CASE("UltimaLive discovery LoadKey", "[ultimalive][discovery]")
{
	g_UltimaLive.LoadKey(CSString("ULTIMALIVEDISCOVERY=1"));
	g_UltimaLive.LoadKey(CSString("ULTIMALIVEDISCOVERYVIEWBLOCKS=5"));
	g_UltimaLive.LoadKey(CSString("ULTIMALIVEDISCOVERYREVEALONLOGIN=0"));
	CHECK(g_UltimaLive.IsDiscoveryEnabled() == false); // UltimaLive not enabled
	g_UltimaLive.LoadKey(CSString("ULTIMALIVEENABLED=1"));
	CHECK(g_UltimaLive.IsDiscoveryEnabled());
	CHECK(g_UltimaLive.GetDiscoveryViewBlocks() == 5);
	CHECK_FALSE(g_UltimaLive.IsDiscoveryRevealOnLogin());
	g_UltimaLive.LoadKey(CSString("ULTIMALIVEENABLED=0"));
	g_UltimaLive.LoadKey(CSString("ULTIMALIVEDISCOVERY=0"));
}
