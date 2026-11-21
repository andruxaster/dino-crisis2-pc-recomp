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
#define IMAGE_DATA 0x4E8000u      /* the first section after the code (.psxseg): standalone code reads nothing below */

extern u8 *g_membase;             /* host address of IMAGE_BASE */

#define MEM(a)    (g_membase + ((u32)(a) - IMAGE_BASE))
#define U8(a)     (*(u8 *)MEM(a))
#define S8(a)     (*(s8 *)MEM(a))
/* packed members: byte accesses wherever the compiler cannot prove alignment */
typedef struct __attribute__((packed, may_alias)) { u16 v; } dc2_pu16;
typedef struct __attribute__((packed, may_alias)) { s16 v; } dc2_ps16;
typedef struct __attribute__((packed, may_alias)) { u32 v; } dc2_pu32;
typedef struct __attribute__((packed, may_alias)) { s32 v; } dc2_ps32;
typedef struct __attribute__((packed, may_alias)) { float v; } dc2_pf32;
typedef struct __attribute__((packed, may_alias)) { double v; } dc2_pf64;
/* game data the x86 original reads unaligned (the pad at 0x87DC12 as a dword):
   safe on CPUs that fault on unaligned accesses (the Dreamcast's SH-4) */
#define UU16(a)   (((dc2_pu16 *)MEM(a))->v)
#define UU32(a)   (((dc2_pu32 *)MEM(a))->v)
#if defined(DC2_UNALIGNED)
/* every wider access through a packed member (slow on the SH-4: a debugging
   aid; the Dreamcast build emulates the rare unaligned accesses left in its
   exception handler instead, src/port/dc/unaligned.c) */
#define U16(a)    (((dc2_pu16 *)MEM(a))->v)
#define S16(a)    (((dc2_ps16 *)MEM(a))->v)
#define U32(a)    (((dc2_pu32 *)MEM(a))->v)
#define S32(a)    (((dc2_ps32 *)MEM(a))->v)
#define F32(a)    (((dc2_pf32 *)MEM(a))->v)
#define D64(a)    (((dc2_pf64 *)MEM(a))->v)
#else
#define U16(a)    (*(u16 *)MEM(a))
#define S16(a)    (*(s16 *)MEM(a))
#define U32(a)    (*(u32 *)MEM(a))
#define S32(a)    (*(s32 *)MEM(a))
#define F32(a)    (*(float *)MEM(a))
#define D64(a)    (*(double *)MEM(a))
#endif
/* game address of a host pointer into the image */
#define GADDR(p)  ((gptr)((const u8 *)(p) - g_membase) + IMAGE_BASE)

/* the x87's extended precision, which the game's float maths runs in: the PC
   builds repeat it (long double; bit-identical on the 32-bit x86, see gte.c),
   the SH-4 computes in single precision (its doubles go through the FPU's
   mode switch, its long double is double: both slow) */
#if defined(__SH4__) || defined(__sh__)
typedef float xreal;
#define XSQRT(x)     sqrtf(x)
#define XATAN2(y, x) atan2f(y, x)
#define XR(c)        ((xreal)(c))
#else
typedef long double xreal;
#define XSQRT(x)     sqrtl(x)
#define XATAN2(y, x) atan2l(y, x)
#define XR(c)        (c##L)
#endif

/* calling convention of the original code (MSVC6 cdecl) */
#if defined(_WIN32) && defined(__i386__)
#define DC2CALL __cdecl
#else
#define DC2CALL
#endif

#endif
