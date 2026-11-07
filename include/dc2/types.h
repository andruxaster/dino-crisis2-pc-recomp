/* dc2re -- Dino Crisis 2 PC (Dino2.exe, 2002) recompilation.
 * Basic types and access to the game's memory.
 *
 * All game data keeps the layout of the original executable: a pointer
 * stored inside game memory is a 32-bit address of the original process
 * (0x400000..0x883000). Code never dereferences such a value directly; it
 * goes through MEM(), which is the identity when this code runs inside
 * Dino2.exe (hook build) and an offset into an emulated image elsewhere
 * (standalone Linux / Windows / Dreamcast builds).
 */
#ifndef DC2_TYPES_H
#define DC2_TYPES_H
#include <stdint.h>
#include <string.h>

typedef uint8_t u8;   typedef int8_t s8;
typedef uint16_t u16; typedef int16_t s16;
typedef uint32_t u32; typedef int32_t s32;
typedef u32 gptr;                 /* address in the original process */

#define IMAGE_BASE 0x400000u
#define IMAGE_SIZE 0x483000u      /* SizeOfImage of Dino2.exe */
#define TMP_SIZE   0x4000u        /* standalone: scratch after the image (orig.c) */

extern u8 *g_membase;             /* host address of IMAGE_BASE */

#define MEM(a)    (g_membase + ((u32)(a) - IMAGE_BASE))
#define U8(a)     (*(u8 *)MEM(a))
#define S8(a)     (*(s8 *)MEM(a))
#define U16(a)    (*(u16 *)MEM(a))
#define S16(a)    (*(s16 *)MEM(a))
#define U32(a)    (*(u32 *)MEM(a))
#define S32(a)    (*(s32 *)MEM(a))
/* game address of a host pointer into the image */
#define GADDR(p)  ((gptr)((const u8 *)(p) - g_membase) + IMAGE_BASE)

/* calling convention of the original code (MSVC6 cdecl) */
#if defined(_WIN32) && defined(__i386__)
#define DC2CALL __cdecl
#else
#define DC2CALL
#endif

#endif
