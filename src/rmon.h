/*
 * What is left of libultra's remote debugger (rmon): an empty thread, and osSyncPrintf, which
 * prints to the crash screen's text buffer.
 */
#ifndef _RMON_H_
#define _RMON_H_

void rmonMain(void *arg);
void osSyncPrintf(const char *fmt, ...);

#endif
