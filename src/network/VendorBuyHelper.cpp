#include "VendorBuyHelper.h"

bool VendorBuyHelper::FindOrAllocSlot(VendorItem* items, uint itemCount, const CUID& serial, uint& outIndex) noexcept
{
	for (outIndex = 0; outIndex < itemCount; ++outIndex)
	{
		if (serial == items[outIndex].m_serial)
			return true;
		if (!items[outIndex].m_serial.IsValidUID())
		{
			items[outIndex].m_serial = serial;
			return true;
		}
	}
	return false;
}
