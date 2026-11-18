/* The save room's terminal (task 0x40D710, from the save room): save the
 * game, or spend the extinction points on weapons, ammunition, items and
 * the files of the dinosaurs. Its state at 0x746D28 (0x10C bytes): +0 the
 * mode (table 0x71E130: 0 set-up, 1 save?, 2 the menu, 3..6 the shops, 7
 * leaving), +4/+6 a page's steps, +5 the menu's item, +0xB save yes/no,
 * +0xD6 the side panels sliding, +0xE4 a page's counter, +0x100/+0x102 "new
 * items" marks of the weapon and file shops, +0x104 the room's camera kept.
 * The prices by difficulty are set once from the normal ones (0x71DCB8...).
 * The background is the save slots screen's (savefile.c). */
#include "dc2/game.h"
#include "dc2/gte.h"
#include "dc2/orig.h"
#include "dc2/pcio.h"
#include "dc2/task.h"

#define SH 0x746D28u

static void lights_a0(void)
{
    gptr g = G();
    U8(g + 0xB58) = 0xA0; U8(g + 0xB59) = 0xA0; U8(g + 0xB5A) = 0xA0;
    U8(g + 0xB5B) = 0; U8(g + 0xB5C) = 0;
}

/* the prices by difficulty (easy: half, hard: double), from the normal
   tables; 9999 marks one not for sale */
void DC2CALL Shop_Prices(gptr o)                                     /* 0x40D820 */
{
    (void)o;
    u8 d = U8(G() + 0x108E);
    for (u32 i = 0; i < 0x1A; i++) {
        gptr p = 0x71D7E8 + 8 * i;
        if (U32(p) == 9999) continue;
        if (d == 0) U32(p) = U32(0x71DCB8 + 8 * i) >> 1;
        else if (d == 2) U32(p) = U32(0x71DCB8 + 8 * i) << 1;
    }
    for (u32 i = 0; i < 5; i++)
        if (!U8(G() + 0x108E)) U16(0x71D8BE + 8 * i) = (u16)(U16(0x71DD8E + 8 * i) >> 1);
    if (U8(G() + 0x108E) == 2) U16(0x71D8E6) = 0x2710;
    for (u32 i = 0; i < 0x1A; i++) {
        gptr p = 0x71D8E8 + 0x20 * i;
        if (U32(p) == 9999 || U8(G() + 0x108E)) continue;
        U32(p) = U32(p + 0x4D0) >> 1;
        U32(p + 0x14) = U32(p + 0x4D0 + 0x14) >> 1;
        U32(p + 0x18) = U32(p + 0x4D0 + 0x18) >> 1;
        U32(p + 0x1C) = U32(p + 0x4D0 + 0x1C) >> 1;
    }
    for (u32 i = 0; i < 7; i++)
        if (!U8(G() + 0x108E)) U16(0x71DC2E + 8 * i) = (u16)(U16(0x71E0FE + 8 * i) >> 1);
}

/* mode 0 */
void DC2CALL Shop_Init(gptr o)                                       /* 0x40D920 */
{
    U32(o) = 1;
    U8(o + 0xB) = 0;
}

/* the title glyph and the corner label (shops: 0x65F580, else 0x65F578) */
void DC2CALL Shop_Title(gptr o)                                      /* 0x40E170 */
{
    gptr t = Tmp_Push(4);
    U16(t) = 0x1B; U16(t + 2) = 0xF000;
    Text_Draw(t, 0x84, 0xD6, 0x64, 0x404040, 2, 0);
    Tmp_Pop(4);
    s8 m = (s8)U8(o);
    Menu_DrawFrames(m > 1 && m < 7 ? 0x65F580 : 0x65F578, 0x98, 0xD6, 0x16, 0x7C8E, 0x8068C040, 2);
}

/* the frame: the save / shop labels sliding with +0xD6, the header, the
   two corner pieces, the title */
void DC2CALL Shop_Frame(gptr o)                                      /* 0x40E010 */
{
    if (U8(o) == 1 || U8(o) == 7) {
        U16(o + 0xD6) -= 4;
        if (S16(o + 0xD6) < 0) U16(o + 0xD6) = 0;
    } else {
        U16(o + 0xD6) += 4;
        if (S16(o + 0xD6) > 0x40) U16(o + 0xD6) = 0x40;
    }
    u32 a = U8(o + 0xB) ? 0x303030 : 0x808080, b = U8(o + 0xB) ? 0x808080 : 0x303030;
    Menu_DrawFrames(0x65F538, (u32)(0x84 - S16(o + 0xD6)), 0x50, 0x16, 0x7C8E, a, 0xF);
    Menu_DrawFrames(0x65F530, (u32)(0x84 - S16(o + 0xD6)), 0x70, 0x16, 0x7C8E, b, 0xF);
    Menu_DrawFrames(0x65F500, 0xA0, 0x20, 0x16, 0x7C8F, Color_Pulse(0x808080, 0x404040, 4), 0xF);
    Menu_DrawFrames(0x65F520, 0x50, 0x18, 0x16, 0x7C8F, Color_Pulse(0x8020C0C0, 0x106060, 4), 0xA);
    Menu_DrawFrames(0x65F528, 0xF0, 0x18, 0x16, 0x7C8F, Color_Pulse(0x8020C0C0, 0x106060, 4), 0xA);
    Shop_Title(o);
}

/* three pulsing arrows pointing left / right */
void DC2CALL Shop_ArrowsL(u32 x, u32 y)                             /* 0x40E200 */
{
    x &= 0xFFFF; y &= 0xFFFF;
    Menu_DrawFrames(0x65FBE0, x, y, 0x16, 0x7C8F, Color_Pulse(0x80002060, 0x80000840, 4), 2);
    Menu_DrawFrames(0x65FBE8, x + 4, y, 0x16, 0x7C8F, Color_Pulse(0x80004080, 0x80000840, 4), 2);
    Menu_DrawFrames(0x65FBF0, x + 7, y, 0x16, 0x7C8F, Color_Pulse(0x800060A0, 0x80000840, 4), 2);
}
void DC2CALL Shop_ArrowsR(u32 x, u32 y)                             /* 0x40E2B0 */
{
    x &= 0xFFFF; y &= 0xFFFF;
    Menu_DrawFrames(0x65FBF8, x - 8, y, 0x16, 0x7C8F, Color_Pulse(0x80002060, 0x80000840, 4), 2);
    Menu_DrawFrames(0x65FC00, x - 0xC, y, 0x16, 0x7C8F, Color_Pulse(0x80004080, 0x80000840, 4), 2);
    Menu_DrawFrames(0x65FC08, x - 0xF, y, 0x16, 0x7C8F, Color_Pulse(0x800060A0, 0x80000840, 4), 2);
}

/* what is for sale now (by the flags 6 of the table 0x71D790 and the
   kinds' costume masks 0x70426A): the lists kept per costume in G+0x1224..
   0x122C, a "new" mark when they grew; the files on sale not read yet
   counted against G+0x1230 */
void DC2CALL Shop_Check(gptr o)                                      /* 0x40DAD0 */
{
    u32 both = 0, m1 = 0, m2 = 0, m4 = 0;
    for (gptr e = 0x71D790; U16(e); e += 8) {
        if (!U8(G() + 0x108E) && U16(e + 2) == 6) continue;
        if (!Flag_Check(6, U16(e + 4))) continue;
        u32 t = U16(e + 2) & 31;                    /* (x86 shifts by the low 5 bits of cl) */
        u16 w = U16(0x70426A + 12 * U16(e + 2));
        if (w & 1) m1 |= 1u << t;
        if (w & 2) m2 |= 1u << t;
        if (w & 4) m4 |= 1u << t;
        if ((w & 1) && (w & 2)) both |= 1u << t;
    }
    gptr g = G();
    switch (U8(g + 0x108F)) {
    case 0:
        U16(o + 0x100) = U32(g + 0x1224) != m1;
        U32(G() + 0x1224) = m1;
        U32(G() + 0x1228) |= both;
        break;
    case 1:
        U16(o + 0x100) = U32(g + 0x1228) != m2;
        U32(G() + 0x1228) = m2;
        U32(G() + 0x1224) |= both;
        break;
    case 2:
        U16(o + 0x100) = U32(g + 0x122C) != m4;
        U32(G() + 0x122C) = m4;
        break;
    }
    u32 n = 0;
    for (gptr e = 0x71DC28; U16(e); e += 8)
        if (Flag_Check(6, U16(e + 4)) && !Flag_Check(9, U16(e + 2))) n++;
    n &= 0xFFFF;
    U16(o + 0x102) = U32(G() + 0x1230) < n;
    U32(G() + 0x1230) = n;
}

/* mode 1: save the game? (yes: the save slots screen; no: the menu) */
void DC2CALL Shop_SaveAsk(gptr o)                                    /* 0x40D930 */
{
    u32 p = U32(0x87DC00);
    if (p & 0x5000) { Se_Play(0, 0); U8(o + 0xB) ^= 1; }
    else if (U8(0x87DC00) & 0x40) U32(o) = 7;
    else if (U8(0x87DC00) & 0x20) {
        Se_Play(0, 2);
        if (U8(o + 0xB)) {
            Shop_Frame(o);
            lights_a0();
            DrawSync(0);
            u32 b = U32(0x87DE0C) << 12;
            Gpu_DrawOT(U32(0x87F94C) + 0x78, b + 0x87CBFC, b + 0x87BC00);
            U16(0x87DC08) = 0; U16(0x87DC94) = 0; U16(0x87DDFC) = 0; U16(0x87DC00) = 0;
            Task_Chain(0x46D690);
        } else {
            Shop_Check(o);
            U32(o) = 2;
            U8(o + 5) = 0;
        }
    }
    Shop_Frame(o);
    gptr t = U32(0x71E150 + 4 * (s32)(s8)U8(o + 0xB));
    Text_Draw(t, Text_Left(t), 0xA0, 0x64, 0x808080, 0xA, 0);
    U32(0x87DE00) = 2;
    lights_a0();
    DrawSync(0);
}

/* mode 2: the menu (weapons, items, files, ...), "new" marks on the
   weapon and file shops */
void DC2CALL Shop_Menu(gptr o)                                       /* 0x40DCF0 */
{
    static const u32 help[4] = {0x724934, 0x72494A, 0x724964, 0x724988};
    u32 p = U32(0x87DC00);
    if (p & 0x1000) {
        Se_Play(0, 0);
        if ((s8)--U8(o + 5) < 0) U8(o + 5) = 3;
    } else if (p & 0x4000) {
        Se_Play(0, 0);
        if ((s8)++U8(o + 5) > 3) U8(o + 5) = 0;
    } else if (p & 0x40) {
        Se_Play(0, 1);
        U32(o) = 1;
    } else if (p & 0x20) {
        U16(o + 0xD6) = 0x40;
        Se_Play(0, 2);
        U16(o + 0xE4) = 0; U8(o + 4) = 0; U8(o + 6) = 0;
        switch (U8(o + 5)) {
        case 0: U32(o) = 3; break;
        case 1: U32(o) = 4; break;
        case 2: U32(o) = 5; break;
        case 3: U32(o) = 6; break;
        }
    }
    Shop_Frame(o);
    u32 c[4] = {0x303030, 0x303030, 0x303030, 0x303030};
    if (U8(o + 5) < 4) c[U8(o + 5)] = 0x808080;
    Menu_DrawFrames(0x65F540, 0xB4, 0x46, 0x16, 0x7C8E, c[0], 0xF);
    Menu_DrawFrames(0x65F548, 0xB4, 0x5A, 0x16, 0x7C8E, c[1], 0xF);
    Menu_DrawFrames(0x65F550, 0xB4, 0x6E, 0x16, 0x7C8E, c[2], 0xF);
    Menu_DrawFrames(0x65FC70, 0xB4, 0x82, 6, 0x7C8E, c[3], 0xF);
    u8 k = U8(o + 5);
    if ((k == 0 && U16(o + 0x100)) || (k == 3 && U16(o + 0x102)))
        Text_Draw(0x72469A, Text_Left(0x72469A), 0xB4, 0x64, Color_Pulse(0x4040B0, 0x202060, 4), 0xA, 0);
    gptr t = help[(s8)U8(o + 5)];
    Text_Draw(t, Text_Left(t), 0xA0, 0x64, 0x808080, 0xA, 0);
    U32(0x87DE00) = 2;
    lights_a0();
    DrawSync(0);
}

static gptr rect(u16 x, u16 y, u16 w, u16 h)
{
    gptr r = Tmp_Push(8);
    U16(r) = x; U16(r + 2) = y; U16(r + 4) = w; U16(r + 6) = h;
    return r;
}
static s32 ftol(float f) { return (s32)(int64_t)f; }
static void mode_into(u32 ot, u32 tpage)
{
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, tpage, 0);
    Gpu_AddPrim(ot, U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}
#define KIND(t) (0x704260 + 12 * (u32)(t))      /* the item kinds: name, text, u, v, flags */

/* a weapon's figures: power, rate, range as bars and numbers growing with
   +0xE8, its picture, name and text */
void DC2CALL WS_Stats(gptr o, u32 t)                                 /* 0x4097C0 */
{
    t &= 0xFFFF;
    Menu_DrawFrames(0x685018, 0x18, 0x58, 0xC, 0x7C8F, 0x808080, 2);
    Menu_Line(0x3A, 0x62, 0x3A, 0x90, 0x68C040, 0, 2);
    U16(o + 0xE8) += 4;
    if (S16(o + 0xE8) > 0x64) U16(o + 0xE8) = 0x64;
    static const u16 by[3] = {0x66, 0x76, 0x86};
    static const u32 bc[3] = {0x4040C0, 0x40C040, 0xC04040};
    for (u32 k = 0; k < 3; k++) {
        gptr r = rect(0x3B, by[k], (u16)((s32)U8(0x7041F8 + 4 * t + k) * S16(o + 0xE8) / 200), 0xA);
        Menu_Tile(r, bc[k], 2, 2);
        Tmp_Pop(8);
    }
    static const u32 lf[3] = {0x685418, 0x685420, 0x685428};
    static const u16 lx[3] = {6, 0x18, 0}, ly[3] = {0x64, 0x74, 0x84};
    for (u32 k = 0; k < 3; k++) {
        Menu_DrawFrames(lf[k], lx[k], ly[k], 0xC, 0x7C8F, bc[k], 2);
        Menu_Number8(0x18, ly[k] + 8, 3, 0, 0x808080, (u32)((s32)U8(0x7041F8 + 4 * t + k) * S16(o + 0xE8) / 100), 0x64);
    }
    gptr r = U8(KIND(t) + 0xA) & 0x40 ? rect(0x38, 0x30, 0x40, 0x20) : rect(0x18, 0x30, 0x80, 0x20);
    Menu_Sprite(r, U8(KIND(t) + 8), U8(KIND(t) + 9), 0x7C80, 0x808080, 0, 0xA);
    Tmp_Pop(8);
    mode_into(0x87BC28 + (U32(0x87DE0C) << 12), 0x8D);
    Text_Draw(U32(KIND(t)), 0x84, 0x30, 0x64, 0x8080, 1, 0);
    Text_Draw(U32(KIND(t) + 4), 0x84, 0x48, 0x3E8, 0x808080, 1, 0);
}

/* one weapon of the carousel at its projected place: a pulsing frame
   when chosen, its picture and price grayed by depth */
void DC2CALL WS_Item(gptr o, u32 x, u32 y, u32 k, u32 z)             /* 0x409520 */
{
    u32 zz = z & 0xFFFF, g = 0xC0 - (zz >> 1), col = g << 8 | g << 16 | g;
    k &= 0xFFFF;
    gptr e = U32(o + 0x10 + 4 * k);
    u32 t = U16(e + 2);
    int wide = !(U8(KIND(t) + 0xA) & 0x40);
    if (k == (u32)(s32)(s8)U8(o + 6))
        Menu_DrawFrames(wide ? 0x685178 : 0x685108, x, y, 0xC, 0x7C8E, Color_Pulse(0xC0C0C0, 0x606060, 5) | 0x80000000, zz + 1);
    gptr r = wide ? rect((u16)(x - 0x40), (u16)(y + 0xC), 0x80, 0x20) : rect((u16)(x - 0x20), (u16)(y + 0xC), 0x40, 0x20);
    Menu_Sprite(r, U8(KIND(t) + 8), U8(KIND(t) + 9), 0x7C80, col, 0, z);
    Tmp_Pop(8);
    mode_into(0x87BC00 + 4 * ((U32(0x87DE0C) << 10) + zz), 0x8D);
    z--;
    Menu_Number11(x - 0x20, y + 0x28, 6, 0, col & 0xFFFFFF, U32(0x71D7E8 + 8 * U16(e + 2)), z);
    r = rect((u16)(x + 0x10), (u16)(y + 0x28), 0x10, 0xA);
    Menu_Sprite(r, 0x50, 0, 0x7C8F, col & 0xFF00, 0, z);
    Tmp_Pop(8);
    mode_into(0x87BBFC + 4 * ((U32(0x87DE0C) << 10) + zz), 0xC);
}

/* the carousel of the weapons on sale, turning to the chosen one (the
   left button turns it the other way when half round) */
void DC2CALL WS_Carousel(gptr o)                                     /* 0x409360 */
{
    s32 n = (s8)U8(o + 9);
    s32 step = 0x1000 / n;
    u32 target = 0u - (u32)((s32)(s8)U8(o + 6) * (step & 0xFFF));
    u32 d = Angle_Approach(U16(o + 0xE4), (u32)(step >> 4) & 0xFF, target);
    if (((target - U16(o + 0xE4)) & 0xFFF) == 0x800 && (U32(0x87DDFC) & 0x8000)) d = 0u - d;
    U16(o + 0xE4) += (u16)d;
    gptr a = Tmp_Push(8), m = Tmp_Push(0x20), m2 = Tmp_Push(0x20), v = Tmp_Push(8), sxy = Tmp_Push(8), p = Tmp_Push(4), f = Tmp_Push(4);
    U16(a) = 0x980; U16(a + 2) = U16(o + 0xE4); U16(a + 4) = 0;
    RotMatrix(a, m);
    S32(m + 0x14) = 0; S32(m + 0x18) = -0x120; S32(m + 0x1C) = 0x340;
    U16(a + 4) = 0; U16(a + 2) = 0; U16(a) = 0;
    if (!(u16)d && n > 1) { Shop_ArrowsL(0xC8, 0x76); Shop_ArrowsR(0x78, 0x76); }
    for (u32 k = 0; (s32)k < n; k = (u8)(k + 1)) {
        RotMatrix(a, m2);
        U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = 0x1B8;
        ApplyMatrixSV(m2, v, v);
        SetRotMatrix(m);
        SetTransMatrix(m);
        s32 z = RotTransPers(v, sxy, p, f);
        U16(a + 2) = (u16)((0x1000 / n + (s32)U16(a + 2)) & 0xFFF);
        s32 sy = ftol(F32(sxy + 4)), sx = ftol(F32(sxy));
        WS_Item(o, (u32)sx, (u32)sy, k, (u32)z);
    }
    Tmp_Pop(0x60);
}

/* step 0 */
void DC2CALL WS_Init(gptr o)                                         /* 0x408D80 */
{
    U8(o + 1) = 1; U8(o + 6) = 0; U16(o + 0xE4) = 0; U8(o + 4) = 0;
}

/* step 1: turning the carousel (left/right, a pause of 16 frames), 0x20
   to buy, 0x40 back */
void DC2CALL WS_Browse(gptr o)                                       /* 0x408DA0 */
{
    if (U8(o + 2)) U8(o + 2)--;
    else {
        u32 p = U32(0x87DC00);
        if (p & 0x2000) {
            Se_Play(0, 8);
            s8 i = (s8)(U8(o + 6) + 1);
            U8(o + 2) = 0x10; U8(o + 6) = (u8)i;
            if (i > (s8)U8(o + 9) - 1) U8(o + 6) = 0;
        } else if (p & 0x8000) {
            Se_Play(0, 8);
            U8(o + 2) = 0x10;
            if ((s8)--U8(o + 6) < 0) U8(o + 6) = (u8)(U8(o + 9) - 1);
        } else if (p & 0x40) {
            Se_Play(0, 1);
            U8(o + 1) = 7;
        } else if (p & 0x20) {
            Se_Play(0, 2);
            U8(o + 1) = 2; U8(o + 4) = 0; U16(o + 0xEA) = 0;
        }
    }
    WS_Carousel(o);
    gptr e = U32(o + 0x10 + 4 * (s32)(s8)U8(o + 6));
    u32 t = U16(e + 2);
    Text_Draw(U32(KIND(t)), 0x20, 0xAA, 0x64, 0x8080, 1, 0);
    Text_Draw(U8(KIND(t) + 0xA) & 0x20 ? 0x724AA2 : 0x724ABE, 0x30, 0xBA, 0x64, 0x808080, 1, 0);
    Text_Draw(U8(KIND(t) + 0xA) & 8 ? 0x724A74 : 0x724A8C, 0xA4, 0xBA, 0x64, 0x808000, 1, 0);
}

/* step 2: buy it? (the points paid; not for this costume: step 6; too
   few points: step 4) */
void DC2CALL WS_Confirm(gptr o)                                      /* 0x408F10 */
{
    gptr e = U32(o + 0x10 + 4 * (s32)(s8)U8(o + 6));
    u32 p = U32(0x87DC00);
    if (p & 0xA000) { Se_Play(0, 0); U8(o + 4) ^= 1; }
    else if (p & 0x40) { Se_Play(0, 1); U16(o + 0xE8) = 0; U8(o + 1) = 1; }
    else if (p & 0x20) {
        Se_Play(0, 2);
        U8(o + 1) = 1; U16(o + 0xE8) = 0;
        if (!U8(o + 4)) {
            u32 t = U16(e + 2);
            if (!(U16(0x70426A + 12 * t) & (1u << (U8(G() + 0x108F) & 31)))) { U8(o + 1) = 6; U16(o + 0xEA) = 0; }
            else {
                u32 pts = U32(G() + 0x11F4), price = U32(0x71D7E8 + 8 * t);
                if (pts >= price) { U32(G() + 0x11F4) = pts - price; U8(o + 1) = 3; }
                else U8(o + 1) = 4;
                U16(o + 0xE8) = 0x64; U16(o + 0xEA) = 0;
            }
        }
    }
    Text_Draw(0x7244CA, 0x20, 0xAC, 0x64, 0x808080, 1, 0);
    Menu_YesNo(0x94, 0xBC, U8(o + 4), 1);
    WS_Stats(o, U16(e + 2));
}

/* step 3: "... bought": after a second the weapon goes into a free slot
   of the items with its ammunition */
void DC2CALL WS_Bought(gptr o)                                       /* 0x409050 */
{
    gptr e = U32(o + 0x10 + 4 * (s32)(s8)U8(o + 6));
    WS_Stats(o, U16(e + 2));
    gptr b = Tmp_Push(0x200);
    Msg_Concat(b, 0x7244A8, U32(KIND(U16(e + 2))));
    Msg_Concat(b, b, 0x723FCC);
    Text_Draw(b, 0x20, 0xAC, (u16)(U16(o + 0xEA) << 1), 0x808080, 1, 1);
    Tmp_Pop(0x200);
    U16(o + 0xEA)++;
    if (S16(o + 0xEA) <= 0x3C) return;
    gptr s = G() + 0x10BC + 12 * (s32)(s8)Item_FindFree(G() + 0x10BC, 0x14);
    gptr d = 0x71D7E8 + 8 * U16(e + 2);
    U8(s) = 1; U8(s + 1) = U8(e + 2);
    U16(s + 2) = 0; U32(s + 4) = 0;
    U16(s + 8) = U16(d + 4); U16(s + 0xA) = U16(d + 6);
    U8(o + 6) = 0; U16(o + 0xE4) = 0; U8(o + 4) = 0; U16(o + 0xE8) = 0; U16(o + 0xEA) = 0;
    U8(o + 1) = 1;
}

static void msg_up(gptr o)
{
    U16(o + 0xEA)++;
    if (S16(o + 0xEA) > 0x64) U16(o + 0xEA) = 0x64;
}

/* step 4: not enough points */
void DC2CALL WS_NoPoints(gptr o)                                     /* 0x409160 */
{
    gptr e = U32(o + 0x10 + 4 * (s32)(s8)U8(o + 6));
    if (U8(0x87DC00) & 0x40) { Se_Play(0, 1); U8(o + 1) = 1; U16(o + 0xE8) = 0; U16(o + 0xEA) = 0; }
    WS_Stats(o, U16(e + 2));
    Text_Draw(0x7247C0, 0x18, 0xAC, (u16)(U16(o + 0xEA) << 1), 0x8080C0, 1, 1);
    msg_up(o);
}

/* step 5: nothing left to buy */
void DC2CALL WS_SoldOut(gptr o)                                      /* 0x4091F0 */
{
    if (U8(0x87DC00) & 0x40) { Se_Play(0, 1); U32(o) = 2; U16(o + 0xE8) = 0; U16(o + 0xEA) = 0; }
    Text_Draw(0x72484C, 0x18, 0xAC, (u16)(U16(o + 0xEA) << 1), 0x8080C0, 1, 1);
    msg_up(o);
}

/* step 6: the other character's weapon */
void DC2CALL WS_NotYours(gptr o)                                     /* 0x409270 */
{
    if (U8(0x87DC00) & 0x40) { Se_Play(0, 1); U8(o + 1) = 1; U16(o + 0xE8) = 0; U16(o + 0xEA) = 0; }
    WS_Carousel(o);
    gptr b = Tmp_Push(0x200);
    Msg_Concat(b, 0x72487E, U8(G() + 0x108F) ? 0x72405A : 0x72404C);
    Msg_Concat(b, b, 0x724882);
    Text_Draw(b, 0x18, 0xAC, (u16)(U16(o + 0xEA) << 1), 0x8080C0, 1, 1);
    Tmp_Pop(0x200);
    msg_up(o);
}

/* step 7: back to the menu */
void DC2CALL WS_Back(gptr o)                                         /* 0x409350 */
{
    U32(o) = 2;
}

/* one item of the items' carousel: picture, price (from the entry's +6)
   and points mark, a pulsing frame when chosen, darker when not */
void DC2CALL IS_Item(gptr o, u32 x, u32 y, u32 sel, u32 k, u32 col, u32 z) /* 0x40A370 */
{
    gptr e = U32(o + 0x50 + 4 * (k & 0xFFFF));
    if ((u16)sel)
        Menu_DrawFrames(0x685108, x & 0xFFFF, y & 0xFFFF, 0xC, 0x7C8E, Color_Pulse(0xC0C0C0, 0x606060, 5) | 0x80000000, z & 0xFFFF);
    else { z += 5; col = 0x202020; }
    u32 t = U16(e + 2);
    gptr r = rect((u16)(x - 0x20), (u16)(y + 4), 0x40, 0x20);
    Menu_Sprite(r, U8(KIND(t) + 8), U8(KIND(t) + 9), 0x7C40, col, 0, z);
    Tmp_Pop(8);
    u32 zz = z & 0xFFFF;
    mode_into(0x87BC00 + 4 * ((U32(0x87DE0C) << 10) + zz), 0x95);
    z--;
    Menu_Number11((x & 0xFFFF) - 0x20, (y & 0xFFFF) + 0x24, 6, 0, col & 0xFFFFFF, U16(e + 6), z);
    r = rect((u16)(x + 0x10), (u16)(y + 0x24), 0x10, 0xA);
    Menu_Sprite(r, 0x50, 0, 0x7C8F, col & 0xFF00, 0, z);
    Tmp_Pop(8);
    mode_into(0x87BBFC + 4 * ((U32(0x87DE0C) << 10) + zz), 0xC);
}

/* the items' carousel (flat, seen from above) and the six slots of the
   items carried with their counts */
void DC2CALL IS_Carousel(gptr o)                                     /* 0x40A0F0 */
{
    s32 n = S16(o + 0xD4);
    s32 step = 0x1000 / n;
    u32 target = 0u - (u32)((step & 0xFFF) * (u32)(u16)(s16)(s8)U8(o + 6));
    u32 d = Angle_Approach(U16(o + 0xE4), (u32)(step >> 4) & 0xFF, target);
    if (((target - U16(o + 0xE4)) & 0xFFF) == 0x800 && (U32(0x87DDFC) & 0x8000)) d = 0u - d;
    U16(o + 0xE4) += (u16)d;
    gptr a = Tmp_Push(8), m = Tmp_Push(0x20), v = Tmp_Push(8);
    U16(a) = 0; U16(a + 2) = U16(o + 0xE4); U16(a + 4) = 0;
    if (!(u16)d) { Shop_ArrowsL(0x80, 0x78); Shop_ArrowsR(0x30, 0x78); }
    for (u32 k = 0; (s32)(u16)k < n; k = (u8)(k + 1)) {
        RotMatrix(a, m);
        U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = 0x2D;
        ApplyMatrixSV(m, v, v);
        U16(v) += 0x58;
        U16(v + 4) += 0x3C;
        U16(a + 2) = (u16)((0x1000 / n + (s32)U16(a + 2)) & 0xFFF);
        IS_Item(o, U32(v), U32(v + 4), k == (u32)(s32)(s8)U8(o + 6), k, 0x808080, 0x32);
    }
    Tmp_Pop(0x30);
    gptr s = G() + 0x11AC;
    Menu_DrawFrames(0x65F5B8, 0xF0, 0x29, 0xD, 0x7C8E, 0x808080, 0x14);
    Menu_DrawFrames(0x684FF0, 0xD0, 0x29, 0xC, 0x7C8F, 0x8068C040, 2);
    for (u32 y = 0x33; y < 0x9F; y += 0x12, s += 12) {
        Menu_DrawFrames(0x65F600, 0xE8, y + 7 - 9, 0xD, 0x7C8E, 0x808080, 0x14);
        if (!U8(s)) continue;
        Text_Draw(U32(KIND(U8(s + 1))), 0xB8, y, 0x64, 0x808080, 1, 0);
        gptr b = Tmp_Push(0x40);
        Crt_sprintf3(b, 0x71D738, (u32)(s32)S16(s + 8), (u32)(s32)S16(s + 0xA), 0);
        Text_Ascii(0x118, y + 7, 0xFF00, b);
        Tmp_Pop(0x40);
    }
    Menu_DrawFrames(0x65F710, 0xE8, 0x9F, 0xD, 0x7C8E, 0x808080, 0x14);
}

/* step 0 */
void DC2CALL IS_Init(gptr o)                                         /* 0x409C00 */
{
    U8(o + 1) = 1; U16(o + 0xE4) = 0; U8(o + 6) = 0; U8(o + 4) = 0;
}

/* step 1: turning the items' carousel, 0x20 to buy (no room: step 4) */
void DC2CALL IS_Browse(gptr o)                                       /* 0x409C20 */
{
    if (U8(o + 2)) U8(o + 2)--;
    else {
        u32 p = U32(0x87DC00);
        if (p & 0x2000) {
            Se_Play(0, 8);
            s8 i = (s8)(U8(o + 6) + 1);
            U8(o + 2) = 0x10; U8(o + 6) = (u8)i;
            if (i > S16(o + 0xD4) - 1) U8(o + 6) = 0;
        } else if (p & 0x8000) {
            Se_Play(0, 8);
            U8(o + 2) = 0x10;
            if ((s8)--U8(o + 6) < 0) U8(o + 6) = (u8)(U8(o + 0xD4) - 1);
        } else if (p & 0x40) {
            Se_Play(0, 1);
            U8(o + 1) = 6;
        } else if (p & 0x20) {
            Se_Play(0, 2);
            gptr e = U32(o + 0x50 + 4 * (s32)(s8)U8(o + 6));
            if ((u8)Item_FindRoom(G() + 0x11AC, 6, U8(e + 2)) == 0xFF) { U8(o + 1) = 4; U16(o + 0xE8) = 0; U16(o + 0xEA) = 0; }
            else { U8(o + 1) = 2; U8(o + 4) = 0; }
        }
    }
    IS_Carousel(o);
    gptr e = U32(o + 0x50 + 4 * (s32)(s8)U8(o + 6));
    Text_Draw(U32(KIND(U16(e + 2))), 0x18, 0xB2, 0x64, 0x8080, 1, 0);
    e = U32(o + 0x50 + 4 * (s32)(s8)U8(o + 6));
    Text_Draw(U32(KIND(U16(e + 2)) + 4), 0x88, 0xA7, 0x64, U16(e + 2) == 0x1E ? 0x3030B0 : 0x808080, 1, 0);
}

/* step 2: buy it? (too few points: step 5) */
void DC2CALL IS_Confirm(gptr o)                                      /* 0x409D90 */
{
    u32 p = U32(0x87DC00);
    if (p & 0xA000) { Se_Play(0, 0); U8(o + 4) ^= 1; }
    else if (p & 0x40) { Se_Play(0, 1); U8(o + 1) = 1; }
    else if (p & 0x20) {
        Se_Play(0, 2);
        if (U8(o + 4)) U8(o + 1) = 1;
        else {
            gptr e = U32(o + 0x50 + 4 * (s32)(s8)U8(o + 6));
            s32 price = U16(e + 6), pts = (s32)U32(G() + 0x11F4);
            if (pts < price) U8(o + 1) = 5;
            else { U32(G() + 0x11F4) = (u32)(pts - price); U8(o + 1) = 3; }
            U16(o + 0xEA) = 0;
        }
    }
    IS_Carousel(o);
    Text_Draw(0x7244CA, 0x20, 0xAC, 0x64, 0x808080, 1, 0);
    Menu_YesNo(0x94, 0xBC, U8(o + 4), 1);
}

/* step 3: "... bought", the slot it goes to blinking; after a second one
   more in that slot (its maximum by difficulty) */
void DC2CALL IS_Bought(gptr o)                                       /* 0x409E70 */
{
    gptr e = U32(o + 0x50 + 4 * (s32)(s8)U8(o + 6));
    s32 r = (s8)Item_FindRoom(G() + 0x11AC, 6, U8(e + 2));
    if (U8(o + 0xEA) & 8) {
        e = U32(o + 0x50 + 4 * (s32)(s8)U8(o + 6));
        Text_Draw(U32(KIND(U16(e + 2))), 0xB8, (u32)(r * 18 + 0x33), 0x64, 0x8080, 1, 0);
    }
    IS_Carousel(o);
    e = U32(o + 0x50 + 4 * (s32)(s8)U8(o + 6));
    gptr b = Tmp_Push(0x200);
    Msg_Concat(b, 0x7244A8, U32(KIND(U16(e + 2))));
    Msg_Concat(b, b, 0x723FCC);
    Text_Draw(b, 0x20, 0xAC, (u16)(U16(o + 0xEA) << 1), 0x808080, 1, 1);
    Tmp_Pop(0x200);
    U16(o + 0xEA)++;
    if (S16(o + 0xEA) <= 0x3C) return;
    gptr s = G() + 12 * ((u32)(u16)r + 0x179);
    U8(s) = 1;
    U16(s + 8)++;
    U8(s + 1) = U8(U32(o + 0x50 + 4 * (s32)(s8)U8(o + 6)) + 2);
    U16(s + 2) = 0; U32(s + 4) = 0;
    u8 d = U8(G() + 0x108E);
    U16(s + 0xA) = d == 0 ? 5 : d == 1 ? 2 : 1;
    U8(o + 1) = 1;
}

/* step 4: no room for it */
void DC2CALL IS_Full(gptr o)                                         /* 0x409FF0 */
{
    if (U8(0x87DC00) & 0x40) { Se_Play(0, 1); U32(o) = 2; U16(o + 0xE8) = 0; U16(o + 0xEA) = 0; }
    IS_Carousel(o);
    Text_Draw(0x72476E, 0x18, 0xAC, U16(o + 0xEA), 0x8080C0, 1, 1);
    msg_up(o);
}

/* step 5: not enough points */
void DC2CALL IS_NoPoints(gptr o)                                     /* 0x40A070 */
{
    if (U8(0x87DC00) & 0x40) { Se_Play(0, 1); U8(o + 1) = 1; U16(o + 0xE8) = 0; U16(o + 0xEA) = 0; }
    IS_Carousel(o);
    Text_Draw(0x7247C0, 0x18, 0xAC, (u16)(U16(o + 0xEA) << 1), 0x8080C0, 1, 1);
    msg_up(o);
}

/* mode 4: the items shop (the list 0x71D8B8, one more past normal), its
   step (table 0x71D71C), the frame */
void DC2CALL Shop_Items(gptr o)                                      /* 0x409B20 */
{
    U16(o + 0xD4) = 0;
    for (gptr e = 0x71D8B8; U16(e); e += 8) { U32(o + 0x50 + 4 * S16(o + 0xD4)) = e; U16(o + 0xD4)++; }
    if (U8(G() + 0x108E)) { U32(o + 0x50 + 4 * S16(o + 0xD4)) = 0x71D8E0; U16(o + 0xD4)++; }
    Orig_Call1(U32(0x71D71C + 4 * (s32)(s8)U8(o + 1)), o);
    Menu_DrawFrames(0x65F518, 0xA0, 0x18, 0x16, 0x7C8E, 0x80C0C0C0, 0xF);
    Menu_Points(8, 0x98, 0x808080);
    Menu_Line(0x10, 0xA8, 0x130, 0xA8, 0x802020, 0, 0xF);
    Menu_Line(0x10, 0xD6, 0x130, 0xD6, 0x802020, 0, 0xF);
    Shop_Title(o);
}

/* ---- mode 5: ammunition: bought by the round, or the magazine enlarged ---- */
#define AS_W(o) U32((o) + 0x90 + 4 * (s32)(s8)U8((o) + 6))   /* the weapon's slot in G+0x10BC */

/* one weapon of the ammunition carousel: picture, rounds / magazine */
void DC2CALL AS_Item(gptr o, u32 x, u32 y, u32 k, u32 z)             /* 0x40B4B0 */
{
    k &= 0xFFFF;
    gptr e = U32(o + 0x90 + 4 * k);
    u32 zz = z & 0xFFFF, g = 0xC0 - (zz >> 1), col = g << 8 | g << 16 | g;
    u32 t = U8(e + 1);
    int narrow = U8(KIND(t) + 0xA) & 0x40;
    if (k == (u32)(s32)(s8)U8(o + 6))
        Menu_DrawFrames(narrow ? 0x685108 : 0x685178, x, y, 0xC, 0x7C8E, Color_Pulse(0xC0C0C0, 0x606060, 5) | 0x80000000, zz + 1);
    gptr r = narrow ? rect((u16)(x - 0x20), (u16)(y + 0xC), 0x40, 0x20) : rect((u16)(x - 0x40), (u16)(y + 0xC), 0x80, 0x20);
    Menu_Sprite(r, U8(KIND(t) + 8), U8(KIND(t) + 9), 0x7C80, col, 0, z);
    Tmp_Pop(8);
    mode_into(0x87BC00 + 4 * ((U32(0x87DE0C) << 10) + zz), 0x8D);
    Menu_Number8(x - 0x21, y + 8, 4, 0, col, (u32)(s32)S16(e + 8), z - 1);
    Menu_Number8(x + 2, y + 8, 4, 0, col & 0xFF00FFFF, (u32)(s32)S16(e + 0xA), z - 1);
    r = rect((u16)(x - 5), (u16)(y + 8), 8, 8);
    Menu_Sprite(r, 0x50, 0x10, 0x7C8E, col, 0, z);
    Tmp_Pop(8);
    mode_into(0x87BC00 + 4 * ((U32(0x87DE0C) << 10) + zz), 0xC);
}

/* the carousel of the weapons carried (arrows when asked) */
void DC2CALL AS_List(gptr o, u32 arrows)                             /* 0x40B2E0 */
{
    s32 n = (s8)U8(o + 9);
    s32 step = 0x1000 / n;
    u32 target = 0u - (u32)((s32)(s8)U8(o + 6) * (step & 0xFFF));
    u32 d = Angle_Approach(U16(o + 0xE4), (u32)(step >> 4) & 0xFF, target);
    if (((target - U16(o + 0xE4)) & 0xFFF) == 0x800 && (U32(0x87DDFC) & 0x8000)) d = 0u - d;
    U16(o + 0xE4) += (u16)d;
    gptr a = Tmp_Push(8), m = Tmp_Push(0x20), m2 = Tmp_Push(0x20), v = Tmp_Push(8), sxy = Tmp_Push(8), p = Tmp_Push(4), f = Tmp_Push(4);
    U16(a) = 0x9C0; U16(a + 2) = U16(o + 0xE4); U16(a + 4) = 0;
    RotMatrix(a, m);
    S32(m + 0x14) = 0; S32(m + 0x18) = -0xE0; S32(m + 0x1C) = 0x330;
    U16(a + 4) = 0; U16(a + 2) = 0; U16(a) = 0;
    if (!(u16)d && n > 1 && (u16)arrows) { Shop_ArrowsL(0xC8, 0x9E); Shop_ArrowsR(0x78, 0x9E); }
    for (u32 k = 0; (s32)k < n; k = (u8)(k + 1)) {
        RotMatrix(a, m2);
        U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = 0x1B8;
        ApplyMatrixSV(m2, v, v);
        SetRotMatrix(m);
        SetTransMatrix(m);
        s32 z = RotTransPers(v, sxy, p, f);
        U16(a + 2) = (u16)((0x1000 / n + (s32)U16(a + 2)) & 0xFFF);
        s32 sy = ftol(F32(sxy + 4)), sx = ftol(F32(sxy));
        AS_Item(o, (u32)sx, (u32)sy, k, (u32)z);
    }
    Tmp_Pop(0x60);
}

static void spr(u16 x, u16 y, u16 w, u16 h, u32 u, u32 v, u32 tp, u32 col, u32 semi, u32 z)
{
    gptr r = rect(x, y, w, h);
    Menu_Sprite(r, u, v, tp, col, semi, z);
    Tmp_Pop(8);
}

/* a number in 14-pixel digits (leading zeros when lead), "pts" after it
   when asked */
void DC2CALL AS_Num(u32 x, u32 y, u32 digits, u32 lead, u32 col, u32 v, u32 z, u32 pts) /* 0x40C160 */
{
    u32 c = col & 0xFFFFFF, d = 1, n = 0;
    if ((u16)digits > 1) for (u32 k = (digits & 0xFFFF) - 1; k; k--) d *= 10;
    if ((u16)digits) {
        n = digits;
        u32 xx = x;
        for (u32 left = digits & 0xFFFF; left; left--, xx += 0xE) {
            u32 q = v / d;
            if ((u16)q) lead = 1;
            v -= (q & 0xFFFF) * d;
            d /= 10;
            if (!d) lead = 1;
            else if (!(u16)lead && !(u16)q) continue;
            spr((u16)xx, (u16)y, 0xE, 0xA, (q + 6) << 4, 0xB0, 0x7C8F, c, 0, z);
        }
    }
    if ((u16)pts) spr((u16)(x + 14 * n), (u16)y, 0x18, 0xA, 0x48, 0xB0, 0x7C8F, c, 0, z);
    mode_into(0x87BC00 + 4 * ((U32(0x87DE0C) << 10) + (z & 0xFFFF)), 0xC);
}

/* the rounds panel: the magazine's bar (carried, to buy, room), its
   percentage and numbers, the weapon's name and picture, the price */
void DC2CALL AS_Panel(gptr o)                                        /* 0x40B870 */
{
    gptr e = AS_W(o);
    s16 k = (s8)Item_Find(G() + 0x10BC, 0x14, U8(e + 1));
    s32 mx = S16(e + 0xA), have = S16(e + 8), buy = S16(o + 0xD0);
    s32 room = (mx - have - buy) * 144000 / mx, add = buy * 144000 / mx;
    u32 w1 = 0x90 - (u32)(add + room) / 1000;
    u32 ot = 0x87BC28 + (U32(0x87DE0C) << 12);
    spr(0x78, 0x50, 0xA0, 0x20, 0x40, 0xD8, 0x7C8F, 0x404040, 0, 0xA);
    mode_into(ot, 6);
    spr(0x118, 0x50, 0x10, 0x20, 0xE0, 0xD8, 0x7C8F, 0x404040, 0, 0xA);
    mode_into(ot, 6);
    spr(0x87, 0x59, (u16)w1, 0x10, 0x40, 0xC8, 0x7C8F, 0x308030, 1, 0xA);
    u32 w2 = (u32)add / 1000, w3 = (u32)room / 1000;
    spr((u16)(w1 + 0x87), 0x59, (u16)w2, 0x10, 0x40, 0xC8, 0x7C8F, Color_Pulse(0xC06060, 0xA03030, 4), 1, 0xA);
    spr((u16)(w2 + w1 + 0x87), 0x59, (u16)w3, 0x10, 0x40, 0xC8, 0x7C8F, 0x101060, 1, 0xA);
    mode_into(ot, 6);
    u32 xe = 0x113 - w3;
    Menu_DrawFrames(0x65FC50, xe, 0x50, 6, 0x7C8F, Color_Pulse(0xC05050, 0x802020, 4), 2);
    Menu_DrawFrames(0x65FC60, xe, 0x58, 6, 0x7C8F, Color_Pulse(0x80C0C0C0, 0x80808080, 4), 2);
    Menu_DrawFrames(0x65FC40, 0x7B, 0x5D, 6, 0x7C8F, 0x40, 1);
    Menu_DrawFrames(0x65FC48, 0x11C, 0x5D, 6, 0x7C8F, 0x400000, 1);
    Menu_Number11(0xFA, 0x72, 3, 0, 0x808080, (u32)(have * 100 / mx), 0xA);
    spr(0x113, 0x74, 8, 8, 0x28, 0xF0, 0x7DCE, 0x808080, 0, 0xA);
    mode_into(ot, 9);
    u32 c = U16(o + 0xD0) ? Color_Pulse(0x60C0, 0x4080, 6) : 0x808080;
    AS_Num(0x90, 0x5C, 4, 0, c, (u32)(S16(e + 8) + S16(o + 0xD0)), 0x14, 0);
    spr(0xCA, 0x5C, 0x10, 0x10, 0x88, 0xA0, 0x7C8F, 0x808080, 0, 0x14);
    mode_into(0x87BC50 + (U32(0x87DE0C) << 12), 0xC);
    AS_Num(0xD8, 0x5C, 4, 0, 0x808080, (u32)(s32)S16(e + 0xA), 0x14, 0);
    Text_Draw(U32(KIND(U8(AS_W(o) + 1))), 0x28, 0x2A, 0x64, 0x808080, 1, 0);
    Menu_Line(0x1E, 0x3A, 0x110, 0x3A, 0x68C040, 0, 2);
    spr(0x10, 0x3E, 0x60, 0x60, 0x40, 0, 0x7C8F, 0x60A0, 1, 0xA);
    mode_into(ot, 6);
    spr(0x18, 0x4E, 0x58, 0x50, 0xA8, 0x50, 0x7C8F, 0x60A0, 1, 0xA);
    mode_into(ot, 6);
    gptr s = G() + 12 * (u16)k;
    Menu_Number8(0x20, 0x78, 4, 0, 0x707070, (u32)(s32)S16(s + 0x10C4), 0xA);
    Menu_Number8(0x44, 0x78, 4, 0, 0x7070, (u32)(s32)S16(s + 0x10C6), 0xA);
    spr(0x3C, 0x78, 8, 8, 0x50, 0x10, 0x7C8E, 0x707070, 0, 0xA);
    mode_into(ot, 0xC);
    u32 t = U8(e + 1);
    if (U8(KIND(t) + 0xA) & 0x40) spr(0x20, 0x5C, 0x40, 0x20, U8(KIND(t) + 8), U8(KIND(t) + 9), 0x7C80, 0x808080, 0, 0xA);
    else spr(0, 0x5C, 0x80, 0x20, U8(KIND(t) + 8), U8(KIND(t) + 9), 0x7C80, 0x808080, 0, 0xA);
    mode_into(ot, 0x8D);
    Menu_DrawFrames(0x65F798, 0xD6, 0xA0, 0x16, 0x7C8F, 0x808080, 1);
    Menu_DrawFrames(0x65F7A8, 0x82, 0x78, 0x16, 0x7C8F, 0x808080, 1);
    Menu_DrawFrames(0x65FAE0, 0xE0, 0x88, 0x16, 0x7C8F, 0x80004080, 0xA);
    AS_Num(0xA2, 0x8B, 7, 0, 0x808080, U32(0x71D8E8 + 32 * U8(e + 1)) * (u32)(s32)S16(o + 0xD0), 1, 1);
    Menu_DrawFrames(0x65FB58, 0xC6, 0x78, 0x16, 0x7C8F, 0x50B0, 0xA);
    Menu_DrawFrames(0x65FA98, 0xCE, 0x78, 0x16, 0x7C8F, 0x808080, 0xA);
    AS_Num(0xD9, 0x7C, 3, 0, 0x808080, U32(0x71D8E8 + 32 * U8(e + 1)), 1, 1);
    Menu_Points(0x10, 0xA0, 0x808080);
    Menu_DrawFrames(0x683AB8, 0x50, 0xB0, 0xD, 0x7C8E, 0x80808080, 0xF);
    Menu_DrawFrames(0x683BB8, 0xD8, 0xB0, 0xD, 0x7C8E, 0x80808080, 0xF);
}

/* one of the three enlargements: the magazine pictures stacked (one, two
   or three), the rounds added, the price */
void DC2CALL AS_UpItem(gptr o, u32 x, u32 y, u32 id, u32 z, u32 k)   /* 0x40C7F0 */
{
    u32 zz = z & 0xFFFF, g = 0xE0 - (zz >> 1), col = g << 8 | g << 16 | g;
    k &= 0xFFFF; id &= 0xFFFF;
    if ((s32)(s8)U8(o + 0xE) == (s32)k)
        Menu_DrawFrames(0x685CE0, x, y + 4, 0xC, 0x7C8E, Color_Pulse(0xC0C0C0, 0x606060, 5) | 0x80000000, zz + 3);
    Menu_DrawFrames(U32(0x71D8EC + 32 * id), x + 8, y + 0xC, 0x87, 0x7D00, col, zz);
    Menu_DrawFrames(0x65FE80, x + 8, y + 0xC, 0x87, 0x7D00, col, zz);
    Menu_DrawFrames(0x65FE98, x + 8, y + 0xC, 0x87, 0x7D00, col, zz);
    Menu_DrawFrames(0x65FE88, x - 0x28, y + 7, 0x87, 0x7D00, col, zz);
    z--;
    u32 ix = 4 * (k + 8 * id);
    u32 n = Menu_Number8(x - 0x20, y + 0xA, 4, 0, col & 0xFF00FFFF, U32(0x71D8F0 + ix), z);
    spr((u16)(x - (n * 8 + 8)), (u16)(y + 0xA), 8, 8, 0x70, 0xA0, 0x7C8F, col & 0xFF00, 0, z);
    mode_into(0x87BBFC + 4 * ((U32(0x87DE0C) << 10) + zz), 0xC);
    switch (k) {
    case 0:
        Menu_DrawFrames(0x65FE90, x - 0x28, y + 0xC, 0x87, 0x7D00, col, zz);
        break;
    case 1:
        Menu_DrawFrames(0x65FE90, x - 0x20, y + 0xC, 0x87, 0x7D00, col, zz);
        Menu_DrawFrames(0x65FE90, x - 0x34, y + 8, 0x87, 0x7D00, col, zz + 1);
        break;
    case 2:
        Menu_DrawFrames(0x65FE90, x - 0x20, y + 0x10, 0x87, 0x7D00, col, zz + 1);
        Menu_DrawFrames(0x65FE90, x - 0x30, y + 0xC, 0x87, 0x7D00, col, zz + 2);
        Menu_DrawFrames(0x65FE90, x - 0x22, y - 4, 0x87, 0x7D00, col, zz);
        break;
    }
    Menu_Number11(x - 0x20, y + 0x28, 6, 0, col & 0xFFFFFF, U32(0x71D8FC + ix), z);
    spr((u16)(x + 0x10), (u16)(y + 0x28), 0x10, 0xA, 0x50, 0, 0x7C8F, col & 0xFF00, 0, z);
    mode_into(0x87BBFC + 4 * ((U32(0x87DE0C) << 10) + zz), 0xC);
}

/* the enlargements panel: the three turning, the weapon, its rounds */
void DC2CALL AS_UpPanel(gptr o)                                      /* 0x40C300 */
{
    gptr e = AS_W(o);
    s16 k = (s8)Item_Find(G() + 0x10BC, 0x14, U8(e + 1));
    u32 target = 0u - (u32)(s32)(s8)U8(o + 0xE) * 0x555;
    u32 d = Angle_Approach(U16(o + 0xE6), 0x55, target);
    if (((target - U16(o + 0xE6)) & 0xFFF) == 0x800 && (U32(0x87DDFC) & 0x8000)) d = 0u - d;
    U16(o + 0xE6) += (u16)d;
    gptr a = Tmp_Push(8), m = Tmp_Push(0x20), m2 = Tmp_Push(0x20), v = Tmp_Push(8), sxy = Tmp_Push(8), p = Tmp_Push(4), f = Tmp_Push(4);
    U16(a) = 0x970; U16(a + 2) = U16(o + 0xE6); U16(a + 4) = 0;
    RotMatrix(a, m);
    S32(m + 0x14) = 0; S32(m + 0x18) = -0x122; S32(m + 0x1C) = 0x4AC;
    U16(a + 4) = 0; U16(a + 2) = 0; U16(a) = 0;
    if (!(u16)d) { Shop_ArrowsL(0x100, 0x86); Shop_ArrowsR(0xA0, 0x86); }
    for (u32 i = 0; i < 3; i++) {
        RotMatrix(a, m2);
        U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = 0x1B8;
        ApplyMatrixSV(m2, v, v);
        SetRotMatrix(m);
        SetTransMatrix(m);
        s32 z = RotTransPers(v, sxy, p, f);
        U16(a + 2) = (u16)((U16(a + 2) + 0x555) & 0xFFF);
        s32 sy = ftol(F32(sxy + 4)), sx = ftol(F32(sxy));
        AS_UpItem(o, (u32)sx, (u32)sy, U8(e + 1), (u32)z, i);
    }
    Tmp_Pop(0x60);
    u32 ot = 0x87BC28 + (U32(0x87DE0C) << 12);
    Text_Draw(U32(KIND(U8(e + 1))), 0x28, 0x2A, 0x64, 0x808080, 1, 0);
    Menu_Line(0x1E, 0x3A, 0x110, 0x3A, 0x68C040, 0, 2);
    spr(0x10, 0x3E, 0x60, 0x60, 0x40, 0, 0x7C8F, 0x60A0, 1, 0xA);
    mode_into(ot, 6);
    spr(0x18, 0x4E, 0x58, 0x50, 0xA8, 0x50, 0x7C8F, 0x60A0, 1, 0xA);
    mode_into(ot, 6);
    gptr s = G() + 12 * (u16)k;
    Menu_Number8(0x20, 0x78, 4, 0, 0x707070, (u32)(s32)S16(s + 0x10C4), 0xA);
    Menu_Number8(0x44, 0x78, 4, 0, 0x7070, (u32)(s32)S16(s + 0x10C6), 0xA);
    spr(0x3C, 0x78, 8, 8, 0x50, 0x10, 0x7C8E, 0x707070, 0, 0xA);
    mode_into(ot, 0xC);
    u32 t = U8(e + 1);
    if (U8(KIND(t) + 0xA) & 0x40) spr(0x20, 0x58, 0x40, 0x20, U8(KIND(t) + 8), U8(KIND(t) + 9), 0x7C80, 0x808080, 0, 0xA);
    else spr(0, 0x58, 0x80, 0x20, U8(KIND(t) + 8), U8(KIND(t) + 9), 0x7C80, 0x808080, 0, 0xA);
    mode_into(ot, 0x8D);
    Menu_DrawFrames(0x65F7A0, 0xBE, 0xA0, 0x16, 0x7C8F, 0x808080, 1);
    Menu_Points(0x10, 0xA0, 0x808080);
    Menu_DrawFrames(0x683AB8, 0x50, 0xB0, 0xD, 0x7C8E, 0x80808080, 0xF);
    Menu_DrawFrames(0x683BB8, 0xD8, 0xB0, 0xD, 0x7C8E, 0x80808080, 0xF);
}

/* step 0: the weapons carried that take ammunition (none: step 10) */
void DC2CALL AS_Init(gptr o)                                         /* 0x40A5A0 */
{
    gptr g = G();
    U8(o + 1) = U8(g + 0x1094) & 1 ? 0xA : 1;
    U8(o + 9) = 0; U8(o + 6) = 0; U8(o + 0xE) = 0;
    U16(o + 0xE4) = 0; U16(o + 0xE6) = 0;
    gptr s = g + 0x10BC;
    for (u32 i = 0; i < 0x14; i++, s += 12) {
        if (!U8(s)) continue;
        u16 w = U16(KIND(U8(s + 1)) + 0xA);
        if (w & 0x80) continue;
        if (!(w & (1u << (U8(G() + 0x108F) & 31)))) continue;
        U32(o + 0x90 + 4 * (s32)(s8)U8(o + 9)) = s;
        U8(o + 9)++;
    }
    if (!U8(o + 9)) U8(o + 1) = 0xA;
}

static void as_bottom(void)
{
    Menu_DrawFrames(0x684A40, 0x60, 0xC4, 0xD, 0x7C8E, 0x80808080, 3);
    Menu_DrawFrames(0x684B60, 0xE8, 0xC4, 0xD, 0x7C8E, 0x80808080, 3);
}

/* step 1: the weapon chosen (a pause of 16 frames between moves) */
void DC2CALL AS_Browse(gptr o)                                       /* 0x40A650 */
{
    u8 was = U8(o + 6);
    if (U8(o + 2)) U8(o + 2)--;
    else {
        u32 p = U32(0x87DC00);
        if (p & 0x2000) {
            s8 i = (s8)(was + 1);
            U8(o + 6) = (u8)i; U8(o + 2) = 0x10;
            if (i > (s8)U8(o + 9) - 1) U8(o + 6) = 0;
        } else if (p & 0x8000) {
            s8 i = (s8)(was - 1);
            U8(o + 2) = 0x10; U8(o + 6) = (u8)i;
            if (i < 0) U8(o + 6) = (u8)(U8(o + 9) - 1);
        } else if (p & 0x40) { Se_Play(0, 1); U8(o + 1) = 0xC; }
        else if (p & 0x20) { Se_Play(0, 2); U8(o + 1) = 2; U8(o + 0xC) = 0; }
        if ((s32)(s8)U8(o + 6) != (s32)was) Se_Play(0, 8);
    }
    Menu_DrawFrames(0x65F798, 0x7E, 0x64, 0x16, 0x7C8F, 0x80303030, 1);
    Menu_DrawFrames(0x65F7A0, 0x70, 0x7C, 0x16, 0x7C8F, 0x80303030, 1);
    AS_List(o, 1);
    Text_Draw(0x72451E, 0x20, 0xC5, 0x64, 0x808080, 1, 0);
    as_bottom();
}

/* step 2: rounds or a larger magazine */
void DC2CALL AS_Choice(gptr o)                                       /* 0x40A7A0 */
{
    u32 p = U32(0x87DC00);
    if (p & 0x5000) { Se_Play(0, 0); U8(o + 0xC) ^= 1; }
    else if (p & 0x40) { Se_Play(0, 1); U8(o + 1) = 1; }
    else if (p & 0x20) {
        Se_Play(0, 2);
        U8(o + 1) = U8(o + 0xC) ? 6 : 3;
        U8(o + 4) = 0; U16(o + 0xD0) = 0;
        U16(o + 0xE8) = 0; U16(o + 0xEA) = 0; U16(o + 0xEC) = 0; U16(o + 0xEE) = 0;
    }
    u32 a = 0x80303030, b = 0x80303030;
    if (!U8(o + 0xC)) {
        Menu_DrawFrames(0x65FA88, 0x69, 0x5A, 0x16, 0x7C8F, Color_Pulse(0x80008080, 0x80004040, 5), 2);
        a = Color_Pulse(0x8000E0E0, 0x80008080, 5);
        Text_Draw(0x7245B8, 0x28, 0xC5, 0x64, 0x808080, 1, 0);
    } else {
        Menu_DrawFrames(0x65FA90, 0x60, 0x75, 0x16, 0x7C8F, Color_Pulse(0x80008080, 0x80004040, 5), 2);
        b = Color_Pulse(0x8000E0E0, 0x80008080, 5);
        Text_Draw(0x72460C, 0x28, 0xC5, 0x64, 0x808080, 1, 0);
    }
    Menu_DrawFrames(0x65F798, 0x7E, 0x64, 0x16, 0x7C8F, a, 1);
    Menu_DrawFrames(0x65F7A0, 0x70, 0x7C, 0x16, 0x7C8F, b, 1);
    AS_List(o, 0);
    as_bottom();
}

/* the rounds asked limited by the magazine's room and the points */
void DC2CALL AS_Clamp(gptr o)                                        /* 0x40AB50 */
{
    gptr e = AS_W(o);
    if (S16(o + 0xD0) + S16(e + 8) > S16(e + 0xA)) U16(o + 0xD0) = (u16)(U16(e + 0xA) - U16(e + 8));
    u32 price = U32(0x71D8E8 + 32 * U8(e + 1)), pts = U32(G() + 0x11F4);
    if ((u32)(S16(o + 0xD0) * (s32)price) > pts) U16(o + 0xD0) = (u16)(pts / price);
}

/* the step of a held left/right: 1, 10, 100, then 1000 each 6 frames */
static u32 hold_step(gptr cnt)
{
    s16 t = S16(cnt);
    if (t < 0x1E) return 1;
    if (t < 0x3C) return 10;
    if (t < 0x5A) return 100;
    U16(cnt) = 0x3E8;
    return 0x3E8;
}

/* step 3: how many rounds (left/right, held: faster) */
void DC2CALL AS_Count(gptr o)                                        /* 0x40A960 */
{
    u16 was = U16(o + 0xD0);
    if (U32(0x87DDFC) & 0x8000) {
        if (U32(0x87DC00) & 0x8000) {
            U16(o + 0xE8) = 0; U16(o + 0xEA) = 0; U16(o + 0xD0) = (u16)(was - 1);
        } else {
            U16(o + 0xE8)++; U16(o + 0xEA)++;
            u32 st = hold_step(o + 0xEA);
            if (S16(o + 0xE8) > 5) { U16(o + 0xE8) = 0; U16(o + 0xD0) = (u16)(was - st); }
        }
        if (S16(o + 0xD0) < 0) U16(o + 0xD0) = 0;
        if ((s32)S16(o + 0xD0) != (s32)was) Se_Play(0, 0);
    } else if (U32(0x87DDFC) & 0x2000) {
        if (U32(0x87DC00) & 0x2000) {
            U16(o + 0xEC) = 0; U16(o + 0xEE) = 0; U16(o + 0xD0) = (u16)(was + 1);
        } else {
            U16(o + 0xEC)++; U16(o + 0xEE)++;
            u32 st = hold_step(o + 0xEE);
            if (S16(o + 0xEC) > 5) { U16(o + 0xEC) = 0; U16(o + 0xD0) = (u16)(was + st); }
        }
        AS_Clamp(o);
        if ((s32)S16(o + 0xD0) != (s32)was) Se_Play(0, 0);
    } else if (U8(0x87DC00) & 0x40) { Se_Play(0, 1); U8(o + 1) = 2; }
    else if (U8(0x87DC00) & 0x20) { Se_Play(0, 2); U8(o + 1) = U16(o + 0xD0) ? 4 : 2; }
    AS_Panel(o);
    Text_Draw(0x724550, 0x20, 0xB4, 0x64, 0x808080, 1, 0);
}

/* step 4: buy them? */
void DC2CALL AS_Confirm(gptr o)                                      /* 0x40ABC0 */
{
    u32 p = U32(0x87DC00);
    if (p & 0xA000) { Se_Play(0, 0); U8(o + 4) ^= 1; }
    else if (p & 0x40) { Se_Play(0, 1); U8(o + 1) = 3; }
    else if (p & 0x20) {
        Se_Play(0, 2);
        U8(o + 1) = 2;
        if (!U8(o + 4)) { U16(o + 0xE8) = 0; U16(o + 0xEA) = 0x3C; U8(o + 1) = 5; U8(o + 2) = 0; }
    }
    AS_Panel(o);
    Text_Draw(0x7245E6, 0x20, 0xB4, 0x64, 0x808080, 1, 0);
    Menu_YesNo(0x94, 0xC4, U8(o + 4), 1);
}

/* step 5: the rounds counted into the magazine over 60 frames, the points
   paid as they go; then half a second */
void DC2CALL AS_Buying(gptr o)                                       /* 0x40AC70 */
{
    U16(o + 0xE8)++;
    Text_Draw(0x7245CC, 0x20, 0xB4, (u16)(U16(o + 0xE8) * 2), 0x808080, 1, 1);
    if (!U8(o + 2)) {
        gptr e = AS_W(o);
        s16 k = (s8)Item_Find(G() + 0x10BC, 0x14, U8(e + 1));
        s32 q = S16(o + 0xD0) / S16(o + 0xEA);
        U16(G() + 0x10C4 + 12 * (u16)k) += (u16)q;
        U16(o + 0xD0) -= (u16)q;
        U32(G() + 0x11F4) -= U32(0x71D8E8 + 32 * U8(e + 1)) * (u32)(s32)(s16)q;
        U32(G() + 0x768) = U32(G() + 0x11F4);
        U16(o + 0xEA)--;
        if (S16(o + 0xEA) <= 0) { U8(o + 2)++; U16(o + 0xEA) = 0x1E; U16(o + 0xD0) = 0; }
    } else {
        U16(o + 0xEA)--;
        if (S16(o + 0xEA) <= 0) { U8(o + 1) = 3; U8(o + 2) = 0; U16(o + 0xE8) = 0; U16(o + 0xEA) = 0; }
    }
    AS_Panel(o);
}

/* step 6: which enlargement (three) */
void DC2CALL AS_UpChoose(gptr o)                                     /* 0x40ADB0 */
{
    if (U8(o + 2)) U8(o + 2)--;
    else {
        u32 p = U32(0x87DC00);
        if (p & 0x2000) {
            U8(o + 2) = 0x10; Se_Play(0, 0);
            if ((s8)++U8(o + 0xE) > 2) U8(o + 0xE) = 0;
        } else if (p & 0x8000) {
            U8(o + 2) = 0x10; Se_Play(0, 0);
            if ((s8)--U8(o + 0xE) < 0) U8(o + 0xE) = 2;
        } else if (p & 0x40) { Se_Play(0, 1); U8(o + 1) = 2; }
        else if (p & 0x20) { Se_Play(0, 2); U8(o + 1) = 7; U8(o + 2) = 0; }
    }
    AS_UpPanel(o);
    Text_Draw(0x724586, 0x20, 0xB4, 0x64, 0x808080, 1, 0);
}

/* step 7: enlarge it? (already 9999: step 11; too few points: step 9) */
void DC2CALL AS_UpConfirm(gptr o)                                    /* 0x40AE70 */
{
    gptr e = AS_W(o);
    u32 p = U32(0x87DC00);
    if (p & 0xA000) { Se_Play(0, 0); U8(o + 4) ^= 1; }
    else if (p & 0x40) { Se_Play(0, 1); U8(o + 1) = 2; }
    else if (p & 0x20) {
        Se_Play(0, 2);
        U8(o + 1) = 2;
        if (!U8(o + 4)) {
            s16 k = (s8)Item_Find(G() + 0x10BC, 0x14, U8(e + 1));
            if (S16(G() + 0x10C6 + 12 * (u16)k) >= 0x270F) { U8(o + 1) = 0xB; U16(o + 0xEA) = 0; }
            else {
                s32 j = (s8)U8(o + 0xE);
                u32 ix = (u32)(j + 8 * (s32)U8(e + 1));
                if (U32(G() + 0x11F4) >= U32(0x71D8FC + 4 * ix)) {
                    U8(o + 1) = 8; U8(o + 2) = 0;
                    U16(o + 0xD2) = U16(0x71D8F0 + 4 * ix);
                    U32(G() + 0x11F4) -= U32(0x71D8FC + 4 * ix);
                    U16(o + 0xE8) = 0; U16(o + 0xEA) = 0x3C;
                } else { U8(o + 1) = 9; U16(o + 0xEA) = 0; }
            }
        }
    }
    AS_UpPanel(o);
    Text_Draw(0x724666, 0x20, 0xB4, 0x64, 0x808080, 1, 0);
    Menu_YesNo(0x94, 0xC4, U8(o + 4), 1);
}

/* step 8: the magazine growing over 60 frames (and filled), at most 9999 */
void DC2CALL AS_Upgrading(gptr o)                                    /* 0x40AFE0 */
{
    U16(o + 0xE8)++;
    Text_Draw(0x724642, 0x20, 0xB4, (u16)(U16(o + 0xE8) * 2), 0x808080, 1, 1);
    if (!U8(o + 2)) {
        gptr e = AS_W(o);
        s16 k = (s8)Item_Find(G() + 0x10BC, 0x14, U8(e + 1));
        s32 q = S16(o + 0xD2) / S16(o + 0xEA);
        gptr s = G() + 12 * (u16)k;
        U16(s + 0x10C6) += (u16)q;
        if (S16(s + 0x10C6) > 0x270F) U16(s + 0x10C6) = 0x270F;
        U16(s + 0x10C4) += (u16)q;
        if (S16(s + 0x10C4) > 0x270F) U16(s + 0x10C4) = 0x270F;
        U16(o + 0xD2) -= (u16)q;
        U16(o + 0xEA)--;
        if (S16(o + 0xEA) <= 0) { U8(o + 2)++; U16(o + 0xD2) = 0; U16(o + 0xEA) = 0x1E; }
    } else {
        U16(o + 0xEA)--;
        if (S16(o + 0xEA) <= 0) { U8(o + 1) = 6; U8(o + 2) = 0; U16(o + 0xE8) = 0; U16(o + 0xEA) = 0; }
    }
    AS_UpPanel(o);
}

/* step 9: not enough points */
void DC2CALL AS_UpNoPoints(gptr o)                                   /* 0x40B130 */
{
    if (U8(0x87DC00) & 0x40) { Se_Play(0, 1); U8(o + 1) = 6; }
    AS_UpPanel(o);
    Text_Draw(0x7247C0, 0x18, 0xB4, (u16)(U16(o + 0xEA) << 1), 0x8080C0, 1, 1);
    msg_up(o);
}

/* step 10: no weapon to buy for */
void DC2CALL AS_NoWeapons(gptr o)                                    /* 0x40B1A0 */
{
    if (U8(0x87DC00) & 0x40) { Se_Play(0, 1); U32(o) = 2; U16(o + 0xE8) = 0; U16(o + 0xEA) = 0; }
    Menu_DrawFrames(0x683AB8, 0x50, 0xB0, 0xD, 0x7C8E, 0x80808080, 0xF);
    Menu_DrawFrames(0x683BB8, 0xD8, 0xB0, 0xD, 0x7C8E, 0x80808080, 0xF);
    Text_Draw(0x724886, 0x18, 0xB4, U16(o + 0xEA), 0x8080C0, 1, 1);
    msg_up(o);
}

/* step 11: the magazine is already the largest */
void DC2CALL AS_UpMax(gptr o)                                        /* 0x40B260 */
{
    if (U8(0x87DC00) & 0x40) { Se_Play(0, 1); U32(o) = 2; U16(o + 0xE8) = 0; U16(o + 0xEA) = 0; }
    AS_UpPanel(o);
    Text_Draw(0x724802, 0x18, 0xB4, (u16)(U16(o + 0xEA) << 1), 0x8080C0, 1, 1);
    msg_up(o);
}

/* mode 5 */
void DC2CALL Shop_Ammo(gptr o)                                       /* 0x40A560 */
{
    Orig_Call1(U32(0x71D740 + 4 * (s32)(s8)U8(o + 1)), o);
    Menu_DrawFrames(0x65F508, 0xA0, 0x18, 0x16, 0x7C8E, 0x80C0C0C0, 0xF);
    Shop_Title(o);
}

/* ---- mode 6: the files (the dinosaurs' data) ---- */
#define FS_E(o) U32((o) + 0x50 + 4 * (s32)(s8)U8((o) + 7))

/* one file of the carousel: picture, price */
void DC2CALL FS_Item(gptr o, u32 x, u32 y, u32 k, u32 z)             /* 0x40D390 */
{
    k &= 0xFFFF;
    u32 zz = z & 0xFFFF, g = 0xC0 - (zz >> 1), col = g << 8 | g << 16 | g;
    gptr e = U32(o + 0x50 + 4 * k);
    if (k == (u32)(s32)(s8)U8(o + 7))
        Menu_DrawFrames(0x685108, x, y, 0xC, 0x7C8E, Color_Pulse(0xC0C0C0, 0x606060, 5) | 0x80000000, zz + 1);
    u32 t = U16(e + 2);
    spr((u16)(x - 0x20), (u16)(y + 0xC), 0x40, 0x20, U8(KIND(t) + 8), U8(KIND(t) + 9), 0x7D00, col, 0, z);
    mode_into(0x87BC00 + 4 * ((U32(0x87DE0C) << 10) + zz), 0x87);
    z--;
    Menu_Number11(x - 0x20, y + 0x28, 6, 0, col & 0xFFFFFF, U16(e + 6), z);
    spr((u16)(x + 0x10), (u16)(y + 0x28), 0x10, 0xA, 0x50, 0, 0x7C8F, col & 0xFF00, 0, z);
    mode_into(0x87BBFC + 4 * ((U32(0x87DE0C) << 10) + zz), 0xC);
}

/* the carousel of the files on sale */
void DC2CALL FS_Carousel(gptr o)                                     /* 0x40D1D0 */
{
    s32 n = (s8)U8(o + 0xA);
    s32 step = 0x1000 / n;
    u32 target = 0u - (u32)((s32)(s8)U8(o + 7) * (step & 0xFFF));
    u32 d = Angle_Approach(U16(o + 0xE4), (u32)(step >> 4) & 0xFF, target);
    if (((target - U16(o + 0xE4)) & 0xFFF) == 0x800 && (U32(0x87DDFC) & 0x8000)) d = 0u - d;
    U16(o + 0xE4) += (u16)d;
    gptr a = Tmp_Push(8), m = Tmp_Push(0x20), m2 = Tmp_Push(0x20), v = Tmp_Push(8), sxy = Tmp_Push(8), p = Tmp_Push(4), f = Tmp_Push(4);
    U16(a) = 0x980; U16(a + 2) = U16(o + 0xE4); U16(a + 4) = 0;
    RotMatrix(a, m);
    S32(m + 0x14) = 0; S32(m + 0x18) = -0x120; S32(m + 0x1C) = 0x340;
    U16(a + 4) = 0; U16(a + 2) = 0; U16(a) = 0;
    if (!(u16)d && n > 1) { Shop_ArrowsL(0xC8, 0x76); Shop_ArrowsR(0x78, 0x76); }
    for (u32 k = 0; (s32)k < n; k = (u8)(k + 1)) {
        RotMatrix(a, m2);
        U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = 0x1B8;
        ApplyMatrixSV(m2, v, v);
        SetRotMatrix(m);
        SetTransMatrix(m);
        s32 z = RotTransPers(v, sxy, p, f);
        U16(a + 2) = (u16)((0x1000 / n + (s32)U16(a + 2)) & 0xFFF);
        s32 sy = ftol(F32(sxy + 4)), sx = ftol(F32(sxy));
        FS_Item(o, (u32)sx, (u32)sy, k, (u32)z);
    }
    Tmp_Pop(0x60);
}

/* the file's picture, price, name and text */
void DC2CALL FS_Info(gptr o, gptr e)                                 /* 0x40D570 */
{
    (void)o;
    u32 t = U16(e + 2);
    if (U8(KIND(t) + 0xA) & 0x40) spr(0x48, 0x30, 0x40, 0x20, U8(KIND(t) + 8), U8(KIND(t) + 9), 0x7D00, 0x808080, 0, 0xA);
    else spr(0x28, 0x30, 0x40, 0x20, U8(KIND(t) + 8), U8(KIND(t) + 9), 0x7D00, 0x808080, 0, 0xA);
    mode_into(0x87BC28 + (U32(0x87DE0C) << 12), 0x87);
    Menu_Number11(0x28, 0x50, 6, 0, 0x808080, U16(e + 6), 5);
    spr(0x58, 0x50, 0x10, 0xA, 0x50, 0, 0x7C8F, 0x8000, 0, 5);
    mode_into(0x87BC14 + (U32(0x87DE0C) << 12), 0xC);
    Text_Draw(U32(KIND(t)), 0x70, 0x40, 0x64, 0x8080, 1, 0);
    Text_Draw(U32(KIND(t) + 4), 0x20, 0x60, 0x64, 0x808080, 1, 0);
}

/* step 0 */
void DC2CALL FS_Init(gptr o)                                         /* 0x40CC70 */
{
    U8(o + 1) = 1; U8(o + 7) = 0; U16(o + 0xE4) = 0; U8(o + 4) = 0;
}

/* step 1: turning the files' carousel */
void DC2CALL FS_Browse(gptr o)                                       /* 0x40CC90 */
{
    if (U8(o + 2)) U8(o + 2)--;
    else {
        u32 p = U32(0x87DC00);
        u8 was = U8(o + 7);
        if (p & 0x2000) {
            s8 i = (s8)(was + 1);
            U8(o + 2) = 0x10; U8(o + 7) = (u8)i;
            if (i > (s8)U8(o + 0xA) - 1) U8(o + 7) = 0;
        } else if (p & 0x8000) {
            s8 i = (s8)(was - 1);
            U8(o + 2) = 0x10; U8(o + 7) = (u8)i;
            if (i < 0) U8(o + 7) = (u8)(U8(o + 0xA) - 1);
        } else if (p & 0x40) { Se_Play(0, 1); U8(o + 1) = 6; }
        else if (p & 0x20) { Se_Play(0, 2); U8(o + 1) = 2; U8(o + 4) = 0; U16(o + 0xEA) = 0; }
        if ((s32)(s8)U8(o + 7) != (s32)(u16)(s16)(s8)was) Se_Play(0, 8);
    }
    FS_Carousel(o);
    Text_Draw(U32(KIND(U16(FS_E(o) + 2))), 0x20, 0xAA, 0x64, 0x8080, 1, 0);
}

/* step 2: buy it? */
void DC2CALL FS_Confirm(gptr o)                                      /* 0x40CD80 */
{
    gptr e = FS_E(o);
    FS_Info(o, e);
    u32 p = U32(0x87DC00);
    if (p & 0xA000) { Se_Play(0, 0); U8(o + 4) ^= 1; }
    else if (p & 0x40) { Se_Play(0, 1); U16(o + 0xE8) = 0; U8(o + 1) = 1; }
    else if (p & 0x20) {
        Se_Play(0, 2);
        U8(o + 1) = 1; U16(o + 0xE8) = 0;
        if (!U8(o + 4)) {
            s32 pts = (s32)U32(G() + 0x11F4), price = U16(e + 6);
            if (pts >= price) { U32(G() + 0x11F4) = (u32)(pts - price); U8(o + 1) = 3; }
            else U8(o + 1) = 4;
            U16(o + 0xE8) = 0x64; U16(o + 0xEA) = 0;
        }
    }
    Text_Draw(0x7244CA, 0x20, 0xAC, 0x64, 0x808080, 1, 0);
    Menu_YesNo(0x94, 0xBC, U8(o + 4), 1);
}

/* the costume's weapon slot upgraded by a file (slot word hi byte from..to) */
static void file_weapon(gptr slot, u32 from, u32 to)
{
    u16 w = U16(slot);
    if ((w & 0xFF00) == from) U16(slot) = (u16)((w & 0xFF) | to);
}

/* step 3: "... bought" for a second, then the file read (flag 9); the
   files 0x37 and 0x38 also change a weapon (items 0x11 -> 0x17, 0x12 ->
   0x18) and the costumes' weapon sets */
void DC2CALL FS_Bought(gptr o)                                       /* 0x40CE80 */
{
    gptr e = FS_E(o);
    FS_Info(o, e);
    gptr b = Tmp_Push(0x200);
    Msg_Concat(b, 0x7244A8, U32(KIND(U16(e + 2))));
    Msg_Concat(b, b, 0x723FCC);
    Text_Draw(b, 0x20, 0xAC, (u16)(U16(o + 0xEA) << 1), 0x808080, 1, 1);
    Tmp_Pop(0x200);
    U16(o + 0xEA)++;
    if (S16(o + 0xEA) <= 0x3C) return;
    Flag_Set(9, U16(e + 2), 1);
    if (U16(e + 2) == 0x37) {
        s32 i = (s8)Item_Find(G() + 0x10BC, 0x14, 0x11);
        U8(G() + 0x10BD + 12 * i) = 0x17;
        if (U8(G() + 0x108F) == 1) {
            if ((U16(G() + 0x10B4) & 0xFF00) == 0x100) { file_weapon(G() + 0x10B4, 0x100, 0x700); U8(G() + 0x663) = 1; }
        } else file_weapon(G() + 0x10B8, 0x100, 0x700);
    }
    if (U16(e + 2) == 0x38) {
        s32 i = (s8)Item_Find(G() + 0x10BC, 0x14, 0x12);
        U8(G() + 0x10BD + 12 * i) = 0x18;
        if (U8(G() + 0x108F) == 0) {
            if ((U16(G() + 0x10B4) & 0xFF00) == 0x200) { file_weapon(G() + 0x10B4, 0x200, 0x800); U8(G() + 0x663) = 1; }
        } else file_weapon(G() + 0x10B6, 0x200, 0x800);
    }
    u16 t = U16(e + 2);
    if (t == 0x33 || t == 0x38 || t == 0x37) U8(G() + 0x663) = 1;
    U8(o + 7) = 0; U16(o + 0xE4) = 0; U8(o + 4) = 0; U16(o + 0xE8) = 0; U16(o + 0xEA) = 0;
    U8(o + 1) = 1;
}

/* step 4: not enough points */
void DC2CALL FS_NoPoints(gptr o)                                     /* 0x40D0C0 */
{
    gptr e = FS_E(o);
    if (U8(0x87DC00) & 0x40) { Se_Play(0, 1); U8(o + 1) = 1; U16(o + 0xE8) = 0; U16(o + 0xEA) = 0; }
    FS_Info(o, e);
    Text_Draw(0x7247C0, 0x18, 0xAC, (u16)(U16(o + 0xEA) << 1), 0x8080C0, 1, 1);
    msg_up(o);
}

/* step 5: no file left to buy */
void DC2CALL FS_SoldOut(gptr o)                                      /* 0x40D150 */
{
    if (U8(0x87DC00) & 0x40) { Se_Play(0, 1); U32(o) = 2; U16(o + 0xE8) = 0; U16(o + 0xEA) = 0; }
    Text_Draw(0x72484C, 0x18, 0xAC, U16(o + 0xEA), 0x8080C0, 1, 1);
    msg_up(o);
}

/* mode 6: the files on sale not read yet listed at +0x50 (count +0xA;
   none: step 5), the step (table 0x71D774), the frame */
void DC2CALL Shop_Files(gptr o)                                      /* 0x40CB70 */
{
    U8(o + 0xA) = 0;
    U16(o + 0x102) = 0;
    for (gptr e = 0x71DC28; U16(e); e += 8) {
        if (!Flag_Check(6, U16(e + 4)) || Flag_Check(9, U16(e + 2))) continue;
        U32(o + 0x50 + 4 * (s32)(s8)U8(o + 0xA)) = e;
        U8(o + 0xA)++;
    }
    if (!U8(o + 0xA)) U8(o + 1) = 5;
    Orig_Call1(U32(0x71D774 + 4 * (s32)(s8)U8(o + 1)), o);
    Menu_DrawFrames(0x65FC68, 0xA0, 0x18, 6, 0x7C8E, 0x80C0C0C0, 0xF);
    Menu_Points(0x10, 0x98, 0x808080);
    Menu_DrawFrames(0x683AB8, 0x50, 0xA8, 0xD, 0x7C8E, 0x80808080, 0xF);
    Menu_DrawFrames(0x683BB8, 0xD8, 0xA8, 0xD, 0x7C8E, 0x80808080, 0xF);
    Shop_Title(o);
}

/* mode 3: the weapons shop: the weapons on sale that the player does not
   have and his costume can use listed at +0x10 (count +9; none: step 5),
   then its step (table 0x71D6FC) and the frame */
void DC2CALL Shop_Weapons(gptr o)                                    /* 0x408C50 */
{
    U8(o + 9) = 0;
    U16(o + 0x100) = 0;
    for (gptr e = 0x71D790; U16(e); e += 8) {
        if (!Flag_Check(6, U16(e + 4))) continue;
        if ((u8)Item_Find(G() + 0x10BC, 0x14, U8(e + 2)) != 0xFF) continue;
        if (!(U16(0x70426A + 12 * U16(e + 2)) & (1u << (U8(G() + 0x108F) & 31)))) continue;
        U32(o + 0x10 + 4 * (s32)(s8)U8(o + 9)) = e;
        U8(o + 9)++;
    }
    if (!U8(o + 9)) U8(o + 1) = 5;
    Orig_Call1(U32(0x71D6FC + 4 * (s32)(s8)U8(o + 1)), o);
    Menu_DrawFrames(0x65F510, 0xA0, 0x18, 0x16, 0x7C8E, 0x80C0C0C0, 0xF);
    Menu_Points(0x10, 0x98, 0x808080);
    Menu_DrawFrames(0x683AB8, 0x50, 0xA8, 0xD, 0x7C8E, 0x80808080, 0xF);
    Menu_DrawFrames(0x683BB8, 0xD8, 0xA8, 0xD, 0x7C8E, 0x80808080, 0xF);
    Shop_Title(o);
}

/* mode 7: back to the game (the stage reloaded when the weapons changed) */
void DC2CALL Shop_Leave(gptr o)                                      /* 0x40DF80 */
{
    if (U8(G() + 0x663)) {
        U16(G() + 0x674) = 0;
        u16 v = U16(G() + 0x10B4);
        Stage_Load(v & 0xFF, v >> 8);
    }
    U8(G() + 0x663) = 0;
    Flag_Set(2, 4, (u32)(s32)(s8)U8(G() + 0x14F0));
    Se_Play(0, 7);
    Snd_Unduck();
    U32(G() + 0x768) = U32(o + 0x104);
    Task_Chain(0x451660);
}

/* the task: the state cleared, the sound ducked and the prices set when
   coming from the game, then each frame the background and the mode */
void DC2CALL Shop_Task(void)                                         /* 0x40D710 */
{
    Flag_Set(2, 0x10, 0);
    Flag_Set(3, 0, 0);
    for (u32 k = 0; k < 0x10C; k += 4) U32(SH + k) = 0;
    Flag_Set(2, 0x11, 0);
    Flag_Set(2, 4, 1);
    Lights_Unmark();
    U32(SH + 0x104) = U32(G() + 0x768);
    U32(G() + 0x6CC) = U32(G() + 0x11F4);
    Shake_Off();
    SetGeomScreen(0xC0);
    if (!U8(G() + 0xB48)) {
        Snd_Duck();
        Se_Play(0, 6);
        Shop_Prices(SH);
    }
    U8(G() + 0xB48) = 1;
    for (;;) {
        U32(0x87DE00) = 0;
        Card_Back();
        Card_Ring1();
        Card_Ring2();
        Orig_Call1(U32(0x71E130 + 4 * (s32)(s8)U8(SH)), SH);
        u32 b = U32(0x87DE0C) << 12;
        Gpu_DrawOT(U32(0x87F94C) + 0x78, b + 0x87CBFC, b + 0x87BC00);
        Task_Sleep(1);
    }
}
