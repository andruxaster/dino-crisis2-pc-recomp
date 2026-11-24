/* The game's streams (the room music, the speech) played by KallistiOS
 * streams on the AICA's own channels (the Dreamcast, DC2_AICA): the SH-4
 * only decodes their blocks into the stream's buffer at their own rate;
 * the AICA resamples, sets volume and pan and mixes them. The main stream
 * (src/port/dc/main.c) keeps the rest (the movies' sound, sounds the AICA
 * could not take). Used by src/pc/audio.c for its file streams.
 *
 * Threads: AicaStrm_Open / _Gains / _Close only leave requests (any thread);
 * AicaStrm_Poll, in the sound thread, starts, turns and stops the streams
 * and calls the fill functions from there. */
#ifndef DC2_AICA_STRM_H
#define DC2_AICA_STRM_H
#include "dc2/types.h"

/* up to frames frames (interleaved by ch) into out; fewer at the end */
typedef u32 (*AicaFill)(void *ctx, s16 *out, u32 frames);

int  AicaStrm_Init(void);         /* after snd_stream_init: the streams' slots; how many */
/* a stream slot started for fill at rate (1 or 2 channels); -1: none free */
int  AicaStrm_Open(u32 rate, u32 ch, AicaFill fill, void *ctx, float gl, float gr);
void AicaStrm_Gains(int s, float gl, float gr);   /* linear left and right gains, 0..1 */
void AicaStrm_Close(int s);
void AicaStrm_Poll(void);          /* the sound thread's turn */
u32  AicaStrm_Live(void);          /* streams playing (the log) */
#endif
