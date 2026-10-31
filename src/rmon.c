/*
 * THIS FILE IS AN OS FILE (PR\libultra\rmon\rmonMain.c)
 */
#include <stdarg.h>
#include <ultra64.h>
#include <PR/os.h>
#include "crash.h"
#include "rmon.h" /*<PR/rmon.h>*/

/************************************************************************
Function: rmonMain
Args: none
Type: void
Purpose: This is the main loop of the rmon debugger. It is mostly a protocol
	parser/dispatcher.
************************************************************************/
void rmonMain([[maybe_unused]] void *arg) {
}


/*
 * -----------------------------------------------------------------
 * This is a new file (PR/libultra/libc/syncprintf.c)
*/


/**
 * Send text to Remote Debugger, GE redirects to screen (crashAppendChar)
 */
// libultra's printf engine (lib/ultralib/src/libc/xprintf.c)
int _Printf(void *prout(void *, const char *, size_t), void *arg, const char *fmt, va_list ap);

static void *proutSyncPrintf([[maybe_unused]] void *str, const char *buf, size_t n)
{
    for (size_t sent = 0; sent < n; sent++)
    {
        crashAppendChar(buf[sent]);
    }
    return (void *)1; /* return a fake pointer so that it's not NULL */
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

