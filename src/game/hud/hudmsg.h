#ifndef _HUDMSG_H_
#define _HUDMSG_H_
#include <ultra64.h>
#include <bondtypes.h>

#define HUDMSG_TOP_LEN    0x97
#define HUDMSG_BOTTOM_LEN 0x65

#if defined(BUGFIX_R1)
#define HUDMESSAGEBOTTOM hudmsgBottomShowText
void hudmsgBottomShow(char *string, struct fontchar *font, struct font *font1);
void hudmsgBottomShowText(char *string);
#else
#define HUDMESSAGEBOTTOM hudmsgBottomShow
void hudmsgBottomShow(char *string);
void hudmsgSetFonts(struct fontchar *font2, struct font *font1);
#endif

void hudmsgResetIntroText(void);
void hudmsgsSetOn(s32 flag);
void hudmsgsSetOff(s32 flags);
void hudmsgTickIntroText(void);
Gfx *hudmsgBottomRender(Gfx *gdl);

void hudmsgTopReset(void);
void hudmsgTopClearFlag(int flag);
void hudmsgTopSetFlag(PLAYERFLAG flag);
void hudmsgTopShow(char *mess);
void hudmsgTopTick(void);
Gfx *hudmsgTopRender(Gfx *gdl);

#endif
