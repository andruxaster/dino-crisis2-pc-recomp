/* The PC port's drawing (src/pc/drawot.c): DrawOTag and its helpers, copied
 * from the port call for call onto include/dc2/gpudev.h. */
#ifndef DC2_PCDRAW_H
#define DC2_PCDRAW_H
#include "types.h"

void PcDraw_DrawOTag(gptr ot);                                   /* 0x4B50F0 */
void PcDraw_ClutRowConvert(u32 x, u32 row, s32 n);               /* 0x4B4BF0 */
void PcDraw_InvalidateRect(gptr rect, u32 mark);                 /* 0x4B4790 */
void PcDraw_DropTextures(void);                                         /* (port) every cached texture dropped: rebuilt from VRAM */
void PcDraw_VramCopy(gptr rect, s32 x, s32 y);                   /* 0x4B4A60 */
void PcDraw_MoveImage(gptr rect, s32 x, s32 y);                  /* 0x4B4B30 */
/* the background masks (the PC layer's 0x4BE330.. module) */
void PcDraw_MasksCreate(u32 nverts, gptr pixels);                /* 0x4BE3C0 */
void PcDraw_MasksSetQuad(u32 i, u32 x, u32 y, u32 z);            /* 0x4BE430 */
void PcDraw_MasksShift(u32 d);                                   /* 0x4BE6A0 */
void PcDraw_MasksBuild(void);                                    /* 0x4BE6E0 */
void PcDraw_MasksUpload(u32 again);                              /* 0x4BEAC0 */
u32  PcDraw_MasksMarker(u32 i, gptr p);                          /* 0x4BEC40 */
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

/* the software rasterizers (src/pc/soft.c; no 3D device): parameters at prm (0x75CEC0),
   dst the locked back buffer's pixels */
void Raster_Flat(void *dst, gptr prm);                             /* 0x4C3FFE */
void Raster_Tex(void *dst, const void *tex, gptr prm);             /* 0x4C3E1F */
void Raster_Gouraud(void *dst, gptr prm);                          /* 0x4C4149 */
void Raster_Glyph(void *dst, const void *glyph, gptr prm);         /* 0x4C42F3 */
void PcSoft_Init(void);                                            /* 0x4C3C81: spans for the pixel format */
extern int g_soft_mmx;                                             /* -1: ask the CPU (tests set 0 / 1) */
#endif
