/* A sampling profiler for the Dreamcast's log (src/port/dc/prof.c): where
 * the SH-4 spends its time, in all frames and in the slow ones. */
#ifndef DC2_DC_PROF_H
#define DC2_DC_PROF_H
void Prof_Init(void);                /* TMU1 at 1 kHz */
void Prof_FrameEnd(int slow);        /* the frame's samples to the slow histograms too, if it ran slow */
void Prof_Report(void);              /* the hottest places, as addresses (tools/dreamcast/dc2_prof.py names them) */
#endif
