/*
 * THIS FILE IS AN OS FILE (include\PR\rmon.h)
 * needs moving to libultrare since its modified
 * apparently they ifdeffed out func contents rather than while file?
 */
#ifndef _RMON_H_
#define _RMON_H_

void rmonMain(void *arg);
void osSyncPrintf(const char *fmt, ...);

#endif
