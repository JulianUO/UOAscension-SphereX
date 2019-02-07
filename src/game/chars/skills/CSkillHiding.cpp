#include <cmath>
#include "../../common/resource/blocks/CSkillClassDef.h"
#include "../../common/resource/blocks/CRegionResourceDef.h"
#include "../../common/resource/CResourceLock.h"
#include "../CLog.h"
#include "../CUIDExtra.h"
#include "src/game/CWorld.h"
#include "src/game/triggers.h"
#include "src/game/chars/CChar.h"
#include "src/game/chars/CCharNPC.h"
#include "src/game/clients/CClient.h"


int CChar::Skill_Hiding(SKTRIG_TYPE stage)
{
	ADDTOCALLSTACK("CChar::Skill_Hiding");

	if (stage == SKTRIG_SELECT)
	{
		// If we're in a fight and target can see me, can't hide.
		if (Fight_IsActive())
		{
			CChar * pCharFight = m_Fight_Targ_UID.CharFind();
			if (pCharFight->CanSeeLOS(this, LOS_NB_WINDOWS))
			{
				SysMessageDefault(DEFMSG_HIDING_INFIGHT);
				return -SKTRIG_QTY;
			}
		}
	}

	if (stage == SKTRIG_START)
	{
		// Make sure I'm not carrying a light.
		for (CItem *pItem = GetContentHead(); pItem != nullptr; pItem = pItem->GetNext())
		{
			if (!CItemBase::IsVisibleLayer(pItem->GetEquipLayer()))
				continue;
			if (pItem->Can(CAN_I_LIGHT))
			{
				SysMessageDefault(DEFMSG_HIDING_TOOLIT);
				return -SKTRIG_QTY;
			}
		}

		// If we're on a fight, make it more dificult.
		if (Fight_IsActive())
			return Calc_GetRandVal(95);
		else
			return Calc_GetRandVal(65);
	}

	if (stage == SKTRIG_STROKE)
		return 0;

	if (stage == SKTRIG_SUCCESS)
	{
		ObjMessage(g_Cfg.GetDefaultMsg(DEFMSG_HIDING_SUCCESS), this);
		StatFlag_Set(STATF_HIDDEN);
		Reveal(STATF_INVISIBLE);	// Clear previous invisibility spell effect, this will not reveal the char.
		UpdateMode();
		if (IsClient())
		{
			GetClient()->removeBuff(BI_HIDDEN);
			GetClient()->addBuff(BI_HIDDEN, 1075655, 1075656);
		}
		return 0;
	}

	if (stage == SKTRIG_FAIL)
	{
		Reveal();
		return 0;
	}

	ASSERT(0);
	return -SKTRIG_QTY;
}
