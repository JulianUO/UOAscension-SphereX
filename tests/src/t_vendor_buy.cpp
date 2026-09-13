#include <doctest/doctest.h>
#include "../../src/network/VendorBuyHelper.h"

TEST_CASE("VendorBuyHelper allocates first slot")
{
    VendorItem items[3]{};
    uint index = 0;
    CUID serial(0x100);
    CHECK(VendorBuyHelper::FindOrAllocSlot(items, 3, serial, index));
    CHECK(index == 0);
    CHECK(items[0].m_serial == serial);
}

TEST_CASE("VendorBuyHelper fails when all slots used by different serials")
{
    VendorItem items[2]{};
    items[0].m_serial = CUID(0x100);
    items[1].m_serial = CUID(0x200);

    uint index = 0;
    CHECK_FALSE(VendorBuyHelper::FindOrAllocSlot(items, 2, CUID(0x300), index));
}

TEST_CASE("VendorBuyHelper reuses slot for matching serial")
{
    VendorItem items[2]{};
    items[0].m_serial = CUID(0x100);

    uint index = 99;
    CHECK(VendorBuyHelper::FindOrAllocSlot(items, 2, CUID(0x100), index));
    CHECK(index == 0);
}
