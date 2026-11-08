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
#include "dc2/game.h"
#include "../src/hook/hooks.h"
#include "dc2/orig.h"
#include "dc2/gte.h"
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
static int g_ign_objpads;            /* matrix pads (+0x32) of test objects and parts */
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
        for (int i = 0; i <= 16; i++) {
            u32 a = i == 16 ? 0x6C8000 + 0x32 : 0x6C8400 + i * 0x80 + 0x32;
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
    for (u32 k = 0; k < g_ign_len; k++)
        for (int i = 0; i < 2; i++)
            if (g_ign_addr + k >= RW[i].lo && g_ign_addr + k < RW[i].hi)
                snap1[i][g_ign_addr + k - RW[i].lo] = U8(g_ign_addr + k);
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

static const char *g_cur_name;
static int g_cur_orig;
static int check(u32 addr, void *fn, const char *name, int nargs, const u32 *args, u32 rmask)
{
    g_cur_name = name; g_cur_orig = 1;
    snap(snap0);
    u32 r0 = call((void *)(uintptr_t)addr, nargs, args);
    snap(snap1);
    restore(snap0);
    g_cur_orig = 0;
    u32 r1 = call(fn, nargs, args);
    int ok = compare(name, r0, r1, rmask);
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

static void script_setup(void)
{
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
    for (int i = 0; i < 4; i++) { U16(AN_EVT + 4 + 4 * i) = rnd() % 16; U16(AN_EVT + 6 + 4 * i) = (u16)rnd(); }
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
    {0x405DE0, "dd"}, {0, ""}, {0x403A30, ""}, {0x448080, "d"}, {0, ""},
    {0, ""}, {0x4529C0, "d"}, {0x450A70, "ddd"}, {0x46FBC0, "ddd"}, {0x4718F0, "dddd"},
    {0x473130, "d"}, {0x4731F0, ""}, {0x474B20, "d"}, {0x478B80, "dd"}, {0x47A8C0, "ddpp", {16, 8}},
    {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0x48C0B0, "pd", {0x60}},
    {0x48C600, "d"}, {0x48C640, "ddd"}, {0x48C9C0, "w"}, {0x48CD70, "dd"}, {0x48CE20, "bwww"},
    {0x48CE60, "d"}, {0x48D070, "dd"}, {0, ""}, {0, ""}, {0, ""},
    {0x495E90, "wwwww"}, {0x4B4D20, "pd", {8}}, {0x4B4DB0, "pd", {8}}, {0x4DDDA2, ""},
    {0, "d"},                   /* 34: object type handlers (*0x87DCA0) */
    {0, "d"},                   /* 35: AOT kind handlers (0x738B68) */
    /* file loading: 36.. (impl set below: a fake file system) */
    {0x4C03F0, "sdd"}, {0x4C04E0, "ddd"}, {0x4C0540, "d"}, {0x4C0580, "ddd"}, {0x4C05A0, "d"},
    {0x4DD711, "d"}, {0x4DD628, "d"}, {0x4DD5D6, ""},
    {0x48CEB0, "d"}, {0x48CF10, "d"}, {0x4B49F0, "pd", {8}}, {0x4BD480, "d"}, {0x4BF720, "dd"},
    {0x4BFC10, "ddd"},
    /* 50..: loading and room set-up */
    {0x4C29A9, "dd"}, {0, ""}, {0, ""}, {0, ""}, {0x471A30, ""}, {0x471A50, ""},
    {0x48D470, "d"}, {0x478790, ""},
    {0x4C0560, "dd"},        /* 58: Pc_SeekSet */
    {0x4C29D6, "d"},         /* 59: Task_Chain */
    /* 60..: title */
    {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""},
    {0x494BA0, "ddddddd"}, {0x4C0600, "ddd"}, {0x4C0690, "d"}, {0x4C05D0, "dd"}, {0x4C06F0, "ddddd"},
    /* 71..: game task */
    {0x496410, ""}, {0x46FC50, "d"}, {0x403480, ""}, {0x470CB0, ""}, {0x452450, ""}, {0x48FB50, "d"},
    {0, ""}, {0x4BF130, "d"}, {0, ""}, {0x403520, ""}, {0x46B8C0, ""}, {0x407170, ""},
    {0x491B10, "bb"}, {0x48CB40, ""}, {0x4A7250, ""}, {0x405E40, ""}, {0x472FD0, ""}, {0x452370, ""},
    /* 89..: object handlers (collision) */
    {0, ""}, {0, ""},
    /* 91..: player */
    {0, ""}, {0x4542F0, "pdpd", {6, 6}}, {0x4BF330, ""},
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
static void *const g_recs[] = {rec0, rec1, rec2, rec3, rec4, rec5, rec6, rec7, rec8, rec9, rec10,
    rec11, rec12, rec13, rec14, rec15, rec16, rec17, rec18, rec19, rec20, rec21, rec22, rec23,
    rec24, rec25, rec26, rec27, rec28, rec29, rec30, rec31, rec32, rec33, rec34, rec35, rec36, rec37,
    rec38, rec39, rec40, rec41, rec42, rec43, rec44, rec45, rec46, rec47, rec48, rec49, rec50, rec51,
    rec52, rec53, rec54, rec55, rec56, rec57, rec58, rec59, rec60, rec61, rec62, rec63,
    rec64, rec65, rec66, rec67, rec68, rec69, rec70, rec71, rec72, rec73, rec74, rec75, rec76, rec77,
    rec78, rec79, rec80, rec81, rec82, rec83, rec84, rec85, rec86, rec87, rec88, rec89, rec90, rec91, rec92, rec93};
/* Task_Sleep: the file loader "runs" while the task sleeps */
static u32 __cdecl ts_sleep(u32 n)
{ u32 v[5] = {n, U16(0x87FFE8)}; rec_common(STUB_LOAD, v); U16(0x87FFEA) = 0; return 0; }   /* + file asked for */
#define TYPE_TAB 0x6DD000u      /* fake *0x87DCA0: 256 entries */
static u32 g_aot_saved[12], g_wep_saved[24];

static void stubs_install(int on)
{
    g_stubs[STUB_LOAD].impl = ts_sleep;
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
    case 0x1B: U8(t + T_SEL0) = rnd() % 5; if (rnd() % 4 == 0) U32(O_OBJ + SEL(t) * 0x260 + 0x64) = 0; break;
    case 0x20: U8(0x87FC45) = rnd() % 3 ? (u8)rnd() : 0xFF; break;
    case 0x23: A(1) = rnd() % 2 ? 0x6DC000 + rnd() % 0x100 : rnd() % 64;
               U16(g + 0x680) = rnd() % 16; U16(g + 0x682) = rnd() % 12; U16(0x87FFEA) = rnd() % 2; break;
    case 0x24: U8(g + 0xB4A) = rnd() % 3 ? (u8)rnd() : 0xFF; break;
    case 0x28: A(2) = rnd() % 12; A(1) = rnd() % 64; A(0) = rnd() % 2; break;
    case 0x29: { s32 v[4] = {-1, -2, -3, (s32)(rnd() % 16 | (rnd() % 8) << 8)}; A(0) = (u32)v[rnd() % 4];
               U8(g + 0x108F) = rnd() % 4; U16(g + 0x10B4) = (u16)(rnd() % 16 | (rnd() % 8) << 8);   /* weapon, sub-weapon */
               if (A(0) == 0xFFFFFFFF) U8(g + 0x108F) = rnd() % 2;   /* else stage 0xFE: outside the CORE table */
               for (int i = 0; i < 4; i++) U16(g + 0x10B6 + 2 * i) = (u16)(rnd() % 16 | (rnd() % 8) << 8);
               anim_setup_at(O_OBJ);                 /* Stage_Load restarts the player's animation */
               U32(O_OBJ + 0x50) = rnd(); U8(O_OBJ + 0x58) = (u8)rnd();
               U16(g + 0x67A) = rnd() % 2 ? 0x16 + rnd() % 4 : 0; U16(g + 0x674) = (u16)rnd(); U16(g + 0x676) = (u16)rnd();
               U16(0x87FFEA) = rnd() % 2; Flag_Set(9, 0x33, rnd() & 1); break; }
    case 0x2B: A(3) = rnd() % 16; break;
    case 0x2C: A(4) = rnd() % 3; Flag_Set(2, 6, rnd() % 4 == 0); break;
    case 0x2D: A(0) = rnd() % 4 ? (rnd() % 10) << 8 | rnd() % 32 : 0xFFFF; break;
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
    case 0x50: case 0x51: Flag_Set(2, 0x17, rnd() & 1); break;
    case 0x52: U8(t + T_SEL0) = rnd() % 4; break;
    case 0x54: U32(g + 0xA7C) = rnd() % 16; A(0) = rnd() % 3; break;
    case 0x58: { gptr part = U32(O_OBJ + SEL(t) * 0x260 + 0x64);
               A(1) = U32(part + 0x54) && rnd() % 2; A(0) = rnd() % 3; break; }
    case 0x59: Flag_Set(9, 0x33, rnd() & 1); A(0) = rnd() % 2; U16(0x87FFEA) = rnd() % 2; break;
    case 0x4D: A(0) = rnd() % 16; U16(g + 0x680) = rnd() % 2 ? 0xFFFF : rnd() % 16; U16(0x87FFEA) = rnd() % 2; break;
    case 0x49: case 0x55: break;
    }
#undef A
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
        /* floors */
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
        U16(pa) = U16(pb); U16(pa + 4) = U16(pa); U16(pa + 2) = U16(pb + 4); U16(pa + 6) = 0x7CFF;
        a[0] = w0; a[1] = pa;
        check(0x454870, Poly_RayCount, "Poly_RayCount", 2, a, 0xFFFF);
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
        stub_at(0x4542F0)->ret = rnd() % 4 != 0;
        a[0] = o;
        { char *w = g_what + sprintf(g_what, "objs %u floor %u:", U32(0x87F940), stub_at(0x4542F0)->ret);
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
        stub_at(0x4542F0)->ret = 0;
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
            o = O_OBJ; do anim_setup_at(o); while (U32(o + 0x68) < 7);      /* parts 3 and 6: the guns */
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
static u32 __cdecl fk_sprintf(u32 buf, u32 fmt, u32 name)
{
    char t[64];
    const char *f = (const char *)MEM(fmt);
    if (strstr(f, "%s")) snprintf(t, sizeof t, f, (char *)MEM(name));
    else snprintf(t, sizeof t, f, (int)name);
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

static void game_state(void)
{
    gptr g = G();
    world_setup();
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
    };
    void *areas[10] = {Area0_SetTables, Area1_SetTables, Area2_SetTables, Area3_SetTables, Area4_SetTables,
                       Area5_SetTables, Area6_SetTables, Area7_SetTables, Area8_SetTables, Area9_SetTables};
    for (int i = 0; i < iters; i++) {
        for (unsigned k = 0; k < sizeof F / sizeof F[0]; k++) {
            game_state();
            fake_room_block();
            U32(LOG_BASE) = 0;
            u32 a[1];
            switch (k) {
            case 0: a[0] = rnd() % 20; break;
            case 3: a[0] = (rnd() % 4) << 8 | rnd() % 8; break;
            case 4: a[0] = rnd() % 20; break;
            case 5: a[0] = rnd() % 2; break;
            default: a[0] = 0x74DB90; U8(0x74DB91) = 0; U8(0x74DB92) = rnd() % 4; break;
            }
            g_stubs[STUB_GAME + 12].ret = rnd();
            sprintf(g_what, "mode %u area %u room %u extra %u arg %X", U16(G() + 0x664), U8(G() + 0x1091), U8(G() + 0x1090), U8(G() + 0x927), a[0]);
            check(F[k].addr, F[k].fn, F[k].name, F[k].nargs, a, F[k].nargs ? 0 : 0);
        }
        u32 a0 = rnd() % 10;
        U32(LOG_BASE) = 0;
        check(0x48D170 + 0x40 * a0, areas[a0], "AreaN_SetTables", 0, NULL, 0);
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

__declspec(dllexport) void harness_main(int argc, char **argv)
{
    SetUnhandledExceptionFilter(on_crash);
    const char *exe = argc > 1 ? argv[1] : "Dino2.exe", *data = NULL, *only = NULL;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "-n") && i + 1 < argc) iters = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-t") && i + 1 < argc) only = argv[++i];   /* one test group */
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
        {"script run", test_script_run}, {"script ops 2", test_ops2}, {"gte", test_gte}, {"animation", test_anim}, {"objects 2", test_object}, {"collision", test_collision}, {"player", test_player}, {"player states", test_player2}, {"zones", test_zones}, {"loading", test_room}, {"title", test_title}, {"game task", test_game},
    };
    for (unsigned i = 0; i < sizeof T / sizeof T[0]; i++) {
        int f0 = g_fail, t0 = g_tests;
        if (only && strcmp(only, T[i].name)) continue;
        T[i].fn();
        printf("%-16s %6d checks, %d failed\n", T[i].name, g_tests - t0, g_fail - f0);
    }
    if (data && !only) { int f0 = g_fail, t0 = g_tests; test_lzss(data);
                printf("%-16s %6d checks, %d failed\n", "lzss", g_tests - t0, g_fail - f0);
                f0 = g_fail; t0 = g_tests; test_files(data);
                printf("%-16s %6d checks, %d failed\n", "files", g_tests - t0, g_fail - f0);
                f0 = g_fail; t0 = g_tests; test_root();
                printf("%-16s %6d checks, %d failed\n", "root", g_tests - t0, g_fail - f0); }
    printf("hook table: %d functions; total %d checks, %d failed\n", g_nhooks, g_tests, g_fail);
    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), g_fail != 0);
}
