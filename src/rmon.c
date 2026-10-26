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
void rmonMain(void) {
}

/**
 * rmonGetToken
 * returns true if this if rmon is disabled
 */
s32 rmonGetToken(void) {
    #if defined(DEBUG)
        //flesh out sending a token from pc app
        return FALSE;
    #else
        return TRUE;
    #endif
}

/**
 * REMOVED
 * rmonStatus
 * returns the status of rmon
 */
s32 rmonStatus(void) {
    #ifdef DEBUG
        //removed
    #else
        return -1;
    #endif

}



/**
 * Removed
 * reimpliment osWriteHost
 * target rmon and usb
*/
void osWriteHost(void * buffer, u32 size)
{
    if (buffer);
	if (size);
}


/**
 * Removed
 * reimpliment osReadHost
 * target rmon and usb
*/
void osReadHost(void * buffer, u32 size)
{
    if (buffer);
	if (size);
}


/**
 * Removed
 * rmon7000CEC8
 * unknown function
 */
void rmon7000CEC8(void) {
    #ifdef DEBUG
        //removed
    #endif
}

/**
 * Removed
 * rmon7000CED0
 * unknown function
 */
void rmon7000CED0(void) {
    #ifdef DEBUG
        //removed
    #endif
}

/**
 * Removed
 * rmon7000CED8
 * unknown function
 */
void rmon7000CED8(void) {
    #ifdef DEBUG
        //removed
    #endif
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
void *proutSyncPrintf(void *str, const char *buf, size_t n)
{
    u32 sent = 0;

#ifndef _FINALROM
    while (sent < n)
    {
#    ifdef USERDB
        sent += __osRdbSend((u8 *)&buf[sent], n - sent, RDB_TYPE_GtoH_PRINT);
#    endif
        crashAppendChar(buf[sent++]);
    }
#endif
    return ((void *)1); /* return a fake pointer so that it's not NULL */
}


/**
 * Print formatted string to Debugger
 */
void osSyncPrintf(const char *fmt, ...)
{

    int     ans;
    va_list args;
    va_start(args, fmt);
    ans = _Printf(&proutSyncPrintf, NULL, fmt, args);
    va_end(args);

}

