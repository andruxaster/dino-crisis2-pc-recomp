/* Sound each frame and the voice: the stream slots (0x87FB80, 4 x 0x14) and
 * the sound slots (0x87F980, 32 x 0x10) ramp their volume, the voice
 * (Speech\NNNN.DAT, flag 2/0 while it plays) ends when its buffer has. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/pcio.h"

#define A_STREAMS 0x87FB80u     /* +0 handle, +8 on, +0xA volume, +0xC target, +0x10 ramp (bit 7: stop at the end), +0x11 step */
#define A_SLOTS   0x87F980u     /* +0 handle, +4 frames, +8 on, +0xA ramp, +0xB step, +0xC volume, +0xE target */
#define A_VOICE   0x7598C8u     /* handle, wave, frames since the last check */

void DC2CALL Snd_Tick(void)                                         /* 0x48C700 */
{
    for (u32 i = 0; i < 4; i++) {
        gptr s = A_STREAMS + i * 0x14;
        if (!U8(s + 8) || !U8(s + 0x10)) continue;
        if (S16(s + 0xA) > S16(s + 0xC)) {
            U16(s + 0xA) -= U8(s + 0x11);
            if (S16(s + 0xA) <= S16(s + 0xC)) {
                U16(s + 0xA) = U16(s + 0xC);
                if (U8(s + 0x10) & 0x80) Strm_Halt(i);
                U8(s + 0x10) = 0;
            }
        } else if (S16(s + 0xA) < S16(s + 0xC)) {
            U16(s + 0xA) += U8(s + 0x11);
            if (S16(s + 0xA) >= S16(s + 0xC)) { U16(s + 0xA) = U16(s + 0xC); U8(s + 0x10) = 0; }
        }
        Strm_SetVolume(U32(s), S16(s + 0xA) * S8(G() + 0x14ED) / 15);
    }
    for (u32 i = 0; i < 0x20; i++) {
        gptr s = A_SLOTS + i * 0x10;
        if (!U8(s + 8)) continue;
        if (U8(s + 0xA)) {
            if (S16(s + 0xC) > S16(s + 0xE)) {
                U16(s + 0xC) -= U8(s + 0xB);
                if (S16(s + 0xC) <= S16(s + 0xE)) {
                    U16(s + 0xC) = U16(s + 0xE);
                    if (U8(s + 0xA) & 0x80) {
                        Snd_Stop(U32(s));
                        U8(s + 0xA) = 0; U8(s + 8) = 0;
                        continue;
                    }
                }
            } else if (S16(s + 0xC) < (s16)U8(s + 0xB)) {     /* (compares with the step) */
                U16(s + 0xC) += U8(s + 0xB);
                if (S16(s + 0xC) >= S16(s + 0xE)) { U16(s + 0xC) = U16(s + 0xE); U8(s + 0xA) = 0; }
            }
            Snd_SetVolume(U32(s), S16(s + 0xC) * S8(G() + 0x14EE) / 15);
        }
        if (++U32(s + 4) >= 0x1E) {
            U32(s + 4) = 0;
            if (!Snd_IsPlaying(U32(s))) { U8(s + 0xA) = 0; U8(s + 8) = 0; }
        }
    }
    if (Flag_Check(2, 0) && ++S32(A_VOICE + 8) >= 0x1E) {
        U32(A_VOICE + 8) = 0;
        if (!Snd_IsPlaying(U32(A_VOICE))) Voice_Stop();
    }
}

/* the voice's volume by kind (1, 2, other) and the sound volume G+0x14EE */
s32 DC2CALL Voice_Volume(u32 kind)                                  /* 0x48D000 */
{
    s32 v = S8(G() + 0x14EE);
    v *= kind == 1 ? 120 : kind == 2 ? 127 : 110;
    return v / 15;
}

void DC2CALL Voice_Stop(void)                                       /* 0x48D130 */
{
    Snd_Stop(U32(A_VOICE));
    Snd_Free(U32(A_VOICE));
    Crt_free(U32(A_VOICE + 4));
    Flag_Set(2, 0, 0);
}

/* Speech\NNNN.DAT (a wave file) played as the voice */
void DC2CALL Voice_Play(u32 kind, u32 id)                           /* 0x48D070 */
{
    if (Flag_Check(2, 0)) Voice_Stop();
    gptr name = Tmp_Push(0x28);
    Crt_sprintf1(name, 0x738DCC, id);
#if defined(DC2_LOWMEM) && !defined(DC2_IN_PROCESS)
    /* small-memory builds (the Dreamcast): played from the disc, not read
       whole into the heap (up to 1.5 MB); a small stand-in as its data, which
       Voice_Stop frees */
    u32 vh = Snd_OpenFileVoice((const char *)MEM(name));
    Tmp_Pop(0x28);
    if (vh == 0xFFFFFFFF) return;
    U32(A_VOICE + 4) = Crt_malloc(16);
    U32(A_VOICE) = vh;
    U32(A_VOICE + 8) = 0;
    Snd_Play(U32(A_VOICE), 0, 0);
    Snd_SetVolume(U32(A_VOICE), Voice_Volume(kind));
    Flag_Set(2, 0, 1);
    return;
#endif
    s32 fd = Pc_Open(name, 0, 0);
    Tmp_Pop(0x28);
    if (fd == -1) return;
    u32 n = Pc_Size(fd);
    gptr w = Crt_malloc(n);
    U32(A_VOICE + 4) = w;
    Pc_Read(fd, w, n);
    Pc_Close(fd);
    U32(A_VOICE) = Snd_CreateBuffer(U32(A_VOICE + 4), 1);
    U32(A_VOICE + 8) = 0;
    Snd_Play(U32(A_VOICE), 0, 0);
    Snd_SetVolume(U32(A_VOICE), Voice_Volume(kind));
    Flag_Set(2, 0, 1);
}

/* ---- all the slots at once ---- */

void DC2CALL Snd_ClearSlots(void)                                   /* 0x48C040 */
{
    memset(MEM(A_SLOTS), 0, 0x200);
}

void DC2CALL Strm_HaltAll(void)                                     /* 0x48C060 */
{
    for (u32 i = 0; i < 4; i++) Strm_Halt(i);
}

/* the streams down to 0x5A (their volume kept at +0xE, else 0xFFFF), the
   sounds playing down to 0x32 */
void DC2CALL Snd_Duck(void)                                         /* 0x48C8D0 */
{
    for (u32 i = 0; i < 4; i++) {
        gptr s = A_STREAMS + i * 0x14;
        U16(s + 0xE) = 0xFFFF;
        if (S16(s + 0xC) > 0x5A && !(U8(s + 0x10) & 0x80)) {
            U16(s + 0xE) = U16(s + 0xC);
            Strm_SetFade(i, 1, 0x5A, 1);
        }
    }
    for (u32 i = 0; i < 0x20; i++) {
        gptr s = A_SLOTS + i * 0x10;
        if (U8(s + 8) && Snd_IsPlaying(U32(s)) && !(U8(s + 0xA) & 0x80)) Snd_SetFade(i, 4, 0x32, 1);
    }
}

/* back up: the streams to the volume kept, the sounds to 0x7F */
void DC2CALL Snd_Unduck(void)                                       /* 0x48C950 */
{
    for (u32 i = 0; i < 4; i++) {
        u16 v = U16(A_STREAMS + i * 0x14 + 0xE);
        if (v != 0xFFFF) Strm_SetFade(i, 2, v, 2);
    }
    for (u32 i = 0; i < 0x20; i++) {
        gptr s = A_SLOTS + i * 0x10;
        if (U8(s + 8) && Snd_IsPlaying(U32(s)) && !(U8(s + 0xA) & 0x80)) Snd_SetFade(i, 4, 0x7F, 2);
    }
}

/* every sound fading out, stopped at the end */
void DC2CALL Snd_FadeOutAll(void)                                   /* 0x48CBA0 */
{
    for (u32 i = 0; i < 0x20; i++)
        if (U8(A_SLOTS + i * 0x10 + 8)) Snd_SetFade(i, 5, 0, 0x80);
}

void DC2CALL Snd_PauseAll(void)                                     /* 0x48CF60 */
{
    for (gptr s = A_SLOTS; s < A_STREAMS; s += 0x10) if (U8(s + 8)) Snd_Pause(U32(s));
    for (gptr s = A_STREAMS; s < A_STREAMS + 0x50; s += 0x14) if (U8(s + 8)) Strm_Pause(U32(s));
}

void DC2CALL Snd_ResumeAll(void)                                    /* 0x48CFB0 */
{
    for (gptr s = A_SLOTS; s < A_STREAMS; s += 0x10) if (U8(s + 8)) Snd_Resume(U32(s));
    for (gptr s = A_STREAMS; s < A_STREAMS + 0x50; s += 0x14) if (U8(s + 8)) Strm_Resume(U32(s));
}
