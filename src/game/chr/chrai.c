/**
 * The AI script interpreter. aiRunList() runs a chr's (or object's) AI list from its saved
 * offset until a command yields. Each command is one case of the switch in aiRunList(), with
 * its record layout in bondaicommands.h; aiCommandSize gives each command's length.
 * Both are written out by hand: src/aicommands.def only builds the AI_CMD enum
 * (bondconstants.h). Co-op and counter-op handling is in chraicoop.c. Also the AI
 * sound slots. See docs/subsystems/guards.md.
 */
#include "game/audio/sfxvolume.h"
#include "game/chr/chrai.h"
#include "game/chr/chraicoop.h"
#include "game/world/bg.h"
#include "game/world/bgfog.h"
#include "game/player/inventory.h"
#include "game/menu/cheat.h"
#include "game/menu/front.h"
#include "game/chr/chr.h"
#include "game/chr/chraction.h"
#include "game/chr/chraidata.h"
#include "game/save/file.h"
#include "game/player/gun.h"
#include "game/model/anim.h"
#include "game/text/language.h"
#include "game/prop/setup.h"
#include "game/level/lv.h"
#include "game/math/trig.h"
#include "game/math/round.h"
#include "game/model/model.h"
#include "game/audio/levelmusic.h"
#include "game/model/model.h"
#include "game/model/jointlist.h"
#include "game/model/modelfile.h"
#include "game/mission/objective.h"
#include "game/player/player.h"
#include "game/hud/hud.h"
#include "game/prop/propobj.h"
#include "game/prop/door.h"
#include "game/nav/stan.h"
#include <assert.h>
#include <bondaicommands.h>
#include <bondgame.h>
#include <bondtypes.h>
#include <boss.h>
#include <limits.h>
#include <music.h>
#include <random.h>
#include <snd.h>
#include <ultra64.h>
#include "game/prop/alarm.h"
#include "game/prop/monitor.h"
#include "game/prop/projectile.h"
#include "game/level/gamemode.h"
#include "game/mission/mission.h"

AiSound g_AiSounds[AI_SOUND_SLOTS];

/**
 * Keeps an AI sound slot's volume up to date while it plays: follows its object or pad
 * if it has one, fades towards the target volume, and is silent while the controls are
 * locked (cutscenes).
 */
static void aiSoundTick(s32 slot)
{
    AiSound *sfx = &g_AiSounds[slot];
    s32 volume;
    s32 ticks;

    if (sfx->state && sndGetPlayingState(sfx->state))
    {
        if (sfx->pos)
        {
            sfx->targetVolume = sfxVolumeAt(sfx->pos);
        }
        else if (sfx->obj && sfx->obj->prop)
        {
            sfx->targetVolume = sfxVolumeAt(&sfx->obj->runtime_pos);
        }

        volume = sfx->targetVolume;

        if (sfx->fadeTime >= 0)
        {
            ticks = g_ClockTimer;

            if (ticks < sfx->fadeTime)
            {
                volume = (((sfx->targetVolume - sfx->volume) * ticks) / sfx->fadeTime) + sfx->volume;
            }

            sfx->fadeTime -= ticks;
        }

        if (lvGetControlsLocked())
        {
            volume = 0;
        }

        if (volume != sfx->volume)
        {
            sndCreatePostEvent(sfx->state, 8, volume);
            sfx->volume = volume;
        }

        return;
    }

    sfx->volume = 0;
}

void aiSoundsTick(void)
{
    s32 i;

    for (i = 0; i < AI_SOUND_SLOTS; i++)
    {
        aiSoundTick(i);
    }
}

/**
 * Plays a sound, in a slot (so scripts can change its volume or stop it) if the slot
 * is free, otherwise without one.
 */
static void aiSoundPlay(s32 slot, s16 soundIndex)
{
    AiSound *sfx = NULL;

    if (slot >= 0 && slot < AI_SOUND_SLOTS)
    {
        if (!g_AiSounds[slot].state || !sndGetPlayingState(g_AiSounds[slot].state))
        {
            sfx = &g_AiSounds[slot];

            sfx->targetVolume = SHRT_MAX;
            sfx->volume = SHRT_MAX;
            sfx->fadeTime = -1;
            sfx->pos = NULL;
            sfx->obj = NULL;
        }
    }

    sndPlaySfx(g_musicSfxBufferPtr, soundIndex, sfx != NULL ? &sfx->state : NULL);
}

static void aiSoundStop(s32 slot)
{
    if ((slot >= 0) && (slot < AI_SOUND_SLOTS))
    {
        sndDeactivate(g_AiSounds[slot].state);
    }
}

/**
 * A fade time from a script (60ths of a second) in clock ticks.
 */
static s32 aiSoundFadeTicks(u16 time60)
{
#ifdef VERSION_EU
    return (time60 * 50) / 60;
#else
    return time60;
#endif
}

/**
 * Get AI Command Size in bytes
 * @param AIList: u8 Pointer to The AI list containing the command
 * @param offset: The offset (in bytes) to the command you want the size of
 * @return The number of bytes of AI command
 */
s32 aiCommandSize(AIRecord *list, s32 offset)
{
    u8 *AIList = (u8 *)list;

    switch (AIList[offset])
    {
        case AI_GotoNext:
            return sizeof(AiGotoNextRecord);
        case AI_GotoFirst:
            return sizeof(AiGotoFirstRecord);
        case AI_Label:
            return sizeof(AiLabelRecord);
        case AI_Yield:
            return sizeof(AiYieldRecord);
        case AI_EndList:
            return sizeof(AiEndListRecord);
        case AI_SetChrAiList:
            return sizeof(AiSetChrAiListRecord);
        case AI_SetReturnAiList:
            return sizeof(AiSetReturnAiListRecord);
        case AI_Return:
            return sizeof(AiReturnRecord);
        case AI_Stop:
            return sizeof(AiStopRecord);
        case AI_Kneel:
            return sizeof(AiKneelRecord);
        case AI_PlayAnimation:
            return sizeof(AiPlayAnimationRecord);
        case AI_IFPlayingAnimation:
            return sizeof(AiIFPlayingAnimationRecord);
        case AI_PointAtBond:
            return sizeof(AiPointAtBondRecord);
        case AI_LookSurprised:
            return sizeof(AiLookSurprisedRecord);
        case AI_TRYSidestepping:
            return sizeof(AiTRYSidesteppingRecord);
        case AI_TRYSideHopping:
            return sizeof(AiTRYSideHoppingRecord);
        case AI_TRYSideRunning:
            return sizeof(AiTRYSideRunningRecord);
        case AI_TRYFiringWalk:
            return sizeof(AiTRYFiringWalkRecord);
        case AI_TRYFiringRun:
            return sizeof(AiTRYFiringRunRecord);
        case AI_TRYFiringRoll:
            return sizeof(AiTRYFiringRollRecord);
        case AI_TRYFireOrAimAtTarget:
            return sizeof(AiTRYFireOrAimAtTargetRecord);
        case AI_TRYFireOrAimAtTargetKneel:
            return sizeof(AiTRYFireOrAimAtTargetKneelRecord);
        case AI_IFImFiring: /* enum = 232 despite following enum = 21 in the list */
            return sizeof(AiIFImFiringRecord);
        case AI_IFImFiringAndLockedForward: /* enum = 231 despite being followed by enum = 22 in the list */
            return sizeof(AiIFImFiringAndLockedForwardRecord);
        case AI_TRYFireOrAimAtTargetUpdate:
            return sizeof(AiTRYFireOrAimAtTargetUpdateRecord);
        case AI_TRYFacingTarget:
            return sizeof(AiTRYFacingTargetRecord);
        case AI_HitChrWithItem:
            return sizeof(AiHitChrWithItemRecord);
        case AI_ChrHitChr:
            return sizeof(AiChrHitChrRecord);
        case AI_TRYThrowingGrenade:
            return sizeof(AiTRYThrowingGrenadeRecord);
        case AI_TRYDroppingItem:
            return sizeof(AiTRYDroppingItemRecord);
        case AI_RunToPad:
            return sizeof(AiRunToPadRecord);
        case AI_RunToPadPreset:
            return sizeof(AiRunToPadPresetRecord);
        case AI_WalkToPad:
            return sizeof(AiWalkToPadRecord);
        case AI_SprintToPad:
            return sizeof(AiSprintToPadRecord);
        case AI_StartPatrol:
            return sizeof(AiStartPatrolRecord);
        case AI_Surrender:
            return sizeof(AiSurrenderRecord);
        case AI_RemoveMe:
            return sizeof(AiRemoveMeRecord);
        case AI_ChrRemoveInstant:
            return sizeof(AiChrRemoveInstantRecord);
        case AI_TRYTriggeringAlarmAtPad:
            return sizeof(AiTRYTriggeringAlarmAtPadRecord);
        case AI_AlarmOn:
            return sizeof(AiAlarmOnRecord);
        case AI_AlarmOff:
            return sizeof(AiAlarmOffRecord);
        case AI_TRYRunFromBond:
            return sizeof(AiTRYRunFromBondRecord);
        case AI_TRYRunToBond:
            return sizeof(AiTRYRunToBondRecord);
        case AI_TRYWalkToBond:
            return sizeof(AiTRYWalkToBondRecord);
        case AI_TRYSprintToBond:
            return sizeof(AiTRYSprintToBondRecord);
        case AI_TRYFindCover:
            return sizeof(AiTRYFindCoverRecord);
        case AI_TRYRunToChr:
            return sizeof(AiTRYRunToChrRecord);
        case AI_TRYWalkToChr:
            return sizeof(AiTRYWalkToChrRecord);
        case AI_TRYSprintToChr:
            return sizeof(AiTRYSprintToChrRecord);
        case AI_IFImOnPatrolOrStopped:
            return sizeof(AiIFImOnPatrolOrStoppedRecord);
        case AI_IFChrDyingOrDead:
            return sizeof(AiIFChrDyingOrDeadRecord);
        case AI_IFChrDoesNotExist:
            return sizeof(AiIFChrDoesNotExistRecord);
        case AI_IFISeeBond:
            return sizeof(AiIFISeeBondRecord);
        case AI_SetNewRandom:
            return sizeof(AiSetNewRandomRecord);
        case AI_IFRandomLessThan:
            return sizeof(AiIFRandomLessThanRecord);
        case AI_IFRandomGreaterThan:
            return sizeof(AiIFRandomGreaterThanRecord);
        case AI_IFICanHearAlarm:
            return sizeof(AiIFICanHearAlarmRecord);
        case AI_IFAlarmIsOn:
            return sizeof(AiIFAlarmIsOnRecord);
        case AI_IFGasIsLeaking:
            return sizeof(AiIFGasIsLeakingRecord);
        case AI_IFIHeardBond:
            return sizeof(AiIFIHeardBondRecord);
        case AI_IFISeeSomeoneShot:
            return sizeof(AiIFISeeSomeoneShotRecord);
        case AI_IFISeeSomeoneDie:
            return sizeof(AiIFISeeSomeoneDieRecord);
        case AI_IFICouldSeeBond:
            return sizeof(AiIFICouldSeeBondRecord);
        case AI_IFICouldSeeBondsStan:
            return sizeof(AiIFICouldSeeBondsStanRecord);
        case AI_IFIWasShotRecently:
            return sizeof(AiIFIWasShotRecentlyRecord);
        case AI_IFIHeardBondRecently:
            return sizeof(AiIFIHeardBondRecentlyRecord);
        case AI_IFImInRoomWithChr:
            return sizeof(AiIFImInRoomWithChrRecord);
        case AI_IFIveNotBeenSeen:
            return sizeof(AiIFIveNotBeenSeenRecord);
        case AI_IFImOnScreen:
            return sizeof(AiIFImOnScreenRecord);
        case AI_IFMyRoomIsOnScreen:
            return sizeof(AiIFMyRoomIsOnScreenRecord);
        case AI_IFRoomWithPadIsOnScreen:
            return sizeof(AiIFRoomWithPadIsOnScreenRecord);
        case AI_IFImTargetedByBond:
            return sizeof(AiIFImTargetedByBondRecord);
        case AI_IFBondMissedMe:
            return sizeof(AiIFBondMissedMeRecord);
        case AI_IFMyAngleToBondLessThan:
            return sizeof(AiIFMyAngleToBondLessThanRecord);
        case AI_IFMyAngleToBondGreaterThan:
            return sizeof(AiIFMyAngleToBondGreaterThanRecord);
        case AI_IFMyAngleFromBondLessThan:
            return sizeof(AiIFMyAngleFromBondLessThanRecord);
        case AI_IFMyAngleFromBondGreaterThan:
            return sizeof(AiIFMyAngleFromBondGreaterThanRecord);
        case AI_IFMyDistanceToBondLessThanDecimeter:
            return sizeof(AiIFMyDistanceToBondLessThanDecimeterRecord);
        case AI_IFMyDistanceToBondGreaterThanDecimeter:
            return sizeof(AiIFMyDistanceToBondGreaterThanDecimeterRecord);
        case AI_IFChrDistanceToPadLessThanDecimeter:
            return sizeof(AiIFChrDistanceToPadLessThanDecimeterRecord);
        case AI_IFChrDistanceToPadGreaterThanDecimeter:
            return sizeof(AiIFChrDistanceToPadGreaterThanDecimeterRecord);
        case AI_IFMyDistanceToChrLessThanDecimeter:
            return sizeof(AiIFMyDistanceToChrLessThanDecimeterRecord);
        case AI_IFMyDistanceToChrGreaterThanDecimeter:
            return sizeof(AiIFMyDistanceToChrGreaterThanDecimeterRecord);
        case AI_TRYSettingMyPresetToChrWithinDistanceDecimeter:
            return sizeof(AiTRYSettingMyPresetToChrWithinDistanceDecimeterRecord);
        case AI_IFBondDistanceToPadLessThanDecimeter:
            return sizeof(AiIFBondDistanceToPadLessThanDecimeterRecord);
        case AI_IFBondDistanceToPadGreaterThanDecimeter:
            return sizeof(AiIFBondDistanceToPadGreaterThanDecimeterRecord);
        case AI_IFChrInRoomWithPad:
            return sizeof(AiIFChrInRoomWithPadRecord);
        case AI_IFBondInRoomWithPad:
            return sizeof(AiIFBondInRoomWithPadRecord);
        case AI_IFBondCollectedObject:
            return sizeof(AiIFBondCollectedObjectRecord);
        case AI_IFKeyDropped:
            return sizeof(AiIFKeyDroppedRecord);
        case AI_IFItemIsAttachedToObject:
            return sizeof(AiIFItemIsAttachedToObjectRecord);
        case AI_IFBondHasItemEquipped:
            return sizeof(AiIFBondHasItemEquippedRecord);
        case AI_IFObjectExists:
            return sizeof(AiIFObjectExistsRecord);
        case AI_IFObjectNotDestroyed:
            return sizeof(AiIFObjectNotDestroyedRecord);
        case AI_IFObjectWasActivated:
            return sizeof(AiIFObjectWasActivatedRecord);
        case AI_IFBondUsedGadgetOnObject:
            return sizeof(AiIFBondUsedGadgetOnObjectRecord);
        case AI_ActivateObject:
            return sizeof(AiActivateObjectRecord);
        case AI_DestroyObject:
            return sizeof(AiDestroyObjectRecord);
        case AI_DropObject:
            return sizeof(AiDropObjectRecord);
        case AI_ChrDropAllConcealedItems:
            return sizeof(AiChrDropAllConcealedItemsRecord);
        case AI_ChrDropAllHeldItems:
            return sizeof(AiChrDropAllHeldItemsRecord);
        case AI_BondCollectObject:
            return sizeof(AiBondCollectObjectRecord);
        case AI_ChrEquipObject:
            return sizeof(AiChrEquipObjectRecord);
        case AI_MoveObject:
            return sizeof(AiMoveObjectRecord);
        case AI_DoorOpen:
            return sizeof(AiDoorOpenRecord);
        case AI_DoorClose:
            return sizeof(AiDoorCloseRecord);
        case AI_IFDoorStateEqual:
            return sizeof(AiIFDoorStateEqualRecord);
        case AI_IFDoorHasBeenOpenedBefore:
            return sizeof(AiIFDoorHasBeenOpenedBeforeRecord);
        case AI_DoorSetLock:
            return sizeof(AiDoorSetLockRecord);
        case AI_DoorUnsetLock:
            return sizeof(AiDoorUnsetLockRecord);
        case AI_IFDoorLockEqual:
            return sizeof(AiIFDoorLockEqualRecord);
        case AI_IFObjectiveNumComplete:
            return sizeof(AiIFObjectiveNumCompleteRecord);
        case AI_TRYUnknown6e:
            return sizeof(AiTRYUnknown6eRecord);
        case AI_TRYUnknown6f:
            return sizeof(AiTRYUnknown6fRecord);
        case AI_IFGameDifficultyLessThan:
            return sizeof(AiIFGameDifficultyLessThanRecord);
        case AI_IFGameDifficultyGreaterThan:
            return sizeof(AiIFGameDifficultyGreaterThanRecord);
        case AI_IFMissionTimeLessThan:
            return sizeof(AiIFMissionTimeLessThanRecord);
        case AI_IFMissionTimeGreaterThan:
            return sizeof(AiIFMissionTimeGreaterThanRecord);
        case AI_IFSystemPowerTimeLessThan:
            return sizeof(AiIFSystemPowerTimeLessThanRecord);
        case AI_IFSystemPowerTimeGreaterThan:
            return sizeof(AiIFSystemPowerTimeGreaterThanRecord);
        case AI_IFLevelIdLessThan:
            return sizeof(AiIFLevelIdLessThanRecord);
        case AI_IFLevelIdGreaterThan:
            return sizeof(AiIFLevelIdGreaterThanRecord);
        case AI_IFMyNumArghsLessThan:
            return sizeof(AiIFMyNumArghsLessThanRecord);
        case AI_IFMyNumArghsGreaterThan:
            return sizeof(AiIFMyNumArghsGreaterThanRecord);
        case AI_IFMyNumCloseArghsLessThan:
            return sizeof(AiIFMyNumCloseArghsLessThanRecord);
        case AI_IFMyNumCloseArghsGreaterThan:
            return sizeof(AiIFMyNumCloseArghsGreaterThanRecord);
        case AI_IFChrHealthLessThan:
            return sizeof(AiIFChrHealthLessThanRecord);
        case AI_IFChrHealthGreaterThan:
            return sizeof(AiIFChrHealthGreaterThanRecord);
        case AI_IFChrWasDamagedSinceLastCheck:
            return sizeof(AiIFChrWasDamagedSinceLastCheckRecord);
        case AI_IFBondHealthLessThan:
            return sizeof(AiIFBondHealthLessThanRecord);
        case AI_IFBondHealthGreaterThan:
            return sizeof(AiIFBondHealthGreaterThanRecord);
        case AI_SetMyMorale:
            return sizeof(AiSetMyMoraleRecord);
        case AI_AddToMyMorale:
            return sizeof(AiAddToMyMoraleRecord);
        case AI_SubtractFromMyMorale:
            return sizeof(AiSubtractFromMyMoraleRecord);
        case AI_IFMyMoraleLessThan:
            return sizeof(AiIFMyMoraleLessThanRecord);
        case AI_IFMyMoraleLessThanRandom:
            return sizeof(AiIFMyMoraleLessThanRandomRecord);
        case AI_SetMyAlertness:
            return sizeof(AiSetMyAlertnessRecord);
        case AI_AddToMyAlertness:
            return sizeof(AiAddToMyAlertnessRecord);
        case AI_SubtractFromMyAlertness:
            return sizeof(AiSubtractFromMyAlertnessRecord);
        case AI_IFMyAlertnessLessThan:
            return sizeof(AiIFMyAlertnessLessThanRecord);
        case AI_IFMyAlertnessLessThanRandom:
            return sizeof(AiIFMyAlertnessLessThanRandomRecord);
        case AI_SetMyHearingScale:
            return sizeof(AiSetMyHearingScaleRecord);
        case AI_SetMyVisionRange:
            return sizeof(AiSetMyVisionRangeRecord);
        case AI_SetMyGrenadeProbability:
            return sizeof(AiSetMyGrenadeProbabilityRecord);
        case AI_SetMyChrNum:
            return sizeof(AiSetMyChrNumRecord);
        case AI_SetMyHealthTotal:
            return sizeof(AiSetMyHealthTotalRecord);
        case AI_SetMyArmour:
            return sizeof(AiSetMyArmourRecord);
        case AI_SetMySpeedRating:
            return sizeof(AiSetMySpeedRatingRecord);
        case AI_SetMyArghRating:
            return sizeof(AiSetMyArghRatingRecord);
        case AI_SetMyAccuracyRating:
            return sizeof(AiSetMyAccuracyRatingRecord);
        case AI_SetMyFlags2:
            return sizeof(AiSetMyFlags2Record);
        case AI_UnsetMyFlags2:
            return sizeof(AiUnsetMyFlags2Record);
        case AI_IFMyFlags2Has:
            return sizeof(AiIFMyFlags2HasRecord);
        case AI_SetChrBitfield:
            return sizeof(AiSetChrBitfieldRecord);
        case AI_UnsetChrBitfield:
            return sizeof(AiUnsetChrBitfieldRecord);
        case AI_IFChrBitfieldHas:
            return sizeof(AiIFChrBitfieldHasRecord);
        case AI_SetObjectiveBitfield:
            return sizeof(AiSetObjectiveBitfieldRecord);
        case AI_UnsetObjectiveBitfield:
            return sizeof(AiUnsetObjectiveBitfieldRecord);
        case AI_IFObjectiveBitfieldHas:
            return sizeof(AiIFObjectiveBitfieldHasRecord);
        case AI_SetMychrflags:
            return sizeof(AiSetMychrflagsRecord);
        case AI_UnsetMychrflags:
            return sizeof(AiUnsetMychrflagsRecord);
        case AI_IFMychrflagsHas:
            return sizeof(AiIFMychrflagsHasRecord);
        case AI_SetChrchrflags:
            return sizeof(AiSetChrchrflagsRecord);
        case AI_UnsetChrchrflags:
            return sizeof(AiUnsetChrchrflagsRecord);
        case AI_IFChrchrflagsHas:
            return sizeof(AiIFChrchrflagsHasRecord);
        case AI_SetObjectFlags:
            return sizeof(AiSetObjectFlagsRecord);
        case AI_UnsetObjectFlags:
            return sizeof(AiUnsetObjectFlagsRecord);
        case AI_IFObjectFlagsHas:
            return sizeof(AiIFObjectFlagsHasRecord);
        case AI_SetObjectFlags2:
            return sizeof(AiSetObjectFlags2Record);
        case AI_UnsetObjectFlags2:
            return sizeof(AiUnsetObjectFlags2Record);
        case AI_IFObjectFlags2Has:
            return sizeof(AiIFObjectFlags2HasRecord);
        case AI_SetMyChrPreset:
            return sizeof(AiSetMyChrPresetRecord);
        case AI_SetChrChrPreset:
            return sizeof(AiSetChrChrPresetRecord);
        case AI_SetMyPadPreset:
            return sizeof(AiSetMyPadPresetRecord);
        case AI_SetChrPadPreset:
            return sizeof(AiSetChrPadPresetRecord);
        case AI_MyTimerStart:
            return sizeof(AiMyTimerStartRecord);
        case AI_MyTimerReset:
            return sizeof(AiMyTimerResetRecord);
        case AI_MyTimerPause:
            return sizeof(AiMyTimerPauseRecord);
        case AI_MyTimerResume:
            return sizeof(AiMyTimerResumeRecord);
        case AI_IFMyTimerIsNotRunning:
            return sizeof(AiIFMyTimerIsNotRunningRecord);
        case AI_IFMyTimerLessThanTicks:
            return sizeof(AiIFMyTimerLessThanTicksRecord);
        case AI_IFMyTimerGreaterThanTicks:
            return sizeof(AiIFMyTimerGreaterThanTicksRecord);
        case AI_HudCountdownShow:
            return sizeof(AiHudCountdownShowRecord);
        case AI_HudCountdownHide:
            return sizeof(AiHudCountdownHideRecord);
        case AI_HudCountdownSet:
            return sizeof(AiHudCountdownSetRecord);
        case AI_HudCountdownStop:
            return sizeof(AiHudCountdownStopRecord);
        case AI_HudCountdownStart:
            return sizeof(AiHudCountdownStartRecord);
        case AI_IFHudCountdownIsNotRunning:
            return sizeof(AiIFHudCountdownIsNotRunningRecord);
        case AI_IFHudCountdownLessThan:
            return sizeof(AiIFHudCountdownLessThanRecord);
        case AI_IFHudCountdownGreaterThan:
            return sizeof(AiIFHudCountdownGreaterThanRecord);
        case AI_TRYSpawningChrAtPad:
            return sizeof(AiTRYSpawningChrAtPadRecord);
        case AI_TRYSpawningChrNextToChr:
            return sizeof(AiTRYSpawningChrNextToChrRecord);
        case AI_TRYGiveMeItem:
            return sizeof(AiTRYGiveMeItemRecord);
        case AI_TRYGiveMeHat:
            return sizeof(AiTRYGiveMeHatRecord);
        case AI_TRYCloningChr:
            return sizeof(AiTRYCloningChrRecord);
        case AI_TextPrintBottom:
            return sizeof(AiTextPrintBottomRecord);
        case AI_TextPrintTop:
            return sizeof(AiTextPrintTopRecord);
        case AI_SfxPlay:
            return sizeof(AiSfxPlayRecord);
        case AI_SfxEmitFromObject:
            return sizeof(AiSfxEmitFromObjectRecord);
        case AI_SfxEmitFromPad:
            return sizeof(AiSfxEmitFromPadRecord);
        case AI_SfxSetChannelVolume:
            return sizeof(AiSfxSetChannelVolumeRecord);
        case AI_SfxFadeChannelVolume:
            return sizeof(AiSfxFadeChannelVolumeRecord);
        case AI_SfxStopChannel:
            return sizeof(AiSfxStopChannelRecord);
        case AI_IFSfxChannelVolumeLessThan:
            return sizeof(AiIFSfxChannelVolumeLessThanRecord);
        case AI_VehicleStartPath:
            return sizeof(AiVehicleStartPathRecord);
        case AI_VehicleSpeed:
            return sizeof(AiVehicleSpeedRecord);
        case AI_AircraftRotorSpeed:
            return sizeof(AiAircraftRotorSpeedRecord);
        case AI_IFCameraIsInIntro:
            return sizeof(AiIFCameraIsInIntroRecord);
        case AI_IFCameraIsInBondSwirl:
            return sizeof(AiIFCameraIsInBondSwirlRecord);
        case AI_TvChangeScreenBank:
            return sizeof(AiTvChangeScreenBankRecord);
        case AI_IFBondInTank:
            return sizeof(AiIFBondInTankRecord);
        case AI_EndLevel:
            return sizeof(AiEndLevelRecord);
        case AI_CameraReturnToBond:
            return sizeof(AiCameraReturnToBondRecord);
        case AI_CameraLookAtBondFromPad:
            return sizeof(AiCameraLookAtBondFromPadRecord);
        case AI_CameraSwitch:
            return sizeof(AiCameraSwitchRecord);
        case AI_IFBondYPosLessThan:
            return sizeof(AiIFBondYPosLessThanRecord);
        case AI_BondDisableControl:
            return sizeof(AiBondDisableControlRecord);
        case AI_BondEnableControl:
            return sizeof(AiBondEnableControlRecord);
        case AI_TRYTeleportingChrToPad:
            return sizeof(AiTRYTeleportingChrToPadRecord);
        case AI_ScreenFadeToBlack:
            return sizeof(AiScreenFadeToBlackRecord);
        case AI_ScreenFadeFromBlack:
            return sizeof(AiScreenFadeFromBlackRecord);
        case AI_IFScreenFadeCompleted:
            return sizeof(AiIFScreenFadeCompletedRecord);
        case AI_HideAllChrs:
            return sizeof(AiHideAllChrsRecord);
        case AI_ShowAllChrs:
            return sizeof(AiShowAllChrsRecord);
        case AI_DoorOpenInstant:
            return sizeof(AiDoorOpenInstantRecord);
        case AI_ChrRemoveItemInHand:
            return sizeof(AiChrRemoveItemInHandRecord);
        case AI_IfNumberOfActivePlayersLessThan:
            return sizeof(AiIfNumberOfActivePlayersLessThanRecord);
        case AI_IFBondItemTotalAmmoLessThan:
            return sizeof(AiIFBondItemTotalAmmoLessThanRecord);
        case AI_BondEquipItem:
            return sizeof(AiBondEquipItemRecord);
        case AI_BondEquipItemCinema:
            return sizeof(AiBondEquipItemCinemaRecord);
        case AI_BondSetLockedVelocity:
            return sizeof(AiBondSetLockedVelocityRecord);
        case AI_IFObjectInRoomWithPad:
            return sizeof(AiIFObjectInRoomWithPadRecord);
        case AI_SwitchSky:
            return sizeof(AiSwitchSkyRecord);
        case AI_TriggerFadeAndExitLevelOnButtonPress:
            return sizeof(AiTriggerFadeAndExitLevelOnButtonPressRecord);
        case AI_IFBondIsDead:
            return sizeof(AiIFBondIsDeadRecord);
        case AI_BondDisableDamageAndPickups:
            return sizeof(AiBondDisableDamageAndPickupsRecord);
        case AI_BondHideWeapons:
            return sizeof(AiBondHideWeaponsRecord);
        case AI_CameraOrbitPad:
            return sizeof(AiCameraOrbitPadRecord);
        case AI_CreditsRoll:
            return sizeof(AiCreditsRollRecord);
        case AI_IFCreditsHasCompleted:
            return sizeof(AiIFCreditsHasCompletedRecord);
        case AI_IFObjectiveAllCompleted:
            return sizeof(AiIFObjectiveAllCompletedRecord);
        case AI_IFFolderActorIsEqual:
            return sizeof(AiIFFolderActorIsEqualRecord);
        case AI_IFBondDamageAndPickupsDisabled:
            return sizeof(AiIFBondDamageAndPickupsDisabledRecord);
        case AI_MusicPlaySlot:
            return sizeof(AiMusicPlaySlotRecord);
        case AI_MusicStopSlot:
            return sizeof(AiMusicStopSlotRecord);
        case AI_TriggerExplosionsAroundBond:
            return sizeof(AiTriggerExplosionsAroundBondRecord);
        case AI_IFKilledCiviliansGreaterThan:
            return sizeof(AiIFKilledCiviliansGreaterThanRecord);
        case AI_IFChrWasShotSinceLastCheck:
            return sizeof(AiIFChrWasShotSinceLastCheckRecord);
        case AI_BondKilledInAction:
            return sizeof(AiBondKilledInActionRecord);
        case AI_RaiseArms:
            return sizeof(AiRaiseArmsRecord);
        case AI_GasLeakAndFadeFog:
            return sizeof(AiGasLeakAndFadeFogRecord);
        case AI_ObjectRocketLaunch:
            return sizeof(AiObjectRocketLaunchRecord);
        case AI_PRINT:
        {
            s32 pos = offset + 1;
            while (AIList[pos] != 0)
            {
                ++pos;
            }
            return (pos - offset) + 1;
        }
        default:
            return 1;
    }
}

/**
 * Get ID of AIList
 * @param AIList: Ailist to get ID of
 * @return ID of AIList
 */
s32 ailistGetId(AIRecord *AIList, bool *isGlobalAIList)
{
    s32 i;

    if (g_CurrentSetup.ailists)
    {
        for (i = 0; g_CurrentSetup.ailists[i].ailist; i++)
        {
            if (g_CurrentSetup.ailists[i].ailist == AIList)
            {
                *isGlobalAIList = FALSE;
                return g_CurrentSetup.ailists[i].ID;
            }
        }
    }

    for (i = 0; g_GlobalAILists[i].ailist; i++)
    {
        if (g_GlobalAILists[i].ailist == AIList)
        {
            *isGlobalAIList = TRUE;
            return g_GlobalAILists[i].ID;
        }
    }

    return -1;
}

/**
 * GoTo Label
 * @param AIlist: AIList containing label
 * @param LabelNum: Integer/enum ID to go to
 * @return Offset of label from beggining of AIList.
 */
s32 aiFindLabel(AIRecord *AIList, s32 Offset, u8 LabelNum)
{
    bool  isGlobalAIList;

    for (;;)
    {
        if (AIList[Offset].cmd == AI_Label)
        {
            if (AIList[Offset].val[0] == LabelNum)
            {
                // exit loop and return offset to label number
                return Offset;
            }
        }
        else if (AIList[Offset].cmd == AI_EndList)
        {
            // restart ai list PC if next label not found - causes infinite loop outside of debug
            ailistGetId(AIList, &isGlobalAIList);
            return 0;
        }

        Offset += aiCommandSize(AIList, Offset);
    }
}

AIRecord *ailistFindById(s32 ID)
{
    s32 i;

    if (!isGlobalAIListID(ID))
    {
        if (g_CurrentSetup.ailists)
        {
            for (i = 0; g_CurrentSetup.ailists[i].ailist; i++)
            {
                if (g_CurrentSetup.ailists[i].ID == ID)
                {
                    return g_CurrentSetup.ailists[i].ailist;
                }
            }
        }
    }
    else
    {
        for (i = 0; g_GlobalAILists[i].ailist; i++)
        {
            if (g_GlobalAILists[i].ID == ID)
            {
                return g_GlobalAILists[i].ailist;
            }
        }
    }
    return NULL;
}

PathRecord *pathFindById(s32 ID)
{
    int i;

    for (i = 0; g_CurrentSetup.patrolpaths[i].waypoints; i++)
    {
        if (ID == g_CurrentSetup.patrolpaths[i].ID)
        {
            return &g_CurrentSetup.patrolpaths[i];
        }
    }

    return NULL;
}



/**
 * Runs an AI list: a chr's (also the level-event chrs in g_BgChrs), a vehicle's or an
 * aircraft's. EntityType is PROP_TYPE_CHR or PROP_TYPE_OBJ. Commands run from the saved
 * offset until one yields; the offset is then saved for next time. There is little
 * checking: a chr-only command in a vehicle's list would use a NULL chr.
 *
 * The original name was ai(), from its debug strings ("ai(void) enery tune on").
 */
void aiRunList(PropDefHeaderRecord *Entityp, PROP_TYPE EntityType)
{
    // Each case declares its own pointer, ai, to its command record.

    ChrRecord      *ChrEntityp      = NULL;
    VehichleRecord *VehichleEntityp = NULL;
    AircraftRecord *AircraftEntityp = NULL;
    AIRecord       *AiListp         = NULL;
    s32             Offset;
    struct CoopAiState coop;

    coopAiReset(&coop);

    if (EntityType == PROP_TYPE_CHR)
    {
        ChrEntityp = (ChrRecord *)Entityp;
    }
    else if (EntityType == PROP_TYPE_OBJ)
    {
        if (Entityp->type == PROPDEF_VEHICHLE)
        {
            VehichleEntityp = (VehichleRecord *)Entityp;
        }
        else if (Entityp->type == PROPDEF_AIRCRAFT)
        {
            AircraftEntityp = (AircraftRecord *)Entityp;
        }
    }

    // Load ailist
    if (ChrEntityp)
    {
        Offset  = ChrEntityp->aioffset;
        AiListp = ChrEntityp->ailist;
    }
    else if (VehichleEntityp)
    {
        Offset  = VehichleEntityp->aioffset;
        AiListp = VehichleEntityp->ailist;
    }
    else if (AircraftEntityp)
    {
        Offset  = AircraftEntityp->aioffset;
        AiListp = AircraftEntityp->ailist;
    }

    if (AiListp) // continue if Has ailist (check once)
    {
        // loop forever (or until broken)
        for (;;)
        {
            if (gameIsTwoPlayerMission())
            {
                coopAiBegin(&coop, AiListp, Offset, ChrEntityp);
            }

            /*
             * GE uses long Switch/case while PD uses Bool functions and tests
             * for TRUE/FALSE if(funcpointer[ai]) break;
             */
            switch ((AiListp + Offset)->cmd)
            {
                case AI_GotoNext:
                {
                    AiGotoNextRecord *ai = (AiGotoNextRecord *)(AiListp + Offset);
                    Offset               = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    break;
                }
                case AI_GotoFirst:
                {
                    AiGotoFirstRecord *ai = (AiGotoFirstRecord *)(AiListp + Offset);
                    Offset                = aiFindLabel(AiListp, 0, ai->GOTOLABEL);
                    break;
                }
                case AI_Label:
                {
                    Offset += sizeof(AiLabelRecord);
                    break;
                }
                case AI_Yield:
                {
                    Offset += sizeof(AiYieldRecord);
                    if (ChrEntityp)
                    {
                        ChrEntityp->ailist   = AiListp;
                        ChrEntityp->aioffset = Offset;
                    }
                    else if (VehichleEntityp)
                    {
                        VehichleEntityp->ailist   = AiListp;
                        VehichleEntityp->aioffset = Offset;
                    }
                    else if (AircraftEntityp)
                    {
                        AircraftEntityp->ailist   = AiListp;
                        AircraftEntityp->aioffset = Offset;
                    }
                    return;
                }
                case AI_EndList:
                {
                    // Not an error to be here, same as yield except without pushing offset past it. (just keeps looping)

                    return;
                }
                case AI_SetChrAiList:
                {
                    AiSetChrAiListRecord *ai = (AiSetChrAiListRecord *)(AiListp + Offset);
                    ChrRecord            *chr;
                    u16                   AI_LIST_ID = ntohs(ai->AI_LIST_ID);
                    u8                    CHR_NUM    = ai->CHR_NUM;

                    if (CHR_NUM == ((u8)CHR_SELF))
                    {
                        AiListp = ailistFindById(AI_LIST_ID);
                        Offset  = 0;
                    }
                    else
                    {
                        chr = chrFindById(ChrEntityp, CHR_NUM);
                        if (chr)
                        {
                            chr->ailist   = ailistFindById(AI_LIST_ID);
                            chr->aioffset = 0;
                            chr->sleep    = 0;
                        }
                        Offset += sizeof(AiSetChrAiListRecord);
                    }
                    break;
                }
                case AI_SetReturnAiList:
                {
                    AiSetReturnAiListRecord *ai         = (AiSetReturnAiListRecord *)(AiListp + Offset);
                    u16                      AI_LIST_ID = ntohs(ai->AI_LIST_ID);

                    if (ChrEntityp)
                    {
                        ChrEntityp->aireturnlist = AI_LIST_ID;
                    }
                    else if (VehichleEntityp)
                    {
                        VehichleEntityp->aireturnlist = AI_LIST_ID;
                    }
                    else if (AircraftEntityp)
                    {
                        AircraftEntityp->aireturnlist = AI_LIST_ID;
                    }

                    Offset += sizeof(AiSetReturnAiListRecord);
                    break;
                }
                case AI_Return:
                {
                    if (ChrEntityp)
                    {
                        AiListp = ailistFindById(ChrEntityp->aireturnlist);
                    }
                    else if (VehichleEntityp)
                    {
                        AiListp = ailistFindById(VehichleEntityp->aireturnlist);
                    }
                    else if (AircraftEntityp)
                    {
                        AiListp = ailistFindById(AircraftEntityp->aireturnlist);
                    }
                    Offset = 0;
                    break;
                }
                case AI_Stop:
                {
                    chrTryStopAnim(ChrEntityp);
                    Offset += sizeof(AiStopRecord);
                    break;
                }
                case AI_Kneel:
                {
                    chrTryKneel(ChrEntityp);
                    Offset += sizeof(AiKneelRecord);
                    break;
                }
                case AI_PlayAnimation:
                {
                    AiPlayAnimationRecord *ai = (AiPlayAnimationRecord *)(AiListp + Offset);
                    s32                    startframe, anim_id, endframe;

                    anim_id    = ntohs(ai->ANIMATION_ID);
                    startframe = ntohs(ai->START_TIME30);
                    endframe   = ntohs(ai->END_TIME30);

                    if (startframe == (u16)-1)
                    {
                        startframe = 0;
                    }
                    if (endframe == (u16)-1)
                    {
                        endframe = -1;
                    }

                    if (ChrEntityp)
                    {
                        chrTryStartAnim(ChrEntityp, anim_id, startframe, endframe, ai->BITFIELD, ai->INTERPOL_TIME60);
                    }
                    else if (AircraftEntityp)
                    {
                        modelSetAnimation(AircraftEntityp->model, animation_table_ptrs2[anim_id], 0, startframe, 0.5f, (s32)ai->INTERPOL_TIME60);
                        if (endframe >= 0)
                        {
                            modelSetAnimEndFrame(AircraftEntityp->model, endframe);
                        }
                    }
                    Offset += sizeof(AiPlayAnimationRecord);
                    break;
                }
                case AI_IFPlayingAnimation:
                {
                    AiIFPlayingAnimationRecord *ai = (AiIFPlayingAnimationRecord *)(AiListp + Offset);

                    if (ChrEntityp->actiontype == ACT_ANIM)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFPlayingAnimationRecord);
                    }
                    break;
                }
                case AI_PointAtBond:
                {
                    chrTrySurprisedOneHand(ChrEntityp);
                    Offset += sizeof(AiPointAtBondRecord);
                    break;
                }
                case AI_LookSurprised:
                {
                    chrTrySurprisedLookAround(ChrEntityp);
                    Offset += sizeof(AiLookSurprisedRecord);
                    break;
                }
                case AI_IFImOnPatrolOrStopped:
                {
                    AiIFImOnPatrolOrStoppedRecord *ai = (AiIFImOnPatrolOrStoppedRecord *)(AiListp + Offset);
                    if (chrHasStoppedOrPatroling(ChrEntityp))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFImOnPatrolOrStoppedRecord);
                    }
                    break;
                }
                case AI_IFChrDyingOrDead:
                {
                    AiIFChrDyingOrDeadRecord *ai  = (AiIFChrDyingOrDeadRecord *)(AiListp + Offset);
                    ChrRecord                *chr = chrFindById(ChrEntityp, ai->CHR_NUM);

                    if (!chr || chrIsDead(chr))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFChrDyingOrDeadRecord);
                    }
                    break;
                }
                case AI_IFChrDoesNotExist:
                {
                    AiIFChrDoesNotExistRecord *ai  = (AiIFChrDoesNotExistRecord *)(AiListp + Offset);
                    ChrRecord                 *chr = chrFindById(ChrEntityp, ai->CHR_NUM);

                    if (!chr || !chr->model)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFChrDoesNotExistRecord);
                    }
                    break;
                }
                case AI_IFISeeBond:
                {
                    AiIFISeeBondRecord *ai = (AiIFISeeBondRecord *)(AiListp + Offset);

                    if (chrCheckTargetInSight(ChrEntityp))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFISeeBondRecord);
                    }
                    break;
                }

                case AI_TRYSidestepping:
                {
                    AiTRYSidesteppingRecord *ai = (AiTRYSidesteppingRecord *)(AiListp + Offset);

                    if (chrTrySidestep(ChrEntityp))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYSidesteppingRecord);
                    }
                    break;
                }
                case AI_TRYSideHopping:
                {
                    AiTRYSideHoppingRecord *ai = (AiTRYSideHoppingRecord *)(AiListp + Offset);

                    if (chrTryJumpOut(ChrEntityp))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYSideHoppingRecord);
                    }
                    break;
                }
                case AI_TRYSideRunning:
                {
                    AiTRYSideRunningRecord *ai = (AiTRYSideRunningRecord *)(AiListp + Offset);

                    if (chrTryRunSideways(ChrEntityp))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYSideRunningRecord);
                    }
                    break;
                }
                case AI_TRYFiringWalk:
                {
                    AiTRYFiringWalkRecord *ai = (AiTRYFiringWalkRecord *)(AiListp + Offset);

                    if (chrTryAttackWalk(ChrEntityp))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYFiringWalkRecord);
                    }
                    break;
                }
                case AI_TRYFiringRun:
                {
                    AiTRYFiringRunRecord *ai = (AiTRYFiringRunRecord *)(AiListp + Offset);

                    if (chrTryAttackRun(ChrEntityp))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYFiringRunRecord);
                    }
                    break;
                }
                case AI_TRYFiringRoll:
                {
                    AiTRYFiringRollRecord *ai = (AiTRYFiringRollRecord *)(AiListp + Offset);

                    if (chrTryAttackRoll(ChrEntityp))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYFiringRollRecord);
                    }
                    break;
                }
                case AI_TRYFireOrAimAtTarget:
                {
                    AiTRYFireOrAimAtTargetRecord *ai         = (AiTRYFireOrAimAtTargetRecord *)(AiListp + Offset);
                    s32                           targetid   = ntohs(ai->TARGET);
                    s32                           targettype = ntohs(ai->BITFIELD);
                    if (chrTryAttackStand(ChrEntityp, targettype, targetid))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYFireOrAimAtTargetRecord);
                    }
                    break;
                }
                case AI_TRYFireOrAimAtTargetKneel:
                {
                    AiTRYFireOrAimAtTargetKneelRecord *ai         = (AiTRYFireOrAimAtTargetKneelRecord *)(AiListp + Offset);
                    s32                                targetid   = ntohs(ai->TARGET);
                    s32                                targettype = ntohs(ai->BITFIELD);
                    if (chrTryAttackKneel(ChrEntityp, targettype, targetid))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYFireOrAimAtTargetKneelRecord);
                    }
                    break;
                }
                case AI_IFImFiringAndLockedForward:
                {
                    AiIFImFiringAndLockedForwardRecord *ai = (AiIFImFiringAndLockedForwardRecord *)(AiListp + Offset);

                    if (ChrEntityp->actiontype == ACT_ATTACK &&
                        !ChrEntityp->act_attack.type_of_motion &&
                        ChrEntityp->act_attack.attacktype & ATTACKTYPE_DONTTURN)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFImFiringAndLockedForwardRecord);
                    }
                    break;
                }
                case AI_IFImFiring:
                {
                    AiIFImFiringRecord *ai = (AiIFImFiringRecord *)(AiListp + Offset);

                    if (ChrEntityp->actiontype == ACT_ATTACK)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFImFiringRecord);
                    }
                    break;
                }

                case AI_TRYFireOrAimAtTargetUpdate:
                {
                    AiTRYFireOrAimAtTargetUpdateRecord *ai         = (AiTRYFireOrAimAtTargetUpdateRecord *)(AiListp + Offset);
                    s32                                 targetid   = ntohs(ai->TARGET);
                    s32                                 targettype = ntohs(ai->BITFIELD);
                    if (chrTryModifyAttack(ChrEntityp, targettype, targetid))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYFireOrAimAtTargetUpdateRecord);
                    }
                    break;
                }
                case AI_TRYFacingTarget:
                {
                    AiTRYFacingTargetRecord *ai         = (AiTRYFacingTargetRecord *)(AiListp + Offset);
                    s32                      targetid   = ntohs(ai->TARGET);
                    s32                      targettype = ntohs(ai->BITFIELD);
                    if (chrTryStop(ChrEntityp, targettype, targetid))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYFacingTargetRecord);
                    }
                    break;
                }
                case AI_HitChrWithItem:
                {
                    AiHitChrWithItemRecord *ai  = (AiHitChrWithItemRecord *)(AiListp + Offset);
                    ChrRecord              *chr = chrFindById(ChrEntityp, ai->CHR_NUM);
                    coord3d                   vec = {};

                    if (chr && chr->prop)
                    {
                        chrTakeShot(chr, ai->PART_NUM, &vec, ai->ITEM_NUM, FALSE);
                    }

                    Offset += sizeof(AiHitChrWithItemRecord);
                    break;
                }
                case AI_ChrHitChr:
                {
                    AiChrHitChrRecord *ai   = (AiChrHitChrRecord *)(AiListp + Offset);
                    ChrRecord         *chr1 = chrFindById(ChrEntityp, ai->CHR_NUM);
                    ChrRecord         *chr2 = chrFindById(ChrEntityp, ai->CHR_NUM_TARGET);

                    if (chr1 && chr2 && chr1->prop && chr2->prop)
                    {
                        PropRecord      *prop = chrGetEquippedWeaponPropWithCheck(chr1, GUNRIGHT);
                        WeaponObjRecord *weapon;
                        coord3d            vec = {};

                        if (!prop)
                        {
                            prop = chrGetEquippedWeaponPropWithCheck(chr1, GUNLEFT);
                        }

                        if (prop)
                        {
                            vec.x = chr2->prop->pos.x - chr1->prop->pos.x;
                            vec.y = chr2->prop->pos.y - chr1->prop->pos.y;
                            vec.z = chr2->prop->pos.z - chr1->prop->pos.z;
                            guNormalize(&vec.x, &vec.y, &vec.z);
                            weapon = prop->weapon;
                            chrTakeShot(chr2, ai->PART_NUM, &vec, weapon->weaponnum, 0);
                        }
                    }
                    Offset += sizeof(AiChrHitChrRecord);
                    break;
                }
                case AI_TRYThrowingGrenade:
                {
                    AiTRYThrowingGrenadeRecord *ai = (AiTRYThrowingGrenadeRecord *)(AiListp + Offset);

                    if (chrTryThrowGrenade(ChrEntityp))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYThrowingGrenadeRecord);
                    }
                    break;
                }
                case AI_TRYDroppingItem:
                {
                    AiTRYDroppingItemRecord *ai       = (AiTRYDroppingItemRecord *)(AiListp + Offset);
                    u16                      modelnum = ntohs(ai->PROP_NUM);
                    if (chrDropItem(ChrEntityp, modelnum, ai->ITEM_NUM))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYDroppingItemRecord);
                    }
                    break;
                }

                case AI_Surrender:
                {
                    chrTrySurrender(ChrEntityp);
                    Offset += sizeof(AiSurrenderRecord);
                    break;
                }
                case AI_RemoveMe:
                {
                    chrActorFadeAway(ChrEntityp);
                    Offset += sizeof(AiRemoveMeRecord);
                    break;
                }
                case AI_ChrRemoveInstant:
                {
                    AiChrRemoveInstantRecord *ai  = (AiChrRemoveInstantRecord *)(AiListp + Offset);
                    ChrRecord                *chr = chrFindById(ChrEntityp, ai->CHR_NUM);
                    if (chr && chr->prop)
                    {
                        chr->hidden |= CHRHIDDEN_REMOVE;
                    }
                    Offset += sizeof(AiChrRemoveInstantRecord);
                    break;
                }
                case AI_TRYTriggeringAlarmAtPad:
                {
                    AiTRYTriggeringAlarmAtPadRecord *ai     = (AiTRYTriggeringAlarmAtPadRecord *)(AiListp + Offset);
                    u16                              pad_id = ntohs(ai->PAD);
                    if (chrTryStartAlarm(ChrEntityp, pad_id))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYTriggeringAlarmAtPadRecord);
                    }
                    break;
                }
                case AI_AlarmOn:
                {
                    alarmActivate();
                    Offset += sizeof(AiAlarmOnRecord);
                    break;
                }
                case AI_AlarmOff:
                {
                    alarmDeactivate();
                    Offset += sizeof(AiAlarmOffRecord);
                    break;
                }
                case AI_TRYRunFromBond:
                { // run from bond
                    // Never in the game: always fails
                    Offset += sizeof(AiTRYRunFromBondRecord);
                    break;
                }
                case AI_TRYRunToBond:
                {
                    AiTRYRunToBondRecord *ai = (AiTRYRunToBondRecord *)(AiListp + Offset);
                    if (chrGoToBond(ChrEntityp, SPEED_RUN))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYRunToBondRecord);
                    }
                    break;
                }
                case AI_TRYWalkToBond:
                {
                    AiTRYWalkToBondRecord *ai = (AiTRYWalkToBondRecord *)(AiListp + Offset);
                    if (chrGoToBond(ChrEntityp, SPEED_WALK))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYWalkToBondRecord);
                    }
                    break;
                }
                case AI_TRYSprintToBond:
                {
                    AiTRYSprintToBondRecord *ai = (AiTRYSprintToBondRecord *)(AiListp + Offset);
                    if (chrGoToBond(ChrEntityp, SPEED_SPRINT))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYSprintToBondRecord);
                    }
                    break;
                }
                case AI_TRYFindCover:
                { // Find Cover
                    // Never in the game: always fails
                    Offset += sizeof(AiTRYFindCoverRecord);
                    break;
                }
                case AI_TRYRunToChr:
                {
                    AiTRYRunToChrRecord *ai = (AiTRYRunToChrRecord *)(AiListp + Offset);
                    if (chrGoToChr(ChrEntityp, ai->CHR_NUM, SPEED_RUN))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYRunToChrRecord);
                    }
                    break;
                }
                case AI_TRYWalkToChr:
                {
                    AiTRYWalkToChrRecord *ai = (AiTRYWalkToChrRecord *)(AiListp + Offset);
                    if (chrGoToChr(ChrEntityp, ai->CHR_NUM, SPEED_WALK))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYWalkToChrRecord);
                    }
                    break;
                }
                case AI_TRYSprintToChr:
                {
                    AiTRYSprintToChrRecord *ai = (AiTRYSprintToChrRecord *)(AiListp + Offset);

                    if (chrGoToChr(ChrEntityp, ai->CHR_NUM & 0xff, SPEED_SPRINT)) // &0xff is here to increase t reg by 1
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYSprintToChrRecord);
                    }
                    break;
                }

                case AI_SetNewRandom:
                {
                    ChrEntityp->random = randomGetNext();
                    Offset += sizeof(AiSetNewRandomRecord);
                    break;
                }
                case AI_IFRandomLessThan:
                {
                    AiIFRandomLessThanRecord *ai = (AiIFRandomLessThanRecord *)(AiListp + Offset);

                    if (ai->BYTE > ChrEntityp->random)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFRandomLessThanRecord);
                    }
                    break;
                }
                case AI_IFRandomGreaterThan:
                {
                    AiIFRandomGreaterThanRecord *ai = (AiIFRandomGreaterThanRecord *)(AiListp + Offset);
                    if (ai->BYTE < ChrEntityp->random)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFRandomGreaterThanRecord);
                    }
                    break;
                }

                case AI_RunToPad:
                {
                    AiRunToPadRecord *ai  = (AiRunToPadRecord *)(AiListp + Offset);
                    u16               pad = ntohs(ai->PAD);
                    chrGoToPad(ChrEntityp, pad, SPEED_RUN);
                    Offset += sizeof(AiRunToPadRecord);
                    break;
                }
                case AI_RunToPadPreset:
                {
                    /* PD uses GoTo Pad (speed) which seems better
                    switch (ai->val[0])
                    {
                        case SPEED_WALK:
                            chrGoToPad(ChrEntityp, ChrEntityp->padpreset1, SPEED_WALK);
                            break;
                        case SPEED_RUN:
                            etc...
                     */
                    chrGoToPad(ChrEntityp, ChrEntityp->padpreset1, SPEED_RUN);
                    Offset += sizeof(AiRunToPadPresetRecord);
                    break;
                }
                case AI_WalkToPad:
                {
                    AiWalkToPadRecord *ai  = (AiWalkToPadRecord *)(AiListp + Offset);
                    u16                pad = ntohs(ai->PAD);
                    chrGoToPad(ChrEntityp, pad, SPEED_WALK);
                    Offset += sizeof(AiWalkToPadRecord);
                    break;
                }
                case AI_SprintToPad:
                {
                    AiSprintToPadRecord *ai  = (AiSprintToPadRecord *)(AiListp + Offset);
                    u16                  pad = ntohs(ai->PAD);
                    chrGoToPad(ChrEntityp, pad, SPEED_SPRINT);
                    Offset += sizeof(AiSprintToPadRecord);
                    break;
                }
                case AI_StartPatrol:
                {
                    AiStartPatrolRecord *ai   = (AiStartPatrolRecord *)(AiListp + Offset);
                    PathRecord          *path = pathFindById(ai->PATH_NUM);
                    chrTryStartPatrol(ChrEntityp, path);
                    Offset += sizeof(AiStartPatrolRecord);
                    break;
                }
                case AI_IFICanHearAlarm:
                {
                    AiIFICanHearAlarmRecord *ai = (AiIFICanHearAlarmRecord *)(AiListp + Offset);
                    if (alarmIsActive())
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFICanHearAlarmRecord);
                    }
                    break;
                }
                case AI_IFAlarmIsOn:
                {
                    AiIFAlarmIsOnRecord *ai = (AiIFAlarmIsOnRecord *)(AiListp + Offset);
                    if (alarmIsActive())
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFAlarmIsOnRecord);
                    }
                    break;
                }
                case AI_IFGasIsLeaking:
                {
                    AiIFGasIsLeakingRecord *ai = (AiIFGasIsLeakingRecord *)(AiListp + Offset);
                    if (gasIsActive())
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFGasIsLeakingRecord);
                    }
                    break;
                }
                case AI_IFIHeardBond:
                {
                    AiIFIHeardBondRecord *ai = (AiIFIHeardBondRecord *)(AiListp + Offset);
                    if (ChrEntityp->hidden & CHRHIDDEN_ALERT_GUARD_RELATED)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFIHeardBondRecord);
                    }
                    break;
                }
                case AI_IFISeeSomeoneShot:
                {
                    AiIFISeeSomeoneShotRecord *ai = (AiIFISeeSomeoneShotRecord *)(AiListp + Offset);
                    if (ChrEntityp->chrseeshot >= 0)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFISeeSomeoneShotRecord);
                    }
                    break;
                }
                case AI_IFISeeSomeoneDie:
                {
                    AiIFISeeSomeoneDieRecord *ai = (AiIFISeeSomeoneDieRecord *)(AiListp + Offset);
                    if (ChrEntityp->chrseedie >= 0)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFISeeSomeoneDieRecord);
                    }
                    break;
                }
                case AI_IFICouldSeeBond:
                {
                    AiIFICouldSeeBondRecord *ai = (AiIFICouldSeeBondRecord *)(AiListp + Offset);
                    if (chrCanSeeBond(ChrEntityp))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFICouldSeeBondRecord);
                    }
                    break;
                }
                case AI_IFICouldSeeBondsStan:
                {
                    AiIFICouldSeeBondsStanRecord *ai = (AiIFICouldSeeBondsStanRecord *)(AiListp + Offset);
                    if (chrIsTargetNearlyInSight(ChrEntityp))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFICouldSeeBondsStanRecord);
                    }
                    break;
                }
                case AI_IFIWasShotRecently:
                {
                    AiIFIWasShotRecentlyRecord *ai = (AiIFIWasShotRecentlyRecord *)(AiListp + Offset);
                    if (chrSawTargetRecently(ChrEntityp))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFIWasShotRecentlyRecord);
                    }
                    break;
                }
                case AI_IFIHeardBondRecently:
                {
                    AiIFIHeardBondRecentlyRecord *ai = (AiIFIHeardBondRecentlyRecord *)(AiListp + Offset);
                    if (chrHeardTargetRecently(ChrEntityp))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFIHeardBondRecentlyRecord);
                    }
                    break;
                }
                case AI_IFImInRoomWithChr:
                {
                    AiIFImInRoomWithChrRecord *ai  = (AiIFImInRoomWithChrRecord *)(AiListp + Offset);
                    ChrRecord                 *chr = chrFindById(ChrEntityp, ai->CHR_NUM);
                    if (chr && chr->prop && chrHasLineOfSightToPos(ChrEntityp, &chr->prop->pos, chr->prop->stan))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFImInRoomWithChrRecord);
                    }
                    break;
                }
                case AI_IFIveNotBeenSeen:
                {
                    AiIFIveNotBeenSeenRecord *ai = (AiIFIveNotBeenSeenRecord *)(AiListp + Offset);
                    if (!(ChrEntityp->chrflags & CHRFLAG_HAS_BEEN_ON_SCREEN))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFIveNotBeenSeenRecord);
                    }
                    break;
                }
                case AI_IFImOnScreen:
                {
                    AiIFImOnScreenRecord *ai = (AiIFImOnScreenRecord *)(AiListp + Offset);
                    if ((ChrEntityp->prop->flags & PROPFLAG_ONSCREEN))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFImOnScreenRecord);
                    }
                    break;
                }
                case AI_IFMyRoomIsOnScreen:
                {
                    AiIFMyRoomIsOnScreenRecord *ai = (AiIFMyRoomIsOnScreenRecord *)(AiListp + Offset);

                    if (bgRoomIsVisible(ChrEntityp->prop->stan->room))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFMyRoomIsOnScreenRecord);
                    }
                    break;
                }
                case AI_IFRoomWithPadIsOnScreen:
                {
                    AiIFRoomWithPadIsOnScreenRecord *ai     = (AiIFRoomWithPadIsOnScreenRecord *)(AiListp + Offset);
                    u16                              pad_id = ntohs(ai->PAD);
                    if (chrPadRoomIsVisible(ChrEntityp, pad_id))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFRoomWithPadIsOnScreenRecord);
                    }
                    break;
                }
                case AI_IFImTargetedByBond:
                {
                    AiIFImTargetedByBondRecord *ai = (AiIFImTargetedByBondRecord *)(AiListp + Offset);
                    if (chrIsTargetedByPlayer(ChrEntityp))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFImTargetedByBondRecord);
                    }
                    break;
                }
                case AI_IFBondMissedMe:
                {
                    AiIFBondMissedMeRecord *ai = (AiIFBondMissedMeRecord *)(AiListp + Offset);
                    if (ChrEntityp->chrflags & CHRFLAG_NEAR_MISS)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFBondMissedMeRecord);
                    }
                    break;
                }
                case AI_IFMyAngleToBondLessThan:
                {
                    // Alternative Names?
                    // aiIfTargetInFovLeft or aiIfBondOutOfFov
                    AiIFMyAngleToBondLessThanRecord *ai  = (AiIFMyAngleToBondLessThanRecord *)(AiListp + Offset);
                    float                            rad = chrGetAngleToBond(ChrEntityp);
                    if (ByteToRadian((ai->ANGLE)) > rad)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFMyAngleToBondLessThanRecord);
                    }
                    break;
                }
                case AI_IFMyAngleToBondGreaterThan:
                {
                    AiIFMyAngleToBondGreaterThanRecord *ai  = (AiIFMyAngleToBondGreaterThanRecord *)(AiListp + Offset);
                    float                               rad = chrGetAngleToBond(ChrEntityp);
                    if (ByteToRadian((ai->ANGLE)) < rad)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFMyAngleToBondGreaterThanRecord);
                    }
                    break;
                }
                case AI_IFMyAngleFromBondLessThan:
                {
                    AiIFMyAngleFromBondLessThanRecord *ai  = (AiIFMyAngleFromBondLessThanRecord *)(AiListp + Offset);
                    float                              rad = chrGetAngleFromBond(ChrEntityp);
                    if (ByteToRadian((ai->ANGLE)) > rad)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFMyAngleFromBondLessThanRecord);
                    }
                    break;
                }
                case AI_IFMyAngleFromBondGreaterThan:
                {
                    AiIFMyAngleFromBondGreaterThanRecord *ai  = (AiIFMyAngleFromBondGreaterThanRecord *)(AiListp + Offset);
                    float                                 rad = chrGetAngleFromBond(ChrEntityp);
                    if (ByteToRadian((ai->ANGLE)) < rad)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFMyAngleFromBondGreaterThanRecord);
                    }
                    break;
                }
                case AI_IFMyDistanceToBondLessThanDecimeter:
                {
                    AiIFMyDistanceToBondLessThanDecimeterRecord *ai       = (AiIFMyDistanceToBondLessThanDecimeterRecord *)(AiListp + Offset);
                    f32                                          distance = ntohs(ai->DISTANCE) * 10.0f;
                    if (distance > chrGetDistanceToBond(ChrEntityp))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFMyDistanceToBondLessThanDecimeterRecord);
                    }
                    break;
                }
                case AI_IFMyDistanceToBondGreaterThanDecimeter:
                {
                    AiIFMyDistanceToBondGreaterThanDecimeterRecord *ai       = (AiIFMyDistanceToBondGreaterThanDecimeterRecord *)(AiListp + Offset);
                    f32                                             distance = ntohs(ai->DISTANCE) * 10.0f;
                    if (distance < chrGetDistanceToBond(ChrEntityp))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFMyDistanceToBondGreaterThanDecimeterRecord);
                    }
                    break;
                }
                case AI_IFChrDistanceToPadLessThanDecimeter:
                {
                    AiIFChrDistanceToPadLessThanDecimeterRecord *ai     = (AiIFChrDistanceToPadLessThanDecimeterRecord *)(AiListp + Offset);
                    ChrRecord                                   *chr    = chrFindById(ChrEntityp, ai->CHR_NUM);
                    u16                                          padnum = ntohs(ai->PAD);
                    f32                                          value  = ntohs(ai->DISTANCE) * 10.0f;
                    if (chr && (value > chrGetDistanceToPad(chr, padnum)))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFChrDistanceToPadLessThanDecimeterRecord);
                    }
                    break;
                }
                case AI_IFChrDistanceToPadGreaterThanDecimeter:
                {
                    AiIFChrDistanceToPadGreaterThanDecimeterRecord *ai     = (AiIFChrDistanceToPadGreaterThanDecimeterRecord *)(AiListp + Offset);
                    ChrRecord                                      *chr    = chrFindById(ChrEntityp, ai->CHR_NUM);
                    u16                                             padnum = ntohs(ai->PAD);
                    f32                                             value  = ntohs(ai->DISTANCE) * 10.0f;
                    if (chr && (value < chrGetDistanceToPad(chr, padnum)))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFChrDistanceToPadGreaterThanDecimeterRecord);
                    }
                    break;
                }
                case AI_IFMyDistanceToChrLessThanDecimeter:
                {
                    AiIFMyDistanceToChrLessThanDecimeterRecord *ai     = (AiIFMyDistanceToChrLessThanDecimeterRecord *)(AiListp + Offset);
                    f32                                         cutoff = ntohs(ai->DISTANCE) * 10.0f;
                    if (cutoff > chrGetDistanceToChr(ChrEntityp, ai->CHR_NUM))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFMyDistanceToChrLessThanDecimeterRecord);
                    }
                    break;
                }
                case AI_IFMyDistanceToChrGreaterThanDecimeter:
                {
                    AiIFMyDistanceToChrGreaterThanDecimeterRecord *ai     = (AiIFMyDistanceToChrGreaterThanDecimeterRecord *)(AiListp + Offset);
                    f32                                            cutoff = ntohs(ai->DISTANCE) * 10.0f;
                    if (cutoff < chrGetDistanceToChr(ChrEntityp, ai->CHR_NUM))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFMyDistanceToChrGreaterThanDecimeterRecord);
                    }
                    break;
                }
                case AI_TRYSettingMyPresetToChrWithinDistanceDecimeter:
                {
                    AiTRYSettingMyPresetToChrWithinDistanceDecimeterRecord *ai       = (AiTRYSettingMyPresetToChrWithinDistanceDecimeterRecord *)(AiListp + Offset);
                    f32                                                     distance = ntohs(ai->DISTANCE) * 10.0f;
                    if (chrSetChrPresetToNearbyChr(ChrEntityp, distance))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYSettingMyPresetToChrWithinDistanceDecimeterRecord);
                    }
                    break;
                }
                case AI_IFBondDistanceToPadLessThanDecimeter:
                {
                    AiIFBondDistanceToPadLessThanDecimeterRecord *ai    = (AiIFBondDistanceToPadLessThanDecimeterRecord *)(AiListp + Offset);
                    u16                                           pad   = ntohs(ai->PAD);
                    f32                                           value = ntohs(ai->DISTANCE) * 10.0f;
                    if (value > chrGetDistanceFromBondToPad(ChrEntityp, pad))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFBondDistanceToPadLessThanDecimeterRecord);
                    }
                    break;
                }
                case AI_IFBondDistanceToPadGreaterThanDecimeter:
                {
                    AiIFBondDistanceToPadGreaterThanDecimeterRecord *ai    = (AiIFBondDistanceToPadGreaterThanDecimeterRecord *)(AiListp + Offset);
                    u16                                              pad   = ntohs(ai->PAD);
                    f32                                              value = ntohs(ai->DISTANCE) * 10.0f;
                    if (value < chrGetDistanceFromBondToPad(ChrEntityp, pad))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFBondDistanceToPadGreaterThanDecimeterRecord);
                    }
                    break;
                }
                case AI_IFChrInRoomWithPad:
                {
                    AiIFChrInRoomWithPadRecord *ai     = (AiIFChrInRoomWithPadRecord *)(AiListp + Offset);
                    u16                         pad_id = ntohs(ai->PAD);
                    if (chrIfInPadRoom(ChrEntityp, ai->CHR_NUM, pad_id))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFChrInRoomWithPadRecord);
                    }
                    break;
                }
                case AI_IFBondInRoomWithPad:
                {
                    AiIFBondInRoomWithPadRecord *ai     = (AiIFBondInRoomWithPadRecord *)(AiListp + Offset);
                    u16                          pad_id = ntohs(ai->PAD);
                    if (chrPlayerIsInPadRoom(ChrEntityp, pad_id))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFBondInRoomWithPadRecord);
                    }
                    break;
                }
                case AI_IFBondCollectedObject:
                {
                    AiIFBondCollectedObjectRecord *ai  = (AiIFBondCollectedObjectRecord *)(AiListp + Offset);
                    ObjectRecord                  *obj = objFindByTagId(ai->OBJECT_TAG);
                    if (obj && obj->prop && invHasProp(obj->prop))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFBondCollectedObjectRecord);
                    }
                    break;
                }
                case AI_IFKeyDropped:
                {
                    AiIFKeyDroppedRecord *ai = (AiIFKeyDroppedRecord *)(AiListp + Offset);
                    // Co-op: where a script checks "placed on the right
                    // object, else placed anywhere = failed" (the attached
                    // check comes just before this one), a misplaced item
                    // only counts while no player still has a copy they
                    // could place correctly. Other uses, like Silo's
                    // plastique starting a countdown, are unchanged.
                    s32 coopsecondcopy = gameIsTwoPlayerMission()
                        && (Offset >= (s32)sizeof(AiIFItemIsAttachedToObjectRecord))
                        && (((u8 *)AiListp)[Offset - sizeof(AiIFItemIsAttachedToObjectRecord)] == AI_IFItemIsAttachedToObject)
                        && (((u8 *)AiListp)[Offset - sizeof(AiIFItemIsAttachedToObjectRecord) + 1] == ai->KEY_ID)
                        && coopAnyPlayerStillHasItem(ai->KEY_ID);

                    if (weaponFindThrown(ai->KEY_ID) && !coopsecondcopy)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFKeyDroppedRecord);
                    }
                    break;
                }
                case AI_IFItemIsAttachedToObject:
                {
                    AiIFItemIsAttachedToObjectRecord *ai   = (AiIFItemIsAttachedToObjectRecord *)(AiListp + Offset);
                    ObjectRecord                     *obj  = objFindByTagId(ai->OBJECT_TAG);
                    bool                              pass = FALSE;
                    if (obj && obj->prop)
                    {
                        PropRecord *prop = propChild(obj->prop);
                        while (prop)
                        {
                            if (prop->type == PROP_TYPE_WEAPON)
                            {
                                WeaponObjRecord *weapon = prop->weapon;
                                if (weapon->weaponnum == ai->ITEM_NUM)
                                {
                                    pass = TRUE;
                                    break;
                                }
                            }
                            prop = prop->prev;
                        }
                    }
                    if (pass)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFItemIsAttachedToObjectRecord);
                    }
                    break;
                }
                case AI_IFBondHasItemEquipped:
                {
                    AiIFBondHasItemEquippedRecord *ai = (AiIFBondHasItemEquippedRecord *)(AiListp + Offset);
                    if (ai->ITEM_NUM == gunGetWeapon(GUNRIGHT) || ai->ITEM_NUM == gunGetWeapon(GUNLEFT)) // order matters
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFBondHasItemEquippedRecord);
                    }
                    break;
                }
                case AI_IFObjectExists:
                {
                    AiIFObjectExistsRecord *ai  = (AiIFObjectExistsRecord *)(AiListp + Offset);
                    ObjectRecord           *obj = objFindByTagId(ai->OBJECT_TAG);
                    if (obj && obj->prop)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFObjectExistsRecord);
                    }
                    break;
                }
                case AI_IFObjectNotDestroyed:
                {
                    AiIFObjectNotDestroyedRecord *ai  = (AiIFObjectNotDestroyedRecord *)(AiListp + Offset);
                    ObjectRecord                 *obj = objFindByTagId(ai->OBJECT_TAG);
                    if (obj && obj->prop && objIsHealthy(obj))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFObjectNotDestroyedRecord);
                    }
                    break;
                }
                case AI_IFObjectWasActivated:
                {
                    AiIFObjectWasActivatedRecord *ai  = (AiIFObjectWasActivatedRecord *)(AiListp + Offset);
                    ObjectRecord                 *obj = objFindByTagId(ai->OBJECT_TAG);
                    if (obj && obj->prop && (obj->runtime_bitflags & RUNTIMEBITFLAG_ACTIVATED))
                    {
                        obj->runtime_bitflags &= ~RUNTIMEBITFLAG_ACTIVATED;
                        coopAiActOnActivator(ChrEntityp);
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFObjectWasActivatedRecord);
                    }
                    break;
                }
                case AI_IFBondUsedGadgetOnObject:
                {
                    AiIFBondUsedGadgetOnObjectRecord *ai  = (AiIFBondUsedGadgetOnObjectRecord *)(AiListp + Offset);
                    ObjectRecord                     *obj = objFindByTagId(ai->OBJECT_TAG);
                    if (obj && obj->prop && (obj->state & PROPSTATE_ACTIVATED))
                    {
                        obj->state &= ~PROPSTATE_ACTIVATED;
                        coopAiActOnActivator(ChrEntityp);
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFBondUsedGadgetOnObjectRecord);
                    }
                    break;
                }
                case AI_ActivateObject:
                {
                    AiActivateObjectRecord *ai  = (AiActivateObjectRecord *)(AiListp + Offset);
                    ObjectRecord           *obj = objFindByTagId(ai->OBJECT_TAG);
                    if (obj && obj->prop)
                    {
                        if (obj->prop->type == PROP_TYPE_DOOR)
                        {
                            doorToggle(obj->prop);
                        }
                        else if (obj->prop->type == PROP_TYPE_OBJ || obj->prop->type == PROP_TYPE_WEAPON)
                        {
                            objInteract(obj->prop);
                        }
                    }
                    Offset += sizeof(AiActivateObjectRecord);
                    break;
                }
                case AI_DestroyObject: // canonicly destroyobj
                {
                    AiDestroyObjectRecord *ai  = (AiDestroyObjectRecord *)(AiListp + Offset);
                    ObjectRecord          *obj = objFindByTagId(ai->OBJECT_TAG);
                    if (obj && obj->prop)
                    {
                        if (!objGetDestroyedLevel(obj))
                        {
                            f32 damage = ((obj->health - obj->damageTaken) + 1) / 250.0f;
                            /*
                            osSyncPrintf("ai_destroyobj 2 : (def->obj == PROP_ELVIS_SAUCER)\n");
                            osSyncPrintf("Elvis BOOM\n");
                            */

                            objApplyDamage(obj, damage, &obj->runtime_pos, 0x1D, -1);
                        }
                    }
                    Offset += sizeof(AiDestroyObjectRecord);
                    break;
                }
                case AI_DropObject:
                {
                    AiDropObjectRecord *ai  = (AiDropObjectRecord *)(AiListp + Offset);
                    ObjectRecord       *obj = objFindByTagId(ai->OBJECT_TAG);
                    if (obj && obj->prop && propParent(obj->prop) && propParent(obj->prop)->type == PROP_TYPE_CHR)
                    {
                        ChrRecord *chr = propParent(obj->prop)->chr;
                        objSetDropType(obj->prop, 2);
                        chr->hidden |= CHRHIDDEN_DROP_HELD_ITEMS;
                    }
                    Offset += sizeof(AiDropObjectRecord);
                    break;
                }
                case AI_ChrDropAllConcealedItems:
                {
                    AiChrDropAllConcealedItemsRecord *ai  = (AiChrDropAllConcealedItemsRecord *)(AiListp + Offset);
                    ChrRecord                        *chr = chrFindById(ChrEntityp, ai->CHR_NUM);
                    if (chr && chr->prop)
                    {
                        chrDropItems(chr);
                    }
                    Offset += sizeof(AiChrDropAllConcealedItemsRecord);
                    break;
                }
                case AI_ChrDropAllHeldItems:
                {
                    AiChrDropAllHeldItemsRecord *ai  = (AiChrDropAllHeldItemsRecord *)(AiListp + Offset);
                    ChrRecord                   *chr = chrFindById(ChrEntityp, ai->CHR_NUM);
                    if (chr && chr->prop)
                    {
                        if (chr->weapons_held[GUNRIGHT])
                        {
                            objSetDropType(chr->weapons_held[GUNRIGHT], 1);
                            chr->hidden |= CHRHIDDEN_DROP_HELD_ITEMS;
                        }
                        if (chr->weapons_held[GUNLEFT])
                        {
                            objSetDropType(chr->weapons_held[GUNLEFT], 1);
                            chr->hidden |= CHRHIDDEN_DROP_HELD_ITEMS;
                        }
                    }
                    Offset += sizeof(AiChrDropAllHeldItemsRecord);
                    break;
                }
                case AI_BondCollectObject:
                {
                    AiBondCollectObjectRecord *ai  = (AiBondCollectObjectRecord *)(AiListp + Offset);
                    ObjectRecord              *obj = objFindByTagId(ai->OBJECT_TAG);
                    if (obj && obj->prop)
                    {
                        TICKOP op = propPickupByPlayer(obj->prop, FALSE);
                        propExecuteTickOperation(obj->prop, op);
                    }
                    Offset += sizeof(AiBondCollectObjectRecord);
                    break;
                }
                case AI_ChrEquipObject:
                {
                    AiChrEquipObjectRecord *ai  = (AiChrEquipObjectRecord *)(AiListp + Offset);
                    ObjectRecord           *obj = objFindByTagId(ai->OBJECT_TAG);
                    ChrRecord              *chr = chrFindById(ChrEntityp, ai->CHR_NUM);
                    if (obj && obj->prop && chr)
                    {
                        if (propParent(obj->prop))
                        {
                            objDetach(obj->prop);
                        }
                        else
                        {
                            propRemoveFromRooms(obj->prop);
                            propDelist(obj->prop);
                            propDisable(obj->prop);
                        }
                        if (obj->type != PROPDEF_COLLECTABLE || !chrEquipWeapon((WeaponObjRecord *)obj, chr))
                        {
                            propReparent(obj->prop, chr->prop);
                        }
                    }
                    Offset += sizeof(AiChrEquipObjectRecord);
                    break;
                }
                case AI_MoveObject: // canonicly aiMoveObj
                {
                    AiMoveObjectRecord *ai  = (AiMoveObjectRecord *)(AiListp + Offset);
                    ObjectRecord       *obj = objFindByTagId(ai->OBJECT_TAG);
                    PadRecord          *pad;
                    u16                 padnum = ntohs(ai->PAD);
                    Mtxf                matrix;

                    if (obj && obj->prop)
                    {
                        if (isNotBoundPad(padnum))
                        {
                            pad = &g_CurrentSetup.pads[padnum];
                        }
                        else
                        {
                            pad = (PadRecord *)&g_CurrentSetup.boundpads[getBoundPadNum(padnum)];
                        }
                        mtx4LoadFacing(&matrix, 0, 0, 0, -pad->look.x, -pad->look.y, -pad->look.z, pad->up.x, pad->up.y, pad->up.z);

                        if (obj->model)
                        {
                            mtx4ScaleRows012(obj->model->scale, &matrix);
                        }
                        objPlaceAtPad(obj, &pad->pos, &matrix, pad->stan, &pad->pos);
                        setupUpdateObjectRoomPosition(obj);
                    }
                    Offset += sizeof(AiMoveObjectRecord);
                    break;
                }
                case AI_DoorOpen:
                {
                    AiDoorOpenRecord *ai  = (AiDoorOpenRecord *)(AiListp + Offset);
                    DoorRecord       *obj = (DoorRecord *)objFindByTagId(ai->OBJECT_TAG);
                    if (obj && obj->prop && obj->prop->type == PROP_TYPE_DOOR)
                    {
                        // DoorRecord *door = (DoorRecord *)obj;
                        doorMove(obj, DOORSTATE_OPENING);
                    }
                    Offset += sizeof(AiDoorOpenRecord);
                    break;
                }
                case AI_DoorClose:
                {
                    AiDoorCloseRecord *ai  = (AiDoorCloseRecord *)(AiListp + Offset);
                    DoorRecord        *obj = (DoorRecord *)objFindByTagId(ai->OBJECT_TAG);
                    if (obj && obj->prop && obj->prop->type == PROP_TYPE_DOOR)
                    {
                        // DoorRecord *door = (DoorRecord *)obj;
                        doorMove(obj, DOORSTATE_CLOSING);
                    }
                    Offset += sizeof(AiDoorCloseRecord);
                    break;
                }
                case AI_IFDoorStateEqual:
                {
                    AiIFDoorStateEqualRecord *ai   = (AiIFDoorStateEqualRecord *)(AiListp + Offset);
                    ObjectRecord             *obj  = objFindByTagId(ai->OBJECT_TAG);
                    bool                      pass = FALSE;
                    if (obj && obj->prop && obj->type == PROPDEF_DOOR)
                    {
                        DoorRecord *door = (DoorRecord *)obj;
                        if (door->openstate == DOORSTATE_STATIONARY)
                        {
                            if (door->openPosition <= 0)
                            {
                                pass = (ai->DOOR_STATE & AI_DOOR_STATE_CLOSED) != 0;
                            }
                            else
                            {
                                pass = (ai->DOOR_STATE & AI_DOOR_STATE_OPEN) != 0;
                            }
                        }
                        else if (door->openstate == DOORSTATE_OPENING || door->openstate == DOORSTATE_WAITING)
                        {
                            pass = (ai->DOOR_STATE & AI_DOOR_STATE_OPENING) != 0;
                        }
                        else if (door->openstate == DOORSTATE_CLOSING)
                        {
                            pass = (ai->DOOR_STATE & AI_DOOR_STATE_CLOSING) != 0;
                        }
                    }
                    if (pass)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFDoorStateEqualRecord);
                    }
                    break;
                }
                case AI_IFDoorHasBeenOpenedBefore:
                {
                    AiIFDoorHasBeenOpenedBeforeRecord *ai  = (AiIFDoorHasBeenOpenedBeforeRecord *)(AiListp + Offset);
                    ObjectRecord                      *obj = objFindByTagId(ai->OBJECT_TAG);
                    if (obj && obj->prop && obj->type == PROPDEF_DOOR && (obj->runtime_bitflags & RUNTIMEBITFLAG_BEENOPENED))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFDoorHasBeenOpenedBeforeRecord);
                    }
                    break;
                }
                case AI_DoorSetLock:
                {
                    AiDoorSetLockRecord *ai  = (AiDoorSetLockRecord *)(AiListp + Offset);
                    ObjectRecord        *obj = objFindByTagId(ai->OBJECT_TAG);
                    if (obj && obj->prop && obj->prop->type == PROP_TYPE_DOOR)
                    {
                        DoorRecord *door = (DoorRecord *)obj;
                        u8          bits = ai->LOCK_FLAG;
                        door->keyflags   = door->keyflags | bits;
                    }
                    Offset += sizeof(AiDoorSetLockRecord);
                    break;
                }
                case AI_DoorUnsetLock:
                {
                    AiDoorUnsetLockRecord *ai  = (AiDoorUnsetLockRecord *)(AiListp + Offset);
                    ObjectRecord          *obj = objFindByTagId(ai->OBJECT_TAG);
                    if (obj && obj->prop && obj->prop->type == PROP_TYPE_DOOR)
                    {
                        DoorRecord *door = (DoorRecord *)obj;
                        u8          bits = ai->LOCK_FLAG;
                        door->keyflags &= ~bits;
                    }
                    Offset += sizeof(AiDoorUnsetLockRecord);
                    break;
                }
                case AI_IFDoorLockEqual:
                {
                    AiIFDoorLockEqualRecord *ai   = (AiIFDoorLockEqualRecord *)(AiListp + Offset);
                    ObjectRecord            *obj  = objFindByTagId(ai->OBJECT_TAG);
                    bool                     pass = FALSE;
                    if (obj && obj->prop && obj->prop->type == PROP_TYPE_DOOR)
                    {
                        DoorRecord *door = (DoorRecord *)obj;
                        u32         bits = ai->LOCK_FLAG;
                        if ((door->keyflags & bits) == bits)
                        {
                            pass = TRUE;
                        }
                    }
                    if (pass)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFDoorLockEqualRecord);
                    }
                    break;
                }
                case AI_IFObjectiveNumComplete:
                {
                    AiIFObjectiveNumCompleteRecord *ai = (AiIFObjectiveNumCompleteRecord *)(AiListp + Offset);
                    /*  additional PD code for dificulty filtering
                     == OBJECTIVE_COMPLETE && objectivelvlGetSelectedDifficultyBits(ai->val[0]) & (1 << lvGetDifficulty()))  *
                    */
                    if (objectiveGetCount() > ai->OBJ_NUM && OBJECTIVESTATUS_COMPLETE == objectiveCheckStatus(ai->OBJ_NUM))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFObjectiveNumCompleteRecord);
                    }
                    break;
                }
                case AI_TRYUnknown6e:
                {
                    AiTRYUnknown6eRecord *ai = (AiTRYUnknown6eRecord *)(AiListp + Offset);
                    if (chrSetPadPresetToWaypointInTargetQuadrant(ChrEntityp, ai->val))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYUnknown6eRecord);
                    }
                    break;
                }
                case AI_TRYUnknown6f:
                {
                    AiTRYUnknown6fRecord *ai = (AiTRYUnknown6fRecord *)(AiListp + Offset);
                    if (chrSetPadPresetToWaypointInQuadrant(ChrEntityp, ai->val))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYUnknown6fRecord);
                    }
                    break;
                }

                case AI_IFMyNumArghsLessThan:
                {
                    AiIFMyNumArghsLessThanRecord *ai = (AiIFMyNumArghsLessThanRecord *)(AiListp + Offset);
                    if (ai->val > ChrEntityp->numarghs) // order matter
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFMyNumArghsLessThanRecord);
                    }
                    break;
                }
                case AI_IFMyNumArghsGreaterThan:
                {
                    AiIFMyNumArghsGreaterThanRecord *ai = (AiIFMyNumArghsGreaterThanRecord *)(AiListp + Offset);
                    if (ai->val < ChrEntityp->numarghs) // order matter
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFMyNumArghsGreaterThanRecord);
                    }
                    break;
                }
                case AI_IFMyNumCloseArghsLessThan:
                {
                    AiIFMyNumCloseArghsLessThanRecord *ai = (AiIFMyNumCloseArghsLessThanRecord *)(AiListp + Offset);
                    if (ai->val > ChrEntityp->numclosearghs)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFMyNumCloseArghsLessThanRecord);
                    }
                    break;
                }
                case AI_IFMyNumCloseArghsGreaterThan:
                {
                    AiIFMyNumCloseArghsGreaterThanRecord *ai = (AiIFMyNumCloseArghsGreaterThanRecord *)(AiListp + Offset);
                    if (ai->val < ChrEntityp->numclosearghs)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFMyNumCloseArghsGreaterThanRecord);
                    }
                    break;
                }
                case AI_IFChrHealthLessThan:
                {
                    AiIFChrHealthLessThanRecord *ai    = (AiIFChrHealthLessThanRecord *)(AiListp + Offset);
                    f32                          value = (ai->HEALTH) * 0.1f;
                    ChrRecord                   *chr   = chrFindById(ChrEntityp, ai->CHR_NUM);

                    if (chr && ((chr->maxdamage - chr->damage) < value))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFChrHealthLessThanRecord);
                    }
                    break;
                }
                case AI_IFChrHealthGreaterThan:
                {
                    AiIFChrHealthGreaterThanRecord *ai    = (AiIFChrHealthGreaterThanRecord *)(AiListp + Offset);
                    f32                             value = (ai->HEALTH) * 0.1f;
                    ChrRecord                      *chr   = chrFindById(ChrEntityp, ai->CHR_NUM);

                    if (chr && ((chr->maxdamage - chr->damage) > value))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFChrHealthGreaterThanRecord);
                    }
                    break;
                }
                case AI_IFChrWasDamagedSinceLastCheck:
                {
                    AiIFChrWasDamagedSinceLastCheckRecord *ai  = (AiIFChrWasDamagedSinceLastCheckRecord *)(AiListp + Offset);
                    ChrRecord                             *chr = chrFindById(ChrEntityp, ai->CHR_NUM);
                    if (chr && (chr->chrflags & CHRFLAG_WAS_DAMAGED))
                    {
                        chr->chrflags &= ~CHRFLAG_WAS_DAMAGED; // disable flag
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFChrWasDamagedSinceLastCheckRecord);
                    }
                    break;
                }
                case AI_IFBondHealthLessThan:
                {
                    AiIFBondHealthLessThanRecord *ai  = (AiIFBondHealthLessThanRecord *)(AiListp + Offset);
                    float                         val = (ai->HEALTH) / 255.0f;
                    if (val > currentPlayerGetHealth())
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFBondHealthLessThanRecord);
                    }
                    break;
                }
                case AI_IFBondHealthGreaterThan:
                {
                    AiIFBondHealthGreaterThanRecord *ai  = (AiIFBondHealthGreaterThanRecord *)(AiListp + Offset);
                    float                            val = (ai->HEALTH) / 255.0f;
                    if (val < currentPlayerGetHealth())
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFBondHealthGreaterThanRecord);
                    }
                    break;
                }
                case AI_IFGameDifficultyLessThan:
                {
                    AiIFGameDifficultyLessThanRecord *ai = (AiIFGameDifficultyLessThanRecord *)(AiListp + Offset);
                    if (ai->DIFICULTY_ID > lvGetDifficulty())
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFGameDifficultyLessThanRecord);
                    }
                    break;
                }
                case AI_IFGameDifficultyGreaterThan:
                {
                    AiIFGameDifficultyGreaterThanRecord *ai = (AiIFGameDifficultyGreaterThanRecord *)(AiListp + Offset);
                    if (ai->DIFICULTY_ID < lvGetDifficulty())
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFGameDifficultyGreaterThanRecord);
                    }
                    break;
                }
                case AI_IFMissionTimeLessThan:
                {
                    AiIFMissionTimeLessThanRecord *ai     = (AiIFMissionTimeLessThanRecord *)(AiListp + Offset);
                    f32                            target = ntohs(ai->SECONDS);
                    if (target > lvGetMissionTimeSec())
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFMissionTimeLessThanRecord);
                    }
                    break;
                }
                case AI_IFMissionTimeGreaterThan:
                {
                    AiIFMissionTimeGreaterThanRecord *ai     = (AiIFMissionTimeGreaterThanRecord *)(AiListp + Offset);
                    f32                               target = ntohs(ai->SECONDS);
                    if (target < lvGetMissionTimeSec())
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFMissionTimeGreaterThanRecord);
                    }
                    break;
                }
                case AI_IFSystemPowerTimeLessThan:
                {
                    AiIFSystemPowerTimeLessThanRecord *ai     = (AiIFSystemPowerTimeLessThanRecord *)(AiListp + Offset);
                    f32                                target = ntohs(ai->MINUTES) * CHRAI_TICKRATE_F;
                    if (target > lvGetSystemPowerTimeSec())
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFSystemPowerTimeLessThanRecord);
                    }
                    break;
                }
                case AI_IFSystemPowerTimeGreaterThan:
                {
                    AiIFSystemPowerTimeGreaterThanRecord *ai     = (AiIFSystemPowerTimeGreaterThanRecord *)(AiListp + Offset);
                    f32                                   target = ntohs(ai->MINUTES) * CHRAI_TICKRATE_F;
                    if (target < lvGetSystemPowerTimeSec())
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFSystemPowerTimeGreaterThanRecord);
                    }
                    break;
                }
                case AI_IFLevelIdLessThan:
                {
                    AiIFLevelIdLessThanRecord *ai = (AiIFLevelIdLessThanRecord *)(AiListp + Offset);
                    if (ai->LEVEL_ID > bossGetStageNum())
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFLevelIdLessThanRecord);
                    }
                    break;
                }
                case AI_IFLevelIdGreaterThan:
                {
                    AiIFLevelIdGreaterThanRecord *ai = (AiIFLevelIdGreaterThanRecord *)(AiListp + Offset);
                    if (ai->LEVEL_ID < bossGetStageNum())
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFLevelIdGreaterThanRecord);
                    }
                    break;
                }
                case AI_SetMyMorale:
                {
                    AiSetMyMoraleRecord *ai = (AiSetMyMoraleRecord *)(AiListp + Offset);
                    ChrEntityp->morale      = ai->val;
                    Offset += sizeof(AiSetMyMoraleRecord);
                    break;
                }
                case AI_AddToMyMorale:
                {
                    AiAddToMyMoraleRecord *ai = (AiAddToMyMoraleRecord *)(AiListp + Offset);
                    if (255 - ai->val < ChrEntityp->morale) // clamp to 255
                    {
                        ChrEntityp->morale = 255; // max
                    }
                    else
                    {
                        ChrEntityp->morale += ai->val;
                    }

                    Offset += sizeof(AiAddToMyMoraleRecord);
                    break;
                }
                case AI_SubtractFromMyMorale:
                {
                    AiSubtractFromMyMoraleRecord *ai = (AiSubtractFromMyMoraleRecord *)(AiListp + Offset);
                    if (ai->val > ChrEntityp->morale) // clamp to 0
                    {
                        ChrEntityp->morale = 0;
                    }
                    else
                    {
                        ChrEntityp->morale -= ai->val;
                    }
                    Offset += sizeof(AiSubtractFromMyMoraleRecord);
                    break;
                }
                case AI_IFMyMoraleLessThan:
                {
                    AiIFMyMoraleLessThanRecord *ai = (AiIFMyMoraleLessThanRecord *)(AiListp + Offset);
                    if (ai->val > ChrEntityp->morale)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFMyMoraleLessThanRecord);
                    }
                    break;
                }
                case AI_IFMyMoraleLessThanRandom:
                {
                    AiIFMyMoraleLessThanRandomRecord *ai = (AiIFMyMoraleLessThanRandomRecord *)(AiListp + Offset);
                    if (ChrEntityp->morale < ChrEntityp->random)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFMyMoraleLessThanRandomRecord);
                    }
                    break;
                }
                case AI_SetMyAlertness:
                {
                    AiSetMyAlertnessRecord *ai = (AiSetMyAlertnessRecord *)(AiListp + Offset);
                    ChrEntityp->alertness      = ai->val;
                    Offset += sizeof(AiSetMyAlertnessRecord);
                    break;
                }
                case AI_AddToMyAlertness:
                {
                    AiAddToMyAlertnessRecord *ai = (AiAddToMyAlertnessRecord *)(AiListp + Offset);
                    if (255 - ai->val < ChrEntityp->alertness) // clamp to 255
                    {
                        ChrEntityp->alertness = 255; // max
                    }
                    else
                    {
                        ChrEntityp->alertness += ai->val;
                    }
                    Offset += sizeof(AiAddToMyAlertnessRecord);
                    break;
                }
                case AI_SubtractFromMyAlertness:
                {
                    AiSubtractFromMyAlertnessRecord *ai = (AiSubtractFromMyAlertnessRecord *)(AiListp + Offset);
                    if (ai->val > ChrEntityp->alertness) // clamp to 0
                    {
                        ChrEntityp->alertness = 0;
                    }
                    else
                    {
                        ChrEntityp->alertness -= ai->val;
                    }
                    Offset += sizeof(AiSubtractFromMyAlertnessRecord);
                    break;
                }
                case AI_IFMyAlertnessLessThan:
                {
                    AiIFMyAlertnessLessThanRecord *ai = (AiIFMyAlertnessLessThanRecord *)(AiListp + Offset);
                    if (ai->CHRBYTE > ChrEntityp->alertness)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFMyAlertnessLessThanRecord);
                    }
                    break;
                }
                case AI_IFMyAlertnessLessThanRandom:
                {
                    AiIFMyAlertnessLessThanRandomRecord *ai = (AiIFMyAlertnessLessThanRandomRecord *)(AiListp + Offset);
                    if (ChrEntityp->alertness < ChrEntityp->random)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFMyAlertnessLessThanRandomRecord);
                    }
                    break;
                }
                case AI_SetMyHearingScale:
                {
                    AiSetMyHearingScaleRecord *ai       = (AiSetMyHearingScaleRecord *)(AiListp + Offset);
                    f32                        distance = ntohs(ai->HEARING_SCALE) / 1000.0f;
                    ChrEntityp->hearingscale            = distance;
                    Offset += sizeof(AiSetMyHearingScaleRecord);
                    break;
                }
                case AI_SetMyVisionRange:
                {
                    AiSetMyVisionRangeRecord *ai = (AiSetMyVisionRangeRecord *)(AiListp + Offset);
                    ChrEntityp->visionrange      = (ai->VISION_RANGE);
                    Offset += sizeof(AiSetMyVisionRangeRecord);
                    break;
                }
                case AI_SetMyGrenadeProbability:
                {
                    AiSetMyGrenadeProbabilityRecord *ai = (AiSetMyGrenadeProbabilityRecord *)(AiListp + Offset);
                    ChrEntityp->grenadeprob             = ai->GRENADE_PROB;
                    Offset += sizeof(AiSetMyGrenadeProbabilityRecord);
                    break;
                }
                case AI_SetMyChrNum:
                {
                    AiSetMyChrNumRecord *ai = (AiSetMyChrNumRecord *)(AiListp + Offset);
                    ChrEntityp->chrnum      = ai->CHR_NUM;
                    chrSlotSync(ChrEntityp);
                    Offset += sizeof(AiSetMyChrNumRecord);
                    break;
                }
                case AI_SetMyHealthTotal:
                {
                    AiSetMyHealthTotalRecord *ai     = (AiSetMyHealthTotalRecord *)(AiListp + Offset);
                    f32                       amount = ntohs(ai->HEALTH) * 0.1f;
                    chrSetMaxDamage(ChrEntityp, amount);
                    Offset += sizeof(AiSetMyHealthTotalRecord);
                    break;
                }
                case AI_SetMyArmour:
                {
                    AiSetMyArmourRecord *ai     = (AiSetMyArmourRecord *)(AiListp + Offset);
                    f32                  amount = ntohs(ai->AMOUNT) * 0.1f; /*if (cheatIsActive(CHEAT_ENEMYSHIELDS)) { amount = amount < 8 ? 8 : amount; } */
                    chrAddHealth(ChrEntityp, amount);
                    Offset += sizeof(AiSetMyArmourRecord);
                    break;
                }
                case AI_SetMySpeedRating:
                {
                    AiSetMySpeedRatingRecord *ai = (AiSetMySpeedRatingRecord *)(AiListp + Offset);
                    ChrEntityp->speedrating = ai->val;
                    Offset += sizeof(AiSetMySpeedRatingRecord);
                    break;
                }
                case AI_SetMyArghRating:
                {
                    AiSetMyArghRatingRecord *ai = (AiSetMyArghRatingRecord *)(AiListp + Offset);
                    ChrEntityp->arghrating = ai->val;
                    Offset += sizeof(AiSetMyArghRatingRecord);
                    break;
                }
                case AI_SetMyAccuracyRating:
                {
                    AiSetMyAccuracyRatingRecord *ai = (AiSetMyAccuracyRatingRecord *)(AiListp + Offset);
                    ChrEntityp->accuracyrating      = ai->val;
                    Offset += sizeof(AiSetMyAccuracyRatingRecord);
                    break;
                }
                case AI_SetMyFlags2:
                {
                    AiSetMyFlags2Record *ai = (AiSetMyFlags2Record *)(AiListp + Offset);
                    ChrEntityp->flags2 |= ai->BITS;
                    Offset += sizeof(AiSetMyFlags2Record);
                    break;
                }
                case AI_UnsetMyFlags2:
                {
                    AiUnsetMyFlags2Record *ai = (AiUnsetMyFlags2Record *)(AiListp + Offset);
                    ChrEntityp->flags2 &= ~ai->BITS;
                    Offset += sizeof(AiUnsetMyFlags2Record);
                    break;
                }
                case AI_IFMyFlags2Has:
                {
                    AiIFMyFlags2HasRecord *ai = (AiIFMyFlags2HasRecord *)(AiListp + Offset);
                    if (ChrEntityp->flags2 & ai->BITS)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFMyFlags2HasRecord);
                    }
                    break;
                }
                case AI_SetChrBitfield:
                {
                    AiSetChrBitfieldRecord *ai = (AiSetChrBitfieldRecord *)(AiListp + Offset);
                    chrSetFlags2ById(ChrEntityp, ai->CHR_NUM, ai->BITS);
                    Offset += sizeof(AiSetChrBitfieldRecord);
                    break;
                }
                case AI_UnsetChrBitfield:
                {
                    AiUnsetChrBitfieldRecord *ai = (AiUnsetChrBitfieldRecord *)(AiListp + Offset);
                    chrUnsetFlags2ById(ChrEntityp, ai->CHR_NUM, ai->BITS);
                    Offset += sizeof(AiUnsetChrBitfieldRecord);
                    break;
                }
                case AI_IFChrBitfieldHas:
                {
                    AiIFChrBitfieldHasRecord *ai = (AiIFChrBitfieldHasRecord *)(AiListp + Offset);
                    if (chrHasFlags2ById(ChrEntityp, ai->CHR_NUM, ai->BITS))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFChrBitfieldHasRecord);
                    }
                    break;
                }
                case AI_SetObjectiveBitfield:
                {
                    AiSetObjectiveBitfieldRecord *ai    = (AiSetObjectiveBitfieldRecord *)(AiListp + Offset);
                    s32                           flags = ntohl(ai->BITFIELD);
                    chrSetStageFlags(flags);
                    Offset += sizeof(AiSetObjectiveBitfieldRecord);
                    break;
                }
                case AI_UnsetObjectiveBitfield:
                {
                    AiUnsetObjectiveBitfieldRecord *ai    = (AiUnsetObjectiveBitfieldRecord *)(AiListp + Offset);
                    s32                             flags = ntohl(ai->BITFIELD);
                    chrUnsetStageFlags(flags);
                    Offset += sizeof(AiUnsetObjectiveBitfieldRecord);
                    break;
                }
                case AI_IFObjectiveBitfieldHas:
                {
                    AiIFObjectiveBitfieldHasRecord *ai    = (AiIFObjectiveBitfieldHasRecord *)(AiListp + Offset);
                    u32                             flags = ntohl(ai->BITS);
                    if (chrHasStageFlag(flags)) /* PD && ai->val[4] == 1) || (!chrHasStageFlag(flags) && ai->val[4] == 0  * */
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFObjectiveBitfieldHasRecord);
                    }
                    break;
                }
                case AI_SetMychrflags:
                {
                    AiSetMychrflagsRecord *ai    = (AiSetMychrflagsRecord *)(AiListp + Offset);
                    CHRFLAG                flags = ntohl(ai->CHRFLAGS);
                    ChrEntityp->chrflags |= flags;
                    Offset += sizeof(AiSetMychrflagsRecord);
                    break;
                }
                case AI_UnsetMychrflags:
                {
                    AiUnsetMychrflagsRecord *ai    = (AiUnsetMychrflagsRecord *)(AiListp + Offset);
                    CHRFLAG                  flags = ntohl(ai->CHRFLAGS);
                    ChrEntityp->chrflags &= ~flags;
                    Offset += sizeof(AiUnsetMychrflagsRecord);
                    break;
                }
                case AI_IFMychrflagsHas:
                {
                    AiIFMychrflagsHasRecord *ai    = (AiIFMychrflagsHasRecord *)(AiListp + Offset);
                    CHRFLAG                  flags = ntohl(ai->CHRFLAGS);
                    if ((ChrEntityp->chrflags & flags) == flags)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFMychrflagsHasRecord);
                    }
                    break;
                }
                case AI_SetChrchrflags:
                {
                    AiSetChrchrflagsRecord *ai    = (AiSetChrchrflagsRecord *)(AiListp + Offset);
                    CHRFLAG                 flags = ntohl(ai->CHRFLAGS);
                    ChrRecord              *chr   = chrFindById(ChrEntityp, ai->CHR_NUM);
                    if (chr)
                    {
                        chr->chrflags |= flags;
                    }
                    Offset += sizeof(AiSetChrchrflagsRecord);
                    break;
                }
                case AI_UnsetChrchrflags:
                {
                    AiUnsetChrchrflagsRecord *ai    = (AiUnsetChrchrflagsRecord *)(AiListp + Offset);
                    CHRFLAG                   flags = ntohl(ai->CHRFLAGS);
                    ChrRecord                *chr   = chrFindById(ChrEntityp, ai->CHR_NUM);
                    if (chr)
                    {
                        chr->chrflags &= ~flags;
                    }
                    Offset += sizeof(AiUnsetChrchrflagsRecord);
                    break;
                }
                case AI_IFChrchrflagsHas:
                {
                    AiIFChrchrflagsHasRecord *ai    = (AiIFChrchrflagsHasRecord *)(AiListp + Offset);
                    CHRFLAG                   flags = ntohl(ai->CHRFLAGS);
                    ChrRecord                *chr   = chrFindById(ChrEntityp, ai->CHR_NUM);
                    if (chr && (chr->chrflags & flags) == flags)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFChrchrflagsHasRecord);
                    }
                    break;
                }
                case AI_SetObjectFlags:
                {
                    AiSetObjectFlagsRecord *ai    = (AiSetObjectFlagsRecord *)(AiListp + Offset);
                    s32                     flags = ntohl(ai->BITFIELD);
                    ObjectRecord           *obj   = objFindByTagId(ai->OBJECT_TAG);
                    if (obj && obj->prop)
                    {
                        obj->flags |= flags;
                    }
                    Offset += sizeof(AiSetObjectFlagsRecord);
                    break;
                }
                case AI_UnsetObjectFlags:
                {
                    AiUnsetObjectFlagsRecord *ai    = (AiUnsetObjectFlagsRecord *)(AiListp + Offset);
                    s32                       flags = ntohl(ai->BITFIELD);
                    ObjectRecord             *obj   = objFindByTagId(ai->OBJECT_TAG);
                    if (obj && obj->prop)
                    {
                        obj->flags &= ~flags;
                    }
                    Offset += sizeof(AiUnsetObjectFlagsRecord);
                    break;
                }
                case AI_IFObjectFlagsHas:
                {
                    AiIFObjectFlagsHasRecord *ai    = (AiIFObjectFlagsHasRecord *)(AiListp + Offset);
                    u32                       flags = ntohl(ai->BITS);
                    ObjectRecord             *obj   = objFindByTagId(ai->OBJECT_TAG);
                    if (obj && obj->prop && (obj->flags & flags) == flags)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFObjectFlagsHasRecord);
                    }
                    break;
                }
                case AI_SetObjectFlags2:
                {
                    AiSetObjectFlags2Record *ai    = (AiSetObjectFlags2Record *)(AiListp + Offset);
                    u32                      flags = ntohl(ai->BITS);
                    ObjectRecord            *obj   = objFindByTagId(ai->OBJECT_TAG);
                    if (obj && obj->prop)
                    {
                        obj->flags2 |= flags;
                    }
                    Offset += sizeof(AiSetObjectFlags2Record);
                    break;
                }
                case AI_UnsetObjectFlags2:
                {
                    AiUnsetObjectFlags2Record *ai    = (AiUnsetObjectFlags2Record *)(AiListp + Offset);
                    u32                        flags = ntohl(ai->BITS);
                    ObjectRecord              *obj   = objFindByTagId(ai->OBJECT_TAG);
                    if (obj && obj->prop)
                    {
                        obj->flags2 &= ~flags;
                    }
                    Offset += sizeof(AiUnsetObjectFlags2Record);
                    break;
                }
                case AI_IFObjectFlags2Has:
                {
                    AiIFObjectFlags2HasRecord *ai    = (AiIFObjectFlags2HasRecord *)(AiListp + Offset);
                    u32                        flags = ntohl(ai->BITFIELD);
                    ObjectRecord              *obj   = objFindByTagId(ai->OBJECT_TAG);
                    if (obj && obj->prop && ((obj->flags2 & flags) == flags))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFObjectFlags2HasRecord);
                    }
                    break;
                }
                case AI_SetMyChrPreset:
                {
                    AiSetMyChrPresetRecord *ai = (AiSetMyChrPresetRecord *)(AiListp + Offset);
                    chrSetChrPreset(ChrEntityp, ai->PRESET);
                    Offset += sizeof(AiSetMyChrPresetRecord);
                    break;
                }
                case AI_SetChrChrPreset:
                {
                    AiSetChrChrPresetRecord *ai = (AiSetChrChrPresetRecord *)(AiListp + Offset);
                    chrSetChrPresetOf(ChrEntityp, ai->CHR_NUM, ai->PRESET);
                    Offset += sizeof(AiSetChrChrPresetRecord);
                    break;
                }
                case AI_SetMyPadPreset:
                {
                    AiSetMyPadPresetRecord *ai     = (AiSetMyPadPresetRecord *)(AiListp + Offset);
                    u16                     pad_id = ntohs(ai->PAD_PRESET);
                    if (ChrEntityp)
                    {
                        chrSetPadPreset(ChrEntityp, pad_id);
                    }
                    else if (AircraftEntityp)
                    {
                        AircraftEntityp->pad = pad_id;
                    }
                    Offset += sizeof(AiSetMyPadPresetRecord);
                    break;
                }
                case AI_SetChrPadPreset:
                {
                    AiSetChrPadPresetRecord *ai     = (AiSetChrPadPresetRecord *)(AiListp + Offset);
                    u16                      pad_id = ntohs(ai->PAD_PRESET);
                    chrSetPadPresetByChrnum(ChrEntityp, ai->CHR_NUM, pad_id);
                    Offset += sizeof(AiSetChrPadPresetRecord);
                    break;
                }
                case AI_PRINT:
                {
                    Offset += aiCommandSize(AiListp, Offset);
                    break;
                }
                case AI_MyTimerStart:
                {
                    chrRestartTimer(ChrEntityp); // Set timer60 to 0 and set flag
                    Offset += sizeof(AiMyTimerStartRecord);
                    break;
                }
                case AI_MyTimerReset:
                {
                    ChrEntityp->timer60 = 0;
                    Offset += sizeof(AiMyTimerResetRecord);
                    break;
                }
                case AI_MyTimerPause:
                {
                    ChrEntityp->hidden &= ~CHRHIDDEN_TIMER_ACTIVE;
                    Offset += sizeof(AiMyTimerPauseRecord);
                    break;
                }
                case AI_MyTimerResume:
                {
                    ChrEntityp->hidden |= CHRHIDDEN_TIMER_ACTIVE;
                    Offset += sizeof(AiMyTimerResumeRecord);
                    break;
                }
                case AI_IFMyTimerIsNotRunning:
                {
                    AiIFMyTimerIsNotRunningRecord *ai = (AiIFMyTimerIsNotRunningRecord *)(AiListp + Offset);
                    if (((ChrEntityp->hidden & CHRHIDDEN_TIMER_ACTIVE) == 0))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFMyTimerIsNotRunningRecord);
                    }
                    break;
                }
                case AI_IFMyTimerLessThanTicks:
                {
                    AiIFMyTimerLessThanTicksRecord *ai   = (AiIFMyTimerLessThanTicksRecord *)(AiListp + Offset);
                    f32                             valf = ((unsigned)CharArrayTo24(((unsigned char *)(&(ai->TICKS))), 0)) / CHRAI_TICKRATE_F;

                    if (chrGetTimer(ChrEntityp) < valf)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFMyTimerLessThanTicksRecord);
                    }
                    break;
                }
                case AI_IFMyTimerGreaterThanTicks:
                {
                    AiIFMyTimerGreaterThanTicksRecord *ai   = (AiIFMyTimerGreaterThanTicksRecord *)(AiListp + Offset);
                    f32                                valf = ((unsigned)CharArrayTo24(((unsigned char *)(&(ai->TICKS))), 0)) / CHRAI_TICKRATE_F;
                    if (chrGetTimer(ChrEntityp) > valf)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFMyTimerGreaterThanTicksRecord);
                    }
                    break;
                }
                case AI_HudCountdownShow:
                {
                    countdownTimerSetVisible(COUNTDOWN_HIDE_SCRIPT, TRUE);
                    Offset += sizeof(AiHudCountdownShowRecord);
                    break;
                }
                case AI_HudCountdownHide:
                {
                    countdownTimerSetVisible(COUNTDOWN_HIDE_SCRIPT, FALSE);
                    Offset += sizeof(AiHudCountdownHideRecord);
                    break;
                }
                case AI_HudCountdownSet:
                {
                    AiHudCountdownSetRecord *ai      = (AiHudCountdownSetRecord *)(AiListp + Offset);
                    f32                      seconds = ntohs(ai->SECONDS);
                    countdownTimerSetValue(seconds * CHRAI_TICKRATE_F);
                    Offset += sizeof(AiHudCountdownSetRecord);
                    break;
                }
                case AI_HudCountdownStop:
                {
                    countdownTimerSetRunning(FALSE);
                    Offset += sizeof(AiHudCountdownStopRecord);
                    break;
                }
                case AI_HudCountdownStart:
                {
                    countdownTimerSetRunning(TRUE);
                    Offset += sizeof(AiHudCountdownStartRecord);
                    break;
                }
                case AI_IFHudCountdownIsNotRunning:
                {
                    AiIFHudCountdownIsNotRunningRecord *ai = (AiIFHudCountdownIsNotRunningRecord *)(AiListp + Offset);
                    if (!countdownTimerIsRunning())
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFHudCountdownIsNotRunningRecord);
                    }
                    break;
                }
                case AI_IFHudCountdownLessThan:
                {
                    AiIFHudCountdownLessThanRecord *ai    = (AiIFHudCountdownLessThanRecord *)(AiListp + Offset);
                    f32                             value = ntohs(ai->SECONDS);
                    if (countdownTimerGetValue() < value * CHRAI_TICKRATE_F)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFHudCountdownLessThanRecord);
                    }
                    break;
                }
                case AI_IFHudCountdownGreaterThan:
                {
                    AiIFHudCountdownGreaterThanRecord *ai    = (AiIFHudCountdownGreaterThanRecord *)(AiListp + Offset);
                    f32                                value = ntohs(ai->SECONDS);
                    if (countdownTimerGetValue() > value * CHRAI_TICKRATE_F)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFHudCountdownGreaterThanRecord);
                    }
                    break;
                }
                case AI_TRYSpawningChrAtPad:
                {
                    AiTRYSpawningChrAtPadRecord *ai       = (AiTRYSpawningChrAtPadRecord *)(AiListp + Offset);
                    u16                          pad      = ntohs(ai->PAD);
                    CHRFLAG                      flags    = ntohl(ai->BITFIELD);
                    u16                          ailistid = ntohs(ai->AI_LIST_ID);
                    AIRecord                    *ailist   = ailistFindById(ailistid);
                    if (chrSpawnAtPad(ChrEntityp, ai->BODY_NUM, ai->HEAD_NUM, pad, ailist, flags))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYSpawningChrAtPadRecord);
                    }
                    break;
                }
                case AI_TRYSpawningChrNextToChr:
                {
                    AiTRYSpawningChrNextToChrRecord *ai       = (AiTRYSpawningChrNextToChrRecord *)(AiListp + Offset);
                    CHRFLAG                          flags    = ntohl(ai->BITFIELD);
                    u16                              ailistid = ntohs(ai->AI_LIST_ID);
                    AIRecord                        *ailist   = ailistFindById(ailistid);
                    if (chrSpawnAtChr(ChrEntityp, ai->BODY_NUM, ai->HEAD_NUM, ai->CHR_NUM_TARGET, ailist, flags))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYSpawningChrNextToChrRecord);
                    }
                    break;
                }
                case AI_TRYGiveMeItem:
                {
                    AiTRYGiveMeItemRecord *ai    = (AiTRYGiveMeItemRecord *)(AiListp + Offset);
                    s32                    flags = ntohl(ai->PROPFLAG);
                    s32                    model = ntohs(ai->PROP_NUM);
                    PropRecord            *prop  = NULL;

                    if (ChrEntityp && ChrEntityp->prop && ChrEntityp->model)
                    {
                        /*  more nice PD code that might be usefull in future
                        if (cheatIsActive(CHEAT_MARQUIS))
                        {
                            flags &= ~0x10000000;
                            flags |= 0x20000000;
                            prop = chrGiveWeapon(ChrEntityp, model, ai->val[2], flags);
                        }
                        else
                        */
                        if (cheatIsActive(28)) // CHEAT_ENEMYROCKETS
                        {
                            switch (ai->ITEM_NUM) // ITEM_IDS
                            {
                                case ITEM_KNIFE:
                                case ITEM_THROWKNIFE:
                                case ITEM_WPPK:
                                case ITEM_WPPKSIL:
                                case ITEM_TT33:
                                case ITEM_SKORPION:
                                case ITEM_AK47:
                                case ITEM_UZI:
                                case ITEM_MP5K:
                                case ITEM_MP5KSIL:
                                case ITEM_SPECTRE:
                                case ITEM_M16:
                                case ITEM_FNP90:
                                case ITEM_SHOTGUN:
                                case ITEM_AUTOSHOT:
                                case ITEM_SNIPERRIFLE:
                                case ITEM_RUGER:
                                case ITEM_GOLDENGUN:
                                case ITEM_SILVERWPPK:
                                case ITEM_GOLDWPPK:
                                case ITEM_LASER:
                                case ITEM_WATCHLASER:
                                case ITEM_REMOTEMINE:
                                case ITEM_TRIGGER:
                                case ITEM_TASER:

                                    prop = chrGiveWeapon(ChrEntityp, PROP_CHRROCKETLAUNCH, ITEM_ROCKETLAUNCH, flags);
                                    //! Bug, No Break! relying on chrGiveWeapon checking weapon already given
                                    [[fallthrough]];
                                case ITEM_TIMEDMINE:
                                case ITEM_PROXIMITYMINE:
                                default:
                                    prop = chrGiveWeapon(ChrEntityp, model, ai->ITEM_NUM, flags);
                                    break;
                            }
                        }
                        else
                        {
                            prop = chrGiveWeapon(ChrEntityp, model, ai->ITEM_NUM, flags);
                        }
                    }
                    if (prop)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYGiveMeItemRecord);
                    }
                    break;
                }
                case AI_TRYGiveMeHat:
                {
                    AiTRYGiveMeHatRecord *ai       = (AiTRYGiveMeHatRecord *)(AiListp + Offset);
                    s32                   flags    = ntohl(ai->PROP_BITFIELD);
                    s32                   modelnum = ntohs(ai->PROP_NUM);
                    bool                  ok       = FALSE;
                    if (ChrEntityp && ChrEntityp->prop && ChrEntityp->model)
                    {
                        ok = hatCreateForChr(ChrEntityp, modelnum, flags);
                    }
                    if (ok)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYGiveMeHatRecord);
                    }
                    break;
                }
                case AI_TRYCloningChr:
                {
                    AiTRYCloningChrRecord *ai       = (AiTRYCloningChrRecord *)(AiListp + Offset);
                    u16                    ailistid = ntohs(ai->AI_LIST_ID);
                    AIRecord              *ailist   = ailistFindById((u16)ailistid);
                    ChrRecord             *chr      = chrFindById(ChrEntityp, ai->CHR_NUM);
                    bool                   pass     = FALSE;
                    int                    chrnum;
                    PropRecord            *srcweaponLprop   = NULL;
                    PropRecord            *srcweaponRprop   = NULL;
                    PropRecord            *cloneweaponRprop = NULL;
                    PropRecord            *cloneweaponLprop = NULL;
                    PropRecord            *cloneprop        = NULL;
                    ChrRecord             *clone            = NULL;
                    WeaponObjRecord       *srcweaponL       = NULL;
                    WeaponObjRecord       *cloneweaponL     = NULL;
                    WeaponObjRecord       *cloneweaponR     = NULL;
                    WeaponObjRecord       *srcweaponR       = NULL;
                    PropRecord            *hatprop;
                    ObjectRecord          *hatobj;
                    // bool tryhat;
                    if (chr && (chr->chrflags & CHRFLAG_CLONE))
                    {
                        cloneprop = chrSpawnAtChr(ChrEntityp, chr->bodynum, -1, chr->chrnum, ailist, 0);
                        if (cloneprop)
                        {
                            clone  = cloneprop->chr;
                            chrnum = chr->chrnum + 10000;
                            if (!chrFindById(ChrEntityp, chrnum))
                            {
                                clone->chrnum = chrnum;
                                chrSlotSync(clone);
                            }
                            // chrSetChrnum(clone, getLowestUnusedChrId());
                            // chr->chrdup = clone->chrnum;
                            srcweaponRprop = chrGetEquippedWeaponProp(chr, GUNRIGHT);
                            if (srcweaponRprop)
                            {
                                srcweaponR       = srcweaponRprop->weapon;
                                cloneweaponRprop = chrGiveWeapon(clone, srcweaponR->obj, srcweaponR->weaponnum, 0);
                                if (cloneweaponRprop)
                                {
                                    cloneweaponR = cloneweaponRprop->weapon;
                                }
                            }

                            srcweaponLprop = chrGetEquippedWeaponProp(chr, GUNLEFT);
                            if (srcweaponLprop)
                            {
                                srcweaponL       = srcweaponLprop->weapon;
                                cloneweaponLprop = chrGiveWeapon(clone, srcweaponL->obj, srcweaponL->weaponnum, 0x10000000);
                                if (cloneweaponLprop)
                                {
                                    cloneweaponL = cloneweaponLprop->weapon;
                                }
                            }

                            if (srcweaponL && srcweaponR && cloneweaponL && cloneweaponR && srcweaponR == srcweaponL->dualweapon && srcweaponL == srcweaponR->dualweapon)
                            {
                                weaponSetDual(cloneweaponL, cloneweaponR);
                            }
                            {
                                hatprop = chr->handle_positiondata_hat;
                                if (hatprop)
                                {
                                    hatobj = hatprop->obj;

                                    hatCreateForChr(clone, hatobj->obj, 0);
                                }
                            }
                            /* PD extras */
                            // clone->morale     = chr->morale;
                            // clone->alertness  = chr->alertness;
                            // clone->padpreset1 = chr->padpreset1;
                            pass = TRUE;
                        }
                    }
                    if (pass)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYCloningChrRecord);
                    }
                    break;
                }
                case AI_TextPrintBottom:
                {
                    AiTextPrintBottomRecord *ai   = (AiTextPrintBottomRecord *)(AiListp + Offset);
                    char                    *text = langGet(ntohs(ai->txt));
                    s32 prevplayer = counterOpTextBegin();
    #ifdef BUGFIX_R1
                    hudmsgBottomShowText(text);
    #else
                    hudmsgBottomShow(text);
    #endif
                    playerSetCurrent(prevplayer);
                    Offset += sizeof(AiTextPrintBottomRecord);
                    break;
                }
                case AI_TextPrintTop:
                {
                    AiTextPrintTopRecord *ai   = (AiTextPrintTopRecord *)(AiListp + Offset);
                    char                 *text = langGet(ntohs(ai->txt));


                    s32 prevplayer = counterOpTextBegin();
                    hudmsgTopShow(text);
                    playerSetCurrent(prevplayer);
                    Offset += sizeof(AiTextPrintTopRecord);
                    break;
                }
                case AI_SfxPlay:
                {
                    AiSfxPlayRecord *ai       = (AiSfxPlayRecord *)(AiListp + Offset);
                    s16              audio_id = ntohs(ai->SOUND_NUM);
                    aiSoundPlay((s8)ai->CHANNEL_NUM, audio_id);
                    Offset += sizeof(AiSfxPlayRecord);
                    break;
                }

                case AI_SfxStopChannel:
                {
                    AiSfxStopChannelRecord *ai = (AiSfxStopChannelRecord *)(AiListp + Offset);
                    aiSoundStop(ai->CHANNEL_NUM);
                    Offset += sizeof(AiSfxStopChannelRecord);
                    break;
                }
                case AI_SfxSetChannelVolume:
                {
                    AiSfxSetChannelVolumeRecord *ai    = (AiSfxSetChannelVolumeRecord *)(AiListp + Offset);
                    s16                          vol   = ntohs(ai->TARGET_VOLUME);
                    u16                          fadeTime = ntohs(ai->FADE_TIME);
                    if (ai->slotID >= 0 && ai->slotID < AI_SOUND_SLOTS)
                    {
                        g_AiSounds[ai->slotID].fadeTime = aiSoundFadeTicks(fadeTime);
                        g_AiSounds[ai->slotID].targetVolume = vol;
                        g_AiSounds[ai->slotID].pos = NULL;
                        g_AiSounds[ai->slotID].obj = NULL;
                        if (fadeTime == 0)
                        {
                            aiSoundTick(ai->slotID);
                        }
                    }
                    /*
                      "AI : Bad sound setup command\n"
                    */
                    Offset += sizeof(AiSfxSetChannelVolumeRecord);
                    break;
                }
                case AI_SfxFadeChannelVolume:
                {
                    AiSfxFadeChannelVolumeRecord *ai    = (AiSfxFadeChannelVolumeRecord *)(AiListp + Offset);
                    f32                           vol   = ntohs(ai->TARGET_VOLUME);
                    u16                           fadeTime = ntohs(ai->FADE_TIME);
                    /*
                        "fadeTo\n"
                     */
                    if (ai->slotID >= 0 && ai->slotID < AI_SOUND_SLOTS)
                    {
                        g_AiSounds[ai->slotID].fadeTime = aiSoundFadeTicks(fadeTime);
                        g_AiSounds[ai->slotID].targetVolume = sfxVolumeForDistance(vol);
                        g_AiSounds[ai->slotID].pos = NULL;
                        g_AiSounds[ai->slotID].obj = NULL;
                        if (fadeTime == 0)
                        {
                            aiSoundTick(ai->slotID);
                        }
                    }
                    Offset += sizeof(AiSfxFadeChannelVolumeRecord);
                    break;
                }
                case AI_SfxEmitFromObject:
                {
                    AiSfxEmitFromObjectRecord *ai    = (AiSfxEmitFromObjectRecord *)(AiListp + Offset);
                    ObjectRecord              *obj   = objFindByTagId(ai->OBJECT_TAG);
                    u16                        fadeTime = ntohs(ai->FADE_TIME);
                    if (ai->slotID >= 0 && ai->slotID < AI_SOUND_SLOTS && obj)
                    {
                        g_AiSounds[ai->slotID].fadeTime = aiSoundFadeTicks(fadeTime);
                        g_AiSounds[ai->slotID].pos = NULL;
                        g_AiSounds[ai->slotID].obj = obj;
                        if (fadeTime == 0)
                        {
                            aiSoundTick(ai->slotID);
                        }
                    }
                    Offset += sizeof(AiSfxEmitFromObjectRecord);
                    break;
                }
                case AI_SfxEmitFromPad:
                {
                    AiSfxEmitFromPadRecord *ai     = (AiSfxEmitFromPadRecord *)(AiListp + Offset);
                    u16                     padnum = ntohs(ai->PAD);
                    PadRecord              *pad;
                    u16                     fadeTime = ntohs(ai->FADE_TIME);
                    if (isNotBoundPad(padnum))
                    {
                        pad = &g_CurrentSetup.pads[padnum];
                    }
                    else
                    {
                        pad = (PadRecord *)&g_CurrentSetup.boundpads[getBoundPadNum(padnum)];
                    }
                    if (ai->slotID >= 0 && ai->slotID < AI_SOUND_SLOTS && pad)
                    {
                        g_AiSounds[ai->slotID].fadeTime = aiSoundFadeTicks(fadeTime);
                        g_AiSounds[ai->slotID].pos = &pad->pos;
                        g_AiSounds[ai->slotID].obj = NULL;
                        if (fadeTime == 0)
                        {
                            aiSoundTick(ai->slotID);
                        }
                    }
                    Offset += sizeof(AiSfxEmitFromPadRecord);
                    break;
                }

                case AI_IFSfxChannelVolumeLessThan:
                {
                    AiIFSfxChannelVolumeLessThanRecord *ai  = (AiIFSfxChannelVolumeLessThanRecord *)(AiListp + Offset);
                    s16                                 vol = ntohs(ai->VOLUME);
                    /*
                     * "ai_ifmusicqueueemptyjumpf : %s, State=%x (getlvleveltime60=%f)\n"
                     */

                    if ((ai->slotID >= 0) && (ai->slotID < AI_SOUND_SLOTS) && (g_AiSounds[ai->slotID].volume < vol))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFSfxChannelVolumeLessThanRecord);
                    }
                    break;
                }
                case AI_VehicleStartPath:
                {
                    AiVehicleStartPathRecord *ai   = (AiVehicleStartPathRecord *)(AiListp + Offset);
                    PathRecord               *path = pathFindById(ai->PATH_NUM);
                    if (VehichleEntityp)
                    {
                        VehichleEntityp->path     = path;
                        VehichleEntityp->nextstep = 0;
                    }
                    Offset += sizeof(AiVehicleStartPathRecord);
                    break;
                }
                case AI_VehicleSpeed:
                {
                    AiVehicleSpeedRecord *ai        = (AiVehicleSpeedRecord *)(AiListp + Offset);
                    f32                   speedtime = ntohs(ai->ACCELERATION_TIME60);
                    f32                   speedaim  = ntohs(ai->TOP_SPEED) * 100.0f / 15360.0f;
                    if (VehichleEntityp)
                    {
                        VehichleEntityp->speedaim    = speedaim;
                        VehichleEntityp->speedtime60 = speedtime;
                    }
                    Offset += sizeof(AiVehicleSpeedRecord);
                    break;
                }
                case AI_AircraftRotorSpeed:
                {
                    AiAircraftRotorSpeedRecord *ai        = (AiAircraftRotorSpeedRecord *)(AiListp + Offset);
                    f32                         speedtime = ntohs(ai->ACCELERATION_TIME60);
                    f32                         speedaim  = ntohs(ai->ROTOR_SPEED) * M_TAU_F / 3600.0f;
                    if (AircraftEntityp)
                    {
                        AircraftEntityp->rotaryspeedaim  = speedaim;
                        AircraftEntityp->rotaryspeedtime = speedtime;
                    }
                    Offset += sizeof(AiAircraftRotorSpeedRecord);
                    break;
                }
                case AI_IFCameraIsInIntro:
                {
                    AiIFCameraIsInIntroRecord *ai = (AiIFCameraIsInIntroRecord *)(AiListp + Offset);
                    if ((playerGetCameraMode() == 1) || (playerGetCameraMode() == 2))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFCameraIsInIntroRecord);
                    }
                    break;
                }
                case AI_IFCameraIsInBondSwirl:
                {
                    AiIFCameraIsInBondSwirlRecord *ai = (AiIFCameraIsInBondSwirlRecord *)(AiListp + Offset);
                    if (playerGetCameraMode() == 3)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFCameraIsInBondSwirlRecord);
                    }
                    break;
                }
                case AI_TvChangeScreenBank:
                {
                    AiTvChangeScreenBankRecord *ai  = (AiTvChangeScreenBankRecord *)(AiListp + Offset);
                    ObjectRecord               *obj = objFindByTagId(ai->OBJECT_TAG);
                    if (obj && obj->prop)
                    {
                        if (obj->type == PROPDEF_MONITOR)
                        {
                            MonitorObjRecord *sm = (MonitorObjRecord *)obj;
                            monitorSetScript(&sm->Monitor, ai->SCREEN_BANK);
                        }
                        else if (obj->type == PROPDEF_MULTI_MONITOR)
                        {
                            u8 slot = ai->SCREEN_INDEX;
                            if (slot < 4)
                            {
                                MultiMonitorObjRecord *mm = (MultiMonitorObjRecord *)obj; // need new size here 0x74 (116) + 0x80 (so monitor is obj + 74)
                                monitorSetScript(&mm->Monitor[slot], ai->SCREEN_BANK);
                            }
                        }
                    }
                    Offset += sizeof(AiTvChangeScreenBankRecord);
                    break;
                }
                case AI_IFBondInTank: // canonical name
                {
                    AiIFBondInTankRecord *ai = (AiIFBondInTankRecord *)(AiListp + Offset);
                    if (playerIsInTank() == TRUE)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFBondInTankRecord);
                    }
                    break;
                }
                case AI_EndLevel: // canonical name
                {
                    /*"aiEndLevel" */
                    if (cameraBufferToggle)
                    {
                        if (cameraFrameCounter2 == FALSE)
                        {
                            cameraFrameCounter2 = TRUE;
                        }
                    }
                    else
                    {
                        bossReturnTitleStage();
                    }
                    Offset += sizeof(AiEndLevelRecord);
                    break;
                }
                case AI_CameraReturnToBond:
                {
                    playerSetCameraMode(CAMERAMODE_FP_NOINPUT);
                    Offset += sizeof(AiCameraReturnToBondRecord);
                    break;
                }
                case AI_CameraLookAtBondFromPad:
                {
                    AiCameraLookAtBondFromPadRecord *ai     = (AiCameraLookAtBondFromPadRecord *)(AiListp + Offset);
                    u16                              padnum = ntohs(ai->PAD);
                    if (isNotBoundPad(padnum))
                    {
                        g_CameraLookAtBondPad = &g_CurrentSetup.pads[padnum];
                    }
                    else
                    {
                        g_CameraLookAtBondPad = (PadRecord *)&g_CurrentSetup.boundpads[getBoundPadNum(padnum)];
                    }
                    playerSetCameraMode(CAMERAMODE_POSEND);
                    Offset += sizeof(AiCameraLookAtBondFromPadRecord);
                    break;
                }
                case AI_CameraSwitch:
                {
                    AiCameraSwitchRecord *ai  = (AiCameraSwitchRecord *)(AiListp + Offset);
                    TagObjectRecord      *tag = tagFind(ai->OBJECT_TAG);
                    if (tag)
                    {
                        int TagIndex = tagGetCommandIndex((ObjectRecord *)tag); // get index
                        if (TagIndex >= 0)
                        {
                            CutsceneRecord *cdef = (CutsceneRecord *)setupGetPtrToCommandByIndex(tag->OffsetToObj + TagIndex); // get obj

                            g_CameraLookAtBondPad = NULL;
                            gBondViewCutscene       = cdef;
                            g_CutsceneCamLookAtBond = ntohs(ai->LOOK_AT_BOND_FLAG);
                            g_CutsceneCamUnusedFlag = ntohs(ai->UNUSED_FLAG);
                            playerSetCameraMode(CAMERAMODE_POSEND);
                        }
                    }
                    Offset += AI_CameraSwitch_LENGTH;
                    break;
                }
                case AI_IFBondYPosLessThan:
                {
                    AiIFBondYPosLessThanRecord *ai      = (AiIFBondYPosLessThanRecord *)(AiListp + Offset);
                    f32                         bondpos = (s16)ntohs(ai->Y_POS);
                    if (currentPlayerGetProp()->pos.y < bondpos)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFBondYPosLessThanRecord);
                    }
                    break;
                }
                case AI_BondDisableControl:
                {
                    AiBondDisableControlRecord *ai = (AiBondDisableControlRecord *)(AiListp + Offset);
                    gunSetSightVisible(GUNSIGHTREASON_NOCONTROL, FALSE);
                    gunSetGunAmmoVisible(GUNAMMOREASON_NOCONTROL, FALSE);
                    if (!(PLAYERFLAG_NOCONTROL & ai->val))
                    {
                        hudmsgsSetOff(PLAYERFLAG_NOCONTROL);
                    }
                    if (!(ai->val & PLAYERFLAG_LOCKCONTROLS))
                    {
                        hudmsgTopSetFlag(PLAYERFLAG_NOCONTROL);
                    }
                    if (!(ai->val & PLAYERFLAG_NOTIMER))
                    {
                        countdownTimerSetVisible(COUNTDOWN_HIDE_NOCONTROL, FALSE);
                    }
                    g_Mission.playerHasControl = FALSE;
                    Offset += sizeof(AiBondDisableControlRecord);
                    break;
                }
                case AI_BondEnableControl:
                {
                    gunSetSightVisible(GUNSIGHTREASON_NOCONTROL, TRUE);
                    gunSetGunAmmoVisible(GUNAMMOREASON_NOCONTROL, TRUE);
                    hudmsgsSetOn(PLAYERFLAG_NOCONTROL);
                    hudmsgTopClearFlag(2);
                    countdownTimerSetVisible(COUNTDOWN_HIDE_NOCONTROL, TRUE);
                    g_Mission.playerHasControl = TRUE;
                    Offset += sizeof(AiBondEnableControlRecord);
                    break;
                }
                case AI_TRYTeleportingChrToPad:
                {
                    AiTRYTeleportingChrToPadRecord *ai     = (AiTRYTeleportingChrToPadRecord *)(AiListp + Offset);
                    s32                             padnum = ntohs(ai->PAD);
                    ChrRecord                      *chr    = chrFindById(ChrEntityp, ai->CHR_NUM);
                    bool                            pass   = FALSE;
                    PadRecord                      *pad;
                    f32                             FacingDirection;
                    coord3d                         pos;
                    StandTile                      *stan;

                    if (chr)
                    {
                        padnum = chrResolvePadId(ChrEntityp, padnum);
                        if (isNotBoundPad(padnum))
                        {
                            pad = &g_CurrentSetup.pads[padnum];
                        }
                        else
                        {
                            pad = (PadRecord *)&g_CurrentSetup.boundpads[getBoundPadNum(padnum)];
                        }

                        FacingDirection = mathAtan2(pad->look.x, pad->look.z);
                        pos.x           = pad->pos.x;
                        pos.y           = pad->pos.y;
                        pos.z           = pad->pos.z;
                        // pos  = pad->pos; <-uses lw instead of lwc1
                        stan            = pad->stan;
                        propSetMoving(chr->prop, FALSE);

                        if (chrAdjustPosForSpawn(&pos, &stan, FacingDirection, TRUE))
                        {
                            {
                                chr->prop->pos.x = pos.x;
                                chr->prop->pos.y = pos.y;
                                chr->prop->pos.z = pos.z;
                            }
                            // chr->prop->pos  = pos;
                            chr->prop->stan = stan;
                            chr->chrflags   = chr->chrflags | CHRFLAG_INIT;
                            modelSetRootRotY(chr->model, FacingDirection);
                            modelSetRootPosition(chr->model, &pos);
                            chrDetectRooms(chr);
                            if (chr->prop == g_CurrentPlayer->prop)
                            {
                                g_CurrentPlayer->bond.collision_position.x = pos.x;
                                g_CurrentPlayer->bond.collision_position.y = pos.y;
                                g_CurrentPlayer->bond.collision_position.z = pos.z;

                                // g_CurrentPlayer->pos = pos;
                                g_CurrentPlayer->bond.current_tile_ptr = stan;
                            }
                            pass = TRUE;
                        }
                        propSetMoving(chr->prop, TRUE);
                    }
                    if (pass)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiTRYTeleportingChrToPadRecord);
                    }
                    break;
                }
                case AI_ScreenFadeToBlack:
                {
                    if (g_Mission.exitOnPress != 2)
                    {
                        currentPlayerSetFadeColour(0, 0, 0, 0);
                        currentPlayerSetFadeFrac(CHRAI_TICKRATE_F, 1);
                    }
                    Offset += sizeof(AiScreenFadeToBlackRecord);
                    break;
                }
                case AI_ScreenFadeFromBlack:
                {
                    if (g_Mission.exitOnPress != 2)
                    {
                        currentPlayerSetFadeColour(0, 0, 0, 1);
                        currentPlayerSetFadeFrac(CHRAI_TICKRATE_F, 0);
                    }
                    Offset += sizeof(AiScreenFadeFromBlackRecord);
                    break;
                }
                case AI_IFScreenFadeCompleted:
                {
                    AiIFScreenFadeCompletedRecord *ai = (AiIFScreenFadeCompletedRecord *)(AiListp + Offset);
                    if (g_CurrentPlayer->colourfadetimemax60 < 0)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFScreenFadeCompletedRecord);
                    }
                    break;
                }
                case AI_HideAllChrs:
                {
                    s32 num;
                    for (num = chrsGetNumSlots() - 1; num >= 0; num--)
                    {
                        if (g_ChrSlots[num].model != NULL)
                        {
                            // Co-op: the players' bodies stay, since the
                            // cutscene that follows animates one of them
                            // (solo creates Bond's body after this). A
                            // hidden chr doesn't run its AI list.
                            if (gameIsTwoPlayerMission() && (g_ChrSlots[num].prop != NULL) && (g_ChrSlots[num].prop->type == PROP_TYPE_VIEWER))
                            {
                                continue;
                            }

                            g_ChrSlots[num].chrflags |= CHRFLAG_HIDDEN;
                        }
                    }
                    Offset += sizeof(AiHideAllChrsRecord);
                    break;
                }
                case AI_ShowAllChrs:
                {
                    s32 num;
                    for (num = chrsGetNumSlots() - 1; num >= 0; num--)
                    {
                        g_ChrSlots[num].chrflags &= ~CHRFLAG_HIDDEN;
                    }

                    Offset += sizeof(AiShowAllChrsRecord);
                    break;
                }
                case AI_DoorOpenInstant:
                {
                    AiDoorOpenInstantRecord *ai   = (AiDoorOpenInstantRecord *)(AiListp + Offset);
                    DoorRecord              *door = (DoorRecord *)objFindByTagId(ai->OBJECT_TAG);
                    if (door && door->prop)
                    {
                        doorOpenInstantly(door);
                    }
                    Offset += sizeof(AiDoorOpenInstantRecord);
                    break;
                }
                case AI_ChrRemoveItemInHand:
                {
                    AiChrRemoveItemInHandRecord *ai  = (AiChrRemoveItemInHandRecord *)(AiListp + Offset);
                    ChrRecord                   *chr = chrFindById(ChrEntityp, ai->CHR_NUM);
                    if (chr)
                    {
                        chrRemoveHeldWeapon(chr, ai->HAND_INDEX);
                    }
                    Offset += sizeof(AiChrRemoveItemInHandRecord);
                    break;
                }
                case AI_IfNumberOfActivePlayersLessThan:
                {
                    AiIfNumberOfActivePlayersLessThanRecord *ai = (AiIfNumberOfActivePlayersLessThanRecord *)(AiListp + Offset);
                    if (getPlayerCount() < ai->NUMBER)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIfNumberOfActivePlayersLessThanRecord);
                    }
                    break;
                }
                case AI_IFBondItemTotalAmmoLessThan:
                {
                    AiIFBondItemTotalAmmoLessThanRecord *ai = (AiIFBondItemTotalAmmoLessThanRecord *)(AiListp + Offset);
                    if (ammoGetTotal(ai->ITEM_NUM) < ai->AMMO_TOTAL)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFBondItemTotalAmmoLessThanRecord);
                    }
                    break;
                }
                case AI_BondEquipItem:
                {
                    AiBondEquipItemRecord *ai = (AiBondEquipItemRecord *)(AiListp + Offset);
                    gunEquip(GUNRIGHT, ai->ITEM_NUM);
                    gunEquip(GUNLEFT, 0);
                    Offset += sizeof(AiBondEquipItemRecord);
                    break;
                }
                case AI_BondEquipItemCinema:
                {
                    AiBondEquipItemCinemaRecord *ai = (AiBondEquipItemCinemaRecord *)(AiListp + Offset);
                    gunUnequip(GUNRIGHT, ai->ITEM_NUM);
                    gunUnequip(GUNLEFT, 0);
                    Offset += sizeof(AiBondEquipItemCinemaRecord);
                    break;
                }
                case AI_BondSetLockedVelocity:
                {
                    /*
                    g_Vars.currentplayer->bondforcespeed.x = (s8)ai->val[1];
                    g_Vars.currentplayer->bondforcespeed.y = 0;
                    g_Vars.currentplayer->bondforcespeed.z = (s8)ai->val[2];
                    */
                    AiBondSetLockedVelocityRecord *ai = (AiBondSetLockedVelocityRecord *)(AiListp + Offset);
                    g_ForceBondMoveOffset.x           = ai->X_SPEED60;
                    g_ForceBondMoveOffset.y           = 0;
                    g_ForceBondMoveOffset.z           = ai->Z_SPEED60;
                    // Only the player this script is acting on is pushed.
                    g_CoopForceMovePlayer             = playerGetCurrentNum();
                    Offset += sizeof(AiBondSetLockedVelocityRecord);
                    break;
                }
                case AI_IFObjectInRoomWithPad:
                {
                    AiIFObjectInRoomWithPadRecord *ai     = (AiIFObjectInRoomWithPadRecord *)(AiListp + Offset);
                    u16                            padnum = ntohs(ai->PAD);
                    PadRecord                     *pad;
                    ObjectRecord                  *obj = objFindByTagId(ai->OBJECT_TAG);

                    if (isNotBoundPad(padnum))
                    {
                        pad = &g_CurrentSetup.pads[padnum];
                    }
                    else
                    {
                        pad = (PadRecord *)&g_CurrentSetup.boundpads[getBoundPadNum(padnum)];
                    }

                    if (pad->stan && obj && obj->prop && (pad->stan->room == obj->prop->stan->room))
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFObjectInRoomWithPadRecord);
                    }
                    break;
                }
                case AI_SwitchSky:
                { // SWITCHENVIRONMENT
                    fogSwitchToSolosky2(1.0);
                    Offset += sizeof(AiSwitchSkyRecord);
                    break;
                }
                case AI_TriggerFadeAndExitLevelOnButtonPress:
                {
                    if (g_Mission.exitOnPress == FALSE)
                    {
                        g_Mission.exitOnPress = TRUE;
                    }
                    Offset += sizeof(AiTriggerFadeAndExitLevelOnButtonPressRecord);
                    break;
                }
                case AI_IFBondIsDead:
                {
                    AiIFBondIsDeadRecord *ai = (AiIFBondIsDeadRecord *)(AiListp + Offset);
                    if (g_CurrentPlayer->bonddead)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFBondIsDeadRecord);
                    }
                    break;
                }
                case AI_BondDisableDamageAndPickups:
                {
                    g_Mission.playerInvincible = TRUE;
                    Offset += sizeof(AiBondDisableDamageAndPickupsRecord);
                    break;
                }
                case AI_BondHideWeapons:
                {
                    /*"remove guntype %d\n" */
                    gunClearHandModel(GUNRIGHT);
                    gunClearHandModel(GUNLEFT);
                    Offset += sizeof(AiBondHideWeaponsRecord);
                    break;
                }
                case AI_CameraOrbitPad:
                {
                    AiCameraOrbitPadRecord *ai = (AiCameraOrbitPadRecord *)(AiListp + Offset);
                    s32                     padnum;
                    s32                     speed60;
                    s32                     camDististance;
                    s32                     targetHeight;
                    s32                     camHeight;
                    s32                     start;
                    camDististance          = ntohs(ai->LAT_DISTANCE);
                    camHeight               = (s16)ntohs(ai->VERT_DISTANCE);
                    speed60                 = (s16)ntohs(ai->ORBIT_SPEED60);
                    padnum                  = ntohs(ai->PAD);
                    targetHeight            = (s16)ntohs(ai->Y_POS_OFFSET);
                    start                   = ntohs(ai->INITIAL_ROTATION);
                    g_CameraLookAtBondPad = NULL;
                    gBondViewCutscene       = NULL;
                    g_OrbitCamAngle   = (start * M_TAU_F) / M_U16_MAX_VALUE_F;   /*unit direction 0 - 1 (increments are 0.000001) */
                    g_OrbitCamSpeed   = (speed60 * M_TAU_F) / M_U16_MAX_VALUE_F; /*how many increments per frame */
                    g_OrbitCamDist   = camDististance;
                    g_OrbitCamHeight   = camHeight;
                    g_OrbitCamTargetHeight   = targetHeight;
                    g_OrbitCamPad = padnum;
                    playerSetCameraMode(CAMERAMODE_POSEND);
                    Offset += sizeof(AiCameraOrbitPadRecord);
                    break;
                }
                case AI_CreditsRoll:
                {
                    credits_state = TRUE;
                    Offset += sizeof(AiCreditsRollRecord);
                    break;
                }
                case AI_IFCreditsHasCompleted:
                {
                    AiIFCreditsHasCompletedRecord *ai = (AiIFCreditsHasCompletedRecord *)(AiListp + Offset);
                    if (credits_state == 2)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFCreditsHasCompletedRecord);
                    }
                    break;
                }
                case AI_IFObjectiveAllCompleted:
                {
                    AiIFObjectiveAllCompletedRecord *ai = (AiIFObjectiveAllCompletedRecord *)(AiListp + Offset);
                    // bool a = objectivesAllComplete();
                    if (objectivesAllComplete())
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFObjectiveAllCompletedRecord);
                    }
                    break;
                }
                case AI_IFFolderActorIsEqual:
                {
                    AiIFFolderActorIsEqualRecord *ai = (AiIFFolderActorIsEqualRecord *)(AiListp + Offset);
                    if (fileGetBondForCurrentFolder() == ai->BOND_ACTOR_INDEX)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFFolderActorIsEqualRecord);
                    }
                    break;
                }
                case AI_IFBondDamageAndPickupsDisabled:
                {
                    AiIFBondDamageAndPickupsDisabledRecord *ai = (AiIFBondDamageAndPickupsDisabledRecord *)(AiListp + Offset);
                    if (g_Mission.playerInvincible)
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFBondDamageAndPickupsDisabledRecord);
                    }
                    break;
                }
                case AI_MusicPlaySlot:
                {
                    AiMusicPlaySlotRecord *ai = (AiMusicPlaySlotRecord *)(AiListp + Offset);
                    Offset += sizeof(AiMusicPlaySlotRecord);
                    levelMusicSlotStart(ai->MUSIC_SLOT, ai->SECONDS_STOPPED_DURATION, ai->SECONDS_TOTAL_DURATION);
                    break;
                }
                case AI_MusicStopSlot:
                {
                    AiMusicStopSlotRecord *ai = (AiMusicStopSlotRecord *)(AiListp + Offset);
                    Offset += sizeof(AiMusicStopSlotRecord);
                    levelMusicSlotStop(ai->MUSIC_SLOT);
                    break;
                }
                case AI_TriggerExplosionsAroundBond:
                {
                    playerSurroundWithExplosions(0);
                    Offset += sizeof(AiTriggerExplosionsAroundBondRecord);
                    break;
                }
                case AI_IFKilledCiviliansGreaterThan:
                {
                    AiIFKilledCiviliansGreaterThanRecord *ai = (AiIFKilledCiviliansGreaterThanRecord *)(AiListp + Offset);
                    if (ai->CIVILIANS_KILLED < statsGetCivilianKills())
                    {
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFKilledCiviliansGreaterThanRecord);
                    }
                    break;
                }
                case AI_IFChrWasShotSinceLastCheck:
                {
                    AiIFChrWasShotSinceLastCheckRecord *ai  = (AiIFChrWasShotSinceLastCheckRecord *)(AiListp + Offset);
                    ChrRecord                          *chr = chrFindById(ChrEntityp, ai->CHR_NUM);
                    if (chr && chr->chrflags & CHRFLAG_WAS_HIT)
                    {
                        chr->chrflags &= ~CHRFLAG_WAS_HIT;
                        Offset = aiFindLabel(AiListp, Offset, ai->GOTOLABEL);
                    }
                    else
                    {
                        Offset += sizeof(AiIFChrWasShotSinceLastCheckRecord);
                    }
                    break;
                }
                case AI_BondKilledInAction:
                {
                    g_FrontBondKilled = TRUE;
                    Offset += sizeof(AiBondKilledInActionRecord);
                    break;
                }
                case AI_RaiseArms:
                {
                    chrTrySurprisedSurrender(ChrEntityp);
                    Offset += sizeof(AiRaiseArmsRecord);
                    break;
                }
                case AI_GasLeakAndFadeFog:
                {
                    coord3d emitPos = {};
                    gasStart(&emitPos);
                    Offset += sizeof(AiGasLeakAndFadeFogRecord);
                    break;
                }
                case AI_ObjectRocketLaunch:
                {
                    AiObjectRocketLaunchRecord *ai  = (AiObjectRocketLaunchRecord *)(AiListp + Offset);
                    ObjectRecord               *obj = objFindByTagId(ai->OBJECT_TAG);

                    if (obj && obj->prop)
                    {
                        objEnsureProjectile(obj->prop);

                        if (obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
                        {
                            obj->projectile->flags |= PROJECTILEFLAG_SMOKETRAIL | PROJECTILEFLAG_NOTUMBLE | PROJECTILEFLAG_AIRBORNE;
                            projectileEnableCollide3D(obj->prop);
                            mtx4LoadIdentity(&obj->projectile->mtx);
                            obj->projectile->speed.x = 0;
                            obj->projectile->speed.y = 0.016666666f; // step height?
                            obj->projectile->speed.z = 0;
                            obj->projectile->accel.x = 0;
                            obj->projectile->accel.y = 0.29166666f; // direction to move?
                            obj->projectile->accel.z = 0;
                        }
                    }
                    Offset += sizeof(AiObjectRocketLaunchRecord);
                    break;
                } //============================================================================================================
                default:
                    /*
                     * No Command found, advance ailist by 1.
                     * This is attempting to handle situations where the command
                     * type is invalid by passing over them and continuing
                     * execution.
                     * aiCommandSize returns 1 which is pointless really
                     * could have done it here without a jump
                     *
                     * Outcome:crash
                     */
                    {
                        Offset += aiCommandSize(AiListp, Offset);
                    }
            } // switch

            if (gameIsTwoPlayerMission())
            {
                coopAiEnd(&coop, &Offset, ChrEntityp);
            }
        } // for
    } // Has ailist
} // aiRunList()
