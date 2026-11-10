/* The PC port's drawing (src/pc/drawot.c): DrawOTag and its helpers, copied
 * from the port call for call onto include/dc2/gpudev.h. */
#ifndef DC2_PCDRAW_H
#define DC2_PCDRAW_H
#include "types.h"

void PcDraw_DrawOTag(gptr ot);                                   /* 0x4B50F0 */
void PcDraw_ClutRowConvert(u32 x, u32 row, s32 n);               /* 0x4B4BF0 */
void PcDraw_InvalidateRect(gptr rect, u32 mark);                 /* 0x4B4790 */
void PcDraw_Background(gptr img, u32 scroll);                     /* 0x4BDC90 */
void PcDraw_SetFarColor(u32 rgb, u32 start, u32 end);              /* 0x4B4E60 */
void PcDraw_SetFogEnable(u32 on);
/* the font (glyphs of 0x188 bytes: 28 x 28 4-bit); the one before freed */
void PcDraw_SetFont(gptr font);                                    /* 0x4BD480 */                                  /* 0x4B4F50 */
void PcDraw_Clear(u32 mode);                                       /* 0x4B4F90 */
void PcDraw_Present(void);                                         /* 0x4B5070 */
void PcDraw_SetBackground(u32 r, u32 g, u32 b);                    /* 0x4BB7C0 */
void PcDraw_PutDispEnv(gptr env);                                  /* 0x4C0770 */
void PcDraw_PutDrawEnv(gptr env);                                  /* 0x4C07A0 */
#endif
