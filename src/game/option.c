/* The options screen (Game_Start mode 0: OPTION.DAT, task 0x475720), from
 * the title or the in-game menu.
 * Its state at 0x87FDC0 (0x1B8 bytes):
 *   +0 mode (0 set-up, 1 the list, 2 a page, 3 leaving), +1 the item (0
 *   controller, 1 sound, 2 display, 3 exit game, 4 back), +2 the exit
 *   page's choice, +3 a page's line animation step, +4/+0x68 and
 *   +0xCC/+0x130 two flickering lines (50 x, 50 y each), +0x194 the key
 *   page's frame blink, +0x196 a page's row, +0x198 the sound page's row,
 *   +0x19A/+0x19C the list sliding (speed, offset), +0x19E..+0x1A8 a page's
 *   lines and text growing, +0x1AC the main mode on entry, +0x1B0 the help
 *   text's letters, +0x1B2 the key page's button (0 ok, 1 cancel, 2
 *   default), +0x1B4 the key page shown, +0x1B6 the pad.
 * The key page: 9 pad bits (0x737944) and their names (0x737920), the keys'
 * names as glyphs at 0x74E194 (0x74E32C pointers, 0x74E294 x), name tags
 * of sprite pieces at 0x74DB94 (0x74E2CC, 0x74E300), the keys on entry
 * 0x74E2F0. The display page edits a copy of the display set-up
 * (0x723CA0..: device, window, can-window, size, depth, name) at 0x74E2BC.. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/pcio.h"
#include "dc2/pcinput.h"
#include "dc2/task.h"

#define OPT 0x87FDC0u

static s32 ftol(long double x) { return (s32)(int64_t)x; }

/* a drawing mode packet into the menu's order table */
static void opt_mode(u32 tpage)
{
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, tpage, 0);
    Gpu_AddPrim(0x87BCC8 + (U32(0x87DE0C) << 12), U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}

/* a page's lines and text grow: phase 0 the first line, 1 the second, 2 the text */
static void grow(gptr o, s16 l1, s16 l2, u32 step)
{
    switch ((s8)U8(o + 3)) {
    case 0: U16(o + 0x1A2) += (u16)step; if (S16(o + 0x1A2) > l1) { U16(o + 0x1A2) = (u16)l1; U8(o + 3)++; } break;
    case 1: U16(o + 0x1A4) += (u16)step; if (S16(o + 0x1A4) > l2) { U16(o + 0x1A4) = (u16)l2; U8(o + 3)++; } break;
    case 2: U16(o + 0x1A6)++; if (S16(o + 0x1A6) > 0xC8) { U16(o + 0x1A6) = 0xC8; U8(o + 3)++; } break;
    }
}

static void anim_reset(gptr o)
{
    for (u32 k = 0x19E; k <= 0x1A8; k += 2) U16(o + k) = 0;
}

/* the display set-up copied for the page */
static void display_load(void)
{
    U32(0x74E2FC) = U32(0x723CA0); U32(0x74E2C8) = U32(0x723CA4);
    U32(0x74E2C4) = U32(0x723CB0); U32(0x74E2BC) = U32(0x723CB4);
    U32(0x74E328) = U32(0x723CB8);
    U32(0x74E2C0) = U32(0x87FF9C); U32(0x74E324) = U32(0x87FFA0);
}

/* the width of glyph text (ended by 0xFxxx) */
s32 DC2CALL Opt_TextWidth(gptr s)                                    /* 0x475AC0 */
{
    long double w = F32(0x7033B4);
    for (u16 c; ((c = U16(s)) & 0xF000) != 0xF000; s += 2)
        w += (long double)F32(0x723D4C + 4 * (c & 0xFFF)) - F32(0x703498);
    return ftol(w);
}

/* the width of ASCII text: its widest line */
s32 DC2CALL Text_Width(gptr s)                                       /* 0x471E10 */
{
    long double cur = F32(0x7033B4);
    float mx = 0.0f;
    for (u8 c; (c = U8(s)) != 0; s++) {
        if (c == '\n') { if ((long double)mx < cur) mx = (float)cur; cur = F32(0x7033B4); }
        else if (c == ' ') cur += F32(0x703468);
        else cur += (long double)F32(0x737370 + 4 * c) - D64(0x703460);
    }
    if (mx == F32(0x7033B4)) return ftol(cur);
    if ((long double)mx < cur) mx = (float)cur;
    return ftol(mx);
}

/* the key page's names: each key's name as glyphs, centred on a tag of
   sprite pieces (left column even rows, right odd) and a filler to the
   label */
void DC2CALL Opt_KeyNames(void)                                      /* 0x475840 */
{
    static const u8 u[4] = {0x40, 0x5E, 0xB7, 0x46};
    gptr r = 0x74DB94, g = 0x74E194;
    for (u32 k = 0; k < 9; k++) {
        u32 odd = k & 1;
        s32 w = Opt_TextWidth(U32(0x737920 + 4 * k));
        U32(0x74E32C + 4 * k) = g;
        U16(0x74E2A8 + 2 * k) = (u16)(0xA0 - (w >> 1));
        U32(0x74E2CC + 4 * k) = r;
        U8(r) = u[odd]; U8(r + 1) = 0x60; U8(r + 2) = 3; U8(r + 3) = 0x12; U8(r + 4) = u[odd + 2]; U8(r + 5) = 0xFF; U8(r + 7) = 2;
        r += 8;
        gptr name = PcIn_KeyName(PcIn_KeyOf(U16(0x737944 + 2 * k)));
        for (u32 i = 0; U8(name + i); i++) { U16(g) = U8(name + i); g += 2; }
        U16(g) = 0xF000; g += 2;
        s32 w2 = Opt_TextWidth(U32(0x74E32C + 4 * k));
        s32 n = w2 / 12 + (w2 % 12 ? 1 : 0);
        U16(0x74E294 + 2 * k) = (u16)(odd ? 0x135 - w2 / 2 - 6 * n : 6 * n - w2 / 2 + 9);
        u32 cnt = 2;
        if (n > 0) {
            cnt = (u32)n + 2;
            for (s32 i = 0; i < n; i++, r += 8) {
                U8(r) = 0x46; U8(r + 1) = 0x60; U8(r + 2) = 0xC; U8(r + 3) = 0x12; U8(r + 5) = 0xFF; U8(r + 7) = 2;
                U8(r + 4) = odd ? (u8)(U8(r - 4) - U8(r + 2)) : (u8)(U8(r - 6) + U8(r - 4));
            }
        }
        U8(r) = u[odd ^ 1]; U8(r + 1) = 0x60; U8(r + 2) = 3; U8(r + 3) = 0x12; U8(r + 5) = 0xFF; U8(r + 7) = 2;
        U8(r + 4) = odd ? (u8)(U8(r - 4) - U8(r + 2)) : (u8)(U8(r - 6) + U8(r - 4));
        U8(U32(0x74E2CC + 4 * k) + 6) = (u8)cnt;
        r += 8;
        s32 left = (s32)U16(0x74E2A8 + 2 * k) - (s32)cnt * 12 - 6;
        U32(0x74E300 + 4 * k) = r;
        u32 m = 0;
        if (left > 0) {
            m = (u32)(left + 7) >> 3;
            for (u32 i = 0; i < m; i++, r += 8) {
                U8(r) = 0xF8; U8(r + 1) = 0x90; U8(r + 2) = 8; U8(r + 3) = 0x10; U8(r + 5) = 0; U8(r + 7) = 0;
                U8(r + 4) = odd ? (u8)(U8(r - 4) - U8(r + 2)) : (u8)(U8(r - 6) + U8(r - 4));
            }
        }
        U8(U32(0x74E300 + 4 * k) + 6) = (u8)m;
    }
}

/* mode 0: the keys kept, the names made, the display set-up copied */
void DC2CALL Opt_Init(gptr o)                                        /* 0x4757B0 */
{
    U32(o) = 1;
    for (u32 k = 0; k < 9; k++) U8(0x74E2F0 + k) = (u8)PcIn_KeyOf(U16(0x737944 + 2 * k));
    Opt_KeyNames();
    display_load();
}

/* two flickering lines of 49 segments (a: x, b: y; they move on when
   G+4 bit 0 is set) */
void DC2CALL Opt_Lines(gptr o, gptr a, gptr b)                       /* 0x478510 */
{
    if (U8(G() + 4) & 1) {
        switch (Rand8() & 3) {
        case 0: U16(a + 0x62) -= (u16)((Rand8() & 0xFF) % 0x78); break;
        case 1: U16(a + 0x62) += (u16)((Rand8() & 0xFF) % 0x78); break;
        case 2: U16(b + 0x62) -= (u16)((Rand8() & 0xFF) % 0x60); break;
        case 3: U16(b + 0x62) += (u16)((Rand8() & 0xFF) % 0x60); break;
        }
        if (S16(a + 0x62) > 0xF4) U16(a + 0x62) = 0xF4;
        if (S16(a + 0x62) < 0) U16(a + 0x62) = 0;
        if (S16(b + 0x62) > 0x90) U16(b + 0x62) = 0x90;
        if (S16(b + 0x62) < 0) U16(b + 0x62) = 0;
    }
    for (u32 i = 1; i <= 0x31; i++) {
        gptr p = U32(0x87DC98);
        F32(p + 8) = (float)(S16(a + 2 * i) + 0x30);
        F32(p + 0xC) = (float)(S16(b + 2 * i) + 0x30);
        F32(p + 0x18) = (float)(S16(a + 2 * i - 2) + 0x30);
        F32(p + 0x1C) = (float)(S16(b + 2 * i - 2) + 0x30);
        U32(p + 0x20) = 0x3C23D70A; U32(p + 0x10) = 0x3C23D70A;
        U32(p + 4) = (((((i & ~3u) | 0x14800) << 7) | (i & ~1u)) << 7) | (i >> 2);
        u32 q = (i - 1) >> 2;
        U32(p + 0x14) = ((((i - 1) >> 1) | q << 8) << 8) | q;
        gptr ot = 0x87CA10 + (U32(0x87DE0C) << 12);
        U32(p) = U32(ot) | 0x04000000;
        U32(ot) = p & 0xFFFFFF;
        U32(0x87DC98) = p + 0x24;
        if (U8(G() + 4) & 1) { U16(a + 2 * i - 2) = U16(a + 2 * i); U16(b + 2 * i - 2) = U16(b + 2 * i); }
    }
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, U8(o) == 2 ? 5 : 0x25, 0);
    Gpu_AddPrim(0x87CA10 + (U32(0x87DE0C) << 12), U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}

/* the list: background, title, the five items sliding, the help line */
void DC2CALL Opt_Draw(gptr o)                                        /* 0x478150 */
{
    static const u32 help[5] = {0x7271A6, 0x7271D4, 0x727250, 0x727282, 0x7272A0};
    static const u32 fa[5] = {0x65F510, 0x65F518, 0x65F508, 0x65F520, 0x65F528};
    static const u32 fb[5] = {0x65FBF0, 0x65FC50, 0x65FC28, 0x65FC70, 0x65FC88};
    static const u32 fc[5] = {0x65FC98, 0x65FCE0, 0x65FD20, 0x65FD60, 0x65FDA0};
    Opt_Lines(o, o + 4, o + 0x68);
    Opt_Lines(o, o + 0xCC, o + 0x130);
    gptr r = Tmp_Push(8);
    U16(r) = 0x20; U16(r + 2) = 0; U16(r + 4) = 0x100; U16(r + 6) = 0xF0;
    Menu_Sprite(r, 0, 0, 0x7C00, Color_Pulse(0x606060, 0x808080, 6), 1, 0x3FF);
    Tmp_Pop(8);
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 5, 0);
    Gpu_AddPrim(0x87CBFC + (U32(0x87DE0C) << 12), U32(0x87DC98));
    U32(0x87DC98) += 0xC;
    Menu_DrawFrames(0x65FA80, 0, 0x10, 0x26, 0x7C00, 0x80808080, 0x64);
    Menu_DrawFrames(0x65FB00, 0xE0, 0x10, 0x26, 0x7C00, 0x80808080, 0x64);
    Menu_DrawFrames(0x65F500, 0x11, 0x11, 6, 0x7C01, 0x606060, 2);
    Menu_SmallText(0x737A2C, 0x84, 0x1D, 0x64, 0x3A7219, 1);
    s32 dl = 0, dr = 0;
    for (u32 i = 0, y = 0x2C; y < 0xA4; i++, y += 0x18, dl -= 3, dr += 3) {
        if ((s32)i == (s8)U8(o + 1)) {
            u32 col = U8(o) == 2 ? 0x606060 : Color_Pulse(0x606060, 0x303030, 4);
            s32 q = S16(o + 0x19C) / 6;
            Menu_DrawFrames(fa[i], (u32)(dl - q + 0xCC), y, 6, 0x7C01, col, 0xA);
            Menu_DrawFrames(fb[i], (u32)(dl - q + 0xC5), y - 3, 6, 0x7C00, col, 0x32);
            Menu_DrawFrames(fc[i], 0xD0, y, 0x26, 0x7C02, col | 0x80000000, 0x32);
            if (U16(o + 0x1B0) < 0x1F4) U16(o + 0x1B0)++;
            if (U8(o) == 1) Text_Draw(help[i], 0x30, 0xB4, U16(o + 0x1B0), 0x3380, 1, 1);
        } else {
            s32 s = S16(o + 0x19C);
            Menu_DrawFrames(fa[i], (u32)(2 * s + 0xCC - dr), y, 6, 0x7C01, 0x80303030, 0x320);
            Menu_DrawFrames(fb[i], (u32)(2 * s + 0xC5 - dr), y - 3, 6, 0x7C00, 0x80303030, 0x352);
            Menu_DrawFrames(fc[i], (u32)(2 * s + 0xD0), y, 6, 0x7C02, 0x80303030, 0x352);
        }
    }
}

/* mode 1: the list (up/down an item, 0x20 into its page, 0x40 back) */
void DC2CALL Opt_List(gptr o)                                        /* 0x475B10 */
{
    if (!U16(o + 0x19C)) {
        u32 p = U32(0x87DC00);
        if (p & 0x1000) {
            Se_Play(0, 0); U16(o + 0x1B0) = 0;
            if ((s8)--U8(o + 1) < 0) U8(o + 1) = 4;
        } else if (p & 0x4000) {
            Se_Play(0, 0); U16(o + 0x1B0) = 0;
            if ((s8)++U8(o + 1) > 4) U8(o + 1) = 0;
        } else if (p & 0x40) {
            Se_Play(0, 1); U8(o) = 3;
        } else if (p & 0x20) {
            Se_Play(0, 2);
            U8(o) = 2; U8(o + 2) = 0; U8(o + 3) = 0;
            anim_reset(o);
            U16(o + 0x196) = 0; U16(o + 0x198) = 0;
        }
    }
    U16(o + 0x19A) += 2;
    U16(o + 0x19C) -= U16(o + 0x19A);
    if (S16(o + 0x19C) < 0) { U16(o + 0x19A) = 0; U16(o + 0x19C) = 0; }
    Opt_Draw(o);
}

/* mode 2: the item's page over the list slid out */
void DC2CALL Opt_Page(gptr o)                                        /* 0x475C20 */
{
    Orig_Call1(U32(0x737974 + 4 * (s32)(s8)U8(o + 1)), o);
    U16(o + 0x19A) += 2;
    U16(o + 0x19C) += U16(o + 0x19A);
    if (S16(o + 0x19C) > 0x50) { U16(o + 0x19A) = 0; U16(o + 0x19C) = 0x50; }
    Opt_Draw(o);
    U8(G() + 0xB58) = 0xA0; U8(G() + 0xB59) = 0xA0; U8(G() + 0xB5A) = 0xA0;
    U8(G() + 0xB5B) = 0; U8(G() + 0xB5C) = 0;
    DrawSync(0);
}

/* the controller page's pad labels */
static void pad_label(u32 frames, u32 text, u32 x, u32 y, u32 tx, u32 ty, u32 col, u32 z)
{
    Menu_DrawFrames(frames, x & 0xFFFF, y & 0xFFFF, 6, 0x7C00, col, z);
    Text_Draw(text, x + tx, y + ty, 0x64, col, z, 2);
}
void DC2CALL Opt_PadFire(gptr o, u32 x, u32 y, u32 col)  { (void)o; pad_label(0x6600D8, 0x727430, x, y, 0xA, 2, col, 0x28); }  /* 0x4771F0 */
void DC2CALL Opt_PadAim(gptr o, u32 x, u32 y, u32 col)   { (void)o; pad_label(0x660198, 0x727552, x, y, 8, 1, col, 0x28); }  /* 0x477250 */
void DC2CALL Opt_PadMap(gptr o, u32 x, u32 y, u32 col)   { (void)o; pad_label(0x65F770, 0x72745A, x, y, 8, 1, col, 0x1F4); } /* 0x477380 */
void DC2CALL Opt_PadSub(gptr o, u32 x, u32 y, u32 col)                /* 0x4772B0 */
{
    (void)o;
    if (U8(G() + 0x14F2)) {
        if (U8(G() + 0x14EF) == 1) y += 0xFFF0;
        pad_label(0x65FF60, 0x72749E, x, y, 8, 2, col, 0x28);
    } else pad_label(0x65FEA0, 0x727470, x, y, 8, 2, col, 0x28);
}

/* the joystick's buttons for layout k (G+0x14EF) */
static void joy_binds(void)
{
    s32 k = (s8)U8(G() + 0x14EF);
    PcIn_BindButton(0x20, U8(0x737958 + 4 * k));
    PcIn_BindButton(0x40, U8(0x737959 + 4 * k));
    PcIn_BindButton(0x10, U8(0x73795A + 4 * k));
    PcIn_BindButton(2, U8(0x73795B + 4 * k));
}

/* page 0: the controller (layout, control type, auto aim, the key page) */
void DC2CALL Opt_Controller(gptr o)                                  /* 0x475CC0 */
{
    static const u32 frm[4] = {0x65F5A0, 0x65F5A8, 0x65F5B0, 0x65F5B8};
    static const u16 fx[4] = {0x94, 0x84, 0x94, 0xA4}, fy[4] = {0x78, 0x68, 0x58, 0x68};
    static const u32 txt[4] = {0x7273D2, 0x7273E6, 0x7273FA, 0x72741A};
    if (U16(o + 0x1B4)) { Opt_Keys(o); return; }
    u32 p = U32(0x87DC00);
    if (p & 0x1000) {
        Se_Play(0, 0);
        U16(o + 0x196)--;
        anim_reset(o);
        if (S16(o + 0x196) < 0) U16(o + 0x196) = 3;
        U8(o + 3) = 0;
    } else if (p & 0x4000) {
        Se_Play(0, 0);
        U16(o + 0x196)++;
        anim_reset(o);
        if (S16(o + 0x196) > 3) U16(o + 0x196) = 0;
        U8(o + 3) = 0;
    } else if ((p & 0x40) || ((p & 0x20) && U16(o + 0x196) != 3)) {
        Se_Play(0, p & 0x40 ? 1 : 2);
        U8(o) = 1; U8(o + 1) = 0; U8(o + 2) = 0; U8(o + 3) = 0;
    } else switch (S16(o + 0x196)) {
    case 0:
        if (p & 0xA000) {
            Se_Play(0, 0);
            if (U32(0x87DC00) & 0x2000) { U8(G() + 0x14EF)++; if ((s8)U8(G() + 0x14EF) > 2) U8(G() + 0x14EF) = 0; }
            else { U8(G() + 0x14EF)--; if ((s8)U8(G() + 0x14EF) < 0) U8(G() + 0x14EF) = 2; }
            joy_binds();
        }
        break;
    case 1:
        if (p & 0xA000) {
            Se_Play(0, 0);
            U8(G() + 0x14F2) ^= 1;
            Flag_Set(2, 0x1C, (u32)(s32)(s8)U8(G() + 0x14F2));
            U32(0x737924) = U8(G() + 0x14F2) ? 0x727514 : 0x7274FE;
            Opt_KeyNames();
        }
        break;
    case 2:
        if (p & 0xA000) { Se_Play(0, 0); U8(G() + 0x14F1) ^= 1; }
        break;
    case 3:
        if (p & 0x20) { Se_Play(0, 0); U16(o + 0x1B4) = 1; }
        break;
    }
    U16(o + 0x19E)++;
    U16(o + 0x1A0) += U16(o + 0x19E);
    if (S16(o + 0x1A0) > 0x3C) { U16(o + 0x19E) = 0; U16(o + 0x1A0) = 0x3C; }
    grow(o, 0x30, 0x10, 4);

    u32 col = U16(o + 0x196) ? 0x303030 : 0x808080;
    for (u32 k = 0; k < 4; k++) Menu_DrawFrames(frm[k], fx[k], fy[k], 6, 0x7C01, col, 2);
    /* (the labels by layout; any other layout: the original's are stack garbage) */
    static const u16 lx[3][4] = {{8, 0xC0, 0xC0, 0x18}, {0xC0, 0xC0, 8, 0x18}, {8, 8, 0xC0, 0xC0}};
    static const u16 ly[3][4] = {{0x40, 0x40, 0x88, 0x90}, {0x40, 0x88, 0x88, 0x40}, {0x90, 0x40, 0x88, 0x40}};
    s32 lay = (s8)U8(G() + 0x14EF);
    if (lay >= 0 && lay <= 2) {
        Opt_PadAim(o, lx[lay][0], ly[lay][0], col);
        Opt_PadSub(o, lx[lay][1], ly[lay][1], col);
        Opt_PadFire(o, lx[lay][2], ly[lay][2], col);
        Opt_PadMap(o, lx[lay][3], ly[lay][3], col);
    }
    Menu_DrawFrames(0x65F838, 0x70, 0x40, 6, 0x7C03, U16(o + 0x196) ? 0x80404040 : 0x80808080, 2);

    u32 hi = 0x404040, lo = 0x808080, third = 0x404040;   /* (the page's choices: on/off colours) */
    for (u32 i = 0; i < 4; i++) {
        s32 a = S16(o + 0x1A0);
        if ((s32)i == S16(o + 0x196)) {
            switch (i) {
            case 0:
                switch ((s8)U8(G() + 0x14EF)) {
                case 0: hi = 0x303030; lo = 0x808080; third = 0x303030; break;
                case 1: lo = 0x303030; hi = 0x808080; third = 0x303030; break;
                case 2: lo = 0x303030; third = 0x808080; hi = 0x303030; break;
                }
                Menu_DrawArea(0, 0, 0x140, 0xF0, 5);
                Menu_DrawFrames(0x65F578, (u32)(a + 0x38), 0x32, 6, 0x7C01, lo, 5);
                Menu_DrawFrames(0x65F588, (u32)(a + 0x40), 0x32, 6, 0x7C01, 0x303030, 5);
                Menu_DrawFrames(0x65FBD0, (u32)(a + 0x48), 0x32, 6, 0x7C01, hi, 5);
                Menu_DrawFrames(0x65F588, (u32)(a + 0x50), 0x32, 6, 0x7C01, 0x303030, 5);
                Menu_DrawFrames(0x65FBD8, (u32)(a + 0x58), 0x32, 6, 0x7C01, third, 5);
                Menu_DrawFrames(0x65F728, (u32)(a + 0x1A), 0x2E, 6, 0x7C00, 0x808080, 5);
                Menu_DrawFrames(0x65F720, (u32)(a + 0x5C), 0x2E, 6, 0x7C00, 0x808080, 5);
                Menu_DrawArea(0x54, 0x2C, 0xB0, 0x50, 5);
                break;
            case 1: case 2: {
                u32 y = i == 1 ? 0xB2 : 0xC2, ly2 = i == 1 ? 0xC8 : 0xB6;
                int on = i == 1 ? U8(G() + 0x14F2) != 0 : U8(G() + 0x14F1) == 0;
                lo = on ? 0x808080 : 0x303030; hi = on ? 0x303030 : 0x808080;
                Menu_DrawArea(0, 0, 0x140, 0xF0, 5);
                Menu_DrawFrames(0x65F580, (u32)(a + 0xA0), y, 6, 0x7C01, lo, 5);
                Menu_DrawFrames(0x65F588, (u32)(a + 0xB0), y, 6, 0x7C01, 0x303030, 5);
                Menu_DrawFrames(0x65F590, (u32)(a + 0xB8), y, 6, 0x7C01, hi, 5);
                Menu_DrawFrames(0x65F728, (u32)(a + 0x8C), y - 4, 6, 0x7C00, 0x808080, 5);
                Menu_DrawFrames(0x65F720, (u32)(a + 0xCC), y - 4, 6, 0x7C00, 0x808080, 5);
                Menu_DrawArea(0xCC, 0xA0, 0xB0, 0x50, 5);
                u16 l1 = U16(o + 0x1A2), l2 = U16(o + 0x1A4);
                if (l1) Menu_Line(0x20, ly2, (u32)l1 + 0x20, ly2, 0xFF, 1, 0x32);
                if (l2) Menu_Line(0x50, ly2, (u32)l2 + 0x50, i == 1 ? 0xC8 - (u32)l2 : (u32)l2 + 0xB6, 0xFF, 1, 0x32);
                opt_mode(0xC);
                if (U16(o + 0x1A6))
                    Menu_SmallText(i == 1 ? 0x737A00 : 0x7379F4, 0x24, i == 1 ? 0xC0 : 0xB2, U16(o + 0x1A6), 0xA0A0A0, 1);
                break;
            }
            case 3:
                Menu_Line(0x20, 0xC4, (u16)(U16(o + 0x1A2) + 0x20), 0xC4, 0xFF, 1, 0x32);
                Menu_Line(0x50, 0xC4, (u32)U16(o + 0x1A4) + 0x50, (u32)U16(o + 0x1A4) + 0xC4, 0xFF, 1, 0x32);
                opt_mode(0xC);
                Menu_SmallText(0x7379E4, 0x24, 0xBC, U16(o + 0x1A6), 0xA0A0A0, 1);
                break;
            }
            lo = Color_Pulse(0xC0C0, 0x6060, 4);
        } else lo = 0x303030;
        if (!i) {
            Text_Draw(txt[0], 0x20, 0x2F, 0x64, lo, 2, 2);
            Menu_DrawFrames(0x660028, 0xE, 0x2E, 6, 0x7C00, 0x808080, 5);
            if (U16(o + 0x196)) Menu_DrawFrames(0x65F720, 0x5C, 0x2E, 6, 0x7C00, 0x808080, 5);
        } else {
            Text_Draw(txt[i], 0x70, (i << 4) + 0x9F, 0x64, lo, 2, 2);
            Menu_DrawFrames(0x660070, 0x5E, (i << 4) + 0x9E, 6, 0x7C00, 0x808080, 5);
            if ((s32)i != S16(o + 0x196) || i == 3) Menu_DrawFrames(0x65F720, 0xCC, (i << 4) + 0x9E, 6, 0x7C00, 0x808080, 5);
        }
    }
    if (U16(o + 0x1B4)) {
        for (u32 k = 0x19E; k <= 0x1A6; k += 2) U16(o + k) = 0;
        U8(o + 3) = 0;
        U16(o + 0x196) = 0; U16(o + 0x1B2) = 0;
    }
}

/* the key page: 9 keys (a key pressed binds the row's bit, swapping with a
   key already bound) and ok / cancel / default */
void DC2CALL Opt_Keys(gptr o)                                        /* 0x476A60 */
{
    if (!U16(o + 0x1B4)) { Opt_Controller(o); return; }
    u32 p = U32(0x87DC00);
    if (p & 0x1000) {
        Se_Play(0, 0);
        U16(o + 0x196)--;
        anim_reset(o);
        if (S16(o + 0x196) < 0) U16(o + 0x196) = 9;
        U8(o + 3) = 0;
    } else if (p & 0x4000) {
        Se_Play(0, 0);
        U16(o + 0x196)++;
        anim_reset(o);
        if (S16(o + 0x196) > 9) U16(o + 0x196) = 0;
        U8(o + 3) = 0;
    } else if (U16(o + 0x196) != 9) {
        u32 key = Input_ReadKey();
        if (key) {
            Se_Play(0, 0);
            u32 old = PcIn_KeyOf(U16(0x737944 + 2 * S16(o + 0x196)));
            for (s32 k = 0; k < 9; k++) {
                if (k == S16(o + 0x196)) continue;
                if (PcIn_KeyOf(U16(0x737944 + 2 * k)) == key) { PcIn_BindKey(U16(0x737944 + 2 * k), old); break; }
            }
            PcIn_BindKey(U16(0x737944 + 2 * S16(o + 0x196)), key);
            Opt_KeyNames();
        }
    } else if (p & 0x60) {
        if (p & 0x40) U16(o + 0x1B2) = 1;
        switch (S16(o + 0x1B2)) {
        case 0: Se_Play(0, 2); U16(o + 0x1B4) = 0; break;
        case 1:
            Se_Play(0, 1);
            for (u32 k = 0; k < 9; k++) PcIn_BindKey(U16(0x737944 + 2 * k), U8(0x74E2F0 + k));
            U16(o + 0x1B4) = 0;
            break;
        case 2: Se_Play(0, 2); PcIn_DefaultKeys(); break;
        }
        Opt_KeyNames();
    } else if (p & 0x2000) {
        Se_Play(0, 0);
        U16(o + 0x1B2)++;
        if (S16(o + 0x1B2) > 2) U16(o + 0x1B2) = 0;
    } else if (p & 0x8000) {
        Se_Play(0, 0);
        U16(o + 0x1B2)--;
        if (S16(o + 0x1B2) < 0) U16(o + 0x1B2) = 2;
    }
    U16(o + 0x19E)++;
    U16(o + 0x1A0) += U16(o + 0x19E);
    if (S16(o + 0x1A0) > 0x50) { U16(o + 0x19E) = 0; U16(o + 0x1A0) = 0x50; }
    grow(o, 0x30, 0x10, 4);

    for (u32 i = 0; i < 10; i++) {
        u32 col = 0x303030;
        if ((s32)i == S16(o + 0x196)) {
            if (i == 9) {
                U16(o + 0x194)++;
                if (S16(o + 0x194) > 0x46) U16(o + 0x194) = 0;
                gptr r = Tmp_Push(8);
                U16(r) = 0x88; U16(r + 2) = 0xD9; U16(r + 4) = 0x3C; U16(r + 6) = 0xE;
                Menu_Border(r, U16(o + 0x194), 0x23, 2, 0xFFFF);
                U16(r) = 0x88; U16(r + 2) = 0xD9; U16(r + 4) = 0x3C; U16(r + 6) = 0xE;
                Menu_Border(r, (u16)(U16(o + 0x194) + 0x23), 0x23, 2, 0xFFFF);
                Tmp_Pop(8);
                u32 c[3];
                for (u32 j = 0; j < 3; j++) c[j] = (s32)j == S16(o + 0x1B2) ? 0x808080 : 0x303030;
                s32 a = S16(o + 0x1A0);
                Menu_DrawFrames(0x65FBC0, (u32)(a + 0x80), 0xDC, 6, 0x7C01, c[0], 5);
                Menu_DrawFrames(0x65F588, (u32)(a + 0x94), 0xDC, 6, 0x7C01, 0x303030, 5);
                Menu_DrawFrames(0x65FBC8, (u32)(a + 0x9B), 0xDC, 6, 0x7C01, c[1], 5);
                Menu_DrawFrames(0x65F588, (u32)(a + 0xAA), 0xDC, 6, 0x7C01, 0x303030, 5);
                Menu_DrawFrames(0x65F768, (u32)(a + 0xB1), 0xDC, 6, 0x7C01, c[2], 5);
                Menu_DrawFrames(0x65F728, (u32)(a + 0x84), 0xD8, 6, 0x7C00, 0x808080, 5);
                Menu_DrawFrames(0x65F720, (u32)(a + 0xC4), 0xD8, 6, 0x7C00, 0x808080, 5);
                Menu_Line(0x38, 0xD2, (u16)(U16(o + 0x1A2) + 0x38), 0xD2, 0xFF, 1, 0x32);
                Menu_Line(0x68, 0xD2, (u32)U16(o + 0x1A4) + 0x68, (u32)U16(o + 0x1A4) + 0xD2, 0xFF, 1, 0x32);
                opt_mode(0xC);
                Menu_SmallText(0x737A18, 0x3C, 0xCE, U16(o + 0x1A6), 0xA0A0A0, 1);
            }
            col = 0x808080;
        }
        if (i == 9) {
            Text_Draw(0x72741A, 0x88, 0xD9, 0x64, col, 2, 2);
            Menu_DrawFrames(0x660028, 0x76, 0xD8, 6, 0x7C00, 0x808080, 5);
            if (U16(o + 0x196) != 9) Menu_DrawFrames(0x65F720, 0xC4, 0xD8, 6, 0x7C00, 0x808080, 5);
        } else {
            u32 y = (i + 4) << 4, x = i & 1 ? 0xF0 : 0x50;
            Text_Draw(U32(0x737920 + 4 * i), U16(0x74E2A8 + 2 * i), y, 0x64, col, 2, 2);
            Text_Draw(U32(0x74E32C + 4 * i), U16(0x74E294 + 2 * i), y + 1, 0x64, col, 2, 2);
            Menu_DrawFrames(U32(0x74E2CC + 4 * i), x, y, 6, 0x7C01, col, 5);
            Menu_DrawFrames(U32(0x74E300 + 4 * i), x, y, 6, 0x7C03, col, 5);
        }
    }
    if (!U16(o + 0x1B4)) {
        for (u32 k = 0x19E; k <= 0x1A6; k += 2) U16(o + k) = 0;
        U8(o + 3) = 0;
        U16(o + 0x196) = 3;
    }
}

/* a sound volume (+0x14EE/+0x14ED) stepped by the pad, 0..15 */
static void volume(u32 at, u32 p)
{
    u8 c = U8(G() + at);
    u16 was = (u16)(s16)(s8)c;
    if (p & 0x2000) { if ((s8)c < 0xF) U8(G() + at) = (u8)(c + 1); }
    else if (p & 0x8000) { if ((s8)c > 0) U8(G() + at) = (u8)(c - 1); }
    if ((u32)was != (u32)(s32)(s8)U8(G() + at)) Se_Play(0, 0);
}

/* page 1: sound (stereo / mono, the volumes) */
void DC2CALL Opt_Sound(gptr o)                                       /* 0x4773E0 */
{
    u32 p = U32(0x87DC00);
    if (p & 0x1000) {
        U16(o + 0x198)--;
        if (S16(o + 0x198) < 0) U16(o + 0x198) = 2;
        Se_Play(0, 0);
    } else if (p & 0x4000) {
        U16(o + 0x198)++;
        if (S16(o + 0x198) > 2) U16(o + 0x198) = 0;
        Se_Play(0, 0);
    } else if (p & 0x60) {
        Se_Play(0, p & 0x40 ? 1 : 2);
        U8(o) = 1; U8(o + 1) = 1; U8(o + 2) = 0;
        U8(o + 3) = 0;
    } else switch (S16(o + 0x198)) {
    case 0:
        if (p & 0xA000) { Se_Play(0, 0); U8(G() + 0x14EC) ^= 1; U8(o + 3) = 0; }
        break;
    case 1: volume(0x14EE, p); break;
    case 2: volume(0x14ED, p); break;
    }
    u32 a = 0x202020, b = 0x202020;
    if (!U16(o + 0x198)) {
        if (U8(G() + 0x14EC)) a = Color_Pulse(0xC0C0, 0x4040, 4); else b = Color_Pulse(0xC0C0, 0x4040, 4);
    } else if (U8(G() + 0x14EC)) a = 0x505050; else b = 0x505050;
    Text_Draw(0x727206, 0x32, 0x64, 0xC8, a, 1, 0);
    Text_Draw(0x727214, 0x96, 0x64, 0xC8, b, 1, 0);
    u32 c = U16(o + 0x198) == 1 ? Color_Pulse(0xC0C0, 0x4040, 4) : 0x202020;
    Text_Draw(0x727226, 0x32, 0x8C, 0xC8, c, 1, 0);
    Menu_Number12(0xA0, 0x8C, 2, 0, c, (u32)(s32)(s8)U8(G() + 0x14EE), 1);
    c = U16(o + 0x198) == 2 ? Color_Pulse(0xC0C0, 0x4040, 4) : 0x202020;
    Text_Draw(0x72723A, 0x32, 0xB4, 0xC8, c, 1, 0);
    Menu_Number12(0xA0, 0xB4, 2, 0, c, (u32)(s32)(s8)U8(G() + 0x14ED), 1);
}

static void display_step(s32 dir)
{
    PcIn_DisplayStep(U32(0x74E2FC), U32(0x74E2C8), 0x74E2C0, 0x74E324, 0x74E2C4, 0x74E2BC, 0x74E328, dir);
}

/* page 2: the display (device, size, window); 0x40 drops the changes,
   0x820 applies them */
void DC2CALL Opt_Display(gptr o)                                     /* 0x4776D0 */
{
    u32 p = U32(0x87DC00);
    if (p & 0x5000) {
        Se_Play(0, 0);
        anim_reset(o);
        U8(o + 3) = 0;
        if (U32(0x87DC00) & 0x4000) { U16(o + 0x196)++; if (S16(o + 0x196) > 2) U16(o + 0x196) = 0; }
        else if (U32(0x87DC00) & 0x1000) { U16(o + 0x196)--; if (S16(o + 0x196) < 0) U16(o + 0x196) = 2; }
    } else if (p & 0xA000) {
        Se_Play(0, 0);
        switch (S16(o + 0x196)) {
        case 2:
            if (U32(0x74E2C0)) { U32(0x74E2C8) ^= 1; display_step(0); }
            break;
        case 1: display_step(U16(0x87DC00) & 0x2000 ? 1 : -1); break;
        case 0:
            if (U32(0x74D738) == 2) {
                if (++U32(0x74E2FC) > 2) U32(0x74E2FC) = 1;
                display_step(0);
                if (!U32(0x74E2C0) && !U32(0x74E2C8)) U32(0x74E2C8) = 1;
            }
            break;
        }
    } else if (p & 0x40) {
        Se_Play(0, 1);
        U8(o) = 1; U8(o + 1) = 2; U8(o + 2) = 0; U8(o + 3) = 0;
        display_load();
    } else if (p & 0x820) {
        Se_Play(0, 2);
        U8(o) = 1; U8(o + 1) = 2; U8(o + 2) = 0; U8(o + 3) = 0;
        if (U32(0x723CA0) != U32(0x74E2FC) || U32(0x723CA4) != U32(0x74E2C8) || U32(0x723CB0) != U32(0x74E2C4)
            || U32(0x723CB4) != U32(0x74E2BC) || U32(0x723CB8) != U32(0x74E328)) {
            U32(0x723CA0) = U32(0x74E2FC); U32(0x723CB8) = U32(0x74E328); U32(0x723CA4) = U32(0x74E2C8);
            U32(0x723CB0) = U32(0x74E2C4); U32(0x723CB4) = U32(0x74E2BC);
            U32(0x87FF9C) = U32(0x74E2C0); U32(0x87FFA0) = U32(0x74E324);
            Display_Apply();
        }
    }
    s32 sel = S16(o + 0x196);
    u32 c[3];
    for (s32 j = 0; j < 3; j++) c[j] = j == sel ? 0x808080 : 0x303030;
    grow(o, 0x36, 0xC, 4);
    u32 ey = (u32)(sel * 45 + 0x4E);
    u16 l1 = U16(o + 0x1A2), l2 = U16(o + 0x1A4);
    if (l1) Menu_Line(0x10, ey, (u32)l1 + 0x10, ey, 0xFF, 1, 0x32);
    if (l2) Menu_Line(0x46, ey, (u32)l2 + 0x46, (u32)l2 + ey, 0xFF, 1, 0x32);
    opt_mode(0xC);
    if (U16(o + 0x1A6)) Menu_SmallText(U32(0x737990 + 4 * S16(o + 0x196)), 0x12, ey - 4, U16(o + 0x1A6), 0xA0A0A0, 1);
    Menu_DrawFrames(0x65F820, 0x26, 0x5E, 6, 0x7C01, c[0], 2);
    Menu_DrawFrames(0x65F7C0, 0x16, 0x5B, 6, 0x7C03, c[0], 0x32);
    Menu_DrawFrames(0x65FA50, 0x8A, 0x63, 6, 0x7C03, c[0], 0x32);
    Menu_DrawFrames(0x65F828, 0x2E, 0x8B, 6, 0x7C01, c[1], 2);
    Menu_DrawFrames(0x65F7C0, 0x16, 0x88, 6, 0x7C03, c[1], 0x32);
    Menu_DrawFrames(0x65FA18, 0x8A, 0x90, 6, 0x7C03, c[1], 0x32);
    Menu_DrawFrames(0x65F830, 0x2C, 0xB8, 6, 0x7C01, c[2], 2);
    Menu_DrawFrames(0x65F7C0, 0x16, 0xB5, 6, 0x7C03, c[2], 0x32);
    Menu_DrawFrames(0x65FA18, 0x8A, 0xBD, 6, 0x7C03, c[2], 0x32);
    Menu_DrawFrames(0x65F8F8, 0x16, (u32)(S16(o + 0x196) * 45 + 0x5B), 6, 0x7C03, 0x80C0C0C0, 0x32);
    Text_AsciiZ(U32(0x74E324), (u32)(0xA0 - Text_Width(U32(0x74E324)) / 2), 0x3C, c[0], 2);
    gptr buf = Tmp_Push(0x20);
    Crt_sprintf3(buf, 0x737A20, U32(0x74E2C4), U32(0x74E2BC), U32(0x74E328));
    Text_AsciiZ(buf, 0xC8, 0x8B, c[1], 2);
    Tmp_Pop(0x20);
    Text_AsciiZ(U32(0x737988 + 4 * U32(0x74E2C8)), 0xC8, 0xB9, c[2], 2);
}

/* page 3: exit the game? (no / yes; yes ends the program) */
void DC2CALL Opt_Quit(gptr o)                                        /* 0x477CF0 */
{
    u32 p = U32(0x87DC00);
    if (p & 0xF000) {
        Se_Play(0, 0);
        U8(o + 2) ^= 1; U8(o + 3) = 0;
        anim_reset(o);
    } else if (p & 0x40) {
        Se_Play(0, 1);
        U8(o) = 1; U8(o + 1) = 3; U8(o + 2) = 0; U8(o + 3) = 0;
    } else if (p & 0x20) {
        Se_Play(0, 2);
        if (U8(o + 2)) { Task_Sleep(0xA); Flag_Set(2, 5, 1); return; }
        U8(o) = 1; U8(o + 1) = 3; U8(o + 2) = 0; U8(o + 3) = 0;
    }
    u32 a, b;
    U16(o + 0x19E) += 2;
    u16 d = U16(o + 0x19E);
    if (U8(o + 2)) {
        switch ((s8)U8(o + 3)) {
        case 0: U16(o + 0x1A2) += d; if (S16(o + 0x1A2) > 0x2C) { U16(o + 0x1A2) = 0x2C; U8(o + 3)++; } break;
        case 1: U16(o + 0x1A4) += d; if (S16(o + 0x1A4) > 0x64) { U16(o + 0x1A4) = 0x64; U8(o + 3)++; } break;
        }
        if (U16(o + 0x1A2)) Menu_Line(0x20, 0x3C, 0x20, (u32)U16(o + 0x1A2) + 0x3C, 0x67FF, 1, 0x32);
        if (U16(o + 0x1A4)) Menu_Line(0x1C, 0x62, (u32)U16(o + 0x1A4) + 0x1C, 0x62, 0x67FF, 1, 0x32);
        opt_mode(0xC);
        a = 0x808080; b = 0x303030;
    } else {
        switch ((s8)U8(o + 3)) {
        case 0: U16(o + 0x1A2) += d; if (S16(o + 0x1A2) > 0x54) { U16(o + 0x1A2) = 0x54; U8(o + 3)++; } break;
        case 1: U16(o + 0x1A4) += d; if (S16(o + 0x1A4) > 0x64) { U16(o + 0x1A4) = 0x64; U8(o + 3)++; } break;
        }
        if (U16(o + 0x1A2)) Menu_Line(0x20, 0x3C, 0x20, (u32)U16(o + 0x1A2) + 0x3C, 0x67FF, 1, 0x32);
        if (U16(o + 0x1A4)) Menu_Line(0x1C, 0x88, (u32)U16(o + 0x1A4) + 0x1C, 0x88, 0x67FF, 1, 0x32);
        opt_mode(0xC);
        a = 0x303030; b = 0x808080;
    }
    Menu_DrawFrames(0x65F9F8, 0xC0, 0x4C, 6, 0x7C03, 0x80808080, 0x32);
    Text_Draw(0x7272B8, 0x22, 0x3A, 0x64, 0x3380, 1, 0);
    Menu_Line(0x14, 0x4C, 0xE8, 0x4C, 0x67FF, 1, 0x32);
    opt_mode(0xC);
    Menu_DrawFrames(0x65FBC0, 0x54, 0x5E, 6, 0x7C01, a, 2);
    Menu_DrawFrames(0x65FC70, 0x4A, 0x5A, 6, 0x7C02, a | 0x80000000, 0x32);
    Menu_DrawFrames(0x65FBC8, 0x58, 0x84, 6, 0x7C01, b, 2);
    Menu_DrawFrames(0x65FC70, 0x4A, 0x80, 6, 0x7C02, b | 0x80000000, 0x32);
}

/* mode 3 (and page 4): leaving, back into the game (its sound settings
   kept as flags) or to the title */
void DC2CALL Opt_Leave(gptr o)                                       /* 0x478090 */
{
    Flag_Set(2, 9, 0);
    U32(0x87DE00) = U32(o + 0x1AC);
    if (U8(G() + 0xB48)) {
        Flag_Set(2, 0x22, 0);
        Flag_Set(2, 0xE, (u32)(s32)(s8)U8(G() + 0x14F1));
        Flag_Set(2, 4, (u32)(s32)(s8)U8(G() + 0x14F0));
        Snd_Unduck();
        Task_Chain(0x478E20);
        return;
    }
    U16(0x87DC08) = 0; U16(0x87DC94) = 0; U16(0x87DDFC) = 0; U16(0x87DC00) = 0;
    if (U8(G() + 0x108D)) Game_Start(0x16);
    else Task_Chain(0x4A5730);
}

/* back into the game from the options: the stage loaded again (the first
   time through), its camera, then the game task */
void DC2CALL Opt_ToGame(void)                                        /* 0x478E20 */
{
    if (!U8(0x74E378)) { U8(0x74E378) = 1; Game_Start(0); Frame_Draw(); return; }
    gptr g = G();
    if (U8(g + 0xB48) && U16(g + 0x1090) != 0x905) {
        U8(g + 0xB49) = 1;
        u8 c = U8(g + 0x7C4);
        U8(g + 0x7C4) = U8(g + 0x7C6);
        U8(G() + 0x84B) = 0xFF;
        Camera_Switch(c);
        Camera_Load();
        Fade_Set(1, 1, 1);
        Task_Sleep(1);
        U8(G() + 0xB48) = 0;
        g = G();
    }
    if (U8(g + 0x1091) == 7) {
        U8(G() + 0xB58) = 0x70; U8(G() + 0xB59) = 0x70; U8(G() + 0xB5A) = 0x70;
        U8(G() + 0xB5B) = 0; U8(G() + 0xB5C) = 0;
    }
    Flag_Set(2, 0x10, 0);
    Fade_Set(0, 0xF, 1);
    U16(0x87DC08) = 0; U16(0x87DC94) = 0; U16(0x87DDFC) = 0; U16(0x87DC00) = 0;
    U8(0x74E378) = 0;
    Task_Chain(0x4510F0);
    Frame_Draw();
}

/* ---- the pause screen (G flag 2:0x10, set by pad 0x800 in Game_Play) ------------------
 * 0x74E378 the stage reloaded on the way back (cleared: Opt_ToGame loads the
 * options first), 0x74E379 choice (0 options, 1 back to the game; only 1 in
 * rooms 1 and 0x80A), 0x74E37B frames since the last key (keys after 2),
 * 0x737A58 the backdrop's slide (0..2). */
#define P_STAGE   0x74E378u
#define P_CHOICE  0x74E379u
#define P_FRAMES  0x74E37Bu
#define P_SLIDE   0x737A58u

/* a sprite r {x, y, w, h} from u, v (code 0x64, semi 0x66) in the pause OT slot */
void DC2CALL Pause_Sprite(gptr r, u32 u, u32 v, u32 tp, u32 color, u32 semi) /* 0x4795E0 */
{
    gptr p = U32(0x87DC98);
    F32(p + 8) = (float)S16(r);
    F32(p + 0xC) = (float)S16(r + 2);
    U32(p + 0x10) = 0x3C23D70A;                             /* z 0.01 */
    U16(p + 0x18) = U16(r + 4); U16(p + 0x1A) = U16(r + 6);
    U8(p + 0x14) = (u8)u; U8(p + 0x15) = (u8)v; U16(p + 0x16) = (u16)tp;
    U32(p + 4) = color | ((u16)semi ? 0x66000000 : 0x64000000);
    gptr ot = U32(0x87F94C) + 0x74;
    U32(p) = U32(ot) | 0x04000000;
    U32(ot) = p & 0xFFFFFF;
    U32(0x87DC98) += 0x1C;
}

/* sprites from a frame f (count at f+6) of 8-byte parts {u, v, w, h, s8 dx,
 * s8 dy, ...} at x, y (w taken as it is: no 0 -> 256); color bit 31 blends;
 * then a draw mode packet E1 with tpage (dfe on) */
void DC2CALL Pause_Frames(gptr f, u32 x, u32 y, u32 tpage, u32 tp, u32 color) /* 0x479500 */
{
    u32 n = U8(f + 6);
    if (n) {
        gptr r = Tmp_Push(8);
        u32 semi = color >> 31, c = color & 0xFFFFFF;
        for (; n; n--, f += 8) {
            U16(r) = (u16)((s8)U8(f + 4) + x); U16(r + 2) = (u16)((s8)U8(f + 5) + y);
            U16(r + 4) = U8(f + 2); U16(r + 6) = U8(f + 3);
            Pause_Sprite(r, U8(f), U8(f + 1), tp, c, semi);
        }
        Tmp_Pop(8);
    }
    gptr p = U32(0x87DC98);
    U32(p + 4) = tpage | 0xE1000400;
    U32(p + 8) = 0;
    gptr ot = U32(0x87F94C) + 0x74;
    U32(p) = U32(ot) | 0x02000000;
    U32(ot) = p & 0xFFFFFF;
    U32(0x87DC98) += 0xC;
}

/* the backdrop: the frame's picture in four 160x120 quarters (port code
 * 0x8E), drawn a little smaller each of the first frames (P_SLIDE) */
void DC2CALL Pause_Back(void)                                        /* 0x479240 */
{
    s16 t = S16(P_SLIDE);
    for (u32 k = 0; k < 4; k++) {
        gptr p = U32(0x87DC98);
        u32 right = k & 1, low = k >> 1;
        F32(p + 8) = right ? 160.0f : (float)t;
        F32(p + 0xC) = low ? 120.0f : (float)t;
        U16(p + 0x14) = (u16)(0xA0 - U16(P_SLIDE));
        U16(p + 0x16) = (u16)(0x78 - U16(P_SLIDE));
        U16(p + 0x10) = right ? 0xA0 : 0;
        U16(p + 0x18) = 0xA0;
        U8(p + 0x12) = low ? 0x78 : 0;
        U16(p + 0x1A) = 0x78;
        U8(p + 0x13) = 0;
        U32(p + 4) = 0x8E808080;
        gptr ot = U32(0x87F94C) + 0x78;
        U32(p) = U32(ot) | 0x02000000;
        U32(ot) = p & 0xFFFFFF;
        U32(0x87DC98) += 0x1C;
    }
}

static int pause_one_choice(void)
{
    u16 room = U16(G() + 0x1090);
    return room == 0x80A || room == 1;
}

static void pause_resume(void)
{
    Flag_Set(2, 4, (u32)(s32)(s8)U8(G() + 0x14F0));
    Snd_Unduck();
    Flag_Set(2, 0x10, 0);
    Flag_Set(2, 0x22, 0);
    U16(0x87DC08) = 0; U16(0x87DC94) = 0; U16(0x87DDFC) = 0; U16(0x87DC00) = 0;
    Task_Chain(0x4510F0);
}

/* the task: the game paused under its last picture; up/down choose, 0x20
 * takes the choice (the options through Opt_ToGame, or back), 0x840 goes
 * back to the game */
void DC2CALL Pause_Task(void)                                        /* 0x478F40 */
{
    Flag_Set(2, 0x22, 1);
    Flag_Set(2, 1, 0);
    Flag_Set(2, 0xD, 0);
    Flag_Set(2, 4, 1);
    Lights_Unmark();
    U8(P_STAGE) = 0; U8(P_CHOICE) = 0; U8(P_FRAMES) = 0; U8(0x74E37A) = 0;
    U16(P_SLIDE) = 0;
    if (pause_one_choice()) U8(P_CHOICE) = 1;
    Snd_Duck();
    Shake_Off();
    for (;;) {
        if (!pause_one_choice()) {
            u32 a = 0xC0C0, b = 0xC0C0;
            if (!U8(P_CHOICE)) b = 0x303030; else a = 0x303030;
            Pause_Frames(0x685D50, 0x6A, 0x50, 9, 0x7DCB, 0x1537D1);
            Pause_Frames(0x685D58, 0x78, 0x6C, 9, 0x7DCB, a);
            Pause_Frames(0x685D88, 0x8A, 0x7A, 9, 0x7DCB, b);
        } else {
            Pause_Frames(0x685D50, 0x6A, 0x70, 9, 0x7DCB, 0x1537D1);
        }
        Pause_Back();
        Frame_Draw();
        s8 fr = (s8)(U8(P_FRAMES) + 1);
        if (fr > 10) fr = 0xB;
        U8(P_FRAMES) = (u8)fr;
        s16 sl = (s16)(U16(P_SLIDE) + 1);
        if (sl > 2) sl = 2;
        U16(P_SLIDE) = (u16)sl;
        if (fr > 2) {
            u32 pad = U32(0x87DC00);
            if (pad & 0x5000) {
                if (!pause_one_choice()) {
                    U8(P_FRAMES) = 0;
                    Se_Play(0, 0);
                    U8(P_CHOICE) ^= 1;
                } else {
                    U8(P_CHOICE) = 1;
                }
            } else if (pad & 0x840) {
                Flag_Set(2, 4, (u32)(s32)(s8)U8(G() + 0x14F0));
                Se_Play(0, 1);
                Snd_Unduck();
                Flag_Set(2, 0x10, 0);
                Flag_Set(2, 0x22, 0);
                U16(0x87DC08) = 0; U16(0x87DC94) = 0; U16(0x87DDFC) = 0; U16(0x87DC00) = 0;
                Task_Chain(0x4510F0);
            } else if (U8(0x87DC00) & 0x20) {
                Se_Play(0, 2);
                if (U8(P_CHOICE)) {
                    pause_resume();
                } else {
                    U8(G() + 0xB48) = 1;
                    U8(P_STAGE) = 0;
                    Task_Chain(0x478E20);
                }
            }
        }
        Task_Sleep(1);
    }
}

/* the task: the state cleared, the main mode 2, then each frame its mode
   and the menu's order table */
void DC2CALL Option_Task(void)                                       /* 0x475720 */
{
    Flag_Set(3, 0, 0);
    for (u32 k = 0; k < 0x1B8; k += 4) U32(OPT + k) = 0;
    U16(OPT + 0x1B4) = 0;
    U32(OPT + 0x1AC) = U32(0x87DE00);
    Shake_Off();
    U32(0x87DE00) = 2;
    for (;;) {
        U16(OPT + 0x1B6) = U16(0x87DC16);
        Orig_Call1(U32(0x737964 + 4 * (s32)(s8)U8(OPT)), OPT);
        u32 b = U32(0x87DE0C) << 12;
        Gpu_DrawOT(U32(0x87F94C) + 0x78, b + 0x87CBFC, b + 0x87BC00);
        Task_Sleep(1);
    }
}
