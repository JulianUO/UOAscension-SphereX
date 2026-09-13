#include <doctest/doctest.h>
#include "../../src/common/sphere_library/CSReferenceCount.h"

struct TestCountedData
{
    int value = 0;
};

TEST_CASE("CSReferenceCounted copy increments owner count")
{
    CSReferenceCountedOwned<TestCountedData> owner;
    owner._heldObj.value = 42;

    {
        CSReferenceCounted<TestCountedData> ref1 = owner.GetRef();
        CHECK(owner._counted_references == 2);

        CSReferenceCounted<TestCountedData> ref2(ref1);
        CHECK(owner._counted_references == 3);
    }

    CHECK(owner._counted_references == 1);
}

TEST_CASE("CSReferenceCounted assignment releases previous owner")
{
    CSReferenceCountedOwned<TestCountedData> ownerA;
    CSReferenceCountedOwned<TestCountedData> ownerB;

    CSReferenceCounted<TestCountedData> ref = ownerA.GetRef();
    CHECK(ownerA._counted_references == 2);
    CHECK(ownerB._counted_references == 1);

    ref = ownerB.GetRef();
    CHECK(ownerA._counted_references == 1);
    CHECK(ownerB._counted_references == 2);
}

TEST_CASE("CSReferenceCounted self-assignment is safe")
{
    CSReferenceCountedOwned<TestCountedData> owner;
    CSReferenceCounted<TestCountedData> ref = owner.GetRef();
    ref = ref;
    CHECK(owner._counted_references == 2);
}
