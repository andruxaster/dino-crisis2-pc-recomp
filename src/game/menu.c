/* The in-game menu (task 0x491950): the inventory, maps, files, the item box
 * and the other screens of pause. State M = G+0x68C: +0 mode (table
 * 0x739C28), +1 screen (mode 2 runs entry 4 + screen of that table), +2 step,
 * +8..+0x17 key repeat counters, +0x44 the keys of this frame, +0x5D items
 * held, +0x60 the area to start in, +0x64 how the menu was opened (Menu_Open:
 * 0 pause, 1 an item, 3 a room's door, 4 a game over, 5 flag 2:0x20),
 * +0x65 frames before the menu button works, +0x8C/+0x8D the screen flash,
 * +0x8E the frames of the opening. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/pcio.h"
#include "dc2/gte.h"
#include "dc2/task.h"

#define M_MODES   0x739C28u     /* modes; screens from entry 4 */
#define M_SCREENS 0x739C38u

/* key i pressed this frame, or held 20 frames (then every 10) */
u32 DC2CALL Menu_Pad(gptr m)                                        /* 0x494630 */
{
    u32 keys = 0;
    for (u32 i = 0; i < 16; i++) {
        gptr c = m + 8 + i;
        u32 bit = 1u << i;
        if (U16(0x87DDFC) & bit) U8(c)++; else U8(c) = 0;
        if (U8(c) >= 20) { U8(c) = 10; keys |= bit & U16(0x87DDFC); }
        else keys |= bit & U16(0x87DC00);
    }
    return keys;
}

/* a tile r {x, y, w, h} in OT slot z; semi: its blend mode + 1 (0 opaque) */
void DC2CALL Menu_Tile(gptr r, u32 color, u32 semi, u32 z)          /* 0x494730 */
{
    gptr p = U32(0x87DC98);
    F32(p + 8) = (float)S16(r); F32(p + 0xC) = (float)S16(r + 2);
    U32(p + 0x10) = 0x3C23D70A;                             /* z 0.01 */
    U16(p + 0x14) = U16(r + 4); U16(p + 0x16) = U16(r + 6);
    U32(p + 4) = color | ((u16)semi ? 0x62000000 : 0x60000000);
    gptr ot = 0x87BC00 + 4 * ((U32(0x87DE0C) << 10) + (u16)z);
    U32(p) = U32(ot) | 0x03000000;
    U32(ot) = p & 0xFFFFFF;
    p += 0x18;
    U32(0x87DC98) = p;
    if ((u16)semi) {
        Gpu_SetDrawMode(p, 1, 0, (((u32)(u16)semi << 5) - 0x20) | 0xC, 0);
        Gpu_AddPrim(0x87BC00 + 4 * ((U32(0x87DE0C) << 10) + (u16)z), U32(0x87DC98));
        U32(0x87DC98) += 0xC;
    }
}

/* line y (h lines) of the screen moved dx pixels across (dx < 0: from x 0) */
void DC2CALL Menu_ShiftLine(u32 dx, u32 y, u32 h)                   /* 0x4956C0 */
{
    if (!(u16)dx) return;
    u16 r[4];
    s16 ry;
    s32 to;
    if ((s16)dx > 0) {
        r[2] = (u16)(0x140 - dx);
        to = (s16)dx;
    } else {
        r[2] = (u16)(dx + 0x140);
        to = 0;
    }
    ry = (s16)(U32(0x87DE0C) ? y + 0x100 : y);
    r[0] = 0; r[1] = (u16)ry; r[3] = (u16)h;
    gptr p = U32(0x87DC98);
    U32(p + 4) = 0xEC000000;
    memcpy(MEM(p + 8), r, 8);
    U32(p + 0x10) = (u32)to; U32(p + 0x14) = (u32)(s32)ry;
    Gpu_AddPrim(U32(0x87F94C) + 0x74, U32(0x87DC98));
    U32(0x87DC98) += 0x18;
}

/* items in use among n records of 12 bytes */
u32 DC2CALL Item_CountUsed(gptr p, u32 n)                           /* 0x495CE0 */
{
    u8 c = 0;
    for (u32 k = (u8)n; k; k--, p += 12) if (U8(p)) c++;
    return c;
}

/* the white flash and torn lines when the menu opens */
void DC2CALL Menu_Flash(gptr m)                                     /* 0x491D10 */
{
    switch ((s8)U8(m + 0x8C)) {
    case 1: U8(m + 0x8D) = 10; U8(m + 0x8C) = 2; break;
    case 2: break;
    default: return;
    }
    u8 v = (u8)(U8(m + 0x8D) * 10);
    gptr r = Tmp_Push(8);
    U16(r) = 0; U16(r + 2) = 0; U16(r + 4) = 0x140; U16(r + 6) = 0xF0;
    Menu_Tile(r, (u32)v << 16 | (u32)v << 8 | v, 1, 2);
    Tmp_Pop(8);
    for (u8 i = 0; (s32)i < (s8)U8(m + 0x8D) * 20; i++) {
        u32 y = Rand8() % 0xEF;
        u32 dx = (u32)(s32)(Rand8() % 0xF) - 8;
        Menu_ShiftLine(dx, y, 1);
    }
    if ((s8)--U8(m + 0x8D) <= 0) U8(m + 0x8C) = 0;
}

/* mode 0: the menu opened */
void DC2CALL MenuMode_Init(gptr m)                                  /* 0x491B30 */
{
    U8(m + 0x4D) = 0; U8(m + 0x4F) = 0; U8(m + 0x4E) = 0; U8(m + 0x58) = 0;
    U8(m + 0x61) = 0; U8(m + 0x5E) = 0; U8(m + 0x5F) = 0;
    U32(m + 8) = 0; U32(m + 0xC) = 0; U32(m + 0x10) = 0; U32(m + 0x14) = 0;
    U32(m) = 1;
    if (Flag_Check(2, 0x20)) { U8(m + 0x64) = 5; return; }
    switch (U8(m + 0x64)) {
    case 1:                                 /* an item: its picture */
        while (File_IsBusy()) Task_Sleep(1);
        if (U16(m + 0x38) != 0x1F) Orig_Call1(0x403EE0, (m & 0xFFFF0000) | U16(m + 0x38));
        break;
    case 4:
        Game_Start(4);
        U8(G() + 0xB48) = 4;
        break;
    case 3:
        Pause_LoadScreen();
        Game_Start((u32)(s32)(s8)U8(m + 0x60));
        U8(G() + 0xB48) = U8(m + 0x60);
        break;
    }
}

/* mode 1: the menu opening (10 frames), then its first screen */
void DC2CALL MenuMode_Enter(gptr m)                                 /* 0x491C00 */
{
    switch ((s8)U8(m + 1)) {
    case 0:
        U16(m + 0x8E) = 10; U8(m + 0x8C) = 1; U8(m + 1) = 1;
        /* fall through */
    case 1:
        if (--U16(m + 0x8E)) break;
        U8(m) = 2;
        switch (U8(m + 0x64)) {
        case 0: U8(m + 1) = 0; break;
        case 1: U8(m + 1) = 6; break;
        case 3: U8(m + 1) = 4; break;
        case 4: U8(m + 1) = 7; break;
        case 5:
            U8(m + 1) = 2;
            U8(m + 0x5D) = (u8)Item_CountUsed(G() + 0x11AC, 6);
            if (!U8(m + 0x5D)) U8(m + 2) = 5;
            break;
        }
        break;
    }
    Menu_Flash(m);
    if (U8(m + 0x64)) Menu_Backdrop(m); else Menu_PauseScreen(m);
    gptr g = G();
    U8(g + 0xB58) = 0xA0; U8(g + 0xB59) = 0xA0; U8(g + 0xB5A) = 0xA0;
    U8(g + 0xB5B) = 0; U8(g + 0xB5C) = 0;
    DrawSync(0);
}

/* mode 2: the screen +1 */
void DC2CALL MenuMode_Screen(gptr m)                                /* 0x491E20 */
{
    if (U8(m + 1) != 5) Menu_Backdrop(m);
    Menu_Flash(m);
    Orig_Call1(U32(M_SCREENS + (s8)U8(m + 1) * 4), m);
}

/* the menu task (Game_Play chains it when flag 2:8 is set) */
void DC2CALL Menu_Task(void)                                        /* 0x491950 */
{
    gptr m = G() + 0x68C;
    Flag_Set(2, 0x10, 0);
    Flag_Set(3, 0, 0);
    U32(m) = 0;
    U32(m + 0x6C) = U32(0x87DE00);
    Flag_Set(2, 1, 0);
    Flag_Set(2, 0xD, 0);
    Flag_Set(2, 4, 1);
    Lights_Unmark();
    U32(m + 0x40) = U32(G() + 0x11F4);
    U8(m + 0xCE) = 0; U8(m + 0xCF) = 0;
    U16(m + 0x46) = U16(G() + 0x10B4);
    U16(m + 0x4A) = U16(U32(G() + 0x940) + 0x108);
    Shake_Off();
    SetGeomScreen(0xC0);
    Snd_Duck();
    Se_Play(0, 6);
    U8(m + 0x65) = 0x1E;
    if (Flag_Check(U8(G() + 0x1222), U8(G() + 0x1223))) {
        U8(G() + 0x1222) = 0; U8(G() + 0x1223) = 0; U8(G() + 0x1220) = 0; U8(G() + 0x1221) = 0;
    }
    Task_Sleep(1);
    for (;;) {
        U32(0x87DE00) = 0;
        s8 w = (s8)U8(m + 0x65);
        if (w > 0) U8(m + 0x65) = (u8)(w - 1);
        else if (w == 0 && (U32(0x87DC00) & 0x100) && !File_IsBusy())
            U32(m) = U8(G() + 0x10B4) ? 0x502 : 0x90102;           /* the menu button: to the inventory */
        u32 k = Menu_Pad(m);
        U16(m + 0x44) = (u16)((k & 0xF000) | (U16(0x87DC00) & 0xFFF));
        Orig_Call1(U32(M_MODES + (s8)U8(m) * 4), m);
        u32 o = U32(0x87DE0C) << 12;
        Gpu_DrawOT(U32(0x87F94C) + 0x78, 0x87CBFC + o, 0x87BC00 + o);
        Task_Sleep(1);
    }
}

/* ---- primitives (the port's format: float xy, z 0.01) ---- */

/* p's place in OT slot z (low 16 bits), n words long */
static void ot_link(gptr p, u32 n, u32 z)
{
    gptr ot = 0x87BC00 + 4 * ((U32(0x87DE0C) << 10) + (u16)z);
    U32(p) = U32(ot) | n << 24;
    U32(ot) = p & 0xFFFFFF;
}
static void xy(gptr p, gptr v) { F32(p) = (float)S16(v); F32(p + 4) = (float)S16(v + 2); }
/* a draw mode packet (dfe on) after the prims, in slot z (all 32 bits) */
static void draw_mode(u32 tpage, u32 z)
{
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, tpage, 0);
    Gpu_AddPrim(0x87BC00 + 4 * ((U32(0x87DE0C) << 10) + z), U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}

/* a sprite r {x, y, w, h} from u, v (code 0x64; semi: 0x66) */
void DC2CALL Menu_Sprite(gptr r, u32 u, u32 v, u32 tp, u32 color, u32 semi, u32 z) /* 0x495040 */
{
    gptr p = U32(0x87DC98);
    xy(p + 8, r);
    U32(p + 0x10) = 0x3C23D70A;
    U16(p + 0x18) = U16(r + 4); U16(p + 0x1A) = U16(r + 6);
    U8(p + 0x14) = (u8)u; U8(p + 0x15) = (u8)v; U16(p + 0x16) = (u16)tp;
    U32(p + 4) = color | ((u16)semi ? 0x66000000 : 0x64000000);
    ot_link(p, 4, z);
    U32(0x87DC98) += 0x1C;
}

/* the same with the port's own code 0x80 (semi: 0x82), its tpage fixed */
void DC2CALL Menu_Sprite80(gptr r, u32 u, u32 v, u32 tp, u32 color, u32 semi, u32 z) /* 0x495130 */
{
    gptr p = U32(0x87DC98);
    (void)tp;
    xy(p + 8, r);
    U32(p + 0x10) = 0x3C23D70A;
    U16(p + 0x18) = U16(r + 4); U16(p + 0x1A) = U16(r + 6);
    U8(p + 0x14) = (u8)u; U8(p + 0x15) = (u8)v; U16(p + 0x16) = 0x7DCE;
    U32(p + 4) = color | ((u16)semi ? 0x82000000 : 0x80000000);
    ot_link(p, 4, z);
    U32(0x87DC98) += 0x1C;
}

/* sprites from a frame f (count at f+6) of 8-byte parts {u, v, w (0: 256),
 * h, s8 dx, s8 dy, n, flags (2: code 0x80)} at x, y; color bit 31 blends;
 * then the draw mode tpage */
void DC2CALL Menu_DrawFrames(gptr f, u32 x, u32 y, u32 tpage, u32 tp, u32 color, u32 z) /* 0x495220 */
{
    u32 n = U8(f + 6);
    gptr r = Tmp_Push(8);
    for (; n; n--, f += 8) {
        U16(r) = (u16)((s8)U8(f + 4) + x); U16(r + 2) = (u16)((s8)U8(f + 5) + y);
        U16(r + 4) = U8(f + 2) ? U8(f + 2) : 0x100;
        U16(r + 6) = U8(f + 3);
        if (U8(f + 7) & 2) Menu_Sprite80(r, U8(f), U8(f + 1), tp, color & 0xFFFFFF, color >> 31, z);
        else Menu_Sprite(r, U8(f), U8(f + 1), tp, color & 0xFFFFFF, color >> 31, z);
    }
    Tmp_Pop(8);
    draw_mode(tpage, z);
}

/* a four-colored quad (points v[4] {x, y}; code 0x38, semi 0x3A) and a draw mode */
void DC2CALL Menu_PolyG4(gptr v, u32 c0, u32 c1, u32 c2, u32 c3, u32 semi, u32 z) /* 0x494850 */
{
    gptr p = U32(0x87DC98);
    U32(p + 0x10) = U32(p + 0x20) = U32(p + 0x30) = U32(p + 0x40) = 0x3C23D70A;
    for (u32 k = 0; k < 4; k++) xy(p + 8 + 0x10 * k, v + 4 * k);
    U32(p + 4) = c0 | ((u16)semi ? 0x3A000000 : 0x38000000);
    U32(p + 0x14) = c1; U32(p + 0x24) = c2; U32(p + 0x34) = c3;
    ot_link(p, 8, z);
    U32(0x87DC98) += 0x44;
    draw_mode(0xD, (u16)z);
}

/* a line (code 0x40, semi 0x42) */
void DC2CALL Menu_Line(u32 x0, u32 y0, u32 x1, u32 y1, u32 color, u32 semi, u32 z) /* 0x4949E0 */
{
    gptr p = U32(0x87DC98);
    U32(p + 0x10) = U32(p + 0x1C) = 0x3C23D70A;
    F32(p + 8) = (float)(s16)x0; F32(p + 0xC) = (float)(s16)y0;
    F32(p + 0x14) = (float)(s16)x1; F32(p + 0x18) = (float)(s16)y1;
    U32(p + 4) = color | ((u16)semi ? 0x42000000 : 0x40000000);
    ot_link(p, 3, z);
    U32(0x87DC98) += 0x20;
}

/* a flat triangle (code 0x20, color bit 31: 0x22) */
void DC2CALL Menu_PolyF3(gptr v, u32 color, u32 z)                 /* 0x495980 */
{
    gptr p = U32(0x87DC98);
    for (u32 k = 0; k < 3; k++) xy(p + 8 + 0xC * k, v + 4 * k);
    U32(p + 0x28) = U32(p + 0x1C) = U32(p + 0x10) = 0x3C23D70A;
    U32(p + 4) = (color & 0xFFFFFF) | (color >> 31 ? 0x22000000 : 0x20000000);
    ot_link(p, 4, z);
    U32(0x87DC98) += 0x2C;
}

/* a three-colored triangle (code 0x30, c0 bit 31: 0x32) */
void DC2CALL Menu_PolyG3(gptr v, u32 c0, u32 c1, u32 c2, u32 z)    /* 0x495AA0 */
{
    gptr p = U32(0x87DC98);
    for (u32 k = 0; k < 3; k++) xy(p + 8 + 0x10 * k, v + 4 * k);
    U32(p + 0x30) = U32(p + 0x20) = U32(p + 0x10) = 0x3C23D70A;
    U32(p + 4) = (c0 & 0xFFFFFF) | (c0 >> 31 ? 0x32000000 : 0x30000000);
    U32(p + 0x14) = c1; U32(p + 0x24) = c2;
    ot_link(p, 6, z);
    U32(0x87DC98) += 0x34;
}

/* the drawing area x, y (+256 in the second buffer), w, h in slot z */
void DC2CALL Menu_DrawArea(u32 x, u32 y, u32 w, u32 h, u32 z)       /* 0x4946A0 */
{
    gptr r = Tmp_Push(8);
    U16(r) = (u16)x; U16(r + 2) = (u16)(U32(0x87DE0C) ? y + 0x100 : y);
    U16(r + 4) = (u16)w; U16(r + 6) = (u16)h;
    Gpu_SetDrawArea(U32(0x87DC98), r);
    Tmp_Pop(8);
    Gpu_AddPrim(0x87BC00 + 4 * ((U32(0x87DE0C) << 10) + (u16)z), U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}

/* text a then b (u16 codes, 0xFxxx ends) into d */
void DC2CALL Msg_Concat(gptr d, gptr a, gptr b)                     /* 0x494E80 */
{
    for (; (U16(a) & 0xF000) != 0xF000; a += 2, d += 2) U16(d) = U16(a);
    for (; (U16(b) & 0xF000) != 0xF000; b += 2, d += 2) U16(d) = U16(b);
    U16(d) = U16(b);
}

/* small letters (a.., 4x4 cells of 8x8 in the font at v 0x80) of an ASCII
 * string, n characters at most; 1 if it ended (0 byte) */
u32 DC2CALL Menu_SmallText(gptr s, u32 x, u32 y, u32 n, u32 color, u32 z) /* 0x494EE0 */
{
    u32 end = 0, cx = x;
    gptr r = Tmp_Push(8);
    for (; (u16)n; n--, s++) {
        u8 c = U8(s);
        if (c == '\n') { cx = x; y += 4; continue; }
        if (c == ' ') { cx += 4; continue; }
        if (!c) { end = 1; break; }
        s32 k = (s32)c - 0x61;
        U16(r) = (u16)cx; U16(r + 2) = (u16)y; U16(r + 4) = 4; U16(r + 6) = 4;
        Menu_Sprite80(r, (u32)((k % 13) << 3), (u32)((k / 13) * 8 + 0x80), 0x7C8F, color & 0xFFFFFF, color >> 31, z);
        cx += 4;
    }
    Tmp_Pop(8);
    draw_mode(0x28, (u16)z);
    return end;
}

/* the three arrows (frame 0x685028), pulsing with the frame count */
void DC2CALL Menu_Arrows(u32 x, u32 y)                              /* 0x495350 */
{
    u32 v = (u8)((U8(G() + 4) & 0x1E) << 3);
    x &= 0xFFFF; y &= 0xFFFF;
    Menu_DrawFrames(0x685028, x, y, 0xC, 0x7C8F, 0x80000000 | (v + 0xA0) << 8 | (v + 0xA0), 2);
    Menu_DrawFrames(0x685028, x + 6, y, 0xC, 0x7C8F, 0x80000000 | (v + 0x50) << 8 | (v + 0x50), 2);
    Menu_DrawFrames(0x685028, x + 0xC, y, 0xC, 0x7C8F, 0x80000000 | v << 8 | v, 2);
}

/* flags 9:from..to-1 set */
u32 DC2CALL Flag_CountRange(u32 from, u32 to)                       /* 0x495BE0 */
{
    u8 c = 0;
    if ((u8)from < (u8)to)
        for (u32 i = (u8)from, k = (u8)to - (u8)from; k; k--, i++) if (Flag_Check(9, i)) c++;
    return c;
}

/* the first max of them into out */
void DC2CALL Flag_ListRange(u32 from, u32 to, u32 max, gptr out)    /* 0x495C30 */
{
    u8 c = 0;
    for (u8 i = (u8)from; i < (u8)to; i++)
        if (Flag_Check(9, i)) {
            U8(out++) = i;
            if (++c >= (u8)max) return;
        }
}

/* the first free record of n (0xFF: none) */
u32 DC2CALL Item_FindFree(gptr p, u32 n)                            /* 0x495D10 */
{
    for (u8 i = 0; i < (u8)n; i++, p += 12) if (!U8(p)) return i;
    return 0xFF;
}

/* where item id goes: a free record or one of id not full (+8 < +0xA) */
u32 DC2CALL Item_FindRoom(gptr p, u32 n, u32 id)                    /* 0x495D30 */
{
    for (u8 i = 0; i < (u8)n; i++, p += 12) {
        if (!U8(p)) return i;
        if (U8(p + 1) == (u8)id && S16(p + 8) < S16(p + 0xA)) return i;
    }
    return 0xFF;
}

/* ---- numbers: digits sprites of 8 x h at v in the font (0x7C8F) ---- */
static u32 number(u32 x, u32 y, u32 digits, u32 lead, u32 color, u32 value, u32 z, u32 v, u32 h, u32 step)
{
    u32 drawn = 0, pw = 1;
    for (u32 k = (u16)digits; k > 1; k--) pw *= 10;
    gptr r = Tmp_Push(8);
    for (u32 k = (u16)digits; k; k--, x += step) {
        u32 q = value / pw;
        if ((u16)q) lead = 1;
        value -= (q & 0xFFFF) * pw;
        pw /= 10;
        if (!pw) lead = 1;                      /* the last digit, 0 or not */
        else if (!(u16)lead && !(u16)q) continue;
        U16(r) = (u16)x; U16(r + 2) = (u16)y; U16(r + 4) = 8; U16(r + 6) = (u16)h;
        Menu_Sprite(r, q << 3, v, 0x7C8F, color & 0xFFFFFF, color >> 31, z);
        drawn++;
    }
    Tmp_Pop(8);
    draw_mode(0xC, (u16)z);
    return drawn;
}

/* value in digits places (leading zeros if lead); the digits drawn */
u32 DC2CALL Menu_Number8(u32 x, u32 y, u32 digits, u32 lead, u32 color, u32 value, u32 z)  /* 0x492E20 */
{
    return number(x, y, digits, lead, color, value, z, 0x10, 8, 7);
}
u32 DC2CALL Menu_Number11(u32 x, u32 y, u32 digits, u32 lead, u32 color, u32 value, u32 z) /* 0x492F80 */
{
    return number(x, y, digits, lead, color, value, z, 0, 0xB, 8);
}
u32 DC2CALL Menu_Number12(u32 x, u32 y, u32 digits, u32 lead, u32 color, u32 value, u32 z) /* 0x4930E0 */
{
    return number(x, y, digits, lead, color, value, z, 0x18, 0xC, 7);
}

/* "yes" and "no" (texts 0x723FD4, 0x723FDC), the one chosen (sel: 0 yes) lit and boxed */
void DC2CALL Menu_YesNo(u32 x, u32 y, u32 sel, u32 z)               /* 0x494AC0 */
{
    u32 c0 = (u16)sel ? 0x404040 : 0x808080, c1 = (u16)sel ? 0x808080 : 0x404040;
    Text_Draw(0x723FD4, x + 9, y, 0x64, c0, z, 0);
    Text_Draw(0x723FDC, x + 0x52, y, 0x64, c1, z, 0);
    gptr r = Tmp_Push(8);
    U16(r) = (u16)((u16)sel ? x + 0x44 : x); U16(r + 2) = (u16)y; U16(r + 4) = 0x30; U16(r + 6) = 0x10;
    Menu_Tile(r, 0x8000, 1, z);
    Tmp_Pop(8);
}

/* the points (M+0x40 counts towards G+0x11F4, a third of the way a frame,
 * ticking) in a box at x, y */
void DC2CALL Menu_Points(u32 x, u32 y, u32 color)                   /* 0x494150 */
{
    gptr g = G(), pts = g + 0x6CC;
    s32 d = (s32)(U32(pts) - U32(g + 0x11F4));
    if (d) {
        if (!(U8(g + 4) & 3)) Se_Play(0, 0);
        U32(pts) -= (u32)(d > 3 || d < -3 ? d / 3 : d > 0 ? 1 : -1);
    }
    gptr r = Tmp_Push(8);
    U16(r) = (u16)(x + 0x18); U16(r + 2) = (u16)y; U16(r + 4) = 0x80; U16(r + 6) = 0x10;
    Menu_Tile(r, 0x274C11, 1, 3);
    Menu_Number11(x + 0x48, y + 4, 7, 1, color, U32(pts), 1);
    U16(r) = (u16)(x + 0x84); U16(r + 2) = (u16)(y + 4); U16(r + 4) = 0x10; U16(r + 6) = 0xA;
    Menu_Sprite(r, 0x50, 0, 0x7C8F, color, 0, 1);
    Tmp_Pop(8);
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0xC, 0);
    Gpu_AddPrim(0x87BC04 + (U32(0x87DE0C) << 12), U32(0x87DC98));
    U32(0x87DC98) += 0xC;
    Menu_DrawFrames(0x6854C0, x + 8, y, 0xC, 0x7C8E, 0x808080, 2);
    Menu_DrawFrames(0x6854C8, x + 0x98, y, 0xC, 0x7C8E, 0x808080, 2);
    Menu_DrawFrames(0x685010, x + 0x18, y + 1, 0xC, 0x7C8F, 0x808080, 2);
}

/* the four entries of the pause screen (frames 0x684FC0..), the one at
 * M+0x4D lit with the pulse M+0x84 */
void DC2CALL Menu_PauseList(gptr m)                                 /* 0x493F40 */
{
    gptr v = Tmp_Push(0x20);
    for (u32 i = 0; i < 4; i++) {
        s32 pulse = S16(m + 0x84);
        u32 c = (s8)U8(m + 0x4D) == (s32)i ? (u32)(pulse / 2 + 0x20) << 8 | (u32)(pulse / 2) << 16 | (u32)(pulse / 2) : 0x2000;
        Menu_DrawFrames(0x684FC0 + 8 * i, 0x14 + 8 * i, 0x78 + 0x10 * i, 0xC, 0x7C8F, c, 2);
        if ((s8)U8(m + 0x4D) == (s32)i) {
            s32 q = S16(m + 0x84) / 4, h = S16(m + 0x84) / 2;
            c = (u32)(h + 0x6D) << 8 | (u32)(q + 0x56) << 16 | (u32)(q + 0x14);
        } else c = 0x2B5F2F;
        static const u16 ax[2][2] = {{0x10, 0x3C}, {0x3C, 0x68}};
        for (u32 k = 0; k < 2; k++) {
            gptr p = v + 0x10 * k;
            U16(p) = U16(p + 8) = (u16)(8 * i + ax[k][0]); U16(p + 4) = U16(p + 0xC) = (u16)(8 * i + ax[k][1]);
            U16(p + 2) = U16(p + 6) = (u16)(16 * i + 0x78); U16(p + 0xA) = U16(p + 0xE) = (u16)(16 * i + 0x88);
        }
        Menu_PolyG4(v, 0x2000, c, 0x2000, c, 1, 0x64);
        Menu_PolyG4(v + 0x10, c, 0x2000, c, 0x2000, 1, 0x64);
    }
    Tmp_Pop(0x20);
}

/* lines sweeping out of a corner at x, y: phase, dir 1 or -1 (0xFF) */
void DC2CALL Menu_Sweep(u32 x, u32 y, u32 phase, u32 dir)           /* 0x492850 */
{
    Menu_DrawArea(0, 0, 0x140, 0xF0, 0x64);
    Menu_Line(x, y, x, y + 0x40, 0x808080, 0, 0x64);
    s32 lim = (s32)(y & 0xFFFF) + 0x40;
    for (u8 i = 0; i < 0x14; i++) {
        u32 ly = y + i * 4 - 8 + phase;
        if ((s16)ly > lim) break;
        if (i & 1) Menu_Line(x, ly, x + (s8)dir * 4, ly, 0x808080, 0, 0x64);
        else Menu_Line(x, ly, x + (s8)dir * 8, ly, 0x606060, 0, 0x64);
    }
    Menu_DrawArea((u8)dir == 0xFF ? x + 0xFFF8 : x, y, 9, 0x38, 0x64);
}

/* the header: brackets, the sweeps, the stage name, the scrolling hint
 * (M+0x5E letters of text M+0x5F), the portrait and its shading */
void DC2CALL Menu_Header(gptr m)                                    /* 0x492520 */
{
    static const u8 L[6][4] = {{0x18, 0x18, 0x26, 0x18}, {0x18, 0x57, 0x26, 0x57}, {0x18, 0x18, 0x18, 0x57},
                               {0x69, 0x18, 0x78, 0x18}, {0x69, 0x57, 0x78, 0x57}, {0x78, 0x18, 0x78, 0x57}};
    for (int k = 0; k < 6; k++) Menu_Line(L[k][0], L[k][1], L[k][2], L[k][3], 0x808080, 0, 0x64);
    gptr g = G();
    if (U8(g + 4) & 1) U16(m + 0x80)++;
    U16(m + 0x80) &= 7;
    Menu_Sweep(0x1B, 0x1C, U16(m + 0x80), 1);
    if (U8(G() + 4) & 1) U16(m + 0x82)--;
    U16(m + 0x82) &= 7;
    Menu_Sweep(0x74, 0x1C, U16(m + 0x82), 0xFFFFFFFF);
    u8 st = U8(G() + 0x108F);
    if (st == 1) Menu_DrawFrames(0x685038, 0x48, 0x17, 0xC, 0x7C8F, 0x707070, 2);
    else if (st == 0) Menu_DrawFrames(0x685030, 0x48, 0x17, 0xC, 0x7C8F, 0x707070, 2);
    static const u32 hint[9] = {0x739D48, 0x739D24, 0x739D08, 0x739CF4, 0x739CE0, 0x739CC0, 0x739C98, 0x739C8C, 0x739C58};
    if (!(U8(G() + 4) & 3)) U8(m + 0x5E)++;
    if (Menu_SmallText(hint[U8(m + 0x5F)], 0x50, 0x28, U8(m + 0x5E), 0x505050, 0x64)) {
        U8(m + 0x5E) = 0;
        if (++U8(m + 0x5F) > 8) U8(m + 0x5F) = 0;
    }
    gptr r = Tmp_Push(0x20);
    U16(r) = 0x18; U16(r + 2) = 0x18; U16(r + 4) = 0x40; U16(r + 6) = 0x40;
    Menu_Sprite(r, 0x40, 0, 0x7CC0, 0x808080, 0, 0x64);
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0x8D, 0);
    Gpu_AddPrim(0x87BD90 + (U32(0x87DE0C) << 12), U32(0x87DC98));
    U32(0x87DC98) += 0xC;
    for (u32 k = 0; k < 2; k++) {
        gptr v = r + 0x10 * k;
        U16(v) = U16(v + 8) = 0x17; U16(v + 4) = U16(v + 0xC) = 0x78;
        U16(v + 2) = U16(v + 6) = (u16)(k ? 0x38 : 0x18); U16(v + 0xA) = U16(v + 0xE) = (u16)(k ? 0x50 : 0x38);
    }
    Menu_PolyG4(r, 0x1000, 0x1000, 0x8000, 0x8000, 1, 0xC8);
    Menu_PolyG4(r + 0x10, 0x8000, 0x8000, 0x1000, 0x1000, 1, 0xC8);
    Tmp_Pop(0x20);
    Menu_DrawFrames(0x6836C8, 0x48, 0x18, 0xD, 0x7C8E, 0x80808080, 2);
}

/* two rows of three gauge boxes at x - 0x58, y, blinking with the frame count */
void DC2CALL Menu_Gauges(gptr m, u32 x, u32 y)                      /* 0x492930 */
{
    (void)m;
    Menu_DrawFrames(0x683BB8, x & 0xFFFF, (y & 0xFFFF) + 8, 0xD, 0x7C8E, 0x808080, 1);
    u32 a = (U8(G() + 4) >> 2 & 1) ^ 1, c = U8(G() + 4) & 2 ? 0 : 2;
    gptr r = Tmp_Push(8);
    U16(r + 4) = 0x3B; U16(r + 6) = 0x1C;
    for (u32 row = 0; row < 2; row++, y += 0x1C) {
        u32 cx = x - 0x58;
        for (u32 k = 0; k < 3; k++, cx += 0x3B) {
            U16(r) = (u16)cx; U16(r + 2) = (u16)y;
            Menu_Sprite(r, c + 0xB0, a + 0x80, 0x7C8E, 0x3080, 1, 1);
            Menu_Sprite(r, a + 0xB2, 0x82 - c, 0x7C8E, 0x402000, 1, 1);
        }
    }
    Tmp_Pop(8);
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0xC, 0);
    Gpu_AddPrim(0x87BC04 + (U32(0x87DE0C) << 12), U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}

/* the menu's frame: title, borders, the button hints */
void DC2CALL Menu_Frame(gptr m)                                     /* 0x492A80 */
{
    gptr t = Tmp_Push(4);
    U16(t) = 0x1B; U16(t + 2) = 0xF000;                 /* a text of one glyph */
    Menu_DrawFrames(U8(G() + 0x108F) == 1 ? 0x685008 : 0x685000, 0x88, 0xE, 0xC, 0x7C8F, 0x8068C040, 2);
    Menu_DrawFrames(0x683798, 8, 0x78, 0xD, 0x7C8E, 0x80808080, 0x1F4);
    Menu_DrawFrames(0x683CD8, 0x78, 0x40, 0xD, 0x7C8E, 0x181818, 0x3E8);
    Menu_DrawFrames(0x683928, 0x138, 0x78, 0xD, 0x7C8E, 0x80808080, 0x1F4);
    Text_Draw(t, 0x86, 0xD8, 0x64, 0x404040, 2, 0);
    Tmp_Pop(4);
    if (U8(m) == 2 && U8(m + 1) && U8(m + 1) != 5 && !U8(m + 0x64))
        Menu_DrawFrames(0x685040, 0x98, 0xDB, 0xC, 0x7C8F, 0x8068C040, 2);
    else Menu_DrawFrames(0x6850B8, 0x9A, 0xDB, 0xC, 0x7C8F, 0x8068C040, 2);
}

/* the backdrop: three light bars sliding across (M+0x74.. their rows, +0x7A
 * how far, +0x7C its speed) over a grid that blinks with the frame count */
void DC2CALL Menu_Backdrop(gptr m)                                  /* 0x492BC0 */
{
    static const u16 X[3][3] = {{0x140, 0x294, 0x280}, {0x17C, 0x2E4, 0x2D0}, {0x190, 0x2D0, 0x2BC}};
    static const u32 C[3] = {0x453A31, 0x3A4531, 0x313A45};
    u16 a = U16(m + 0x7A);
    gptr v = Tmp_Push(0x30);
    for (u32 k = 0; k < 3; k++) {
        gptr q = v + 0x10 * k;
        u16 y = U16(m + 0x74 + 2 * k);
        U16(q) = U16(q + 8) = (u16)(X[k][0] - a); U16(q + 4) = (u16)(X[k][1] - a); U16(q + 0xC) = (u16)(X[k][2] - a);
        U16(q + 2) = U16(q + 6) = y; U16(q + 0xA) = U16(q + 0xE) = (u16)(y + 3);
    }
    if (!a) for (u32 k = 0; k < 3; k++) U16(m + 0x74 + 2 * k) = (u16)(Rand8() % 0x140);
    U16(m + 0x7A) += U16(m + 0x7C);
    U16(m + 0x7C)++;
    if (S16(m + 0x7A) > 0x3C0) { U16(m + 0x7A) = 0; U16(m + 0x7C) = 0; }
    for (u32 k = 0; k < 3; k++) Menu_PolyG4(v + 0x10 * k, C[k], 0x2000, C[k], 0x2000, 1, 0x384);
    Tmp_Pop(0x30);
    u32 c = U32(G() + 4) & 0x40 ? 0x3000 : 0x103010;
    for (u32 i = 1; i < 0x13; i++) Menu_Line(i * 16, 0, i * 16, 0xF0, c, 1, 0x3FF);
    for (u32 i = 1; i < 0xF; i++) Menu_Line(0, i * 16, 0x140, i * 16, c, 1, 0x3FF);
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0xC, 0);
    Gpu_AddPrim(0x87CBFC + (U32(0x87DE0C) << 12), U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}

/* gauge cells blinking with the frame count: rows x cols of w x 0x14 at
 * x0 + col * dx, y0 + row * 20 (slot 0x320) */
static void cells(u32 x0, u32 dx, u32 y0, u32 w)
{
    u32 a = (U8(G() + 4) >> 2 & 1) ^ 1, c = U8(G() + 4) & 2 ? 0 : 2;
    gptr r = Tmp_Push(8);
    U16(r + 4) = (u16)w; U16(r + 6) = 0x14;
    for (u32 j = 0; j < 4; j++)
        for (u32 i = 0; i < 4; i++) {
            U16(r) = (u16)(x0 + j * dx); U16(r + 2) = (u16)(y0 + i * 20);
            Menu_Sprite(r, c + 0xB0, a + 0x80, 0x7C8E, 0x3080, 1, 0x320);
            Menu_Sprite(r, a + 0xB2, c + 0x82, 0x7C8E, 0x402000, 1, 0x320);
        }
    Tmp_Pop(8);
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0xC, 0);
    Gpu_AddPrim(0x87C880 + (U32(0x87DE0C) << 12), U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}

/* the right-hand panels of the pause screen and their two grids of cells */
void DC2CALL Menu_Panels(void)                                      /* 0x493C10 */
{
    Menu_DrawFrames(0x6835C0, 0xD8, 0x18, 0xD, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x684FF8, 0xB0, 0x18, 0xC, 0x7C8F, 0x8068C040, 2);
    Menu_DrawFrames(0x6854B0, 0xD8, 0x18, 0xC, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x683628, 0xD8, 0x73, 0xD, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x6853D8, 0xD8, 0x73, 0xC, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x684208, 0xD8, 0xA4, 0xD, 0x7C8E, 0x808080, 1);
    cells(0x88, 0x28, 0x20, 0x28);
    cells(0x80, 0x2C, 0x7C, 0x2C);
    Menu_DrawFrames(0x683E38, 0xD8, 0xCD, 0xD, 0x7C8E, 0x808080, 1);
}

/* the ammunition of item record k: loaded (+0x10C4) and spare (+0x10C6), or
 * the marks of infinite (weapon flag 0x80) or of "no ammunition" (G+0x1094 & 1) */
static void ammo(u32 flags, s32 k, u32 x, u32 dxmark, u32 dx2, u32 c0, u32 c1)
{
    if (flags & 0x80) {
        Menu_DrawFrames(0x685CA8, x, 0x48, 0xC, 0x7C8F, 0x707070, 0x64);
        Menu_DrawFrames(0x685CA8, x + dx2, 0x48, 0xC, 0x7C8F, 0x7070, 0x64);
    } else if (U8(G() + 0x1094) & 1) {
        Menu_DrawFrames(0x685CC8, x + dxmark, 0x48, 0xC, 0x7C8F, 0x707070, 0x64);
        Menu_DrawFrames(0x685CC8, x + dxmark + dx2, 0x48, 0xC, 0x7C8F, 0x7070, 0x64);
    } else {
        Menu_Number8(x, 0x48, 4, 0, c0, (u32)(s32)S16(G() + k * 12 + 0x10C4), 0x64);
        Menu_Number8(x + dx2, 0x48, 4, 0, c1, (u32)(s32)S16(G() + k * 12 + 0x10C6), 0x64);
    }
}

/* a weapon's picture (u, v of its record) in r, then the draw mode tp */
static void weapon_pic(u32 x, u32 u, u32 v, u32 tp, u32 tpage, u32 z, u32 zs)
{
    gptr r = Tmp_Push(8);
    U16(r) = (u16)x; U16(r + 2) = 0x28; U16(r + 4) = 0x40; U16(r + 6) = 0x20;
    Menu_Sprite(r, u, v, tp, 0x808080, 0, zs);
    Tmp_Pop(8);
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, tpage, 0);
    Gpu_AddPrim(0x87C3D0 + 4 * ((U32(0x87DE0C) << 10) + z), U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}

/* the ammunition box: its left end, middle and right end */
static void ammo_box(u32 x0, u32 x1, u32 x2, u32 y, u32 z)
{
    Menu_DrawFrames(0x6857A0, x0, y + 0x14, 0xC, 0x7C8E, 0x808080, z + 0x258);
    Menu_DrawFrames(0x685058, x1, y + 0x1C, 0xC, 0x7C8E, 0x808080, z + 0x258);
    Menu_DrawFrames(0x6857A8, x2, y + 0x14, 0xC, 0x7C8E, 0x808080, z + 0x258);
}

/* the weapons panel at x, y in slots z..: the main weapon (G+0x10B4, its
 * record 0x704268 + 12 * id {u, v, flags: 0x20 both hands, 0x80 infinite})
 * and the sub-weapon (G+0x10B5, records 0x704328; 9: its own picture), their
 * ammunition, and the cells */
void DC2CALL Menu_Weapons(gptr m, u32 x, u32 y, u32 z)              /* 0x493240 */
{
    (void)m;
    u32 zs = z;                                  /* (the sprites' slots: all 32 bits) */
    x &= 0xFFFF; y &= 0xFFFF; z &= 0xFFFF;
    gptr g = G();
    Menu_DrawFrames(0x6846C8, x + 0x58, y, 0xD, 0x7C8E, 0x80808080, z + 2);
    Menu_DrawFrames(0x6835C0, x + 0x58, y, 0xD, 0x7C8E, 0x80808080, z + 2);
    Menu_DrawFrames(0x683628, x + 0x58, y + 0x38, 0xD, 0x7C8E, 0x80808080, z + 2);
    Menu_DrawFrames(0x684FE8, x + 0x30, y, 0xC, 0x7C8F, 0x8068C040, z + 2);
    Menu_DrawFrames(0x685048, x + 8, y + 8, 0xC, 0x7C8F, 0x707070, z + 2);
    gptr w = 0x704268 + U8(g + 0x10B4) * 12;
    if (U8(w + 2) & 0x20) {                      /* both hands: one wide picture */
        Menu_DrawFrames(0x683ED8, x + 0x58, y + 8, 0xD, 0x7C8E, 0x80808080, z + 0x190);
        Menu_DrawFrames(0x683ED8, x + 0x58, y + 8, 0x4D, 0x7C8E, 0xFFFFFFFF, z + 0x2BC);
        gptr r = Tmp_Push(8);
        U16(r) = 0x98; U16(r + 2) = 0x28; U16(r + 4) = 0x80; U16(r + 6) = 0x20;
        Menu_Sprite(r, U8(w), U8(w + 1), 0x7C80, 0x808080, 0, zs + 0x1F4);
        Tmp_Pop(8);
        Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0x8D, 0);
        Gpu_AddPrim(0x87C3D0 + 4 * ((U32(0x87DE0C) << 10) + z), U32(0x87DC98));
        U32(0x87DC98) += 0xC;
        s8 k = (s8)Item_Find(G() + 0x10BC, 0x14, U8(G() + 0x10B4));
        ammo_box(x + 0x3A, x + 0x50, x + 0x6E, y, z);
        ammo(U8(0x704268 + U8(G() + 0x10B4) * 12 + 2), k, 0xDA, 8, 0x2A, 0x707070, 0x7070);
    } else {
        Menu_DrawFrames(0x685050, x + 0x58, y + 8, 0xC, 0x7C8F, 0x707070, z + 2);
        Menu_DrawFrames(0x6846D8, x + 0x58, y + 8, 0xD, 0x7C8E, 0x80808080, z + 0x190);
        Menu_DrawFrames(0x6846D8, x + 0x58, y + 8, 0x4D, 0x7C8E, 0xFFFFFFFF, z + 0x2BC);
        u8 id = U8(G() + 0x10B4);
        if (id) {
            gptr w1 = 0x704268 + id * 12;
            weapon_pic(0x90, U8(w1), U8(w1 + 1), 0x7C80, 0x8D, z, zs + 0x1F4);
            s8 k = (s8)Item_Find(G() + 0x10BC, 0x14, U8(G() + 0x10B4));
            ammo_box(x + 0x1C, x + 0x28, x + 0x3C, y, z);
            ammo(U8(0x704268 + U8(G() + 0x10B4) * 12 + 2), k, 0x8A, 8, 0x2A, 0x707070, 0x7070);
        }
        if (U8(G() + 0x10B5)) {                  /* the sub-weapon */
            u16 sw = U16(G() + 0x10B4);
            if ((sw & 0xFF00) == 0x900) weapon_pic(0xE0, 0x40, 0x40, 0x7F00, 0x9B, z, zs + 0x1F4);
            else {
                gptr w2 = 0x704328 + (sw >> 8) * 12;
                weapon_pic(0xE0, U8(w2), U8(w2 + 1), 0x7C80, 0x8D, z, zs + 0x1F4);
            }
            s8 k = (s8)Item_Find(G() + 0x10BC, 0x14, (u8)(U8(G() + 0x10B5) + 0x10));
            ammo_box(x + 0x6C, x + 0x78, x + 0x8C, y, z);
            ammo(U8(0x70432A + (U16(G() + 0x10B4) >> 8) * 12), k, 0xDA, 8, 0x2A, 0x808080, 0x8080);
        }
    }
    /* the cells */
    u32 a = (U8(G() + 4) >> 2 & 1) ^ 1, c = U8(G() + 4) & 2 ? 0 : 2;
    gptr r = Tmp_Push(8);
    U16(r + 4) = 0x28; U16(r + 6) = 0x18;
    for (u32 j = 0; j < 2; j++)
        for (u32 i = 0; i < 4; i++) {
            U16(r) = (u16)(x + 40 * i + 8); U16(r + 2) = (u16)(y + 24 * j + 8);
            Menu_Sprite(r, c + 0xB0, a + 0x80, 0x7C8E, 0x3080, 1, zs + 0x320);
            Menu_Sprite(r, a + 0xB2, c + 0x82, 0x7C8E, 0x402000, 1, zs + 0x320);
        }
    Tmp_Pop(8);
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0xC, 0);
    Gpu_AddPrim(0x87C880 + 4 * ((U32(0x87DE0C) << 10) + z), U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}

/* the health bar at x, y: M+0x4A shown health rising 10 a frame to the
 * player's (+0x108 of +0x10A), the bar 0x70 wide; low health blinks,
 * poison (+0x17E) pulses; a full bar shimmers (M+0x59) */
void DC2CALL Menu_Health(u32 x, u32 y)                              /* 0x4942E0 */
{
    gptr g = G(), pl = U32(g + 0x940), m = g + 0x68C;
    u16 shown = U16(m + 0x4A);
    u32 bar = (u32)((s32)(shown * 0x70) / S16(pl + 0x10A));
    if ((s16)bar < 0) bar = 0; else if ((s16)bar > 0x70) bar = 0x70;
    U16(m + 0x4A) = (u16)(shown + 10);
    if ((s32)U16(m + 0x4A) > S16(pl + 0x108)) U16(m + 0x4A) = U16(pl + 0x108);
    if ((s32)U16(m + 0x4A) <= S16(pl + 0x10A) / 4 && (U8(G() + 4) & 0x10))
        Menu_DrawFrames(0x685020, x - 2, y, 0xC, 0x7C8F, 0x40, 2);
    u32 col;
    if (S16(pl + 0x17E)) col = Color_Pulse(0x80, 0x30, 3) | 0x2000000;
    else { Menu_Arrows(x + 0x30, y + 8); col = 0x808080; }
    Menu_DrawFrames(0x6850A8, x + 0x40, y + 8, 0xC, 0x7C8F, col, 2);
    Menu_DrawFrames(0x683620, x, y, 0xD, 0x7C8E, col, 2);
    gptr r = Tmp_Push(8);
    if ((s32)U16(m + 0x4A) == S16(pl + 0x108) && (s16)bar > 1) {
        U16(r) = (u16)x; U16(r + 2) = (u16)y; U16(r + 4) = 1; U16(r + 6) = 0x10;
        if (U8(G() + 4) & 1) U8(m + 0x59)++;
        if ((s16)bar <= (s16)U8(m + 0x59)) U8(m + 0x59) = 0;
        u32 si = U8(m + 0x59);
        s32 prev = -1;
        int wrapped = 0;
        for (u16 j = 0; j < 0x18; j++, si++) {
            if ((s16)bar <= (s32)(u16)si) {
                si = 0;
                if (wrapped) break;
                wrapped = 1;
            }
            if ((s16)prev == (s32)(u16)si) break;
            U16(r) = (u16)(si + x);
            u32 v = j * 3u;
            Menu_Sprite(r, si + 0x70, 0x50, 0x7C8F, v << 8 | v << 16 | v, 1, 1);
            prev = (s32)si;
        }
        Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0x2C, 0);
        Gpu_AddPrim(0x87BC04 + (U32(0x87DE0C) << 12), U32(0x87DC98));
        U32(0x87DC98) += 0xC;
    }
    s32 mx = S16(U32(G() + 0x940) + 0x10A);
    u16 now = U16(m + 0x4A);
    u32 c;
    if ((s32)now > mx / 4 && !S16(U32(G() + 0x940) + 0x17E))
        c = (s32)now > mx / 2 ? Color_Pulse(0x8000, 0x4000, 5) : Color_Pulse(0x8080, 0x4040, 4);
    else c = Color_Pulse(0x80, 0x40, 3);
    U16(r) = (u16)x; U16(r + 2) = (u16)y; U16(r + 4) = (u16)bar; U16(r + 6) = 0x10;
    Menu_Sprite(r, 0x70, 0x50, 0x7C8F, c, 1, 1);
    Tmp_Pop(8);
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0xC, 0);
    Gpu_AddPrim(0x87BC04 + (U32(0x87DE0C) << 12), U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}

/* the pause screen: header, health, points, weapons (not when opened for an item), frame */
void DC2CALL Menu_PauseScreen(gptr m)                               /* 0x493BC0 */
{
    Menu_Header(m);
    Menu_Health(0x10, 0x58);
    Menu_Points(0x80, 0x58, 0x808080);
    if (!U8(m + 0x64)) Menu_Weapons(m, 0x80, 0x18, 2);
    Menu_Frame(m);
}

/* n cells of s x s round the rect r (moved out by s: r is changed) from
 * place pos, their color rising from black to color */
void DC2CALL Menu_Border(gptr r, u32 pos, u32 n, u32 s, u32 color)  /* 0x4957A0 */
{
    s32 nw = S16(r + 4) / (s32)(s & 0xFFFF), nh = S16(r + 6) / (s32)(s & 0xFFFF);
    U16(r) -= (u16)s; U16(r + 2) -= (u16)s;
    u32 cr = color & 0xFF, cg = color >> 8 & 0xFF, cb = color >> 16 & 0xFF;
    u32 cnt = (u32)(s32)(s16)n, dn = cnt;
    if (!cnt) return;
    u32 half = (u32)(nw + nh), ar = 0, ag = 0, ab = 0;
    gptr c = Tmp_Push(8);
    U16(c + 4) = (u16)s; U16(c + 6) = (u16)s;
    for (; cnt; cnt--) {
        u32 k = (pos & 0xFFFF) % (2 * half + 4);
        pos = k + 1;
        u32 col = ((ab / dn) << 8 | ag / dn) << 8 | ar / dn;
        if (k < (u32)nw + 2) {                                  /* top */
            U16(c) = (u16)(k * s + U16(r)); U16(c + 2) = U16(r + 2);
        } else if (k < half + 3) {                              /* right */
            U16(c) = (u16)(U16(r) + U16(r + 4) + s); U16(c + 2) = (u16)((k - nw - 1) * s + U16(r + 2));
        } else if (k < (u32)nh + 2 * ((u32)nw + 2)) {           /* bottom */
            U16(c) = (u16)(U16(r) - (k - nh - nw - 3) * s + U16(r + 4)); U16(c + 2) = (u16)(U16(r + 6) + s + U16(r + 2));
        } else {                                                /* left */
            U16(c) = U16(r); U16(c + 2) = (u16)(U16(r + 6) - (k - (2 * nw + 4) - nh) * s + U16(r + 2));
        }
        Menu_Tile(c, col, 1, 1);
        ab += cb; ag += cg; ar += cr;
    }
    Tmp_Pop(8);
}

/* the line about pause entry i */
void DC2CALL Pause_Describe(u32 i, u32 x, u32 y, u32 w)             /* 0x4920F0 */
{
    static const u32 text[4] = {0x724146, 0x724208, 0x72427A, 0x724292};
    (void)x; (void)y; (void)w;
    Menu_DrawFrames(0x684A40, 0x60, 0xC4, 0xD, 0x7C8E, 0x80808080, 3);
    Menu_DrawFrames(0x684B60, 0xE8, 0xC4, 0xD, 0x7C8E, 0x80808080, 3);
    Text_Draw(text[i & 0xFFFF], 0x20, 0xC5, 0x64, 0x707070, 0x64, 0);
}

/* screen 0: the pause menu (M+0x4D the entry: up/down, select opens screen
 * entry + 1, cancel leaves through screen 5); the entry's box slides open
 * (M+0x84 by M+0x86) and its border runs round (M+0x88) */
void DC2CALL Screen_Pause(gptr m)                                   /* 0x491E50 */
{
    u16 k = U16(m + 0x44);
    if (k & 0x1000) {
        Se_Play(0, 0);
        U16(m + 0x84) = 0; U16(m + 0x86) = 0;
        if ((s8)--U8(m + 0x4D) < 0) U8(m + 0x4D) = 3;
    } else if (k & 0x4000) {
        Se_Play(0, 0);
        U16(m + 0x84) = 0; U16(m + 0x86) = 0;
        if ((s8)++U8(m + 0x4D) > 3) U8(m + 0x4D) = 0;
    } else if (U8(0x87DC00) & 0x40) {
        U8(m + 1) = 5;
        U16(m + 0x84) = 0; U16(m + 0x86) = 0; U8(m + 2) = 0;
    } else if (U8(0x87DC00) & 0x20) {
        U16(m + 0x84) = 0xFA; U16(m + 0x86) = 0xFA;
        U8(m + 1) = (u8)(U8(m + 0x4D) + 1); U8(m + 2) = 0; U8(m + 0x4F) = 0;
        Se_Play(0, 2);
        s32 e = (s8)U8(m + 0x4D) + 1;
        if (e == 3 && U8(G() + 0xB48) != 3) { Game_Start(3); U8(G() + 0xB48) = 3; }
        if ((s8)U8(m + 0x4D) + 1 == 4) {
            u8 s = (u8)Area_SoundArea(U8(G() + 0x1091), U8(G() + 0x1090));
            U8(m + 0x62) = s; U8(m + 0x60) = (u8)(s + 5);
            U8(m + 0x63) = U8(G() + 0x1090);
            if ((s32)U8(G() + 0xB48) != (s8)U8(m + 0x60)) {
                Pause_LoadScreen();
                Game_Start((u32)(s32)(s8)U8(m + 0x60));
                U8(G() + 0xB48) = U8(m + 0x60);
            }
        }
    }
    Menu_PauseScreen(m);
    Menu_PauseList(m);
    u16 sp = U16(m + 0x86);
    U16(m + 0x84) += sp;
    if (S16(m + 0x84) > 0xFA) U16(m + 0x84) = 0xFA;
    else U16(m + 0x86) = (u16)(sp + 3);
    s16 a = (s8)U8(m + 0x4D);
    Pause_Describe((u16)a, (u32)(a * 8 + 0x68), (u32)(a * 16 + 0x78), (u16)(U16(m + 0x84) - 0x32));
    U16(m + 0x88)++;
    if (S16(m + 0x88) > 0x64) U16(m + 0x88) = 0;
    gptr r = Tmp_Push(8);
    for (u32 j = 0; j < 2; j++) {
        a = (s8)U8(m + 0x4D);
        U16(r) = (u16)(a * 8 + 0x14); U16(r + 2) = (u16)(a * 16 + 0x78); U16(r + 4) = 0x50; U16(r + 6) = 0x10;
        Menu_Border(r, (u16)(U16(m + 0x88) + 0x32 * j), 0x32, 2, 0xFFFF);
    }
    Tmp_Pop(8);
}

/* screen 5 (and mode 9): leaving; a line closes across the middle, then mode 3 */
void DC2CALL Screen_Leave(gptr m)                                   /* 0x492180 */
{
    gptr g;
    switch ((s8)U8(m + 2)) {
    case 0:
        U8(m + 2)++;
        U8(m + 0x65) = 0xFF; U16(m + 0x8E) = 0; U16(m + 0x90) = 0;
        Se_Play(0, 7);
        Snd_Unduck();
        /* fall through */
    case 1:
        U16(m + 0x8E) += 6;
        U16(m + 0x90) += U16(m + 0x8E);
        if (S16(m + 0x90) > 0x78) U8(m + 2)++;
        break;
    case 2:
        U8(m + 2)++;
        U16(m + 0x8E) = 0; U16(m + 0x90) = 0;
        /* fall through */
    case 3: {
        u16 w = U16(m + 0x90);
        Menu_Line(w, 0x78, 0x140 - w, 0x78, 0x808080, 1, 1);
        Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0xC, 0);
        Gpu_AddPrim(0x87BC04 + (U32(0x87DE0C) << 12), U32(0x87DC98));
        U32(0x87DC98) += 0xC;
        U16(m + 0x8E) += 4;
        U16(m + 0x90) += U16(m + 0x8E);
        if (S16(m + 0x90) > 0xA0) { U8(m) = 3; U8(m + 1) = 0; U8(m + 2) = 0; U8(m + 3) = 0; }
        break; }
    default:
        return;
    }
    g = G();
    U8(g + 0xB58) = 0xA0; U8(g + 0xB59) = 0xA0; U8(g + 0xB5A) = 0xA0;
    U8(g + 0xB5B) = 0; U8(g + 0xB5C) = 0;
    DrawSync(0);
}

/* mode 3: back to the game (the weapon set if changed, the room's camera if
 * a door screen was loaded, flags, the game task) */
void DC2CALL MenuMode_Leave(gptr m)                                 /* 0x492330 */
{
    gptr g;
    switch ((s8)U8(m + 1)) {
    case 0:
        Cutscene_End();
        while (File_IsBusy()) Task_Sleep(1);
        U8(m + 1)++;
        return;
    case 1:
        break;
    default:
        return;
    }
    g = G();
    u16 w = U16(g + 0x10B4);
    if (U16(m + 0x46) != w) Stage_Load(w & 0xFF, w >> 8);
    g = G();
    if (U8(g + 0xB48) && U16(g + 0x1090) != 0x905) {
        U8(g + 0xB49) = 1;
        u8 cam = U8(g + 0x7C4);
        U8(g + 0x7C4) = U8(g + 0x7C6);
        U8(G() + 0x84B) = 0xFF;
        Camera_Switch(cam);
        Camera_Load();
    }
    Fade_Set(1, 1, 1);
    Task_Sleep(1);
    g = G();
    U8(g + 0xB48) = 0;
    U32(0x87DE00) = U32(m + 0x6C);
    if (U8(G() + 0x1091) == 7) {
        g = G();
        U8(g + 0xB58) = 0x70; U8(g + 0xB59) = 0x70; U8(g + 0xB5A) = 0x70;
        U8(g + 0xB5B) = 0; U8(g + 0xB5C) = 0;
    }
    Flag_Set(3, 0, 1);
    Flag_Set(2, 8, 0);
    if (U8(G() + 0xB4A) != 0xFF) Flag_Set(9, (u32)(s32)(s8)U8(m + 0x53), 0);
    else if (U8(m + 0x64) != 4) Fade_Set(0, 0xF, 1);
    U8(m + 0x64) = 0;
    U16(m + 0x38) = 0; U16(m + 0x3A) = 0; U16(m + 0x3C) = 0; U16(m + 0x3E) = 0;
    g = G();
    SetGeomScreen(U16(U32(g + 0x7B8) + U8(g + 0x7C4) * 16 + 6));
    Flag_Set(2, 4, (u32)(s32)(s8)U8(G() + 0x14F0));
    Frame_Draw();
    U16(0x87DC08) = 0; U16(0x87DC94) = 0; U16(0x87DDFC) = 0; U16(0x87DC00) = 0;
    Task_Chain(0x4510F0);
}
