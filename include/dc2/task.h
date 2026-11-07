/* Cooperative tasks of the PC layer (0x4C2900..0x4C2A50): 4 slots of 0x20
 * bytes at 0x73C870 {u16 state (0 free, 1 sleeping, 2 ready), u16 frames
 * left, u32 saved stack, ...}, 0x73C8F0 = offset of the running slot.
 * Task_RunAll runs every ready task once per frame until it sleeps.
 * In-process the original switches stacks; standalone builds use host
 * coroutines (task.c) and call the C version of the task function, found
 * by its original address in the hook table. */
#ifndef DC2_TASK_H
#define DC2_TASK_H
#include "types.h"

#define A_TASK_SLOTS  0x73C870u
#define A_TASK_CUR    0x73C8F0u

void DC2CALL Task_RunAll(void);                      /* 0x4C2900 */
void DC2CALL Task_InitStackBase(void);               /* 0x4C2967 */
void DC2CALL Task_Start(u32 slot, gptr fn);          /* 0x4C2974 */
void DC2CALL Task_Chain(gptr fn);                    /* 0x4C29D6: replace the running task */
void DC2CALL Task_Kill(u32 slot);                    /* 0x4C2A0D */
/* Task_Sleep 0x4C29A9 is in pcio.h */

/* original address -> C function (hook table); 0 if not rewritten */
void *Hook_Find(gptr addr);
#endif
