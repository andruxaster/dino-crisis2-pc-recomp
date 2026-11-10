/* The PC port's drawing (src/pc/drawot.c): DrawOTag and its helpers, copied
 * from the port call for call onto include/dc2/gpudev.h. */
#ifndef DC2_PCDRAW_H
#define DC2_PCDRAW_H
#include "types.h"

void PcDraw_DrawOTag(gptr ot);                                   /* 0x4B50F0 */
void PcDraw_ClutRowConvert(u32 x, u32 row, s32 n);               /* 0x4B4BF0 */
void PcDraw_InvalidateRect(gptr rect, u32 mark);                 /* 0x4B4790 */
#endif
