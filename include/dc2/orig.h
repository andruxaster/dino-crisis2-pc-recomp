/* Functions not rewritten yet. In the hook build and the harness they call
 * the original code at their address; in standalone builds they report the
 * call once and do nothing (orig.c). Each entry disappears from here when
 * its C version is written.
 * ORIG_LIST(F, V): F(ret, name, addr, params, args) for functions with a
 * result, V(name, addr, params, args) for void ones. */
#ifndef DC2_ORIG_H
#define DC2_ORIG_H
#include "types.h"

#define ORIG_LIST(F, V) \
    V(Gpu_StoreImage,     0x4B49F0, (gptr rect, gptr p), (rect, p)) \
    V(Pc_SetBuffer,       0x4BD480, (gptr p), (p)) \
    F(u32, Strm_Open,     0x4BFC10, (gptr data, u32 size, u32 a), (data, size, a)) \
    V(Gpu_LoadImage,      0x4B4D20, (gptr rect, gptr p), (rect, p)) \
    V(Fn_4BF130,          0x4BF130, (u32 a), (a)) \
    F(u32, Fn_4BF330,     0x4BF330, (void), ()) \
    V(Gpu_DrawOT,         0x4C0600, (gptr ot, gptr a, gptr b), (ot, a, b)) \
    V(Gpu_SetSprt,        0x4C0690, (gptr p), (p)) \
    V(Gpu_AddPrim,        0x4C05D0, (gptr ot, gptr p), (ot, p)) \
    V(Gpu_SetDrawMode,    0x4C06F0, (gptr p, u32 dfe, u32 dtd, u32 tpage, u32 tw), (p, dfe, dtd, tpage, tw)) \
    V(Gpu_LoadImageIfChanged, 0x4B4DB0, (gptr rect, gptr p), (rect, p)) \
    V(Gpu_SetDrawArea,    0x4C06E0, (gptr p, gptr r), (p, r)) \
    V(JPEG_Decode,        0x4D2990, (gptr dst, gptr src, u32 size, u32 a), (dst, src, size, a)) \
    V(Gpu_SetTile,        0x4C06A0, (gptr p), (p)) \
    V(Gpu_SetSemiTrans,   0x4C06C0, (gptr p, u32 on), (p, on)) \
    F(u32, Gpu_GetTPage,  0x4C08A0, (u32 tp, u32 abr, u32 x, u32 y), (tp, abr, x, y)) \

#define ORIG_DECL_F(ret, name, addr, params, args) ret DC2CALL name params;
#define ORIG_DECL_V(name, addr, params, args) void DC2CALL name params;
ORIG_LIST(ORIG_DECL_F, ORIG_DECL_V)

/* MSVC rand() (0x4DDDA2; per-thread state of the CRT) */
u32 DC2CALL Crt_rand(void);

/* calls through function tables of the original (object type handlers
 * *0x87DCA0, AOT kind handlers 0x738B68) */
void DC2CALL Orig_Call1(gptr fn, u32 a);
#ifndef DC2_IN_PROCESS
/* standalone: optional stand-in for those handlers (test drivers) */
extern void (*g_orig_call1)(gptr fn, u32 a);
#endif

/* temporaries that original code receives by address (a local SVECTOR etc.) */
gptr Tmp_Push(u32 n);
void Tmp_Pop(u32 n);
#endif
