/* Files and heap as the PC port provides them (0x4C03F0.. and the CRT).
 * In the hook build and the harness these call the original functions; the
 * standalone builds implement them on the host (src/game/pcio.c): files are
 * read from g_pc_root, memory comes from a heap inside the emulated image so
 * that the game can keep 32-bit addresses to it. */
#ifndef DC2_PCIO_H
#define DC2_PCIO_H
#include "types.h"

s32  DC2CALL Pc_Open(gptr path, u32 a, u32 b);          /* 0x4C03F0: fd or -1 */
s32  DC2CALL Pc_Read(s32 fd, gptr buf, u32 n);          /* 0x4C04E0 */
u32  DC2CALL Pc_Size(s32 fd);                           /* 0x4C0540 */
s32  DC2CALL Pc_Seek(s32 fd, s32 off, u32 whence);      /* 0x4C0580 */
s32  DC2CALL Pc_SeekSet(s32 fd, s32 off);               /* 0x4C0560: from the start */
void DC2CALL Pc_Close(s32 fd);                          /* 0x4C05A0 */
gptr DC2CALL Crt_malloc(u32 n);                         /* 0x4DD711 */
void DC2CALL Crt_free(gptr p);                          /* 0x4DD628 */
void DC2CALL Crt_sprintf1(gptr buf, gptr fmt, u32 a);   /* 0x4DD5D6: sprintf with one argument */
void DC2CALL Crt_sprintf3(gptr buf, gptr fmt, u32 a, u32 b, u32 c);   /* ... with three numbers */

/* DirectSound buffers (0x4BF720..): in the standalone builds a silent
 * stand-in that only keeps time, so the game sees sounds start and end */
u32  DC2CALL Snd_CreateBuffer(gptr wave, u32 voice);   /* 0x4BF720: handle (0 for the voice) or -1 */
void DC2CALL Snd_Play(u32 h, u32 a, u32 loop);          /* 0x4BF970 */
void DC2CALL Snd_SetVolume(u32 h, s32 vol);             /* 0x4BFA20 */
u32  DC2CALL Snd_IsPlaying(u32 h);                      /* 0x4BFAA0 */
void DC2CALL Snd_Stop(u32 h);                           /* 0x4BFAE0 */
void DC2CALL Snd_Free(u32 h);                           /* 0x4BFBD0 */
void DC2CALL Strm_SetVolume(u32 h, s32 vol);            /* 0x4BFF20 */
void DC2CALL Strm_Play(u32 h);                          /* 0x4BFEE0 */
void DC2CALL Strm_Stop(u32 h);                          /* 0x4BFFA0 */
void DC2CALL Snd_SetPan(u32 h, u32 pan);                /* 0x4BFA60 */
void DC2CALL Strm_Close(u32 h);                         /* 0x4C0050 */

/* movies (standalone: none, the game goes on as if the file were missing) */
u32  DC2CALL Movie_Open(gptr name, u32 w, u32 h);       /* 0x4BF3A0: 0 when it plays */

/* the background masks as Direct3D quads (standalone: not drawn yet) */
void DC2CALL Masks_CreateVB(u32 nverts, gptr pixels);   /* 0x4BE3C0 */
void DC2CALL Masks_SetQuad(u32 i, u32 x, u32 y, u32 z); /* 0x4BE430 */
void DC2CALL Masks_BuildTexture(void);                  /* 0x4BE6E0 */
void DC2CALL Masks_Upload(u32 a);                       /* 0x4BEAC0 */
void DC2CALL Masks_ShiftDepth(u32 d);                   /* 0x4BE6A0: all masks d deeper */
u32  DC2CALL Masks_FillMarker(u32 i, gptr p);           /* 0x4BEC40: mask i into the marker p; its z */
void DC2CALL SetMaskMarker(gptr p);                     /* 0x4C06B0: code 0x88 */

/* the pads (DirectInput): raw buttons into 0x87DC42 + 0x24 * pad */
void DC2CALL Input_ReadPad(u32 pad);                    /* 0x4BEF40 */
#ifndef DC2_IN_PROCESS
extern u16 g_pad_raw[2];          /* standalone: what the driver presses */
#endif

/* cooperative tasks: give up the CPU for n frames (0x4C29A9, task.c) */
void DC2CALL Task_Sleep(u32 n);

#ifndef DC2_IN_PROCESS
extern const char *g_pc_root;     /* folder that holds "data" (default ".") */
#define HEAP_SIZE 0x2000000u      /* after the image and the TMP area; music streams are ~8 MB */
#endif
#endif
