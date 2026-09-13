/**
* @file CUIDGame.cpp
* @brief UID object lookup — game-layer implementation (depends on g_World).
*/

#include "../game/chars/CChar.h"
#include "../game/items/CItem.h"
#include "../game/CWorld.h"
#include "../common/CUID.h"

CObjBase * CUID::ObjFindFromUID(dword dwPrivateUID, bool fInvalidateBeingDeleted) noexcept
{
    if ( IsResource(dwPrivateUID) || !IsValidUID(dwPrivateUID) )
        return nullptr;

	CObjBase *pObj = g_World.FindUID(dwPrivateUID & UID_O_INDEX_MASK);

	if (fInvalidateBeingDeleted && (!pObj || pObj->_IsBeingDeleted()))
		return nullptr;
	return pObj;
}

CItem * CUID::ItemFindFromUID(dword dwPrivateUID, bool fInvalidateBeingDeleted) noexcept
{
    return dynamic_cast<CItem *>(ObjFindFromUID(dwPrivateUID, fInvalidateBeingDeleted));
}

CChar * CUID::CharFindFromUID(dword dwPrivateUID, bool fInvalidateBeingDeleted) noexcept
{
    return dynamic_cast<CChar *>(ObjFindFromUID(dwPrivateUID, fInvalidateBeingDeleted));
}
