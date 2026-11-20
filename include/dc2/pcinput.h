/* The PC layer's key, joystick and display-mode tables (src/pc/input.c). */
#ifndef DC2_PCINPUT_H
#define DC2_PCINPUT_H
#include "dc2/types.h"

gptr DC2CALL PcIn_KeyName(u32 k);                                    /* 0x4BF140 */
u32  DC2CALL PcIn_KeyOf(u32 b);                                      /* 0x4BF1E0 */
void DC2CALL PcIn_BindKey(u32 b, u32 k);                             /* 0x4BF210 */
void DC2CALL PcIn_DefaultKeys(void);                                 /* 0x4BF250 */
void DC2CALL PcIn_FixedKeys(u32 on);                                 /* 0x4BF130 */
u32  DC2CALL PcIn_ByEscape(void);                                    /* 0x4BF330 */
void DC2CALL PcIn_BindButton(u32 b, u32 n);                          /* 0x4BF2A0 */
void DC2CALL PcIn_DisplayStep(u32 dev, u32 win, gptr can_win, gptr name, gptr w, gptr h, gptr bpp, s32 dir); /* 0x4BB8D0 */

#endif
