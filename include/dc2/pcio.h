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

/* cooperative tasks: give up the CPU for n frames (0x4C29A9, task.c) */
void DC2CALL Task_Sleep(u32 n);

#ifndef DC2_IN_PROCESS
extern const char *g_pc_root;     /* folder that holds "data" (default ".") */
#define HEAP_SIZE 0x2000000u      /* after the image and the TMP area; music streams are ~8 MB */
#endif
#endif
