/**
 * The language banks: the text files every string lives in, loaded at boot or with each
 * level, and looking a string up by id (langGet). See docs/subsystems/text.md.
 */
#include <ultra64.h>
#include <bondgame.h>
#include <memp.h>
#include "game/text/language.h"
#include "game/asset/asset.h"

// bss
u32 *g_LangBanks[45]; // each bank starts with a table of string offsets



// US loads the English banks (E), PAL the British-spelling ones (P). Both are
// built from the same source; see tools/brittext.py.
#if defined(VERSION_EU)
#define LANG_SUFFIX "P"
#else
#define LANG_SUFFIX "E"
#endif


// The text bank file for each language bank id.
const char *g_LangFileNames[45] = {
    NULL,            /* Null (unused) */
    "Lame" LANG_SUFFIX,         /* Library (multi) */
    "Larch" LANG_SUFFIX,        /* Archives */
    "Lark" LANG_SUFFIX,         /* Facility */
    "Lash" LANG_SUFFIX,         /* Stack (multi) */
    "Lazt" LANG_SUFFIX,         /* Aztec */
    "Lcat" LANG_SUFFIX,         /* Citadel (multi) */
    "Lcave" LANG_SUFFIX,        /* Caverns */
    "Larec" LANG_SUFFIX,        /* Control */
    "Lcrad" LANG_SUFFIX,        /* Cradle */
    "Lcryp" LANG_SUFFIX,        /* Egypt */
    "Ldam" LANG_SUFFIX,         /* Dam */
    "Ldepo" LANG_SUFFIX,        /* Depot */
    "Ldest" LANG_SUFFIX,        /* Frigate */
    "Ldish" LANG_SUFFIX,        /* Temple (multi) */
    "Lear" LANG_SUFFIX,         /* Ear (unused) */
    "Leld" LANG_SUFFIX,         /* Eld (unused) */
    "Limp" LANG_SUFFIX,         /* Basement (multi) */
    "Ljun" LANG_SUFFIX,         /* Jungle */
    "Llee" LANG_SUFFIX,         /* Lee (unused) */
    "Llen" LANG_SUFFIX,         /* Cuba */
    "Llip" LANG_SUFFIX,         /* Lip (unused) */
    "Llue" LANG_SUFFIX,         /* Lue (unused) */
    "Loat" LANG_SUFFIX,         /* Cave (multi) */
    "Lpam" LANG_SUFFIX,         /* Pam (unused) */
    "Lpete" LANG_SUFFIX,        /* Streets */
    "Lref" LANG_SUFFIX,         /* Complex (multi) */
    "Lrit" LANG_SUFFIX,         /* Rit (unused) */
    "Lrun" LANG_SUFFIX,         /* Runway */
    "Lsevb" LANG_SUFFIX,        /* Bunker 2 */
    "Lsev" LANG_SUFFIX,         /* Bunker 1 */
    "Lsevx" LANG_SUFFIX,        /* Surface 1 */
    "Lsevxb" LANG_SUFFIX,       /* Surface 2 */
    "Lsho" LANG_SUFFIX,         /* Shooting Range (unused) */
    "Lsilo" LANG_SUFFIX,        /* Silo */
    "Lstat" LANG_SUFFIX,        /* Statue */
    "Ltra" LANG_SUFFIX,         /* Train */
    "Lwax" LANG_SUFFIX,         /* Wax (unused) */
    "Lgun" LANG_SUFFIX,         /* Guns */
    "Ltitle" LANG_SUFFIX,       /* Stage and menu titles */
    "Lmpmenu" LANG_SUFFIX,      /* Multi menus */
    "Lpropobj" LANG_SUFFIX,     /* In-game pickups */
    "Lmpweapons" LANG_SUFFIX,   /* Multi weapon select */
    "Loptions" LANG_SUFFIX,     /* Solo in-game menus */
    "Lmisc" LANG_SUFFIX         /* Cheat options */
};


// cannonically gettextloadnum
TEXTBANK_LEVEL_INDEX langGetBankForStage(LEVELID level)
{
    TEXTBANK_LEVEL_INDEX return_id;

    switch(level)
    {
        case LEVELID_DAM:
            return_id = LDAM;
            break;
        case LEVELID_FACILITY:
            return_id = LARK;
            break;
        case LEVELID_RUNWAY:
            return_id = LRUN;
            break;
        case LEVELID_SURFACE:
            return_id = LSEVX;
            break;
        case LEVELID_BUNKER1:
            return_id = LSEV;
            break;
        case LEVELID_SILO:
            return_id = LSILO;
            break;
        case LEVELID_FRIGATE:
            return_id = LDEST;
            break;
        case LEVELID_SURFACE2:
            return_id = LSEVXB;
            break;
        case LEVELID_BUNKER2:
            return_id = LSEVB;
            break;
        case LEVELID_STATUE:
            return_id = LSTAT;
            break;
        case LEVELID_ARCHIVES:
            return_id = LARCH;
            break;
        case LEVELID_STREETS:
            return_id = LPETE;
            break;
        case LEVELID_DEPOT:
            return_id = LDEPO;
            break;
        case LEVELID_TRAIN:
            return_id = LTRA;
            break;
        case LEVELID_JUNGLE:
            return_id = LJUN;
            break;
        case LEVELID_CONTROL:
            return_id = LAREC;
            break;
        case LEVELID_CAVERNS:
            return_id = LCAVE;
            break;
        case LEVELID_CRADLE:
            return_id = LCRAD;
            break;
        case LEVELID_AZTEC:
            return_id = LAZT;
            break;
        case LEVELID_EGYPT:
            return_id = LCRYP;
            break;
        case LEVELID_TEMPLE:
            return_id = LDISH;
            break;
        case LEVELID_COMPLEX:
            return_id = LREF;
            break;
        case LEVELID_LIBRARY:
            return_id = LAME;
            break;
        case LEVELID_BASEMENT:
            return_id = LIMP;
            break;
        case LEVELID_STACK:
            return_id = LASH;
            break;
        case LEVELID_CAVES:
            return_id = LOAT;
            break;
        case LEVELID_CUBA:
            return_id = LLEN;
            break;
        default:
        {
            /* infinite loop on invalid text bank */
            while(1) {};
        }
    }

    return return_id;
}


void langInit(void) {
    s32 i;


	for (i = 0; i < 45; i++) {
		g_LangBanks[i] = NULL;
	}
    g_LangBanks[LGUN] = assetLoad(g_LangFileNames[LGUN], MEMPOOL_PERMANENT);
    g_LangBanks[LTITLE] = assetLoad(g_LangFileNames[LTITLE], MEMPOOL_PERMANENT);
    g_LangBanks[LMPMENU] = assetLoad(g_LangFileNames[LMPMENU], MEMPOOL_PERMANENT);
    g_LangBanks[LPROPOBJ] = assetLoad(g_LangFileNames[LPROPOBJ], MEMPOOL_PERMANENT);
    g_LangBanks[LMPWEAPONS] = assetLoad(g_LangFileNames[LMPWEAPONS], MEMPOOL_PERMANENT);
    g_LangBanks[LOPTIONS] = assetLoad(g_LangFileNames[LOPTIONS], MEMPOOL_PERMANENT);
    g_LangBanks[LMISC] = assetLoad(g_LangFileNames[LMISC], MEMPOOL_PERMANENT);
}


void langLoadToAddr(u32 id)
{
    g_LangBanks[id] = assetLoad(g_LangFileNames[id], MEMPOOL_STAGE);
}


void langLoadToBank(int id,u8 *target,int size)
{
    g_LangBanks[id] = assetLoadInto(g_LangFileNames[id], target, size);
}


void langClearBank(s32 textBank) {
    g_LangBanks[textBank] = 0;
}

/**
 * Get a pointer to a string.
 * @param slotID: UniqueID of string (a combination of Bank ID and string index)
 * @return char* string.
 */
const char *langGet(s32 slotID)
{
    const u32 *textbank_ptr = g_LangBanks[slotID >> 10]; /* get the text file bank ID index the text ptr table */
    u32 textslot_offset = textbank_ptr[slotID & 0x03FF]; /* load the textbank ptr table then get the slot's offset */

    u32 output_slot = textslot_offset; /* add the text slot offset to the base ptr to get the ptr to text file's slot */
    output_slot += (u32)textbank_ptr;
    return (textslot_offset != 0) ? (char *)output_slot : NULL;
}
