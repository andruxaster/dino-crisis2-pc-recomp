/* Where the time of a stalled frame went (the Dreamcast's log, DC2_STALL_TIMES):
 * the disc, the JPEG backgrounds, the texture uploads, in microseconds since
 * the frame began. Nothing in the other builds. */
#ifndef DC2_STALL_H
#define DC2_STALL_H
enum { STALL_DISC, STALL_JPEG, STALL_TEX, STALL_N };
#ifdef DC2_STALL_TIMES
extern unsigned g_stall_us[STALL_N];
unsigned long long Port_TimeUs(void);
#define STALL_BEGIN() unsigned long long stall_t0_ = Port_TimeUs()
#define STALL_END(k) (g_stall_us[k] += (unsigned)(Port_TimeUs() - stall_t0_))
#else
#define STALL_BEGIN() ((void)0)
#define STALL_END(k) ((void)0)
#endif
#endif
