/* harness.exe -- checks every reimplemented function against the original.
 * Loads Dino2.exe at its own base 0x400000 inside this process (no game
 * start-up is run), then for each test: snapshot memory, run the original,
 * keep the result, restore, run the reimplementation, compare return value
 * and every byte of the writable sections.
 *   usage: harness Dino2.exe [DATA_DIR] [-n ITERATIONS]
 * Build: i686-w64-mingw32 as harness.dll + harness_stub.exe (image base
 * 0x400000, reserves the game's range), run on Windows/wine. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "dc2/game.h"
#include "../src/hook/hooks.h"
#include "dc2/orig.h"
#include "dc2/gte.h"
#include "dc2/gpu.h"
#include "dc2/pcdraw.h"
#include "dc2/task.h"

u8 *g_membase = (u8 *)IMAGE_BASE;

/* ---------------------------------------------------------- PE image load */
static int load_image(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) { printf("cannot open %s\n", path); return 0; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    u8 *d = malloc(n); fread(d, 1, n, f); fclose(f);
    u32 pe = *(u32 *)(d + 0x3C);
    u16 nsec = *(u16 *)(d + pe + 6), optsz = *(u16 *)(d + pe + 20);
    u32 base = *(u32 *)(d + pe + 24 + 28), size = *(u32 *)(d + pe + 24 + 56);
    if (base != IMAGE_BASE || size != IMAGE_SIZE) { printf("unexpected image %x/%x\n", base, size); return 0; }
    /* the range belongs to the stub image: make it writable/executable, clear it */
    u8 *m = (u8 *)IMAGE_BASE;
    DWORD old;
    if (!VirtualProtect(m + 0x1000, IMAGE_SIZE - 0x1000, PAGE_EXECUTE_READWRITE, &old)) {
        printf("cannot unprotect 0x401000 (err %lu): run through harness_stub.exe\n", GetLastError()); return 0;
    }
    memset(m + 0x1000, 0, IMAGE_SIZE - 0x1000);
    /* headers stay the stub's (the loader still refers to them) */
    for (int i = 0; i < nsec; i++) {
        u8 *s = d + pe + 24 + optsz + 40 * i;
        u32 va = *(u32 *)(s + 12), rs = *(u32 *)(s + 16), ro = *(u32 *)(s + 20);
        if (rs) memcpy(m + va, d + ro, rs);
    }
    /* imports: real functions where available (never called by the tests) */
    u32 imp = *(u32 *)(d + pe + 24 + 104);
    for (u32 o = imp; *(u32 *)(m + o + 12); o += 20) {
        HMODULE lib = LoadLibraryA((char *)m + *(u32 *)(m + o + 12));
        u32 *ilt = (u32 *)(m + (*(u32 *)(m + o) ? *(u32 *)(m + o) : *(u32 *)(m + o + 16)));
        u32 *iat = (u32 *)(m + *(u32 *)(m + o + 16));
        for (; *ilt; ilt++, iat++) {
            FARPROC p = NULL;
            if (lib) p = (*ilt & 0x80000000) ? GetProcAddress(lib, (char *)(uintptr_t)(*ilt & 0xFFFF))
                                            : GetProcAddress(lib, (char *)m + *ilt + 2);
            *iat = (u32)(uintptr_t)p;
        }
    }
    free(d);
    return 1;
}

/* ------------------------------------------------------- snapshot compare */
typedef struct { u32 lo, hi; } Range;
static const Range RW[] = { {0x4E8000, 0x703000},      /* .psxseg */
                            {0x71B000, 0x882000} };    /* .data + bss */
static u8 *snap0[2], *snap1[2];

static void snap(u8 **dst) { for (int i = 0; i < 2; i++) memcpy(dst[i], MEM(RW[i].lo), RW[i].hi - RW[i].lo); }
static void restore(u8 **src) { for (int i = 0; i < 2; i++) memcpy(MEM(RW[i].lo), src[i], RW[i].hi - RW[i].lo); }

static int g_fail, g_tests;
static u32 g_ign_addr, g_ign_len;    /* bytes the original fills with stack garbage */
static u32 g_ign2_addr, g_ign2_len;  /* a second such range */
static int g_ign_objpads;            /* matrix pads (+0x32) of test objects and parts */
static int g_ign_door;               /* door scene: matrix pads */
static int g_ign_campad;             /* the camera matrix's pad (G+0x78A) */
static int g_ign_draw;              /* the frame's drawing (see compare) */
static int g_ign_throw;              /* thrown sub-weapons: stack pointers / bytes kept (see below) */
static char g_what[256];

/* after the reimplementation ran: compare with snap1 (original's result) */
static int compare(const char *name, u32 r_orig, u32 r_new, u32 rmask)
{
    g_tests++;
    if ((r_orig & rmask) != (r_new & rmask)) {
        if (g_fail++ < 4000) printf("FAIL %s: return %08X, original %08X  [%s]\n", name, r_new, r_orig, g_what);
        return 0;
    }
    if (g_ign_objpads) {        /* ... and the RT pad, copied from a parent matrix */
        snap1[1][GTE_RT + 0x12 - RW[1].lo] = U8(GTE_RT + 0x12);
        snap1[1][GTE_RT + 0x13 - RW[1].lo] = U8(GTE_RT + 0x13);
    }
    for (u32 k = 0; k < 36; k++) {             /* matrix pads (+0x12: MulMatrix0's stack bytes) of objects, parts, effects, props */
        u32 ad[5] = {0x690000 + (k % 16) * 0x260 + 0x12, 0x693000 + k * 0x80 + 0x12, 0x694000 + k * 0x70 + 0x12, 0x695000 + (k % 20) * 0xAC + 0x12,
                     k < 20 ? 0x6C8400 + k * 0x80 + 0x12 : 0x6C8000 + 0x12};   /* (and the animation test's object and parts) */
        for (int j = 0; j < 5; j++) { snap1[0][ad[j] - RW[0].lo] = U8(ad[j]); snap1[0][ad[j] + 1 - RW[0].lo] = U8(ad[j] + 1); }
    }
    if (g_ign_draw) {           /* the drawing tests: matrix pads (MulMatrix0's stack bytes), the GTE vectors' pads */
        u32 ad[128]; int n = 0;
        for (u32 i = 0; i < 16; i++) { ad[n++] = 0x690000 + i * 0x260 + 0x12; ad[n++] = 0x693000 + i * 0x100 + 0x12; }
        for (u32 i = 0; i < 8; i++) { ad[n++] = 0x694000 + i * 0x70 + 0x12; ad[n++] = 0x694000 + i * 0x70 + 0x32; }
        for (u32 i = 0; i < 20; i++) { ad[n++] = 0x695000 + i * 0xAC + 0x12; ad[n++] = 0x695000 + i * 0xAC + 0x32; }
        ad[n++] = GTE_V0 + 6; ad[n++] = GTE_V1 + 6; ad[n++] = GTE_V2 + 6; ad[n++] = 0x6ED112;
        for (int k = 0; k < n; k++)
            for (u32 a = ad[k]; a < ad[k] + 2; a++) { int i = a < 0x703000 ? 0 : 1; snap1[i][a - RW[i].lo] = U8(a); }
    }
    if (g_ign_objpads)
        for (int i = 0; i < 16; i++) {
            u32 o = 0x690000 + i * 0x260, p = 0x693000 + i * 0x100;
            u32 pads[3] = {o + 0x32, p + 0x32, p + 0x80 + 0x32};
            for (int j = 0; j < 3; j++) {
                snap1[0][pads[j] - RW[0].lo] = U8(pads[j]);
                snap1[0][pads[j] + 1 - RW[0].lo] = U8(pads[j] + 1);
            }
        }
    if (g_ign_objpads)          /* animation test object and its parts */
        for (int i = 0; i <= 20; i++) {
            u32 a = i == 20 ? 0x6C8000 + 0x32 : 0x6C8400 + i * 0x80 + 0x32;
            snap1[0][a - RW[0].lo] = U8(a); snap1[0][a + 1 - RW[0].lo] = U8(a + 1);
        }
    for (u32 k = 0; k < 12; k++) {              /* object-effect slots: matrix pads (stack garbage of MulMatrix0) */
        u32 a = 0x694000u + k * 0x70 + 0x32;
        snap1[0][a - RW[0].lo] = U8(a); snap1[0][a + 1 - RW[0].lo] = U8(a + 1);
    }
    for (u32 k = 0; k < 40; k++) {              /* parts of the pool (Model_Setup): matrix pads */
        u32 a = 0x6DA000u + k * 0xCC + 0x32;
        snap1[0][a - RW[0].lo] = U8(a); snap1[0][a + 1 - RW[0].lo] = U8(a + 1);
    }
    if (g_ign_throw) {          /* the effects' position pointer (a stack vector in the original), a mine's +0x46 (a stack byte) */
        for (u32 k = 0; k < 40; k++)
            for (u32 j = 0; j < 4; j++) { u32 a = 0x6D6000u + k * 0xAC + 0xA0 + j; if (a >= RW[0].lo && a < RW[0].hi) snap1[0][a - RW[0].lo] = U8(a); }
        for (u32 k = 0; k < 12; k++) { u32 a = 0x694000u + k * 0x70 + 0x46; snap1[0][a - RW[0].lo] = U8(a); }
    }
    if (g_ign_campad)
        for (u32 a = 0x87DE20u + 0x78A; a < 0x87DE20u + 0x78C; a++) snap1[1 - (a < 0x703000)][a - RW[1 - (a < 0x703000)].lo] = U8(a);
    if (g_ign_door)             /* the door scene's objects and parts: matrix pads */
        for (u32 k = 0; k < 3 + 0x1E + 3; k++) {
            u32 a = k < 3 ? 0x748A60u + 0x530 + k * 0x260 + 0x32 : k < 0x21 ? 0x748A60u + 0xC50 + (k - 3) * 0xCC + 0x32
                                                                          : 0x6DDB00u + (k - 0x21) * 0x80 + 0x32;
            snap1[1 - (a < 0x703000)][a - RW[1 - (a < 0x703000)].lo] = U8(a);
            snap1[1 - (a < 0x703000)][a + 1 - RW[1 - (a < 0x703000)].lo] = U8(a + 1);
        }
    for (u32 k = 0; k < g_ign_len; k++)
        for (int i = 0; i < 2; i++)
            if (g_ign_addr + k >= RW[i].lo && g_ign_addr + k < RW[i].hi)
                snap1[i][g_ign_addr + k - RW[i].lo] = U8(g_ign_addr + k);
    for (u32 k = 0; k < g_ign2_len; k++)
        for (int i = 0; i < 2; i++)
            if (g_ign2_addr + k >= RW[i].lo && g_ign2_addr + k < RW[i].hi)
                snap1[i][g_ign2_addr + k - RW[i].lo] = U8(g_ign2_addr + k);
    for (int i = 0; i < 2; i++) {
        u8 *cur = MEM(RW[i].lo);
        u32 n = RW[i].hi - RW[i].lo;
        if (memcmp(cur, snap1[i], n)) {
            u32 k = 0; while (cur[k] == snap1[i][k]) k++;
            if (g_fail++ < 4000) printf("FAIL %s: memory %08X = %02X, original %02X  [%s]\n",
                                      name, RW[i].lo + k, cur[k], snap1[i][k], g_what);
            if (getenv("ALLDIFF"))
                for (u32 j = k; j < n; j++) if (cur[j] != snap1[i][j]) printf("  diff %08X = %02X, original %02X\n", RW[i].lo + j, cur[j], snap1[i][j]);
            return 0;
        }
    }
    return 1;
}

/* run original (address) and new function with the same arguments */
typedef u32 (__cdecl *F0)(void);
typedef u32 (__cdecl *F1)(u32);
typedef u32 (__cdecl *F2)(u32, u32);
typedef u32 (__cdecl *F3)(u32, u32, u32);
typedef u32 (__cdecl *F4)(u32, u32, u32, u32);
typedef u32 (__cdecl *F10)(u32, u32, u32, u32, u32, u32, u32, u32, u32, u32);

static u32 call(void *f, int n, const u32 *a)
{
    switch (n) {
    case 0: return ((F0)f)();
    case 1: return ((F1)f)(a[0]);
    case 2: return ((F2)f)(a[0], a[1]);
    case 3: return ((F3)f)(a[0], a[1], a[2]);
    case 4: return ((F4)f)(a[0], a[1], a[2], a[3]);
    default: return ((F10)f)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9]);
    }
}

static void __attribute__((noinline)) clear_stack(void) { volatile u32 b[0x1000]; for (int i = 0; i < 0x1000; i++) b[i] = 0; (void)b[0]; }
/* a function without arguments called with the 4 KB under its return address zeroed
   (its own frame then holds zeros where the original copies unwritten stack bytes) */
u32 g_save_edi __asm__("g_save_edi_");
u32 __cdecl call0z(void *f) __asm__("call0z_");
__asm__(".text\n"
        "call0z_:\n\t"
        "movl 4(%esp), %edx\n\t"
        "movl %edi, g_save_edi_\n\t"
        "leal -0x1000(%esp), %edi\n\t"
        "movl $0x400, %ecx\n\t"
        "xorl %eax, %eax\n\t"
        "rep stosl\n\t"
        "movl g_save_edi_, %edi\n\t"
        "jmp *%edx\n");
static const char *g_cur_name;
static int g_cur_orig;
static int check(u32 addr, void *fn, const char *name, int nargs, const u32 *args, u32 rmask)
{
    g_cur_name = name; g_cur_orig = 1;
    snap(snap0);
    clear_stack();                      /* (stack bytes the original copies are 0 then) */
    u32 r0 = nargs ? call((void *)(uintptr_t)addr, nargs, args) : call0z((void *)(uintptr_t)addr);
    snap(snap1);
    restore(snap0);
    g_cur_orig = 0;
    gptr t0 = Tmp_Push(0);              /* (the scratch stack balanced again after it) */
    u32 r1 = call(fn, nargs, args);
    int ok = compare(name, r0, r1, rmask);
    if (Tmp_Push(0) != t0) {
        if (ok && g_fail++ < 4000) printf("FAIL %s: scratch stack off by %d  [%s]\n", name, (int)(t0 - Tmp_Push(0)), g_what);
        Tmp_Pop(t0 - Tmp_Push(0)); ok = 0;
    }
    restore(snap1);                     /* continue from the original's state */
    return ok;
}

/* ------------------------------------------------------------------ random */
static u32 rs = 0x2468ACE1;
static u32 rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static s32 rnd_s16(void) { return (s16)rnd(); }

/* ------------------------------------------------------------------- tests */
static int iters = 2000;

static void test_flags(void)
{
    for (int i = 0; i < iters; i++) {
        u32 g = rnd() % 12, cnt = U32(A_FLAG_GROUPS + g * 8 + 4);
        u32 a[3] = {g, rnd() % cnt, rnd() & 1};
        if (rnd() % 16 == 0) { a[0] = 1; a[1] = 0; }
        sprintf(g_what, "group %u bit %u on %u", a[0], a[1], a[2]);
        check(0x46FAA0, Flag_Set, "Flag_Set", 3, a, 0);
        check(0x46FB20, Flag_Check, "Flag_Check", 2, a, 0xFFFFFFFF);
        u32 b[3] = {0x6F0000 + (rnd() % 64) * 4, rnd() % 256, rnd() & 1};
        check(0x46FB50, Bits_Set, "Bits_Set", 3, b, 0);
        check(0x46FB90, Bits_Check, "Bits_Check", 2, b, 0xFFFFFFFF);
    }
}

static void test_geom(void)
{
    gptr pos = 0x6F1000, quad = 0x6F1010;
    for (int i = 0; i < iters * 4; i++) {
        int sc = (rnd() & 1) ? 0x3FFF : 0x7FFF;           /* also big coordinates */
        for (int k = 0; k < 3; k++) S16(pos + 2 * k) = (s16)((s32)(rnd() % (2 * sc + 1)) - sc);
        if (i & 1) {       /* convex quad around a point, so that hits happen */
            s32 cx = rnd_s16() / 4, cz = rnd_s16() / 4, r = 200 + rnd() % 8000;
            S16(quad + 0) = cx - r; S16(quad + 2) = cz + r; S16(quad + 4) = cx + r; S16(quad + 6) = cz + r;
            S16(quad + 8) = cx + r; S16(quad + 10) = cz - r; S16(quad + 12) = cx - r; S16(quad + 14) = cz - r;
            S16(pos) = cx + (s32)(rnd() % (4 * r)) - 2 * r; S16(pos + 4) = cz + (s32)(rnd() % (4 * r)) - 2 * r;
            switch (rnd() % 4) {           /* points on a corner or an edge: equality in the tests */
            case 0: { int v = rnd() % 4; S16(pos) = S16(quad + 4 * v); S16(pos + 4) = S16(quad + 4 * v + 2); break; }
            case 1: S16(pos) = cx + (s32)(rnd() % (2 * r)) - r; S16(pos + 4) = (rnd() & 1) ? cz + r : cz - r; break;
            case 2: S16(pos + 4) = cz + (s32)(rnd() % (2 * r)) - r; S16(pos) = (rnd() & 1) ? cx + r : cx - r; break;
            }
        } else for (int k = 0; k < 8; k++) S16(quad + 2 * k) = rnd_s16() / 2;
        u32 a[2] = {pos, quad};
        sprintf(g_what, "pos %d,%d", S16(pos), S16(pos + 4));
        check(0x48F790, PointInQuad, "PointInQuad", 2, a, 0xFFFFFFFF);
    }
    for (u32 area = 0; area < 10; area++)
        for (u32 room = 0; room < 24; room++) {
            u32 a[2] = {area, room};
            sprintf(g_what, "area %u room %u", area, room);
            check(0x472210, Room_GlobalIndex, "Room_GlobalIndex", 2, a, 0xFF);
        }
}

/* script: scratch thread area and script image in .psxseg */
#define T_BASE   0x6E0000u     /* 10 threads */
#define S_BASE   0x6E1000u     /* routine table + code */
#define GL_BASE  0x6E3000u     /* globals */

static void snd_setup(void);
static void draw_safe(void);
static void draw_quiet(void);
static void script_setup(void)
{
    snd_setup();
    U32(A_G_PTR) = A_G_STATIC;
    U32(A_SCR_THREADS) = T_BASE;
    U32(A_SCR_GLOBALS) = GL_BASE;
    U32(G() + G_SCRIPT) = S_BASE;
    for (int i = 0; i < 64; i++) U32(GL_BASE + 4 * i) = rnd();
    for (int i = 0; i < 16; i++) U32(S_BASE + 4 * i) = 0x40 + 0x80 * i;   /* routine offsets */
}

static void random_thread(gptr t)
{
    memset(MEM(t), 0, SCR_THREAD_SIZE);
    for (int i = 4; i < 0x80; i += 4) U32(t + i) = rnd() % 7 == 0 ? (u32)((s32)(rnd() % 64) - 32) : rnd();
    U32(t + T_SP) = t + 0x40 + 4 * (rnd() % 8);
    for (int i = 0; i < 4; i++) U32(t + T_REG + 4 * i) = rnd() % 3 ? rnd() % 200 - 100 : rnd();
    U8(t + T_COND) = (u8)rnd(); U8(t + T_DEPTH) = rnd() % 3; U8(t + T_STATE) = 1;
    U32(t + T_PC) = S_BASE + 0x400 + 2 * (rnd() % 64);
}

static void test_script_values(void)
{
    script_setup();
    gptr t = T_BASE;
    for (int i = 0; i < iters; i++) {
        random_thread(t);
        u32 types[] = {0, 1, 2, 6, 3, 0x101, 4};
        u32 ty = types[rnd() % 7];
        s32 v = ty == 1 ? (s32)(rnd() % 4) : ty == 2 ? (s32)(rnd() % 8) - 2 : ty == 6 ? (s32)(rnd() % 64) : rnd_s16();
        u32 a[4] = {t, ty, (u32)v, rnd()};
        sprintf(g_what, "type %x v %d", ty, v);
        check(0x48B8E0, Script_GetValue, "Script_GetValue", 3, a, 0xFFFFFFFF);
        check(0x48B930, Script_SetValue, "Script_SetValue", 4, a, 0);
        u32 b[3] = {rnd() % 4 ? rnd() % 2000 - 1000 : rnd(), rnd() % 4 ? rnd() % 40 - 20 : rnd(), rnd() % 30};
        if (rnd() % 4 == 0) b[1] = b[0];                  /* equal operands: edge cases of compares */
        if ((b[2] == 3 || b[2] == 4) && (b[1] == 0 || (b[1] == 0xFFFFFFFF && b[0] == 0x80000000))) b[1] = 7;
        sprintf(g_what, "binop %u (%d, %d)", b[2], b[0], b[1]);
        check(0x48B980, Script_BinOp, "Script_BinOp", 3, b, 0xFFFFFFFF);
        u32 c[2] = {t, rnd() % 3 ? rnd() % 5 - 2 : rnd()};
        check(0x48BB20, Script_SetCond, "Script_SetCond", 2, c, 0);
    }
}

static void test_script_ops(void)
{
    static const struct { u8 op; u32 addr; void *fn; const char *name; } ops[] = {
        {0x00, 0x489990, ScrOp_Nop, "ScrOp_Nop"},       {0x01, 0x4873C0, ScrOp_Load, "ScrOp_Load"},
        {0x02, 0x487400, ScrOp_Store, "ScrOp_Store"},   {0x03, 0x487440, ScrOp_Arith, "ScrOp_Arith"},
        {0x04, 0x4874D0, ScrOp_Return, "ScrOp_Return"}, {0x05, 0x487510, ScrOp_Push, "ScrOp_Push"},
        {0x06, 0x487550, ScrOp_Pop, "ScrOp_Pop"},       {0x07, 0x487590, ScrOp_Branch, "ScrOp_Branch"},
        {0x08, 0x4875D0, ScrOp_Call, "ScrOp_Call"},     {0x10, 0x487620, ScrOp_Wait, "ScrOp_Wait"},
        {0x1C, 0x487B70, ScrOp_FlagSet, "ScrOp_FlagSet"}, {0x1D, 0x487BF0, ScrOp_FlagCheck, "ScrOp_FlagCheck"},
    };
    script_setup();
    gptr t = T_BASE + SCR_THREAD_SIZE * 3;
    for (int i = 0; i < iters; i++)
        for (unsigned k = 0; k < sizeof ops / sizeof ops[0]; k++) {
            random_thread(t);
            gptr pc = U32(t + T_PC);
            for (int b = 0; b < 8; b++) U8(pc + b) = (u8)rnd();
            U8(pc) = ops[k].op;
            u8 types[] = {0, 1, 2, 6, 4};
            switch (ops[k].op) {
            case 1: case 2: U8(pc + 1) = rnd() % 4; U8(pc + 2) = types[rnd() % 5]; S16(pc + 4) = rnd() % 8; break;
            case 3: {
                u8 opn = rnd() % 27; U8(pc + 1) = opn; U8(pc + 2) = rnd() % 5 == 0 ? 0xFF : rnd() % 4;
                U8(pc + 3) = types[rnd() % 5]; S16(pc + 4) = rnd() % 8;
                if (opn == 3 || opn == 4) { U8(pc + 3) = 0; S16(pc + 4) = 1 + rnd() % 9; }
                if (U8(pc + 2) == 0xFF) { U8(pc + 1) = rnd() % 2; U8(pc + 3) = 0; S16(pc + 4) = rnd() % 5; }
                break; }
            case 5: case 6: U8(pc + 1) = types[rnd() % 5]; S16(pc + 2) = rnd() % 8; break;
            case 7: U8(pc + 1) = rnd() % 10; S16(pc + 2) = (rnd() % 64) * 2 - 64; break;
            case 8: S16(pc + 2) = rnd() % 16; break;
            case 0x1C: case 0x1D: {
                gptr sp = U32(t + T_SP); u32 g = rnd() % 12;
                U32(sp + (ops[k].op == 0x1C ? 8 : 4)) = g;
                U32(sp + (ops[k].op == 0x1C ? 4 : 0)) = rnd() % 8 == 0 ? 0xFF : rnd() % U32(A_FLAG_GROUPS + g * 8 + 4);
                if (ops[k].op == 0x1C) { U32(sp) = rnd() & 1; if (rnd() % 4 == 0) { U32(sp + 8) = 1; U32(sp + 4) = 0xFF; } }
                else if (U32(sp) == 0xFF) U32(sp) = 3;
                break; }
            }
            u32 a[1] = {t};
            sprintf(g_what, "bytes %02X %02X %02X %02X %02X %02X", U8(pc), U8(pc + 1), U8(pc + 2), U8(pc + 3), U8(pc + 4), U8(pc + 5));
            check(ops[k].addr, ops[k].fn, ops[k].name, 1, a, 0xFFFFFFFF);
        }
}


/* ---- objects and object built-ins ------------------------------------------- */
#define O_OBJ   0x690000u      /* 16 objects x 0x260 */
#define O_PART  0x693000u      /* one or two linked parts per object */
#define O_EFF   0x694000u
#define O_K2    0x695000u
#define O_K3    0x696000u
#define O_AOT   0x697000u
#define O_ZONE  0x698000u
#define O_COLL  0x699000u
#define O_CAM   0x69A000u

/* part links as the game keeps them (Obj_Free clears them; the game never
   updates a freed object afterwards) */
static void objects_links(void)
{
    for (int i = 0; i < 16; i++) {
        gptr o = O_OBJ + i * 0x260, p = O_PART + i * 0x100, q = p + 0x80;
        U32(o + 0x64) = p;
        U32(p + 0x58) = o; U32(q + 0x58) = p;              /* parent */
        U32(p + 0x54) = rnd() & 1 ? q : 0; U32(q + 0x54) = 0;
        U8(o + 0x54) = rnd() & 1;
    }
}

static void objects_setup(void)
{
    snd_setup();
    for (u32 a = O_OBJ; a < O_CAM + 0x100; a += 4) U32(a) = rnd() % 4 ? rnd() % 0x10000 : rnd();
    objects_links();
    for (int i = 0; i < 16; i++) U32(O_COLL + 8 + 4 * i) = 0x100 + 0x20 * i;
    U32(G() + 0x940) = O_OBJ; U32(G() + 0x93C) = O_EFF; U32(G() + 0x944) = O_K2;
    U32(G() + 0x948) = O_K3;  U32(G() + 0xA5C) = O_AOT; U32(G() + 0xA44) = O_ZONE;
    U32(G() + 0xA4C) = O_COLL; U32(G() + 0x7B8) = O_CAM;
    U32(0x87DC88) = 20; U32(G() + 0xA70) = rnd() % 20;   /* effect slots in O_K2 */
}

/* models (Model_Setup), the part pool and the effect/light arrays */
#define PART_POOL  0x6DA000u    /* 40 parts of 0xCC */
#define MODEL_DATA 0x6D9000u
static void model_data(gptr d)
{
    u32 n = 1 + rnd() % 16;
    U32(d + 0x14) = n; U16(d + 0x18) = (u16)rnd(); U16(d + 0x1A) = (u16)rnd();
    for (u32 i = 0; i < n; i++) {
        gptr r = d + 0x1C + i * 0x14;
        for (int k = 0; k < 3; k++) S16(r + 2 * k) = (s16)(rnd() % 400) - 200;
        S8(r + 6) = (s8)(i == 0 || rnd() % 3 == 0 ? -1 : (s32)(rnd() % i));
        U8(r + 7) = (u8)rnd();
    }
}

static void world_setup(void)
{
    U32(G() + 0x938) = PART_POOL; U32(0x87DC0C) = 40;
    for (u32 k = 0; k < 40; k++) U32(PART_POOL + k * 0xCC + 0x50) = rnd() % 8 == 0;
    model_data(MODEL_DATA); model_data(0x662500);
    U32(G() + 0x93C) = O_EFF; U32(G() + 0x948) = O_K3;
    U32(G() + 0x944) = O_K2; U32(0x87DC88) = 20; U32(G() + 0xA70) = rnd() % 20;
    U8(G() + 0x852) = rnd() % 4 ? (u8)(rnd() % 20) : 0xFF;
    for (u32 k = 0; k < 20; k++) {
        gptr e = O_K2 + k * 0xAC;
        U8(e + 0x86) = rnd() % 4 ? (u8)(rnd() % 20) : 0xFF; U8(e + 0x87) = (u8)(rnd() % 20);
        U32(e + 0x8C) = rnd() % 3 ? 0 : 0x6D5F00u + 0x20 * (rnd() % 4);   /* source block or none */
        U8(e + 0x58) = rnd() % 8;
    }
    U32(0x87F948) = 0x6DD000u;                      /* TYPE_TAB: recorders */
    for (u32 k = 0; k < 16; k++) U8(O_OBJ + k * 0x260 + 0xFD) &= 1;   /* group: Obj_PickGroup counts 2 */
}

static void obj_thread(gptr t)
{
    random_thread(t);
    U32(t + T_SP) = t + 0x40;
    for (int i = 0; i < 16; i++) U32(t + 0x40 + 4 * i) = rnd() % 3 ? rnd() % 64 : rnd();
    U8(t + T_SEL1) = rnd() % 16;
    U8(t + T_SEL0) = rnd() % 9;
}

static void test_objects(void)
{
    g_ign_objpads = 1;
    static const struct { u32 addr; void *fn; const char *name; int argmode; } ops[] = {
        {0x487690, ScrOp_ObjSelect, "ScrOp_ObjSelect", 0},
        {0x487C50, ScrOp_ObjSet, "ScrOp_ObjSet", 1},
        {0x488770, ScrOp_ObjGet, "ScrOp_ObjGet", 2},
        {0x4893E0, ScrOp_WaitFrame, "ScrOp_WaitFrame", 3},
        {0x48A220, ScrOp_Obj44Init, "ScrOp_Obj44Init", 4},
        {0x48A2C0, ScrOp_ObjEnable, "ScrOp_ObjEnable", 5},
        {0x48A670, ScrOp_CutsceneCamera, "ScrOp_CutsceneCamera", 6},
        {0x4876C0, ScrOp_EffectInit, "ScrOp_EffectInit", 7},
        {0x4877A0, ScrOp_Obj14Init, "ScrOp_Obj14Init", 8},
        {0x4878B0, ScrOp_ThreadStart, "ScrOp_ThreadStart", 9},
        {0x487900, ScrOp_ThreadKill, "ScrOp_ThreadKill", 10},
        {0x487940, ScrOp_ThreadPause, "ScrOp_ThreadPause", 10},
        {0x487970, ScrOp_ThreadResume, "ScrOp_ThreadResume", 10},
        {0x4879B0, ScrOp_ThreadGoto, "ScrOp_ThreadGoto", 11},
    };
    script_setup();
    for (int i = 0; i < iters; i++) {
        objects_setup();
        for (unsigned k = 0; k < sizeof ops / sizeof ops[0]; k++) {
            gptr t = T_BASE + SCR_THREAD_SIZE * (rnd() % 10);
            objects_links();
            obj_thread(t);
            gptr sp = U32(t + T_SP);
            switch (ops[k].argmode) {
            case 1:            /* obj_set(param, value) */
                U32(sp + 4) = rnd() % 8 ? rnd() % 45 : rnd();
                if (U8(t + T_SEL0) == 5) U32(sp + 4) = rnd() % 8 ? 30 + rnd() % 46 : rnd();   /* globals 33..72 */
                break;
            case 2:            /* obj_get(param) */
                if (U8(t + T_SEL0) == 4) U8(t + T_SEL0) = 0;      /* undefined result */
                U32(sp) = U8(t + T_SEL0) == 5 ? 25 + rnd() % 52 : rnd() % 8 ? rnd() % 45 : rnd();
                break;
            case 3: S16(0x87FC4A) = rnd() % 64; U32(sp) = rnd() % 64; if (rnd() % 8 == 0) U32(sp) = rnd(); break;
            case 4: U32(sp + 0x1C) = rnd() % 16; break;
            case 5: U32(sp + 8) = rnd() % 10; U32(sp + 4) = rnd() % 16; U32(sp) = rnd() % 3; break;
            case 6: U32(sp) = rnd() % 4 ? rnd() % 8 : (u32)-(s32)(rnd() % 3 + 1);
                    U8(G() + 0x7C4) = rnd() % 8; Flag_Set(2, 0, rnd() % 4 == 0); break;
            case 7: U32(sp + 0x38) = rnd() % 16; break;
            case 8: U32(sp + 0x28) = rnd() % 16; break;
            case 9: U32(sp + 4) = rnd() % 10; U32(sp) = rnd() % 16; Flag_Set(0, 9, rnd() % 4 == 0); break;
            case 10: U32(sp) = rnd() % 10; break;
            case 11: U32(sp) = rnd() % 16; break;
            }
            u32 a[1] = {t};
            sprintf(g_what, "kind %u sel %d sp %08X %08X %08X", U8(t + T_SEL0), S8(t + T_SEL1), U32(sp), U32(sp + 4), U32(sp + 8));
            check(ops[k].addr, ops[k].fn, ops[k].name, 1, a, 0xFFFFFFFF);
        }
        objects_links();
        u32 o[1] = {O_OBJ + (rnd() % 16) * 0x260};
        U8(o[0] + 0x50) = rnd() & 1;
        if (i < 6)          /* every kind x every parameter of obj_set / obj_get */
            for (u32 kind = 0; kind < 9; kind++)
                for (u32 p = 0; p < 80; p++) {
                    gptr t = T_BASE + SCR_THREAD_SIZE * (rnd() % 10);
                    objects_links();
                    obj_thread(t);
                    U8(t + T_SEL0) = (u8)kind;
                    gptr sp = U32(t + T_SP);
                    u32 a[1] = {t};
                    U32(sp + 4) = p; U32(sp) = rnd() % 2 ? rnd() : rnd() % 0x100 | 0x80;
                    sprintf(g_what, "set kind %u param %u value %08X", kind, p, U32(sp));
                    check(0x487C50, ScrOp_ObjSet, "ScrOp_ObjSet", 1, a, 0xFFFFFFFF);
                    if (kind == 4) continue;                  /* undefined result */
                    U32(sp) = p;
                    sprintf(g_what, "get kind %u param %u", kind, p);
                    check(0x488770, ScrOp_ObjGet, "ScrOp_ObjGet", 1, a, 0xFFFFFFFF);
                }
        check(0x48FC70, Obj_SyncLinks, "Obj_SyncLinks", 1, o, 0);
        check(0x48FC40, Obj_SavePos, "Obj_SavePos", 1, o, 0);
        check(0x48D800, Obj_Disable, "Obj_Disable", 1, o, 0);
        o[0] = O_OBJ + (rnd() % 16) * 0x260;
        check(0x48D890, Obj_Free, "Obj_Free", 1, o, 0);
        u32 c[1] = {rnd() % 4 ? rnd() % 16 : rnd()};
        U8(G() + 0xB49) = rnd() & 1;
        check(0x403A50, Camera_Switch, "Camera_Switch", 1, c, 0);
    }
    g_ign_objpads = 0;
}


/* ---- animation ------------------------------------------------------------- */
#define AN_OBJ   0x6C8000u      /* object */
#define AN_PART  0x6C8400u      /* parts, 0x80 apart */
#define AN_DATA  0x6CA000u      /* two animations, 0x1000 apart */
#define AN_TAB   0x6CE000u      /* obj+0x88 table */
#define AN_EVT   0x6CF000u      /* event table */

static void anim_data(gptr a, u32 nparts, u32 nfr)
{
    u32 stride = (u8)(((nparts * 6 + 0x13) >> 2) << 2);
    U32(a) = nfr | (rnd() % 4 ? 0 : rnd() % 8) << 16;
    U32(a + 4) = rnd() % 3 ? AN_EVT : 0;
    U32(a + 8) = rnd() % 2 ? rnd() : 0; U32(a + 0xC) = rnd() % 2 ? rnd() : 0;
    U32(a + 0x10) = rnd();
    int wide = rnd() % 4 == 0;
    for (u32 f = 0; f < 10; f++) {          /* also frames past the end (tests raise the count) */
        gptr fr = a + 0x14 + f * stride;
        for (u32 i = 0; i < stride; i += 2) U16(fr + i) = wide ? (u16)rnd() : (u16)(rnd() % 0x1400);
        S16(fr + 2) = 1 + rnd() % 8;
    }
}

/* a long animation (0x20 frames at AN_DATA + 0x2000) for o's parts, o at
   frame f of it: the sub-weapons act at frames up to 0x15 */
static void anim_long(gptr o, u32 f)
{
    u32 n = U32(o + 0x68), stride = (u8)(((n * 6 + 0x13) >> 2) << 2), nfr = 0x20;
    gptr a = AN_DATA + 0x2000, fr = a + 0x14;
    U32(a) = nfr; U32(a + 4) = 0; U32(a + 8) = 0; U32(a + 0xC) = 0; U32(a + 0x10) = rnd();
    for (u32 k = 0; k < nfr + 2; k++) {
        for (u32 i = 0; i < stride; i += 2) U16(fr + k * stride + i) = (u16)(rnd() % 0x1400);
        S16(fr + k * stride + 2) = 1 + rnd() % 8;
    }
    U32(o + 0x8C) = fr; U8(o + 0xAC) = (u8)stride; U16(o + 0x90) = (u16)nfr;
    U32(o + 0x78) = U32(o + 0x7C) = U32(o + 0x80) = U32(o + 0x84) = fr + f * stride;
    U16(o + 0x92) = (u16)f; U16(o + 0x94) = (u16)f; U16(o + 0x98) = 0; U16(o + 0x9A) = 0;
}

static void anim_setup_at(gptr o)
{
    for (u32 a = o; a < o + 0x260; a += 4) U32(a) = rnd();
    U8(o + 0xFD) &= 1;                              /* group: Obj_PickGroup counts two */
    u32 n = 1 + rnd() % 16;
    U32(o + 0x68) = n;
    U32(o + 0x64) = AN_PART;
    for (u32 i = 0; i < n; i++) {
        gptr p = AN_PART + i * 0x80;
        for (u32 a = p; a < p + 0x80; a += 4) U32(a) = rnd() % 2 ? rnd() % 0x1000 : rnd();
        U32(p + 0x54) = i + 1 < n ? p + 0x80 : 0;
        U32(p + 0x58) = i ? p - 0x80 : o;                  /* parent */
    }
    u32 nfr = 1 + rnd() % 8;
    anim_data(AN_DATA, n, nfr);
    anim_data(AN_DATA + 0x1000, n, 2 + rnd() % 7);
    for (int k = 0; k < 0x40; k++) U32(AN_TAB + 4 * k) = AN_DATA + 0x1000 * (k & 1);   /* the player uses up to 0x3C */
    U32(o + 0x88) = AN_TAB;
    U32(AN_EVT) = rnd() % 4;
    for (int i = 0; i < 4; i++) { U16(AN_EVT + 4 + 4 * i) = rnd() % 16; U16(AN_EVT + 6 + 4 * i) = (u16)(rnd() % 0x100); }   /* sound ids (Se_Remap may add 0x20: still in the 0x120 table) */
    u32 stride = (u8)(((n * 6 + 0x13) >> 2) << 2);
    gptr fr = AN_DATA + 0x14;
    U32(o + 0x8C) = fr; U8(o + 0xAC) = (u8)stride; U16(o + 0x90) = (u16)nfr;
    U32(o + 0x78) = fr + (rnd() % nfr) * stride; U32(o + 0x7C) = fr + (rnd() % nfr) * stride;
    U32(o + 0x80) = fr + (rnd() % nfr) * stride; U32(o + 0x84) = fr + (rnd() % nfr) * stride;
    U16(o + 0x94) = rnd() % nfr;
    U16(o + 0x98) = rnd() % 10;
    U16(o + 0x92) = rnd() % 64;
    U16(o + 0xAA) = rnd() % 2;
    U16(o + 0x102) = (u16)((rnd() & 0x27F) | (rnd() % 4 ? 0 : 0x20));
    U8(o + 0x103) = (u8)(U8(o + 0x103) & 0xFE) | (rnd() & 1);
    U16(o + 0x9A) = rnd() % 3 ? 0 : 1 + rnd() % 3;
    U32(o + 0x9C) = rnd() % 3 ? AN_EVT : 0;
    U16(o + 0x5A) = rnd() % 8;
    U8(G() + 0x1091) = rnd() % 3 ? 9 : 1; U8(G() + 0x1090) = rnd() % 3 ? 3 : 0;
}

static void anim_setup(void) { anim_setup_at(AN_OBJ); }

/* ---- remaining built-ins: callees replaced by recorders --------------------- */
/* While these tests run, each callee that is not rewritten yet starts with a
 * JMP to a recorder that logs its arguments into .psxseg (LOG_BASE), so the
 * memory compare also checks what the original and the new code passed on.
 * spec per argument: d dword, w low 16 bits, b low 8 bits, p pointer (the
 * bytes it points to are logged, plen[] of them). */
#define LOG_BASE 0x6DE000u
#define SEL(t)   ((s32)S8((t) + T_SEL1))
#define LOG_REC  0x80
typedef struct { u32 addr; const char *spec; u8 plen[2]; u32 ret; u8 saved[5]; void *impl; } Stub;
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
static Stub g_stubs[] = {
    {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""},
    {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""},
    {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""},
    {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""},
    {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""},
    {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""},
    {0, ""}, {0x4B4D20, "pd", {8}}, {0x4B4DB0, "pd", {8}}, {0x4DDDA2, ""},
    {0, "d"},                   /* 34: object type handlers (*0x87DCA0) */
    {0, "d"},                   /* 35: AOT kind handlers (0x738B68) */
    /* file loading: 36.. (impl set below: a fake file system) */
    {0x4C03F0, "sdd"}, {0x4C04E0, "ddd"}, {0x4C0540, "d"}, {0x4C0580, "ddd"}, {0x4C05A0, "d"},
    {0x4DD711, "d"}, {0x4DD628, "d"}, {0x4DD5D6, ""},
    {0, ""}, {0, ""}, {0x4B49F0, "pd", {8}}, {0, ""} /* (0x4BD480: rewritten) */, {0x4BF720, "dd"},
    {0x4BFC10, "ddd"},
    /* 50..: loading and room set-up */
    {0x4C29A9, "dd"}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""},
    {0, ""}, {0, ""},
    {0x4C0560, "dd"},        /* 58: Pc_SeekSet */
    {0x4C29D6, "d"},         /* 59: Task_Chain */
    /* 60..: title */
    {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""},
    {0, ""}, {0x4C0600, "ddd"}, {0x4C0690, "d"}, {0x4C05D0, "dd"}, {0x4C06F0, "ddddd"},
    /* 71..: game task */
    {0, ""}, {0, "d"}, {0, ""}, {0, ""}, {0, ""}, {0, ""},
    {0, ""}, {0x4BF130, "d"}, {0, ""}, {0, ""}, {0, ""}, {0, ""},
    {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""},
    /* 89..: object handlers (collision) */
    {0, ""}, {0, ""},
    /* 91..: player */
    {0, ""}, {0, ""}, {0x4BF330, ""},
    /* 94..: doors */
    {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""},
    /* 102..: the frame of play */
    {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0x4D2990, "dddd"}, {0, ""},
    {0, ""}, {0, ""}, {0, ""},
    /* 111..: the PC sound buffers */
    {0x4BF970, "ddd"}, {0x4BFA20, "dd"}, {0x4BFAA0, "d"}, {0x4BFAE0, "d"}, {0x4BFBD0, "d"}, {0x4BFF20, "dd"},
    {0, ""}, {0, ""},
    {0x4C06A0, "d"}, {0x4C06C0, "dd"}, {0x4C08A0, "dddd"},
    /* 122..: PC layer under the helpers (misc.c) */
    {0x4BE3C0, "dd"}, {0x4BE430, "dddd"}, {0x4BE6E0, ""}, {0x4BEAC0, "d"}, {0x4BFEE0, "d"}, {0x4C0050, "d"}, {0x4BF3A0, "sdd"},
    {0, ""},                   /* 129: steps called without an argument (movie, message tables) */
    {0x4BFA60, "dd"}, {0x4BFFA0, "d"},           /* 130: Snd_SetPan, Strm_Stop */
    {0x4BEF40, "d"},                             /* 132: Input_ReadPad */
    {0x4C06E0, "dp", {8}},                       /* 133: Gpu_SetDrawArea */
};
#define STUB_GAME 71
#define STUB_LOAD 50
#define STUB_FILE 36
#define NSTUBS (int)(sizeof g_stubs / sizeof g_stubs[0])
#define STUB_TYPE 34
static Stub *stub_at(u32 addr)
{
    for (u32 k = 0; k < sizeof g_stubs / sizeof g_stubs[0]; k++) if (g_stubs[k].addr == addr) return &g_stubs[k];
    return &g_stubs[0];
}
#define STUB_AOT  35

static u32 rec_common(int id, const u32 *a)
{
    Stub *st = &g_stubs[id];
    u32 n = U32(LOG_BASE);
    U32(LOG_BASE) = n + 1;
    if (n < 64) {
        gptr r = LOG_BASE + 4 + n * LOG_REC, d = r + 0x20;
        U32(r) = st->addr ? st->addr : 0xF0000000u + id;
        int np = 0;
        for (int i = 0; st->spec[i]; i++) {
            u32 v = a[i];
            switch (st->spec[i]) {
            case 'w': v &= 0xFFFF; break;
            case 'b': v &= 0xFF; break;
            case 's': {
                u32 k = 0;
                for (; k < 0x3F && U8(v + k); k++) U8(d + k) = U8(v + k);
                for (; k < 0x40; k++) U8(d + k) = 0;
                d += 0x40; v = 0;
                break; }
            case 'p': if (v >= 0x401000) {
                          memcpy(MEM(d), MEM(v), st->plen[np]); v = 0;
                          if (st->plen[np] == 0x60) U16(d + 0x32) = 0;    /* object: matrix pad is garbage */
                      }
                      d += st->plen[np++]; break;
            }
            U32(r + 4 + 4 * i) = v;
        }
    }
    return st->ret;
}
#define REC(n) static u32 __cdecl rec##n(u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5, u32 a6) \
    { u32 a[7] = {a0, a1, a2, a3, a4, a5, a6}; return rec_common(n, a); }
REC(0) REC(1) REC(2) REC(3) REC(4) REC(5) REC(6) REC(7) REC(8) REC(9) REC(10) REC(11)
REC(12) REC(13) REC(14) REC(15) REC(16) REC(17) REC(18) REC(19) REC(20) REC(21) REC(22)
REC(23) REC(24) REC(25) REC(26) REC(27) REC(28) REC(29) REC(30) REC(31) REC(32) REC(33)
REC(34) REC(35) REC(36) REC(37) REC(38) REC(39) REC(40) REC(41) REC(42) REC(43) REC(44) REC(45)
REC(46) REC(47) REC(48) REC(49) REC(50) REC(51) REC(52) REC(53) REC(54) REC(55) REC(56) REC(57)
REC(58) REC(59) REC(60) REC(61) REC(62) REC(63) REC(64) REC(65) REC(66) REC(67) REC(68) REC(69)
REC(70) REC(71) REC(72) REC(73) REC(74) REC(75) REC(76) REC(77) REC(78) REC(79) REC(80) REC(81)
REC(82) REC(83) REC(84) REC(85) REC(86) REC(87) REC(88) REC(89) REC(90) REC(91) REC(92) REC(93)
REC(94) REC(95) REC(96) REC(97) REC(98) REC(99) REC(100) REC(101)
REC(102) REC(103) REC(104) REC(105) REC(106) REC(107) REC(108) REC(109) REC(110)
REC(111) REC(112) REC(113) REC(114) REC(115) REC(116) REC(117) REC(118) REC(119) REC(120) REC(121)
REC(122) REC(123) REC(124) REC(125) REC(126) REC(127) REC(128) REC(129) REC(130) REC(131) REC(132) REC(133)
static void *const g_recs[] = {rec0, rec1, rec2, rec3, rec4, rec5, rec6, rec7, rec8, rec9, rec10,
    rec11, rec12, rec13, rec14, rec15, rec16, rec17, rec18, rec19, rec20, rec21, rec22, rec23,
    rec24, rec25, rec26, rec27, rec28, rec29, rec30, rec31, rec32, rec33, rec34, rec35, rec36, rec37,
    rec38, rec39, rec40, rec41, rec42, rec43, rec44, rec45, rec46, rec47, rec48, rec49, rec50, rec51,
    rec52, rec53, rec54, rec55, rec56, rec57, rec58, rec59, rec60, rec61, rec62, rec63,
    rec64, rec65, rec66, rec67, rec68, rec69, rec70, rec71, rec72, rec73, rec74, rec75, rec76, rec77,
    rec78, rec79, rec80, rec81, rec82, rec83, rec84, rec85, rec86, rec87, rec88, rec89, rec90, rec91, rec92, rec93,
    rec94, rec95, rec96, rec97, rec98, rec99, rec100, rec101, rec102, rec103, rec104, rec105,
    rec106, rec107, rec108, rec109, rec110, rec111, rec112, rec113, rec114, rec115, rec116, rec117, rec118, rec119, rec120, rec121,
    rec122, rec123, rec124, rec125, rec126, rec127, rec128, rec129, rec130, rec131, rec132, rec133};
_Static_assert(sizeof g_recs / sizeof g_recs[0] >= sizeof g_stubs / sizeof g_stubs[0], "a recorder per stub");
/* Task_Sleep: the file loader "runs" while the task sleeps */
static u32 __cdecl ts_sleep(u32 n)
{ u32 v[5] = {n, U16(0x87FFE8)}; rec_common(STUB_LOAD, v); U16(0x87FFEA) = 0; return 0; }   /* + file asked for */
#define TYPE_TAB 0x6DD000u      /* fake *0x87DCA0: 256 entries */
static u32 g_aot_saved[12], g_wep_saved[24], g_tab_saved[20];

static u32 __cdecl fk_sprintf(u32 buf, u32 fmt, u32 name, u32 b, u32 c);
static void stubs_install(int on)
{
    g_stubs[STUB_LOAD].impl = ts_sleep;
    if (!g_stubs[STUB_FILE + 7].impl) g_stubs[STUB_FILE + 7].impl = fk_sprintf;   /* sprintf does its work */
    for (int i = 0; i < NSTUBS; i++) {
        Stub *st = &g_stubs[i];
        if (!st->addr) continue;
        u8 *c = MEM(st->addr);
        if (on) {
            memcpy(st->saved, c, 5);
            c[0] = 0xE9; *(u32 *)(c + 1) = (u32)(uintptr_t)(st->impl ? st->impl : g_recs[i]) - (st->addr + 5);
        } else memcpy(c, st->saved, 5);
    }
    for (int k = 0; k < 12; k++) {
        if (on) { g_aot_saved[k] = U32(0x738B68 + 4 * k); U32(0x738B68 + 4 * k) = (u32)(uintptr_t)g_recs[STUB_AOT]; }
        else U32(0x738B68 + 4 * k) = g_aot_saved[k];
    }
    for (int k = 0; k < 24; k++) {              /* weapon set-ups 0x737FA0 (16), 0x737FF0 (8) */
        gptr a = k < 16 ? 0x737FA0 + 4 * k : 0x737FF0 + 4 * (k - 16);
        if (on) { g_wep_saved[k] = U32(a); U32(a) = (u32)(uintptr_t)g_recs[STUB_TYPE]; }
        else U32(a) = g_wep_saved[k];
    }
    for (int k = 0; k < 20; k++) {              /* movie steps 0x73762C (3), message slots 0x723F70 (7), box 0x723F8C (8), lights 0x737A50 (2) */
        gptr a = k < 3 ? 0x73762C + 4 * k : k < 10 ? 0x723F70 + 4 * (k - 3) : k < 18 ? 0x723F8C + 4 * (k - 10) : 0x737A50 + 4 * (k - 18);
        if (on) { g_tab_saved[k] = U32(a); U32(a) = (u32)(uintptr_t)g_recs[k < 3 || (k >= 10 && k < 18) ? 129 : STUB_TYPE]; }
        else U32(a) = g_tab_saved[k];
    }
}

static void ops2_args(u8 op, gptr t)
{
    gptr sp = U32(t + T_SP), g = G();
#define A(i) U32(sp + 4 * (i))
    switch (op) {
    case 0x1A: A(9) = rnd() % 16; A(8) = rnd() % 40; A(0) = rnd() % 4 ? rnd() % 64 : 0;
               A(2) = rnd() % 4 ? rnd() % 8 : 0x500000 + rnd() % 0x100000;
               A(6) = rnd_s16(); if (rnd() % 4 == 0) A(6) = (u32)(s32)(s16)(rnd() % 3 - 1);
               Flag_Set(4, A(0), rnd() & 1); Flag_Set(2, 0x14, 0); if (rnd() % 4 == 0) A(8) = 15 + rnd() % 2;
               break;
    case 0x1B: U8(t + T_SEL0) = rnd() % 5; if (rnd() % 4 == 0) U32(O_OBJ + SEL(t) * 0x260 + 0x64) = 0; A(0) = rnd() % 0x120; break;
    case 0x20: U8(0x87FC45) = rnd() % 3 ? (u8)rnd() : 0xFF; break;
    case 0x23: A(1) = rnd() % 2 ? 0x6DC000 + rnd() % 0x100 : rnd() % 64;
               U16(g + 0x680) = rnd() % 16; U16(g + 0x682) = rnd() % 12; U16(0x87FFEA) = rnd() % 2; break;
    case 0x24: U8(g + 0xB4A) = rnd() % 3 ? (u8)rnd() : 0xFF; break;
    case 0x57: draw_safe(); U16(g + 0x664) = rnd() % 5; break;   /* GameOver_Start's prims, the frame it draws */
    case 0x4C: A(0) = 0x6DC000 + rnd() % 0x80;               /* Flash_Play: flash data (n, loops, fades) */
               U8(A(0)) = 4 + rnd() % 8; break;
    case 0x28: A(2) = rnd() % 12; A(1) = rnd() % 64; A(0) = rnd() % 2; break;
    case 0x29: { s32 v[4] = {-1, -2, -3, (s32)(rnd() % 16 | (rnd() % 8) << 8)}; A(0) = (u32)v[rnd() % 4];
               U8(g + 0x108F) = rnd() % 4; U16(g + 0x10B4) = (u16)(rnd() % 16 | (rnd() % 8) << 8);   /* weapon, sub-weapon */
               if (A(0) == 0xFFFFFFFF) U8(g + 0x108F) = rnd() % 2;   /* else stage 0xFE: outside the CORE table */
               for (int i = 0; i < 4; i++) U16(g + 0x10B6 + 2 * i) = (u16)(rnd() % 16 | (rnd() % 8) << 8);
               anim_setup_at(O_OBJ);                 /* Stage_Load restarts the player's animation */
               U32(O_OBJ + 0x50) = rnd(); U8(O_OBJ + 0x58) = (u8)rnd();
               U16(g + 0x67A) = rnd() % 2 ? 0x16 + rnd() % 4 : 0; U16(g + 0x674) = (u16)rnd(); U16(g + 0x676) = (u16)rnd();
               U16(0x87FFEA) = rnd() % 2; Flag_Set(9, 0x33, rnd() & 1); break; }
    case 0x27: A(1) = 1 + rnd() % 60; break;           /* frames (never 0 in the scripts) */
    case 0x2B: A(3) = rnd() % 16; break;
    case 0x2C: A(4) = rnd() % 3; Flag_Set(2, 6, rnd() % 4 == 0); break;
    case 0x2D: A(0) = rnd() % 4 ? (rnd() % 10) << 8 | rnd() % 32 : 0xFFFF; break;
    case 0x2E: A(2) = rnd() % 0x120; break;
    case 0x2F: A(0) = rnd() % 2 ? 0xFFFFFFFF : rnd(); U32(0x87DE04) = rnd() % 28;
               for (int i = 0; i < 28; i++) { gptr e = O_EFF + i * 0x70; U8(e + 0x4E) = rnd() % 4;
                   U8(e + 0x4F) = rnd() % 3 ? rnd() % 20 : (u8)rnd(); } break;
    case 0x30: A(18) = rnd() % 32; break;
    case 0x31: case 0x32: A(12) = rnd() % 32; break;
    case 0x33: case 0x39: A(13) = rnd() % 32; break;
    case 0x35: A(16) = rnd() % 32; A(5) = rnd() % 128; if (rnd() % 4 == 0) A(5) = 0;
               Flag_Set(5, A(5), rnd() & 1); A(1) = rnd() % 3 ? rnd() % 16 : 0xFF;
               if (rnd() % 4 == 0) for (int i = 6; i < 15; i++) A(i) = rnd();
               break;
    case 0x36: case 0x3B: A(11) = rnd() % 32; break;
    case 0x37: case 0x38: A(10) = rnd() % 32; break;
    case 0x3A: A(14) = rnd() % 32; A(2) = rnd() % 64; Flag_Set(8, A(2), rnd() & 1);
               A(1) = rnd() % 16; A(3) = rnd() % 3; break;
    case 0x3E: A(0) = rnd() % 32; U8(O_AOT + A(0) * 0x28 + 1) = rnd() % 12; break;
    case 0x40: A(4) = rnd() % 6; break;
    case 0x41: case 0x42: if (rnd() % 2) A(0) = rnd() % 2 ? 0 : 0xFFFFFFFF;
               Flag_Set(2, 0, rnd() % 3 == 0); Flag_Set(2, 2, rnd() % 3 == 0); break;
    case 0x43: A(0) = (rnd() % 8) << 8 | (u8)rnd(); A(1) = rnd(); break;
    case 0x46: case 0x21: {     /* Anim_Start on a valid animated object */
        gptr o = op == 0x21 ? O_OBJ + (A(4) = rnd() % 16) * 0x260 : O_OBJ + SEL(t) * 0x260;
        anim_setup_at(o);
        A(3) = rnd() % 2; A(1) = rnd() % 10; A(2) = rnd() & 0x27F; A(0) = rnd() % 3 ? 0 : rnd() % 4;
        if (A(2) & 0x80) U16(o + 0x90) |= 2;
        break; }
    case 0x47: U8(t + T_SEL0) = 1 + rnd() % 2; A(2) = rnd() % 3; A(1) = rnd() % 16; A(0) = rnd() % 2; break;
    case 0x48: g_stubs[33].ret = rnd(); break;
    case 0x4B: U8(g + 0x7C4) = rnd() % 4; U8(t + T_A0) = rnd() % 4; break;
    case 0x4E: U16(0x87FFEA) = rnd() % 3 == 0; break;
    case 0x4F: A(0) = rnd() % 4 ? 0x6DC000 + 0x10 * (rnd() % 4) : 0; U32(g + 0xA68) = rnd() % 4 ? 0 : A(0);
               U8(g + 0x1091) = rnd() % 10; U8(g + 0x1090) = rnd() % 32; break;
    case 0x50: case 0x51: Flag_Set(2, 0x17, rnd() & 1); A(0) = rnd() % 10; stub_at(0x4BF3A0)->ret = rnd() % 4 == 0; break;
    case 0x52: U8(t + T_SEL0) = rnd() % 4;
        for (u32 k = 0; k < 28; k++) U32(U32(G() + 0x93C) + k * 0x70 + 0x54) = rnd() % 3 ? 0 : O_OBJ + 0x260 * (rnd() % 16);   /* Eff_Detach: parents */
        break;
    case 0x54: U32(g + 0xA7C) = rnd() % 16; A(0) = rnd() % 3; break;
    case 0x58: { gptr part = U32(O_OBJ + SEL(t) * 0x260 + 0x64);
               A(1) = U32(part + 0x54) && rnd() % 2; A(0) = rnd() % 3; break; }
    case 0x59: Flag_Set(9, 0x33, rnd() & 1); A(0) = rnd() % 2; U16(0x87FFEA) = rnd() % 2; break;
    case 0x4D: A(0) = rnd() % 16; U16(g + 0x680) = rnd() % 2 ? 0xFFFF : rnd() % 16; U16(0x87FFEA) = rnd() % 2; break;
    case 0x49: case 0x55: break;
    }
#undef A
}

/* a frame that may be drawn (Frame_Draw from the code under test): a primitive buffer,
   no glyphs (their quads carry stack bytes), effects without parents, the drawing flags off */
static void draw_quiet(void)
{
    for (gptr q = 0x74D980; q < 0x74DB80; q += 0x10) U8(q) &= ~1;
    for (u32 q = 0; q < 0x8C; q++) U8(0x74D750 + 4 * q + 3) = 0;
    static const u32 fl[] = {1, 2, 3, 4, 5, 8};
    for (unsigned k = 0; k < sizeof fl / sizeof fl[0]; k++) Flag_Set(3, fl[k], 0);
    Flag_Set(3, 9, 0);                          /* no event timer: its glyph quads carry stack bytes too (Timer_Draw has its own test) */
    if (!S16(G() + 0x92A)) S16(G() + 0x92A) = 1;   /* the boss gauge divides by the boss's maximum */
}
static void draw_safe(void)
{
    U32(0x87DC98) = 0x6D0000; U32(0x87F94C) = 0x6CF800;
    gptr pl = U32(G() + 0x940);
    if (pl >= 0x690000 && pl < 0x693000 && !S16(pl + 0x10A)) S16(pl + 0x10A) = 1;   /* the gauge divides by the maximum health */
    for (u32 k = 0; k < 36; k++) U32(0x694000 + k * 0x70 + 0x54) = 0;
    draw_quiet();
}

static void test_ops2(void)
{
    static const struct { u8 op; u32 addr; void *fn; const char *name; } ops[] = {
#define O(op, a, f) {op, a, f, #f}
        O(0x11, 0x487650, ScrOp_Message),    O(0x1A, 0x487A10, ScrOp_ObjSpawn),
        O(0x1B, 0x48AC60, ScrOp_ObjCall),    O(0x20, 0x488F70, ScrOp_WaitEvent),
        O(0x21, 0x488FA0, ScrOp_AnimSet),    O(0x22, 0x489000, ScrOp_Set87FC47),
        O(0x23, 0x489020, ScrOp_LoadModel),  O(0x24, 0x489070, ScrOp_WaitB4A),
        O(0x25, 0x4890B0, ScrOp_Call46FBC0), O(0x26, 0x4890E0, ScrOp_Timer),
        O(0x27, 0x489110, ScrOp_Fade),       O(0x28, 0x489140, ScrOp_WaitFlag),
        O(0x29, 0x489180, ScrOp_Character),  O(0x2B, 0x489410, ScrOp_ObjMove),
        O(0x2C, 0x489460, ScrOp_Menu),       O(0x2D, 0x4894C0, ScrOp_RoomReset),
        O(0x2E, 0x48AD30, ScrOp_Call48C640), O(0x2F, 0x489540, ScrOp_Effect2F),
        O(0x30, 0x4895E0, ScrOp_AotDoor),    O(0x31, 0x489710, ScrOp_Aot31),
        O(0x32, 0x4897E0, ScrOp_Aot32),      O(0x33, 0x4898B0, ScrOp_Aot33),
        O(0x35, 0x4899A0, ScrOp_AotItem),    O(0x36, 0x489B80, ScrOp_Aot36),
        O(0x37, 0x489C50, ScrOp_Aot37),      O(0x38, 0x489D10, ScrOp_Aot38),
        O(0x39, 0x489DD0, ScrOp_Aot39),      O(0x3A, 0x489EB0, ScrOp_Aot3A),
        O(0x3B, 0x48A000, ScrOp_Aot3B),      O(0x3E, 0x48A0D0, ScrOp_AotRun),
        O(0x40, 0x48A120, ScrOp_CamMove),    O(0x41, 0x48AD60, ScrOp_WaitCutscene),
        O(0x42, 0x48ADC0, ScrOp_Cutscene),   O(0x43, 0x48AE50, ScrOp_Sound),
        O(0x46, 0x48A530, ScrOp_ObjAnim),    O(0x47, 0x48A580, ScrOp_LinkPart),
        O(0x48, 0x48A640, ScrOp_Random),     O(0x49, 0x48AF80, ScrOp_SoundSlot),
        O(0x4B, 0x48A720, ScrOp_WaitCamera), O(0x4C, 0x48A770, ScrOp_Call478B80),
        O(0x4D, 0x48A7A0, ScrOp_Call481D20), O(0x4E, 0x48A7D0, ScrOp_WaitFile),
        O(0x4F, 0x48A7F0, ScrOp_EnemySet),   O(0x50, 0x48A8C0, ScrOp_Op50),
        O(0x51, 0x48A930, ScrOp_Op51),       O(0x52, 0x48A990, ScrOp_Op52),
        O(0x53, 0x48AD00, ScrOp_Call48C600), O(0x54, 0x48AA20, ScrOp_ObjsHold),
        O(0x55, 0x48AE10, ScrOp_Call48CE20), O(0x56, 0x48AAC0, ScrOp_Op56),
        O(0x57, 0x48AAE0, ScrOp_Call4529C0), O(0x58, 0x48AB10, ScrOp_PartHide),
        O(0x59, 0x48AB90, ScrOp_Texture),
#undef O
    };
    g_ign_objpads = 1;
    script_setup();
    stubs_install(1);
    U32(0x87DCA0) = TYPE_TAB;
    for (int i = 0; i < 256; i++) U32(TYPE_TAB + 4 * i) = (u32)(uintptr_t)g_recs[STUB_TYPE];
    U32(0x87F940) = 16;
    for (int i = 0; i < iters; i++) {
        objects_setup(); world_setup();
        for (u32 a = 0x6DC000; a < 0x6DC100; a += 4) U32(a) = rnd();
        for (unsigned k = 0; k < sizeof ops / sizeof ops[0]; k++) {
            gptr t = T_BASE + SCR_THREAD_SIZE * (rnd() % 10);
            objects_links();
            random_thread(t);
            U32(t + T_SP) = t + 0x18;
            for (int j = 0; j < 26; j++) U32(t + 0x18 + 4 * j) = rnd() % 3 ? rnd() % 64 : rnd();
            U8(t + T_SEL1) = rnd() % 16;
            U8(t + T_SEL0) = rnd() % 4;
            U32(LOG_BASE) = 0;
            ops2_args(ops[k].op, t);
            gptr sp = U32(t + T_SP);
            u32 a[1] = {t};
            sprintf(g_what, "op %02X sel %u/%d sp %08X %08X %08X %08X %08X", ops[k].op, U8(t + T_SEL0),
                    S8(t + T_SEL1), U32(sp), U32(sp + 4), U32(sp + 8), U32(sp + 12), U32(sp + 16));
            check(ops[k].addr, ops[k].fn, ops[k].name, 1, a, 0xFFFFFFFF);
        }
        /* helpers */
        u32 h[2] = {rnd() % 16, 0};
        check(0x48D820, Obj_Init, "Obj_Init", 1, h, 0);
        objects_links();
        h[0] = U32(O_OBJ + (rnd() % 16) * 0x260 + 0x64); h[1] = U32(h[0] + 0x54) ? rnd() % 3 : rnd() % 2;
        check(0x48D3F0, Obj_Part, "Obj_Part", 2, h, 0xFFFFFFFF);
        gptr t = T_BASE + SCR_THREAD_SIZE * (rnd() % 10);
        random_thread(t); U8(t + T_SEL0) = rnd() % 6; U8(t + T_SEL1) = (u8)(rnd() % 40 - 20);
        h[0] = t;
        check(0x48BE50, Script_SelObj, "Script_SelObj", 1, h, 0xFFFFFFFF);
        h[0] = rnd() % 4 ? rnd() % 12 : rnd(); h[1] = rnd();
        check(0x48CC30, Room_SoundSlot, "Room_SoundSlot", 2, h, 0xFFFFFFFF);
        U8(G() + 0x1091) = rnd() % 4 ? rnd() % 12 : (u8)rnd(); U8(G() + 0x1090) = (u8)rnd(); U32(LOG_BASE) = 0;
        check(0x48CA10, Room_SoundInit, "Room_SoundInit", 0, h, 0);
    }
    stubs_install(0);
    g_ign_objpads = 0;
}

/* ---- GTE (PC layer) ----------------------------------------------------------- */
#define G_SCR 0x6C0000u         /* 12 scratch blocks of 0x100 bytes */

static float rnd_coord(void) { return (float)((s32)(rnd() % 1281) - 320) + (float)(rnd() % 256) / 256.0f; }

static void gte_random_state(void)
{
    for (u32 a = 0x87B370; a < 0x87B710; a += 2) U16(a) = (u16)(rnd() % 4 ? (u32)((s32)(rnd() % 8193) - 4096) : rnd());
    for (int i = 0; i < 6; i++) F32(GTE_SXY + 4 * i) = rnd_coord();
    F32(GTE_ZSCALE) = (float)(1 + rnd() % 65536);
    S32(GTE_H) = 100 + rnd() % 900;
    S32(GTE_ZNEAR) = S32(GTE_H) / 2;
    for (u32 r = GTE_MAC1; r < GTE_MAC1 + 12; r += 4) S32(r) = (s32)(rnd() % 40001) - 20000;
    switch (rnd() % 4) {                       /* z: far, near, behind */
    case 0: S32(GTE_MAC1 + 8) = S32(GTE_ZNEAR) + rnd() % 30000; break;
    case 1: S32(GTE_MAC1 + 8) = 1 + rnd() % S32(GTE_ZNEAR); break;
    case 2: S32(GTE_MAC1 + 8) = -(s32)(rnd() % 1000); break;
    }
    S32(GTE_DQNEAR) = rnd() % 4000; S32(GTE_DQFAR) = S32(GTE_DQNEAR) + 1 + rnd() % 60000;
    S32(GTE_OFX) = rnd() % 640; S32(GTE_OFY) = rnd() % 480;
    for (int i = 0; i < 3; i++) S32(GTE_TR + 4 * i) = (s32)(rnd() % 20001) - 10000;
    for (u32 a = G_SCR; a < G_SCR + 0xC00; a += 2) U16(a) = (u16)(rnd() % 4 ? (u32)((s32)(rnd() % 8193) - 4096) : rnd());
}

static u32 gte_arg(char k, int i, int same)
{
    switch (k) {
    case 'P': return same ? G_SCR : G_SCR + 0x100 * (u32)i;
    case 'A': return rnd() % 4 ? (u32)((s32)(rnd() % 8192) - 4096) : rnd();
    case 'I': return rnd() % 4 ? rnd() % 512 : rnd();
    case 'Q': return rnd() % 4 ? rnd() % 0x1000000 : rnd() & 0x7FFFFFFF;
    case 'H': return 1 + rnd() % 2000;
    case 'S': return rnd() % 4097;
    case 'Z': switch (rnd() % 6) {             /* depth cue range edges */
              case 0: return S32(GTE_DQNEAR) + rnd() % 3 - 1;
              case 1: return S32(GTE_DQFAR) + rnd() % 3 - 1;
              case 2: return rnd();
              default: return rnd() % 70000; }
    case 'F': { float f = rnd() % 8 ? (float)((s32)(rnd() % 20001) - 10000) / (1 + rnd() % 64) : 0.0f; u32 u; memcpy(&u, &f, 4); return u; }
    }
    return 0;
}

static void test_gte(void)
{
    static const struct { u32 addr; void *fn; const char *name; int n; const char *kinds; int ign_arg; u32 ign_off, ign_len; u32 rmask; } F[] = {
        {0x4C08E0, (void *)rsin, "rsin", 1, "A", -1, 0x0, 0, 0xFFFFFFFF},
        {0x4C0920, (void *)rcos, "rcos", 1, "A", -1, 0x0, 0, 0xFFFFFFFF},
        {0x4C0940, (void *)ratan2, "ratan2", 2, "FF", -1, 0x0, 0, 0xFFFFFFFF},
        {0x4C0960, (void *)SquareRoot0, "SquareRoot0", 1, "Q", -1, 0x0, 0, 0xFFFFFFFF},
        {0x4C0970, (void *)SquareRoot12, "SquareRoot12", 1, "Q", -1, 0x0, 0, 0xFFFFFFFF},
        {0x4C0990, (void *)InitGeom, "InitGeom", 0, "", -1, 0x0, 0, 0},
        {0x4C09E0, (void *)SetGeomOffset, "SetGeomOffset", 2, "II", -1, 0x0, 0, 0},
        {0x4C0A00, (void *)ReadGeomScreen, "ReadGeomScreen", 0, "", -1, 0x0, 0, 0xFFFFFFFF},
        {0x4C0A10, (void *)SetGeomScreen, "SetGeomScreen", 1, "H", -1, 0x0, 0, 0},
        {0x4C0A70, (void *)SetBackColor, "SetBackColor", 3, "III", -1, 0x0, 0, 0},
        {0x4C0AA0, (void *)ApplyMatrix, "ApplyMatrix", 3, "PPP", -1, 0x0, 0, 0xFFFFFFFF},
        {0x4C0B20, (void *)ApplyMatrixSV, "ApplyMatrixSV", 3, "PPP", -1, 0x0, 0, 0xFFFFFFFF},
        {0x4C0BA0, (void *)ApplyMatrixLV, "ApplyMatrixLV", 3, "PPP", -1, 0x0, 0, 0xFFFFFFFF},
        {0x4C0C20, (void *)ApplyRotMatrixLV, "ApplyRotMatrixLV", 2, "PP", -1, 0x0, 0, 0xFFFFFFFF},
        {0x4C0CB0, (void *)MulMatrix0, "MulMatrix0", 3, "PPP", 2, 0x12, 2, 0xFFFFFFFF},
        {0x4C0E50, (void *)RotMatrixX, "RotMatrixX", 2, "AP", 1, 0x12, 2, 0xFFFFFFFF},
        {0x4C0EC0, (void *)RotMatrixY, "RotMatrixY", 2, "AP", 1, 0x12, 2, 0xFFFFFFFF},
        {0x4C0F30, (void *)RotMatrixZ, "RotMatrixZ", 2, "AP", 1, 0x12, 2, 0xFFFFFFFF},
        {0x4C0FA0, (void *)RotMatrix, "RotMatrix", 2, "PP", 1, 0x12, 2, 0xFFFFFFFF},
        {0x4C0FF0, (void *)RotMatrixYXZ, "RotMatrixYXZ", 2, "PP", 1, 0x12, 2, 0xFFFFFFFF},
        {0x4C1040, (void *)RotMatrixZXY, "RotMatrixZXY", 2, "PP", 1, 0x12, 2, 0xFFFFFFFF},
        {0x4C1090, (void *)RotMatrixZYX, "RotMatrixZYX", 2, "PP", 1, 0x12, 2, 0xFFFFFFFF},
        {0x4C10E0, (void *)ScaleMatrix, "ScaleMatrix", 2, "PP", -1, 0x0, 0, 0xFFFFFFFF},
        {0x4C1170, (void *)TransposeMatrix, "TransposeMatrix", 2, "PP", -1, 0x0, 0, 0xFFFFFFFF},
        {0x4C11C0, (void *)OuterProduct12, "OuterProduct12", 3, "PPP", 2, 0xC, 4, 0},
        {0x4C1220, (void *)RotTransPers, "RotTransPers", 4, "PPPP", -1, 0x0, 0, 0xFFFFFFFF},
        {0x4C1310, (void *)Gte_DepthCue, "Gte_DepthCue", 1, "Z", -1, 0x0, 0, 0xFFFFFFFF},
        {0x4C1350, (void *)Gte_Project, "Gte_Project", 3, "PPP", -1, 0x0, 0, 0},
        {0x4C1470, (void *)RotTransPers3, "RotTransPers3", 8, "PPPPPPPP", -1, 0x0, 0, 0xFFFFFFFF},
        {0x4C15D0, (void *)RotTransPers4, "RotTransPers4", 10, "PPPPPPPPPP", -1, 0x0, 0, 0xFFFFFFFF},
        {0x4C1790, (void *)VectorNormalS, "VectorNormalS", 2, "PP", -1, 0x0, 0, 0},
        {0x4C18E0, (void *)Gte_NormalColor, "Gte_NormalColor", 3, "PPP", -1, 0x0, 0, 0},
        {0x4C19B0, (void *)Gte_ClampIR, "Gte_ClampIR", 2, "PP", -1, 0x0, 0, 0},
        {0x4C1A30, (void *)SetLightMatrix, "SetLightMatrix", 1, "P", -1, 0x0, 0, 0},
        {0x4C1A50, (void *)SetColorMatrix, "SetColorMatrix", 1, "P", -1, 0x0, 0, 0},
        {0x4C1A70, (void *)LoadAverageShort0, "LoadAverageShort0", 5, "PPSSP", -1, 0x0, 0, 0},
        {0x4C1AD0, (void *)LoadAverageCol, "LoadAverageCol", 5, "PPSSP", -1, 0x0, 0, 0},
        {0x4C1B40, (void *)SetRotMatrix, "SetRotMatrix", 1, "P", -1, 0x0, 0, 0},
        {0x4C1B60, (void *)SetTransMatrix, "SetTransMatrix", 1, "P", -1, 0x0, 0, 0},
        {0x4C1B90, (void *)SetDepthScale, "SetDepthScale", 1, "F", -1, 0x0, 0, 0},
        {0x4C1BA0, (void *)gte_ldclmv, "gte_ldclmv", 1, "P", -1, 0x0, 0, 0},
        {0x4C1BD0, (void *)gte_ldrgb, "gte_ldrgb", 1, "P", -1, 0x0, 0, 0},
        {0x4C1BE0, (void *)gte_ldsxy3, "gte_ldsxy3", 3, "PPP", -1, 0x0, 0, 0},
        {0x4C1C20, (void *)gte_ldv0, "gte_ldv0", 1, "P", -1, 0x0, 0, 0},
        {0x4C1C40, (void *)gte_ldv3, "gte_ldv3", 3, "PPP", -1, 0x0, 0, 0},
        {0x4C1C80, (void *)gte_ldv3c, "gte_ldv3c", 1, "P", -1, 0x0, 0, 0},
        {0x4C1CC0, (void *)gte_nccs, "gte_nccs", 0, "", -1, 0x0, 0, 0},
        {0x4C1CE0, (void *)gte_ncct, "gte_ncct", 0, "", -1, 0x0, 0, 0},
        {0x4C1D20, (void *)gte_nclip, "gte_nclip", 0, "", -1, 0x0, 0, 0},
        {0x4C1D70, (void *)gte_rtv0, "gte_rtv0", 0, "", -1, 0x0, 0, 0},
        {0x4C1DB0, (void *)gte_rtv1, "gte_rtv1", 0, "", -1, 0x0, 0, 0},
        {0x4C1DF0, (void *)gte_rtv2, "gte_rtv2", 0, "", -1, 0x0, 0, 0},
        {0x4C1E30, (void *)gte_rtir, "gte_rtir", 0, "", -1, 0x0, 0, 0},
        {0x4C1E70, (void *)gte_rtps, "gte_rtps", 0, "", -1, 0x0, 0, 0},
        {0x4C1F40, (void *)gte_rtpt, "gte_rtpt", 0, "", -1, 0x0, 0, 0},
        {0x4C2090, (void *)gte_stclmv, "gte_stclmv", 1, "P", -1, 0x0, 0, 0},
        {0x4C20C0, (void *)gte_stlvnl, "gte_stlvnl", 1, "P", -1, 0x0, 0, 0},
        {0x4C20E0, (void *)gte_stopz, "gte_stopz", 1, "P", -1, 0x0, 0, 0},
        {0x4C20F0, (void *)gte_stflg, "gte_stflg", 1, "P", -1, 0x0, 0, 0},
        {0x4C2100, (void *)gte_strgb, "gte_strgb", 1, "P", -1, 0x0, 0, 0},
        {0x4C2110, (void *)gte_strgb3, "gte_strgb3", 1, "P", -1, 0x0, 0, 0},
        {0x4C2130, (void *)gte_strgb3_gt3, "gte_strgb3_gt3", 1, "P", -1, 0x0, 0, 0},
        {0x4C2150, (void *)gte_strgb3_gt3, "gte_strgb3_gt3", 1, "P", -1, 0x0, 0, 0},
        {0x4C2170, (void *)gte_stsv, "gte_stsv", 1, "P", -1, 0x0, 0, 0},
        {0x4C2190, (void *)gte_stsxy, "gte_stsxy", 1, "P", -1, 0x0, 0, 0},
        {0x4C21B0, (void *)gte_stsxy3c, "gte_stsxy3c", 1, "P", -1, 0x0, 0, 0},
        {0x4C21F0, (void *)gte_stsxy3_g3, "gte_stsxy3_g3", 1, "P", -1, 0x0, 0, 0},
        {0x4C2230, (void *)gte_stsxy3_gt3, "gte_stsxy3_gt3", 1, "P", -1, 0x0, 0, 0},
        {0x4C2270, (void *)gte_stsxy3_gt3, "gte_stsxy3_gt3", 1, "P", -1, 0x0, 0, 0},
        {0x4C22B0, (void *)gte_stsz, "gte_stsz", 1, "P", -1, 0x0, 0, 0},
        {0x4C22C0, (void *)gte_stsz3, "gte_stsz3", 1, "P", -1, 0x0, 0, 0},
        {0x4C22E0, (void *)gte_stsz4, "gte_stsz4", 1, "P", -1, 0x0, 0, 0},
        {0x4C2310, (void *)PC_stsz4_g4, "PC_stsz4_g4", 1, "P", -1, 0x0, 0, 0},
        {0x4C2360, (void *)PC_stsz_g4_flat, "PC_stsz_g4_flat", 1, "P", -1, 0x0, 0, 0},
        {0x4C2390, (void *)PC_stsz3_g3, "PC_stsz3_g3", 1, "P", -1, 0x0, 0, 0},
        {0x4C23D0, (void *)PC_stsz4_g4, "PC_stsz4_g4", 1, "P", -1, 0x0, 0, 0},
        {0x4C2420, (void *)PC_stsz3_gt3, "PC_stsz3_gt3", 1, "P", -1, 0x0, 0, 0},
        {0x4C2460, (void *)PC_stsz4_gt4, "PC_stsz4_gt4", 1, "P", -1, 0x0, 0, 0},
        {0x4C24B0, (void *)gte_stszotz, "gte_stszotz", 1, "P", -1, 0x0, 0, 0},
    };
    for (int it = 0; it < iters / 4; it++)
        for (unsigned k = 0; k < sizeof F / sizeof F[0]; k++) {
            gte_random_state();
            u32 a[10];
            int same = rnd() % 8 == 0;            /* in-place calls (out == in) */
            for (int i = 0; i < F[k].n; i++) a[i] = gte_arg(F[k].kinds[i], i, same);
            if (F[k].addr == 0x4C1350) { a[0] = GTE_SXY + 16; a[1] = GTE_SXY + 20; a[2] = GTE_SZ3; }
            g_ign_addr = F[k].ign_arg >= 0 ? a[F[k].ign_arg] + F[k].ign_off : 0;
            g_ign_len = F[k].ign_arg >= 0 ? F[k].ign_len : 0;
            sprintf(g_what, "args %08X %08X %08X", a[0], F[k].n > 1 ? a[1] : 0, F[k].n > 2 ? a[2] : 0);
            check(F[k].addr, F[k].fn, F[k].name, F[k].n, a, F[k].rmask);
            g_ign_len = 0;
        }
    /* users of the GTE */
    for (int it = 0; it < iters; it++) {
        gte_random_state();
        objects_setup();
        u32 o[1] = {O_OBJ + (rnd() % 16) * 0x260};
        for (gptr p = U32(o[0] + 0x64); p; p = U32(p + 0x54)) S16(p + 0x48) = rnd() % 4096;
        g_ign_len = 0;
        g_ign_objpads = 1;
        check(0x48FDE0, Obj_UpdateMatrix, "Obj_UpdateMatrix", 1, o, 0);
        g_ign_objpads = 0;
        u32 c[4] = {G_SCR, G_SCR + 0x100, G_SCR + 0x200, G_SCR + 0x300};
        check(0x403840, Camera_LookAt, "Camera_LookAt", 4, c, 0);
    }
}

/* random programs made only of reimplemented opcodes, run by Script_Run */
static void test_script_run(void)
{
    script_setup();
    for (int i = 0; i < iters / 4; i++) {
        /* 16 routines of 0x80 bytes: instructions on known boundaries,
           forward branches only, few pushes and calls (stack stays in the thread) */
        for (int r = 0; r < 16; r++) {
            gptr p = S_BASE + 0x40 + 0x80 * r, end = p + 0x70;
            gptr at[42]; int n = 0, br[40], nb = 0, pushes = 0, depth[42];
            while (p + 12 < end && n < 40) {
                u32 k = rnd() % 100;
                at[n] = p; depth[n] = pushes;
                if (k < 20 && pushes < 6) { U8(p) = 5; U8(p + 1) = rnd() % 3 ? 0 : 6; S16(p + 2) = rnd() % 16; p += 4; pushes++; }
                else if (k < 30 && pushes > 0) { U8(p) = 6; U8(p + 1) = 1; S16(p + 2) = rnd() % 4; p += 4; pushes--; }
                else if (k < 50) { U8(p) = 3; U8(p + 1) = rnd() % 22; U8(p + 2) = rnd() % 4; U8(p + 3) = 0; S16(p + 4) = 1 + rnd() % 5; p += 6; }
                else if (k < 62) { U8(p) = 1; U8(p + 1) = rnd() % 4; U8(p + 2) = rnd() % 2 ? 1 : 2; S16(p + 4) = rnd() % 4; if (U8(p + 2) == 2 && pushes == 0) U8(p + 2) = 1; p += 6; }
                else if (k < 74) { U8(p) = 7; U8(p + 1) = rnd() % 7; br[nb++] = n; p += 4; }
                else if (k < 78 && r < 4) { U8(p) = 8; U8(p + 1) = 0; S16(p + 2) = r + 1 + rnd() % 3; p += 4; }
                else if (k < 86) { U8(p) = 0x10; U8(p + 1) = 0; p += 2; }
                else { U8(p) = 0; U8(p + 1) = 0; p += 2; }
                n++;
            }
            /* balance the stack, then return */
            at[n] = p; depth[n] = pushes;
            U8(p) = 3; U8(p + 1) = 0; U8(p + 2) = 0xFF; U8(p + 3) = 0; S16(p + 4) = pushes; p += 6;
            while (p < end + 0x10) { U8(p) = 4; U8(p + 1) = 0; p += 2; }
            for (int j = 0; j < nb; j++) {           /* forward, to an instruction at the same stack depth */
                int i0 = br[j], cand[42], nc = 0;
                for (int q = i0 + 1; q <= n; q++) if (depth[q] == depth[i0]) cand[nc++] = q;
                int tgt = nc ? cand[rnd() % nc] : i0 + 1;
                if (!nc) { U8(at[i0] + 1) = 0x7; }       /* never taken: bit 7 is never set */
                S16(at[i0] + 2) = (s16)(at[tgt] - at[i0]);
            }
        }
        memset(MEM(T_BASE), 0, SCR_NTHREADS * SCR_THREAD_SIZE);
        for (int th = 0; th < SCR_NTHREADS; th++)
            if (rnd() & 1) {
                u32 a[2] = {(u32)th, rnd() % 16};
                check(0x48BDB0, Script_StartThread, "Script_StartThread", 2, a, 0);
                /* keep waits short and the stack inside the thread */
            }
        for (int f = 0; f < 6; f++) {
            for (int th = 0; th < SCR_NTHREADS; th++) {
                gptr t = T_BASE + th * SCR_THREAD_SIZE;
                if (U8(t + T_STATE) == 2 && U16(t + T_WAIT) > 3) U16(t + T_WAIT) = 2;
                if (U32(t + T_SP) < t + 0x20 || U32(t + T_SP) > t + 0x80) U8(t + T_STATE) = 0;
            }
            sprintf(g_what, "program %d frame %d", i, f);
            if (!check(0x48BC50, Script_Run, "Script_Run", 0, NULL, 0)) break;
        }
        u32 a[1] = {rnd() % 10};
        check(0x48BE20, Script_KillThread, "Script_KillThread", 1, a, 0);
    }
}

/* LZSS: every type 5/6 record of the game's .DAT files */
static void test_anim(void)
{
    script_setup();
    stubs_install(1);
    for (int i = 0; i < iters; i++) {
        gptr o = AN_OBJ;
        u32 a[10] = {0};
        anim_setup();
        U32(LOG_BASE) = 0;
        a[0] = o; a[1] = rnd() % 2 ? 1 + rnd() % 3 : 1 + rnd() % 8;
        sprintf(g_what, "parts %u frames %u fl %04X blend %u", U32(o + 0x68), U16(o + 0x90), U16(o + 0x102), U16(o + 0x9A));
        check(0x48E3D0, Anim_ApplyFrame, "Anim_ApplyFrame", 2, a, 0xFFFFFFFF);
        anim_setup(); U32(LOG_BASE) = 0;
        check(0x48E100, Anim_Step, "Anim_Step", 1, a, 0xFFFFFFFF);
        anim_setup(); U32(LOG_BASE) = 0;
        a[1] = rnd() % 2 ? rnd() % 2 : AN_DATA + 0x1000 * (rnd() % 2);
        a[2] = rnd() % 10; a[3] = (rnd() & 0x2FF) | (rnd() % 2 ? 0x80000000u : 0); a[4] = rnd() % 4;
        if (a[3] & 0x80) { if (U16(o + 0x90) < 2) U16(o + 0x90) = 2; U16(AN_DATA) |= 2; U16(AN_DATA + 0x1000) |= 2; }
        sprintf(g_what, "anim %X frame %u fl %08X blend %u", a[1], a[2], a[3], a[4]);
        check(0x48DEF0, Anim_Start, "Anim_Start", 5, a, 0);
        anim_setup(); U32(LOG_BASE) = 0;
        if (a[3] & 0x80) { if (U16(o + 0x90) < 2) U16(o + 0x90) = 2; U16(AN_DATA) |= 2; U16(AN_DATA + 0x1000) |= 2; }
        check(0x48DDE0, Anim_Set, "Anim_Set", 5, a, 0);
        anim_setup(); U32(LOG_BASE) = 0;
        a[1] = 1 + rnd() % 4;
        check(0x48DE80, Anim_Events, "Anim_Events", 2, a, 0);
        /* rotations */
        gptr va = G_SCR, vb = G_SCR + 8, vo = G_SCR + 16, m = G_SCR + 0x40;
        for (int k = 0; k < 4; k++) { U16(va + 2 * k) = (u16)rnd(); U16(vb + 2 * k) = (u16)rnd(); U16(vo + 2 * k) = (u16)rnd(); }
        if (rnd() % 3 == 0) for (int k = 0; k < 3; k++) U16(vb + 2 * k) = (U16(va + 2 * k) + rnd() % 64) & 0xFFF;
        u32 w = rnd() % 3 ? rnd() % 0x1001 : 0x1000 / (1 + rnd() % 3);
        u32 b[4] = {va, vb, rnd() % 4 ? vo : va, w};
        sprintf(g_what, "a %04X %04X %04X b %04X %04X %04X w %X", U16(va), U16(va + 2), U16(va + 4), U16(vb), U16(vb + 2), U16(vb + 4), w);
        check(0x48FEA0, Anim_BlendLinear, "Anim_BlendLinear", 4, b, 0);
        check(0x48FF90, Anim_BlendMatrix, "Anim_BlendMatrix", 4, b, 0);
        if (rnd() % 2) RotMatrixYXZ(va, m);
        else for (int k = 0; k < 9; k++) S16(m + 2 * k) = (s16)(rnd() % 8193) - 4096;
        u32 c[2] = {m, vo};
        check(0x48F1D0, Matrix_ToAngles, "Matrix_ToAngles", 2, c, 0xFFFFFFFF);
        /* player layers: object 0 is the animated test object */
        anim_setup(); U32(LOG_BASE) = 0;
        U32(G() + 0x940) = o;
        for (u32 l = 0; l < 2; l++) {
            gptr sh = 0x7598D8 + l * 0x260;
            for (u32 k = 0x78; k < 0x110; k += 4) U32(sh + k) = U32(o + k);
            U32(sh + 0x64) = rnd() % 4 ? AN_PART : 0;
            U32(sh + 0x8C) = AN_DATA + 0x14; U8(sh + 0xAC) = U8(o + 0xAC);
            U32(sh + 0x7C) = U32(o + 0x7C) ; U32(sh + 0x80) = U32(o + 0x80); U32(sh + 0x84) = U32(o + 0x84);
            U32(sh + 0x78) = U32(o + 0x78);
            for (u32 k = 0; k < 0x78; k += 2) U16(0x759D98 + l * 0x78 + k) = (u16)(rnd() % 0x1000);
        }
        u32 L[10] = {rnd() % 2};
        check(0x48E9C0, Anim_SaveLayer, "Anim_SaveLayer", 1, L, 0);
        check(0x48EBC0, Anim_LoadLayer, "Anim_LoadLayer", 1, L, 0);
        U16(o + 0x92) = rnd() % 64; if (rnd() % 3) U32(o + 0xA4) = AN_EVT + 4 * (rnd() % 4); else U32(o + 0xA4) = 0;
        a[0] = o;
        check(0x48E970, Anim_FrameFlag, "Anim_FrameFlag", 1, a, 0xFFFFFFFF);
        check(0x48E930, Player_AnimReset, "Player_AnimReset", 0, NULL, 0);
        L[0] = rnd() % 3 ? rnd() % 0x1001 : 0x1000u >> (rnd() % 4);
        sprintf(g_what, "layers parts %u fl %04X w %X", U32(o + 0x68), U16(o + 0x102), L[0]);
        check(0x48EF50, Anim_StepLayers, "Anim_StepLayers", 1, L, 0xFFFFFFFF);
        anim_setup(); U32(LOG_BASE) = 0; U32(G() + 0x940) = o;
        L[0] = rnd() % 2; L[1] = AN_DATA + 0x1000 * (rnd() % 2); L[2] = rnd() % 6;
        L[3] = rnd() & 0x37F; L[4] = rnd() % 3; L[5] = rnd() % 0x1001;
        if (U16(o + 0x90) < 2) U16(o + 0x90) = 2;
        U16(AN_DATA) |= 2; U16(AN_DATA + 0x1000) |= 2;
        sprintf(g_what, "start layers %X %X frame %u fl %X blend %u w %X", L[0], L[1], L[2], L[3], L[4], L[5]);
        check(0x48EDC0, Anim_StartLayers, "Anim_StartLayers", 6, L, 0);
    }
    stubs_install(0);
}

/* ---- generic object handler and object helpers ----------------------------- */
static void obj_head_setup(gptr o, u32 n)
{
    U32(G() + 0x940) = o;
    for (u32 k = 0x140; k < 0x170; k += 2) U16(o + k) = (u16)(rnd() % 4 ? rnd() % 0x1000 : rnd());
    U8(o + 0x15D) = 0;
    U8(o + 0x15E) = rnd() % 3 ? (u8)(rnd() % n) : 0xFF;
    U8(o + 0x15F) = (u8)(rnd() % n);
    U8(o + 0x158) = (u8)(rnd() % 5);
    U8(o + 0x161) = (u8)(rnd() % 4);
    if (rnd() % 4 == 0) U16(o + 0x16A) = 0;
    if (rnd() % 4 == 0) U16(o + 0x16E) = 0;
    U8(G() + 0x108F) = (u8)(rnd() % 6); U8(0x87E747) = (u8)(rnd() % 3);
    for (u32 k = 0; k < 4; k++) U32(o + 0x120 + 4 * k) = AN_PART + 0x80 * (rnd() % n);
    U8(o + 0x11B) = rnd() % 3 ? (u8)((rnd() & 0xCF) | (rnd() % 2) << 4 | 1) : 0;   /* feet 0/1 (+0x128 is the wall cylinder) */
}

static void test_object(void)
{
    script_setup();
    stubs_install(1);
    g_ign_objpads = 1;
    gptr o = AN_OBJ;
    for (int i = 0; i < iters; i++) {
        u32 a[10] = {0};
        /* pure helpers */
        a[0] = rnd() % 4 ? rnd() % 0x1000 : rnd(); a[1] = rnd() % 3 ? rnd() % 0x800 : rnd(); a[2] = rnd() % 4 ? rnd() % 0x1000 : rnd();
        a[3] = rnd() % 3 ? rnd() % 0x800 : rnd();
        if (rnd() % 8 == 0) a[2] = a[0] + (rnd() % 3) - 1;
        if (rnd() % 6 == 0) a[2] = a[0] - a[1] + (rnd() % 3) - 1;     /* exactly one step away */
        if (rnd() % 6 == 0) a[3] = rnd() % 2 ? (a[2] & 0x7FF) : (-a[2] & 0x7FF); /* |target| == lim */
        sprintf(g_what, "%X %X %X %X", a[0], a[1], a[2], a[3]);
        check(0x47A5E0, Angle_Approach, "Angle_Approach", 3, a, 0xFFFFFFFF);
        check(0x47A6F0, Angle_ApproachLim, "Angle_ApproachLim", 4, a, 0xFFFF);
        a[0] = rnd() % 3 ? rnd() % 0x10000 : rnd() >> (rnd() % 32); if (rnd() % 16 == 0) a[0] = 0;
        check(0x48F590, Sqrt_Refined, "Sqrt_Refined", 1, a, 0xFFFFFFFF);
        a[0] = rnd() % 2 ? 0 : rnd() % 256; U8(0x74B16C) = rnd() % 2 ? 0 : (u8)rnd();
        check(0x448730, Latch_74B16C, "Latch_74B16C", 1, a, 0xFF);
        for (u32 k = 0; k < 16; k += 2) U16(G_SCR + k) = (u16)(rnd() % 3 ? (u32)((s32)(rnd() % 8001) - 4000) : rnd());
        if (rnd() % 8 == 0) { U16(G_SCR + 8) = U16(G_SCR); U16(G_SCR + 12) = U16(G_SCR + 4); }
        a[0] = G_SCR; a[1] = G_SCR + 8; a[2] = G_SCR + 0x40;
        sprintf(g_what, "a %d %d %d b %d %d %d", S16(G_SCR), S16(G_SCR + 2), S16(G_SCR + 4), S16(G_SCR + 8), S16(G_SCR + 10), S16(G_SCR + 12));
        check(0x487300, Dist2_XZ, "Dist2_XZ", 2, a, 0xFFFFFFFF);
        check(0x48F5C0, Camera_Distance, "Camera_Distance", 2, a, 0xFFFFFFFF);
        check(0x48F620, Angles_LookAt, "Angles_LookAt", 3, a, 0);
        check(0x48F6C0, Angle_XZ, "Angle_XZ", 2, a, 0xFFFFFFFF);
        check(0x48F710, Angle_Seg, "Angle_Seg", 1, a, 0xFFFFFFFF);
        /* object groups */
        U32(G() + 0x940) = O_OBJ; U32(0x87F940) = rnd() % 17;
        for (u32 k = 0; k < 16; k++) { U32(O_OBJ + k * 0x260 + 0x50) = rnd(); U8(O_OBJ + k * 0x260 + 0xFD) = rnd() % 2; }
        a[0] = O_OBJ + 0x260 * (rnd() % 16);
        check(0x46B8F0, Obj_PickGroup, "Obj_PickGroup", 1, a, 0);
        /* object with parts */
        anim_setup(); U32(LOG_BASE) = 0;
        u32 n = U32(o + 0x68);
        obj_head_setup(o, n);
        a[0] = o; a[1] = rnd() % n; a[2] = rnd() % n; a[3] = (u16)rnd();
        sprintf(g_what, "parts %u head %u tgt %X mode %u", n, U8(o + 0x15F), U8(o + 0x15E), U8(o + 0x158));
        check(0x47A880, Head_Reset, "Head_Reset", 1, a, 0);
        check(0x47A970, Head_Relax, "Head_Relax", 1, a, 0);
        check(0x47AA60, Head_LookAtObj, "Head_LookAtObj", 1, a, 0);
        check(0x47AB40, Head_LookAtPoint, "Head_LookAtPoint", 1, a, 0);
        check(0x47ABA0, Head_Turn, "Head_Turn", 1, a, 0);
        check(0x47A930, Head_Update, "Head_Update", 1, a, 0);
        check(0x48F880, Obj_Set118, "Obj_Set118", 4, a, 0);
        check(0x48FCA0, Obj_SetFeet, "Obj_SetFeet", 3, a, 0);
        obj_head_setup(o, n);
        sprintf(g_what, "parts %u foot %02X", n, U8(o + 0x11B));
        check(0x48FD20, Obj_AnchorFoot, "Obj_AnchorFoot", 1, a, 0xFFFFFFFF);
        a[1] = G_SCR;
        check(0x48F3A0, Obj_MoveLocal, "Obj_MoveLocal", 2, a, 0);
        /* the handler and its modes */
        u32 mode = rnd() % 7;
        do { anim_setup(); } while (mode == 0 && U32(o + 0x68) != 16);
        U32(LOG_BASE) = 0;
        n = U32(o + 0x68);
        obj_head_setup(o, n);
        U8(o + 0x54) = (u8)mode;
        world_setup(); U32(o + 0x60) = MODEL_DATA; U32(MODEL_DATA + 0x14) = 15 + rnd() % 2;   /* mode 0 builds the model */
        U32(G() + 0x930) = (u32)(uintptr_t)g_recs[STUB_TYPE];
        /* collision: no other objects, an empty wall block */
        U32(o + 0x12C) = rnd() % 2 ? 0x6D0000 : 0; U32(0x87F940) = 0;
        U32(0x6D0000) = 0; U32(0x6D0004) = (u32)rnd() << 16; U32(0x6D0008) = 0x00FF0100;
        U32(o + 0x128) = rnd() % 4 ? 0x6D0000 : 0; U8(0x6D4000) = 0; U32(G() + 0xA4C) = 0x6D4000;
        sprintf(g_what, "mode %u parts %u fl %02X head %u foot %02X", mode, n, U8(o + 0x50), U8(o + 0x158), U8(o + 0x11B));
        check(0x490120, Generic_Handler, "Generic_Handler", 1, a, 0);
        do { anim_setup(); } while (U32(o + 0x68) != 16);
        world_setup(); U32(o + 0x60) = MODEL_DATA; U32(MODEL_DATA + 0x14) = 15 + rnd() % 2;   /* feet: parts 11, 14 */
        U32(LOG_BASE) = 0;
        check(0x4901B0, Generic_Init, "Generic_Init", 1, a, 0);
        anim_setup(); U32(LOG_BASE) = 0; U32(G() + 0x930) = (u32)(uintptr_t)g_recs[STUB_TYPE];
        check(0x490270, Generic_StepCb, "Generic_StepCb", 1, a, 0);
        anim_setup(); U32(LOG_BASE) = 0;
        check(0x4A56D0, Generic_Step, "Generic_Step", 1, a, 0);
        /* animated props (mode 2 removes the object first: not reached with a live model) */
        mode = rnd() % 2;
        anim_setup(); U32(LOG_BASE) = 0;
        n = U32(o + 0x68);
        obj_head_setup(o, n);
        U8(o + 0x54) = (u8)mode; U8(o + 0x55) = rnd() % 3;
        U16(o + 0x1C4) = rnd() % 3 ? (u16)(rnd() % 3) : (u16)rnd();
        U32(o + 0x50) ^= rnd() % 4 ? 0 : 0x40000000;
        world_setup(); U32(o + 0x60) = MODEL_DATA; U32(MODEL_DATA + 0x14) = 1 + rnd() % 16;
        U32(LOG_BASE) = 0;
        sprintf(g_what, "mode %u/%u parts %u fl %08X timer %u", mode, U8(o + 0x55), n, U32(o + 0x50), U16(o + 0x1C4));
        check(0x446170, Prop_Handler, "Prop_Handler", 1, a, 0);
        anim_setup(); world_setup(); U32(o + 0x60) = MODEL_DATA; U32(MODEL_DATA + 0x14) = 1 + rnd() % 16;
        U32(LOG_BASE) = 0;
        check(0x4461F0, Prop_Init, "Prop_Init", 1, a, 0);
        anim_setup(); U8(o + 0x55) = rnd() % 3; U16(o + 0x1C4) = (u16)(rnd() % 3); U32(LOG_BASE) = 0;
        check(0x4462B0, Prop_Run, "Prop_Run", 1, a, 0);
        check(0x446250, Prop_Play, "Prop_Play", 1, a, 0);
        U16(o + 0x1C4) = (u16)(rnd() % 3);
        check(0x446270, Prop_PlayTimed, "Prop_PlayTimed", 1, a, 0);
    }
    g_ign_objpads = 0;
    stubs_install(0);
}

/* ---- collision: objects against objects and room walls ---------------------- */
#define CL_CYL  0x6D0000u       /* cylinder lists, 0x40 per object */
#define CL_ROOM 0x6D4000u       /* wall polygon block */

static s16 cl_coord(s32 c, s32 r) { return (s16)(c + (s32)(rnd() % (2 * r + 1)) - r); }

static void cl_cyl(gptr c, u32 nparts, int last)
{
    U16(c) = cl_coord(0, 150); U16(c + 2) = cl_coord(0, 300); U16(c + 4) = cl_coord(0, 150);
    U16(c + 6) = 50 + rnd() % 500; U16(c + 8) = 50 + rnd() % 900;
    U8(c + 0xA) = rnd() % 2 ? 0xFF : (u8)(rnd() % nparts);
    U8(c + 0xB) = (u8)((last ? 0 : 1) | (rnd() % 4 == 0 ? 2 : 0) | (rnd() & 0xFC & -(rnd() % 4 == 0)));
}

static void coll_setup(void)
{
    objects_setup();
    U32(0x87F940) = rnd() % 4 ? 16 : rnd() % 17;
    for (u32 i = 0; i < 16; i++) {
        gptr o = O_OBJ + i * 0x260, p = U32(o + 0x64);
        u32 np = U32(p + 0x54) ? 2 : 1;
        U32(o + 0x50) = (rnd() % 4 ? 1 : 0) | (rnd() & 0x3DFE & -(rnd() % 2));
        S16(o + 0x40) = cl_coord(0, 1500); S16(o + 0x42) = cl_coord(0, 300); S16(o + 0x44) = cl_coord(0, 1500);
        for (gptr q = p; q; q = U32(q + 0x54)) {
            S16(q + 0x40) = cl_coord(S16(o + 0x40), 200); S16(q + 0x42) = cl_coord(S16(o + 0x42), 200);
            S16(q + 0x44) = cl_coord(S16(o + 0x44), 200);
            S16(q + 0x6C) = cl_coord(S16(q + 0x40), 150); S16(q + 0x6E) = cl_coord(S16(q + 0x42), 50);
            S16(q + 0x70) = cl_coord(S16(q + 0x44), 150);
            for (int k = 0; k < 9; k++) S16(q + 0x20 + 2 * k) = (s16)(rnd() % 8193) - 4096;
        }
        u32 n = 1 + rnd() % 3;
        gptr c = CL_CYL + i * 0x40;
        for (u32 k = 0; k < n; k++) cl_cyl(c + k * 0xC, np, k + 1 == n);
        U32(o + 0x12C) = rnd() % 8 ? c : 0;
        cl_cyl(c + 0x30, np, 1);
        U32(o + 0x128) = rnd() % 8 ? c + 0x30 : 0;
        S16(o + 0xE0) = cl_coord(S16(o + 0x40), 250); S16(o + 0xE4) = cl_coord(S16(o + 0x44), 250);
        U16(o + 0x5A) = rnd() % 4 ? rnd() % 4 : (u16)rnd();
        U8(o + 0x184) = rnd() % 6 ? 0 : (u8)rnd();
    }
    /* walls: polygons around the objects' area */
    u32 np = 1 + rnd() % 4;
    U8(CL_ROOM) = (u8)np;
    gptr w = CL_ROOM + 0x40;
    for (u32 k = 0; k < np; k++) {
        U32(CL_ROOM + 8 + 4 * k) = w - CL_ROOM;
        u32 n = 2 + rnd() % 6;
        U8(w + 1) = (u8)n; U8(w + 3) = rnd() % 4 ? 0 : (u8)(rnd() & 0xE0);
        U16(w + 4) = rnd() % 4 ? 0xFFFF : (u16)rnd();
        for (u32 e = 0; e < n; e++) {
            U8(w + 8 + 4 * e) = (u8)((rnd() % 6 ? 1 : 0) | (rnd() % 5 ? 0 : 0x80));
            U8(w + 9 + 4 * e) = rnd() % 5 ? 0 : (u8)(rnd() & 0xE0);
        }
        gptr v = w + 8 + 4 * n;
        s32 cx = cl_coord(0, 1200), cz = cl_coord(0, 1200), r = 300 + rnd() % (rnd() % 2 ? 500 : 1500);
        u32 a0 = rnd() % 0x1000;
        for (u32 e = 0; e < n; e++) {
            u32 a = (a0 + e * 0x1000 / n + rnd() % (0x400 / n)) & 0xFFF;
            S16(v + 4 * e) = (s16)(cx + rcos(a) * r / 4096); S16(v + 4 * e + 2) = (s16)(cz + rsin(a) * r / 4096);
        }
        U32(v + 4 * n) = U32(v);                        /* closed */
        w = v + 4 * n + 4;
    }
    U32(G() + 0xA4C) = CL_ROOM;
}

/* put o's wall cylinder at (or across) an edge or corner of a wall polygon */
static void cl_place(gptr o)
{
    gptr c = U32(o + 0x128);
    if (!c || !U8(CL_ROOM)) return;
    gptr w = CL_ROOM + U32(CL_ROOM + 8 + 4 * (rnd() % U8(CL_ROOM)));
    u32 n = U8(w + 1), e = rnd() % n;
    gptr v = w + 8 + 4 * n + 4 * e;
    U8(w + 3) &= rnd() % 2 ? 0xFF : 0;
    for (u32 k = 0; k < n; k++) if (rnd() % 2) U8(w + 8 + 4 * k) |= 1;
    U16(w + 4) |= (u16)(1u << (U8(o + 0x5A) & 15));
    double x0 = S16(v), z0 = S16(v + 2), x1 = S16(v + 4), z1 = S16(v + 6);
    double len = sqrt((x1 - x0) * (x1 - x0) + (z1 - z0) * (z1 - z0)); if (len < 1) len = 1;
    double nx = (z1 - z0) / len, nz = -(x1 - x0) / len, tx = -nz, tz = nx;
    u32 tk = rnd() % 4;
    double t = tk == 0 ? 0 : tk == 1 ? 1 : tk == 2 ? (rnd() % 2 ? 0.05 : 0.95) : (rnd() % 1001) / 1000.0;
    double r = S16(c + 6);
    double d = (double)(s32)(rnd() % (u32)(2 * r + 201)) - r - 100;
    double px = x0 + (x1 - x0) * t + nx * d + tx * (s32)(rnd() % 61 - 30);
    double pz = z0 + (z1 - z0) * t + nz * d + tz * (s32)(rnd() % 61 - 30);
    double step = (s32)(rnd() % 401) - 200, side = (s32)(rnd() % 301) - 150;
    double qx = px + nx * step + tx * side, qz = pz + nz * step + tz * side;     /* previous position */
    if (U8(c + 0xA) == 0xFF && !(U32(o + 0x50) & 0x100)) {
        S16(o + 0x40) = (s16)px; S16(o + 0x44) = (s16)pz;
        S16(o + 0xE0) = (s16)qx; S16(o + 0xE4) = (s16)qz;
        return;
    }
    gptr part = Obj_Part(U32(o + 0x64), U8(c + 0xA) != 0xFF ? U8(c + 0xA) : 0);
    for (int k = 0; k < 9; k++) S16(part + 0x20 + 2 * k) = k % 4 ? 0 : 0x1000;
    S16(part + 0x40) = (s16)(px - S16(c)); S16(part + 0x44) = (s16)(pz - S16(c + 4));
    S16(part + 0x6C) = (s16)qx; S16(part + 0x70) = (s16)qz;
}

static void rnd_seg(gptr s, int big)
{
    for (int k = 0; k < 4; k++) S16(s + 2 * k) = big ? (s16)rnd() : cl_coord(0, 2000);
    if (rnd() % 6 == 0) { S16(s + 4) = S16(s); }
    if (rnd() % 6 == 0) { S16(s + 6) = S16(s + 2); }
}

static void test_collision(void)
{
    script_setup();
    stubs_install(1);
    for (int i = 0; i < iters; i++) {
        u32 a[10] = {0};
        /* geometry helpers */
        int big = rnd() % 8 == 0;
        gptr s0 = G_SCR, s1 = G_SCR + 8, out = G_SCR + 0x10, p = G_SCR + 0x20, v = G_SCR + 0x30;
        rnd_seg(s0, big); rnd_seg(s1, big);
        if (rnd() % 4 == 0) { S16(s1) = S16(s0 + 4); S16(s1 + 2) = S16(s0 + 6); }    /* shared end */
        for (int k = 0; k < 8; k++) U16(out + 2 * k) = (u16)rnd();
        a[0] = s0; a[1] = s1; a[2] = out;
        sprintf(g_what, "seg %d %d %d %d / %d %d %d %d", S16(s0), S16(s0 + 2), S16(s0 + 4), S16(s0 + 6),
                S16(s1), S16(s1 + 2), S16(s1 + 4), S16(s1 + 6));
        check(0x453290, Seg_Intersect, "Seg_Intersect", 2, a, 0xFFFFFFFF);
        check(0x4540B0, Seg_Cross, "Seg_Cross", 3, a, 0xFFFFFFFF);
        a[2] = s1 + 4;
        check(0x453EE0, Seg_Side, "Seg_Side", 3, a, 0xFFFFFFFF);
        S16(p) = cl_coord(S16(s0 + 4 * (rnd() % 2)), 600); S16(p + 2) = (s16)rnd(); S16(p + 4) = cl_coord(S16(s0 + 2), 600);
        S16(p + 6) = (s16)(rnd() % 8 ? 50 + rnd() % 700 : rnd());
        if (rnd() % 3 == 0) { S16(p) = (S16(s0) + S16(s0 + 4)) / 2 + cl_coord(0, 300); S16(p + 4) = (S16(s0 + 2) + S16(s0 + 6)) / 2 + cl_coord(0, 300); }
        a[0] = s0; a[1] = p;
        sprintf(g_what, "seg %d %d %d %d p %d %d r %d", S16(s0), S16(s0 + 2), S16(s0 + 4), S16(s0 + 6), S16(p), S16(p + 4), S16(p + 6));
        check(0x453F50, Circle_Seg, "Circle_Seg", 2, a, 0xFFFFFFFF);
        if (!big && !(S16(s0 + 6) - S16(s0 + 2) + 1 == 0 && S16(s0) - S16(s0 + 4) + 1 == 0)) {
            a[2] = v; a[3] = out;
            check(0x4541F0, Wall_Push, "Wall_Push", 4, a, 0xFFFFFFFF);
        }
        for (int k = 0; k < 4; k++) S16(p + 2 * k) = cl_coord(0, 800);
        for (int k = 0; k < 4; k++) S16(v + 2 * k) = cl_coord(0, 800);
        S16(p + 6) = rnd() % 400; S16(v + 6) = rnd() % 400;
        if (rnd() % 4 == 0) { S16(v) = S16(p); S16(v + 4) = S16(p + 4); }
        a[0] = p; a[1] = v; a[2] = out; a[3] = rnd() % 4 ? (rnd() % 600) << 16 | rnd() % 600 : rnd();
        sprintf(g_what, "cyl %d %d %d r %d / %d %d %d r %d h %08X", S16(p), S16(p + 2), S16(p + 4), S16(p + 6),
                S16(v), S16(v + 2), S16(v + 4), S16(v + 6), a[3]);
        check(0x452EE0, Coll_Cylinders, "Coll_Cylinders", 4, a, 0xFFFFFFFF);
        /* objects */
        coll_setup(); U32(LOG_BASE) = 0;
        a[0] = O_OBJ + 0x260 * (rnd() % 16);
        sprintf(g_what, "obj %u fl %08X cyl %X n %u", (a[0] - O_OBJ) / 0x260, U32(a[0] + 0x50), U32(a[0] + 0x12C), U32(0x87F940));
        check(0x452BD0, Obj_CollideObjs, "Obj_CollideObjs", 1, a, 0xFFFFFFFF);
        for (int rep = 0; rep < 8; rep++) {
        coll_setup(); U32(LOG_BASE) = 0;
        a[0] = O_OBJ + 0x260 * (rnd() % 16);
        if (rnd() % 2) U32(G() + 0x940) = a[0];
        if (rnd() % 8) cl_place(a[0]);
        sprintf(g_what, "obj %u fl %08X cyl %X polys %u 184 %02X", (a[0] - O_OBJ) / 0x260, U32(a[0] + 0x50), U32(a[0] + 0x128),
                U8(CL_ROOM), U8(a[0] + 0x184));
        check(0x453430, Obj_CollideWalls, "Obj_CollideWalls", 1, a, 0xFFFFFFFF);
        }
        /* floors (Floor_Height, Floor_Cross, Poly_RayCount) */
        coll_setup(); U32(LOG_BASE) = 0;
        for (u32 k = 0; k < U8(CL_ROOM); k++) {
            gptr w = CL_ROOM + U32(CL_ROOM + 8 + 4 * k);
            U8(w) = rnd() % 5 ? (u8)((rnd() % 3 == 0 ? 0x80 : rnd() % 3 == 0 ? 0x40 : 0) | (rnd() & 0x30) | 1) : 0;
            U16(w + 4) = rnd() % 4 ? (u16)(1u << (rnd() % 3)) | (u16)(rnd() % 2 ? 1u << (rnd() % 3) : 0)
                       : rnd() % 2 ? (u16)(rnd() % 2 ? 0x8000 : 0) : (u16)rnd();
            U8(w + 6) = (u8)rnd();
            U8(w + 3) = (u8)(2 * k + 2);                    /* tells which polygon was taken */
            gptr w0 = CL_ROOM + U32(CL_ROOM + 8);
            u32 n = U8(w + 1);
            if (k && U8(w0 + 1) == n && rnd() % 4)          /* same place as polygon 0 */
                memcpy(MEM(w + 8 + 4 * n), MEM(w0 + 8 + 4 * n), 4 * (n + 1));
        }
        gptr pa = G_SCR, pb = G_SCR + 8;
        gptr w0 = CL_ROOM + U32(CL_ROOM + 8);
        gptr v0 = w0 + 8 + 4 * U8(w0 + 1);
        S16(pb) = cl_coord((S16(v0) + S16(v0 + 4)) / 2, 600); S16(pb + 4) = cl_coord((S16(v0 + 2) + S16(v0 + 6)) / 2, 600);
        S16(pb + 2) = (s16)(-(s32)(rnd() % 6000) + 500);
        S16(pa) = rnd() % 2 ? S16(pb) : cl_coord(S16(pb), 300); S16(pa + 4) = rnd() % 2 ? S16(pb + 4) : cl_coord(S16(pb + 4), 300);
        S16(pa + 2) = rnd() % 3 ? (rnd() % 2 ? 10000 : -10000) : S16(pb + 2) + (s16)(rnd() % 3 - 1) * (s16)(rnd() % 2000);
        if (rnd() % 8 == 0) S16(pa + 2) = S16(pb + 2);
        if (rnd() % 3 == 0) { S16(pb) = S16(v0 + 4 * (rnd() % U8(w0 + 1))); S16(pa) = S16(pb); }   /* on a vertex */
        U32(G_SCR + 0x20) = rnd();
        a[0] = pa; a[1] = pb; a[2] = rnd() % 3 ? 0x80 : (u32)rnd(); a[3] = rnd() % 2 ? G_SCR + 0x20 : 0;
        sprintf(g_what, "a %d %d %d b %d %d %d polys %u", S16(pa), S16(pa + 2), S16(pa + 4), S16(pb), S16(pb + 2), S16(pb + 4), U8(CL_ROOM));
        check(0x454420, Floor_Height, "Floor_Height", 4, a, 0xFFFF);
        {   /* where a -> b meets the floor: into a third point, or over a itself */
            u32 c[4] = {pa, pb, rnd() % 3 ? G_SCR + 0x40 : pa, a[2]};
            if (rnd() % 4 == 0) S16(pa + 2) = (s16)(S16(pb + 2) + (s32)(rnd() % 200) - 100);   /* the same band of 100, or next to it */
            if (rnd() % 3 == 0) S16(pa + 2) = (s16)(-1000 * (s32)(rnd() % 4) + 100 * (s32)(rnd() % 16) + (s32)(rnd() % 3) - 1);   /* at a floor's height (and next to it) */
            if (rnd() % 3 == 0) {                   /* looking up from b to exactly polygon 0's underside */
                u16 ly = U16(w0 + 4); s32 lo = 0; while (!(ly & (1u << lo)) && ++lo < 15) ;
                S16(pa) = S16(pb); S16(pa + 4) = S16(pb + 4); S16(pa + 2) = (s16)(-1000 * lo + (s32)(rnd() % 3) - 1);
                if (S16(pb + 2) <= S16(pa + 2) + 100) S16(pb + 2) = (s16)(S16(pa + 2) + 100 + rnd() % 2000);
            }
            for (u32 q = 0; q < 6; q += 2) U16(G_SCR + 0x40 + q) = (u16)rnd();
            u32 keep[2] = {U32(pa), U32(pa + 4)};
            check(0x4542F0, Floor_Cross, "Floor_Cross", 4, c, 0xFFFF);   /* (the "no" carries Floor_Height's upper bits: stack bytes for slopes; the callers use 16) */
            U32(pa) = keep[0]; U32(pa + 4) = keep[1];
        }
        U16(pa) = U16(pb); U16(pa + 4) = U16(pa); U16(pa + 2) = U16(pb + 4); U16(pa + 6) = 0x7CFF;
        a[0] = w0; a[1] = pa;
        check(0x454870, Poly_RayCount, "Poly_RayCount", 2, a, 0xFFFF);
    }
    stubs_install(0);
}

/* ---- the player ---------------------------------------------------------------- */
#define A_PLAYER_EXT_H 0x87FD60u
#define PL_K2   0x6D6000u       /* effect slots, 0xAC each */
#define PL_EXT_SRC 0x6D5F00u    /* +0x8C source blocks of effect slots */

static void k2_setup(void)
{
    u32 n = 1 + rnd() % 40;
    U32(0x87DC88) = n; U32(G() + 0x944) = PL_K2;
    U32(G() + 0xA70) = rnd() % 4 ? rnd() % n : n;
    for (u32 i = 0; i < n; i++) {
        gptr p = PL_K2 + i * 0xAC;
        for (u32 k = 0; k < 0xAC; k += 4) U32(p + k) = rnd();
        U32(p + 0x50) = rnd() % 2 ? 1 : (rnd() & ~1u);
        U8(p + 0x86) = rnd() % 4 ? (u8)(rnd() % n) : 0xFF;
        U8(p + 0x87) = (u8)(rnd() % n);
        U32(p + 0x8C) = rnd() % 3 ? PL_EXT_SRC + 0x20 * (rnd() % 4) : 0;
    }
    for (u32 k = 0; k < 0x80; k++) U8(PL_EXT_SRC + k) = rnd() % 3 ? 1 : (u8)rnd();
    U8(G() + 0x852) = rnd() % 4 ? (u8)(rnd() % n) : 0xFF;
    U32(0x87F948) = TYPE_TAB;
}

/* objects, walls, and o as the player */
static void zones_setup(void);
static gptr player_setup_obj(void)
{
    coll_setup(); zones_setup();
    gptr o = O_OBJ + 0x260 * (rnd() % 16);
    u32 np = U32(U32(o + 0x64) + 0x54) ? 2 : 1;
    U8(o + 0x15F) = (u8)(rnd() % np); U8(o + 0x15E) = rnd() % 2 ? 0xFF : 0; U8(o + 0x161) = rnd() % 4;
    U8(o + 0x11B) = rnd() % 3 ? 0 : (u8)(0x10 * (rnd() % 2) + 1);
    U32(o + 0x120) = U32(o + 0x64); U32(o + 0x124) = U32(o + 0x64);
    U8(o + 0x54) = rnd() % 4; U8(o + 0x55) = rnd() % 3 ? rnd() % 10 : 9;
    U16(o + 0x17E) = rnd() % 3 ? 0 : rnd() % 2 ? rnd() % 200 : rnd() % 2 ? rnd() % 1000 : 12 * (rnd() % 90) + 2;
    U8(o + 0x1C0 + 0x79) = rnd() % 2 ? (u8)rnd() : rnd() % 2 ? 0x4F : 0xBD;
    U16(o + 0x108) = rnd() % 0x500;
    for (u32 k = 0; k < 0x60; k += 2) U16(o + 0x1C0 + k) = (u16)rnd();
    U16(o + 0x1C0 + 0x3A) = rnd() % 0x110;
    U8(G() + 0x108F) = rnd() % 3 ? rnd() % 3 : rnd() % 10;
    U8(0x87E747) = rnd() % 3;
    for (u32 i = 0; i < 16; i++) U8(O_OBJ + i * 0x260 + 0x17B) = rnd() % 3 ? (u8)(rnd() % 3) : (u8)rnd();
    Flag_Set(0, 0xB, rnd() % 4 == 0); Flag_Set(2, 0xF, rnd() % 4 == 0);
    k2_setup();
    U32(G() + 0x93C) = O_EFF;                           /* no sub-weapon in flight (tested on its own) */
    for (u32 k = 0; k < 12; k++) U8(O_EFF + k * 0x70 + 0x4E) = 0;
    return o;
}

static void test_player(void)
{
    script_setup();
    stubs_install(1);
    g_ign_objpads = 1;
    u32 saved[16];
    for (int k = 0; k < 16; k++) { saved[k] = U32(0x7380FC + 4 * k); U32(0x7380FC + 4 * k) = (u32)(uintptr_t)g_recs[STUB_TYPE]; }
    for (int i = 0; i < iters; i++) {
        u32 a[10] = {0};
        /* effect slots */
        k2_setup(); U32(LOG_BASE) = 0;
        sprintf(g_what, "n %u first %u chain %02X", U32(0x87DC88), U32(G() + 0xA70), U8(G() + 0x852));
        check(0x48DA40, K2_Alloc, "K2_Alloc", 0, a, 0xFFFFFFFF);
        check(0x48D480, K2_Reuse, "K2_Reuse", 0, a, 0xFFFFFFFF);
        check(0x48D580, K2_AllocBack, "K2_AllocBack", 0, a, 0xFFFFFFFF);
        a[0] = O_OBJ + 0x260 * (rnd() % 16); a[1] = rnd();
        check(0x48DA00, Effect_Attach, "Effect_Attach", 2, a, 0xFFFFFFFF);
        a[1] = rnd() % 4 == 0 ? 0 : rnd() % 3 ? rnd() % 20 : (u32)rnd(); a[2] = rnd();
        check(0x48DC80, Effect_Drops, "Effect_Drops", 3, a, 0xFFFFFFFF);
        /* the player */
        gptr o = player_setup_obj(); U32(LOG_BASE) = 0;
        U32(A_PLAYER_EXT_H) = o + 0x1C0;
        a[0] = o;
        sprintf(g_what, "char %u state %02X/%02X breath %X", U8(G() + 0x108F), U8(o + 0x54), U8(o + 0x55), U16(o + 0x1FA));
        check(0x47A7E0, Player_Breath, "Player_Breath", 1, a, 0);
        o = player_setup_obj(); U32(LOG_BASE) = 0;
        U32(0x87DCA0) = TYPE_TAB; U8(o + 0x58) = (u8)rnd();
        U8(G() + 0x927) = rnd() % 2; U16(G() + 0x674) = rnd() % 2 ? 0x211 : (u16)rnd(); U16(0x87FFEA) = rnd() % 2;
        a[0] = o;
        sprintf(g_what, "char %u type %02X", U8(G() + 0x108F), U8(o + 0x58));
        check(0x47FE10, Player_Setup, "Player_Setup", 1, a, 0);
        o = player_setup_obj(); U32(LOG_BASE) = 0;
        a[0] = o;
        sprintf(g_what, "char %u state %02X/%02X fl %08X bleed %u", U8(G() + 0x108F), U8(o + 0x54), U8(o + 0x55), U32(o + 0x50), U16(o + 0x17E));
        check(0x47CE40, Player_Handler, "Player_Handler", 1, a, 0);
    }
    for (int k = 0; k < 16; k++) U32(0x7380FC + 4 * k) = saved[k];
    g_ign_objpads = 0;
    stubs_install(0);
}

/* ---- player states, aiming ------------------------------------------------------ */
static const u32 g_pl_tabs[][2] = {{0x7380FC, 7}, {0x738118, 12}, {0x738148, 12}, {0x738178, 4}, {0x7380A0, 8}, {0x737A5C, 8},
                                    {0x738018, 12}, {0x738048, 12}, {0x738078, 10}};
#define PL_NTABS 9
static u32 g_pl_saved[PL_NTABS][16];
static void player_tables_stub(int on)
{
    for (int t = 0; t < PL_NTABS; t++)
        for (u32 k = 0; k < g_pl_tabs[t][1]; k++) {
            gptr a = g_pl_tabs[t][0] + 4 * k;
            if (on) { g_pl_saved[t][k] = U32(a); if (U32(a) >= 0x401000 && U32(a) < 0x4C0000) U32(a) = (u32)(uintptr_t)g_recs[STUB_TYPE]; }
            else U32(a) = g_pl_saved[t][k];
        }
}

/* targets in the object array: aim descriptors +0x134 */
static void targets_setup(void)
{
    for (u32 i = 0; i < 16; i++) {
        gptr ob = O_OBJ + i * 0x260, c = CL_CYL + i * 0x40 + 0x3C;
        U8(c + 0xA) = rnd() % 2 ? 0 : (U32(U32(ob + 0x64) + 0x54) ? 1 : 0);
        U32(ob + 0x134) = c;
        U32(ob + 0x50) = (U32(ob + 0x50) & ~0x10008u) | (rnd() % 3 ? 8 : 0) | (rnd() % 6 ? 0 : 0x10000);
        U8(ob + 0xFF) = (u8)rnd();
        U8(ob + 0x184) = rnd() % 4 ? 0 : (u8)rnd();
        if (rnd() % 4 == 0 && i) { S16(ob + 0x40) = S16(O_OBJ + 0x260 + 0x40); S16(ob + 0x44) = S16(O_OBJ + 0x260 + 0x44); }
        if (rnd() % 3 == 0) for (gptr q = U32(ob + 0x64); q; q = U32(q + 0x54)) S16(q + 0x42) = cl_coord(0, 4000);
    }
}

/* aim descriptors of the objects (radius +6, height +8, part +0xA, more +0xB) */
static void aim_desc_setup(int always)
{
    { gptr dsc = 0x6DCC00;
      for (u32 k = 0; k < 16; k++) {
          gptr ob = O_OBJ + k * 0x260;
          u32 np = U32(U32(ob + 0x64) + 0x54) ? 2 : 1, nd = 1 + rnd() % 3;
          U32(ob + 0x134) = always || rnd() % 8 ? dsc : 0;
          U8(ob + 0x17B) = rnd() % 4 ? 0 : (u8)rnd();
          for (u32 m = 0; m < nd; m++, dsc += 0xC) {
              for (u32 b = 0; b < 6; b += 2) S16(dsc + b) = cl_coord(0, 200);
              U16(dsc + 6) = rnd() % 400; U16(dsc + 8) = rnd() % 1200;
              U8(dsc + 0xA) = (u8)(rnd() % np); U8(dsc + 0xB) = (u8)((rnd() & 0xFE) | (m + 1 < nd));
          }
      } }
}


static void test_player2(void)
{
    script_setup();
    stubs_install(1);
    player_tables_stub(1);
    g_ign_objpads = 1;
    for (int i = 0; i < iters; i++) {
        u32 a[10] = {0};
        /* models, effect removal, weapon set-ups */
        objects_setup(); world_setup(); U32(LOG_BASE) = 0; U32(0x87F940) = 16;
        gptr mo = O_OBJ + 0x260 * (rnd() % 16);
        a[0] = mo; a[1] = MODEL_DATA;
        sprintf(g_what, "model parts %u", U32(MODEL_DATA + 0x14));
        check(0x48D740, Part_Alloc, "Part_Alloc", 0, a, 0xFFFFFFFF);
        check(0x48D8B0, Model_Setup, "Model_Setup", 2, a, 0);
        a[0] = O_K2 + 0xAC * (rnd() % 20);
        sprintf(g_what, "slot %u chain %02X", (a[0] - O_K2) / 0xAC, U8(G() + 0x852));
        check(0x48D620, K2_Unlink, "K2_Unlink", 1, a, 0);
        check(0x41C9B0, Lights_Off, "Lights_Off", 0, a, 0);
        a[0] = rnd() % 8; U16(G() + 0x10B4) = (u16)rnd();
        check(0x448580, Eff_KillKind, "Eff_KillKind", 1, a, 0);
        U8(G() + 0x1091) = rnd() % 10;
        for (u32 k = 0; k < U32(0x87DC88); k++) {                       /* kinds 1..16, sub-kinds */
            gptr e = U32(G() + 0x944) + k * 0xAC;
            U8(e + 0x58) = rnd() % 4 ? 1 + rnd() % 16 : (u8)rnd();
            U8(e + 0x7A) = rnd() % 4 ? rnd() % 16 : (u8)rnd();
            if (rnd() % 4 == 0) { U8(e + 0x58) = 3; U8(e + 0x50) |= 1; }
        }
        U8(G() + 0xF) = (u8)rnd();
        sprintf(g_what, "slots %u from %u area %u", U32(0x87DC88), U32(G() + 0xA70), U8(G() + 0x1091));
        check(0x448620, Eff_KillRoom, "Eff_KillRoom", 1, a, 0);
        U32(0x87FD60) = mo + 0x1C0; a[0] = mo; U8(G() + 0x108F) = rnd() % 3;
        { static const struct { u32 addr; void *fn; const char *name; } W[] = {
            {0x4B2C20, Weapon_SetNone, "Weapon_SetNone"}, {0x4AC980, Weapon_SetGun, "Weapon_SetGun"},
            {0x4AE2F0, Weapon_SetLight, "Weapon_SetLight"}, {0x4AEF10, Weapon_SetTwin, "Weapon_SetTwin"},
            {0x4B1C90, Weapon_SetHeavy, "Weapon_SetHeavy"}, {0x4B2C30, Sub_SetKnives, "Sub_SetKnives"},
            {0x4B2FC0, Sub_SetKnife, "Sub_SetKnife"}, {0x4B33C0, Sub_SetGrenade, "Sub_SetGrenade"},
            {0x4B3A10, Sub_SetMine, "Sub_SetMine"}, {0x4B3D00, Sub_SetKnives2, "Sub_SetKnives2"}};
          for (unsigned k = 0; k < sizeof W / sizeof W[0]; k++) {
              if (k >= 5 && U32(mo + 0x68) < 13) {
                  for (u32 q = 0; q < 40; q++) U32(PART_POOL + q * 0xCC + 0x50) = 0;   /* room for the model */
                  a[1] = MODEL_DATA; U32(MODEL_DATA + 0x14) = 13 + rnd() % 4; Model_Setup(mo, MODEL_DATA);
              }
              check(W[k].addr, W[k].fn, W[k].name, 1, a, 0);
          } }
        a[1] = rnd() % 8;
        check(0x41C970, Light_Reset, "Light_Reset", 1, a + 1, 0xFFFFFFFF);
        U16(G() + 0x10B4) = (u16)(rnd() % 16 | (rnd() % 8) << 8);
        for (int k = 0; k < 24; k++) U32(k < 16 ? 0x737FA0 + 4 * k : 0x737FF0 + 4 * (k - 16)) = g_wep_saved[k];
        if (U32(mo + 0x68) < 13) {
            for (u32 q = 0; q < 40; q++) U32(PART_POOL + q * 0xCC + 0x50) = 0;
            U32(MODEL_DATA + 0x14) = 13 + rnd() % 4; Model_Setup(mo, MODEL_DATA);
        }
        check(0x47ADA0, Player_LoadWeapons, "Player_LoadWeapons", 1, a, 0);
        for (int k = 0; k < 24; k++) U32(k < 16 ? 0x737FA0 + 4 * k : 0x737FF0 + 4 * (k - 16)) = (u32)(uintptr_t)g_recs[STUB_TYPE];
        /* qsort with the target comparator */
        u32 n = rnd() % 4 ? rnd() % 12 : rnd() % 60;
        for (u32 k = 0; k < n; k++) {
            U32(0x6F0000 + 8 * k) = rnd() % 3 ? rnd() % 8 : rnd();
            U16(0x6F0004 + 8 * k) = (u16)k; U16(0x6F0006 + 8 * k) = rnd() % 4 == 0;
        }
        a[0] = 0x6F0000; a[1] = n; a[2] = 8; a[3] = 0x47BF30;
        sprintf(g_what, "qsort n %u", n);
        check(0x4DDBD4, Crt_qsort, "Crt_qsort", 4, a, 0);
        a[0] = 0x6F0000; a[1] = 0x6F0008;
        check(0x47BF30, Target_Compare, "Target_Compare", 2, a, 0xFFFFFFFF);
        /* cones */
        { u32 lo = rnd() % 400, blo = rnd() % 200;                    /* hi >= lo (the game's tables) */
          a[0] = lo | (lo + rnd() % 3000) << 16; a[1] = blo | (blo + rnd() % 2000) << 16; }
        a[2] = G_SCR; a[3] = G_SCR + 8;
        for (int k = 0; k < 8; k++) S16(G_SCR + 2 * k) = cl_coord(0, 1500);
        sprintf(g_what, "cone %08X %08X", a[0], a[1]);
        check(0x47C1A0, Cone_HitX, "Cone_HitX", 4, a, 0xFFFFFFFF);
        check(0x47C250, Cone_HitZ, "Cone_HitZ", 4, a, 0xFFFFFFFF);
        /* object effects */
        U32(G() + 0x93C) = O_EFF;
        for (u32 k = 0; k < 12; k++) U32(O_EFF + k * 0x70 + 0x50) = rnd() % 3 ? 1 : 0;
        check(0x47C150, Eff_Alloc, "Eff_Alloc", 0, a, 0xFFFFFFFF);
        check(0x47AF60, Eff_FreeAll, "Eff_FreeAll", 0, a, 0);
        /* targets */
        gptr o = player_setup_obj(); targets_setup(); U32(LOG_BASE) = 0;
        int pass = 1;                                   /* GUNS=1: only the firearms, once (mutation runs) */
        if (getenv("GUNS")) goto firearms;
        U32(0x87FD60) = o + 0x1C0;
        a[0] = o; a[1] = rnd() % 3 ? (rnd() % 2 ? 0x400 : 0x180) : rnd() % 0x800;
        sprintf(g_what, "objs %u range %X", U32(0x87F940), a[1]);
        check(0x47BDE0, Target_Find, "Target_Find", 2, a, 0);
        /* aiming at the target found (weapon handler tables 0x738018/48/78 are recorders) */
        U16(o + 0x1C0 + 0x30) = rnd() % 3 ? (u16)(rnd() % 0x1000) : (u16)rnd();
        U16(G() + 0x10B4) = (u16)(rnd() % 12 | (rnd() % 10) << 8);
        a[0] = o;
        sprintf(g_what, "objs %u pitch %X weapon %04X", U32(0x87F940), U16(o + 0x1F0), U16(G() + 0x10B4));
        for (u32 k = 0; k < 34; k++) U32(0x738018 + 4 * k) = (u32)(uintptr_t)g_recs[k % 12 + 71];   /* which entry was called */
        check(0x47AE00, Player_Aim, "Player_Aim", 1, a, 0);
        check(0x47ADE0, Player_WeaponState, "Player_WeaponState", 1, a, 0);
        check(0x47AF20, Player_SubState, "Player_SubState", 1, a, 0);
        for (u32 k = 0; k < 34; k++) U32(0x738018 + 4 * k) = (u32)(uintptr_t)g_recs[STUB_TYPE];
        a[0] = rnd() % 2 ? (u32)rnd() : rnd() % 0x1000; a[1] = rnd() % 2 ? 0x180 : (u32)rnd();
        a[2] = rnd() % 4 == 0 ? a[0] : rnd() % 2 ? (u32)rnd() : (a[0] + rnd() % 0x400 - 0x200) & 0xFFFF;
        sprintf(g_what, "angle %08X step %08X to %08X", a[0], a[1], a[2]);
        check(0x47CAE0, Angle_Step, "Angle_Step", 3, a, 0xFFFF);
        a[0] = o;
        check(0x48FCE0, Obj_SaveFeet, "Obj_SaveFeet", 1, a, 0);
        /* shots */
        aim_desc_setup(0);
        gptr shot = G_SCR + 0x40;
        S16(G_SCR + 0x60) = (s16)(rnd() % 0x1000); S16(G_SCR + 0x62) = (s16)(rnd() % 0x1000); S16(G_SCR + 0x64) = (s16)(rnd() % 0x1000);
        if (rnd() % 2) { S16(G_SCR + 0x60) = 0; S16(G_SCR + 0x64) = 0; }
        RotMatrix(G_SCR + 0x60, shot);
        U8(0x87E747) = rnd() % 3; U8(G() + 0x108F) = rnd() % 10;
        U32(G() + 0x948) = O_K3; U16(O_K3 + 0x18) = (u16)(0x200 + rnd() % 0x900);   /* kind 5: the light's reach */
        a[0] = shot; a[1] = o + 0x40; a[2] = rnd() % 4 ? rnd() % 0x23 : rnd() % 2 ? 5 : 2;
        sprintf(g_what, "objs %u kind %X rot %X/%X/%X", U32(0x87F940), a[2], U16(G_SCR + 0x60), U16(G_SCR + 0x62), U16(G_SCR + 0x64));
        check(0x47B430, Weapon_Hit, "Weapon_Hit", 3, a, 0xFFFFFFFF);
        { u32 n = 1 + rnd() % 24;
          for (u32 k = 0; k < n * 4; k++) U32(0x74E380 + 4 * k) = rnd() % 4 ? (u32)rnd() : rnd() % 8;
          a[0] = 0x74E380; a[1] = 0; a[2] = n - 1;
          sprintf(g_what, "sort %u", n);
          check(0x47C030, Hit_Sort, "Hit_Sort", 3, a, 0); }
        check(0x496400, Stat_Shot, "Stat_Shot", 0, a, 0);
        /* sub-weapons in flight */
        o = player_setup_obj(); world_setup(); aim_desc_setup(0); U32(LOG_BASE) = 0;
        for (int k = 0; k < 256; k++) U32(TYPE_TAB + 4 * k) = (u32)(uintptr_t)g_recs[STUB_TYPE];
        U16(O_K3 + 0x18) = (u16)(0x200 + rnd() % 0x900);
        U32(0x87FD60) = o + 0x1C0;
        U32(G() + 0x93C) = O_EFF;
        for (u32 k = 0; k < 12; k++) {
            gptr e = O_EFF + k * 0x70;
            static const u8 sts[] = {3, 3, 5, 6, 7, 8, 9, 10, 0x10, 2};
            U32(e + 0x50) = rnd() % 4 ? 1 | (rnd() & 0x40) : 0;
            U8(e + 0x4E) = rnd() % 6 ? 3 : (u8)rnd();
            U8(e + 0x4F) = rnd() % 8 ? sts[rnd() % 10] : (u8)(rnd() % 0x13);   /* weapon kinds stay in the table */
            U8(e + 0x46) = rnd() % 3 ? (u8)(rnd() % 3) : (u8)rnd(); U8(e + 0x47) = (u8)(rnd() % 3);
            S16(e + 0x40) = cl_coord(0, 1500); S16(e + 0x42) = rnd() % 3 ? cl_coord(-500, 400) : cl_coord(0, 2500);
            S16(e + 0x44) = cl_coord(0, 1500);
            if (rnd() % 16 == 0) S16(e + 0x40 + 2 * (rnd() % 3)) = (s16)(rnd() % 2 ? 29900 : -29900);
            for (u32 b = 0; b < 6; b += 2) S16(e + 0x48 + b) = (s16)(rnd() % 0x1000);
            RotMatrix(e + 0x48, e + 0x20);
            s32 sp = rnd() % 2 ? 1500 : 150;                /* fast ones cross the walls */
            S16(e + 0x60) = cl_coord(0, sp); S16(e + 0x62) = cl_coord(0, 300); S16(e + 0x64) = cl_coord(0, sp);
            S16(0x87FD80 + 4 * k) = (s16)(rnd() % 4 ? (s32)(rnd() % 16) : -1);
            S16(0x87FD82 + 4 * k) = (s16)(U32(U32(O_OBJ + (rnd() % 16) * 0x260 + 0x64) + 0x54) ? rnd() % 2 : 0);
        }
        for (u32 k = 0; k < 12; k++) {                  /* homing targets: parts that exist */
            s16 ti = S16(0x87FD80 + 4 * k);
            if (ti > 0 && !U32(U32(O_OBJ + ti * 0x260 + 0x64) + 0x54)) S16(0x87FD82 + 4 * k) = 0;
        }
        a[0] = o;
        { char *w = g_what + sprintf(g_what, "objs %u:", U32(0x87F940));
          for (u32 k = 0; k < 12; k++) {
              gptr e = O_EFF + k * 0x70;
              w += sprintf(w, " %X%s/%u", U8(e + 0x4F), (U8(e + 0x50) & 1) && U8(e + 0x4E) == 3 ? "" : "x", U8(e + 0x46));
          } }
        check(0x47AFA0, Thrown_Update, "Thrown_Update", 1, a, 0);
        { gptr e = O_EFF + 0x70 * (rnd() % 12);
          S16(e + 0x74) = rnd() % 8 ? cl_coord(S16(e + 0x40), 600) : S16(e + 0x40);
          S16(e + 0x78) = rnd() % 8 ? cl_coord(S16(e + 0x44), 600) : S16(e + 0x44);
          U16(e + 0x5A) = (u16)(rnd() % 3); U8(e + 0x85) = (u8)(rnd() & 0x18);
          a[0] = e;
          check(0x453060, Eff_WallHit, "Eff_WallHit", 1, a, 0xFFFFFFFF);
          a[0] = e + 0x40; a[1] = rnd(); a[2] = rnd(); a[3] = rnd(); a[4] = rnd(); a[5] = rnd();
          check(0x48DBC0, Effect_Spawn, "Effect_Spawn", 6, a, 0xFFFFFFFF); }
        a[0] = rnd(); a[1] = rnd(); a[2] = rnd();
        U8(G() + 0x10B4) = rnd() % 3 ? (u8)(rnd() % 12) : 0xB; U8(G() + 0x10B5) = (u8)(rnd() % 10);
        check(0x48B830, Noise_Make, "Noise_Make", 3, a, 0);
        targets_setup();                                /* the arms' target needs its descriptors */
        /* arms: the player is the animated object (enough parts) */
        do { anim_setup(); } while (U32(AN_OBJ + 0x68) < 5);
        U32(0x87FD60) = AN_OBJ + 0x1C0; U32(LOG_BASE) = 0;
        for (u32 k = 0x58; k < 0x78; k += 2) U16(AN_OBJ + 0x1C0 + k) = (u16)rnd();
        S16(AN_OBJ + 0x40) = cl_coord(0, 1500); S16(AN_OBJ + 0x44) = cl_coord(0, 1500);
        a[0] = AN_OBJ; a[1] = rnd() % 2 ? 1 : 4; a[2] = rnd() % 0x1000;
        sprintf(g_what, "arm %u yaw %X objs %u", a[1], a[2], U32(0x87F940));
        check(0x47C4C0, Arm_Relax, "Arm_Relax", 2, a, 0);
        check(0x47C2C0, Arm_Aim, "Arm_Aim", 3, a, 0);
        /* states */
        player_setup_obj(); targets_setup();
        do { anim_setup(); } while (U32(AN_OBJ + 0x68) < 5);       /* arms are parts 1 and 4 */
        o = AN_OBJ; U32(o + 0x120) = AN_PART; U32(o + 0x124) = AN_PART + 0x80;
        S16(o + 0x40) = cl_coord(0, 1500); S16(o + 0x44) = cl_coord(0, 1500);
        U32(LOG_BASE) = 0;
        U32(0x87FD60) = o + 0x1C0;
        U32(G() + 0x92C) = (u32)(uintptr_t)g_recs[STUB_TYPE]; U32(G() + 0x930) = (u32)(uintptr_t)g_recs[STUB_TYPE];
        U16(G() + 0x10B4) = (u16)((rnd() % 8) << 8 | (rnd() % 2 ? 6 : rnd() % 13));
        U8(o + 0x55) = rnd() % 12; U8(o + 0x56) = rnd() % 3;
        U32(o + 0x1C0) = rnd() % 3 ? rnd() % 0x1000000 : rnd();
        a[0] = o;
        sprintf(g_what, "char %u set %04X sub %u/%u", U8(G() + 0x108F), U16(G() + 0x10B4), U8(o + 0x55), U8(o + 0x56));
        check(0x47D500, Player_Normal, "Player_Normal", 1, a, 0);
        U8(o + 0x55) = rnd() % 4;
        check(0x47DA60, Player_State2, "Player_State2", 1, a, 0);
        check(0x47DBD0, Player_State5, "Player_State5", 1, a, 0);
        check(0x47DC50, Player_State6, "Player_State6", 1, a, 0);
        check(0x47AF40, Player_Weapon, "Player_Weapon", 1, a, 0);
        /* damage */
        U8(G() + 0x108E) = rnd() % 3; U8(G() + 0x927) = rnd() % 3 == 0; U8(G() + 0x108F) = rnd() % 10;
        Flag_Set(2, 0x26, rnd() % 3 == 0); Flag_Set(2, 0x27, rnd() % 3 == 0); Flag_Set(9, 0x33, rnd() % 3 == 0);
        a[0] = rnd() % 3 ? rnd() % 200 : rnd();
        sprintf(g_what, "dmg %X diff %u", a[0], U8(G() + 0x108E));
        check(0x479700, Damage_Scale, "Damage_Scale", 1, a, 0xFFFF);
        U16(0x87DC12) = (u16)rnd();
        check(0x479D70, Pad_Shake, "Pad_Shake", 0, a, 0xFFFFFFFF);
        o = AN_OBJ; anim_setup(); U32(LOG_BASE) = 0;
        U32(0x87FD60) = o + 0x1C0;
        U8(o + 0x55) = rnd() % 2; U8(o + 0x56) = rnd() % 3; U16(o + 0x178) = rnd() % 4; U16(o + 0x176) = rnd() % 300;
        U16(o + 0x108) = rnd() % 400; U16(o + 0x92) = rnd() % 12; U8(o + 0x57) = (u8)rnd();
        a[0] = o;
        sprintf(g_what, "hit %u/%u dir %u dmg %u hp %d", U8(o + 0x55), U8(o + 0x56), U16(o + 0x178), U16(o + 0x176), S16(o + 0x108));
        check(0x47D6A0, Player_HitLight, "Player_HitLight", 1, a, 0);
        anim_setup(); U32(LOG_BASE) = 0;
        U8(o + 0x55) = 2 + rnd() % 2; U8(o + 0x56) = rnd() % 5; U16(o + 0x176) = rnd() % 300; U16(o + 0x108) = rnd() % 400;
        U8(o + 0x57) = (u8)rnd(); U16(o + 0x1C0 + 0x28) = rnd() % 3 ? rnd() % 40 : (u16)rnd(); U16(0x87DC12) = (u16)rnd();
        U16(o + 0x5A) = rnd() % 4; S16(o + 0x42) = rnd() % 2 ? -1000 * S16(o + 0x5A) : cl_coord(0, 3000);
        if (rnd() % 2) U8(G() + 0x108F) = 2;
        sprintf(g_what, "down %u/%u shake %u char %u", U8(o + 0x55), U8(o + 0x56), U16(o + 0x1E8), U8(G() + 0x108F));
        check(0x47D820, Player_HitDown, "Player_HitDown", 1, a, 0);
        o = player_setup_obj(); U32(LOG_BASE) = 0; U32(0x87FD60) = o + 0x1C0;
        do anim_setup_at(o); while (0);
        U32(o + 0x64) = AN_PART; U8(o + 0x55) = rnd() % 3; U8(G() + 0x108F) = rnd() % 2 ? 2 : rnd() % 10;
        a[0] = o;
        U32(0x87DC98) = 0x6D0000; stub_at(0x4C08A0)->ret = rnd();   /* GameOver_Start's prims */
        sprintf(g_what, "die %u char %u", U8(o + 0x55), U8(G() + 0x108F));
        check(0x47DAB0, Player_Die, "Player_Die", 1, a, 0);
        U8(o + 0x55) = rnd() % 8;
        check(0x4796C0, Player_Scripted, "Player_Scripted", 1, a, 0);
        check(0x4796E0, Obj_Stop, "Obj_Stop", 1, a, 0);
        /* inventory */
        for (u32 k = 0; k < 0x14 * 12; k += 2) U16(G() + 0x10BC + k) = rnd() % 3 ? rnd() % 0x20 : (u16)rnd();
        U8(G() + 0x10B5) = rnd() % 8; U8(G() + 0x1094) = rnd() % 4 == 0;
        if (rnd() % 2) { u32 k = rnd() % 0x14; U8(G() + 0x10BC + 12 * k) = 1; U8(G() + 0x10BD + 12 * k) = U8(G() + 0x10B5) | 0x10; }
        if (rnd() % 2) { u32 k = rnd() % 0x14; U8(G() + 0x10BC + 12 * k) = 0; U8(G() + 0x10BD + 12 * k) = U8(G() + 0x10B5) | 0x10; }
        a[0] = G() + 0x10BC; a[1] = rnd() % 0x18; a[2] = U8(G() + 0x10B5) | (rnd() % 2 ? 0x10 : 0);
        check(0x495CB0, Item_Find, "Item_Find", 3, a, 0xFF);
        a[0] = rnd() % 4 ? rnd() % 4 : rnd();
        check(0x496240, Ammo_Have, "Ammo_Have", 1, a, 0xFFFFFFFF);
        U32(G() + 0x93C) = O_EFF; U8(o + 0x1C0 + 0x78) = rnd() % 2;
        for (u32 k = 0; k < 12; k++) U8(O_EFF + k * 0x70 + 0x4F) = rnd() % 5;
        U32(0x87FD60) = o + 0x1C0;
        check(0x47C860, Player_SubDone, "Player_SubDone", 0, a, 0xFFFFFFFF);
        /* walls ahead */
        for (int rep = 0; rep < 4; rep++) {
            o = player_setup_obj(); U32(LOG_BASE) = 0;
            for (u32 k = 0; k < U8(CL_ROOM); k++) {
                gptr w = CL_ROOM + U32(CL_ROOM + 8 + 4 * k);
                U8(w) |= rnd() % 4 ? 1 : 0;
                for (u32 e = 0; e < U8(w + 1); e++) U8(w + 8 + 4 * e) = (u8)((rnd() % 3 ? 0x81 : rnd()) | (rnd() % 3 ? 0 : 0x44) | (rnd() % 4 ? 0 : 8));
            }
            cl_place(o);
            if (rnd() % 2) U32(G() + 0x940) = o;
            a[0] = o; a[1] = rnd() % 3 ? 300 + rnd() % 1200 : (u32)rnd();
            U16(o + 0x48) = (u16)rnd(); U16(o + 0x4A) = (u16)rnd(); U16(o + 0x4C) = rnd() % 4 ? 0 : (u16)rnd();
            sprintf(g_what, "dist %X yaw %X fl %08X", a[1], U16(o + 0x4A), U32(o + 0x50));
            check(0x479DA0, Wall_Ahead, "Wall_Ahead", 2, a, 0xFFFFFFFF);
            /* steps: some polygons one layer pair up/down, ledges */
            for (u32 k = 0; k < U8(CL_ROOM); k++) {
                gptr w = CL_ROOM + U32(CL_ROOM + 8 + 4 * k);
                u32 l = U8(o + 0x5A) & 15;
                U16(w + 4) = (u16)(rnd() % 3 ? 1u << ((l + (rnd() % 2 ? 2 : 14)) & 15) | (rnd() % 2 ? 1u << l : 0) : (u16)rnd());
                U8(w) = (u8)((rnd() % 4 ? 1 : 0) | (rnd() % 3 ? 0 : 0x40));
                U8(w + 6) = rnd() % 2 ? 0 : (u8)rnd();
                for (u32 e = 0; e < U8(w + 1); e++) U8(w + 8 + 4 * e) = (u8)((rnd() % 4 ? 1 : 0) | (rnd() % 2 ? 0x80 : 0) | (rnd() % 2 ? 0x40 : 0) | (rnd() % 3 ? 0 : 4));
            }
            cl_place(o);
            if (rnd() % 3 == 0) U16(o + 0x42) = (u16)(-1000 * S16(o + 0x5A));
            a[0] = o; a[1] = rnd() % 3 ? 0x3E8 : rnd() % 2000; a[2] = rnd() % 3 ? 0xC8 : rnd() % 400; a[3] = rnd() % 3 ? (rnd() & 0x30) << 6 : rnd() % 0x1000;
            sprintf(g_what, "step len %u w %u yaw %X layer %d", a[1], a[2], a[3], S16(o + 0x5A));
            check(0x479FC0, Step_Ahead, "Step_Ahead", 4, a, 0xFFFFFFFF);
        }
        /* standing */
        for (pass = 0; pass < 2; pass++) {
            player_setup_obj(); do anim_setup(); while (U32(AN_OBJ + 0x68) < 5); o = AN_OBJ; U32(LOG_BASE) = 0;
            U32(0x87FD60) = o + 0x1C0; U32(o + 0x128) = CL_CYL + 0x30; U8(o + 0x5A) = 0;
            U8(CL_CYL + 0x30 + 0xA) = rnd() % 2 ? 0xFF : 0;     /* a part the animated object has */
            for (u32 k = 0; k < U8(CL_ROOM); k++) {
                gptr w = CL_ROOM + U32(CL_ROOM + 8 + 4 * k);
                for (u32 e = 0; e < U8(w + 1); e++) U8(w + 8 + 4 * e) |= rnd() % 2 ? 0x81 : 0;
            }
            if (rnd() % 2) cl_place(o);
            U8(o + 0x56) = rnd() % 4 ? rnd() % 7 : 0xFF;
            U16(o + 0x1C0 + 0x38) = rnd() % 3 ? rnd() % 3 : (u16)rnd(); U16(o + 0x1C0 + 0x1C) = rnd() % 4; U16(o + 0x1C0 + 0x1E) = rnd() % 4;
            U32(0x87DC08) = rnd() % 3 ? (rnd() & 0xF07E) : rnd(); U32(0x87DC94) = rnd() % 2 ? 0 : (u8)rnd();
            U16(G() + 0x10B4) = (u16)(rnd() % 2 ? 0 : (rnd() % 8) << 8) | (u16)(rnd() % 2 ? 8 : rnd() % 16);
            Flag_Set(2, 0x1C, rnd() % 2); Flag_Set(2, 0xF, rnd() % 4 == 0);
            a[0] = o;
            sprintf(g_what, "stand %02X held %08X press %02X set %04X", U8(o + 0x56), U32(0x87DC08), U8(0x87DC94), U16(G() + 0x10B4));
            check(0x47DC70, Player_Stand, "Player_Stand", 1, a, 0);
            U8(o + 0x56) = rnd() % 4 ? rnd() % 3 : 0xFF; U16(o + 0x92) = rnd() % 64;
            U32(0x87DC08) = rnd() % 3 ? (rnd() & 0xF0FE) : rnd();
            check(0x47E290, Player_Walk, "Player_Walk", 1, a, 0);
            for (u32 r = 0; r < 2; r++) {                   /* in water */
                U8(o + 0x56) = (u8)(rnd() % 4 ? rnd() % 2 : rnd() % 4);
                U32(0x87DC08) = rnd() % 3 ? (rnd() & 0xF0FE) : rnd() % 2 ? 0x20 : (u32)(1u << (rnd() % 16));
                U32(0x87DC94) = rnd() % 2 ? 0 : 1u << (rnd() % 8);
                stub_at(0x4BF330)->ret = rnd() % 2;
                sprintf(g_what, "water %02X held %08X press %02X set %04X", U8(o + 0x56), U32(0x87DC08), U8(0x87DC94), U16(G() + 0x10B4));
                check(r ? 0x47E640 : 0x47E050, r ? (void *)Player_WaterWalk : (void *)Player_WaterStand,
                      r ? "Player_WaterWalk" : "Player_WaterStand", 1, a, 0);
            }
            U8(o + 0x56) = rnd() % 3; U32(0x87DC08) = rnd() % 3 ? (rnd() & 0xF0FE) : rnd(); U32(0x87DC94) = rnd() % 2 ? 0 : (u8)rnd();
            check(0x47E890, Player_Turn, "Player_Turn", 1, a, 0);
            U8(o + 0x56) = rnd() % 3; U32(0x87DC08) = rnd() % 3 ? (rnd() & 0xF0FE) : rnd(); U32(0x87DC94) = rnd() % 2 ? 0 : (u8)rnd();
            check(0x47EBB0, Player_Back, "Player_Back", 1, a, 0);
            U8(o + 0x56) = rnd() % 3; U16(o + 0x1C0 + 0x22) = (u16)rnd(); U16(o + 0x1C0 + 0x20) = U16(o + 0x1C0 + 0x22) + rnd() % 0x400;
            check(0x47EED0, Player_TurnBack, "Player_TurnBack", 1, a, 0);
            check(0x48F8B0, Obj_ShadowOff, "Obj_ShadowOff", 1, a, 0);
            U8(o + 0x56) = rnd() % 4; U8(o + 0x11A) = (u8)rnd(); U16(o + 0x1C0 + 0x20) = (u16)((U16(o + 0x4A) + rnd() % 0x80) & 0xFFF);
            if (rnd() % 2) U16(o + 0x1C0 + 0x20) = U16(o + 0x4A);
            check(0x47EF90, Player_Climb, "Player_Climb", 1, a, 0);
            U8(o + 0x56) = rnd() % 9; U16(o + 0x92) = rnd() % 20;
            check(0x47FC70, Player_Push, "Player_Push", 1, a, 0);
        firearms:
            /* firearms: the player is object 0 (the animation layers work on it) */
            player_setup_obj(); targets_setup(); aim_desc_setup(1);
            o = O_OBJ; do anim_setup_at(o); while (U32(o + 0x68) < 13);     /* parts 3, 6: the guns; 12: the sub-weapon */
            U32(G() + 0x940) = O_OBJ; U32(0x87FD60) = o + 0x1C0; a[0] = o;
            U32(0x87F948) = TYPE_TAB;                       /* effect kind handlers: recorders */
            for (int k = 0; k < 256; k++) U32(TYPE_TAB + 4 * k) = (u32)(uintptr_t)g_recs[STUB_TYPE];
            for (u32 l = 0; l < 2; l++) {                   /* the two layers' shadow objects */
                gptr sh = 0x7598D8 + l * 0x260;
                for (u32 k = 0x60; k < 0x110; k += 4) U32(sh + k) = U32(o + k);
                for (u32 k = 0; k < 0x78; k += 2) U16(0x759D98 + l * 0x78 + k) = (u16)(rnd() % 0x1000);
            }
            U32(LOG_BASE) = 0;
            for (u32 k = 0x14; k < 0x40; k += 2) U16(o + 0x1C0 + k) = (u16)(rnd() % 3 ? rnd() % 0x1000 : rnd());
            U32(o + 0x1C0 + 0x14) = rnd() % 3 ? O_OBJ + 0x260 * (1 + rnd() % 15) : 0;
            U8(o + 0x1C0 + 8) = (u8)(rnd() % 16); U8(o + 0x1C0 + 0x2E) = rnd() % 2; U8(o + 0x1C0 + 0x2F) = rnd() % 2;
            S16(o + 0x1C0 + 0x3C) = (s16)(rnd() % 2 ? rnd() % 5 : rnd() % 0x18);
            for (u32 k = 0; k < 20; k++) {
                gptr it = G() + 0x10BC + k * 12;
                U8(it) = rnd() % 2; U8(it + 1) = (u8)(rnd() % 12); S16(it + 8) = (s16)(rnd() % 3); U32(it + 4) = rnd();
                if (k < 12 && rnd() % 2) { U8(it) = 1; U8(it + 1) = (u8)k; }      /* most weapons have an entry */
            }
            U8(G() + 0x1094) = rnd() % 4 == 0; U8(G() + 0x75B) = (u8)rnd();
            U16(G() + 0x10B4) = (u16)(rnd() % 12 | (rnd() % 3 ? 0 : (rnd() % 10) << 8));
            U32(0x87DC08) = rnd() % 3 ? (rnd() & 0xF0FE) : rnd(); U32(0x87DC94) = rnd() % 2 ? 0 : rnd() % 2 ? (u8)rnd() : rnd();
            Flag_Set(2, 0x1C, rnd() % 2); Flag_Set(2, 0xE, rnd() % 4 == 0); Flag_Set(2, 4, rnd() % 4 == 0);
            stub_at(0x4BF330)->ret = rnd() % 2;                             /* the heavy weapons' dodge */
            U32(G() + 0x948) = O_K3; U16(O_K3 + 0x18) = (u16)(0x200 + rnd() % 0x900);   /* kind 5 (machine gun): the light's reach */
            for (u32 k = 0; k < 10; k++) U8(G() + 0x94C + k * 0x18 + 0xC) = (u8)(rnd() % 3 ? 1 : rnd());
            U8(o + 0x190 + 0xC) = (u8)rnd(); U8(o + 0x1A8 + 0xC) = (u8)rnd();
            U16(o + 0x92) = rnd() % 2 ? 9 + rnd() % 2 : rnd() % 3 ? rnd() % 30 : (u16[]){2, 3, 0xD, 0xE, 0x17, 0x18}[rnd() % 6];
            U16(0x7598D8 + 0x92) = U16(o + 0x92); U16(0x7598D8 + 0x260 + 0x92) = U16(o + 0x92);   /* the layers' frames */
            sprintf(g_what, "gun %02X/%02X held %08X press %08X set %04X tgt %X", U8(o + 0x56), U8(o + 0x57),
                    U32(0x87DC08), U32(0x87DC94), U16(G() + 0x10B4), U32(o + 0x1C0 + 0x14));
            { static const struct { u32 addr; void *fn; const char *name; u8 sub; } GF[] = {
                {0x4AB490, Handgun_Fire, "Handgun_Fire", 0}, {0x4ACCC0, Gun_Ready, "Gun_Ready", 1},
                {0x4AD0D0, Gun_Hold, "Gun_Hold", 1}, {0x4AABD0, Gun_Lower, "Gun_Lower", 1},
                {0x4AE9E0, Gun_NextTarget, "Gun_NextTarget", 1}, {0x4B27C0, Gun_TurnBack, "Gun_TurnBack", 1},
                {0x4AB640, Gun_Walk, "Gun_Walk", 1}, {0x4AB870, Gun_Back, "Gun_Back", 1}, {0x4ABAA0, Gun_Turn, "Gun_Turn", 1},
                {0x4AD9B0, Spray_Walk, "Spray_Walk", 1}, {0x4AD370, Spray_Back, "Spray_Back", 1}, {0x4B0220, Rifle_Turn, "Rifle_Turn", 1},
                {0x4B0AE0, Rifle_Walk, "Rifle_Walk", 1}, {0x4B0CE0, Rifle_Back, "Rifle_Back", 1},
                {0x4AC9E0, Weapon3_Fire, "Weapon3_Fire", 0}, {0x4AD6E0, Weapon4_Fire, "Weapon4_Fire", 0},
                {0x4B04D0, Weapon8_Fire, "Weapon8_Fire", 0}, {0x4B0910, Weapon9_Fire, "Weapon9_Fire", 0},
                {0x4ABD50, Shotgun_Fire, "Shotgun_Fire", 0}, {0x4AC140, Shotgun_Ready, "Shotgun_Ready", 1},
                {0x4AC550, Shotgun_Lower, "Shotgun_Lower", 1}, {0x4ABF00, Shotgun_Walk, "Shotgun_Walk", 1},
                {0x4AC750, Shotgun_Back, "Shotgun_Back", 1}, {0x4AE360, MGun_Fire, "MGun_Fire", 0},
                {0x4ADCE0, MGun_Ready, "MGun_Ready", 1}, {0x4AE0F0, MGun_Lower, "MGun_Lower", 1},
                {0x4AE5A0, MGun_Walk, "MGun_Walk", 1}, {0x4AE7D0, MGun_Back, "MGun_Back", 1},
                {0x4AEFA0, Twin_Fire, "Twin_Fire", 0}, {0x4AF7B0, Twin_Ready, "Twin_Ready", 1},
                {0x4AFDF0, Twin_Lower, "Twin_Lower", 1}, {0x4AF260, Twin_Walk, "Twin_Walk", 1},
                {0x4AEC70, Twin_Back, "Twin_Back", 1}, {0x4AF540, Weapon7_Fire, "Weapon7_Fire", 0},
                {0x4AFBC0, Weapon7_Walk, "Weapon7_Walk", 1}, {0x4AFFF0, Weapon7_Back, "Weapon7_Back", 1},
                {0x4B23A0, Heavy_Hold, "Heavy_Hold", 1},
                {0x4B1A50, Heavy_Turn, "Heavy_Turn", 1},
                {0x4B0F10, Heavy10_Fire, "Heavy10_Fire", 0},
                {0x4B12C0, Heavy10_Ready, "Heavy10_Ready", 1},
                {0x4B1690, Heavy10_Lower, "Heavy10_Lower", 1},
                {0x4B10C0, Heavy10_Walk, "Heavy10_Walk", 1},
                {0x4B1850, Heavy10_Back, "Heavy10_Back", 1},
                {0x4B1CF0, Heavy11_Fire, "Heavy11_Fire", 0},
                {0x4B1FD0, Heavy11_Ready, "Heavy11_Ready", 1},
                {0x4B2600, Heavy11_Lower, "Heavy11_Lower", 1},
                {0x4B2890, Heavy11_Walk, "Heavy11_Walk", 1},
                {0x4B2A60, Heavy11_Back, "Heavy11_Back", 1},
                {0x4AA7C0, Bare_Ready, "Bare_Ready", 1},
                {0x4AADD0, Bare_Walk, "Bare_Walk", 1},
                {0x4AAFD0, Bare_Back, "Bare_Back", 1},
                {0x4AB1E0, Bare_Turn, "Bare_Turn", 1}};
              for (u32 k = 0; k < sizeof GF / sizeof GF[0]; k++) {
                  if (GF[k].sub) U8(o + 0x57) = rnd() % 4 ? rnd() % 3 : 0xFF; else U8(o + 0x56) = (u8)(rnd() % 2 ? 1 : rnd() % 3);
                  if (!GF[k].sub && rnd() % 3 == 0) {                  /* aim held, maybe the trigger, maybe a button */
                      U32(0x87DC08) = 8 | (rnd() % 2 ? 0x20 : 0); U32(0x87DC94) = rnd() % 3 ? 0 : 1u << rnd() % 8;
                  }
                  if (GF[k].sub && rnd() % 4 == 0) {                   /* firing on the walk: aim, trigger, a direction */
                      U32(0x87DC08) = 0x28 | (rnd() % 2 ? 0x1000 : 0x4000) | (rnd() % 2 ? 0x2000 : 0); U32(0x87DC94) = 0;
                      U8(o + 0x1C0 + 0x2F) = 1; U8(G() + 0x1094) |= rnd() % 2;
                  } else if (GF[k].sub && rnd() % 3 == 0) {            /* standing: aim and trigger, or one button */
                      U32(0x87DC08) = rnd() % 2 ? 0x28 : 0x1028; U32(0x87DC94) = rnd() % 2 ? 0 : 0x40u << (rnd() % 2 * 8);
                      U8(o + 0x1C0 + 0x2E) = 1; U8(o + 0x1C0 + 0x2F) = 1; U16(o + 0x1C0 + 0x20) = U16(o + 0x4A);
                      U8(G() + 0x1094) |= rnd() % 2;
                  }
                  if (rnd() % 3 == 0) U16(o + 0x1C0 + 0x20) = U16(o + 0x4A);          /* already facing */
                  U8(o + 0x190 + 0xC) = rnd() % 2 ? 0 : (u8)rnd(); U8(o + 0x1A8 + 0xC) = rnd() % 2 ? 0 : (u8)rnd();   /* flashes */
                  S16(o + 0x1C0 + 0x3C) = (s16)(rnd() % 2 ? rnd() % 5 : rnd() % 0x18);
                  if (rnd() % 3 == 0) {                                /* frames at the thresholds */
                      U16(o + 0x92) = (u16[]){2, 3, 0xD, 0xE, 0x17, 0x18}[rnd() % 6];
                      U16(0x7598D8 + 0x92) = U16(o + 0x92); U16(0x7598D8 + 0x260 + 0x92) = U16(o + 0x92);
                  }
                  if (rnd() % 3 == 0) U16(o + 0x1C0 + 0x32) = U16(o + 0x1C0 + 0x30);
                  check(GF[k].addr, GF[k].fn, GF[k].name, 1, a, 0);
              } }
            { static const struct { u32 addr; void *fn; } WS[] = {
                {0x4AA7A0, WState_0}, {0x4AB450, WState_1}, {0x4ABD10, WState_2}, {0x4AC9A0, WState_3},
                {0x4AD6A0, WState_4}, {0x4AE320, WState_5}, {0x4AEF60, WState_6}, {0x4AF500, WState_7},
                {0x4B0490, WState_8}, {0x4B08D0, WState_9}, {0x4B0ED0, WState_10}, {0x4B1CB0, WState_11}};
              u32 saved[108], k = rnd() % 12;
              for (u32 q = 0; q < 108; q++) { saved[q] = U32(0x73B44C + 4 * q); U32(0x73B44C + 4 * q) = (u32)(uintptr_t)g_recs[71 + q % 12]; }
              U8(o + 0x56) = (u8)(rnd() % 9);
              check(WS[k].addr, WS[k].fn, "WState", 1, a, 0);
              for (u32 q = 0; q < 108; q++) U32(0x73B44C + 4 * q) = saved[q]; }
            /* sub-weapons: in use (substate 10) and put back */
            { static const struct { u32 addr; void *fn; const char *name; u32 an[2]; } SW[] = {   /* an: their animations */
                {0x4B2CF0, Sub1_Use, "Sub1_Use", {0x67CB68}}, {0x4B3030, Sub2_Use, "Sub2_Use", {0x67CEF4}},
                {0x4B3DD0, Sub7_Use, "Sub7_Use", {0x67CB68}}, {0x4B41F0, Sub8_Use, "Sub8_Use", {0x67CEF4}},
                {0x4B38E0, Sub5_Use, "Sub5_Use", {0x67C500}}, {0x4B4600, Sub9_Use, "Sub9_Use", {0x67C978, 0x67D420}},
                {0x4B40A0, Sub1_PutBack, "Sub1_PutBack"}, {0x4B3330, Sub2_PutBack, "Sub2_PutBack"},
                {0x4B3650, Sub3_PutBack, "Sub3_PutBack"}, {0x4B3CA0, Sub4_PutBack, "Sub4_PutBack"},
                {0x4B44F0, Sub8_PutBack, "Sub8_PutBack"}, {0x4B4580, Sub9_PutBack, "Sub9_PutBack"},
                {0x4B3440, Sub3_Use, "Sub3_Use", {0x67C9F4, 0x67D0D0}}, {0x4B36B0, Sub4_Use, "Sub4_Use", {0x67CB8C, 0x67D268}},
                {0x4B3AA0, Sub6_Use, "Sub6_Use", {0x67CB8C, 0x67D268}}};
              U32(0x87DE14) = 2 + rnd() % 12; U32(G() + 0xA74) = rnd() % 4;   /* lights for the flare */
              for (u32 k = 0; k < 16; k++) U8(O_K3 + k * 0x34) = (u8)(rnd() % 3 ? 1 : rnd());
              for (u32 k = 0; k < sizeof SW / sizeof SW[0]; k++) {
                  U8(o + 0x56) = rnd() % 4 ? 1 : rnd() % 3;
                  anim_long(o, rnd() % 2 ? rnd() % 0x20 : (u16[]){0, 1, 4, 5, 6, 7, 8, 9, 0xA, 0xC, 0xD, 0xE, 0x11,
                                                                  0x12, 0x13, 0x14, 0x15, 0x1C, 0x1D, 0x1E}[rnd() % 20]);
                  if (SW[k].addr == 0x4B38E0 && rnd() % 2) { anim_long(o, 9 + rnd() % 2); U8(o + 0x56) = 1; }   /* the kick lands */
                  U32(0x87DC08) = rnd() % 2 ? (rnd() % 2 ? 0x1000 : 8) : rnd();
                  U32(O_EFF + 0x50) = rnd() % 2 ? 3 : rnd(); U8(G() + 0x108F) = rnd() % 2;
                  for (u32 q = 0; q < 2; q++) {                       /* valid animations at their addresses (some overlap: only this one's) */
                      gptr an = SW[k].an[q]; if (!an) continue;
                      u32 stride = (u8)(((U32(o + 0x68) * 6 + 0x13) >> 2) << 2);
                      U32(an) = 2; for (u32 i = 4; i < 0x14; i += 4) U32(an + i) = 0;
                      for (u32 i = 0; i < 3 * stride; i += 2) U16(an + 0x14 + i) = (u16)(rnd() % 0x1400);
                      for (u32 f = 0; f < 3; f++) S16(an + 0x14 + f * stride + 2) = 1 + rnd() % 8;
                  }
                  if (rnd() % 3 == 0) { anim_long(o, 5 + rnd() % 2); U8(o + 0x56) = rnd() % 3 ? 1 : 2; }   /* the landing frame */
                  for (u32 j = 0; j < 12; j++) U32(O_EFF + j * 0x70 + 0x50) = rnd() % 3 ? 0 : 1;
                  if (rnd() % 3 == 0) {                               /* few free effect slots */
                      for (u32 j = 0; j < 12; j++) U32(O_EFF + j * 0x70 + 0x50) = 1;
                      for (u32 j = rnd() % 3; j; j--) U32(O_EFF + rnd() % 12 * 0x70 + 0x50) = 0;
                  }
                  g_ign_throw = 1;
                  check(SW[k].addr, SW[k].fn, SW[k].name, 1, a, 0);
                  g_ign_throw = 0;
              }
              a[1] = O_OBJ + 0x260 * (rnd() % 16); U16(0x6DDA00u) = (u16)rnd(); U16(0x6DDA00u + 2) = (u16)rnd(); U16(0x6DDA00u + 4) = (u16)rnd();
              a[2] = 0x6DDA00u;
              for (u32 j = 0; j < 12; j++) U32(O_EFF + j * 0x70 + 0x50) = rnd() % 3 ? 0 : 1;
              check(0x47C8F0, Mines_Place, "Mines_Place", 2, a + 1, 0xFFFFFFFF);
            /* substate 9: crouched with a heavy weapon */
            for (u32 r = 0; r < 4; r++) {
                gptr px = o + 0x1C0;
                U8(o + 0x56) = rnd() % 8 ? rnd() % 7 : rnd() % 9;
                anim_long(o, rnd() % 0x20);
                U16(G() + 0x10B4) = (u16)((rnd() % 2 ? 0xA : 0xB) | (U16(G() + 0x10B4) & 0xFF00));
                for (u32 q = 0x40; q < 0x56; q += 2) U16(px + q) = (u16)(rnd() % 3 ? (s16)(rnd() % 0x800 - 0x400) : (u16)rnd());
                if (rnd() % 3 == 0) U16(px + 0x4C) = (u16)((rnd() % 2 ? 1 : -1) * (s16)(0x2AF + rnd() % 0x40));
                if (rnd() % 3 == 0) U16(px + 0x42) = (u16)(-0x10 + rnd() % 0x10);
                if (U8(o + 0x56) == 3 && rnd() % 2) { anim_long(o, 0x1F); U16(o + 0x98) = 8; U16(o + 0x102) &= ~0x21; }   /* standing up: the end */
                U8(G() + 0x1094) = rnd() % 3 == 0;                                       /* ammunition: some out */
                for (u32 q = 0; q < 20; q++) S16(G() + 0x10BC + q * 12 + 8) = (s16)(rnd() % 2);
                U8(px + 0x7A) = rnd() % 2; U8(px + 0x7D) = rnd() % 3 != 0; U8(px + 0x2F) = rnd() % 2;
                U16(px + 0x20) = rnd() % 2 ? U16(o + 0x4A) : (u16)rnd();
                S16(px + 0x3C) = (s16)(rnd() % 0x14);
                U32(0x87DC08) = rnd() % 3 ? (rnd() & 0xF0FE) : rnd() % 2 ? 0x28 : 0x28 | (rnd() % 2 ? 0x1000 : 0x4000) | (rnd() % 2 ? 0x2000 : 0x8000);
                U32(0x87DC94) = rnd() % 2 ? 0 : rnd() % 3 ? 1u << rnd() % 8 : 2;
                stub_at(0x4BF330)->ret = rnd() % 2;
                U16(o + 0x42) = (u16)(rnd() % 3 ? -(s16)(rnd() % 0x1800) : (s16)(rnd() % 0x400));
                sprintf(g_what, "crouch %u px42 %04X 4C %04X 7D %u held %08X press %08X", U8(o + 0x56), U16(px + 0x42), U16(px + 0x4C),
                        U8(px + 0x7D), U32(0x87DC08), U32(0x87DC94));
                check(0x47F0F0, Player_Crouch, "Player_Crouch", 1, a, 0);
                U8(px + 0x3C) = 0;
                U32(0x87DC94) = rnd() % 2 ? 0x20 : 0;
                check(0x47FA80, Crouch_Fire, "Crouch_Fire", 1, a, 0xFFFFFFFF);
            }
              a[1] = rnd() % 4;
              check(0x496350, Sub_AmmoUse, "Sub_AmmoUse", 1, a + 1, 0);
              check(0x48D410, Light_Alloc, "Light_Alloc", 0, a, 0xFFFFFFFF);
              a[1] = O_OBJ + 0x260 * (rnd() % 16); a[2] = rnd(); a[3] = rnd();
              check(0x48DB80, Effect_SpawnAt, "Effect_SpawnAt", 3, a + 1, 0xFFFFFFFF); }
            a[1] = rnd() % 4; a[2] = rnd() % 2 ? 0x46 : (u32)rnd();
            check(0x4961D0, Ammo_Check, "Ammo_Check", 1, a + 1, 0xFFFFFFFF);
            check(0x4962B0, Ammo_Use, "Ammo_Use", 1, a + 1, 0);
            check(0x478AA0, Flash_Alloc, "Flash_Alloc", 0, a, 0xFFFFFFFF);
            check(0x478AF0, Flash_AllocObj, "Flash_AllocObj", 1, a, 0xFFFFFFFF);
            { u32 b[3] = {rnd() % 3 ? o : 0, a[1], a[2]};
              check(0x478A60, Flash_Start, "Flash_Start", 2, b, 0xFFFFFFFF);
              check(0x478B40, Flash_StartFor, "Flash_StartFor", 3, b, 0xFFFFFFFF); }
        }
        player_setup_obj(); o = O_OBJ; do anim_setup_at(o); while (U32(o + 0x68) < 15);
        world_setup(); U32(0x662514) = 15 + rnd() % 2;   /* the player's model: feet are parts 11/14 */   /* Stage_Load restarts the animation; feet are parts 11/14 */
        U32(LOG_BASE) = 0; U32(0x87FD60) = o + 0x1C0;
        U16(G() + 0x10B4) = (u16)((rnd() % 8) << 8 | rnd() % 13);
        for (u32 k = 0x840; k < 0x848; k += 2) U16(G() + k) = (u16)cl_coord(0, 3000);
        a[0] = o;
        check(0x47D2F0, Player_Init, "Player_Init", 1, a, 0);
    }
    g_ign_objpads = 0;
    player_tables_stub(0);
    stubs_install(0);
}

/* ---- file loading: a fake file system behind the PC file functions --------- */
#define FK_POS   0x6DD800u      /* read position (in compared memory, so both runs agree) */
#define FK_HEAP  0x6DD804u      /* bump pointer of the fake malloc */
#define FK_BASE  0x4F0000u      /* fake heap 0x4F0000..0x5A0000 */
static u8 *g_fake; static u32 g_fake_len; static s32 g_fake_fd; static u32 g_synth_types;

static int g_fk_by_name;          /* save scan: whether a file exists depends on its name */
static u32 __cdecl fk_open(u32 path, u32 a, u32 b)
{
    u32 v[5] = {path, a, b}; rec_common(STUB_FILE + 0, v); U32(FK_POS) = 0;
    if (g_fk_by_name) { u32 h = 0; for (u32 k = 0; U8(path + k); k++) h = h * 31 + U8(path + k); return h % 3 ? (u32)-1 : 3; }
    return (u32)g_fake_fd;
}
static u32 __cdecl fk_seekset(u32 fd, u32 off) { u32 v[5] = {fd, off}; rec_common(STUB_LOAD + 8, v); U32(FK_POS) = off; return 0; }
static u32 __cdecl fk_read(u32 fd, u32 buf, u32 n)
{
    u32 v[5] = {fd, buf, n}; rec_common(STUB_FILE + 1, v);
    u32 pos = U32(FK_POS), k = pos < g_fake_len ? g_fake_len - pos : 0;
    if (k > n) k = n;
    memcpy(MEM(buf), g_fake + pos, k);
    U32(FK_POS) = pos + k;
    return k;
}
static u32 __cdecl fk_size(u32 fd) { u32 v[5] = {fd}; rec_common(STUB_FILE + 2, v); return g_fake_len; }
static u32 __cdecl fk_seek(u32 fd, u32 off, u32 w)
{ u32 v[5] = {fd, off, w}; rec_common(STUB_FILE + 3, v); U32(FK_POS) = (w == 1 ? U32(FK_POS) : 0) + off; return 0; }
static u32 __cdecl fk_close(u32 fd) { u32 v[5] = {fd}; rec_common(STUB_FILE + 4, v); return 0; }
static u32 __cdecl fk_malloc(u32 n)
{
    u32 v[5] = {n}; rec_common(STUB_FILE + 5, v);
    u32 p = U32(FK_HEAP); U32(FK_HEAP) = (p + n + 15) & ~15u;
    return p;
}
static u32 __cdecl fk_free(u32 p) { u32 v[5] = {p}; rec_common(STUB_FILE + 6, v); return 0; }
static u32 __cdecl fk_sprintf(u32 buf, u32 fmt, u32 name, u32 b, u32 c)   /* (b, c: the timer's minutes, seconds, frames) */
{
    char t[64];
    const char *f = (const char *)MEM(fmt);
    if (strstr(f, "%s")) snprintf(t, sizeof t, f, (char *)MEM(name));
    else snprintf(t, sizeof t, f, (int)name, (int)b, (int)c);
    memcpy(MEM(buf), t, strlen(t) + 1);
    return 0;
}

static int load_host(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    free(g_fake); g_fake = malloc(n + 1); g_fake_len = (u32)n;
    int ok = fread(g_fake, 1, n, f) == (size_t)n;
    fclose(f);
    return ok;
}

/* ---- zones (navigation graph) ----------------------------------------------------- */
#define ZG 0x6DC800u            /* zone graph G+0xA50 */

/* a connected graph (a random tree plus extra edges), zone points around
 * the objects, edge lengths per neighbour from the highest index down */
static void zones_setup(void)
{
    u32 n = rnd() % 8 ? 2 + rnd() % 15 : 1, adj[16] = {0};
    for (u32 i = 1; i < n; i++) { u32 j = rnd() % i; adj[i] |= 1u << j; adj[j] |= 1u << i; }
    for (u32 k = rnd() % (2 * n); k--; ) {
        u32 i = rnd() % n, j = rnd() % n;
        if (i != j) { adj[i] |= 1u << j; adj[j] |= 1u << i; }
    }
    U16(ZG) = (u16)n; U16(ZG + 2) = (u16)rnd();
    gptr len = ZG + 4 + 16 * 0x14;
    for (u32 i = 0; i < n; i++) {
        gptr r = ZG + 4 + i * 0x14;
        U32(r) = rnd(); U32(r + 4) = adj[i];
        S16(r + 8) = cl_coord(0, 1500); S16(r + 0xA) = rnd() % 3 ? cl_coord(0, 300) : cl_coord(-2000, 1800);
        S16(r + 0xC) = cl_coord(0, 1500);
        if (i && rnd() % 4 == 0) { U16(r + 8) = U16(r + 8 - 0x14); U16(r + 0xC) = U16(r + 0xC - 0x14); }   /* equal distances */
        U16(r + 0xE) = rnd() % 3 ? (u16)(rnd() % 4 ? 0 : rnd() % 4) : 0x8000;
        U16(r + 0x10) = (u16)(len - ZG);
        U16(r + 0x12) = (u16)rnd();
        for (u32 j = n; j--; ) if (adj[i] >> j & 1) { U16(len) = rnd() % 3 ? rnd() % 2000 : rnd() % 3; len += 2; }
    }
    U32(G() + 0xA50) = ZG;
    for (u32 i = 0; i < 16; i++) {
        gptr o = O_OBJ + i * 0x260;
        U8(o + 0x184) = rnd() % 2 ? 0 : (u8)(rnd() & 0x34);
        U32(o + 0x188) = U32(U32(o + 0x64) + 0x54) ? rnd() % 2 : 0;
        U16(o + 0x5A) = rnd() % 3 ? 0 : (u16)(rnd() % 4);
        U16(o + 0x138) = (u16)(rnd() % n);
        S32(o + 0x6C) = 0x800 + rnd() % 0x1000;
        if (rnd() % 8 == 0) U32(o + 0x50) |= 0x20000;
    }
    U32(G() + 0x940) = O_OBJ;
    U32(G() + 0xB50) = rnd() % 3 ? 0 : O_OBJ + 0x260 * (rnd() % 16);
    if (rnd() % 4 == 0) { U8(G() + 0x1091) = 9; U8(G() + 0x1090) = 3; }
    else { U8(G() + 0x1091) = (u8)(rnd() % 9); U8(G() + 0x1090) = (u8)rnd(); }
}

static void test_zones(void)
{
    script_setup();
    stubs_install(1);
    g_ign_objpads = 1;
    for (int i = 0; i < iters; i++) {
        u32 a[10] = {0};
        coll_setup(); zones_setup(); U32(LOG_BASE) = 0;
        /* segments: random, sharing a point, parallel */
        for (int k = 0; k < 8; k++) S16(G_SCR + 2 * k) = cl_coord(0, 1500);
        if (rnd() % 4 == 0) U32(G_SCR + 8) = U32(G_SCR + 4 * (rnd() % 2));
        if (rnd() % 4 == 0) { S16(G_SCR + 12) = (s16)(S16(G_SCR + 8) + S16(G_SCR + 4) - S16(G_SCR));
                              S16(G_SCR + 14) = (s16)(S16(G_SCR + 10) + S16(G_SCR + 6) - S16(G_SCR + 2)); }
        if (rnd() % 8 == 0) for (int k = 0; k < 8; k++) S16(G_SCR + 2 * k) = (s16)rnd();
        a[0] = G_SCR; a[1] = G_SCR + 8;
        sprintf(g_what, "seg %d %d %d %d / %d %d %d %d", S16(G_SCR), S16(G_SCR + 2), S16(G_SCR + 4), S16(G_SCR + 6),
                S16(G_SCR + 8), S16(G_SCR + 10), S16(G_SCR + 12), S16(G_SCR + 14));
        check(0x487160, Wall_Cross, "Wall_Cross", 2, a, 0xFFFFFFFF);
        check(0x47C6C0, Seg_Hit, "Seg_Hit", 2, a, 0xFFFFFFFF);
        check(0x487300, Dist2_XZ, "Dist2_XZ", 2, a, 0xFFFFFFFF);
        /* sight lines */
        gptr o = O_OBJ + 0x260 * (rnd() % 16), t = O_OBJ + 0x260 * (rnd() % 16);
        gptr pt = G_SCR + 0x20;
        S16(pt) = cl_coord(0, 1500); S16(pt + 2) = rnd() % 3 ? cl_coord(0, 300) : cl_coord(-2000, 2500);
        S16(pt + 4) = cl_coord(0, 1500);
        a[0] = o; a[1] = o + 0x40; a[2] = rnd() % 2 ? pt : t + 0x40; a[3] = rnd() % 2 ? 0 : rnd();
        sprintf(g_what, "zones %u walls %u who %X fl %X", U16(ZG), U8(CL_ROOM), U8(o + 0x184), U32(o + 0x50));
        check(0x486FB0, Line_Blocked, "Line_Blocked", 4, a, 0xFF);
        for (u32 k = 0; k < U8(CL_ROOM); k++) {          /* flag 4: not a wall for shots */
            gptr w = CL_ROOM + U32(CL_ROOM + 8 + 4 * k);
            if (rnd() % 4 == 0) U8(w + 3) |= 4;
            for (u32 j = 0; j < U8(w + 1); j++) if (rnd() % 4 == 0) U8(w + 9 + 4 * j) |= 4;
        }
        a[0] = o + 0x40; a[1] = rnd() % 2 ? pt : t + 0x40;
        check(0x47C550, Wall_Between, "Wall_Between", 2, a, 0xFFFFFFFF);
        a[0] = o;
        check(0x486D00, Zone_Find, "Zone_Find", 1, a, 0xFF);
        a[1] = 0x87FCE0;
        check(0x486EC0, Zone_Paths, "Zone_Paths", 2, a, 0);
        /* goals: the paths lead to t */
        Zone_Find(t); Zone_Paths(t, 0x87FCE0); U32(LOG_BASE) = 0;
        a[0] = o; a[1] = t;
        check(0x4869F0, Zone_Track, "Zone_Track", 2, a, 0);
        a[1] = pt;
        check(0x486C00, Zone_TrackPoint, "Zone_TrackPoint", 2, a, 0xFF);
        U32(0x87F940) = 1 + rnd() % 16; a[0] = t;
        check(0x486870, Zone_Update, "Zone_Update", 1, a, 0xFF);
    }
    g_ign_objpads = 0;
    stubs_install(0);
}

static void test_files(const char *dir)
{
    void *impl[8] = {fk_open, fk_read, fk_size, fk_seek, fk_close, fk_malloc, fk_free, fk_sprintf};
    for (int i = 0; i < 8; i++) g_stubs[STUB_FILE + i].impl = impl[i];
    stubs_install(1);
    memset(MEM(0x87FB80), 0, 0xA0);                 /* no sound banks (8) or streams loaded (left by the tests) */
    static char names[400][MAX_PATH]; int n = 0;
    char pat[MAX_PATH]; snprintf(pat, sizeof pat, "%s\\*.DAT", dir);
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {        /* containers whose records stay inside .psxseg */
            char path[MAX_PATH]; snprintf(path, sizeof path, "%s\\%s", dir, fd.cFileName);
            FILE *f = fopen(path, "rb"); u8 hdr[0x800]; int ok = 0;
            if (f) { ok = fread(hdr, 1, 0x800, f) == 0x800; fclose(f); }
            for (int r = 0; ok && r < 64; r++) {
                u32 t = *(u32 *)(hdr + 32 * r), sz = *(u32 *)(hdr + 32 * r + 4), dst = *(u32 *)(hdr + 32 * r + 8);
                if (!memcmp(hdr + 32 * r, "dummy", 5) || (!t && !sz)) continue;
                if (t > 7 || sz > 0x80000 || ((t == 0 || t == 5) && (dst < 0x4E8000 || dst + (t ? 0 : sz) > 0x703000)) ||
                    (t == 3 && dst > 7) || (t == 4 && dst > 3)) ok = 0;
            }
            if (ok && n < 400) strcpy(names[n++], path);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    for (int i = 0; i < iters; i++) {
        /* requests, the idle tick and its random numbers */
        U16(0x87FFEA) = rnd() % 3 == 0; U16(0x87FFEE) = (u16)rnd(); U16(0x87DE10) = (u16)rnd();
        if (rnd() % 8 == 0) U16(0x87DE10) = 0;
        u32 a[2] = {rnd() % 676, 0x6D8000};
        check(0x403E50, File_RequestLoad, "File_RequestLoad", 1, a, 0xFF);
        U16(0x87FFEA) = rnd() % 3 == 0;
        check(0x403E90, File_RequestRaw, "File_RequestRaw", 2, a, 0xFF);
        check(0x48F750, Rand8, "Rand8", 0, NULL, 0xFF);
        U16(0x87FFEA) = 0;
        check(0x403E40, File_Tick, "File_Tick", 0, NULL, 0);
        if (!n || i % 4) continue;
        /* whole files: a container (flag 1) or one raw record */
        int raw = rnd() % 4 == 0, synth = !raw && rnd() % 3 == 0;
        if (synth) {        /* every record type, small sizes */
            u32 nrec = 1 + rnd() % 8, len = 0x800;
            u32 sizes[8], types[8];
            for (u32 r = 0; r < nrec; r++) {
                types[r] = rnd() % 9;
                sizes[r] = types[r] == 5 ? 0x20 + rnd() % 0x3E0 : 0x20 + rnd() % 0x1800;  /* LZSS unpacks to ~9x */
                len += (sizes[r] + 0x7FF) & ~0x7FFu;
            }
            free(g_fake); g_fake = calloc(1, len); g_fake_len = len;
            for (u32 k = 0x800; k < len; k++) g_fake[k] = (u8)rnd();
            for (u32 r = 0; r < nrec; r++) {
                u32 *h = (u32 *)(g_fake + 32 * r), t = types[r];
                h[0] = t == 8 ? 0x6D6D7564 : t; h[1] = sizes[r];
                h[2] = t == 0 ? 0x6D8000 : t == 5 ? 0x6C0000 : t == 3 ? rnd() % 5 : t == 4 ? rnd() % 4 : rnd();
                h[3] = rnd();
                if (t == 1 || t == 6) {
                    u16 *q = (u16 *)(h + 2);
                    q[0] = rnd() % 0x400; q[1] = rnd() % 4 == 0 ? 0x280 : rnd() % 0x200;
                    q[2] = 0x20 << (rnd() % 3); q[3] = 0x20 << (rnd() % 3);
                    if (t == 1 && rnd() % 4 == 0) { q[0] = 0x300; q[1] = 0; q[2] = 0x80; q[3] = 0x100; }
                }
            }
            U16(0x87FFEA) = 0; U16(0x87FFEE) = 0; File_RequestLoad(rnd() % 676);
            sprintf(g_what, "synthetic %u records", nrec);
            for (u32 r = 0; r < nrec; r++) g_synth_types |= 1u << types[r];
        } else if (raw) {
            g_fake_len = 0x100 + rnd() % 0x3000; free(g_fake); g_fake = malloc(g_fake_len);
            for (u32 k = 0; k < g_fake_len; k++) g_fake[k] = (u8)rnd();
            U16(0x87FFEA) = 0; File_RequestRaw(rnd() % 676, 0x6D8000);
        } else {
            const char *f = names[rnd() % n];
            if (!load_host(f)) continue;
            U16(0x87FFEA) = 0; U16(0x87FFEE) = 0; File_RequestLoad(rnd() % 676);
            strcpy(g_what, f);
        }
        g_fake_fd = rnd() % 16 ? 3 : -1;
        U32(FK_POS) = 0; U32(FK_HEAP) = FK_BASE; U32(LOG_BASE) = 0;
        U32(0x746CA0) = rnd() % 2 ? 0x6D9000 : 0; U32(0x87F944) = rnd() % 2 ? 0x6D9000 : 0;
        g_stubs[STUB_FILE + 12].ret = rnd(); g_stubs[STUB_FILE + 13].ret = rnd();
        if (raw) sprintf(g_what, "raw %u bytes", g_fake_len);
        check(0x404290, File_Process, "File_Process", 0, NULL, 0);
    }
    stubs_install(0);
    printf("synthetic record types seen: %03X\n", g_synth_types);
}

/* ---- loading and room set-up ------------------------------------------------ */
static void test_room(void)
{
    script_setup();
    stubs_install(1);
    g_ign_objpads = 1;
    for (int i = 0; i < iters; i++) {
        gptr g = G();
        U32(LOG_BASE) = 0;
        for (u32 k = 0x740; k < 0x7D0; k += 4) U32(g + k) = rnd();          /* fields the set-up clears */
        for (u32 k = 0x87FC40; k < 0x87FCD0; k += 4) U32(k) = rnd();
        U16(0x87FFEA) = rnd() % 3 == 0; U16(0x87FFEE) = (u16)rnd();
        check(0x403A20, File_IsBusy, "File_IsBusy", 0, NULL, 0xFFFFFFFF);
        /* models */
        u32 types[] = {0, 1, 2, 3, 6, 7, 8, 13, 15, 16, 0x1C, 0x1D, 0x20, 0x23};
        u32 a[10] = {types[rnd() % 14], rnd() % 4 ? rnd() % 16 : rnd() % 3 ? 0x40000000 : rnd()};
        U16(g + 0x680) = rnd() % 2 ? (u16)a[0] : rnd() % 16; U16(g + 0x682) = rnd() % 2 ? (u16)a[1] : rnd() % 12;
        S16(g + 0x1388) = rnd() % 4; S16(g + 0x138A) = rnd() % 3 == 0;
        Flag_Set(2, 0x16, rnd() & 1);
        sprintf(g_what, "type %u variant %X (loaded %u/%u)", a[0], a[1], U16(g + 0x680), U16(g + 0x682));
        check(0x482070, Enemy_LoadModel, "Enemy_LoadModel", 2, a, 0);
        /* stage */
        anim_setup(); world_setup(); U32(LOG_BASE) = 0;
        U32(g + 0x940) = AN_OBJ; U32(0x87F940) = 1;      /* the only object (Obj_PickGroup counts them) */
        U8(g + 0x108F) = rnd() % 13;                  /* 13 CORE files */
        U32(0x662514) = U32(AN_OBJ + 0x68);           /* the player's model: as many parts as its animation's frames (else a stride off: durations of 0) */
        for (u32 i = 0; i < U32(0x662514); i++) { gptr r = 0x66251C + i * 0x14; S8(r + 6) = (s8)(i == 0 || rnd() % 3 == 0 ? -1 : (s32)(rnd() % i)); }
        U16(g + 0x67A) = rnd() % 2 ? 0x16 + U8(g + 0x108F) : 0; U16(g + 0x674) = rnd() % 3 ? (u16)rnd() : 0x1FF;
        U16(g + 0x676) = rnd() % 2 ? (u16)rnd() : 0x214; Flag_Set(9, 0x33, rnd() & 1);
        a[0] = rnd() % 6; a[1] = rnd() % 4;
        if (rnd() % 4 == 0) U32(AN_OBJ + 0x64) = 0;
        sprintf(g_what, "stage %u w %u s %u", U8(g + 0x108F), a[0], a[1]);
        check(0x4822D0, Stage_Load, "Stage_Load", 2, a, 0);
        /* room block header and object arrays */
        gptr hdr = 0x6DB000, sh = 0x6DB100;
        for (int k = 0; k < 0x30; k += 4) U32(hdr + k) = 0x600000 + rnd() % 0x10000;
        U32(hdr + 0x14) = sh; U32(hdr + 0x2C) = 0x690000;
        for (int k = 0; k < 0x1C; k++) U8(sh + k) = (u8)(rnd() % 12);
        a[0] = hdr;
        check(0x481E10, Room_SetPointers, "Room_SetPointers", 1, a, 0);
        check(0x481EC0, Room_Reset, "Room_Reset", 0, NULL, 0);
        U32(g + 0xA44) = 0x6DB200; U32(0x6DB200) = rnd() % 16;
        check(0x4034D0, CamZones_Init, "CamZones_Init", 0, NULL, 0);
        check(0x48AFD0, Aot_Reset, "Aot_Reset", 0, NULL, 0);
        /* script start and the per-frame modes; every routine just returns */
        script_setup();
        for (int r = 0; r < 16; r++) { U8(S_BASE + 0x40 + 0x80 * r) = 0x04; U8(S_BASE + 0x41 + 0x80 * r) = 0; }
        U32(g + G_SCRIPT) = S_BASE - 0x1C; U32(S_BASE - 0x1C + 0xC) = 0x2000 + 4 * (rnd() % 16);
        U32(0x87DC00) = rnd(); Flag_Set(2, 6, rnd() % 3 == 0); U32(0x87DE14) = rnd() % 8;
        U32(g + 0x948) = O_K3;
        for (int k = 0; k < 8; k++) { U8(O_K3 + k * 0x34) = (u8)rnd(); U8(O_K3 + k * 0x34 + 1) = rnd() % 3; }
        check(0x48BB60, Script_Init, "Script_Init", 0, NULL, 0);
        for (u32 m = 0; m < 3; m++) {
            script_setup();
            for (int r = 0; r < 16; r++) { U8(S_BASE + 0x40 + 0x80 * r) = 0x04; U8(S_BASE + 0x41 + 0x80 * r) = 0; }
            U8(0x87FC44) = (u8)m; U8(0x87FC47) = rnd() % 2 ? 0xFF : rnd() % 16; U32(0x87DC00) = rnd();
            sprintf(g_what, "script mode %u event %u", m, U8(0x87FC47));
            check(0x48BBF0, Script_Tick, "Script_Tick", 0, NULL, 0);
        }
    }
    g_ign_objpads = 0;
    stubs_install(0);
}

/* ---- root task, boot screens, save scan, task slots ---------------------------- */
static void test_root(void)
{
    void *impl[8] = {fk_open, fk_read, fk_size, fk_seek, fk_close, fk_malloc, fk_free, fk_sprintf};
    for (int i = 0; i < 8; i++) g_stubs[STUB_FILE + i].impl = impl[i];
    g_stubs[STUB_LOAD + 8].impl = fk_seekset;
    stubs_install(1);
    gptr r = 0x74D608;
    for (int i = 0; i < iters; i++) {
        U32(LOG_BASE) = 0;
        U16(0x87FFEA) = rnd() % 2;
        u32 a[2] = {rnd() % 16};
        check(0x4828D0, Screen_LoadImage, "Screen_LoadImage", 1, a, 0);
        /* the boot modes: random step, timer, skip flag, fade flag, buttons */
        static const struct { u32 addr; void *fn; const char *name; } M[] = {
            {0x46D740, Root_Boot, "Root_Boot"}, {0x46D7C0, Root_Logo, "Root_Logo"},
            {0x46D880, Root_Warning, "Root_Warning"}, {0x46D970, Root_ToTitle, "Root_ToTitle"},
        };
        for (int k = 0; k < 4; k++) {
            U32(LOG_BASE) = 0;
            U8(r) = (u8)k; U8(r + 1) = (u8)rnd(); U8(r + 2) = rnd() % 5; U8(r + 3) = rnd() % 4 == 0;
            U16(r + 4) = rnd() % 3 ? 1 + rnd() % 3 : (u16)rnd();
            Flag_Set(2, 6, rnd() & 1); U32(0x87DC12) = rnd() % 2 ? rnd() & 0x820 : 0;
            U16(0x87FFEA) = rnd() % 2;
            a[0] = r;
            sprintf(g_what, "mode %d step %u timer %u skip %u", k, U8(r + 2), U16(r + 4), U8(r + 3));
            check(M[k].addr, M[k].fn, M[k].name, 1, a, 0);
        }
        /* save scan: SAVExx.DAT exists for some names; the right size or not */
        if (i % 8 == 0) {
            g_fk_by_name = 1;
            g_fake_len = rnd() % 4 ? 0x5F4 : 0x600; free(g_fake); g_fake = malloc(g_fake_len);
            for (u32 k = 0; k < g_fake_len; k++) g_fake[k] = (u8)rnd();
            U16(0x87DC08) = (u16)rnd(); U16(0x87DC94) = (u16)rnd(); U16(0x87DDFC) = (u16)rnd(); U16(0x87DC00) = (u16)rnd();
            U32(G() + 0x664) = rnd(); U8(0x87DC90) = (u8)rnd();
            U32(FK_POS) = 0; U32(FK_HEAP) = FK_BASE; U32(LOG_BASE) = 0;
            check(0x403920, Save_ScanTask, "Save_ScanTask", 0, NULL, 0);
            g_fk_by_name = 0;
        }
        for (u32 k = 0x73C870; k < 0x73C8F0; k += 4) U32(k) = rnd();
        a[0] = rnd() % 4;
        check(0x4C2A0D, Task_Kill, "Task_Kill", 1, a, 0);
    }
    stubs_install(0);
}

/* ---- game start, opening, game task ----------------------------------------------- */
static void fake_room_block(void)
{
    gptr h = 0x5E0000, sc = 0x5E1000;
    memset(MEM(h), 0, 0x40);
    for (int k = 0; k < 0x30; k += 4) U32(h + k) = 0x5E2000 + 0x100 * (k / 4);
    U32(h + 0xC) = 0x5E2C00; U32(0x5E2C00) = rnd() % 4;          /* camera zones */
    U32(h + 0x14) = sc; U32(h + 0x2C) = 0x690000;
    memset(MEM(sc), 0, 0x200);
    for (int k = 2; k < 0xC; k++) U8(sc + k) = (u8)(1 + rnd() % 6);
    U8(sc + 0xB) = 10;                                            /* threads */
    U32(sc + 0xC) = 0x100;                                        /* globals after the routines */
    U32(sc + 0x1C) = 0x40; U8(sc + 0x1C + 0x40) = 0x04;           /* routine 0: return */
}

/* the sound table (0x6EED00) and four sound banks (0x87FBE0) as the room loads them */
static void snd_setup(void)
{
    for (u32 id = 0; id < 0x120; id++) {
        gptr t = 0x6EED00 + id * 8;
        for (int b = 0; b < 8; b++) U8(t + b) = (u8)rnd();
        U8(t + 1) = rnd() % 8 ? rnd() % 4 : 0xFF;
        U8(t + 2) = rnd() % 8; U8(t + 3) = (u8)((rnd() % 4) << 4 | (rnd() & 0xF));
    }
    for (u32 b = 0; b < 4; b++) {
        gptr bk = 0x6E9000 + b * 0x400, hd = 0x6EA000 + b * 0x800;     /* 8 groups of 16 sounds; 256 buffers */
        U32(0x87FBE0 + b * 8) = bk; U32(0x87FBE4 + b * 8) = hd;
        for (u32 q = 0; q < 0x400; q += 4) U32(bk + q) = rnd();
        for (u32 q = 0; q < 0x400; q += 8) if (rnd() % 4 == 0) U8(bk + q) = 0;
        for (u32 q = 0; q < 0x800; q += 4) U32(hd + q) = rnd() % 0x400;
    }
    gptr t = 0x6EC000;                          /* the room's sound areas (G+0xA64) */
    U32(G() + 0xA64) = t; U32(t) = rnd() % 4;
    for (u32 i = 0; i < 4; i++) {
        gptr r = t + 4 + i * 0x14;
        U8(r) = (u8)rnd(); U8(r + 1) = rnd() % 2; U8(r + 2) = rnd() % 3 ? 0 : (u8)(rnd() % 3); U8(r + 3) = rnd() % 4;
        s16 l = (s16)(rnd() % 0x4000 - 0x3000), b = (s16)(rnd() % 0x4000 - 0x3000), w = (s16)(0x800 + rnd() % 0x3000);
        s16 q[8] = {l, (s16)(b + w), (s16)(l + w), (s16)(b + w), (s16)(l + w), b, l, b};
        for (int c = 0; c < 8; c++) S16(r + 4 + 2 * c) = q[c];
    }
}

/* objects' part counts (+0x68) within their chains, where +0xFE bit 0 says to
   walk that many (the lights pick, Obj_PickLights) */
static void obj_part_counts(void)
{
    gptr o = U32(G() + 0x940);
    for (u32 i = 0; i < 16; i++, o += 0x260) {
        u32 n = 0;
        for (gptr q = U32(o + 0x64); q && n < 16; q = U32(q + 0x54)) n++;
        if (!n) { gptr q = O_PART + 0x800 + i * 0x80; U32(o + 0x64) = q;   /* (past the player's chain of 12: camera_state) */ U32(q + 0x54) = 0; U32(q + 0x58) = o; n = 1; }   /* (objects in use have parts) */
        if (U8(o + 0xFE) & 1) U32(o + 0x68) = 1;     /* (the frame may unlink its later parts) */
    }
    if (!S16(U32(G() + 0x940) + 0x10A)) S16(U32(G() + 0x940) + 0x10A) = 1;   /* the gauge divides by the maximum health */
    gptr ls = 0x6ED400;                             /* the camera's light set (G+0xA3C, 0x298 a camera) */
    U32(G() + 0xA3C) = ls - 8 - 0x298 * U8(G() + 0x7C4);
    for (u32 q = 0; q < 0x100; q += 4) U32(ls + q) = rnd();
    U8(ls + 6) = rnd() % 4; U8(ls + 7) = rnd() % 4; U8(ls + 0xB) = rnd() % 4;
}

static void game_state(void)
{
    gptr g = G();
    world_setup();
    snd_setup();
    U8(O_OBJ + 0x58) = 0;
    for (u32 k = 0x660; k < 0x680; k += 2) U16(g + k) = rnd() % 3 ? rnd() % 6 : (u16)rnd();
    U16(g + 0x664) = rnd() % 4;
    for (u32 k = 0x674; k < 0x684; k += 2) U16(g + k) = rnd() % 2 ? 0xFFFF : rnd() % 0x200;
    U8(g + 0x1091) = rnd() % 10; U8(g + 0x1090) = rnd() % 5;
    U8(g + 0x662) = rnd() % 2; U8(g + 0x926) = rnd() % 2; U8(g + 0x927) = rnd() % 4 == 0 ? 1 + rnd() % 2 : 0;
    U8(g + 0x108E) = rnd() % 3; U8(g + 0x108F) = rnd() % 4; U8(g + 0x850) = rnd() % 8; U8(g + 0x1094) = (u8)rnd();
    for (u32 k = 0x10AC; k < 0x10B4; k += 2) U16(g + k) = (u16)rnd();
    for (u32 k = 0x840; k < 0x848; k += 2) U16(g + k) = (u16)rnd();
    U8(g + 0x14F0) = rnd() % 3; U8(g + 0x14F1) = (u8)rnd(); U8(g + 0x14F2) = rnd() % 2;
    U16(0x87FBD4) = rnd() % 2 ? 0xFFFF : (u16)rnd(); U16(0x87FFEA) = rnd() % 2;
    U32(0x87DC94) = rnd() % 2 ? 0 : (rnd() & 0x901);
    gptr pl = O_OBJ; U32(g + 0x940) = pl; U16(pl + 0x174) = rnd() % 3 == 0; U8(pl + 0x54) = rnd() % 3 ? 1 : 0;
    u32 fl[][2] = {{2, 0x1B}, {2, 6}, {2, 0xF}, {1, 9}, {1, 0x10}, {1, 0xA}, {2, 0x25}, {2, 8}, {2, 0x11}, {2, 0x10}, {3, 9}, {2, 0x17}};
    for (unsigned k = 0; k < sizeof fl / sizeof fl[0]; k++) Flag_Set(fl[k][0], fl[k][1], rnd() % 5 == 0);
    U16(0x87DE10) = (u16)rnd();
    Flag_Set(2, 0x1D, 1);           /* Game_Frame's object part (the "frame" tests) skipped */
    U8(g + 0x84B) = U8(g + 0x84A);   /* and no camera load */
}

/* ---- doors (the room change) --------------------------------------------------------- */
static void type_tables_stub(int on);
#define T_DOOR  0x748A60u
#define T_DEST  0x6DDA00u       /* the door's destination record */
#define T_CAM   0x6DDA40u
static void test_doors(void)
{
    stubs_install(1);
    g_ign_objpads = 1; g_ign_door = 1;
    for (int i = 0; i < iters; i++) {
        gptr d = T_DOOR, g = G();
        game_state(); fake_room_block(); anim_setup();
        for (u32 k = 0; k < 0x24A0; k += 4) U32(d + k) = rnd();
        memset(MEM(d + 0x530), 0, 0x2438 - 0x530);          /* the scene's objects and parts start cleared (the task clears all) */
        U8(d + 0x2474) = rnd() % 3; U8(d + 0x2475) = rnd() % 6; U8(d + 0x2476) = rnd() % 4;
        U8(d + 0x2478) = rnd() % 4; U8(d + 0x2479) = rnd() % 10; U8(d + 0x2481) = rnd() % 20;
        u8 t = (u8)(rnd() % 4 ? rnd() % 0x5A : (u8[]){0x44, 0x58, 0x59}[rnd() % 3]);
        U8(d + 0x2480) = t; U8(d + 0x2492) = rnd() % 2;
        for (u32 q = 0; q < 0x40; q += 2) U16(0x6ED200 + q) = (u16)(rnd() % 4 ? rnd() % 0x60 : 0x4000);   /* the room's name */
        U16(0x6ED200 + 2 * (rnd() % 0x20)) = 0xF000; U16(0x6ED240) = 0xF000; U32(d + 0x2498) = 0x6ED200;
        for (u32 k = 0; k < 0xC; k += 2) U16(T_DEST + k) = (u16)rnd();
        U8(T_DEST + 8) = rnd() % 4; U8(T_DEST + 9) = rnd() % 10; U8(T_DEST + 0xA) = t; U8(T_DEST + 0xB) = rnd() % 20;
        U32(0x7593B4) = T_DEST;
        for (u32 k = 4; k <= 8; k += 4) {                     /* the type's door models: a few parts each */
            gptr m = U32(0x71F1F0 + t * 0x20 + k);
            if (m) { model_data(m); u32 n = 1 + rnd() % 4; if (U32(m + 0x14) > n) U32(m + 0x14) = n; }
        }
        for (u32 k = 0; k < 0x14; k += 2) U16(T_CAM + k) = (u16)(rnd() % 0x2000 - 0x1000);
        gptr pl = U32(g + 0x940);
        U32(pl + 0x60) = MODEL_DATA; U32(pl + 0x88) = 0x6CE000u;                       /* AN_TAB */
        for (u32 k = 0; k < 0x1E; k++) U32(d + 0xC50 + k * 0xCC + 0x50) = rnd() % 8 == 0;   /* the door's parts */
        U16(g + 0x67C) = rnd() % 2 ? U8(d + 0x2479) : rnd() % 10;
        U16(g + 0x678) = (u16)(rnd() % 3 ? rnd() : 0); U32(g + 0xC14) = rnd() % 2; U16(g + 0xC24) = rnd() % 0x28;
        U16(g + 0x11F8) = (u16)rnd(); U16(g + 0x11FC) = (u16)rnd();     /* the characters' saved health */
        U16(g + 0x138A) = rnd() % 3; Flag_Set(2, 0x14, rnd() % 2); Flag_Set(2, 6, rnd() % 4 == 0); Flag_Set(1, 3, rnd() % 2);
        stub_at(0x494DE0)->ret = rnd() % 0x100;
        u32 a[5] = {0};
        U32(LOG_BASE) = 0;
        sprintf(g_what, "door type %02X mode %u step %u/%u dest %u/%u busy %u", t, U8(d + 0x2474), U8(d + 0x2475),
                U8(d + 0x2476), U8(T_DEST + 9), U8(T_DEST + 8), U16(0x87FFEA));
        check(0x48B820, Door_Dest, "Door_Dest", 0, a, 0xFFFFFFFF);
        a[0] = rnd() % 10; a[1] = rnd() % 4;
        check(0x495C90, Room_Name, "Room_Name", 2, a, 0xFFFFFFFF);
        check(0x46B890, Lights_ListReset, "Lights_ListReset", 0, a, 0);
        a[0] = T_CAM; a[1] = T_CAM + 6; a[2] = T_CAM + 0x20; a[3] = T_CAM + 0x28;
        check(0x4038E0, Camera_Aim, "Camera_Aim", 4, a, 0);
        a[0] = d; a[1] = rnd() % 3; a[2] = T_CAM;
        check(0x41E6B0, Door_SetupPlayer, "Door_SetupPlayer", 3, a, 0);
        U32(g + 0x940) = d + 0x530; U32(g + 0x938) = d + 0xC50; U32(0x87DC0C) = 0x1E;
        for (u32 k = 0; k < 0x1E; k++) U32(d + 0xC50 + k * 0xCC + 0x50) = rnd() % 8 == 0;
        a[0] = d; a[1] = rnd() % 2; a[2] = MODEL_DATA; a[3] = rnd(); a[4] = rnd();
        check(0x41E790, Door_SetupModel, "Door_SetupModel", 5, a, 0);
        a[0] = T_CAM;
        check(0x41E820, Door_SetCamera, "Door_SetCamera", 1, a, 0);
        a[0] = d;
        check(0x41E8F0, Door_SetLight, "Door_SetLight", 1, a, 0);
        U32(g + 0x940) = pl;
        memset(MEM(d + 0x530), 0, 0x2438 - 0x530);
        check(0x41E310, Door_Init, "Door_Init", 1, a, 0);
        /* running: the door object animated, the type's handler and the steps recorders */
        anim_setup_at(d + 0x530);
        U32(0x87F940) = 1 + rnd() % 3; U32(g + 0x940) = d + 0x530;
        for (u32 k = 1; k < 3; k++) {
            gptr o = d + 0x530 + k * 0x260, p = 0x6DDB00u + k * 0x80;
            U32(o + 0x64) = p; U32(p + 0x54) = 0; U32(p + 0x58) = o;
        }
        u32 saved[4] = {U32(0x71F1FC + t * 0x20), U32(0x71FFD0), U32(0x71FFD4), U32(0x71FFD8)};
        U32(0x71F1FC + t * 0x20) = U32(0x71FFD0) = U32(0x71FFD4) = U32(0x71FFD8) = (u32)(uintptr_t)g_recs[STUB_TYPE];
        U8(d + 0x2475) = rnd() % 3;
        U32(0x87DC98) = 0x6D0000; U32(0x87DE0C) = rnd() & 1;   /* the room name's prims */
        check(0x41E9C0, Door_Run, "Door_Run", 1, a, 0);
        U32(0x71F1FC + t * 0x20) = saved[0]; U32(0x71FFD0) = saved[1]; U32(0x71FFD4) = saved[2]; U32(0x71FFD8) = saved[3];
        type_tables_stub(1);
        U8(d + 0x2476) = rnd() % 4;
        check(0x41EAD0, Door_AreaSwitch, "Door_AreaSwitch", 1, a, 0);
        type_tables_stub(0);
        U8(d + 0x2476) = rnd() % 3; U8(g + 0x1091) = rnd() % 10; U8(g + 0x1090) = rnd() % 4;
        check(0x41EBB0, Room_LoadTask, "Room_LoadTask", 1, a, 0);
        U8(d + 0x2476) = rnd() % 4;
        check(0x41EC50, Door_Music, "Door_Music", 1, a, 0);
        check(0x41ED10, Door_End, "Door_End", 1, a, 0);
        /* the door types' handlers: the stand-in near each threshold */
        static const struct { u32 addr; void *fn; const char *name; s32 arg[3]; u32 n; } H[] = {
            {0x41CB70, Door_Common, "Door_Common", {0}, 0},
            {0x41CA70, Door_SwingAMove, "Door_SwingAMove", {0}, 0}, {0x41CA50, Door_SwingA, "Door_SwingA", {0}, 0},
            {0x41CC60, Door_SwingBMove, "Door_SwingBMove", {0}, 0}, {0x41CC40, Door_SwingB, "Door_SwingB", {0}, 0},
            {0x41D090, Door_Shutter, "Door_Shutter", {0}, 1},
            {0x41D080, Door_ShutterSlow, "Door_ShutterSlow", {0}, 0}, {0x41D210, Door_ShutterFast, "Door_ShutterFast", {0}, 0},
            {0x41D230, Door_Slide, "Door_Slide", {0}, 1},
            {0x41D220, Door_SlideL, "Door_SlideL", {0}, 0}, {0x41D420, Door_SlideR, "Door_SlideR", {0}, 0},
            {0x41D460, Door_Common2, "Door_Common2", {0}, 0},
            {0x41D4F0, Door_Double, "Door_Double", {0}, 3},
            {0x41D430, Door_DoubleA, "Door_DoubleA", {0}, 0}, {0x41D670, Door_DoubleB, "Door_DoubleB", {0}, 0},
            {0x41D6A0, Door_DoubleC, "Door_DoubleC", {0}, 0},
            {0x41D6F0, Door_Double2, "Door_Double2", {0}, 1},
            {0x41D6D0, Door_Double2A, "Door_Double2A", {0}, 0}, {0x41D820, Door_Double2B, "Door_Double2B", {0}, 0},
            {0x41D840, Door_Double2C, "Door_Double2C", {0}, 0},
            {0x41CEA0, Door_CamTarget, "Door_CamTarget", {0}, 2}, {0x41D990, Door_CamEye, "Door_CamEye", {0}, 2},
            {0x41CD60, Door_LiftDown, "Door_LiftDown", {0}, 0}, {0x41CF20, Door_LiftUp, "Door_LiftUp", {0}, 0},
            {0x41D860, Door_Drop, "Door_Drop", {0}, 0},
            {0x41DA10, Door_WaitHidden, "Door_WaitHidden", {0}, 0}, {0x41DB90, Door_Wait, "Door_Wait", {0}, 0},
            {0x41DAF0, Door_Message, "Door_Message", {0}, 0}, {0x41DC70, Door_Walk, "Door_Walk", {0}, 0},
            {0x41DDF0, Door_StairsDown, "Door_StairsDown", {0}, 0}, {0x41E050, Door_StairsUp, "Door_StairsUp", {0}, 0},
            {0x41F040, Door_ScoreBox, "Door_ScoreBox", {0}, 0}, {0x41ED70, Door_Score, "Door_Score", {0}, 0},
        };
        for (unsigned kk = 0; kk < 4 * sizeof H / sizeof H[0]; kk++) {
            unsigned k = kk % (sizeof H / sizeof H[0]);
            gptr o = d + 0x530;
            U32(g + 0x940) = o;
            anim_setup_at(o);
            U32(d + 0x249C) = AN_TAB;
            for (u32 j = 1; j < 3; j++) {
                gptr q = o + j * 0x260, p = 0x6DDB00u + j * 0x80;
                U32(q + 0x64) = p; U32(p + 0x54) = 0; U32(p + 0x58) = q;
                /* thresholds, hit exactly after a step (or one off) */
                static const s16 edge[] = {-2000, -1000, -0x578, -0x3E8, -0x320, -0x258, -0x190, 0, 0x190, 0x258, 0x320, 0x3E8, 0x578, 1000, 2000};
                static const s16 dlt[] = {0, 1, -1, 0x14, -0x14, 0x19, -0x19, 0x1E, -0x1E, 0x32, -0x32, 0x50, -0x50};
                U16(q + 0x40) = rnd() % 4 ? (u16)(edge[rnd() % 15] + dlt[rnd() % 13] + (rnd() % 3 ? 0 : (s16)(rnd() % 3) - 1)) : (u16)(rnd() % 3000 - 1500);
                U16(q + 0x42) = rnd() % 4 ? (u16)(edge[rnd() % 15] + dlt[rnd() % 13] + (rnd() % 3 ? 0 : (s16)(rnd() % 3) - 1)) : (u16)(rnd() % 5000 - 2500);
                U16(q + 0x48) = (u16)rnd(); U16(q + 0x4A) = (u16)rnd(); U16(q + 0x4C) = (u16)rnd();
            }
            U16(o + 0x92) = rnd() % 0x22;
            static const s16 zed[] = {0, 0x90, 0x300};
            U16(o + 0x44) = rnd() % 2 ? (u16)(zed[rnd() % 3] + (s16)(rnd() % 3) - 1) : (u16)(rnd() % 0x400 - 0x200);
            U8(d + 0x2477) = rnd() % 10;
            U8(d + 0x247D) = rnd() % 2 ? 0x11 : (u8)rnd();
            U8(d + 0x247E) = rnd() % 2; U8(d + 0x2474) = rnd() % 3;
            U8(g + 0x108F) = rnd() % 4;
            Flag_Set(2, 6, rnd() % 3 == 0); Flag_Set(1, 3, rnd() % 2);
            static const s32 st[] = {-0x50, -0x32, -0x19, -0x14, 0x14, 0x19, 0x1E, 0x32, 0x50};
            a[0] = d;
            a[1] = H[k].n == 1 && k == 8 ? rnd() % 2 * (rnd() % 2 ? 1 : 0x100) : (u32)st[rnd() % 9];
            a[2] = rnd() % 2 ? 0x3E8 : 0x258; a[3] = rnd() % 2 ? 0x320 : 0x190;
            /* the lifts, stairs, camera drop, waits, walk and score panel */
            gptr c = g + 0x778;
            static const s16 ty[] = {-8000, 0x1F40, 0xCE0, -2750, -2000};
            static const s16 tdy[] = {0, 0x30, -0x30, 0x12, -0x12, 0x13, -0x13};
            if (rnd() % 2) U16(o + 0x42) = (u16)(ty[rnd() % 5] + tdy[rnd() % 7] + (s16)(rnd() % 3) - 1);
            for (u32 j = 0x20; j < 0x40; j += 2) U16(c + j) = (u16)(rnd() % 0x2000 - 0x1000);
            if (rnd() % 3 == 0) U16(c + 0x3A) = (u16)(-30000 + 0x80 + (s16)(rnd() % 3) - 1);
            U8(d + 0x247C) = rnd() % 2 ? 0x3A + rnd() % 3 : (u8)rnd();
            U8(d + 0x247F) = rnd() % 2;
            U16(d + 0x2478) = rnd() % 2 ? 1 : (u16)rnd();
            U16(o + 0xAA) = (u16)(rnd() % 8 * 2); U32(o + 0x9C) = rnd() % 4 ? AN_EVT : 0;
            if (rnd() % 2) U16(AN_EVT + 4 + 4 * (rnd() % 4)) = (u16)((S16(o + 0xAA) + U16(o + 0x92) * 2) / 2);
            U32(o + 0x60) = MODEL_DATA; U32(g + 0x938) = d + 0xC50; U32(0x87DC0C) = 0x1E;
            U32(0x87DC98) = 0x6D0000; U32(0x87F94C) = 0x6CF800;          /* the score panel's sprites */
            for (u32 j = 0; j < 0x1E; j++) U32(d + 0xC50 + j * 0xCC + 0x50) = 0;
            U16(0x87FFEA) = rnd() % 2;
            Flag_Set(2, 0x1F, rnd() % 2);
            U8(d + 0x2491) = (u8)(rnd() % 8 - 1);
            U16(d + 0x2488) = rnd() % 2 ? (u16)(rnd() % 3 ? 0x27 + rnd() % 3 : 9 + rnd() % 3) : (u16)rnd();
            U32(g + 0xC14) = rnd() % 4 ? rnd() % 100000 : rnd();
            U32(d + 0x248C) = rnd() % 2 ? U32(g + 0xC14) - rnd() % 0x2000 : rnd();
            U16(g + 0xC24) = rnd() % 3 ? rnd() % 3 : rnd() % 0x400; U16(d + 0x248A) = rnd() % 2 ? U16(g + 0xC24) - rnd() % 0x50 : (u16)rnd();
            U16(g + 0xC20) = rnd() % 3 ? (u16)(rnd() % 0x40) : rnd() % 2 ? 0 : (u16)rnd();
            U32(d + 0x2484) = rnd() % 2 ? U16(g + 0xC20) - rnd() % 4 : rnd();
            U16(g + 0xC22) = rnd() % 0x11;
            U32(g + 0x11F4) = rnd() % 2 ? 9999999 - rnd() % 0x80 : rnd(); U32(g + 0x120C) = rnd();
            if (rnd() % 3 == 0) {                                   /* exact thresholds of the lifts, stairs, panel */
                const char *nm = H[k].name; s16 j = (s16)(rnd() % 3) - 1;
                if (!strcmp(nm, "Door_LiftUp")) { U8(d + 0x2477) = 2; U16(o + 0x42) = (u16)(0x1F40 - 0x30 + j); }
                if (!strcmp(nm, "Door_LiftDown")) { U8(d + 0x2477) = 2; U16(o + 0x42) = (u16)(-8000 + 0x30 + j); }
                if (!strcmp(nm, "Door_StairsUp")) { U8(d + 0x2477) = 2; U16(o + 0x42) = (u16)(-2000 + j); }
                if (!strcmp(nm, "Door_StairsDown")) { U8(d + 0x2477) = 2; U16(o + 0x42) = (u16)(-2750 + j); }
                if (!strcmp(nm, "Door_Message")) U16(d + 0x2478) = (u16[]){1, 0x101, 0x201}[rnd() % 3];
                if (!strcmp(nm, "Door_Score")) {
                    u32 p = U32(g + 0xC14);
                    U32(d + 0x248C) = p - (p / 15 + 1) + j;
                    u16 n = U16(g + 0xC24) = 2 + rnd() % 0x200;
                    U16(d + 0x248A) = (u16)(n - (n / 15 + 1) + j);
                    u16 t = U16(g + 0xC22) = rnd() % 0xF;
                    U16(g + 0xC20) = (u16)(U8(0x71FF68 + t * 6 + 4 + rnd() % 2) + j);
                    if (!U16(g + 0xC20)) U16(g + 0xC20) = 1;
                    U32(d + 0x2484) = U16(g + 0xC20) - (U16(g + 0xC20) / 15 + 1) + j;
                }
            }
            if (H[k].n == 2) { a[0] = rnd() % 0x2000 - 0x1000; a[1] = rnd() % 0x2000 - 0x1000; a[2] = rnd() % 0x2000 - 0x1000; }
            if (k >= 12 && k <= 15 && rnd() % 4 == 0) {           /* the double doors closing onto lim2 */
                static const s32 ds[4][2] = {{0, 0}, {-0x32, 0x320}, {0x32, 0x320}, {0x1E, 0x190}};
                s32 step = k == 12 ? (s32)a[1] : ds[k - 12][0], l2 = k == 12 ? (s32)(s16)a[3] : ds[k - 12][1];
                U8(d + 0x2477) = 6;
                U16(o + 0x2A0) = (u16)((rnd() % 2 ? l2 : -l2) - step + (s32)(rnd() % 3) - 1);
            }
            U32(LOG_BASE) = 0;
            sprintf(g_what, "door handler %s state %u frame %u", H[k].name, U8(d + 0x2477), U16(o + 0x92));
            check(H[k].addr, H[k].fn, H[k].name, 1 + H[k].n, a, 0);
        }
    }
    g_ign_objpads = 0; g_ign_door = 0;
    stubs_install(0);
}

/* ---- title screen ---------------------------------------------------------------- */
static void title_state(void)
{
    gptr g = G(), t = 0x759EF0;
    U32(g + 0x93C) = O_EFF;                         /* object effects (Stage_Load frees them) */
    U32(g + 0x940) = O_OBJ; U8(O_OBJ + 0x58) = 0;  /* the player: Player_Setup calls its type handler */
    U8(g + 0x108F) = rnd() % 4;                     /* a playable character (types with handlers) */
    world_setup();
    for (u32 k = 0; k < 0x40; k += 4) U32(t + k) = rnd();
    U8(t + 2) = rnd() % 3; U8(t + 0x2A) = 1 + rnd() % 2; U8(t + 0x28) = rnd() % 2; U8(t + 0x2B) = rnd() % 2;
    U8(t + 3) = rnd() % 4;
    U16(t) = rnd() % 3 ? 1 + rnd() % 3 : rnd() % 2 ? 0x13 + rnd() % 3 : (u16)rnd();
    for (int i = 0; i < 3; i++) U32(t + 8 + 12 * i) = 0x625800 + 8 * (rnd() % 0x30);
    U32(t + 0x2C) = 0x65F600 + 0x10 * (rnd() % 16);
    U16(g + 0x76C) = rnd() % 6; U16(g + 0x76E) = rnd() % 4; U16(g + 0x770) = rnd() % 8; U16(g + 0x772) = rnd() % 3;
    u32 btns[] = {0, 0x40, 0x1000, 0x4000, 0x20, 0x800, 0x820, 0x5040};
    U32(0x87DC00) = btns[rnd() % 8]; U32(0x87DC12) = rnd() % 3 ? 0 : rnd() % 2 ? 0x100 : rnd();
    Flag_Set(2, 6, rnd() % 3 == 0); Flag_Set(2, 0x17, rnd() % 3 == 0);
    U8(0x87DC90) = rnd() % 3; U8(g + 0x814) = rnd() % 2;
    U32(0x87DC98) = 0x6D0000; U32(0x87F94C) = 0x6CF800; U32(0x87DE0C) = rnd() & 1;
    U16(0x87FFEA) = rnd() % 2;
    Flag_Set(2, 0x1D, 1); U8(g + 0x84B) = U8(g + 0x84A);
    if (U16(g + 0x76E) == 3 || U16(g + 0x76C) == 5 || U8(t + 0x28)) { game_state(); fake_room_block(); }
    U16(0x87FFEA) = rnd() % 2;
}

/* the areas' object type tables (0x738DDC..): handlers -> recorder while the
   title and game task tests run (their set-up calls the player's type set-up) */
static u32 g_typetab_saved[0x340];
static void type_tables_stub(int on)
{
    for (u32 k = 0; k < 0x340; k++) {
        gptr a = 0x738DDC + 4 * k;
        if (on) {
            g_typetab_saved[k] = U32(a);
            if (U32(a) >= 0x401000 && U32(a) < 0x4B4000) U32(a) = (u32)(uintptr_t)g_recs[STUB_TYPE];
        } else U32(a) = g_typetab_saved[k];
    }
}

static void test_title(void)
{
    stubs_install(1);
    type_tables_stub(1);
    for (u32 a = 0x625800; a < 0x625C00; a += 4) U32(a) = rnd();
    for (u32 a = 0x625800; a < 0x625C00; a += 8) {
        U8(a + 4) = (u8)((rnd() % 3 ? rnd() % 4 : rnd()) | (rnd() % 4 == 0 ? 0x20 : 0)) & 0xE7;
        U8(a + 5) = rnd() % 4;                      /* frame time, 0 and 1 are special */
    }
    for (u32 a = 0x65F500; a < 0x65F800; a += 4) U32(a) = rnd();
    U8(0x65F508) = 1; U8(0x65F509) = 2;          /* the demo's game state: a real room (area 2, room 1) */
    static const struct { u32 addr; void *fn; const char *name; } F[] = {
        {0x4A5760, Title_Init, "Title_Init"}, {0x4A5790, Title_Run, "Title_Run"},
        {0x4A57B0, Title_Menu, "Title_Menu"}, {0x4A5B30, Title_Attract, "Title_Attract"},
        {0x4A5B50, Title_Movie, "Title_Movie"}, {0x4A5BF0, Title_Demo, "Title_Demo"},
        {0x4A5EC0, Title_NewGame, "Title_NewGame"}, {0x4A60F0, Title_Load, "Title_Load"},
        {0x4A6180, Title_Option, "Title_Option"},
    };
    for (int i = 0; i < iters; i++) {
        for (unsigned k = 0; k < sizeof F / sizeof F[0]; k++) {
            title_state(); U32(LOG_BASE) = 0;
            { u32 pb = U32(0x87DC98), ob = U32(0x87F94C); draw_safe(); U32(0x87DC98) = pb; U32(0x87F94C) = ob; }   /* the demo draws a frame */
            u32 a[1] = {0x759EF0};
            sprintf(g_what, "screen %u step %u/%u cursor %d btn %X", U16(G() + 0x76E), U16(G() + 0x770),
                    U16(G() + 0x772), S8(0x759EF2), U32(0x87DC00));
            check(F[k].addr, F[k].fn, F[k].name, 1, a, 0);
        }
        title_state(); U32(LOG_BASE) = 0;
        u32 b[4] = {0x759EF0, rnd() % 3, 0x625800 + 8 * (rnd() % 0x40)};
        check(0x4A6370, Title_SetAnim, "Title_SetAnim", 3, b, 0);
        U8(0x759EF0 + 4 + b[1] * 12 + 8) = rnd() % 3;
        check(0x4A63A0, Title_AnimStep, "Title_AnimStep", 2, b, 0xFFFFFFFF);
        u32 c[4] = {(u32)(rnd() % 400) - 50, (u32)(rnd() % 300) - 50, rnd(), 0x625800 + 8 * (rnd() % 0x40)};
        check(0x4A6210, Title_Draw, "Title_Draw", 4, c, 0);
    }
    type_tables_stub(0);
    stubs_install(0);
}


/* ---- the frame of play: object loops, trigger areas ---------------------------------- */
static void camera_state(void);
static void enemyset_state(void);
static void frame_state(void)
{
    gptr g = G();
    U32(0x87DCA0) = TYPE_TAB; U32(0x87DC8C) = TYPE_TAB; U32(0x87F320) = TYPE_TAB; U32(0x87F948) = TYPE_TAB;
    for (int k = 0; k < 256; k++) U32(TYPE_TAB + 4 * k) = (u32)(uintptr_t)g_recs[k & 1 ? STUB_AOT : STUB_TYPE];  /* which entry shows */
    U32(g + 0x940) = O_OBJ; U32(0x87F940) = 1 + rnd() % 8;
    for (u32 k = 0; k < 8; k++) {
        gptr o = O_OBJ + k * 0x260;
        U8(o + 0x50) = (u8)rnd(); U8(o + 0x58) = (u8)rnd(); U16(o + 0x4A) = (u16)rnd();
    }
    U32(g + 0x93C) = O_EFF; U32(0x87DE04) = rnd() % 8;
    for (u32 k = 0; k < 8; k++) { gptr o = O_EFF + k * 0x70; U8(o + 0x50) = (u8)rnd(); U8(o + 0x4E) = rnd() % 0x80;
        for (u32 j = 0x48; j < 0x4E; j += 2) U16(o + j) = (u16)rnd(); }
    U32(g + 0x948) = O_K3; U32(0x87DE14) = rnd() % 8;
    for (u32 k = 0; k < 8; k++) { U8(O_K3 + k * 0x34) = (u8)rnd(); U8(O_K3 + k * 0x34 + 1) = (u8)rnd(); }
    U32(g + 0x944) = O_K2; U32(0x87DC88) = rnd() % 8;
    for (u32 k = 0; k < 8; k++) { U8(O_K2 + k * 0xAC + 0x50) = (u8)rnd(); U8(O_K2 + k * 0xAC + 0x58) = (u8)rnd(); }
    U8(g + 0x84A) = rnd() % 4; U8(g + 0x84B) = rnd() % 2 ? U8(g + 0x84A) : rnd() % 4;
    static const u32 fl[][2] = {{2, 0x1D}, {1, 1}, {1, 8}, {1, 6}, {1, 0xB}, {1, 7}, {1, 3}, {1, 2}, {1, 0xD}, {1, 4},
                                {1, 0xC}, {3, 0xB}, {1, 5}, {1, 0}, {2, 1}, {2, 0xD}, {2, 0xA}};
    for (unsigned k = 0; k < sizeof fl / sizeof fl[0]; k++) Flag_Set(fl[k][0], fl[k][1], rnd() % 3 == 0);
    Flag_Set(1, 6, 1);                                      /* Script_Tick has its own tests */
    draw_quiet();                                           /* the frame drawn on the way (camera, events): nothing drawn but matrices */
    camera_state();
    enemyset_state();
}

/* the camera zones around the player and a camera to load (a fake DBS record
   in the fake heap: Crt_malloc returns 0x500000) */
static void camera_state(void)
{
    gptr g = G(), pl = U32(g + 0x940), c = g + 0x778;
    U32(g + 0xA44) = O_ZONE; U32(g + 0xA48) = rnd() % 8;
    U32(pl + 0x50) = (U32(pl + 0x50) & ~0x100u) | (rnd() % 4 == 0 ? 0x100 : 0);
    U32(pl + 0x64) = O_PART;                                        /* Obj_Part: a chain of 12 */
    for (u32 k = 0; k < 12; k++) {
        U32(O_PART + k * 0x80 + 0x54) = k < 11 ? O_PART + (k + 1) * 0x80 : 0;
        for (u32 j = 0x40; j < 0x46; j += 2) U16(O_PART + k * 0x80 + j) = (u16)(S16(pl + j) + (s16)(rnd() % 1000 - 500));
    }
    for (u32 k = 0; k < 8; k++) {
        gptr e = O_ZONE + k * 0x2C;
        for (u32 j = 0; j < 0x2C; j += 4) U32(e + j) = rnd();
        U8(e) = rnd() % 4 ? 0 : 0x80; U8(e + 4) = rnd() % 4; U8(e + 6) = rnd() % 3; U8(e + 7) = rnd() % 3;
        s16 cx = S16(pl + 0x40) + (s16)(rnd() % 1000 - 500), cz = S16(pl + 0x44) + (s16)(rnd() % 1000 - 500), r = (s16)(50 + rnd() % 900);
        U16(e + 0xC) = (u16)(cx + r); U16(e + 0xE) = (u16)(cz + r); U16(e + 0x10) = (u16)(cx + r); U16(e + 0x12) = (u16)(cz - r);
        U16(e + 0x14) = (u16)(cx - r); U16(e + 0x16) = (u16)(cz - r); U16(e + 0x18) = (u16)(cx - r); U16(e + 0x1A) = (u16)(cz + r);
    }
    U8(c + 0x4C) = rnd() % 4; U8(c + 0x4D) = (u8)rnd(); U8(c + 0x4F) = (u8)rnd();
    U32(c + 0x40) = O_CAM + 0x100;
    for (u32 k = 0; k < 4 * 0x10; k += 2) U16(O_CAM + 0x100 + k) = (u16)(rnd() % 0x4000 - 0x2000);
    for (u32 k = 0; k < 4; k++) U16(O_CAM + 0x106 + k * 0x10) = (u16)(0x100 + rnd() % 0x200);
    U32(g + 0xA6C) = O_CAM;
    for (u32 k = 0; k < 4; k++) { U32(O_CAM + k * 8) = rnd() % 0x100; U32(O_CAM + k * 8 + 4) = 0x2000 + rnd() % 0x1000; }
    U8(g + 0x84A) = rnd() % 4; U8(g + 0x84B) = rnd() % 2 ? U8(g + 0x84A) : rnd() % 4;
    U8(g + 0x927) = rnd() % 4 == 0; U8(g + 0xB49) = (u8)rnd();
    gptr p = 0x500000;
    for (u32 k = 0; k < 0x4000; k += 4) U32(p + k) = rnd();
    U32(p + 4) = rnd() % 0x400; U32(p + 0x24) = rnd() % 2 ? 0x800 * (rnd() % 3) + rnd() % 3 : rnd() % 0x1800;
    U16(g + 0x664) = rnd() % 5; Flag_Set(3, 0, 0);
    U32(p + 0x40) = rnd() % 3 ? rnd() : 0x6D6D7564; U32(p + 0x60) = rnd() % 2;
    U32(p + 0x64) = rnd() % 0x100; U32(p + 0x68) = rnd() % 0x400;
    stub_at(0x4C03F0)->ret = rnd() % 5 ? 3 : 0xFFFFFFFF;
    stub_at(0x4DD711)->ret = p;
}

/* an enemy set (in O_AOT): groups around the player, spawn points at +0x100 */
static void enemyset_state(void)
{
    gptr g = G(), pl = U32(g + 0x940), set = O_AOT, st = g + 0xA80, st2 = g + 0xAC8;
    U32(g + 0xA68) = rnd() % 6 ? set : 0;
    U32(g + 0xB50) = rnd() % 6 ? 0 : O_OBJ + 0x260 * (1 + rnd() % 3);
    u32 n = rnd() % 5;
    for (u32 k = 0; k < 0x200; k += 4) U32(set + k) = rnd();
    U8(set) = (u8)n; U8(set + 1) = (u8[]){2, 7, 3, 0x1B}[rnd() % 4]; U8(set + 2) = rnd() % 5;
    U8(set + 3) = rnd() % 3 ? 0xFF : (u8[]){0, 1, 2, 3, 0xE5, 0xE6, 0xE7}[rnd() % 7]; U8(set + 4) = rnd() % 4;
    U8(set + 6) = (u8[]){1, 2, 3, 0}[rnd() % 4]; U8(set + 8) = rnd() % 2;
    for (u32 k = 0; k < 0x40; k += 4) U32(g + 0x12D0 + k) = rnd() % 2 ? rnd() & 0x03030303 : rnd();
    for (u32 k = 0; k < 4; k++) {
        gptr e = set + 0x10 + k * 0x20;
        s16 cx = S16(pl + 0x40) + (s16)(rnd() % 1000 - 500), cz = S16(pl + 0x44) + (s16)(rnd() % 1000 - 500), r = (s16)(50 + rnd() % 900);
        U16(e) = (u16)(cx + r); U16(e + 2) = (u16)(cz + r); U16(e + 4) = (u16)(cx + r); U16(e + 6) = (u16)(cz - r);
        U16(e + 8) = (u16)(cx - r); U16(e + 0xA) = (u16)(cz - r); U16(e + 0xC) = (u16)(cx - r); U16(e + 0xE) = (u16)(cz + r);
        for (u32 j = 0; j < 0x10; j += 2) U16(e + j) += (u16)(rnd() % 7 - 3);   /* centres off the grid */
        U16(e + 0x10) = (u16)(0x100 + rnd() % 4 * 0x10); U8(e + 0x12) = rnd() % 4 ? 1 + rnd() % 4 : 0;
        U8(e + 0x13) = rnd() % 3 ? (u8)U16(pl + 0x5A) : rnd() % 3; U8(e + 0x14) = rnd() % 5; U8(e + 0x15) = rnd() % 6;
        U8(e + 0x16) = rnd() % 5; U16(e + 0x18) = rnd() % 0x20;
    }
    for (u32 k = 0; k < 8; k++) {
        gptr p = set + 0x100 + k * 0x10;
        U16(p) = (u16)(S16(pl + 0x40) + (s16)(rnd() % 4000 - 2000)); U16(p + 4) = (u16)(S16(pl + 0x44) + (s16)(rnd() % 4000 - 2000));
        U8(p + 8) = (u8[]){0, 4, 1, 2}[rnd() % 4]; U8(p + 9) = rnd() % 3; U8(p + 0xA) = rnd() % 8;
    }
    for (u32 k = 0; k < 0x48; k += 4) U32(st + k) = rnd();
    U16(st) = rnd() % 3 ? rnd() % 3 : (u16)rnd(); U8(st + 4) = rnd() % 5; U8(st + 6) = rnd() % 2;
    U8(st + 8) = rnd() % 4; U8(st + 9) = rnd() % 3 ? rnd() % 4 : 0xFF; U8(st + 0xA) = rnd() % 3 ? rnd() % 4 : 0xFF;
    U8(st + 0xB) = rnd() % 2; U8(st + 0xC) = rnd() % 2; U16(st + 0xE) = rnd() % 2 ? 0 : rnd() % 0x100;
    for (u32 k = 0; k < 4; k++) U8(st + 0x18 + k) = rnd() % 6;
    if (rnd() % 4 == 0) U8(st + 4) = (u8)(U8(set + 2) + 8 + rnd() % 3);
    if (rnd() % 4 == 0) { U32(st + 0x10) = 0; U32(st + 0x14) = 0; }
    U16(O_PART + 8 * 0x80 + 0x5A) = rnd() % 3;
    for (u32 k = 0; k < 0x10; k += 4) U32(g + 0xAB8 + k) = rnd() % 3 ? 0 : (rnd() & 0x0305030F);
    U16(st2) = rnd() % 3 ? rnd() % 3 : rnd() % 2 ? 0x8001 : (u16)rnd(); U8(st2 + 5) = rnd() % 6; U8(st2 + 9) = rnd() % 3 ? rnd() % 4 : 0xFF;
    U16(pl + 0x17E) = rnd() % 3 ? (u16)(rnd() % 0x500) : rnd() % 2 ? 0 : (u16)(0x41A + rnd() % 3 - 1);
    U16(g + 0x138A) = rnd() % 3 ? 0 : rnd() % 3; U16(g + 0x1388) = rnd() % 4;
    U8(g + 0x108E) = rnd() % 3; U8(g + 0x1090) = rnd() % 3; U8(g + 0x1091) = rnd() % 3;
    U32(g + 0xA7C) = rnd() % 4;
    for (u32 k = 1; k < 8; k++) U8(O_OBJ + k * 0x260 + 0x58) = (u8[]){2, 7, 3, 0x1B}[rnd() % 4];
}

/* the trigger areas: some around the player (or the point in front of it, or
   the event position 0x7598B8), on its level or not */
static void aot_state(void)
{
    gptr g = G(), pl = U32(g + 0x940);
    U32(g + 0xA5C) = 0x7593B8;
    for (u32 k = 0; k < 0x20; k += 4) U32(g + 0xB28 + k) = rnd();
    U16(pl + 0x40) = (u16)(rnd() % 4000 - 2000); U16(pl + 0x42) = (u16)(rnd() % 4000 - 2000); U16(pl + 0x44) = (u16)(rnd() % 4000 - 2000);
    U16(pl + 0x48) = (u16)rnd(); U16(pl + 0x4A) = (u16)rnd(); U16(pl + 0x4C) = (u16)rnd();
    U16(pl + 0x5A) = rnd() % 3; U8(pl + 0x55) = rnd() % 3 ? 0xA : (u8)rnd(); U8(pl + 0x54) = rnd() % 3 ? 1 : rnd() % 4;
    U16(g + 0x10B4) = (u16)((rnd() % 3 ? (u16[]){3, 6, 2}[rnd() % 3] : rnd() % 8) << 8 | (rnd() & 0xFF));
    U16(0x7598B8) = (u16)(rnd() % 4000 - 2000); U16(0x7598BC) = (u16)(rnd() % 4000 - 2000);
    U16(0x7598BA) = (u16)(rnd() % 3 ? -(s16)(rnd() % 3 * 1000 + rnd() % 3 - 1) : (u16)rnd());
    U16(0x7593B0) = rnd() % 2 ? (u16[]){0x10, 0x20, 0x30, 0x1030, 0x2010, 0x1020}[rnd() % 6] : (u16)rnd();
    for (gptr e = 0x7593B8; e < 0x7598B8; e += 0x28) {
        for (u32 k = 0; k < 0x28; k += 4) U32(e + k) = rnd();
        U8(e) = rnd() % 3 ? (u8)(rnd() | 1) : (u8)rnd();
        if (rnd() % 2) U8(e) &= ~4;
        if (rnd() % 3 == 0) U8(e) = (u8)(0x0D | (u8[]){0x10, 0x20, 0x30}[rnd() % 3] | (rnd() & 0x42));   /* camera events */
        U8(e + 1) = rnd() % 12; U8(e + 2) = rnd() % 3;
        s16 cx = rnd() % 2 ? S16(pl + 0x40) : rnd() % 2 ? S16(0x7598B8) : (s16)(rnd() % 4000 - 2000);
        s16 cz = rnd() % 2 ? S16(pl + 0x44) : rnd() % 2 ? S16(0x7598BC) : (s16)(rnd() % 4000 - 2000);
        s16 r = (s16)(10 + rnd() % 900);                   /* clockwise in x/z, as the rooms' */
        U16(e + 4) = (u16)(cx + r); U16(e + 6) = (u16)(cz + r); U16(e + 8) = (u16)(cx + r); U16(e + 0xA) = (u16)(cz - r);
        U16(e + 0xC) = (u16)(cx - r); U16(e + 0xE) = (u16)(cz - r); U16(e + 0x10) = (u16)(cx - r); U16(e + 0x12) = (u16)(cz + r);
        U8(e + 0x20) = rnd() % 2 ? 0 : 1 + rnd() % 8; U8(e + 0x21) = (u8)rnd();
    }
}

static void test_frame(void)
{
    g_ign_campad = 1;
    stubs_install(1);
    type_tables_stub(1);
    script_setup();
    for (int i = 0; i < iters; i++) {
        game_state(); fake_room_block();
        static const struct { u32 addr; void *fn; const char *name; } F[] = {
            {0x471EF0, Obj_RunAll, "Obj_RunAll"}, {0x471F90, Prop_RunAll, "Prop_RunAll"},
            {0x471FE0, Eff_RunAll, "Eff_RunAll"}, {0x472040, ObjEff_RunAll, "ObjEff_RunAll"},
            {0x48B040, AOT_Check, "AOT_Check"}, {0x48AFF0, Aot_Tick, "Aot_Tick"},
            {0x472FD0, Game_Frame, "Game_Frame"},
            {0x403570, Camera_Tick, "Camera_Tick"}, {0x403AC0, Camera_Load, "Camera_Load"},
        };
        for (unsigned k = 0; k < sizeof F / sizeof F[0]; k++) {
            frame_state(); aot_state(); obj_part_counts();
            U32(LOG_BASE) = 0;
            sprintf(g_what, "objects %u effects %u/%u props %u level %u climb %X", U32(0x87F940), U32(0x87DE14),
                    U32(0x87DE04), U32(0x87DC88), U16(U32(G() + 0x940) + 0x5A), U16(G() + 0x10B4));
            check(F[k].addr, F[k].fn, F[k].name, 0, NULL, 0);
        }
        for (int r = 0; r < 4; r++) {                          /* the camera events' areas */
            frame_state(); aot_state(); obj_part_counts(); Flag_Set(2, 0xA, 1); Flag_Set(2, 0x1D, 0);
            U32(LOG_BASE) = 0;
            sprintf(g_what, "camera event %X at %d,%d,%d", U16(0x7593B0), S16(0x7598B8), S16(0x7598BA), S16(0x7598BC));
            check(0x48B040, AOT_Check, "AOT_Check", 0, NULL, 0);
        }
        /* sound each frame and the voice */
        for (u32 k = 0; k < 4; k++) {
            gptr q = 0x87FB80 + k * 0x14;
            U32(q) = rnd() % 8; U8(q + 8) = rnd() % 4 != 0; U8(q + 0x10) = rnd() % 4 ? (u8)(rnd() % 2 ? 1 : 0x81) : 0;
            U8(q + 0x11) = (u8)(1 + rnd() % 0x20); U16(q + 0xC) = (u16)(rnd() % 0x80);
            U16(q + 0xA) = rnd() % 3 ? (u16)(U16(q + 0xC) + (s16)(rnd() % 0x41) - 0x20) : (u16)(rnd() % 0x80);
        }
        for (u32 k = 0; k < 0x20; k++) {
            gptr q = 0x87F980 + k * 0x10;
            U32(q) = rnd() % 8; U32(q + 4) = rnd() % 2 ? 0x1D : rnd() % 0x20; U8(q + 8) = rnd() % 4 != 0;
            U8(q + 0xA) = rnd() % 4 ? (u8)(rnd() % 2 ? 1 : 0x81) : 0; U8(q + 0xB) = (u8)(1 + rnd() % 0x20);
            U16(q + 0xE) = (u16)(rnd() % 0x80);
            U16(q + 0xC) = rnd() % 3 ? (u16)(U16(q + 0xE) + (s16)(rnd() % 0x41) - 0x20) : (u16)(rnd() % 0x80);
        }
        U8(G() + 0x14ED) = rnd() % 16; U8(G() + 0x14EE) = rnd() % 16;
        Flag_Set(2, 0, rnd() % 2); U32(0x7598C8) = rnd() % 4; U32(0x7598CC) = 0x6DDC40; U32(0x7598D0) = rnd() % 2 ? 0x1D : rnd() % 0x20;
        stub_at(0x4BFAA0)->ret = rnd() % 2;
        U32(LOG_BASE) = 0;
        sprintf(g_what, "sound volumes %u/%u voice %u", U8(G() + 0x14ED), U8(G() + 0x14EE), Flag_Check(2, 0));
        check(0x48C700, Snd_Tick, "Snd_Tick", 0, NULL, 0);
        frame_state();
        u32 cz[2] = {G() + 0x778, O_ZONE};
        U32(LOG_BASE) = 0;
        check(0x4035B0, CamZones_FindForCamera, "CamZones_FindForCamera", 2, cz, 0xFFFFFFFF);
        check(0x4035F0, CamZones_Check, "CamZones_Check", 2, cz, 0);
        u32 v[2] = {rnd() % 4, rnd() % 10000};
        check(0x48D000, Voice_Volume, "Voice_Volume", 1, v, 0xFFFFFFFF);
        Flag_Set(2, 0, rnd() % 2);
        stub_at(0x4C03F0)->ret = rnd() % 4 ? 3 : 0xFFFFFFFF; stub_at(0x4C0540)->ret = rnd() % 0x1000;
        stub_at(0x4DD711)->ret = 0x6DDC40; stub_at(0x4BF720)->ret = rnd() % 4;
        U32(LOG_BASE) = 0;
        check(0x48D070, Voice_Play, "Voice_Play", 2, v, 0);
        check(0x48D130, Voice_Stop, "Voice_Stop", 0, NULL, 0);
        frame_state(); aot_state();
        u32 a[1] = {0x7593B8 + 0x28 * (rnd() % 32)};
        Flag_Set(8, U8(a[0] + 0x21), rnd() % 2);
        stub_at(0x4718F0)->ret = 0x6DDC00; U8(0x6DDC00) = 0;  /* the message window (compared memory) */
        U32(LOG_BASE) = 0;
        sprintf(g_what, "door aot %X flags %02X msg %u", a[0], U8(a[0]), U8(a[0] + 0x20));
        check(0x48B390, AOT_Door, "AOT_Door", 1, a, 0);
    }
    type_tables_stub(0);
    stubs_install(0);
    g_ign_campad = 0;
}

static void test_enemyset(void)
{
    g_ign_campad = 1;
    stubs_install(1);
    type_tables_stub(1);
    script_setup();
    for (int i = 0; i < iters; i++) {
        game_state(); fake_room_block();
        /* the enemy sets */
        static const struct { u32 addr; void *fn; const char *name; } E[] = {
            {0x472230, EnemySet_Tick, "EnemySet_Tick"}, {0x472D20, EnemySet_Small, "EnemySet_Small"},
            {0x472130, EnemySet_Cries, "EnemySet_Cries"},
        };
        for (int r = 0; r < 4; r++)
            for (unsigned k = 0; k < sizeof E / sizeof E[0]; k++) {
                frame_state();
                U32(LOG_BASE) = 0;
                u32 ea[1] = {G() + 0xAB8};
                sprintf(g_what, "set %X groups %u type %u objects %u timer %u", U32(G() + 0xA68), U8(O_AOT), U8(O_AOT + 1),
                        U32(0x87F940), U16(G() + 0xA80));
                check(E[k].addr, E[k].fn, E[k].name, k == 2, ea, 0);
            }
        frame_state();
        {
            u32 a2[3] = {rnd() % 8, rnd() % 8};
            check(0x4720B0, EnemySet_Cry, "EnemySet_Cry", 2, a2, 0);
            a2[0] = O_AOT + 0x100 + 0x10 * (rnd() % 8);
            check(0x4721B0, EnemySet_Light, "EnemySet_Light", 1, a2, 0);
            a2[0] = O_AOT + 0x10 + 0x20 * (rnd() % 4); a2[1] = O_AOT + 0x10 + 0x20 * (rnd() % 4); a2[2] = G() + 0xA90;
            check(0x472BB0, EnemySet_Way, "EnemySet_Way", 3, a2, 0);
            a2[0] = O_AOT + 0x10; a2[1] = rnd() % 6 - 1;
            check(0x472C60, EnemySet_FindGroup, "EnemySet_FindGroup", 2, a2, 0xFF);
            check(0x48D7B0, Obj_AllocEnemy, "Obj_AllocEnemy", 0, NULL, 0xFFFFFFFF);
        }
    }
    type_tables_stub(0);
    stubs_install(0);
    g_ign_campad = 0;
}

/* ---- what the enemies share -------------------------------------------------------- */
/* the player (object 0) with hit spheres, enemies 1..7 with attack spheres;
   both on parts of their own (O_PART chains) */
static void enemy_state(void)
{
    gptr g = G(), pl = O_OBJ;
    for (int k = 0; k < 256; k++) U32(TYPE_TAB + 4 * k) = (u32)(uintptr_t)g_recs[k & 1 ? STUB_AOT : STUB_TYPE];
    U32(g + 0x940) = pl; U32(0x87F940) = 2 + rnd() % 6;
    U32(g + 0xB50) = rnd() % 4 ? 0 : O_OBJ + 0x260 * 7;
    for (u32 k = 0; k < 8; k++) {
        gptr o = O_OBJ + k * 0x260;
        U8(o + 0x50) = (u8)(rnd() % 4 ? 1 : rnd()); U8(o + 0x58) = (u8[]){2, 2, 7, 8, 3}[rnd() % 5];
        U8(o + 0x54) = rnd() % 3; U8(o + 0x55) = rnd() % 4; U8(o + 0xFD) = rnd() % 2;
        for (u32 j = 0x40; j < 0x46; j += 2) U16(o + j) = (u16)(rnd() % 2000 - 1000);
        for (u32 j = 0x48; j < 0x4E; j += 2) U16(o + j) = (u16)rnd();
        U16(o + 0x4A) = (u16)rnd(); U16(o + 0x178) = (u16)rnd(); U8(o + 0x184) = (u8)((rnd() % 2 || !U32(g + 0xB50) ? 0 : 0x40) | (rnd() & 8));
        U32(o + 0x64) = O_PART + k * 0x200;
        for (u32 j = 0; j < 4; j++) {
            gptr q = O_PART + k * 0x200 + j * 0x80;
            U32(q + 0x54) = j < 3 ? q + 0x80 : 0;
            for (u32 m = 0x20; m < 0x46; m += 2) U16(q + m) = (u16)(rnd() % 0x2000 - 0x1000);
        }
        U16(o + 0x92) = rnd() % 0x20; U16(o + 0x102) = (u16)(rnd() & 0xE); U16(o + 0x10C) = rnd() % 7;
        U8(o + 0x11D) = (u8)(rnd() % 3 ? rnd() % 0x80 - 0x40 : rnd()); U8(o + 0x174) = rnd() % 4 == 0; U8(o + 0x17B) = rnd() % 4 == 0;
        U8(o + 0xF8) = rnd() % 3 ? 0 : (u8)rnd(); U8(o + 0xF9) = (u8)rnd(); U8(o + 0xFA) = (u8)rnd();
        U32(o + 0x6C) = rnd(); U32(o + 0x70) = rnd(); U32(o + 0x74) = rnd();
        /* attack spheres (0x10) and hit spheres (0xC) */
        gptr a = O_COLL + k * 0x80;
        U32(o + 0xA0) = rnd() % 5 ? a : 0;
        u32 na = 1 + rnd() % 3;
        for (u32 j = 0; j < na; j++, a += 0x10) {
            for (u32 m = 0; m < 6; m += 2) U16(a + m) = (u16)(rnd() % 400 - 200);
            U16(a + 6) = 50 + rnd() % 200; U8(a + 8) = rnd() % 0x10; U8(a + 9) = U8(a + 8) + rnd() % 0x10;
            U8(a + 0xA) = rnd() % 3 ? 0xFF : rnd() % 4; U8(a + 0xB) = j + 1 < na; U16(a + 0xC) = rnd() % 300;
        }
        gptr h = O_COLL + 0x400 + k * 0x40;
        U32(o + 0x134) = rnd() % 5 ? h : 0;
        u32 nh = 1 + rnd() % 3;
        for (u32 j = 0; j < nh; j++, h += 0xC) {
            for (u32 m = 0; m < 6; m += 2) U16(h + m) = (u16)(rnd() % 400 - 200);
            U16(h + 6) = 50 + rnd() % 200; U16(h + 8) = rnd() % 500; U8(h + 0xA) = rnd() % 3 ? 0xFF : rnd() % 4; U8(h + 0xB) = j + 1 < nh;
        }
        U16(o + 0x17E) = rnd() % 2 ? 0 : (u16)rnd();
    }
    U8(g + 0x108F) = rnd() % 4; U8(g + 0x108E) = rnd() % 3; U8(g + 0x927) = rnd() % 3 == 0;
    U16(g + 0x1090) = (u16[]){0x808, 0x407, 0x100, 0x80A, 0x408, 0x409, 0x001, 0x203}[rnd() % 8];
    U16(g + 0x664) = rnd() % 5;
    gptr r = g + 0x68C;
    U8(r + 0xCC) = rnd() % 2; U8(r + 0xCD) = rnd() % 2; U16(r + 0xC4) = rnd() % 2 ? rnd() % 12 : (u16)rnd(); U32(r + 0xB4) = rnd();
    U16(g + 0xC24) = rnd() % 12; U32(g + 0x11F4) = rnd() % 2 ? 9999999 - rnd() % 0x2000 : rnd() % 100000;
    Flag_Set(9, 0x36, rnd() % 2); Flag_Set(2, 0x18, rnd() % 2); Flag_Set(2, 0xB, rnd() % 2);
    U32(0x87DC98) = 0x6D0000;
    stub_at(0x4C08A0)->ret = rnd();
    draw_safe();                                /* (game over draws a frame) */
}


static void test_enemies(void)
{
    g_ign_objpads = 1;
    stubs_install(1);
    type_tables_stub(1);
    for (int i = 0; i < iters; i++) {
        game_state(); world_setup(); enemy_state();
        gptr o = O_OBJ + 0x260 * (1 + rnd() % 6);
        u32 a[6] = {o};
        U32(LOG_BASE) = 0;
        sprintf(g_what, "enemy %X type %u frame %u flags %X", o, U8(o + 0x58), U16(o + 0x92), U16(o + 0x102));
        check(0x479830, Enemy_AttackHit, "Enemy_AttackHit", 1, a, 0xFF);
        check(0x404AF0, Score_Kill, "Score_Kill", 1, a, 0);
        check(0x404980, Score_Event, "Score_Event", 1, a, 0);
        check(0x479D00, Enemy_HitDir, "Enemy_HitDir", 1, a, 0xFF);
        check(0x479CB0, Obj_HitDir, "Obj_HitDir", 1, a, 0xFF);
        check(0x48FBB0, Obj_DeathFade, "Obj_DeathFade", 1, a, 0);
        check(0x414D20, Light_Flash, "Light_Flash", 1, a, 0);
        a[1] = rnd() % 4;
        check(0x47AD00, Enemy_FindState, "Enemy_FindState", 2, a, 0xFFFFFFFF);
        a[1] = O_OBJ + 0x40; a[2] = rnd() % 2 ? rnd() % 0x100000 : rnd(); a[3] = rnd() % 0x800;
        if (rnd() % 2) a[2] = Dist2_XZ(o + 0x40, a[1]) + rnd() % 3 - 1;          /* the edges */
        if (rnd() % 2) a[3] = (((u32)(s32)(s16)Angle_XZ(o + 0x40, a[1]) - U16(o + 0x4A)) & 0xFFF) + rnd() % 3 - 1;
        check(0x47A780, Obj_InSight, "Obj_InSight", 4, a, 0xFFFFFFFF);
        a[1] = rnd();
        check(0x48DAB0, Prop_Attach, "Prop_Attach", 2, a, 0xFFFFFFFF);
        for (u32 k = 0; k < 20; k++) if (rnd() % 3 == 0) U32(O_K2 + k * 0xAC + 0xA8) = o;
        check(0x48DAB0, Prop_Attach, "Prop_Attach", 2, a, 0xFFFFFFFF);
        u32 b[6] = {(u32)(rnd() % 8), O_OBJ + 0x40, rnd(), rnd(), rnd(), rnd()};
        check(0x48DC20, Prop_SpawnC, "Prop_SpawnC", 6, b, 0xFFFFFFFF);
        b[0] = rnd() % 2 ? o : 0; b[1] = 0x6DE100;
        U8(0x6DE100) = 4 + rnd() % 8; U8(0x6DE101) = rnd() % 2; U8(0x6DE102) = rnd() % 2;
        check(0x478B80, Flash_Play, "Flash_Play", 2, b, 0xFFFFFFFF);
        b[0] = rnd() % 2 ? rnd() % 0x400 : (u32)(u16)rnd();
        for (int r = 0; r < 8; r++) {                       /* the random edge 0x80 */
            U16(0x87DE10) = (u16)rnd();
            check(0x4797D0, Player_Bleed, "Player_Bleed", 1, b, 0xFFFF);
        }
        check(0x46B960, Layers_Assign, "Layers_Assign", 0, NULL, 0);
        {   /* two spheres (x, y, z, radius) and the heights they reach */
            gptr sa = 0x6DE100, sb = 0x6DE108;
            for (int k = 0; k < 8; k += 2) { S16(sa + k) = (s16)(rnd() % 600 - 300); S16(sb + k) = (s16)(rnd() % 600 - 300); }
            S16(sa + 6) = (s16)(rnd() % 300); S16(sb + 6) = (s16)(rnd() % 300);
            if (rnd() % 2) {                                 /* touching, about */
                s32 dx = S16(sa) - S16(sb), dz = S16(sa + 4) - S16(sb + 4);
                S16(sb + 6) = (s16)((s32)sqrt((double)(dx * dx + dz * dz)) - S16(sa + 6) + (s32)(rnd() % 3) - 1);
            }
            s32 dy = S16(sa + 2) - S16(sb + 2); if (dy < 0) dy = -dy;
            u32 c[4] = {sa, sb, (u32)(rnd() % 400), 0};
            c[3] = rnd() % 2 ? (u32)(u16)(dy - (s16)c[2] + (s32)(rnd() % 3) - 1) : rnd() % 400;
            check(0x452FF0, Sphere_Hit, "Sphere_Hit", 4, c, 0xFFFFFFFF);
        }
        b[0] = rnd() % 2 ? 0 : rnd() % 0x40;
        draw_safe();
        check(0x4529C0, GameOver_Start, "GameOver_Start", 1, b, 0);
    }
    type_tables_stub(0);
    stubs_install(0);
    g_ign_objpads = 0;
}

/* ---- object effects (G+0x93C) ---------------------------------------------------- */
#define OE_MODEL 0x6CF200u      /* the model block an effect points at */
static void test_obj_effects(void)
{
    static const struct { u32 addr; void *fn; const char *name; } k[] = {
        {0x474BB0, ObjEff_Turning, "ObjEff_Turning"}, {0x474BD0, ObjEff_Turn, "ObjEff_Turn"}, {0x474BE0, ObjEff_Blink, "ObjEff_Blink"},
        {0x474C00, ObjEff_BlinkMove, "ObjEff_BlinkMove"}, {0x474C80, ObjEff_BlinkDark, "ObjEff_BlinkDark"}, {0x474CA0, ObjEff_BlinkLit, "ObjEff_BlinkLit"},
        {0x474D50, ObjEff_ThrownInit, "ObjEff_ThrownInit"}, {0x474DC0, ObjEff_ThrownMove, "ObjEff_ThrownMove"}, {0x474E50, ObjEff_Thrown, "ObjEff_Thrown"},
        {0x474E70, ObjEff_Debris, "ObjEff_Debris"}, {0x474EA0, ObjEff_DebrisInit, "ObjEff_DebrisInit"}, {0x474F10, ObjEff_DebrisMove, "ObjEff_DebrisMove"},
        {0x474FB0, ObjEff_InHand, "ObjEff_InHand"}, {0x474FD0, ObjEff_InHandInit, "ObjEff_InHandInit"}, {0x475080, ObjEff_InHandHold, "ObjEff_InHandHold"},
        {0x402520, Nothing, "Nothing"}, {0x474B10, DrawSync, "DrawSync"}};
    g_ign_objpads = 1;
    stubs_install(1);
    for (int i = 0; i < iters; i++) {
        game_state();
        do anim_setup_at(O_OBJ); while (U32(O_OBJ + 0x68) < 8);   /* the player, with a part 7 */
        U32(G() + 0x940) = O_OBJ;
        S16(O_OBJ + 0x42) = (s16)(rnd() % 4000 - 2000);
        U8(G() + 0x108F) = rnd() % 3 ? 0 : rnd() % 4;
        Flag_Set(9, 0x2E, rnd() % 3 != 0);
        U32(OE_MODEL) = OE_MODEL + 0x100; U32(OE_MODEL + 4) = OE_MODEL + 0x400;
        U16(OE_MODEL + 8) = rnd() % 10; U16(OE_MODEL + 0xA) = rnd() % 10;
        for (u32 a = OE_MODEL + 0x100; a < OE_MODEL + 0xA00; a += 4) U32(a) = rnd();
        for (unsigned j = 0; j < sizeof k / sizeof k[0]; j++) {
            gptr e = O_EFF + 0x70 * (rnd() % 12);
            for (u32 a = 0; a < 0x70; a += 4) U32(e + a) = rnd();
            U32(e + 0x68) = OE_MODEL; U8(e + 0x4F) = rnd() % 4; U8(e + 0x47) = rnd() % 2 ? (u8)(0x76 + rnd() % 4) : (u8)rnd();
            U8(e + 0x46) = rnd() % 2; S16(e + 0x42) = (s16)(S16(O_OBJ + 0x42) + (s32)(rnd() % 3) - 1 - (rnd() % 2 ? 0 : (s32)(rnd() % 300)));
            if (rnd() % 2) U32(e + 0x50) &= ~0x40000000u;
            if (j == 6 || j == 7 || j == 8) { if (!U8(0x737790 + S8(e + 0x4F) * 5)) U8(e + 0x4F) = 0; }
            if (j >= 9 && j <= 11) { if (!U8(0x7377C4 + S8(e + 0x4F) * 5) || U8(0x7377C8 + S8(e + 0x4F) * 5) > 2) U8(e + 0x4F) = 0; }
            if (j == 8) U8(e + 0x46) = rnd() % 3;
            if (j == 9) U8(e + 0x46) = rnd() % 2;
            if (j == 12) U8(e + 0x46) = rnd() % 4;
            if (j == 0) U8(e + 0x46) = rnd() % 3;
            if (j == 2) U8(e + 0x46) = rnd() % 3;
            if (rnd() % 2) {                                /* lands at the player's height, about */
                s32 d = (s32)(rnd() % 3) - 1, inc;
                if (j == 7 || j == 8) inc = S16(e + 0x62);
                else { s8 v = S8(e + 0x47); if (v < 0x78) v++; inc = (v - 0xC) * 10; }
                S16(e + 0x42) = (s16)(S16(O_OBJ + 0x42) - inc + d);
            }
            U32(LOG_BASE) = 0;
            u32 a[2] = {e, rnd()};
            sprintf(g_what, "effect %X step %u kind %u", e, U8(e + 0x46), U8(e + 0x4F));
            check(k[j].addr, k[j].fn, k[j].name, 1, a, 0);
            if (j == 5) check(0x474CC0, ObjEff_Shade, "ObjEff_Shade", 2, a, 0);
        }
    }
    stubs_install(0);
    g_ign_objpads = 0;
}

/* ---- effect slots (G+0x944): sprites, splashes, emitters ------------------------- */
#define K2_RECS 0x6CF200u       /* emitter records, 0x28 each */
static void k2_records(gptr r, int n, int t0)
{
    for (int i = 0; i < n; i++, r += 0x28) {
        for (u32 a = 0; a < 0x28; a += 4) U32(r + a) = rnd();
        U8(r) = rnd() % 4; U8(r + 0xF) = (u8)(t0 + i / 2); U8(r + 0x1E) = (u8)((rnd() % 3 ? 0 : rnd() % 2 ? 4 : 1) << 4);
        U8(r + 0x1F) = rnd() % 2; U32(r + 0x18) = rnd() % 2 ? 0 : 0x6CF900;
    }
    U32(r) = 0xFFFFFFFF; U8(r + 0xF) = rnd() % 3 ? 0xFF : (u8)(t0 + rnd() % (n / 2 + 2));   /* the end's time, sometimes due */
}

static void test_k2(void)
{
    static const struct { u32 addr; void *fn; const char *name; int n; } k[] = {
        {0x448100, K2_ToWorld, "K2_ToWorld", 2}, {0x448140, K2_OwnerObj, "K2_OwnerObj", 1}, {0x4481A0, K2_SyncParent, "K2_SyncParent", 1},
        {0x4481E0, K2_PushFront, "K2_PushFront", 1}, {0x448240, K2_Light, "K2_Light", 2}, {0x4482B0, K2Step_SetAngles, "K2Step_SetAngles", 1},
        {0x4482E0, K2Step_SetYaw, "K2Step_SetYaw", 1}, {0x448300, K2Step_Accel, "K2Step_Accel", 1}, {0x448330, K2Step_SpinAccel, "K2Step_SpinAccel", 1},
        {0x448350, K2Step_Rise, "K2Step_Rise", 1}, {0x4483A0, K2Step_Grow, "K2Step_Grow", 1}, {0x448430, K2Step_GrowLaterInit, "K2Step_GrowLaterInit", 1},
        {0x448470, K2Step_GrowLater, "K2Step_GrowLater", 1}, {0x448520, K2Step_Hold, "K2Step_Hold", 1}, {0x448760, K2_MatrixAngles, "K2_MatrixAngles", 2},
        {0x48DD10, K2_AnimStep, "K2_AnimStep", 1}, {0x48DCE0, K2_SetAnim, "K2_SetAnim", 2}, {0x48F8C0, Pos_Hidden, "Pos_Hidden", 1},
        {0x48F9B0, K2_UpdateHidden, "K2_UpdateHidden", 1}, {0x448900, K2_Handler, "K2_Handler", 1}, {0x4489B0, K2_Init, "K2_Init", 1},
        {0x448B70, K2_Move, "K2_Move", 1}, {0x448BE0, K2_FadeOut, "K2_FadeOut", 1}, {0x448C70, K2_Handler2, "K2_Handler2", 1},
        {0x448CC0, K2_SplashInit, "K2_SplashInit", 1}, {0x448E10, K2_SplashFall, "K2_SplashFall", 1}, {0x448EB0, K2_FloatA, "K2_FloatA", 1},
        {0x448FC0, K2_FloatB, "K2_FloatB", 1}, {0x4490D0, K2_Splash, "K2_Splash", 1}, {0x449120, K2_Emitter, "K2_Emitter", 1},
        {0x449140, K2_EmitterStart, "K2_EmitterStart", 1}, {0x449180, K2_EmitterStep, "K2_EmitterStep", 1}, {0x4494C0, K2_Emitter2, "K2_Emitter2", 1},
        {0x449530, K2_Emitter2Start, "K2_Emitter2Start", 1}, {0x449650, K2_StepNext, "K2_StepNext", 1}, {0x449660, K2_Emitter2Step, "K2_Emitter2Step", 1}};
    g_ign_objpads = 1;
    stubs_install(1);
    for (int i = 0; i < iters; i++) {
        for (unsigned j = 0; j < sizeof k / sizeof k[0]; j++) {
            game_state(); objects_setup(); world_setup();
            for (u32 q = 0; q < 16; q++) U8(O_OBJ + q * 0x260 + 0xFD) &= 1;
            U32(0x87F940) = 16;
            gptr kk = O_K2 + 0xAC * (j >= 29 && j <= 31 ? 19 : j >= 32 || j == 23 ? rnd() % 14 : rnd() % 20);   /* emitters away from the slots they fill */
            for (u32 a = 0x20; a < 0xAC; a += 4) U32(kk + a) = rnd() % 3 ? rnd() % 0x10000 : rnd();
            U8(kk + 0x50) = (u8)(rnd() % 4 ? 1 | (rnd() & 6) : rnd() & 6);
            U8(kk + 0x7B) = rnd() % 10; U8(kk + 0x54) = rnd() % 4; U8(kk + 0x55) = rnd() % 3; U8(kk + 0x7A) = rnd() % 0x10;
            U8(kk + 0x46) = rnd() % 3; U8(kk + 0x87) = (u8)((kk - O_K2) / 0xAC); U8(kk + 0x86) = rnd() % 4 ? (u8)(rnd() % 20) : 0xFF;
            gptr src = 0x6CF000 + 0x40 * (rnd() % 4);
            U8(src) = (u8)rnd(); U8(src + 1) = rnd() % 2; U8(src + 0x18) = (u8)rnd();
            U32(kk + 0x8C) = rnd() % 3 ? src : 0;
            u32 pr = rnd() % 5;
            U32(kk + 0x60) = pr == 0 ? 0 : pr == 1 ? kk : pr == 2 ? O_OBJ + 0x260 * (rnd() % 16) : pr == 3 ? O_K2 + 0xAC * (rnd() % 20) : U32(O_OBJ + 0x260 * (rnd() % 16) + 0x64);
            if (pr == 4 && rnd() % 2) {                     /* a part of a part of an object */
                gptr p1 = PART_POOL + 0xCC * (rnd() % 20), p2 = PART_POOL + 0xCC * (20 + rnd() % 20);
                U32(p1 + 0x58) = rnd() % 2 ? p2 : O_OBJ + 0x260 * (rnd() % 16); U32(p2 + 0x58) = O_OBJ + 0x260 * (rnd() % 16);
                U32(kk + 0x60) = p1;
            }
            for (u32 q = 0; q < 6; q++) U16(kk + 0x6C + 2 * (q & 1)) = rnd() % 2 ? rnd() % 0x120 : (u16)rnd();
            U16(kk + 0x98) = rnd() % 4 ? rnd() % 4 : (u16)rnd(); U16(kk + 0x9C) = rnd() % 3 ? rnd() % 3 : (u16)rnd(); U16(kk + 0x9E) = rnd() % 3 ? rnd() % 3 : (u16)rnd();
            U8(kk + 0x64) = rnd() % 3 ? 2 : (u8)rnd();
            /* the sprite animation list, sometimes at its end or jumping */
            gptr an = 0x6CF900;
            for (u32 a = 0; a < 0x80; a += 4) U32(an + a) = rnd();
            for (u32 q = 0; q < 16; q++) { U8(an + 8 * q + 4) = (u8)((rnd() % 4 == 0 ? 0x20 * (rnd() % 8) : 0) | (rnd() % 16)); U8(an + 8 * q + 5) = rnd() % 3; }
            U32(kk + 0x90) = an; U32(kk + 0x94) = an + 8 * (rnd() % 8); U16(kk + 0x82) = rnd() % 3;
            /* emitters */
            if (j >= 29 && j <= 31) {
                k2_records(K2_RECS, 2 + rnd() % 8, rnd() % 3);
                u32 at = rnd() % 3;
                U32(kk + 0x9C) = K2_RECS + 0x28 * at; U32(kk + 0x98) = rnd();
                for (u32 q = 0; q < 20; q++) if (O_K2 + 0xAC * q != kk) U8(O_K2 + 0xAC * q + 0x50) &= ~1;   /* the slots it fills are free */
                U8(kk + 0x50) |= 1;                         /* itself in use */
                U32(G() + 0xA70) = rnd() % 4 ? 0 : rnd() % 14;   /* free slots before it: none stolen (K2_Reuse) */
                U8(kk + 0xA0) = rnd() % 12; U8(kk + 0xA1) = rnd() % 6; U8(kk + 0xA2) = (u8)(U8(K2_RECS + 0x28 * at + 0xF) - rnd() % 2); U8(kk + 0xA3) = (u8)(U8(kk + 0xA1) + at);   /* as far on as its list */
                U8(kk + 0xA4) = (u8)rnd(); U16(kk + 0xA8) = rnd() % 2 ? 0 : (u16)rnd();
                U32(0x87DC88) = 20;
                for (u32 q = 0; q < 24; q++) U8(O_EFF + q * 0x70 + 0x50) = rnd() % 2;
                U32(G() + 0x93C) = O_EFF;
            }
            if (j == 23) U8(kk + 0x54) = (u8[]){0, 1, 2, 3, 4, 5, 9, 10, 11}[rnd() % 9];
            if ((j == 23 && U8(kk + 0x54) >= 9) || j == 32) U8(kk + 0x7B) = 0;   /* emitters do not move (+0x98.. are their list) */
            if (j == 32) U8(kk + 0x54) = rnd() % 4;
            if (j >= 32 || j == 23) {
                int nrec = 2 + rnd() % 8;
                k2_records(K2_RECS, nrec, rnd() % 3);
                u32 c = rnd() % 3, start = j == 33 || (j == 32 && U8(kk + 0x54) == 0) || (j == 23 && U8(kk + 0x54) == 9);
                U8(kk + 0x9C) = (u8)(rnd() % (c + 1)); U8(kk + 0x9D) = rnd() % 2 ? 0xFF : (u8)(rnd() % 8);
                U32(kk + 0x98) = start ? K2_RECS : K2_RECS + 0x28 * c;   /* where it is in its list, counted in +0x9F */
                U8(kk + 0x50) |= 1;
                U8(kk + 0x9F) = (u8)c; U8(kk + 0x9E) = (u8)(U8(K2_RECS + 0x28 * c + 0xF) + rnd() % 2);
                gptr par = rnd() % 2 ? O_OBJ + 0x260 * (1 + rnd() % 15) : O_K2 + 0xAC * (rnd() % 20);
                if (par == kk) par = O_OBJ + 0x260;
                U32(kk + 0xA0) = par; U32(par + 0x6C) = rnd() % 0x2000; U8(par + 0x50) = (u8)((U8(par + 0x50) & ~1) | (rnd() % 4 != 0));
                U32(kk + 0xA4) = (u32[]){0, 2, 4, 0x20, 0x200}[rnd() % 5] | (rnd() & 0xC8) | (u32[]){0, 1, 0x100}[rnd() % 3];   /* one way to follow */
                U32(kk + 0xA8) = rnd() % 2 ? 0 : rnd();
                if (j == 33) U32(kk + 0xA4) &= rnd() % 2 ? ~0x221u : ~0u;
                if (!(U32(kk + 0xA4) & 0x226)) {                /* follows an object's part */
                    par = O_OBJ + 0x260 * (1 + rnd() % 15);
                    U32(kk + 0xA0) = par; U32(par + 0x6C) = rnd() % 0x2000; U8(par + 0x50) = (u8)((U8(par + 0x50) & ~1) | (rnd() % 4 != 0));
                }
                U8(kk + 0x50) |= 1;                         /* itself in use */
                U32(G() + 0xA70) = rnd() % 4 ? 0 : rnd() % 14;   /* where slots from the back may be taken: */
                for (u32 q = 14; q < 20; q++) U8(O_K2 + 0xAC * q + 0x50) &= ~1;   /* free ones there (no slot stolen) */
            }
            if (j == 29) U8(kk + 0x54) = rnd() % 3;
            if (j == 24) U8(kk + 0xA2) = rnd() % 2;
            if (j == 24 || j == 23) U8(kk + 0x7A) = rnd() % 2;   /* the splash kinds (0x7216F8) */
            if (j == 26 || j == 27) S16(kk + 0x42) = (s16)(S16(kk + 0xA4) + (s32)(rnd() % 3) - 1);
            if (j == 25) S16(kk + 0x7E) = (s16)(0x27 + rnd() % 3);
            U8(G() + 0x852) = rnd() % 4 ? (u8)(rnd() % 20) : 0xFF;
            Flag_Set(3, 0xA, rnd() % 4 == 0);
            U32(G() + 0x7BC) = rnd() % 2 ? 0 : O_ZONE;
            {   /* the camera zone's mask: a rectangle about the middle */
                s32 l = (s32)(rnd() % 0x4000) - 0x3000, b = (s32)(rnd() % 0x4000) - 0x3000, r = l + 0x800 + (s32)(rnd() % 0x2000), t = b + 0x800 + (s32)(rnd() % 0x2000);
                s16 q[8] = {(s16)l, (s16)t, (s16)r, (s16)t, (s16)r, (s16)b, (s16)l, (s16)b};
                for (int c = 0; c < 8; c++) S16(O_ZONE + 0x1C + 2 * c) = q[c];
            }
            if (j == 10 && rnd() % 2) {                     /* Grow: the edges 0x100, 0 */
                if (rnd() % 2) U16(kk + 0x6E) = (u16)(0x100 - U16(kk + 0x9E) + rnd() % 3 - 1);
                else { U16(kk + 0x6E) = (u16)(rnd() % 0x80); U16(kk + 0x6C) = (u16)(-(s32)U16(kk + 0x9E) + (s32)(rnd() % 3) - 1); }
            }
            if (j == 12 && rnd() % 2) { S16(kk + 0x98) = -1; U16(kk + 0x6E) = (u16)(rnd() % 0x80); U16(kk + 0x9A) = 0; U16(kk + 0x6C) = (u16)(0xFF - U16(kk + 0x9C) + rnd() % 3 - 1); }
            if (j == 22 && rnd() % 2) { U16(kk + 0x6E) = rnd() % 2; U8(kk + 0x70) = (u8)rnd(); }
            if ((j == 26 || j == 27) && rnd() % 2) U16(kk + 0x48) = (u16)((j == 26 ? 0x500 : 0xD00) - U16(kk + 0x98) + rnd() % 3 - 1);
            u32 a[2] = {kk, rnd()};
            if (j == 0 || j == 14) a[1] = 0x6DE100;
            if (j == 14) { a[0] = kk + 0x20; for (int q = 0; q < 3; q++) U16(0x6DE100 + 2 * q) = (u16)rnd(); }
            if (j == 16) a[1] = an + 8 * (rnd() % 8);
            if (j == 17) { a[0] = 0x6DE100; U16(0x6DE100) = (u16)(rnd() % 4000 - 2000); U16(0x6DE104) = (u16)(rnd() % 4000 - 2000); }
            U32(LOG_BASE) = 0;
            sprintf(g_what, "slot %X step %u/%u motion %u kind %02X parent %X", kk, U8(kk + 0x54), U8(kk + 0x55), U8(kk + 0x7B), U8(kk + 0x7A), U32(kk + 0x60));
            check(k[j].addr, k[j].fn, k[j].name, k[j].n, a, j == 1 || j == 15 || j == 17 ? 0xFFFFFFFF : 0);
        }
    }
    stubs_install(0);
    g_ign_objpads = 0;
}

/* ---- effect slots: falling drops, bubbles, a sprite stuck to an effect ------------- */
static void test_k2_drops(void)
{
    static const struct { u32 addr; void *fn; const char *name; int n; } k[] = {
        {0x44B4C0, K2Drops_Start, "K2Drops_Start", 1}, {0x44B540, K2Drops_Back, "K2Drops_Back", 1}, {0x44B660, K2Drops_Spawn, "K2Drops_Spawn", 2},
        {0x44B5A0, K2Drops_Init, "K2Drops_Init", 1}, {0x44B790, K2Drops_Drop, "K2Drops_Drop", 4}, {0x44B730, K2Drops_Run, "K2Drops_Run", 1},
        {0x44B9E0, K2Bubbles_Start, "K2Bubbles_Start", 1}, {0x44BA90, K2Bubbles_Keep, "K2Bubbles_Keep", 1}, {0x44BB60, K2Bubble_Init, "K2Bubble_Init", 1},
        {0x44BCE0, K2Bubble_Rise, "K2Bubble_Rise", 1}, {0x44BD70, K2Bubble_Sink, "K2Bubble_Sink", 1}, {0x44BE00, K2Bubble_Wait, "K2Bubble_Wait", 1},
        {0x44BE10, K2Bubble_Shrink, "K2Bubble_Shrink", 1}, {0x44B4A0, K2_Drops, "K2_Drops", 1}, {0x44BEF0, K2Stuck_Place, "K2Stuck_Place", 1},
        {0x44BE30, K2Stuck_Init, "K2Stuck_Init", 1}, {0x44BF30, K2Stuck_Run, "K2Stuck_Run", 1}, {0x44BF80, K2_Stuck, "K2_Stuck", 1}};
    g_ign_objpads = 1;
    stubs_install(1);
    for (int i = 0; i < iters; i++) {
        for (unsigned j = 0; j < sizeof k / sizeof k[0]; j++) {
            game_state(); objects_setup(); world_setup();
            gptr g = G(), pl = O_OBJ;
            U32(0x87DC88) = 22;
            gptr kk = O_K2 + 0xAC * (rnd() % 20);
            for (u32 a = 0x20; a < 0xAC; a += 4) U32(kk + a) = rnd() % 3 ? rnd() % 0x10000 : rnd();
            U8(kk + 0x50) |= 1; U8(kk + 0x87) = (u8)((kk - O_K2) / 0xAC); U8(kk + 0x86) = rnd() % 4 ? (u8)(rnd() % 20) : 0xFF;
            U8(g + 0x852) = rnd() % 4 ? (u8)(rnd() % 20) : 0xFF;
            U32(kk + 0x8C) = 0; U32(kk + 0x60) = rnd() % 2 ? kk : 0;
            /* the sprite animation lists */
            gptr an = 0x6CF900;
            for (u32 a = 0; a < 0x80; a += 4) U32(an + a) = rnd();
            for (u32 q = 0; q < 16; q++) { U8(an + 8 * q + 4) = (u8)((rnd() % 4 == 0 ? 0x20 * (rnd() % 8) : 0) | (rnd() % 16)); U8(an + 8 * q + 5) = rnd() % 3; }
            U32(kk + 0x90) = an; U32(kk + 0x94) = an + 8 * (rnd() % 8); U16(kk + 0x82) = rnd() % 3;
            U32(kk + 0x70) = an + 8 * (rnd() % 8);
            /* the camera, the screen and the drawing */
            for (int q = 0; q < 3; q++) {
                U16(g + 0x798 + 2 * q) = (u16)rnd(); U16(g + 0x7A0 + 2 * q) = (u16)(rnd() % 8000 - 4000);
                U16(g + 0x7A8 + 2 * q) = (u16)(rnd() % 4000 - 2000);
            }
            for (int q = 0; q < 9; q++) S16(g + 0x778 + 2 * q) = (s16)((q % 4 == 0 ? 0x1000 : 0) + (s32)(rnd() % 0x400) - 0x200);
            for (int q = 0; q < 3; q++) S32(g + 0x78C + 4 * q) = (s32)(rnd() % 600) - 300;
            gptr tr = 0x6DE200; S32(tr + 0x14) = (s32)(rnd() % 400) - 200; S32(tr + 0x18) = (s32)(rnd() % 400) - 200; S32(tr + 0x1C) = (s32)(rnd() % 2400);
            SetTransMatrix(tr); SetGeomScreen(0x100 + rnd() % 0x100); SetGeomOffset(160, 120);
            U8(g + 0xB4B) = rnd() % 4 ? 2 + rnd() % 2 : (u8)rnd();
            U32(0x87DC98) = 0x6D0000; U32(0x87F94C) = 0x6CF800; U32(0x87DE0C) = rnd() & 1;
            S16(pl + 0x42) = (s16)(rnd() % 1000 - 500); S16(pl + 0x40) = (s16)(rnd() % 4000 - 0x3CF0 - 2000); S16(pl + 0x44) = (s16)(rnd() % 4000 + 0x36B0 - 2000);
            if (rnd() % 4 == 0) { S16(pl + 0x40) = (s16)rnd(); S16(pl + 0x44) = (s16)rnd(); }
            /* the drops near the player's height, colours about to run out */
            gptr ps[4] = {kk + 0x40, kk + 0x48, kk + 0x7C, kk + 0x6C}, cs[4] = {kk + 0x64, kk + 0x54, kk + 0x68, kk + 0x70};
            for (int q = 0; q < 4; q++) {
                S16(ps[q]) = (s16)(rnd() % 600 - 300); S16(ps[q] + 4) = (s16)(rnd() % 600 - 300);
                S16(ps[q] + 2) = (s16)(S16(pl + 0x42) + (s32)(rnd() % 5) - 2 - (rnd() % 2 ? (s32)(rnd() % 300) : 0));
                if (q != 1) U8(cs[q]) = rnd() % 2 ? (u8)(6 + rnd() % 12) : (u8)rnd();
            }
            /* bubbles: type 10 slots, some on */
            for (u32 q = 0; q < 22; q++) {
                gptr e = O_K2 + 0xAC * q;
                if (e == kk) continue;
                U8(e + 0x58) = rnd() % 2 ? 0xA : (u8)rnd(); U32(e + 0x50) = rnd() % 3 == 0;
            }
            if (j == 6) { for (u32 q = 0; q < 22; q++) if (O_K2 + 0xAC * q != kk) U8(O_K2 + 0xAC * q + 0x50) &= ~1; U32(g + 0xA70) = 0; }
            S16(kk + 0x7E) = (s16)(rnd() % 2 ? (s32)(rnd() % 7) - 3 - (s32)U16(kk + 0xA2) * 0 : (s32)(rnd() % 2000) - 1000);
            if (j == 9 && rnd() % 2) S16(kk + 0xA2) = (s16)(-S16(kk + 0x7E) + (s32)(rnd() % 3) - 1);
            if (j == 10) { S16(kk + 0xA0) = (s16)(S16(kk + 0x42) + (s32)(rnd() % 130) - 64); U16(kk + 0x46) = rnd() % 2 ? 0 : (u16)rnd(); }
            if (j == 11) U16(kk + 0x46) = (u16)(rnd() % 3 - 1);
            if (j == 12) { U8(kk + 0x55) = rnd() % 2 ? 0 : (u8)rnd(); U16(kk + 0x6C) = rnd() % 3; }
            if (j == 0) { U16(kk + 0xA2) = rnd() % 0x40; Flag_Set(8, U16(kk + 0xA2), rnd() % 2); U32(kk + 0x98) = an + 8 * (rnd() % 8); }
            if (j == 1) U32(kk + 0x9C) = an + 8 * (rnd() % 8);
            if (j == 13) {
                U8(kk + 0x54) = rnd() % 15;
                if (U8(kk + 0x54) == 0) { U16(kk + 0xA2) = rnd() % 0x40; U32(kk + 0x98) = an + 8 * (rnd() % 8); }
                if (U8(kk + 0x54) == 2) U32(kk + 0x9C) = an + 8 * (rnd() % 8);
                if (U8(kk + 0x54) == 7) { for (u32 q = 0; q < 22; q++) if (O_K2 + 0xAC * q != kk) U8(O_K2 + 0xAC * q + 0x50) &= ~1; U32(g + 0xA70) = 0; }
            }
            if (j >= 14) {                                  /* the effects: some of kind 0x14 */
                U32(g + 0x948) = O_K3; U32(0x87DE14) = rnd() % 12;
                for (u32 q = 0; q < 12; q++) { U8(O_K3 + q * 0x34) = (u8)rnd(); U8(O_K3 + q * 0x34 + 1) = rnd() % 3 ? 0x14 : (u8)rnd(); }
                U8(g + 0x1091) = rnd() % 4; U8(kk + 0x7A) = rnd() % 3 ? (u8)(rnd() % 3) : (u8)rnd();
                U32(kk + 0x98) = rnd() % 3 == 0 ? 0 : O_K3 + 0x34 * (rnd() % 12);
                if (j == 17) U8(kk + 0x54) = rnd() % 2;
            }
            if (j == 16 || (j == 17 && U8(kk + 0x54) == 1)) U32(kk + 0x94) = an + 8 * (rnd() % 16);
            u32 a[4] = {kk, ps[0], cs[0], kk + 0x98};
            if (j == 2) { u32 q = rnd() % 4; a[0] = ps[q]; a[1] = cs[q]; }
            if (j == 4) { u32 q = rnd() % 4; a[1] = ps[q]; a[2] = cs[q]; a[3] = kk + (q & 1 ? 0x9A : 0x98); }
            if (j == 4 && rnd() % 3 == 0) {                 /* lands at the screen's edges, x 0 and 320 */
                for (int q = 0; q < 9; q++) S16(g + 0x778 + 2 * q) = q % 4 == 0 ? 0x1000 : 0;
                S32(tr + 0x14) = 0; S32(tr + 0x18) = 0; S32(tr + 0x1C) = 0x200;
                SetTransMatrix(tr); SetGeomScreen(0x100); U8(g + 0xB4B) = 2;
                S16(a[1]) = (s16)((rnd() % 2 ? 320 : -320) + (s32)(rnd() % 5) - 2); S16(a[1] + 2) = (s16)(S16(pl + 0x42) + 1); S16(a[1] + 4) = 0;
                U8(a[2]) = 0x80;
            }
            U32(LOG_BASE) = 0;
            sprintf(g_what, "slot %X step %u player y %d tz %d", kk, U8(kk + 0x54), S16(pl + 0x42), S32(tr + 0x1C));
            check(k[j].addr, k[j].fn, k[j].name, k[j].n, a, 0);
        }
    }
    stubs_install(0);
    g_ign_objpads = 0;
}

/* ---- effects that fill effect slots (0x40EED0, 0x4140E0) --------------------------- */
static void test_eff_slots(void)
{
    static const struct { u32 addr; void *fn; const char *name; } k[] = {
        {0x40EF20, EffSlots_Step, "EffSlots_Step"}, {0x40EEF0, EffSlots_Start, "EffSlots_Start"}, {0x40F210, EffSlots_Wait, "EffSlots_Wait"},
        {0x40EED0, EffSlots, "EffSlots"}, {0x4140E0, EffArea_Start, "EffArea_Start"}, {0x4141A0, EffArea_Run, "EffArea_Run"}};
    g_ign_objpads = 1;
    stubs_install(1);
    for (int i = 0; i < iters; i++) {
        for (unsigned j = 0; j < sizeof k / sizeof k[0]; j++) {
            game_state(); objects_setup(); world_setup();
            gptr g = G();
            for (int q = 0; q < 256; q++) U32(TYPE_TAB + 4 * q) = (u32)(uintptr_t)g_recs[STUB_TYPE];
            U32(0x87DC88) = 20 + rnd() % 3;
            for (u32 q = 0; q < 22; q++) U8(O_K2 + 0xAC * q + 0x50) = (u8)(rnd() % 3 == 0 ? 1 | (rnd() & 6) : rnd() & 6);
            U32(g + 0xA70) = rnd() % 4 ? 0 : rnd() % 20;
            for (u32 q = 0; q < 36 * 0x70; q += 4) U32(O_EFF + q) = rnd();
            for (u32 q = 0; q < 36; q++) U8(O_EFF + q * 0x70 + 0x50) = rnd() % 2;
            gptr e = O_K3 + 0x34 * (rnd() % 12);
            for (u32 a = 0; a < 0x34; a += 4) U32(e + a) = rnd();
            int nrec = 2 + rnd() % 8;
            k2_records(K2_RECS, nrec, rnd() % 3);
            u32 at = rnd() % 3;
            U32(e + 0x10) = K2_RECS + 0x28 * at; U32(e + 0xC) = 0x6CF900 + 8 * (rnd() % 4);
            U8(e + 2) = (u8)(rnd() & 0xF); U8(e + 3) = rnd() % 3;
            S8(e + 0x14) = (s8)(rnd() % 16); U8(e + 0x15) = rnd() % 20; U8(e + 0x17) = rnd() % 2 ? U8(e + 0x15) + (u8)(rnd() % 4) : (u8)(rnd() % 22);
            S8(e + 0x16) = (s8)(U8(K2_RECS + 0x28 * at + 0xF) - rnd() % 2); U8(e + 0x18) = rnd() % 3 ? (u8)(rnd() % 4) : 0;
            if (rnd() % 3 == 0) U16(e + 0x1C) = 0;
            if (rnd() % 4 == 0) S8(e + 0x16) = (s8)(nrec / 2 + 4);   /* past the list's end */
            if (j >= 4) {
                U16(g + 0x67C) = rnd() % 10; U8(e + 3) = rnd() % 3 ? 0 : (u8)rnd();
                U8(e + 0xD) = rnd() % 6; U8(e + 0xC) = (u8)(rnd() % (21 - U8(e + 0xD)));
            }
            U32(LOG_BASE) = 0;
            u32 a[1] = {e};
            sprintf(g_what, "effect %X step %u flags %X time %d next %u/%u", e, U8(e + 3), U8(e + 2), S8(e + 0x16), U8(e + 0x17), U8(e + 0x15));
            check(k[j].addr, k[j].fn, k[j].name, 1, a, 0);
        }
    }
    stubs_install(0);
    g_ign_objpads = 0;
}

/* ---- effect slots: puffs, a glow, lone sprites, screen sparkles and bursts --------- */
static void test_k2_more(void)
{
    static const struct { u32 addr; void *fn; const char *name; int n; } k[] = {
        {0x449FD0, K2Puff_Init, "K2Puff_Init", 1}, {0x44A160, K2Puff_Move, "K2Puff_Move", 1}, {0x44A210, K2Puff_Wait, "K2Puff_Wait", 1},
        {0x44A250, K2Puff_Shrink, "K2Puff_Shrink", 1}, {0x449F90, K2_Puff, "K2_Puff", 1}, {0x44A2A0, K2Glow_Init, "K2Glow_Init", 1},
        {0x44A300, K2Glow_Dim, "K2Glow_Dim", 1}, {0x44A330, K2Glow_Brighten, "K2Glow_Brighten", 1}, {0x44A360, K2Glow_Place, "K2Glow_Place", 1},
        {0x44A280, K2_Glow, "K2_Glow", 1}, {0x44A400, K2Lone_Init, "K2Lone_Init", 1}, {0x44C0A0, K2Lone_Move, "K2Lone_Move", 1},
        {0x44CAF0, K2Lone_End, "K2Lone_End", 1}, {0x44A380, K2_Lone, "K2_Lone", 1}, {0x44A9E0, K2Spark_New, "K2Spark_New", 5},
        {0x44AAD0, K2Spark_Draw, "K2Spark_Draw", 6}, {0x44A4A0, K2Sparks_Init, "K2Sparks_Init", 1}, {0x44A620, K2Sparks_Turn, "K2Sparks_Turn", 1},
        {0x44A890, K2Sparks_Draw, "K2Sparks_Draw", 1}, {0x44AFC0, K2Sparks_Off, "K2Sparks_Off", 1}, {0x44AFD0, K2Burst_New, "K2Burst_New", 3},
        {0x44AD30, K2Bursts_Init, "K2Bursts_Init", 1}, {0x44B030, K2Burst_Draw, "K2Burst_Draw", 5}, {0x44AE80, K2Bursts_Run, "K2Bursts_Run", 1},
        {0x44A480, K2_Sparks, "K2_Sparks", 1}, {0x448080, K2_Detach, "K2_Detach", 1}, {0x48D470, Eff_Off, "Eff_Off", 1}};
    g_ign_objpads = 1;
    stubs_install(1);
    for (int i = 0; i < iters; i++) {
        for (unsigned j = 0; j < sizeof k / sizeof k[0]; j++) {
            game_state(); objects_setup(); world_setup();
            if (j == 0 || j == 4) coll_setup();             /* the floor under a kind 5 puff */
            gptr g = G();
            U32(0x87DC88) = 22;
            gptr kk = O_K2 + 0xAC * (rnd() % 20);
            for (u32 a = 0x20; a < 0xAC; a += 4) U32(kk + a) = rnd() % 3 ? rnd() % 0x10000 : rnd();
            U8(kk + 0x50) |= 1; U8(kk + 0x87) = (u8)((kk - O_K2) / 0xAC); U8(kk + 0x86) = rnd() % 4 ? (u8)(rnd() % 20) : 0xFF;
            U8(g + 0x852) = rnd() % 4 ? (u8)(rnd() % 20) : 0xFF;
            gptr src = 0x6CF000 + 0x40 * (rnd() % 4);
            U8(src) = (u8)rnd(); U8(src + 1) = rnd() % 2; U8(src + 0x18) = (u8)rnd();
            U32(kk + 0x8C) = rnd() % 3 ? src : 0;
            u32 pr = rnd() % 4;
            U32(kk + 0x60) = pr == 0 ? 0 : pr == 1 ? kk : pr == 2 ? O_OBJ + 0x260 * (rnd() % 16) : O_K2 + 0xAC * (rnd() % 20);
            gptr an = 0x6CF900;
            for (u32 a = 0; a < 0x80; a += 4) U32(an + a) = rnd();
            for (u32 q = 0; q < 16; q++) { U8(an + 8 * q + 4) = (u8)((rnd() % 4 == 0 ? 0x20 * (rnd() % 8) : 0) | (rnd() % 16)); U8(an + 8 * q + 5) = rnd() % 3; }
            U32(kk + 0x90) = an; U32(kk + 0x94) = an + 8 * (rnd() % 8); U16(kk + 0x82) = rnd() % 3;
            U32(kk + 0x70) = an + 8 * (rnd() % 8);
            U8(kk + 0x7B) = rnd() % 10; U8(kk + 0x7A) = rnd() % 9; U8(kk + 0x46) = rnd() % 3;
            U32(kk + 0xA4) = O_OBJ + 0x260 * (rnd() % 16);
            Flag_Set(3, 0xA, rnd() % 4 == 0); U32(g + 0x7BC) = rnd() % 2 ? 0 : O_ZONE;
            /* the camera and the screen */
            for (int q = 0; q < 3; q++) {
                U16(g + 0x798 + 2 * q) = (u16)rnd(); U16(g + 0x7A0 + 2 * q) = (u16)(rnd() % 8000 - 4000);
                U16(g + 0x7A8 + 2 * q) = (u16)(rnd() % 4000 - 2000);
            }
            for (int q = 0; q < 9; q++) S16(g + 0x778 + 2 * q) = (s16)((q % 4 == 0 ? 0x1000 : 0) + (s32)(rnd() % 0x400) - 0x200);
            SetGeomScreen(0x100 + rnd() % 0x100); SetGeomOffset(160, 120);
            U8(g + 0xB4B) = rnd() % 4 ? 2 + rnd() % 2 : (u8)rnd();
            U32(0x87DC98) = 0x6D0000; U32(0x87F94C) = 0x6CF800; U32(0x87DE0C) = rnd() & 1;
            Flag_Set(2, 0x10, rnd() % 4 == 0);
            U8(g + 0x7C4) = (u8)rnd(); if (rnd() % 2) U8(kk + 0x59) = U8(g + 0x7C4);
            if ((j >= 14 && j <= 18) || j == 24) {   /* the sparkles' depths as the game makes them (1..511) */
                static const u8 dp[9] = {0x24, 0x2C, 0x34, 0x3C, 0x44, 0x4C, 0x60, 0x68, 0x70};
                for (int q = 0; q < 9; q++) U16(kk + dp[q]) = (u16)(rnd() % 512);
            }
            /* step-specific edges */
            if (j == 1 && rnd() % 2) { U8(kk + 0x7A) = 3 + rnd() % 2; U16(kk + 0xA0) = rnd() % 2; U16(kk + 0x6C) = rnd() % 2; U16(kk + 0x6E) = rnd() % 2; }
            if (j == 2) U16(kk + 0x9E) = rnd() % 3;
            if (j == 3) U16(kk + 0xA2) = rnd() % 0x30;
            if (j == 4) U8(kk + 0x54) = rnd() % 5;
            if (j == 6 || j == 7) { U8(kk + 0x64) = rnd() % 2 ? (u8)(8 * (rnd() % 4)) : (u8)(0x40 + rnd() % 0x20); }
            if (j == 9) U8(kk + 0x54) = rnd() % 3;
            if (j == 13) U8(kk + 0x54) = rnd() % 3;
            if (j == 24) U8(kk + 0x54) = (u8[]){0, 1, 2, 3, 5, 6}[rnd() % 6];
            if (j == 16 && rnd() % 2) { U8(kk + 0x98) = 0; U8(kk + 0x9A) = 0; U8(kk + 0x9C) = 0; }
            if (j == 21 && rnd() % 2) { U8(kk + 0x98) = 0; U8(kk + 0x9A) = 0; U8(kk + 0x9C) = 0; }
            if (j == 25) { U8(kk + 0x7B) = rnd() % 2 ? 3 : (u8)(rnd() % 10); U8(kk + 0x84) = (u8)rnd(); }
            u32 a[6] = {kk, 0, 0, 0, 0, 0};
            if (j == 14 || j == 15) {                       /* one sparkle */
                u32 q = rnd() % 9;
                static const u8 sp[9][5] = {{0x20, 0x74, 0x75, 0x76, 0x92}, {0x28, 0x78, 0x79, 0x77, 0x94}, {0x30, 0x7A, 0x7B, 0x7C, 0x96},
                    {0x38, 0x7E, 0x7F, 0x7D, 0x98}, {0x40, 0x80, 0x81, 0x82, 0x9A}, {0x48, 0x84, 0x85, 0x83, 0x9C},
                    {0x5C, 0x88, 0x89, 0x8A, 0x9E}, {0x64, 0x8C, 0x8D, 0x8B, 0xA0}, {0x6C, 0x8E, 0x8F, 0x90, 0x5A}};
                for (int c = 0; c < 5; c++) a[c + (j == 15)] = kk + sp[q][c];
                if (j == 15) {
                    if (rnd() % 2) S16(kk + sp[q][4]) = (s16)(rnd() % 3 - 1);
                    if (rnd() % 2) for (int c = 1; c < 4; c++) U8(kk + sp[q][c]) = (u8)(rnd() % 12);
                }
            }
            if (j == 20) { a[0] = kk + 0x54; a[1] = kk + 0x76; a[2] = kk + 0x26; }
            if (j == 22) {                                  /* one bit of a burst */
                static const u8 bu[7][4] = {{0x20, 0x54, 0x76, 0x26}, {0x28, 0x66, 0x7A, 0x2E}, {0x30, 0x6A, 0x7E, 0x36}, {0x38, 0x6E, 0x82, 0x3E},
                    {0x40, 0x72, 0x86, 0x46}, {0x48, 0x98, 0x8A, 0x4E}, {0x5C, 0x9C, 0x8E, 0x62}};
                u32 q = rnd() % 7;
                for (int c = 0; c < 4; c++) a[c + 1] = kk + bu[q][c];
                if (rnd() % 2) U8(kk + bu[q][2] + 3) = (u8)(rnd() % 6);
                if (rnd() % 2) S16(kk + bu[q][1]) = (s16)(-14 + (s32)(rnd() % 8));
                if (rnd() % 2) S16(kk + bu[q][3]) = (s16)(rnd() % 3 - 1);
                if (rnd() % 2) for (int c = 0; c < 3; c++) U8(kk + bu[q][2] + c) = (u8)(rnd() % 12);
                /* a point in front of the camera */
                S16(kk + bu[q][0]) = (s16)(rnd() % 600 - 300); S16(kk + bu[q][0] + 2) = (s16)(rnd() % 600 - 300); S16(kk + bu[q][0] + 4) = (s16)(rnd() % 600 - 300);
                S16(kk + 0x92) = (s16)(rnd() % 200 - 100); S16(kk + 0x94) = (s16)(rnd() % 200 - 100); S16(kk + 0x96) = (s16)(rnd() % 4000);
            }
            U32(LOG_BASE) = 0;
            sprintf(g_what, "slot %X step %u kind %u motion %u parent %X", kk, U8(kk + 0x54), U8(kk + 0x7A), U8(kk + 0x7B), U32(kk + 0x60));
            check(k[j].addr, k[j].fn, k[j].name, k[j].n, a, 0);
        }
    }
    stubs_install(0);
    g_ign_objpads = 0;
}

/* ---- small helpers (misc.c): camera, masks, gauge, shake, fades, sound slots,
   hidden flags, messages, movies ------------------------------------------------- */
#define HB 0x6ED000u            /* scratch data the helpers read */
#define BOX_T 0x87FF80u         /* the message box */
static void test_helpers(void)
{
    static const struct { u32 addr; void *fn; const char *name; int n; u32 rm; } k[] = {
        {0x403480, Camera_Reset, "Camera_Reset", 0}, {0x403520, Camera_ZoneTick, "Camera_ZoneTick", 0}, {0x4036F0, Masks_Setup, "Masks_Setup", 3},
        {0x403A30, Cutscene_End, "Cutscene_End", 0}, {0x404940, Gauge_Clear, "Gauge_Clear", 0}, {0x406020, Gauge_Show, "Gauge_Show", 0},
        {0x407170, Gauge_Reset, "Gauge_Reset", 0}, {0x46B8C0, Room_SetLighting, "Room_SetLighting", 0}, {0x46FBC0, Shake_Set, "Shake_Set", 3},
        {0x452370, Shake_Tick, "Shake_Tick", 0}, {0x452BA0, Shake_Off, "Shake_Off", 0}, {0x4509C0, Dim_Tick, "Dim_Tick", 0},
        {0x450A70, Fade_Set, "Fade_Set", 3}, {0x450B10, Fade_Draw, "Fade_Draw", 1}, {0x450AF0, Fade_Tick, "Fade_Tick", 0},
        {0x445D10, Eff0_Trail, "Eff0_Trail", 0}, {0x478790, Lights_Unmark, "Lights_Unmark", 0}, {0x48C080, Snd_ReleaseAll, "Snd_ReleaseAll", 0},
        {0x48C5D0, Snd_SetFade, "Snd_SetFade", 4}, {0x48CE20, Strm_SetFade, "Strm_SetFade", 4}, {0x48CD70, Strm_Start, "Strm_Start", 2},
        {0x48C9C0, Strm_StartMask, "Strm_StartMask", 1}, {0x48CA40, Snd_LeaveRoom, "Snd_LeaveRoom", 0}, {0x48CB40, Snd_FadeAll, "Snd_FadeAll", 0},
        {0x48CEB0, Snd_FreeBank, "Snd_FreeBank", 1}, {0x48CF10, Snd_FreeStream, "Snd_FreeStream", 1}, {0x48F910, Obj_UpdateHidden, "Obj_UpdateHidden", 1},
        {0x48F980, Eff_UpdateHidden, "Eff_UpdateHidden", 1}, {0x48FA70, Hidden_UpdateAll, "Hidden_UpdateAll", 0}, {0x48FB50, Light_SetFar, "Light_SetFar", 1},
        {0x491B10, Area_SoundArea, "Area_SoundArea", 2, 0xFF}, {0x494DE0, Text_Left, "Text_Left", 1, 0xFFFFFFFF}, {0x471A30, Msg_Clear, "Msg_Clear", 0},
        {0x471A50, Msg_ClearBox, "Msg_ClearBox", 0}, {0x471A70, Msg_SetScale, "Msg_SetScale", 2}, {0x470CB0, Msg_Reset, "Msg_Reset", 0},
        {0x470CF0, Msg_Tick, "Msg_Tick", 0}, {0x4718F0, Message_Show, "Message_Show", 4, 0xFFFFFFFF}, {0x471AC0, Msg_Flush, "Msg_Flush", 0},
        {0x473130, Movie_Start, "Movie_Start", 1}, {0x4731F0, Movie_Tick, "Movie_Tick", 0},
        {0x470D30, MsgSlot_Step, "MsgSlot_Step", 1}, {0x470D50, Box_Type, "Box_Type", 0}, {0x470F40, Box_Wait, "Box_Wait", 0},
        {0x470F70, Box_Prompt, "Box_Prompt", 0}, {0x4710D0, Box_Release, "Box_Release", 0}, {0x4710F0, Box_YesNo, "Box_YesNo", 0},
        {0x4712B0, Box_Choose, "Box_Choose", 0}, {0x4713C0, Box_Close, "Box_Close", 0},
        {0x48C220, Se_Info, "Se_Info", 2}, {0x48C2D0, Se_Calc, "Se_Calc", 5}, {0x48C3B0, Se_Remap, "Se_Remap", 2, 0xFFFFFFFF},
        {0x48C4F0, Se_Start, "Se_Start", 6}, {0x48C0B0, Se_Play, "Se_Play", 2}, {0x48C640, Se_PlayFlat, "Se_PlayFlat", 3},
        {0x48C600, Se_StopGroup, "Se_StopGroup", 1}, {0x48CE60, Strm_Halt, "Strm_Halt", 1}, {0x494BA0, Text_Draw, "Text_Draw", 7, 0xFFFFFFFF},
        {0x478CF0, Lights_Run, "Lights_Run", 0}, {0x478DF0, Pad_Map, "Pad_Map", 2}, {0x478BF0, Input_Frame, "Input_Frame", 0},
        {0x46BC20, Light_Try, "Light_Try", 3, 0xFFFFFFFF}, {0x46BAC0, Part_Lights, "Part_Lights", 2}, {0x46BA00, Obj_PickLights, "Obj_PickLights", 1},
        {0x48FAE0, Color_Pulse, "Color_Pulse", 3, 0xFFFFFFFF}, {0x406810, Hud_DrawArea, "Hud_DrawArea", 4}, {0x406C90, Hud_Tile, "Hud_Tile", 3},
        {0x406E30, Hud_Sprite, "Hud_Sprite", 6}, {0x406D50, Hud_Picture, "Hud_Picture", 6}, {0x4062C0, Gauge_Bar, "Gauge_Bar", 2, 0xFFFFFFFF},
        {0x406050, Gauge_Draw, "Gauge_Draw", 2},
        {0x496410, Game_InitStock, "Game_InitStock", 0},
        {0x406F10, Hud_Glyph, "Hud_Glyph", 6}, {0x4070A0, Hud_Line, "Hud_Line", 6},
        {0x406890, Hud_Number, "Hud_Number", 6}, {0x406A10, Hud_SmallNum, "Hud_SmallNum", 6},
        {0x406B50, Hud_GlyphNum, "Hud_GlyphNum", 6}, {0x404CC0, Hud_Weapon, "Hud_Weapon", 0},
        {0x404CC0, Hud_Weapon, "Hud_Weapon", 0}, {0x404CC0, Hud_Weapon, "Hud_Weapon", 0}, {0x404CC0, Hud_Weapon, "Hud_Weapon", 0},
        {0x405DE0, Timer_Set, "Timer_Set", 1}, {0x474B20, Eff_Detach, "Eff_Detach", 1}, {0x47A8C0, Obj_MoveReset, "Obj_MoveReset", 2},
        {0x48FDA0, Room_CountUp, "Room_CountUp", 0, 0xFF}, {0x495D70, Item_Compact, "Item_Compact", 2}, {0x495DF0, Item_Merge, "Item_Merge", 0},
        {0x41F080, Panel_Sprite, "Panel_Sprite", 6}, {0x41F1C0, Panel_Digits, "Panel_Digits", 5}, {0x41F170, Panel_Bonus, "Panel_Bonus", 1},
        {0x41F390, Panel_Kills, "Panel_Kills", 1}, {0x41F3E0, Panel_Points, "Panel_Points", 1},
        {0x405E40, Timer_Draw, "Timer_Draw", 0}, {0x4065A0, Boss_Bar, "Boss_Bar", 2}, {0x406520, Boss_Gauge, "Boss_Gauge", 2},
        {0x495E90, Menu_Open, "Menu_Open", 5}, {0x452450, Game_ModeStock, "Game_ModeStock", 0}};
    g_ign_objpads = 1;
    stubs_install(1);
    for (int i = 0; i < iters; i++) {
        for (unsigned j = 0; j < sizeof k / sizeof k[0]; j++) {
            game_state(); objects_setup(); world_setup();
            gptr g = G();
            u32 a[7] = {rnd(), rnd(), rnd(), rnd(), rnd(), rnd(), rnd()};
            for (u32 q = 0; q < 0x200; q += 4) U32(HB + q) = rnd();
            U32(0x87DC98) = 0x6D0000; U32(0x87F94C) = 0x6CF800; U32(0x87DE0C) = rnd() & 1;
            for (u32 q = 0x660; q < 0x830; q += 4) if (q < 0x778 + 0x8C ? q >= 0x68C && q < 0x760 : q >= 0x804) U32(g + q) = rnd();   /* gauge, fade blocks */
            if (j == 0) for (u32 q = 0x778; q < 0x810; q += 4) U32(g + q) = rnd();
            U32(0x746C9C) = rnd(); U32(0x746CA0) = rnd();
            for (u32 q = 0x87FF80; q < 0x87FFA0; q += 4) U32(q) = rnd();   /* the message box block */
            U32(0x87FBD4) = rnd();
            for (u32 q = 0x138C; q < 0x1500; q += 2) U16(g + q) = (u16)rnd();
            switch (j) {
            case 1: camera_state(); Flag_Set(1, 0xA, rnd() % 3 == 0); U8(g + 0x778 + 0x4C) = rnd() % 4 ? rnd() % 4 : (u8)rnd(); break;
            case 2: a[0] = HB; a[1] = rnd() % 4 ? rnd() % 6 : 0; break;
            case 3: Flag_Set(2, 0, rnd() & 1); break;
            case 7: U32(g + 0xA3C) = HB; break;
            case 8: if (rnd() % 3 == 0) a[0] |= 0x80000000u; else a[0] &= 0xFF; break;
            case 9: Flag_Set(2, 0xC, rnd() % 4 != 0); U8(g + 0xB72) = rnd() % 2; U8(g + 0xB4F) = rnd() % 3 ? rnd() % 8 : 0;
                    U8(g + 0xB4E) = (u8)(U8(g + 0xB4F) && rnd() % 2 ? U8(g + 0xB4F) - 1u : rnd() % 8); break;
            case 11: U8(g + 0xB12) = rnd() % 5; Flag_Set(2, 7, rnd() & 1);
                     U16(g + 0xB10) = rnd() % 2 ? (u16)(0xF0 + rnd() % 0x20) : (u16)(rnd() % 30); break;
            case 12: a[0] = rnd() % 4 ? rnd() % 3 : (u8)rnd(); a[1] = 1 + rnd() % 60; a[2] = rnd() % 4;
                     if (rnd() % 3 == 0) { U8(g + 0x814) = (u8)a[0]; } break;
            case 13: case 14:
                     a[0] = g + 0x804; U8(g + 0x816) = rnd() % 5; U8(g + 0x814) = (u8)rnd(); U8(g + 0x815) = rnd() % 3;
                     Flag_Set(2, 0x1D, rnd() % 4 == 0); Flag_Set(2, 0x16, rnd() % 4 == 0); Flag_Set(2, 6, rnd() & 1);
                     if (j == 14) { U8(g + 0x816) = rnd() % 4; } break;
            case 15: {
                     gptr e = O_EFF; U32(g + 0x93C) = e;
                     U32(0x87DE04) = rnd() % 4 ? 1 + rnd() % 20 : 0; U32(e + 0x50) = (rnd() & 0x8001) | (rnd() % 4 ? 0x8001 : 0);
                     U8(e + 0x4E) = rnd() % 3 ? 3 : (u8)rnd(); U8(e + 0x4F) = (u8)(rnd() % 3 ? rnd() % 3 : rnd());
                     U8(e + 0x46) = (u8[]){0, 0, 1, 1, 2, 0xFF}[rnd() % 6]; U16(e + 0x66) = (u16)(rnd() % 6 - 1);
                     U32(e + 0x54) = rnd() % 3 ? O_OBJ + 0x260 * (rnd() % 16) : 0;
                     for (u32 q = 0x74AFB0; q < 0x74B030; q += 4) U32(q) = rnd();
                     break; }
            case 18: case 19: a[0] = rnd() % 4 ? rnd() % 4 : rnd() % 4 + 0x10000 * (rnd() % 4); break;
            case 20: case 21: case 22: case 23: case 25: case 17:
                     for (u32 q = 0; q < 4; q++) { gptr st = 0x87FB80 + q * 0x14; U32(st + 4) = rnd() % 3 ? 0x6DE300u : 0u; U8(st + 8) = rnd() % 3; U8(st + 0x10) = (u8)rnd(); }
                     for (u32 q = 0; q < 32; q++) { gptr sn = 0x87F980 + q * 0x10; U8(sn + 8) = rnd() % 2; U8(sn + 0xA) = (u8)rnd(); }
                     Flag_Set(0, 0xA, rnd() % 4 == 0); U8(g + 0x14ED) = (u8)rnd();
                     stub_at(0x4BFAA0)->ret = rnd() % 2;
                     a[0] = rnd() % 4; a[1] = rnd();
                     if (j == 21) a[0] = rnd() % 2 ? rnd() & 0xFF : rnd();
                     if (j == 22) { U32(0x7593B4) = HB; U8(HB + 8) = rnd() % 10; U8(HB + 9) = rnd() % 8;
                                    U16(0x87FBD4) = rnd() % 2 ? U16(Room_SoundSlot(U8(HB + 9), U8(HB + 8))) ^ (u16)(rnd() % 0x100) : (u16)rnd(); }
                     break;
            case 24: a[0] = rnd() % 4; U32(0x87FBE0 + a[0] * 8) = rnd() % 3 ? 0x6DE300u : 0u; U32(0x87FBE4 + a[0] * 8) = HB + 4 * (rnd() % 2); U32(0x87FBE8 + a[0] * 8) = rnd();
                     for (u32 q = 0; q < 0x100; q += 8) { U32(HB + q) = rnd() % 2 ? 0 : rnd(); } break;
            case 26: case 27: case 28: {
                     Flag_Set(3, 0xA, rnd() % 4 == 0); U32(g + 0x7BC) = rnd() % 4 ? O_ZONE : 0;
                     s16 q[8] = {-0x800, 0x800, 0x800, 0x800, 0x800, -0x800, -0x800, -0x800};
                     for (int c = 0; c < 8; c++) S16(O_ZONE + 0x1C + 2 * c) = q[c];
                     for (u32 n = 0; n < 16; n++) {
                         gptr o = O_OBJ + 0x260 * n; U32(o + 0x50) = rnd() & 7;
                         S16(o + 0x40) = (s16)(rnd() % 0x2000 - 0x1000); S16(o + 0x44) = rnd() % 4 ? (s16)(rnd() % 0x2000 - 0x1000) : (s16)(rnd() % 3 ? 0x7D00 + (s32)(rnd() % 3) - 1 : -0x7D00 + (s32)(rnd() % 3) - 1);
                     }
                     for (u32 n = 0; n < 28; n++) {
                         gptr e = O_EFF + 0x70 * n; U32(e + 0x50) = rnd() & 7; U32(e + 0x54) = rnd() % 3 ? 0u : O_OBJ;
                         S16(e + 0x40) = (s16)(rnd() % 0x2000 - 0x1000); S16(e + 0x44) = (s16)(rnd() % 0x2000 - 0x1000);
                     }
                     U32(g + 0x93C) = O_EFF; U32(0x87DE04) = rnd() % 28; U32(0x87F940) = rnd() % 17;
                     a[0] = j == 26 ? O_OBJ + 0x260 * (rnd() % 16) : O_EFF + 0x70 * (rnd() % 28);
                     break; }
            case 30: a[0] = (rnd() % 2 ? 4 : rnd() % 12) | (rnd() % 4 ? 0 : rnd() & ~0xFFu); a[1] = rnd() % 16 | (rnd() % 4 ? 0 : rnd() & ~0xFFu); break;
            case 31: {
                     gptr t = HB; u32 n = rnd() % 40;
                     for (u32 q = 0; q < n; q++) U16(t + 2 * q) = rnd() % 6 == 0 ? 0x4000 : (u16)(((rnd() % 4) << 12) | rnd() % 0x60);
                     U16(t + 2 * n) = 0xF000; a[0] = t; break; }
            case 34: break;
            case 36: case 37: case 35: case 32:
                     for (gptr sl = 0x74D980; sl < 0x74DB80; sl += 0x10) { U8(sl) = rnd() % 3 == 0; U8(sl + 1) = rnd() % 7; }
                     if (rnd() % 4 == 0) { for (gptr sl = 0x74D980; sl < 0x74DB80; sl += 0x10) U8(sl) |= 1; }
                     U8(0x87FF80) = rnd() % 2; U8(0x87FF82) = rnd() % 8; Flag_Set(2, 0xC, rnd() & 1);
                     a[3] = rnd() % 4 ? rnd() % 6 : rnd(); a[2] = HB; break;
            case 39: a[0] = rnd() % 10; stub_at(0x4BF3A0)->ret = rnd() % 4 == 0; break;
            case 40: U32(0x74DB84) = rnd() % 3 ? rnd() % 10 : 0xFFFFFFFF; U32(0x74DB88) = rnd() % 3; break;
            case 41: a[0] = 0x74D980 + 0x10 * (rnd() % 32); U8(a[0] + 2) = rnd() % 4; break;
            case 42: case 43: case 44: case 45: case 46: case 47: case 48: {
                     gptr t = HB;                       /* a text with every code */
                     u32 n = 2 + rnd() % 30;
                     for (u32 q = 0; q < n; q++) {
                         u32 c = rnd() % 3 ? 0 : (u32[]){1, 2, 3, 4, 5, 6, 7, 8, 9, 0xF}[rnd() % 10];
                         U16(t + 2 * q) = (u16)(c << 12 | (rnd() % 4 ? rnd() % 4 : rnd() & 0xFFF));
                     }
                     U16(t + 2 * n) = 0xF000;
                     U32(BOX_T + 4) = rnd() % 4 ? t + 2 * (rnd() % n) : t + 2 * n;
                     U16(BOX_T + 8) = rnd() % 3 ? (u16)(rnd() % 0x80) : (u16)(rnd() % 0x2000); U16(BOX_T + 0xA) = (u16)(rnd() % 0x40);
                     U16(BOX_T + 0xC) = rnd() % 2 ? (u16)(rnd() % 0x18) : (u16)(rnd() % 3); U8(BOX_T + 0xE) = rnd() % 0x40; U8(BOX_T + 0xF) = rnd() % 4;
                     U8(BOX_T) = (u8)(1 | (rnd() & 2)); U8(BOX_T + 2) = (u8)(rnd() % 8); U8(BOX_T + 3) = (u8)rnd();
                     U8(BOX_T + 0x10) = rnd() % 0x40; U8(BOX_T + 0x11) = rnd() % 0x40;
                     u32 bt[] = {0, 0x20, 0x40, 0x60, 0x2000, 0x8000, 0xA000, 0x8020, 1, 0x2001, 0x8001};
                     U32(0x87DC00) = bt[rnd() % 11]; U32(0x87DDFC) = rnd() % 2 ? 0x20 : 0;
                     for (u32 q = 0; q < 0x8C; q++) U32(0x74D750 + 4 * q) = rnd() % 4 ? 0x1000000 | rnd() % 0x100 : 0x2000000 | rnd() % 5;
                     for (gptr sl = 0x74D980; sl < 0x74DB80; sl += 0x10) U8(sl) = rnd() % 3 == 0;
                     if (rnd() % 4 == 0) U32(0x74D980) = 0x2000000 | (u32)(U8(BOX_T + 0xF) + rnd() % 3 - 1);   /* the one after the marks */
                     break; }
            }
            if (j >= 49) {                              /* sound effects: objects in and out of the sound areas */
                for (u32 q = 0; q < 32; q++) { gptr sn = 0x87F980 + q * 0x10; U8(sn + 8) = rnd() % 3 != 0; U32(sn) = rnd() % 0x400; U8(sn + 9) = rnd() % 4; }
                for (u32 q = 0; q < 4; q++) { gptr st = 0x87FB80 + q * 0x14; U32(st + 4) = rnd() % 3 ? 0x6DE300u : 0u; U8(st + 8) = rnd() % 3; }
                gptr o = O_OBJ + 0x260 * (rnd() % 16);
                gptr ar = 0x6EC000 + 4 + 0x14 * (rnd() % 4);
                S16(o + 0x40) = (s16)(rnd() % 2 ? (S16(ar + 4) + S16(ar + 8)) / 2 : (s16)rnd()); S16(o + 0x44) = (s16)(rnd() % 2 ? (S16(ar + 6) + S16(ar + 0xE)) / 2 : (s16)rnd());
                U16(o + 0x5A) = rnd() % 3 ? 0 : (u16)(rnd() % 3); U8(o + 0x58) = (u8[]){3, 6, 8, 9, 0xC, 0x14, 0x1A, 0x1B, 1}[rnd() % 9];
                U8(g + 0x927) = rnd() % 2; U8(0x87E747) = rnd() % 3; Flag_Set(2, 0xF, rnd() % 3 == 0);
                U8(g + 0x14EC) = rnd() % 2; U8(g + 0x14EE) = (u8)rnd();
                for (int c = 0; c < 3; c++) S16(g + 0x7B0 + 2 * c) = (s16)(S16(o + 0x40 + 2 * c) + (s32)(rnd() % 4000) - 2000);
                u32 ids[] = {0x30, 0x31, 0x50, 0x51, 0x10, 0x11, 0x1A, 0x21, 0x2F, 0x38, 0};
                u32 id = rnd() % 4 ? ids[rnd() % (rnd() % 2 ? 4 : 11)] : rnd() % 0x100;
                if (rnd() % 4) {                        /* standing in one of the areas, on its level */
                    u32 na = 1 + rnd() % 4, ai = rnd() % na; U32(0x6EC000) = na;
                    ar = 0x6EC000 + 4 + 0x14 * ai;
                    S16(o + 0x40) = (s16)((S16(ar + 4) + S16(ar + 8)) / 2); S16(o + 0x44) = (s16)((S16(ar + 6) + S16(ar + 0xE)) / 2);
                    U16(o + 0x5A) = (u16)(s16)S8(ar + 2); U8(ar + 3) = rnd() % 4; U8(ar + 1) = rnd() % 2;
                }
                U8(o + 0x59) = (u8)rnd(); Flag_Set(2, 0x10, rnd() % 2);
                a[0] = rnd() % 4 ? o : 0;
                switch (j) {
                case 49: a[0] = rnd() % 0x120; a[1] = HB; break;
                case 50: a[1] = rnd() % 0x80; a[2] = HB; a[3] = HB + 4; a[4] = rnd() % 2 ? 1000000 : 5000000;
                         if (a[0] && rnd() % 2) { a[4] = Camera_Distance(g + 0x7B0, a[0] + 0x40) + rnd() % 3 - 1; if (!a[4] || a[4] > 0x7FFFFFFF) a[4] = 1; } break;
                case 51: a[1] = id; break;
                case 52: a[1] = a[0]; a[0] = rnd() % 0x400; a[2] = 0x6E9000 + 8 * (rnd() % 0x80); a[3] = rnd() % 0x80; a[4] = rnd() % 0x80; a[5] = id;
                         if (rnd() % 3 == 0) { a[0] = U32(0x87F980 + 0x10 * (rnd() % 32)); } break;
                case 53: a[1] = id; break;
                case 54: a[0] = rnd() % 0x120; a[1] = rnd() % 0x80; a[2] = rnd() % 0x80; break;
                case 55: a[0] = rnd() % 5; break;
                case 56: a[0] = rnd() % 4 | (rnd() % 4 ? 0 : 0x10000); break;
                }
            }
            if (j == 57) {
                gptr t = HB; u32 n = rnd() % 40;
                for (u32 q = 0; q < n; q++) U16(t + 2 * q) = rnd() % 6 == 0 ? 0x4000 : (u16)(((rnd() % 4) << 12) | rnd() % 0x60);
                U16(t + 2 * n) = 0xF000;
                a[0] = t; a[1] = rnd() % 0x140 | (rnd() % 4 ? 0 : rnd() & 0xFFFF0000u); a[2] = rnd() % 0xF0; a[3] = rnd() % 4 ? rnd() % 50 : rnd() % 3 ? 0x10000 * (rnd() % 3) + rnd() % 5 : rnd();
                a[4] = rnd(); a[5] = rnd() % 0x400 | (rnd() % 4 ? 0 : 0x10000); a[6] = rnd() % 3 ? rnd() % 3 : 2 + 0x10000 * (rnd() % 2);
            }
            if (j >= 58 && j <= 60) {                 /* lights marked, pads */
                for (u32 q = 0; q < 16; q++) for (u32 l = 0; l < 2; l++) U8(O_OBJ + q * 0x260 + 0x190 + l * 0x18 + 0xC) = (u8)rnd();
                for (gptr l = g + 0x94C; l < g + 0xA3C; l += 0x18) U8(l + 0xC) = (u8)rnd();
                U32(0x87F940) = rnd() % 17; Flag_Set(2, 4, rnd() % 3 == 0); Flag_Set(1, 0, rnd() % 3 == 0); Flag_Set(3, 4, rnd() % 2);
                if (rnd() % 8 == 0) U32(g + 0x940) = 0;
                U16(0x87DC42) = (u16)rnd(); U16(0x87DC66) = (u16)rnd();
                for (u32 q = 0; q < 0x10; q += 2) { U16(0x87DC10 + q) = (u16)rnd(); U16(0x87DC20 + q) = (u16)rnd(); }
                a[0] = 0x87DC10 + 0x10 * (rnd() % 2); a[1] = 0x74E358 + 0x10 * (rnd() % 2);
            }
            if (j >= 61) {                              /* camera light sets near the parts */
                gptr ls = HB;                           /* (G+0xA3C: 0x298 per camera) */
                U32(g + 0xA3C) = ls - 8 - 0x298 * (U8(g + 0x7C4) = rnd() % 2);
                for (u32 q = 0; q < 0x100; q += 4) U32(ls + q) = rnd();
                U8(ls + 6) = rnd() % 4; U8(ls + 7) = rnd() % 4; U8(ls + 0xB) = rnd() % 4;
                gptr pt = O_PART + 0x80 * (rnd() % 20);
                for (u32 q = 0; q < 8; q++) {
                    gptr l = ls + 0x18 + q * 0x14;
                    U8(l + 3) = rnd() % 4 != 0;
                    for (int c = 0; c < 3; c++) U16(l + 4 + 2 * c) = (u16)(S16(pt + 0x40 + 2 * c) + (s32)(rnd() % 2000) - 1000);
                    U16(l + 0xA) = (u16)(rnd() % 3 ? 300 + rnd() % 1500 : rnd() % 3);
                }
                U8(g + 0xF) = (u8)rnd();
                for (int c = 0; c < 3; c++) U16(g + 0xC + 4 + 2 * c) = (u16)(S16(pt + 0x40 + 2 * c) + (s32)(rnd() % 2000) - 1000);
                U16(g + 0xC + 0xA) = (u16)(300 + rnd() % 1500);
                for (u32 q = 0; q < 16; q++) { gptr o = O_OBJ + 0x260 * q; U8(o + 0x50) = (u8)rnd(); U8(o + 0xFE) = rnd() % 4; U32(o + 0x68) = U32(U32(o + 0x64) + 0x54) ? 1 + rnd() % 2 : 1; }
                U32(0x87F940) = rnd() % 17;
                a[0] = pt; a[1] = j == 61 ? ls + 0x18 + 0x14 * (rnd() % 8) : rnd() % 4 ? 0 : 1; a[2] = rnd() % 3;
                if (j == 61 && rnd() % 4 == 0) {        /* exactly at its radius (and one in or out) */
                    gptr l = a[1]; s32 r = 100 + rnd() % 1000;
                    U16(l + 0xA) = (u16)r; U16(l + 4) = (u16)(S16(pt + 0x40) + r + (s32)(rnd() % 3) - 1); U16(l + 6) = U16(pt + 0x42); U16(l + 8) = U16(pt + 0x44);
                }
                if (j == 61 && rnd() % 4 == 0) a[1] = g + 0xC;
            }
            if (j >= 64) {                              /* the health gauge */
                gptr pl = U32(g + 0x940), b = g + 0x68C;
                S16(pl + 0x10A) = (s16)(rnd() % 4 ? 1200 : 1 + rnd() % 3000);
                S16(pl + 0x108) = (s16)(rnd() % 4 ? (s32)(rnd() % (S16(pl + 0x10A) + 1)) : (s32)(rnd() % 2 ? S16(pl + 0x10A) / 4 + (s32)(rnd() % 3) - 1 : S16(pl + 0x10A) / 2 + (s32)(rnd() % 3) - 1));
                if (rnd() % 8 == 0) S16(pl + 0x108) = (s16)(rnd() % 2 ? -(s32)(rnd() % 100) : S16(pl + 0x10A) + (s32)(rnd() % 100));
                U16(pl + 0x17E) = rnd() % 3 == 0 ? (u16)(1 + rnd() % 3) : 0; U16(pl + 0x174) = rnd() % 3 == 0;
                U16(b + 0xBC) = (u16)(S16(pl + 0x108) + (rnd() % 2 ? (s32)(rnd() % 400) : (s32)(rnd() % 13) - 6));
                U8(b + 0xC8) = rnd() % 6; U16(b + 0xB8) = rnd() % 3 ? (u16)(rnd() % 10) : (u16)(rnd() % 0x80);
                U32(g + 4) = rnd(); U8(g + 0x108E) = rnd() % 3;
                Flag_Set(2, 7, rnd() % 6 == 0); Flag_Set(2, 0xF, rnd() % 6 == 0); Flag_Set(3, 7, rnd() % 6 != 0);
                for (u32 q = 0; q < 0x40; q++) U8(HB + 0x100 + q) = (u8)rnd();
                U8(HB + 0x106) = rnd() % 5;            /* a picture of up to 4 pieces */
                a[0] = rnd(); a[1] = rnd(); a[2] = rnd() % 13; a[3] = rnd(); a[4] = rnd(); a[5] = rnd() % 3 ? rnd() % 2 : rnd();
                switch (j) {
                case 64: a[0] = rnd(); a[1] = rnd(); a[2] = rnd() % 13; break;
                case 65: case 69: case 70: a[0] = rnd() % 0x140; a[1] = rnd() % 0xF0; break;
                case 66: a[0] = HB + 0x100; a[1] = rnd() & 0xFFFFFF; break;
                case 67: a[0] = HB + 0x100; a[3] = rnd() & 0xFFFF; break;
                case 68: a[0] = HB + 0x100; a[1] = rnd() % 0x140; a[2] = rnd() % 0xF0; a[3] = rnd() % 0x20; a[4] = rnd() & 0xFFFF; break;
                case 72: a[0] = HB + 0x100; U16(a[0]) = (u16)rnd(); U16(a[0] + 2) = (u16)rnd(); a[1] = rnd(); a[2] = rnd(); a[3] = rnd(); a[4] = rnd(); a[5] = rnd() % 3 ? rnd() % 2 : rnd() % 2 ? 0x100u << rnd() % 8 : rnd(); break;
                case 73: for (int q = 0; q < 4; q++) a[q] = rnd() % 3 ? rnd() % 0x200 : rnd(); a[4] = rnd(); a[5] = rnd() % 3 ? rnd() % 2 : rnd() % 2 ? 0x100u << rnd() % 8 : rnd(); break;
                case 74: case 75: case 76: {
                         static const u32 p10[] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};
                         a[0] = rnd() % 4 ? rnd() % 0x140 : rnd(); a[1] = rnd() % 4 ? rnd() % 0xF0 : rnd();
                         a[2] = rnd() % 11 | (rnd() % 4 ? 0 : rnd() & 0xFFFF0000u);
                         a[3] = rnd() % 3 ? rnd() % 2 : (u32[]){0x10000, 0x100, 0x200, 0}[rnd() % 4] | (rnd() % 8 ? 0 : rnd());
                         a[4] = rnd();
                         switch (rnd() % 5) {                /* zeros, a power of ten (and next to it), too big, any */
                         case 0: a[5] = rnd() % 100; break;
                         case 1: a[5] = p10[rnd() % 10] * (rnd() % 10) + rnd() % 3; break;
                         case 2: a[5] = p10[rnd() % 10] - rnd() % 2; break;
                         case 3: a[5] = p10[(u16)a[2] ? ((u16)a[2] - 1) % 10 : 0] * (rnd() % 2 ? 10 + rnd() % 60000 : (rnd() % 2 ? 0x100 : 0x10000) * (1 + rnd() % 8)); break;
                         default: a[5] = rnd(); break;
                         }
                         break; }
                case 77: case 78: case 79: case 80: {   /* the weapon boxes and the ammo pop-up (often: many states) */
                         gptr pl2 = U32(g + 0x940);
                         U8(g + 0x108F) = rnd() % 4 ? (u8[]){0, 1, 2, 5, 6}[rnd() % 5] : rnd() % 8;
                         U8(b + 0xCE) = rnd() % 2 ? 0 : 1 + rnd() % 3; U8(b + 0xCF) = rnd() % 2 ? 0 : 1 + rnd() % 3;
                         U8(g + 0x10B4) = rnd() % 0x20; U8(g + 0x10B5) = rnd() % 4 ? 9 : rnd() % 0x10;
                         for (u32 q = 0; q < 0x14; q++) { gptr it = g + 0x10BC + 12 * q; U8(it) = rnd() % 3 != 0; U8(it + 1) = rnd() % 0x20; U16(it + 8) = (u16)rnd(); }
                         if (rnd() % 2) { gptr it = g + 0x10BC + 12 * (rnd() % 0x14); U8(it) = 1; U8(it + 1) = rnd() % 2 ? U8(g + 0x10B4) : (u8)(U8(g + 0x10B5) + 0x10); }
                         U8(g + 0x1094) = (u8)rnd();
                         U16(pl2 + 0x54) = rnd() % 4 ? (u16[]){0x501, 0x901, 1, 0x101, 0x201}[rnd() % 5] : (u16[]){0x500, 0x900, 0, 0x102}[rnd() % 4];
                         U8(0x87DC08) = (u8)rnd();
                         Flag_Set(9, 0x34, rnd() % 3 == 0); Flag_Set(3, 0xB, rnd() % 2); Flag_Set(9, 0x39, rnd() % 2);
                         U16(g + 0x928) = (u16)rnd(); U16(b + 0xBE) = rnd() % 3 ? U16(g + 0x928) : (u16)(U16(g + 0x928) ^ (rnd() % 2 ? 0x100 : 1));
                         U8(b + 0xCA) = rnd() % 8 ? 1 + rnd() % 7 : rnd() % 2 ? 0 : (u8)rnd();
                         { static const s16 th[8][3] = {{0, 1, -1}, {0, 1, 2}, {0x7F, 0x80, 0}, {0x1E, 0x1F, 0x20}, {0, 1, 2},
                                                        {0, 1, 2}, {0x15, 0x16, 0x17}, {0, 1, -1}};   /* each state's turns */
                           U16(b + 0xC0) = rnd() % 4 ? (u16)th[U8(b + 0xCA) & 7][rnd() % 3] : (u16)(rnd() % 0x200); }
                         U16(b + 0xC4) = rnd() % 4 ? (u16)(rnd() % 4) : (u16[]){0xFFFF, 0x101, 0x8000, 99}[rnd() % 4];
                         U32(b + 0xB4) = rnd() % 2 ? rnd() % 10000000 : rnd();
                         U8(b + 0xCB) = rnd() % 4 ? rnd() % 6 : (u8)rnd();
                         { s16 th[] = {0, 1, -1, 0xB8, 0xC0, 0xB9, 0x1E, 0x20, 0x7C, 0x84, 0x94};
                           U16(b + 0xC2) = rnd() % 3 ? (u16)th[rnd() % 11] : (u16)(rnd() % 0x100); }
                         U8(g + 0x108E) = rnd() % 3;
                         break; }
                case 81: a[0] = rnd() % 3 ? 0 : rnd() % 2 ? rnd() % 1000 : rnd(); Flag_Set(3, 9, rnd() % 2); Flag_Set(2, 0xB, rnd() % 2); break;
                case 82: {
                    gptr e = O_EFF + 0x70 * (rnd() % 8);
                    for (u32 q = 0; q < 0x70; q += 4) U32(e + q) = rnd();
                    U32(e + 0x54) = rnd() % 4 ? O_OBJ + 0x260 * (rnd() % 16) : 0;
                    if (U32(e + 0x54)) for (int q = 0; q < 9; q++) U16(U32(e + 0x54) + 0x20 + 2 * q) = (u16)((s32)(rnd() % 8193) - 4096);
                    a[0] = e; break; }
                case 83: a[0] = O_OBJ + 0x260 * (rnd() % 16); a[1] = rnd() % 3 ? rnd() % 8 : rnd(); break;
                case 84: U8(g + 0x1090) = rnd() % 24; U8(g + 0x1091) = rnd() % 10; break;
                case 85: case 86: {
                    gptr l = rnd() % 2 || j == 86 ? g + 0x11AC : HB;
                    for (u32 q = 0; q < 0x60; q += 4) U32(l + q) = rnd() % 2 ? 0 : rnd();
                    for (u32 q = 0; q < 8; q++) {
                        gptr e = l + 12 * q;
                        U8(e) = rnd() % 3 != 0 ? 0 : (u8)(1 + rnd() % 2); if (rnd() % 2) U8(e) = (u8)(rnd() % 2 ? 1 : 0x100 - 1);
                        U8(e + 1) = rnd() % 4; U16(e + 8) = (u16)(rnd() % 4 ? rnd() % 6 : rnd() % 2 ? 0xFFFF : 0x8000); U16(e + 0xA) = (u16)(rnd() % 6);
                    }
                    a[0] = l; a[1] = rnd() % 4 ? 1 + rnd() % 8 : (u32[]){0, 0x100, 0x108, 1}[rnd() % 4];
                    break; }
                case 87: a[0] = rnd() % 4 ? rnd() % 0x140 : rnd(); a[1] = rnd() % 4 ? rnd() % 0xF0 : rnd(); a[2] = rnd(); a[3] = rnd();
                         a[4] = rnd() % 3 ? rnd() % 2 : (u32)(rnd() % 2 ? 0x100 : rnd()); a[5] = rnd(); break;
                case 88: {
                    static const u32 p10[] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};
                    a[3] = rnd() % 4 ? rnd() % 9 : (u32[]){0x107, 0x100, 0x10A, 7}[rnd() % 4];
                    a[0] = rnd() % 3 ? rnd() % 1000000 : rnd() % 2 ? p10[rnd() % 10] * (rnd() % 10) + rnd() % 2 : rnd();
                    if ((a[3] & 0xFF) < 10 && rnd() % 4 == 0) a[0] = p10[(a[3] & 0xFF) ? (a[3] & 0xFF) - 1 : 0] * (10 + rnd() % 100);
                    a[1] = rnd() % 4 ? rnd() % 0x140 : rnd(); a[2] = rnd() % 4 ? rnd() % 0xF0 : rnd(); a[4] = rnd();
                    break; }
                case 92: {                              /* the event timer: colour bands, minutes, seconds, frames */
                    u32 tot = rnd() % 4 ? 30 * (1 + rnd() % 600) : rnd() % 100;
                    U32(g + 0x12CC) = tot;
                    U32(g + 0x12C8) = rnd() % 4 ? rnd() % (tot + 2) : (u32[]){tot / 10, tot / 10 + 1, tot / 2, tot / 2 + 1, 0, 1, 2, 1800 * 100}[rnd() % 8];
                    Flag_Set(1, 0xE, rnd() % 3 == 0); Flag_Set(3, 9, 1); Flag_Set(2, 0xB, rnd() % 2);
                    break; }
                case 93: case 94: {                     /* the boss gauge: bands by quarter and half, the lost health behind */
                    s16 mx = (s16)(rnd() % 4 ? 1 + rnd() % 3000 : (rnd() % 2 ? 1 : 4 + 4 * (rnd() % 100)));
                    U16(g + 0x92A) = (u16)mx;
                    static const s16 fr[] = {0, 1, -1};
                    s16 hp = rnd() % 3 ? (s16)(rnd() % (mx + 1)) : (s16)((rnd() % 2 ? mx / 4 : mx / 2) + fr[rnd() % 3]);
                    if (rnd() % 8 == 0) hp = (s16)(rnd() % 2 ? -(s32)(rnd() % 100) : mx + (s32)(rnd() % 100));
                    U16(g + 0x928) = (u16)hp;
                    U16(b + 0xBE) = (u16)(hp + (rnd() % 2 ? (s32)(rnd() % 13) - 3 : (s32)(rnd() % (mx + 1))));
                    if (rnd() % 4 == 0) U16(b + 0xBE) = rnd() % 2 ? (u16)(mx + 6 + (s32)(rnd() % 3) - 1) : (u16)(hp + 0x106);   /* full width; the same low byte */
                    Flag_Set(2, 7, rnd() % 4 == 0); Flag_Set(3, 7, rnd() % 4 != 0);
                    a[0] = rnd() % 0x140; a[1] = rnd() % 0xF0; break; }
                case 95: {                              /* the menus by kind, their four arguments */
                    a[0] = rnd() % 4 ? (rnd() % 3 ? 1 + rnd() % 6 : 5) : (u32[]){0, 7, 0x10001, 0x10003, rnd()}[rnd() % 5];
                    a[1] = rnd() % 2 ? (rnd() % 10) << 8 | rnd() % 24 : rnd(); if (rnd() % 4 == 0) a[1] = (a[1] & ~0xFFFFu) | 0x1E;
                    a[2] = rnd(); a[3] = rnd(); a[4] = rnd();
                    U8(g + 0x108E) = rnd() % 3; U32(g + 0xB14) = rnd();
                    for (u32 q = 0x10B0; q < 0x10BC; q += 4) U32(g + q) = rnd();   /* (the entry before the list: the key not found) */
                    for (u32 q = 0; q < 0x14; q++) { gptr it = g + 0x10BC + 12 * q; U8(it) = rnd() % 2; U8(it + 1) = rnd() % 4 ? (u8)(0x17 + rnd() % 4) : (u8)rnd(); }
                    if (rnd() % 2) for (u32 q = 0; q < 0x14; q++) if (U8(g + 0x10BC + 12 * q + 1) == 0x19) U8(g + 0x10BC + 12 * q + 1) = 0x18;   /* no key */
                    U16(g + 0x10B4) = (u16)((rnd() % 3 ? 0x900 : (rnd() % 16) << 8) | (rnd() & 0xFF));
                    U16(g + 0x10B8) = (u16)((rnd() % 3 ? 0x900 : (rnd() % 16) << 8) | (rnd() & 0xFF));
                    Flag_Set(9, 0x38, rnd() % 2); Flag_Set(9, 0x37, rnd() % 2); Flag_Set(2, 8, rnd() % 2); Flag_Set(2, 0x11, rnd() % 2);
                    break; }
                case 96: for (u32 q = 0x10B8; q < 0x1200; q += 4) U32(g + q) = rnd();
                         U8(g + 0x108F) = rnd() % 4 ? rnd() % 13 : (u8)rnd(); break;
                case 89: case 90: case 91:
                    a[0] = HB - 0x2480;
                    U32(HB + 4) = rnd() % 3 ? rnd() % 10000000 : rnd(); U16(HB + 0xA) = (u16)(rnd() % 3 ? rnd() % 100 : rnd());
                    U32(HB + 0xC) = rnd() % 3 ? rnd() % 10000000 : rnd(); break;
                case 71: for (u32 q = 0x10B8; q < 0x1200; q += 4) U32(g + q) = rnd();   /* stock tables (and around) */
                         U8(g + 0x108E) = rnd() % 4 ? rnd() % 3 : (u8)rnd(); Flag_Set(9, 0x74, rnd() % 2); break;
                }
            }
            g_ign_addr = 0x74AFC2; g_ign_len = j == 15 ? 2 : 0;   /* MulMatrix0's pad (original: stack bytes) */
            U32(LOG_BASE) = 0;
            sprintf(g_what, "args %X %X %X %X %X %X %X", a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
            check(k[j].addr, k[j].fn, k[j].name, k[j].n, a, k[j].rm);
        }
    }
    g_ign_len = 0;
    stubs_install(0);
    g_ign_objpads = 0;
}

/* ---- the frame's drawing (draw.c) --------------------------------------------------- */
#define DS_MODEL 0x5B0000u      /* models of the objects, 0x100 each */
#define DS_VERT  0x5B1000u      /* their vertices and normals */
#define DS_POLY  0x5B4000u      /* their triangles and quads */
#define DS_MESH  0x5B8000u      /* the effects' meshes */
#define DS_SPR   0x5BC000u      /* the props' sprite lists */
#define DS_TEXT  0x5BD000u      /* text of the message slots */
#define DS_MASK  0x5BE000u      /* the mask table (0x864490) */
#define DS_PRIM  0x5C0000u      /* the primitive buffer */

static s16 ds_coord(s32 r) { return (s16)((s32)(rnd() % (2 * r + 1)) - r); }
static void ds_matrix(gptr m, int rot)          /* a rotation (or any matrix) and a translation */
{
    if (rot) { gptr a = HB + 0x1F0; for (int k = 0; k < 3; k++) U16(a + 2 * k) = (u16)rnd(); RotMatrix(a, m); }
    else for (int k = 0; k < 9; k++) U16(m + 2 * k) = (u16)ds_coord(0x1000);
    for (int k = 0; k < 3; k++) U32(m + 0x14 + 4 * k) = (u32)(s32)ds_coord(k == 2 ? 0x400 : 0x200) + (k == 2 ? 0x800 : 0);
}
static void ds_svecs(gptr v, u32 n, s32 r)
{
    for (u32 k = 0; k < n; k++) { for (int c = 0; c < 3; c++) U16(v + 8 * k + 2 * c) = (u16)ds_coord(r); U16(v + 8 * k + 6) = (u16)rnd(); }
}
/* a mesh list of n entries of size sz (vertices, then uv/tp/colours at random) */
static void ds_mesh(gptr e, u32 n, u32 sz, u32 nv)
{
    for (u32 k = 0; k < n * sz; k += 4) U32(e + k) = rnd();
    for (u32 k = 0; k < n; k++) {
        ds_svecs(e + k * sz, nv, rnd() % 4 ? 0x180 : 0x2000);
        if (rnd() % 4) {                        /* mostly a square (either way round) */
            s32 h = 0x40 + rnd() % 0xC0, sx = rnd() % 2 ? h : -h;
            static const s32 cx[4] = {-1, 1, -1, 1}, cy[4] = {-1, -1, 1, 1};
            for (u32 v = 0; v < nv; v++) {
                U16(e + k * sz + 8 * v) = (u16)(cx[v] * sx + (s32)(rnd() % 9) - 4);
                U16(e + k * sz + 8 * v + 2) = (u16)(cy[v] * h + (s32)(rnd() % 9) - 4);
                U16(e + k * sz + 8 * v + 4) = (u16)((s32)(rnd() % 9) - 4);
            }
        }
    }
}

static void draw_scene(void)
{
    gptr g = G();
    coll_setup();
    SetGeomOffset(160, 120); SetGeomScreen(rnd() % 4 ? 0x200 : 0x100 + rnd() % 0x300);
    gptr c = g + 0x778;
    int ahead = rnd() % 2;                      /* a camera looking along z, the scene in front of it */
    ds_matrix(c, rnd() % 4 != 0);
    if (ahead) { for (int k = 0; k < 9; k++) U16(c + 2 * k) = k % 4 ? 0 : 0x1000; }
    for (int k = 0; k < 3; k++) U16(c + 0x28 + 2 * k) = (u16)ds_coord(0x800);
    for (int k = 0; k < 3; k++) U16(c + 0x30 + 2 * k) = (u16)ds_coord(0x200);
    U8(g + 0xB4B) = rnd() % 4 ? rnd() % 3 : rnd() % 8;
    U32(0x87DE0C) = rnd() % 2; U32(0x87DC98) = DS_PRIM; U32(0x87F94C) = 0x6CF800;
    /* objects (and their parts): 0x87E760 and G+0x940 */
    u32 no = 1 + rnd() % 4, nvt = 0;
    U32(0x87E760) = O_OBJ; U32(g + 0x940) = O_OBJ; U32(0x87F940) = no;
    gptr vb = DS_VERT;
    for (u32 i = 0; i < no; i++) {
        gptr o = O_OBJ + i * 0x260, m = DS_MODEL + i * 0x100;
        U32(o + 0x50) = rnd() % 5 ? 3 | (rnd() & 0x1000) : rnd() & 0x1003;
        ds_matrix(o + 0x20, 1); for (int k = 0; k < 3; k++) U16(o + 0x40 + 2 * k) = (u16)(S16(c + 0x28 + 2 * k) + ds_coord(0x600));
        if (ahead) U16(o + 0x44) = (u16)(S16(c + 0x2C) + 0x300 + rnd() % 0x900);
        U16(o + 0x4A) = (u16)rnd(); U8(o + 0xFE) = (u8)rnd(); U16(o + 0x104) = (u16)rnd(); U16(o + 0x106) = (u16)rnd();
        U16(o + 0x110) = (u16)(rnd() % 0x2000); U16(o + 0x112) = (u16)(rnd() % 0x2000); U16(o + 0x5A) = rnd() % 4;
        u32 np = 1 + rnd() % 3;
        U8(o + 0x118) = (u8)((rnd() % 3 ? 0x80 : 0) | (rnd() % 4) << 4 | rnd() % np);
        U32(o + 0x60) = m; U32(m) = vb; U32(m + 4) = vb + 0x800;
        U32(o + 0x64) = O_PART + i * 0x400;
        u32 vtot = 0;
        for (u32 j = 0; j < np; j++) {
            gptr q = O_PART + i * 0x400 + j * 0x100;
            for (u32 k = 0; k < 0x100; k += 4) U32(q + k) = rnd();
            ds_matrix(q, 1); ds_matrix(q + 0x20, rnd() % 2);
            for (int k = 0; k < 3; k++) U16(q + 0x40 + 2 * k) = (u16)(S16(o + 0x40 + 2 * k) + ds_coord(0x200));
            U32(q + 0x50) = rnd() % 4 ? 3 : rnd();
            U32(q + 0x54) = j + 1 < np ? q + 0x100 : 0; U32(q + 0x58) = o;
            u32 nt = rnd() % 4 ? 1 + rnd() % 3 : 0;
            if (vtot + nt > 8) nt = 0;
            U8(q + 0x62) = (u8)nt; vtot += nt;
            ds_svecs(q + 0x74, 3, 0x1000);
            for (u32 k = 0; k < 0x20; k += 2) U16(q + 0xAC + k) = (u16)ds_coord(0x1000);
        }
        ds_svecs(vb, vtot * 3, rnd() % 4 ? 0x100 : 0x1000);
        ds_svecs(vb + 0x800, vtot * 3, 0x1000);
        u32 nv = vtot * 3; if (!nv) nv = 1;
        nvt = nvt > nv ? nvt : nv;
        /* groups: triangles and quads over the model's vertices */
        u32 ng = rnd() % 4 ? 1 + rnd() % 4 : (u32)(rnd() % 2 ? 0 : -1);
        U32(m + 0x14) = ng;
        for (u32 k = 0; k < 4; k++) {
            gptr gr = m + 0x24 + k * 0x14, tl = DS_POLY + i * 0x1000 + k * 0x400, ql = tl + 0x200;
            u32 nt = rnd() % 8, nq = rnd() % 6;
            U32(gr) = tl; U32(gr + 4) = ql; U16(gr + 8) = (u16)nt; U16(gr + 0xA) = (u16)nq;
            for (u32 t = 0; t < nt; t++) { for (int v = 0; v < 3; v++) U16(tl + t * 0xC + 2 * v) = (u16)(rnd() % nv); for (int v = 6; v < 0xC; v += 2) U16(tl + t * 0xC + v) = (u16)rnd(); }
            for (u32 t = 0; t < nq; t++) { for (int v = 0; v < 4; v++) U16(ql + t * 0x10 + 2 * v) = (u16)(rnd() % nv); for (int v = 8; v < 0x10; v += 2) U16(ql + t * 0x10 + v) = (u16)rnd(); }
        }
        vb += 0x1000;
    }
    /* the projected vertices (for the polygon drawers alone) */
    for (u32 k = 0; k < 24; k++) {
        U32(0x759F70 + 4 * k) = rnd() % 8 ? rnd() % 0x8000 : rnd() % 2 ? 0 : rnd();
        U32(0x75A61C + 4 * k) = rnd();
        U16(0x75ACAC + 2 * k) = (u16)(rnd() % 0x400);
        static const float ex[] = {0.0f, -0.5f, 320.0f, 320.5f, 240.0f, 240.5f, -1.0f};   /* (the screen's edges) */
        F32(0x75B028 + 8 * k) = rnd() % 4 ? (float)((s32)(rnd() % 400) - 40) : ex[rnd() % 7];
        F32(0x75B028 + 8 * k + 4) = rnd() % 4 ? (float)((s32)(rnd() % 300) - 30) : ex[rnd() % 7];
    }
    /* effects: G+0x93C */
    u32 ne = rnd() % 6;
    U32(g + 0x93C) = O_EFF; U32(0x87DE04) = ne;
    for (u32 i = 0; i < 8; i++) {
        gptr e = O_EFF + i * 0x70, m = DS_MESH + i * 0x800;
        for (u32 k = 0; k < 0x70; k += 4) U32(e + k) = rnd();
        U32(e + 0x50) = rnd() % 5 ? 3 | (rnd() % 3 ? 0 : 0x8000) : rnd();
        U32(e + 0x54) = rnd() % 3 ? 0 : O_OBJ + 0x260 * (rnd() % no);
        ds_matrix(e + 0x20, 1); ds_matrix(e, 1);
        for (int k = 0; k < 3; k++) U16(e + 0x40 + 2 * k) = (u16)(S16(c + 0x28 + 2 * k) + ds_coord(0x600));
        if (ahead) U16(e + 0x44) = (u16)(S16(c + 0x2C) + 0x300 + rnd() % 0x900);
        U16(e + 0x6C) = rnd() % 6; U16(e + 0x6E) = rnd() % 3 ? (u16)(rnd() % 0x400) : (u16)rnd();
        U32(e + 0x68) = m;
        u32 nt = rnd() % 6, nq = rnd() % 7;
        U32(m) = m + 0x10; U32(m + 4) = m + 0x400; U16(m + 8) = (u16)nt; U16(m + 0xA) = (u16)nq;
        ds_mesh(m + 0x10, nt, 0x28, 3); ds_mesh(m + 0x400, nq, 0x34, 4);
        U8(e + 0x4F) = rnd() % 2; U16(e + 0x5E) = rnd() % 2; U16(e + 0x66) = rnd() % 4 ? (u16)(rnd() % 4) : 0;
    }
    for (u32 k = 0; k < 5 * 0x20; k += 0x20) ds_matrix(0x74AFB0 + k, 1);
    /* props: G+0x944 */
    u32 nk = rnd() % 6;
    U32(g + 0x944) = O_K2; U32(0x87DC88) = nk;
    for (u32 i = 0; i < 8; i++) {
        gptr p = O_K2 + i * 0xAC, s = DS_SPR + i * 0x100;
        for (u32 k = 0; k < 0xAC; k += 4) U32(p + k) = rnd();
        U32(p + 0x50) = rnd() % 5 ? 3 : rnd();
        U32(p + 0x60) = rnd() % 3 ? 0 : O_OBJ + 0x260 * (rnd() % no);
        U8(p + 0x84) = (u8)((rnd() % 3 ? 0 : 0x80) | (rnd() & 5));
        for (int k = 0; k < 3; k++) U16(p + 0x40 + 2 * k) = (u16)(S16(c + 0x28 + 2 * k) + ds_coord(0x600));
        if (ahead) U16(p + 0x44) = (u16)(S16(c + 0x2C) + 0x300 + rnd() % 0x900);
        ds_matrix(p + 0x20, 1);
        for (int k = 0; k < 3; k++) U32(p + 0x34 + 4 * k) = (u32)(s32)ds_coord(0x100);
        U32(p + 0x64) = rnd() % 4 ? rnd() : (rnd() & 0xFF000000) | 0xD00000;
        U16(p + 0x6C) = rnd() % 4 ? 1 + rnd() % 8 : (u16)rnd(); U16(p + 0x6E) = rnd() % 4 ? 1 + rnd() % 8 : (u16)rnd();
        U16(p + 0x44) = rnd() % 4 ? (u16)(rnd() % 0x400) : (u16)rnd() % 0x3FF;
        U32(p + 0x94) = s;
        for (u32 k = 0; k < 0x100; k += 4) U32(s + k) = rnd();
        U8(s + 4) = (u8)(rnd() % 6 | (rnd() & 0xE0));
    }
    /* the handlers at G+0x948 (recorders) */
    U32(g + 0x948) = O_K3; U32(0x87DE14) = rnd() % 6;
    for (u32 k = 0; k < 8; k++) { U8(O_K3 + k * 0x34) = (u8)rnd(); U32(O_K3 + k * 0x34 + 4) = rnd() % 4 ? (u32)(uintptr_t)g_recs[STUB_TYPE] : 0; }
    /* text: the slots and the sentence */
    for (gptr s = 0x74D980; s < 0x74DB80; s += 0x10) {
        U8(s) = rnd() % 3 == 0; U8(s + 1) = rnd() % 4 ? (u8)(rnd() % 4) : 4;
        gptr t = DS_TEXT + (s - 0x74D980) * 8;
        U32(s + 4) = t; U32(s + 8) = rnd(); U16(s + 0xC) = (u16)ds_coord(300); U16(s + 0xE) = (u16)ds_coord(200);
        u32 n = rnd() % 12;
        for (u32 q = 0; q < n; q++) U16(t + 2 * q) = (u16)((u32[]){0, 0, 0, 1, 4, 2, 3, 0}[rnd() % 8] << 12 | (rnd() % 4 ? rnd() % 0x80 : rnd() & 0xFFF));
        U16(t + 2 * n) = (u16)(0xF000 | (rnd() & 0xFFF));
    }
    for (u32 q = 0; q < 0x8C; q++)
        U32(0x74D750 + 4 * q) = (u32)(rnd() % 4 ? 1 : rnd() % 3) << 24 | (rnd() % 0x100) << 16 | (rnd() % 2 ? rnd() % 4 : rnd() % 4 ? rnd() % 0x100 : rnd() & 0xFFF);
    U32(0x87FF90) = rnd(); U16(0x87FF8C) = rnd() % 3 ? rnd() % 10 : (u16)rnd(); U8(0x87FF8F) = rnd() % 4;
    /* masks */
    U16(c + 0x50) = rnd() % 3 ? (u16)(rnd() % 4) : 0;
    U8(g + 0xC27) = (u8)rnd(); U32(0x746C9C) = rnd() % 2 ? U8(g + 0xC27) : (u32)(rnd() % 0x100);
    U32(0x864490) = DS_MASK; U32(0x864494) = 32; F32(0x8644B8) = 1.0f / (1 + rnd() % 64);
    for (u32 k = 0; k < 0x20 * 0x24; k += 4) U32(DS_MASK + k) = rnd();
    for (u32 k = 0; k < 0x24; k++) U32(DS_MASK + k * 0x20 + 0x14) = 1 + rnd() % 6;
    U16(g + 0xB10) = rnd() % 2 ? 0 : (u16)rnd();
    for (u32 k = 0; k < U8(CL_ROOM); k++) U8(CL_ROOM + U32(CL_ROOM + 8 + 4 * k) + 3) = (u8[]){0, 0x40, 0x80, 0x20}[rnd() % 4];   /* floors under some marks */
    stub_at(0x4C08A0)->ret = rnd();
    static const u32 fl[][2] = {{2, 0x12}, {3, 1}, {3, 2}, {3, 3}, {3, 4}, {3, 5}, {3, 8}};
    for (unsigned k = 0; k < sizeof fl / sizeof fl[0]; k++) Flag_Set(fl[k][0], fl[k][1], rnd() % 3 != 0);
}

static void test_draw(void)
{
    static const struct { u32 addr; void *fn; const char *name; int n; } F[] = {
        {0x4A6DF0, World_Matrices, "World_Matrices", 0}, {0x4A71B0, Obj_ToView, "Obj_ToView", 2},
        {0x4460A0, Mat_ToView, "Mat_ToView", 2}, {0x4A7510, Obj_DrawAll, "Obj_DrawAll", 0},
        {0x4A8BA0, Model_DrawGroups, "Model_DrawGroups", 3}, {0x4A8C00, Model_Tris, "Model_Tris", 3},
        {0x4A8F40, Model_Quads, "Model_Quads", 3}, {0x4A8470, Mesh_TriTex, "Mesh_TriTex", 3},
        {0x4A8690, Mesh_QuadTex, "Mesh_QuadTex", 3}, {0x4A8930, Mesh_QuadFlat, "Mesh_QuadFlat", 3},
        {0x4A82F0, Z_Fixed, "Z_Fixed", 0}, {0x4A8310, Z_Min3, "Z_Min3", 0}, {0x4A8360, Z_Min4, "Z_Min4", 0},
        {0x4A83C0, Z_Max3, "Z_Max3", 0}, {0x4A8410, Z_Max4, "Z_Max4", 0},
        {0x4A7840, Eff_DrawAll, "Eff_DrawAll", 0}, {0x4A7ED0, Obj_DrawShadows, "Obj_DrawShadows", 0},
        {0x4A7A20, Prop_DrawAll, "Prop_DrawAll", 0}, {0x4A7330, Draw_Handlers, "Draw_Handlers", 0},
        {0x4A7390, Dim_Bars, "Dim_Bars", 0}, {0x403760, Masks_Draw, "Masks_Draw", 1},
        {0x445E10, Rope_Draw, "Rope_Draw", 0}, {0x471440, Text_DrawAll, "Text_DrawAll", 0},
        {0x4A7250, Frame_Draw, "Frame_Draw", 0},
    };
    static const u32 zf[] = {0x4A82F0, 0x4A8310, 0x4A8360, 0x4A83C0, 0x4A8410};
    stubs_install(1);
    g_ign_objpads = 1; g_ign_draw = 1;
    for (int i = 0; i < iters; i++) {
        for (unsigned k = 0; k < sizeof F / sizeof F[0]; k++) {
            game_state();
            draw_scene();
            u32 a[7] = {0};
            gptr o = O_OBJ + 0x260 * (rnd() % U32(0x87F940));
            switch (F[k].addr) {
            case 0x4A71B0: a[0] = rnd() % 2 ? o : O_EFF + 0x70 * (rnd() % 8); a[1] = a[0] + 0x20; break;
            case 0x4460A0: a[0] = 0x74AFB0 + 0x20 * (rnd() % 4); a[1] = HB + 0x100; break;
            case 0x4A7250:              /* no glyphs: their quads carry stack bytes the earlier steps left (Text_DrawAll has its own test) */
                for (gptr q = 0x74D980; q < 0x74DB80; q += 0x10) if (U8(q + 1) != 4) U8(q) &= ~1;
                for (u32 q = 0; q < 0x8C; q++) U8(0x74D750 + 4 * q + 3) = 0;   /* (nor the cursor: placed by the last glyph's width) */
                World_Matrices(); break;
            case 0x4A7ED0:
                if (rnd() % 2) {                /* one floor under everything, its kind against the shadow's mask */
                    gptr w = CL_ROOM + 0x40;
                    U8(CL_ROOM) = 1; U32(CL_ROOM + 8) = 0x40; U32(G() + 0xA4C) = CL_ROOM;
                    U8(w) = 1; U8(w + 1) = 4; U8(w + 3) = (u8[]){0, 0x40, 0x80, 0x20}[rnd() % 4]; U16(w + 4) = 1; U8(w + 6) = rnd() % 4;
                    for (u32 k = 0; k < 4; k++) { U32(w + 8 + 4 * k) = 0; U16(w + 0x18 + 4 * k) = (u16)(k == 1 || k == 2 ? 0x7000 : -0x7000);
                                                   U16(w + 0x1A + 4 * k) = (u16)(k >= 2 ? 0x7000 : -0x7000); }
                    for (u32 k = 0; k < 16 * 0x100; k += 0x100) U16(O_PART + k + 0x42) = (u16)(-1000 - (s32)(rnd() % 3000));
                    for (u32 k = 0; k < 4; k++) U8(O_OBJ + 0x260 * k + 0x118) = (u8)((U8(O_OBJ + 0x260 * k + 0x118) & 0x8F) | 0x20);
                }
                World_Matrices(); break;
            case 0x4A7510: World_Matrices(); break;
            case 0x4A8BA0: a[0] = U32(o + 0x60); a[1] = rnd() % 4 ? rnd() : 0xFFFFFFFF; a[2] = rnd(); break;
            case 0x4A8C00: case 0x4A8F40: {
                gptr gr = U32(o + 0x60) + 0x24 + 0x14 * (rnd() % 4);
                a[0] = U32(gr + (F[k].addr == 0x4A8C00 ? 0 : 4)); a[1] = U16(gr + (F[k].addr == 0x4A8C00 ? 8 : 0xA)); a[2] = rnd();
                break; }
            case 0x4A8470: case 0x4A8690: case 0x4A8930: {
                gptr e = O_EFF + 0x70 * (rnd() % 8), m = U32(e + 0x68);
                World_Matrices(); SetRotMatrix(e); SetTransMatrix(e);
                a[0] = U32(m + (F[k].addr == 0x4A8470 ? 0 : 4)); a[1] = U16(m + (F[k].addr == 0x4A8470 ? 8 : 0xA)); a[2] = zf[rnd() % 5];
                U32(0x75A618) = rnd() % 0x500;     /* (a u16 in the game: never negative) */
                U32(0x75BD54) = rnd() % 0x2000;     /* (a triangle's Z_Min4/Z_Max4 read a fourth z: the last one, never negative) */
                break; }
            case 0x4A82F0: case 0x4A8310: case 0x4A8360: case 0x4A83C0: case 0x4A8410:
                for (u32 q = 0; q < 4; q++) U32(0x75BD48 + 4 * q) = rnd() % 4 ? rnd() % 0x2000 : rnd();
                U32(0x75A618) = rnd() % 2 ? rnd() % 0x500 : rnd();
                break;
            case 0x4A7840: World_Matrices(); break;
            case 0x4A7A20: World_Matrices(); break;
            case 0x403760: a[0] = G() + 0x778; break;
            case 0x445E10: Flag_Set(3, 2, rnd() % 4 != 0); U32(U32(G() + 0x93C) + 0x50) |= rnd() % 2 ? 0x8003 : 0;
                           U32(U32(G() + 0x940) + 0x50) |= 2; if (rnd() % 4) U32(0x87DE04) |= 1; break;
            }
            U32(LOG_BASE) = 0;
            sprintf(g_what, "objects %u effects %u props %u shift %u args %X %X %X", U32(0x87F940), U32(0x87DE04), U32(0x87DC88),
                    U8(G() + 0xB4B), a[0], a[1], a[2]);
            check(F[k].addr, F[k].fn, F[k].name, F[k].n, a, 0);
        }
    }
    g_ign_objpads = 0; g_ign_draw = 0;
    stubs_install(0);
}


/* ---- the PC port's GPU layer (src/pc/gpu.c) ----------------------------------------- */
static void test_pcgpu(void)
{
    gptr ot = 0x5B0000, pr = 0x5B0100, buf = 0x5B1000, rect = 0x5B0F00;
    for (int i = 0; i < iters; i++) {
        for (u32 k = 0; k < 0x100; k += 4) { U32(ot + k) = rnd(); U32(pr + k) = rnd(); }
        if (rnd() % 3 == 0) U8(pr + 7) = 0;                 /* no code: a plain link */
        u32 a[6] = {ot + 4 * (rnd() % 4), pr, pr + 0x40, 0, 0, 0};
        if (rnd() % 3 == 0) U8(pr + 0x47) = 0;
        sprintf(g_what, "ot %X prim %X code %02X", a[0], a[1], U8(pr + 7));
        check(0x4C05D0, PcGpu_AddPrim, "PcGpu_AddPrim", 2, a, 0);
        check(0x4C0600, PcGpu_AddPrims, "PcGpu_AddPrims", 3, a, 0);
        u32 o[6] = {ot + 4 * (rnd() % 4), rnd() % 4 ? rnd() % 0x3C : rnd() % 3 ? (u32)-(s32)(rnd() % 3) : 0, 0, 0, 0, 0};
        check(0x4C0870, PcGpu_ClearOTagR, "PcGpu_ClearOTagR", 2, o, 0);
        u32 e[6] = {pr + rnd() % 4, rnd() % 2 ? rnd() % 0x400 : rnd(), rnd() % 2 ? rnd() % 0x200 : rnd(), rnd(), rnd(), 0};
        check(0x4C07F0, PcGpu_SetDefDispEnv, "PcGpu_SetDefDispEnv", 5, e, 0);
        check(0x4C0820, PcGpu_SetDefDrawEnv, "PcGpu_SetDefDrawEnv", 5, e, 0);
        u32 b[6] = {pr, rnd() % 3 ? rnd() % 2 : rnd(), 0, 0, 0, 0};
        check(0x4C0690, PcGpu_SetSprt, "PcGpu_SetSprt", 1, b, 0);
        check(0x4C06A0, PcGpu_SetTile, "PcGpu_SetTile", 1, b, 0);
        check(0x4C06B0, PcGpu_SetMaskMarker, "PcGpu_SetMaskMarker", 1, b, 0);
        check(0x4C06C0, PcGpu_SetSemiTrans, "PcGpu_SetSemiTrans", 2, b, 0);
        check(0x4C06E0, PcGpu_SetDrawArea, "PcGpu_SetDrawArea", 2, b, 0);
        u32 d[6] = {pr, rnd() % 3 ? rnd() % 2 : rnd(), rnd() % 3 ? rnd() % 2 : rnd(), rnd(), rnd(), 0};
        check(0x4C06F0, PcGpu_SetDrawMode, "PcGpu_SetDrawMode", 5, d, 0);
        u32 t[6] = {rnd() % 2 ? rnd() % 4 : rnd(), rnd() % 2 ? rnd() % 4 : rnd(), rnd() % 2 ? rnd() % 0x400 : rnd(), rnd() % 2 ? rnd() % 0x200 : rnd(), 0, 0};
        check(0x4C08A0, PcGpu_GetTPage, "PcGpu_GetTPage", 4, t, 0xFFFFFFFF);
        /* VRAM: a rect in (or past) 1024 x 512, data from buf */
        s16 w = (s16)(1 + rnd() % 64), h = (s16)(rnd() % 16), x = (s16)(rnd() % 4 ? rnd() % (0x401 - w) : 0x400 - w + 1 + rnd() % 8),
            y = (s16)(rnd() % 4 ? rnd() % (0x201 - h) : 0x200 - h + 1 + rnd() % 8);
        if (rnd() % 4 == 0) x = (s16)(0x400 - w + (s32)(rnd() % 2));   /* at the right edge (and one past) */
        if (rnd() % 4 == 0) y = (s16)(0x200 - h + (s32)(rnd() % 2));
        S16(rect) = x; S16(rect + 2) = y; S16(rect + 4) = w; S16(rect + 6) = h;
        for (u32 k = 0; k < 0x1000; k += 4) U32(buf + k) = rnd();
        u32 v[6] = {rect, buf, 0, 0, 0, 0};
        sprintf(g_what, "rect %d,%d %dx%d", x, y, w, h);
        check(0x4B4D20, PcGpu_LoadImage, "PcGpu_LoadImage", 2, v, 0);
        if (x + w <= 0x400 && y + h <= 0x200) {
            for (u32 k = 0; k < 0x1000; k += 4) U32(buf + k) = rnd();
            check(0x4B49F0, PcGpu_StoreImage, "PcGpu_StoreImage", 2, v, 0);
            /* CLUT rows: some the same as VRAM already */
            for (s32 r = 0; r < h; r++)
                if (rnd() % 2) memcpy(MEM(buf + 2 * w * r), MEM(VRAM_BASE + 2 * ((u32)(y + r) * 0x400 + x)), 2 * w);
                else if (rnd() % 2) U16(buf + 2 * w * r + 2 * (rnd() % w)) ^= (u16)(1u << (rnd() % 16));
            /* the original's palettes converted to its D3D texture format: row stamps and buffers */
            for (u32 r = 0; r < 0x200; r++) U32(0x75CEEC + 8 * r) = 0x5B8000;
            check(0x4B4DB0, PcGpu_LoadImageIfChanged, "PcGpu_LoadImageIfChanged", 2, v, 0);
        }
    }
}

/* ---- the PC layer's drawing, against a fake Direct3D ------------------------------------
 * Fake DirectDraw / Direct3D objects (deterministic addresses: a pool indexed by a
 * counter in game memory) log every call into game memory, so the original's
 * calls and the C copy's are compared with the rest of the memory. */
#define FK_LOG    0x640000u     /* the call log */
#define FK_LOGMAX 0x1E000u
#define FK_LOGN   0x63F000u     /* bytes logged */
#define FK_NOBJ   0x63F004u     /* objects made */
#define FK_HEAPN  0x63F008u     /* the CRT malloc stand-in: bytes given */
#define FK_ARENA  0x5E8000u     /* .. 0x600000 */
typedef struct Fake { void **vt; u32 kind, id, w, h, bpp; u8 *pix; } Fake;
static Fake g_fo[512];
static u8 *g_fpix[512];
static u32 g_fcap[512];
static void *g_fvt[6][48];   /* kinds: 0 material, 1 device, 2 DirectDraw, 3 surface, 4 texture, 5 viewport (and others) */

static void fk_log(u32 v) { u32 n = U32(FK_LOGN); if (n < FK_LOGMAX) { U32(FK_LOG + n) = v; U32(FK_LOGN) = n + 4; } }
static void fk_logmem(const void *p, u32 n) { for (u32 i = 0; i < n; i += 4) { u32 v = 0; memcpy(&v, (const u8 *)p + i, n - i < 4 ? n - i : 4); fk_log(v); } }
static u32 fk_id(u32 p)
{
    if (!p) return 0xFFFFFFFF;
    if (p >= (u32)(uintptr_t)g_fo && p < (u32)(uintptr_t)(g_fo + 512)) return 0x0B000000 | (u32)(((Fake *)(uintptr_t)p) - g_fo);
    return p;
}
static Fake *fk_new(u32 kind, u32 w, u32 h)
{
    u32 id = U32(FK_NOBJ)++;
    if (id >= 512) { printf("fake COM: out of objects\n"); fflush(stdout); TerminateProcess(GetCurrentProcess(), 3); }
    Fake *f = &g_fo[id];
    u32 bpp = U8(0x87BB43) == 4 ? 4 : 2;
    f->vt = g_fvt[kind]; f->kind = kind; f->id = id; f->w = w; f->h = h; f->bpp = bpp;
    if (w) {
        if (g_fcap[id] < w * h * 4) { free(g_fpix[id]); g_fcap[id] = w * h * 4; g_fpix[id] = malloc(g_fcap[id]); }
        memset(g_fpix[id], 0xCD, w * h * bpp);
    }
    f->pix = g_fpix[id];
    return f;
}
static u32 fk_hash(const u8 *p, u32 n) { u32 h = 0x811C9DC5; for (u32 i = 0; i < n; i++) h = (h ^ p[i]) * 0x01000193; return h; }

#define ST __attribute__((stdcall))
/* the device */
static u32 ST d_begin(Fake *t) { (void)t; fk_log(0x10024); return 0; }
static u32 ST d_end(Fake *t) { (void)t; fk_log(0x10028); return 0; }
static u32 ST d_rs(Fake *t, u32 s, u32 v) { (void)t; fk_log(0x10058); fk_log(s); fk_log(v); return 0; }
static int g_fk_nospec;
static u32 g_fk_ssurf, g_fk_stex;
static u32 ST d_dp(Fake *t, u32 type, u32 fvf, void *v, u32 n, u32 fl)
{
    (void)t;
    if (g_fk_nospec && fvf == 0x1C4) for (u32 i = 0; i < n; i++) ((u32 *)v)[i * 8 + 5] = 0;
    u32 st = (fvf & 0xE) == 4 ? 16 : 12;
    if (fvf & 0x40) st += 4;
    if (fvf & 0x80) st += 4;
    st += 8 * (fvf >> 8 & 0xF);
    fk_log(0x10070); fk_log(type); fk_log(fvf); fk_log(n); fk_log(fl); fk_logmem(v, n * st);
    return 0;
}
static u32 ST d_dpvb(Fake *t, u32 type, u32 vb, u32 s, u32 n, u32 fl) { (void)t; fk_log(0x10088); fk_log(type); fk_log(fk_id(vb)); fk_log(s); fk_log(n); fk_log(fl); return 0; }
static u32 ST d_st(Fake *t, u32 stage, u32 tex) { (void)t; fk_log(0x10098); fk_log(stage); fk_log(fk_id(tex)); return 0; }
static u32 ST d_tss(Fake *t, u32 s, u32 ty, u32 v) { (void)t; fk_log(0x100A0); fk_log(s); fk_log(ty); fk_log(v); return 0; }
/* DirectDraw */
static u32 ST dd_cs(Fake *t, u8 *desc, u32 *pp, u32 outer)
{
    (void)t;
    fk_log(0x20018); fk_logmem(desc + 4, 0xC); fk_logmem(desc + 0x48, 0x30); fk_log(outer);
    Fake *f = fk_new(3, *(u32 *)(desc + 0xC), *(u32 *)(desc + 8));
    *pp = (u32)(uintptr_t)f;
    return 0;
}
/* surfaces and textures */
static u32 ST s_qi(Fake *t, const u32 *iid, u32 *pp)
{
    fk_log(0x30000); fk_log(t->id); fk_log(iid[0]);
    Fake *f = fk_new(4, 0, 0);
    f->w = t->w; f->h = t->h; f->bpp = t->bpp; f->pix = t->pix;
    *pp = (u32)(uintptr_t)f;
    return 0;
}
static u32 ST s_release(Fake *t) { fk_log(0x30008); fk_log(t->id); return 0; }
static void fk_logract(const u32 *r) { if (r) fk_logmem(r, 16); else fk_log(0xFFFFFFFF); }
static u32 ST s_blt(Fake *t, const u32 *dr, u32 src, const u32 *sr, u32 fl, const u8 *fx)
{
    fk_log(0x30014); fk_log(t->id); fk_logract(dr); fk_log(fk_id(src)); fk_logract(sr); fk_log(fl);
    fk_log(fx && (fl & 0x400) ? *(u32 *)(fx + 0x50) : 0);
    Fake *s = (Fake *)(uintptr_t)src;
    if (s && s->pix && t->pix && s->w == t->w && s->h == t->h) memcpy(t->pix, s->pix, t->w * t->h * t->bpp);
    return 0;
}
static u32 g_fk_flip_hr;
static u32 ST s_flip(Fake *t, u32 tg, u32 fl) { fk_log(0x3002C); fk_log(t->id); fk_log(fk_id(tg)); fk_log(fl); return g_fk_flip_hr; }
static u32 ST s_lock(Fake *t, const u32 *r, u8 *desc, u32 fl, u32 ev)
{
    fk_log(0x30064); fk_log(t->id); fk_logract(r); fk_log(fl); fk_log(ev);
    *(u32 *)(desc + 8) = t->h; *(u32 *)(desc + 0xC) = t->w; *(u32 *)(desc + 0x10) = t->w * t->bpp;
    *(u8 **)(desc + 0x24) = t->pix;
    return 0;
}
static u32 ST s_restore(Fake *t) { fk_log(0x3006C); fk_log(t->id); return 0; }
static u32 ST s_colorkey(Fake *t, u32 fl, const u32 *k) { fk_log(0x30074); fk_log(t->id); fk_log(fl); fk_logmem(k, 8); return 0; }
static u32 ST s_unlock(Fake *t, const u32 *r) { fk_log(0x30080); fk_log(t->id); fk_logract(r); fk_log(fk_hash(t->pix, t->w * t->h * t->bpp)); return 0; }
/* the viewport, the material */
static u32 ST vp_clear(Fake *t, u32 n, const u32 *r, u32 fl) { (void)t; fk_log(0x50030); fk_log(n); fk_logmem(r, 16 * n); fk_log(fl); return 0; }
static u32 ST vp_setbg(Fake *t, u32 h) { (void)t; fk_log(0x50020); fk_log(h); return 0; }
static u32 ST mat_set(Fake *t, const u8 *d) { fk_log(0x6000C); fk_log(t->id); fk_logmem(d, 0x50); return 0; }

/* any other method: reported, then the harness stops (its arguments are unknown) */
static u32 ST fk_trap(Fake *t)
{
    printf("fake COM: an unexpected method of a kind-%u object called\n", t->kind);
    fflush(stdout); TerminateProcess(GetCurrentProcess(), 3); return 0;
}
static u32 __cdecl fk_det_malloc(u32 n)
{
    u32 a = FK_ARENA + U32(FK_HEAPN);
    U32(FK_HEAPN) += (n + 15) & ~15u;
    if (U32(FK_HEAPN) > 0x18000) { printf("fake malloc: arena full\n"); fflush(stdout); TerminateProcess(GetCurrentProcess(), 3); }
    return a;
}

/* the objects of a fresh Direct3D, the texture cache and palettes empty */
static void fk_reset(void)
{
    for (int k = 0; k < 6; k++) for (int i = 0; i < 48; i++) g_fvt[k][i] = (void *)fk_trap;
    void **d = g_fvt[1];
    d[0x24 / 4] = d_begin; d[0x28 / 4] = d_end; d[0x58 / 4] = d_rs; d[0x70 / 4] = d_dp; d[0x88 / 4] = d_dpvb;
    d[0x98 / 4] = d_st; d[0xA0 / 4] = d_tss;
    g_fvt[2][0x18 / 4] = dd_cs;
    for (int k = 3; k <= 4; k++) {
        void **s = g_fvt[k];
        s[0] = s_qi; s[2] = s_release; s[0x14 / 4] = s_blt; s[0x2C / 4] = s_flip; s[0x64 / 4] = s_lock;
        s[0x6C / 4] = s_restore; s[0x74 / 4] = s_colorkey; s[0x80 / 4] = s_unlock;
    }
    g_fvt[5][0x30 / 4] = vp_clear; g_fvt[5][0x20 / 4] = vp_setbg;
    for (int i = 0; i < 48; i++) g_fvt[0][i] = (void *)fk_trap;
    g_fvt[0][0x0C / 4] = mat_set;
    U32(FK_NOBJ) = 0; U32(FK_LOGN) = 0; U32(FK_HEAPN) = 0;
    U32(0x865EBC) = (u32)(uintptr_t)fk_new(2, 0, 0);
    U32(0x865ED8) = (u32)(uintptr_t)fk_new(1, 0, 0);
    U32(0x865EC0) = (u32)(uintptr_t)fk_new(3, 640, 480);
    U32(0x865EC4) = (u32)(uintptr_t)fk_new(3, 640, 480);
    U32(0x865ECC) = (u32)(uintptr_t)fk_new(3, 256, 256);
    U32(0x865EDC) = (u32)(uintptr_t)fk_new(5, 0, 0);
    U32(0x865EE0) = (u32)(uintptr_t)fk_new(0, 0, 0);    /* the viewport's background material */
    U32(0x86449C) = (u32)(uintptr_t)fk_new(5, 0, 0);    /* the masks' vertex buffer */
    for (int k = 0; k < 4; k++) U32(0x8644AC + 4 * k) = (u32)(uintptr_t)fk_new(4, 0, 0);   /* their textures */
    U32(0x865D58) = 0;                                  /* the frame copy: made on first use */
    g_fk_ssurf = (u32)(uintptr_t)fk_new(3, 256, 256); g_fk_stex = (u32)(uintptr_t)fk_new(4, 0, 0);   /* (made-up slots' texture) */
    g_fo[g_fk_stex ? ((Fake *)(uintptr_t)g_fk_stex)->id : 0].pix = ((Fake *)(uintptr_t)g_fk_ssurf)->pix;
    memset(MEM(0x8644C0), 0, 256 * 0x14);               /* glyphs */
    memset(MEM(0x75E488), 0, 32 * 32 * 0x18);          /* texture cache */
    memset(MEM(0x75CEE8), 0, 512 * 8);                  /* converted palettes */
}

/* the texture format: 16-bit 1555 or 32-bit 8888 */
static void fk_pixfmt(int b32)
{
    U8(0x87BB42) = b32 ? 0x20 : 0x10; U8(0x87BB43) = b32 ? 4 : 2;
    u32 sh[3] = {b32 ? 0 : 9, b32 ? 8 : 14, b32 ? 16 : 19}, m[4] = {b32 ? 0xFF0000 : 0x7C00, b32 ? 0xFF00 : 0x3E0, b32 ? 0xFF : 0x1F, b32 ? 0xFF000000 : 0x8000};
    for (int i = 0; i < 3; i++) U32(0x87BB44 + 4 * i) = sh[i];
    for (int i = 0; i < 4; i++) U32(0x87BB50 + 4 * i) = m[i];
    memset(MEM(0x87BB60), 0, 0x40);
    U32(0x87BB60) = 0x20; U32(0x87BB64) = 0x41; U32(0x87BB6C) = b32 ? 32 : 16;
    for (int i = 0; i < 4; i++) U32(0x87BB70 + 4 * i) = m[i];
}

#define DO_OT    0x5D0000u      /* the order table's packets */

/* check() with the fake surfaces' pixels saved and restored as the memory is */
static u8 *g_fkpix_save[2][512];
static u32 g_fkcap[2][512];
static void fk_pix(int to, int i)
{
    for (u32 k = 0; k < U32(FK_NOBJ) && k < 512; k++) {
        if (!g_fpix[k]) continue;
        u32 n = g_fcap[k];
        if (!g_fkpix_save[i][k] || g_fkcap[i][k] < n) { free(g_fkpix_save[i][k]); g_fkpix_save[i][k] = malloc(n); g_fkcap[i][k] = n; }
        if (to) memcpy(g_fkpix_save[i][k], g_fpix[k], n); else memcpy(g_fpix[k], g_fkpix_save[i][k], n);
    }
}
static int check_fk(u32 addr, void *fn, const char *name, int nargs, const u32 *args)
{
    g_cur_name = name; g_cur_orig = 1;
    snap(snap0); fk_pix(1, 0);
    u32 nobj0 = U32(FK_NOBJ);
    clear_stack();
    u32 r0 = call((void *)(uintptr_t)addr, nargs, args);
    snap(snap1); fk_pix(1, 1);
    u32 nobj1 = U32(FK_NOBJ);
    restore(snap0); U32(FK_NOBJ) = nobj0; fk_pix(0, 0);
    g_cur_orig = 0;
    u32 r1 = call(fn, nargs, args);
    int ok = compare(name, r0, r1, 0);
    restore(snap1); (void)nobj1; fk_pix(0, 1);
    return ok;
}

/* (debugging: the two logs side by side around their first difference; the packets) */
static void fk_dump(gptr ot)
{
    printf("  OT order:");
    for (gptr q = ot, n = 0; q != 0xFFFFFF && n < 40; q = U32(q) & 0xFFFFFF, n++) printf(" %X:%02X", q, U8(q + 7));
    printf("\n");

    restore(snap0); PcDraw_DrawOTag(ot);
    u32 n = U32(FK_LOGN), no = *(u32 *)(snap1[0] + FK_LOGN - RW[0].lo), k = 0;
    const u32 *o = (const u32 *)(snap1[0] + FK_LOG - RW[0].lo), *c = (const u32 *)MEM(FK_LOG);
    while (k < n / 4 && k < no / 4 && o[k] == c[k]) k++;
    printf("logs: new %u bytes, original %u; first difference at dword %u\n", n, no, k);
    for (u32 j = k > (getenv("FKBACK") ? (u32)atoi(getenv("FKBACK")) : 40) ? k - (getenv("FKBACK") ? (u32)atoi(getenv("FKBACK")) : 40) : 0; j < k + 24 && j < (n > no ? n : no) / 4; j++)
        printf("  %4u: %08X %08X%s\n", j, j < n / 4 ? c[j] : 0, j < no / 4 ? o[j] : 0, c[j] != o[j] ? "  <" : "");
    for (gptr p = 0x5D0000; p < 0x5D0000 + 13 * 0x60; p += 0x60) {
        printf("  pkt %X: %08X %08X\n", p, U32(p), U32(p + 4));
        if ((U8(p + 7) & 0xFC) == 0x64 || (U8(p + 7) & 0xFC) == 0x80)
            printf("    sprite u %u w %d v %u h %d: texel reads at %X, %X\n", U8(p + 0x14), S16(p + 0x18), U8(p + 0x15), S16(p + 0x1A),
                   0x865940 - 4 + 4 * (U8(p + 0x14) + S16(p + 0x18) * ((U8(p + 7) & 0xFC) == 0x80 ? 2 : 1)), 0x865940 - 4 + 4 * (U8(p + 0x15) + S16(p + 0x1A) * ((U8(p + 7) & 0xFC) == 0x80 ? 2 : 1)));
    }
    {   /* the converted palettes that differ, and where the rows' slots point */
        int nd = 0;
        for (gptr a = FK_ARENA; a < FK_ARENA + 0x18000 && nd < 48; a += 4) {
            u32 ov = *(const u32 *)(snap1[0] + a - RW[0].lo);
            if (ov != U32(a)) { printf("  arena %X: %08X original %08X\n", a, U32(a), ov); nd++; }
        }
        for (u32 r = 0; r < 512; r++) if (U32(0x75CEEC + 8 * r)) printf("  row %u: slot %X stamp %u\n", r, U32(0x75CEEC + 8 * r), U32(0x75CEE8 + 8 * r));
    }
    restore(snap1);
}

static float rndf(float lo, float hi) { return lo + (hi - lo) * (float)(rnd() % 10001) / 10000.0f; }

static void rnd_xyz(gptr v, int z)
{
    F32(v) = rndf(-200.0f, 520.0f); F32(v + 4) = rndf(-200.0f, 440.0f);
    if (z) F32(v + 8) = rnd() % 8 ? rndf(1.0f, 4000.0f) : rnd() % 2 ? 0.0f : -rndf(0.0f, 50.0f);
}
static s16 rnd_size(void) { return (s16)(rnd() % 8 ? (s32)(1 + rnd() % 64) : rnd() % 4 ? 0 : -(s32)(rnd() % 8)); }

/* a random packet of `code` at p */
static void pkt_random(gptr p, u32 code)
{
    static const u16 cluts[4] = {0x7800, 0x7810, 0x7840, 0x7C00};          /* rows 480, 481, 496 */
    for (u32 k = 4; k < 0x58; k += 4) U32(p + k) = rnd();
    U8(p + 7) = (u8)(code | (rnd() % 4));
    if (code < 0x40) {                                                       /* polygons */
        int tex = code & 4, gour = code & 0x10, quad = code & 8;
        u32 n = quad ? 4 : 3, step = gour ? (tex ? 0x14 : 0x10) : tex ? 0x10 : 0xC;
        for (u32 i = 0; i < n; i++) rnd_xyz(p + 8 + step * i, 1);
        if (tex) {
            u32 tp = rnd() & 0x3FFF;
            if (rnd() % 4) tp &= ~0x180u;                                    /* (mostly 4-bit) */
            if (rnd() % 4) tp = (tp & ~0x1Fu) | (rnd() % 2 ? 5 : 0x17);       /* (mostly 2 pages: textures found again) */
            U16(p + 0x16) = cluts[rnd() % 4];
            U16(p + (gour ? 0x2A : 0x26)) = (u16)tp;
            if (code == 0x24 && rnd() % 4 == 0) { U16(p + 0x24) = U16(p + 0x14); U16(p + 0x34) = U16(p + 0x14); }
        }
        return;
    }
    switch (code) {
    case 0x40: case 0x48: case 0x4C: for (u32 i = 0; i < 4; i++) rnd_xyz(p + 8 + 0xC * i, 1); break;
    case 0x50: case 0x58: case 0x5C: for (u32 i = 0; i < 4; i++) rnd_xyz(p + 8 + 0x10 * i, 1); break;
    case 0x60: rnd_xyz(p + 8, 1); S16(p + 0x14) = rnd_size(); S16(p + 0x16) = rnd_size(); break;
    case 0x68: rnd_xyz(p + 8, 1); break;
    case 0x64: case 0x74: case 0x7C: case 0x80:
        rnd_xyz(p + 8, 1); U16(p + 0x16) = cluts[rnd() % 4]; S16(p + 0x18) = rnd_size(); S16(p + 0x1A) = rnd_size();
        if ((code == 0x64 || code == 0x80) && rnd() % 3 == 0) {         /* the far corner's texel off the table's start: in the vertices */
            /* (the fields the sprite wrote before, or left from earlier draws: colours read as floats) */
            static const s8 tu[] = {-31, -30, -25, -24, -23, -22, -16}, tv[] = {-31, -30, -25, -24, -23, -22, -16, -17, -15, -14, -9, -7};
            s32 st = code == 0x80 ? 2 : 1;
            for (int k = 0; k < 2; k++) {
                s32 t = rnd() % 2 ? -(s32)(rnd() % 32) : k ? tv[rnd() % sizeof tv] : tu[rnd() % sizeof tu], u0 = (s32)(rnd() % 8);
                if ((t - u0) % st) u0++;
                U8(p + 0x14 + k) = (u8)u0;
                S16(p + 0x18 + 2 * k) = (s16)((t - u0) / st);
            }
        }
        break;
    case 0x6C:
        for (u32 i = 0; i < 4; i++) rnd_xyz(p + 8 + 0xC * i, 0);
        U16(p + 0x12) = cluts[rnd() % 4]; U16(p + 0x1E) = (u16)(rnd() % 3 ? rnd() % 8 : rnd() % 64);
        break;
    case 0x88: U32(p + 8) = rnd() % 64; U16(p + 0xC) = (u16)(rnd() % 32); U16(p + 0xE) = (u16)(rnd() % 4); break;
    case 0x8C:
        rnd_xyz(p + 8, 0); U16(p + 0x14) = (u16)(rnd() % 320); U16(p + 0x16) = (u16)(rnd() % 240);
        U16(p + 0x10) = (u16)(rnd() % 320); U16(p + 0x18) = (u16)(rnd() % 320); U16(p + 0x1A) = (u16)(rnd() % 240);
        break;
    case 0xEC: case 0xFC: {                                                  /* VRAM moves: small rects (palette rows for FC) */
        s16 w = (s16)(1 + rnd() % 32), h = (s16)(1 + rnd() % (code == 0xFC ? 2 : 16));
        /* (a rect partly left of or above VRAM is clipped; one wholly outside would copy 64K rows) */
        S16(p + 8) = (s16)(rnd() % 8 == 0 && code == 0xEC ? -(s32)(rnd() % w) : (s32)(rnd() % (0x400 - 32)));
        S16(p + 0xA) = (s16)(code == 0xFC ? (s32)(480 + rnd() % 14) : rnd() % 8 == 0 ? -(s32)(rnd() % h) : (s32)(rnd() % (0x200 - 16)));
        if (code == 0xEC && rnd() % 8 == 0) S16(p + 8) = (s16)(0x400 - 1 - (s32)(rnd() % w));      /* (past the right edge) */
        if (code == 0xEC && rnd() % 8 == 0) S16(p + 8) = (s16)(0x400 - w - (s32)(rnd() % 16));     /* (just inside) */
        if (code == 0xEC && rnd() % 8 == 0) S16(p + 0xA) = (s16)(0x200 - 1 - (s32)(rnd() % h));    /* (past the bottom) */
        S16(p + 0xC) = w; S16(p + 0xE) = h;
        /* (the destination stays >= 0 after clipping: the port's copy keeps dx's top half in its row count) */
        S32(p + 0x10) = (s32)(rnd() % (0x400 - 72)) + 32; S32(p + 0x14) = code == 0xFC ? 480 + (s32)(rnd() % 14) : (s32)(rnd() % (0x200 - 32)) + 16;
        if (code == 0xFC && rnd() % 3 == 0) {                               /* overlapping, along its rows: copied forwards */
            S16(p + 8) = (s16)(32 + rnd() % (0x400 - 96)); S32(p + 0x10) = S16(p + 8) + (rnd() % 2 ? (s32)(1 + rnd() % 8) : -(s32)(1 + rnd() % 8));
            S32(p + 0x14) = S16(p + 0xA);
        }
        U32(p + 4) = code << 24; break; }
    }
}

/* the texture window: x, y, w, h that the converters accept */
static void twin_random(gptr a)
{
    static const s16 sz[6] = {8, 16, 32, 64, 128, 256};
    s16 w = sz[rnd() % 6], h = sz[rnd() % 6];
    if (rnd() % 4) { S32(a) = 0; S16(a + 4) = S16(a + 6) = 256; return; }   /* (mostly the whole page) */
    if (rnd() % 2) w = h = 256;
    S16(a) = (s16)(rnd() % 2 ? 0 : (rnd() % 4) * 8); S16(a + 2) = (s16)(rnd() % 2 ? 0 : rnd() % 32);
    S16(a + 4) = w; S16(a + 6) = h;
}

static void test_pcdraw(void)
{
    static const u8 codes[] = {0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C, 0x40, 0x48, 0x4C, 0x50, 0x58, 0x5C, 0x60, 0x64,
                               0x68, 0x6C, 0x74, 0x7C, 0x80, 0x88, 0x8C, 0xEC, 0xFC, 0x2C, 0x3C, 0x64, 0x6C, 0x34};
    unsigned fpu = _controlfp(0, 0);
    _controlfp(_PC_24, _MCW_PC);                    /* the x87 as Direct3D leaves it in the game */
    u8 saved[5]; u8 *c = MEM(0x4DD711); memcpy(saved, c, 5);
    c[0] = 0xE9; *(u32 *)(c + 1) = (u32)(uintptr_t)fk_det_malloc - (0x4DD711 + 5);
    for (u32 k = 0; k < 0x100000; k += 4) U32(VRAM_BASE + k) = rnd();
    for (u32 i = 0; i < 256; i++) F32(0x865940 + 4 * i) = (float)((double)i * 0.00390625 + 0.002);
    U32(0x75DEF0) = 0;
    U32(0x865EE4) = 0x660000;                           /* the font: 64 glyphs */
    for (u32 k = 0; k < 64 * 0x188; k += 4) U32(0x660000 + k) = rnd();
    for (int i = 0; i < iters; i++) {
        if (i % 16 == 0) { fk_reset(); fk_pixfmt(i % 64 == 32); }
        U32(FK_LOGN) = 0;
        /* VRAM: some palettes and texels changed */
        for (int k = rnd() % 4; k; k--) U16(VRAM_BASE + 2 * (480 * 1024 + rnd() % 2048)) = (u16)(rnd() % 4 ? rnd() : 0x8000);   /* (0x8000: black, not transparent) */
        for (int k = rnd() % 64; k; k--) U32(VRAM_BASE + 4 * (rnd() % 0x40000)) = rnd();
        if (rnd() % 8 == 0) PcDraw_ClutRowConvert(0, 480 + rnd() % 2, 0x400);
        /* the draw state and the shadowed render states */
        U16(0x87BAF4) = (u16)(rnd() & 0x3FFF); U8(0x87BAF7) = rnd() % 16 != 0; U8(0x87BAF6) = (u8)(rnd() % 2);
        twin_random(0x87BAEC);
        F32(0x8644BC) = rnd() % 2 ? 2.0f : rndf(0.5f, 3.0f); F32(0x8644B8) = rnd() % 2 ? 2.0f : rndf(0.5f, 3.0f);
        U32(0x73B67C) = rnd() % 4 != 0; U32(0x865EEC) = rnd() % 2; U32(0x865EE8) = rnd() % 3;
        F32(0x73B674) = rndf(0.0f, 2000.0f); F32(0x73B678) = F32(0x73B674) + (rnd() % 8 ? rndf(1.0f, 30000.0f) : 0.0f);
        for (u32 k = 0; k < 5; k++) if (rnd() % 4 == 0) U32(0x73B680 + 4 * k) = rnd() % 3 ? rnd() % 4 : 0xFFFFFFFF;
        if (rnd() % 4 == 0) U32(0x73B690) = (rnd() % 4) << 10;
        U16(0x865D4A) = 0;
        U16(0x865D48) = rnd() % 4 != 0;             /* a frame copy there (code 0x8C) */
        U16(0x865DAC) = (u16)(rnd() % 2);
        if (rnd() % 8 == 0) U32(0x865D58) = 0;      /* (made again) */
        U8(0x865E44) = (u8)(rnd() % 4 ? 2 : rnd() & 0x22); U32(0x865E6C) = rnd() % 2 ? 0 : 8; U32(0x865E70) = rnd() % 2 ? 0 : 8;
        U32(0x865E74) = rnd() % 2 ? 1024 : 256; U32(0x865E78) = rnd() % 2 ? 1024 : 128;
        for (int k = 0; k < 256; k++) if (rnd() % 16 == 0) U16(0x8644D0 + k * 0x14) = (u16)(rnd() % 2);
        /* the order table: n packets, a few draw-mode packets between */
        u32 n = 1 + rnd() % 12; gptr p = DO_OT, first = p; int spec = 0;
        for (u32 k = 0; k < n; k++) {
            gptr nx = p + 0x60;
            if (rnd() % 6 == 0) {                   /* draw mode (E1), maybe with a texture window */
                U32(p + 4) = 0xE1000000 | (rnd() & 0x30000) | (rnd() & 0x3FFF);
                U32(p + 8) = rnd() % 2 ? 0 : p + 0x10;
                twin_random(p + 0x10);
            } else if (rnd() % 12 == 0) {           /* texture window (F0) */
                U32(p + 4) = 0xF0000000; U32(p + 8) = p + 0x10; twin_random(p + 0x10);
            } else {
                u32 code = codes[rnd() % sizeof codes];
                pkt_random(p, code);
                if (code == 0x8C) spec = 1;
            }
            U32(p) = (rnd() % 8 ? 0x80000000u : 0) | nx;
            p = nx;
        }
        U32(p) = 0xFFFFFFFF;
        u32 a[1] = {first};
        sprintf(g_what, "iteration %d, %u packets", i, n);
        g_fk_nospec = spec;                         /* (code 0x8C leaves a stack word in the specular) */
        if (spec) { g_ign2_addr = 0x8658C0; g_ign2_len = 0x80; }
        if (!check_fk(0x4B50F0, PcDraw_DrawOTag, "PcDraw_DrawOTag", 1, a) && getenv("FKDUMP")) fk_dump(first);
        g_fk_nospec = 0; g_ign2_len = 0;
        /* the textures over a VRAM rect, marked or released; sometimes the page left of it full */
        if (rnd() % 4 == 0) {
            gptr r = DO_OT + 0x5F00;
            S16(r) = (s16)(rnd() % 0x400); S16(r + 2) = (s16)(rnd() % 0x200);
            if (rnd() % 4 == 0) {                   /* (32 slots of 8- or 15-bit textures in the page to the left) */
                u32 pg = rnd() % 15;
                for (u32 k = 0; k < 32; k++) {
                    gptr e = 0x75E488 + (pg * 32 + k) * 0x18;
                    U8(e) = 1; U8(e + 1) = (u8)(rnd() % 3); U16(e + 2) = (u16)rnd(); U32(e + 4) = rnd();
                    U32(e + 8) = 0; U32(e + 0xC) = 0x01000100; U32(e + 0x10) = g_fk_ssurf; U32(e + 0x14) = g_fk_stex;
                }
                S16(r) = (s16)((pg + 1) * 64 + rnd() % 64); S16(r + 2) = (s16)(rnd() % 256);
            }
            S16(r + 4) = (s16)(rnd() % 4 ? 1 + rnd() % 300 : 1); S16(r + 6) = (s16)(1 + rnd() % 300);
            if (S16(r) + S16(r + 4) > 0x400) S16(r + 4) = (s16)(0x400 - S16(r));
            if (S16(r + 2) + S16(r + 6) > 0x200) S16(r + 6) = (s16)(0x200 - S16(r + 2));
            u32 b[2] = {r, rnd() % 2};
            sprintf(g_what, "iteration %d: rect %d,%d %dx%d mark %u", i, S16(r), S16(r + 2), S16(r + 4), S16(r + 6), b[1]);
            check_fk(0x4B4790, PcDraw_InvalidateRect, "PcDraw_InvalidateRect", 2, b);
        }
        if (rnd() % 8 == 0) {
            u32 b[3] = {rnd() % 0x400, 480 + rnd() % 14, rnd() % 0x40};
            if (b[0] + b[2] > 0x400) b[2] = 0x400 - b[0];
            sprintf(g_what, "iteration %d: palette row %u at %u, %u", i, b[1], b[0], b[2]);
            check_fk(0x4B4BF0, PcDraw_ClutRowConvert, "PcDraw_ClutRowConvert", 3, b);
        }
    }
    memcpy(c, saved, 5);
    _controlfp(fpu, _MCW_PC);
}

/* the frame-level functions of the PC layer against the fake Direct3D */
static void test_pcframe(void)
{
    unsigned fpu = _controlfp(0, 0);
    _controlfp(_PC_24, _MCW_PC);
    gptr img = 0x660000, env = 0x5D8000;           /* (a 320 x 240 picture; environments) */
    for (int i = 0; i < iters; i++) {
        if (i % 16 == 0) {
            fk_reset(); fk_pixfmt(i % 64 == 32); memcpy(MEM(0x87BBC0), MEM(0x87BB40), 0x40); memset(MEM(0x75CE90), 0, 0x28);
            U32(0x75DEF0) = rnd() % 4 ? 0 : rnd() % 2 ? 0x200 : 1;     /* (windowed; no 3D device) */
        }
        U32(FK_LOGN) = 0;
        U8(0x865E44) = (u8)(rnd() % 4 ? 2 : rnd() & 0x22); U32(0x865E6C) = rnd() % 2 ? 0 : 8; U32(0x865E70) = rnd() % 2 ? 0 : 8;
        U32(0x865E74) = rnd() % 3 ? 256 : 1024; U32(0x865E78) = rnd() % 2 ? 1024 : 256;
        F32(0x8644BC) = rnd() % 2 ? 2.0f : rndf(0.5f, 3.0f); F32(0x8644B8) = rnd() % 2 ? 2.0f : rndf(0.5f, 3.0f);
        for (u32 k = 0; k < 5; k++) if (rnd() % 4 == 0) U32(0x73B680 + 4 * k) = rnd() % 3 ? rnd() % 4 : 0xFFFFFFFF;
        U32(0x865EEC) = rnd() % 2; U32(0x865EE8) = rnd() % 3; U8(0x87BAF8) = (u8)(rnd() % 4 == 0);
        g_fk_flip_hr = rnd() % 8 ? 0 : 0x887601C2;
        if (rnd() % 16 == 0) U32(0x75CE9C) = 0;     /* (the background's surfaces made again) */
        u32 kind = rnd() % 8;
        if (kind < 3) {
            if (rnd() % 2) for (u32 k = 0; k < 320 * 240 * 2; k += 4) U32(img + k) = rnd();
            else if (rnd() % 2) U32(img + 0x2800 * (rnd() % 15) + 0x20 * (rnd() % 20)) ^= 1 + rnd() % 255;   /* (a word of the sum) */
            u32 a[2] = {img, rnd() % 4 ? 0 : rnd() % 241};
            sprintf(g_what, "iteration %d: background, scroll %u", i, a[1]);
            check_fk(0x4BDC90, PcDraw_Background, "PcDraw_Background", 2, a);
        } else if (kind == 3) {
            float st = rndf(0.0f, 3000.0f), en = st + rndf(1.0f, 30000.0f);
            u32 a[3] = {rnd() & 0xFFFFFF, 0, 0}; memcpy(&a[1], &st, 4); memcpy(&a[2], &en, 4);
            sprintf(g_what, "iteration %d: far colour, mode %u", i, U32(0x865EE8));
            check_fk(0x4B4E60, PcDraw_SetFarColor, "PcDraw_SetFarColor", 3, a);
            u32 b[1] = {rnd() % 3 ? rnd() % 2 : rnd()};
            check_fk(0x4B4F50, PcDraw_SetFogEnable, "PcDraw_SetFogEnable", 1, b);
            u32 font = U32(0x865EE4), c[1] = {rnd() % 4 ? rnd() : 0};
            U32(0x865EE4) = 0;                          /* (no font yet: none freed) */
            check_fk(0x4BD480, PcDraw_SetFont, "PcDraw_SetFont", 1, c);
            U32(0x865EE4) = font;
        } else if (kind == 4) {
            u32 a[1] = {rnd() % 4};
            sprintf(g_what, "iteration %d: clear %u", i, a[0]);
            check_fk(0x4B4F90, PcDraw_Clear, "PcDraw_Clear", 1, a);
            check_fk(0x4B5070, PcDraw_Present, "PcDraw_Present", 0, a);
        } else if (kind == 5) {
            u32 a[3] = {rnd(), rnd(), rnd()};
            if (rnd() % 8 == 0) U32(0x865EE0) = 0;
            sprintf(g_what, "iteration %d: background colour", i);
            check_fk(0x4BB7C0, PcDraw_SetBackground, "PcDraw_SetBackground", 3, a);
        } else {
            for (u32 k = 0; k < 0x60; k += 4) U32(env + k) = rnd();
            if (rnd() % 2) U16(env + 2) = U16(0x87BAC2);
            if (rnd() % 2) { U8(env + 0x19) = U8(0x87BAF9); U8(env + 0x1A) = U8(0x87BAFA); U8(env + 0x1B) = U8(0x87BAFB); }
            u32 a[1] = {env};
            sprintf(g_what, "iteration %d: environments", i);
            check_fk(0x4C0770, PcDraw_PutDispEnv, "PcDraw_PutDispEnv", 1, a);
            check_fk(0x4C07A0, PcDraw_PutDrawEnv, "PcDraw_PutDrawEnv", 1, a);
        }
    }
    _controlfp(fpu, _MCW_PC);
}

/* ---- the rooms' JPEG backgrounds (libjpeg 6b, 0x4D2990) --------------------------- */
static u32 __cdecl host_malloc(u32 n) { return (u32)(uintptr_t)malloc(n); }
static u32 __cdecl host_free(u32 p) { free((void *)(uintptr_t)p); return 0; }
static u32 __cdecl host_getenv(u32 name) { (void)name; return 0; }   /* (libjpeg asks for JPEGMEM) */
static u32 __cdecl host_jerr(u32 f, u32 fmt, u32 msg) { (void)f; (void)fmt; (void)msg; return 0; }

/* every camera picture of the DBS files (record: JPEG at +0x1000, its size at
   +0x24), decoded by the original and by PcJpeg_Decode into 0x6C1500; a few
   also cut short or with bytes changed (libjpeg's own recovery) */
static void test_jpeg(const char *dir)
{
    static const u32 fn[4] = {0x4DD711, 0x4DD628, 0x4DE357, 0x4DE4F3};
    void *impl[4] = {host_malloc, host_free, host_jerr, host_getenv};
    u8 saved[4][5];
    for (int k = 0; k < 4; k++) {                   /* the CRT's malloc, free, fprintf, getenv: the host's */
        u8 *c = MEM(fn[k]); memcpy(saved[k], c, 5);
        c[0] = 0xE9; *(u32 *)(c + 1) = (u32)(uintptr_t)impl[k] - (fn[k] + 5);
    }
    unsigned fpu = _controlfp(0, 0);
    _controlfp(_PC_24, _MCW_PC);                    /* the x87 as Direct3D leaves it in the game */
    char pat[MAX_PATH]; snprintf(pat, sizeof pat, "%s\\*.DBS", dir);
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pat, &fd);
    u8 *src = malloc(0x40000 + 16);                 /* (+ 2 fill bytes) */
    int n = 0;
    if (h != INVALID_HANDLE_VALUE) {
        do {
            char path[MAX_PATH]; snprintf(path, sizeof path, "%s\\%s", dir, fd.cFileName);
            if (!load_host(path)) continue;
            for (u32 o = 0; o + 0x1004 < g_fake_len; o += 0x800) {
                const u8 *j = (u8 *)g_fake + o + 0x1000;
                u32 size = *(u32 *)((u8 *)g_fake + o + 0x24);
                if (j[0] != 0xFF || j[1] != 0xD8 || j[2] != 0xFF || !size || size > 0x40000 || o + 0x1000 + size > g_fake_len) continue;
                memcpy(src, j, size);
                u32 sz = size;
                if (n % 8 == 3) {                   /* fill bytes before a stuffed FF (FF FF 00: legal, same picture) */
                    u32 at = size / 2;
                    while (at + 1 < size && !(src[at] == 0xFF && src[at + 1] == 0)) at++;
                    if (at + 1 < size) { memmove(src + at + 2, src + at, size - at); src[at] = src[at + 1] = 0xFF; sz = size + 2; }
                }
                if (n % 8 == 7) {                   /* damaged: bytes changed in the scan, or the end cut off */
                    if (rnd() % 2) sz = size / 2 + rnd() % (size / 2);
                    else for (int k = 0; k < 4; k++) {     /* (no new markers: libjpeg's fatal errors exit) */
                        u32 at = size / 2 + rnd() % (size / 2 - 2); u8 v = (u8)(src[at] ^ (1 + rnd() % 255));
                        if (src[at] != 0xFF && src[at - 1] != 0xFF && v != 0xFF) src[at] = v;
                    }
                }
                memset(MEM(0x6C1500), 0, 320 * 240 * 2);
                U16(0x87B728) = 0;                  /* (the source's fake EOI buffer: written when the data runs out) */
                u32 a[4] = {0x6C1500, (u32)(uintptr_t)src, sz, 2};
                sprintf(g_what, "%s +%X size %X of %X", fd.cFileName, o, sz, size);
                check(0x4D2990, PcJpeg_Decode, "PcJpeg_Decode", 4, a, 0);
                n++;
            }
        } while (FindNextFileA(h, &fd) && n < iters);
        FindClose(h);
    }
    free(src);
    for (int k = 0; k < 4; k++) memcpy(MEM(fn[k]), saved[k], 5);
    _controlfp(fpu, _MCW_PC);
}

/* ---- the trigger areas' kinds (table 0x738B68) ----------------------------------- */
static void test_aot_kinds(void)
{
    static const struct { u32 addr; void *fn; const char *name; } k[] = {
        {0x48B460, AOT_Event, "AOT_Event"}, {0x48B480, AOT_Message, "AOT_Message"}, {0x48B4C0, AOT_Flag, "AOT_Flag"},
        {0x48B4E0, AOT_Item, "AOT_Item"}, {0x48B530, AOT_EffectStart, "AOT_EffectStart"}, {0x48B590, AOT_Kind7, "AOT_Kind7"},
        {0x48B5B0, AOT_Menu, "AOT_Menu"}, {0x48B5D0, AOT_Wait, "AOT_Wait"}, {0x48B620, AOT_Kind10, "AOT_Kind10"},
        {0x48B7D0, AOT_Kind11, "AOT_Kind11"}};
    stubs_install(1);
    for (int i = 0; i < iters; i++) {
        game_state(); script_setup();
        for (int t = 0; t < 10; t++) random_thread(T_BASE + t * SCR_THREAD_SIZE);
        gptr g = G();
        U32(g + 0xA5C) = O_AOT; U32(g + 0xA54) = S_BASE;
        U32(g + 0x93C) = O_EFF; U32(g + 0xA78) = rnd() % 4; U32(0x87DE04) = U32(g + 0xA78) + 12 + rnd() % 13;
        for (u32 j = 0; j < 28 * 0x70; j += 4) U32(O_EFF + j) = rnd();
        for (u32 j = 0; j < 28; j++) U8(O_EFF + j * 0x70 + 0x50) = (u8)(rnd() % 3 == 0 ? 1 | (rnd() & 2) : rnd() & 2);
        gptr e = O_AOT + 0x28 * (rnd() % 32);
        for (u32 j = 0; j < 0x28; j++) U8(e + j) = (u8)rnd();
        U8(e + 0x14) = rnd() % 10; U8(e + 0x15) = rnd() % 0x40; U8(e + 0x16) = rnd() % 4;
        U8(e + 0x17) = rnd() % 20; U16(e + 0x18) = rnd() % 20; U8(e + 0x19) = rnd() % 0x20;
        U8(g + 0xB4A) = rnd() % 2 ? U8(e + 0x19) : (u8)rnd();
        U8(g + 0x10B5) = (u8[]){1, 7, 2, 8, 0, 3}[rnd() % 6];
        stub_at(0x4718F0)->ret = 0x6DDC00; U8(0x6DDC00) = (u8)rnd();
        u32 a[1] = {e};
        for (unsigned j = 0; j < sizeof k / sizeof k[0]; j++) {
            U32(LOG_BASE) = 0;
            sprintf(g_what, "entry %X: %02X %02X %02X %02X char %u", e, U8(e), U8(e + 0x14), U8(e + 0x15), U8(e + 0x16), U8(g + 0x10B5));
            u32 keep = U32(e + 0x14);
            u32 keep2 = U32(e + 0x18);
            if (k[j].addr == 0x48B530) U32(e + 0x14) = rnd() % 12;   /* a dword: the effect's index */
            if (k[j].addr == 0x48B620) U32(e + 0x18) = rnd() % 20;   /* a dword: the effect slot's */
            check(k[j].addr, k[j].fn, k[j].name, 1, a, 0);
            U32(e + 0x14) = keep; U32(e + 0x18) = keep2;
        }
        check(0x48D6C0, Eff_AllocBack, "Eff_AllocBack", 0, a, 0xFFFFFFFF);
    }
    stubs_install(0);
}

/* ---- the velociraptor ------------------------------------------------------------- */
/* its state tables point at recorders: one per entry, so the log shows which */
static const u32 g_rap_tabs[][3] = {{0x72002C, 5, 1}, {0x720040, 9, 1}, {0x720064, 6, 1}, {0x72007C, 5, 1}, {0x720090, 5, 2}};
static const int g_rap_rec1[] = {1, 4, 5, 15, 16, 17, 18, 26, 27}, g_rap_rec2[] = {28, 29, 51, 52, 53};
static u32 g_rap_saved[5][9];
static void raptor_tables_stub(int on)
{
    for (int k = 0; k < 9; k++) g_stubs[g_rap_rec1[k]].spec = on ? "d" : "";
    for (int k = 0; k < 5; k++) g_stubs[g_rap_rec2[k]].spec = on ? "dd" : "";
    for (int t = 0; t < 5; t++)
        for (u32 k = 0; k < g_rap_tabs[t][1]; k++) {
            gptr a = g_rap_tabs[t][0] + 4 * k;
            if (on) { g_rap_saved[t][k] = U32(a); U32(a) = (u32)(uintptr_t)g_recs[g_rap_tabs[t][2] == 1 ? g_rap_rec1[k] : g_rap_rec2[k]]; }
            else U32(a) = g_rap_saved[t][k];
        }
}

/* a raptor among the collision test objects; its target is the player or
   the part G+0xB50 it rides on */
static gptr raptor_setup(void)
{
    gptr o = player_setup_obj(), g = G(), pl = U32(g + 0x940);
    if (o == pl) o += 0x260;
    U32(LOG_BASE) = 0;
    U32(g + 0xB50) = rnd() % 4 ? 0 : O_OBJ + 0x260 * 15;
    U32(o + 0x184) = (rnd() & 0xFFFFFFBF) | (U32(g + 0xB50) && rnd() % 2 ? 0x40 : 0);
    U32(0x74AF00) = o + 0x1C0;
    gptr t = U32(o + 0x184) & 0x40 ? U32(g + 0xB50) : pl;
    if (rnd() % 2) {                                        /* near, in sight or not */
        S16(t + 0x40) = (s16)(S16(o + 0x40) + (s32)(rnd() % 10000) - 5000);
        S16(t + 0x44) = (s16)(S16(o + 0x44) + (s32)(rnd() % 10000) - 5000);
        U16(o + 0x4A) = (u16)(((u32)(s32)(s16)Angle_XZ(o + 0x40, t + 0x40) + rnd() % 0x200 - 0x100) & 0xFFF);
    }
    U16(t + 0x5A) = rnd() % 4 ? U16(o + 0x5A) : (u16)rnd();
    U8(t + 0x54) = (u8[]){1, 2, 5, 0, 1}[rnd() % 5]; U8(t + 0x55) = rnd() % 2 ? 8 : rnd() % 10;
    U16(t + 0x174) = rnd() % 3 == 0;
    gptr e = o + 0x1C0;
    for (u32 k = 0; k < 0x40; k += 4) U32(e + k) = rnd();
    U32(e + 0xC) = (rnd() & 0xFFFF00) | rnd() % 5;          /* the state it had */
    U8(e + 0x19) = rnd() % 4; U32(e + 0x24) = rnd() % 2 ? 0 : rnd() % 3;
    U8(o + 0x54) = rnd() % 2 ? 2 : rnd() % 5; U8(o + 0x55) = rnd() % 4 ? rnd() % 2 : rnd() % 9; U8(o + 0x56) = rnd() % 3 ? 0 : rnd() % 5;
    U8(o + 0x18D) = rnd() % 3 ? 0 : (u8)rnd(); U8(o + 0x17B) = rnd() % 3 ? 0 : (u8)rnd();
    U8(o + 0x13A) = rnd() % 2; U8(o + 0xFF) = (u8)rnd(); U8(o + 0x119) = (u8)rnd();
    U16(o + 0x10C) = rnd() % 3 ? (rnd() % 2 ? 4 + rnd() % 2 : rnd() % 16) | (rnd() % 4 ? 0 : (rnd() & 0x30)) : (u16)rnd();
    U8(o + 0x18C) = (u8)rnd();
    U8(o + 0x17D) = (u8[]){0, 1, 2, 3, 0x10, 0x11, 0x1D, 0x1E, 0x20}[rnd() % 9];
    U16(o + 0x172) = rnd() % 6; U16(o + 0x176) = rnd() % 2 ? rnd() % 0x100 : (u16)rnd();
    U16(o + 0x108) = rnd() % 2 ? rnd() % 0x100 : (u16)rnd();
    if (rnd() % 3 == 0) U16(o + 0x176) = (u16)((U16(o + 0x108) << (rnd() % 2)) + rnd() % 3 - 1);   /* health to (about) 0 */
    U32(o + 0xA0) = 0;                                      /* no attack spheres */
    U32(o + 0x158) = rnd() % 2;                             /* head: still or relaxing */
    { u32 np = U32(U32(o + 0x64) + 0x54) ? 2 : 1; U8(o + 0x15F) = (u8)(rnd() % np); U8(o + 0x15E) = rnd() % 2 ? 0xFF : 0; }
    U32(o + 0x50) = (U32(o + 0x50) & ~0x40000000u) | (rnd() % 8 == 0 ? 0x40000000 : 0);
    U8(g + 0x108E) = rnd() % 3; U8(g + 0x927) = rnd() % 3 == 0;
    U16(g + 0x1090) = rnd() % 3 ? 0x407 : (u16)(rnd() % 0x500);
    Flag_Set(2, 0x1A, rnd() % 3 == 0);
    U16(0x87DE10) = (u16)rnd();
    U32(0x87DC98) = 0x6D0000;
    for (u32 k = 0; k < 16; k++) U8(O_OBJ + k * 0x260 + 0xFD) &= 1;   /* group: Obj_PickGroup counts 2 */
    for (u32 k = 0; k < U8(CL_ROOM); k++) {                             /* floor kinds (Floor_Height masks) */
        gptr w = CL_ROOM + U32(CL_ROOM + 8 + 4 * k);
        U8(w) = rnd() % 5 ? (u8)((rnd() % 3 == 0 ? 0x80 : rnd() % 3 == 0 ? 0x40 : 0) | (rnd() & 0x30) | 1) : 0;
        U16(w + 4) = rnd() % 4 ? (u16)(1u << (rnd() % 3)) | (u16)(rnd() % 2 ? 1u << (rnd() % 3) : 0) : (u16)rnd();
        U8(w + 3) = (u8)((2 * k + 2) | (rnd() % 3 ? 0 : rnd() % 2 ? 0x40 : 0x80));   /* which one; the kinds of objects it ignores */
    }
    U32(0x87F948) = TYPE_TAB;                                           /* effect slot handlers: recorders */
    for (int k = 0; k < 256; k++) U32(TYPE_TAB + 4 * k) = (u32)(uintptr_t)g_recs[STUB_TYPE];
    { u32 pb = U32(0x87DC98), ob = U32(0x87F94C); draw_safe(); U32(0x87DC98) = pb; U32(0x87F94C) = ob; }   /* (game over draws a frame) */
    for (u32 k = 0; k < 16; k++) { gptr q = O_OBJ + k * 0x260; if (U32(q + 0x9C) != AN_EVT) U32(q + 0x9C) = 0; }   /* animation events: the test table (sound ids in range) or none */
    return o;
}

/* a Rand8 seed whose first value is want (and the second 0 when zero2) */
static u16 rand8_seed(u8 want, int zero2)
{
    u32 v = rnd() & 0xFFFF;
    for (u32 n = 0; n < 0x10000; n++, v = (v + 1) & 0xFFFF) {
        u8 hi = (u8)((v * 3u) >> 8); u16 x = (u16)(hi << 8 | (u8)(v + hi));
        if (x == v) x += 0x101;
        if ((u8)x != want) continue;
        if (!zero2) return (u16)v;
        u8 h2 = (u8)((x * 3u) >> 8); u16 y = (u16)(h2 << 8 | (u8)(x + h2));
        if (y == x) y += 0x101;
        if (!(u8)y) return (u16)v;
    }
    return (u16)v;
}

/* the target at the distances it compares, in front or not; Rand8's first
   value at the edges it compares */
static void raptor_think_setup(gptr o)
{
    gptr t = U8(o + 0x184) & 0x40 ? U32(G() + 0xB50) : U32(G() + 0x940);
    static const s16 d[][2] = {{5000, 0}, {3000, 0}, {2000, 1000}, {3000, 2000}, {4000, 0}, {2500, 0}, {1000, 0}, {6000, 0}};
    int k = rnd() % 8, sx = rnd() % 2 ? 1 : -1, sw = rnd() % 2;
    s16 dx = (s16)(d[k][sw] * sx + (s32)(rnd() % 3) - 1), dz = d[k][!sw];
    S16(o + 0x40) = (s16)(rnd() % 2000 - 1000); S16(o + 0x44) = (s16)(rnd() % 2000 - 1000);
    S16(t + 0x40) = (s16)(S16(o + 0x40) + dx); S16(t + 0x44) = (s16)(S16(o + 0x44) + dz);
    U16(o + 0x4A) = (u16)(((u32)(s32)(s16)Angle_XZ(o + 0x40, t + 0x40) + (rnd() % 4 ? rnd() % 0xA0 - 0x50 : rnd())) & 0xFFF);
    U16(t + 0x5A) = rnd() % 4 == 0 ? U16(o + 0x5A) ^ 0x100 : rnd() % 6 ? U16(o + 0x5A) : (u16)rnd();
    U8(t + 0x54) = (u8[]){1, 2, 5, 0}[rnd() % 4]; U8(t + 0x55) = rnd() % 2 ? 8 : rnd() % 10;
    U16(t + 0x174) = rnd() % 3 == 0;
    U8(o + 0x55) = rnd() % 2; U8(o + 0x18D) = 0; U8(o + 0x17B) = 0; U8(o + 0x13A) = rnd() % 2; U8(o + 0xFF) = (u8)rnd();
    U8(o + 0x1C0 + 0x19) = rnd() % 3; U32(o + 0x1C0 + 0x24) = rnd() % 4 ? 1 + rnd() % 3 : 0;
    U8(G() + 0x108E) = rnd() % 2 ? 0 : 1 + rnd() % 2;
    u32 att = rnd() % 3 ? 99 : 1 + rnd() % 14;          /* another one attacking or not (Enemy_FindState) */
    for (u32 k = 1; k < 16; k++) {
        gptr q = O_OBJ + k * 0x260;
        if (q == o || q == t) continue;
        U8(q + 0x50) |= 1; U8(q + 0x58) = rnd() % 4 ? U8(o + 0x58) : (u8)rnd(); U8(q + 0x54) = 1; U8(q + 0x55) = k == att ? 2 : rnd() % 2;
    }
    if (rnd() % 2) {                                    /* one wall across its way, near or far (Wall_Ahead) */
        gptr w = 0x6CFC00u, pg = w + 0x10;
        U8(w) = 1; U32(w + 8) = 0x10;
        double a = U16(o + 0x4A) * 3.14159265358979 / 2048, d = rnd() % 2 ? 1000 : 1700 + rnd() % 3 * 100;
        double cx = S16(o + 0x40) + d * sin(a), cz = S16(o + 0x44) + d * cos(a), ex = 800 * cos(a), ez = -800 * sin(a);
        int sw = rnd() % 2;
        U8(pg) = 1; U8(pg + 1) = 1; U8(pg + 3) = 0; U16(pg + 4) = 0xFFFF; U8(pg + 8) = 0x81;
        S16(pg + 0xC + 4 * sw) = (s16)(cx + ex); S16(pg + 0xE + 4 * sw) = (s16)(cz + ez);
        S16(pg + 0x10 - 4 * sw) = (s16)(cx - ex); S16(pg + 0x12 - 4 * sw) = (s16)(cz - ez);
        U32(G() + 0xA4C) = w;
    }
    static const u8 r[] = {0x27, 0x28, 0x29, 0x3F, 0x40, 0x41, 0x7F, 0x80, 0x81, 0x10, 0x11, 0};
    U16(0x87DE10) = rand8_seed(r[rnd() % 12], rnd() % 4 == 0);
    U32(LOG_BASE) = 0;
}

/* a jump on its way: the level, the height about it, the vertical speed */
/* half the time the animation ends at its next step (Anim_Step returns 1) */
static void anim_maybe_end(gptr o)
{
    if (rnd() % 2) return;
    U16(o + 0x102) &= ~0x20; U16(o + 0x94) = (u16)(U16(o + 0x90) - 1); U16(o + 0x98) = 0x7000;
}

static void raptor_jump_setup(gptr o)
{
    gptr e = o + 0x1C0;
    U32(0x74AF00) = e;
    U8(o + 0x56) = rnd() % 5;
    S16(o + 0x5A) = (s16)(rnd() % 4); U16(e + 0x16) = (u16)(S16(o + 0x5A) + rnd() % 3);
    S16(o + 0x42) = (s16)(-1000 * (s32)(S16(o + 0x5A) + rnd() % 3) + (s32)(rnd() % 3) - 1 + (rnd() % 2 ? (s32)(rnd() % 400) - 200 : 0));
    S16(e + 0x14) = (s16)(rnd() % 2 ? (s32)(rnd() % 3) - 0x3D : (s32)(rnd() % 0x400) - 0x200);
    S16(o + 0xF4) = (s16)(rnd() % 2 ? 0xC6 + rnd() % 4 : rnd() % 0x200);
    U8(o + 0xFF) = (u8)rnd();
    if (rnd() % 2) {                                    /* after this step's fall, at a level's height */
        s32 base = rnd() % 2 ? -1000 * (s32)U16(e + 0x16) : -1000 * S16(o + 0x5A);
        S16(o + 0x42) = (s16)(base + (s32)(rnd() % 3) - 1 - (S16(e + 0x14) + 0x3C));
    }
    anim_maybe_end(o);
}

static void test_raptor(void)
{
    g_ign_objpads = 1;
    stubs_install(1);
    raptor_tables_stub(1);
    player_tables_stub(1);                                  /* the player's weapon set-ups (Player_Weapon) */
    for (int i = 0; i < iters; i++) {
        game_state();
        gptr o = raptor_setup();
        u32 a[2] = {o};
        sprintf(g_what, "raptor %X state %02X/%02X/%02X level %X hit %X/%X", o, U8(o + 0x54), U8(o + 0x55), U8(o + 0x56),
                U16(o + 0x10C), U8(o + 0x17D), U16(o + 0x172));
        check(0x41FB20, Raptor_Think, "Raptor_Think", 1, a, 0);
        for (int k = 0; k < 24; k++) {                      /* the edges of its choices */
            raptor_think_setup(o);
            sprintf(g_what, "raptor %X think %02X dist %u level %X/%X rnd %04X", o, U8(o + 0x55), Dist2_XZ(o + 0x40, (U8(o + 0x184) & 0x40 ? U32(G() + 0xB50) : U32(G() + 0x940)) + 0x40),
                    U16(o + 0x5A), U16(U32(G() + 0x940) + 0x5A), U16(0x87DE10));
            check(0x41FB20, Raptor_Think, "Raptor_Think", 1, a, 0);
        }
        U32(G() + 0xA4C) = CL_ROOM;                         /* the room's polygons again, not the test wall */
        check(0x41F960, Raptor_Normal, "Raptor_Normal", 1, a, 0);
        check(0x41F9A0, Raptor_Hit, "Raptor_Hit", 1, a, 0);
        check(0x420A00, Raptor_Grab, "Raptor_Grab", 1, a, 0);
        u8 st0[0x260]; memcpy(st0, MEM(o), 0x260);
        for (int k = 0; k < 2; k++) {                       /* the damage it takes, to about 0 */
            U8(o + 0x54) = 2; U8(o + 0x56) = 0; U8(o + 0x10C) = (u8)((rnd() & 0xF0) | (4 + rnd() % 2));
            U8(o + 0x17D) = (u8[]){0, 1, 2, 3, 0x10, 0x11, 0x1D, 0x1E}[rnd() % 8]; U16(o + 0x172) = rnd() % 6;
            U16(o + 0x108) = rnd() % 0x200; U16(o + 0x176) = (u16)((U16(o + 0x108) << (rnd() % 2)) + rnd() % 3 - 1);
            U8(o + 0x17B) = rnd() % 3 ? 0 : (u8)rnd(); U32(o + 0x50) &= ~0x40000000u;
            U32(0x74AF00) = o + 0x1C0; U32(o + 0x1C0 + 0xC) = (rnd() & 0xFFFF00) | rnd() % 5;
            U32(LOG_BASE) = 0;
            sprintf(g_what, "raptor %X hit %X kind %X health %d damage %d", o, U8(o + 0x17D), U8(o + 0x10C) & 0xF, S16(o + 0x108), S16(o + 0x176));
            check(0x41F430, Raptor_Handler, "Raptor_Handler", 1, a, 0);
        }
        memcpy(MEM(o), st0, 0x260); U32(0x74AF00) = o + 0x1C0;
        sprintf(g_what, "raptor %X state %02X/%02X/%02X level %X hit %X/%X", o, U8(o + 0x54), U8(o + 0x55), U8(o + 0x56),
                U16(o + 0x10C), U8(o + 0x17D), U16(o + 0x172));
        check(0x41F430, Raptor_Handler, "Raptor_Handler", 1, a, 0);
        U8(o + 0x11A) = (u8)rnd();
        anim_setup_at(AN_OBJ); memcpy(MEM(o + 0x60), MEM(AN_OBJ + 0x60), 0x50);   /* animated */
        for (int k = 0x40; k < 0xC0; k++) U32(AN_TAB + 4 * k) = AN_DATA + 0x1000 * ((k ^ k >> 4 ^ k >> 5) & 1);
        raptor_jump_setup(o);
        if (rnd() % 2 && U8(CL_ROOM)) {                         /* on a floor polygon, below its layers */
            gptr w = CL_ROOM + U32(CL_ROOM + 8 + 4 * (rnd() % U8(CL_ROOM))), v = w + 8 + U8(w + 1) * 4;
            s32 sx = 0, sz = 0, n = U8(w + 1) ? U8(w + 1) : 1;
            for (s32 j = 0; j < U8(w + 1); j++) { sx += S16(v + 4 * j); sz += S16(v + 4 * j + 2); }
            S16(o + 0x40) = (s16)(sx / n); S16(o + 0x44) = (s16)(sz / n); S16(o + 0x42) = (s16)(-(s32)(rnd() % 5000));
            if (rnd() % 2) U8(o + 0x56) = 0;
        }
        if (rnd() % 4 == 0) S16(o + 0x42) = (s16)(0x7918 - rnd() % 0x30);   /* between the heights it looks down from */
        check(0x420410, Raptor_Jump, "Raptor_Jump", 1, a, 0);
        /* hit: the states by the kind of hit, in the air, down or standing */
        static const struct { u32 addr; void *fn; const char *name; } ht[] = {
            {0x422400, Raptor_Hurt, "Raptor_Hurt"}, {0x422430, Raptor_HurtStand, "Raptor_HurtStand"}, {0x4226E0, Raptor_HurtDown, "Raptor_HurtDown"},
            {0x4228C0, Raptor_Knock, "Raptor_Knock"}, {0x4228F0, Raptor_KnockStand, "Raptor_KnockStand"}, {0x422BF0, Raptor_KnockDown, "Raptor_KnockDown"},
            {0x422E00, Raptor_Blow, "Raptor_Blow"}, {0x422E30, Raptor_BlowStand, "Raptor_BlowStand"}, {0x423080, Raptor_HurtAir, "Raptor_HurtAir"},
            {0x423300, Raptor_Burn, "Raptor_Burn"}, {0x4234B0, Raptor_Drown, "Raptor_Drown"}};
        for (unsigned k = 0; k < 2 * sizeof ht / sizeof ht[0]; k++) {
            gptr e = o + 0x1C0;
            U32(o + 0x50) ^= rnd() & 0x2878;
            U32(0x74AF00) = e;
            U8(o + 0x56) = rnd() % 3; U16(o + 0x174) = (u16)(rnd() % 2 ? rnd() & 6 : rnd());
            U16(o + 0x170) = (u16)rnd(); U16(o + 0x172) = rnd() % 6; U16(o + 0x178) = rnd() % 2 ? rnd() % 4 : (u16)rnd() & 0xFFF;
            U8(o + 0x184) = (u8)rnd(); U8(o + 0x180) = (u8)rnd(); U8(o + 0xFF) = (u8)rnd();
            S16(o + 0x10A) = (s16)(rnd() % 0x400); S16(o + 0x108) = (s16)(rnd() % 3 ? rnd() % 0x200 : rnd());
            U16(o + 0x176) = rnd() % 2 ? (u16)(U16(o + 0x108) + rnd() % 3 - 1) : rnd() % 0x100;
            U8(e + 0x1D) = (u8)rnd(); U8(e + 0x18) = rnd() % 2; U8(e + 0xC) = rnd() % 3; U8(e + 0x1A) = (u8)rnd();
            U16(o + 0x92) = rnd() % 2 ? (u16[]){3, 4, 0xA, 0xB, 0xD, 0xE}[rnd() % 6] : (u16)rnd();
            S16(o + 0x42) = (s16)(rnd() % 2 ? -1000 * (s32)(rnd() % 3) + (s32)(rnd() % 600) - 400 : (s32)(rnd() % 0x10000) - 0x8000);
            if (rnd() % 2) U16(0x87DE10) = rand8_seed((u8[]){0x3F, 0x40, 0x41, 0xDF, 0xE0, 0xE1, 4, 0x10, 0}[rnd() % 9], 0);
            if (rnd() % 3 == 0) { S16(o + 0x40) = (s16)(rnd() % 0x10000); S16(o + 0x44) = (s16)(rnd() % 0x10000); }
            if (rnd() % 3 == 0) {                               /* in the water's quads */
                gptr q = rnd() % 2 ? 0x7032B0 : 0x7032C0;
                S16(o + 0x40) = (s16)((S16(q) + S16(q + 4) + S16(q + 8) + S16(q + 12)) / 4);
                S16(o + 0x44) = (s16)((S16(q + 2) + S16(q + 6) + S16(q + 10) + S16(q + 14)) / 4);
            }
            if (rnd() % 2 && U8(CL_ROOM)) {                     /* on a floor polygon */
                gptr w = CL_ROOM + U32(CL_ROOM + 8 + 4 * (rnd() % U8(CL_ROOM))), v = w + 8 + U8(w + 1) * 4;
                s32 sx = 0, sz = 0, n = U8(w + 1) ? U8(w + 1) : 1;
                for (s32 j = 0; j < U8(w + 1); j++) { sx += S16(v + 4 * j); sz += S16(v + 4 * j + 2); }
                S16(o + 0x40) = (s16)(sx / n); S16(o + 0x44) = (s16)(sz / n);
                U16(o + 0xE0) = U16(o + 0x40); U16(o + 0xE4) = U16(o + 0x44); S16(o + 0xE2) = (s16)(rnd() % 2 ? -10000 : 10000);
            }
            if (k % 11 == 4 && rnd() % 2) {                     /* staggering: low health, falls? */
                U16(o + 0x170) &= ~1; S16(o + 0x10A) = 0x400; S16(o + 0x108) = rnd() % 0x80; U16(o + 0x176) = rnd() % 0x10; U8(e + 0x18) = 1;
                U16(0x87DE10) = rand8_seed((u8[]){0x3F, 0x40, 0x41}[rnd() % 3], 0);
            }
            if (rnd() % 2) {                                    /* the height at the floor's edges */
                s16 h = (s16)Floor_Height(o + 0xE0, o + 0x40, 0x40, 0);
                s32 fl = -1000 * ((s32)h / -1000), d = (s32)(rnd() % 3) - 1;
                S16(o + 0x42) = (s16)(rnd() % 3 == 0 ? h - 0x118 + d : rnd() % 2 ? fl - 0x118 + d : fl + d);
            }
            anim_maybe_end(o);
            U32(LOG_BASE) = 0;
            sprintf(g_what, "raptor %X hit step %u fl %X kind %u health %d/%d dmg %d y %d", o, U8(o + 0x56), U16(o + 0x174), U16(o + 0x172),
                    S16(o + 0x108), S16(o + 0x10A), S16(o + 0x176), S16(o + 0x42));
            check(ht[k % 11].addr, ht[k % 11].fn, ht[k % 11].name, 1, a, 0);
        }
        model_data(0x633000);
        if (!U32(G() + 0xB50)) U32(G() + 0xB50) = O_OBJ + 0x260 * 15;   /* room 407: rides the part */
        check(0x41F660, Raptor_Init, "Raptor_Init", 1, a, 0);
        game_state(); world_setup(); enemy_state();
        o = O_OBJ + 0x260 * (1 + rnd() % 6); a[0] = o;
        anim_setup_at(o);
        U8(o + 0x55) = rnd() % 4; U8(o + 0x170) = (u8)rnd(); U8(o + 0x11C) = (u8)rnd();
        U32(LOG_BASE) = 0;
        sprintf(g_what, "raptor %X dies %02X", o, U8(o + 0x55));
        check(0x41F9D0, Raptor_Die, "Raptor_Die", 1, a, 0);
        /* the normal sub-states, each step */
        static const struct { u32 addr; void *fn; const char *name; } st[] = {
            {0x41FF30, Raptor_Wait, "Raptor_Wait"}, {0x420190, Raptor_Walk, "Raptor_Walk"}, {0x4203B0, Raptor_Attack, "Raptor_Attack"},
            {0x4204A0, Raptor_Cry, "Raptor_Cry"}, {0x4205D0, Raptor_Back, "Raptor_Back"}, {0x420660, Raptor_Eat, "Raptor_Eat"},
            {0x4207E0, Raptor_Enter, "Raptor_Enter"}, {0x420960, Raptor_Feint, "Raptor_Feint"}};
        gptr e = o + 0x1C0;
        U32(0x74AF00) = e;
        for (unsigned k = 0; k < sizeof st / sizeof st[0]; k++) {
            U8(o + 0x56) = rnd() % 4; U8(o + 0x18C) = (u8)rnd(); U8(o + 0x18D) = rnd() % 2 ? 0 : (u8)rnd();
            U16(o + 0x172) = rnd() % 3; U8(o + 0x180) = (u8)rnd(); U8(o + 0xFF) = (u8)rnd();
            U16(o + 0x92) = rnd() % 3 == 0 ? 0x29 + rnd() % 3 : rnd() % 2 ? (u16)(U16(o + 0x90) - 1) : (u16)rnd();   /* the edge, the last frame */
            u32 da = rnd() % 2 ? rnd() % 0x1000 : rnd() % 4 == 0 ? 0x800 : (u32[]){0x200, 0x400, 0x600, 0x700, 0x800, 0x900, 0xA00, 0xC00, 0xE00}[rnd() % 9] + rnd() % 3 - 1;
            if (rnd() % 2) U16(0x87DE10) = rand8_seed((u8[]){0x63, 0x64, 0x65, 0x7F, 0x80, 0x1F, 0x20}[rnd() % 7], 0);
            {   /* the player near, at the edges of the angles it looks within */
                double an = (U16(o + 0x4A) + (rnd() % 2 ? 1 : -1) * (s32)(u32[]){0xFF, 0x101, 0x1FF, 0x201, 0x27F, 0x281, 0x400}[rnd() % 7]) * 3.14159265358979 / 2048;
                double dd = 1000 + rnd() % 3000;
                S16(O_OBJ + 0x40) = (s16)(S16(o + 0x40) + dd * sin(an)); S16(O_OBJ + 0x44) = (s16)(S16(o + 0x44) + dd * cos(an));
            }
            U16(e + 8) = (u16)(U16(o + 0x4A) + da);
            U16(e + 0x28) = rnd() % 2 ? 0x1C0 + rnd() % 4 : (u16)rnd();
            U8(e + 0x1C) = rnd() % 2 ? rnd() % 2 : (u8)rnd();
            S16(O_OBJ + 0x108) = (s16)(rnd() % 3) - 1;
            U32(LOG_BASE) = 0;
            anim_maybe_end(o);
            sprintf(g_what, "raptor %X step %u angle %X level %02X", o, U8(o + 0x56), da, U8(o + 0x18C));
            check(st[k].addr, st[k].fn, st[k].name, 1, a, 0);
        }
        for (int k = 0; k < 3; k++) {
            raptor_jump_setup(o);
            sprintf(g_what, "raptor %X jump step %u y %d level %d vy %d", o, U8(o + 0x56), S16(o + 0x42), S16(o + 0x5A), S16(e + 0x14));
            check(0x421F50, Raptor_Climb, "Raptor_Climb", 1, a, 0);
            raptor_jump_setup(o);
            check(0x4221E0, Raptor_JumpOver, "Raptor_JumpOver", 1, a, 0);
        }
        /* the pounce, with the player animated too (its animations in the raptor's table) */
        game_state(); world_setup(); enemy_state();
        o = O_OBJ + 0x260 * (1 + rnd() % 6); a[0] = o; e = o + 0x1C0;
        anim_setup_at(O_OBJ);
        do anim_setup_at(o); while (U32(o + 0x68) < 16);  /* all the parts the player's weapon may look for */
        memcpy(MEM(O_OBJ + 0x60), MEM(o + 0x60), 0x50);   /* the same parts and animation data */
        memcpy(MEM(O_OBJ + 0x260 * 7 + 0x60), MEM(o + 0x60), 0x50);   /* ... for the part it may ride */
        U32(G() + 0x940) = O_OBJ;
        for (int k = 0x40; k < 0xC0; k++) U32(AN_TAB + 4 * k) = AN_DATA + 0x1000 * ((k ^ k >> 4 ^ k >> 5) & 1);   /* per character apart */
        U32(0x74AF00) = e;
        U32(G() + 0xB50) = O_OBJ + 0x260 * 7; U32(o + 0x184) = (u32)rnd() & ~(rnd() % 4 ? 0x40u : 0);
        U8(G() + 0x108F) = rnd() % 4;
        u8 sv[0x260]; memcpy(sv, MEM(o), 0x260);
        for (int r = 0; r < 3; r++) {
        memcpy(MEM(o), sv, 0x260);
        U8(o + 0x57) = rnd() % 4 ? rnd() % 13 : 0xB; U8(o + 0xFF) = (u8)rnd();
        U16(o + 0x92) = rnd() % 2 ? (u16[]){5, 6, 0xD, 0xE, 0x11, 0x12, 0x13}[rnd() % 7] : (u16)rnd();
        U16(e + 2) = rnd() % 2 ? rnd() % 13 : (u16)rnd();
        for (int k = 0; k < 2; k++) { gptr q = k ? O_OBJ + 0x260 * 7 : O_OBJ; S16(q + 0x40) = S16(o + 0x40) + (s16)(rnd() % 6000) - 3000; S16(q + 0x44) = S16(o + 0x44) + (s16)(rnd() % 6000) - 3000; }
        U32(LOG_BASE) = 0;
        anim_maybe_end(o);
        sprintf(g_what, "raptor %X pounce step %u frame %u shake %u", o, U8(o + 0x57), U16(o + 0x92), U16(e + 2));
        check(0x420A40, Raptor_Pounce, "Raptor_Pounce", 1, a, 0);
        }
        /* the other attacks and their hits (o, player) */
        static const struct { u32 addr; void *fn; const char *name; int n; } at[] = {
            {0x421460, Raptor_SideBite, "Raptor_SideBite", 1}, {0x4216B0, Raptor_Leap, "Raptor_Leap", 1}, {0x421C10, Raptor_Tail, "Raptor_Tail", 1},
            {0x421290, Raptor_LungeHit, "Raptor_LungeHit", 2}, {0x4215A0, Raptor_SideBiteHit, "Raptor_SideBiteHit", 2},
            {0x421870, Raptor_LeapHit, "Raptor_LeapHit", 2}, {0x421CC0, Raptor_TailHit, "Raptor_TailHit", 2}};
        u32 b[2] = {o, O_OBJ};
        for (unsigned k = 0; k < 2 * sizeof at / sizeof at[0]; k++) {
            gptr p = U32(o + 0x184) & 0x40 ? O_OBJ + 0x260 * 7 : O_OBJ;
            U8(o + 0x57) = at[k % 7].n == 2 ? (rnd() % 4 ? 1 : rnd() % 4) : rnd() % 5;
            U16(o + 0x92) = rnd() % 2 ? (u16[]){0xC, 0xD, 0x16, 0x17, 0x18}[rnd() % 5] : (u16)rnd();
            U8(o + 0x119) = rnd() % 2 ? 0 : rnd() % 2 ? 3 : (u8)rnd(); U8(o + 0x10C) = (u8)rnd();
            for (int j = 0x19; j < 0x24; j++) U8(e + j) = rnd() % 2;
            U8(e + 0x23) = rnd() % 8; U8(e + 0xC) = rnd() % 3; U8(e + 0xD) = rnd() % 3;
            U16(e + 8) = rnd() % 4 ? (u16)rnd() & 0xFFF : (u16)(0x7FF + rnd() % 3);
            if (rnd() % 3 == 0) U8(o + 0x10C) = (u8)((rnd() & 0xF0) | (1 + rnd() % 3));
            if (rnd() % 2) U16(0x87DE10) = rand8_seed((u8[]){0x1F, 0x20, 0x21, 0x5F, 0x60, 0x61}[rnd() % 6], 0);
            U16(p + 0x4A) = (u16)(U16(o + 0x4A) + 0x400 * (rnd() % 4) + rnd() % 0x400 - 0x200);
            U16(p + 0x178) = rnd() % 2 ? 0 : (u16)rnd(); U16(p + 0x174) = rnd() % 2 ? 0 : (u16)rnd();
            U8(p + 0x55) = rnd() % 2 ? 2 : (u8)rnd();
            U8(G() + 0x108E) = rnd() % 3;
            S16(p + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x60 : rnd() % 2 ? 0x100 + rnd() % 0x400 : rnd());
            if (rnd() % 3 == 0)                                 /* left at 0 by the pinning damage */
                S16(p + 0x108) = (s16)(Damage_Scale(U8(0x7031C4 + U8(e + 0x23) * 9 + 7 + rnd() % 2)) + rnd() % 3 - 1);
            U32(LOG_BASE) = 0;
            anim_maybe_end(o);
            sprintf(g_what, "raptor %X step %u frame %X dir %X", o, U8(o + 0x57), U16(o + 0x92), (U16(p + 0x4A) - U16(o + 0x4A)) & 0xFFF);
            check(at[k % 7].addr, at[k % 7].fn, at[k % 7].name, at[k % 7].n, b, 0);
        }
    }
    player_tables_stub(0);
    raptor_tables_stub(0);
    stubs_install(0);
    g_ign_objpads = 0;
}

/* ---- the oviraptor: its tables of states replaced by recorders, like the raptor's ---- */
static const u32 g_ovi_tabs[][3] = {{0x7212C8, 5, 1}, {0x7212DC, 9, 1}, {0x721300, 5, 1}, {0x721314, 6, 2}, {0x72132C, 6, 2}};
static const int g_ovi_rec2[] = {28, 29, 51, 52, 53, 54};
static u32 g_ovi_saved[5][9];
static void ovi_tables_stub(int on)
{
    for (int k = 0; k < 9; k++) g_stubs[g_rap_rec1[k]].spec = on ? "d" : "";
    for (int k = 0; k < 6; k++) g_stubs[g_ovi_rec2[k]].spec = on ? "dd" : "";
    for (int t = 0; t < 5; t++)
        for (u32 k = 0; k < g_ovi_tabs[t][1]; k++) {
            gptr a = g_ovi_tabs[t][0] + 4 * k;
            if (on) { g_ovi_saved[t][k] = U32(a); U32(a) = (u32)(uintptr_t)g_recs[g_ovi_tabs[t][2] == 1 ? g_rap_rec1[k] : g_ovi_rec2[k]]; }
            else U32(a) = g_ovi_saved[t][k];
        }
}

static gptr ovi_setup(void)
{
    gptr o = raptor_setup(), e = o + 0x1C0;
    U32(0x74AFA8) = e;
    U8(o + 0x57) = rnd() % 3 ? rnd() % 3 : rnd() % 8;
    U16(e + 0x2A) = rnd() % 2 ? rnd() % 3 : (u16)rnd();
    U16(o + 0x1E8) = rnd() % 2 ? 9 + rnd() % 4 : (u16)rnd();
    U8(e + 0x1E) = rnd() % 2; U8(e + 0x22) = rnd() % 2; U8(e + 0x23) = rnd() % 8;
    return o;
}

/* the angle to the target about a value (o+0x4A + d), the target near */
static void ovi_aim(gptr o, gptr t)
{
    u32 d = rnd() % 3 == 0 ? (u32[]){0, 1, 0xFFF, 0x2F, 0x30, 0x31, 0xFCF, 0xFD0, 0xFD1}[rnd() % 9] : rnd() % 2 ? rnd() % 0x1000 : (u32[]){0xFF, 0x100, 0x101, 0x1FF, 0x200, 0x201, 0x6FF, 0x700, 0x701, 0x8FF, 0x900, 0x901, 0xDFF, 0xE00, 0xE01, 0xF00, 0xF01}[rnd() % 17];
    double an = ((U16(o + 0x4A) + d) & 0xFFF) * 3.14159265358979 / 2048, dd = rnd() % 2 ? 1000 + rnd() % 4000 : (u32[]){0x7A1, 0xBB8, 0xC35, 0x9C4}[rnd() % 4];
    S16(t + 0x40) = (s16)(S16(o + 0x40) + dd * sin(an)); S16(t + 0x44) = (s16)(S16(o + 0x44) + dd * cos(an));
    U16(o + 0x1C0 + 8) = (u16)((U16(o + 0x4A) + d) & 0xFFF);
    if (rnd() % 2) { U16(o + 0xD8) = U16(t + 0x40); U16(o + 0xDC) = U16(t + 0x44); }
    if (rnd() % 2) {                                    /* exactly: the target on an axis, it faces d off */
        static const s16 dist[] = {1000, 1999, 2000, 2001, 2999, 3000, 3001, 4800};
        s16 dd = dist[rnd() % 8];
        if (rnd() % 2) { S16(t + 0x40) = (s16)(S16(o + 0x40) + (rnd() % 2 ? dd : -dd)); S16(t + 0x44) = S16(o + 0x44); }
        else { S16(t + 0x44) = (s16)(S16(o + 0x44) + (rnd() % 2 ? dd : -dd)); S16(t + 0x40) = S16(o + 0x40); }
        U16(o + 0xD8) = U16(t + 0x40); U16(o + 0xDC) = U16(t + 0x44);
        u16 an = (u16)Angle_XZ(o + 0x40, t + 0x40);
        U16(o + 0x4A) = (u16)((an - d) & 0xFFF); U16(o + 0x1C0 + 8) = an;
    }
}

static void test_ovi(void)
{
    g_ign_objpads = 1;
    stubs_install(1);
    ovi_tables_stub(1);
    player_tables_stub(1);
    for (int i = 0; i < iters; i++) {
        game_state();
        gptr o = ovi_setup(), e = o + 0x1C0;
        u32 a[2] = {o};
        sprintf(g_what, "ovi %X state %02X/%02X/%02X level %X", o, U8(o + 0x54), U8(o + 0x55), U8(o + 0x56), U16(o + 0x10C));
        check(0x4425D0, Ovi_Think, "Ovi_Think", 1, a, 0);
        for (int k = 0; k < 16; k++) {                      /* the edges of its choices */
            gptr t = U8(o + 0x184) & 0x40 ? U32(G() + 0xB50) : U32(G() + 0x940);
            U8(o + 0x18D) = rnd() % 4 ? 0 : (u8)rnd(); U8(o + 0x17B) = rnd() % 4 ? 0 : 1;
            U8(o + 0x55) = rnd() % 4 ? 0 : (u8)rnd(); U8(o + 0x56) = rnd() % 4 ? 0 : (u8)rnd(); U8(o + 0x13A) = rnd() % 2;
            ovi_aim(o, t);
            U16(t + 0x5A) = rnd() % 4 ? U16(o + 0x5A) : (u16)rnd(); U16(t + 0x174) = rnd() % 2 ? 0 : (u16)rnd();
            U8(t + 0x55) = rnd() % 2 ? 8 : (u8)rnd();
            if (rnd() % 2) U16(0x87DE10) = rand8_seed((u8[]){0x2F, 0x30, 0x31, 0x3F, 0x40, 0x41, 0x8F, 0x90, 0x91, 0x10, 0}[rnd() % 11], 0);
            U32(LOG_BASE) = 0;
            sprintf(g_what, "ovi %X think dist %u", o, Dist2_XZ(o + 0x40, t + 0x40));
            check(0x4425D0, Ovi_Think, "Ovi_Think", 1, a, 0);
        }
        U8(o + 0x54) = rnd() % 5; U8(o + 0x55) = rnd() % 9; U8(o + 0x56) = rnd() % 6; U16(o + 0x172) = rnd() % 5;   /* (in the tables) */
        check(0x4423D0, Ovi_Normal, "Ovi_Normal", 1, a, 0);
        check(0x442410, Ovi_Hit, "Ovi_Hit", 1, a, 0);
        check(0x4427E0, Ovi_Grab, "Ovi_Grab", 1, a, 0);
        check(0x441FB0, Ovi_Handler, "Ovi_Handler", 1, a, 0);
        U8(o + 0x11A) = (u8)rnd();
        anim_setup_at(AN_OBJ); memcpy(MEM(o + 0x60), MEM(AN_OBJ + 0x60), 0x50);
        for (int k = 0x40; k < 0xC0; k++) U32(AN_TAB + 4 * k) = AN_DATA + 0x1000 * ((k ^ k >> 4 ^ k >> 5) & 1);
        for (int k = 0; k < 2; k++) {                       /* holding the player: grabs on from the handler */
            gptr t = U8(o + 0x184) & 0x40 ? U32(G() + 0xB50) : U32(G() + 0x940);
            U8(o + 0x54) = 1; U8(o + 0x55) = 2; U8(o + 0x56) = rnd() % 6;   /* (state 1 moves the parts) */ U8(o + 0x119) = rnd() % 4 ? 0 : 0x80;
            U8(t + 0x174) &= rnd() % 4 ? 0xFE : 0xFF; U32(o + 0x50) = (U32(o + 0x50) & ~0x40000000u) | 0x30;
            U32(o + 0xA0) = 1; U16(o + 0x102) &= ~2; U32(t + 0x134) = 0; U8(t + 0x17B) = 0;   /* (no collisions; its hit left as it is: the target has no hit boxes) */
            U32(LOG_BASE) = 0;
            check(0x441FB0, Ovi_Handler, "Ovi_Handler", 1, a, 0);
        }
        U32(0x74AF00) = e; raptor_jump_setup(o); U32(0x74AFA8) = e;
        if (rnd() % 4 == 0) S16(o + 0x42) = (s16)(0x7918 - rnd() % 0x30);
        sprintf(g_what, "ovi %X jump step %u y %d level %d", o, U8(o + 0x56), S16(o + 0x42), S16(o + 0x5A));
        check(0x442B60, Ovi_Jump, "Ovi_Jump", 1, a, 0);
        for (int k = 0; k < 3; k++) {
            U32(0x74AF00) = e; raptor_jump_setup(o); U32(0x74AFA8) = e;
            U8(e + 0x1E) = rnd() % 2; U16(e + 0x10) = rnd() % 2 ? U16(o + 0x4A) : (u16)rnd();
            if (rnd() % 2) S16(o + 0xF4) = (s16)(0xB3 + rnd() % 3);
            sprintf(g_what, "ovi %X climb step %u y %d level %d vy %d", o, U8(o + 0x56), S16(o + 0x42), S16(o + 0x5A), S16(e + 0x14));
            check(0x442BF0, Ovi_Climb, "Ovi_Climb", 1, a, 0);
            U32(0x74AF00) = e; raptor_jump_setup(o); U32(0x74AFA8) = e;
            check(0x442E90, Ovi_JumpOver, "Ovi_JumpOver", 1, a, 0);
        }
        /* hits */
        static const struct { u32 addr; void *fn; const char *name; } ht[] = {
            {0x444470, Ovi_Hurt, "Ovi_Hurt"}, {0x4444A0, Ovi_HurtStand, "Ovi_HurtStand"}, {0x4447F0, Ovi_HurtDown, "Ovi_HurtDown"},
            {0x444980, Ovi_Knock, "Ovi_Knock"}, {0x4449B0, Ovi_KnockStand, "Ovi_KnockStand"}, {0x444B80, Ovi_KnockDown, "Ovi_KnockDown"},
            {0x444D00, Ovi_Blow, "Ovi_Blow"}, {0x444D30, Ovi_BlowStand, "Ovi_BlowStand"}, {0x444F90, Ovi_BlowDown, "Ovi_BlowDown"},
            {0x445190, Ovi_HurtAir, "Ovi_HurtAir"}};
        const unsigned nh = sizeof ht / sizeof ht[0];
        for (unsigned k = 0; k < 2 * nh; k++) {
            U32(o + 0x50) ^= rnd() & 0x2878;
            U32(0x74AFA8) = e;
            U8(o + 0x56) = rnd() % 4 ? rnd() % 2 : rnd() % 4; U16(o + 0x174) = (u16)(rnd() % 2 ? rnd() & 6 : rnd());
            U16(o + 0x170) = (u16)rnd(); U16(o + 0x172) = rnd() % 5; U16(o + 0x178) = rnd() % 2 ? rnd() % 4 : (u16)rnd() & 0xFFF;
            U8(o + 0x184) = (u8)rnd(); U8(o + 0x180) = (u8)rnd(); U8(o + 0xFF) = (u8)rnd();
            S16(o + 0x10A) = (s16)(rnd() % 0x400); S16(o + 0x108) = (s16)(rnd() % 3 ? rnd() % 0x200 : rnd());
            U16(o + 0x176) = rnd() % 2 ? (u16)(U16(o + 0x108) + rnd() % 3 - 1) : rnd() % 0x100;
            U8(e + 0x1D) = (u8)rnd(); U8(e + 0x1A) = (u8)rnd();
            U16(o + 0x92) = rnd() % 2 ? (u16[]){0x12, 0x13, 0x14, 0x1B, 0x1C, 0x1D}[rnd() % 6] : (u16)rnd();
            if (rnd() % 2) U16(0x87DE10) = rand8_seed((u8[]){0x3F, 0x40, 0x41, 0x0F, 0x10, 0}[rnd() % 6], 0);
            if (k % nh == 1 && rnd() % 2) {                     /* low health: knocked down? */
                S16(o + 0x10A) = 0x400; S16(o + 0x108) = rnd() % 0x120; U16(o + 0x176) = rnd() % 0x10;
                U16(0x87DE10) = rand8_seed((u8[]){0x3F, 0x40, 0x41}[rnd() % 3], 0);
            }
            if (rnd() % 2 && U8(CL_ROOM)) {
                gptr w = CL_ROOM + U32(CL_ROOM + 8 + 4 * (rnd() % U8(CL_ROOM))), v = w + 8 + U8(w + 1) * 4;
                s32 sx = 0, sz = 0, n = U8(w + 1) ? U8(w + 1) : 1;
                for (s32 j = 0; j < U8(w + 1); j++) { sx += S16(v + 4 * j); sz += S16(v + 4 * j + 2); }
                S16(o + 0x40) = (s16)(sx / n); S16(o + 0x44) = (s16)(sz / n);
                U16(o + 0xE0) = U16(o + 0x40); U16(o + 0xE4) = U16(o + 0x44); S16(o + 0xE2) = (s16)(rnd() % 2 ? -10000 : 10000);
            }
            if (rnd() % 2) {                                    /* about the floor's height */
                s16 h = (s16)Floor_Height(o + 0xE0, o + 0x40, 0x40, 0);
                S16(o + 0x42) = (s16)(h - (rnd() % 2 ? 0xF0 : 0) + (s32)(rnd() % 3) - 1);
            }
            anim_maybe_end(o);
            U32(LOG_BASE) = 0;
            sprintf(g_what, "ovi %X hit step %u fl %X kind %u health %d/%d dmg %d y %d", o, U8(o + 0x56), U16(o + 0x174), U16(o + 0x172),
                    S16(o + 0x108), S16(o + 0x10A), S16(o + 0x176), S16(o + 0x42));
            check(ht[k % nh].addr, ht[k % nh].fn, ht[k % nh].name, 1, a, 0);
        }
        model_data(0x63D000);
        if (!U32(G() + 0xB50)) U32(G() + 0xB50) = O_OBJ + 0x260 * 15;
        if (rnd() % 2) U16(G() + 0x1090) = (u16)(0x400 | (rnd() % 2 ? 8 + rnd() % 2 : rnd() % 16));
        check(0x442080, Ovi_Init, "Ovi_Init", 1, a, 0);
        game_state(); world_setup(); enemy_state();
        o = O_OBJ + 0x260 * (1 + rnd() % 6); a[0] = o; e = o + 0x1C0;
        anim_setup_at(o);
        U8(o + 0x55) = rnd() % 4; U8(o + 0x170) = (u8)rnd(); U8(o + 0x11C) = (u8)rnd(); U8(o + 0x184) = (u8)rnd();
        U32(LOG_BASE) = 0;
        sprintf(g_what, "ovi %X dies %02X", o, U8(o + 0x55));
        check(0x442450, Ovi_Die, "Ovi_Die", 1, a, 0);
        /* the normal sub-states, each step */
        static const struct { u32 addr; void *fn; const char *name; } st[] = {
            {0x442840, Ovi_Walk, "Ovi_Walk"}, {0x4430A0, Ovi_Look, "Ovi_Look"}, {0x4432D0, Ovi_Hide, "Ovi_Hide"},
            {0x4433D0, Ovi_Turn, "Ovi_Turn"}, {0x4429B0, Ovi_Enter, "Ovi_Enter"}};
        U32(0x74AFA8) = e;
        for (unsigned k = 0; k < 2 * sizeof st / sizeof st[0]; k++) {
            U8(o + 0x56) = rnd() % 4; U8(o + 0x18C) = (u8)rnd(); U8(o + 0x18D) = rnd() % 2 ? 0 : (u8)rnd();
            U8(o + 0x180) = (u8)rnd(); U8(o + 0xFF) = (u8)rnd();
            U16(o + 0x92) = rnd() % 3 == 0 ? 0x2E + rnd() % 3 : rnd() % 2 ? (u16)(U16(o + 0x90) - 1) : (u16)rnd();
            if (rnd() % 2) U16(0x87DE10) = rand8_seed((u8[]){0xEF, 0xF0, 0xF1, 0x80, 0x81, 0}[rnd() % 6], 0);
            ovi_aim(o, O_OBJ);
            U32(e + 4) = rnd() % 2 ? Dist2_XZ(o + 0x40, O_OBJ + 0x40) : (u32[]){0x3D08FF, 0x3D0900, 0x7A11FF, 0x7A1200, 0x989680, 0x989681}[rnd() % 6];
            U16(e + 0x2A) = rnd() % 2 ? rnd() % 2 : (u16)rnd();
            if (U16(o + 0x90))                                  /* the frames it blends between: of its animation (not left at 0) */
                for (u32 j = 0x78; j <= 0x84; j += 4) U32(o + j) = U32(o + 0x8C) + (rnd() % U16(o + 0x90)) * U8(o + 0xAC);
            U32(LOG_BASE) = 0;
            anim_maybe_end(o);
            sprintf(g_what, "ovi %X step %u level %02X", o, U8(o + 0x56), U8(o + 0x18C));
            check(st[k % 5].addr, st[k % 5].fn, st[k % 5].name, 1, a, 0);
        }
        { u32 c[3] = {o, rnd() % 0x100, O_OBJ + 0x40}; check(0x48F440, Obj_TurnToPoint, "Obj_TurnToPoint", 3, c, 0); }
        for (int k = 0; k < 6; k++) {                       /* at the ends of animations: looking about, turning */
            int look = k < 4;
            U8(o + 0x56) = look ? (k < 2 ? 0 : 2) : 1;
            if (U16(o + 0x90))
                for (u32 j = 0x78; j <= 0x84; j += 4) U32(o + j) = U32(o + 0x8C) + (rnd() % U16(o + 0x90)) * U8(o + 0xAC);
            ovi_aim(o, O_OBJ);
            U32(e + 4) = (u32[]){0x7A11FF, 0x7A1200, 0x7A1201, 0x98967F, 0x989680, 0x989681}[rnd() % 6];
            U16(0x87DE10) = rand8_seed(look ? (u8)(rnd() % 2 ? 1 : 3) : (u8[]){0xEF, 0xF0, 0xF1, 1, 2}[rnd() % 5], 0);
            U16(o + 0x102) &= ~0x20; U16(o + 0x94) = (u16)(U16(o + 0x90) - 1); U16(o + 0x98) = 0x7000;   /* the animation ends */
            U32(LOG_BASE) = 0;
            sprintf(g_what, "ovi %X %s at the end, step %u angle %X dist %X", o, look ? "looks" : "turns", U8(o + 0x56), (U16(e + 8) - U16(o + 0x4A)) & 0xFFF, U32(e + 4));
            if (look) check(0x4430A0, Ovi_Look, "Ovi_Look", 1, a, 0);
            else check(0x4433D0, Ovi_Turn, "Ovi_Turn", 1, a, 0);
        }
        for (int k = 0; k < 3; k++) {                       /* an attack's step; the player's health about 0 */
            U8(o + 0x56) = rnd() % 6; S16(O_OBJ + 0x108) = (s16)(rnd() % 3) - 1; U32(G() + 0x940) = O_OBJ;
            U32(LOG_BASE) = 0;
            sprintf(g_what, "ovi %X attack step %u health %d", o, U8(o + 0x56), S16(O_OBJ + 0x108));
            check(0x442B20, Ovi_Attack, "Ovi_Attack", 1, a, 0);
        }
        /* the attacks and their hits (o, player), the player animated (its animations in the oviraptor's table) */
        game_state(); world_setup(); enemy_state();
        o = O_OBJ + 0x260 * (1 + rnd() % 6); a[0] = o; e = o + 0x1C0;
        anim_setup_at(O_OBJ);
        do anim_setup_at(o); while (U32(o + 0x68) < 16);
        memcpy(MEM(O_OBJ + 0x60), MEM(o + 0x60), 0x50);
        memcpy(MEM(O_OBJ + 0x260 * 7 + 0x60), MEM(o + 0x60), 0x50);
        U32(G() + 0x940) = O_OBJ;
        for (int k = 0x40; k < 0xC0; k++) U32(AN_TAB + 4 * k) = AN_DATA + 0x1000 * ((k ^ k >> 4 ^ k >> 5) & 1);
        U32(0x74AFA8) = e;
        U32(G() + 0xB50) = O_OBJ + 0x260 * 7; U32(o + 0x184) = (u32)rnd() & ~(rnd() % 4 ? 0x40u : 0);
        U8(G() + 0x108F) = rnd() % 4;
        static const struct { u32 addr; void *fn; const char *name; u8 steps; } at[] = {
            {0x4434B0, Ovi_LeapBack, "Ovi_LeapBack", 3}, {0x4436B0, Ovi_Peck, "Ovi_Peck", 2}, {0x443730, Ovi_Pounce, "Ovi_Pounce", 7},
            {0x443E30, Ovi_Lunge, "Ovi_Lunge", 2}, {0x443FA0, Ovi_Kick, "Ovi_Kick", 3}, {0x4441A0, Ovi_BackJump, "Ovi_BackJump", 5},
            {0x4435D0, Ovi_GrabBite, "Ovi_GrabBite", 2}, {0x443C70, Ovi_PounceCatch, "Ovi_PounceCatch", 2}, {0x443EF0, Ovi_LungeHit, "Ovi_LungeHit", 2},
            {0x4440D0, Ovi_KickHit, "Ovi_KickHit", 2}, {0x444330, Ovi_BackCatch, "Ovi_BackCatch", 2}};
        const unsigned na = sizeof at / sizeof at[0];
        u8 sv[0x260]; memcpy(sv, MEM(o), 0x260);
        u32 b[2] = {o, O_OBJ};
        for (unsigned k = 0; k < 3 * na; k++) {
            memcpy(MEM(o), sv, 0x260);
            gptr p = O_OBJ;
            U8(o + 0x57) = rnd() % 4 ? rnd() % at[k % na].steps : (u8)rnd();
            if (at[k % na].steps == 2 && k % na >= 6) U8(o + 0x57) = rnd() % 4 ? 1 : rnd() % 3;
            U16(o + 0x92) = rnd() % 2 ? (u16[]){3, 4, 7, 8, 9, 0x18, 0x19, 0x1A, 0x1B, 0x23, 0x24}[rnd() % 11] : (u16)rnd();
            U8(o + 0x119) = rnd() % 2 ? 0 : (u8)rnd(); U8(o + 0x10C) = (u8)rnd(); U8(o + 0x184) = (u8)rnd();
            U16(o + 0x1E8) = rnd() % 2 ? 9 + rnd() % 4 : (u16)rnd();
            for (int j = 0x19; j < 0x24; j++) U8(e + j) = rnd() % 2;
            U8(e + 0x23) = rnd() % 8;
            U16(e + 8) = (u16)rnd() & 0xFFF;
            U16(p + 0x172) = rnd() % 4; U16(p + 0x174) = rnd() % 2 ? 0 : (u16)rnd();
            U8(G() + 0x108E) = rnd() % 3;
            S16(p + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x60 : rnd());
            U16(p + 0x4A) = (u16)(U16(o + 0x4A) + 0x400 * (rnd() % 4) + rnd() % 0x400 - 0x200);
            U32(LOG_BASE) = 0;
            anim_maybe_end(o);
            sprintf(g_what, "ovi %X attack step %u frame %X", o, U8(o + 0x57), U16(o + 0x92));
            check(at[k % na].addr, at[k % na].fn, at[k % na].name, 2, b, 0);
        }
    }
    player_tables_stub(0);
    ovi_tables_stub(0);
    stubs_install(0);
    g_ign_objpads = 0;
}

/* ---- the allosaurus ---- */
static const u32 g_allo_tabs[][2] = {{0x720FB8, 5}, {0x720FCC, 16}, {0x72100C, 16}, {0x72104C, 4}, {0x72105C, 4}, {0x72106C, 16}};
static const int g_allo_rec[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
static u32 g_allo_saved[6][16];
static void allo_tables_stub(int on)
{
    for (int k = 0; k < 16; k++) g_stubs[g_allo_rec[k]].spec = on ? "d" : "";
    for (int t = 0; t < 6; t++)
        for (u32 k = 0; k < g_allo_tabs[t][1]; k++) {
            gptr a = g_allo_tabs[t][0] + 4 * k;
            if (on) { g_allo_saved[t][k] = U32(a); U32(a) = (u32)(uintptr_t)g_recs[g_allo_rec[k]]; }
            else U32(a) = g_allo_saved[t][k];
        }
}

static const u32 g_allo_dist[] = {0, 0x7CF, 0x7D0, 0x9C4, 0x9C5, 0xBB7, 0xBB8, 0xDAB, 0xDAC, 0xDAD, 0xF9F, 0xFA0, 0x1387, 0x1388,
                                  0x1389, 0x176F, 0x1770, 0x1B57, 0x1B58, 0x2EE0, 0x2EE1, 0x7FFFFFFF};
static u32 allo_dist(void) { return rnd() % 3 ? g_allo_dist[rnd() % 22] : rnd() % 0x3000; }

/* an allosaurus among the test objects; the player its target */
static gptr allo_setup(void)
{
    gptr o = raptor_setup(), pl = U32(G() + 0x940);
    U32(o + 0x184) &= ~0x40u;
    U32(o + 0x1C0) = rnd() % 4 ? rnd() & 0x3FF : rnd();
    U32(o + 0x1C4) = rnd() % 4 ? rnd() % 3 : rnd();
    U8(o + 0x1C8) = 1 + rnd() % 3; U8(o + 0x1C9) = rnd() % 16; U8(o + 0x1CA) = rnd() % 6; U8(o + 0x1CB) = rnd() % 2;
    for (u32 k = 0x1CC; k <= 0x1D8; k += 4) U32(o + k) = rnd() % 3 ? rnd() % 3 : rnd() % 2 ? rnd() % 0x800 : rnd();
    U32(o + 0x1DC) = rnd() % 0x20;
    for (u32 k = 0x1E0; k < 0x210; k += 2) U16(o + k) = rnd() % 3 ? rnd() % 3 : (u16)rnd();
    U16(o + 0x1F8) = (u16)(rnd() % 2 ? rnd() % 0x400 - 0x200 : rnd());
    U16(o + 0x202) = rnd() % 2 ? (u16)(0x1D + rnd() % 3) : rnd() % 2 ? rnd() % 0x20 : (u16)rnd();
    U8(o + 0x206) = rnd() % 4; U8(o + 0x207) = (u8)rnd();
    U8(o + 0x210) = rnd() % 2;
    U32(o + 0x13C) = allo_dist();
    U8(o + 0x57) = rnd() % 2;
    U32(o + 0x120) = U32(o + 0x64); U32(o + 0x124) = U32(U32(o + 0x64) + 0x54) ? U32(U32(o + 0x64) + 0x54) : U32(o + 0x64);   /* its feet */
    U16(pl + 0x5A) = rnd() % 3 ? U16(o + 0x5A) : rnd() % 2 ? (u16)(U16(o + 0x5A) + 2) : (u16)rnd();
    U16(pl + 0x174) = rnd() % 2 ? rnd() & 3 : (u16)rnd();
    return o;
}

/* the angle to its point o+0xD8 (the player there or not) about a value; the
 * player's heading about its own */
static void allo_aim(gptr o)
{
    static const u16 ed[] = {0, 1, 0x5F, 0x60, 0x61, 0x7F, 0x80, 0x81, 0xFF, 0x100, 0x101, 0x1FF, 0x200, 0x201, 0x3FF, 0x400,
                             0x401, 0x4FF, 0x500, 0x501, 0x5FF, 0x600, 0x601, 0x7FF, 0x800};
    gptr pl = U32(G() + 0x940);
    u32 d = rnd() % 4 ? (rnd() % 2 ? ed[rnd() % 25] : (u16)(0x1000 - ed[rnd() % 25])) & 0xFFF : rnd() % 0x1000;
    static const s16 dist[] = {1000, 1999, 2000, 2001, 3000, 4800};
    s16 dd = dist[rnd() % 6];
    if (rnd() % 2) { S16(o + 0xD8) = (s16)(S16(o + 0x40) + (rnd() % 2 ? dd : -dd)); S16(o + 0xDC) = S16(o + 0x44); }
    else { S16(o + 0xDC) = (s16)(S16(o + 0x44) + (rnd() % 2 ? dd : -dd)); S16(o + 0xD8) = S16(o + 0x40); }
    u16 an = (u16)Angle_XZ(o + 0x40, o + 0xD8);
    U16(o + 0x4A) = (u16)((an - d) & 0xFFF);
    if (rnd() % 2) { U16(pl + 0x40) = U16(o + 0xD8); U16(pl + 0x44) = U16(o + 0xDC); }
    u32 e = rnd() % 2 ? ed[rnd() % 25] : rnd();
    U16(pl + 0x4A) = (u16)((U16(o + 0x4A) + (rnd() % 2 ? e : 0x1000 - e)) & 0xFFF);
}

/* a wall across its way at about 0x6A4 (Wall_Ahead), or the room's */
static void allo_wall(gptr o)
{
    int k = rnd() % 4;
    if (k == 0) { U32(G() + 0xA4C) = CL_ROOM; return; }
    if (k == 3 && U8(CL_ROOM)) {                        /* steps and ledges of the room (Step_Ahead) */
        U32(G() + 0xA4C) = CL_ROOM;
        for (u32 j = 0; j < U8(CL_ROOM); j++) {
            gptr w = CL_ROOM + U32(CL_ROOM + 8 + 4 * j);
            u32 l = U8(o + 0x5A) & 15;
            U16(w + 4) = (u16)(rnd() % 3 ? 1u << ((l + (rnd() % 2 ? 2 : 14)) & 15) | (rnd() % 2 ? 1u << l : 0) : (u16)rnd());
            U8(w) = (u8)((rnd() % 4 ? 1 : 0) | (rnd() % 3 ? 0 : 0x40));
            U8(w + 6) = rnd() % 2 ? 0 : (u8)rnd();
            for (u32 e = 0; e < U8(w + 1); e++) U8(w + 8 + 4 * e) = (u8)((rnd() % 4 ? 1 : 0) | (rnd() % 2 ? 0x80 : 0) | (rnd() % 2 ? 0x40 : 0) | (rnd() % 3 ? 0 : 4));
        }
        U32(o + 0x128) = 0x720F78;
        cl_place(o);
        if (rnd() % 3 == 0) U16(o + 0x42) = (u16)(-1000 * S16(o + 0x5A));
        return;
    }
    if (k == 1) return;
    gptr w = 0x6CFC00u, pg = w + 0x10;
    U8(w) = 1; U32(w + 8) = 0x10;
    double a = U16(o + 0x4A) * 3.14159265358979 / 2048, d = rnd() % 2 ? 1000 : 1600 + rnd() % 3 * 100;
    double cx = S16(o + 0x40) + d * sin(a), cz = S16(o + 0x44) + d * cos(a), ex = 800 * cos(a), ez = -800 * sin(a);
    int sw = rnd() % 2;
    U8(pg) = 1; U8(pg + 1) = 1; U8(pg + 3) = rnd() % 4 ? 0 : 0x40; U16(pg + 4) = rnd() % 4 ? 0xFFFF : (u16)rnd(); U8(pg + 8) = 0x81 | (rnd() & 0x44);
    S16(pg + 0xC + 4 * sw) = (s16)(cx + ex); S16(pg + 0xE + 4 * sw) = (s16)(cz + ez);
    S16(pg + 0x10 - 4 * sw) = (s16)(cx - ex); S16(pg + 0x12 - 4 * sw) = (s16)(cz - ez);
    U32(G() + 0xA4C) = w;
}

static void allo_rand8(void)
{
    static const u8 r[] = {0x07, 0x08, 0x09, 0x0F, 0x10, 0x11, 0x3F, 0x40, 0x41, 0x7F, 0x80, 0x81, 0, 1};
    if (rnd() % 4) U16(0x87DE10) = rand8_seed(r[rnd() % 14], rnd() % 4 == 0);
}

typedef struct { u32 addr; void *fn; const char *name; u8 steps, sides; } AlloFn;

/* frames about a move's own limits, by step */
typedef struct { u32 addr; u8 step, nf; u16 fr[10]; } AlloEdge;
static const AlloEdge g_allo_edge[] = {
    {0x43AF00, 1, 6, {4, 5, 0x18, 0x19, 0xD, 0xE}}, {0x43AF00, 3, 4, {5, 6, 0x36, 0x37}}, {0x43AF00, 4, 2, {0x28, 0x29}},
    {0x43A390, 1, 4, {0x10, 0x11, 0x28, 0x29}}, {0x43A7E0, 1, 10, {0x40, 0x41, 0x1A, 0x1B, 0x47, 0x48, 0x29, 0x2A, 0x42, 0x23}},
    {0x43AC70, 1, 3, {0x15, 0x16, 0x17}}, {0x43AD40, 1, 3, {0x2B, 0x2C, 0x2D}}, {0x43B3C0, 1, 4, {2, 3, 0x18, 0x19}},
    {0x43B4E0, 1, 4, {6, 7, 0x15, 0x16}}, {0x43C700, 1, 2, {0xE, 0xF}}, {0x43C700, 2, 6, {0x25, 0x26, 0x4E, 0x4F, 0x56, 0x30}},
    {0x43C890, 1, 2, {9, 0xA}}, {0x43C890, 2, 6, {0x10, 0x11, 0x39, 0x3A, 0x34, 0x1B}}, {0x43C450, 1, 4, {0x1E, 0x28, 0xB, 0xC}},
    {0x43C600, 1, 2, {0x46, 0x47}}, {0x439F70, 1, 2, {0xF, 0x10}}};

static void test_allo(void)
{
    g_ign_objpads = 1;
    stubs_install(1);
    allo_tables_stub(1);
    player_tables_stub(1);
    for (int i = 0; i < iters; i++) {
        game_state();
        gptr o = allo_setup(), b = o + 0x1C0, pl = U32(G() + 0x940);
        u32 a[3] = {o};
        /* the helpers */
        {
            u32 c[3] = {o + 0x40, o + 0xD8, (u32)rnd()};
            allo_aim(o);
            check(0x48F3E0, Angle_DiffTo, "Angle_DiffTo", 3, c, 0xFFFFFFFF);
            u32 d[2] = {(u32)rnd(), (u32)rnd()};
            if (rnd() % 2) d[1] = d[0] + (rnd() % 2 ? 0x800 : 0x7FF) + (rnd() % 2 ? 0 : 0x1000);
            check(0x48F410, Angle_Diff, "Angle_Diff", 2, d, 0xFFFFFFFF);
            for (int j = 0; j < 4; j++) {          /* about one step either way */
                u32 st = rnd() % 0x100, cur = (u16)rnd(), del = rnd() % 3 ? (u32)((s32)st + (rnd() % 2 ? 0 : (s32)(rnd() % 5) - 2)) : rnd();
                U16(o + 0xF4) = (u16)cur;
                u32 v[3] = {o + 0xF4, rnd() % 4 ? st : rnd(), cur + (rnd() % 2 ? del : (u32)-(s32)del)};
                check(0x48F4B0, Value_Approach, "Value_Approach", 3, v, 0xFFFFFFFF);
            }
            S16(o + 0x1E0) = (s16)rnd(); S16(o + 0x1E2) = (s16)rnd(); S16(o + 0x1E4) = (s16)rnd();
            u32 r[3] = {o + 0x1E8, o + 0x1E0, (u32)rnd()};
            check(0x48F470, Vec_RotY, "Vec_RotY", 3, r, 0);
            allo_wall(o);
            u32 w[3] = {o, rnd() % 2 ? 0x400 : (u32)-0x400, rnd() % 2 ? 0x40 : (u8)rnd()};
            check(0x454B50, Wall_Dist, "Wall_Dist", 3, w, 0xFFFFFFFF);
            U32(LOG_BASE) = 0;
            u32 sc[2] = {o, rnd() % 3 ? rnd() % 0x200 : rnd()};
            U8(o + 0x58) = rnd() % 2 ? 8 : (u8)rnd(); if (rnd() % 2) U16(G() + 0x1090) = 0x808;
            if (rnd() % 4 == 0) U32(G() + 0x11F4) = 9999999 - rnd() % 0x100;
            check(0x404A20, Score_Hit, "Score_Hit", 2, sc, 0);
            U32(0x87F940) = rnd() % 4 ? 1 + rnd() % 16 : rnd() % 2;
            u32 bf[2] = {rnd() % 4 ? o : O_OBJ + 0x260 * (rnd() % 16), rnd() % 2 ? 0 : rnd()};
            check(0x415A40, Boss_Flash, "Boss_Flash", 2, bf, 0);
            check(0x42B710, Enemy_BiteOn, "Enemy_BiteOn", 1, a, 0);
            Bits_Set(b, 0, rnd() % 2);
            check(0x433960, Enemy_BiteOff, "Enemy_BiteOff", 1, a, 0);
            U16(o + 0x10C) = rnd() % 2 ? 0x20 : (u16)rnd(); U8(o + 0x18F) = rnd() % 8; U8(0x87E8B8 + U8(o + 0x18F)) = rnd() % 2 ? 0 : (u8)rnd();
            check(0x47AD60, Enemy_Remove, "Enemy_Remove", 1, a, 0);
            check(0x478840, Obj_ClearHits, "Obj_ClearHits", 1, a, 0);
            check(0x48E780, Player_Release, "Player_Release", 1, a, 0);
            U8(G() + 0x108E) = rnd() % 3; U8(G() + 0x108F) = rnd() % 2 ? 3 : rnd() % 4;
            u32 ph[4] = {o, rnd() % 2 ? rnd() % 0x200 : rnd(), rnd() % 2 ? 1 + rnd() % 3 : rnd(), rnd() % 2 ? 0x1E : rnd()};
            check(0x48E7E0, Player_Hurt, "Player_Hurt", 4, ph, 0);
            S16(pl + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x100 - 0x10 : rnd());
            u32 hd[3] = {o, rnd() % 2 ? 0x82 : rnd() % 0x400, 0x48E100};
            if (rnd() % 2) S16(pl + 0x108) = (s16)(Damage_Scale(hd[1]) + (s32)(rnd() % 3) - 1);   /* to 0 and about */
            check(0x48E8A0, Player_Hold, "Player_Hold", 3, hd, 0);
        }
        /* deciding */
        static const AlloFn th[] = {
            {0x439D50, Allo_ThinkIdle, "Allo_ThinkIdle"}, {0x43A040, Allo_ThinkWalk, "Allo_ThinkWalk"}, {0x43A240, Allo_ThinkRun, "Allo_ThinkRun"},
            {0x43A4C0, Allo_Steer, "Allo_Steer"}, {0x43ABA0, Allo_ThinkBack, "Allo_ThinkBack"}, {0x43B390, Allo_ThinkLunge, "Allo_ThinkLunge"}};
        for (int k = 0; k < 96; k++) {
            game_state();
            o = allo_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
            allo_aim(o); allo_wall(o); allo_rand8();
            if (rnd() % 2) {                                /* all clear for its choices, but one thing */
                for (u32 j = 0x1F8; j < 0x210; j += 2) U16(o + j) = 0;
                U32(o + 0x1D8) = 0; U32(o + 0x1C0) &= rnd() % 2 ? 8 : 0; U8(o + 0x13A) = 0;
                U16(pl + 0x5A) = rnd() % 3 ? U16(o + 0x5A) : (u16)(U16(o + 0x5A) + 2);
                Bits_Set(0x87E974, 0, 0); Flag_Set(2, 0x1A, 0);
                U16(pl + 0x174) = rnd() % 2 ? 2 : 0;
                switch (rnd() % 8) {
                case 0: U32(o + 0x1D8) = 1; break;
                case 1: U16(o + 0x1FA + 2 * (rnd() % 10)) = rnd() % 2 ? 1 : (u16)(2 + rnd() % 0x40); break;
                case 2: Bits_Set(0x87E974, 0, 1); break;
                case 3: Flag_Set(2, 0x1A, 1); break;
                case 4: U8(o + 0x13A) = 1; break;
                }
                U8(o + 0x56) = (u8[]){2, 1, 2, 2, 1, 1}[k % 6];
            }
            U8(o + 0x56) = rnd() % 4 ? (u8)(rnd() % 3 ? U8(o + 0x56) : rnd() % 3) : (u8)rnd();
            if (rnd() % 2) U16(o + 0xE8) = U16(o + 0xEA) = U16(o + 0xEC) = 0;
            U16(o + 0x20E) = rnd() % 2 ? 0x3B : (u16)rnd();
            if (rnd() % 2) U16(o + 0x92) = (u16)(9 + rnd() % 9);
            const AlloFn *f = &th[k % 6];
            U32(LOG_BASE) = 0;
            sprintf(g_what, "allo %X %s step %u dist %X bits %X", o, f->name, U8(o + 0x56), U32(o + 0x13C), U32(b));
            check(f->addr, f->fn, f->name, 1, a, f->addr == 0x43A4C0 ? 0xFFFFFFFF : 0);
        }
        /* deciding: all its way clear, each limit exact */
        for (int k = 0; k < 60; k++) {
            game_state();
            o = allo_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
            allo_aim(o); allo_wall(o); allo_rand8();
            for (u32 j = 0x1F8; j < 0x210; j += 2) U16(o + j) = 0;
            if (rnd() % 3 == 0) U16(o + 0x1FA + 2 * (rnd() % 10)) = (u16)(1 + rnd() % 2);
            U16(o + 0x204) = rnd() % 2; U8(o + 0x206) = rnd() % 3;
            U32(o + 0x1D8) = rnd() % 2; U32(b) = rnd() & 0x148; U8(o + 0x13A) = rnd() % 5 == 0;
            U16(pl + 0x5A) = (u16)(U16(o + 0x5A) + (rnd() % 4 ? 0 : 2));
            Bits_Set(0x87E974, 0, rnd() % 5 == 0); Flag_Set(2, 0x1A, rnd() % 4 == 0);
            U16(pl + 0x174) = rnd() % 2 ? 2 : 0;
            U32(o + 0x13C) = g_allo_dist[rnd() % 22];
            U16(o + 0xE8) = rnd() % 2 ? 0 : rnd() % 2 ? (u16)(1 + rnd() % 0xFF) : (u16)rnd();
            U16(o + 0xEA) = rnd() % 4 ? 0 : (u16)rnd(); U16(o + 0xEC) = rnd() % 4 ? 0 : (u16)rnd();
            U16(o + 0x20E) = rnd() % 2 ? 0x3B : 0;
            U16(o + 0x92) = (u16[]){9, 0xA, 0x10, 0x11, 0x12}[rnd() % 5];
            if (rnd() % 2) {                                    /* nearly facing the player, the player about its way */
                u16 an = (u16)Angle_XZ(o + 0x40, o + 0xD8);
                U16(o + 0x4A) = (u16)((an - (u16[]){0, 1, 0x5F, 0x60, 0x61, 0xFFF, 0xFA1, 0xFA0, 0xF9F}[rnd() % 9]) & 0xFFF);
                U16(pl + 0x4A) = (u16)((U16(o + 0x4A) + (u16[]){0, 1, 0x1FF, 0x200, 0xE01, 0xE00}[rnd() % 6]) & 0xFFF);
            }
            if (rnd() % 2) U16(o + 0x208) = rnd() % 2 ? 1 : 0x3C;
            if (k % 6 == 0 && rnd() % 2) {                      /* the grab: the player 0x60 off its way (and one in) */
                U32(o + 0x1D8) = 1; U16(pl + 0x174) = 2; Flag_Set(2, 0x1A, 0);
                U32(o + 0x13C) = (u32[]){0x9C5, 0xBB8, 0x1387}[rnd() % 3];
                u16 an = (u16)Angle_XZ(o + 0x40, o + 0xD8);
                U16(o + 0x4A) = (u16)((an - (u16[]){0x5F, 0x60, 0x61, 0xFA1, 0xFA0, 0xF9F}[rnd() % 6]) & 0xFFF);
                U16(pl + 0x4A) = (u16)((U16(o + 0x4A) + (rnd() % 2 ? 0 : 0x1FF)) & 0xFFF);
            }
            const AlloFn *f = &th[k % 6];
            U8(o + 0x56) = (u8[]){2, 1, 2, 1, 1, 1}[k % 6] + (k % 6 == 1 || k % 6 == 3 ? rnd() % 2 : 0);
            U32(LOG_BASE) = 0;
            sprintf(g_what, "allo %X %s edge step %u dist %X bits %X", o, f->name, U8(o + 0x56), U32(o + 0x13C), U32(b));
            check(f->addr, f->fn, f->name, 1, a, f->addr == 0x43A4C0 ? 0xFFFFFFFF : 0);
        }
        /* the state handlers */
        game_state();
        o = allo_setup(); a[0] = o; b = o + 0x1C0;
        U8(o + 0x54) = rnd() % 5; U8(o + 0x55) = rnd() % 16; U8(o + 0x56) = rnd() % 6;
        check(0x439CA0, Allo_Normal, "Allo_Normal", 1, a, 0);
        U8(o + 0x55) = rnd() % 4;
        check(0x439CE0, Allo_Hit, "Allo_Hit", 1, a, 0);
        check(0x439D00, Allo_Die, "Allo_Die", 1, a, 0);
        U8(o + 0x55) = rnd() % 16;
        check(0x439D20, Allo_Grab, "Allo_Grab", 1, a, 0);
        U8(o + 0x54) = rnd() % 5; U8(o + 0x55) = rnd() % 16;
        if (rnd() % 2) { U32(o + 0x1C4) = 1; S16(o + 0x40) = (s16)(rnd() % 2 ? (u32)(-0x1B58 - (s32)(rnd() % 2)) : rnd()); S16(o + 0x44) = (s16)(0x9C4 + rnd() % 2);
                         U8(G() + 0x108F) = rnd() % 2; U8(G() + 0x7C4) = rnd() % 2 ? 0xC : (u8)rnd(); }
        allo_aim(o);
        if (rnd() % 2) {                                    /* the head where it stands: its angle exact */
            gptr hp = Obj_Part(U32(o + 0x64), U8(o + 0x15F));
            U16(hp + 0x40) = U16(o + 0x40); U16(hp + 0x44) = U16(o + 0x44);
        }
        sprintf(g_what, "allo %X handler kind %X bits %X", o, U32(o + 0x1C4), U32(b));
        check(0x4397C0, Allo_Handler, "Allo_Handler", 1, a, 0);
        for (int k = 0; k < 8; k++) {                       /* the head about its looking limit */
            game_state();
            o = allo_setup(); a[0] = o; b = o + 0x1C0;
            U8(o + 0x54) = rnd() % 5; U8(o + 0x55) = rnd() % 16; Bits_Set(b, 1, rnd() % 4 != 0);
            allo_aim(o);
            gptr hp = Obj_Part(U32(o + 0x64), U8(o + 0x15F));
            U16(hp + 0x40) = U16(o + 0x40); U16(hp + 0x44) = U16(o + 0x44);
            u16 an = (u16)Angle_XZ(o + 0x40, o + 0xD8);
            U16(o + 0x4A) = (u16)((an - (u16[]){0x4FF, 0x500, 0x501, 0xB01, 0xB00, 0xAFF}[rnd() % 6]) & 0xFFF);
            U16(o + 0xF0) = U16(o + 0xF2) = U16(o + 0xF4) = 0;
            sprintf(g_what, "allo %X handler head bits %X", o, U32(b));
            check(0x4397C0, Allo_Handler, "Allo_Handler", 1, a, 0);
        }
        U8(o + 0x11A) = (u8)rnd();
        anim_setup_at(AN_OBJ); memcpy(MEM(o + 0x60), MEM(AN_OBJ + 0x60), 0x50);
        for (int k = 0; k < 2; k++) {                       /* its attack lands from the handler */
            U8(o + 0x54) = 1; U8(o + 0x55) = rnd() % 16; U8(o + 0x56) = rnd() % 3; U8(o + 0x119) = rnd() % 4 ? 0 : 0x80;
            U32(o + 0x50) = (U32(o + 0x50) & ~0x40000000u) | 0x30;
            U32(o + 0xA0) = 1; U16(o + 0x102) &= ~2; U32(pl + 0x134) = 0; U8(pl + 0x17B) = 0;
            U32(LOG_BASE) = 0;
            memcpy(MEM(o + 0x60), MEM(AN_OBJ + 0x60), 0x50);   /* (a disabled one has no parts) */
            U8(o + 0x15F) = (u8)(rnd() % U32(o + 0x68));
            U32(o + 0x120) = U32(o + 0x124) = U32(o + 0x64);
            check(0x4397C0, Allo_Handler, "Allo_Handler", 1, a, 0);
        }
        model_data(0x638000);
        U32(0x638000 + 0x14) = 18;                          /* (its feet: parts 12, 16) */
        for (u32 j = 0; j < 18; j++) { gptr r = 0x638000 + 0x1C + j * 0x14; S8(r + 6) = (s8)(j ? (s32)(rnd() % j) : -1); }
        anim_setup_at(AN_OBJ); memcpy(MEM(o + 0x60), MEM(AN_OBJ + 0x60), 0x50);
        U32(0x63AD4C) = AN_DATA;                            /* its first animation */
        U8(G() + 0x1091) = rnd() % 2 ? 8 : (u8)rnd(); U8(G() + 0x1090) = rnd() % 2 ? 8 : (u8)rnd();
        U8(G() + 0x927) = rnd() % 3 == 0; U8(G() + 0x108E) = rnd() % 3; U16(o + 0x10C) = (u16)rnd(); U32(0x74AFA0) = rnd();
        check(0x439A80, Allo_Init, "Allo_Init", 1, a, 0);
        /* moving, each step */
        static const AlloFn mv[] = {
            {0x439F70, Allo_Idle, "Allo_Idle", 5, 1}, {0x43A1B0, Allo_Walk, "Allo_Walk", 3, 1}, {0x43A390, Allo_Run, "Allo_Run", 4, 1},
            {0x43A7E0, Allo_Leap, "Allo_Leap", 3, 2}, {0x43AA10, Allo_Turn, "Allo_Turn", 3, 4}, {0x43AC70, Allo_Back, "Allo_Back", 3, 2},
            {0x43AD40, Allo_Roar, "Allo_Roar", 3, 1}, {0x43AE00, Allo_Enter, "Allo_Enter", 1, 1}, {0x43AE60, Allo_Anger, "Allo_Anger", 3, 1},
            {0x43AF00, Allo_Bite, "Allo_Bite", 7, 1}, {0x43B3C0, Allo_Lunge, "Allo_Lunge", 3, 1}, {0x43B4E0, Allo_SnapUp, "Allo_SnapUp", 3, 1},
            {0x43B5A0, Allo_Snap, "Allo_Snap", 3, 1}, {0x43B640, Allo_Toss, "Allo_Toss", 5, 1}, {0x43B910, Allo_Sidestep, "Allo_Sidestep", 3, 2},
            {0x43B9B0, Allo_Dash, "Allo_Dash", 3, 1},
            {0x43C3C0, Allo_Flinch, "Allo_Flinch", 3, 1}, {0x43C450, Allo_Stagger, "Allo_Stagger", 3, 2}, {0x43C600, Allo_Fall, "Allo_Fall", 3, 1},
            {0x43C700, Allo_DieA, "Allo_DieA", 5, 1}, {0x43C890, Allo_DieB, "Allo_DieB", 5, 2}};
        const unsigned nm = sizeof mv / sizeof mv[0];
        game_state(); world_setup(); enemy_state();
        o = O_OBJ + 0x260 * (1 + rnd() % 6); a[0] = o; b = o + 0x1C0;
        anim_setup_at(O_OBJ);
        do anim_setup_at(o); while (U32(o + 0x68) < 16);
        memcpy(MEM(O_OBJ + 0x60), MEM(o + 0x60), 0x50);
        for (u32 k = 16; k < 20; k++) for (u32 j = 0; j < 0x80; j += 4) U32(AN_PART + k * 0x80 + j) = rnd() % 0x1000;
        for (u32 k = 0; k < 20; k++) {                      /* 20 parts (Allo_Burn) */
            U32(AN_PART + k * 0x80 + 0x54) = k < 19 ? AN_PART + (k + 1) * 0x80 : 0;
            U32(AN_PART + k * 0x80 + 0x58) = k ? AN_PART + (k - 1) * 0x80 : o;
        }
        U32(G() + 0x940) = pl = O_OBJ;
        U32(pl + 0x128) = rnd() % 2 ? 0x720F78 : 0; U32(o + 0x128) = 0x720F78;   /* (wall collision boxes) */
        for (int k = 0x20; k < 0x40; k++) U32(AN_TAB + 4 * k) = AN_DATA + 0x1000 * ((k ^ k >> 2) & 1);
        u8 sv[0x260]; memcpy(sv, MEM(o), 0x260);
        for (unsigned k = 0; k < 10 * nm; k++) {
            const AlloFn *f = &mv[k % nm];
            memcpy(MEM(o), sv, 0x260);
            for (u32 j = 0x1C0; j < 0x214; j += 4) U32(o + j) = rnd();
            U32(o + 0x1C0) = rnd() & 0x3FF; U32(o + 0x1C4) = rnd() % 4 ? rnd() % 3 : rnd();
            for (u32 j = 0x1F8; j < 0x210; j += 2) U16(o + j) = rnd() % 3 ? rnd() % 3 : (u16)rnd();
            U32(o + 0x1D4) = rnd() % 3 ? (u32)(rnd() % 3) - 1 : rnd(); U32(o + 0x1D0) = rnd() % 2 ? 1000 - rnd() % 0x100 : rnd();
            U32(o + 0x1DC) = rnd() % 0x18;
            U8(o + 0x56) = (u8)(rnd() % 8 ? rnd() % f->steps : rnd());
            U8(o + 0x57) = (u8)(rnd() % f->sides);
            U32(o + 0x13C) = allo_dist();
            if (f->addr == 0x43A390 && rnd() % 2) {            /* Run: a bite start nearer than 0x1B58 */
                U8(o + 0x56) = 0; U16(o + 0x1FC) = rnd() % 4 == 0; U32(o + 0x13C) = 0x1B57 + rnd() % 3;
            }
            static const u16 fr[] = {0, 2, 3, 4, 5, 6, 7, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x15, 0x16, 0x18, 0x19, 0x1A, 0x1B, 0x1E,
                                     0x23, 0x26, 0x28, 0x29, 0x2B, 0x2C, 0x2E, 0x30, 0x33, 0x34, 0x37, 0x39, 0x3E, 0x3F, 0x40, 0x42, 0x46,
                                     0x47, 0x4E, 0x4F, 0x54, 0x56};
            U16(o + 0x92) = rnd() % 4 ? (u16)(fr[rnd() % 45] + rnd() % 3 - 1) : rnd() % 2 ? rnd() % 0x60 : (u16)rnd();
            U16(pl + 0x92) = rnd() % 2 ? (rnd() % 2 ? 0x2B : 0x33) : (u16)rnd();
            U16(pl + 0x174) = rnd() % 2 ? rnd() & 3 : (u16)rnd();
            U16(pl + 0xE8) = rnd() % 2 ? 0 : (u16)rnd(); U16(pl + 0xEA) = rnd() % 2 ? 0 : (u16)rnd(); U16(pl + 0xEC) = rnd() % 2 ? 0 : (u16)rnd();
            S16(pl + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x100 - 0x10 : rnd());
            U16(o + 0xD4) = (u16)rnd(); U16(o + 0x172) = rnd() % 5; U16(o + 0x176) = rnd() % 2 ? rnd() % 0x400 : (u16)rnd();
            U8(o + 0x170) = (u8)rnd(); U8(o + 0x11C) = (u8)rnd(); U8(o + 0x18C) = rnd() % 2 ? 4 + 16 * (rnd() % 16) : (u8)rnd();
            U8(G() + 0x108E) = rnd() % 3;
            Bits_Set(0x87E974, 2, rnd() % 2);
            allo_aim(o); allo_rand8();
            if (U16(o + 0x90))
                for (u32 j = 0x78; j <= 0x84; j += 4) U32(o + j) = U32(o + 0x8C) + (rnd() % U16(o + 0x90)) * U8(o + 0xAC);
            if (rnd() % 3 == 0) U16(o + 0x176) = (u16)(1001 - U32(o + 0x1D0) - rnd() % 2);
            U16(0x87DC12) = rnd() % 2 ? 0 : (u16)(1u << (rnd() % 16)); U32(o + 0x1DC) = rnd() % 2 ? 0x13 - rnd() % 3 + (U16(0x87DC12) ? 0 : 1) : rnd() % 0x18;
            if (rnd() % 3 == 0) S16(pl + 0x108) = (s16)(Damage_Scale((u32[]){0x28, 0xFA}[rnd() % 2]) + rnd() % 3 - 1);
            anim_maybe_end(o);
            if (rnd() % 2) {                                    /* a frame at its own limits */
                const AlloEdge *e[4]; int ne = 0;
                for (unsigned j = 0; j < sizeof g_allo_edge / sizeof g_allo_edge[0]; j++) if (g_allo_edge[j].addr == f->addr && ne < 4) e[ne++] = &g_allo_edge[j];
                if (ne) {
                    const AlloEdge *x = e[rnd() % ne];
                    U8(o + 0x56) = x->step;
                    if (rnd() % 3) { U16(o + 0x92) = x->fr[rnd() % x->nf]; if (rnd() % 4) U16(o + 0x102) |= 0x20; }
                    if (rnd() % 2) Bits_Set(b, 3, 0);
                    if (rnd() % 2) S16(pl + 0x108) = (s16)(Damage_Scale(0x28) + rnd() % 3 - 1);
                    if (rnd() % 2) { U16(pl + 0xE8) = U16(pl + 0xEA) = U16(pl + 0xEC) = 0; }
                }
            }
            if (rnd() % 2) U16(o + 0x102) |= 0x20;              /* paused: the frame as set */
            if (rnd() % 2) anim_maybe_end(pl);
            if (rnd() % 2) U16(pl + 0x102) |= 0x20;
            U32(LOG_BASE) = 0;
            sprintf(g_what, "allo %X %s step %u side %u frame %X dist %X bits %X", o, f->name, U8(o + 0x56), U8(o + 0x57), U16(o + 0x92), U32(o + 0x13C), U32(b));
            check(f->addr, f->fn, f->name, 1, a, 0);
        }
        /* the attacks land */
        static const AlloFn gh[] = {
            {0x43BA30, Allo_BiteHit, "Allo_BiteHit"}, {0x43BBC0, Allo_LungeHit, "Allo_LungeHit"}, {0x43BC40, Allo_SnapUpHit, "Allo_SnapUpHit"},
            {0x43BCC0, Allo_SnapHit, "Allo_SnapHit"}, {0x43BD30, Allo_SideHit, "Allo_SideHit"}, {0x43BDB0, Allo_LeapHit, "Allo_LeapHit"}};
        for (int k = 0; k < 48; k++) {
            memcpy(MEM(o), sv, 0x260);
            U8(o + 0x56) = rnd() % 4 ? 1 : (u8)rnd(); U8(o + 0x210) = rnd() % 2;
            U8(G() + 0x108E) = rnd() % 3; U8(G() + 0x108F) = rnd() % 4;
            Flag_Set(2, 0x1A, rnd() % 2);
            S16(pl + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x100 - 0x10 : rnd());
            allo_aim(o);
            if (rnd() % 2) U16(pl + 0x4A) = (u16)((U16(o + 0x4A) + (rnd() % 2 ? 0x400 : 0xC00) + rnd() % 3 - 1) & 0xFFF);
            U32(LOG_BASE) = 0;
            sprintf(g_what, "allo %X %s step %u", o, gh[k % 6].name, U8(o + 0x56));
            check(gh[k % 6].addr, gh[k % 6].fn, gh[k % 6].name, 1, a, 0);
        }
        for (int k = 0; k < 6; k++) {                       /* the player thrown */
            anim_maybe_end(pl);
            U16(pl + 0x92) = rnd() % 2 ? 0x2B : (u16)rnd(); S16(pl + 0x108) = (s16)(rnd() % 2 ? rnd() % 3 - 1 + (s32)Damage_Scale(0x82) : rnd() % 0x100 - 0x10);
            if (rnd() % 2) U16(pl + 0x102) |= 0x20;
            u32 p1[1] = {pl};
            U32(LOG_BASE) = 0;
            check(0x43B2E0, Allo_Thrown, "Allo_Thrown", 1, p1, 0);
        }
        /* the damage taken */
        for (int k = 0; k < 40; k++) {
            memcpy(MEM(o), sv, 0x260);
            U32(o + 0x1C0) = rnd() & 0x3FF; U32(o + 0x1C4) = rnd() % 3;
            U8(o + 0x1C8) = 1 + rnd() % 3; U8(o + 0x1C9) = rnd() % 16; U8(o + 0x1CA) = rnd() % 6; U8(o + 0x1CB) = rnd() % 2;
            U8(o + 0x54) = 2; U8(o + 0x55) = rnd() % 16; U8(o + 0x56) = rnd() % 3 ? 0 : rnd() % 3; U8(o + 0x57) = rnd() % 2;
            U8(o + 0x184) = (u8)rnd(); U8(o + 0x17C) = rnd() % 2 ? (u8)(rnd() % 3) : (u8)rnd();
            U8(o + 0x17D) = (u8[]){0, 1, 2, 3, 4, 0xF, 0x10, 0x11, 0x12, 0x1C}[rnd() % 10];
            U16(o + 0x172) = rnd() % 5; U16(o + 0x176) = rnd() % 2 ? rnd() % 0x400 : (u16)rnd();
            S16(o + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x400 : rnd());
            if (rnd() % 3 == 0) S16(o + 0x108) = (s16)(U16(o + 0x176) * (rnd() % 2 ? 1 : 3) / (rnd() % 2 ? 1 : 5) + rnd() % 3 - 1);
            U16(o + 0x178) = (u16)((U16(o + 0x4A) + 0x800 + rnd() % 3 - 1 + (rnd() % 2 ? 0 : rnd())) & 0xFFF);
            U8(o + 0x11D) = (u8)(rnd() % 2 ? 0xD4 - rnd() % 8 : rnd());
            U16(o + 0x1F8) = (u16)(rnd() % 2 ? rnd() % 0x100 : rnd());
            U16(o + 0x1E6) = rnd() % 2 ? 0 : (u16)rnd();
            U16(pl + 0x5A) = rnd() % 2 ? U16(o + 0x5A) : (u16)rnd();
            Bits_Set(0x87E974, 1, rnd() % 2);
            U8(o + 0x170) = (u8)rnd();
            U32(0x87F940) = 1 + rnd() % 8;
            for (u32 j = 0; j < 20; j++) if (rnd() % 2) U8(O_K2 + j * 0xAC + 0x50) &= ~1;   /* free effect slots */
            if (rnd() % 2) {                                    /* angry: the anger used up exactly, or not */
                Bits_Set(b, 3, 1); Bits_Set(b, 2, rnd() % 4 == 0); U32(o + 0x1C4) = rnd() % 2 ? 0 : 2;
                U8(o + 0x184) &= ~8; U8(o + 0x17C) = rnd() % 3; U8(o + 0x17D) = rnd() % 2 ? 0 : 0x1C;
                U16(o + 0x176) = (u16)(0x40 + rnd() % 0x200);
                u16 d = U16(o + 0x176); if ((s8)U8(o + 0x17C) <= 1) d += d >> 1; d >>= 2;
                U8(o + 0x1C8) = rnd() % 2 ? 1 : 2; U8(o + 0x1CA) = rnd() % 3;
                S16(o + 0x1F8) = (s16)((rnd() % 2 ? d : d / 5) + rnd() % 3 - 1);
                if (rnd() % 2) S16(o + 0x108) = (s16)(d / 5 + rnd() % 3 - 1);
                if (rnd() % 2) U16(o + 0x178) = (u16)((U16(o + 0x4A) + 0x800) & 0xFFF);
            }
            allo_rand8();
            U32(LOG_BASE) = 0;
            sprintf(g_what, "allo %X damage hp %d dmg %u how %X kind %u bits %X", o, S16(o + 0x108), U16(o + 0x176), U8(o + 0x17D), U16(o + 0x172), U32(b));
            check(0x43BE00, Allo_Damage, "Allo_Damage", 1, a, 0);
        }
        /* its own helpers */
        for (int k = 0; k < 4; k++) {
            memcpy(MEM(o), sv, 0x260);
            U32(o + 0x1C0) = rnd() & 0x3FF; U8(o + 0x210) = (u8)(rnd() % 2 ? 0 : rnd());
            U32(o + 0x1D8) = rnd() % 3; U8(pl + 0x54) = (u8[]){2, 5, 1, 3}[rnd() % 4]; U16(pl + 0x174) = (u16)rnd();
            U16(o + 0x202) = rnd() % 2 ? (u16)(0x1D + rnd() % 3) : rnd() % 2 ? rnd() % 3 : (u16)rnd(); U16(o + 0x204) = rnd() % 3;
            U32(o + 0x13C) = allo_dist(); allo_aim(o); allo_wall(o); allo_rand8();
            U16(o + 0x92) = (u16)(rnd() % 0x20);
            check(0x43CAF0, Allo_PlAnim, "Allo_PlAnim", 1, a, 0xFFFFFFFF);
            check(0x43CB10, Allo_Timer, "Allo_Timer", 1, a, 0xFFFFFFFF);
            check(0x43CB90, Allo_Tint, "Allo_Tint", 1, a, 0);
            check(0x43CCB0, Allo_Pick, "Allo_Pick", 1, a, 0);
            u32 fd[3] = {o, rnd() % 0x20, allo_dist()};
            allo_rand8();
            check(0x43CD00, Allo_FrameFar, "Allo_FrameFar", 3, fd, 0xFFFFFFFF);
            check(0x43CD80, Allo_FrameNear, "Allo_FrameNear", 3, fd, 0xFFFFFFFF);
            u32 ch[2] = {o, rnd() % 2 ? 0x80 : (u32)rnd()};
            allo_rand8();
            check(0x43CDC0, Allo_Chance, "Allo_Chance", 2, ch, 0xFFFFFFFF);
            for (u32 j = 0; j < 20; j++) for (u32 c = 0x8C; c < 0x8F; c++) U8(AN_PART + j * 0x80 + c) = (u8)(rnd() % 2 ? 0x30 + rnd() % 0x50 : rnd());
            check(0x43CDF0, Allo_Burn, "Allo_Burn", 1, a, 0);
        }
    }
    player_tables_stub(0);
    allo_tables_stub(0);
    stubs_install(0);
    g_ign_objpads = 0;
}

/* ---- the pteranodon ---- */
static const u32 g_pt_tabs[][2] = {{0x720708, 5}, {0x72074C, 8}, {0x72076C, 8}, {0x72078C, 4}, {0x72079C, 4}, {0x7207AC, 8}};
static u32 g_pt_saved[6][8];
static void ptera_tables_stub(int on)
{
    for (int k = 1; k <= 8; k++) g_stubs[k].spec = on ? "d" : "";
    for (int t = 0; t < 6; t++)
        for (u32 k = 0; k < g_pt_tabs[t][1]; k++) {
            gptr a = g_pt_tabs[t][0] + 4 * k;
            if (on) { g_pt_saved[t][k] = U32(a); U32(a) = (u32)(uintptr_t)g_recs[1 + k]; }
            else U32(a) = g_pt_saved[t][k];
        }
}

/* a pteranodon among the test objects; the player its target */
static gptr ptera_setup(void)
{
    gptr o = raptor_setup(), pl = U32(G() + 0x940);
    U32(o + 0x184) &= ~0x40u;
    U32(o + 0x1C0) = rnd() % 4 ? rnd() & 7 : rnd();
    U8(o + 0x1CC) = rnd() % 4 ? 1 + rnd() % 3 : rnd() % 5; U8(o + 0x1CD) = rnd() % 3 ? rnd() % 8 : (u8)rnd();
    U8(o + 0x1CE) = rnd() % 5; U8(o + 0x1CF) = rnd() % 3;
    U32(o + 0x1D0) = rnd() % 3 ? rnd() % 3 : rnd() % 2 ? rnd() % 0x800 : rnd();
    U32(o + 0x1D4) = rnd() % 3 ? rnd() % 3 : rnd();
    for (u32 k = 0x1D8; k < 0x210; k += 2) U16(o + k) = rnd() % 3 ? rnd() % 3 : (u16)rnd();
    U16(o + 0x1FE) = (u16)(rnd() % 2 ? (u32)(S16(o + 0x42) + (s32)(rnd() % 0x800) - 0x400) : rnd());
    U8(o + 0x206) = rnd() % 3;
    U8(o + 0x57) = rnd() % 4 ? rnd() % 3 : (u8)rnd();
    U16(pl + 0x1FA) = (u16)(rnd() % 3 ? 0x7F + rnd() % 3 : rnd() % 2 ? rnd() % 0x100 : rnd());
    U16(pl + 0x174) = rnd() % 2 ? rnd() & 3 : (u16)rnd();
    U16(pl + 0xE8) = rnd() % 2 ? 0 : (u16)rnd(); U16(pl + 0xEA) = rnd() % 2 ? 0 : (u16)rnd(); U16(pl + 0xEC) = rnd() % 2 ? 0 : (u16)rnd();
    U16(o + 0xE8) = rnd() % 2 ? 0 : (u16)rnd(); U16(o + 0xEA) = rnd() % 2 ? 0 : (u16)rnd(); U16(o + 0xEC) = rnd() % 2 ? 0 : (u16)rnd();
    Bits_Set(0x87E974, 0, rnd() % 2); Bits_Set(0x87E974, 1, rnd() % 2);
    return o;
}

/* Rand8's first value about the player's 0x1FA (and the other edges) */
static void ptera_rand8(gptr pl)
{
    u8 c = (u8)U16(pl + 0x1FA);
    static const u8 r[] = {4, 5, 6, 0x45, 0x46, 0x47, 0, 1, 0xFF};
    if (rnd() % 4) U16(0x87DE10) = rand8_seed(rnd() % 2 ? (u8)(c + rnd() % 3 - 1) : r[rnd() % 9], rnd() % 4 == 0);
}

/* the player d away from o, a degrees off its heading (or o's heading about it) */
static void ptera_place(gptr o, gptr pl, s32 d, u32 off)
{
    static const s16 dir[][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
    int k = rnd() % 4;
    S16(pl + 0x40) = (s16)(S16(o + 0x40) + dir[k][0] * d); S16(pl + 0x44) = (s16)(S16(o + 0x44) + dir[k][1] * d);
    U16(o + 0x4A) = (u16)((Angle_XZ(o + 0x40, pl + 0x40) - off) & 0xFFF);
}

/* the swoop's first part as Ptera_ThinkSwoop does it: the angle to the player then (o kept) */
static s16 ptera_swoop_angle(gptr o, gptr pl, u16 *head)
{
    u8 sv[0x50]; memcpy(sv, MEM(o + 0x40), 0x50);
    u32 d = (u32)SquareRoot0((s32)Dist2_XZ(o + 0x40, pl + 0x40));
    Obj_TurnToPoint(o, 0x50, pl + 0x40);
    gptr v = Tmp_Push(8);
    U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = (u16)(U8(pl + 0x174) & 2 ? d / 30 : d / 80);
    Vec_RotY(v, v, (u16)(Angle_XZ(o + 0x40, pl + 0x40) + U16(o + 0x1E2)));
    U16(o + 0x44) += U16(v + 4); U16(o + 0x40) += U16(v); U16(o + 0x42) += U16(v + 2);
    Tmp_Pop(8);
    s16 a = (s16)Angle_DiffTo(o + 0x40, pl + 0x40, U16(o + 0x4A));
    *head = U16(o + 0x4A);
    memcpy(MEM(o + 0x40), sv, 0x50);
    return a;
}

/* the room's water areas (G+0x948, 0x34 each), o's point in or about one */
static void ptera_water(gptr pos)
{
    gptr r = U32(G() + 0x948);
    u32 n = rnd() % 5;
    U32(0x87DE14) = n;
    for (u32 k = 0; k < n; k++, r += 0x34) {
        for (u32 j = 0; j < 0x34; j += 4) U32(r + j) = rnd();
        U8(r + 1) = rnd() % 3 ? 3 : (u8)rnd();
        U8(r + 0x1A) = rnd() % 3 ? 1 + rnd() % 4 : (u8)rnd();
        U8(r + 0x10) = (u8)(rnd() % 2 ? 1 + rnd() % 20 : rnd()); U8(r + 0x11) = (u8)(rnd() % 2 ? 1 + rnd() % 20 : rnd());
        U16(r + 0x14) = rnd() % 2 ? 0 : rnd() % 2 ? (u16)(0x400 * (rnd() % 4)) : (u16)rnd();
        U8(r + 0x12) = (u8)(rnd() % 2 ? rnd() % 0x20 - 0x10 : rnd());
        s32 w = (s32)U8(r + 0x1A) * (s8)U8(r + 0x10) * 500, h = (s32)U8(r + 0x1A) * (s8)U8(r + 0x11) * 500;
        if (rnd() % 2) {                                /* its corner at pos, or an edge through it */
            s32 fx = rnd() % 3 == 0 ? 0 : rnd() % 2 ? w : w / 2, fz = rnd() % 3 == 0 ? 0 : rnd() % 2 ? h : h / 2;
            S16(r + 0xC) = (s16)(S16(pos) - fx + (s32)(rnd() % 3) - 1); S16(r + 0xE) = (s16)(S16(pos + 4) - fz + (s32)(rnd() % 3) - 1);
        } else { S16(r + 0xC) = (s16)(S16(pos) + (s32)(rnd() % 20000) - 10000); S16(r + 0xE) = (s16)(S16(pos + 4) + (s32)(rnd() % 20000) - 10000); }
    }
}

typedef struct { u32 addr; void *fn; const char *name; u8 steps; } PteraFn;

static void test_ptera(void)
{
    g_ign_objpads = 1;
    g_ign_addr = 0x87B3C6; g_ign_len = 2;                   /* the GTE V0 pad: Water_Level's stack vectors' pads */
    stubs_install(1);
    ptera_tables_stub(1);
    player_tables_stub(1);
    for (int i = 0; i < iters; i++) {
        game_state();
        gptr o = ptera_setup(), b = o + 0x1C0, pl = U32(G() + 0x940);
        u32 a[3] = {o};
        (void)b;
        /* the helpers */
        {
            S16(pl + 0x42) = (s16)(S16(o + 0x42) + (s32)(rnd() % 2 ? rnd() % 0x4000 : rnd() % 3) - (rnd() % 2 ? 0x2000 : 1));
            if (rnd() % 4 == 0) { S16(pl + 0x40) = S16(o + 0x40); S16(pl + 0x44) = S16(o + 0x44); }
            u32 vt[4] = {o + 0x40, pl + 0x40, rnd() % 2 ? (rnd() % 2 ? 0x1C2 : 0x28A) : rnd(), o + 0x1E8};
            check(0x48F500, Vec_Toward, "Vec_Toward", 4, vt, 0);
            U16(o + 0x1E0) = (u16)rnd(); U16(o + 0x1E2) = (u16)rnd(); U16(o + 0x1E4) = rnd() % 2 ? 0xFD44 : (u16)rnd();
            u32 hp[3] = {o, o + 0x1F0, o + 0x1E0};
            check(0x433860, Ptera_HoldPoint, "Ptera_HoldPoint", 3, hp, 0);
            U8(pl + 0x54) = (u8[]){2, 5, 1, 3}[rnd() % 4]; U16(pl + 0x174) = (u16)rnd(); U8(G() + 0x108E) = rnd() % 3;
            U32(o + 0x1D4) = rnd() % 3 ? rnd() % 3 : rnd();
            check(0x4338F0, Ptera_Timer, "Ptera_Timer", 1, a, 0xFFFFFFFF);
            U8(G() + 0x108F) = rnd() % 2 ? 0 : (u8)rnd();
            check(0x42B6F0, Ptera_PlAnim, "Ptera_PlAnim", 1, a, 0xFFFFFFFF);
            U8(o + 0x57) = rnd() % 4 ? rnd() % 3 : (u8)rnd(); U16(o + 0x1DE) = (u16)(rnd() % 2 ? rnd() % 0x60 - 0x30 : rnd());
            U16(o + 0x4C) = (u16)(rnd() % 2 ? rnd() % 0x400 - 0x200 : rnd());
            check(0x4297D0, Ptera_Steer, "Ptera_Steer", 1, a, 0);
            for (int k = 0; k < 4; k++) {
                ptera_water(o + 0x40);
                u32 wl[1] = {o + 0x40};
                sprintf(g_what, "water %u areas", U32(0x87DE14));
                check(0x413980, Water_Level, "Water_Level", 1, wl, 0xFFFFFFFF);
            }
        }
        /* deciding */
        for (int k = 0; k < 24; k++) {                      /* circling */
            game_state();
            o = ptera_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
            U8(o + 0x56) = rnd() % 4 ? 1 + rnd() % 2 : (u8)rnd();
            U8(pl + 0x54) = rnd() % 2 ? 1 : (u8)rnd(); U8(pl + 0x55) = rnd() % 2 ? 2 : (u8)rnd();
            if (rnd() % 3 == 0) { U16(pl + 0xE8) = 0; U16(pl + 0xEC) = 0; U16(pl + 0xEA) = (u16)(1 + rnd() % 0xFF); }   /* (its low byte only) */
            static const s32 dd[] = {0, 1, 7999, 8000, 8001, 3000};
            static const u16 ao[] = {0, 0x5F, 0x60, 0x61, 0x3FF, 0x400, 0x401, 0xFA1, 0xFA0, 0xF9F, 0xC01, 0xC00, 0xBFF};
            if (rnd() % 3) ptera_place(o, pl, dd[rnd() % 6], ao[rnd() % 13]);
            if (rnd() % 2) U16(o + 0x1F6) = 0;
            if (rnd() % 2) U16(o + 0x1E4) = 0;
            if (rnd() % 2) U16(o + 0x1E6) = rnd() % 2;
            ptera_rand8(pl);
            U32(LOG_BASE) = 0;
            sprintf(g_what, "ptera %X think fly step %u side %u", o, U8(o + 0x56), U8(o + 0x57));
            check(0x4294D0, Ptera_ThinkFly, "Ptera_ThinkFly", 1, a, 0);
        }
        for (int k = 0; k < 32; k++) {                      /* the swoop */
            game_state();
            o = ptera_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
            U8(o + 0x56) = rnd() % 4 ? 3 : (u8)(rnd() % 4);
            U16(o + 0x1E2) = rnd() % 3 ? (rnd() % 2 ? 0x400 : 0xFC00) : (u16)rnd();
            static const s32 dd[] = {0x1770, 0x1771, 0x176F, 3000, 1500, 600};
            ptera_place(o, pl, dd[rnd() % 6], (u32)rnd() % 0x100 - 0x80);
            U16(pl + 0x174) = rnd() % 2 ? 2 : (u16)(rnd() & 3);
            if (rnd() % 2) Bits_Set(0x87E974, 1, 0);
            if (rnd() % 2) Bits_Set(0x87E974, 0, 0);
            S16(o + 0x1EC) = (s16)(rnd() % 0x100); S16(o + 0x1E4) = (s16)(S16(o + 0x1EC) + (s32)(rnd() % 3) - 1);
            if (rnd() % 2) {                                /* the angle then at its limit, the player's heading about its own */
                static const s16 want[] = {0x5F, 0x60, 0x61, -0x5F, -0x60, -0x61};
                s16 w = want[rnd() % 6]; u16 h = 0;
                u16 base = U16(o + 0x4A);
                for (u32 e = 0; e < 0x200; e++) {
                    U16(o + 0x4A) = (u16)((base + (e & 1 ? e / 2 : 0x1000 - e / 2)) & 0xFFF);
                    if (ptera_swoop_angle(o, pl, &h) == w) break;
                }
                ptera_swoop_angle(o, pl, &h);
                static const u16 ho[] = {0, 0xF, 0x10, 0x11, 0xFF1, 0xFF0, 0xFEF};
                U16(pl + 0x4A) = (u16)((h + ho[rnd() % 7]) & 0xFFF);
                if (rnd() % 2) U32(o + 0x1D4) = rnd() % 2;
                if (rnd() % 2) { Flag_Set(2, 0x1A, 0); U16(pl + 0x174) = (u16)(2 | (rnd() & 1) | (rnd() % 2 ? rnd() & 0xFFF0 : 0)); }
            }
            ptera_rand8(pl);
            U32(LOG_BASE) = 0;
            sprintf(g_what, "ptera %X think swoop step %u", o, U8(o + 0x56));
            check(0x429A10, Ptera_ThinkSwoop, "Ptera_ThinkSwoop", 1, a, 0);
        }
        for (int k = 0; k < 24; k++) {                      /* perched */
            game_state();
            o = ptera_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
            U8(o + 0x56) = rnd() % 4 ? 1 : (u8)(rnd() % 3);
            static const s32 dd[] = {6000, 6001, 5999, 1000, 999, 1001, 3000};
            static const u16 ao[] = {0x44F, 0x450, 0x451, 0xBB1, 0xBB0, 0xBAF, 0, 0x800};
            ptera_place(o, pl, dd[rnd() % 7], ao[rnd() % 8]);
            if (rnd() % 2) Bits_Set(0x87E974, 1, 0);
            ptera_rand8(pl);
            U32(LOG_BASE) = 0;
            sprintf(g_what, "ptera %X think perch step %u", o, U8(o + 0x56));
            check(0x429E40, Ptera_ThinkPerch, "Ptera_ThinkPerch", 1, a, 0);
        }
        /* the state handlers */
        game_state();
        o = ptera_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
        U8(o + 0x54) = rnd() % 5; U8(o + 0x55) = rnd() % 8; U8(o + 0x56) = rnd() % 6;
        check(0x429430, Ptera_Normal, "Ptera_Normal", 1, a, 0);
        U8(o + 0x55) = rnd() % 4;
        check(0x429460, Ptera_Hit, "Ptera_Hit", 1, a, 0);
        check(0x429480, Ptera_Die, "Ptera_Die", 1, a, 0);
        U8(o + 0x55) = rnd() % 8;
        check(0x4294A0, Ptera_Grab, "Ptera_Grab", 1, a, 0);
        for (int k = 0; k < 6; k++) {
            game_state();
            o = ptera_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
            U8(o + 0x54) = rnd() % 5; U8(o + 0x55) = rnd() % 8;
            U8(pl + 0x54) = rnd() % 2 ? 1 : (u8)(rnd() % 6); U8(pl + 0x55) = rnd() % 2 ? 6 : (u8)rnd();
            U16(G() + 0x10B4) = (u16[]){2, 3, 4, 7, 8, 9, 10}[rnd() % 7];
            U16(G() + 0x1090) = rnd() % 2 ? 0x10C : (u16)rnd();
            U16(o + 0xF0) = U16(o + 0xF2) = U16(o + 0xF4) = 0;
            s16 f = (s16)Floor_Height(o + 0x40, o + 0x40, 0x40, 0);
            static const s32 dy[] = {0, 1, -1, -0x1388, -0x1389, -0x1387, -0x3000};
            if (rnd() % 2) S16(o + 0x42) = (s16)(f + dy[rnd() % 7]);
            if (rnd() % 3 == 0) S16(o + 0x42) = (s16)(-0x3A98 + (s32)(rnd() % 3) - 1);
            if (rnd() % 2) S16(pl + 0x42) = (s16)(f + (s32)(rnd() % 3) - 1 + (rnd() % 2 ? 0 : (s32)(rnd() % 0x2000) - 0x1000));
            U16(pl + 0x1FA) = (u16)(0x7F + rnd() % 3);
            U16(o + 0x1F6) = rnd() % 2; U32(o + 0x1D0) = rnd() % 2;
            U8(o + 0x118) = (u8)((rnd() & 0xF0) | rnd() % (U32(U32(o + 0x64) + 0x54) ? 2 : 1));   /* (a part it has) */
            sprintf(g_what, "ptera %X handler state %X", o, U32(o + 0x54));
            if (k % 3) check(0x429040, Ptera_Handler, "Ptera_Handler", 1, a, 0);
            else check(0x429030, Ptera_Type, "Ptera_Type", 1, a, 0);
        }
        model_data(0x640000);
        anim_setup_at(AN_OBJ); memcpy(MEM(o + 0x60), MEM(AN_OBJ + 0x60), 0x50);
        U32(0x6425DC) = AN_DATA;                            /* its first animation */
        U8(G() + 0x108E) = rnd() % 3; U32(0x74AF10) = rnd(); U8(0x87E747) = rnd() % 2;
        check(0x429290, Ptera_Init, "Ptera_Init", 1, a, 0);
        /* moving, each step */
        static const PteraFn mv[] = {
            {0x4296A0, Ptera_Fly, "Ptera_Fly", 4}, {0x429890, Ptera_Rise, "Ptera_Rise", 4}, {0x429C50, Ptera_Swoop, "Ptera_Swoop", 4},
            {0x429F20, Ptera_Perch, "Ptera_Perch", 2}, {0x42A070, Ptera_Peck, "Ptera_Peck", 3}, {0x42A210, Ptera_Claw, "Ptera_Claw", 3},
            {0x42A3B0, Ptera_Carry, "Ptera_Carry", 5}, {0x42A810, Ptera_Pin, "Ptera_Pin", 7}, {0x42AE50, Ptera_Damage, "Ptera_Damage", 1},
            {0x42B0A0, Ptera_Knock, "Ptera_Knock", 2}, {0x42B1D0, Ptera_KnockHard, "Ptera_KnockHard", 2},
            {0x42B300, Ptera_KnockHeavy, "Ptera_KnockHeavy", 2}, {0x42B440, Ptera_Dying, "Ptera_Dying", 4}};
        const unsigned nm = sizeof mv / sizeof mv[0];
        game_state(); world_setup(); enemy_state();
        o = O_OBJ + 0x260 * (1 + rnd() % 6); a[0] = o; b = o + 0x1C0;
        anim_setup_at(O_OBJ);
        do anim_setup_at(o); while (U32(o + 0x68) < 4);
        memcpy(MEM(O_OBJ + 0x60), MEM(o + 0x60), 0x50);     /* (the player: the same parts and animations) */
        U32(G() + 0x940) = pl = O_OBJ;
        U32(pl + 0x128) = rnd() % 2 ? 0x720730 : 0; U32(o + 0x128) = 0x720730;
        u8 sv[0x260]; memcpy(sv, MEM(o), 0x260);
        for (unsigned k = 0; k < 12 * nm; k++) {
            const PteraFn *f = &mv[k % nm];
            memcpy(MEM(o), sv, 0x260);
            for (u32 j = 0x1C0; j < 0x214; j += 4) U32(o + j) = rnd();
            U32(o + 0x1C0) = rnd() & 7;
            U8(o + 0x1CC) = rnd() % 4 ? 1 + rnd() % 3 : rnd() % 5; U8(o + 0x1CD) = rnd() % 3 ? rnd() % 8 : (u8)rnd();
            for (u32 j = 0x1DE; j < 0x210; j += 2) U16(o + j) = rnd() % 3 ? rnd() % 3 : (u16)rnd();
            U32(o + 0x1D0) = rnd() % 2 ? rnd() % 0x200 : rnd(); U32(o + 0x1D4) = rnd() % 3;
            U8(o + 0x56) = (u8)(rnd() % 8 ? rnd() % f->steps : rnd());
            U8(o + 0x57) = (u8)(rnd() % 4 ? rnd() % 3 : rnd());
            static const u16 fr[] = {0, 3, 4, 5, 9, 0xA, 0xB, 0xE, 0xF, 0x10, 0x12, 0x13, 0x14, 0x15, 0x16, 0x1D, 0x1E, 0x1F, 0x26, 0x27, 0x2B, 0x2C, 0x2D};
            U16(o + 0x92) = rnd() % 4 ? (u16)(fr[rnd() % 23] + rnd() % 3 - 1) : (u16)(rnd() % 0x40);
            S16(o + 0x1F8) = (s16)(rnd() % 2 ? 0x26 - rnd() % 3 + 1 : rnd() % 0x40);
            if (rnd() % 2) S16(o + 0x1F8) = (s16)(U16(o + 0x92) + rnd() % 3 - 1);
            U16(o + 0x1E8) = rnd() % 3 ? (u16[]){3, 4, 5, 6, 0x1D, 0x1E, 0x1F, 0x27, 0x28, 0x29, 0x3B, 0x3C, 0x4F, 0x50}[rnd() % 14] : (u16)rnd();
            S16(o + 0x1EA) = (s16)(rnd() % 2 ? (s32)(rnd() % 0x20) : (s32)(rnd() % 3) + (rnd() % 2 ? 0xF : 0x14) - 1);
            S16(o + 0x1FE) = (s16)(S16(o + 0x42) + (s32)(rnd() % 0x1000) - 0x800);
            if (rnd() % 3 == 0) S16(o + 0x1FE) = (s16)(S16(o + 0x42) + (s32)(rnd() % 3) - 1 - (rnd() % 2 ? 0x5DC : 0));
            S16(o + 0x1F2) = (s16)(S16(o + 0x42) + (s32)(rnd() % 0x2000) - 0x1000);
            if (rnd() % 3 == 0) S16(o + 0x1F2) = (s16)(S16(o + 0x42) + (s32)(rnd() % 3) - 1 + (s32)(u16[]){0x64, 0x578, 0xBB8}[rnd() % 3]);
            U16(o + 0xE8) = rnd() % 2 ? 0 : (u16)rnd(); U16(o + 0xEA) = rnd() % 2 ? 0 : (u16)rnd(); U16(o + 0xEC) = rnd() % 2 ? 0 : (u16)rnd();
            U16(pl + 0xE8) = rnd() % 2 ? 0 : (u16)rnd(); U16(pl + 0xEA) = rnd() % 2 ? 0 : (u16)rnd(); U16(pl + 0xEC) = rnd() % 2 ? 0 : (u16)rnd();
            S16(pl + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x100 - 0x10 : rnd());
            if (rnd() % 3 == 0) S16(pl + 0x108) = (s16)(Damage_Scale((u32[]){0x28, 0x96, 0xC8}[rnd() % 3]) + rnd() % 3 - 1);
            U16(o + 0xD4) = (u16)rnd(); U16(o + 0x172) = rnd() % 5; U16(o + 0x176) = rnd() % 2 ? rnd() % 0x400 : (u16)rnd();
            U16(o + 0x178) = (u16)((U16(o + 0x4A) + (u16[]){0, 0x3FF, 0x400, 0x401, 0xC01, 0xC00, 0xBFF, 0x800}[rnd() % 8] + (rnd() % 4 ? 0 : rnd())) & 0xFFF);
            S16(o + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x400 - 0x10 : rnd());
            U16(o + 0x10A) = (u16)(rnd() % 2 ? rnd() % 0x800 : rnd());
            if (rnd() % 3 == 0) U32(o + 0x1D0) = (u32)(S16(o + 0x10A) / 2 - (s32)U16(o + 0x176) + (s32)(rnd() % 3) - 1);
            U8(o + 0x170) = (u8)rnd(); U8(o + 0x184) = (u8)rnd();
            S16(o + 0x10E) = (s16)(rnd() % 2 ? S16(o + 0x42) + (s32)(rnd() % 3) - 1 : (s32)(rnd() % 3 == 0 ? 0 : rnd()));
            U8(G() + 0x108E) = rnd() % 3; U8(G() + 0x108F) = rnd() % 2 ? 0 : (u8)rnd();
            for (u32 j = 0; j < 20; j++) if (rnd() % 2) U8(O_K2 + j * 0xAC + 0x50) &= ~1;
            if (rnd() % 2) { U16(o + 0xF0) = (u16)(rnd() % 0x600 - 0x300); U16(o + 0xF2) = (u16)(rnd() % 0x600 - 0x300); U16(o + 0xF4) = (u16)(rnd() % 0x600 - 0x300); }
            else if (rnd() % 2) U16(o + 0xF0) = U16(o + 0xF2) = U16(o + 0xF4) = 0;   /* (still: the heights' edges hold) */
            if (rnd() % 3 == 0) { U16(o + 0xE8) = U16(o + 0xEA) = U16(o + 0xEC) = 0; U16(o + 0x1FC) = (u16)(2 + rnd() % 0x20); }   /* (a dive going on) */
            if (f->addr == 0x42AE50) { U8(o + 0x58) = (u8)(rnd() % 16); U8(G() + 0x757) = 0; }   /* (a kill that scores) */
            U16(0x87DC12) = rnd() % 2 ? 0 : (u16)rnd();     /* (Pad_Shake) */
            if (f->addr == 0x429F20 && rnd() % 2) {         /* perched: the player near, about the hold point's 4000 and the 0x60 angle */
                static const s32 dd[] = {3999, 4000, 4001, 2000};
                static const u16 ao[] = {0, 0, 0x5F, 0x60, 0x61, 0xFA1, 0xFA0, 0xF9F};
                U16(o + 0xF0) = U16(o + 0xF2) = U16(o + 0xF4) = 0;
                ptera_place(o, pl, dd[rnd() % 4], ao[rnd() % 8]);
                static const u16 ho[] = {0, 0x3FF, 0x400, 0x401, 0xC01, 0xC00, 0xBFF, 0x800};
                U16(pl + 0x4A) = (u16)((U16(o + 0x4A) + ho[rnd() % 8]) & 0xFFF);
                Flag_Set(2, 0x1A, rnd() % 2);
            }
            ptera_rand8(pl);
            if (f->addr == 0x42B440) {                      /* falling: the cliff, the water */
                static const u16 rm[][2] = {{1, 0xC}, {0, 1}, {6, 2}, {1, 1}, {3, 3}};
                int r = rnd() % 5; U8(G() + 0x1091) = (u8)rm[r][0]; U8(G() + 0x1090) = (u8)rm[r][1];
                S16(o + 0x40) = (s16)(rnd() % 2 ? (s32)(s16)0xE0C0 - (s32)(rnd() % 3) + 1 - (rnd() % 2 ? 0x400 : 0) : (s32)rnd());
                S16(o + 0x44) = (s16)(rnd() % 2 ? (s32)(rnd() % 3) - 1 + (rnd() % 2 ? 0x1289 : -0x4C7) : (s32)rnd());
                S16(o + 0x42) = (s16)(rnd() % 2 ? (s32)(rnd() % 3) - 1 + (rnd() % 2 ? -0x1194 : -0x1964) : (s32)(rnd() % 0x4000) - 0x2000);
                if (rnd() % 2) U16(o + 0x10E) = 0;
                ptera_water(o + 0x40);
                if (rnd() % 2 && U32(0x87DE14)) { gptr w = U32(G() + 0x948); U8(w + 1) = 3; U8(w + 0x12) = (u8)(s8)(S16(o + 0x42) / 100 + (s32)(rnd() % 3) - 1); }
            }
            anim_maybe_end(o);
            if (rnd() % 2) U16(o + 0x102) |= 0x20;
            if (rnd() % 2) anim_maybe_end(pl);
            if (rnd() % 4 == 0) { U16(o + 0xE8) = (u16)(1 + rnd() % 0xFF); U16(o + 0xEA) = U16(o + 0xEC) = 0; }   /* (a low byte only) */
            if (f->addr == 0x429890 && rnd() % 2) {         /* rising: at the frames that end it, the height's edge */
                U8(o + 0x56) = 2; U16(o + 0xF0) = U16(o + 0xF2) = U16(o + 0xF4) = 0;
                U16(o + 0x92) = (u16[]){0x11, 0x12, 0x25, 0x26}[rnd() % 4];
                S16(o + 0x1FE) = (s16)(S16(o + 0x42) - 0x5DC + (s32)(rnd() % 3) - 1);
            }
            if (f->addr == 0x42A810 && rnd() % 2) {         /* pinning: still diving */
                U8(o + 0x56) = 2; U16(o + 0xE8) = U16(o + 0xEA) = U16(o + 0xEC) = 0; U16(o + 0x1FC) = (u16)(2 + rnd() % 0x20);
                S16(o + 0x1F2) = (s16)(S16(o + 0x42) + 0x64 + (s32)(rnd() % 0x400));
            }
            if (f->addr == 0x42B440 && rnd() % 2) {         /* falling: exactly at the cliff's limits, the water's surface */
                U8(o + 0x56) = 1; U16(o + 0xF0) = U16(o + 0xF2) = U16(o + 0xF4) = 0;
                if (rnd() % 2) {
                    U8(G() + 0x1091) = 1; U8(G() + 0x1090) = 0xC;
                    S16(o + 0x40) = (s16)(-0x1F41 - (s32)(rnd() % 0x400));
                    S16(o + 0x44) = (s16)(rnd() % 2 ? -0x4C7 - (s32)(rnd() % 2) : -0x4C6);
                    S16(o + 0x42) = (s16)((S16(o + 0x44) > -0x4C7 ? -0x1194 : -0x1964) + (s32)(rnd() % 3) - 1);
                } else {
                    U8(G() + 0x1091) = 0; U8(G() + 0x1090) = 1; U16(o + 0x10E) = 0;
                    S16(o + 0x42) = (s16)(-100 * (s32)(1 + rnd() % 20));
                    ptera_water(o + 0x40);
                    gptr w = U32(G() + 0x948);
                    U32(0x87DE14) = 1 + U32(0x87DE14);
                    U8(w + 1) = 3; U8(w + 0x1A) = 1; U8(w + 0x10) = U8(w + 0x11) = 5; U16(w + 0x14) = 0;
                    S16(w + 0xC) = (s16)(S16(o + 0x40) - 100); S16(w + 0xE) = (s16)(S16(o + 0x44) - 100);
                    U8(w + 0x12) = (u8)(s8)(S16(o + 0x42) / 100 + (s32)(rnd() % 3) - 1);
                }
            }
            U32(LOG_BASE) = 0;
            sprintf(g_what, "ptera %X %s step %u side %u frame %X", o, f->name, U8(o + 0x56), U8(o + 0x57), U16(o + 0x92));
            check(f->addr, f->fn, f->name, 1, a, 0);
        }
        /* the attacks land */
        static const PteraFn gh[] = {{0x42AC30, Ptera_PeckHit, "Ptera_PeckHit"}, {0x42ACB0, Ptera_ClawHit, "Ptera_ClawHit"}, {0x42AD30, Ptera_PinHit, "Ptera_PinHit"}};
        for (int k = 0; k < 24; k++) {
            memcpy(MEM(o), sv, 0x260);
            U8(o + 0x56) = rnd() % 4 ? (k % 3 == 2 ? 2 : 1) : (u8)(rnd() % 4);
            U8(G() + 0x108E) = rnd() % 3; U8(G() + 0x108F) = rnd() % 4;
            U16(pl + 0x4A) = (u16)((U16(o + 0x4A) + (u16[]){0, 0x3FF, 0x400, 0x401, 0xC01, 0xC00, 0xBFF, 0x800}[rnd() % 8]) & 0xFFF);
            S16(pl + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x100 - 0x10 : rnd());
            for (u32 j = 0; j < 20; j++) if (rnd() % 2) U8(O_K2 + j * 0xAC + 0x50) &= ~1;
            U32(LOG_BASE) = 0;
            sprintf(g_what, "ptera %X %s step %u", o, gh[k % 3].name, U8(o + 0x56));
            check(gh[k % 3].addr, gh[k % 3].fn, gh[k % 3].name, 1, a, 0);
        }
        for (int k = 0; k < 8; k++) {                       /* the player falls, gets up */
            anim_maybe_end(pl);
            U8(pl + 0x57) = rnd() % 2;
            U16(pl + 0x92) = (u16)(rnd() % 2 ? (U8(pl + 0x57) ? 0x24 : 0x20) + rnd() % 3 - 1 : rnd() % 0x40);
            S16(pl + 0x108) = (s16)(rnd() % 2 ? (s32)Damage_Scale(U8(pl + 0x57) ? 0xC8 : 0x96) + (s32)(rnd() % 3) - 1 : (s32)(rnd() % 0x100) - 0x10);
            if (rnd() % 2) U16(pl + 0x102) |= 0x20;
            u32 p1[1] = {pl};
            U32(LOG_BASE) = 0;
            check(0x42A730, Ptera_Dropped, "Ptera_Dropped", 1, p1, 0);
            anim_maybe_end(pl);
            check(0x42AC00, Ptera_Released, "Ptera_Released", 1, p1, 0);
        }
    }
    player_tables_stub(0);
    ptera_tables_stub(0);
    stubs_install(0);
    g_ign_objpads = 0;
    g_ign_addr = 0; g_ign_len = 0;
}

/* ---- the tyrannosaurus ---- */
static const u32 g_tr_tabs[][2] = {{0x7201B0, 5}, {0x7201C4, 12}, {0x7201F4, 12}, {0x720224, 4}, {0x720234, 4}, {0x720244, 12}};
static u32 g_tr_saved[6][12];
static void trex_tables_stub(int on)
{
    for (int k = 1; k <= 12; k++) g_stubs[k].spec = on ? "d" : "";
    for (int t = 0; t < 6; t++)
        for (u32 k = 0; k < g_tr_tabs[t][1]; k++) {
            gptr a = g_tr_tabs[t][0] + 4 * k;
            if (on) { g_tr_saved[t][k] = U32(a); U32(a) = (u32)(uintptr_t)g_recs[1 + k]; }
            else U32(a) = g_tr_saved[t][k];
        }
}

#define TR_ATK  0x6A1000u       /* attack spheres */
#define TR_QUAD 0x6A1800u       /* a quad; +0x40 a point out; +0x100 a frame-flag table */
#define TR_GATE 0x720168u
#define TR_PT   0x720198u

/* 16 linked parts about o (its head is part 0xF), at the animation test's parts */
static void trex_parts(gptr o)
{
    for (u32 i = 0; i < 16; i++) {
        gptr p = AN_PART + i * 0x80;
        for (u32 a = p; a < p + 0x80; a += 4) U32(a) = rnd() % 2 ? rnd() % 0x1000 : rnd();
        for (u32 a = 0x40; a < 0x46; a += 2) U16(p + a) = (u16)(S16(o + a) + (s32)(rnd() % 4000) - 2000);
        U32(p + 0x54) = i < 15 ? p + 0x80 : 0;
        U32(p + 0x58) = i ? p - 0x80 : o;
    }
    U32(o + 0x64) = AN_PART;
}

/* Anim_FrameFlag 0 or 1 */
static void trex_ff(gptr o, int on)
{
    if (!on) { U32(o + 0xA4) = rnd() % 2 ? 0 : TR_QUAD + 0x100; for (u32 k = 0; k < 0x40; k += 4) U32(TR_QUAD + 0x100 + k) = 0; return; }
    U32(o + 0xA4) = TR_QUAD + 0x100;
    for (u32 k = 0; k < 0x40; k += 4) U32(TR_QUAD + 0x100 + k) = 0xFFFFFFFF;
}

static const u32 g_tr_dist[] = {0, 0x7CF, 0x7D0, 0x7D1, 0xBB7, 0xBB8, 0xBB9, 0xDAB, 0xDAC, 0xDAD, 0x1387, 0x1388, 0x1389,
                                0x157B, 0x157C, 0x157D, 0x1963, 0x1964, 0x1965, 0x1B57, 0x1B58, 0x1B59, 0x2EDF, 0x2EE0, 0x2EE1, 0x7FFFFFFF};
static const u16 g_tr_ang[] = {0, 0x1F, 0x20, 0x21, 0x7F, 0x80, 0x81, 0xFF, 0x100, 0x101, 0x17F, 0x180, 0x181, 0x1FF, 0x200, 0x201,
                               0x3FF, 0x400, 0x401, 0x5FF, 0x600, 0x601, 0x800};

/* o's heading so that the angle from 'from' to p is about an edge */
static void trex_aim(gptr from, gptr o, gptr p)
{
    u32 e = rnd() % 4 ? g_tr_ang[rnd() % 23] : rnd() % 0x1000;
    u16 an = (u16)Angle_XZ(from, p);
    U16(o + 0x4A) = (u16)((an + (rnd() % 2 ? e : 0x1000 - e)) & 0xFFF);
}

static void trex_rand8(void)
{
    static const u8 r[] = {1, 2, 3, 4, 5, 7, 8, 9, 0xF, 0x10, 0x11, 0x1F, 0x20, 0x21, 0, 0xFF};
    if (rnd() % 4) U16(0x87DE10) = rand8_seed(r[rnd() % 16], rnd() % 4 == 0);
}

/* a tyrannosaurus among the test objects; the player its target */
static gptr trex_setup(void)
{
    gptr o = raptor_setup(), pl = U32(G() + 0x940);
    U32(o + 0x184) &= ~0x40u;
    trex_parts(o);
    U32(o + 0x120) = AN_PART + 0xE * 0x80; U32(o + 0x124) = AN_PART + 0xA * 0x80;   /* its feet */
    U8(o + 0x15F) = rnd() % 4 ? 3 : rnd() % 16;
    U32(o + 0x1C0) = rnd() % 4 ? rnd() & 0x1FFF : rnd();
    U32(o + 0x1C4) = rnd() % 4 ? rnd() % 3 : rnd() % 2 ? 3 : rnd();
    U8(o + 0x1C8) = 1 + rnd() % 3; U8(o + 0x1C9) = rnd() % 4 ? rnd() % 12 : 6; U8(o + 0x1CA) = rnd() % 4; U8(o + 0x1CB) = rnd() % 2;
    for (u32 k = 0x1CC; k <= 0x1D8; k += 4) U32(o + k) = rnd() % 3 ? rnd() % 3 : rnd() % 2 ? rnd() % 0x2000 : rnd();
    for (u32 k = 0x1E0; k < 0x206; k += 2) U16(o + k) = rnd() % 3 ? rnd() % 3 : (u16)rnd();
    U16(o + 0x1E6) = rnd() % 3;
    U8(o + 0x207) = rnd() % 2 ? rnd() % 6 : (u8)rnd(); U8(o + 0x208) = rnd() % 3;
    U32(o + 0x13C) = rnd() % 4 ? g_tr_dist[rnd() % 26] : rnd() % 0x3000;
    U8(o + 0x13A) = rnd() % 3 == 0;
    U8(o + 0x57) = rnd() % 2;
    U16(pl + 0x5A) = rnd() % 3 ? U16(o + 0x5A) : (u16)(U16(o + 0x5A) + 1 + rnd() % 2);
    U16(pl + 0x174) = rnd() % 2 ? rnd() & 3 : (u16)rnd();
    U32(pl + 0x134) = 0;                                /* (no hit spheres: Enemy_AttackHit keeps +0x119) */
    Bits_Set(0x87E974, 0, rnd() % 3 == 0);
    U8(G() + 0x7C4) = rnd() % 2 ? (u8[]){0, 9, 0xA, 0xB, 0xC, 0x10}[rnd() % 6] : (u8)rnd();
    U16(o + 0xD8) = U16(pl + 0x40); U16(o + 0xDA) = U16(pl + 0x42); U16(o + 0xDC) = U16(pl + 0x44);
    if (rnd() % 3 == 0) { S16(o + 0xD8) = (s16)(S16(o + 0x40) + (s32)(rnd() % 8000) - 4000); S16(o + 0xDC) = (s16)(S16(o + 0x44) + (s32)(rnd() % 8000) - 4000); }
    trex_aim(o + 0x40, o, o + 0xD8);
    trex_ff(o, rnd() % 2);
    trex_rand8();
    return o;
}

/* attack spheres at TR_ATK: n of them, on the parts or o itself, about the frame */
static void trex_spheres(gptr o, u32 n)
{
    for (u32 k = 0; k < n; k++) {
        gptr a = TR_ATK + k * 0x10;
        for (u32 j = 0; j < 6; j += 2) S16(a + j) = (s16)(rnd() % 400 - 200);
        U16(a + 6) = (u16)rnd();
        U8(a + 8) = rnd() % 0x30; U8(a + 9) = (u8)(U8(a + 8) + rnd() % 0x20);
        U8(a + 0xA) = rnd() % 2 ? 0xFF : rnd() % 16;
        U8(a + 0xB) = k + 1 < n ? (rnd() % 2 ? 1 : (u8)(1 + rnd() % 0xFF)) : 0;
        U32(a + 0xC) = rnd();
    }
    U32(o + 0xA0) = TR_ATK;
    U16(o + 0x102) = (u16)(rnd() % 3 ? rnd() & 0xC : rnd());
    gptr a = TR_ATK + (rnd() % n) * 0x10;
    u32 div = (U16(o + 0x102) & 8) ? 1 : (U16(o + 0x102) & 4) ? 4 : 2;
    u32 e = (rnd() % 2 ? U8(a + 8) : U8(a + 9)) * 2 / div;
    U16(o + 0x92) = rnd() % 4 ? (u16)(e + rnd() % 3 - 1) : (u16)(rnd() % 0x60);
}

/* the player d (exactly) away from p along an axis */
static void trex_place(gptr o, gptr p, s32 d)
{
    static const s16 dir[][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
    int k = rnd() % 4;
    S16(o + 0x40) = (s16)(S16(p) + dir[k][0] * d); S16(o + 0x44) = (s16)(S16(p + 4) + dir[k][1] * d);
}

typedef struct { u32 addr; void *fn; const char *name; u8 steps; } TRexFn;

static void test_trex(void)
{
    g_ign_objpads = 1;
    stubs_install(1);
    trex_tables_stub(1);
    player_tables_stub(1);
    for (int i = 0; i < iters; i++) {
        game_state();
        gptr o = trex_setup(), b = o + 0x1C0, pl = U32(G() + 0x940);
        u32 a[3] = {o};
        /* the helpers */
        {
            static const u32 mx[] = {5, 0xA, 0x11, 0x23, 0x2D, 0x230};
            u32 th[3] = {o, rnd() % 4 ? mx[rnd() % 6] : rnd() % 0x1000, rnd() % 3 ? o + 0xD8 : TR_PT + 8 * (rnd() % 3)};
            check(0x45B3D0, Obj_TurnFromHead, "Obj_TurnFromHead", 3, th, 0);
            for (u32 k = 0; k < 0x40; k += 4) U32(TR_QUAD + 0x40 + k) = rnd();
            trex_rand8();
            u32 zr[2] = {o, TR_QUAD + 0x40};
            check(0x487340, Zone_RandomPoint, "Zone_RandomPoint", 2, zr, 0);
            for (int k = 0; k < 4; k++) {               /* attack spheres in a quad (about o, or far) */
                trex_spheres(o, 1 + rnd() % 4);
                if (rnd() % 8 == 0) U32(o + 0xA0) = 0;
                s32 r = (s32)(u32[]){100, 300, 2000, 30000}[rnd() % 4], x = S16(o + 0x40), z = S16(o + 0x44);
                if (rnd() % 4 == 0) x += 5000;
                s32 q[4][2] = {{x - r, z - r}, {x - r, z + r}, {x + r, z + r}, {x + r, z - r}};
                int w = rnd() % 4 == 0;                     /* (the other winding: nothing inside) */
                for (int c = 0; c < 4; c++) { S16(TR_QUAD + 4 * c) = (s16)q[w ? 3 - c : c][0]; S16(TR_QUAD + 4 * c + 2) = (s16)q[w ? 3 - c : c][1]; }
                if (rnd() % 2) {                            /* a sphere on a part at the quad's edge */
                    gptr s = TR_ATK + (rnd() % 2) * 0x10;
                    if (U8(s + 0xA) == 0xFF) U8(s + 0xA) = 1 + rnd() % 15;
                    gptr p = AN_PART + U8(s + 0xA) * 0x80;
                    for (u32 j = 0x20; j < 0x32; j += 2) U16(p + j) = 0;
                    S16(p + 0x40) = (s16)(x + r + (s32)(rnd() % 3) - 1); S16(p + 0x44) = (s16)(z + (s32)(rnd() % 3) - 1);
                    if (rnd() % 2) { S16(p + 0x40) = (s16)x; S16(p + 0x44) = (s16)(z - r + (s32)(rnd() % 3) - 1); }
                    U16(o + 0x92) = (u16)(U8(s + 8) * 2 / ((U16(o + 0x102) & 8) ? 1 : (U16(o + 0x102) & 4) ? 4 : 2));
                }
                u32 aq[2] = {o, TR_QUAD};
                sprintf(g_what, "trex %X spheres frame %X flags %X", o, U16(o + 0x92), U16(o + 0x102));
                check(0x479B30, Enemy_AttackInQuad, "Enemy_AttackInQuad", 2, aq, 0xFF);
            }
            U8(pl + 0x54) = (u8[]){2, 5, 1, 3}[rnd() % 4]; U16(pl + 0x174) = (u16)rnd(); U8(G() + 0x108E) = rnd() % 3;
            U32(o + 0x1D8) = rnd() % 3 ? rnd() % 3 : rnd();
            check(0x426320, TRex_Timer, "TRex_Timer", 1, a, 0xFFFFFFFF);
            U8(G() + 0x7C4) = rnd() % 2 ? 0xC : (u8)(rnd() % 2 ? 0xB : rnd());
            check(0x426390, TRex_Event, "TRex_Event", 1, a, 0xFFFFFFFF);
            for (int k = 0; k < 4; k++) {
                U32(o + 0x1C4) = rnd() % 4 ? 1 : rnd() % 3;
                U32(b) = rnd() & 0x70;
                U8(G() + 0x7C4) = (u8[]){0, 8, 9, 0xA, 0xC, 0xF, 0x10, 0x11}[rnd() % 8];
                sprintf(g_what, "trex %X script bits %X event %X", o, U32(b), U8(G() + 0x7C4));
                check(0x426410, TRex_Script, "TRex_Script", 1, a, 0xFFFFFFFF);
            }
            for (int k = 0; k < 4; k++) {               /* the gates: spheres on parts at a gate or not */
                U32(o + 0x1C4) = rnd() % 4 ? 0 : 1 + rnd() % 2;
                U8(o + 0x54) = rnd() % 3 ? 1 : (u8)(rnd() % 4); U8(o + 0x55) = (u8[]){7, 8, 9, 0xA, 0xB, 0xC, 0, 4}[rnd() % 8];
                u32 n = 1 + rnd() % 3;
                trex_spheres(o, n);
                for (u32 s = 0; s < n; s++) {
                    gptr sa = TR_ATK + s * 0x10, g = TR_GATE + 0x10 * (rnd() % 3);
                    U8(sa + 8) = 0; U8(sa + 9) = 0x7F; U8(sa + 0xA) = (u8)(1 + s);
                    gptr p = AN_PART + (1 + s) * 0x80;
                    for (u32 j = 0x20; j < 0x32; j += 2) U16(p + j) = 0;
                    s32 cx = 0, cz = 0;
                    for (int c = 0; c < 4; c++) { cx += S16(g + 4 * c); cz += S16(g + 4 * c + 2); }
                    S16(p + 0x40) = (s16)(cx / 4 + (rnd() % 3 ? 0 : 3000)); S16(p + 0x44) = (s16)(cz / 4);
                }
                U16(o + 0x92) = rnd() % 0x40;
                for (u32 f = 0x10; f < 0x13; f++) Flag_Set(7, f, rnd() % 4 == 0);
                sprintf(g_what, "trex %X gates kind %u state %X", o, U32(o + 0x1C4), U32(o + 0x54));
                check(0x4266A0, TRex_Gates, "TRex_Gates", 1, a, 0xFFFFFFFF);
            }
            for (int k = 0; k < 8; k++) {               /* the gate points: distance, angle, where the player is */
                game_state();
                o = trex_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
                U32(o + 0x1C4) = rnd() % 6 ? 0 : 1; U16(o + 0x202) = rnd() % 6 ? 0 : 1;
                gptr p = TR_PT + 8 * (rnd() % 3);
                static const s32 dd[] = {3999, 4000, 4001, 5999, 6000, 6001, 2000, 8000};
                trex_place(o, p, dd[rnd() % 8]);
                trex_aim(o + 0x40, o, p);
                static const s16 px[] = {0x2327, 0x2328, 0x2329, (s16)0xF05F, (s16)0xF060, (s16)0xF061, (s16)0xFC17, (s16)0xFC18, (s16)0xFC19, 0x3000};
                static const s16 pz[] = {0x1F3, 0x1F4, 0x1F5, 0, -0x1000};
                S16(pl + 0x40) = px[rnd() % 10]; S16(pl + 0x44) = pz[rnd() % 5];
                for (u32 f = 0x10; f < 0x13; f++) Flag_Set(7, f, rnd() % 4 == 0);
                if (rnd() % 2) U16(0x87DE10) = rand8_seed((u8[]){7, 8, 9}[rnd() % 3], 0);
                sprintf(g_what, "trex %X target pos %d,%d player %d,%d", o, S16(o + 0x40), S16(o + 0x44), S16(pl + 0x40), S16(pl + 0x44));
                check(0x426530, TRex_Target, "TRex_Target", 1, a, 0xFFFFFFFF);
            }
        }
        /* deciding */
        static const TRexFn th[] = {
            {0x4243B0, TRex_Decide, "TRex_Decide", 1}, {0x423C60, TRex_ThinkStand, "TRex_ThinkStand", 2},
            {0x423F70, TRex_ThinkWalk, "TRex_ThinkWalk", 2}, {0x4241F0, TRex_ThinkRoar, "TRex_ThinkRoar", 2},
            {0x4242D0, TRex_ThinkLook, "TRex_ThinkLook", 2}, {0x4246A0, TRex_ThinkSnatch, "TRex_ThinkSnatch", 1},
            {0x424E20, TRex_ThinkBite, "TRex_ThinkBite", 1}, {0x424F50, TRex_ThinkWalkBite, "TRex_ThinkWalkBite", 1},
            {0x425040, TRex_ThinkCharge, "TRex_ThinkCharge", 1}};
        for (int k = 0; k < 72; k++) {
            const TRexFn *f = &th[k < 54 ? k % 3 : k % 9];
            game_state();
            o = trex_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
            U8(o + 0x56) = rnd() % 6 ? 1 : (u8)(rnd() % 3);
            if (rnd() % 4) {                                /* nothing keeps it from attacking */
                U8(o + 0x13A) = 0; U16(o + 0x1FE) = 0; Bits_Set(0x87E974, 0, 0); U32(o + 0x1D8) = 0; U16(o + 0x1F8) = 0;
                U16(o + 0x202) = 1; U32(o + 0x1C4) = rnd() % 3 ? rnd() % 3 : 1; U16(pl + 0x174) &= ~2;
                U16(pl + 0x5A) = rnd() % 3 ? U16(o + 0x5A) : (u16)(U16(o + 0x5A) + 1);
                U32(o + 0x13C) = g_tr_dist[rnd() % 25];
                if (f->addr != 0x423C60 && U32(o + 0x1C4) == 1) U8(G() + 0x7C4) = 0;
            }
            if (rnd() % 2) { U16(o + 0x200) = 0; U16(o + 0x202) = rnd() % 2; U16(o + 0x1FC) = 0; }
            if (rnd() % 3 == 0) U32(o + 0x1C4) = 0;
            if (rnd() % 2) { U16(pl + 0x174) &= ~2; U16(pl + 0x5A) = rnd() % 3 ? U16(o + 0x5A) : (u16)(U16(o + 0x5A) + 1); }
            for (u32 j = 0x10; j < 0x13; j++) Flag_Set(7, j, rnd() % 3 == 0);
            if (f->addr == 0x423C60 && rnd() % 3 == 0) {    /* standing: no attack, near: a point to walk to */
                U8(o + 0x13A) = 1; U16(o + 0x202) = 1; U16(pl + 0x5A) = U16(o + 0x5A); U16(pl + 0x174) &= ~2;
                U32(o + 0x1C4) = rnd() % 3 ? 0 : rnd() % 4; U16(o + 0x200) = rnd() % 4 == 0;
                U32(o + 0x13C) = (u32[]){0, 0x7CF, 0xBB7, 0xBB8, 0x1964, 0x1965}[rnd() % 6];
                U8(o + 0x56) = 1; U8(G() + 0x7C4) = 0;
            }
            static const s16 ex[] = {0, 1, -1, 0x1B57, 0x1B58, 0x1B59};
            if (rnd() % 2) S16(o + 0x40) = ex[rnd() % 6];
            if (rnd() % 2) S16(o + 0x44) = ex[rnd() % 3];
            trex_aim(o + 0x40, o, o + 0xD8);
            static const u16 fr[] = {0x27, 0x28, 0x29};
            U16(o + 0x92) = rnd() % 2 ? fr[rnd() % 3] : (u16)(rnd() % 0x60);
            U8(G() + 0x108F) = rnd() % 2 ? (u8[]){3, 4, 6, 7, 8}[rnd() % 5] : (u8)rnd();
            trex_rand8();
            U32(LOG_BASE) = 0;
            sprintf(g_what, "trex %X %s step %u kind %u dist %X", o, f->name, U8(o + 0x56), U32(o + 0x1C4), U32(o + 0x13C));
            check(f->addr, f->fn, f->name, 1, a, f->addr == 0x4243B0 ? 0xFFFFFFFF : 0);
        }
        for (int k = 0; k < 16; k++) {                      /* each attack's choice at its edges */
            game_state();
            o = trex_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
            int sc = k % 8;
            U8(o + 0x56) = 1; U8(o + 0x13A) = 0; U16(o + 0x1FE) = 0; Bits_Set(0x87E974, 0, 0); U32(o + 0x1D8) = 0;
            U16(o + 0x1F8) = rnd() % 4 == 0; U16(o + 0x202) = 1; U16(pl + 0x174) &= ~2;
            U32(o + 0x1C4) = rnd() % 2 ? 1 : rnd() % 3; U8(G() + 0x7C4) = 0;
            U16(pl + 0x5A) = sc == 3 || sc == 4 || sc == 6 ? (u16)(U16(o + 0x5A) + 1) : U16(o + 0x5A);
            static const u32 dl[8][4] = {{0xDAD, 0x1000, 0x157B, 0xDAC}, {0x157C, 0x2000, 0x2EDF, 0x2EE0}, {0x7D1, 0x1000, 0x1387, 0x1388},
                                         {0x1387, 0x1388, 0x500, 0x1389}, {0x1389, 0x2EDF, 0x2000, 0x1388}, {0x800, 0x1000, 0x1B57, 0x7D1},
                                         {0x1000, 0x2000, 0x2EDF, 0x1389}, {0x1000, 0x7D0, 0x1B58, 0x1B57}};
            static const u16 al[8][6] = {{0xFF, 0x100, 0x101, 0, 0x80, 0xFF}, {0xFF, 0x100, 0x101, 0, 0x80, 0x100}, {0x1F, 0x20, 0x21, 0x1FF, 0x200, 0x201},
                                         {0x7F, 0x80, 0x81, 0, 0x40, 0x80}, {0xFF, 0x100, 0x101, 0, 0x80, 0x100}, {0x17F, 0x180, 0x181, 0, 0x100, 0x17F},
                                         {0xFF, 0x100, 0x101, 0, 0x80, 0x100}, {0x17F, 0x180, 0x181, 0, 0x100, 0x180}};
            u8 thr = U32(o + 0x1C4) == 1 ? 2 : 0x20;
            static const u8 rl[8] = {0x10, 0, 0x20, 0x10, 4, 0x20, 0x20, 0x20};
            u8 want = sc == 1 ? thr : rl[sc];
            U32(o + 0x13C) = dl[sc][rnd() % 4];
            u32 e = al[sc][rnd() % 6];
            U16(o + 0x4A) = (u16)((Angle_XZ(o + 0x40, o + 0xD8) + (rnd() % 2 ? e : 0x1000 - e)) & 0xFFF);
            trex_ff(o, sc == 1 || sc == 4 || sc == 6 ? rnd() % 4 == 0 : rnd() % 2);
            U16(0x87DE10) = rand8_seed((u8)(want + rnd() % 3 - 1), rnd() % 2);
            const TRexFn *f = &th[sc < 5 ? 0 : 2];
            if (f->addr == 0x423F70 && rnd() % 4) {         /* walking: the angle after it turns its head (0x2D) at the edge */
                s32 want_a = (s32)al[sc][rnd() % 3] * (rnd() % 2 ? 1 : -1);
                u8 sv2[0x10]; memcpy(sv2, MEM(o + 0x40), 0x10);
                u16 base = U16(o + 0x4A), best = base;
                for (u32 t = 0; t < 0x1000; t++) {
                    U16(o + 0x4A) = (u16)((base + (t & 1 ? t / 2 : 0x1000 - t / 2)) & 0xFFF);
                    u16 h = U16(o + 0x4A);
                    Obj_TurnFromHead(o, 0x2D, o + 0xD8);
                    s32 r2 = (s16)Angle_DiffTo(o + 0x40, o + 0xD8, U16(o + 0x4A));
                    memcpy(MEM(o + 0x40), sv2, 0x10);
                    if (r2 == want_a) { best = h; break; }
                }
                U16(o + 0x4A) = best;
            }
            U32(LOG_BASE) = 0;
            sprintf(g_what, "trex %X %s case %d dist %X angle %X kind %u", o, f->name, sc, U32(o + 0x13C), e, U32(o + 0x1C4));
            check(f->addr, f->fn, f->name, 1, a, f->addr == 0x4243B0 ? 0xFFFFFFFF : 0);
        }
        /* the state handlers */
        game_state();
        o = trex_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
        U8(o + 0x54) = rnd() % 5; U8(o + 0x55) = rnd() % 12; U8(o + 0x56) = rnd() % 6;
        check(0x423BC0, TRex_Normal, "TRex_Normal", 1, a, 0);
        U8(o + 0x55) = rnd() % 4;
        check(0x423BF0, TRex_Hit, "TRex_Hit", 1, a, 0);
        check(0x423C10, TRex_Die, "TRex_Die", 1, a, 0);
        U8(o + 0x55) = rnd() % 12;
        if (rnd() % 2) trex_spheres(o, 1 + rnd() % 3);
        check(0x423C30, TRex_Grab, "TRex_Grab", 1, a, 0);
        for (int k = 0; k < 12; k++) {
            game_state();
            o = trex_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
            U8(o + 0x54) = rnd() % 5; U8(o + 0x55) = (u8[]){0, 3, 4, 8, 9, 0xA, 0xB, 0xC}[rnd() % 8];
            U32(o + 0x1C4) = rnd() % 3 ? rnd() % 2 : 2;
            S16(o + 0x10A) = (s16)(rnd() % 2 ? 100 + rnd() % 0x2000 : rnd() % 30);
            s32 m = S16(o + 0x10A);
            static const s32 dh[] = {-1, 0, 1};
            S16(o + 0x108) = (s16)(rnd() % 2 ? m - m / 10 + dh[rnd() % 3] : (s16)(S16(o + 0x10A) >> 1) + dh[rnd() % 3]);
            U16(o + 0x1FE) = rnd() % 4;
            U8(G() + 0x108F) = rnd() % 13;
            U32(o + 0x50) = (U32(o + 0x50) & ~0x40000030u) | (rnd() & 0x30) | (rnd() % 8 == 0 ? 0x40000000 : 0);
            for (u32 j = 0x1CC; j <= 0x1D4; j += 4) U32(o + j) = rnd() % 2;
            for (u32 j = 0x1F8; j <= 0x202; j += 2) U16(o + j) = rnd() % 3;
            U8(o + 0x208) = rnd() % 2;
            if (rnd() % 2) trex_spheres(o, 1 + rnd() % 3);
            Flag_Set(7, 0x13, rnd() % 4 == 0);
            if (rnd() % 2) { trex_aim(Obj_Part(U32(o + 0x64), U8(o + 0x15F)) + 0x40, o, o + 0xD8); Bits_Set(b, 1, 1); }
            sprintf(g_what, "trex %X handler state %X kind %u", o, U32(o + 0x54), U32(o + 0x1C4));
            check(0x423680, TRex_Handler, "TRex_Handler", 1, a, 0);
        }
        do model_data(0x640000); while (U32(0x640000 + 0x14) != 16);   /* (its feet: parts 0xE and 0xA) */
        do anim_setup_at(AN_OBJ); while (U32(AN_OBJ + 0x68) != 16);
        memcpy(MEM(o + 0x60), MEM(AN_OBJ + 0x60), 0x50);
        U32(0x642C84) = AN_DATA;                            /* its first animation */
        U8(G() + 0x108E) = rnd() % 3; U32(0x74AF04) = rnd(); U8(0x87E747) = rnd() % 2;
        static const u8 rm[][2] = {{9, 3}, {9, 2}, {8, 3}, {1, 1}};
        int r = rnd() % 4; U8(G() + 0x1091) = rm[r][0]; U8(G() + 0x1090) = rm[r][1];
        U8(G() + 0x927) = rnd() % 3 == 0;
        check(0x423970, TRex_Init, "TRex_Init", 1, a, 0);
        /* moving, hit, dying: each step */
        static const TRexFn mv[] = {
            {0x423E40, TRex_Stand, "TRex_Stand", 3}, {0x4240C0, TRex_Walk, "TRex_Walk", 3}, {0x424210, TRex_Roar, "TRex_Roar", 3},
            {0x424300, TRex_Look, "TRex_Look", 3}, {0x4246D0, TRex_Snatch, "TRex_Snatch", 3}, {0x4249B0, TRex_Recoil, "TRex_Recoil", 3},
            {0x424A30, TRex_Burst, "TRex_Burst", 4}, {0x424C80, TRex_Stagger, "TRex_Stagger", 5}, {0x424E70, TRex_Bite, "TRex_Bite", 3},
            {0x424F70, TRex_WalkBite, "TRex_WalkBite", 3}, {0x425080, TRex_Charge, "TRex_Charge", 4}, {0x425190, TRex_Lunge, "TRex_Lunge", 3},
            {0x425740, TRex_Damage, "TRex_Damage", 2}, {0x425CA0, TRex_Flinch, "TRex_Flinch", 3}, {0x425DC0, TRex_FlinchHard, "TRex_FlinchHard", 3},
            {0x425ED0, TRex_Reel, "TRex_Reel", 3}, {0x426010, TRex_Down, "TRex_Down", 5}, {0x4261D0, TRex_Dead, "TRex_Dead", 4}};
        const unsigned nm = sizeof mv / sizeof mv[0];
        game_state(); world_setup(); enemy_state(); zones_setup();
        o = O_OBJ + 0x260 * (1 + rnd() % 6); a[0] = o; b = o + 0x1C0;
        anim_setup_at(O_OBJ);
        do anim_setup_at(o); while (U32(o + 0x68) != 16);
        memcpy(MEM(O_OBJ + 0x60), MEM(o + 0x60), 0x50);     /* (the player: the same parts and animations) */
        U32(G() + 0x940) = pl = O_OBJ;
        U32(pl + 0x128) = rnd() % 2 ? 0x7200E8 : 0; U32(o + 0x128) = 0x7200E8;
        U8(o + 0x58) = 3;
        u8 sv[0x260]; memcpy(sv, MEM(o), 0x260);
        for (unsigned k = 0; k < 14 * nm; k++) {
            const TRexFn *f = &mv[k % nm];
            memcpy(MEM(o), sv, 0x260);
            for (u32 j = 0x1C0; j < 0x210; j += 4) U32(o + j) = rnd();
            U32(o + 0x1C0) = rnd() & 0x1FFF;
            U32(o + 0x1C4) = rnd() % 4 ? rnd() % 3 : rnd() % 2 ? 3 : rnd();
            U8(o + 0x1C8) = 1 + rnd() % 3; U8(o + 0x1C9) = rnd() % 3 ? rnd() % 12 : 6; U8(o + 0x1CA) = rnd() % 4; U8(o + 0x1CB) = rnd() % 2;
            for (u32 j = 0x1CC; j <= 0x1D8; j += 4) U32(o + j) = rnd() % 3 ? rnd() % 3 : rnd() % 2 ? rnd() % 0x2000 : rnd();
            for (u32 j = 0x1E0; j < 0x206; j += 2) U16(o + j) = rnd() % 3 ? rnd() % 3 : (u16)rnd();
            U8(o + 0x207) = rnd() % 2 ? rnd() % 6 : (u8)rnd(); U8(o + 0x208) = rnd() % 3;
            U8(o + 0x54) = rnd() % 4 ? 1 + rnd() % 3 : rnd() % 5;
            U8(o + 0x55) = rnd() % 12;
            U8(o + 0x56) = (u8)(rnd() % 8 ? rnd() % f->steps : rnd());
            U8(o + 0x57) = (u8)(f->addr == 0x424E70 ? rnd() % 3 : rnd() % 2);
            static const u16 fr[] = {0, 4, 5, 9, 0xA, 0xB, 0xD, 0x10, 0x11, 0x12, 0x14, 0x18, 0x1C, 0x1E, 0x1F, 0x28, 0x29, 0x2D, 0x32, 0x38, 0x3C, 0x4B, 0x50, 0x64, 0x73};
            U16(o + 0x92) = rnd() % 4 ? (u16)(fr[rnd() % 25] + rnd() % 3 - 1) : (u16)(rnd() % 0x80);
            trex_ff(o, rnd() % 2);
            U16(o + 0x174) = (u16)rnd();
            U8(pl + 0x54) = (u8[]){1, 2, 5, 3}[rnd() % 4]; U16(pl + 0x174) = rnd() % 2 ? rnd() & 3 : (u16)rnd();
            Flag_Set(2, 0x1A, rnd() % 3 == 0);
            U16(pl + 0xE8) = rnd() % 2 ? 0 : (u16)rnd(); U16(pl + 0xEA) = rnd() % 2 ? 0 : (u16)rnd(); U16(pl + 0xEC) = rnd() % 2 ? 0 : (u16)rnd();
            S16(pl + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x100 - 0x10 : rnd());
            /* the damage */
            U16(o + 0x172) = rnd() % 5; U16(o + 0x176) = rnd() % 2 ? rnd() % 0x400 : (u16)rnd();
            S16(o + 0x10A) = (s16)(rnd() % 2 ? rnd() % 0x2000 : rnd());
            S16(o + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x800 - 0x10 : rnd());
            if (rnd() % 3 == 0) S16(o + 0x108) = (s16)(U16(o + 0x176) + rnd() % 3 - 1);
            if (rnd() % 3 == 0) U32(o + 0x1CC) = (u32)(S16(o + 0x10A) / 2 - (s32)U16(o + 0x176) + (s32)(rnd() % 3) - 1);
            if (rnd() % 3 == 0) U32(o + 0x1D4) = (u32)(S16(o + 0x10A) / 4 * 3 - (s32)U16(o + 0x176) + (s32)(rnd() % 3) - 1);
            U8(o + 0x170) = (u8)rnd(); U8(o + 0x184) = (u8)rnd(); U8(o + 0x17D) = rnd() % 2 ? 4 : (u8)rnd();
            if (f->addr == 0x425740) {
                if (rnd() % 2) U8(o + 0x56) = 0;
                if (rnd() % 2) U8(o + 0x1C8) = 2;
                if (rnd() % 2) U8(o + 0x55) = 1;
                U8(o + 0x58) = (u8)(rnd() % 16); U8(G() + 0x757) = 0;
            }
            U8(G() + 0x108E) = rnd() % 3; U8(G() + 0x108F) = rnd() % 2 ? rnd() % 12 : (u8)rnd();
            for (u32 j = 0; j < 20; j++) if (rnd() % 2) U8(O_K2 + j * 0xAC + 0x50) &= ~1;
            if (rnd() % 2) { U16(o + 0xF0) = (u16)(rnd() % 0x600 - 0x300); U16(o + 0xF2) = (u16)(rnd() % 0x600 - 0x300); U16(o + 0xF4) = (u16)(rnd() % 0x300); }
            U16(0x87DC12) = rnd() % 2 ? 0 : (u16)rnd();     /* (Pad_Shake) */
            if (f->addr == 0x424A30 && rnd() % 2) {         /* the break through: about its corner */
                S16(o + 0x40) = (s16)(0xBB8 + (s32)(rnd() % 3) - 1 + (rnd() % 2 ? 0 : 0x800));
                S16(o + 0x44) = (s16)((s16)0xAA10 + (s32)(rnd() % 3) - 1 - (rnd() % 2 ? 0 : 0x800));
                S16(o + 0x42) = (s16)(rnd() % 2 ? (s32)(rnd() % 3) - 1 : (s32)(rnd() % 0x800) - 0x400);
            }
            if (f->addr == 0x425080 && rnd() % 2) U8(o + 0x56) = 2;
            if (f->addr == 0x424C80) U16(o + 0x204) = rnd() % 3;
            trex_rand8();
            anim_maybe_end(o);
            if (rnd() % 2) U16(o + 0x102) |= 0x20;
            static const struct { u32 addr; u8 step, n; u16 fr[8]; } edge[] = {
                {0x4246D0, 1, 6, {0x37, 0x38, 0x39, 0x4F, 0x50, 0x51}}, {0x425CA0, 1, 6, {0x17, 0x18, 9, 0xA, 4, 5}},
                {0x425DC0, 1, 2, {0x28, 0x29}}, {0x425ED0, 1, 6, {0x31, 0x32, 13, 27, 100, 115}}, {0x425080, 2, 3, {0xE, 0xF, 0x10}},
                {0x424E70, 1, 2, {0x4A, 0x4B}}, {0x426010, 1, 2, {0x4F, 0x50}}, {0x4261D0, 1, 2, {0x4F, 0x50}},
                {0x424C80, 1, 2, {0xF, 0x10}}, {0x424A30, 2, 4, {0x1C, 0x1D, 0x1F, 0x20}}, {0x424A30, 1, 4, {0xA, 0xB, 0x28, 0x29}}};
            for (unsigned e = 0; e < sizeof edge / sizeof edge[0]; e++)
                if (edge[e].addr == f->addr && rnd() % 2) {   /* (held at the frame: Anim_Step leaves it) */
                    U8(o + 0x56) = edge[e].step; U16(o + 0x92) = edge[e].fr[rnd() % edge[e].n]; U16(o + 0x102) |= 0x20;
                }
            if (f->addr == 0x4246D0 && rnd() % 2) {         /* snatching: the player free to be held or not */
                U8(pl + 0x54) = (u8[]){1, 3, 2, 5}[rnd() % 4]; U16(pl + 0x174) = (u16)(rnd() % 4); Flag_Set(2, 0x1A, rnd() % 4 == 0);
            }
            if (f->addr == 0x424A30 && rnd() % 2) {         /* the break through: close to the point it turns to */
                U8(o + 0x56) = 2; S16(o + 0x40) = (s16)(0x1770 + (s32)(rnd() % 7) - 3); S16(o + 0x44) = (s16)((s16)0xC568 + (s32)(rnd() % 7) - 3);
            }
            if (f->addr == 0x425740 && rnd() % 3 == 0) {    /* the damage leaves exactly 0 or about it */
                U8(o + 0x1C8) = 1 + rnd() % 3; U8(o + 0x56) = 1;
                S16(o + 0x108) = (s16)((s32)U16(o + 0x176) / 5 + (s32)(rnd() % 3) - 1);
            }
            U32(LOG_BASE) = 0;
            sprintf(g_what, "trex %X %s step %u side %u frame %X kind %u", o, f->name, U8(o + 0x56), U8(o + 0x57), U16(o + 0x92), U32(o + 0x1C4));
            check(f->addr, f->fn, f->name, 1, a, 0);
        }
        /* the attacks land */
        static const TRexFn gh[] = {{0x425240, TRex_BiteHit, "TRex_BiteHit"}, {0x425300, TRex_WalkBiteHit, "TRex_WalkBiteHit"},
                                    {0x425690, TRex_LungeHit, "TRex_LungeHit"}, {0x4253B0, TRex_ChargeHit, "TRex_ChargeHit"}};
        for (int k = 0; k < 32; k++) {
            memcpy(MEM(o), sv, 0x260);
            U8(o + 0x56) = rnd() % 4 ? 1 : (u8)(rnd() % 3);
            U8(o + 0x57) = (u8)(k % 4 == 0 ? rnd() % 3 : rnd() % 2);
            U32(o + 0x1C0) = rnd() & 0x1FFF;
            U8(G() + 0x108E) = rnd() % 3; U8(G() + 0x108F) = rnd() % 5;
            Flag_Set(2, 0x1A, rnd() % 3 == 0);
            U16(pl + 0x4A) = (u16)((U16(o + 0x4A) + (u16[]){0, 0x3FF, 0x400, 0x401, 0xC01, 0xC00, 0xBFF, 0x800}[rnd() % 8]) & 0xFFF);
            S16(pl + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x100 - 0x10 : rnd());
            for (u32 j = 0; j < 20; j++) if (rnd() % 2) U8(O_K2 + j * 0xAC + 0x50) &= ~1;
            U32(LOG_BASE) = 0;
            sprintf(g_what, "trex %X %s step %u side %u", o, gh[k % 4].name, U8(o + 0x56), U8(o + 0x57));
            check(gh[k % 4].addr, gh[k % 4].fn, gh[k % 4].name, 1, a, 0);
        }
        for (int k = 0; k < 24; k++) {                      /* the player in its jaws */
            memcpy(MEM(o), sv, 0x260);
            anim_maybe_end(pl);
            U32(pl + 0x1D8) = o;
            U8(pl + 0x57) = rnd() % 3 ? (rnd() % 3 ? 0 : 1) : (u8)rnd();
            U16(U32(pl + 0x64) + 0x4A) = (u16)rnd(); U16(pl + 0x4A) = (u16)rnd();
            U32(o + 0x1C0) = rnd() & 0xF;
            U8(o + 0x57) = rnd() % 2;
            U16(pl + 0x92) = (u16)(rnd() % 2 ? 0x14 + rnd() % 3 - 1 : rnd() % 0x40);
            if (rnd() % 2) U16(pl + 0x102) |= 0x20;
            u32 p1[1] = {pl};
            U32(LOG_BASE) = 0;
            sprintf(g_what, "trex %X player step %u bits %X", o, U8(pl + 0x57), U32(o + 0x1C0));
            check(0x4248A0, TRex_PlSnatched, "TRex_PlSnatched", 1, p1, 0);
            anim_maybe_end(pl);
            if (rnd() % 3) { U8(pl + 0x57) = 0; U16(pl + 0x102) &= ~0x20; U16(pl + 0x94) = (u16)(U16(pl + 0x90) - 1); U16(pl + 0x98) = 0x7000; }
            U16(U32(pl + 0x64) + 0x4A) = (u16)rnd(); U16(pl + 0x4A) = (u16)rnd();
            check(0x4255D0, TRex_PlBitten, "TRex_PlBitten", 1, p1, 0);
        }
    }
    player_tables_stub(0);
    trex_tables_stub(0);
    stubs_install(0);
    g_ign_objpads = 0;
}

/* ---- the plesiosaurus ---- */
static const u32 g_ps_tabs[][2] = {{0x720838, 5}, {0x72084C, 6}, {0x720864, 6}, {0x72087C, 4}, {0x72088C, 4}, {0x72089C, 6}};
static u32 g_ps_saved[6][6];
static void plesio_tables_stub(int on)
{
    for (int k = 1; k <= 6; k++) g_stubs[k].spec = on ? "d" : "";
    for (int t = 0; t < 6; t++)
        for (u32 k = 0; k < g_ps_tabs[t][1]; k++) {
            gptr a = g_ps_tabs[t][0] + 4 * k;
            if (on) { g_ps_saved[t][k] = U32(a); U32(a) = (u32)(uintptr_t)g_recs[1 + k]; }
            else U32(a) = g_ps_saved[t][k];
        }
}

/* the bits: each of 2..7 now and then, 7 rarely */
static u32 plesio_bits(void)
{
    u32 v = rnd() % 4 ? rnd() & 0x7E : rnd();
    if (rnd() % 4) v &= ~0x80u;
    return v;
}

/* a plesiosaurus among the test objects; the player its target */
static gptr plesio_setup(void)
{
    gptr o = raptor_setup(), pl = U32(G() + 0x940);
    U32(o + 0x184) &= ~0x40u;
    trex_parts(o);
    U32(o + 0x1C0) = plesio_bits();
    U8(o + 0x1C8) = 1 + rnd() % 3; U8(o + 0x1C9) = rnd() % 6; U8(o + 0x1CA) = rnd() % 3; U8(o + 0x1CB) = rnd() % 2;
    for (u32 k = 0x1CC; k <= 0x1D0; k += 4) U32(o + k) = rnd() % 3 ? rnd() % 3 : rnd() % 2 ? rnd() % 0x2000 : rnd();
    for (u32 k = 0x1E8; k < 0x1F8; k += 2) U16(o + k) = rnd() % 3 ? (u16)(rnd() % 0x400 - 0x200) : (u16)rnd();
    for (u32 k = 0x1F0; k < 0x1F6; k += 2) U16(o + k) = rnd() % 3 ? rnd() % 3 : (u16)rnd();
    U32(o + 0x1F8) = rnd();
    U8(o + 0x57) = rnd() % 2;
    U16(pl + 0x174) = rnd() % 2 ? rnd() & 3 : (u16)rnd();
    U16(pl + 0x1FA) = (u16)(rnd() % 2 ? 0x7F + rnd() % 3 : rnd());
    U32(pl + 0x134) = 0;                                /* (no hit spheres: Enemy_AttackHit keeps +0x119) */
    Bits_Set(0x87E974, 0, rnd() % 3 == 0);
    U8(G() + 0x7C4) = rnd() % 2 ? (u8[]){0, 0xD, 0xE, 0xF, 0x10, 0x11}[rnd() % 6] : (u8)rnd();
    S16(pl + 0x42) = (s16)(rnd() % 2 ? -5000 + (s32)(rnd() % 3) - 1 : (s32)(rnd() % 20000) - 15000);
    trex_ff(o, rnd() % 2);
    trex_rand8();
    return o;
}

/* p (x/z) at distance d from c, by angle t */
static void plesio_at(gptr p, gptr c, s32 d, u32 t)
{
    gptr v = Tmp_Push(8);
    U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = (u16)d;
    Vec_RotY(v, v, t & 0xFFF);
    U16(p) = (u16)(U16(c) + U16(v)); U16(p + 4) = (u16)(U16(c + 4) + U16(v + 4));
    Tmp_Pop(8);
}

static void test_plesio(void)
{
    g_ign_objpads = 1;
    stubs_install(1);
    plesio_tables_stub(1);
    player_tables_stub(1);
    for (int i = 0; i < iters; i++) {
        game_state();
        gptr o = plesio_setup(), b = o + 0x1C0, pl = U32(G() + 0x940);
        u32 a[1] = {o};
        /* the wait */
        U8(pl + 0x54) = (u8[]){2, 5, 1, 3}[rnd() % 4]; U16(pl + 0x174) = (u16)rnd(); U8(G() + 0x108E) = rnd() % 3;
        check(0x42CCA0, Plesio_Wait, "Plesio_Wait", 1, a, 0xFFFFFFFF);
        /* swimming about: diving away, out and back, circling */
        for (int k = 0; k < 12; k++) {
            game_state();
            o = plesio_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
            int sc = k % 4;
            if (sc == 0) {                                  /* dives away */
                U32(b) |= 0x80;
                S16(o + 0x42) = (s16)(rnd() % 2 ? -29900 + (s32)(rnd() % 3) - 1 : (s32)(rnd() % 60000) - 30000);
                S16(o + 0x44) = (s16)(rnd() % 2 ? -19900 + (s32)(rnd() % 3) - 1 : (s32)(rnd() % 60000) - 30000);
                U8(G() + 0x7C4) = rnd() % 2 ? 0x10 : (u8)rnd();
            } else if (sc == 1) {                           /* back out to its place */
                U32(b) = (U32(b) & ~0x80u) | 0x28;
                static const s32 dd[] = {2999, 3000, 3001, 1000, 6000};
                gptr t = Tmp_Push(8);
                U16(t) = 0x2AF8; U16(t + 4) = 0x16A8;
                plesio_at(o + 0x40, t, dd[rnd() % 5], rnd() % 4 ? (u32[]){0, 0x400, 0x800, 0xC00}[rnd() % 4] : rnd());
                Tmp_Pop(8);
                U16(o + 0x4A) = (u16)rnd(); U16(o + 0xF4) = (u16)(rnd() % 0x200);
                if (rnd() % 2) U8(G() + 0x7C4) = (u8[]){0xD, 0xE, 0xF, 0x10, 0x11}[rnd() % 5];
                U32(b) = (U32(b) & ~0x40u) | (rnd() % 2 ? 0x40 : 0);
                Flag_Set(3, 0xA, rnd() % 2);                /* (Pos_Hidden) */
            } else if (sc == 2) {                           /* up and at the player */
                U32(b) = (U32(b) & ~0xA0u) | 8;
                static const s32 dd[] = {11999, 12000, 12001, 12499, 12500, 12501, 2000, 20000};
                plesio_at(o + 0x40, pl + 0x40, dd[rnd() % 8], rnd() % 4 ? (u32[]){0, 0x400, 0x800, 0xC00}[rnd() % 4] : rnd());
                S16(o + 0x44) = (s16)(rnd() % 2 ? S16(o + 0x44) : 6000 + (s32)(rnd() % 3) - 1);
                U16(o + 0xF4) = (u16)(rnd() % 0x300 - 0x180);
                S16(o + 0x42) = (s16)(rnd() % 2 ? -7000 + (s32)(rnd() % 201) - 100 : (s32)(rnd() % 20000) - 10000);
                if (rnd() % 2) U8(G() + 0x7C4) = (u8[]){0xD, 0xE, 0xF, 0x10, 0x11}[rnd() % 5];
            } else {                                        /* circling: the angle to the player at its edges */
                U32(b) &= ~0x88u;
                S16(o + 0x1EA) = (s16)(rnd() % 2 ? (s32)(rnd() % 0xE0) - 0x70 : (s32)(rnd() % 0x400) - 0x200);
                S16(o + 0x1EC) = (s16)(rnd() % 2 ? (s32)(rnd() % 0x840) - 0x420 : (s32)rnd());
                S16(o + 0x1EE) = (s16)(rnd() % 2 ? 0x1A0 * (rnd() % 2 ? 1 : -1) + (s32)(rnd() % 17) - 8 : (s32)(rnd() % 0x400) - 0x200);
                U16(o + 0x1F0) = rnd() % 3 == 0;
                u8 sv[0x260]; memcpy(sv, MEM(o), 0x260);
                u8 svp[0x10]; memcpy(svp, MEM(pl + 0x40), 0x10);
                Plesio_Swim(o);                             /* (where it gets to, and its heading's base) */
                u16 px = U16(o + 0x40), pz = U16(o + 0x44), base = (u16)(U16(o + 0x1EC) + U16(o + 0x1E8));
                memcpy(MEM(o), sv, 0x260); memcpy(MEM(pl + 0x40), svp, 0x10);
                static const s32 ae[] = {0x33F, 0x340, 0x341, 0, 0x100};
                s32 want = ae[rnd() % 5] * (rnd() % 2 ? 1 : -1);
                if (rnd() % 3 == 0) {                       /* its head's turn to (about) the clamp */
                    U16(o + 0x1F0) = 0;
                    want = (0x1A0 + (s32)(rnd() % 3) - 1) * (rnd() % 2 ? 1 : -1);
                    S16(o + 0x1EE) = (s16)(want + (s32)(rnd() % 9) - 4);
                }
                gptr c = Tmp_Push(8); U16(c) = px; U16(c + 4) = pz;
                s32 r = 2000 + rnd() % 20000;
                for (u32 t = 0; t < 0x1000; t++) {
                    plesio_at(pl + 0x40, c, r, t);
                    if ((s16)Angle_DiffTo(c, pl + 0x40, base) == want) break;
                }
                Tmp_Pop(8);
                S16(pl + 0x42) = (s16)(S16(o + 0x42) + (rnd() % 2 ? 2000 : 3000) + (s32)(rnd() % 3) - 1);
                if (rnd() % 3 == 0) S16(pl + 0x42) = (s16)rnd();
            }
            U32(LOG_BASE) = 0;
            sprintf(g_what, "plesio %X swim case %d bits %X event %X", o, sc, U32(b), U8(G() + 0x7C4));
            check(0x42BFF0, Plesio_Swim, "Plesio_Swim", 1, a, 0);
        }
        /* the attacks, and turning about */
        for (int k = 0; k < 12; k++) {
            game_state();
            o = plesio_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
            if (rnd() % 4) { Bits_Set(0x87E974, 0, 0); U32(o + 0x1D0) = 0; U32(b) &= ~0x80u; }
            static const s32 dd[] = {15999, 16000, 16001, 17999, 18000, 18001, 8000};
            s32 d = dd[rnd() % 7];
            plesio_at(pl + 0x40, o + 0x40, d, rnd() % 4 ? 0 : rnd());
            trex_aim(o + 0x40, o, pl + 0x40);
            u32 d2 = Dist2_XZ(o + 0x40, pl + 0x40);
            U32(o + 0x1F8) = rnd() % 2 ? d2 + (u32)(rnd() % 3) - 1 : rnd();
            U16(o + 0x1F2) = rnd() % 3 == 0;
            S16(pl + 0x42) = (s16)(rnd() % 2 ? -5000 + (s32)(rnd() % 3) - 1 : (s32)(rnd() % 20000) - 15000);
            U8(G() + 0x7C4) = 0;
            trex_rand8();
            U32(LOG_BASE) = 0;
            static const TRexFn th[] = {{0x42BE70, Plesio_Decide, "Plesio_Decide"}, {0x42BC40, Plesio_ThinkSwim, "Plesio_ThinkSwim"},
                                        {0x42BDB0, Plesio_ThinkTurn, "Plesio_ThinkTurn"}, {0x42C400, Plesio_ThinkBite, "Plesio_ThinkBite"},
                                        {0x42C4A0, Plesio_ThinkLunge, "Plesio_ThinkLunge"}, {0x42C590, Plesio_ThinkSide, "Plesio_ThinkSide"},
                                        {0x42C680, Plesio_ThinkRise, "Plesio_ThinkRise"}};
            const TRexFn *f = &th[k < 8 ? k % 2 : k % 7];
            if (k % 4 == 1 && rnd() % 2) {                  /* swimming first: the player about 18000 from where it gets to */
                U32(b) &= ~0x88u;
                u8 sv[0x260]; memcpy(sv, MEM(o), 0x260);
                Plesio_Swim(o);
                gptr c = Tmp_Push(8); U16(c) = U16(o + 0x40); U16(c + 4) = U16(o + 0x44);
                memcpy(MEM(o), sv, 0x260);
                plesio_at(pl + 0x40, c, dd[3 + rnd() % 3], 0);
                d2 = Dist2_XZ(c, pl + 0x40);
                Tmp_Pop(8);
                U32(o + 0x1F8) = d2 + (u32)(rnd() % 3) - 1;
                U16(o + 0x1F2) = rnd() % 4 == 0;
            }
            if (k < 8 && k % 2 == 0) {                      /* deciding: each attack at its edges */
                U32(b) = (U32(b) & ~0x88u) | (rnd() % 2 ? 8 : 0);
                U32(o + 0x1D0) = rnd() % 6 == 0; Bits_Set(0x87E974, 0, rnd() % 6 == 0);
                plesio_at(pl + 0x40, o + 0x40, rnd() % 4 ? (s32[]){15999, 16000, 8000, 300}[rnd() % 4] : 16001, 0);
                static const u16 ea[] = {0, 1, 0xFF, 0x100, 0x101, 0x1FF, 0x200, 0x201};
                u32 e = rnd() % 3 ? ea[rnd() % 8] : 0;
                U16(o + 0x4A) = (u16)((Angle_XZ(o + 0x40, pl + 0x40) + (rnd() % 2 ? e : 0x1000 - e)) & 0xFFF);
                S16(pl + 0x42) = (s16)(rnd() % 2 ? -5000 + (s32)(rnd() % 3) - 1 : (s32)(rnd() % 20000) - 15000);
            }
            sprintf(g_what, "plesio %X %s bits %X dist %d", o, f->name, U32(b), d);
            check(f->addr, f->fn, f->name, 1, a, f->addr == 0x42BE70 ? 0xFFFFFFFF : 0);
        }
        /* the state handlers */
        game_state();
        o = plesio_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
        U8(o + 0x54) = rnd() % 5; U8(o + 0x55) = rnd() % 6; U8(o + 0x56) = rnd() % 3;
        check(0x42BBA0, Plesio_Normal, "Plesio_Normal", 1, a, 0);
        U8(o + 0x55) = rnd() % 4;
        check(0x42BBD0, Plesio_Hit, "Plesio_Hit", 1, a, 0);
        check(0x42BBF0, Plesio_Die, "Plesio_Die", 1, a, 0);
        for (int k = 0; k < 3; k++) {
            U8(o + 0x55) = rnd() % 6;
            U8(o + 0x119) = (u8[]){0, 1, 0xFF, 0x7F, 0x80}[rnd() % 5];
            U16(pl + 0x174) &= ~1; U8(pl + 0x17B) = 0; U32(o + 0x184) &= ~0x40u;   /* (Enemy_AttackHit keeps +0x119) */
            U32(o + 0xA0) = TR_ATK; U16(o + 0x102) &= ~2;
            check(0x42BC10, Plesio_Grab, "Plesio_Grab", 1, a, 0);
        }
        for (int k = 0; k < 6; k++) {
            game_state();
            o = plesio_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
            U8(o + 0x54) = rnd() % 5; U8(o + 0x55) = rnd() % 6;
            if (rnd() % 2) {                                /* the parts about 20000 from the player */
                for (u32 p = U32(o + 0x64); p; p = U32(p + 0x54))
                    if (rnd() % 2) plesio_at(p + 0x40, pl + 0x40, 20000 + (s32)(rnd() % 3) - 1, (u32[]){0, 0x400, 0x800, 0xC00}[rnd() % 4]);
            }
            U8(G() + 0x7C4) = rnd() % 2 ? 0xF : (u8)rnd();
            U8(G() + 0x108F) = rnd() % 13;
            U32(o + 0x50) = (U32(o + 0x50) & ~0x40000030u) | (rnd() & 0x30) | (rnd() % 8 == 0 ? 0x40000000 : 0);
            S16(o + 0x42) = (s16)(rnd() % 2 ? (s32)(rnd() % 20000) - 15000 : (s32)rnd());
            for (u32 j = 0x1F0; j < 0x1F6; j += 2) U16(o + j) = rnd() % 3;
            U32(o + 0x1CC) = rnd() % 2;
            if (rnd() % 2) trex_spheres(o, 1 + rnd() % 3);
            if (rnd() % 2) {                                /* still: its height about the floor's less 5000 */
                U16(o + 0xF0) = 0; U16(o + 0xF2) = 0; U16(o + 0xF4) = 0;
                for (int j = 0; j < 4; j++) S16(o + 0x42) = (s16)((s16)Floor_Height(o + 0x40, o + 0x40, 0x40, 0) - 5000);
                S16(o + 0x42) = (s16)(S16(o + 0x42) + (s32)(rnd() % 3) - 1);
            }
            sprintf(g_what, "plesio %X handler state %X bits %X", o, U32(o + 0x54), U32(b));
            check(0x42B750, Plesio_Handler, "Plesio_Handler", 1, a, 0);
        }
        do model_data(0x640000); while (U32(0x640000 + 0x14) != 16);
        do anim_setup_at(AN_OBJ); while (U32(AN_OBJ + 0x68) != 16);
        memcpy(MEM(o + 0x60), MEM(AN_OBJ + 0x60), 0x50);
        U32(0x642984) = AN_DATA;                            /* its first animation */
        U8(G() + 0x108E) = rnd() % 3; U32(0x74AF14) = rnd(); U8(0x87E747) = rnd() % 2;
        U8(0x87EEC9) = rnd() % 2 ? 8 : (u8)rnd(); U8(0x87EEC8) = rnd() % 2 ? 0 : (u8)rnd();
        check(0x42B9A0, Plesio_Init, "Plesio_Init", 1, a, 0);
        /* moving, hit, dying: each step */
        static const TRexFn mv[] = {
            {0x42BD10, Plesio_Swimming, "Plesio_Swimming", 2}, {0x42BDD0, Plesio_Turn, "Plesio_Turn", 2},
            {0x42C410, Plesio_Bite, "Plesio_Bite", 2}, {0x42C4B0, Plesio_Lunge, "Plesio_Lunge", 2},
            {0x42C5A0, Plesio_Side, "Plesio_Side", 2}, {0x42C690, Plesio_Rise, "Plesio_Rise", 2},
            {0x42C890, Plesio_Damage, "Plesio_Damage", 1}, {0x42CAA0, Plesio_Flinch, "Plesio_Flinch", 2},
            {0x42CB00, Plesio_Flinch2, "Plesio_Flinch2", 2}, {0x42CB60, Plesio_Flinch3, "Plesio_Flinch3", 2},
            {0x42CBC0, Plesio_Dead, "Plesio_Dead", 3}};
        const unsigned nm = sizeof mv / sizeof mv[0];
        game_state(); world_setup(); enemy_state(); zones_setup();
        o = O_OBJ + 0x260 * (1 + rnd() % 6); a[0] = o; b = o + 0x1C0;
        anim_setup_at(O_OBJ);
        do anim_setup_at(o); while (U32(o + 0x68) != 16);
        memcpy(MEM(O_OBJ + 0x60), MEM(o + 0x60), 0x50);
        U32(G() + 0x940) = pl = O_OBJ;
        U32(pl + 0x128) = rnd() % 2 ? 0x7200E8 : 0; U32(o + 0x128) = 0x7207F8;
        U8(o + 0x58) = 5;
        u8 sv[0x260]; memcpy(sv, MEM(o), 0x260);
        for (unsigned k = 0; k < 12 * nm; k++) {
            const TRexFn *f = &mv[k % nm];
            memcpy(MEM(o), sv, 0x260);
            for (u32 j = 0x1C0; j < 0x200; j += 4) U32(o + j) = rnd();
            U32(b) = plesio_bits();
            U8(o + 0x1C8) = 1 + rnd() % 3; U8(o + 0x1C9) = rnd() % 6; U8(o + 0x1CA) = rnd() % 3; U8(o + 0x1CB) = rnd() % 2;
            for (u32 j = 0x1CC; j <= 0x1D0; j += 4) U32(o + j) = rnd() % 3 ? rnd() % 3 : rnd() % 2 ? rnd() % 0x2000 : rnd();
            for (u32 j = 0x1E8; j < 0x1F8; j += 2) U16(o + j) = rnd() % 3 ? (u16)(rnd() % 0x400 - 0x200) : (u16)rnd();
            for (u32 j = 0x1F0; j < 0x1F6; j += 2) U16(o + j) = rnd() % 3 ? rnd() % 3 : (u16)rnd();
            U8(o + 0x54) = rnd() % 4 ? 1 + rnd() % 3 : rnd() % 5;
            U8(o + 0x55) = rnd() % 6;
            U8(o + 0x56) = (u8)(rnd() % 8 ? rnd() % f->steps : rnd());
            U8(o + 0x57) = rnd() % 2;
            static const u16 fr[] = {0, 0x34, 0x35, 0x4F, 0x5A, 0x60, 0x64, 0x10, 0x20};
            U16(o + 0x92) = rnd() % 4 ? (u16)(fr[rnd() % 9] + rnd() % 3 - 1) : (u16)(rnd() % 0x80);
            trex_ff(o, rnd() % 2);
            U8(pl + 0x54) = (u8[]){1, 2, 5, 3}[rnd() % 4]; U16(pl + 0x174) = rnd() % 2 ? rnd() & 3 : (u16)rnd();
            /* the damage */
            U16(o + 0x172) = rnd() % 6; U16(o + 0x176) = rnd() % 2 ? rnd() % 0x400 : (u16)rnd();
            S16(o + 0x10A) = (s16)(rnd() % 2 ? rnd() % 0x2000 : rnd());
            S16(o + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x800 - 0x10 : rnd());
            if (rnd() % 3 == 0) S16(o + 0x108) = (s16)(U16(o + 0x176) + rnd() % 3 - 1);
            if (rnd() % 3 == 0) S16(o + 0x108) = (s16)((s32)U16(o + 0x176) / 5 + (s32)(rnd() % 3) - 1);
            if (rnd() % 3 == 0) U32(o + 0x1CC) = (u32)(S16(o + 0x10A) / 5 - (s32)U16(o + 0x176) + (s32)(rnd() % 3) - 1);
            U8(o + 0x184) = (u8)rnd(); U8(o + 0x17D) = rnd() % 2 ? 0x12 : (u8)rnd();
            if (f->addr == 0x42C890) {
                if (rnd() % 2) U8(o + 0x56) = 0;
                if (rnd() % 2) U8(o + 0x1C8) = 2;
                if (rnd() % 2) U16(o + 0x172) = 3 + rnd() % 2;
                U8(o + 0x58) = (u8)(rnd() % 16); U8(G() + 0x757) = 0;
            }
            if (f->addr == 0x42CBC0) {                      /* sinking: about the bottom, near 0 */
                S16(o + 0x42) = (s16)(rnd() % 2 ? -19970 + (s32)(rnd() % 3) - 1 : (s32)(rnd() % 40000) - 20000);
                S16(o + 0x44) = (s16)(rnd() % 2 ? 0x50 + (s32)(rnd() % 3) - 1 : (s32)(rnd() % 40000) - 20000);
            }
            U8(G() + 0x108E) = rnd() % 3; U8(G() + 0x108F) = rnd() % 2 ? rnd() % 12 : (u8)rnd();
            for (u32 j = 0; j < 20; j++) if (rnd() % 2) U8(O_K2 + j * 0xAC + 0x50) &= ~1;
            if (rnd() % 2) { U16(o + 0xF0) = (u16)(rnd() % 0x600 - 0x300); U16(o + 0xF2) = (u16)(rnd() % 0x600 - 0x300); U16(o + 0xF4) = (u16)(rnd() % 0x300); }
            trex_rand8();
            anim_maybe_end(o);
            if (rnd() % 2) U16(o + 0x102) |= 0x20;
            static const struct { u32 addr; u16 fr[3]; } edge[] = {
                {0x42BDD0, {0x34, 0x35, 0x36}}, {0x42C410, {0x4E, 0x4F, 0x50}}, {0x42C4B0, {0x59, 0x5A, 0x5B}},
                {0x42C5A0, {0x5F, 0x60, 0x61}}, {0x42C690, {0x63, 0x64, 0x65}}};
            for (unsigned e = 0; e < sizeof edge / sizeof edge[0]; e++)
                if (edge[e].addr == f->addr && rnd() % 2) {   /* (held at the frame: Anim_Step leaves it) */
                    U8(o + 0x56) = 1; U16(o + 0x92) = edge[e].fr[rnd() % 3]; U16(o + 0x102) |= 0x20;
                }
            U32(LOG_BASE) = 0;
            sprintf(g_what, "plesio %X %s step %u side %u frame %X", o, f->name, U8(o + 0x56), U8(o + 0x57), U16(o + 0x92));
            check(f->addr, f->fn, f->name, 1, a, 0);
        }
        /* the attacks land */
        static const TRexFn gh[] = {{0x42C750, Plesio_BiteHit, "Plesio_BiteHit"}, {0x42C7C0, Plesio_LungeHit, "Plesio_LungeHit"},
                                    {0x42C830, Plesio_RiseHit, "Plesio_RiseHit"}};
        for (int k = 0; k < 18; k++) {
            memcpy(MEM(o), sv, 0x260);
            U8(o + 0x56) = rnd() % 4 ? 1 : (u8)(rnd() % 3);
            U8(G() + 0x108E) = rnd() % 3; U8(G() + 0x108F) = rnd() % 5;
            Flag_Set(2, 0x1A, rnd() % 3 == 0);
            U16(pl + 0x4A) = (u16)((U16(o + 0x4A) + (u16[]){0, 0x3FF, 0x400, 0x401, 0xC01, 0xC00, 0xBFF, 0x800}[rnd() % 8]) & 0xFFF);
            S16(pl + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x100 - 0x10 : rnd());
            for (u32 j = 0; j < 20; j++) if (rnd() % 2) U8(O_K2 + j * 0xAC + 0x50) &= ~1;
            U32(LOG_BASE) = 0;
            sprintf(g_what, "plesio %X %s step %u", o, gh[k % 3].name, U8(o + 0x56));
            check(gh[k % 3].addr, gh[k % 3].fn, gh[k % 3].name, 1, a, 0);
        }
    }
    player_tables_stub(0);
    plesio_tables_stub(0);
    stubs_install(0);
    g_ign_objpads = 0;
}

/* ---- the plesiosaurs in the water (types 11 and 12) ---- */
static const u32 g_pw_tabs[2][6][2] = {
    {{0x720900, 5}, {0x720984, 13}, {0x7209B8, 13}, {0x7209EC, 4}, {0x7209FC, 4}, {0x720A0C, 13}},
    {{0x720A40, 5}, {0x720AC4, 13}, {0x720AF8, 13}, {0x720B2C, 4}, {0x720B3C, 4}, {0x720B4C, 13}}};
static u32 g_pw_saved[2][6][13];
static void pw_tables_stub(int on)
{
    for (int k = 1; k <= 13; k++) g_stubs[k].spec = on ? "d" : "";
    for (int y = 0; y < 2; y++)
        for (int t = 0; t < 6; t++)
            for (u32 k = 0; k < g_pw_tabs[y][t][1]; k++) {
                gptr a = g_pw_tabs[y][t][0] + 4 * k;
                if (on) { g_pw_saved[y][t][k] = U32(a); U32(a) = (u32)(uintptr_t)g_recs[1 + (k + 3 * t) % 13]; }   /* (each table its own order) */
                else U32(a) = g_pw_saved[y][t][k];
            }
}

#define PW_QUAD (TR_QUAD + 0x180)
#define PW_TMP  (TR_QUAD + 0x1C0)
static const u16 g_pw_ed[] = {0, 1, 2, 3, 4, 5, 6, 7, 0x40, 0x45, 0x46, 0x47, 0x5E, 0x5F, 0x60, 0x63, 0x64, 0x65, 0xFFFF, 0xFFFE};

/* a glow (effect slot) or none */
static gptr pw_slot(u32 j)
{
    gptr k = O_K2 + 0xAC * j;
    U16(k + 0x6C) = rnd() % 3 ? g_pw_ed[rnd() % 20] : (u16)rnd();
    U16(k + 0x6E) = rnd() % 3 ? g_pw_ed[rnd() % 20] : (u16)rnd();
    return k;
}

/* the room's water quad around o, or not */
static void pw_quad(gptr o)
{
    s32 cx = S16(o + 0x40), cz = S16(o + 0x44), r = 500 + rnd() % 4000;
    if (rnd() % 3 == 0) cx += (rnd() % 2 ? 1 : -1) * (r + 1 + (s32)(rnd() % 3000));
    S16(PW_QUAD + 0) = (s16)(cx - r); S16(PW_QUAD + 2) = (s16)(cz + r); S16(PW_QUAD + 4) = (s16)(cx + r); S16(PW_QUAD + 6) = (s16)(cz + r);
    S16(PW_QUAD + 8) = (s16)(cx + r); S16(PW_QUAD + 10) = (s16)(cz - r); S16(PW_QUAD + 12) = (s16)(cx - r); S16(PW_QUAD + 14) = (s16)(cz - r);
    U32(G() + 0x7BC) = PW_QUAD - 0x1C;
}

/* the others: live or not, near (about 4000) or not, diving or not */
static void pw_others(gptr o)
{
    gptr pl = U32(G() + 0x940);
    U32(0x87F940) = rnd() % 4 ? 16 : rnd() % 17;
    for (u32 k = 1; k < 16; k++) {
        gptr q = O_OBJ + k * 0x260;
        if (q == o || q == pl) continue;
        U8(q + 0x50) = (u8)((U8(q + 0x50) & ~1) | (rnd() % 5 == 0));
        U8(q + 0x54) = rnd() % 2 ? 1 : (u8)(rnd() % 4); U8(q + 0x55) = rnd() % 2 ? 4 : (u8)(rnd() % 13);
        if (rnd() % 3 == 0) plesio_at(q + 0x40, o + 0x40, rnd() % 2 ? 4000 + (s32)(rnd() % 3) - 1 : 1000 + (s32)(rnd() % 8000),
                                      rnd() % 2 ? (u32[]){0, 0x400, 0x800, 0xC00}[rnd() % 4] : rnd());
        else plesio_at(q + 0x40, o + 0x40, 6000 + rnd() % 20000, rnd());
    }
}

/* one of them among the test objects; the player its target */
static gptr pw_setup(void)
{
    gptr o = raptor_setup(), pl = U32(G() + 0x940);
    U32(o + 0x184) &= ~0x40u;
    trex_parts(o);
    U32(pl + 0x64) = AN_PART;                           /* (the player's head, part 8: one of them) */
    U32(o + 0x1C0) = rnd() % 4 ? rnd() & 0x1F : rnd();
    U32(o + 0x1C4) = rnd() % 4 ? rnd() % 4 : rnd();
    U16(o + 0x1C8) = rnd() % 2 ? (u16[]){0, 1, 0x3B, 0x3C, 0x3D, 0x5A}[rnd() % 6] : (u16)rnd();
    U8(o + 0x1CC) = 1 + rnd() % 3; U8(o + 0x1CD) = rnd() % 13; U8(o + 0x1CE) = rnd() % 3; U8(o + 0x1CF) = rnd() % 2;
    for (u32 k = 0x1D0; k <= 0x1D8; k += 4) U32(o + k) = rnd() % 3 ? rnd() % 3 : rnd() % 2 ? rnd() % 0x2000 : rnd();
    for (u32 k = 0x1E8; k < 0x200; k += 2) U16(o + k) = rnd() % 3 ? (u16)(rnd() % 0x400 - 0x200) : (u16)rnd();
    for (u32 k = 0x1F0; k < 0x1F4; k += 2) U16(o + k) = rnd() % 3 ? rnd() % 3 : (u16)rnd();
    U16(o + 0x1EE) = rnd() % 2 ? 0x59 + rnd() % 3 : (u16)rnd();
    U16(o + 0x1FE) = rnd() % 2 ? 0x59 + rnd() % 3 : (u16)rnd();
    U32(o + 0x1F4) = rnd();
    for (u32 k = 0x200; k < 0x228; k += 2) U16(o + k) = rnd() % 3 ? rnd() % 3 : (u16)rnd();
    S16(o + 0x208) = (s16)(S16(o + 0x42) + (s32)(rnd() % 4000) - 2000);
    U16(o + 0x224) = rnd() % 2 ? 0x59 + rnd() % 3 : (u16)rnd();
    U32(o + 0x210) = rnd() % 4 ? pw_slot(rnd() % 10) : 0;
    U32(o + 0x214) = rnd() % 4 ? pw_slot(10 + rnd() % 10) : 0;
    U32(o + 0x218) = 0;
    if (rnd() % 2) { gptr k = O_K2 + 0xAC * (rnd() % 20); U32(k + 0xA0) = rnd() % 3 ? o : pl; U32(o + 0x218) = k; }
    U32(o + 0x21C) = rnd() % 4 ? rnd() % 6 : rnd();
    U8(o + 0x57) = rnd() % 2;
    U16(pl + 0x174) = rnd() % 2 ? rnd() & 3 : (u16)rnd();
    U16(pl + 0x1FA) = (u16)(rnd() % 2 ? 0x7F + rnd() % 3 : rnd() % 2 ? rnd() % 0x120 : rnd());
    U32(pl + 0x134) = 0;                                /* (no hit spheres: Enemy_AttackHit keeps +0x119) */
    for (u32 k = 0; k < 3; k++) Bits_Set(0x87E974, k, rnd() % 3 == 0);
    for (u32 k = 0; k < 8; k += 2) {
        U16(0x74AF80 + k) = rnd() % 2 ? 0 : (u16)rnd();
        U16(0x74AF90 + k) = rnd() % 2 ? 0 : (u16)rnd();
    }
    pw_quad(o);
    if (rnd() % 2) pw_others(o);
    trex_ff(o, rnd() % 2);
    trex_rand8();
    return o;
}

static void test_pw(void)
{
    g_ign_objpads = 1;
    stubs_install(1);
    pw_tables_stub(1);
    player_tables_stub(1);
    for (int i = 0; i < iters; i++) {
        game_state();
        gptr o = pw_setup(), b = o + 0x1C0, pl = U32(G() + 0x940);
        u32 a[4] = {o};
        /* the wait; the parts by the water; turning; the player's animation set */
        U8(pl + 0x54) = (u8[]){2, 5, 1, 3}[rnd() % 4]; U16(pl + 0x174) = (u16)rnd(); U8(G() + 0x108E) = rnd() % 3;
        check(0x42FEA0, Ples_Wait, "Ples_Wait", 1, a, 0xFFFFFFFF);
        {
            gptr p = AN_PART + 0x80 * (rnd() % 16);
            static const s16 ed[] = {300, 299, 301, -200, -199, -201, -400, -399, -401, 0};
            S16(o + 0x208) = (s16)(S16(p + 0x42) - ed[rnd() % 10]);
            check(0x42FF10, Ples_PartsWater, "Ples_PartsWater", 1, a, 0);
        }
        check(0x42F4C0, Ples_TurnToPlayer, "Ples_TurnToPlayer", 1, a, 0);
        U8(G() + 0x108F) = rnd() % 2 ? 0 : (u8)rnd();
        check(0x42FE80, Ples11_PlayerSet, "Ples11_PlayerSet", 0, a, 0xFFFFFFFF);
        /* the splash, the glows, the bubbles */
        a[1] = rnd() % 2 ? 0x6578D4 : 0x65A000 + (rnd() % 0x100) * 4;
        if (U32(o + 0x218)) {                               /* the old splash in use; its own or not */
            gptr k = U32(o + 0x218);
            U32(k + 0x50) = rnd() | 1;
            if (rnd() % 2) U32(k + 0xA0) = rnd() % 2 ? o : pl;
        }
        U32(LOG_BASE) = 0;
        check(0x42FFC0, Ples11_Splash, "Ples11_Splash", 2, a, 0);
        for (int k = 0; k < 6; k++) {
            game_state();
            o = pw_setup(); a[0] = o;
            a[1] = rnd() % 4 ? (u32)k % 5 : rnd() % 2 ? 0x100 + rnd() % 5 : rnd();
            U8(o + 0x54) = rnd() % 2 ? 1 : (u8)rnd(); U8(o + 0x55) = rnd() % 2 ? 4 : (u8)rnd();
            if (rnd() % 2) {                                /* a part at the water less 300 */
                gptr p = AN_PART + 0x80 * (rnd() % 16);
                S16(o + 0x208) = (s16)(S16(p + 0x42) + 300 + (s32)(rnd() % 3) - 1);
                for (u32 q = AN_PART; q < AN_PART + 0x800; q += 0x80)
                    if (q != p && rnd() % 3) S16(q + 0x42) = (s16)(S16(o + 0x208) - 300 + (s32)(rnd() % 400));
            }
            for (u32 j = 0; j < 20; j++) if (rnd() % 2) U8(O_K2 + j * 0xAC + 0x50) &= ~1;
            U32(LOG_BASE) = 0;
            sprintf(g_what, "pw %X glow %X state %X", o, a[1], U32(o + 0x54));
            check(k % 2 ? 0x431B60 : 0x430000, k % 2 ? (void *)Ples12_Glow : (void *)Ples11_Glow, k % 2 ? "Ples12_Glow" : "Ples11_Glow", 2, a, 0);
        }
        U32(o + 0x210) = pw_slot(rnd() % 20);
        U32(o + 0x21C) = rnd() % 3 ? 2 + rnd() % 3 : rnd();
        for (u32 j = 0; j < 20; j++) if (rnd() % 2) U8(O_K2 + j * 0xAC + 0x50) &= ~1;
        trex_rand8();
        check(0x431EC0, Ples12_Bubbles, "Ples12_Bubbles", 1, a, 0);
        /* taken away */
        for (u32 k = 0; k < 8; k += 2) { U16(0x74AF80 + k) = rnd() % 3 ? (u16)rnd() : 0; U16(0x74AF90 + k) = rnd() % 3 ? (u16)rnd() : 0; }
        check(0x42EA20, Ples11_Remove, "Ples11_Remove", 1, a, 0);
        game_state(); o = pw_setup(); a[0] = o;
        for (u32 k = 0; k < 8; k += 2) { U16(0x74AF80 + k) = rnd() % 3 ? (u16)rnd() : 0; U16(0x74AF90 + k) = rnd() % 3 ? (u16)rnd() : 0; }
        trex_rand8();
        check(0x430E50, Ples12_Remove, "Ples12_Remove", 1, a, 0);
        /* holding on and letting go */
        game_state(); o = pw_setup(); a[0] = o; trex_rand8();
        check(0x431A70, Ples12_BiteOn, "Ples12_BiteOn", 1, a, 0);
        check(0x431AD0, Ples12_BiteOff, "Ples12_BiteOff", 1, a, 0);
        /* a step toward a point; the combo of kills */
        {
            gptr p0 = PW_TMP, p1 = PW_TMP + 8, out = PW_TMP + 0x10;
            for (u32 k = 0; k < 0x18; k += 4) U32(PW_TMP + k) = rnd();
            S16(p0) = (s16)(rnd() % 30000 - 15000); S16(p0 + 4) = (s16)(rnd() % 30000 - 15000);
            s32 sp = rnd() % 2 ? 0x8C : rnd() % 2 ? 0xFA : (s32)(rnd() % 0x400);
            s32 d = rnd() % 2 ? sp + (s32)(rnd() % 3) - 1 : (s32)(rnd() % 3000);
            plesio_at(p1, p0, d, rnd() % 2 ? (u32[]){0, 0x400, 0x800, 0xC00}[rnd() % 4] : rnd());
            u32 v[4] = {p0, p1, rnd() % 8 ? (u32)sp : (rnd() & 0xFFFF0000) | (u32)sp, out};
            if (rnd() % 8 == 0) v[2] = rnd();
            if (rnd() % 6 == 0) {                           /* far apart, a speed with bit 15 */
                S16(p0) = (s16)(-16000 - (s32)(rnd() % 500)); S16(p1) = (s16)(16500 + rnd() % 500);
                S16(p0 + 4) = (s16)(rnd() % 2000 - 1000); S16(p1 + 4) = (s16)(rnd() % 2000 - 1000);
                v[2] = (rnd() % 2 ? rnd() & 0xFFFF0000 : 0) | 0x8000 | (rnd() % 0x400);
            }
            check(0x47A630, Vec_StepXZ, "Vec_StepXZ", 4, v, 0xFFFFFFFF);
        }
        {
            U16(0x746E4C) = rnd() % 2 ? (u16)(rnd() % 10) : rnd() % 2 ? 0x7FFF : (u16)rnd();
            U16(G() + 0xC24) = rnd() % 2 ? (u16)(U16(0x746E4C) + rnd() % 3) : (u16)rnd();
            U32(0x746E44) = rnd(); U32(0x746E48) = rnd();
            u32 v[1] = {rnd() % 2 ? 0xC : (u32[]){0xB, 0x10C, 0xFFFFFF0C, 0xD, rnd()}[rnd() % 5]};
            check(0x412CC0, Combo_Kill, "Combo_Kill", 1, v, 0);
        }
        /* deciding */
        for (int k = 0; k < 64; k++) {
            game_state();
            o = pw_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
            int y = k % 2;
            if (rnd() % 4) { Bits_Set(0x87E974, 0, 0); U32(o + 0x1D8) = 0; }
            static const s32 d11[] = {5099, 5100, 5101, 4699, 4700, 4701, 3499, 3500, 3501, 2799, 2800, 2801, 11999, 12000, 12001, 1000};
            static const s32 d12[] = {6699, 6700, 6701, 6999, 7000, 7001, 3000, 9000};
            static const s32 w11[] = {4701, 4800, 5000, 5099, 5100, 5100, 4700, 2801, 3000, 3499};   /* (the attack ranges) */
            s32 d = y ? d12[rnd() % 8] : rnd() % 2 ? w11[rnd() % 10] : d11[rnd() % 16];
            if (y && rnd() % 3 == 0) {                      /* about the stray limits */
                S16(o + 0x40) = (s16)((0x4650 + (s32)(rnd() % 3) - 1) * (rnd() % 2 ? 1 : -1));
                S16(o + 0x44) = (s16)(rnd() % 2 ? -5000 + (s32)(rnd() % 3) - 1 : (s32)(rnd() % 20000) - 10000);
            }
            plesio_at(pl + 0x40, o + 0x40, d, rnd() % 4 ? 0 : rnd());
            u16 fin;                                        /* its heading after the turn */
            if (rnd() % 3) {
                /* exact: it turns (by 0x19) toward the player's place (type 11: the head put
                 * there), then the player in front about the edge */
                gptr hd = Obj_Part(U32(pl + 0x64), 8);
                if (!y && hd) { U16(hd + 0x40) = U16(pl + 0x40); U16(hd + 0x44) = U16(pl + 0x44); }
                static const u16 ea[] = {0, 1, 0x7E, 0x7F, 0x80, 0x81, 0x40, 0x200};
                u32 e = ea[rnd() % 8], an = Angle_XZ(o + 0x40, pl + 0x40);
                int s = rnd() % 2 ? 1 : -1;
                U16(o + 0x4A) = (u16)((an + s * (s32)(e + 0x19)) & 0xFFF);
                fin = (u16)((an + s * (s32)e) & 0xFFF);
                if (!y && !hd) fin = U16(o + 0x4A);
            } else { trex_aim(o + 0x40, o, pl + 0x40); fin = U16(o + 0x4A); }
            static const u16 ra[] = {0, 1, 0x3F, 0x40, 0x41, 0x27F, 0x280, 0x281, 0x37F, 0x380, 0x381, 0x800};
            u32 e = ra[rnd() % 12];
            U16(pl + 0x4A) = (u16)((fin + (rnd() % 2 ? e : 0x1000 - e)) & 0xFFF);
            U8(pl + 0x54) = rnd() % 4 ? 1 : (u8)(rnd() % 6); U8(pl + 0x55) = rnd() % 4 ? 2 : (u8)(rnd() % 10);
            u32 d2 = Dist2_XZ(o + 0x40, pl + 0x40);
            U32(o + 0x1F4) = rnd() % 2 ? d2 + (u32)(rnd() % 3) - 1 : rnd();
            static const u8 r8[] = {1, 2, 3, 4, 5, 7, 8, 9, 0xF, 0x10, 0x11, 0x1F, 0x20, 0x21, 0, 0xFF};
            u8 r0 = r8[rnd() % 16];
            U16(0x87DE10) = rand8_seed(r0, rnd() % 4 == 0);
            U16(pl + 0x1FA) = rnd() % 2 ? (u16)(r0 + rnd() % 3 - 1) : (u16)rnd();
            Flag_Set(3, 0xA, rnd() % 2);                    /* (Pos_Hidden) */
            U32(LOG_BASE) = 0;
            static const TRexFn th[2][4] = {
                {{0x42E590, Ples11_Decide, "Ples11_Decide"}, {0x42E200, Ples11_ThinkWait, "Ples11_ThinkWait"},
                 {0x42E330, Ples11_ThinkSwim, "Ples11_ThinkSwim"}, {0x42E470, Ples11_ThinkBack, "Ples11_ThinkBack"}},
                {{0x430B00, Ples12_Decide, "Ples12_Decide"}, {0x4308C0, Ples12_ThinkWait, "Ples12_ThinkWait"},
                 {0x4309D0, Ples12_ThinkSwim, "Ples12_ThinkSwim"}, {0x4309D0, Ples12_ThinkSwim, "Ples12_ThinkSwim"}}};
            const TRexFn *f = &th[y][(k / 2) % 4];
            U8(o + 0x56) = rnd() % 3 ? 1 : (u8)(rnd() % 3);
            sprintf(g_what, "pw %X %s dist %d bits %X", o, f->name, d, U32(b));
            check(f->addr, f->fn, f->name, 1, a, (k / 2) % 4 == 0 ? 0xFFFFFFFF : 0);
        }
        /* the state handlers */
        game_state();
        o = pw_setup(); a[0] = o; pl = U32(G() + 0x940);
        for (int y = 0; y < 2; y++) {
            U8(o + 0x54) = rnd() % 5; U8(o + 0x55) = rnd() % 13; U8(o + 0x56) = rnd() % 3;
            check(y ? 0x430820 : 0x42E160, y ? (void *)Ples12_Normal : (void *)Ples11_Normal, y ? "Ples12_Normal" : "Ples11_Normal", 1, a, 0);
            U8(o + 0x55) = rnd() % 4;
            check(y ? 0x430850 : 0x42E190, y ? (void *)Ples12_Hit : (void *)Ples11_Hit, y ? "Ples12_Hit" : "Ples11_Hit", 1, a, 0);
            check(y ? 0x430870 : 0x42E1B0, y ? (void *)Ples12_Die : (void *)Ples11_Die, y ? "Ples12_Die" : "Ples11_Die", 1, a, 0);
            for (int k = 0; k < 3; k++) {
                U8(o + 0x55) = rnd() % 13;
                U8(o + 0x119) = (u8[]){0, 1, 0xFF, 0x7F, 0x80}[rnd() % 5];
                U16(pl + 0x174) &= ~1; U8(pl + 0x17B) = 0; U32(o + 0x184) &= ~0x40u;   /* (Enemy_AttackHit keeps +0x119) */
                U32(o + 0xA0) = TR_ATK; U16(o + 0x102) &= ~2;
                check(y ? 0x430890 : 0x42E1D0, y ? (void *)Ples12_Grab : (void *)Ples11_Grab, y ? "Ples12_Grab" : "Ples11_Grab", 1, a, 0);
            }
        }
        for (int k = 0; k < 8; k++) {
            game_state();
            o = pw_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
            U8(o + 0x54) = rnd() % 5; U8(o + 0x55) = rnd() % 13;
            U8(G() + 0x108F) = rnd() % 13;
            U32(o + 0x50) = (U32(o + 0x50) & ~0x40000030u) | (rnd() & 0x30) | (rnd() % 6 == 0 ? 0x40000000 : 0);
            U16(pl + 0x1FA) = (u16)(rnd() % 2 ? 0x7F + rnd() % 3 : rnd());
            U8(o + 0x17B) = rnd() % 3 ? rnd() % 2 : (u8)rnd();
            if (rnd() % 2) trex_spheres(o, 1 + rnd() % 3);
            if (k % 2) U32(o + 0x210) = pw_slot(rnd() % 10);    /* (the bubbles come from it) */
            for (u32 j = 0; j < 20; j++) if (rnd() % 2) U8(O_K2 + j * 0xAC + 0x50) &= ~1;
            sprintf(g_what, "pw %X handler state %X bits %X", o, U32(o + 0x54), U32(b));
            if (k % 2) check(0x430360, Ples12_Type, "Ples12_Type", 1, a, 0);
            else check(0x42DCC0, Ples11_Type, "Ples11_Type", 1, a, 0);
        }
        /* state 0 */
        for (int y = 0; y < 2; y++) {
            game_state();
            o = pw_setup(); a[0] = o; pl = U32(G() + 0x940);
            do model_data(0x640000); while (U32(0x640000 + 0x14) != 16);
            do anim_setup_at(AN_OBJ); while (U32(AN_OBJ + 0x68) != 16);
            memcpy(MEM(o + 0x60), MEM(AN_OBJ + 0x60), 0x50);
            U32(0x642348 + 6 * 4) = AN_DATA; U32(0x644924 + 5 * 4) = AN_DATA;   /* the first animations */
            U8(G() + 0x108E) = rnd() % 3; U8(0x87E747) = rnd() % 2;
            U8(G() + 0x1091) = rnd() % 2 ? 6 : (u8)rnd(); U8(G() + 0x1090) = rnd() % 2 ? 1 : (u8)rnd();
            pw_others(o);
            trex_rand8();
            for (u32 k = 0; k < 8; k += 2) { U16(0x74AF80 + k) = rnd() % 3 ? 0 : (u16)rnd(); U16(0x74AF90 + k) = rnd() % 3 ? 0 : (u16)rnd(); }
            U32(LOG_BASE) = 0;
            if (y) check(0x430590, Ples12_Init, "Ples12_Init", 1, a, 0);
            else check(0x42DEA0, Ples11_Init, "Ples11_Init", 1, a, 0);
        }
        /* moving, attacks landing, hit, dying: each step */
        static const TRexFn mv[] = {
            {0x42E240, Ples11_Wait2, "Ples11_Wait2", 5}, {0x42E3A0, Ples11_Swim, "Ples11_Swim", 3},
            {0x42E4F0, Ples11_Back, "Ples11_Back", 3}, {0x42E840, Ples11_Leap, "Ples11_Leap", 3},
            {0x42E950, Ples11_Dive, "Ples11_Dive", 3}, {0x42EA80, Ples11_Rise, "Ples11_Rise", 3},
            {0x42EB30, Ples11_Lunge, "Ples11_Lunge", 3}, {0x42ECA0, Ples11_Bite, "Ples11_Bite", 3},
            {0x42ED60, Ples11_ThinkSide, "Ples11_ThinkSide", 2}, {0x42EDD0, Ples11_Side, "Ples11_Side", 4},
            {0x42EFA0, Ples11_ThinkDrag, "Ples11_ThinkDrag", 3}, {0x42EFD0, Ples11_Drag, "Ples11_Drag", 6},
            {0x42F300, Ples11_Eat, "Ples11_Eat", 4}, {0x42F4E0, Ples11_Resurface, "Ples11_Resurface", 4},
            {0x42F5F0, Ples11_Snap, "Ples11_Snap", 3}, {0x42F6E0, Ples11_BiteHit, "Ples11_BiteHit", 3},
            {0x42F770, Ples11_SideHit, "Ples11_SideHit", 3}, {0x42F820, Ples11_ResurfaceHit, "Ples11_ResurfaceHit", 3},
            {0x42F890, Ples11_SnapHit, "Ples11_SnapHit", 3}, {0x42F910, Ples11_Damage, "Ples11_Damage", 1},
            {0x42FBD0, Ples11_Flinch, "Ples11_Flinch", 3}, {0x42FC30, Ples11_Knock, "Ples11_Knock", 3},
            {0x42FCA0, Ples11_Stagger, "Ples11_Stagger", 3}, {0x42FD90, Ples11_Dead, "Ples11_Dead", 4},
            {0x4308F0, Ples12_Wait2, "Ples12_Wait2", 5}, {0x430A30, Ples12_Swim, "Ples12_Swim", 3},
            {0x430C40, Ples12_Leap, "Ples12_Leap", 3}, {0x430D80, Ples12_Dive, "Ples12_Dive", 3},
            {0x430EB0, Ples12_Rise, "Ples12_Rise", 3}, {0x430F80, Ples12_Lunge, "Ples12_Lunge", 3},
            {0x4310E0, Ples12_Bite, "Ples12_Bite", 3}, {0x431190, Ples12_Side, "Ples12_Side", 4},
            {0x4312D0, Ples12_Resurface, "Ples12_Resurface", 4}, {0x4313E0, Ples12_BiteHit, "Ples12_BiteHit", 3},
            {0x431440, Ples12_SideHit, "Ples12_SideHit", 3}, {0x4314D0, Ples12_ResurfaceHit, "Ples12_ResurfaceHit", 3},
            {0x431530, Ples12_SnapHit, "Ples12_SnapHit", 3}, {0x4315B0, Ples12_Damage, "Ples12_Damage", 1},
            {0x431810, Ples12_Flinch, "Ples12_Flinch", 3}, {0x431860, Ples12_Knock, "Ples12_Knock", 3},
            {0x4318C0, Ples12_Stun, "Ples12_Stun", 3}, {0x431970, Ples12_Dead, "Ples12_Dead", 4}};
        const unsigned nm = sizeof mv / sizeof mv[0];
        game_state(); world_setup(); enemy_state(); zones_setup();
        o = O_OBJ + 0x260 * (1 + rnd() % 6); a[0] = o; b = o + 0x1C0;
        anim_setup_at(O_OBJ);
        do anim_setup_at(o); while (U32(o + 0x68) != 16);
        memcpy(MEM(O_OBJ + 0x60), MEM(o + 0x60), 0x50);
        U32(G() + 0x940) = pl = O_OBJ;
        U32(pl + 0x128) = rnd() % 2 ? 0x7200E8 : 0; U32(o + 0x128) = 0x720938;
        U8(o + 0x58) = 11;
        u8 sv[0x260]; memcpy(sv, MEM(o), 0x260);
        for (unsigned k = 0; k < 4 * nm; k++) {
            const TRexFn *f = &mv[k % nm];
            memcpy(MEM(o), sv, 0x260);
            for (int j = 0; j < 0x20; j++) U32(AN_TAB + 4 * j) = AN_DATA + 0x1000 * (rnd() % 2);   /* (which animation started) */
            for (u32 j = 0x1C0; j < 0x228; j += 4) U32(o + j) = rnd();
            U32(b) = rnd() % 4 ? rnd() & 0x1F : rnd();
            U32(o + 0x1C4) = rnd() % 4 ? rnd() % 4 : rnd();
            U16(o + 0x1C8) = rnd() % 2 ? (u16[]){0, 1, 0x3B, 0x3C, 0x3D}[rnd() % 5] : (u16)rnd();
            U8(o + 0x1CC) = 1 + rnd() % 3; U8(o + 0x1CD) = rnd() % 13; U8(o + 0x1CE) = rnd() % 3; U8(o + 0x1CF) = rnd() % 2;
            for (u32 j = 0x1D0; j <= 0x1D8; j += 4) U32(o + j) = rnd() % 3 ? rnd() % 3 : rnd() % 2 ? rnd() % 0x2000 : rnd();
            for (u32 j = 0x200; j < 0x228; j += 2) U16(o + j) = rnd() % 3 ? rnd() % 3 : (u16)rnd();
            U32(o + 0x210) = rnd() % 4 ? pw_slot(rnd() % 10) : 0;
            U32(o + 0x214) = rnd() % 4 ? pw_slot(10 + rnd() % 10) : 0;
            U32(o + 0x218) = 0;
            if (rnd() % 2) { gptr kk = O_K2 + 0xAC * (rnd() % 20); U32(kk + 0xA0) = rnd() % 3 ? o : pl; U32(o + 0x218) = kk; }
            for (u32 j = 0; j < 8; j += 2) { U16(0x74AF80 + j) = rnd() % 3 ? (u16)rnd() : 0; U16(0x74AF90 + j) = rnd() % 3 ? (u16)rnd() : 0; }
            U8(o + 0x54) = rnd() % 4 ? 1 + rnd() % 3 : rnd() % 5;
            U8(o + 0x55) = rnd() % 13;
            U8(o + 0x56) = (u8)(rnd() % 8 ? rnd() % f->steps : rnd());
            U8(o + 0x57) = (f->addr == 0x42ECA0 || f->addr == 0x42EDD0 || f->addr == 0x42F6E0 || f->addr == 0x4310E0 || f->addr == 0x431190) ? rnd() % 3 : rnd() % 2;
            static const u16 fr[] = {0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x4E, 0x4F, 0x50, 0, 0x10};
            U16(o + 0x92) = rnd() % 4 ? fr[rnd() % 10] : (u16)(rnd() % 0x80);
            S16(o + 0x208) = (s16)(rnd() % 2 ? S16(o + 0x42) : (s32)(rnd() % 20000) - 10000);
            trex_ff(o, rnd() % 2);
            U8(pl + 0x54) = (u8[]){1, 2, 5, 3}[rnd() % 4]; U16(pl + 0x174) = rnd() % 2 ? rnd() & 3 : (u16)rnd();
            U16(pl + 0x4A) = (u16)((U16(o + 0x4A) + (u16[]){0, 0x3FF, 0x400, 0x401, 0xC01, 0xC00, 0xBFF, 0x800}[rnd() % 8]) & 0xFFF);
            S16(pl + 0x108) = (s16)(rnd() % 2 ? (s32[]){3, 4, 5, 0xF9, 0xFA, 0xFB, 0, 1}[rnd() % 8] : rnd() % 2 ? (s32)(rnd() % 0x200) - 0x10 : (s32)rnd());
            Flag_Set(2, 0x1A, rnd() % 3 == 0);
            /* the damage */
            U16(o + 0x172) = rnd() % 6; U16(o + 0x176) = rnd() % 2 ? rnd() % 0x400 : (u16)rnd();
            S16(o + 0x10A) = (s16)(rnd() % 2 ? rnd() % 0x2000 : rnd());
            S16(o + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x800 - 0x10 : rnd());
            if (rnd() % 3 == 0) S16(o + 0x108) = (s16)(U16(o + 0x176) + rnd() % 3 - 1);
            if (rnd() % 3 == 0) S16(o + 0x108) = (s16)((s32)U16(o + 0x176) / 5 + (s32)(rnd() % 3) - 1);
            if (rnd() % 3 == 0) U32(o + 0x1D0) = (u32)(S16(o + 0x10A) / 2 - (s32)U16(o + 0x176) + (s32)(rnd() % 3) - 1);
            U8(o + 0x184) = (u8)rnd(); U8(o + 0x17C) = rnd() % 2 ? 0 : (u8)rnd();
            U8(o + 0x58) = rnd() % 2 ? 0xC : (u8)rnd();
            U8(G() + 0x108E) = rnd() % 3; U8(G() + 0x108F) = rnd() % 2 ? rnd() % 12 : (u8)rnd();
            for (u32 j = 0; j < 3; j++) Bits_Set(0x87E974, j, rnd() % 3 == 0);
            U16(0x746E4C) = (u16)(rnd() % 10); U16(G() + 0xC24) = (u16)(rnd() % 10);
            if (f->addr == 0x42F910 || f->addr == 0x4315B0) {
                if (rnd() % 2) U8(o + 0x56) = 0;
                if (rnd() % 2) U16(o + 0x222) = 0;
                if (rnd() % 2) U16(o + 0x206) = 0;
                if (rnd() % 2) U8(o + 0x1CC) = 2;
                if (rnd() % 2) U16(o + 0x172) = 3 + rnd() % 2;
                U8(G() + 0x757) = 0;
            }
            if (f->addr == 0x42EB30 || f->addr == 0x430F80) {   /* the lunge's end about 500 away, or within a step */
                u32 sp = f->addr == 0x42EB30 ? 0x8C : 0xFA;
                if (rnd() % 2) U8(o + 0x56) = 1;
                if (rnd() % 4) U8(pl + 0x174) |= 2;
                s32 h = f->addr == 0x42EB30 ? 0xFA : 4;     /* the player's health about the edge */
                if (rnd() % 2) S16(pl + 0x108) = (s16)(h + (s32)(rnd() % 3) - 1);
                s32 d = (s32[]){499, 500, 501, (s32)sp - 1, (s32)sp, (s32)sp + 1, 300, 2000}[rnd() % 8];
                plesio_at(o + 0x1E8, o + 0x40, d, rnd() % 2 ? (u32[]){0, 0x400, 0x800, 0xC00}[rnd() % 4] : rnd());
            }
            if (f->addr == 0x42ED60 && rnd() % 2) {         /* the player about 3800 or 6500 away */
                plesio_at(pl + 0x40, o + 0x40, (s32[]){3799, 3800, 3801, 6499, 6500, 6501}[rnd() % 6], rnd() % 4 ? 0 : rnd());
                if (rnd() % 2) U16(o + 0x220) = 0;
                if (rnd() % 2) U8(pl + 0x54) = 1;
                if (rnd() % 2) U8(o + 0x56) = 1;
            }
            if (f->addr == 0x42F4E0 || f->addr == 0x4312D0) U16(o + 0x20C) = rnd() % 3 ? rnd() % 3 : (u16)rnd();
            if (f->addr == 0x42E950 || f->addr == 0x430D80) pw_quad(o);
            if (f->addr == 0x42EFD0 || f->addr == 0x42F300) {   /* the player's health about the damage */
                S16(pl + 0x108) = (s16)(rnd() % 2 ? 0xFA + (s32)(rnd() % 400) - 200 : (s32)rnd());
            }
            for (u32 j = 0; j < 20; j++) if (rnd() % 2) U8(O_K2 + j * 0xAC + 0x50) &= ~1;
            if (rnd() % 2) { U16(o + 0xF0) = (u16)(rnd() % 0x600 - 0x300); U16(o + 0xF2) = (u16)(rnd() % 0x600 - 0x300); U16(o + 0xF4) = (u16)(rnd() % 0x300); }
            trex_rand8();
            anim_maybe_end(o);
            if (rnd() % 2) U16(o + 0x102) |= 0x20;
            U32(LOG_BASE) = 0;
            if (rnd() % 2) {                                /* the speed within a step of its target */
                static const struct { u32 addr; s32 t; int base; } tg[] = {
                    {0x42E3A0, 0x8C, 1}, {0x42E4F0, -0x8C, 0}, {0x42E950, -0x8C, 0}, {0x42F4E0, 0x32, 0},
                    {0x42FC30, 0xC8, 0}, {0x42FD90, 10, 0}, {0x430A30, 0xFA, 1}, {0x430C40, 0x82, 0},
                    {0x430D80, -0xFA, 0}, {0x430EB0, 0xFA, 1}, {0x4312D0, 0xC8, 0}, {0x431860, 0x258, 0}};
                s32 t = 0;
                for (unsigned j = 0; j < sizeof tg / sizeof tg[0]; j++)
                    if (tg[j].addr == f->addr) t = tg[j].t + (tg[j].base ? U16(o + 0x1F0) : 0);
                U16(o + 0xF4) = (u16)(t + (s32)(rnd() % 0x51) - 0x28);
            }
            if ((f->addr == 0x42F910 || f->addr == 0x4315B0) && rnd() % 2)
                U16(0x87DE10) = rand8_seed((u8)(0x7F + rnd() % 3), 0);   /* (its first Rand8 about 0x80) */
            if (f->addr == 0x42EFD0 && rnd() % 2) {         /* the drag's bite: health about the damage */
                U8(o + 0x56) = 2;
                S16(pl + 0x108) = (s16)(Damage_Scale(0xFA) + (s32)(rnd() % 3) - 1);
            }
            sprintf(g_what, "pw %X %s step %u side %u frame %X", o, f->name, U8(o + 0x56), U8(o + 0x57), U16(o + 0x92));
            check(f->addr, f->fn, f->name, 1, a, 0);
        }
        /* the player back up after the drag */
        anim_maybe_end(pl);
        a[0] = pl;
        check(0x42F2C0, Ples11_PlayerUp, "Ples11_PlayerUp", 1, a, 0);
    }
    player_tables_stub(0);
    pw_tables_stub(0);
    stubs_install(0);
    g_ign_objpads = 0;
}

/* ---- the roamer (area 3, type 22) ---- */
#define RM_G    0x6A2000u       /* its graph */
#define RM_CYL  0x6A2800u       /* two cylinders */
#define RM_TMP  0x6A2840u
#define RM_PT(o, n) (U32((o) + 0x1C8) + (u32)(n) * 8)
static u32 g_rm_saved[8];
static void roam_tables_stub(int on)
{
    for (int k = 1; k <= 8; k++) g_stubs[k].spec = on ? "d" : "";
    for (u32 k = 0; k < 8; k++) {
        gptr a = 0x7208E0 + 4 * k;
        if (on) { g_rm_saved[k] = U32(a); U32(a) = (u32)(uintptr_t)g_recs[1 + k]; }
        else U32(a) = g_rm_saved[k];
    }
}

/* the graph: connected; lengths short (paths below 0x7530) or, when wild, any */
static void roam_graph(gptr o, int wild)
{
    static const u8 kinds[] = {0, 0, 0, 0, 0, 4, 4, 1, 1, 0x21, 3, 8, 0x10, 0x14, 0x11, 0x40, 0x12, 0x18, 0x15, 0x30};
    u32 nn = rnd() % 4 ? 2 + rnd() % 15 : 1 + rnd() % 32, ne = nn - 1 + rnd() % (nn + 2);
    gptr p = RM_G, pts = p + 4, ln = pts + nn * 8;
    U16(p) = (u16)nn; U16(p + 2) = (u16)ne;
    int plain = rnd() % 6;                                  /* (0: maybe no plain point) */
    for (u32 i = 0; i < nn; i++) {
        gptr e = pts + i * 8;
        S16(e) = (s16)(S16(o + 0x40) + (s32)(rnd() % 16000) - 8000);
        S16(e + 4) = (s16)(S16(o + 0x44) + (s32)(rnd() % 16000) - 8000);
        U16(e + 2) = rnd() % 3 ? 0 : (u16)rnd();
        u8 k = rnd() % 6 ? kinds[rnd() % 20] : (u8)rnd();
        if (!plain && rnd() % 2) k |= 1;
        U8(e + 6) = k;
        U8(e + 7) = (u8)(k & 1 && rnd() % 4 ? rnd() % 3 : rnd() % 32);
    }
    if (plain) U8(pts + (rnd() % nn) * 8 + 6) &= ~0x1Du;
    for (u32 j = 0; j < ne; j++) {
        gptr l = ln + j * 8;
        u32 a, b;
        if (j + 1 < nn) { a = j + 1; b = rnd() % (j + 1); }
        else { a = rnd() % nn; b = rnd() % nn; }
        if (rnd() % 2) { u32 t = a; a = b; b = t; }
        U8(l) = (u8)a; U8(l + 1) = (u8)b;
        S16(l + 2) = (s16)(wild && rnd() % 3 == 0 ? (rnd() % 2 ? (s32)(s16)rnd() : 0x7000 + (s32)(rnd() % 0x1000))
                           : rnd() % 2 ? 100 * (s32)(1 + rnd() % 3) : 100 + (s32)(rnd() % 800));   /* (equal ways) */
        U16(l + 4) = (u16)rnd(); U16(l + 6) = (u16)rnd();
    }
    U32(o + 0x1C0) = p; U32(o + 0x1C4) = U32(p); U32(o + 0x1C8) = pts; U32(o + 0x1CC) = ln;
    u8 n5 = 0, n1 = 0;
    for (u32 i = 0; i < nn; i++) {
        u8 k = U8(pts + i * 8 + 6);
        if (k & 5) U8(0x74AF18 + n5++) = (u8)i;
        if (k & 1) U8(0x74AF58 + n1++) = (u8)i;
    }
    U8(0x74AF78) = n5; U8(0x74AF79) = n1;
    if (rnd() % 6 == 0) { U8(0x74AF78) = (u8)(rnd() % (n5 + 1)); U8(0x74AF79) = (u8)(rnd() % (n1 + 1)); }
    for (u32 i = 0; i < 32; i++) U8(0x74AF38 + i) = (u8)(rnd() % nn);
    U8(0x74AF7A) = (u8)(rnd() % nn); U8(0x74AF7B) = (u8)(rnd() % nn);
    U8(o + 0x1D0) = (u8)(rnd() % nn); U8(o + 0x1D1) = (u8)(rnd() % nn);
}

/* one among the test objects, the player near or not */
static gptr roam_setup(int wild, int anim)
{
    gptr o, g, pl;
    if (anim) {                                             /* (animations it starts) */
        game_state(); world_setup(); enemy_state(); objects_links(); zones_setup();   /* (parts back: a handler may have taken them) */
        o = O_OBJ + 0x260 * (1 + rnd() % 6);
        anim_setup_at(O_OBJ);
        do anim_setup_at(o); while (U32(o + 0x68) != 16);
        memcpy(MEM(O_OBJ + 0x60), MEM(o + 0x60), 0x50);
        U32(G() + 0x940) = O_OBJ;
        U32(O_OBJ + 0x128) = rnd() % 2 ? 0x7200E8 : 0; U32(o + 0x128) = 0x7208C8;
        U8(o + 0x58) = 22;
        U32(o + 0x50) = rnd() % 2 ? 0xAD : rnd();
        for (int j = 0; j < 0x20; j++) U32(AN_TAB + 4 * j) = AN_DATA + 0x1000 * (rnd() % 2);
    } else {
        o = raptor_setup();
        trex_parts(o);
    }
    g = G(); pl = U32(g + 0x940);
    U32(o + 0x184) &= ~0x40u;
    trex_ff(o, rnd() % 2);
    roam_graph(o, wild);
    U8(o + 0x1D2) = (u8)rnd(); U8(o + 0x1D3) = rnd() % 2;
    U16(o + 0x1D4) = rnd() % 2 ? 0x95 + rnd() % 4 : (u16)rnd();
    U16(o + 0x1D8) = (u16)rnd();
    U32(o + 0x1DC) = rnd() % 2 ? 0x9C4 + rnd() % 0x5DD : rnd();
    U8(o + 0x1E0) = rnd() % 2 ? 3 : (u8)(rnd() % 5);
    U8(g + 0x1090) = rnd() % 4 ? (u8)(3 + rnd() % 3) : (u8)(rnd() % 13);
    U8(g + 0x1274) = rnd() % 2 ? U8(g + 0x1090) : (u8)(rnd() % 7);
    for (u32 f = 0x69; f <= 0x6D; f++) Flag_Set(6, f, rnd() % 2);
    Flag_Set(6, 0x7C, rnd() % 2);
    for (u32 f = 0; f < 32; f++) Flag_Set(7, f, rnd() % 3 == 0);
    Flag_Set(2, 0x19, rnd() % 2);
    U32(CL_ROOM + 0x3C) = rnd() % 2 ? 0x40 : 0x200 + rnd() % 0x100;
    for (u32 k = 0; k < 8; k++) U8(RM_CYL + k) = (u8)rnd();
    for (u32 k = 0; k < 2; k++) {
        S16(RM_CYL + k * 8) = (s16)(rnd() % 400 - 200); S16(RM_CYL + k * 8 + 4) = (s16)(rnd() % 400 - 200);
        U16(RM_CYL + k * 8 + 6) = (u16)(rnd() % 4 ? 100 + rnd() % 700 : rnd());
    }
    U32(o + 0x134) = RM_CYL; U32(pl + 0x12C) = RM_CYL + 8;
    U16(pl + 0x174) = rnd() % 2 ? 0 : (u16)rnd();
    U16(o + 0xF4) = (u16)rnd();
    trex_rand8();
    return o;
}

/* p at about d from point e */
static void roam_near(gptr p, gptr e, s32 d)
{
    plesio_at(p, e, d, rnd() % 2 ? (u32[]){0, 0x400, 0x800, 0xC00}[rnd() % 4] : rnd());
}

/* no walls in the way of sight (Line_Blocked) */
static void roam_open(gptr o)
{
    gptr c = U32(G() + 0xA4C);
    for (u32 k = 0; k < U8(c); k++) U8(c + U32(c + 8 + 4 * k)) = 0;
    U8(o + 0x184) &= ~0x10;
}

/* point c the nearest of all, at about d, the others far */
static void roam_only(gptr o, u32 c, s32 d)
{
    for (u32 j = 0; j < U16(o + 0x1C4); j++) if (j != c) roam_near(RM_PT(o, j), o + 0x40, 3000 + rnd() % 5000);
    roam_near(o + 0x40, RM_PT(o, c), d);
}

/* one wall: a square whose edge (z = const) is within o's cylinder */
static void roam_wall(gptr o)
{
    gptr c = CL_ROOM, w = c + 0x40, v = w + 8 + 16;
    U8(c) = 1; U32(c + 8) = 0x40; U32(c + 0x3C) = 0x40;
    U8(w) = 1; U8(w + 1) = 4; U8(w + 2) = 0; U8(w + 3) = 0; U16(w + 4) = 0xFFFF;
    for (u32 e = 0; e < 4; e++) { U8(w + 8 + 4 * e) = 1; U8(w + 9 + 4 * e) = 0; }
    s32 x = S16(o + 0x40), z = S16(o + 0x44), sg = rnd() % 2 ? 1 : -1, z0 = z + sg * (s32)(50 + rnd() % 300), r = 3000;
    s32 q[4][2] = {{x - r, z0}, {x + r, z0}, {x + r, z0 + sg * 2 * r}, {x - r, z0 + sg * 2 * r}};
    int rev = rnd() % 2;
    for (u32 e = 0; e < 4; e++) { u32 k = rev ? 3 - e : e; S16(v + 4 * e) = (s16)q[k][0]; S16(v + 4 * e + 2) = (s16)q[k][1]; }
    U32(v + 16) = U32(v);
    U32(G() + 0xA4C) = c; U32(o + 0x128) = 0x7208C8; U8(o + 0x184) &= ~0x10;
}

static void test_roam(void)
{
    g_ign_objpads = 1;
    stubs_install(1);
    roam_tables_stub(1);
    player_tables_stub(1);
    for (int i = 0; i < iters; i++) {
        game_state();
        gptr o = roam_setup(1, 0), pl = U32(G() + 0x940);
        u32 a[3] = {o};
        check(0x426C60, Obj_Vanish, "Obj_Vanish", 1, a, 0);
        /* the graph: shortest paths, the nearest point, the exits */
        sprintf(g_what, "roam %X points %u links %u from %d", o, U16(o + 0x1C4), U16(o + 0x1C6), (s8)U8(o + 0x1D0));
        if (rnd() % 8 == 0) U8(o + 0x1D0) = rnd() % 2 ? 0xFF : (u8)(U16(o + 0x1C4) + rnd() % (32 - U16(o + 0x1C4) + 1));
        if (U8(o + 0x1D0) > 0x1F && U8(o + 0x1D0) != 0xFF) U8(o + 0x1D0) = 0;
        check(0x42DA50, Roam_Path, "Roam_Path", 1, a, 0);
        a[1] = RM_TMP; U32(RM_TMP) = rnd();
        if (rnd() % 2) roam_near(o + 0x40, RM_PT(o, rnd() % U16(o + 0x1C4)), rnd() % 3000);
        if (rnd() % 3 == 0) {                               /* two at the same place */
            gptr p0 = RM_PT(o, rnd() % U16(o + 0x1C4)), p1 = RM_PT(o, rnd() % U16(o + 0x1C4));
            U16(p1) = U16(p0); U16(p1 + 4) = U16(p0 + 4);
            if (rnd() % 2) roam_open(o);
        }
        if (rnd() % 6 == 0) {                               /* all over 30000 away */
            S16(o + 0x40) = -16000; S16(o + 0x44) = (s16)(rnd() % 2000);
            for (u32 j = 0; j < U16(o + 0x1C4); j++) { S16(RM_PT(o, j)) = (s16)(15000 + rnd() % 2000); S16(RM_PT(o, j) + 4) = (s16)(rnd() % 2000); }
            if (rnd() % 2) roam_open(o);
        }
        check(0x42DC20, Roam_Nearest, "Roam_Nearest", 2, a, 0xFF);
        if (rnd() % 8 == 0) U32(o + 0x1C8) = 0;
        { gptr c = U32(G() + 0xA4C); U8(c + U32(c + 0x3C) + 3) = (u8)rnd(); }
        if (rnd() % 3 == 0) {                               /* exits listed that are not (or no longer) */
            U8(0x74AF79) = (u8)(1 + rnd() % 8);
            for (u32 j = 0; j < 8; j++) U8(0x74AF58 + j) = (u8)(rnd() % U16(o + 0x1C4));
        }
        check(0x42CFF0, Roam_Exits, "Roam_Exits", 1, a, 0);
        /* a point to go to */
        game_state(); o = roam_setup(0, 0); a[0] = o;
        for (int k = 0; k < 4; k++) {
            a[1] = rnd() % 4 ? rnd() % U16(o + 0x1C4) : (rnd() & 0xFFFFFF00) | rnd() % U16(o + 0x1C4);
            a[2] = rnd() % 4 ? rnd() % 4 : rnd();
            Flag_Set(6, 0x6C, rnd() % 2);
            check(0x42D380, Roam_Pick, "Roam_Pick", 3, a, 0xFF);
        }
        /* turning toward a point */
        a[1] = RM_TMP; a[2] = RM_PT(o, rnd() % U16(o + 0x1C4));
        U32(RM_TMP) = rnd() % 2 ? 0x64 : rnd();
        if (rnd() % 2) {                                    /* about 0x64 off */
            static const s32 ed[] = {0, 1, -1, 0x63, 0x64, 0x65, -0x63, -0x64, -0x65, 0x7FF, 0x800};
            U16(o + 0x4A) = (u16)((Angle_XZ(o + 0x40, a[2]) + ed[rnd() % 11]) & 0xFFF);
        }
        check(0x42D7E0, Roam_TurnTo, "Roam_TurnTo", 3, a, 0xFFFF);
        /* bumping the player */
        game_state(); o = roam_setup(0, 0); a[0] = o; pl = U32(G() + 0x940);
        {
            gptr ca = RM_CYL, cb = RM_CYL + 8, c = RM_TMP, t = RM_TMP + 8;
            s32 r = U16(ca + 6) + U16(cb + 6);
            U16(c) = (u16)(U16(o + 0x40) + U16(ca)); U16(c + 4) = (u16)(U16(o + 0x44) + U16(ca + 4));
            plesio_at(t, c, rnd() % 3 ? r + (s32)(rnd() % 3) - 1 : (s32)(rnd() % 3000), rnd() % 3 ? (u32[]){0, 0x400, 0x800, 0xC00}[rnd() % 4] : rnd());
            U16(pl + 0x40) = (u16)(U16(t) - U16(cb)); U16(pl + 0x44) = (u16)(U16(t + 4) - U16(cb + 4));
            U16(pl + 0x174) = rnd() % 2 ? 0 : (u16[]){1, 0x100, 0x8000, (u16)rnd()}[rnd() % 4];
            check(0x42D880, Roam_Bump, "Roam_Bump", 1, a, 0);
        }
        /* set up, started */
        game_state(); o = roam_setup(0, 0); a[0] = o;
        do model_data(0x640000); while (U32(0x640000 + 0x14) != 16);
        do anim_setup_at(AN_OBJ); while (U32(AN_OBJ + 0x68) != 16);
        memcpy(MEM(o + 0x60), MEM(AN_OBJ + 0x60), 0x50);
        U32(o + 0x60) = 0x640000;                           /* its model */
        U8(o + 0x54) = (u8)rnd(); U8(G() + 0x1274) = rnd() % 2 ? 0 : (u8)rnd();
        check(0x42CDF0, Roam_Init, "Roam_Init", 1, a, 0);
        o = roam_setup(0, 1); a[0] = o;
        U8(o + 0x54) = (u8)rnd();
        U32(o + 0x1C4) = rnd(); U32(o + 0x1C8) = rnd(); U32(o + 0x1CC) = rnd();
        if (rnd() % 4) U8(G() + 0x1274) = U8(G() + 0x1090);
        snd_setup(); U8(G() + 0x927) = rnd() % 2; U8(0x87E747) = rnd() % 3;
        for (u32 id = 0x61; id <= 0x81; id += 0x20) U8(0x6EED00 + id * 8 + 1) = (u8)(rnd() % 4);   /* (its cry plays) */
        sprintf(g_what, "roam %X start points %u room %u/%u", o, U16(RM_G), U8(G() + 0x1090), U8(G() + 0x1274));
        check(0x42CE60, Roam_Start, "Roam_Start", 1, a, 0);
        /* the moves */
        for (int k = 0; k < 12; k++) {
            int sc = k % 6;
            if (sc == 5) { game_state(); o = roam_setup(0, 0); } else o = roam_setup(0, 1);
            a[0] = o; pl = U32(G() + 0x940);
            u32 nn = U16(o + 0x1C4);
            U8(o + 0x55) = rnd() % 4 ? (u8)(sc % 3) : (u8)(rnd() % 3);
            U8(o + 0x56) = rnd() % 4 ? (u8)(rnd() % 2) : (u8)rnd();
            if (sc == 0) {                                  /* walking: near a point to turn at, the player near */
                u32 c = rnd() % nn;
                s32 n = (s32)U32(0x7208AC + U8(G() + 0x1090) * 4);
                roam_near(o + 0x40, RM_PT(o, c), rnd() % 2 ? n + (s32)(rnd() % 3) - 1 : (s32)(rnd() % 2000));
                for (u32 j = 0; j < nn; j++) if (j != c && rnd() % 2) roam_near(RM_PT(o, j), o + 0x40, 3000 + rnd() % 5000);
                if (rnd() % 2) { roam_open(o); roam_only(o, c, n + (s32)(rnd() % 3) - 1); }
                if (rnd() % 2) U8(RM_PT(o, c) + 6) |= 0x10;
                u8 v = (u8)rnd(); U16(0x87DE10) = rand8_seed(v, 0);
                if (rnd() % 2) U32(o + 0x1DC) = 0xFA0 - v % 20 + rnd() % 2;
                s32 k2 = (s32)((U32(o + 0x1DC) + v % 20) > 0xFA0 ? 0x9C4 : U32(o + 0x1DC) + v % 20);
                roam_near(pl + 0x40, o + 0x40, rnd() % 2 ? k2 + (s32)(rnd() % 3) - 1 : (s32)(rnd() % 6000));
                sprintf(g_what, "roam %X walk near %u", o, c);
                check(0x42D180, Roam_Walk, "Roam_Walk", 1, a, 0);
            } else if (sc == 1) {                           /* waiting: the timer, the player about 6000 away */
                roam_near(pl + 0x40, o + 0x40, rnd() % 2 ? 6000 + (s32)(rnd() % 3) - 1 : (s32)(rnd() % 12000));
                if (rnd() % 2) U8(o + 0x1E0) = 3;
                sprintf(g_what, "roam %X wait at %u", o, U8(o + 0x1D0));
                check(0x42D400, Roam_Wait, "Roam_Wait", 1, a, 0);
            } else if (sc == 2) {                           /* running along the path */
                u32 s = rnd() % nn, t = rnd() % nn;
                U8(o + 0x1D0) = (u8)s; Roam_Path(o);
                u32 on = t, steps = rnd() % 4;              /* where it is: on the path from s to t */
                for (u32 j = 0; j < steps && on != s; j++) on = U8(0x74AF38 + on);
                if (rnd() % 3 == 0) on = s;
                U8(o + 0x1D0) = (u8)on; U8(o + 0x1D1) = (u8)t;
                if (rnd() % 4) U8(RM_PT(o, t) + 6) &= ~2u;
                if (rnd() % 4 == 0) U8(RM_PT(o, t) + 6) |= 2;  /* (its exit shut) */
                gptr e = RM_PT(o, on);
                if (on == t && rnd() % 3 == 0) U8(e + 6) = (u8)((U8(e + 6) & ~5u) | 8);   /* the way out for good */
                roam_near(o + 0x40, e, rnd() % 2 ? 500 + (s32)(rnd() % 3) - 1 : (s32)(rnd() % 3000));
                if (rnd() % 2) {                            /* about 0x64 off */
                    static const s32 ed[] = {0, 1, -1, 0x63, 0x64, 0x65, -0x63, -0x64, -0x65, 0x100};
                    U16(o + 0x4A) = (u16)((Angle_XZ(o + 0x40, e) + ed[rnd() % 10]) & 0xFFF);
                }
                if (U8(RM_PT(o, t) + 6) & 2 && rnd() % 2) {   /* shut: near an exit */
                    u32 c = rnd() % nn;
                    s32 n = (s32)U32(0x7208AC + U8(G() + 0x1090) * 4);
                    roam_near(o + 0x40, RM_PT(o, c), rnd() % 2 ? n + (s32)(rnd() % 3) - 1 : (s32)(rnd() % 2000));
                    if (rnd() % 2) {                        /* the exit the only one in sight, at its edge */
                        roam_open(o); roam_only(o, c, n + (s32)(rnd() % 3) - 1);
                        if (rnd() % 3) U8(RM_PT(o, c) + 6) |= 1;
                    }
                }
                sprintf(g_what, "roam %X run %u to %u kind %X step %u", o, on, t, U8(e + 6), U8(o + 0x56));
                check(0x42D5C0, Roam_Run, "Roam_Run", 1, a, 0);
            } else if (sc == 3) {
                if (U8(0x74AF78) > 1 && rnd() % 2) {        /* two at the same place */
                    gptr p0 = RM_PT(o, U8(0x74AF18 + rnd() % U8(0x74AF78))), p1 = RM_PT(o, U8(0x74AF18 + rnd() % U8(0x74AF78)));
                    U16(p1) = U16(p0); U16(p1 + 4) = U16(p0 + 4);
                    U8(p0 + 6) &= ~2u; U8(p1 + 6) &= ~2u;
                    if (rnd() % 2) roam_near(o + 0x40, p0, 300 + rnd() % 2000);
                }
                sprintf(g_what, "roam %X hit sub %u", o, U8(o + 0x55));
                check(0x42D940, Roam_Hit, "Roam_Hit", 1, a, 0);
            } else if (sc == 4) {
                U8(o + 0x55) = (u8)(rnd() % 3);
                U8(pl + 0x54) = rnd() % 2 ? 2 : (u8)(rnd() % 6);
                U16(0x87DE10) = rand8_seed(rnd() % 2 ? 0 : 180, 0);
                check(0x42D820, Roam_Normal, "Roam_Normal", 1, a, 0);
            } else {                                        /* the handler; a wall pushing it out the way it faces */
                U8(o + 0x54) = (u8)(rnd() % 8);
                U32(o + 0x50) = (U32(o + 0x50) & ~0x40000010u) | (rnd() % 3 ? 0 : 0x10) | (rnd() % 8 ? 0 : 0x40000000);
                if (rnd() % 2) cl_place(o);
                else if (rnd() % 2) roam_wall(o);
                if (rnd() % 3 == 0) {
                    U16(o + 0xF0) = 0; U16(o + 0xF2) = 0; U16(o + 0xF4) = 0;
                    static u8 sv[0x260], sg[0x400], sq[8];
                    gptr q = CL_ROOM + U32(CL_ROOM + 0x3C);
                    memcpy(sv, MEM(o), 0x260); memcpy(sg, MEM(RM_G), 0x400); memcpy(sq, MEM(q), 8);
                    Roam_Type(o);
                    u16 h = U16(o + 0x4A), z = U16(o + 0x1D8), t = U8(o + 0x1D3);
                    memcpy(MEM(o), sv, 0x260); memcpy(MEM(RM_G), sg, 0x400); memcpy(MEM(q), sq, 8);
                    if (t) U16(o + 0x4A) = (u16)((z ? h : h - 3) & 0xFFF);
                }
                U32(LOG_BASE) = 0;
                sprintf(g_what, "roam %X handler state %u flags %X", o, U8(o + 0x54), U32(o + 0x50));
                check(0x42CD10, Roam_Type, "Roam_Type", 1, a, 0);
            }
        }
    }
    player_tables_stub(0);
    roam_tables_stub(0);
    stubs_install(0);
    g_ign_objpads = 0;
}

/* ---- the giganotosaurus (type 6) ---- */
static const u32 g_gi_tabs[][2] = {{0x720D48, 5}, {0x720D5C, 15}, {0x720D98, 15}, {0x720DD4, 4}, {0x720DE4, 4}, {0x720DF4, 15}};
static u32 g_gi_saved[6][15];
static void gig_tables_stub(int on)
{
    for (int k = 1; k <= 15; k++) g_stubs[k].spec = on ? "d" : "";
    for (int t = 0; t < 6; t++)
        for (u32 k = 0; k < g_gi_tabs[t][1]; k++) {
            gptr a = g_gi_tabs[t][0] + 4 * k;
            if (on) { g_gi_saved[t][k] = U32(a); U32(a) = (u32)(uintptr_t)g_recs[1 + k]; }
            else U32(a) = g_gi_saved[t][k];
        }
}

static const u32 g_gi_dist[] = {0, 0x1B57, 0x1B58, 0x1B59, 0x1F3F, 0x1F40, 0x1F41, 0x2327, 0x2328, 0x2329, 0x270F, 0x2710, 0x2711,
                                0x2AF7, 0x2AF8, 0x2AF9, 0x2EDF, 0x2EE0, 0x2EE1, 0x32C7, 0x32C8, 0x32C9, 0x7FFFFFFF};
static u32 gig_dist(void) { return rnd() % 4 ? g_gi_dist[rnd() % 23] : rnd() % 0x4000; }

static void gig_rand8(void)
{
    static const u8 r[] = {7, 8, 9, 0x1F, 0x20, 0x21, 0x3F, 0x40, 0x41, 0x7F, 0x80, 0x81, 0xC7, 0xC8, 0xC9, 0, 1, 0xFF};
    if (rnd() % 4) U16(0x87DE10) = rand8_seed(r[rnd() % 18], rnd() % 4 == 0);
}

/* its state block: bits, kind, the last state, timers */
static void gig_state(gptr o)
{
    U32(o + 0x1C0) = rnd() % 4 ? rnd() & 0x7FFF : rnd();
    U32(o + 0x1C4) = rnd() % 4 ? rnd() % 3 : rnd() % 2 ? 3 : rnd();
    U8(o + 0x1C8) = 1 + rnd() % 3; U8(o + 0x1C9) = rnd() % 15; U8(o + 0x1CA) = rnd() % 4; U8(o + 0x1CB) = rnd() % 2;
    for (u32 k = 0x1CC; k <= 0x1D4; k += 4) U32(o + k) = rnd() % 3 ? rnd() % 3 : rnd() % 2 ? rnd() % 0x2000 : rnd();
    for (u32 k = 0x1E0; k < 0x1FA; k += 2) U16(o + k) = rnd() % 3 ? rnd() % 3 : (u16)rnd();
    U16(o + 0x1F2) = rnd() % 4 ? rnd() % 11 : (u16)rnd();
    U16(o + 0x1F0) = rnd() % 2;
    U8(o + 0x18D) = rnd() % 3 ? 0 : (u8)rnd();
}

/* a giganotosaurus among the test objects (no animations); the player its target */
static gptr gig_setup(void)
{
    gptr o = raptor_setup(), pl = U32(G() + 0x940);
    U32(o + 0x184) &= ~0x40u;
    trex_parts(o);
    U32(o + 0x120) = AN_PART + 0xE * 0x80; U32(o + 0x124) = AN_PART + 0xA * 0x80;
    U8(o + 0x15F) = rnd() % 4 ? 3 : rnd() % 16;
    gig_state(o);
    U32(o + 0x13C) = gig_dist();
    U8(o + 0x13A) = rnd() % 3 == 0;
    U8(o + 0x57) = rnd() % 2;
    U16(pl + 0x174) = rnd() % 2 ? rnd() & 3 : (u16)rnd();
    U32(pl + 0x134) = 0;
    Bits_Set(0x87E974, 0, rnd() % 3 == 0); Bits_Set(0x87E974, 0x1F, rnd() % 3 == 0);
    U16(o + 0xD8) = U16(pl + 0x40); U16(o + 0xDA) = U16(pl + 0x42); U16(o + 0xDC) = U16(pl + 0x44);
    if (rnd() % 3 == 0) { S16(o + 0xD8) = (s16)(S16(o + 0x40) + (s32)(rnd() % 8000) - 4000); S16(o + 0xDC) = (s16)(S16(o + 0x44) + (s32)(rnd() % 8000) - 4000); }
    trex_aim(o + 0x40, o, o + 0xD8);
    trex_ff(o, rnd() % 2);
    gig_rand8();
    return o;
}

/* spheres on parts in the bridge's sections (or off them) */
static void gig_bridge_spheres(gptr o)
{
    u32 n = 1 + rnd() % 3;
    trex_spheres(o, n);
    for (u32 s = 0; s < n; s++) {
        gptr sa = TR_ATK + s * 0x10, q = 0x720D38 - 0x10 * (rnd() % 7);
        U8(sa + 8) = 0; U8(sa + 9) = 0x7F; U8(sa + 0xA) = (u8)(1 + s);
        gptr p = AN_PART + (1 + s) * 0x80;
        for (u32 j = 0x20; j < 0x32; j += 2) U16(p + j) = 0;
        s32 cx = 0, cz = 0;
        for (int c = 0; c < 4; c++) { cx += S16(q + 4 * c); cz += S16(q + 4 * c + 2); }
        S16(p + 0x40) = (s16)(cx / 4 + (rnd() % 4 ? 0 : 5000)); S16(p + 0x44) = (s16)(cz / 4);
    }
    U16(o + 0x92) = rnd() % 0x40;
}

typedef struct { u32 addr; void *fn; const char *name; u8 steps; } GigFn;

static void test_gig(void)
{
    g_ign_objpads = 1;
    stubs_install(1);
    gig_tables_stub(1);
    player_tables_stub(1);
    for (int i = 0; i < iters; i++) {
        game_state();
        gptr o = gig_setup(), b = o + 0x1C0, pl = U32(G() + 0x940);
        u32 a[3] = {o};
        /* the helpers */
        U8(pl + 0x54) = (u8[]){2, 5, 1, 3}[rnd() % 4]; U16(pl + 0x174) = (u16)rnd(); U8(G() + 0x108E) = rnd() % 3;
        U32(o + 0x1D0) = rnd() % 3 ? rnd() % 3 : rnd();
        check(0x436A90, Gig_Timer, "Gig_Timer", 1, a, 0xFFFFFFFF);
        U32(b) = rnd(); Bits_Set(0x87E974, 0, rnd() % 2);
        check(0x436B00, Gig_Calm, "Gig_Calm", 1, a, 0);
        for (int k = 0; k < 8; k++) {
            gig_state(o);
            U32(b) = (rnd() & 0x1E0) | (rnd() % 4 ? 0x20 : 0);
            U16(o + 0x1F2) = rnd() % 4 ? rnd() % 11 : (u16)rnd(); U16(o + 0x1F0) = rnd() % 2;
            U32(o + 0x13C) = rnd() % 2 ? (u32[]){0x32C7, 0x32C8, 0}[rnd() % 3] : gig_dist();
            u32 f = rnd() % 0x60;
            U16(o + 0x92) = (u16)(rnd() % 4 ? f : f + 1);
            if (rnd() % 4 == 0) f |= rnd() << 16;
            gig_rand8();
            u32 cb[2] = {o, f};
            sprintf(g_what, "gig %X combo bits %X n %u frame %X", o, U32(b), U16(o + 0x1F2), U16(o + 0x92));
            check(0x436B80, Gig_Combo, "Gig_Combo", 2, cb, 0);
        }
        for (int k = 0; k < 8; k++) {                   /* on the bridge: where the player and it are */
            game_state();
            o = gig_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
            U32(o + 0x1C4) = rnd() % 4 ? 1 : rnd() % 3;
            Bits_Set(b, 0xA, rnd() % 4 != 0);
            Flag_Set(7, 0x18, rnd() % 4 == 0);
            static const s16 oz[] = {0xBB7, 0xBB8, 0xBB9, 0xF9F, 0xFA0, 0xFA1, 0, -3000, 2000};
            S16(o + 0x44) = oz[rnd() % 9];
            S16(pl + 0x44) = (s16)(S16(o + 0x44) + (s16[]){0x1F3, 0x1F4, 0x1F5, 0, -100, 3000}[rnd() % 6]);
            sprintf(g_what, "gig %X on bridge kind %u z %d player %d", o, U32(o + 0x1C4), S16(o + 0x44), S16(pl + 0x44));
            check(0x4370B0, Gig_OnBridge, "Gig_OnBridge", 1, a, 0xFFFFFFFF);
        }
        for (int k = 0; k < 8; k++) {
            game_state();
            o = gig_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
            if (rnd() % 2) { Bits_Set(b, 0xA, 0); U8(o + 0x13A) = 0; Bits_Set(0x87E974, 0, 0); U32(o + 0x1D0) = rnd() % 4 == 0; }
            U32(o + 0x13C) = (u32[]){0x1B57, 0x1B58, 0x1B59, 0, 0x7FFFFFFF}[rnd() % 5];
            u32 tu[2] = {o, rnd() % 2 ? rnd() % 2 : (rnd() & ~0xFFu) | (rnd() % 2)};
            sprintf(g_what, "gig %X turn bits %X dist %X", o, U32(b), U32(o + 0x13C));
            check(0x437180, Gig_Turn, "Gig_Turn", 2, tu, 0xFFFFFFFF);
        }
        /* deciding */
        static const GigFn th[] = {
            {0x434400, Gig_Decide, "Gig_Decide"}, {0x433F20, Gig_ThinkWalk, "Gig_ThinkWalk"}, {0x4340C0, Gig_ThinkStand, "Gig_ThinkStand"},
            {0x4341D0, Gig_ThinkRun, "Gig_ThinkRun"}, {0x434BD0, Gig_ThinkFace, "Gig_ThinkFace"}, {0x434C70, Gig_ThinkBite, "Gig_ThinkBite"},
            {0x435080, Gig_ThinkSwipe, "Gig_ThinkSwipe"}, {0x4351B0, Gig_ThinkSnatch, "Gig_ThinkSnatch"}, {0x4353C0, Gig_ThinkMaul, "Gig_ThinkMaul"},
            {0x435780, Gig_ThinkCharge, "Gig_ThinkCharge"}};
        for (int k = 0; k < 80; k++) {
            const GigFn *f = &th[k < 48 ? k % 4 : k % 10];
            game_state();
            o = gig_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
            U8(o + 0x56) = rnd() % 6 ? 1 : (u8)(rnd() % 3);
            if (rnd() % 4) {                                /* nothing keeps it from attacking */
                U8(o + 0x13A) = 0; Bits_Set(0x87E974, 0, 0); U32(o + 0x1D0) = rnd() % 6 == 0; U8(o + 0x18D) = 0;
                Bits_Set(b, 0xA, rnd() % 6 == 0); U16(o + 0x1EC) = rnd() % 4 == 0; U16(o + 0x1EE) = rnd() % 4 == 0;
                U32(o + 0x1C4) = rnd() % 4 ? rnd() % 3 : 1;
                Flag_Set(2, 0x1A, rnd() % 4 == 0);
                u16 an = (u16)Angle_XZ(o + 0x40, o + 0xD8);
                U16(o + 0x4A) = (u16)((an - (u16[]){0, 1, 0x7F, 0x80, 0x81, 0xF81, 0xF80, 0xF7F}[rnd() % 8]) & 0xFFF);
                U32(o + 0x13C) = g_gi_dist[rnd() % 23];
                S16(pl + 0x10A) = (s16)(rnd() % 2 ? 100 + rnd() % 0x400 : rnd());
                S16(pl + 0x108) = (s16)(S16(pl + 0x10A) / 2 + (s32)(rnd() % 3) - 1);
                U16(pl + 0x174) = rnd() % 2 ? 2 : (u16)(rnd() & ~2u);
            }
            if (f->addr == 0x434BD0 || f->addr == 0x434C70 || f->addr == 0x435780)
                U16(o + 0x92) = (u16)((u16[]){0xE, 0xF, 0x10, 0x1D, 0x1E, 0x1F}[rnd() % 6]);
            if (f->addr == 0x435080 || f->addr == 0x4353C0) U16(o + 0x92) = (u16)((u16[]){0x13, 0x14, 0x15, 0x3B, 0x3C, 0x3D}[rnd() % 6]);
            if (f->addr == 0x4340C0 && rnd() % 2) U16(o + 0x1F6) = 0;
            if (rnd() % 4 == 0) U16(o + 0x1E2) = (u16)rnd();
            gig_rand8();
            u32 dc[2] = {o, rnd() % 2 ? rnd() % 2 : rnd()};
            U32(LOG_BASE) = 0;
            sprintf(g_what, "gig %X %s step %u kind %u dist %X bits %X", o, f->name, U8(o + 0x56), U32(o + 0x1C4), U32(o + 0x13C), U32(b));
            if (f->addr == 0x434400) check(f->addr, f->fn, f->name, 2, dc, 0xFFFFFFFF);
            else check(f->addr, f->fn, f->name, 1, a, 0);
        }
        /* the state handlers */
        game_state();
        o = gig_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
        U8(o + 0x54) = rnd() % 5; U8(o + 0x55) = rnd() % 15; U8(o + 0x56) = rnd() % 6;
        check(0x433E80, Gig_Normal, "Gig_Normal", 1, a, 0);
        U8(o + 0x55) = rnd() % 4;
        check(0x433EB0, Gig_Hit, "Gig_Hit", 1, a, 0);
        check(0x433ED0, Gig_Die, "Gig_Die", 1, a, 0);
        U8(o + 0x55) = rnd() % 15;
        if (rnd() % 2) trex_spheres(o, 1 + rnd() % 3);
        check(0x433EF0, Gig_Grab, "Gig_Grab", 1, a, 0);
        for (int k = 0; k < 12; k++) {
            game_state();
            o = gig_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
            U8(o + 0x54) = rnd() % 5; U8(o + 0x55) = rnd() % 15;
            U32(o + 0x1C4) = rnd() % 4 ? rnd() % 3 : 1;
            if (U32(o + 0x1C4) == 1 && rnd() % 2) Bits_Set(b, 9, 1);
            for (u32 j = 0x12; j <= 0x18; j++) Flag_Set(7, j, 1);   /* (the bridge: no section left to break) */
            Flag_Set(6, 0x8D, rnd() % 4 == 0 && Bits_Check(b, 9));
            U32(o + 0x50) = (U32(o + 0x50) & ~0x40000030u) | (rnd() & 0x30) | (rnd() % 8 == 0 ? 0x40000000 : 0);
            for (u32 j = 0x1EC; j <= 0x1F8; j += 2) U16(o + j) = rnd() % 3;
            U32(o + 0x1CC) = rnd() % 2; U32(o + 0x1D4) = rnd() % 2; U8(o + 0x18D) = rnd() % 2;
            U16(pl + 0x1FA) = (u16[]){0x7F, 0x80, 0x81, 0}[rnd() % 4];
            S16(pl + 0x44) = rnd() % 2 ? (s16)(0x2748 + rnd() % 3 - 1) : S16(pl + 0x44);
            if (rnd() % 2) trex_spheres(o, 1 + rnd() % 3);
            if (rnd() % 2) { trex_aim(Obj_Part(U32(o + 0x64), U8(o + 0x15F)) + 0x40, o, o + 0xD8); Bits_Set(b, 1, 1); }
            if (rnd() % 2) {                                /* the head where it stands, about its looking limit */
                gptr hp = Obj_Part(U32(o + 0x64), U8(o + 0x15F));
                U16(hp + 0x40) = U16(o + 0x40); U16(hp + 0x44) = U16(o + 0x44);
                u16 an = (u16)Angle_XZ(o + 0x40, o + 0xD8);
                U16(o + 0x4A) = (u16)((an - (u16[]){0x4FF, 0x500, 0x501, 0xB01, 0xB00, 0xAFF}[rnd() % 6]) & 0xFFF);
                U16(o + 0xF0) = U16(o + 0xF2) = U16(o + 0xF4) = 0; Bits_Set(b, 1, rnd() % 4 != 0); Bits_Set(b, 0xA, 0);
            }
            sprintf(g_what, "gig %X handler state %X kind %u bits %X", o, U32(o + 0x54), U32(o + 0x1C4), U32(b));
            check(0x4339B0, Gig_Handler, "Gig_Handler", 1, a, 0);
        }
        do model_data(0x63F500); while (U32(0x63F500 + 0x14) != 16);
        do anim_setup_at(AN_OBJ); while (U32(AN_OBJ + 0x68) != 16);
        memcpy(MEM(o + 0x60), MEM(AN_OBJ + 0x60), 0x50);
        U32(0x642D84) = AN_DATA;                            /* its first animation */
        do model_data(0x630000); while (U32(0x630000 + 0x14) != 16);   /* its own model, or the default one */
        U32(o + 0x60) = rnd() % 2 ? 0 : 0x630000;
        U8(G() + 0x108E) = rnd() % 3; U32(0x74AF98) = rnd(); U8(0x87E747) = rnd() % 2;
        static const u8 rm[][2] = {{5, 3}, {5, 2}, {4, 3}, {1, 1}};
        int r = rnd() % 4; U8(G() + 0x1091) = rm[r][0]; U8(G() + 0x1090) = rm[r][1];
        U8(G() + 0x927) = rnd() % 3 == 0;
        check(0x433C70, Gig_Init, "Gig_Init", 1, a, 0);
        /* moving, hit, dying: each step */
        static const GigFn mv[] = {
            {0x433F90, Gig_Walk, "Gig_Walk", 3}, {0x434120, Gig_Stand, "Gig_Stand", 3}, {0x4342A0, Gig_Run, "Gig_Run", 3},
            {0x434670, Gig_TurnStep, "Gig_TurnStep", 3}, {0x434730, Gig_Roar, "Gig_Roar", 3}, {0x434AA0, Gig_Threat, "Gig_Threat", 4},
            {0x434BF0, Gig_Shake, "Gig_Shake", 3}, {0x434CB0, Gig_Bite, "Gig_Bite", 4}, {0x434EF0, Gig_Bite2, "Gig_Bite2", 4},
            {0x4350B0, Gig_Swipe, "Gig_Swipe", 3}, {0x4351E0, Gig_Snatch, "Gig_Snatch", 5}, {0x4353F0, Gig_Maul, "Gig_Maul", 5},
            {0x4355D0, Gig_Lunge, "Gig_Lunge", 4}, {0x4357C0, Gig_Charge, "Gig_Charge", 4}, {0x435970, Gig_Smash, "Gig_Smash", 3},
            {0x436140, Gig_Damage, "Gig_Damage", 2}, {0x436740, Gig_Flinch, "Gig_Flinch", 3}, {0x436860, Gig_Stagger, "Gig_Stagger", 3},
            {0x436910, Gig_Down, "Gig_Down", 3}, {0x4369B0, Gig_Dead, "Gig_Dead", 3}, {0x436D90, Gig_Bridge, "Gig_Bridge", 1}};
        const unsigned nm = sizeof mv / sizeof mv[0];
        game_state(); world_setup(); enemy_state(); zones_setup();
        o = O_OBJ + 0x260 * (1 + rnd() % 6); a[0] = o; b = o + 0x1C0;
        anim_setup_at(O_OBJ);
        do anim_setup_at(o); while (U32(o + 0x68) != 16);
        memcpy(MEM(O_OBJ + 0x60), MEM(o + 0x60), 0x50);
        U32(G() + 0x940) = pl = O_OBJ;
        U32(pl + 0x128) = rnd() % 2 ? 0x720C78 : 0; U32(o + 0x128) = 0x720C78;
        U8(o + 0x58) = 6;
        u8 sv[0x260]; memcpy(sv, MEM(o), 0x260);
        for (unsigned k = 0; k < 14 * nm; k++) {
            const GigFn *f = &mv[k % nm];
            memcpy(MEM(o), sv, 0x260);
            gig_state(o);
            U8(o + 0x54) = rnd() % 4 ? 1 + rnd() % 3 : rnd() % 5;
            U8(o + 0x55) = rnd() % 15;
            U8(o + 0x56) = (u8)(rnd() % 8 ? rnd() % f->steps : rnd());
            U8(o + 0x57) = rnd() % 2;
            static const u16 fr[] = {0, 2, 8, 0x10, 0x11, 0x14, 0x1B, 0x1C, 0x25, 0x26, 0x28, 0x2B, 0x2C, 0x30, 0x37, 0x3C, 0x43, 0x46, 0x4D,
                                     0x50, 0x5B, 0x60, 0x69, 0x70, 0x85};
            U16(o + 0x92) = rnd() % 4 ? (u16)(fr[rnd() % 25] + rnd() % 3 - 1) : (u16)(rnd() % 0x90);
            trex_ff(o, rnd() % 2);
            U32(o + 0x13C) = gig_dist();
            for (u32 j = 0; j < 0x2000; j += 0x1000) {     /* the animations' frame flags and spheres: valid or none */
                U32(AN_DATA + j + 0xC) = rnd() % 2 ? TR_QUAD + 0x100 : 0; U32(AN_DATA + j + 8) = rnd() % 2 ? TR_ATK : 0;
            }
            U8(pl + 0x54) = (u8[]){1, 2, 5, 3}[rnd() % 4]; U16(pl + 0x174) = rnd() % 2 ? rnd() & 3 : (u16)rnd();
            Flag_Set(2, 0x1A, rnd() % 3 == 0);
            S16(pl + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x100 - 0x10 : rnd());
            S16(pl + 0x44) = rnd() % 2 ? (s16)(0x2748 + rnd() % 3 - 1) : (s16)rnd();
            /* the damage */
            U16(o + 0x172) = rnd() % 5; U16(o + 0x176) = rnd() % 2 ? rnd() % 0x400 : (u16)rnd();
            S16(o + 0x10A) = (s16)(rnd() % 2 ? rnd() % 0x2000 : rnd());
            S16(o + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x800 - 0x10 : rnd());
            U8(o + 0x170) = (u8)rnd(); U8(o + 0x184) = (u8)rnd(); U8(o + 0x17D) = rnd() % 2 ? (u8[]){0x19, 4, 0}[rnd() % 3] : (u8)rnd();
            U16(o + 0x178) = (u16)((U16(o + 0x4A) + 0x800 + rnd() % 3 - 1 + (rnd() % 2 ? 0 : rnd())) & 0xFFF);
            U32(0x87E974) = rnd();
            if (f->addr == 0x436140) {
                if (rnd() % 2) U8(o + 0x56) = 0;
                if (rnd() % 2) U8(o + 0x1C8) = 2;
                if (rnd() % 2) U8(o + 0x1CA) = 0;
                U32(o + 0x1C4) = rnd() % 4 ? rnd() % 3 : rnd();
                U16(o + 0x1F8) = rnd() % 2 ? 0 : (u16)rnd();
                if (rnd() % 2) U16(o + 0x172) = 3;
                u16 d = U16(o + 0x176);
                if (U8(o + 0x184) & 8) d = (u16)(d + (d >> 1));
                if (!U32(o + 0x1C4) && U8(o + 0x17D) != 0x19) d = d / 3;
                s32 m = S16(o + 0x10A);
                switch (rnd() % 4) {                        /* a limit reached exactly, or about */
                case 0: U32(o + 0x1CC) = (u32)(m / 30 - d + (s32)(rnd() % 3) - 1); Bits_Set(b, 2, 1); break;
                case 1: U32(o + 0x1CC) = (u32)(m / 11 - (U16(o + 0x172) == 3 ? d : d >> 1) + (s32)(rnd() % 3) - 1); Bits_Set(b, 2, 0); U16(o + 0x1F8) = 0; break;
                case 2: U32(o + 0x1D4) = (u32)(m / 4 - d + (s32)(rnd() % 3) - 1); Bits_Set(b, 5, 1); break;
                }
                if (rnd() % 3 == 0) S16(o + 0x108) = (s16)(d + (s32)(rnd() % 3) - 1);
                if (rnd() % 3 == 0) { U8(o + 0x56) = 1; S16(o + 0x108) = (s16)(d / 5 + (s32)(rnd() % 3) - 1); }
                U8(o + 0x11C) = (u8)rnd();
                for (u32 j = 0; j < 0x20; j++) S8(0x87F989 + j * 0x10) = (s8)(u8[]){0x38, 0x39, 0x3C, 0x3B, 0}[rnd() % 5];   /* sound groups it stops */
            }
            if (f->addr == 0x436D90) {                      /* the bridge scene */
                U32(o + 0x1C4) = rnd() % 4 ? 1 : rnd() % 3;
                Flag_Set(6, 0x8D, rnd() % 2);
                Bits_Set(b, 9, rnd() % 2); Bits_Set(b, 0xA, rnd() % 4 != 0); Bits_Set(b, 0xE, rnd() % 4 == 0);
                for (u32 j = 0x12; j <= 0x18; j++) Flag_Set(7, j, rnd() % 4 == 0);
                gig_bridge_spheres(o);
                U32(AN_DATA + 0x1008) = rnd() % 2 ? TR_ATK : 0;  /* (the scene's animation: its spheres) */
                if (rnd() % 2) S16(pl + 0x44) = (s16)(S16(0x720D3A - 0x10 * (rnd() % 7)) + rnd() % 3 - 1);
            }
            if (f->addr == 0x435970) {                      /* the smash: caught on the bridge */
                Bits_Set(b, 0xB, rnd() % 2);
                if (rnd() % 2) U8(o + 0x56) = 1;
            }
            U8(G() + 0x108E) = rnd() % 3; U8(G() + 0x108F) = rnd() % 2 ? rnd() % 12 : (u8)rnd();
            for (u32 j = 0; j < 20; j++) if (rnd() % 2) U8(O_K2 + j * 0xAC + 0x50) &= ~1;
            if (rnd() % 2) { U16(o + 0xF0) = (u16)(rnd() % 0x600 - 0x300); U16(o + 0xF2) = (u16)(rnd() % 0x600 - 0x300); U16(o + 0xF4) = (u16)(rnd() % 0x300); }
            U16(0x87DC12) = rnd() % 2 ? 0 : (u16)rnd();
            gig_rand8();
            anim_maybe_end(o);
            if (rnd() % 2) U16(o + 0x102) |= 0x20;
            static const struct { u32 addr; u8 step, n; u16 fr[8]; } edge[] = {
                {0x434670, 1, 3, {0x1B, 0x1C, 0x11}}, {0x434730, 1, 6, {0x2B, 0x2C, 0x2D, 0x4C, 0x4D, 0x4E}},
                {0x434BF0, 1, 2, {0x37, 0x38}}, {0x434CB0, 1, 3, {0x2C, 0x3C, 0x3B}}, {0x434EF0, 1, 2, {0x46, 0x45}},
                {0x4355D0, 1, 3, {0x3C, 0x1B, 0x1C}}, {0x4357C0, 1, 3, {0x50, 0x43, 0x42}}, {0x4350B0, 0, 6, {0x1C, 0x1D, 0x3B, 0x3C, 0x10, 0x26}},
                {0x4351E0, 2, 4, {8, 0x50, 0x10, 9}}, {0x4353F0, 1, 4, {0x60, 0x69, 0x70, 0x6A}}, {0x4353F0, 2, 2, {0x70, 0x71}},
                {0x4353F0, 3, 2, {0x14, 0x15}}, {0x435970, 1, 6, {0x3C, 0x3D, 0x28, 0x29, 0x25, 0x27}}, {0x436740, 1, 2, {0x14, 0x15}},
                {0x4369B0, 1, 2, {0x85, 0x84}}, {0x433F90, 1, 2, {0x5B, 0x2B}}, {0x4342A0, 1, 2, {0x10, 0x30}}};
            for (unsigned e = 0; e < sizeof edge / sizeof edge[0]; e++)
                if (edge[e].addr == f->addr && rnd() % 2) {
                    U8(o + 0x56) = edge[e].step; U16(o + 0x92) = edge[e].fr[rnd() % edge[e].n]; U16(o + 0x102) |= 0x20;
                }
            if ((f->addr == 0x434CB0 || f->addr == 0x434EF0 || f->addr == 0x4355D0 || f->addr == 0x4357C0) && rnd() % 2) {
                U32(b) = (U32(b) & ~0x1E0u) | (rnd() % 4 ? 0x20 : 0) | (rnd() & 0x1C0);   /* a combo */
                U16(o + 0x1F2) = rnd() % 10;
                U32(o + 0x13C) = (u32[]){0x32C7, 0x32C8, 0x1000}[rnd() % 3];
            }
            if (f->addr == 0x434730 && rnd() % 2) {         /* the roar: the player free to be held or not */
                U8(pl + 0x54) = (u8[]){1, 3, 2, 5}[rnd() % 4]; U16(pl + 0x174) = (u16)(rnd() % 4); Flag_Set(2, 0x1A, rnd() % 4 == 0);
                for (u32 j = 0xB58; j <= 0xB5C; j++) U8(G() + j) = (u8)rnd();
            }
            if (f->addr == 0x434AA0 && rnd() % 2) {         /* the threat: the player ahead in reach */
                u16 an = (u16)Angle_XZ(o + 0x40, o + 0xD8);
                U16(o + 0x4A) = (u16)((an - (u16[]){0, 0x7F, 0x80, 0xF81, 0xF80}[rnd() % 5]) & 0xFFF);
                U32(o + 0x13C) = (u32[]){0x270F, 0x2710}[rnd() % 2];
            }
            U32(LOG_BASE) = 0;
            sprintf(g_what, "gig %X %s step %u side %u frame %X kind %u bits %X", o, f->name, U8(o + 0x56), U8(o + 0x57), U16(o + 0x92), U32(o + 0x1C4), U32(b));
            u32 r1 = f->addr == 0x436D90 ? 0xFFFFFFFF : 0;
            check(f->addr, f->fn, f->name, 1, a, r1);
        }
        /* the attacks land */
        static const GigFn gh[] = {{0x435B60, Gig_BiteHit, "Gig_BiteHit"}, {0x435CC0, Gig_Bite2Hit, "Gig_Bite2Hit"},
                                   {0x435DB0, Gig_SwipeHit, "Gig_SwipeHit"}, {0x435E90, Gig_MaulHit, "Gig_MaulHit"},
                                   {0x435EF0, Gig_LungeHit, "Gig_LungeHit"}, {0x435FE0, Gig_ChargeHit, "Gig_ChargeHit"},
                                   {0x4360D0, Gig_SmashHit, "Gig_SmashHit"}};
        for (int k = 0; k < 42; k++) {
            memcpy(MEM(o), sv, 0x260);
            U8(o + 0x56) = rnd() % 4 ? 1 : (u8)(rnd() % 3);
            U8(o + 0x57) = rnd() % 2;
            U32(o + 0x1C0) = rnd() & 0x7FFF;
            U32(o + 0x1C4) = rnd() % 4 ? rnd() % 3 : rnd();
            U8(G() + 0x108E) = rnd() % 3; U8(G() + 0x108F) = rnd() % 5;
            Flag_Set(2, 0x1A, rnd() % 3 == 0);
            U16(pl + 0x4A) = (u16)((U16(o + 0x4A) + (u16[]){0, 0x3FF, 0x400, 0x401, 0xC01, 0xC00, 0xBFF, 0x800}[rnd() % 8]) & 0xFFF);
            S16(pl + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x100 - 0x10 : rnd());
            for (u32 j = 0; j < 20; j++) if (rnd() % 2) U8(O_K2 + j * 0xAC + 0x50) &= ~1;
            if (k % 7 == 0 && rnd() % 2) {                 /* the bite's floor probe (part 3, 0x7D0 ahead) at a floor's edge */
                gptr p3 = Obj_Part(U32(o + 0x64), 3), w = 0x6CFC00u, pg = w + 0x10;
                for (u32 j = 0x20; j < 0x34; j += 2) U16(p3 + j) = 0;
                U16(p3 + 0x20) = U16(p3 + 0x28) = U16(p3 + 0x30) = 0x1000;
                S16(p3 + 0x40) = 0; S16(p3 + 0x42) = -2000; S16(p3 + 0x44) = 0;
                U8(w) = 1; U32(w + 8) = 0x10;
                U8(pg) = 1; U8(pg + 1) = 4; U8(pg + 2) = 0; U8(pg + 3) = 0; U16(pg + 4) = 1; U8(pg + 6) = 0; U8(pg + 7) = 0;
                for (u32 j = 8; j < 0x18; j++) U8(pg + j) = 0;
                s16 z1 = (s16)(0x7D0 + (s32)(rnd() % 3) - 1), z0 = (s16)(0x7D0 - 1000);
                static const s16 px[5] = {-500, 500, 500, -500, -500};
                for (int c = 0; c < 5; c++) { S16(pg + 0x18 + 4 * c) = px[c]; S16(pg + 0x1A + 4 * c) = c == 2 || c == 3 ? z1 : z0; }
                if (rnd() % 2) for (int c = 0; c < 5; c++) S16(pg + 0x1A + 4 * c) = c == 2 || c == 3 ? z0 : z1;
                U32(G() + 0xA4C) = w;
            }
            U32(LOG_BASE) = 0;
            sprintf(g_what, "gig %X %s step %u side %u bits %X", o, gh[k % 7].name, U8(o + 0x56), U8(o + 0x57), U32(o + 0x1C0));
            check(gh[k % 7].addr, gh[k % 7].fn, gh[k % 7].name, 1, a, 0);
        }
        for (int k = 0; k < 24; k++) {                      /* the player stunned by the roar, falling with the bridge */
            memcpy(MEM(o), sv, 0x260);
            anim_maybe_end(pl);
            U32(pl + 0x1D8) = o;
            U8(pl + 0x57) = rnd() % 3 ? (rnd() % 3 ? 0 : 1) : (u8)rnd();
            U32(o + 0x1C0) = rnd() & 0x1F; U32(o + 0x1C4) = rnd() % 4 ? rnd() % 3 : rnd();
            U16(o + 0x1E2) = (u16)(rnd() % 2 ? 0xF + rnd() % 0x10 : rnd());
            U16(o + 0x1F4) = rnd() % 3; U16(o + 0x1EA) = (u16)rnd();
            U8(0x87DC08) = (u8)rnd(); U8(G() + 0x10B4) = rnd() % 2 ? 0 : (u8)rnd(); U8(G() + 0x108E) = rnd() % 3;
            U16(G() + 0x664) = rnd() % 2 ? 4 : (u16)(rnd() % 8);
            U16(pl + 0x92) = (u16)(rnd() % 2 ? 0x46 + rnd() % 3 - 1 : rnd() % 0x50);
            if (rnd() % 2) U16(pl + 0x102) |= 0x20;
            u32 p1[1] = {pl};
            U32(LOG_BASE) = 0;
            sprintf(g_what, "gig %X player step %u bits %X", o, U8(pl + 0x57), U32(o + 0x1C0));
            check(0x434900, Gig_RoarStun, "Gig_RoarStun", 1, p1, 0);
            anim_maybe_end(pl);
            if (rnd() % 2) U16(pl + 0x102) |= 0x20;
            check(0x435B00, Gig_BridgeFall, "Gig_BridgeFall", 1, p1, 0);
        }
    }
    player_tables_stub(0);
    gig_tables_stub(0);
    stubs_install(0);
    g_ign_objpads = 0;
}

/* ---- the inostrancevia (type 14) ---- */
static const u32 g_in_tabs[][2] = {{0x720E80, 5}, {0x720E94, 9}, {0x720EB8, 9}, {0x720EDC, 9}, {0x720F00, 9}, {0x720F24, 9}};
static u32 g_in_saved[6][9];
static void ino_tables_stub(int on)
{
    for (int k = 1; k <= 9; k++) g_stubs[k].spec = on ? "d" : "";
    for (int t = 0; t < 6; t++)
        for (u32 k = 0; k < g_in_tabs[t][1]; k++) {
            gptr a = g_in_tabs[t][0] + 4 * k;
            if (on) { g_in_saved[t][k] = U32(a); U32(a) = (u32)(uintptr_t)g_recs[1 + k]; }
            else U32(a) = g_in_saved[t][k];
        }
}

static const u32 g_in_dist[] = {0, 0xBB7, 0xBB8, 0xBB9, 0xDAB, 0xDAC, 0xDAD, 0xF9F, 0xFA0, 0xFA1, 0x157B, 0x157C, 0x157D,
                                0x1769, 0x176F, 0x1770, 0x1771, 0x1963, 0x1964, 0x1965, 0x270F, 0x2710, 0x2711, 0x7FFFFFFF};
static u32 ino_dist(void) { return rnd() % 4 ? g_in_dist[rnd() % 24] : rnd() % 0x3000; }

static void ino_rand8(void)
{
    static const u8 r[] = {0xF, 0x10, 0x11, 0x3F, 0x40, 0x41, 0x7F, 0x80, 0x81, 0, 1, 0xFF};
    if (rnd() % 4) U16(0x87DE10) = rand8_seed(r[rnd() % 12], rnd() % 4 == 0);
}

/* its state block: bits, the last state, timers */
static void ino_state(gptr o)
{
    U32(o + 0x1C0) = rnd() % 4 ? rnd() & 0x7F : rnd();
    U8(o + 0x1C8) = 1 + rnd() % 3; U8(o + 0x1C9) = rnd() % 9; U8(o + 0x1CA) = rnd() % 4; U8(o + 0x1CB) = rnd() % 2;
    for (u32 k = 0x1CC; k <= 0x1D8; k += 4) U32(o + k) = rnd() % 3 ? rnd() % 3 : rnd() % 2 ? rnd() % 0x2000 : rnd();
    for (u32 k = 0x1EC; k < 0x216; k += 2) U16(o + k) = rnd() % 3 ? rnd() % 3 : (u16)rnd();
    U8(o + 0x216) = rnd() % 2;
    U8(o + 0x18D) = rnd() % 3 ? rnd() % 3 : (u8)rnd();
}

/* an inostrancevia among the test objects (no animations); the player its target */
static gptr ino_setup(void)
{
    gptr o = raptor_setup(), pl = U32(G() + 0x940);
    U32(o + 0x184) &= ~0x40u;
    trex_parts(o);
    U8(o + 0x15F) = rnd() % 4 ? 3 : rnd() % 16;
    ino_state(o);
    U32(o + 0x13C) = ino_dist();
    U8(o + 0x13A) = rnd() % 3 == 0;
    U8(o + 0x57) = rnd() % 2;
    U16(pl + 0x174) = rnd() % 2 ? rnd() & 3 : (u16)rnd();
    U32(pl + 0x134) = 0;
    Bits_Set(0x87E974, 0, rnd() % 3 == 0); Bits_Set(0x87E974, 1, rnd() % 3 == 0); Bits_Set(0x87E974, 2, rnd() % 3 == 0);
    U16(o + 0xD8) = U16(pl + 0x40); U16(o + 0xDA) = U16(pl + 0x42); U16(o + 0xDC) = U16(pl + 0x44);
    if (rnd() % 3 == 0) { S16(o + 0xD8) = (s16)(S16(o + 0x40) + (s32)(rnd() % 8000) - 4000); S16(o + 0xDC) = (s16)(S16(o + 0x44) + (s32)(rnd() % 8000) - 4000); }
    trex_aim(o + 0x40, o, o + 0xD8);
    trex_ff(o, rnd() % 2);
    ino_rand8();
    return o;
}

static void test_ino(void)
{
    g_ign_objpads = 1;
    stubs_install(1);
    ino_tables_stub(1);
    player_tables_stub(1);
    for (int i = 0; i < iters; i++) {
        game_state();
        gptr o = ino_setup(), b = o + 0x1C0, pl = U32(G() + 0x940);
        u32 a[3] = {o};
        U8(o + 0x216) = rnd() % 2 ? rnd() % 2 : (u8)rnd();
        check(0x4397A0, Ino_Side, "Ino_Side", 1, a, 0xFFFFFFFF);
        /* deciding */
        static const GigFn th[] = {
            {0x437960, Ino_Decide, "Ino_Decide"}, {0x437780, Ino_ThinkWait, "Ino_ThinkWait"}, {0x437860, Ino_ThinkWalk, "Ino_ThinkWalk"},
            {0x437BF0, Ino_ThinkRoar, "Ino_ThinkRoar"}, {0x437CC0, Ino_ThinkFall, "Ino_ThinkFall"}, {0x438110, Ino_ThinkBite, "Ino_ThinkBite"},
            {0x4381F0, Ino_ThinkGrab, "Ino_ThinkGrab"}};
        for (int k = 0; k < 80; k++) {
            const GigFn *f = &th[k < 48 ? k % 3 : k % 7];
            game_state();
            o = ino_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
            U8(o + 0x56) = rnd() % 6 ? 1 : (u8)(rnd() % 3);
            if (rnd() % 4) {                                /* nothing keeps it from attacking */
                U8(o + 0x13A) = 0; Bits_Set(0x87E974, 0, rnd() % 4 == 0); U32(o + 0x1D4) = rnd() % 6 == 0;
                U8(o + 0x18D) = rnd() % 4 ? 0 : 1;
                U16(o + 0x208) = rnd() % 4 == 0; U16(o + 0x20A) = rnd() % 4 == 0;
                Flag_Set(2, 0x1A, rnd() % 4 == 0);
                u16 an = (u16)Angle_XZ(o + 0x40, o + 0xD8);
                static const u16 ae[] = {0, 1, 0x3F, 0x40, 0x41, 0xFF, 0x100, 0x101, 0x3FF, 0x400, 0x401, 0x800,
                                         0xFFF, 0xFC1, 0xFC0, 0xFBF, 0xF01, 0xF00, 0xEFF, 0xC01, 0xC00, 0xBFF};
                U16(o + 0x4A) = (u16)((an - ae[rnd() % 22]) & 0xFFF);
                static const u16 pe[] = {0, 0x5F, 0x60, 0x61, 0xFA1, 0xFA0, 0xF9F, 0x800};
                U16(pl + 0x4A) = (u16)((U16(o + 0x4A) + pe[rnd() % 8]) & 0xFFF);
                U32(o + 0x13C) = g_in_dist[rnd() % 24];
                U16(pl + 0x174) = rnd() % 2 ? 2 : (u16)(rnd() & ~2u);
            }
            if (f->addr == 0x438110) U16(o + 0x92) = (u16)((u16[]){0x13, 0x14, 0x15, 0}[rnd() % 4]);
            if (rnd() % 4 == 0) U16(o + 0x1E2) = (u16)rnd();
            ino_rand8();
            U32(LOG_BASE) = 0;
            sprintf(g_what, "ino %X %s step %u dist %X bits %X", o, f->name, U8(o + 0x56), U32(o + 0x13C), U32(b));
            check(f->addr, f->fn, f->name, 1, a, f->addr == 0x437960 ? 0xFFFFFFFF : 0);
        }
        /* the state handlers */
        game_state();
        o = ino_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
        U8(o + 0x54) = rnd() % 5; U8(o + 0x55) = rnd() % 9; U8(o + 0x56) = rnd() % 6;
        check(0x4376D0, Ino_Normal, "Ino_Normal", 1, a, 0);
        check(0x437710, Ino_Hit, "Ino_Hit", 1, a, 0);
        check(0x437730, Ino_Die, "Ino_Die", 1, a, 0);
        for (int k = 0; k < 6; k++) {
            U8(o + 0x55) = rnd() % 9; U8(o + 0x56) = rnd() % 3;
            if (rnd() % 2) trex_spheres(o, 1 + rnd() % 3);
            if (rnd() % 2) U32(pl + 0x134) = 0;              /* (no hit boxes: the last hit is kept) */
            U8(o + 0x119) = (u8[]){0, 1, 0x7F, 0x80, 0xFF}[rnd() % 5];
            check(0x437750, Ino_Grab, "Ino_Grab", 1, a, 0);
        }
        for (int k = 0; k < 12; k++) {
            game_state();
            o = ino_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
            U8(o + 0x54) = rnd() % 5; U8(o + 0x55) = rnd() % 9;
            if (rnd() % 3 == 0) { U8(o + 0x54) = 1; U8(o + 0x55) = 4; }
            U32(o + 0x50) = (U32(o + 0x50) & ~0x40000038u) | (rnd() & 0x38) | (rnd() % 8 == 0 ? 0x40000000 : 0);
            static const u8 rm[][2] = {{8, 0xB}, {8, 0xA}, {7, 0xB}, {1, 1}};
            int r = rnd() % 4; U8(G() + 0x1091) = rm[r][0]; U8(G() + 0x1090) = rm[r][1];
            Flag_Set(7, 0x1F, rnd() % 2);
            U8(0x87E747) = rnd() % 3 == 0;
            if (rnd() % 2) {                                /* standing at the floor's height, or about */
                s16 h = (s16)Floor_Height(o + 0x40, o + 0x40, 0x40, 0);
                S16(o + 0x42) = (s16)(h + (s32)(rnd() % 3) - 1);
            }
            U32(o + 0x13C) = (u32[]){0x270F, 0x2710, 0x2711, 0}[rnd() % 4];
            U32(o + 0x1CC) = rnd() % 2; U32(o + 0x1D0) = rnd() % 2; U8(o + 0x18D) = rnd() % 2;
            U16(pl + 0x1FA) = (u16[]){0x7F, 0x80, 0x81, 0}[rnd() % 4];
            if (rnd() % 2) trex_spheres(o, 1 + rnd() % 3);
            if (rnd() % 2) {                                /* the head where it stands, about its looking limit */
                gptr hp = Obj_Part(U32(o + 0x64), U8(o + 0x15F));
                U16(hp + 0x40) = U16(o + 0x40); U16(hp + 0x44) = U16(o + 0x44);
                u16 an = (u16)Angle_XZ(o + 0x40, o + 0xD8);
                U16(o + 0x4A) = (u16)((an - (u16[]){0x4FF, 0x500, 0x501, 0xB01, 0xB00, 0xAFF}[rnd() % 6]) & 0xFFF);
                U16(o + 0xF0) = U16(o + 0xF2) = U16(o + 0xF4) = 0; Bits_Set(b, 1, rnd() % 4 != 0);
            }
            sprintf(g_what, "ino %X handler state %X flags %X bits %X", o, U32(o + 0x54), U32(o + 0x50), U32(b));
            check(0x437230, Ino_Handler, "Ino_Handler", 1, a, 0);
        }
        /* 16 parts, as the animation: another count changes the frame stride and the
           key lengths read could add up to 0 (a division by zero in Anim_ApplyFrame) */
        do model_data(0x640000); while (U32(0x640000 + 0x14) != 16);
        do anim_setup_at(AN_OBJ); while (U32(AN_OBJ + 0x68) != 16);
        memcpy(MEM(o + 0x60), MEM(AN_OBJ + 0x60), 0x50);
        U32(0x642350) = AN_DATA;                           /* its first animation */
        U32(o + 0x60) = rnd() % 2 ? 0 : 0x640000;
        U8(G() + 0x108E) = rnd() % 2 ? rnd() % 3 : (u8)rnd(); U32(0x74AF9C) = rnd(); U8(0x87E747) = rnd() % 2;
        U8(G() + 0x927) = rnd() % 3 == 0; S16(o + 0x42) = (s16)rnd();
        check(0x437540, Ino_Init, "Ino_Init", 1, a, 0);
        /* moving, hit, dying: each step */
        static const GigFn mv[] = {
            {0x437800, Ino_Wait, "Ino_Wait", 2}, {0x4378B0, Ino_Walk, "Ino_Walk", 2}, {0x437C10, Ino_Roar, "Ino_Roar", 2},
            {0x437ED0, Ino_Turn, "Ino_Turn", 2}, {0x437CE0, Ino_Fall, "Ino_Fall", 2}, {0x438060, Ino_Bite, "Ino_Bite", 2},
            {0x438140, Ino_Bite2, "Ino_Bite2", 2}, {0x438210, Ino_Eat, "Ino_Eat", 5}, {0x438650, Ino_Pounce, "Ino_Pounce", 3},
            {0x438B40, Ino_Damage, "Ino_Damage", 2}, {0x438F70, Ino_Flinch, "Ino_Flinch", 2}, {0x439000, Ino_Knock, "Ino_Knock", 2},
            {0x4390B0, Ino_Stagger, "Ino_Stagger", 4}, {0x4391A0, Ino_Down, "Ino_Down", 4}, {0x4392D0, Ino_Hit5, "Ino_Hit5", 2},
            {0x439370, Ino_Hit6, "Ino_Hit6", 2}, {0x4393E0, Ino_Hit7, "Ino_Hit7", 2}, {0x4394B0, Ino_Hit8, "Ino_Hit8", 2},
            {0x439520, Ino_Dead, "Ino_Dead", 3}, {0x4395F0, Ino_DeadDown, "Ino_DeadDown", 4}, {0x4396D0, Ino_DeadFire, "Ino_DeadFire", 3}};
        const unsigned nm = sizeof mv / sizeof mv[0];
        game_state(); world_setup(); enemy_state(); zones_setup();
        o = O_OBJ + 0x260 * (1 + rnd() % 6); a[0] = o; b = o + 0x1C0;
        anim_setup_at(O_OBJ);
        do anim_setup_at(o); while (U32(o + 0x68) != 16);
        memcpy(MEM(O_OBJ + 0x60), MEM(o + 0x60), 0x50);
        U32(G() + 0x940) = pl = O_OBJ;
        U32(pl + 0x128) = rnd() % 2 ? 0x720C78 : 0; U32(o + 0x128) = 0x720E58;
        U8(o + 0x58) = 14;
        u8 sv[0x260], svp[0x260]; memcpy(sv, MEM(o), 0x260); memcpy(svp, MEM(pl), 0x260);
        for (unsigned k = 0; k < 14 * nm; k++) {
            const GigFn *f = &mv[k % nm];
            memcpy(MEM(o), sv, 0x260); memcpy(MEM(pl), svp, 0x260);
            ino_state(o);
            U8(o + 0x54) = rnd() % 4 ? 1 + rnd() % 3 : rnd() % 5;
            U8(o + 0x55) = rnd() % 9;
            U8(o + 0x56) = (u8)(rnd() % 8 ? rnd() % f->steps : rnd());
            U8(o + 0x57) = rnd() % 2;
            static const u16 fr[] = {0, 0xA, 0xE, 0x12, 0x14, 0x15, 0x16, 0x18, 0x24, 0x2D, 0x31, 0x36, 0x3C, 0x44, 0x5A, 0x64};
            U16(o + 0x92) = rnd() % 4 ? (u16)(fr[rnd() % 16] + rnd() % 3 - 1) : (u16)(rnd() % 0x80);
            U16(pl + 0x92) = rnd() % 4 ? (u16)(0x5A + rnd() % 3 - 1) : (u16)(rnd() % 0x80);
            trex_ff(o, rnd() % 2);
            U32(o + 0x13C) = ino_dist();
            for (u32 j = 0; j < 0x2000; j += 0x1000) {     /* the animations' frame flags and spheres: valid or none */
                U32(AN_DATA + j + 0xC) = rnd() % 2 ? TR_QUAD + 0x100 : 0; U32(AN_DATA + j + 8) = rnd() % 2 ? TR_ATK : 0;
            }
            U8(pl + 0x54) = (u8[]){1, 2, 5, 3}[rnd() % 4]; U16(pl + 0x174) = rnd() % 2 ? rnd() & 3 : (u16)rnd();
            Flag_Set(2, 0x1A, rnd() % 3 == 0);
            S16(pl + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x100 - 0x10 : rnd());
            /* the damage */
            U16(o + 0x172) = rnd() % 6; U16(o + 0x176) = rnd() % 2 ? rnd() % 0x400 : (u16)rnd();
            S16(o + 0x10A) = (s16)(rnd() % 2 ? rnd() % 0x2000 : rnd());
            S16(o + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x800 - 0x10 : rnd());
            U8(o + 0x170) = (u8)rnd(); U8(o + 0x184) = (u8)rnd();
            U8(o + 0x17D) = rnd() % 4 ? (u8[]){0, 1, 2, 3, 4, 9, 0x10, 0x11, 0x12, 0x13, 0x14}[rnd() % 11] : (u8)rnd();
            U32(0x87E974) = rnd();
            if (f->addr == 0x438B40) {
                if (rnd() % 2) U8(o + 0x56) = 0;
                if (rnd() % 2) U8(o + 0x1C8) = 2;
                if (rnd() % 2) U8(o + 0x1CA) = 0;
                Bits_Set(b, 2, rnd() % 2);
                u16 d = U16(o + 0x176);                     /* the damage after its scaling, about a limit */
                if (U8(o + 0x184) & 8) d = (u16)(d + (d >> 1));
                if (Bits_Check(b, 3)) d >>= 2;
                u8 w = U8(o + 0x17D);
                if (w == 1 || w == 2 || w == 9 || w == 0x10 || w == 0x11) d >>= 1;
                if (w == 4 && Bits_Check(b, 2)) d <<= 1;
                if (U16(o + 0x172) == 4) d >>= 1;
                if (rnd() % 2) U32(o + 0x1D0) = (u32)(S16(o + 0x10A) / 3 - d + (s32)(rnd() % 3) - 1);
                if (rnd() % 2) S16(o + 0x108) = (s16)(d + (s32)(rnd() % 3) - 1);
                U16(o + 0x204) = rnd() % 2 ? 0 : (u16)rnd();
            }
            if (f->addr == 0x437CE0) {                      /* the fall: below the floor's level or not */
                S16(o + 0x42) = (s16)(rnd() % 3) - 1;
                S16(o + 0x214) = (s16)((s16[]){0, 1, -1, 4, 0xC8}[rnd() % 5]);
                U8(o + 0x202) = rnd() % 5;
            }
            if (f->addr == 0x4378B0) S16(o + 0x210) = (s16)((s16[]){0x77, 0x78, 0x7FFF, 0}[rnd() % 4]);
            if (f->addr == 0x438210 && rnd() % 2) {          /* eaten: the player's struggle about enough */
                U8(o + 0x56) = 3;
                U32(o + 0x1D8) = (u32)(0x13 + rnd() % 3);
                U16(o + 0x92) = (u16)((u16[]){0x14, 0x63, 0x64, 0x65}[rnd() % 4]);
            }
            U8(G() + 0x108E) = rnd() % 3; U8(G() + 0x108F) = rnd() % 2 ? rnd() % 9 : (u8)rnd();
            for (u32 j = 0; j < 20; j++) if (rnd() % 2) U8(O_K2 + j * 0xAC + 0x50) &= ~1;
            if (rnd() % 2) { U16(o + 0xF0) = (u16)(rnd() % 0x600 - 0x300); U16(o + 0xF2) = (u16)(rnd() % 0x600 - 0x300); U16(o + 0xF4) = (u16)(rnd() % 0x300); }
            U16(0x87DC12) = rnd() % 2 ? 0 : (u16)rnd();
            ino_rand8();
            anim_maybe_end(o);
            if (rnd() % 2) U16(o + 0x102) |= 0x20;
            static const struct { u32 addr; u8 step, n; u16 fr[8]; } edge[] = {
                {0x437C10, 1, 4, {0xD, 0xE, 0x31, 0x32}}, {0x438060, 1, 4, {0xD, 0xE, 0x18, 0x19}},
                {0x438140, 1, 4, {0x14, 0x15, 0x24, 0x25}}, {0x438210, 1, 5, {0x14, 0x15, 0x35, 0x36, 0x3C}},
                {0x438210, 4, 2, {0x43, 0x44}}, {0x438650, 2, 5, {0x12, 0x13, 0x59, 0x5A, 0x11}},
                {0x4391A0, 3, 2, {0x16, 0x15}}, {0x4392D0, 1, 2, {0xF, 0x10}}, {0x439520, 1, 2, {0x2D, 0x2C}},
                {0x4395F0, 1, 2, {0x16, 0x17}}, {0x4395F0, 2, 2, {0xA, 0xB}}, {0x4396D0, 2, 2, {0xA, 0xB}}};
            for (unsigned e = 0; e < sizeof edge / sizeof edge[0]; e++)
                if (edge[e].addr == f->addr && rnd() % 2) {
                    U8(o + 0x56) = edge[e].step; U16(o + 0x92) = edge[e].fr[rnd() % edge[e].n]; U16(o + 0x102) |= 0x20;
                }
            if (f->addr == 0x437ED0 || f->addr == 0x438210) U8(o + 0x57) &= 1;   /* (indexes of its tables) */
            if ((f->addr == 0x438210 || f->addr == 0x438650) && rnd() % 2)       /* the player's health about the bite's */
                S16(pl + 0x108) = (s16)(Damage_Scale(f->addr == 0x438210 ? 0x64 : 0xC8) + rnd() % 3 - 1);
            U32(LOG_BASE) = 0;
            sprintf(g_what, "ino %X %s step %u side %u frame %X bits %X", o, f->name, U8(o + 0x56), U8(o + 0x57), U16(o + 0x92), U32(b));
            check(f->addr, f->fn, f->name, 1, a, 0);
        }
        /* the attacks land */
        static const GigFn gh[] = {{0x4388F0, Ino_BiteHit, "Ino_BiteHit"}, {0x438970, Ino_Bite2Hit, "Ino_Bite2Hit"},
                                   {0x4389F0, Ino_EatHit, "Ino_EatHit"}};
        for (int k = 0; k < 30; k++) {
            memcpy(MEM(o), sv, 0x260); memcpy(MEM(pl), svp, 0x260);
            U8(o + 0x56) = rnd() % 4 ? 1 : (u8)(rnd() % 3);
            U32(o + 0x1C0) = rnd() & 0x7F; U8(o + 0x216) = rnd() % 2;
            Flag_Set(2, 0x1A, rnd() % 3 == 0);
            U16(pl + 0x4A) = (u16)((U16(o + 0x4A) + (u16[]){0, 0x3FF, 0x400, 0x401, 0xC01, 0xC00, 0xBFF, 0x800}[rnd() % 8]) & 0xFFF);
            S16(pl + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x100 - 0x10 : rnd());
            for (u32 j = 0; j < 20; j++) if (rnd() % 2) U8(O_K2 + j * 0xAC + 0x50) &= ~1;
            U32(LOG_BASE) = 0;
            sprintf(g_what, "ino %X %s step %u bits %X", o, gh[k % 3].name, U8(o + 0x56), U32(o + 0x1C0));
            check(gh[k % 3].addr, gh[k % 3].fn, gh[k % 3].name, 1, a, 0);
        }
        for (int k = 0; k < 12; k++) {                      /* the player thrown free */
            memcpy(MEM(pl), svp, 0x260);
            anim_maybe_end(pl);
            if (rnd() % 2) U16(pl + 0x102) |= 0x20;
            u32 p1[1] = {pl};
            U32(LOG_BASE) = 0;
            sprintf(g_what, "ino player frame %X", U16(pl + 0x92));
            check(0x4385E0, Ino_EatEnd, "Ino_EatEnd", 1, p1, 0);
        }
    }
    player_tables_stub(0);
    ino_tables_stub(0);
    stubs_install(0);
    g_ign_objpads = 0;
}

/* ---- the triceratops (type 9) ---- */
static const u32 g_tri_tabs[][2] = {{0x72110C, 5}, {0x721120, 6}, {0x721138, 6}, {0x721150, 4}, {0x721160, 4}, {0x721170, 6}};
static u32 g_tri_saved[6][6];
static void tri_tables_stub(int on)
{
    for (int k = 1; k <= 6; k++) g_stubs[k].spec = on ? "d" : "";
    for (int t = 0; t < 6; t++)
        for (u32 k = 0; k < g_tri_tabs[t][1]; k++) {
            gptr a = g_tri_tabs[t][0] + 4 * k;
            if (on) { g_tri_saved[t][k] = U32(a); U32(a) = (u32)(uintptr_t)g_recs[1 + k]; }
            else U32(a) = g_tri_saved[t][k];
        }
}

/* the next Rand8 (most of the time), returned; else -1 */
static int tri_rand8(void)
{
    static const u8 r[] = {0, 1, 0xF, 0x10, 0x11, 0x17, 0x18, 0x19, 0x1F, 0x20, 0x21, 0x3F, 0x40, 0x41, 0x7F, 0x80, 0x81, 0xFE, 0xFF};
    if (rnd() % 4 == 0) return -1;
    u8 v = r[rnd() % 19];
    U16(0x87DE10) = rand8_seed(v, rnd() % 4 == 0);
    return v;
}

/* v about an edge e: e-1, e, e+1, or anything */
static s16 tri_about(s32 e) { return (s16)(rnd() % 4 ? e + (s32)(rnd() % 3) - 1 : (s32)(rnd() % 0x10000) - 0x8000); }

/* its state block: bits, the last state, timers, its phase and its values */
static void tri_state(gptr o)
{
    U32(o + 0x1C0) = rnd() % 4 ? rnd() & 0x1FFF : rnd();
    U32(o + 0x1C4) = rnd() % 4 ? rnd() % 2 : rnd() % 2 ? 2 : rnd();
    U8(o + 0x1C8) = rnd() % 3 ? 1 + rnd() % 3 : 2; U8(o + 0x1C9) = rnd() % 3 ? rnd() % 6 : 2;
    U8(o + 0x1CA) = rnd() % 3; U8(o + 0x1CB) = rnd() % 2;
    U32(o + 0x1CC) = rnd() % 2 ? rnd() % 0x500 : rnd() % 2 ? 0 : rnd();
    U32(o + 0x1D0) = rnd() % 3 ? 0 : rnd() % 3 ? 1 + rnd() % 0x14 : rnd();
    U16(o + 0x1E6) = (u16)(rnd() % 0x100 - 0x80);
    U32(o + 0x1E8) = rnd() % 3 ? (u32[]){0, 1, 2, 4, 5, 7, 8, 0xB, 0xC, 0xD, 0x10, 0x16}[rnd() % 12] : rnd() % 0x7FFF;
    for (u32 k = 0x1EC; k <= 0x1F8; k += 2) U16(o + k) = rnd() % 3 ? rnd() % 3 : (u16)rnd();
    U16(o + 0x1F0) = rnd() % 8 ? rnd() % 7 : rnd() % 2 ? 7 : (u16)rnd();
    U16(o + 0x1F2) = rnd() % 6;
    U16(o + 0x1F4) = rnd() % 3 ? 0xFFFF : rnd() % 2 ? U16(o + 0x1F2) : rnd() % 6;
    U16(o + 0x1FA) = (u16[]){0, 0x80, 0xFF80, 1, 0xFFFF, 0x80, 0xFF80}[rnd() % 7];
    U16(o + 0x1F4) = 0xFFFF; Tri_Phase(o);           /* its values: a phase's, or anything */
    if (rnd() % 4 == 0) for (u32 k = 0x1FC; k < 0x238; k += 2) U16(o + k) = (u16)(rnd() % 4 ? rnd() % 0x6000 - 0x3000 : rnd());
    if (rnd() % 4 == 0) U32(o + 0x238) = rnd() % 2 ? rnd() % 0x800 : rnd();
    U16(o + 0x1F4) = rnd() % 2 ? U16(o + 0x1F2) : rnd() % 3 ? 0xFFFF : (u16)rnd();
    U16(o + 0x23C) = rnd() % 3 ? 0 : rnd() % 2 ? 1 + rnd() % 2 : (u16)rnd();
    U16(o + 0x23E) = rnd() % 4 ? 2 + rnd() % 3 : (u16)rnd();
    U8(o + 0x240) = (u8)rnd(); U8(o + 0x241) = rnd() % 7;
    for (u32 k = 0x242; k < 0x248; k += 2) U16(o + k) = (u16)rnd();
    U8(o + 0x24A) = (u8)rnd();
}

/* a triceratops among the test objects (no animations); the player its target */
static gptr tri_setup(void)
{
    gptr o = raptor_setup(), pl = U32(G() + 0x940);
    U32(o + 0x184) &= ~0x40u;
    trex_parts(o);
    U8(o + 0x15F) = rnd() % 4 ? 0xC : rnd() % 16;
    tri_state(o);
    U8(o + 0x57) = rnd() % 2;
    U16(pl + 0x174) = rnd() % 2 ? rnd() & 3 : (u16)rnd();
    U32(pl + 0x134) = 0;
    U32(0x87E974) = rnd() % 4 ? rnd() & 0x1F : rnd();
    U16(o + 0xD8) = U16(pl + 0x40); U16(o + 0xDA) = U16(pl + 0x42); U16(o + 0xDC) = U16(pl + 0x44);
    trex_aim(o + 0x40, o, o + 0xD8);
    trex_ff(o, rnd() % 2);
    return o;
}

/* across the street (o+0x40) and along it (o+0x44), about its edges */
static void tri_place(gptr o)
{
    s32 hw = S16(o + 0x236) >> 1, ww = S16(o + 0x202);
    s32 xe[] = {0, 0x1F40, 0x1F41, 0x2EDF, 0x2EE0, 0x32C7, 0x32C8, 0x59D8, 0x59D9, hw, hw + 1, ww, 1, 0x4000};
    s32 x = xe[rnd() % 14] + (rnd() % 3 == 0 ? (s32)(rnd() % 3) - 1 : 0);
    S16(o + 0x40) = rnd() % 5 ? (s16)(rnd() % 2 ? x : -x) : (s16)(rnd() % 0xC000 - 0x6000);
    s32 ze[] = {0x2710, 0x2711, 0x32C7, 0x32C8, -10000, S16(o + 0x200), S16(o + 0x210), S16(o + 0x212), S16(o + 0x214),
                S16(o + 0x216), S16(o + 0x230), S16(o + 0x232), S16(o + 0x234), 0x59D8, 0};
    S16(o + 0x44) = rnd() % 5 ? tri_about(ze[rnd() % 15]) : (s16)(rnd() % 0xC000 - 0x6000);
}

static void test_tri(void)
{
    g_ign_objpads = 1;
    stubs_install(1);
    tri_tables_stub(1);
    player_tables_stub(1);
    for (int i = 0; i < iters; i++) {
        game_state(); world_setup();
        gptr o = tri_setup(), b = o + 0x1C0, pl = U32(G() + 0x940);
        u32 a[3] = {o};
        /* deciding, steering, the phase, the effects */
        static const GigFn th[] = {
            {0x43D780, Tri_Steer, "Tri_Steer"}, {0x43DBE0, Tri_Decide, "Tri_Decide"}, {0x43D440, Tri_ThinkRun, "Tri_ThinkRun"},
            {0x43D650, Tri_ThinkAfter, "Tri_ThinkAfter"}, {0x43DF30, Tri_Face, "Tri_Face"}, {0x43EBD0, Tri_Timer, "Tri_Timer"},
            {0x43EC40, Tri_Recover, "Tri_Recover"}, {0x43EDD0, Tri_Phase, "Tri_Phase"}, {0x43ECC0, Tri_Dust, "Tri_Dust"},
            {0x43F6D0, Tri_Debris, "Tri_Debris"}};
        for (int k = 0; k < 100; k++) {
            const GigFn *f = &th[k < 40 ? k % 2 : k % 10];
            game_state(); world_setup();
            o = tri_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
            U8(o + 0x55) = rnd() % 4 ? rnd() % 2 : rnd() % 6;
            U8(o + 0x56) = rnd() % 6 ? 1 : (u8)(rnd() % 3);
            tri_place(o);
            if (rnd() % 2) {                                /* nothing keeps it from attacking */
                U16(o + 0x23C) = 0; U32(o + 0x1D0) = 0; U16(o + 0x1EC) = 0;
                Bits_Set(0x87E974, 0, rnd() % 6 == 0);
            }
            int r = tri_rand8();                            /* its chances about the roll */
            if (r >= 0 && rnd() % 2) {
                static const u32 ch[] = {0x208, 0x20A, 0x20C, 0x20E};
                U16(o + ch[rnd() % 4]) = (u16)(r + (s32)(rnd() % 3) - 1);
            }
            if (rnd() % 3 == 0) {                           /* the player straight ahead, about */
                u16 an = (u16)Angle_XZ(o + 0x40, pl + 0x40);
                U16(o + 0x4A) = (u16)((an + rnd() % 3 - 1) & 0xFFF);
            }
            S16(U32(0x87E760) + 0x2A0) = (s16)(rnd() % 3 - 1);
            if (rnd() % 3 == 0) {                           /* each way it steers, about its edges */
                s32 hw = S16(o + 0x236) >> 1, sg = rnd() % 2 ? 1 : -1;
                switch (rnd() % 5) {
                case 0:                                     /* weaving far down the street */
                    U16(o + 0x1F0) = 2; U8(o + 0x55) = 0; U16(o + 0x20A) = U16(o + 0x20C) = 0;
                    S16(o + 0x44) = tri_about(-10001); S16(o + 0x40) = (s16)(sg * (hw + (s32)(rnd() % 2)));
                    break;
                case 1:                                     /* to the far side, about its z range */
                    U16(o + 0x1F0) = 5; U16(o + 0x20E) = 0x100;
                    S16(o + 0x44) = tri_about(S16(o + (rnd() % 2 ? 0x210 : 0x212)));
                    break;
                case 2:                                     /* the roar, its chance about the roll */
                    U16(o + 0x1F0) = rnd() % 2 ? 0 : 2; U16(o + 0x23C) = 0; U32(o + 0x1D0) = 0; U16(o + 0x1EC) = 0;
                    Bits_Set(0x87E974, 0, 0);
                    S16(o + 0x44) = (s16)(0x2711 + rnd() % 0xBB6);
                    U16(o + 0x208) = (u16)(r < 0 ? 0x80 : r + (s32)(rnd() % 2));
                    break;
                case 3:                                     /* the gore, from either side of the middle */
                    U16(o + 0x1F0) = 3; U16(o + 0x23C) = 0; U32(o + 0x1D0) = 0; Bits_Set(0x87E974, 0, 0);
                    S16(o + 0x44) = (s16)(S16(o + 0x234) - 1); S16(o + 0x40) = (s16)(rnd() % 3 - 1);
                    break;
                default:
                    break;
                }
            }
            /* the player and its dodging (Tri_Timer) */
            U8(pl + 0x54) = (u8[]){1, 2, 5, 3, 0}[rnd() % 5]; U8(pl + 0x174) = (u8)rnd();
            U8(G() + 0x108E) = rnd() % 3;
            /* the phase: health about its limits */
            S16(o + 0x10A) = (s16)(rnd() % 2 ? 0x1F40 : rnd() % 0x4000);
            s32 mx = S16(o + 0x10A);
            s32 he[] = {mx * 3 / 4, mx / 3, mx * 2 / 3, 0, mx};
            S16(o + 0x108) = tri_about(he[rnd() % 5]);
            for (u32 j = 0; j < 20; j++) if (rnd() % 2) U8(O_K2 + j * 0xAC + 0x50) &= ~1;
            a[1] = rnd() % 2 ? (o & ~0xFFu) | (rnd() % 2) : rnd();
            U32(LOG_BASE) = 0;
            sprintf(g_what, "tri %X %s steer %u phase %u x %X z %X bits %X", o, f->name, U16(o + 0x1F0), U16(o + 0x1F2),
                    U16(o + 0x40), U16(o + 0x44), U32(b));
            u32 ret = f->addr == 0x43D780 || f->addr == 0x43DBE0 || f->addr == 0x43EBD0 ? 0xFFFFFFFF : 0;
            check(f->addr, f->fn, f->name, f->addr == 0x43F6D0 ? 2 : 1, a, ret);
        }
        /* the state handlers */
        game_state(); world_setup();
        o = tri_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
        U8(o + 0x55) = rnd() % 4;
        check(0x43D3A0, Tri_Normal, "Tri_Normal", 1, a, 0);
        U8(o + 0x55) = rnd() % 4;
        check(0x43D3D0, Tri_Hit, "Tri_Hit", 1, a, 0);
        U8(o + 0x55) = rnd() % 4;
        check(0x43D3F0, Tri_Die, "Tri_Die", 1, a, 0);
        for (int k = 0; k < 6; k++) {
            U8(o + 0x55) = rnd() % 6; U8(o + 0x56) = rnd() % 3;
            if (rnd() % 2) trex_spheres(o, 1 + rnd() % 3);
            if (rnd() % 2) U32(pl + 0x134) = 0;
            U8(o + 0x119) = (u8[]){0, 1, 0x7F, 0x80, 0xFF}[rnd() % 5];
            check(0x43D410, Tri_Grab, "Tri_Grab", 1, a, 0);
        }
        for (int k = 0; k < 12; k++) {
            game_state(); world_setup();
            o = tri_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
            U8(o + 0x54) = rnd() % 5; U8(o + 0x55) = rnd() % 6;
            U32(o + 0x50) = (U32(o + 0x50) & ~0x40000030u) | (rnd() & 0x30) | (rnd() % 8 == 0 ? 0x40000000 : 0);
            if (rnd() % 2) U32(o + 0x1C4) = 1;
            if (rnd() % 2) {                                /* the player far, about the tint's and the dust's limits */
                s32 d = 0x7530 - 300 + (s32)(rnd() % 1600);
                S16(o + 0x40) = (s16)(-0x3C00 + (s32)(rnd() % 0x200)); S16(pl + 0x40) = (s16)(S16(o + 0x40) + d);
                S16(pl + 0x44) = S16(o + 0x44);
            }
            U16(o + 0x23E) = (u16[]){2, 3, 4, 0xFFFF, 0x7FFF}[rnd() % 5];
            U16(o + 0x23C) = rnd() % 3 ? (u16)(rnd() % 3) : (u16)rnd();
            U16(pl + 0x1FA) = (u16[]){0x7F, 0x80, 0x81, 0}[rnd() % 4];
            U8(o + 0x17B) = rnd() % 2 ? 0 : (u8)rnd();
            U8(G() + 0x108F) = rnd() % 2 ? 0 : (u8)rnd();
            if (rnd() % 2) trex_spheres(o, 1 + rnd() % 3);
            if (rnd() % 2) {                                /* the head where it stands, about its looking limit */
                gptr hp = Obj_Part(U32(o + 0x64), U8(o + 0x15F));
                U16(hp + 0x40) = U16(o + 0x40); U16(hp + 0x44) = U16(o + 0x44);
                u16 an = (u16)Angle_XZ(o + 0x40, o + 0xD8);
                U16(o + 0x4A) = (u16)((an - (u16[]){0x4FF, 0x500, 0x501, 0xB01, 0xB00, 0xAFF}[rnd() % 6]) & 0xFFF);
                U16(o + 0xF0) = U16(o + 0xF2) = U16(o + 0xF4) = 0; Bits_Set(b, 1, rnd() % 4 != 0);
            }
            for (u32 j = 0; j < 20; j++) if (rnd() % 2) U8(O_K2 + j * 0xAC + 0x50) &= ~1;
            sprintf(g_what, "tri %X handler state %X flags %X bits %X", o, U32(o + 0x54), U32(o + 0x50), U32(b));
            check(0x43CF30, Tri_Handler, "Tri_Handler", 1, a, 0);
        }
        /* set up: 16 parts, as the animation */
        do model_data(0x650000); while (U32(0x650000 + 0x14) != 16);
        do anim_setup_at(AN_OBJ); while (U32(AN_OBJ + 0x68) != 16);
        memcpy(MEM(o + 0x60), MEM(AN_OBJ + 0x60), 0x50);
        U32(0x652DC4) = AN_DATA;                            /* its first animation */
        U16(o + 0x10C) = rnd() % 3 ? rnd() % 2 : (u16)rnd();
        U8(G() + 0x108E) = rnd() % 3; U8(0x87E747) = rnd() % 2; S16(o + 0x42) = (s16)rnd();
        check(0x43D1E0, Tri_Init, "Tri_Init", 1, a, 0);
        /* moving, hit, dying: each step */
        static const GigFn mv[] = {
            {0x43D470, Tri_Run, "Tri_Run", 2}, {0x43D670, Tri_After, "Tri_After", 2}, {0x43DD00, Tri_Roar, "Tri_Roar", 2},
            {0x43DDB0, Tri_Swerve, "Tri_Swerve", 2}, {0x43DF50, Tri_Charge, "Tri_Charge", 2}, {0x43E070, Tri_Gore, "Tri_Gore", 2},
            {0x43E1B0, Tri_ChargeHit, "Tri_ChargeHit", 2}, {0x43E200, Tri_GoreHit, "Tri_GoreHit", 2},
            {0x43E250, Tri_Damage, "Tri_Damage", 1}, {0x43E6A0, Tri_Flinch, "Tri_Flinch", 2}, {0x43E880, Tri_Knock, "Tri_Knock", 2},
            {0x43E9D0, Tri_Dead, "Tri_Dead", 3}, {0x43EAB0, Tri_DeadRun, "Tri_DeadRun", 3}};
        const unsigned nm = sizeof mv / sizeof mv[0];
        game_state(); world_setup(); enemy_state(); zones_setup();
        o = O_OBJ + 0x260 * (rnd() % 2 ? 1 : 1 + rnd() % 6); a[0] = o; b = o + 0x1C0;   /* (the second object: the lead one) */
        anim_setup_at(O_OBJ);
        do anim_setup_at(o); while (U32(o + 0x68) != 16);
        memcpy(MEM(O_OBJ + 0x60), MEM(o + 0x60), 0x50);
        U32(G() + 0x940) = pl = O_OBJ;
        U32(pl + 0x128) = rnd() % 2 ? 0x720C78 : 0; U32(o + 0x128) = 0x7210D8;
        U32(0x87E760) = O_OBJ;                              /* the objects */
        U8(o + 0x58) = 9;
        u8 sv[0x260], svp[0x260]; memcpy(sv, MEM(o), 0x260); memcpy(svp, MEM(pl), 0x260);
        for (unsigned k = 0; k < 14 * nm; k++) {
            const GigFn *f = &mv[k % nm];
            memcpy(MEM(o), sv, 0x260); memcpy(MEM(pl), svp, 0x260);
            tri_state(o);
            U8(o + 0x54) = rnd() % 4 ? 1 + rnd() % 3 : rnd() % 5;
            U8(o + 0x55) = rnd() % 4;
            U8(o + 0x56) = (u8)(rnd() % 8 ? rnd() % f->steps : rnd());
            U8(o + 0x57) = rnd() % 2;
            tri_place(o);
            static const u16 fr[] = {0, 1, 2, 3, 0xA, 0xB, 0xE, 0xF, 0x10, 0x18, 0x19, 0x22, 0x23, 0x2C, 0x2D, 0x40};
            U16(o + 0x92) = rnd() % 4 ? fr[rnd() % 16] : (u16)(rnd() % 0x80);
            trex_ff(o, rnd() % 2);
            for (u32 j = 0; j < 0x2000; j += 0x1000) {     /* the animations' frame flags and spheres: valid or none */
                U32(AN_DATA + j + 0xC) = rnd() % 2 ? TR_QUAD + 0x100 : 0; U32(AN_DATA + j + 8) = rnd() % 2 ? TR_ATK : 0;
            }
            U8(pl + 0x54) = (u8[]){1, 2, 5, 3}[rnd() % 4]; U16(pl + 0x174) = rnd() % 2 ? rnd() & 3 : (u16)rnd();
            S16(pl + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x100 - 0x10 : rnd());
            /* the damage */
            U16(o + 0x172) = rnd() % 6; U16(o + 0x176) = rnd() % 2 ? rnd() % 0x400 : (u16)rnd();
            S16(o + 0x10A) = (s16)(rnd() % 2 ? 0x1F40 : rnd() % 0x4000);
            S16(o + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x800 - 0x10 : rnd());
            U8(o + 0x184) = (u8)rnd();
            U32(0x87E974) = rnd() % 4 ? rnd() & 0x1F : rnd();
            if (f->addr == 0x43E250) {
                if (rnd() % 2) U8(o + 0x56) = 0;
                if (rnd() % 3 == 0) { U8(o + 0x1C8) = 2; U8(o + 0x1C9) = 2; }
                if (rnd() % 2) {                            /* the player about its limits */
                    s32 d = rnd() % 2 ? 10000 : 8000;
                    S16(pl + 0x40) = (s16)(S16(o + 0x40) + d + (s32)(rnd() % 3) - 1); S16(pl + 0x44) = S16(o + 0x44);
                }
                if (rnd() % 2) U32(o + 0x238) = U32(o + 0x1CC) + 0xC8 + rnd() % 3 - 1;
                static const u16 hp[] = {0xC8, 0x64, 0x190, 0x258, 0x352, 0x44C, 0x578, 0x7D0, 0x12C, 0x1C2, 0x28};
                if (rnd() % 2) S16(o + 0x108) = tri_about(hp[rnd() % 11]);
                gptr c = U32(0x87E760);                     /* the others' health about a third */
                S16(c + 0x5CA) = (s16)(rnd() % 0x2000); S16(c + 0x5C8) = tri_about(S16(c + 0x5CA) / 3);
                S16(c + 0x36A) = (s16)(rnd() % 0x2000); S16(c + 0x368) = tri_about(S16(c + 0x36A) / 3);
                U16(o + 0x1F2) = (u16[]){0, 2, 5, 3}[rnd() % 4];
                if (rnd() % 3 == 0) {                       /* killed in the herd's last phases */
                    U8(o + 0x56) = 0; U8(o + 0x1C8) = 1; U32(o + 0x1C0) &= ~0x800u;
                    S16(o + 0x108) = (s16)(rnd() % 0xC8); U32(0x87E974) = 4 | (rnd() & 0x13);
                }
            }
            if ((f->addr == 0x43E9D0 || f->addr == 0x43EAB0) && rnd() % 2) S16(o + 0x44) = tri_about(0x59D8);
            if (rnd() % 3 == 0)                             /* its speed near one it heads for */
                U16(o + 0xF4) = (u16)(U16(o + 0x21C + 2 * (rnd() % 10)) + rnd() % 0x30 - 0x18);
            U8(G() + 0x108E) = rnd() % 3; U8(G() + 0x108F) = rnd() % 2 ? rnd() % 9 : (u8)rnd();
            for (u32 j = 0; j < 20; j++) if (rnd() % 2) U8(O_K2 + j * 0xAC + 0x50) &= ~1;
            if (rnd() % 2) { U16(o + 0xF0) = (u16)(rnd() % 0x600 - 0x300); U16(o + 0xF2) = (u16)(rnd() % 0x600 - 0x300); U16(o + 0xF4) = (u16)(rnd() % 0x600 - 0x300); }
            U16(0x87DC12) = rnd() % 2 ? 0 : (u16)rnd();
            tri_rand8();
            anim_maybe_end(o);
            if (rnd() % 2) U16(o + 0x102) |= 0x20;
            U32(LOG_BASE) = 0;
            sprintf(g_what, "tri %X %s step %u side %u frame %X bits %X", o, f->name, U8(o + 0x56), U8(o + 0x57), U16(o + 0x92), U32(b));
            check(f->addr, f->fn, f->name, 1, a, 0);
        }
    }
    player_tables_stub(0);
    tri_tables_stub(0);
    stubs_install(0);
    g_ign_objpads = 0;
}

/* ---- the mosasaurus (type 10) ---- */
static const u32 g_mo_tabs[][2] = {{0x7211E4, 5}, {0x7211F8, 8}, {0x721218, 8}, {0x721238, 7}, {0x721254, 7}, {0x721270, 8}};
static u32 g_mo_saved[6][8];
static void mo_tables_stub(int on)
{
    for (int k = 1; k <= 8; k++) g_stubs[k].spec = on ? "d" : "";
    for (int t = 0; t < 6; t++)
        for (u32 k = 0; k < g_mo_tabs[t][1]; k++) {
            gptr a = g_mo_tabs[t][0] + 4 * k;
            if (on) { g_mo_saved[t][k] = U32(a); U32(a) = (u32)(uintptr_t)g_recs[1 + k]; }
            else U32(a) = g_mo_saved[t][k];
        }
}

/* the distance to the player (o+0x13C) about its limits */
static u32 mo_dist(void)
{
    static const u32 e[] = {0x7D0, 0xBB8, 0xE10, 0xFA0, 0x1770, 0x400, 0x2000};
    return rnd() % 5 ? e[rnd() % 7] + rnd() % 3 - 1 : rnd() % 0x3000;
}

/* its state block, the player's hold on it */
static void mo_state(gptr o)
{
    gptr pl = U32(G() + 0x940);
    U32(o + 0x1C0) = rnd() % 4 ? rnd() & 0x1F : rnd();
    U8(o + 0x1C8) = rnd() % 4 ? 1 + rnd() % 3 : rnd() % 5; U8(o + 0x1C9) = rnd() % 8;
    U8(o + 0x1CA) = rnd() % 5; U8(o + 0x1CB) = rnd() % 2;
    U32(o + 0x1CC) = rnd() % 2 ? rnd() % 0x800 : rnd() % 2 ? 0 : rnd();
    U32(o + 0x1D0) = rnd() % 3 ? 0 : rnd() % 3 ? 1 + rnd() % 0x78 : rnd();
    U16(o + 0x1D8) = (u16)(rnd() % 3 ? rnd() % 0x40 : rnd()); U16(o + 0x1DA) = (u16)(rnd() % 3 ? rnd() % 0x40 : rnd());
    U16(o + 0x1DE) = rnd() % 2 ? 0 : (u16)rnd();
    U16(o + 0x1E6) = (u16[]){0, 1, 2, 0x78, 0xFFFF}[rnd() % 5];
    U16(o + 0x1EE) = rnd() % 3 ? 0x12C : rnd() % 2 ? 0x96 : (u16)(rnd() % 0x800);
    for (u32 k = 0x1F2; k < 0x1F8; k += 2) U16(o + k) = (u16)(U16(o + 0x40 + k - 0x1F2) + rnd() % 0x6000 - 0x3000);
    if (rnd() % 3 == 0) {                               /* its point about 0x895440 / 0x17D7840 off */
        s32 d = rnd() % 2 ? 3000 : 5000;
        U16(o + 0x1F2) = (u16)(U16(o + 0x40) + d + rnd() % 3 - 1); U16(o + 0x1F6) = U16(o + 0x44);
    }
    U16(o + 0x1FA) = rnd() % 2 ? 0 : (u16)(rnd() % 0x3C); U16(o + 0x1FC) = rnd() % 2 ? 0 : (u16)(rnd() % 0x3C);
    U16(o + 0x1FE) = (u16)(rnd() % 4 ? (u32)((rnd() % 2 ? 0x1E : -0x1E) + (s32)(rnd() % 5) - 2) : rnd()); U16(o + 0x200) = rnd() % 2;
    U16(o + 0x202) = (u16)tri_about(0x78);
    U16(o + 0x204) = (u16)(rnd() % 0x2000 - 0x1000);
    U16(o + 0x208) = (u16[]){0, 1, 4, 8, 0xC, 0xFFFC, 7}[rnd() % 7];
    for (u32 k = 0x20A; k < 0x210; k += 2) U16(o + k) = (u16)(rnd() % 0x2000 - 0x1000);
    U8(o + 0x13A) = rnd() % 3 ? 0 : (u8)rnd();
    U32(o + 0x13C) = mo_dist();
    U8(o + 0x18D) = rnd() % 2 ? 0 : rnd() % 2 ? 1 + rnd() % 2 : (u8)rnd();
    U16(o + 0x48) = rnd() % 3 ? (u16)tri_about((rnd() % 2 ? 1 : -1) * (s32)(rnd() % 2 ? 0x3E0 : 0x40)) : (u16)rnd();
    U16(o + 0x172) = rnd() % 6; U8(o + 0x17D) = rnd() % 3 ? 0x12 : (u8)rnd();
    U16(o + 0x176) = rnd() % 2 ? rnd() % 0x400 : (u16)rnd();
    S16(o + 0x10A) = (s16)(rnd() % 2 ? 0x578 : rnd() % 0x4000);
    S16(o + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x400 - 0x10 : rnd());
    U32(pl + 0x1D8) = o;
    Flag_Set(2, 0x1A, rnd() % 3 == 0);
    U32(0x87E974) = rnd() % 4 ? rnd() & 1 : rnd();
}

/* a mosasaurus among the test objects (no animations); the player its target */
static gptr mo_setup(void)
{
    gptr o = raptor_setup(), pl = U32(G() + 0x940);
    U32(o + 0x184) &= ~0x40u;
    trex_parts(o);
    U8(o + 0x15F) = rnd() % 4 ? 0xD : rnd() % 16;
    mo_state(o);
    U8(o + 0x57) = rnd() % 2;
    U16(pl + 0x174) = rnd() % 2 ? rnd() & 3 : (u16)rnd();
    U32(pl + 0x134) = 0;
    U16(o + 0xD8) = U16(pl + 0x40); U16(o + 0xDA) = U16(pl + 0x42); U16(o + 0xDC) = U16(pl + 0x44);
    trex_aim(o + 0x40, o, o + 0xD8);
    trex_ff(o, rnd() % 2);
    return o;
}

/* yaw and pitch off the target by about the deciding limits, past the turn
   (yr) and pitch (pr) it makes first */
static void mo_aim(gptr o, s32 yr, s32 pr)
{
    static const s32 ya[] = {0, 1, 0x1D, 0x1E, 0x1F, 0xFF, 0x100, 0x17F, 0x180, 0x200};
    static const s32 pa[] = {0, 1, 0xF, 0x10, 0x1D, 0x1E, 0x1F, 0x7F, 0x80};
    if (rnd() % 3) {
        s32 d = ya[rnd() % 10] + yr;
        U16(o + 0x4A) = (u16)((Angle_XZ(o + 0x40, o + 0xD8) + (rnd() % 2 ? d : -d)) & 0xFFF);
    }
    if (rnd() % 3) {
        u32 r0 = Mosa_PitchDiff(o + 0x40, o + 0xD8, 0);
        s32 d = pa[rnd() % 9] + pr;
        U16(o + 0x48) = (u16)((r0 - (rnd() % 2 ? d : -d)) & 0xFFF);
    }
    gptr pl = U32(G() + 0x940);
    if (rnd() % 2) U16(pl + 0x4A) = (u16)((U16(o + 0x4A) + (rnd() % 2 ? 1 : -1) * (0x1FF + (s32)(rnd() % 3) - 1)) & 0xFFF);
}

/* a Rand8 seed whose n-th value is want */
static u16 rand8_seed_at(u8 want, int n)
{
    u32 v = rnd() & 0xFFFF;
    for (u32 k = 0; k < 0x10000; k++, v = (v + 1) & 0xFFFF) {
        u16 x = (u16)v;
        for (int i = 0; i < n; i++) {
            u8 hi = (u8)((x * 3u) >> 8); u16 y = (u16)(hi << 8 | (u8)(x + hi));
            if (y == x) y += 0x101;
            x = y;
        }
        if ((u8)x == want) return (u16)v;
    }
    return (u16)v;
}

/* an attack about to be decided: no turning first, nothing keeping it back;
   its pitch, the pitch to the target, the distance and the roll about their limits */
static void mo_attack(gptr o)
{
    gptr pl = U32(G() + 0x940);
    static const s32 pv[] = {0, 1, 0x10, 0x3F, 0x40, 0x41, 0x80};
    static const s32 dv[] = {0, 1, 0xF, 0x10, 0x11, 0x7F, 0x80, 0x81};
    static const u32 dist[] = {0x7D0, 0xBB8, 0xE10, 0xFA0, 0x1770};
    U16(o + 0x1D8) = 0; U16(o + 0x1DA) = 0;
    U8(o + 0x13A) = 0; U32(o + 0x1D0) = rnd() % 4 ? 0 : 1; U32(o + 0x1C0) &= ~0x14u; U32(0x87E974) &= ~1u;
    U32(o + 0x13C) = dist[rnd() % 5] + rnd() % 3 - 1;
    s32 p = pv[rnd() % 7] * (rnd() % 2 ? 1 : -1), want = p + dv[rnd() % 8] * (rnd() % 2 ? 1 : -1);
    /* the target's height giving want (PitchDiff with no pitch), by halving */
    s32 lo = -0x6000, hi = 0x6000;
    S16(o + 0xDA) = (s16)lo; s32 rl = (s16)Mosa_PitchDiff(o + 0x40, o + 0xD8, 0);
    S16(o + 0xDA) = (s16)hi; s32 inc = rl < (s16)Mosa_PitchDiff(o + 0x40, o + 0xD8, 0);
    for (int k = 0; k < 20 && lo < hi; k++) {
        s32 m = (lo + hi) / 2;
        S16(o + 0xDA) = (s16)m;
        s32 r = (s16)Mosa_PitchDiff(o + 0x40, o + 0xD8, 0);
        if ((r < want) == inc) lo = m + 1; else hi = m;
    }
    S16(o + 0xDA) = (s16)lo;
    U16(o + 0x48) = (u16)((s16)Mosa_PitchDiff(o + 0x40, o + 0xD8, 0) - (want - p)) & 0xFFF;
    if (rnd() % 4) U16(pl + 0x4A) = (u16)((U16(o + 0x4A) + (rnd() % 2 ? 1 : -1) * (0x1FF + (s32)(rnd() % 3) - 1)) & 0xFFF);
    U16(0x87DE10) = rand8_seed_at((u8)(0x3F + rnd() % 3), 1 + rnd() % 2);
}

/* one floor polygon around o (Fn_454950 finds its edges; with none in reach,
   or products past 32 bits with long edges, it returns stack garbage) */
static void mo_floor(gptr o, int slope)
{
    gptr w = 0x6CFC00u, pg = w + 0x10, v = pg + 0x18;
    S16(o + 0x40) = (s16)(rnd() % 0x4000 - 0x2000); S16(o + 0x44) = (s16)(rnd() % 0x4000 - 0x2000);
    U16(o + 0x5A) = rnd() % 4;
    s32 r = 3000 + rnd() % 2000, x = S16(o + 0x40), z = S16(o + 0x44);
    U8(w) = 1; U32(w + 8) = 0x10;
    U8(pg) = slope ? (u8)(0x81 | (rnd() % 2 ? 0x10 : 0) | (rnd() % 2 ? 0x20 : 0)) : 1; U8(pg + 1) = 4; U8(pg + 3) = rnd() % 4 ? 0 : 0x20; U16(pg + 4) = slope ? (u16)(rnd() | 1) : rnd() % 4 ? 0xFFFF : (u16)(1u << U16(o + 0x5A));
    for (u32 e = 0; e < 4; e++) U8(pg + 8 + 4 * e) = (u8)(rnd() % 2);
    /* a quadrilateral with no edge along an axis (the rays Fn_454950 casts are) */
    s32 vx[5] = {x - r, x + r + 40 + (s32)(rnd() % 300), x + r, x - r - 40 - (s32)(rnd() % 300), 0};
    s32 vz[5] = {z - r - 40 - (s32)(rnd() % 300), z - r, z + r + 40 + (s32)(rnd() % 300), z + r, 0};
    vx[4] = vx[0]; vz[4] = vz[0];
    for (u32 e = 0; e < 5; e++) { S16(v + 4 * e) = (s16)vx[e]; S16(v + 4 * e + 2) = (s16)vz[e]; }
    U32(G() + 0xA4C) = w;
}

/* the zones (Zone_TrackPoint, Zone_RandomPoint): every object with parts */
static void mo_zones(void)
{
    enemy_state();
    for (u32 k = 8; k < 16; k++) U32(O_OBJ + k * 0x260 + 0x64) = O_PART + (k - 8) * 0x200;
    zones_setup();
}

static void test_mosa(void)
{
    g_ign_objpads = 1;
    stubs_install(1);
    mo_tables_stub(1);
    player_tables_stub(1);
    for (int i = 0; i < iters; i++) {
        gptr o, b, pl;
        u32 a[3];
        /* deciding, steering, the helpers */
        static const GigFn th[] = {
            {0x440180, Mosa_Decide, "Mosa_Decide"}, {0x43FD40, Mosa_ThinkSwim, "Mosa_ThinkSwim"},
            {0x43FE50, Mosa_ThinkRush, "Mosa_ThinkRush"}, {0x43FF60, Mosa_ThinkTurn, "Mosa_ThinkTurn"},
            {0x440070, Mosa_ThinkPitch, "Mosa_ThinkPitch"}, {0x4406F0, Mosa_ThinkLunge, "Mosa_ThinkLunge"},
            {0x440D30, Mosa_ThinkSeize, "Mosa_ThinkSeize"}, {0x441D90, Mosa_Timer, "Mosa_Timer"},
            {0x441E30, Mosa_Bite, "Mosa_Bite"}, {0x441E80, Mosa_Release, "Mosa_Release"},
            {0x441EE0, Mosa_PitchTo, "Mosa_PitchTo"}, {0x441F40, Mosa_PitchDiff, "Mosa_PitchDiff"}};
        for (int k = 0; k < 100; k++) {
            const GigFn *f = &th[k < 40 ? 0 : k % 12];
            game_state(); world_setup(); mo_zones();
            o = mo_setup(); b = o + 0x1C0; pl = U32(G() + 0x940);
            U8(o + 0x55) = rnd() % 3 ? (u8[]){0, 2, 3}[rnd() % 3] : rnd() % 8;
            U8(o + 0x56) = rnd() % 6;
            s32 yr = 0, pr = 0;
            if (rnd() % 2) {                                /* its turning rates as steering leaves them */
                static const u32 st[][3] = {{0x440180, 0, 0}, {0x43FD40, 0xA, 0xA}, {0x43FE50, 0xA, 0xA}, {0x43FF60, 0x3C, 0xA}, {0x440070, 0xA, 0x32}};
                for (u32 j = 0; j < 5; j++)
                    if (f->addr == st[j][0]) { yr = st[j][1]; pr = st[j][2]; U16(o + 0x1DA) = (u16)yr; U16(o + 0x1D8) = (u16)pr; }
            }
            mo_aim(o, yr, pr);
            if (rnd() % 2) {                                /* nothing keeps it from attacking */
                U8(o + 0x13A) = 0; U32(o + 0x1D0) = 0; U32(b) &= ~0x10u; U32(0x87E974) &= ~1u;
            }
            tri_rand8();
            /* the player deep in the water of room 704, or not (Mosa_Timer) */
            u32 rm = rnd() % 4;
            if (rm < 2) { U8(G() + 0x1091) = 7; U8(G() + 0x1090) = 4; }
            else if (rm == 2) { U8(G() + 0x1091) = 7; U8(G() + 0x1090) = (u8)(rnd() % 4); }
            S16(pl + 0x42) = rnd() % 2 ? tri_about(-3000) : (s16)rnd();
            U8(pl + 0x54) = (u8[]){1, 2, 5, 3, 1}[rnd() % 5]; U8(pl + 0x55) = rnd() % 2 ? 9 : (u8)(rnd() % 12);
            U8(G() + 0x108E) = rnd() % 3;
            if (f->addr == 0x441D90 && rnd() % 2) {         /* in the deep water, its roll about the limit */
                U8(G() + 0x1091) = 7; U8(G() + 0x1090) = 4; S16(pl + 0x42) = tri_about(-3000);
                U8(pl + 0x54) = rnd() % 2 ? 3 : 1; U8(pl + 0x55) = 0; U16(pl + 0x174) &= ~1;
                U16(0x87DE10) = rand8_seed_at((u8)(0x3F + rnd() % 3), 1);
            }
            if (f->addr == 0x440180 && rnd() % 2) mo_attack(o);
            a[0] = o; a[1] = U16(o + 0x1D8); a[2] = o + 0xD8;
            if (f->addr == 0x441F40) { a[0] = o + 0x40; a[1] = o + 0xD8; a[2] = rnd() % 2 ? U16(o + 0x48) : rnd(); }
            if (f->addr == 0x441EE0) a[1] = rnd() % 2 ? rnd() % 0x40 : rnd();
            U32(LOG_BASE) = 0;
            sprintf(g_what, "mosa %X %s move %u step %u dist %X bits %X", o, f->name, U8(o + 0x55), U8(o + 0x56), U32(o + 0x13C), U32(b));
            u32 ret = f->addr == 0x440180 || f->addr == 0x441D90 || f->addr == 0x441F40 ? 0xFFFFFFFF : 0;
            check(f->addr, f->fn, f->name, f->addr == 0x441EE0 || f->addr == 0x441F40 ? 3 : 1, a, ret);
        }
        /* the state handlers */
        game_state(); world_setup(); mo_zones();
        o = mo_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
        U8(o + 0x55) = rnd() % 7;
        check(0x43FCA0, Mosa_Normal, "Mosa_Normal", 1, a, 0);
        U8(o + 0x55) = rnd() % 7;
        check(0x43FCD0, Mosa_Hit, "Mosa_Hit", 1, a, 0);
        U8(o + 0x55) = rnd() % 7;
        check(0x43FCF0, Mosa_Die, "Mosa_Die", 1, a, 0);
        for (int k = 0; k < 6; k++) {
            U8(o + 0x55) = rnd() % 8; U8(o + 0x56) = rnd() % 3;
            if (rnd() % 2) trex_spheres(o, 1 + rnd() % 3);
            if (rnd() % 2) U32(pl + 0x134) = 0;
            U8(o + 0x119) = (u8[]){0, 1, 0x7F, 0x80, 0xFF}[rnd() % 5];
            check(0x43FD10, Mosa_Grab, "Mosa_Grab", 1, a, 0);
        }
        for (int k = 0; k < 12; k++) {
            game_state(); world_setup(); mo_zones();
            o = mo_setup(); a[0] = o; b = o + 0x1C0; pl = U32(G() + 0x940);
            U8(o + 0x54) = rnd() % 5; U8(o + 0x55) = rnd() % 8;
            U32(o + 0x50) = (U32(o + 0x50) & ~0x40000030u) | (rnd() & 0x30) | (rnd() % 8 == 0 ? 0x40000000 : 0);
            if (rnd() % 2) {                                /* about the surface, the bottom and the deepest */
                s16 f = (s16)Floor_Height(o + 0x40, o + 0x40, 0x40, 0);
                s32 e[] = {f - S16(o + 0x1EE), S16(o + 0x1EE) - 15000, f};
                S16(o + 0x42) = tri_about(e[rnd() % 3]);
            }
            if (rnd() % 2) U16(o + 0x48) = (u16)(rnd() % 2 ? tri_about(0x3E0) : tri_about(-0x3E0)) & (rnd() % 2 ? 0xFFF : 0xFFFF);
            U8(o + 0x17B) = rnd() % 2 ? 0 : (u8)rnd();
            U8(G() + 0x108F) = rnd() % 2 ? 0 : (u8)rnd();
            if (rnd() % 2) trex_spheres(o, 1 + rnd() % 3);
            if (rnd() % 2) {                                /* the head where it stands, about its looking limit */
                gptr hp = Obj_Part(U32(o + 0x64), U8(o + 0x15F));
                U16(hp + 0x40) = U16(o + 0x40); U16(hp + 0x44) = U16(o + 0x44);
                u16 an = (u16)Angle_XZ(o + 0x40, o + 0xD8);
                U16(o + 0x4A) = (u16)((an - (u16[]){0x4FF, 0x500, 0x501, 0xB01, 0xB00, 0xAFF}[rnd() % 6]) & 0xFFF);
                U16(o + 0xF0) = U16(o + 0xF2) = U16(o + 0xF4) = 0; Bits_Set(b, 1, rnd() % 4 != 0);
            }
            sprintf(g_what, "mosa %X handler state %X flags %X bits %X", o, U32(o + 0x54), U32(o + 0x50), U32(b));
            check(0x43F820, Mosa_Handler, "Mosa_Handler", 1, a, 0);
        }
        /* set up: 16 parts, as the animation */
        do model_data(0x640000); while (U32(0x640000 + 0x14) != 16);
        do anim_setup_at(AN_OBJ); while (U32(AN_OBJ + 0x68) != 16);
        memcpy(MEM(o + 0x60), MEM(AN_OBJ + 0x60), 0x50);
        U32(0x643100 + 8) = AN_DATA;                        /* its animation 2 */
        U8(G() + 0x108E) = rnd() % 4 ? rnd() % 3 : rnd() % 2 ? 3 : (u8)rnd(); S16(o + 0x42) = (s16)rnd();
        check(0x43FB00, Mosa_Init, "Mosa_Init", 1, a, 0);
        /* moving, hit, dying: each step; the player's callbacks */
        static const GigFn mv[] = {
            {0x43FDB0, Mosa_Swim, "Mosa_Swim", 2}, {0x43FEB0, Mosa_Rush, "Mosa_Rush", 2}, {0x43FFD0, Mosa_Turn, "Mosa_Turn", 2},
            {0x4400E0, Mosa_Pitch, "Mosa_Pitch", 2}, {0x4405A0, Mosa_Dive, "Mosa_Dive", 3}, {0x440710, Mosa_Lunge, "Mosa_Lunge", 4},
            {0x440880, Mosa_Leap, "Mosa_Leap", 5}, {0x440D80, Mosa_Seize, "Mosa_Seize", 5},
            {0x4412B0, Mosa_LungeHit, "Mosa_LungeHit", 3}, {0x441340, Mosa_SeizeHit, "Mosa_SeizeHit", 3},
            {0x441440, Mosa_Damage, "Mosa_Damage", 1}, {0x4416B0, Mosa_Flinch, "Mosa_Flinch", 2},
            {0x441720, Mosa_Flinch2, "Mosa_Flinch2", 2}, {0x4417B0, Mosa_HitDive, "Mosa_HitDive", 2},
            {0x441830, Mosa_Knock, "Mosa_Knock", 3}, {0x4418F0, Mosa_Knock2, "Mosa_Knock2", 3},
            {0x4419A0, Mosa_HitDive2, "Mosa_HitDive2", 2}, {0x441A20, Mosa_Dead, "Mosa_Dead", 5},
            {0x441BD0, Mosa_Dead2, "Mosa_Dead2", 5}, {0x440AF0, Mosa_PlThrown, "Mosa_PlThrown", 3},
            {0x4411E0, Mosa_PlDrop, "Mosa_PlDrop", 1}};
        const unsigned nm = sizeof mv / sizeof mv[0];
        game_state(); world_setup(); mo_zones();
        o = O_OBJ + 0x260 * (1 + rnd() % 6); b = o + 0x1C0;
        anim_setup_at(O_OBJ);
        do anim_setup_at(o); while (U32(o + 0x68) != 16);
        memcpy(MEM(O_OBJ + 0x60), MEM(o + 0x60), 0x50);
        U32(G() + 0x940) = pl = O_OBJ;
        U32(pl + 0x128) = rnd() % 2 ? 0x720C78 : 0; U32(o + 0x128) = 0x7211A0;
        U32(0x87E760) = O_OBJ;
        U8(o + 0x58) = 10;
        u8 sv[0x260], svp[0x260]; memcpy(sv, MEM(o), 0x260); memcpy(svp, MEM(pl), 0x260);
        for (unsigned k = 0; k < 14 * nm; k++) {
            const GigFn *f = &mv[k % nm];
            memcpy(MEM(o), sv, 0x260); memcpy(MEM(pl), svp, 0x260);
            mo_state(o);
            U8(o + 0x54) = rnd() % 4 ? 1 + rnd() % 3 : rnd() % 5;
            U8(o + 0x55) = rnd() % 7;
            U8(o + 0x56) = (u8)(rnd() % 8 ? rnd() % f->steps : rnd());
            U8(o + 0x57) = rnd() % 2;
            static const u16 fr[] = {0, 1, 7, 8, 9, 0xA, 0xB, 0x14, 0x15, 0x2C, 0x2D, 0x2E, 0x31, 0x32, 0x40};
            U16(o + 0x92) = rnd() % 4 ? fr[rnd() % 15] : (u16)(rnd() % 0x80);
            trex_ff(o, rnd() % 2);
            for (u32 j = 0; j < 0x2000; j += 0x1000) {     /* the animations' frame flags and spheres: valid or none */
                U32(AN_DATA + j + 0xC) = rnd() % 2 ? TR_QUAD + 0x100 : 0; U32(AN_DATA + j + 8) = rnd() % 2 ? TR_ATK : 0;
            }
            U8(pl + 0x54) = (u8[]){1, 2, 5, 3}[rnd() % 4]; U16(pl + 0x174) = rnd() % 2 ? rnd() & 3 : (u16)rnd();
            S16(pl + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x100 - 0x10 : rnd());
            S16(pl + 0x42) = rnd() % 3 ? 0 : (s16)(rnd() % 0x2000 - 0x1000);
            for (u32 j = 0xE8; j < 0xEE; j += 2) U16(pl + j) = rnd() % 2 ? 0 : (u16)(rnd() % 0x100 - 0x80);
            for (u32 j = 0xE8; j < 0xEE; j += 2) U16(o + j) = rnd() % 2 ? 0 : (u16)(rnd() % 0x100 - 0x80);
            U16(pl + 0xE2) = rnd() % 2 ? U16(pl + 0x42) : (u16)rnd();
            U8(o + 0x184) = (u8)rnd();
            if (f->addr == 0x441440 && rnd() % 2) {         /* about killed */
                U8(o + 0x56) = 0; S16(o + 0x108) = tri_about(U16(o + 0x176)); if (rnd() % 2) U16(o + 0x172) = 0;
                if (rnd() % 2) U8(o + 0x17D) = 0;
                if (rnd() % 2) U32(o + 0x1CC) = (u32)(S16(o + 0x10A) / 2 - U16(o + 0x176) + (s32)(rnd() % 3) - 1);
            }
            if (f->addr == 0x440AF0 || f->addr == 0x4411E0) {  /* the player's callbacks: the player thrown or dropped */
                a[0] = pl;
                U8(pl + 0x57) = rnd() % 4 ? rnd() % 3 : (u8)rnd();
                gptr p = U32(pl + 0x64);
                if (f->addr == 0x4411E0 && rnd() % 2) {     /* on a slope: a floor height of any value */
                    mo_floor(pl, 1);
                    s32 cx = S16(pl + 0x40), cz = S16(pl + 0x44);
                    int odd = rnd() % 2;                    /* (or where its level is off by the rounding) */
                    S16(p + 0x42) = -0x7000;                 /* (above it all: Floor_Height looks down) */
                    for (int t = 0; t < 600; t++) {
                        S16(p + 0x40) = (s16)(cx + (s32)(rnd() % 4000) - 2000); S16(p + 0x44) = (s16)(cz + (s32)(rnd() % 4000) - 2000);
                        s32 fh = (s16)Floor_Height(p + 0x40, p + 0x40, 0x80, 0);
                        if (!odd || fh / -1000 != fh / -999) break;
                    }
                }
                if (rnd() % 2) U16(pl + 0x92) = (u16)(0x2B + rnd() % 5);   /* about its last frame flying */
                s16 fl = (s16)Floor_Height(pl + 0x40, pl + 0x40, 0x80, 0);
                if (rnd() % 2) S16(pl + 0x42) = tri_about(fl - 0x1F4 - 0x40);
                if (rnd() % 2) S16(p + 0x42) = tri_about((s16)Floor_Height(p + 0x40, p + 0x40, 0x80, 0));
                if (rnd() % 2) U16(pl + 0xE2) = U16(pl + 0x42);
            } else a[0] = o;
            U8(G() + 0x108E) = rnd() % 3; U8(G() + 0x108F) = rnd() % 2 ? rnd() % 9 : (u8)rnd();
            for (u32 j = 0; j < 20; j++) if (rnd() % 2) U8(O_K2 + j * 0xAC + 0x50) &= ~1;
            if (rnd() % 2) { U16(o + 0xF0) = (u16)(rnd() % 0x600 - 0x300); U16(o + 0xF2) = (u16)(rnd() % 0x600 - 0x300); U16(o + 0xF4) = (u16)(rnd() % 0x600 - 0x300); }
            if (rnd() % 3 == 0) U16(o + 0xF4) = (u16[]){0, 0x50, 0x64, 0x78, 0xC8, 0xDC}[rnd() % 6] + rnd() % 0x30 - 0x18;
            U16(0x87DC12) = rnd() % 2 ? 0 : (u16)rnd();
            if (rnd() % 3 == 0) S16(pl + 0x108) = (s16)(Damage_Scale(0x64) + rnd() % 3 - 1);   /* its last bit of health */
            if (f->addr == 0x4412B0 && rnd() % 2)           /* the player turned away by about the limit */
                U16(pl + 0x4A) = (u16)((U16(o + 0x4A) + (rnd() % 2 ? 1 : -1) * (0x3FF + (s32)(rnd() % 3))) & 0xFFF);
            if (f->addr == 0x441440 && rnd() % 4 == 0)      /* left with none */
                S16(o + 0x108) = (s16)(U16(o + 0x176) / 5 + rnd() % 3 - 1);
            if ((f->addr == 0x441A20 || f->addr == 0x441BD0) && rnd() % 2)   /* sinking at about its speed */
                U16(o + 0xF2) = (u16)((f->addr == 0x441A20 ? 0x4B : 0x58) + (s32)(rnd() % 7) - 3);
            if (f->addr == 0x440880 || f->addr == 0x440D80) mo_floor(o, 0);   /* (and no root motion to leave it, below) */
            if (f->addr == 0x440D80) {                      /* the seize: reaching the player, its point, letting go */
                if (rnd() % 2) U32(o + 0x13C) = 0x7D0 + rnd() % 3 - 1;
                if (rnd() % 3 == 0) U16(o + 0x42) = (u16)(U16(pl + 0x42) - 0x3EC + (s32)(rnd() % 0x29) - 0x14);
                if (rnd() % 2) {
                    U16(o + 0x1F2) = (u16)(U16(o + 0x40) + 5000 + (s32)(rnd() % 3) - 1); U16(o + 0x1F6) = U16(o + 0x44);
                    for (u32 j = 0xE8; j < 0xEE; j += 2) U16(o + j) = U16(pl + j) = 0;
                }
            }
            if ((f->addr == 0x441A20 || f->addr == 0x441BD0) && rnd() % 2)   /* about the bottom */
                S16(o + 0x42) = tri_about((s16)Floor_Height(o + 0x40, o + 0x40, 0x40, 0) - S16(o + 0x1EE));
            tri_rand8();
            anim_maybe_end(o);
            if (a[0] == pl) anim_maybe_end(pl);
            if (rnd() % 2) U16(o + 0x102) |= 0x20;
            if (f->addr == 0x440880 || f->addr == 0x440D80) U16(o + 0x102) &= ~0x40;
            U32(LOG_BASE) = 0;
            sprintf(g_what, "mosa %X %s step %u side %u frame %X bits %X", o, f->name, U8(o + 0x56), U8(o + 0x57), U16(o + 0x92), U32(b));
            check(f->addr, f->fn, f->name, 1, a, 0);
        }
    }
    player_tables_stub(0);
    mo_tables_stub(0);
    stubs_install(0);
    g_ign_objpads = 0;
}

/* ---- the compsognathus (type 13) ---- */
/* its tables 0x7213CC..0x72141C (they overlap): a recorder per entry */
static u32 g_co_saved[21];
static void co_tables_stub(int on)
{
    for (int k = 1; k <= 21; k++) g_stubs[k].spec = on ? "d" : "";
    for (u32 k = 0; k < 21; k++) {
        gptr a = 0x7213CC + 4 * k;
        if (on) { g_co_saved[k] = U32(a); U32(a) = (u32)(uintptr_t)g_recs[1 + k]; }
        else U32(a) = g_co_saved[k];
    }
}

/* a compsognathus among the test objects, the player its target */
static gptr co_setup(void)
{
    gptr o = raptor_setup(), pl = U32(G() + 0x940);
    trex_parts(o);
    U8(o + 0x54) = rnd() % 4 ? 1 + rnd() % 3 : rnd() % 5;
    U8(o + 0x55) = rnd() % 4; U8(o + 0x56) = rnd() % 4 ? rnd() % 2 : (u8)rnd();
    U8(o + 0x119) = (u8[]){0, 1, 0x7F, 0x80, 0xFF}[rnd() % 5];
    U32(o + 0x50) = rnd() % 2 ? (U32(o + 0x50) & ~0x30u) | (rnd() & 0x30) : rnd();
    U32(o + 0x184) = (rnd() % 2 ? rnd() & 3 : rnd()) & ~0x40u;   /* (0x40: a target at G+0xB50) */
    S16(o + 0x108) = (s16)(rnd() % 2 ? rnd() % 0x400 - 0x10 : rnd());
    U16(o + 0x176) = rnd() % 2 ? rnd() % 0x400 : (u16)rnd();
    if (rnd() % 4 == 0) U16(o + 0x176) = (u16)(S16(o + 0x108) + rnd() % 3 - 1);
    static const s16 ys[] = {0, -1, -999, -1000, -1001, 999, 1000, 1001, -32768, 32767};
    S16(o + 0x42) = rnd() % 2 ? ys[rnd() % 10] : (s16)rnd();
    for (u32 j = 0xF0; j < 0xF6; j += 2) U16(o + j) = rnd() % 3 ? (u16)(rnd() % 0x200 - 0x100) : (u16)rnd();
    if (rnd() % 3 == 0) U16(o + 0xF4) = (u16)(0x8C + rnd() % 0x41 - 0x20);
    U16(pl + 0x1E8) = (u16)rnd(); S16(pl + 0x42) = (s16)rnd();
    U32(pl + 0x134) = 0;                                /* (no hit spheres: Enemy_AttackHit keeps +0x119) */
    U16(o + 0xD8) = U16(pl + 0x40); U16(o + 0xDA) = U16(pl + 0x42); U16(o + 0xDC) = U16(pl + 0x44);
    if (rnd() % 2) {                                    /* about straight at the target */
        s32 d = (s32[]){0, 1, 0x18, 0x19, 0x1A, 0x800}[rnd() % 6] * (rnd() % 2 ? 1 : -1);
        U16(o + 0x4A) = (u16)((Angle_XZ(o + 0x40, o + 0xD8) + d) & 0xFFF);
    } else U16(o + 0x4A) = (u16)rnd();
    return o;
}

static void test_compso(void)
{
    g_ign_objpads = 1;
    stubs_install(1);
    co_tables_stub(1);
    for (int i = 0; i < iters; i++) {
        gptr o;
        u32 a[1];
        /* the state handlers and the moves */
        static const GigFn fn[] = {
            {0x4457F0, Compso_Normal, "Compso_Normal", 15}, {0x445820, Compso_Hit, "Compso_Hit", 14},
            {0x445840, Compso_Die, "Compso_Die", 9}, {0x445860, Compso_Grab, "Compso_Grab", 4},
            {0x445880, Compso_Turn, "Compso_Turn", 0}, {0x4458A0, Compso_Cling, "Compso_Cling", 0},
            {0x445910, Compso_Damage, "Compso_Damage", 0}, {0x445920, Compso_Hurt, "Compso_Hurt", 0},
            {0x445980, Compso_Fade, "Compso_Fade", 0}};
        for (unsigned k = 0; k < 4 * sizeof fn / sizeof fn[0]; k++) {
            const GigFn *f = &fn[k % (sizeof fn / sizeof fn[0])];
            game_state(); world_setup(); mo_zones();
            o = co_setup(); a[0] = o;
            if (f->steps) U8(o + 0x55) = (u8)(rnd() % f->steps);
            if (f->addr == 0x4458A0) {                  /* its animation: valid, at its end or not */
                anim_setup_at(o);
                memcpy(MEM(O_OBJ + 0x60), MEM(o + 0x60), 0x50);
                U8(o + 0x56) = rnd() % 4 ? rnd() % 2 : (u8)rnd();
                anim_maybe_end(o);
                if (rnd() % 2) U16(o + 0x102) |= 0x20;
                if (rnd() % 2) U16(o + 0xF4) = (u16)(0x8C + rnd() % 0x2B - 0x15);   /* about its top speed */
            }
            U32(LOG_BASE) = 0;
            sprintf(g_what, "compso %X %s state %X flags %X", o, f->name, U32(o + 0x54), U32(o + 0x50));
            check(f->addr, f->fn, f->name, 1, a, 0);
        }
        /* the handler */
        for (int k = 0; k < 8; k++) {
            game_state(); world_setup(); mo_zones();
            o = co_setup(); a[0] = o;
            if (rnd() % 2) trex_spheres(o, 1 + rnd() % 3);
            U8(o + 0x54) = rnd() % 5;
            U32(LOG_BASE) = 0;
            sprintf(g_what, "compso %X handler state %X flags %X", o, U32(o + 0x54), U32(o + 0x50));
            check(0x445690, Compso_Handler, "Compso_Handler", 1, a, 0);
        }
        /* set up: 16 parts, as the animation */
        game_state(); world_setup(); mo_zones();
        o = co_setup(); a[0] = o;
        do model_data(0x640000); while (U32(0x640000 + 0x14) != 16);
        do anim_setup_at(AN_OBJ); while (U32(AN_OBJ + 0x68) != 16);
        memcpy(MEM(o + 0x60), MEM(AN_OBJ + 0x60), 0x50);
        U32(0x640868) = AN_DATA;                            /* its animation 0 */
        U32(AN_TAB) = AN_DATA + 0x1000; U32(o + 0x88) = AN_TAB;   /* (another table before) */
        U32(LOG_BASE) = 0;
        sprintf(g_what, "compso %X init y %X", o, U16(o + 0x42));
        check(0x445720, Compso_Init, "Compso_Init", 1, a, 0);
        /* the screen flashes: colour, blend, the quarters' offset about its limits */
        for (int k = 0; k < 4; k++) {
            game_state(); draw_safe();
            if (rnd() % 2) U32(0x87DC98) += 1 + rnd() % 3;    /* (an odd primitive address: the link keeps its low bits) */
            gptr fl = G_SCR + 0x100 * (rnd() % 12);
            for (u32 j = 0; j < 8; j++) U8(fl + j) = (u8)rnd();
            if (rnd() % 2) S8(fl + 5) = (s8[]){0, 1, -1, 0x77, 0x78, 0x79, 0x7F, -0x80, 0x9F}[rnd() % 9];
            a[0] = fl;
            U32(LOG_BASE) = 0;
            sprintf(g_what, "flash %X col %X k %d", fl, U32(fl), S8(fl + 5));
            if (k % 2) check(0x445A60, Flash_Quads, "Flash_Quads", 1, a, 0);
            else check(0x4459A0, Flash_Full, "Flash_Full", 1, a, 0);
        }
    }
    co_tables_stub(0);
    stubs_install(0);
    g_ign_objpads = 0;
}

/* ---- the extra game's playable dinosaurs: shared, the inostrancevia (types 41/42) ---- */
static const u32 g_ki_t[2] = {0x722B08, 0x722C68}, g_ki_v[2] = {0x74B2D8, 0x74B330};
static const u32 g_ki_m[2] = {0x662500, 0x638000}, g_ki_foe[2] = {2, 1};
static const u32 g_ki_pad[2][2] = {{0x87DC94, 0x87DC08}, {0x87DC92, 0x87DE0A}};
static u32 g_ki_saved[2][54];
/* the 54 entries from t+0x60 (state, pick, move, hit, die, landing) as recorders 1..30:
   the tables one function calls together never share one */
static int ki_rec(u32 e) { return e < 24 ? 1 + (int)e : e < 44 ? (int)e - 19 : (int)e - 23; }
static void ki_tables_stub(int on)
{
    for (int k = 1; k <= 30; k++) g_stubs[k].spec = on ? "d" : "";
    for (int c = 0; c < 2; c++)
        for (u32 e = 0; e < 54; e++) {
            gptr a = g_ki_t[c] + 0x60 + 4 * e;
            if (on) { g_ki_saved[c][e] = U32(a); U32(a) = (u32)(uintptr_t)g_recs[ki_rec(e)]; }
            else U32(a) = g_ki_saved[c][e];
        }
}

/* pad words: a few buttons, now and then anything */
static u16 ki_pad(void)
{
    u16 b = 0;
    for (int n = rnd() % 4; n; n--) b |= (u16)(1u << (rnd() % 16));
    return rnd() % 8 ? b : (u16)rnd();
}

/* the inostrancevia of copy c among the test objects (animated: its parts
   from anim_setup_at), its variables, pads and the other player */
static gptr ki_setup(int c, int anim)
{
    gptr o = raptor_setup(), v = g_ki_v[c];
    if (anim) { anim_setup_at(o); anim_maybe_end(o); U8(o + 0xFD) &= 1; }
    else trex_parts(o);
    aim_desc_setup(0);
    U32(0x87E760) = O_OBJ;
    gptr foe = O_OBJ + g_ki_foe[c] * 0x260;
    U8(foe + 0xFF) = (u8)g_ki_foe[c];                   /* (its own index: the head looks at it) */
    U8(o + 0x54) = rnd() % 4 ? 1 + rnd() % 3 : rnd() % 4;
    U8(o + 0x55) = rnd() % 4 ? rnd() % 10 : (u8)rnd();
    U8(o + 0x56) = rnd() % 4 ? rnd() % 3 : (u8)rnd();
    U8(o + 0x57) = rnd() % 3 ? rnd() % 2 : (u8)(rnd() % 0x20);
    U16(o + 0x92) = rnd() % 4 ? rnd() % 0x50 : (u16)rnd();
    U16(o + 0x10A) = rnd() % 2 ? 0x4B0 : (u16)(rnd() % 0x800);
    S16(o + 0x108) = (s16)(rnd() % 2 ? (u32)(S16(o + 0x10A) >> 1) + rnd() % 5 - 2 : rnd() % 3 ? rnd() % 0x800 - 0x10 : rnd());
    U16(o + 0x176) = rnd() % 2 ? rnd() % 0x200 : (u16)rnd();
    if (rnd() % 4 == 0) U16(o + 0x176) = (u16)(S16(o + 0x108) + rnd() % 3 - 1);
    U32(o + 0x184) = rnd() % 2 ? rnd() & 9 : rnd();
    U8(o + 0x15F) = rnd() % 4 ? 3 : (u8)(rnd() % 16);       /* (a part of its chain: Head_Update needs it) */
    U8(o + 0x17B) = rnd() % 3 ? 0 : (u8)rnd();
    U32(o + 0x50) = rnd() % 2 ? (U32(o + 0x50) & ~0x38u) | (rnd() & 0x38) : rnd();
    U16(g_ki_pad[c][0]) = ki_pad(); U16(g_ki_pad[c][1]) = ki_pad();
    U16(v) = ki_pad();
    U32(v + 8) = rnd() % 4 ? rnd() & 0x5A : rnd();
    U16(v + 0x10) = rnd() % 5;
    U16(v + 0x40) = rnd() % 2 ? 0 : (u16)rnd();
    for (u32 k = 0x48; k < 0x4E; k += 2) U16(v + k) = (u16)(S16(o + 0x40 + k - 0x48) + rnd() % 8000 - 4000);
    U32(v + 0x50) = rnd() % 5 ? foe : o;
    U16(v + 0x54) = ki_pad();
    static const s16 ang[] = {0, 1, -1, 0x4FF, 0x500, -0x4FF, -0x500, 0x7FF, -0x800};
    S16(v + 0x56) = rnd() % 2 ? ang[rnd() % 9] : (s16)rnd();
    gptr cb = g_ki_t[c] + 0x50;                         /* the button sequence: 0x10 0x10 0x4000 0x20 */
    U16(cb) = rnd() % 4; U16(cb + 2) = rnd() % 3 ? rnd() % 12 : (u16)rnd();
    if (rnd() % 3 == 0) {                               /* its last button now: the super move, at about half health */
        U16(cb) = 3; U16(cb + 2) = 2 + rnd() % 9; U16(v) = (u16)(0x20 | (rnd() % 2 ? ki_pad() : 0));
        S16(o + 0x108) = (s16)((S16(o + 0x10A) >> 1) + rnd() % 3 - 1);
    }
    else if (rnd() % 4 == 0) {                          /* the super move on, health about full */
        U32(v + 8) |= 0x40; S16(o + 0x108) = (s16)(S16(o + 0x10A) - rnd() % 3);
    }
    U8(G() + 0x927) = rnd() % 3;
    U32(0x87E4A4) = rnd();
    if (rnd() % 2) { U16(o + 0xD8) = U16(foe + 0x40); U16(o + 0xDA) = U16(foe + 0x42); U16(o + 0xDC) = U16(foe + 0x44); }
    if (rnd() % 2) {                                    /* about facing the point turned to */
        s32 d = (s32[]){0, 1, 0x95, 0x96, 0x97, 0x800}[rnd() % 6] * (rnd() % 2 ? 1 : -1);
        U16(o + 0x4A) = (u16)((Angle_XZ(o + 0x40, v + 0x48) + d) & 0xFFF);
    }
    return o;
}

/* the other player right at o (its parts too), open to hits: the attacks land */
static void ki_close(gptr o, gptr foe)
{
    U32(foe + 0x50) |= 8; U8(foe + 0x17B) = 0;
    for (u32 j = 0x40; j < 0x46; j += 2) S16(foe + j) = (s16)(S16(o + j) + rnd() % 200 - 100);
    for (gptr q = U32(foe + 0x64); q; q = U32(q + 0x54))
        for (u32 j = 0x40; j < 0x46; j += 2) S16(q + j) = (s16)(S16(o + j) + rnd() % 400 - 200);
}

/* a state of Rand8 (0x87DE10) whose next value is r */
static u16 ki_seed(u8 r)
{
    for (u32 k = 0, v = rnd() & 0xFFFF; k < 0x10000; k++, v = (v + 1) & 0xFFFF) {
        u8 hi = (u8)((v * 3u) >> 8);
        u16 n = (u16)(hi << 8 | (u8)(v + hi));
        if (n == v) n += 0x101;
        if ((u8)n == r) return (u16)v;
    }
    return 0;
}

typedef struct { u32 addr; void *fn[2]; const char *name; u8 anim, s55; } KiFn;

static void test_kof_ino(void)
{
    g_ign_objpads = 1;
    stubs_install(1);
    ki_tables_stub(1);
    static const KiFn fn[] = {
        {0x45D410, {KIno1_Normal, KIno2_Normal}, "Normal", 0, 10}, {0x45D500, {KIno1_Hit, KIno2_Hit}, "Hit", 0, 10},
        {0x45D520, {KIno1_Die, KIno2_Die}, "Die", 0, 10}, {0x45D540, {KIno1_Attack, KIno2_Attack}, "Attack", 0, 10},
        {0x45D590, {KIno1_PickStand, KIno2_PickStand}, "PickStand", 0, 0}, {0x45D680, {KIno1_Stand, KIno2_Stand}, "Stand", 1, 0},
        {0x45D6F0, {KIno1_PickWalk, KIno2_PickWalk}, "PickWalk", 0, 0}, {0x45D7C0, {KIno1_Walk, KIno2_Walk}, "Walk", 1, 0},
        {0x45D830, {KIno1_Turn, KIno2_Turn}, "Turn", 1, 0}, {0x45D920, {KIno1_PickOn, KIno2_PickOn}, "PickOn", 0, 0},
        {0x45D9E0, {KIno1_On, KIno2_On}, "On", 1, 0}, {0x45DA50, {KIno1_PickBack, KIno2_PickBack}, "PickBack", 0, 0},
        {0x45DB10, {KIno1_Back, KIno2_Back}, "Back", 1, 0}, {0x45DB80, {KIno1_Roar, KIno2_Roar}, "Roar", 1, 0},
        {0x45DC20, {KIno1_About, KIno2_About}, "About", 1, 0}, {0x45DD50, {KIno1_Steer7, KIno2_Steer7}, "Steer7", 0, 0},
        {0x45DD80, {KIno1_Bite, KIno2_Bite}, "Bite", 1, 0}, {0x45DE30, {KIno1_PickBite2, KIno2_PickBite2}, "PickBite2", 1, 0},
        {0x45DE80, {KIno1_Bite2, KIno2_Bite2}, "Bite2", 1, 0}, {0x45DF30, {KIno1_Steer9, KIno2_Steer9}, "Steer9", 0, 0},
        {0x45DF60, {KIno1_Lunge, KIno2_Lunge}, "Lunge", 1, 0}, {0x45E030, {KIno1_Land7, KIno2_Land7}, "Land7", 0, 0},
        {0x45E060, {KIno1_Land8, KIno2_Land8}, "Land8", 0, 0}, {0x45E090, {KIno1_Land9, KIno2_Land9}, "Land9", 0, 0},
        {0x45E0B0, {KIno1_Hurt, KIno2_Hurt}, "Hurt", 0, 10}, {0x45E290, {KIno1_Flinch8, KIno2_Flinch8}, "Flinch8", 1, 0},
        {0x45E300, {KIno1_Flinch9, KIno2_Flinch9}, "Flinch9", 1, 0}, {0x45E370, {KIno1_Dying, KIno2_Dying}, "Dying", 1, 0}};
    for (int i = 0; i < iters; i++) {
        u32 a[5];
        gptr o;
        /* a button sequence */
        for (int k = 0; k < 4; k++) {
            gptr cb = G_SCR + 0x100 * (rnd() % 12);
            u32 n = 1 + rnd() % 5;
            for (u32 j = 0; j < 8; j++) U16(cb + 4 + 2 * j) = j < n ? (u16)(1u << (rnd() % 16)) : 0xFFFF;
            if (rnd() % 4 == 0) U16(cb + 4 + 2 * (rnd() % 8)) = (u16)rnd();
            U16(cb) = (u16)(rnd() % 4 ? rnd() % (n + 1) : rnd() % 8);
            U16(cb + 2) = rnd() % 3 ? rnd() % 12 : (u16)rnd();
            a[0] = cb; a[1] = rnd() % 2 ? U16(cb + 4 + 2 * U16(cb)) | (rnd() % 2 ? ki_pad() : 0) : rnd();
            sprintf(g_what, "combo step %u left %u pad %X", U16(cb), U16(cb + 2), a[1]);
            check(0x418F40, Combo_Input, "Combo_Input", 2, a, 0xFFFFFFFF);
        }
        /* the super move: the shout by mode, stage and kind */
        for (int k = 0; k < 4; k++) {
            game_state(); world_setup();
            o = O_OBJ + 0x260 * (rnd() % 16); a[0] = o;
            U8(o + 0x58) = (u8)(rnd() % 4 ? 0x1C + rnd() % 0x10 : rnd());
            U8(0x87E747) = rnd() % 3; U8(0x87EEAF) = (u8)(rnd() % 4 ? 3 + rnd() % 11 : rnd());
            U32(LOG_BASE) = 0;
            sprintf(g_what, "super type %X mode %u stage %u", U8(o + 0x58), U8(0x87E747), U8(0x87EEAF));
            check(0x418FA0, Kof_Super, "Kof_Super", 1, a, 0);
        }
        /* the other player hit, and a playable dinosaur's attack spheres */
        for (int k = 0; k < 4; k++) {
            game_state(); world_setup();
            o = ki_setup(rnd() % 2, 0);
            targets_setup(); aim_desc_setup(rnd() % 2);
            U32(G() + 0x948) = O_K3; U16(O_K3 + 0x18) = (u16)(0x200 + rnd() % 0x900);
            u32 slot = rnd() % 4 ? (u32[]){0x1F, 2, 5, 0}[rnd() % 4] : rnd() % 0x23;
            gptr e = 0x737A90 + slot * 0x24;
            if (rnd() % 2) {                            /* a record as an attack makes it */
                U32(e + 0xC) = (0x800 + (u32[]){0, 1, 2, 0x100}[rnd() % 4]) | (rnd() % 4 ? 0 : rnd() & 0xFFFF0000u);
                U16(e + 0x10) = 0; U16(e + 0x12) = 0;
                U16(e) = 0; U16(e + 2) = (u16)(rnd() % 2000);  /* (one radius: the cone tests divide by its spread) */
                u16 r = (u16)(rnd() % 800); for (u32 j = 4; j < 0xC; j += 2) U16(e + j) = r;
            }
            if (rnd() % 4 == 0) U32(e + 0xC) = rnd() & 0xD00;
            gptr m = G_SCR + 0x40;
            S16(G_SCR + 0x60) = (s16)(rnd() % 0x1000); S16(G_SCR + 0x62) = (s16)(rnd() % 0x1000); S16(G_SCR + 0x64) = (s16)(rnd() % 0x1000);
            if (rnd() % 2) { S16(G_SCR + 0x60) = 0; S16(G_SCR + 0x64) = 0; }
            RotMatrix(G_SCR + 0x60, m);
            a[0] = m; a[1] = o + 0x40; a[2] = slot; a[3] = rnd() % 4 ? 1 + rnd() % 2 : rnd() % 16;
            if (rnd() % 2) ki_close(o, O_OBJ + 0x260 * a[3]);
            if (rnd() % 2) U16(0x87DE10) = ki_seed((u8[]){0xBF, 0xC0, 0xC1}[rnd() % 3]);   /* the critical hit's limit */
            U32(LOG_BASE) = 0;
            sprintf(g_what, "hit foe %u slot %X", a[3], slot);
            check(0x47B9E0, Kof_HitFoe, "Kof_HitFoe", 4, a, 0xFFFFFFFF);

            game_state(); world_setup();
            o = ki_setup(rnd() % 2, 0);
            targets_setup(); aim_desc_setup(rnd() % 2);
            U32(G() + 0x948) = O_K3; U16(O_K3 + 0x18) = (u16)(0x200 + rnd() % 0x900);
            if (rnd() % 8) trex_spheres(o, 1 + rnd() % 4);
            else U16(o + 0x102) = (u16)rnd();
            a[0] = o;
            a[1] = rnd() % 4 ? (u32[]){0, 1, 2}[rnd() % 3] : rnd() % 3 ? 0x100 | rnd() % 3 : rnd() % 16;   /* (only the low byte counts) */
            a[2] = rnd() % 6 ? 1 + rnd() % 0x200 : rnd() % 2 ? 0 : 0x10000;
            a[3] = rnd() % 4 ? rnd() % 4 : (u32[]){4, 0xFFFF, 0x10002, 0x8000}[rnd() % 4];
            a[4] = rnd() % 4 ? 0x1F : rnd() % 2 ? 0x100 | (rnd() % 0x23) : (u32[]){2, 5}[rnd() % 2];
            if (rnd() % 2 && (a[1] & 0xFF) && (a[1] & 0xFF) < 16) ki_close(o, O_OBJ + 0x260 * (a[1] & 0xFF));
            U32(LOG_BASE) = 0;
            sprintf(g_what, "attack foe %X dmg %X react %X slot %X frame %u 102 %X", a[1], a[2], a[3], a[4], U16(o + 0x92), U16(o + 0x102));
            check(0x47CB20, Kof_Attack, "Kof_Attack", 5, a, 0);
        }
        /* both copies: the states, picks and moves */
        for (unsigned k = 0; k < 2 * sizeof fn / sizeof fn[0]; k++) {
            const KiFn *f = &fn[k % (sizeof fn / sizeof fn[0])];
            int c = rnd() % 2;
            game_state(); world_setup(); mo_zones();
            o = ki_setup(c, f->anim); a[0] = o;
            if (f->s55) U8(o + 0x55) = (u8)(rnd() % f->s55);
            if (f->addr == 0x45DC20) U8(o + 0x57) = rnd() % 2;            /* (the side: a table of two) */
            if (f->addr == 0x45D540 && rnd() % 4) trex_spheres(o, 1 + rnd() % 4);
            if (f->addr == 0x45D540 && rnd() % 2) ki_close(o, O_OBJ + g_ki_foe[c] * 0x260);
            if (f->addr == 0x45E370) U8(o + 0x56) = rnd() % 4;
            if (rnd() % 2) S16(o + 0xF4) = (s16)((s16[]){0x3C, -0x1E, 0x78}[rnd() % 3] + rnd() % 0x2B - 0x15);   /* about a top speed */
            if (f->addr == 0x45D830 && rnd() % 2) U8(o + 0x57) = (u8)(0x12 + rnd() % 3);   /* the turn's 20 frames */
            if (f->addr == 0x45DE30 && rnd() % 2) {     /* the second bite going on: 0x20 held, about frame 0x17 */
                U16(g_ki_v[c] + 0x54) |= 0x20; U16(o + 0x92) = (u16)(0x16 + rnd() % 3);
            }
            if (f->anim && rnd() % 2) {            /* the frames that matter */
                static const u16 fr[] = {0x9, 0xA, 0xD, 0xE, 0x13, 0x23, 0x2C, 0x30, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1C, 0x1D, 0x24, 0x25, 0x26,
                                         0x2A, 0x2B, 0x2D, 0x31, 0x32, 0x35, 0x36, 0x40, 0x41};
                U16(o + 0x92) = fr[rnd() % (sizeof fr / sizeof fr[0])];
                U16(o + 0x102) |= 0x20;                 /* (not at its end: the frame stays) */
            }
            U32(LOG_BASE) = 0;
            sprintf(g_what, "kino%d %X %s state %08X v8 %X held %X pad %X frame %u", c + 1, o, f->name, U32(o + 0x54),
                    U32(g_ki_v[c] + 8), U16(g_ki_v[c] + 0x54), U16(g_ki_v[c]), U16(o + 0x92));
            check(f->addr + c * 0x1330, f->fn[c], f->name, 1, a, 0);
        }
        /* the handler */
        for (int k = 0; k < 4; k++) {
            int c = rnd() % 2;
            game_state(); world_setup(); mo_zones();
            o = ki_setup(c, 0); a[0] = o;
            if (rnd() % 2) trex_spheres(o, 1 + rnd() % 3);
            U8(o + 0x54) = rnd() % 4;
            U8(o + 0x55) %= 10;                          /* (a hit calls the table of its 10 moves) */
            if (rnd() % 2 && U8(o + 0x15F) < 16) {      /* the head about +-0x500 from the other player */
                gptr hp = AN_PART + U8(o + 0x15F) * 0x80, foe = O_OBJ + g_ki_foe[c] * 0x260;
                s32 d = (s32[]){0x4FF, 0x500, 0x501}[rnd() % 3] * (rnd() % 2 ? 1 : -1);
                U16(o + 0x4A) = (u16)((Angle_XZ(hp + 0x40, foe + 0x40) - d) & 0xFFF);
            }
            U32(LOG_BASE) = 0;
            sprintf(g_what, "kino%d %X handler state %08X flags %X", c + 1, o, U32(o + 0x54), U32(o + 0x50));
            check(0x45D100 + c * 0x1330, c ? (void *)KIno2_Handler : (void *)KIno1_Handler, "KIno_Handler", 1, a, 0);
        }
        /* set up: 16 parts, as its standing animation */
        {
            int c = rnd() % 2;
            game_state(); world_setup(); mo_zones();
            o = ki_setup(c, 0); a[0] = o;
            gptr md = g_ki_m[c];
            do model_data(md); while (U32(md + 0x14) != 16);
            do anim_setup_at(AN_OBJ); while (U32(AN_OBJ + 0x68) != 16);
            memcpy(MEM(o + 0x60), MEM(AN_OBJ + 0x60), 0x50);
            U32(md + 0x2350 + 0x16 * 4) = AN_DATA;
            U32(LOG_BASE) = 0;
            sprintf(g_what, "kino%d %X init y %X", c + 1, o, U16(o + 0x42));
            check(0x45D2C0 + c * 0x1330, c ? (void *)KIno2_Init : (void *)KIno1_Init, "KIno_Init", 1, a, 0);
        }
    }
    ki_tables_stub(0);
    stubs_install(0);
    g_ign_objpads = 0;
}

static void test_game(void)
{
    stubs_install(1);
    type_tables_stub(1);
    script_setup();
    static const struct { u32 addr; void *fn; const char *name; int nargs; } F[] = {
        {0x4826F0, Game_Start, "Game_Start", 1}, {0x4827F0, Pause_LoadScreen, "Pause_LoadScreen", 0},
        {0x481C10, Room_LoadDat, "Room_LoadDat", 0}, {0x481CA0, Room_LoadExtra, "Room_LoadExtra", 1},
        {0x481D20, Door_Load, "Door_Load", 1}, {0x481DC0, Armor_Load, "Armor_Load", 1},
        {0x48F2C0, Checkpoint_Save, "Checkpoint_Save", 0}, {0x48F330, Checkpoint_Load, "Checkpoint_Load", 0},
        {0x48BF00, Snd_InitRoomTables, "Snd_InitRoomTables", 0}, {0x48CBD0, Snd_RoomMusic, "Snd_RoomMusic", 0},
        {0x4510D0, Game_ClearState, "Game_ClearState", 0}, {0x450E00, Game_NewState, "Game_NewState", 0},
        {0x475660, Opening_Init, "Opening_Init", 1}, {0x4756A0, Opening_Movie, "Opening_Movie", 1},
        {0x475680, Opening_Run, "Opening_Run", 1}, {0x475700, Opening_ToGame, "Opening_ToGame", 1},
        {0x451120, Game_Init, "Game_Init", 0}, {0x4512F0, Game_AreaLoad, "Game_AreaLoad", 0},
        {0x451330, Game_RoomEnter, "Game_RoomEnter", 0}, {0x451790, Game_Play, "Game_Play", 0},
        {0x471EC0, Game_Leave, "Game_Leave", 0}, {0x46FC50, Game_SetMode723CC0, "Game_SetMode723CC0", 1},
    };
    void *areas[10] = {Area0_SetTables, Area1_SetTables, Area2_SetTables, Area3_SetTables, Area4_SetTables,
                       Area5_SetTables, Area6_SetTables, Area7_SetTables, Area8_SetTables, Area9_SetTables};
    for (int i = 0; i < iters; i++) {
        for (unsigned k = 0; k < sizeof F / sizeof F[0]; k++) {
            game_state();
            fake_room_block(); draw_safe();
            U32(LOG_BASE) = 0;
            u32 a[1];
            switch (k) {
            case 0: a[0] = rnd() % 20; break;
            case 3: a[0] = (rnd() % 4) << 8 | rnd() % 8; break;
            case 4: a[0] = rnd() % 20; break;
            case 5: a[0] = rnd() % 2; break;
            default: a[0] = 0x74DB90; U8(0x74DB91) = 0; U8(0x74DB92) = rnd() % 4; break;
            }
            if (F[k].addr == 0x471EC0) U8(G() + 0x108D) = rnd() % 2;
            if (F[k].addr == 0x46FC50) a[0] = rnd();
            g_stubs[STUB_GAME + 12].ret = rnd();
            sprintf(g_what, "mode %u area %u room %u extra %u arg %X", U16(G() + 0x664), U8(G() + 0x1091), U8(G() + 0x1090), U8(G() + 0x927), a[0]);
            check(F[k].addr, F[k].fn, F[k].name, F[k].nargs, a, F[k].nargs ? 0 : 0);
        }
        u32 a0 = rnd() % 10;
        U32(LOG_BASE) = 0;
        check(0x48D170 + 0x40 * a0, areas[a0], "AreaN_SetTables", 0, NULL, 0);
        /* game mode 4: game over / continue */
        for (u32 a = 0x65F500; a < 0x660300; a += 4) U32(a) = rnd();
        for (u32 a = 0x65F500; a < 0x660300; a += 8) {
            U8(a + 4) = (u8)((rnd() % 3 ? rnd() % 4 : rnd() % 32) | (rnd() % 4 == 0 ? 0x20 : rnd() % 4 == 0 ? 0x40 : 0));
            U8(a + 5) = rnd() % 3;
        }
        for (int r = 0; r < 4; r++) {
            game_state(); fake_room_block();
            gptr g = G();
            U16(g + 0x666) = rnd() % 6; U16(g + 0x668) = rnd() % 3 ? rnd() % 2 : (u16)rnd();
            U16(g + 0x66A) = rnd() % 2 ? rnd() % 8 : (u16)(0x1C + rnd() % 4);
            if (U16(g + 0x666) == 3 && rnd() % 2) U16(g + 0x66A) = 1;
            U8(g + 0x927) = rnd() % 3 == 0; U8(g + 0x108C) = rnd() % 2 ? 0x62 + rnd() % 2 : (u8)rnd();
            U16(g + 0x1090) = (u16[]){1, 0x80A, 0x101, 0x203}[rnd() % 4];
            for (u32 k = 0; k < 6 * 12; k += 4) U32(g + 0x11AC + k) = rnd() % 3 ? 0 : rnd();
            if (rnd() % 2) { U8(g + 0x11AC + 12 * (rnd() % 6) + 1) = 0x1E; }
            for (u32 k = 0; k < 6; k++) U16(g + (0x179 + k) * 12 + 8) = rnd() % 3;
            /* the checkpoint copy (G+0xC2C, loaded over G+0x108C on "yes"): the extra game's continue items */
            U8(g + 0xC2C + 2) = rnd() % 2;
            for (u32 k = 0; k < 6 * 12; k += 4) U32(g + 0xD4C + k) = rnd() % 3 ? 0 : rnd();
            if (rnd() % 4) { gptr e = g + 0xD4C + 12 * (rnd() % 6); U8(e) = 1; U8(e + 1) = 0x1E; U16(e + 8) = rnd() % 3; }
            static const u32 lbl[4] = {0x74B178, 0x74B184, 0x74B190, 0x74B19C};
            for (int k = 0; k < 4; k++) {
                gptr l = lbl[k], f = 0x65F500 + 8 * (rnd() % 0x1C0);
                U32(l) = f; U32(l + 4) = f + 8 * (rnd() % 4); U8(l + 8) = rnd() % 3; U8(l + 9) = k == 2;
            }
            U16(0x74B190) = rnd() % 0x30; U16(0x74B192) = rnd() % 0x30; U32(0x74B194) = rnd() % 0x100;
            U32(0x87DC00) = (u32[]){0, 0x20, 0x2000, 0x8000, 0x40, 0x10, 0x820}[rnd() % 7];
            U32(0x87DC98) = 0x6D0000; Flag_Set(2, 6, rnd() % 3 == 0);
            stub_at(0x494DE0)->ret = rnd() % 0x100;
            U32(LOG_BASE) = 0;
            sprintf(g_what, "continue step %u choice %u frames %u pad %X", U16(g + 0x666), U16(g + 0x668), U16(g + 0x66A), U32(0x87DC00));
            check(0x451990, Game_Continue, "Game_Continue", 0, NULL, 0);
            u32 c[4] = {0x74B178 + 12 * (rnd() % 4)};
            check(0x4522E0, Continue_AnimStep, "Continue_AnimStep", 1, c, 0xFFFFFFFF);
            c[1] = 0x65F500 + 8 * (rnd() % 0x1C0);
            check(0x4522C0, Continue_SetAnim, "Continue_SetAnim", 2, c, 0);
            u32 d[4] = {(u32)(rnd() % 400) - 50, (u32)(rnd() % 300) - 50, rnd(), 0x65F500 + 8 * (rnd() % 0x1C0)};
            check(0x452170, Continue_Draw, "Continue_Draw", 4, d, 0);
            u32 t[1] = {0x7275D0 + (rnd() % 2) * 0x26};
            check(0x471A90, Text_Length, "Text_Length", 1, t, 0xFFFFFFFF);
        }
    }
    type_tables_stub(0);
    stubs_install(0);
}

static void test_lzss(const char *dir)
{
    char pat[MAX_PATH]; snprintf(pat, sizeof pat, "%s\\*.DAT", dir);
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) { printf("LZSS: no .DAT files in %s, skipped\n", dir); return; }
    int nrec = 0;
    do {
        char path[MAX_PATH]; snprintf(path, sizeof path, "%s\\%s", dir, fd.cFileName);
        FILE *f = fopen(path, "rb"); if (!f) continue;
        u8 hdr[0x800];
        if (fread(hdr, 1, 0x800, f) != 0x800) { fclose(f); continue; }
        u32 off = 0x800;
        for (int r = 0; r < 64; r++) {
            u32 type = *(u32 *)(hdr + 32 * r), size = *(u32 *)(hdr + 32 * r + 4);
            if (!memcmp(hdr + 32 * r, "dummy", 5) || (!type && !size)) continue;
            if (type > 0x20) break;
            if ((type == 5 || type == 6) && size && size < 0xF0000) {
                gptr src = 0x610000, dst = 0x4E8000;           /* both inside .psxseg */
                fseek(f, off, SEEK_SET);
                if (fread(MEM(src), 1, size, f) == size) {
                    memset(MEM(dst), 0, src - dst);
                    u32 a[3] = {dst, src, size};
                    sprintf(g_what, "%s record %d (%u bytes)", fd.cFileName, r, size);
                    check(0x4041F0, Lzss_Decode, "Lzss_Decode", 3, a, 0);
                    nrec++;
                }
            }
            off += (size + 0x7FF) & ~0x7FFu;
        }
        fclose(f);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    printf("LZSS: %d records\n", nrec);
}

/* called by harness_stub.exe, whose image covers 0x400000..0x883000 so that
 * nothing else can sit there; the stub's own code is overwritten by the
 * game image, so this never returns */
static LONG WINAPI on_crash(EXCEPTION_POINTERS *e)
{
    printf("CRASH %08lX at %p (address %p) in %s (%s) during [%s] after %d checks\n",
           e->ExceptionRecord->ExceptionCode, e->ExceptionRecord->ExceptionAddress,
           (void *)e->ExceptionRecord->ExceptionInformation[1], g_cur_name ? g_cur_name : "?",
           g_cur_orig ? "original" : "new", g_what, g_tests);
    {   /* return addresses near the top of the stack */
        u32 *sp = (u32 *)(uintptr_t)e->ContextRecord->Esp;
        printf("stack:");
        for (int i = 0; i < 24; i++) if (sp[i] >= 0x401000 && sp[i] < 0x4CD000) printf(" %08X", sp[i]);
        printf("\n");
    }
    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), 4);
    return EXCEPTION_CONTINUE_SEARCH;
}

/* name is one of the comma-separated entries of list (groups still run in table order) */
static int in_list(const char *list, const char *name)
{
    size_t n = strlen(name);
    for (const char *p = list; ; p++) {
        if (!strncmp(p, name, n) && (p[n] == ',' || p[n] == 0)) return 1;
        p = strchr(p, ',');
        if (!p) return 0;
    }
}

__declspec(dllexport) void harness_main(int argc, char **argv)
{
    SetUnhandledExceptionFilter(on_crash);
    const char *exe = argc > 1 ? argv[1] : "Dino2.exe", *data = NULL, *only = NULL;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "-n") && i + 1 < argc) iters = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-t") && i + 1 < argc) only = argv[++i];   /* test groups, comma-separated */
        else if (!strcmp(argv[i], "-s") && i + 1 < argc) rs = (u32)strtoul(argv[++i], NULL, 0);   /* the random seed */
        else data = argv[i];
    }
    if (!load_image(exe)) { fflush(stdout); TerminateProcess(GetCurrentProcess(), 2); }
    for (int i = 0; i < 2; i++) {
        snap0[i] = malloc(RW[i].hi - RW[i].lo);
        snap1[i] = malloc(RW[i].hi - RW[i].lo);
    }
    U32(A_G_PTR) = A_G_STATIC;
    struct { const char *name; void (*fn)(void); } T[] = {
        {"flags", test_flags}, {"geometry", test_geom}, {"script values", test_script_values},
        {"script opcodes", test_script_ops}, {"script objects", test_objects},
        {"script run", test_script_run}, {"script ops 2", test_ops2}, {"gte", test_gte}, {"animation", test_anim}, {"objects 2", test_object}, {"collision", test_collision}, {"player", test_player}, {"player states", test_player2}, {"zones", test_zones}, {"loading", test_room}, {"title", test_title}, {"game task", test_game}, {"doors", test_doors}, {"frame", test_frame}, {"enemy sets", test_enemyset}, {"enemies", test_enemies}, {"raptor", test_raptor}, {"oviraptor", test_ovi}, {"allosaurus", test_allo}, {"pteranodon", test_ptera}, {"tyrannosaurus", test_trex}, {"plesiosaurus", test_plesio}, {"water plesiosaurs", test_pw}, {"roamer", test_roam}, {"giganotosaurus", test_gig},
    {"inostrancevia", test_ino}, {"triceratops", test_tri}, {"mosasaurus", test_mosa}, {"compsognathus", test_compso}, {"duel inostrancevia", test_kof_ino}, {"aot kinds", test_aot_kinds}, {"object effects", test_obj_effects}, {"effect slots", test_k2}, {"effect drops", test_k2_drops}, {"effect emitters", test_eff_slots}, {"effect slots 3", test_k2_more}, {"helpers", test_helpers}, {"draw", test_draw}, {"pc gpu", test_pcgpu}, {"pc draw", test_pcdraw}, {"pc frame", test_pcframe},
    };
    for (unsigned i = 0; i < sizeof T / sizeof T[0]; i++) {
        int f0 = g_fail, t0 = g_tests;
        if (only && !in_list(only, T[i].name)) continue;    /* (-t data: only the passes over the game's files) */
        T[i].fn();
        printf("%-16s %6d checks, %d failed\n", T[i].name, g_tests - t0, g_fail - f0);
    }
    if (data && (!only || in_list(only, "data"))) { int f0 = g_fail, t0 = g_tests; test_lzss(data);
                printf("%-16s %6d checks, %d failed\n", "lzss", g_tests - t0, g_fail - f0);
                f0 = g_fail; t0 = g_tests; test_files(data);
                printf("%-16s %6d checks, %d failed\n", "files", g_tests - t0, g_fail - f0);
                f0 = g_fail; t0 = g_tests; test_root();
                printf("%-16s %6d checks, %d failed\n", "root", g_tests - t0, g_fail - f0); }
    if (data && (!only || in_list(only, "data") || in_list(only, "jpeg"))) { int f0 = g_fail, t0 = g_tests; test_jpeg(data);
                printf("%-16s %6d checks, %d failed\n", "jpeg", g_tests - t0, g_fail - f0); }
    printf("hook table: %d functions; total %d checks, %d failed\n", g_nhooks, g_tests, g_fail);
    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), g_fail != 0);
}
