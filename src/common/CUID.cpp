#include "CUID.h"

bool CUID::IsValidUID(dword dwPrivateUID) noexcept
{
	return ( dwPrivateUID && ( dwPrivateUID & UID_O_INDEX_MASK ) != UID_O_INDEX_MASK );
}

bool CUID::IsResource(dword dwPrivateUID) noexcept
{
    return (dwPrivateUID & UID_F_RESOURCE);
}

bool CUID::IsValidResource(dword dwPrivateUID) noexcept
{
    return (IsResource(dwPrivateUID) && IsValidUID(dwPrivateUID));
}

bool CUID::IsItem(dword dwPrivateUID) noexcept
{
	return ((dwPrivateUID & (UID_F_RESOURCE | UID_F_ITEM)) == UID_F_ITEM);
}

bool CUID::IsChar(dword dwPrivateUID) noexcept
{
	if ( ( dwPrivateUID & (UID_F_RESOURCE|UID_F_ITEM)) == 0 )
		return IsValidUID(dwPrivateUID);
	return false;
}


bool CUID::IsItemEquipped() const noexcept
{
	if ( (m_dwInternalVal & (UID_F_RESOURCE|UID_F_ITEM|UID_O_DISCONNECT)) == (UID_F_ITEM|UID_O_EQUIPPED))
		return IsValidUID();
	return false;
}

bool CUID::IsItemInContainer() const noexcept
{
	if ( ( m_dwInternalVal & (UID_F_RESOURCE|UID_F_ITEM|UID_O_DISCONNECT)) == (UID_F_ITEM|UID_O_CONTAINED) )
		return IsValidUID();
	return false;
}

void CUID::SetObjContainerFlags( dword dwFlags ) noexcept
{
	m_dwInternalVal = (m_dwInternalVal & (UID_O_INDEX_MASK|UID_F_ITEM)) | dwFlags;
}

void CUID::RemoveObjFlags( dword dwFlags ) noexcept
{
    m_dwInternalVal &= ~dwFlags;
}

dword CUID::GetObjUID() const noexcept
{
	return ( m_dwInternalVal & (UID_O_INDEX_MASK|UID_F_ITEM) );
}

void CUID::SetObjUID( dword dwVal ) noexcept
{
	m_dwInternalVal = ( dwVal & (UID_O_INDEX_MASK|UID_F_ITEM) ) | UID_O_DISCONNECT;
}
