#include <doctest/doctest.h>
#include "../../src/common/CUID.h"

TEST_CASE("CUID invalid lookup returns null without crash")
{
    CUID uidInvalid;
    uidInvalid.InitUID();

    CHECK(uidInvalid.IsValidUID() == false);
    CHECK(uidInvalid.ObjFind() == nullptr);
    CHECK(uidInvalid.ItemFind() == nullptr);
    CHECK(uidInvalid.CharFind() == nullptr);
}

TEST_CASE("CUID zero is not valid")
{
    CUID uid;
    CHECK(uid.IsValidUID() == false);
}
