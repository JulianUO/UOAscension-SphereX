/**
* @file VendorBuyHelper.h
* @brief Shared vendor-buy slot resolution (used by receive.cpp and unit tests).
*/

#ifndef _INC_VENDORBUYHELPER_H
#define _INC_VENDORBUYHELPER_H

#include "../common/CUID.h"

struct VendorItem
{
	CUID m_serial;
	word m_vcAmount;
	dword m_price;
};

namespace VendorBuyHelper
{
	// Returns false if no slot available (index == itemCount on failure).
	bool FindOrAllocSlot(VendorItem* items, uint itemCount, const CUID& serial, uint& outIndex) noexcept;
}

#endif // _INC_VENDORBUYHELPER_H
