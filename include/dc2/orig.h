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
    V(Timer_Set,          0x405DE0, (u32 frames, u32 b), (frames, b)) \
    V(Fn_474B20,          0x474B20, (gptr e), (e)) \
    V(Fn_47A8C0,          0x47A8C0, (gptr obj, u32 mode, gptr a, gptr b), (obj, mode, a, b)) \
    V(Menu_Open,          0x495E90, (u32 a, u32 b, u32 c, u32 d, u32 e), (a, b, c, d, e)) \
    V(Gpu_StoreImage,     0x4B49F0, (gptr rect, gptr p), (rect, p)) \
    V(Pc_SetBuffer,       0x4BD480, (gptr p), (p)) \
    F(u32, Strm_Open,     0x4BFC10, (gptr data, u32 size, u32 a), (data, size, a)) \
    V(Gpu_LoadImage,      0x4B4D20, (gptr rect, gptr p), (rect, p)) \
    V(Fn_496410,          0x496410, (void), ()) \
    V(Fn_452450,          0x452450, (void), ()) \
    V(Fn_4BF130,          0x4BF130, (u32 a), (a)) \
    V(Fn_4A7250,          0x4A7250, (void), ()) \
    V(Fn_405E40,          0x405E40, (void), ()) \
    F(u32, Fn_4BF330,     0x4BF330, (void), ()) \
    F(u32, Fn_4542F0,     0x4542F0, (gptr a, gptr b, gptr seg, u32 who), (a, b, seg, who)) \
    V(Gpu_DrawOT,         0x4C0600, (gptr ot, gptr a, gptr b), (ot, a, b)) \
    V(Gpu_SetSprt,        0x4C0690, (gptr p), (p)) \
    V(Gpu_AddPrim,        0x4C05D0, (gptr ot, gptr p), (ot, p)) \
    V(Gpu_SetDrawMode,    0x4C06F0, (gptr p, u32 dfe, u32 dtd, u32 tpage, u32 tw), (p, dfe, dtd, tpage, tw)) \
    V(Gpu_LoadImageIfChanged, 0x4B4DB0, (gptr rect, gptr p), (rect, p)) \
    V(Fn_404CC0,          0x404CC0, (void), ()) \
    V(Fn_406050,          0x406050, (u32 a, u32 b), (a, b)) \
    V(Fn_406520,          0x406520, (u32 a, u32 b), (a, b)) \
    V(JPEG_Decode,        0x4D2990, (gptr dst, gptr src, u32 size, u32 a), (dst, src, size, a)) \
    V(Fn_495D70,          0x495D70, (gptr list, u32 n), (list, n)) \
    V(Fn_495DF0,          0x495DF0, (void), ()) \
    V(Fn_48FDA0,          0x48FDA0, (void), ()) \
    V(Gpu_SetTile,        0x4C06A0, (gptr p), (p)) \
    V(Gpu_SetSemiTrans,   0x4C06C0, (gptr p, u32 on), (p, on)) \
    F(u32, Gpu_GetTPage,  0x4C08A0, (u32 tp, u32 abr, u32 x, u32 y), (tp, abr, x, y)) \
    V(Fn_41F080,          0x41F080, (u32 x, u32 y, u32 c, u32 d, u32 e, u32 f), (x, y, c, d, e, f)) \
    V(Fn_41F3E0,          0x41F3E0, (gptr d), (d)) \
    V(Fn_41F390,          0x41F390, (gptr d), (d)) \
    V(Fn_41F170,          0x41F170, (gptr d), (d)) \

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
