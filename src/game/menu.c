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

#define D64(a) (*(double *)MEM(a))
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
        if (U16(m + 0x38) != 0x1F) Item_LoadIcon(U16(m + 0x38));
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
/* ---- screen 1: weapon and sub-weapon choice (sub-steps 0x739D64 by M+2) ---- */

static void screen_bottom(void)
{
    Menu_DrawFrames(0x684A40, 0x60, 0xC4, 0xD, 0x7C8E, 0x80808080, 3);
    Menu_DrawFrames(0x684B60, 0xE8, 0xC4, 0xD, 0x7C8E, 0x80808080, 3);
}

/* a notice over a dimmed screen (text t) with the gauge boxes */
static void notice(gptr m, gptr t)
{
    gptr r = Tmp_Push(8);
    U16(r) = 0; U16(r + 2) = 0; U16(r + 4) = 0x140; U16(r + 6) = 0xF0;
    Menu_Tile(r, 0x404040, 3, 2);
    Tmp_Pop(8);
    Text_Draw(t, Text_Left(t), 0x6C, 0x64, 0x808080, 1, 0);
    Menu_Gauges(m, 0xA0, 0x60);
}

void DC2CALL Screen_Equip(gptr m)                                   /* 0x496930 */
{
    Orig_Call1(U32(0x739D64 + (s8)U8(m + 2) * 4), m);       /* the step (0x739D64) */
    Menu_Header(m);
    Menu_Frame(m);
    if (U8(m + 2) != 7 && U8(m + 2) != 4) Menu_Weapons(m, 0x80, 0x18, 2);
}

/* step 0: the lists cleared (M+0x18.. weapons, M+0x20.. sub-weapons) */
void DC2CALL Equip_Init(gptr m)                                     /* 0x496970 */
{
    U8(m + 2) = 1; U8(m + 3) = 0; U32(m + 4) = 0;
    U8(m + 0x4E) = 0; U16(m + 0x48) = 0; U8(m + 0x4C) = 0; U8(m + 0x5B) = 0; U8(m + 0x5C) = 0;
    U32(m + 0x18) = 0; U32(m + 0x1C) = 0; U32(m + 0x20) = 0; U32(m + 0x24) = 0;
    Menu_Points(0x80, 0x58, 0x808080);
    Menu_PauseList(m);
    Menu_Health(0x10, 0x58);
    Text_Draw(0x724182, 0x20, 0xC5, 0x64, 0x808080, 1, 0);
    screen_bottom();
}

/* step 1: weapon or sub-weapon (M+0x4E); on select the carried ones of the
 * stage's kind are listed (M+0x5B, M+0x5C of them), the ring turned to the
 * one in hand (M+0x4C, M+0x48) */
void DC2CALL Equip_Choose(gptr m)                                   /* 0x496A30 */
{
    u8 sel_w = 0, sel_s = 0;
    if (U8(m + 0x45) & 0xF0) {
        U8(m + 0x4E) ^= 1;
        Se_Play(0, 0);
    } else if (U8(0x87DC00) & 0x40) {
        U8(m + 2) = 8;
        Se_Play(0, 1);
    } else if (U8(0x87DC00) & 0x20) {
        U8(m + 0x5B) = 0; U8(m + 0x5C) = 0;
        gptr it = G() + 0x10BC;
        for (u8 i = 0; i < 0x14; i++, it += 12) {
            if (!U8(it)) continue;
            u8 st = U8(G() + 0x108F);
            u8 mask = st == 0 ? 1 : st == 1 || st == 5 || st == 6 ? 2 : st == 2 ? 4 : 0;
            u8 id = U8(it + 1);
            u16 fl = U16(0x70426A + id * 12);
            if (!(fl & mask)) continue;
            if (fl & 8) {
                if (U8(G() + 0x10B4) == id) sel_w = U8(m + 0x5B);
                U8(m + 0x18 + (s8)U8(m + 0x5B)) = i;
                U8(m + 0x5B)++;
            } else if (fl & 0x10) {
                if ((s32)(U16(G() + 0x10B4) >> 8) == (s32)id - 0x10) sel_s = U8(m + 0x5C);
                U8(m + 0x20 + (s8)U8(m + 0x5C)) = i;
                U8(m + 0x5C)++;
            }
        }
        if (!U8(m + 0x4E)) {
            U8(m + 0x4C) = sel_w; U8(m + 2) = 2;
            U16(m + 0x48) = (u16)-(((0x1000 / (s8)U8(m + 0x5B)) & 0xFFF) * (s8)sel_w);
        } else {
            U8(m + 0x4C) = sel_s; U8(m + 2) = 5;
            U16(m + 0x48) = (u16)-(((0x1000 / (s8)U8(m + 0x5C)) & 0xFFF) * (s8)sel_s);
        }
        U8(m + 3) = 0; U32(m + 4) = 0;
        Se_Play(0, 2);
    }
    u32 c0 = U8(m + 0x4E) ? 0x303030 : 0x808080, c1 = U8(m + 0x4E) ? 0x808080 : 0x303030;
    Text_Draw(U8(m + 0x4E) ? 0x7241C6 : 0x724182, 0x20, 0xC5, 0x64, 0x808080, 1, 0);
    Menu_DrawFrames(0x6856A0, 0x71, 0x78, 0xC, 0x7C8F, c0, 3);
    Menu_DrawFrames(0x685088, 0xA0, 0x74, 0xC, 0x7C8E, c0, 3);
    Menu_DrawFrames(0x6856B0, 0x74, 0x88, 0xC, 0x7C8F, c1, 3);
    Menu_DrawFrames(0x685088, 0xA0, 0x84, 0xC, 0x7C8E, c1, 3);
    U16(m + 0x88)++;
    if (S16(m + 0x88) > 0x6C) U16(m + 0x88) = 0;
    gptr r = Tmp_Push(8);
    for (u32 j = 0; j < 2; j++) {
        U16(r) = 0x6F; U16(r + 2) = (u16)((s8)U8(m + 0x4E) * 16 + 0x77); U16(r + 4) = 0x62; U16(r + 6) = 0xA;
        Menu_Border(r, (u16)(U16(m + 0x88) + 0x36 * j), 0x36, 2, 0xFFFF);
    }
    Tmp_Pop(8);
    screen_bottom();
    Menu_Points(0x80, 0x58, 0x808080);
    Menu_PauseList(m);
    Menu_Health(0x10, 0x58);
}

/* the stock arrows either side of the ring (frames 0x6859F0 right, 0x6859E8 left) */
void DC2CALL Equip_ArrowsR(u32 x, u32 y)                            /* 0x497ED0 */
{
    x &= 0xFFFF; y &= 0xFFFF;
    Menu_DrawFrames(0x6859F0, x, y, 0x2C, 0x7C8F, Color_Pulse(0x800060A0, 0x80000840, 4), 3);
    Menu_DrawFrames(0x6859F0, x + 4, y, 0x2C, 0x7C8F, Color_Pulse(0x80004080, 0x80000840, 4), 3);
    Menu_DrawFrames(0x6859F0, x + 8, y, 0x2C, 0x7C8F, Color_Pulse(0x80002060, 0x80000840, 4), 3);
}
void DC2CALL Equip_ArrowsL(u32 x, u32 y)                            /* 0x497F80 */
{
    x &= 0xFFFF; y &= 0xFFFF;
    Menu_DrawFrames(0x6859E8, x - 8, y, 0x2C, 0x7C8F, Color_Pulse(0x800060A0, 0x80000840, 4), 3);
    Menu_DrawFrames(0x6859E8, x - 0xC, y, 0x2C, 0x7C8F, Color_Pulse(0x80004080, 0x80000840, 4), 3);
    Menu_DrawFrames(0x6859E8, x - 0x10, y, 0x2C, 0x7C8F, Color_Pulse(0x80002060, 0x80000840, 4), 3);
}

/* one weapon of the ring at x, y: its picture (lit if sel), its ammunition */
void DC2CALL Equip_RingItem(gptr m, u32 x, u32 y, u32 sel, u32 idx, u32 color, u32 z) /* 0x498030 */
{
    (void)m;
    gptr rec = G() + ((idx & 0xFFFF) + 0x165) * 12;
    u8 id = U8(rec + 1);
    int wide = !(U8(0x70426A + id * 12) & 0x40);
    u32 zl = z & 0xFFFF;
    if ((u16)sel)
        Menu_DrawFrames(wide ? 0x685178 : 0x685108, x, y, 0xC, 0x7C8E,
                        Color_Pulse(0xC0C0C0, 0x606060, 5) | 0x80000000, zl + 1);
    gptr r = Tmp_Push(8);
    U16(r) = (u16)(x - (wide ? 0x40 : 0x1E)); U16(r + 2) = (u16)(y + 0xC);
    U16(r + 4) = wide ? 0x80 : 0x40; U16(r + 6) = 0x20;
    u32 tp = 0x8D;
    if (!wide && id == 0x19) { Menu_Sprite(r, 0x40, 0x40, 0x7F00, color, 0, z); tp = 0x9B; }
    else Menu_Sprite(r, U8(0x704268 + id * 12), U8(0x704269 + id * 12), 0x7C80, color, 0, z);
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, tp, 0);
    Gpu_AddPrim(0x87BC00 + 4 * ((U32(0x87DE0C) << 10) + zl), U32(0x87DC98));
    U32(0x87DC98) += 0xC;
    if (U8(0x70426A + U8(rec + 1) * 12) & 0x80) { Tmp_Pop(8); return; }
    if (U8(G() + 0x1094) & 1) {
        Menu_DrawFrames(0x685CC8, x - 0x19, y + 4, 0xC, 0x7C8F, 0x707070, 0x64);
        Menu_DrawFrames(0x685CC8, x + 0xA, y + 4, 0xC, 0x7C8F, 0x7070, 0x64);
    } else {
        Menu_Number8(x - 0x21, y + 4, 4, 0, color, (u32)(s32)S16(rec + 8), z - 1);
        Menu_Number8(x + 2, y + 4, 4, 0, color & 0xFF00FFFF, (u32)(s32)S16(rec + 0xA), z - 1);
    }
    U16(r) = (u16)(x - 5); U16(r + 2) = (u16)(y + 4); U16(r + 4) = 8; U16(r + 6) = 8;
    Menu_Sprite(r, 0x50, 0x10, 0x7C8E, color, 0, z);
    Tmp_Pop(8);
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0xC, 0);
    Gpu_AddPrim(0x87BC00 + 4 * ((U32(0x87DE0C) << 10) + zl), U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}

/* the ring of weapons (main 1) or sub-weapons: M+0x48 turns towards the one
 * chosen (M+0x4C); each drawn where the GTE puts it, the chosen one named */
void DC2CALL Equip_Ring(gptr m, u32 main)                           /* 0x497AE0 */
{
    Menu_DrawFrames(main == 1 ? 0x6856A0 : 0x6856B0, 0x18, 0x5C, 0xC, 0x7C8F, 0x808080, 2);
    u8 n = main == 1 ? U8(m + 0x5B) : U8(m + 0x5C);
    gptr list = m + (main == 1 ? 0x18 : 0x20);
    s32 step = 0x1000 / (s32)n;
    s32 target = -(s32)(((u32)step & 0xFFF) * (u32)(u16)(s16)(s8)U8(m + 0x4C));
    u32 d = Angle_Approach(U16(m + 0x48), (u32)(step >> 4) & 0xFF, (u32)target);
    if ((((u32)target - U16(m + 0x48)) & 0xFFF) == 0x800 && (U32(0x87DDFC) & 0x8000)) d = (u32)-(s32)d;
    U16(m + 0x48) += (u16)d;
    gptr f = Tmp_Push(0x78);                 /* the original's frame: E - 0x78 */
    gptr vec = f + 0x10, sv = f + 0x18, sxy = f + 0x20, quad = f + 0x28, m1 = f + 0x38, m2 = f + 0x58;
    U16(vec) = 0x8D0; U16(vec + 2) = U16(m + 0x48); U16(vec + 4) = 0;
    RotMatrix(vec, m1);
    S32(m1 + 0x14) = 0; S32(m1 + 0x18) = -40; S32(m1 + 0x1C) = 0x401;
    U16(vec + 4) = 0; U16(vec + 2) = 0; U16(vec) = 0;
    if (!(u16)d && n > 1) { Equip_ArrowsR(0xC8, 0xA8); Equip_ArrowsL(0x78, 0xA8); }
    for (u32 i = 0; i < n; i++, list++) {
        RotMatrix(vec, m2);
        U16(sv) = 0; U16(sv + 2) = 0; U16(sv + 4) = 0x1B8;
        ApplyMatrixSV(m2, sv, sv);
        SetRotMatrix(m1);
        SetTransMatrix(m1);
        s32 z = RotTransPers(sv, sxy, f + 0xC, f + 4);
        U16(vec + 2) = (u16)((U32(vec + 2) + (u32)step) & 0xFFF);
        u32 v = 0xA0 - (u32)z / 3;
        Equip_RingItem(m, (u32)(s32)F32(sxy), (u32)(s32)F32(sxy + 4), (s32)i == (s8)U8(m + 0x4C),
                       U8(list), v << 8 | v << 16 | v, (u32)z);
        if ((s32)i == (s8)U8(m + 0x4C)) {
            gptr rec = G() + (U8(list) + 0x165) * 12;
            static const u16 q[8] = {0x18, 0xC8, 0x60, 0xC8, 0x18, 0xD8, 0x60, 0xD8};
            for (int k = 0; k < 8; k++) U16(quad + 2 * k) = q[k];
            Menu_PolyG4(quad, 0x803000, 0x201000, 0x803000, 0x201000, 1, 0x64);
            Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0xC, 0);
            Gpu_AddPrim(0x87BD90 + (U32(0x87DE0C) << 12), U32(0x87DC98));
            U32(0x87DC98) += 0xC;
            Text_Draw(U32(0x704260 + U8(rec + 1) * 12), 0x20, 0xC9, 0x64, 0x808080, 1, 0);
        }
    }
    Tmp_Pop(0x78);
    Menu_DrawFrames(0x684A40, 0x60, 0xC8, 0xD, 0x7C8E, 0x80808080, 3);
    Menu_DrawFrames(0x684B60, 0xE8, 0xC8, 0xD, 0x7C8E, 0x80808080, 3);
    Menu_DrawFrames(0x685278, 0x88, 0x5D, 0xC, 0x7C8E, 0x80808080, 3);
    Menu_DrawFrames(0x685328, 0x100, 0x65, 0xC, 0x7C8E, 0x80808080, 3);
    Menu_DrawFrames(0x6853C8, 0x46, 0xBC, 0xC, 0x7C8F, 0x1080F0, 5);
    Menu_DrawFrames(0x6853D0, 0xD0, 0xBC, 0xC, 0x7C8F, 0x1080F0, 5);
}

/* "equip" / "check" under the ring (M+0x4E) */
void DC2CALL Equip_Options(gptr m)                                  /* 0x4985C0 */
{
    u32 c0 = U8(m + 0x4E) ? 0x303030 : 0x808080, c1 = U8(m + 0x4E) ? 0x808080 : 0x303030;
    Text_Draw(0x7242AC, 0x2E, 0xA0, 0x64, c0, 3, 0);
    Text_Draw(0x7242B8, 0x2E, 0xB2, 0x64, c1, 3, 0);
    Menu_DrawFrames(0x6850C8, 0x46, 0xA0, 0xC, 0x7C8E, c0, 4);
    Menu_DrawFrames(0x6850C8, 0x46, 0xB2, 0xC, 0x7C8E, c1, 4);
    U16(m + 0x88)++;
    if (S16(m + 0x88) > 0x44) U16(m + 0x88) = 0;
    gptr r = Tmp_Push(8);
    for (u32 j = 0; j < 2; j++) {
        U16(r) = 0x2E; U16(r + 2) = (u16)((s8)U8(m + 0x4E) * 18 + 0xA0); U16(r + 4) = 0x30; U16(r + 6) = 0x10;
        Menu_Border(r, (u16)(U16(m + 0x88) + 0x22 * j), 0x22, 2, 0xFFFF);
    }
    Tmp_Pop(8);
}

/* a weapon's figures (main 1, or a sub-weapon): power, speed, range as a
 * triangle growing with M+0x92, its name by M+0x94 letters */
void DC2CALL Equip_Stats(gptr m, u32 main)                          /* 0x4976C0 */
{
    u8 idx = main == 1 ? U8(m + 0x18 + (s8)U8(m + 0x4C)) : U8(m + 0x20 + (s8)U8(m + 0x4C));
    u32 id = U8(G() + idx * 12 + 0x10BD);
    Menu_Panels();
    Menu_DrawFrames(0x685018, 0x18, 0x60, 0xC, 0x7C8F, 0x808080, 2);
    Menu_DrawFrames(0x6836D8, 0x18, 0x5A, 0xD, 0x7C8E, 0x808080, 1);
    Menu_DrawFrames(0x6851D8, 0x46, 0x78, 0xC, 0x7C8E, 0x80808080, 0x1F4);
    static const u8 L[6][4] = {{0x46, 0x82, 0x23, 0xBE}, {0x23, 0xBE, 0x69, 0xBE}, {0x69, 0xBE, 0x46, 0x82},
                               {0x46, 0xAA, 0x46, 0x82}, {0x46, 0xAA, 0x23, 0xBE}, {0x46, 0xAA, 0x69, 0xBE}};
    for (int k = 0; k < 6; k++) Menu_Line(L[k][0], L[k][1], L[k][2], L[k][3], k < 3 ? 0x68C040 : 0x206020, 0, 2);
    U16(m + 0x92) += 2;
    if (S16(m + 0x92) > 0x64) U16(m + 0x92) = 0x64;
    s32 t = S16(m + 0x92);
    s32 a = U8(0x7041F8 + id * 4) * t, b = U8(0x7041F9 + id * 4) * t, c = U8(0x7041FA + id * 4) * t;
    gptr v = Tmp_Push(0x10);
    U16(v) = 0x46; U16(v + 2) = (u16)(0xAA - a * 40 / 10000);
    U16(v + 4) = (u16)(0x46 - b * 35 / 10000); U16(v + 6) = (u16)(0xAA + b * 20 / 10000);
    U16(v + 8) = (u16)(0x46 + c * 35 / 10000); U16(v + 0xA) = (u16)(0xAA + c * 20 / 10000);
    U16(v + 0xC) = 0; U16(v + 0xE) = 0;
    Menu_PolyG3(v, 0x802020C0, 0x20C020, 0xC02020, 0x12C);
    Tmp_Pop(0x10);
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0x2C, 0);
    Gpu_AddPrim(0x87C0B0 + (U32(0x87DE0C) << 12), U32(0x87DC98));
    U32(0x87DC98) += 0xC;
    Menu_DrawFrames(0x685418, 0x28, 0x78, 0xC, 0x7C8F, 0x4040C0, 2);
    Menu_Number8(0x46, 0x82, 3, 0, 0x808080, (u32)(a / 100), 0x64);
    Menu_DrawFrames(0x685420, 0x10, 0xC0, 0xC, 0x7C8F, 0x40C040, 2);
    Menu_Number8(0x1C, 0xCA, 3, 0, 0x808080, (u32)(b / 100), 0x64);
    Menu_DrawFrames(0x685428, 0x44, 0xC0, 0xC, 0x7C8F, 0xC04040, 2);
    Menu_Number8(0x60, 0xCA, 3, 0, 0x808080, (u32)(c / 100), 0x64);
    U16(m + 0x94)++;
    if (S16(m + 0x94) > 0x64) U16(m + 0x94) = 0x64;
    Text_Draw(U32(0x704264 + id * 12), 0x82, 0x7B, U16(m + 0x94), 0x808080, 1, 1);
}

/* the ring turned with left/right (M+3 frames of the turn); main: cancel
 * back (or to step 9 with no weapon), select to step 3; sub: cancel back to
 * step 1 on sub-weapons, select to step 6 */
static void ring_step(gptr m, u32 main)
{
    u8 n = U8(m + (main ? 0x5B : 0x5C));
    if (U8(m + 3)) { U8(m + 3)--; Equip_Ring(m, main); return; }
    u16 k = U16(m + 0x44);
    if (k & 0x8000) {
        if ((s8)n > 1) Se_Play(0, 8);
        U8(m + 3) = 0x10;
        if ((s8)--U8(m + 0x4C) < 0) U8(m + 0x4C) = (u8)(n - 1);
    } else if (k & 0x2000) {
        if ((s8)n > 1) Se_Play(0, 8);
        U8(m + 3) = 0x10;
        U8(m + 0x4C)++;
        if ((s8)U8(m + 0x4C) > (s8)n - 1) U8(m + 0x4C) = 0;
    } else if (U8(0x87DC00) & 0x40) {
        if (main) {
            Se_Play(0, 1);
            U8(m + 2) = U8(G() + 0x10B4) ? 1 : 9;
            U8(m + 0x4E) = 0;
        } else {
            U16(m + 0x48) = 0; U8(m + 0x4C) = 0;
            Se_Play(0, 1);
            U8(m + 2) = 1; U8(m + 0x4E) = 1;
        }
    } else if (U8(0x87DC00) & 0x20) {
        Se_Play(0, 2);
        U8(m + 2) = main ? 3 : 6; U8(m + 0x4E) = 0;
    }
    Equip_Ring(m, main);
}
void DC2CALL Equip_WeaponRing(gptr m) { ring_step(m, 1); }          /* 0x496D90 */
void DC2CALL Equip_SubRing(gptr m) { ring_step(m, 0); }             /* 0x497210 */

/* equip or check the weapon chosen (M+0x4E); a two-handed one clears the
 * sub-weapon, a one-handed after a two-handed gets the stage's default */
void DC2CALL Equip_WeaponSel(gptr m)                                /* 0x496EB0 */
{
    if (U8(m + 0x45) & 0x50) {
        Se_Play(0, 0);
        U8(m + 0x4E) ^= 1;
    } else if (U8(0x87DC00) & 0x40) {
        Se_Play(0, 1);
        U8(m + 2) = 2; U8(m + 3) = 0; U32(m + 4) = 0;
    } else if (U8(0x87DC00) & 0x20) {
        Se_Play(0, 2);
        if (U8(m + 0x4E)) {
            while (File_IsBusy()) Task_Sleep(1);
            Item_LoadIcon(U8(G() + U8(m + 0x18 + (s8)U8(m + 0x4C)) * 12 + 0x10BD));
            U8(m + 2) = 4; U16(m + 0x94) = 0;
        } else {
            gptr g = G();
            u8 id = U8(g + U8(m + 0x18 + (s8)U8(m + 0x4C)) * 12 + 0x10BD);
            u16 w = U16(g + 0x10B4);
            if (U8(0x70426A + id * 12) & 0x20) U16(g + 0x10B4) = id;
            else if (U8(0x70426A + (w & 0xFF) * 12) & 0x20) {
                switch (U8(g + 0x108F)) {
                case 0: U16(G() + 0x10B4) = (u16)(id | (Flag_Check(9, 0x38) ? 0x800 : 0x200)); break;
                case 1: U16(G() + 0x10B4) = (u16)(id | (Flag_Check(9, 0x37) ? 0x700 : 0x100)); break;
                case 5: case 6: U16(g + 0x10B4) = (u16)(id | 0x600); break;
                case 2: U16(g + 0x10B4) = (u16)(id | 0x500); break;
                }
            } else U16(g + 0x10B4) = (u16)((w & 0xFF00) | id);
            U8(m + 2) = 2;
        }
        U16(m + 0x92) = 0; U32(m + 4) = 0;
    }
    Equip_Ring(m, 1);
    Equip_Options(m);
}

/* the same for the sub-weapon: equipping one sets G+0x10B5 (id - 0x10) */
void DC2CALL Equip_SubSel(gptr m)                                   /* 0x497310 */
{
    if (U8(m + 0x45) & 0x50) {
        Se_Play(0, 0);
        U8(m + 0x4E) ^= 1;
    } else if (U8(0x87DC00) & 0x40) {
        Se_Play(0, 1);
        U8(m + 2) = 5; U8(m + 3) = 0; U32(m + 4) = 0;
    } else if (U8(0x87DC00) & 0x20) {
        Se_Play(0, 2);
        if (U8(m + 0x4E)) {
            while (File_IsBusy()) Task_Sleep(1);
            Item_LoadIcon(U8(G() + U8(m + 0x20 + (s8)U8(m + 0x4C)) * 12 + 0x10BD));
            U8(m + 2) = 7; U16(m + 0x94) = 0;
        } else {
            gptr g = G();
            u16 w = U16(g + 0x10B4);
            u8 id = U8(g + U8(m + 0x20 + (s8)U8(m + 0x4C)) * 12 + 0x10BD);
            if (U8(0x70426A + (w & 0xFF) * 12) & 0x20) U16(g + 0x10B4) = (u16)((id + 0xF0) << 8);
            else U16(g + 0x10B4) = (u16)(((id + 0xF0) << 8) | (w & 0xFF));
            U8(m + 2) = U8(G() + 0x10B4) ? 5 : 9;
        }
        U16(m + 0x92) = 0; U32(m + 4) = 0;
    }
    Equip_Ring(m, 0);
    Equip_Options(m);
}

/* checking a weapon: its picture once loaded; cancel back to the ring */
static void view_step(gptr m, u32 main)
{
    switch ((s8)U8(m + 3)) {
    case 0:
        U8(m + 0x4E) = 0;
        if (!File_IsBusy()) U8(m + 3)++;
        break;
    case 1: {
        gptr r = Tmp_Push(8);
        U16(r) = 0x98; U16(r + 2) = 0x28; U16(r + 4) = 0x80; U16(r + 6) = 0x40;
        Menu_Sprite(r, 0, 0xC0, 0x7C40, 0x808080, 0, 1);
        Tmp_Pop(8);
        Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0x8C, 0);
        Gpu_AddPrim(0x87BC04 + (U32(0x87DE0C) << 12), U32(0x87DC98));
        U32(0x87DC98) += 0xC;
        if (U8(0x87DC00) & 0x40) {
            Se_Play(0, 1);
            U8(m + 2) = main ? 2 : 5; U8(m + 3) = 0;
        }
        break; }
    }
    Equip_Stats(m, main);
}
void DC2CALL Equip_WeaponView(gptr m) { view_step(m, 1); }          /* 0x497120 */
void DC2CALL Equip_SubView(gptr m) { view_step(m, 0); }             /* 0x497480 */

/* back to the pause menu (no weapon: step 9) */
void DC2CALL Equip_Back(gptr m)                                     /* 0x497570 */
{
    if (U8(G() + 0x10B4)) { U8(m + 2) = 0; U8(m + 1) = 0; }
    else U8(m + 2) = 9;
    U8(m + 3) = 0; U32(m + 4) = 0;
    Menu_Points(0x80, 0x58, 0x808080);
    Menu_PauseList(m);
    Menu_Health(0x10, 0x58);
    screen_bottom();
    Text_Draw(0x724146, 0x20, 0xC5, 0x64, 0x707070, 0x64, 0);
}

/* no weapon to equip: a dimmed screen and a notice; either button back to step 2 */
void DC2CALL Equip_NoWeapon(gptr m)                                 /* 0x497620 */
{
    U16(m + 0x48) = 0; U8(m + 0x4C) = 0;
    if (U8(0x87DC00) & 0x40) { Se_Play(0, 1); U8(m + 2) = 2; }
    else if (U8(0x87DC00) & 0x20) { Se_Play(0, 2); U8(m + 2) = 2; }
    notice(m, 0x724314);
}

/* ---- screen 2: items and files (steps 0x739D8C by M+2) ---- */

/* the pause screen's parts under screen 2 and its line of text */
static void items_common(gptr m, u32 text)
{
    Menu_Header(m);
    Menu_Weapons(m, 0x80, 0x18, 2);
    Menu_Health(0x10, 0x58);
    Menu_Points(0x80, 0x58, 0x808080);
    Menu_PauseList(m);
    screen_bottom();
    Text_Draw(text, 0x20, 0xC5, 0x64, 0x808080, 0x64, 0);
}

/* M+0x5D items held (G+0x11AC, 6), M+0x50 files found (flags 9:0x20..0x4F) */
void DC2CALL Screen_Items(gptr m)                                   /* 0x498710 */
{
    U8(m + 0x5D) = (u8)Item_CountUsed(G() + 0x11AC, 6);
    U8(m + 0x50) = (u8)Flag_CountRange(0x20, 0x50);
    Orig_Call1(U32(0x739D8C + (s8)U8(m + 2) * 4), m);
    Menu_Frame(m);
}

/* step 0 (opened by flag 2:0x20: straight to the items) */
void DC2CALL Items_Init(gptr m)                                     /* 0x498750 */
{
    U8(m + 2) = U8(m + 0x64) == 5 ? 2 : 1;
    U8(m + 3) = 0; U32(m + 4) = 0;
    U8(m + 0x4E) = 0; U8(m + 0x5A) = 0; U8(m + 0x4F) = 0; U8(m + 0x51) = 0; U8(m + 0x52) = 0; U8(m + 0x53) = 0;
    U16(m + 0x92) = 0x1E;
    U32(m + 0x28) = 0; U32(m + 0x2C) = 0;
    if (U8(m + 0x64) != 5) items_common(m, 0x72423C);
}

/* step 1: items or files (M+0x5A); the files listed from the page M+0x52 */
void DC2CALL Items_Choose(gptr m)                                   /* 0x498840 */
{
    if (U8(m + 0x45) & 0xF0) {
        Se_Play(0, 0);
        U8(m + 0x5A) ^= 1;
    } else if (U8(0x87DC00) & 0x40) {
        Se_Play(0, 1);
        U8(m + 2) = 0xB; U8(m + 3) = 0; U32(m + 4) = 0;
    } else if (U8(0x87DC00) & 0x20) {
        Se_Play(0, 2);
        if (!U8(m + 0x5A)) {
            if (U8(m + 0x5D)) { U8(m + 2) = 2; U16(m + 0x92) = 0x1E; }
            else U8(m + 2) = 5;
        } else if (U8(m + 0x50)) {
            u8 k = U8(m + 0x52), b = 0x20;
            for (;;) {
                if (Flag_Check(9, b) && (s8)--k < 0) break;
                if (++b >= 0x50) break;
            }
            Flag_ListRange(b, 0x50, 6, m + 0x28);
            U8(m + 2) = 6;
        } else U8(m + 2) = 9;
        U8(m + 3) = 0; U32(m + 4) = 0; U8(m + 0x4F) = 0; U8(m + 0x52) = 0; U8(m + 0x4E) = 0; U16(m + 0x94) = 0;
    }
    u32 ca = U8(m + 0x5A) ? 0x80303030 : 0x808080, cb = U8(m + 0x5A) ? 0x808080 : 0x80303030;
    Menu_DrawFrames(0x685408, 0x8B, 0x97, 0xC, 0x7C8F, cb, 2);
    Menu_DrawFrames(0x6850C8, 0xA8, 0x93, 0xC, 0x7C8E, cb, 3);
    Menu_DrawFrames(0x684FE0, 0x87, 0x87, 0xC, 0x7C8F, ca, 2);
    Menu_DrawFrames(0x6850C8, 0xA8, 0x83, 0xC, 0x7C8E, ca, 3);
    U16(m + 0x88)++;
    if (S16(m + 0x88) > 0x4E) U16(m + 0x88) = 0;
    gptr r = Tmp_Push(8);
    for (u32 j = 0; j < 2; j++) {
        U16(r) = 0x86; U16(r + 2) = (u16)((s8)U8(m + 0x5A) * 16 + 0x86); U16(r + 4) = 0x44; U16(r + 6) = 0xA;
        Menu_Border(r, (u16)(U16(m + 0x88) + 0x27 * j), 0x27, 2, 0xFFFF);
    }
    Tmp_Pop(8);
    items_common(m, U8(m + 0x5A) ? 0x724264 : 0x72423C);
}

/* step 2: the items held (M+0x4F the one chosen); its name by M+0x94 letters */
void DC2CALL Items_List(gptr m)                                     /* 0x498B40 */
{
    s8 old = (s8)U8(m + 0x4F);
    u16 k = U16(m + 0x44);
    if (k & 0x1000) {
        if ((s8)--U8(m + 0x4F) < 0) U8(m + 0x4F) = (u8)(U8(m + 0x5D) - 1);
    } else if (k & 0x4000) {
        U8(m + 0x4F)++;
        if ((s8)U8(m + 0x4F) > (s8)U8(m + 0x5D) - 1) U8(m + 0x4F) = 0;
    } else if (U8(0x87DC00) & 0x40) {
        Se_Play(0, 1);
        if (U8(m + 0x64) == 5) { U32(m) = 0x502; U8(m + 0x64) = 0; }
        else U8(m + 2) = 1;
    } else if (U8(0x87DC00) & 0x20) {
        Se_Play(0, 2);
        U8(m + 2) = 3; U8(m + 0x4E) = 0;
        if (U16(m + 0x92)) U16(m + 0x92) = 1;
    }
    if ((s32)(s8)U8(m + 0x4F) != (s32)(u16)(s16)old) {
        Se_Play(0, 8);
        U16(m + 0x92) = 0x1E; U16(m + 0x94) = 0;
    }
    Items_Draw(m);
    U16(m + 0x94)++;
    if (S16(m + 0x94) > 0x64) U16(m + 0x94) = 0x64;
    u8 id = U8(G() + (s8)U8(m + 0x4F) * 12 + 0x11AD);
    Text_Draw(U32(0x704264 + id * 12), 0x84, 0x9C, U16(m + 0x94), id == 0x1E ? 0x3030B0 : 0x808080, 1, 1);
}

/* step 3: use the item? (M+0x4E: 0 yes) the medicines heal (0x1B half,
 * 0x1C full), cure poison (0x1A) or both (0x1D..0x1F); one is used up */
void DC2CALL Items_Use(gptr m)                                      /* 0x498C80 */
{
    if (U8(m + 0x45) & 0xA0) {
        Se_Play(0, 0);
        U8(m + 0x4E) ^= 1;
    } else if (U8(0x87DC00) & 0x40) {
        Se_Play(0, 1);
        U8(m + 2) = 2;
    } else if (U8(0x87DC00) & 0x20) {
        Se_Play(0, 2);
        U8(m + 2) = 2; U8(m + 3) = 0;
        if (!U8(m + 0x4E)) {
            gptr rec = G() + ((s8)U8(m + 0x4F) + 0x179) * 12, pl = U32(G() + 0x940);
            U8(m + 2) = 4;
            switch (U8(rec + 1)) {
            case 0x1B: U16(pl + 0x108) += (u16)(S16(pl + 0x10A) / 2); break;
            case 0x1C: U16(pl + 0x108) = U16(pl + 0x10A); break;
            case 0x1D: case 0x1E: case 0x1F: U16(pl + 0x108) = U16(pl + 0x10A); /* fall through */
            case 0x1A: U16(pl + 0x17E) = 0; break;
            }
            if (S16(pl + 0x108) > S16(pl + 0x10A)) U16(pl + 0x108) = U16(pl + 0x10A);
            U16(m + 0xBC) = U16(pl + 0x108);
            if (--S16(rec + 8) <= 0) {
                U32(rec) = 0; U32(rec + 4) = 0; U32(rec + 8) = 0;
                Item_Compact(G() + 0x11AC, 6);
            }
            if (U8(m + 0x64) == 5) { U32(m) = 0x502; U8(m + 0x64) = 0; }
        }
    }
    Items_Draw(m);
    Text_Draw(0x7244E6, 0x88, 0xA4, 0x64, 0x808080, 1, 0);
    Menu_YesNo(0xB4, 0xB4, (u32)(s32)(s8)U8(m + 0x4E), 1);
}

/* the PC's ASCII letters (8x8, widths 0x7373F4 less 0.5, colors 0x7373D4)
 * in their own list (0x87DDE4); not while flag 2:0x17 is set */
void DC2CALL Text_Ascii(u32 x, u32 y, u32 col, gptr s)              /* 0x471B30 */
{
    if (Flag_Check(2, 0x17)) return;
    float x0 = (float)(s32)x;
    long double cx = (s32)x;
    for (u8 c; (c = U8(s)) != 0; s++) {
        if (c == '\n') { y += 8; cx = x0; continue; }
        if (c == ' ') { cx += F32(0x703458); continue; }
        s32 k = (s32)c - 0x21;
        gptr p = U32(0x87DC98);
        F32(p + 8) = (float)cx; F32(p + 0xC) = (float)(s32)y;
        U32(p + 0x10) = 0x3C23D70A;
        U16(p + 0x18) = 8; U16(p + 0x1A) = 8;
        U8(p + 0x14) = (u8)(k << 4); U8(p + 0x15) = (u8)(k & 0xF0);
        U16(p + 0x16) = 0x7DCE;
        U32(p + 4) = U32(0x7373D4 + (col & 0xFF) * 4);
        gptr l = 0x87DDE4 + U32(0x87DE0C) * 12;
        U32(p) = U32(l) | 0x04000000;
        U32(l) = p & 0xFFFFFF;
        U32(0x87DC98) = p + 0x1C;
        cx += (long double)F32(0x7373F4 + k * 4) - D64(0x703460);
    }
}

/* screen 2's item page: the panels, the picture of the item chosen (loaded
 * after M+0x92 frames), the cells, the list of items held with their counts */
void DC2CALL Items_Draw(gptr m)                                     /* 0x499770 */
{
    Menu_Header(m);
    Menu_Health(0x10, 0x58);
    Menu_DrawFrames(0x684FE0, 0x2A, 0x73, 0xC, 0x7C8F, 0x80808080, 2);
    Menu_DrawFrames(0x6836D8, 0x18, 0x6F, 0xD, 0x7C8E, 0x808080, 1);
    Menu_DrawFrames(0x6835C0, 0xD8, 0x18, 0xD, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x6846C8, 0xD8, 0x18, 0xD, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x684FF0, 0xB0, 0x18, 0xC, 0x7C8F, 0x8068C040, 2);
    Menu_DrawFrames(0x683628, 0xD8, 0x93, 0xD, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x6853D8, 0xD8, 0x93, 0xC, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x684528, 0xD8, 0xAC, 0xD, 0x7C8E, 0x808080, 1);
    if (!(u8)Item_CountUsed(G() + 0x11AC, 6)) U16(m + 0x92) = 0x1E;
    gptr r = Tmp_Push(0x10);
    int pic = 0;                             /* the item's picture, or the rings while it loads */
    if (S16(m + 0x92) > 0) {
        if (--S16(m + 0x92) <= 0) {
            while (File_IsBusy()) Task_Sleep(1);
            s32 k = (s8)U8(m + 0x4F);
            u8 id = U8(G() + k * 12 + 0x11AD);
            if (id != 0x1F) Item_LoadIcon(id);
        }
    } else if (!File_IsBusy()) {
        pic = 1;
        U16(r) = 8; U16(r + 2) = 0x8C; U16(r + 4) = 0x80; U16(r + 6) = 0x40;
        Menu_Sprite(r, 0, 0xC0, 0x7C40, 0x808080, 0, 1);
        Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0x8C, 0);
        Gpu_AddPrim(0x87BC04 + (U32(0x87DE0C) << 12), U32(0x87DC98));
        U32(0x87DC98) += 0xC;
        Menu_DrawFrames(0x6851D8, 0x45, 0x80, 0xC, 0x7C8E, 0x808080, 0x64);
    }
    if (!pic) Items_Panel(m);
    u32 a = (U8(G() + 4) >> 2 & 1) ^ 1, c = U8(G() + 4) & 2 ? 0 : 2;
    U16(r + 4) = 0x2B; U16(r + 6) = 0x16;
    for (u16 y = 0x9C; y < 0xC8; y += 0x16)
        for (u16 x = 0x80; x < 0x12C; x += 0x2B) {
            U16(r) = x; U16(r + 2) = y;
            Menu_Sprite(r, c + 0xB0, a + 0x80, 0x7C8E, 0x3080, 1, 0x64);
            Menu_Sprite(r, a + 0xB2, c + 0x82, 0x7C8E, 0x402000, 1, 0x64);
        }
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0xC, 0);
    Gpu_AddPrim(0x87BD90 + (U32(0x87DE0C) << 12), U32(0x87DC98));
    U32(0x87DC98) += 0xC;
    Menu_DrawFrames(0x683E38, 0xD8, 0xCD, 0xD, 0x7C8E, 0x808080, 1);
    for (u32 k = 0; k < 6; k++) Menu_DrawFrames(0x6843E8, 0xD8, 0x20 + 0x13 * k, 0xD, 0x7C8E, 0x80808080, 2);
    a = (U8(G() + 4) >> 2 & 1) ^ 1; c = U8(G() + 4) & 2 ? 0 : 2;
    U16(r + 4) = 0x28; U16(r + 6) = 0x13;
    for (u16 y = 0x20; y < 0x92; y += 0x13)
        for (u16 x = 0x88; x < 0x128; x += 0x28) {
            U16(r) = x; U16(r + 2) = y;
            Menu_Sprite(r, c + 0xB0, a + 0x80, 0x7C8E, 0x3080, 1, 0x64);
            Menu_Sprite(r, a + 0xB2, c + 0x82, 0x7C8E, 0x402000, 1, 0x64);
        }
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0xC, 0);
    Gpu_AddPrim(0x87BD90 + (U32(0x87DE0C) << 12), U32(0x87DC98));
    U32(0x87DC98) += 0xC;
    U16(r) = 0x89; U16(r + 2) = (u16)((s8)U8(m + 0x4F) * 19 + 0x20); U16(r + 4) = 0x9D; U16(r + 6) = 0x13;
    Menu_Tile(r, 0x358533, 0, 0x320);
    Tmp_Pop(0x10);
    gptr buf = Tmp_Push(0x10), rec = G() + 0x11AC;
    u32 row = 0, ty = 0x22;
    for (u16 i = 0; (s32)i < (s8)U8(m + 0x5D); i++, rec += 12) {
        if (!U8(rec)) continue;
        if (U8(rec) == 1) {
            Text_Draw(U32(0x704260 + U8(rec + 1) * 12), 0x94, ty, 0x64, 0x808080, 1, 0);
            Crt_sprintf3(buf, 0x71D738, (u32)(s32)S16(rec + 8), (u32)(s32)S16(rec + 0xA), 0);
            Text_Ascii(0x104, (row & 0xFFFF) * 19 + 0x28, 0xFF00, buf);
        }
        row++; ty += 0x13;
    }
    Tmp_Pop(0x10);
}

/* a ring of 64 flat quads between radii r1 and r2 round x, y, turned by
 * angle about the vertical (via the GTE); every other 16 left out, the
 * color rising 0x800 a quad (slot 0x64) */
void DC2CALL Menu_Ring(u32 angle, u32 x, u32 y, u32 r1, u32 r2, u32 color) /* 0x495410 */
{
    gptr f = Tmp_Push(0x460);                /* r {0, angle, 0}, sv, matrix, the points */
    gptr rv = f + 0x18, sv = f + 0x10, mt = f + 0x28, pin = f + 0x48, pout = f + 0x250;
    x &= 0xFFFF; y &= 0xFFFF;
    U16(rv + 2) = (u16)angle; U16(rv) = 0; U16(rv + 4) = 0;
    s32 d1 = (s32)(r1 * 400), d2 = (s32)((r2 * 5 + 10) * 80);
    for (u32 i = 0; i < 64; i++) {
        U16(sv) = 0; U16(sv + 2) = 0; U16(sv + 4) = (u16)d1;
        RotMatrix(rv, mt);
        ApplyMatrixSV(mt, sv, sv);
        F32(pin + 8 * i) = (float)(S16(sv) / 400 + (s32)x);
        F32(pin + 8 * i + 4) = (float)(S16(sv + 4) / 400 + (s32)y);
        U16(sv + 2) = 0; U16(sv) = 0; U16(sv + 4) = (u16)d2;
        RotMatrix(rv, mt);
        ApplyMatrixSV(mt, sv, sv);
        U16(rv + 2) += 0x40;
        F32(pout + 8 * i) = (float)(S16(sv) / 400 + (s32)x);
        F32(pout + 8 * i + 4) = (float)(S16(sv + 4) / 400 + (s32)y);
    }
    U32(pin + 0x200) = U32(pin); U32(pin + 0x204) = U32(pin + 4);
    for (u32 i = 0; i < 64; i++, color += 0x800) {
        if (i & 0x10) continue;
        gptr p = U32(0x87DC98);
        U32(p + 8) = U32(pin + 8 * i); U32(p + 0x14) = U32(pin + 8 * i + 8);
        U32(p + 0x20) = U32(pout + 8 * i); U32(p + 0x2C) = U32(pout + 8 * i + 8);
        U32(p + 0xC) = U32(pin + 8 * i + 4); U32(p + 0x18) = U32(pin + 8 * i + 0xC);
        U32(p + 0x24) = U32(pout + 8 * i + 4); U32(p + 0x30) = U32(pout + 8 * i + 0xC);
        U32(p + 0x34) = U32(p + 0x28) = U32(p + 0x1C) = U32(p + 0x10) = 0x3C23D70A;
        U32(p + 4) = color | 0x28000000;
        gptr ot = 0x87BD90 + (U32(0x87DE0C) << 12);
        U32(p) = U32(ot) | 0x05000000;
        U32(ot) = p & 0xFFFFFF;
        U32(0x87DC98) = p + 0x38;
    }
    Tmp_Pop(0x460);
}

/* the two turning rings by the item's picture (M+0x70, M+0x72) */
void DC2CALL Items_Panel(gptr m)                                    /* 0x499D00 */
{
    U16(m + 0x70) = (u16)((U16(m + 0x70) + 0x50) & 0xFFF);
    Menu_Ring(U16(m + 0x70), 0x44, 0xAC, 6, 8, 0x800000);
    U16(m + 0x72) = (u16)((U16(m + 0x72) + 0x14) & 0xFFF);
    Menu_Ring(U16(m + 0x72), 0x44, 0xAC, 0xC, 0xE, 0x80);
}

/* the file page's frame: page number (M+0x51 + M+0x52 + 1), the file's title and text */
void DC2CALL Files_Frame(gptr m)                                    /* 0x499D60 */
{
    Menu_DrawFrames(0x6857B0, 0x1C, 0x18, 0xC, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x6857C0, 0x8C, 0x18, 0xC, 0x7C8E, 0x80808080, 3);
    Menu_DrawFrames(0x6857C8, 0x114, 0x28, 0xC, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x6857D0, 0x24, 0x18, 0xC, 0x7C8F, 0x2060C0, 2);
    Menu_DrawFrames(0x6857D8, 0x58, 0x38, 0xC, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x6858D8, 0xE0, 0x38, 0xC, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x685858, 0x58, 0x90, 0xC, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x685958, 0xE0, 0x90, 0xC, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x685408, 0x3C, 0x1D, 0xC, 0x7C8F, 0x808080, 2);
    Menu_Number11(0x78, 0x1B, 2, 1, 0x104080, (u32)((s8)U8(m + 0x52) + (s8)U8(m + 0x51) + 1), 5);
    Text_Draw(U32(0x704260 + (s8)U8(m + 0x53) * 12), 0x48, 0x28, 0x64, 0x808080, 3, 0);
    Text_Draw(U32(0x704264 + (s8)U8(m + 0x53) * 12), 0x20, 0x9F, 0x64, 0x808080, 3, 0);
    Menu_DrawFrames(0x684C60, 0x58, 0xA0, 0xD, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x684D90, 0xE0, 0xA0, 0xD, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x684EC0, 0x58, 0xD2, 0xD, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x684F40, 0xE0, 0xD2, 0xD, 0x7C8E, 0x80808080, 2);
}

/* step 4: after an item was used, the list closes up: each item flashes
 * (+0 | 0x80), Item_Merge, then they settle; back when the health shown caught up */
void DC2CALL Items_Sort(gptr m)                                     /* 0x498E10 */
{
    U8(m + 0x65) = 2;
    switch ((s8)U8(m + 3)) {
    case 0:
        U8(m + 0x4F) = 0; U16(m + 0x96) = 0;
        if (!(u8)Item_CountUsed(G() + 0x11AC, 6)) U8(m + 3) = 5;
        else U8(m + 3)++;
        break;
    case 1:
        if (U16(m + 0x96)) { U16(m + 0x96) = 0; U8(m + 0x4F)++; }
        for (;;) {
            if ((s8)U8(m + 0x4F) >= 6) { U8(m + 3) = 3; U8(m + 0x4F) = (u8)(U8(m + 0x5D) - 1); break; }
            if (U8(G() + ((s8)U8(m + 0x4F) + 0x179) * 12)) { U8(m + 3)++; break; }
            U8(m + 0x4F)++;
        }
        break;
    case 2:
        if (++S16(m + 0x96) > 2) {
            U8(G() + ((s8)U8(m + 0x4F) + 0x179) * 12) |= 0x80;
            U8(m + 3) = 1;
        }
        break;
    case 3:
        U16(m + 0x96) = 0;
        Item_Merge();
        U8(m + 0x4F) = 0;
        U8(m + 3)++;
        break;
    case 4:
        if (++S16(m + 0x96) > 4) {
            U8(G() + ((s8)U8(m + 0x4F) + 0x179) * 12) &= 0x7F;
            U8(m + 0x4F)++;
            U16(m + 0x96) = 0;
            if ((s8)U8(m + 0x4F) >= (s8)U8(m + 0x5D)) { U8(m + 0x4F) = (u8)(U8(m + 0x5D) - 1); U8(m + 3)++; }
        }
        break;
    case 5:
        if ((s32)U16(m + 0x4A) == S16(U32(G() + 0x940) + 0x108)) {
            U16(m + 0x92) = 0x1E; U16(m + 0x96) = 0; U8(m + 0x4F) = 0;
            U8(m + 2) = (u8)Item_CountUsed(G() + 0x11AC, 6) ? 2 : 1;
            U8(m + 3) = 0;
        }
        break;
    }
    Items_Draw(m);
}

/* step 5: no items; either button back (or out, when opened for the items) */
void DC2CALL Items_None(gptr m)                                     /* 0x499040 */
{
    u8 pad = U8(0x87DC00);
    if (pad & 0x60) {
        Se_Play(0, pad & 0x40 ? 1 : 2);
        if (U8(m + 0x64) == 5) { U32(m) = 0x502; U8(m + 0x64) = 0; }
        else U8(m + 2) = 0xB;
    }
    notice(m, 0x7242CC);
}

/* a file id used here: an enabled zone of kind 9 for the player's room
 * part whose quad holds him (or the point 0x190 ahead), its +0x19 the id */
u32 DC2CALL Zone_FileSpot(u32 id)                                   /* 0x48B280 */
{
    gptr g = G(), pl = U32(g + 0x940), z = U32(g + 0xA5C), end = z + 0x500;
    u32 sw = U16(g + 0x10B4) >> 8;
    if (U8(pl + 0x55) == 0xA && (sw == 3 || sw == 6)) return 0;
    for (; z < end; z += 0x28) {
        if (!(U8(z) & 1) || U8(z + 1) != 9 || U8(z + 2) != U16(pl + 0x5A)) continue;
        gptr at = pl + 0x40, sv = 0;
        if (U8(z) & 2) {
            sv = Tmp_Push(8);
            RotMatrix(pl + 0x48, pl + 0x20);
            U16(sv) = 0; U16(sv + 2) = 0; U16(sv + 4) = 0x190;
            ApplyMatrixSV(pl + 0x20, sv, sv);
            U16(sv) += U16(pl + 0x40); U16(sv + 2) += U16(pl + 0x42); U16(sv + 4) += U16(pl + 0x44);
            at = sv;
        }
        u32 in = PointInQuad(at, z + 4);
        if (sv) Tmp_Pop(8);
        if (in && U8(z + 0x19) == (u8)id) return 1;
    }
    return 0;
}

/* the files page (M+0x28.. the six shown from page M+0x52, M+0x51 the cursor) */
void DC2CALL Files_Draw(gptr m)                                     /* 0x499F80 */
{
    if (U8(m + 2) == 6) {
        Menu_DrawFrames(0x6853F0, 0xD6, 0x21, 0xC, 0x7C8F, 0x808080, 2);
        Menu_Line(0xCC, 0x20, 0x100, 0x20, 0x75E533, 1, 4);
        Menu_Line(0xCC, 0x30, 0x100, 0x30, 0x75E533, 1, 4);
        Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0xC, 0);
        Gpu_AddPrim(0x87BC10 + (U32(0x87DE0C) << 12), U32(0x87DC98));
        U32(0x87DC98) += 0xC;
    }
    Menu_DrawFrames(0x6857B0, 0x1C, 0x18, 0xC, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x6857C0, 0x8C, 0x18, 0xC, 0x7C8E, 0x80808080, 3);
    Menu_DrawFrames(0x6857C8, 0x34, 0x28, 0xC, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x6857D0, 0x24, 0x18, 0xC, 0x7C8F, 0x2060C0, 2);
    Menu_DrawFrames(0x685408, 0x3C, 0x1D, 0xC, 0x7C8F, 0x808080, 2);
    Menu_Number11(0x78, 0x1B, 2, 1, 0x104080, (u32)((s8)U8(m + 0x52) + (s8)U8(m + 0x51) + 1), 5);
    Menu_DrawFrames(0x685080, 0x88, 0x24, 0xC, 0x7C8E, 0x80808080, 2);
    for (u32 k = 0; k < 6; k++) {
        u32 v = (u8)(0x80 - (k << 4));
        u32 c = ((((v | 0xFFFF8000u) << 8) | v) << 8) | v;
        Menu_DrawFrames(0x685060, 0x68 + 5 * k, 0x2C, 0xC, 0x7C8E, c, 2);
        Menu_DrawFrames(0x685028, 0xC0 - 5 * k, 0x24, 0xC, 0x7C8E, c, 2);
    }
    Menu_DrawFrames(0x6857D8, 0x58, 0x38, 0xC, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x6858D8, 0xE0, 0x38, 0xC, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x685858, 0x58, 0xAE, 0xC, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x685958, 0xE0, 0xAE, 0xC, 0x7C8E, 0x80808080, 2);
    u8 k = U8(m + 0x52), b = 0x20;
    for (;;) {
        if (Flag_Check(9, b) && (s8)--k < 0) break;
        if (++b >= 0x50) break;
    }
    Flag_ListRange(b, 0x50, 6, m + 0x28);
    u32 n = (s8)U8(m + 0x50) < 6 ? (u32)(s32)(s8)U8(m + 0x50) : 6, i;
    gptr v = Tmp_Push(0x10);
    for (i = 0; i < n; i++) {
        u32 y = 0x48 + 0x11 * i, s = 0x11 * i;
        int sel = (s8)U8(m + 0x51) == (s32)i;
        if (sel) Menu_DrawFrames(0x6850C0, 0x3C, y, 0xC, 0x7C8F, 0x1060C0, 2);
        U16(v) = U16(v + 8) = 0x40; U16(v + 4) = U16(v + 0xC) = 0x124;
        U16(v + 2) = U16(v + 6) = (u16)(s + 0x47); U16(v + 0xA) = U16(v + 0xE) = (u16)(s + 0x58);
        if (sel) Menu_PolyG4(v, 0x20, 0x80, 0x20, 0x80, 1, 0xA);
        else Menu_PolyG4(v, 0x100000, 0x800000, 0x100000, 0x800000, 1, 0xA);
        Menu_Number12(0x28, y + 2, 2, 1, 0x808080, (u32)((s8)U8(m + 0x52) + (s32)i + 1), 3);
        Menu_DrawFrames(0x6859D8, 0x18, y, 0xC, 0x7C8E, 0x80808080, 3);
        Menu_DrawFrames(0x6859E0, 0x11E, y + 1, 0xC, 0x7C8E, 0x80808080, 3);
        Menu_Line(0x18, s + 0x58, 0x124, s + 0x58, 0x75E533, 1, 4);
        Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0xC, 0);
        Gpu_AddPrim(0x87BC10 + (U32(0x87DE0C) << 12), U32(0x87DC98));
        U32(0x87DC98) += 0xC;
        Text_Draw(U32(0x704260 + U8(m + 0x28 + i) * 12), 0x48, s + 0x48, 0x64, 0x808080, 3, 0);
    }
    Tmp_Pop(0x10);
    if (i < 6)
        for (u32 y = 0x11 * i + 0x48; y < 0xAE; y += 0x11) {
            Menu_DrawFrames(0x6859D8, 0x18, y, 0xC, 0x7C8E, 0x80808080, 3);
            Menu_DrawFrames(0x6859E0, 0x1D, y, 0xC, 0x7C8E, 0x80808080, 3);
        }
    if ((s8)U8(m + 0x52) > 0)
        Menu_DrawFrames(0x685070, 0x9C, 0x3E, 0xC, 0x7C8F, Color_Pulse(0x4040, 0xC0C0, 5), 3);
    if ((s8)U8(m + 0x52) < (s8)U8(m + 0x50) - 6)
        Menu_DrawFrames(0x685068, 0x9C, 0xB0, 0xC, 0x7C8F, Color_Pulse(0x4040, 0xC0C0, 5), 3);
}

/* step 6: the files listed; up/down move M+0x51 and scroll M+0x52 */
void DC2CALL Files_List(gptr m)                                     /* 0x499100 */
{
    u8 max = (u8)(((s8)U8(m + 0x50) < 6 ? (s8)U8(m + 0x50) : 6) - 1);
    u8 o51 = U8(m + 0x51), o52 = U8(m + 0x52);
    u16 k = U16(m + 0x44);
    if (k & 0x1000) {
        U8(m + 0x51) = (u8)(o51 - 1);
        if ((s8)U8(m + 0x51) < 0) {
            U8(m + 0x51) = 0;
            if ((s8)o52 > 0) U8(m + 0x52) = (u8)(o52 - 1);
        }
    } else if (k & 0x4000) {
        U8(m + 0x51) = (u8)(o51 + 1);
        if ((s8)U8(m + 0x51) > (s32)max) {
            U8(m + 0x51) = max;
            if ((s8)o52 < (s8)U8(m + 0x50) - 6) U8(m + 0x52) = (u8)(o52 + 1);
        }
    } else if (U8(0x87DC00) & 0x40) {
        Se_Play(0, 1);
        U8(m + 2) = 1;
    } else if (U8(0x87DC00) & 0x20) {
        Se_Play(0, 2);
        U8(m + 0x53) = U8(m + 0x28 + (s8)U8(m + 0x51));
        U8(m + 2) = 7;
    }
    if ((u32)o51 != (u32)(s32)(s8)U8(m + 0x51) || (u32)o52 != (u32)(s32)(s8)U8(m + 0x52)) Se_Play(0, 0);
    Files_Draw(m);
    Menu_DrawFrames(0x684A40, 0x60, 0xC0, 0xD, 0x7C8E, 0x80808080, 3);
    Menu_DrawFrames(0x684B60, 0xE8, 0xC0, 0xD, 0x7C8E, 0x80808080, 3);
    Text_Draw(0x7248E2, 0x20, 0xC1, 0x64, 0x808080, 2, 0);
}

/* step 7: read or use the file (M+0x53); used where a zone wants it, it is
 * given to the room (G+0xB4A) and the menu closes */
void DC2CALL Files_Sel(gptr m)                                      /* 0x499260 */
{
    if (U8(m + 0x45) & 0x50) {
        Se_Play(0, 0);
        U8(m + 0x4E) ^= 1;
    } else if (U8(0x87DC00) & 0x40) {
        Se_Play(0, 1);
        U8(m + 0x4E) = 0; U8(m + 2) = 6;
    } else if (U8(0x87DC00) & 0x20) {
        if (U8(m + 0x4E)) {
            while (File_IsBusy()) Task_Sleep(1);
            Item_LoadIcon((u16)(s16)(s8)U8(m + 0x53));
            U8(m + 2) = 8;
            Se_Play(0, 2);
        } else if (Flag_Check(0xA, (u32)(s32)(s8)U8(m + 0x53)) && Zone_FileSpot(U8(m + 0x53))) {
            U8(G() + 0xB4A) = U8(m + 0x53);
            U32(m) = 0x502;
        } else U8(m + 2) = 0xA;
        U8(m + 3) = 0;
    }
    u32 c0 = U8(m + 0x4E) ? 0x303030 : 0x808080, c1 = U8(m + 0x4E) ? 0x808080 : 0x303030;
    Text_Draw(0x7242C4, 0xC9, 0x16, 0x64, c0, 2, 0);
    Text_Draw(0x7242B8, 0xC9, 0x26, 0x64, c1, 2, 0);
    U16(m + 0x88)++;
    if (S16(m + 0x88) > 0x44) U16(m + 0x88) = 0;
    gptr r = Tmp_Push(8);
    for (u32 j = 0; j < 2; j++) {
        U16(r) = 0xC8; U16(r + 2) = (u16)((s8)U8(m + 0x4E) * 16 + 0x16); U16(r + 4) = 0x30; U16(r + 6) = 0x10;
        Menu_Border(r, (u16)(U16(m + 0x88) + 0x22 * j), 0x22, 2, 0xFFFF);
    }
    Tmp_Pop(8);
    Files_Draw(m);
}

/* step 8: reading the file: its picture once loaded; cancel back to the list */
void DC2CALL Files_View(gptr m)                                     /* 0x499430 */
{
    switch ((s8)U8(m + 3)) {
    case 0:
        U8(m + 0x4E) = 0;
        if (!File_IsBusy()) U8(m + 3)++;
        break;
    case 1: {
        gptr r = Tmp_Push(8);
        U16(r) = 0x60; U16(r + 2) = 0x50; U16(r + 4) = 0x80; U16(r + 6) = 0x40;
        Menu_Sprite(r, 0, 0xC0, 0x7C40, 0x808080, 0, 1);
        Tmp_Pop(8);
        Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0x8C, 0);
        Gpu_AddPrim(0x87BC04 + (U32(0x87DE0C) << 12), U32(0x87DC98));
        U32(0x87DC98) += 0xC;
        if (U8(0x87DC00) & 0x40) { Se_Play(0, 1); U8(m + 2) = 6; U8(m + 3) = 0; }
        break; }
    }
    Files_Frame(m);
}

/* step 9: no files */
void DC2CALL Files_None(gptr m)                                     /* 0x499520 */
{
    u8 pad = U8(0x87DC00);
    if (pad & 0x60) { Se_Play(0, pad & 0x40 ? 1 : 2); U8(m + 2) = 0xB; }
    notice(m, 0x72438A);
}

/* step 10: the file cannot be used here (0x2D in area 7, 0x33, 0x34, 0x37, 0x38: another text) */
void DC2CALL Files_Cant(gptr m)                                     /* 0x4995C0 */
{
    u8 id = U8(m + 0x53), pad = U8(0x87DC00);
    if (pad & 0x60) { Se_Play(0, pad & 0x40 ? 1 : 2); U8(m + 2) = 6; }
    int other = id == 0x2D ? U8(G() + 0x1091) == 7 : id == 0x37 || id == 0x38 || id == 0x33 || id == 0x34;
    notice(m, other ? 0x7243F0 : 0x7243C0);
}

/* step 11: back to the pause menu */
void DC2CALL Items_Back(gptr m)                                     /* 0x4996C0 */
{
    U8(m + 1) = 0; U8(m + 2) = 0; U8(m + 3) = 0;
    Menu_Header(m);
    Menu_Weapons(m, 0x80, 0x18, 2);
    Menu_Health(0x10, 0x58);
    Menu_Points(0x80, 0x58, 0x808080);
    Menu_PauseList(m);
    screen_bottom();
    Text_Draw(0x724208, 0x20, 0xC5, 0x64, 0x707070, 0x64, 0);
}

/* ---- screen 3: memos (steps 0x739DBC by M+2) ---- */

/* M+0x54 memos found (flags 9:0x50..0x7F) */
void DC2CALL Screen_Memos(gptr m)                                   /* 0x49A540 */
{
    U8(m + 0x54) = (u8)Flag_CountRange(0x50, 0x80);
    Orig_Call1(U32(0x739DBC + (s8)U8(m + 2) * 4), m);
    Menu_Frame(m);
}

/* step 0 */
void DC2CALL Memos_Init(gptr m)                                     /* 0x49A570 */
{
    U8(m + 2) = U8(m + 0x54) ? 1 : 3;
    U8(m + 3) = 0; U32(m + 4) = 0;
    U8(m + 0x55) = 0; U8(m + 0x56) = 0; U8(m + 0x57) = 0; U8(m + 0x58) = 0; U8(m + 0x4E) = 0;
    U16(m + 0x92) = 0;
    U32(m + 0x30) = 0; U32(m + 0x34) = 0;
    Memos_Draw(m);
}

/* step 1: the memos listed; up/down move M+0x55 and scroll M+0x56 */
void DC2CALL Memos_List(gptr m)                                     /* 0x49A5C0 */
{
    u8 max = (u8)(((s8)U8(m + 0x54) < 6 ? (s8)U8(m + 0x54) : 6) - 1);
    u8 o55 = U8(m + 0x55), o56 = U8(m + 0x56);
    u16 k = U16(m + 0x44);
    if (k & 0x1000) {
        U8(m + 0x55) = (u8)(o55 - 1);
        if ((s8)U8(m + 0x55) < 0) {
            U8(m + 0x55) = 0;
            if ((s8)o56 > 0) U8(m + 0x56) = (u8)(o56 - 1);
        }
    } else if (k & 0x4000) {
        U8(m + 0x55) = (u8)(o55 + 1);
        if ((s8)U8(m + 0x55) > (s32)max) {
            U8(m + 0x55) = max;
            if ((s8)o56 < (s8)U8(m + 0x54) - 6) U8(m + 0x56) = (u8)(o56 + 1);
        }
    } else if (U8(0x87DC00) & 0x40) {
        Se_Play(0, 1);
        U8(m + 2) = 4;
    } else if (U8(0x87DC00) & 0x20) {
        Se_Play(0, 2);
        U8(m + 2) = 2; U8(m + 3) = 0;
        U8(m + 0x57) = (u8)(U8(m + 0x30 + (s8)U8(m + 0x55)) - 0x50);
        U8(m + 0x58) = 0;
        U16(m + 0x92) = 0;
    }
    Memos_Draw(m);
    Menu_DrawFrames(0x684A40, 0x60, 0xC0, 0xD, 0x7C8E, 0x80808080, 3);
    Menu_DrawFrames(0x684B60, 0xE8, 0xC0, 0xD, 0x7C8E, 0x80808080, 3);
    Text_Draw(0x72490A, 0x20, 0xC1, 0x64, 0x808080, 2, 0);
    if ((u32)o55 != (u32)(s32)(s8)U8(m + 0x55) || (u32)o56 != (u32)(s32)(s8)U8(m + 0x56)) Se_Play(0, 0);
}

/* step 2: reading memo M+0x57 (records 0x704628: title, text), its picture
 * loaded first; page M+0x58, typed out M+0x92 letters at a time, M+0x94 set
 * once the page is all out; cancel back (or out, when opened for the memo) */
void DC2CALL Memos_Read(gptr m)                                     /* 0x49A740 */
{
    switch ((s8)U8(m + 3)) {
    case 0:
        if (!File_IsBusy()) {
            File_LoadPicture((u16)(s16)(s8)U8(m + 0x57));
            U8(m + 3)++;
        }
        break;
    case 1:
        if (!File_IsBusy()) U8(m + 3)++;
        break;
    case 2: {
        gptr text = U32(0x70462C + (s8)U8(m + 0x57) * 8);
        s16 old = (s8)U8(m + 0x58);
        if (U16(m + 0x94)) {
            u16 k = U16(m + 0x44);
            if (k & 0x1000) {
                U8(m + 0x58)--;
                if ((s8)U8(m + 0x58) < 0) U8(m + 0x58) = 0;
            } else if (k & 0x4020) {
                s16 n = (s16)Text_Pages(text);
                U8(m + 0x58)++;
                if (n <= (s8)U8(m + 0x58)) U8(m + 0x58) = (u8)(n - 1);
            } else if (U8(0x87DC00) & 0x40) {
                if (U8(m + 0x64) == 1) { U32(m) = 0x502; U8(m + 0x64) = 0; }
                else { Se_Play(0, 1); U8(m + 2) = 1; U8(m + 3) = 0; }
            }
        }
        s16 page = (s8)U8(m + 0x58);
        if (page != old) U16(m + 0x92) = 0;
        if (!U16(m + 0x94)) {
            u16 c = (u16)(U16(m + 0x92) + 1);
            U16(m + 0x92) = c;
            if (U32(0x87DC00) & 0x5060) U16(m + 0x92) = (u16)(c + 1000);
            if (S16(m + 0x92) > 1000) U16(m + 0x92) = 1000;
        }
        U16(m + 0x94) = (u16)Memo_Type(text, 0x38, 0x3C, (u16)page, U16(m + 0x92), 0x808080, 5, 1);
        break; }
    }
    Memos_Frame(m);
}

/* a text's pages: one more than its page breaks (0x5xxx) before the end (0xFxxx) */
u32 DC2CALL Text_Pages(gptr t)                                      /* 0x49A8D0 */
{
    u32 n = 1;
    for (;; t += 2) {
        u32 c = U16(t) & 0xF000;
        if (c == 0xF000) return n;
        if (c == 0x5000) n++;
    }
}

/* step 3: no memos */
void DC2CALL Memos_None(gptr m)                                     /* 0x49A900 */
{
    u8 pad = U8(0x87DC00);
    if (pad & 0x60) { Se_Play(0, pad & 0x40 ? 1 : 2); U8(m + 2) = 4; }
    notice(m, 0x724354);
}

/* step 4: back to the pause menu */
void DC2CALL Memos_Back(gptr m)                                     /* 0x49A9A0 */
{
    U8(m + 1) = 0; U8(m + 2) = 0; U8(m + 3) = 0;
    Menu_Header(m);
    Menu_Weapons(m, 0x80, 0x18, 2);
    Menu_Health(0x10, 0x58);
    Menu_Points(0x80, 0x58, 0x808080);
    Menu_PauseList(m);
}

/* the memos page (M+0x30.. the six shown from page M+0x56, M+0x55 the cursor) */
void DC2CALL Memos_Draw(gptr m)                                     /* 0x49A9F0 */
{
    Menu_DrawFrames(0x6853F0, 0xD6, 0x21, 0xC, 0x7C8F, 0x808080, 2);
    Menu_Line(0xCC, 0x20, 0x100, 0x20, 0x75E533, 1, 4);
    Menu_Line(0xCC, 0x30, 0x100, 0x30, 0x75E533, 1, 4);
    draw_mode(0xC, 4);
    Menu_DrawFrames(0x6857B0, 0x1C, 0x18, 0xC, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x6857C0, 0x68, 0x18, 0xC, 0x7C8E, 0x80808080, 3);
    Menu_DrawFrames(0x6857C8, 0x34, 0x28, 0xC, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x6857D0, 0x24, 0x18, 0xC, 0x7C8F, 0x2060C0, 2);
    Menu_DrawFrames(0x685400, 0x3A, 0x1D, 0xC, 0x7C8F, 0x808080, 2);
    Menu_Number11(0x55, 0x1B, 2, 1, 0x104080, (u32)((s8)U8(m + 0x56) + (s8)U8(m + 0x55) + 1), 5);
    Menu_DrawFrames(0x685080, 0x88, 0x24, 0xC, 0x7C8E, 0x80808080, 2);
    for (u32 k = 0; k < 6; k++) {
        u32 v = (u8)(0x80 - (k << 4));
        u32 c = ((((v | 0xFFFF8000u) << 8) | v) << 8) | v;
        Menu_DrawFrames(0x685060, 0x68 + 5 * k, 0x2C, 0xC, 0x7C8E, c, 2);
        Menu_DrawFrames(0x685028, 0xC0 - 5 * k, 0x24, 0xC, 0x7C8E, c, 2);
    }
    Menu_DrawFrames(0x6857D8, 0x58, 0x38, 0xC, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x6858D8, 0xE0, 0x38, 0xC, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x685858, 0x58, 0xAE, 0xC, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x685958, 0xE0, 0xAE, 0xC, 0x7C8E, 0x80808080, 2);
    u8 k = U8(m + 0x56), b = 0x50;
    for (;;) {
        if (Flag_Check(9, b) && (s8)--k < 0) break;
        if (++b >= 0x80) break;
    }
    Flag_ListRange(b, 0x80, 6, m + 0x30);
    u32 n = (u8)((s8)U8(m + 0x54) < 6 ? U8(m + 0x54) : 6), i;
    gptr v = Tmp_Push(0x10);
    for (i = 0; i < n; i++) {
        u32 y = 0x48 + 0x11 * i, s = 0x11 * i;
        if ((s8)U8(m + 0x55) == (s32)i) {
            Menu_DrawFrames(0x6850C0, 0x38, y, 0xC, 0x7C8F, 0x1060C0, 2);
            U16(v) = U16(v + 8) = 0x20; U16(v + 4) = U16(v + 0xC) = 0x124;
            U16(v + 2) = U16(v + 6) = (u16)(s + 0x47); U16(v + 0xA) = U16(v + 0xE) = (u16)(s + 0x58);
            Menu_PolyG4(v, 0, 0x80, 0, 0x80, 1, 0xA);
        } else {
            U16(v) = U16(v + 8) = 0x40; U16(v + 4) = U16(v + 0xC) = 0x124;
            U16(v + 2) = U16(v + 6) = (u16)(s + 0x47); U16(v + 0xA) = U16(v + 0xE) = (u16)(s + 0x58);
            Menu_PolyG4(v, 0x100000, 0x800000, 0x100000, 0x800000, 1, 0xA);
        }
        Menu_Number12(0x24, y + 2, 2, 1, 0x808080, (u32)((s8)U8(m + 0x56) + (s32)i + 1), 3);
        Menu_DrawFrames(0x6859D8, 0x18, y, 0xC, 0x7C8E, 0x80808080, 3);
        Menu_DrawFrames(0x6859E0, 0x11E, y, 0xC, 0x7C8E, 0x80808080, 3);
        Menu_Line(0x18, s + 0x58, 0x124, s + 0x58, 0x75E533, 1, 4);
        draw_mode(0xC, 4);
        Memo_Type(U32(0x7043A8 + U8(m + 0x30 + i) * 8), 0x40, s + 0x48, 0, 0x64, 0x808080, 3, 0);
    }
    Tmp_Pop(0x10);
    if (i < 6)
        for (u32 y = 0x11 * i + 0x48; y < 0xAE; y += 0x11) {
            Menu_DrawFrames(0x6859D8, 0x18, y, 0xC, 0x7C8E, 0x80808080, 3);
            Menu_DrawFrames(0x6859E0, 0x1D, y, 0xC, 0x7C8E, 0x80808080, 3);
        }
    if ((s8)U8(m + 0x56) > 0)
        Menu_DrawFrames(0x685070, 0x9C, 0x3E, 0xC, 0x7C8F, Color_Pulse(0x4040, 0xC0C0, 5), 3);
    if ((s8)U8(m + 0x56) < (s8)U8(m + 0x54) - 6)
        Menu_DrawFrames(0x685068, 0x9C, 0xB0, 0xC, 0x7C8F, Color_Pulse(0x4040, 0xC0C0, 5), 3);
}

/* the memo page's frame: number, title, "PAGE n/m" and the scroll bar */
void DC2CALL Memos_Frame(gptr m)                                    /* 0x49AFA0 */
{
    Menu_DrawFrames(0x685400, 0x3A, 0x1D, 0xC, 0x7C8F, 0x808080, 2);
    Menu_Number11(0x55, 0x1B, 2, 1, 0x104080, (u32)((s8)U8(m + 0x56) + (s8)U8(m + 0x55) + 1), 5);
    Menu_DrawFrames(0x6857B0, 0x1C, 0x18, 0xC, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x6857C0, 0x68, 0x18, 0xC, 0x7C8E, 0x80808080, 3);
    Menu_DrawFrames(0x6857C8, 0x114, 0x28, 0xC, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x6857D0, 0x24, 0x18, 0xC, 0x7C8F, 0x2060C0, 2);
    Menu_DrawFrames(0x6857D8, 0x58, 0x38, 0xC, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x685BB8, 0xE0, 0x38, 0xC, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x685858, 0x58, 0xC8, 0xC, 0x7C8E, 0x80808080, 2);
    Menu_DrawFrames(0x685C30, 0xE0, 0xC8, 0xC, 0x7C8E, 0x80808080, 2);
    s32 id = (s8)U8(m + 0x57);
    Memo_Type(U32(0x704628 + id * 8), 0x40, 0x28, 0, 0x64, 0x808080, 3, 0);
    s32 pages = (s16)Text_Pages(U32(0x70462C + id * 8));
    Text_Ascii(0x100, 0x18, 0xFF06, 0x739DD8);
    gptr r = Tmp_Push(0x10), buf = r + 8;
    s32 page = (s8)U8(m + 0x58);
    Crt_sprintf3(buf, 0x739DD0, (u32)(page + 1), (u32)pages, 0);
    Text_Ascii(0x108, 0x20, 0xFF00, buf);
    U16(r) = 0x11D; U16(r + 4) = 8;
    U16(r + 2) = (u16)(page * 0x92 / pages + 0x3F);
    U16(r + 6) = (u16)(0x92 / pages);
    Menu_Tile(r, 0x75E533, 0, 4);
    Tmp_Pop(0x10);
    Menu_Line(0x11C, 0x3E, 0x125, 0x3E, 0x75E533, 1, 4);
    Menu_Line(0x11C, 0x3E, 0x11C, 0xD0, 0x75E533, 1, 4);
    Menu_Line(0x125, 0x3E, 0x125, 0xD0, 0x75E533, 1, 4);
    Menu_Line(0x11C, 0xD0, 0x125, 0xD0, 0x75E533, 1, 4);
    draw_mode(0xC, 4);
}

/* typed text (16-bit codes) from page `page`, at most `count` codes: letters
 * 14x14 (tpage = the code, widths 0x723D4C), 0x?006 a digit of G+0x1204..,
 * 0x1xxx a color (the given one, red, green, yellow, blue..), 0x2xxx the
 * picture, 0x4xxx a newline after its letter, 0x5xxx the page's end, 0xFxxx
 * the text's end; returns 1 once the page is out, then (arrows) blinks the
 * arrows for the pages before and after */
u32 DC2CALL Memo_Type(gptr t, u32 x, u32 y, u32 page, u32 count, u32 color, u32 z, u32 arrows) /* 0x49B270 */
{
    u32 x16 = x & 0xFFFF, digits = 0, done = 0, last = 0;
    float xc = (float)(s32)x16;
    u32 col[8] = { (color & 0xFFFFFF) | 0x6C000000, 0x6C2A2A80, 0x6C008000, 0x6C808000,
                   0x6C205080, 0x6C205080, 0x6C205080, 0x6C205080 };
    u32 cur = col[0];
    for (u16 p = (u16)page; p; t += 2)
        if ((U16(t) & 0xF000) == 0x5000) p--;
    while ((s16)--count >= 0) {
        u32 ch = U16(t), hi = ch >> 12, g = ch & 0xFFF;
        t += 2;
        if (g == 6) {
            g = U8(G() + (digits & 0xFFFF) + 0x1204) + 0x30;
            digits++;
        } else if (hi == 2) {
            gptr r = Tmp_Push(8);
            U16(r) = 0x58; U16(r + 2) = 0x48; U16(r + 4) = 0x80; U16(r + 6) = 0x60;
            Menu_Sprite(r, 0, 0, 0x7C40, 0x707070, 0, 1);
            Tmp_Pop(8);
            draw_mode(0x96, 1);
            continue;
        } else if (hi == 1) {
            cur = col[g & 7];               /* 8..15 read past the table in the original */
            continue;
        } else if (hi == 0xF) {
            last = 1; done = 1; break;
        } else if (hi == 5) {
            done = 1; break;
        }
        gptr q = U32(0x87DC98);
        u32 yy = y & 0xFFFF;
        float x1 = (float)((long double)xc + F32(0x703454));
        float fy0 = (float)(s32)yy, fy1 = (float)(s32)(yy + 0xE);
        U32(q + 4) = cur;
        F32(q + 8) = xc; F32(q + 0xC) = fy0;
        F32(q + 0x14) = x1; F32(q + 0x18) = fy0;
        F32(q + 0x20) = xc; F32(q + 0x24) = fy1;
        F32(q + 0x2C) = x1; F32(q + 0x30) = fy1;
        U8(q + 0x10) = 0; U8(q + 0x11) = 0;
        U8(q + 0x1C) = 0xE; U8(q + 0x1D) = 0;
        U8(q + 0x28) = 0; U8(q + 0x29) = 0xE;
        U8(q + 0x34) = 0xE; U8(q + 0x35) = 0xE;
        U16(q + 0x1E) = (u16)g;
        U16(q + 0x12) = 0x7DCC;
        ot_link(q, 2, z);
        xc = (float)((long double)xc + F32(0x723D4C + g * 4));
        U32(0x87DC98) = q + 0x38;
        if (hi == 4) { xc = (float)(s32)x16; y += 0xF; }
    }
    if ((u16)arrows && done) {
        if ((u16)page && (U8(G() + 4) & 0x10))
            Menu_DrawFrames(0x685070, 0x24, 0x40, 0xC, 0x7C8F, 0x808080, 3);
        if (!last && (U8(G() + 4) & 0x10))
            Menu_DrawFrames(0x685068, 0x24, 0xC8, 0xC, 0x7C8F, 0x808080, 3);
    }
    return done;
}

/* ---- screen 4: the map (steps 0x73A38C by M+2, the areas' pages 0x73A360 by M+0x62) ---- */

/* a two-code text (a button's sign) at x, y */
static void sign(u32 code, u32 x, u32 y, u32 color)
{
    gptr t = Tmp_Push(4);
    U16(t) = (u16)code; U16(t + 2) = 0xF000;
    Text_Draw(t, x, y, 0x64, color, 2, 0);
    Tmp_Pop(4);
}

/* the room's record in 0x739DE0 (16 bytes: +0 the map's spot, +4, +8 a map) */
static gptr map_room(void)
{
    gptr g = G();
    return 0x739DE0 + ((u8)Room_GlobalIndex(U8(g + 0x1091), U8(g + 0x1090)) << 4);
}

/* the grid behind, the button signs, then the step */
void DC2CALL Screen_Map(gptr m)                                     /* 0x49B620 */
{
    U32(0x87DE00) = 2;
    for (u32 i = 1; i < 0x13; i++) Menu_Line(i << 4, 0, i << 4, 0xF0, 0x204020, 1, 0x30C);
    for (u32 i = 1; i < 0xF; i++) Menu_Line(0, i << 4, 0x140, i << 4, 0x204020, 1, 0x30C);
    draw_mode(0xC, 0x30C);
    if (U8(m + 2) != 7) {
        sign(0x1B, 0x86, 0xD8, 0x404040);
        if (U8(m + 2) != 6 && U8(m + 0x64) == 3) Menu_DrawFrames(0x6850B8, 0x9A, 0xDB, 0xC, 0x7C8F, 0x8068C040, 2);
        else Menu_DrawFrames(0x685040, 0x98, 0xDB, 0xC, 0x7C8F, 0x8068C040, 2);
    }
    Orig_Call1(U32(0x73A38C + (s8)U8(m + 2) * 4), m);
}

/* the areas whose maps were found (flags 11:0x75..0x7F), less one */
u32 DC2CALL Map_CountAreas(void)                                    /* 0x49B770 */
{
    u32 n = 0;
    for (u32 b = 0x75; b < 0x80; b++) if (Flag_Check(0xB, b)) n++;
    return n - 1;
}

/* the next area with its map after M+0x62 (round), or M+0x62 */
u32 DC2CALL Map_NextArea(gptr m)                                    /* 0x49B7A0 */
{
    s16 a;
    for (a = (s16)(U8(m + 0x62) + 1); a <= 10; a++) if (Flag_Check(0xB, (u32)(a + 0x75))) return (u16)a;
    for (a = 0; (s32)a <= (s32)U8(m + 0x62) - 1; a++) if (Flag_Check(0xB, (u32)(a + 0x75))) return (u16)a;
    return U8(m + 0x62);
}

/* the one before (round) */
u32 DC2CALL Map_PrevArea(gptr m)                                    /* 0x49B810 */
{
    s16 a;
    for (a = (s16)(U8(m + 0x62) - 1); a >= 0; a--) if (Flag_Check(0xB, (u32)(a + 0x75))) return (u16)a;
    for (a = 10; (s32)a >= (s32)U8(m + 0x62) + 1; a--) if (Flag_Check(0xB, (u32)(a + 0x75))) return (u16)a;
    return U8(m + 0x62);
}

/* step 0: the screen's colors; once loaded, the map (or step 7: none here) */
void DC2CALL Map_Init(gptr m)                                       /* 0x49B880 */
{
    gptr r = map_room(), g = G();
    U8(g + 0xB58) = 0x80; U8(g + 0xB59) = 0x80; U8(g + 0xB5A) = 0x80; U8(g + 0xB5B) = 0; U8(g + 0xB5C) = 0;
    DrawSync(0);
    if (File_IsBusy()) return;
    U16(m + 0x9A) = 4; U16(m + 0x9C) = 0x34;
    U16(m + 0x92) = 0; U16(m + 0x94) = 0; U16(m + 0x96) = 0x1E;
    U8(m + 2) = U32(r + 8) ? 1 : 7;
}

/* step 1: an area's map; L/R the other areas found, select zooms on the
 * player when the map is his area's */
void DC2CALL Map_View(gptr m)                                       /* 0x49B940 */
{
    Orig_Call1(U32(0x73A360 + U8(m + 0x62) * 4), m);
    u16 n = (u16)Map_CountAreas();
    gptr r = map_room(), g = G();
    u16 area = (u8)Area_SoundArea(U8(g + 0x1091), U8(g + 0x1090));
    if (n && (U8(G() + 4) & 8)) {
        gptr v = Tmp_Push(0x10);
        U16(v) = 0x120; U16(v + 2) = 0x6C; U16(v + 4) = 0x130; U16(v + 6) = 0x78;
        U16(v + 8) = 0x120; U16(v + 0xA) = 0x84; U16(v + 0xC) = 0; U16(v + 0xE) = 0;
        Menu_PolyF3(v, 0x800070C0, 0x64);
        U16(v) = 0x10; U16(v + 2) = 0x78; U16(v + 4) = 0x20; U16(v + 6) = 0x6C;
        U16(v + 8) = 0x20; U16(v + 0xA) = 0x84;
        Menu_PolyF3(v, 0x800070C0, 0x64);
        Tmp_Pop(0x10);
        draw_mode(0xC, 0x64);
    }
    u32 pad = U32(0x87DC00);
    if ((pad & 0x2000) && n) {
        Se_Play(0, 0xD);
        U8(m + 0x60) = (u8)(Map_NextArea(m) + 5);
        U8(m + 2) = 2; U16(m + 0x92) = 0; U16(m + 0x94) = 0;
    } else if ((pad & 0x8000) && n) {
        Se_Play(0, 0xD);
        U8(m + 0x60) = (u8)(Map_PrevArea(m) + 5);
        U8(m + 2) = 2; U16(m + 0x92) = 0; U16(m + 0x94) = 0;
    } else if (pad & 0x40) {
        U8(m + 2) = 8;
    } else if ((pad & 0x20) && U32(r) && area == U8(m + 0x62)) {
        U8(m + 2) = 4;
        Se_Play(0, 0xE);
    }
    if (U32(r) && area == U8(m + 0x62)) {
        sign(0x1A, 0x18, 0x22, 0x808080);
        Text_Draw(0x7276C0, 0x28, 0x22, 0x64, 0x707070, 0x1F4, 0);
    }
}

/* step 2: another area's map wanted (M+0x60, its file) */
void DC2CALL Map_Other(gptr m)                                      /* 0x49BB80 */
{
    Game_Start((u32)(s32)(s8)U8(m + 0x60));
    U8(G() + 0xB48) = U8(m + 0x60);
    U8(m + 2) = 3;
    U8(m + 0x62) = (u8)(U8(m + 0x60) - 5);
}

/* step 3 */
void DC2CALL Map_Shown(gptr m)                                      /* 0x49BBB0 */
{
    U8(m + 2) = 1;
}

/* step 4: the player's room's page of MAP.DAT (once, M+0x61) */
void DC2CALL Map_ZoomLoad(gptr m)                                   /* 0x49BBC0 */
{
    gptr g = G();
    u16 k = (u8)Room_GlobalIndex(U8(g + 0x1091), U8(g + 0x1090));
    if ((u32)k != (u32)(s32)(s8)U8(m + 0x61)) {
        Map_LoadPage(k);
        U8(m + 0x61) = (u8)k;
    }
    Orig_Call1(U32(0x73A360 + U8(m + 0x62) * 4), m);
    U8(m + 2) = 5;
}

/* step 5: until it is loaded */
void DC2CALL Map_ZoomWait(gptr m)                                   /* 0x49BC20 */
{
    Orig_Call1(U32(0x73A360 + U8(m + 0x62) * 4), m);
    if (!File_IsBusy()) { U16(m + 0x92) = 0; U8(m + 2) = 6; U8(m + 3) = 0; }
}

/* step 6: the room's page grows in (M+0x92 to 0x46) centred on the player
 * (M+0x9E, M+0xA0), the pad moves it; cancel shrinks it back */
void DC2CALL Map_Zoom(gptr m)                                       /* 0x49BC50 */
{
    gptr g = G();
    Orig_Call1(U32(0x73A360 + U8(m + 0x62) * 4), m);
    Map_ZoomDraw(m);
    switch ((s8)U8(m + 3)) {
    case 0: {
        U8(g + 0xB58) = 0x80; U8(g + 0xB59) = 0x80; U8(g + 0xB5A) = 0x80; U8(g + 0xB5B) = 0; U8(g + 0xB5C) = 0;
        DrawSync(0);
        U16(m + 0x92) += 4;
        if (S16(m + 0x92) <= 0x46) break;
        gptr r = map_room(), pl = U32(G() + 0x940), out = Tmp_Push(0x10);
        U8(m + 3) = 1;
        U16(m + 0x92) = 0x46; U16(m + 0x9E) = 0x58; U16(m + 0xA0) = 0x82;
        Map_Locate(m, U16(pl + 0x4A), U16(pl + 0x40), U16(pl + 0x44), U32(r), U16(r + 4), out);
        U16(m + 0x9E) += (u16)(0x58 - U16(out + 0xC));
        U16(m + 0xA0) += (u16)(0x82 - U16(out + 0xE));
        Tmp_Pop(0x10);
        s16 x = S16(m + 0x9E);
        if (S16(m + 0xA0) < 0x49) U16(m + 0xA0) = 0x49;
        if (S16(m + 0xA0) > 0xBB) U16(m + 0xA0) = 0xBB;
        if (x < 0x1F) U16(m + 0x9E) = 0x1F;
        if (S16(m + 0x9E) > 0x91) U16(m + 0x9E) = 0x91;
        break; }
    case 1: {
        Menu_Line(0x11, 0x3B, 0x11, 0xC9, 0x806060, 0, 0xBE);
        Menu_Line(0x11, 0x3B, 0x9F, 0x3B, 0x806060, 0, 0xBE);
        Menu_Line(0x9F, 0x3B, 0x9F, 0xC9, 0x806060, 0, 0xBE);
        Menu_Line(0x11, 0xC9, 0x9F, 0xC9, 0x806060, 0, 0xBE);
        u32 k = U32(0x87DDFC);
        if (!(k & 0xF000)) {
            if (U8(0x87DC00) & 0x40) { U8(m + 3) = 2; Se_Play(0, 0xD); }
            break;
        }
        int moved = 0;
        if (k & 0x4000) {
            U16(m + 0xA0) -= 3;
            if (S16(m + 0xA0) < 0x49) U16(m + 0xA0) = 0x49; else moved = 1;
        }
        if (U32(0x87DDFC) & 0x1000) {
            U16(m + 0xA0) += 3;
            if (S16(m + 0xA0) > 0xBB) U16(m + 0xA0) = 0xBB; else moved = 1;
        }
        if (U32(0x87DDFC) & 0x2000) {
            U16(m + 0x9E) -= 3;
            if (S16(m + 0x9E) < 0x1F) U16(m + 0x9E) = 0x1F; else moved = 1;
        }
        if (U32(0x87DDFC) & 0x8000) {
            U16(m + 0x9E) += 3;
            if (S16(m + 0x9E) > 0x91) U16(m + 0x9E) = 0x91; else moved = 1;
        }
        if (moved) Se_Play(0, 0xC);
        break; }
    case 2:
        U16(m + 0x92) -= 4;
        if (S16(m + 0x92) <= 0) { U8(m + 2) = 1; U8(m + 3) = 0; U16(m + 0x92) = 0; }
        break;
    }
}

/* step 7: no map here: the screen torn by lines shifted at random, a band
 * rolling down (M+0x7E), a notice; either button back */
void DC2CALL Map_None(gptr m)                                       /* 0x49BF90 */
{
    for (u32 i = 0; i < 100; i++) {
        s32 y = (s32)(Rand8() & 0xFF) % 0xEF;
        s32 dx = (s32)(Rand8() & 0xFF) % 0xF;
        Menu_ShiftLine((u32)(dx - 8), (u32)y, 1);
    }
    if (U8(G() + 4) & 1) U16(m + 0x7E) += 2;
    s16 y = (s16)(S16(m + 0x7E) % 0xEF);
    U16(m + 0x7E) = (u16)y;
    gptr r = Tmp_Push(8);
    U16(r) = 0; U16(r + 2) = (u16)y; U16(r + 4) = 0x140;
    if (y > 0x8B) {
        U16(r + 6) = (u16)(0xEF - y);
        Menu_Tile(r, 0x101010, 1, 2);
        U16(r + 2) = 0; U16(r + 6) = (u16)(U16(m + 0x7E) - 0x8B);
        Menu_Tile(r, 0x101010, 1, 2);
        Menu_ShiftLine(1, U16(m + 0x7E), (u32)(0xEF - U16(m + 0x7E)));
        Menu_ShiftLine(1, 0, (u16)(U16(m + 0x7E) - 0x8B));
    } else {
        U16(r + 6) = 0x64;
        Menu_Tile(r, 0x101010, 1, 1);
        Menu_ShiftLine(1, U16(m + 0x7E), 0x64);
    }
    Tmp_Pop(8);
    Text_Draw(0x7246CC, Text_Left(0x7246CC), 0x64, 0x64, 0x707070, 2, 0);
    if (U8(0x87DC00) & 0x60) U8(m + 2) = 8;
    Menu_Frame(m);
}

/* step 8: back (or out, when opened for the map) */
void DC2CALL Map_Back(gptr m)                                       /* 0x49C110 */
{
    if (U8(m + 0x64) == 3) { U32(m) = 0x502; U8(m + 0x64) = 0; return; }
    Se_Play(0, 0xD);
    U8(m + 1) = 0; U8(m + 2) = 0; U8(m + 3) = 0;
    U32(0x87DE00) = 0;
}

/* the player's arrow on the zoomed page: turned by his angle ry, at x, z
 * scaled by size / scale round the page (M+0x9E, M+0xA0); out: 3 corners,
 * +0xC/+0xE the centre (the room's own turn: 0x40A 0x800, 0x204 0xC18) */
void DC2CALL Map_Locate(gptr m, u32 ry, u32 x, u32 z, u32 scale, u32 size, gptr out) /* 0x49CEB0 */
{
    static const s16 box[4][2] = {{0, 0x5DC}, {0x1F4, 0x64}, {-0x1F4, 0x64}, {0, 0}};
    static const s16 arrow[4][2] = {{0, 0}, {0, -1000}, {300, -300}, {-300, -300}};
    gptr f = Tmp_Push(0x40), r = f, sv = f + 8, mt = f + 0x28;
    u16 room = U16(G() + 0x1090);
    U16(r) = 0; U16(r + 2) = (u16)ry; U16(r + 4) = 0;
    RotMatrix(r, mt);
    for (u32 i = 0; i < 4; i++) {
        gptr v = sv + 8 * i;
        U16(v) = (u16)box[i][0]; U16(v + 2) = 0; U16(v + 4) = (u16)box[i][1];
        ApplyMatrixSV(mt, v, v);
    }
    U16(r) = 0; U16(r + 4) = 0;
    U16(r + 2) = room == 0x40A ? 0x800 : room == 0x204 ? 0xC18 : 0;
    RotMatrix(r, mt);
    for (u32 i = 0; i < 4; i++) {
        gptr v = sv + 8 * i;
        U16(v) += (u16)x; U16(v + 2) = 0; U16(v + 4) += (u16)z;
        ApplyMatrixSV(mt, v, v);
    }
    gptr c = sv + 0x18;
    U16(out + 0xC) = (u16)((s32)((u32)(S16(c) * (s32)size) * 68) / (s32)scale + U16(m + 0x9E));
    U16(out + 0xE) = (u16)(U16(m + 0xA0) - (s32)((u32)(S16(c + 4) * (s32)size) * 68) / (s32)scale);
    U16(r) = 0;
    s16 a = (s16)-S16(U32(G() + 0x940) + 0x4A);
    U16(r + 2) = (u16)(room == 0x40A ? a + 0xF800 : room == 0x204 ? a + 0xF3E8 : a);
    RotMatrix(r, mt);
    for (u32 i = 0; i < 4; i++) {
        gptr v = sv + 8 * i;
        U16(v) = (u16)arrow[i][0]; U16(v + 2) = 0; U16(v + 4) = (u16)arrow[i][1];
        ApplyMatrixSV(mt, v, v);
        U16(v) = (u16)(S16(v) / 100 + U16(out + 0xC));
        U16(v + 2) = 0;
        U16(v + 4) = (u16)(S16(v + 4) / 100 + U16(out + 0xE));
    }
    for (u32 i = 0; i < 3; i++) { U16(out + 4 * i) = U16(sv + 8 * (i + 1)); U16(out + 4 * i + 2) = U16(sv + 8 * (i + 1) + 4); }
    Tmp_Pop(0x40);
}

/* the zoomed page's doors (0x73A3B0, 31 x {room, flag 8: open, flag 9 or
 * G+0x108F: the key, x, y, w, h}): red open, blinking if the key is had, blue shut */
void DC2CALL Map_Doors(gptr m)                                      /* 0x49CD20 */
{
    gptr r = Tmp_Push(8);
    for (gptr d = 0x73A3B0; d < 0x73A3B0 + 0x1F * 0x10; d += 0x10) {
        gptr g = G();
        u16 room = U16(d);
        if (U16(g + 0x1090) != room) continue;
        s16 part = S16(U32(g + 0x940) + 0x5A);
        if (room == 0x603 && part == 5) continue;
        if (room == 0x705 && part == 0) continue;
        if (room == 0x706 && part < 5) continue;
        int st;
        if (!U16(d + 2) || Flag_Check(8, U16(d + 2))) st = 1;
        else if (U16(d + 6) == U8(G() + 0x108F) || Flag_Check(9, U16(d + 4))) st = 2;
        else st = 0;
        U16(r) = (u16)(U16(d + 8) + U16(m + 0x9E) - 0x80);
        U16(r + 2) = (u16)(U16(d + 0xA) + U16(m + 0xA0) - 0x80);
        U16(r + 4) = U16(d + 0xC); U16(r + 6) = U16(d + 0xE);
        if (st == 0) Menu_Tile(r, 0x1010D0, 0, 0xC8);
        else if (st == 1) Menu_Tile(r, 0xB03030, 0, 0xC8);
        else if (U8(G() + 4) & 4) Menu_Tile(r, Color_Pulse(0x3030A0, 0x80830, 3), 0, 0xC8);
    }
    Tmp_Pop(8);
}

/* the room's page (0x7D00), part of it hidden in some rooms by the player's
 * part of the room or a flag */
static int zoom_page_hidden(u16 room)
{
    s16 part = S16(U32(G() + 0x940) + 0x5A);
    switch (room) {
    case 0x402: return part == 4;
    case 0x603: return part == 1;
    case 0x700: case 0x705: return part == 0;
    case 0x703: return part <= 2;
    case 0x704: return part < 3;
    case 0x706: return part < 5;
    default: return 1;
    }
}

/* the zoomed page: room name, bands, grid, scroll arrows, the page with
 * the doors and the player, all within a growing drawing area */
void DC2CALL Map_ZoomDraw(gptr m)                                   /* 0x49C150 */
{
    gptr g = G(), pl = U32(g + 0x940), rec = map_room();
    s32 z = S16(m + 0x92);
    u32 c = (u32)(z << 9 | z << 17 | z * 2), c3 = (u32)(3 * z << 8 | 3 * z << 16 | 3 * z);
    Text_Draw(Room_Name(U8(g + 0x1091), U8(g + 0x1090)), 0x12, 0x1C, 0x64, c, 0xC8, 0);
    Menu_Line(0x10, 0x2C, 0x130, 0x2C, (u32)(z << 17), 0, 0xC8);
    gptr r = Tmp_Push(0x18), out = r + 8;
    U16(r) = 0; U16(r + 2) = 0; U16(r + 4) = 0x140; U16(r + 6) = 0x2F;
    Menu_Tile(r, c3, 3, 0xC8);
    U16(r + 2) = 0xD1; U16(r + 6) = 0x1F;
    Menu_Tile(r, c3, 3, 0xC8);
    Menu_DrawArea(0, 0, 0x140, 0xF0, 0xC8);
    if (U16(m + 0x92) == 0x46) {
        int drawn = 0;
        if (S16(m + 0xA0) > 0x49) {
            Menu_DrawFrames(0x685068, 0x50, 0xBD, 0xC, 0x7C8F, Color_Pulse(0x80C0, 0x4060, 3), 0xC8);
            if (U32(0x87DDFC) & 0x4000) Menu_DrawFrames(0x685068, 0x50, 0xC1, 0xC, 0x7C8F, Color_Pulse(0xC0, 0x60, 3), 0xC8);
        }
        if (S16(m + 0xA0) < 0xBB) {
            Menu_DrawFrames(0x685070, 0x50, 0x3F, 0xC, 0x7C8F, Color_Pulse(0x80C0, 0x4060, 3), 0xC8);
            if (U32(0x87DDFC) & 0x1000) Menu_DrawFrames(0x685070, 0x50, 0x3B, 0xC, 0x7C8F, Color_Pulse(0xC0, 0x60, 3), 0xC8);
        }
        if (S16(m + 0x9E) > 0x1F) {
            Menu_DrawFrames(0x6850C0, 0x92, 0x78, 0xC, 0x7C8F, Color_Pulse(0x80C0, 0x4060, 3), 0xC8);
            if (U32(0x87DDFC) & 0x2000) Menu_DrawFrames(0x6850C0, 0x97, 0x78, 0xC, 0x7C8F, Color_Pulse(0xC0, 0x60, 3), 0xC8);
        }
        if (S16(m + 0x9E) < 0x91) {
            Menu_DrawFrames(0x685CD8, 0x16, 0x78, 0xC, 0x7C8F, Color_Pulse(0x80C0, 0x4060, 3), 0xC8);
            if (U32(0x87DDFC) & 0x8000) Menu_DrawFrames(0x685CD8, 0x11, 0x78, 0xC, 0x7C8F, Color_Pulse(0xC0, 0x60, 3), 0xC8);
        }
        for (u32 i = 1; i < 0x27; i++) Menu_Line(i * 8, 0, i * 8, 0xF0, 0x303030, 1, 0xC8);
        for (u32 i = 1; i < 0x1D; i++) Menu_Line(0, i * 8, 0x140, i * 8, 0x303030, 1, 0xC8);
        draw_mode(0xC, 0xC8);
        u32 col = Color_Pulse(0xA0A0A0, 0x606060, 5);
        Map_Locate(m, U16(pl + 0x4A), U16(pl + 0x40), U16(pl + 0x44), U32(rec), U16(rec + 4), out);
        Menu_DrawFrames(0x65F570, (u32)(s32)S16(out + 0xC), (u32)(s32)S16(out + 0xE), 0x16, 0x7C02, col, 0xC8);
        Menu_PolyF3(out, Color_Pulse(0x40C0C0, 0x208080, 4), 0xC8);
        Map_Doors(m);
        u16 room = U16(G() + 0x1090);
        U16(r) = (u16)(U16(m + 0x9E) - 0x80); U16(r + 2) = (u16)(U16(m + 0xA0) - 0x80);
        U16(r + 4) = 0xFF; U16(r + 6) = 0xFF;
        if (room == 0x503) {
            static const struct { u8 bit; s8 dy; u8 end; } st[9] = {
                {0x18, -0x2E, 1}, {0x17, -0x24, 0}, {0x16, -0x1B, 0}, {0x15, -0x12, 0}, {0x14, -9, 0},
                {0x13, 0, 0}, {0x12, 9, 0}, {0x11, 0x12, 0}, {0x10, 0x1B, 1}};
            for (u32 i = 0; i < 9; i++)
                if (!Flag_Check(7, st[i].bit))
                    Menu_DrawFrames(st[i].end ? 0x65FC90 : 0x65FC98, (u32)(S16(m + 0x9E) - 6), (u32)(S16(m + 0xA0) + st[i].dy),
                                    6, 0x7D00, 0x808080, 0xC8);
        } else if (room == 0x808) {
            if (Flag_Check(6, 0x19)) Menu_Sprite(r, 0, 0, 0x7D00, 0x808080, 0, 0xC8);
        } else if (!zoom_page_hidden(room)) {
            Menu_Sprite(r, 0, 0, 0x7D00, 0x808080, 0, 0xC8);
            drawn = 1;
        }
        draw_mode(0x66, 0xC8);
        U16(r) = (u16)(U16(m + 0x9E) - 0x80); U16(r + 2) = (u16)(U16(m + 0xA0) - 0x80);
        U16(r + 4) = 0xFF; U16(r + 6) = 0xFF;
        if (!drawn) Menu_Sprite(r, 0, 0, 0x7D00, 0x808080, 0, 0xC8);
        draw_mode(7, 0xC8);
    }
    U16(r) = 0; U16(r + 2) = 0; U16(r + 4) = 0x140; U16(r + 6) = 0xF0;
    Menu_Tile(r, 0, 0, 0xC8);
    Tmp_Pop(0x18);
    s16 k = S16(m + 0x92);
    u32 x = 0x12, y = 0x3C;
    if (k < 0x46) {
        x = (u32)(U16(rec + 0xC) - ((s32)(U16(rec + 0xC) - 0x12) * k) / 0x46);
        y = (u32)(U16(rec + 0xE) - ((s32)(U16(rec + 0xE) - 0x3C) * k) / 0x46);
    }
    Menu_DrawArea(x, y, (u32)(k * 2), (u32)(k * 2), 0xC8);
}

/* ---- the areas' map pages (0x73A360 by M+0x62) ---- */

/* ASCII letters 8x8 in OT slot z (colors as given, semi-transparent), then
 * their draw mode (tpage 0x28) */
void DC2CALL Text_AsciiZ(gptr s, u32 x, u32 y, u32 color, u32 z)     /* 0x471C90 */
{
    float x0 = (float)(s32)x;
    long double cx = (s32)x;
    for (u8 c; (c = U8(s)) != 0; s++) {
        if (c == '\n') { y += 8; cx = x0; continue; }
        if (c == ' ') { cx += F32(0x703468); continue; }
        s32 k = (s32)c - 0x21;
        gptr p = U32(0x87DC98);
        F32(p + 8) = (float)cx; F32(p + 0xC) = (float)(s32)y;
        U32(p + 0x10) = 0x3C23D70A;
        U16(p + 0x18) = 8; U16(p + 0x1A) = 8;
        U8(p + 0x14) = (u8)(k << 4); U8(p + 0x15) = (u8)(k & 0xF0);
        U16(p + 0x16) = 0x7DCE;
        U32(p + 4) = color | 0x80000000;
        gptr ot = 0x87BC00 + 4 * ((U32(0x87DE0C) << 10) + z);
        U32(p) = U32(ot) | 0x04000000;
        U32(ot) = p & 0xFFFFFF;
        U32(0x87DC98) = p + 0x1C;
        cx += (long double)F32(0x7373F4 + k * 4) - D64(0x703460);
    }
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0x28, 0);
    Gpu_AddPrim(0x87BC00 + 4 * ((U32(0x87DE0C) << 10) + z), U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}

/* a door's mark: blue shut, red open (flag 8, or none), blinking when its
 * key is had (G+0x108F or flag 9); rect r already set */
static void door_mark(gptr d, gptr r, u32 z)
{
    int st;
    if (!U16(d + 2) || Flag_Check(8, U16(d + 2))) st = 1;
    else if (U16(d + 6) == U8(G() + 0x108F) || Flag_Check(9, U16(d + 4))) st = 2;
    else st = 0;
    if (st == 0) Menu_Tile(r, 0x1010D0, 0, z);
    else if (st == 1) Menu_Tile(r, 0xB03030, 0, z);
    else if (U8(G() + 4) & 4) Menu_Tile(r, Color_Pulse(0x3030A0, 0x80830, 3), 0, z);
}

/* an area's doors (n records of 16 bytes {room, -, flag 8, flag 9, key, x,
 * y, w, h}) in the rooms seen (flags 11 by room) */
void DC2CALL Map_AreaDoors(gptr d, u32 area, u32 n)                 /* 0x49DD50 */
{
    if (!(u16)n) return;
    gptr r = Tmp_Push(8);
    for (u32 k = (u16)n; k; k--, d += 0x10) {
        if (!Flag_Check(0xB, (u8)Room_GlobalIndex(area, U8(d)))) continue;
        U16(r) = U16(d + 8); U16(r + 2) = U16(d + 0xA); U16(r + 4) = U16(d + 0xC); U16(r + 6) = U16(d + 0xE);
        door_mark(d, r, 0x190);
    }
    Tmp_Pop(8);
}

/* one room on an area page (its record at 0x739DE0 + idx*16): green where
 * the player is, grey when seen; blinking red the room G+0x1220 (the
 * monsters' room, while G+0x1222); its outline */
static void room_mark(u32 idx, u32 k, u32 area, u32 cur, int outline)
{
    gptr rec = 0x739DE0 + ((idx & 0xFFFF) << 4), g = G();
    gptr f = U32(rec + 8);
    if (!f) return;
    u32 x = U16(rec + 0xC), y = U16(rec + 0xE);
    if (Flag_Check(0xB, idx & 0xFFFF)) {
        if ((u8)cur == (u8)k && U8(G() + 0x1091) == (u8)area) Menu_DrawFrames(f, x, y, 0x16, 0x7C01, 0x73060, 0x1A9);
        else Menu_DrawFrames(f, x, y, 0x16, 0x7C01, 0x8003103B, 0x1B3);
    }
    g = G();
    if (U8(g + 0x1220) == (u8)k && U8(g + 0x1221) == (u8)area && U8(g + 0x1222) && (U8(g + 4) & 8))
        Menu_DrawFrames(f, x, y, 0x16, 0x7C01, 0x80802020, 0x1A9);
    if (outline) Menu_DrawFrames(f, x, y, 0x16, 0x7C01, 0x80000000, 0x1B8);
}

/* the rooms 0..n-1 of an area */
void DC2CALL Map_Rooms(u32 area, u32 n)                             /* 0x49E210 */
{
    u16 idx = (u8)Room_GlobalIndex(area, 0);
    for (u8 k = 0; (u16)k < (u16)n; k++, idx++) room_mark(idx, k, area, U8(G() + 0x1090), 1);
}

/* rooms first..end-1 of an area (inline in the pages) */
static void rooms(u32 area, u32 first, u32 end)
{
    u16 idx = (u8)Room_GlobalIndex(area, first);
    for (u32 k = first; k < end; k++, idx++) room_mark(idx, k, area, U8(G() + 0x1090), 1);
}

/* a save room's mark (when seen) */
static void save_mark(u32 area, u32 room, u32 x, u32 y)
{
    if (Flag_Check(0xB, (u16)(u8)Room_GlobalIndex(area, room)))
        Menu_DrawFrames(0x65F610, x, y, 0x16, 0x7C8F, Color_Pulse(0x707000, 0xC0C000, 4), 0x19A);
}

/* the page's title: the frame, the area's name (M+0x62), a line under it */
void DC2CALL Map_Legend(gptr m, u32 x, u32 y)                       /* 0x49D360 */
{
    static const u32 names[11] = {0x7276CE, 0x72772C, 0x727A66, 0x727B7C, 0x727CB4, 0x727EDA,
                                  0x727EDA, 0x727FEE, 0x728202, 0x7283CE, 0x72856E};
    s32 sx = (s16)x, sy = (s16)y;
    Menu_DrawFrames(0x65F508, (u32)(sx + 0xE), (u32)(sy + 1), 0x16, 0x7C01, 0x707070, 0x1F4);
    Menu_DrawFrames(0x65F518, (u32)sx, (u32)(sy + 3), 0x16, 0x7C01, 0x80108030, 0x1F4);
    Text_Draw(names[U8(m + 0x62)], x + 0x50, y, 0x64, 0x707070, 0x1F4, 0);
    Menu_Line(x + 0x3C, y + 0xC, x + 0x40, y + 0x10, 0x30C060, 0, 0x1F4);
    Menu_Line(x + 0x40, y + 0x10, x + 0x100, y + 0x10, 0x30C060, 0, 0x1F4);
}

/* the key: open door, shut door, save room */
void DC2CALL Map_Key(gptr m, u32 x, u32 y)                          /* 0x49D480 */
{
    u32 xs = x & 0xFFFF, ys = y & 0xFFFF;
    gptr r = Tmp_Push(8);
    Text_AsciiZ(0x73AF28, xs + 0xA, ys - 1, 0x707070, 0x1B8);
    U16(r) = (u16)x; U16(r + 2) = (u16)y; U16(r + 4) = 6; U16(r + 6) = 6;
    Menu_Tile(r, 0xB03030, 0, 0x190);
    Text_AsciiZ(0x73AF1C, xs + 0xA, ys + 7, 0x707070, 0x1B8);
    U16(r) = (u16)x; U16(r + 2) = (u16)(y + 8);
    Menu_Tile(r, 0x1010D0, 0, 0x1B8);
    Text_AsciiZ(0x73AF10, xs + 0xA, ys + 0xF, 0x707070, 0x1B8);
    Menu_DrawFrames(0x65F610, xs - 3, ys + 0xF, 0x16, 0x7C8F, Color_Pulse(0x707000, 0xC0C000, 4), 0x19A);
    U16(r) = (u16)(x - 4); U16(r + 2) = (u16)(y - 2); U16(r + 4) = 0x60; U16(r + 6) = 0x1C;
    Menu_Tile(r, 0, 1, 0x1B8);
    Tmp_Pop(8);
}

/* the player's arrow on an area page (his area only) at x, y */
void DC2CALL Map_Arrow(gptr m, u32 x, u32 y)                        /* 0x49DFB0 */
{
    static const s16 pts[4][2] = {{0, 0}, {0, -2000}, {500, 1000}, {-500, 1000}};
    gptr g = G();
    if ((u8)Area_SoundArea(U8(g + 0x1091), U8(g + 0x1090)) != U8(m + 0x62)) return;
    gptr f = Tmp_Push(0x50), r = f, sv = f + 8, tri = f + 0x28, mt = f + 0x30;
    u16 room = U16(g + 0x1090);
    s16 a = (s16)-S16(U32(g + 0x940) + 0x4A);
    U16(r) = 0; U16(r + 4) = 0;
    U16(r + 2) = (u16)(room == 0x40A ? a + 0xF800 : room == 0x204 ? a + 0xF3E8 : a);
    RotMatrix(r, mt);
    for (u32 i = 0; i < 4; i++) {
        gptr v = sv + 8 * i;
        U16(v) = (u16)pts[i][0]; U16(v + 2) = 0; U16(v + 4) = (u16)pts[i][1];
        ApplyMatrixSV(mt, v, v);
        U16(v) = (u16)(S16(v) / 100 + x); U16(v + 2) = 0;
        U16(v + 4) = (u16)(S16(v + 4) / 100 + y);
    }
    for (u32 i = 0; i < 3; i++) { U16(tri + 4 * i) = U16(sv + 8 * (i + 1)); U16(tri + 4 * i + 2) = U16(sv + 8 * (i + 1) + 4); }
    Menu_PolyF3(tri, Color_Pulse(0x40C0C0, 0x208080, 4), 0x1AE);
    Tmp_Pop(0x50);
    draw_mode(0x15, 0x1AE);
    Menu_DrawFrames(0x65F548, x & 0xFFFF, y & 0xFFFF, 0x16, 0x7C8F, 0x707070, 0x1B8);
    Menu_DrawFrames(0x65F550, x & 0xFFFF, y & 0xFFFF, 0x16, 0x7C8F, 0x7080, 0x1B8);
    Menu_DrawFrames(0x65F658, x & 0xFFFF, y & 0xFFFF, 0x16, 0x7C8F, 0x80000000, 0x1B8);
}

/* the page's frame: backdrop (M+0x92), play time and saves, the cursor
 * gliding to the player's room (M+0x9A, M+0x9C over M+0x96 frames, in
 * his area's page), borders, and the paper's texture (fading by M+0x94) */
void DC2CALL Map_Frame(gptr m)                                      /* 0x49D5D0 */
{
    gptr r = Tmp_Push(8), g;
    s32 z = S16(m + 0x92);
    U16(r) = 0; U16(r + 2) = 0x30; U16(r + 4) = 0x140; U16(r + 6) = 0x9F;
    Menu_Tile(r, (u32)(z << 8 | z), 3, 0xD2);
    Menu_Number8(0x10, 0xD2, 6, 1, 0x80303030, U32(0x87DC04) % 999999, 0x1F4);
    Menu_Number8(0x104, 0xD2, 6, 1, 0x80303030, U32(0x87DC9C) % 999999, 0x1F4);
    g = G();
    u8 area = (u8)Area_SoundArea(U8(g + 0x1091), U8(g + 0x1090));
    u8 p = U8(m + 0x62), a = p > 4 ? (u8)(p - 1) : p;
    if (area == p && U8(G() + 0x1090) == U8(m + 0x63)) {
        gptr rec = 0x739DE0 + ((u8)Room_GlobalIndex(a, U8(m + 0x63)) << 4);
        Menu_Number8(0xFA, 0x2A, 4, 1, 0x80707000, (u32)(s32)S16(m + 0x9A), 0x1F4);
        Menu_Number8(0x118, 0x2A, 4, 1, 0x80007070, (u32)(s32)S16(m + 0x9C), 0x1F4);
        U16(m + 0x96)--;
        if (S16(m + 0x96) <= 0) {
            U16(m + 0x96) = 0;
            U16(m + 0x9A) = U16(rec + 0xC); U16(m + 0x9C) = U16(rec + 0xE);
        } else {
            s32 t = S16(m + 0x96);
            s32 dy = ((s32)U16(rec + 0xE) - S16(m + 0x9C)) / t;
            s32 dx = ((s32)U16(rec + 0xC) - S16(m + 0x9A)) / t;
            U16(m + 0x9A) += (u16)dx; U16(m + 0x9C) += (u16)dy;
            Se_Play(0, 0xC);
        }
        u16 cx = U16(m + 0x9A), cy = U16(m + 0x9C);
        Menu_Line(0, cy, 0x140, cy, 0x707070, 1, 0x1AE);
        Menu_Line(cx, 0x30, cx, 0xD0, 0x707070, 1, 0x1AE);
        cx = U16(m + 0x9A); cy = U16(m + 0x9C);
        Menu_Line((u16)(cx - 5), (u16)(cy - 5), (u16)(cx - 5), (u16)(cy + 5), Color_Pulse(0x8080, 0x6060, 4), 1, 0x1AE);
        cx = U16(m + 0x9A); cy = U16(m + 0x9C);
        Menu_Line((u16)(cx - 5), (u16)(cy - 5), (u16)(cx + 5), (u16)(cy - 5), Color_Pulse(0x8080, 0x6060, 4), 1, 0x1AE);
        cx = U16(m + 0x9A); cy = U16(m + 0x9C);
        Menu_Line((u16)(cx + 5), (u16)(cy + 5), (u16)(cx - 5), (u16)(cy + 5), Color_Pulse(0x8080, 0x6060, 4), 1, 0x1AE);
        cx = U16(m + 0x9A); cy = U16(m + 0x9C);
        Menu_Line((u16)(cx + 5), (u16)(cy + 5), (u16)(cx + 5), (u16)(cy - 5), Color_Pulse(0x8080, 0x6060, 4), 1, 0x1AE);
        draw_mode(0xC, 0x1AE);
    }
    Menu_Line(0x10, 0x34, 0x130, 0x34, 0x808080, 1, 0x1C2);
    Menu_Line(0x10, 0x35, 0x10, 0x38, 0x808080, 1, 0x1C2);
    Menu_Line(0x130, 0x35, 0x130, 0x38, 0x808080, 1, 0x1C2);
    Menu_Line(0x10, 0xCC, 0x130, 0xCC, 0x808080, 1, 0x1C2);
    Menu_Line(0x10, 0xC8, 0x10, 0xCB, 0x808080, 1, 0x1C2);
    Menu_Line(0x130, 0xC8, 0x130, 0xCB, 0x808080, 1, 0x1C2);
    draw_mode(0xC, 0x1C2);
    Menu_Line(0, 0x30, 0x140, 0x30, 0x808080, 0, 0x1C2);
    Menu_Line(0, 0xD0, 0x140, 0xD0, 0x808080, 0, 0x1C2);
    s32 f = 0x80 - S16(m + 0x94);
    u32 e1 = (u32)((f << 7) / 128) & 0xFFFF, e2 = (u32)((f * 80) / 128) & 0xFFFF;
    u32 c1 = ((e1 << 8 | e1) << 8) | e1, c2 = ((e2 << 8 | e2) << 8) | e2;
    U16(r) = 0; U16(r + 2) = 0x30; U16(r + 4) = 0x100; U16(r + 6) = 0xA0;
    Menu_Sprite(r, 0, 0x30, 0x7C00, c1, 0, 0x1C2);
    draw_mode(0x15, 0x1C2);
    U16(r) = 0; U16(r + 2) = 0; U16(r + 4) = 0x100; U16(r + 6) = 0x30;
    Menu_Sprite(r, 0, 0, 0x7C00, c2, 0, 0x320);
    U16(r + 2) = 0xD0; U16(r + 6) = 0x20;
    Menu_Sprite(r, 0, 0xD0, 0x7C00, c2, 0, 0x320);
    draw_mode(0x15, 0x320);
    U16(r) = 0x100; U16(r + 2) = 0x30; U16(r + 4) = 0x40; U16(r + 6) = 0xA0;
    Menu_Sprite(r, 0, 0x30, 0x7C00, c1, 0, 0x1C2);
    draw_mode(0x16, 0x1C2);
    U16(r) = 0x100; U16(r + 2) = 0; U16(r + 4) = 0x40; U16(r + 6) = 0x30;
    Menu_Sprite(r, 0, 0, 0x7C00, c2, 0, 0x320);
    U16(r + 2) = 0xD0; U16(r + 6) = 0x20;
    Menu_Sprite(r, 0, 0xD0, 0x7C00, c2, 0, 0x320);
    draw_mode(0x16, 0x320);
    Tmp_Pop(8);
}

static void page_end(gptr m) { Map_Legend(m, 0x18, 0x16); Map_Frame(m); }

void DC2CALL Map_Area0(gptr m)                                      /* 0x49D170 */
{
    Map_AreaDoors(0x73A5A0, 0, 7);
    Map_Key(m, 0xD4, 0xAA);
    save_mark(0, 3, 0x42, 0x97);
    u16 here = U16(G() + 0x1090);
    u8 cur = here == 4 || here == 5 ? 0 : U8(G() + 0x1090);   /* (rooms 4, 5 shown as room 0) */
    u16 idx = (u8)Room_GlobalIndex(0, 0);
    for (u32 k = 0; k < 6; k++, idx++) room_mark(idx, k, 0, cur, k != 4 && k != 5);
    page_end(m);
}

void DC2CALL Map_Area1(gptr m)                                      /* 0x49DE90 */
{
    Map_AreaDoors(0x73A610, 1, 0x28);
    Map_Arrow(m, 0x30, 0x98);
    Map_Key(m, 0xD4, 0xB2);
    Text_AsciiZ(0x73AF68, 0x34, 0xBC, 0x505050, 0x19A);
    Text_AsciiZ(0x73AF54, 0x7E, 0xBC, 0x505050, 0x19A);
    Text_AsciiZ(0x73AF44, 0x10F, 0xA0, 0x505050, 0x19A);
    Text_AsciiZ(0x73AF30, 0x10, 0x52, 0x505050, 0x19A);
    save_mark(1, 6, 0x87, 0x3D);
    Map_Rooms(1, 0x11);
    page_end(m);
}

void DC2CALL Map_Area2(gptr m)                                      /* 0x49E360 */
{
    Map_AreaDoors(0x73A890, 2, 0xC);
    Map_Arrow(m, 0x38, 0x98);
    Map_Key(m, 0xD4, 0xAA);
    Text_AsciiZ(0x73AF78, 0xFE, 0x5D, 0x505050, 0x19A);
    save_mark(2, 3, 0xA8, 0x96);
    Map_Rooms(2, 6);
    page_end(m);
}

void DC2CALL Map_Area3(gptr m)                                      /* 0x49E430 */
{
    Map_AreaDoors(0x73A950, 3, 0xF);
    Map_Arrow(m, 0x38, 0x98);
    Map_Key(m, 0xD4, 0x3A);
    Text_AsciiZ(0x73AF78, 0x82, 0x3D, 0x505050, 0x19A);
    save_mark(3, 1, 0x6A, 0x4A);
    save_mark(3, 4, 0x92, 0x9F);
    Map_Rooms(3, 7);
    page_end(m);
}

/* area 4 above ground (its rooms 7..10 on the next page) */
void DC2CALL Map_Area4(gptr m)                                      /* 0x49E550 */
{
    Map_AreaDoors(0x73AA40, 4, 0xB);
    Map_Arrow(m, 0x30, 0xA0);
    Map_Key(m, 0xD4, 0x3A);
    Text_AsciiZ(0x73AF78, 0xA3, 0x36, 0x505050, 0x19A);
    save_mark(4, 3, 0xCB, 0x7C);
    save_mark(4, 0xB, 0x85, 0x3A);
    u16 idx = (u8)Room_GlobalIndex(4, 0);
    for (u32 k = 0; k < 0xC; k++, idx++)
        if (k < 7 || k > 10) room_mark(idx, k, 4, U8(G() + 0x1090), 1);
    page_end(m);
}

void DC2CALL Map_Area4b(gptr m)                                     /* 0x49E790 */
{
    Map_AreaDoors(0x73AAF0, 4, 5);
    Map_Arrow(m, 0x30, 0xA0);
    Map_Key(m, 0xD4, 0xAA);
    rooms(4, 8, 0xB);
    page_end(m);
}

void DC2CALL Map_Area5(gptr m)                                      /* 0x49E900 */
{
    Map_AreaDoors(0x73AB40, 5, 9);
    Map_Arrow(m, 0x38, 0xA0);
    Map_Key(m, 0xD4, 0xAA);
    save_mark(5, 1, 0x7D, 0x5E);
    Map_Rooms(5, 5);
    page_end(m);
}

/* area 6: rooms 0..5, or 6, 7 (below) when the player is in 0x606/0x607 */
void DC2CALL Map_Area6(gptr m)                                      /* 0x49E9B0 */
{
    u16 here = U16(G() + 0x1090);
    int below = here == 0x606 || here == 0x607;
    gptr r = Tmp_Push(8);
    for (gptr d = 0x73ABD0; d < 0x73ABD0 + 0x11 * 0x10; d += 0x10) {
        if ((U16(d) >= 6) != below) continue;
        if (!Flag_Check(0xB, (u8)Room_GlobalIndex(6, U8(d)))) continue;
        U16(r) = U16(d + 8); U16(r + 2) = U16(d + 0xA); U16(r + 4) = U16(d + 0xC); U16(r + 6) = U16(d + 0xE);
        door_mark(d, r, 0x190);
    }
    Tmp_Pop(8);
    Text_AsciiZ(0x73AF44, 0x10, 0x58, 0x505050, 0x19A);
    Map_Arrow(m, 0x78, 0x54);
    Map_Key(m, 0xD4, 0xAA);
    here = U16(G() + 0x1090);
    if (here != 0x606 && here != 0x607) save_mark(6, 5, 0xDA, 0x43);
    here = U16(G() + 0x1090);
    if (here != 0x606 && here != 0x607) rooms(6, 0, 6);
    else rooms(6, 6, 8);
    page_end(m);
}

void DC2CALL Map_Area7(gptr m)                                      /* 0x49EF50 */
{
    Map_AreaDoors(0x73ACE0, 7, 0x10);
    Map_Arrow(m, 0x38, 0x58);
    Map_Key(m, 0x78, 0x3A);
    save_mark(7, 1, 0x4A, 0xA7);
    save_mark(7, 5, 0xF2, 0x92);
    Map_Rooms(7, 8);
    page_end(m);
}

void DC2CALL Map_Area8(gptr m)                                      /* 0x49F050 */
{
    Map_AreaDoors(0x73ADE0, 8, 0xE);
    Text_AsciiZ(0x73AF44, 0x10, 0xBD, 0x505050, 0x19A);
    Map_Arrow(m, 0x38, 0x58);
    Map_Key(m, 0xD4, 0xAA);
    save_mark(8, 2, 0x20, 0x92);
    save_mark(8, 8, 0x101, 0x6E);
    save_mark(8, 9, 0x117, 0x7B);
    Map_Rooms(8, 0xC);
    page_end(m);
}

void DC2CALL Map_Area9(gptr m)                                      /* 0x49F1C0 */
{
    Map_AreaDoors(0x73AEC0, 9, 5);
    Map_Arrow(m, 0x38, 0xA0);
    Map_Key(m, 0x70, 0xAA);
    save_mark(9, 1, 0xDB, 0x82);
    Map_Rooms(9, 5);
    page_end(m);
}

/* ---- mode 10: an item picked up (M+0x38 the item, +0x3C its flag 5, +0x3E its zone and object) ---- */

/* the item's zone (low byte, 0xFF none) disabled and its object (high
 * byte, 0xFF none) cleared */
void DC2CALL Pickup_Clear(u32 v)                                    /* 0x49F9F0 */
{
    if ((u8)v != 0xFF) U8(U32(G() + 0xA5C) + (v & 0xFF) * 40) = 0;
    if ((v & 0xFF00) != 0xFF00) U32(U32(G() + 0x93C) + ((v & 0xFFFF) >> 8) * 0x70 + 0x50) = 0;
}

/* a line of text typed out (M+0x92 letters) centred at 0x78; nonzero once all shown */
static u32 pickup_text(gptr m, gptr t)
{
    return Text_Draw(t, Text_Left(t), 0x78, U16(m + 0x92), 0x808080, 1, 1);
}

/* the item's picture, the letters counted on every other frame up to 200 */
static void pickup_picture(gptr m)
{
    gptr r = Tmp_Push(8);
    U16(r) = 0x60; U16(r + 2) = 0x28; U16(r + 4) = 0x80; U16(r + 6) = 0x40;
    Menu_Sprite(r, 0, 0xC0, 0x7C40, 0x808080, 0, 1);
    Tmp_Pop(8);
    draw_mode(0x8C, 1);
    if (U8(G() + 4) & 1) U16(m + 0x92)++;
    if (S16(m + 0x92) > 0xC8) U16(m + 0x92) = 0xC8;
}

/* "You got the ..." */
static gptr you_got(gptr buf, u32 id)
{
    Msg_Concat(buf, 0x724446, U32(0x704260 + id * 12));
    Msg_Concat(buf, buf, 0x723FCC);
    return buf;
}

static void picked_up(gptr m)
{
    Pickup_Clear(U16(m + 0x3E));
    U32(m) = 0x502; U8(m + 0x64) = 0;
}

/* step 0 the screen torn while the picture loads; 1 the item: a medicine
 * asks whether to take it (none if no room), a memo is read at once
 * (screen 3), anything else is just had (the 0x2F ammunition box fills
 * the weapons); 2 the medicine taken */
void DC2CALL MenuMode_Pickup(gptr m)                                /* 0x49F270 */
{
    U8(m + 0x65) = 2;
    U32(G() + 0xB14) = U32(m + 0x68);
    gptr buf = Tmp_Push(0x200);
    switch ((s8)U8(m + 2)) {
    case 0:
        for (u32 i = 0; i < 0x1E; i++) {
            s32 y = (s32)(Rand8() & 0xFF) % 0xF0;
            s32 dx = (s32)(Rand8() & 0xFF) % 4 - 2;
            Menu_ShiftLine((u32)dx, (u32)y, 1);
        }
        if (!File_IsBusy()) { U16(m + 0x92) = 0; U8(m + 0x4E) = 0; U8(m + 2)++; }
        break;
    case 1: {
        pickup_picture(m);
        u16 id = U16(m + 0x38);
        if (id >= 0x1A && id < 0x20) {
            if ((s8)Item_FindRoom(G() + 0x11AC, 6, (u8)id) < 0) {
                Msg_Concat(buf, 0x724468, 0x723FC8);
                if (pickup_text(m, buf) && (U8(0x87DC00) & 0x60)) { U32(m) = 0x502; U8(m + 0x64) = 0; }
                break;
            }
            Msg_Concat(buf, 0x724420, U32(0x704260 + id * 12));
            Msg_Concat(buf, buf, 0x723FD0);
            if (!pickup_text(m, buf)) break;
            if (U8(m + 0x45) & 0xA0) {
                U8(m + 0x4E) ^= 1;
                Se_Play(0, 0);
            } else if (U8(0x87DC00) & 0x40) {
                U32(m) = 0x502; U8(m + 0x64) = 0;
            } else if (U8(0x87DC00) & 0x20) {
                if (U8(m + 0x4E)) { U32(m) = 0x502; U8(m + 0x64) = 0; }
                else { Se_Play(0, 2); U8(m + 2) = 2; U16(m + 0x92) = 0; }
            }
            Menu_YesNo(0xB4, 0x8C, (u32)(s32)(s8)U8(m + 0x4E), 1);
        } else if (id >= 0x50 && id < 0x80) {
            Flag_Set(9, id, 1);
            Flag_Set(5, U16(m + 0x3C), 1);
            Pickup_Clear(U16(m + 0x3E));
            U8(m) = 2; U8(m + 1) = 3; U8(m + 2) = 2; U8(m + 3) = 0;
            U8(m + 0x57) = (u8)(U8(m + 0x38) - 0x50);
            u8 n = 0;
            for (u8 b = 0x50; (u16)b < U16(m + 0x38); b++) if (Flag_Check(9, b)) n++;
            U8(m + 0x55) = 0; U8(m + 0x56) = n;
            Game_Start(3);
            U8(G() + 0xB48) = 3;
        } else {
            if (!pickup_text(m, you_got(buf, id)) || !(U8(0x87DC00) & 0x60)) break;
            Flag_Set(5, U16(m + 0x3C), 1);
            if (U16(m + 0x38) == 0x2F) {
                gptr g = G();
                if ((u8)Item_Find(g + 0x10BC, 0x14, 0x2F) == 0xFF) {
                    gptr w = G() + ((s8)Item_FindFree(G() + 0x10BC, 0x14) + 0x165) * 12;
                    U8(w) = 1; U8(w + 1) = 0x19; U16(w + 2) = 0; U32(w + 4) = 0;
                    U16(w + 8) = 9999; U16(w + 0xA) = 9999;
                }
                static const u16 slot[3][2] = {{0x10B4, 0x902}, {0x10B6, 0x902}, {0x10B8, 0x901}};
                for (u32 k = 0; k < 3; k++) {
                    gptr s = G() + slot[k][0];
                    u16 v = U16(s);
                    U16(s) = (U8(0x70426A + (v & 0xFF) * 12) & 3) ? slot[k][1] : (u16)((v & 0xFF) | 0x900);
                }
            } else Flag_Set(9, U16(m + 0x38), 1);
            picked_up(m);
        }
        break; }
    case 2: {
        pickup_picture(m);
        u8 s = (u8)Item_FindRoom(G() + 0x11AC, 6, U8(m + 0x38));
        U8(m + 0x4F) = s;
        if (!pickup_text(m, you_got(buf, U16(m + 0x38))) || !(U8(0x87DC00) & 0x60)) break;
        gptr it = G() + ((s8)s + 0x179) * 12;
        U8(it) = 1; U16(it + 8)++; U8(it + 1) = U8(m + 0x38); U16(it + 2) = 0; U32(it + 4) = 0;
        u8 d = U8(G() + 0x108E);
        U16(it + 0xA) = d == 0 ? 5 : d == 1 ? 2 : 1;
        Flag_Set(9, U16(m + 0x38), 1);
        Flag_Set(5, U16(m + 0x3C), 1);
        picked_up(m);
        break; }
    }
    Tmp_Pop(0x200);
    Menu_Frame(m);
}

/* ---- mode 11: "Set the destination" (steps 0x73AF80 by M+2; G+0x10AA where
 * the player is: 0, 1, 2; M+0x4F the target chosen of the two others) ---- */

/* a target's view: lines growing out from its mark (M+0x94, M+0x96), its
 * name typed (M+0x98), the area's name, three pictures of it in turn
 * (M+0x9A, 200 frames each, fading in) */
struct dest_view {
    u16 ax, ay; s8 adx, ady; u16 bx, by;
    gptr label; u16 lx, ly; gptr frame; u16 fx, fy; gptr name;
    u16 rx, ry; u8 uv[3][2]; u16 clut, tpage; u8 all;   /* all: the third picture stays to the end */
};
static const struct dest_view g_dest_views[3] = {
    {0x78, 0x4C, -1, -1, 0x68, 0x3C, 0x73AFBC, 0x1C, 0x3E, 0x65F768, 0x1D, 0x33, 0x72772C, 0x29, 0x44,
     {{0, 0}, {0x40, 0}, {0, 0x40}}, 0x7C40, 0x87, 0},
    {0xAD, 0x5B, 1, -1, 0xBD, 0x4B, 0x73AFCC, 0xC1, 0x4D, 0x65F758, 0xC2, 0x38, 0x727FEE, 0xD8, 0x52,
     {{0x40, 0x40}, {0, 0x80}, {0x40, 0x80}}, 0x7C40, 0x87, 1},
    {0xC7, 0x6F, 1, 1, 0xD7, 0x7F, 0x73AFAC, 0xDB, 0x81, 0x65F770, 0xDC, 0x76, 0x72856E, 0xDA, 0x44,
     {{0, 0x80}, {0x40, 0x80}, {0, 0xC0}}, 0x7C00, 0x86, 0}};

static void dest_view(gptr m, const struct dest_view *v)
{
    u16 a = U16(m + 0x94), b = U16(m + 0x96);
    if (a) Menu_Line(v->ax, v->ay, (u32)(v->ax + v->adx * a), (u32)(v->ay + v->ady * a), 0xC0, 0, 0x32);
    if (b) Menu_Line(v->bx, v->by, (u32)(v->bx + (v->adx < 0 ? -b : b)), v->by, 0xC0, 0, 0x32);
    draw_mode(0xC, 0x32);
    if (U16(m + 0x98)) {
        Menu_SmallText(v->label, v->lx, v->ly, U16(m + 0x98), 0x808080, 1);
        Menu_DrawFrames(v->frame, v->fx, v->fy, 0x26, 0x7C0E, 0x808080, 0x64);
    }
    Text_Draw(v->name, 0x70, 0xA8, 0x64, 0x808080, 0xA, 0);
    U16(m + 0x9A)++;
    if (S16(m + 0x9A) > 0x258) U16(m + 0x9A) = 0;
    s16 t = S16(m + 0x9A);
    s32 p = t % 200;
    u32 c = p < 0x20 ? (u32)p << 10 | (u32)p << 18 | (u32)p << 2 : 0x808080;
    int k = t < 0xC8 ? 0 : t < 0x190 ? 1 : (t < 0x258 || v->all) ? 2 : -1;
    if (k >= 0) {
        gptr r = Tmp_Push(8);
        U16(r) = v->rx; U16(r + 2) = v->ry; U16(r + 4) = 0x40; U16(r + 6) = 0x30;
        if (p < 0x20) Menu_Sprite(r, v->uv[k][0], v->uv[k][1], v->clut, c, 1, 0x64);
        else Menu_Sprite(r, v->uv[k][0], v->uv[k][1], v->clut, 0x808080, 0, 0x64);
        Tmp_Pop(8);
    }
    draw_mode(v->tpage, 0x64);
}

void DC2CALL Dest_View1(gptr m) { dest_view(m, &g_dest_views[0]); }  /* 0x4A1F70 */
void DC2CALL Dest_View2(gptr m) { dest_view(m, &g_dest_views[1]); }  /* 0x4A2250 */
void DC2CALL Dest_View3(gptr m) { dest_view(m, &g_dest_views[2]); }  /* 0x4A26A0 */

/* the panel for where the player is: three marks (his own lit), the two
 * targets blinking by which is chosen, its view */
struct dest_mark { u32 frame; u16 x, y; u8 tp, which; };
static void dest_panel(gptr m, const u32 dots[3], void (DC2CALL *v1)(gptr), void (DC2CALL *v0)(gptr),
                       const struct dest_mark mk[5])
{
    static const u16 pos[3][2] = {{0xC8, 0x70}, {0x78, 0x4C}, {0xAE, 0x5C}};
    for (u32 i = 0; i < 3; i++) Menu_DrawFrames(0x65F708, pos[i][0], pos[i][1], 5, 0x7C0E, dots[i], 0x1E);
    u32 c[4] = {0x80203010, 0x80000020, 0x80203010, 0x80000020};   /* esi, edi, ebx, ebp */
    if (U8(m + 0x4F)) {
        c[2] = Color_Pulse(0x80A0C040, 0x80607030, 4);
        c[3] = Color_Pulse(0x80000070, 0x80000030, 5);
        v1(m);
    } else {
        c[0] = Color_Pulse(0x80A0C040, 0x80607030, 4);
        c[1] = Color_Pulse(0x80000070, 0x80000030, 5);
        v0(m);
    }
    for (u32 i = 0; i < 5; i++)
        Menu_DrawFrames(mk[i].frame, mk[i].x, mk[i].y, mk[i].tp, 0x7C0E, mk[i].which < 4 ? c[mk[i].which] : 0x80005050,
                        mk[i].tp == 0x26 ? 0x104 : 0xC8);
}

void DC2CALL Dest_Panel0(gptr m)                                    /* 0x4A1DF0 */
{
    static const u32 dots[3] = {0x80, 0x8000, 0x8000};
    static const struct dest_mark mk[5] = {{0x65F720, 0x78, 0x4B, 0x26, 0}, {0x65F528, 0x28, 0x2A, 0x25, 1},
        {0x65F738, 0xAA, 0x5F, 0x26, 2}, {0x65F530, 0xA3, 0x58, 0x25, 3}, {0x65F520, 0xBD, 0x30, 0x25, 4}};
    dest_panel(m, dots, Dest_View2, Dest_View1, mk);
}

void DC2CALL Dest_Panel1(gptr m)                                    /* 0x4A2530 */
{
    static const u32 dots[3] = {0x8000, 0x80, 0x8000};
    static const struct dest_mark mk[5] = {{0x65F718, 0x7C, 0x48, 0x26, 0}, {0x65F520, 0xBD, 0x30, 0x25, 1},
        {0x65F730, 0x7C, 0x40, 0x26, 2}, {0x65F530, 0xA3, 0x58, 0x25, 3}, {0x65F528, 0x28, 0x2A, 0x25, 4}};
    dest_panel(m, dots, Dest_View2, Dest_View3, mk);
}

void DC2CALL Dest_Panel2(gptr m)                                    /* 0x4A2990 */
{
    static const u32 dots[3] = {0x8000, 0x8000, 0x80};
    static const struct dest_mark mk[5] = {{0x65F740, 0xAB, 0x62, 0x26, 0}, {0x65F520, 0xBD, 0x30, 0x25, 1},
        {0x65F728, 0x7A, 0x41, 0x26, 2}, {0x65F528, 0x28, 0x2A, 0x25, 3}, {0x65F530, 0xA3, 0x58, 0x25, 4}};
    dest_panel(m, dots, Dest_View1, Dest_View3, mk);
}

static void dest_frames(void);

/* the screen: lines opening (M+0x92: 0x94 to 16, 0x96 to 80, 0x98 to 100),
 * the frame, the panel for G+0x10AA, the credits in small letters */
void DC2CALL Dest_Frame(gptr m)                                     /* 0x4A1B20 */
{
    u16 st = U16(m + 0x92);
    switch ((s16)st) {
    case 0: U16(m + 0x94)++; if (S16(m + 0x94) > 0x10) { U16(m + 0x94) = 0x10; U16(m + 0x92) = (u16)(st + 1); } break;
    case 1: U16(m + 0x96) += 4; if (S16(m + 0x96) > 0x50) { U16(m + 0x96) = 0x50; U16(m + 0x92) = (u16)(st + 1); } break;
    case 2: U16(m + 0x98)++; if (S16(m + 0x98) > 0x64) { U16(m + 0x98) = 0x64; U16(m + 0x92) = (u16)(st + 1); } break;
    }
    gptr r = Tmp_Push(8);
    U16(r) = 0x20; U16(r + 2) = 0x20; U16(r + 4) = 0x100; U16(r + 6) = 0x88;
    Menu_Sprite(r, 0, 0, 0x7C0D, 0x808080, 0, 0x1F4);
    Tmp_Pop(8);
    draw_mode(5, 0x1F4);
    Menu_DrawFrames(0x65F500, 0x110, 0x68, 5, 0x7C0E, 0x80B0C040, 0x12C);
    Menu_DrawFrames(0x65F510, 0x10, 0x68, 5, 0x7C0E, 0x80B0C040, 0x12C);
    u8 at = U8(G() + 0x10AA);
    if (at == 0) Dest_Panel0(m);
    else if (at == 1) Dest_Panel1(m);
    else Dest_Panel2(m);
    dest_frames();
    Menu_SmallText(0x73AFDC, 0x10, 0x50, 0x3E8, 0x606060, 0x104);   /* (a copy of it in the original) */
}

/* the step, the grid, the screen shaken while M+0x8E counts down */
void DC2CALL MenuMode_Dest(gptr m)                                  /* 0x49FA50 */
{
    U8(m + 0x65) = 2;
    Orig_Call1(U32(0x73AF80 + (s8)U8(m + 2) * 4), m);
    for (u32 i = 1; i < 0x13; i++) Menu_Line(i << 4, 0, i << 4, 0xF0, 0x101010, 1, 0xA);
    for (u32 i = 1; i < 0xF; i++) Menu_Line(0, i << 4, 0x140, i << 4, 0x101010, 1, 0xA);
    draw_mode(0x25, 0xA);
    if (!U16(m + 0x8E)) return;
    for (u8 i = 0; (s32)i < S16(m + 0x8E) * 3; i++) {
        s32 y = (s32)(Rand8() & 0xFF) % 0x88;
        s32 dx = (s32)(Rand8() & 0xFF) % 4 - 2;
        Menu_ShiftLine((u32)dx, (u32)(y + 0x20), 1);
    }
    U16(m + 0x8E)--;
}

static void dest_reset(gptr m)
{
    U16(m + 0x92) = 0; U16(m + 0x94) = 0; U16(m + 0x96) = 0; U16(m + 0x98) = 0;
    U16(m + 0x9A) = 0; U16(m + 0x9C) = 0; U16(m + 0x8E) = 0x1E;
}

/* step 0 */
void DC2CALL Dest_Init(gptr m)                                      /* 0x49FB80 */
{
    U8(m + 2) = 1; U8(m + 0x4F) = 0; U8(m + 0x4E) = 0;
    dest_reset(m);
    U16(m + 0x80) = 0; U16(m + 0x82) = 0; U16(m + 0x90) = 0;
    Dest_Frame(m);
}

/* step 1: the pad switches the target; select asks, cancel leaves */
void DC2CALL Dest_Choose(gptr m)                                    /* 0x49FBE0 */
{
    if (U8(m + 0x45) & 0xF0) {
        Se_Play(0, 0xD);
        U8(m + 0x4F) ^= 1;
        dest_reset(m);
    } else if (U8(0x87DC00) & 0x40) {
        U32(m) = 0x502; U8(m + 0x64) = 0;
    } else if (U8(0x87DC00) & 0x20) {
        U16(m + 0x8E) = 0x1E; U8(m + 2) = 2; U8(m + 0x4E) = 0;
        Se_Play(0, 2);
    }
    Text_Draw(0x724744, 0x20, 0xBC, 0x64, 0x808080, 2, 0);
    Dest_Frame(m);
}

/* step 2: "Are you sure?"; yes with the vehicle working (M+0x38) sets off
 * (G+0x10AA the target, step 10), without it step 3 */
void DC2CALL Dest_Confirm(gptr m)                                   /* 0x49FC90 */
{
    Text_Draw(0x724502, 0x20, 0xBC, 0x64, 0x808080, 2, 0);
    Menu_YesNo(0xB4, 0xCC, (u32)(s32)(s8)U8(m + 0x4E), 1);
    Dest_Frame(m);
    if (U8(m + 0x45) & 0xA0) { Se_Play(0, 0); U8(m + 0x4E) ^= 1; return; }
    u8 pad = U8(0x87DC00);
    if (pad & 0x40) { U16(m + 0x8E) = 0x1E; Se_Play(0, 1); U8(m + 2) = 1; return; }
    if (!(pad & 0x20)) return;
    U8(m + 2) = 1;
    if (U8(m + 0x4E)) { Se_Play(0, 1); return; }
    if (!U16(m + 0x38)) { U8(m + 2) = 3; return; }
    Se_Play(0, 2);
    U8(G() + 0x10AA) = U8(m + 0x4F) ? 2 : 0;
    U8(m + 2) = 0xA;
}

/* step 3: the route shown (steps 4..9 by where from and which target) */
void DC2CALL Dest_Route(gptr m)                                     /* 0x49FD80 */
{
    U16(m + 0x80) = 0; U16(m + 0x82) = 0;
    u8 at = U8(G() + 0x10AA), t = U8(m + 0x4F);
    if (at == 0) U8(m + 2) = t ? 7 : 6;
    else if (at == 1) U8(m + 2) = t ? 5 : 4;
    else if (at == 2) U8(m + 2) = t ? 8 : 9;
    Dest_Frame(m);
}

/* step 10 */
void DC2CALL Dest_Leave(gptr m)                                     /* 0x4A1B10 */
{
    U32(m) = 0x502;
}

/* the screen without its opening or panel (under the routes) */
static void dest_frames(void)
{
    Menu_DrawFrames(0x65F748, 0xF0, 0x13, 6, 0x7C0F, 0x808080, 0xC8);
    Menu_DrawFrames(0x65F538, 0xD8, 0x10, 5, 0x7C0E, 0x80B0C040, 0xC8);
    Menu_DrawFrames(0x65F558, 0x70, 0x10, 5, 0x7C0E, 0x80B0C040, 0xC8);
    Menu_DrawFrames(0x65F750, 0x10, 0xAE, 6, 0x7C0F, 0x808080, 0xC8);
    Menu_DrawFrames(0x65F628, 8, 0xB2, 5, 0x7C0E, 0x80B0C040, 0xC8);
    Menu_DrawFrames(0x65F680, 0xD0, 0xB2, 5, 0x7C0E, 0x80B0C040, 0xC8);
    Menu_DrawFrames(0x65F6D8, 0x60, 0xCC, 5, 0x7C0E, 0x80B0C040, 0x118);
    Menu_DrawFrames(0x65F6D8, 0xE8, 0xCC, 5, 0x7C0E, 0x80B0C040, 0x118);
}

void DC2CALL Dest_Base(gptr m)                                      /* 0x4A0280 */
{
    gptr r = Tmp_Push(8);
    U16(r) = 0x20; U16(r + 2) = 0x20; U16(r + 4) = 0x100; U16(r + 6) = 0x88;
    Menu_Sprite(r, 0, 0, 0x7C0D, 0x808080, 0, 0x1F4);
    Tmp_Pop(8);
    draw_mode(5, 0x1F4);
    Menu_DrawFrames(0x65F500, 0x110, 0x68, 5, 0x7C0E, 0x80B0C040, 0x12C);
    Menu_DrawFrames(0x65F510, 0x10, 0x68, 5, 0x7C0E, 0x80B0C040, 0x12C);
    dest_frames();
}

/* the three places on the route screen: mark, two lines to it, name, frame */
struct dest_place { u32 mark; u16 mx, my, l0[4], l1[4]; u32 label; u16 lx, ly; u32 frame; u16 fx, fy; };
static const struct dest_place g_dest_places[3] = {
    {0x65F528, 0x28, 0x2A, {0x78, 0x4C, 0x68, 0x3C}, {0x18, 0x3C, 0x68, 0x3C}, 0x73AFBC, 0x1C, 0x3E, 0x65F768, 0x1D, 0x33},
    {0x65F530, 0xA3, 0x58, {0xAD, 0x5B, 0xBD, 0x4B}, {0xBD, 0x4B, 0x10D, 0x4B}, 0x73AFCC, 0xC1, 0x4D, 0x65F758, 0xC2, 0x38},
    {0x65F520, 0xBD, 0x30, {0xC7, 0x6F, 0xD7, 0x7F}, {0xD7, 0x7F, 0x127, 0x7F}, 0x73AFAC, 0xDB, 0x81, 0x65F770, 0xDC, 0x76}};

static void dest_place(const struct dest_place *p, u32 n)
{
    Menu_DrawFrames(p->mark, p->mx, p->my, 0x25, 0x7C0E, 0x80008080, 0xC8);
    Menu_Line(p->l0[0], p->l0[1], p->l0[2], p->l0[3], 0xC0, 0, 0x32);
    Menu_Line(p->l1[0], p->l1[1], p->l1[2], p->l1[3], 0xC0, 0, 0x32);
    Menu_SmallText(p->label, p->lx, p->ly, n, 0x808080, 1);   /* (a copy of it in the original) */
    Menu_DrawFrames(p->frame, p->fx, p->fy, 0x26, 0x7C0E, 0x808080, 0x64);
}

/* a route from place `from` to `to` (G+0x10AA set to `at` meanwhile): the
 * marker gliding from its start by M+0x80 * (kx, ky) / div with the offset
 * shown (vm: 0 as is, 1 negated, 2 as u16), cross-hairs, the column of
 * figures (0x6C1500), until M+0x82 passes `end`: arrived (G+0x10AA `at`) */
struct dest_route { s8 set; u8 from, to, at; s16 kx, ky, div; u8 vm; u16 lim, end; };
static void dest_route(gptr m, const struct dest_route *rt)
{
    if (rt->set >= 0) U8(G() + 0x10AA) = (u8)rt->set;
    dest_place(&g_dest_places[rt->from], rt->from == 2 ? U16(m + 0x98) : 0x64);   /* (point c's name always typed) */
    dest_place(&g_dest_places[rt->to], rt->to == 2 ? U16(m + 0x98) : 0x64);
    Dest_Base(m);
    s32 c = S16(m + 0x80);
    s32 dx = c * rt->kx / rt->div, dy = c * rt->ky / rt->div;
    s32 sx = g_dest_places[rt->from].l0[0], sy = g_dest_places[rt->from].l0[1];
    u32 col = Color_Pulse(0x808080, 0x202020, 4);
    Menu_DrawFrames(0x65F710, (u32)(sx + dx), (u32)(sy + dy), 5, 0x7C0E, col, 0x1E);
    u32 vx = rt->vm == 0 ? (u32)(s32)(s16)dx : rt->vm == 1 ? (u32)-(s32)(s16)dx : (u16)dx;
    u32 vy = rt->vm == 0 ? (u32)(s32)(s16)dy : rt->vm == 1 ? (u32)-(s32)(s16)dy : (u16)dy;
    Menu_Number8((u32)(sx + dx), (u32)(sy + dy + (rt->from == 1 ? 0x11 : 0x10)), 4, 1, 0x707070, vx, 2);
    Menu_Number8((u32)(sx + dx + 0x10), (u32)(sy + dy - 6), 4, 1, 0x707070, vy, 2);
    s32 px = sx + dx, py = sy + dy;
    Menu_Line(sx, sy, px, py, 0x800000, 0, 0x32);
    Menu_Line(px - 8, py - 8, px + 8, py - 8, 0xC0C0C0, 1, 0x32);
    Menu_Line(px - 8, py + 8, px + 8, py + 8, 0xC0C0C0, 1, 0x32);
    Menu_Line(px - 8, py - 8, px - 8, py + 8, 0xC0C0C0, 1, 0x32);
    Menu_Line(px + 8, py - 8, px + 8, py + 8, 0xC0C0C0, 1, 0x32);
    Menu_Line(0, py, 0x140, py, 0x808080, 1, 0x32);
    Menu_Line(px, 0x20, px, 0xA8, 0x808080, 1, 0x32);
    draw_mode(5, 0x32);
    Menu_DrawArea(0, 0, 0x140, 0xF0, 2);
    gptr fig = 0x6C1500;
    for (u16 i = 0; (s32)i < S16(m + 0x80); i++, fig += 2) {
        u32 y = (u32)((i >> 1) * 8 + 0x5E);
        if (S16(m + 0x80) > 0x12) y += (u32)(0x12 - S16(m + 0x80));
        Menu_Number8((u32)((i % 2) * 32 + 0x30), y & 0xFFFF, 4, 1, 0x80606060, U16(fig) % 9999, 2);
    }
    Menu_DrawArea(0x30, 0x60, 0x48, 0x48, 2);
    if (S16(m + 0x82) < (s16)rt->lim) Se_Play(0, 0xC);
    U16(m + 0x80)++;
    if (S16(m + 0x80) > (s16)rt->lim) U16(m + 0x80) = rt->lim;
    U16(m + 0x82)++;
    if (S16(m + 0x82) > (s16)rt->end) { U8(m + 2) = 0xA; U8(G() + 0x10AA) = rt->at; }
}

static const struct dest_route g_dest_routes[6] = {
    {-1, 0, 2, 0, 79, 35, 128, 0, 0x80, 0xC8}, {2, 0, 1, 2, 53, 15, 90, 0, 0x5A, 0xA0},
    {1, 2, 0, 1, -79, -35, 128, 1, 0x80, 0xC8}, {2, 2, 1, 2, -26, -20, 32, 1, 0x20, 0x64},
    {1, 1, 0, 1, -53, -15, 90, 2, 0x5A, 0xA0}, {0, 1, 2, 0, 26, 20, 32, 2, 0x20, 0x5A}};

void DC2CALL Dest_Route4(gptr m) { dest_route(m, &g_dest_routes[0]); }   /* 0x49FE10 */
void DC2CALL Dest_Route5(gptr m) { dest_route(m, &g_dest_routes[1]); }   /* 0x4A0460 */
void DC2CALL Dest_Route6(gptr m) { dest_route(m, &g_dest_routes[2]); }   /* 0x4A08C0 */
void DC2CALL Dest_Route7(gptr m) { dest_route(m, &g_dest_routes[3]); }   /* 0x4A0D60 */
void DC2CALL Dest_Route8(gptr m) { dest_route(m, &g_dest_routes[4]); }   /* 0x4A1210 */
void DC2CALL Dest_Route9(gptr m) { dest_route(m, &g_dest_routes[5]); }   /* 0x4A1690 */
