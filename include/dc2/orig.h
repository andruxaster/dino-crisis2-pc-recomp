/* Functions not rewritten yet. In the hook build and the harness they call
 * the original code at their address; in standalone builds they report the
 * call once and do nothing (orig.c). Each entry disappears from here when
 * its C version is written. */
#ifndef DC2_ORIG_H
#define DC2_ORIG_H
#include "types.h"

void DC2CALL Camera_LookAt(gptr target, gptr dist, gptr angles, gptr eye);/* 0x403840 */
void DC2CALL Timer_Set(u32 frames, u32 b);                               /* 0x405DE0 */
void DC2CALL Obj_UpdateMatrix(gptr obj);                                 /* 0x48FDE0 */
void DC2CALL Gte_SetGeomScreen(u32 h);                                   /* 0x4C0A10 */
void DC2CALL Gte_RotMatrix(gptr rot, gptr m);                            /* 0x4C0FA0 */
void DC2CALL Gte_SetFogFar(float f);                                     /* 0x4C1B90 */

/* temporaries that original code receives by address (a local SVECTOR etc.) */
gptr Tmp_Push(u32 n);
void Tmp_Pop(u32 n);
#endif
