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
    V(Cutscene_End,       0x403A30, (void), ()) \
    V(Fn_448080,          0x448080, (gptr e), (e)) \
    V(Fn_448580,          0x448580, (u32 a), (a)) \
    V(Fn_448620,          0x448620, (u32 a), (a)) \
    V(Fn_4529C0,          0x4529C0, (u32 a), (a)) \
    V(Fade_Set,           0x450A70, (u32 a, u32 b, u32 c), (a, b, c)) \
    V(Fn_46FBC0,          0x46FBC0, (u32 a, u32 b, u32 c), (a, b, c)) \
    V(Message_Show,       0x4718F0, (u32 a, u32 b, gptr text, u32 d), (a, b, text, d)) \
    V(Fn_473130,          0x473130, (u32 a), (a)) \
    V(Fn_4731F0,          0x4731F0, (void), ()) \
    V(Fn_474B20,          0x474B20, (gptr e), (e)) \
    V(Fn_478B80,          0x478B80, (u32 a, u32 b), (a, b)) \
    V(Fn_47A8C0,          0x47A8C0, (gptr obj, u32 mode, gptr a, gptr b), (obj, mode, a, b)) \
    V(Fn_481D20,          0x481D20, (u32 a), (a)) \
    V(Fn_481DC0,          0x481DC0, (u32 a), (a)) \
    V(Fn_48C0B0,          0x48C0B0, (gptr obj, u32 a), (obj, a)) \
    V(Fn_48C600,          0x48C600, (u32 a), (a)) \
    V(Fn_48C640,          0x48C640, (u32 a, u32 b, u32 c), (a, b, c)) \
    V(Fn_48C9C0,          0x48C9C0, (u32 mask), (mask)) \
    V(Fn_48CD70,          0x48CD70, (u32 a, u32 b), (a, b)) \
    V(Fn_48CE20,          0x48CE20, (u32 a, u32 b, u32 c, u32 d), (a, b, c, d)) \
    V(Fn_48CE60,          0x48CE60, (u32 a), (a)) \
    V(Fn_48D070,          0x48D070, (u32 a, u32 b), (a, b)) \
    V(Fn_48D730,          0x48D730, (gptr e), (e)) \
    V(Fn_48F880,          0x48F880, (gptr obj, u32 a, u32 b, u32 c), (obj, a, b, c)) \
    V(Menu_Open,          0x495E90, (u32 a, u32 b, u32 c, u32 d, u32 e), (a, b, c, d, e)) \
    V(Snd_FreeBank,       0x48CEB0, (u32 slot), (slot)) \
    V(Snd_FreeStream,     0x48CF10, (u32 slot), (slot)) \
    V(Gpu_StoreImage,     0x4B49F0, (gptr rect, gptr p), (rect, p)) \
    V(Pc_SetBuffer,       0x4BD480, (gptr p), (p)) \
    F(u32, Snd_CreateBuffer, 0x4BF720, (gptr wave, u32 a), (wave, a)) \
    F(u32, Strm_Open,     0x4BFC10, (gptr data, u32 size, u32 a), (data, size, a)) \
    V(Gpu_LoadImage,      0x4B4D20, (gptr rect, gptr p), (rect, p)) \
    V(Fn_47AF60,          0x47AF60, (void), ()) \
    V(Fn_47ADA0,          0x47ADA0, (gptr obj), (obj)) \
    V(Model_Setup,        0x48D8B0, (gptr obj, gptr data), (obj, data)) \
    V(Fn_471A30,          0x471A30, (void), ()) \
    V(Fn_471A50,          0x471A50, (void), ()) \
    V(Fn_48D470,          0x48D470, (gptr e), (e)) \
    V(Fn_478790,          0x478790, (void), ()) \
    V(Gpu_LoadImageIfChanged, 0x4B4DB0, (gptr rect, gptr p), (rect, p))

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
