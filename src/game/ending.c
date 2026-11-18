/* ENDING: the task after the last boss (T 0x74B030, modes 0x721470): the
 * two ending movies, one of the three 3D scenes (model.c) under the results
 * (play time, saves, continues, files, the grade), the two bonus pictures,
 * the costume shop for the extinction points, then the game restarted.
 * A small sprite list at 0x5E0008 (50 entries of 0x18: {parts, parts,
 * u32 colour, u8 flags +0xD (1 used, 2 shown), u16 tpage +0x10, clut +0x12,
 * x +0x14, y +0x16}) carries the pictures. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"
#include "dc2/task.h"
#include "dc2/pcio.h"
#include <string.h>

#define END_T    0x74B030u
#define SPRITES  0x5E0008u
#define SPR_END  0x5E04B8u
#define COSTUMES 0x721498u      /* 12 x {parts, parts, u16 x, y, u16 tpage bits, u8 +0xF, u32 price +0x10, name +0x14} */

static void draw_ot(void)
{
    u32 k = U32(0x87DE0C) << 12;
    Gpu_DrawOT(U32(0x87F94C) + 0x88, 0x87CBFC + k, 0x87BC00 + k);
}

/* a draw mode packet (tpage 5) in order-table slot 0xA */
static void mode5(void)
{
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 5, 0);
    Gpu_AddPrim(0x87BC28 + (U32(0x87DE0C) << 12), U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}

static gptr rect(gptr r, u16 x, u16 y, u16 w, u16 h)
{
    U16(r) = x; U16(r + 2) = y; U16(r + 4) = w; U16(r + 6) = h;
    return r;
}

static u32 grey(u8 c) { return (u32)c << 16 | (u32)c << 8 | c; }

/* ---- the sprite list ---- */
gptr DC2CALL Spr_Alloc(void)                                             /* 0x447E60 */
{
    for (gptr e = SPRITES; e < SPR_END; e += 0x18)
        if (!(U8(e + 0xD) & 1)) { U8(e + 0xD) = 3; return e; }
    return 0;
}

void DC2CALL Spr_Set(gptr e, gptr parts, u32 x, u32 y, u32 col)        /* 0x447FD0 */
{
    U32(e) = parts; U32(e + 4) = parts;
    U8(e + 0xC) = U8(parts + 5);
    U32(e + 8) = col & 0xFFFFFF;
    U16(e + 0x14) = (u16)x; U16(e + 0x16) = (u16)y;
}

/* the shown ones, last first: their parts {u, v, w, h, u8 count (first), s8 dx, dy} */
void DC2CALL Spr_DrawAll(void)                                           /* 0x447E80 */
{
    for (gptr e = SPR_END - 0x18; e >= SPRITES; e -= 0x18) {
        if (!(U8(e + 0xD) & 2)) continue;
        gptr p = U32(e + 4);
        for (u32 n = U8(p + 4) & 0x1F; n; n--, p += 8) {
            gptr pr = U32(0x87DC98);
            Gpu_SetSprt(pr);
            U32(pr + 4) = U32(e + 8) | 0x66000000;
            F32(pr + 8) = (float)((s8)U8(p + 6) + (s32)U16(e + 0x14));
            F32(pr + 0xC) = (float)((s8)U8(p + 7) + (s32)U16(e + 0x16));
            U8(pr + 0x14) = U8(p); U8(pr + 0x15) = U8(p + 1);
            U16(pr + 0x18) = U8(p + 2); U16(pr + 0x1A) = U8(p + 3);
            U16(pr + 0x16) = U16(e + 0x12);
            Gpu_AddPrim(U32(0x87F94C) + 0x8C, U32(0x87DC98));
            U32(0x87DC98) += 0x1C;
        }
        Gpu_SetDrawMode(U32(0x87DC98), 1, 0, U16(e + 0x10), 0);
        Gpu_AddPrim(U32(0x87F94C) + 0x8C, U32(0x87DC98));
        U32(0x87DC98) += 0xC;
    }
}

/* ---- set-up ---- */
void DC2CALL End_Init(gptr t)                                            /* 0x446310 */
{
    gptr g = G();
    U8(t)++;
    if (U8(g + 0x108D) < 0x63) U8(g + 0x108D)++;              /* games cleared */
    if (S32(g + 0x120C) > 9999999) U32(g + 0x120C) = 9999999;   /* points */
    U32(t + 0x14) = U32(g + 0x1098);                            /* play time */
    for (u32 i = 0x75; i < 0x80; i++)                           /* files read */
        if (Flag_Check(9, i)) U16(t + 0x1E)++;
    Flag_Set(3, 0, 0);
}

/* the camera and the stage for the 3D scene */
void DC2CALL End_Setup(gptr t)                                           /* 0x446550 */
{
    memset(MEM(SPRITES), 0, 0x12C * 4);
    Room_Reset();
    gptr c = G() + 0x778;
    U16(c + 0x2C) = 0; U16(c + 0x2A) = 0; U16(c + 0x28) = 0;
    U16(c + 0x30) = 0x2BC; U16(c + 0x32) = 0x1F4; U16(c + 0x34) = 0x9C4;
    U16(c + 0x20) = 0xFD0; U16(c + 0x22) = 0x800; U16(c + 0x24) = 0;
    SetGeomScreen(0x140);
    gptr a = Tmp_Push(8);
    U16(a) = (u16)-U16(c + 0x20); U16(a + 2) = (u16)-U16(c + 0x22); U16(a + 4) = (u16)-U16(c + 0x24);
    RotMatrix(a, G() + 0x778);
    Tmp_Pop(8);
    U8(G() + 0xB4B) = 4;
    SetDepthScale((float)(s32)(0x400u << (U8(G() + 0xB4B) & 31)));
    Flag_Set(3, 1, 1);
    Flag_Set(3, 2, 0); Flag_Set(3, 3, 0); Flag_Set(3, 4, 0); Flag_Set(3, 5, 0);
    Flag_Set(3, 8, 0);
}

/* ---- the movies ---- */
static int movie(gptr t, u32 n)       /* 1 when it has ended */
{
    switch (U8(t + 2)) {
    case 0: U8(t + 2)++; Movie_Start(n); break;
    case 1: Movie_Tick(); if (!Flag_Check(2, 0x17)) U8(t + 2)++; break;
    case 2: return 1;
    }
    return 0;
}

void DC2CALL End_Movie1(gptr t)                                          /* 0x4463B0 */
{
    if (movie(t, 7)) { U8(t + 2) = 0; U8(t + 1)++; }
}

/* the second movie, then the grade by play time and one of the 3D scenes
   (by games cleared, from the fourth on at random) */
void DC2CALL End_Movie2(gptr t)                                          /* 0x446410 */
{
    if (!movie(t, 8)) return;
    u32 tm = U32(t + 0x14);
    U8(t + 0x20) = tm < 0x69780 ? 0 : tm < 0xD2F00 ? 1 : tm < 0x13C680 ? 2 : tm < 0x1A5E00 ? 3 : 4;
    u32 c = U8(G() + 0x108D);
    if (c == 0 || c > 3) c = (u8)Rand8() % 3 + 1;
    switch (c) {
    case 1: Game_Start(0x1B); Task_Start(2, 0x4836E0); break;
    case 2: Game_Start(0x1D); Task_Start(2, 0x485B70); break;
    case 3: Game_Start(0x1C); Task_Start(2, 0x484E00); break;
    }
    U8(t + 2) = 0;
    U16(t + 0x22) = 0x96;
    U8(t + 1)++;
    U32(t + 4) = 0; U32(t + 8) = 0; U32(t + 0xC) = 0; U32(t + 0x10) = 0;
    End_Setup(t);
    Fade_Set(0, 0xA, 1);
}

/* ---- the results ---- */

/* n digits of v from the highest (10^(n-1)); mode 0: 16x16 at step 16,
   1: 16x16 at step 10 (v 0x10), else 8x8 at step 8 */
void DC2CALL End_Digits(u32 x, u32 y, u32 v, u32 n, u32 col, u32 mode)  /* 0x446D80 */
{
    u32 pw = 1;
    if ((u16)n > 1) for (u32 c = (u16)n - 1; c; c--) pw *= 10;
    if ((u16)n) {
        gptr r = Tmp_Push(8);
        u32 x0 = x, x1 = x, x2 = x;
        for (u32 c = (u16)n; c; c--, x0 += 0x10, x1 += 0xA, x2 += 8) {
            u32 q = v / pw;
            v -= (q & 0xFFFF) * pw;
            pw /= 10;
            if ((u16)mode == 0) Menu_Sprite(rect(r, (u16)x0, (u16)y, 0x10, 0x10), q << 4, 0, 0x7C00, col, 0, 0xA);
            else if ((u16)mode == 1) Menu_Sprite(rect(r, (u16)x1, (u16)y, 0x10, 0x10), q << 4, 0x10, 0x7C00, col, 0, 0xA);
            else Menu_Sprite(rect(r, (u16)x2, (u16)y, 8, 8), q * 8 + 0x80, 0x30, 0x7C00, col, 0, 0xA);
        }
        Tmp_Pop(8);
    }
    mode5();
}

/* play time hh:mm and seconds */
void DC2CALL End_Time(gptr t, u32 col)                                   /* 0x446C60 */
{
    u32 v = U32(t + 0x14), h = v / 216000;
    if (h > 99) h = 99;
    End_Digits(0xAA, 0x55, h, 2, col, 1);
    v -= h * 216000;
    gptr r = Tmp_Push(8);
    Menu_Sprite(rect(r, 0xC3, 0x58, 0x10, 0x10), 0xF0, 0x30, 0x7C00, col, 0, 0xA);
    u32 m = v / 3600;
    End_Digits(0xC6, 0x55, m, 2, col, 1);
    Menu_Sprite(rect(r, 0xDF, 0x58, 0x10, 0x10), 0xF0, 0x30, 0x7C00, col, 0, 0xA);
    Tmp_Pop(8);
    End_Digits(0xE4, 0x5A, (v - m * 3600) / 60, 2, col, 2);
}

void DC2CALL End_Points(gptr t, u32 col)                                 /* 0x446F30 */
{
    End_Digits(0xAA, 0x6D, U32(G() + 0x120C), 7, col, 1);
}

/* files read: n/11 */
void DC2CALL End_Files(gptr t, u32 col)                                  /* 0x446F60 */
{
    End_Digits(0xAA, 0x8E, U16(t + 0x1E), 2, col, 1);
    gptr r = Tmp_Push(8);
    Menu_Sprite(rect(r, 0xC0, 0x8B, 0x20, 0x10), 0xD0, 0x20, 0x7C00, col, 0, 0xA);
    Tmp_Pop(8);
}

/* a label (sound when it starts to show) and its backing */
static void label(gptr r, u8 c, u32 se, u16 x, u16 y, u16 w, u32 u, u32 v)
{
    if (!c) Se_Play(0, se);
    Menu_Sprite(rect(r, x, y, w, 0x10), u, v, 0x7C00, grey(c), 0, 0xA);
}
static void backing(gptr r, u16 x, u16 y, u16 w, u16 h, u32 col)
{
    Menu_Tile(rect(r, x, y, w, h), col, 1, 0x14);
}

/* the lines faded in one by one (T+5..T+0xD), the grade last; T+2 the line
   being faded, T+0x22 the wait before */
void DC2CALL End_Results(gptr t)                                         /* 0x446670 */
{
    gptr r = Tmp_Push(8);
    u32 s = U8(t + 2);
    if (s - 1 <= 8) {
        if (s >= 9) {                                       /* the grade */
            if (!U8(t + 5)) Se_Play(0, 0x56);
            Menu_Sprite(rect(r, 0xE8, 0xB0, 0x30, 0x30), (u16)(U8(t + 0x20) * 0x30), 0x58, 0x7C01, grey(U8(t + 0xD)), 0, 0xA);
            backing(r, 0xE7, 0xAE, 0x31, 0x33, 0x202020);
        }
        if (s >= 8) {
            if (!U8(t + 5)) Se_Play(0, 0x56);
            Menu_Sprite(rect(r, 0x81, 0xC1, 0x60, 0x10), 0x80, 0x40, 0x7C00, grey(U8(t + 0xC)), 0, 0xA);
            backing(r, 0x7D, 0xBE, 0x65, 0x14, 0x202020);
        }
        if (s >= 7 && U16(t + 0x1E) >= 0xB) {               /* all the files */
            rect(r, 0x30, 0xA2, 0x80, 0x10);
            u32 c = Color_Pulse(0x80C0, 0x4080, 4);
            Menu_Sprite(r, 0, 0x30, 0x7C00, c, 0, 0xA);
            backing(r, 0x2D, 0xA4, 0x86, 0xF, 0);
        }
        if (s >= 6) { End_Files(t, grey(U8(t + 0xA))); backing(r, 0xA9, 0x8F, 0x2E, 0xF, 0x202020); }
        if (s >= 5) {
            label(r, U8(t + 9), 0x57, 0x3E, 0x8B, 0x50, 0, 0x20);
            backing(r, 0x3A, 0x8F, 0x4D, 0xF, 0x202020);
        }
        if (s >= 4) { End_Points(t, grey(U8(t + 8))); backing(r, 0xA9, 0x6E, 0x4D, 0xF, 0x202020); }
        if (s >= 3) {
            label(r, U8(t + 7), 0x56, 0x3E, 0x6A, 0x58, 0xA0, 0x10);
            backing(r, 0x3A, 0x6E, 0x52, 0xF, 0x202020);
        }
        if (s >= 2) { End_Time(t, grey(U8(t + 6))); backing(r, 0xA9, 0x56, 0x4F, 0xF, 0x202020); }
        label(r, U8(t + 5), 0x56, 0x3E, 0x53, 0x50, 0xA0, 0);
        backing(r, 0x3A, 0x56, 0x52, 0xF, 0x202020);
    }
    Tmp_Pop(8);
    mode5();
    s = U8(t + 2);
    if (s == 0) {
        if (!--U16(t + 0x22)) U8(t + 2)++;
    } else if (s <= 8) {
        U8(t + 4 + s) += 0xC;
        if (U8(t + 4 + s) > 0x78) { U8(t + 4 + s) = 0x78; U8(t + 2)++; }
    } else if (s == 9) {
        if (U8(t + 0xD) > 0x78) {
            if (U8(0x87DC12) & 0x60) {
                U8(t + 2)++;
                Se_Play(0, 2);
                Fade_Set(1, 0xA, 1);
            }
        } else U8(t + 0xD) += 0xC;
    } else if (!Flag_Check(2, 6)) {
        Task_Kill(2);
        U8(t + 1)++;
        U8(t + 2) = 0;
    }
}

/* ---- the bonus pictures (shown once: G+0x1094 bits 1, 2) ---- */
static void bonus(gptr t, u32 bit, int files, u16 tpage, u16 clut, gptr text)
{
    switch (U8(t + 2)) {
    case 0:
        U8(t + 2) = 2;
        if (U8(G() + 0x1094) & bit) break;
        if (files && U16(t + 0x1E) < 0xB) break;
        memset(MEM(SPRITES), 0, 0x12C * 4);
        U8(G() + 0x1094) |= (u8)bit;
        U8(t + 2) = 1;
        U16(t + 0x22) = 0x12C;
        Screen_LoadImage(5);
        {
            gptr e = Spr_Alloc();
            Spr_Set(e, 0x5E0000, 0xA0, 0x3C, 0x808080);
            U16(e + 0x10) = tpage; U16(e + 0x12) = clut;
        }
        Fade_Set(0, 0xA, 1);
        break;
    case 1:
        if (U32(0x87DC00) & 0x920) U16(t + 0x22) = 1;
        if (U16(t + 0x22)--) break;
        U8(t + 2)++;
        Se_Play(0, 2);
        Fade_Set(1, 0xA, 1);
        break;
    case 2:
        if (!Flag_Check(2, 6)) { U8(t + 2) = 0; U8(t + 1)++; }
        break;
    }
    Spr_DrawAll();
    Text_Draw(text, Text_Left(text), 0xA0, 0x64, 0x808080, 4, 0);
    draw_ot();
}

void DC2CALL End_Bonus1(gptr t)                                          /* 0x446FD0 */
{
    bonus(t, 1, 1, 0x98, 0x7E40, 0x728964);
}

void DC2CALL End_Bonus2(gptr t)                                          /* 0x447140 */
{
    bonus(t, 2, 0, 0x86, 0x7E80, 0x728A0A);
}

/* ---- the costume shop (S = T+0x9C, modes 0x7215C8; S+1 the step, modes
   0x7215B8; S+8 the costume, S+9 the page, S+0xC: 12 x {u8 flags, the
   costume's sprite, its "new" mark}, S+0x9C the points shown) ---- */

/* the next set bit of m from bit `from` (dir >= 0 up, else down), round;
   from when there is none */
s32 DC2CALL Bit_Next(u32 m, s32 from, s32 dir)                           /* 0x447590 */
{
    u32 b = 1u << (from & 31);
    s32 i = from;
    for (u32 n = 0x1F; n; n--) {
        if (dir >= 0) {
            b <<= 1;
            if (++i > 0x1F) { i = 0; b = 1; }
        } else {
            b >>= 1;
            if (--i < 0) { i = 0x1F; b = 0x80000000; }
        }
        if (m & b) return i;
    }
    return from;
}

/* the page (5 / 10 / 12 costumes as the pages are bought out) and the costumes' sprites */
void DC2CALL Costume_Build(gptr s)                                          /* 0x4472A0 */
{
    static const u8 first[3] = {0x1F, 4, 9};
    u32 have = (u32)S16(G() + 0x1092), m, page, count;
    if ((have & 0x1F) != 0x1F) { page = 0; count = 5; m = have ^ 0x1F; }
    else if ((have & 0x3E0) != 0x3E0) { page = 1; count = 0xA; m = have ^ 0x3FF; }
    else { page = 2; count = 0xC; m = have ^ 0xFFF; }
    U8(s + 9) = (u8)page;
    U8(s + 8) = (u8)Bit_Next(m, first[page], 1);
    u32 bit = 1;
    for (u32 k = 0; k < 12; k++, bit <<= 1) {
        gptr c = COSTUMES + 0x18 * k, r = s + 0xC + 0xC * k;
        U8(r) = 3;
        gptr e = Spr_Alloc();
        u16 x = U16(c + 8), y = U16(c + 0xA);
        U16(e + 0x14) = x; U16(e + 0x16) = y;
        Spr_Set(e, U32(c), x, y, 0x808080);
        U16(e + 0x10) = (u16)(((u32)U8(c + 0xF) << 4) + (U16(c + 0xC) >> 6)) | 0x80;
        U16(e + 0x12) = (u16)((k + 0x1F4) << 6 | 0x30);
        U32(r + 4) = e;
        gptr n = Spr_Alloc();
        Spr_Set(n, 0x600068, x, y, 0x808080);
        U16(n + 0x10) = 0x45; U16(n + 0x12) = 0x7E41;
        U32(r + 8) = n;
        if (!((u32)S16(G() + 0x1092) & bit)) U8(n + 0xD) &= ~2;
        if (k >= count) { U8(r) &= ~2; U8(U32(r + 4) + 0xD) &= ~2; U8(U32(r + 8) + 0xD) &= ~2; }
        if (page == 0 && k < count) { U16(U32(r + 4) + 0x16) = 0x60; U16(U32(r + 8) + 0x16) = 0x60; }
    }
}

/* the costumes this character has not all bought yet */
static int shop_open(void)
{
    gptr g = G();
    switch ((s8)U8(g + 0x108E)) {
    case 0: return (U8(g + 0x1092) & 0x1F) != 0x1F;
    case 1: return (U16(g + 0x1092) & 0x3FF) != 0x3FF;
    default: return (U16(g + 0x1092) & 0xFFF) != 0xFFF;
    }
}

void DC2CALL Costume_Init(gptr t, gptr s)                                   /* 0x447470 */
{
    if (!shop_open()) { U8(t + 2) = 0; U8(t + 1) = 0; U8(t)++; return; }
    U8(s)++;
    Game_Start(0x1A);
    memset(MEM(SPRITES), 0, 0x12C * 4);
    U8(G() + 0x1092) |= 3;
    Costume_Build(s);
    gptr c = U32(s + 0x10 + 0xC * (s8)U8(s + 8));
    gptr e = Spr_Alloc();
    U32(s + 4) = e;
    Spr_Set(e, 0x600000, U16(c + 0x14), U16(c + 0x16), 0x808080);
    U16(e + 0x10) = 5; U16(e + 0x12) = 0x7E40;
    Flag_Set(3, 0, 1);
    U32(G() + 8) = 0x6C1500;
    U32(s + 0x9C) = U32(G() + 0x120C);
    Fade_Set(0, 0xA, 1);
}

static u32 not_bought(gptr s)
{
    u32 have = (u32)S16(G() + 0x1092);
    switch (U8(s + 9)) {
    case 0: return have ^ 0x1F;
    case 1: return have ^ 0x3FF;
    default: return have ^ 0xFFF;
    }
}

/* choosing a costume */
void DC2CALL Costume_Pick(gptr t, gptr s)                                   /* 0x447600 */
{
    u32 m = not_bought(s), pad = U32(0x87DC00);
    if (pad & 0x8000) { Se_Play(0, 0); U8(s + 8) = (u8)Bit_Next(m, (s8)U8(s + 8), -1); }
    else if (pad & 0x2000) { Se_Play(0, 0); U8(s + 8) = (u8)Bit_Next(m, (s8)U8(s + 8), 1); }
    else if (pad & 0x4000) { Se_Play(0, 0); U8(s + 1) = 3; }
    else if (pad & 0x4040) { Se_Play(0, 1); U8(s + 1) = 3; }
    else if (pad & 0x20) { Se_Play(0, 2); U8(s + 1) = 1; U8(s + 2) = 2; U8(s + 0xA) = 0; }
}

static void shop_frame(void)
{
    gptr r = Tmp_Push(8);
    Menu_Tile(rect(r, 0, 0x50, 0x140, 0x58), 0x808080, 3, 4);
    Tmp_Pop(8);
}

/* buying: the points counted down (0), the page scrolled in (1), yes / no (2), too few points (3) */
void DC2CALL Costume_Buy(gptr t, gptr s)                                    /* 0x4476F0 */
{
    s32 k = (s8)U8(s + 8);
    switch (U8(s + 2)) {
    case 0:
        U32(s + 0x9C) -= U32(COSTUMES + 0x10 + 0x18 * k) / 15;
        if (S32(G() + 0x120C) >= S32(s + 0x9C)) {
            U32(s + 0x9C) = U32(G() + 0x120C);
            U8(s + 1) = 2; U8(s + 2) = 0;
        }
        break;
    case 1:
        for (gptr a = s + 0xC; a <= s + 0x3C; a += 0xC) {
            U16(U32(a + 4) + 0x16) -= 3;
            U16(U32(a + 8) + 0x16) -= 3;
        }
        if (U16(U32(s + 0x10) + 0x16) <= 0x40) { U8(s + 1) = 0; U8(s + 2) = 0; }
        break;
    case 2: {
        u32 pad = U32(0x87DC00);
        if (pad & 0x8000) { Se_Play(0, 0); U8(s + 0xA) = 0; }
        else if (pad & 0x2000) { Se_Play(0, 0); U8(s + 0xA) = 1; }
        else if (pad & 0x40) { Se_Play(0, 1); U8(s + 1) = 0; U8(s + 2) = 0; }
        else if (pad & 0x20) {
            Se_Play(0, 2);
            if (U8(s + 0xA)) { U8(s + 1) = 0; U8(s + 2) = 0; }
            else if (U32(G() + 0x120C) >= U32(COSTUMES + 0x10 + 0x18 * k)) {
                U32(G() + 0x120C) -= U32(COSTUMES + 0x10 + 0x18 * k);
                U16(G() + 0x1092) |= (u16)(1u << (k & 31));
                U8(U32(s + 0x14 + 0xC * k) + 0xD) |= 2;
                U8(s + 1) = 1; U8(s + 2) = 0;
            } else U8(s + 2)++;
        }
        k = (s8)U8(s + 8);
        gptr b = Tmp_Push(0xC0);
        Msg_Concat(b, 0x7288E6, U32(COSTUMES + 0x14 + 0x18 * k));
        Msg_Concat(b, b, 0x728902);
        Text_Draw(b, Text_Left(b), 0x64, 0x64, 0x808080, 4, 0);
        Tmp_Pop(0xC0);
        Menu_YesNo(0x64, 0x8C, U8(s + 0xA), 4);
        shop_frame();
        break; }
    case 3:
        Text_Draw(0x728922, Text_Left(0x728922), 0x78, 0x64, 0x808080, 4, 0);
        shop_frame();
        if (U8(0x87DC00) & 0x60) { U8(s + 1) = 0; U8(s + 2) = 0; }
        break;
    }
}

/* after a purchase: on to the next page when this one is bought out (or done) */
void DC2CALL Costume_Check(gptr t, gptr s)                                  /* 0x4479A0 */
{
    u32 have = (u32)S16(G() + 0x1092), m, page;
    if ((have & 0x1F) != 0x1F) { page = 0; m = have ^ 0x1F; }
    else if ((have & 0x3E0) != 0x3E0) { page = 1; m = have ^ 0x3FF; }
    else if ((have & 0xC00) != 0xC00) { page = 2; m = have ^ 0xFFF; }
    else { page = 3; m = have ^ 0xFFF; }
    if (U8(s + 9) == page) {
        U8(s + 1) = 0;
        U8(s + 8) = (u8)Bit_Next(m, (s8)U8(s + 8), 1);
        return;
    }
    U8(s + 9) = (u8)page;
    gptr a, e;
    switch (page) {
    case 1:
        if (!U8(G() + 0x108E)) { U8(s + 1) = 3; return; }
        U8(s + 1) = 1; U8(s + 2) = 1; U8(s + 8) = 5;
        a = s + 0x48; e = s + 0x78;
        break;
    case 2:
        if (U8(G() + 0x108E) == 1) { U8(s + 1) = 3; return; }
        U8(s + 1) = 0; U8(s + 8) = 0xA;
        a = s + 0x84; e = s + 0x90;
        break;
    case 3: U8(s + 1) = 3; return;
    default: return;
    }
    for (; a <= e; a += 0xC) { U8(a) |= 2; U8(U32(a + 4) + 0xD) |= 2; }
}

void DC2CALL Costume_Done(gptr t, gptr s)                                   /* 0x447AD0 */
{
    U8(s)++; U8(s + 1) = 0;
}

void DC2CALL Costume_Run(gptr t, gptr s)                                    /* 0x447AE0 */
{
    switch (U8(s + 1)) {
    case 0: Costume_Pick(t, s); break;
    case 1: Costume_Buy(t, s); break;
    case 2: Costume_Check(t, s); break;
    case 3: Costume_Done(t, s); break;
    default: ((void (DC2CALL *)(gptr, gptr))(uintptr_t)U32(0x7215B8 + 4 * U8(s + 1)))(t, s); break;
    }
    gptr e = U32(s + 4), c = U32(s + 0x10 + 0xC * (s8)U8(s + 8));
    U16(e + 0x14) = U16(c + 0x14);
    U16(e + 0x16) = U16(c + 0x16);
}

/* leaving: back (to the last page) or on */
void DC2CALL Costume_Exit(gptr t, gptr s)                                   /* 0x447B30 */
{
    if (U8(s + 1) == 0) {
        gptr e = U32(s + 4);
        U8(s + 1)++;
        U16(e + 0x14) = 0xF0; U16(e + 0x16) = 0xC8;
        Spr_Set(e, 0x600070, 0xF0, 0xC8, 0x808080);
    } else if (U8(s + 1) == 1) {
        u32 pad = U32(0x87DC00);
        if (pad & 0x1000) {
            if (U8(s + 9) >= (s32)(s8)U8(G() + 0x108E) + 1) return;
            U8(s + 1) = 0;
            U8(s)--;
            gptr c = U32(s + 0x10 + 0xC * (s8)U8(s + 8));
            Spr_Set(U32(s + 4), 0x600000, U16(c + 0x14), U16(c + 0x16), 0x808080);
        } else if (pad & 0x820) {
            Se_Play(0, 1);
            U8(s)++; U8(s + 1) = 0;
            Fade_Set(1, 0xA, 1);
        }
    }
}

void DC2CALL Costume_End(gptr t, gptr s)                                    /* 0x447C20 */
{
    if (!Flag_Check(2, 6)) { U8(t)++; U8(t + 2) = 0; U8(t + 1) = 0; }
}

void DC2CALL End_Shop(gptr t)                                            /* 0x447C50 */
{
    gptr s = t + 0x9C;
    switch (U8(s)) {
    case 0: Costume_Init(t, s); break;
    case 1: Costume_Run(t, s); break;
    case 2: Costume_Exit(t, s); break;
    case 3: Costume_End(t, s); break;
    default: ((void (DC2CALL *)(gptr, gptr))(uintptr_t)U32(0x7215C8 + 4 * U8(s)))(t, s); break;
    }
    Spr_DrawAll();
    Menu_Number11(0x8C, 0xC8, 7, 0, 0x808080, U32(s + 0x9C), 5);
    for (u32 k = 0; k < 12; k++) {
        gptr r = s + 0xC + 0xC * k, e = U32(r + 8);
        if (!(U8(r) & 2) || (U8(e + 0xD) & 2)) continue;
        Menu_Number11(U16(e + 0x14), U16(e + 0x16) + 0x35, 6, 0, 0x808080, U32(COSTUMES + 0x10 + 0x18 * k), 5);
    }
    draw_ot();
}

/* ---- the run, the restart ---- */
void DC2CALL End_Run(gptr t)                                             /* 0x446390 */
{
    switch (U8(t + 1)) {
    case 0: End_Movie1(t); break;
    case 1: End_Movie2(t); break;
    case 2: End_Results(t); break;
    case 3: End_Bonus1(t); break;
    case 4: End_Bonus2(t); break;
    case 5: End_Shop(t); break;
    default: Orig_Call1(U32(0x72147C + 4 * U8(t + 1)), t); break;
    }
}

/* the game from the start, keeping games cleared, character, costumes,
   bonus bits and play time; a copy of the game state for the next game */
void DC2CALL End_Restart(gptr t)                                         /* 0x447D20 */
{
    gptr g = G();
    u8 cleared = U8(g + 0x108D), ch = U8(g + 0x108E), bon = U8(g + 0x1094);
    u16 cos = U16(g + 0x1092);
    u32 tm = U32(g + 0x1098);
    Obj_Free(U32(g + 0x940));
    U32(U32(g + 0x940) + 0x50) &= ~2u;
    U16(g + 0x772) = 0; U16(g + 0x770) = 0; U16(g + 0x76E) = 0; U16(g + 0x76C) = 0;
    memset(MEM(g + 0x108C), 0, 0x118 * 4);
    Game_NewState();
    g = G();
    U8(g + 0x108D) = cleared; U8(g + 0x108E) = ch; U16(g + 0x1092) = cos;
    U8(g + 0x1094) = bon; U32(g + 0x1098) = tm;
    memcpy(MEM(0x87F4E0), MEM(g + 0x108C), 0x118 * 4);
    memset(MEM(g + 0x938), 0, 0xBC * 4);
    memset(MEM(g + 0x664), 0, 0x42 * 4);
    Flag_Set(2, 0x22, 1);
    Game_Start(0x15);
}

void DC2CALL EndingTask(void)                                               /* 0x4462D0 */
{
    memset(MEM(END_T), 0, 0x4F * 4);
    Flag_Set(2, 0x22, 1);
    for (;;) {
        switch (U8(END_T)) {
        case 0: End_Init(END_T); break;
        case 1: End_Run(END_T); break;
        case 2: End_Restart(END_T); break;
        default: Orig_Call1(U32(0x721470 + U8(END_T) * 4), END_T); break;
        }
        Task_Sleep(1);
    }
}
