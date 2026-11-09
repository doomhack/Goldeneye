#ifndef _HUDMSG_H_
#define _HUDMSG_H_
#include <ultra64.h>
#include <bondtypes.h>

#define HUDMSG_TOP_LEN    0x97
#define HUDMSG_BOTTOM_LEN 0x65

/* Queue a bottom message in the given font (the intro camera uses Zurich). */
void hudmsgBottomShowFont(const char *string, struct fontchar *font, struct font *font1);
/* Queue a bottom message in the usual font. */
void hudmsgBottomShow(const char *string);

void hudmsgResetIntroText(void);
void hudmsgsSetOn(s32 flag);
void hudmsgsSetOff(s32 flags);
void hudmsgTickIntroText(void);
Gfx *hudmsgBottomRender(Gfx *gdl);

void hudmsgTopReset(void);
void hudmsgTopClearFlag(int flag);
void hudmsgTopSetFlag(PLAYERFLAG flag);
void hudmsgTopShow(const char *mess);
void hudmsgTopTick(void);
Gfx *hudmsgTopRender(Gfx *gdl);

#endif
