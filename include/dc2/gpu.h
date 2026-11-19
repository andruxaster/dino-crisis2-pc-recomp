/* The PC port's GPU layer (0x4B4000..0x4C2000, PsyQ libgpu on Direct3D),
 * rewritten for the standalone builds (src/pc/gpu.c). Primitives keep the
 * port's own format in game memory (float screen xy, z 0.01 by default,
 * full 32-bit links with bit 31 for "has a code"); VRAM is a software copy
 * of the PS1's 1024x512 at 0x76448C. In the hook build and the harness the
 * game calls the original functions (orig.h); these are tested against them. */
#ifndef DC2_GPU_H
#define DC2_GPU_H
#include "types.h"

#define VRAM_BASE  0x76448Cu       /* 1024 x 512 x 16 bits */

void DC2CALL PcGpu_AddPrim(gptr ot, gptr p);                        /* 0x4C05D0 */
void DC2CALL PcGpu_AddPrims(gptr ot, gptr p0, gptr p1);             /* 0x4C0600 */
void DC2CALL PcGpu_SetSprt(gptr p);                                 /* 0x4C0690 */
void DC2CALL PcGpu_SetTile(gptr p);                                 /* 0x4C06A0 */
void DC2CALL PcGpu_SetMaskMarker(gptr p);                           /* 0x4C06B0 */
void DC2CALL PcGpu_SetSemiTrans(gptr p, u32 on);                    /* 0x4C06C0 */
void DC2CALL PcGpu_SetDrawArea(gptr p, gptr r);                     /* 0x4C06E0 */
void DC2CALL PcGpu_SetDrawMode(gptr p, u32 dfe, u32 dtd, u32 tpage, u32 tw); /* 0x4C06F0 */
void DC2CALL PcGpu_SetDrawVramOp(gptr p, gptr rect, u32 x, u32 y);  /* 0x4C0740 */
void DC2CALL PcGpu_SetDefDispEnv(gptr env, s32 x, s32 y, s32 w, s32 h); /* 0x4C07F0 */
void DC2CALL PcGpu_SetDefDrawEnv(gptr env, s32 x, s32 y, s32 w, s32 h); /* 0x4C0820 */
void DC2CALL PcGpu_ClearOTagR(gptr ot, s32 n);               /* 0x4C0870 */
u32  DC2CALL PcGpu_GetTPage(u32 tp, u32 abr, u32 x, u32 y);        /* 0x4C08A0 */
void DC2CALL PcGpu_LoadImage(gptr rect, gptr p);                    /* 0x4B4D20 */
void DC2CALL PcGpu_LoadImageIfChanged(gptr rect, gptr p);           /* 0x4B4DB0 */
void DC2CALL PcGpu_StoreImage(gptr rect, gptr p);                   /* 0x4B49F0 */

/* the rooms' JPEG backgrounds (src/pc/jpeg.c; libjpeg 6b's pixels) */
void DC2CALL PcJpeg_Decode(gptr dst, gptr src, u32 size, u32 mode); /* 0x4D2990 */

#endif
