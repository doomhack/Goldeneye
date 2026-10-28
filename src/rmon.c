/*
 * THIS FILE IS AN OS FILE (PR\libultra\rmon\rmonMain.c)
 */
#include <stdarg.h>
#include <ultra64.h>
#include <PR/os.h>
#include "libultra/libc/xstdio.h"
#include "crash.h"
#include "rmon.h" /*<PR/rmon.h>*/

/************************************************************************
Function: rmonMain
Args: none
Type: void
Purpose: This is the main loop of the rmon debugger. It is mostly a protocol
	parser/dispatcher.
************************************************************************/
void rmonMain(void *arg) {
}

/**
 * rmonGetToken
 * returns true if this if rmon is disabled
 */
s32 rmonGetToken(void) {
        return TRUE;
}


/**
 * Removed
 * rmonPrintf
 */
void rmonPrintf(void)
{
}

/*
 * -----------------------------------------------------------------
 * This is a new file (PR/libultra/libc/syncprintf.c)
*/


/**
 * Send text to Remote Debugger, GE redirects to screen (crashAppendChar)
 */
char *proutSyncPrintf(char *str, const char *buf, size_t n)
{
    u32 sent = 0;

#ifndef _FINALROM
    while (sent < n)
    {
        crashAppendChar(buf[sent++]);
    }
#endif
    return (char *)1; /* return a fake pointer so that it's not NULL */
}


/**
 * Print formatted string to Debugger
 */
void osSyncPrintf(const char *fmt, ...)
{

    va_list args;
    va_start(args, fmt);
    _Printf(proutSyncPrintf, NULL, fmt, args);
    va_end(args);

}

