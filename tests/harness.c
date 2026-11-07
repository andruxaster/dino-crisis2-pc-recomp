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
#include "dc2/game.h"
#include "../src/hook/hooks.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

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

static int check(u32 addr, void *fn, const char *name, int nargs, const u32 *args, u32 rmask)
{
    snap(snap0);
    u32 r0 = call((void *)(uintptr_t)addr, nargs, args);
    snap(snap1);
    restore(snap0);
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
    printf("CRASH %08lX at %p (address %p) during [%s] after %d checks\n",
           e->ExceptionRecord->ExceptionCode, e->ExceptionRecord->ExceptionAddress,
           (void *)e->ExceptionRecord->ExceptionInformation[1], g_what, g_tests);
    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), 4);
    return EXCEPTION_CONTINUE_SEARCH;
}

__declspec(dllexport) void harness_main(int argc, char **argv)
{
    SetUnhandledExceptionFilter(on_crash);
    const char *exe = argc > 1 ? argv[1] : "Dino2.exe", *data = NULL;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "-n") && i + 1 < argc) iters = atoi(argv[++i]);
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
        {"script run", test_script_run}, {"gte", test_gte},
    };
    for (unsigned i = 0; i < sizeof T / sizeof T[0]; i++) {
        int f0 = g_fail, t0 = g_tests;
        T[i].fn();
        printf("%-16s %6d checks, %d failed\n", T[i].name, g_tests - t0, g_fail - f0);
    }
    if (data) { int f0 = g_fail, t0 = g_tests; test_lzss(data);
                printf("%-16s %6d checks, %d failed\n", "lzss", g_tests - t0, g_fail - f0); }
    printf("hook table: %d functions; total %d checks, %d failed\n", g_nhooks, g_tests, g_fail);
    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), g_fail != 0);
}
