/* The game's sound buffers played by the AICA's own channels (the Dreamcast,
 * DC2_AICA): uploaded to sound RAM once, then started, turned and stopped by
 * commands; the SH-4 mixes none of them. Used by src/pc/audio.c for the
 * buffers of the sound banks (Snd_CreateBuffer); the streams (music, voices)
 * and sounds over 65534 frames (a channel's limit) stay in its mixer. */
#ifndef DC2_AICA_SFX_H
#define DC2_AICA_SFX_H
#include "dc2/types.h"

typedef struct AicaSnd {
    u32 mem[2];         /* sound RAM: the left (or only) and right channels' samples */
    int chn[2];         /* the AICA channels while it plays (-1: none) */
    u32 frames, rate, ch, bytes;   /* bytes a sample: 2 (16-bit) or 1 (8-bit) */
    u32 freq;           /* the rate it plays at */
    u8 loop, playing, paused;
    u32 offset;         /* frames skipped (resumed after a pause) */
    unsigned long long start_us;   /* when it started (its end: start + frames / freq) */
    u8 vol[2], pan[2];  /* the channels' volume and pan (0..255) */
} AicaSnd;

/* after the sound system is up (snd_stream_init): hardware buffers from now on */
void Aica_Init(void);

/* a sound uploaded (frames x ch samples, 16-bit signed or 8-bit unsigned PCM);
   NULL when it cannot be (too long, no sound RAM, before Aica_Init) */
AicaSnd *Aica_Load(const void *pcm, u32 frames, u32 ch, u32 bits, u32 rate);
void Aica_Unload(AicaSnd *s);

/* gl, gr: the linear left and right gains (0..1) */
void Aica_Play(AicaSnd *s, u32 freq, int loop, float gl, float gr);
void Aica_Gains(AicaSnd *s, float gl, float gr);
int  Aica_IsPlaying(AicaSnd *s);
void Aica_Stop(AicaSnd *s);
void Aica_Pause(AicaSnd *s);
void Aica_Resume(AicaSnd *s);

u32 Aica_Count(void);     /* hardware buffers (the log) */
u32 Aica_Free(void);      /* sound RAM left, bytes */
#endif
