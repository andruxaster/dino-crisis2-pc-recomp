/* The save slots screen: loading a game (Game_Start mode 2, task 0x46D5A0)
 * and saving one from the save room's menu (mode 0x15, task 0x46D690), 100
 * slots SAVE00.DAT..SAVE99.DAT.
 * Its state at 0x74B5E8 (0x2020 bytes): +0 the file image (0x5F4 bytes: a
 * PS1 memory card header "SC", the title in SJIS with the slot and the time,
 * the icon, +0x100 the slot's summary, +0x17C/+0x180 sums of the first 0x184
 * bytes and of the whole, +0x184 the game state G+0x108C (0x460 bytes),
 * +0x5E4 the options G+0x14EC), +0x2000 the mode (a byte read as the dword it
 * starts), +0x2001 its step, +0x2004 the slot (0..99), +0x2008 yes/no,
 * +0x2009 the letters of a message, +0x200A the pad with repeat, +0x200C 16
 * repeat counters, +0x201C the play time saved.
 * The slots: 0x87F340 4 bytes each (file there, readable, bad), their
 * summaries in G+0x20 + 16 * slot (the stage name's area/room, difficulty,
 * costume, items, time).
 * Behind it the background quad and two turning rings of quads. */
#include "dc2/game.h"
#include "dc2/gte.h"
#include "dc2/orig.h"
#include "dc2/pcio.h"
#include "dc2/task.h"

#define SV   0x74B5E8u
#define SLOTS 0x87F340u

static void mode_into(u32 ot, u32 tpage)
{
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, tpage, 0);
    Gpu_AddPrim(ot, U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}
static gptr rect(u16 x, u16 y, u16 w, u16 h)
{
    gptr r = Tmp_Push(8);
    U16(r) = x; U16(r + 2) = y; U16(r + 4) = w; U16(r + 6) = h;
    return r;
}
static void sprite(u16 x, u16 y, u16 w, u16 h, u32 u, u32 v, u32 tp, u32 col, u32 semi, u32 z)
{
    gptr r = rect(x, y, w, h);
    Menu_Sprite(r, u, v, tp, col, semi, z);
    Tmp_Pop(8);
}

/* the background: a dark red quad over the screen */
void DC2CALL Card_Back(void)                                         /* 0x408630 */
{
    sprite(0, 0, 0x100, 0xF0, 0, 0, 0x7C4E, 0xC00808, 0, 0x3E8);
    mode_into(0x87CBA0 + (U32(0x87DE0C) << 12), 5);
    sprite(0x100, 0, 0x40, 0xF0, 0, 0, 0x7C4E, 0xC00808, 0, 0x3E8);
    mode_into(0x87CBA0 + (U32(0x87DE0C) << 12), 6);
}

/* a ring of 64 flat quads turning about y (radius t..t+2, t the counter
   growing to 0x48) */
static void ring(gptr ctr, int step, u32 code)
{
    if (step) {
        U16(ctr)++;
    }
    if (S16(ctr) > 0x48) U16(ctr) = 0;
    gptr m = Tmp_Push(0x20), a = Tmp_Push(8), v = Tmp_Push(8);
    u16 x1[65], x2[65], z1[65], z2[65];
    for (u32 i = 0; i < 65; i++) {
        U16(a) = 0; U16(a + 2) = (u16)(i * 0x40); U16(a + 4) = 0;
        RotMatrix(a, m);
        U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = U16(ctr);
        ApplyMatrixSV(m, v, v);
        x1[i] = (u16)(U16(v) + 0xA0); z1[i] = (u16)(U16(v + 4) + 0x78);
        U16(v + 4) = (u16)(U16(ctr) + 2); U16(v) = 0; U16(v + 2) = 0;
        ApplyMatrixSV(m, v, v);
        x2[i] = (u16)(U16(v) + 0xA0); z2[i] = (u16)(U16(v + 4) + 0x78);
    }
    Tmp_Pop(0x30);
    for (u32 i = 0; i < 64; i++) {
        gptr p = U32(0x87DC98);
        F32(p + 8) = (float)x1[i]; F32(p + 0xC) = (float)z1[i];
        F32(p + 0x14) = (float)x1[i + 1]; F32(p + 0x18) = (float)z1[i + 1];
        F32(p + 0x20) = (float)x2[i]; F32(p + 0x24) = (float)z2[i];
        F32(p + 0x2C) = (float)x2[i + 1]; F32(p + 0x30) = (float)z2[i + 1];
        U32(p + 0x34) = 0x3C23D70A; U32(p + 0x28) = 0x3C23D70A; U32(p + 0x1C) = 0x3C23D70A; U32(p + 0x10) = 0x3C23D70A;
        U32(p + 4) = code;
        gptr ot = 0x87CA10 + (U32(0x87DE0C) << 12);
        U32(p) = U32(ot) | 0x05000000;
        U32(ot) = p & 0xFFFFFF;
        U32(0x87DC98) = p + 0x38;
    }
    mode_into(0x87CA10 + (U32(0x87DE0C) << 12), 0x26);
}
void DC2CALL Card_Ring1(void)                                        /* 0x408730 */
{
    U16(0x746D20) += U32(0x87DE00) ? 2 : 1;
    ring(0x746D20, 0, 0x29100808);
}
void DC2CALL Card_Ring2(void)                                        /* 0x4089C0 */
{
    ring(0x746D22, U32(0x87DE00) || (U8(G() + 4) & 1), 0x29080808);
}

/* two digits (8x8 sprites from u = digit * 8), then the draw mode */
void DC2CALL Card_Number2(u32 x, u32 y, u32 v, u32 col, u32 z)      /* 0x46C780 */
{
    u32 d = 10;
    for (int k = 0; k < 2; k++, x += 8) {
        u32 q = v / d;
        v -= (q & 0xFFFF) * d;
        d /= 10;
        sprite((u16)x, (u16)y, 8, 8, q * 8, 0, 0x7C8F, col & 0xFFFFFF, col >> 31, z);
    }
    mode_into(0x87BC00 + 4 * ((U32(0x87DE0C) << 10) + (s32)(s16)z), 0x15);
}

/* a play time hh:mm:ss (frames at 60 a second) */
void DC2CALL Card_Time(u32 x, u32 y, u32 t, u32 col, u32 z)         /* 0x46C5D0 */
{
    u32 h = t / 216000;
    Card_Number2(x, y, h, col, z);
    sprite((u16)(x + 0x10), (u16)y, 8, 8, 0x50, 0, 0x7C8F, col, 0, z);
    mode_into(0x87BC00 + 4 * ((U32(0x87DE0C) << 10) + z), 0x15);
    t -= h * 216000;
    u32 m = t / 3600;
    Card_Number2(x + 0x18, y, m, col, z);
    sprite((u16)(x + 0x28), (u16)y, 8, 8, 0x50, 0, 0x7C8F, col, 0, z);
    mode_into(0x87BC00 + 4 * ((U32(0x87DE0C) << 10) + z), 0x15);
    Card_Number2(x + 0x30, y, (t - m * 3600) / 60, col, z);
}

/* the summary of a slot with a game in it */
void DC2CALL Card_SlotInfo(gptr o)                                   /* 0x46C1D0 */
{
    gptr e = G() + (U32(o + 0x2004) + 2) * 16;
    Text_Draw(Room_Name(U8(e + 5), U8(e + 4)), 0x60, 0x42, 0x64, 0x808080, 0xA, 0);
    Card_Number2(0xB0, 0x55, U8(e + 1), 0x808080, 0x258);
    s8 k = (s8)U8(e + 2);
    if (k > 2 || k < 0) {                       /* (a difficulty out of range: the slot marked bad) */
        gptr s = SLOTS + 4 * U32(o + 0x2004);
        U8(s) = 1; U8(s + 1) = 1; U8(s + 2) = 1;
        return;
    }
    Menu_DrawFrames(U32(0x723A34 + 4 * k), 0xAC, 0x60, 0x15, 0x7C8F, 0x808080, 0xC8);
    Card_Time(0xF0, 0x6C, U32(e + 0xC), 0x808080, 0xA);
    sprite(0x1B, 0x2D, 0x40, 0x40, U8(e + 3) == 1 ? 0 : 0x40, 0xC0, 0x7CC0, 0x808080, 0, 0xC8);
    mode_into(0x87BF20 + (U32(0x87DE0C) << 12), 0x95);
    if (U8(e + 8) & 1) sprite(0x64, 0x6E, 0x10, 0x10, 0x20, 0x48, 0x7C8F, 0x808080, 0, 0xC8);
    if (U16(e + 6)) sprite(0x78, 0x6E, 0x10, 0x10, 0x30, 0x48, 0x7C4F, 0x808080, 0, 0xC8);
    if (U16(e + 6) & 0xFFE0) sprite(0x8C, 0x6E, 0x10, 0x10, 0x40, 0x48, 0x7C4F, 0x808080, 0, 0xC8);
    mode_into(0x87BF20 + (U32(0x87DE0C) << 12), 0x15);
}

/* an empty, unreadable or bad slot: its message and blank figures */
static void slot_blank(gptr text, u32 col)
{
    Text_Draw(text, 0x60, 0x42, 0x64, col, 0xA, 0);
    Card_Number2(0xB0, 0x55, 0, 0x101030, 0x258);
    Menu_DrawFrames(0x65FC98, 0xAC, 0x60, 0x15, 0x7C8F, 0x101030, 0xC8);
    Card_Time(0xF0, 0x6C, 0, 0x101030, 0xA);
}
void DC2CALL Card_SlotUnread(gptr o) { (void)o; slot_blank(0x726F26, 0x303030); }   /* 0x46C480 */
void DC2CALL Card_SlotEmpty(gptr o)  { (void)o; slot_blank(0x726F5A, 0x202060); }   /* 0x46C4F0 */
void DC2CALL Card_SlotBad(gptr o)    { (void)o; slot_blank(0x726CC6, 0x202060); }   /* 0x46C560 */

/* the panel: its pieces, the slot number, the labels, the slot's summary,
   the arrows */
void DC2CALL Card_Panel(gptr o)                                      /* 0x46BEE0 */
{
    sprite(0x10, 0x24, 0x48, 0x50, 0xA0, 0, 0x7C8F, 0x4080, 1, 0x64);
    sprite(0x18, 0x34, 0x40, 0x24, 0x68, 0x60, 0x7C8F, 0x4080, 1, 0x64);
    mode_into(0x87BD90 + (U32(0x87DE0C) << 12), 6);
    sprite(0x108, 0x58, 0x28, 0x30, 0x40, 0x60, 0x7C8F, 0x4080, 1, 0x64);
    sprite(0x108, 0x60, 0x20, 0x20, 0x40, 0x90, 0x7C8F, 0x4080, 1, 0x64);
    mode_into(0x87BD90 + (U32(0x87DE0C) << 12), 6);
    Menu_DrawFrames(0x65FCB8, 0x70, 0x34, 0x15, 0x7C8F, 0x808080, 0xC8);
    Card_Number2(0x9C, 0x34, U32(o + 0x2004), 0x8080, 0x64);
    Menu_DrawFrames(0x65FCA0, 0x6A, 0x55, 0x15, 0x7C8F, 0x808080, 0xC8);
    Menu_DrawFrames(0x65FD58, 0xA8, 0x51, 0x15, 0x7C8F, 0x80008000, 0xC8);
    Menu_DrawFrames(0x65FCA8, 0x6A, 0x60, 0x15, 0x7C8F, 0x808080, 0xC8);
    Menu_DrawFrames(0x65FD58, 0xA8, 0x5D, 0x15, 0x7C8F, 0x80008000, 0xC8);
    gptr s = SLOTS + 4 * U32(o + 0x2004);
    if (!U8(s)) Card_SlotEmpty(o);
    else if (!U8(s + 1)) Card_SlotUnread(o);
    else if (U8(s + 2)) Card_SlotBad(o);
    else Card_SlotInfo(o);
    Menu_DrawFrames(0x685CD8, 0x60, 0x30, 0xC, 0x7C8F, Color_Pulse(0x80C0, 0x1020, 3), 0xC8);
    Menu_DrawFrames(0x6850C0, 0xB4, 0x30, 0xC, 0x7C8F, Color_Pulse(0x80C0, 0x1020, 3), 0xC8);
}

/* the pad with repeat: a button held 20 frames repeats every 10 */
u32 DC2CALL Card_Repeat(gptr o)                                      /* 0x46D620 */
{
    u32 r = 0;
    for (u32 k = 0; k < 16; k++) {
        gptr c = o + 0x200C + k;
        if (U16(0x87DDFC) & (1u << k)) U8(c)++; else U8(c) = 0;
        if (U8(c) >= 0x14) { U8(c) = 0xA; r |= (1u << k) & U16(0x87DDFC); }
        else r |= (1u << k) & U16(0x87DC00);
    }
    return r;
}

/* the sum of n bytes */
u32 DC2CALL Card_Sum(gptr p, u32 n)                                  /* 0x46CAC0 */
{
    u32 s = 0;
    for (; n; n--, p++) s += U8(p);
    return s;
}

/* the slot chosen (left/right with repeat, 0x40 back, 0x20 on) */
static int choose(gptr o)
{
    u16 k = U16(o + 0x200A);
    if (k & 0x8000) {
        Se_Play(0, 0);
        if ((s32)--U32(o + 0x2004) < 0) U32(o + 0x2004) = 0x63;
    } else if (k & 0x2000) {
        Se_Play(0, 0);
        if ((s32)++U32(o + 0x2004) > 0x63) U32(o + 0x2004) = 0;
    } else if (U8(0x87DC00) & 0x40) {
        Se_Play(0, 1);
        U32(o + 0x2000) = 3;
    } else if (U8(0x87DC00) & 0x20) return 1;
    return 0;
}
static void letters_up(gptr o)
{
    if (++U8(o + 0x2009) > 0x64) U8(o + 0x2009) = 0x64;
}

/* load mode 0: the slot to load */
void DC2CALL Load_List(gptr o)                                       /* 0x46BD80 */
{
    switch ((s8)U8(o + 0x2001)) {
    case 0:
        Se_Play(0, 2);
        U8(o + 0x2009) = 0;
        U8(o + 0x2001)++;
        /* fall through */
    case 1: break;
    default: return;
    }
    if (choose(o)) {
        gptr s = SLOTS + 4 * U32(o + 0x2004);
        Se_Play(0, 2);
        U8(o + 0x2009) = 0;
        U32(o + 0x2000) = U8(s + 1) && !U8(s + 2) ? 1 : 4;
    }
    Card_Panel(o);
    letters_up(o);
    Text_Draw(0x726E34, 0x20, 0x8C, U8(o + 0x2009), 0x808080, 0xA, 0);
}

/* save mode 0: the slot to save into */
void DC2CALL Save_List(gptr o)                                       /* 0x46C880 */
{
    switch ((s8)U8(o + 0x2001)) {
    case 0:
        U8(o + 0x2001)++;
        U8(o + 0x2009) = 0;
        Se_Play(0, 2);
        /* fall through */
    case 1: break;
    default: return;
    }
    if (choose(o)) {
        gptr s = SLOTS + 4 * U32(o + 0x2004);
        Se_Play(0, 2);
        U8(o + 0x2008) = 0;
        if (U8(s + 1)) { U32(o + 0x2000) = 5; U8(o + 0x2008) = 1; }
        else {
            U8(o + 0x2009) = 0;
            U32(o + 0x2000) = U8(s) ? 4 : 1;
        }
    }
    Card_Panel(o);
    Text_Draw(0x726E0A, Text_Left(0x726E0A), 0x8C, 0x64, 0x808080, 0xA, 0);
}

/* load mode 1: the file read and its sums checked */
void DC2CALL Load_Read(gptr o)                                       /* 0x46C9E0 */
{
    gptr name = Tmp_Push(0x20);
    Crt_sprintf1(name, 0x71B220, U32(o + 0x2004));
    s32 fd = Pc_Open(name, 0, 0);
    Tmp_Pop(0x20);
    if (fd == -1) { U8(o + 0x2009) = 0; U32(o + 0x2000) = 4; return; }
    Pc_Read(fd, o, 0x5F4);
    Pc_Close(fd);
    u32 a = U32(o + 0x17C), b = U32(o + 0x180);
    U32(o + 0x17C) = 0; U32(o + 0x180) = 0;
    if (b == Card_Sum(o, 0x5F4)) { U32(o + 0x17C) = a; U32(o + 0x180) = b; U32(o + 0x2000) = 2; }
    else { U8(o + 0x2009) = 0; U32(o + 0x2000) = 5; }
    Card_Panel(o);
    Text_Draw(0x726AA4, 0x30, 0x78, 0x64, 0x808080, 0xA, 0);
}

/* the file image of the game now: header, title "... NO.nn hh:mm:ss",
   icon by costume, summary, the game state and the options, the sums */
void DC2CALL Card_Build(gptr p, gptr o)                              /* 0x46CC30 */
{
    static const u32 icon[3][2] = {{0x723854, 0x723874}, {0x7238F4, 0x723914}, {0x723994, 0x7239B4}};
    gptr g = G();
    u32 t = U32(g + 0x1098), h = t / 216000, r = t - h * 216000, m = r / 3600, s = (r - m * 3600) / 60;
    U8(p) = 'S'; U8(p + 1) = 'C'; U8(p + 2) = 0x11; U8(p + 3) = 1;
    for (u32 k = 0; k < 0x1C; k++) U8(p + 0x44 + k) = 0;
    u32 n = 0;
    do U8(p + 4 + n) = U8(0x723A40 + n); while (U8(0x723A40 + n++));
    s32 slot = (s32)U32(o + 0x2004);
    U16(p + 0x24) = U16(0x723A80 + 2 * (slot / 10)); U16(p + 0x26) = U16(0x723A80 + 2 * (slot % 10));
    U16(p + 0x2C) = U16(0x723A80 + 2 * (h / 10)); U16(p + 0x2E) = U16(0x723A80 + 2 * (h % 10));
    U16(p + 0x32) = U16(0x723A80 + 2 * (m / 10)); U16(p + 0x34) = U16(0x723A80 + 2 * (m % 10));
    U16(p + 0x38) = U16(0x723A80 + 2 * (s / 10)); U16(p + 0x3A) = U16(0x723A80 + 2 * (s % 10));
    u8 c = U8(g + 0x108F);
    const u32 *ic = icon[c == 0 || c == 2 ? 0 : c == 1 ? 1 : 2];
    for (u32 k = 0; k < 0x20; k += 4) U32(p + 0x60 + k) = U32(ic[0] + k);
    for (u32 k = 0; k < 0x80; k += 4) U32(p + 0x80 + k) = U32(ic[1] + k);
    for (u32 k = 0; k < 6; k++) U8(p + 0x100 + k) = U8(g + 0x108C + k);
    U16(p + 0x106) = U16(g + 0x1092);
    U8(p + 0x109) = 0; U8(p + 0x108) = U8(g + 0x1094); U8(p + 0x10A) = 0; U8(p + 0x10B) = 0;
    U32(p + 0x10C) = U32(g + 0x1098); U32(o + 0x201C) = U32(g + 0x1098);
    U32(p + 0x110) = 0x20000710;
    U32(p + 0x180) = 0; U32(p + 0x17C) = 0;
    gptr e = G() + (U32(o + 0x2004) + 2) * 16;
    for (u32 k = 0; k < 16; k += 4) U32(e + k) = U32(p + 0x100 + k);
    gptr a = p + 0x184;
    for (u32 k = 0; k < 0x460; k += 4) U32(a + k) = U32(G() + 0x108C + k);
    if (U16(0x87EEB0) != 0x101) {
        gptr pl = U32(G() + 0x940);
        U16(a + 0x20) = U16(pl + 0x40); U16(a + 0x22) = U16(pl + 0x42); U16(a + 0x24) = U16(pl + 0x44);
        U16(a + 0x26) = U16(pl + 0x4A);
        U16(a + 0x176) = U16(pl + 0x108);
        if (U8(a + 3) == 1) { U16(a + 0x16C) = 0x4B0; U16(a + 0x170) = U16(pl + 0x108); }
        else { U16(a + 0x170) = 0x4B0; U16(a + 0x16C) = U16(pl + 0x108); }
        U16(a + 0x174) = U16(pl + 0x17E);
    } else {
        U16(a + 0x174) = 0; U16(a + 0x16C) = 0x4B0; U16(a + 0x170) = 0x4B0;
    }
    for (u32 k = 0; k < 0x460; k += 4) U32(0x87F4E0 + k) = U32(a + k);
    for (u32 k = 0; k < 16; k += 4) U32(a + 0x460 + k) = U32(G() + 0x14EC + k);
    u32 s1 = Card_Sum(p, 0x5F4), s0 = Card_Sum(p, 0x184);
    U32(p + 0x180) = s1; U32(p + 0x17C) = s0;
}

/* save mode 1: the file written, the slot's summary kept */
void DC2CALL Save_Write(gptr o)                                      /* 0x46CAE0 */
{
    u32 slot = U32(o + 0x2004);
    gptr e = G() + (slot + 2) * 16;
    gptr name = Tmp_Push(0x20);
    Crt_sprintf1(name, 0x71B220, slot);
    Card_Build(o, o);
    s32 fd = Pc_Creat(name);
    Tmp_Pop(0x20);
    if (fd == -1) { U8(o + 0x2009) = 0; U32(o + 0x2000) = 4; return; }
    Pc_Write(fd, o, 0x5F4);
    Pc_Close(fd);
    Se_Play(0, 2);
    U8(SLOTS + 4 * slot + 2) = 0; U8(SLOTS + 4 * slot) = 1; U8(SLOTS + 4 * slot + 1) = 1;
    for (u32 k = 0; k < 6; k++) U8(e + k) = U8(G() + 0x108C + k);
    U16(e + 6) = U16(G() + 0x1092);
    U8(e + 8) = U8(G() + 0x1094);
    U32(e + 0xC) = U32(o + 0x201C);
    U32(o + 0x2000) = 2;
    U8(o + 0x2009) = 0;
    Text_Draw(0x726AC8, 0x30, 0x78, 0xC8, 0x808080, 0xA, 0);
}

/* the pads cleared, then the title (the second title screen when that is
   where the game came from) */
static void to_title(void)
{
    U16(0x87DC08) = 0; U16(0x87DC94) = 0; U16(0x87DDFC) = 0; U16(0x87DC00) = 0;
    if (U8(G() + 0x108D)) Game_Start(0x16);
    else Task_Chain(0x4A5730);
}

/* load mode 2: a second, then the game state taken and the game started */
void DC2CALL Load_Done(gptr o)                                       /* 0x46D0B0 */
{
    Card_Panel(o);
    Text_Draw(0x726AA4, 0x30, 0x78, 0x64, 0x808080, 0xA, 0);
    switch ((s8)U8(o + 0x2001)) {
    case 1:
        if (++U8(o + 0x2009) != 0x3C) break;
        /* fall through */
    case 0:
        U8(o + 0x2001)++;
        U8(o + 0x2009) = 0;
        break;
    case 2:
        for (u32 k = 0; k < 0x460; k += 4) U32(G() + 0x108C + k) = U32(o + 0x184 + k);
        for (u32 k = 0; k < 0x460; k += 4) U32(0x87F4E0 + k) = U32(o + 0x184 + k);
        U32(G() + 0x664) = 0;
        U8(G() + 0x662) = 1;
        Game_ClearState();
        U16(0x87DC08) = 0; U16(0x87DC94) = 0; U16(0x87DDFC) = 0; U16(0x87DC00) = 0;
        if (U8(G() + 0x108D)) Game_Start(0x16);
        else Task_Chain(0x4510F0);
        break;
    }
}

/* save mode 2: "saved" until a button */
void DC2CALL Save_Done(gptr o)                                       /* 0x46D1B0 */
{
    U8(0x87DC90) = 1;
    letters_up(o);
    Card_Panel(o);
    if (U8(0x87DC00) & 0x70) U32(o + 0x2000) = 0;
    Text_Draw(0x726F08, Text_Left(0x726F08), 0x8C, U8(o + 0x2009), 0xA06060, 0xA, 1);
}

/* load mode 3: back to the title */
void DC2CALL Load_Back(gptr o)                                       /* 0x46D230 */
{
    (void)o;
    U32(G() + 0x664) = 0;
    to_title();
}

/* save mode 3: back to the save room's menu, or to the title */
void DC2CALL Save_Back(gptr o)                                       /* 0x46D280 */
{
    (void)o;
    U16(0x87DC08) = 0; U16(0x87DC94) = 0; U16(0x87DDFC) = 0; U16(0x87DC00) = 0;
    if (U8(G() + 0xB48) == 1) { Task_Chain(0x40D710); return; }
    U16(G() + 0x772) = 0; U16(G() + 0x770) = 0; U16(G() + 0x76E) = 0; U16(G() + 0x76C) = 0;
    to_title();
}

/* a message until a button, then the list again */
static void message(gptr o, gptr text, u32 x)
{
    Text_Draw(text, x, 0x8C, U8(o + 0x2009), 0x2020C0, 0xA, 1);
    if (U8(0x87DC00) & 0x70) { U32(o + 0x2000) = 0; Se_Play(0, 1); }
}

/* load mode 4: no game in the slot */
void DC2CALL Load_NoData(gptr o)                                     /* 0x46D310 */
{
    switch ((s8)U8(o + 0x2001)) {
    case 0:
        U8(o + 0x2009) = 0;
        U8(o + 0x2001)++;
        /* fall through */
    case 1: break;
    default: return;
    }
    Card_Panel(o);
    letters_up(o);
    message(o, 0x726D0A, 0x30);
}

/* save mode 4: the file could not be made */
void DC2CALL Save_Fail(gptr o)                                       /* 0x46D3B0 */
{
    Card_Panel(o);
    letters_up(o);
    message(o, 0x726D46, Text_Left(0x726D46));
}

/* load mode 5: the file is bad */
void DC2CALL Load_Bad(gptr o)                                        /* 0x46D430 */
{
    Card_Panel(o);
    letters_up(o);
    gptr b = Tmp_Push(0x200);
    Msg_Concat(b, 0x726CC6, 0x723FC8);
    Msg_Concat(b, b, 0x726CE6);
    message(o, b, Text_Left(b));
    Tmp_Pop(0x200);
}

/* save mode 5: overwrite the game there? */
void DC2CALL Save_Overwrite(gptr o)                                  /* 0x46D4F0 */
{
    u32 p = U32(0x87DC00);
    if (p & 0xA000) { Se_Play(0, 0); U8(o + 0x2008) ^= 1; }
    else if (p & 0x40) U32(o + 0x2000) = 0;
    else if (p & 0x20) {
        if (!U8(o + 0x2008)) { Se_Play(0, 2); U32(o + 0x2000) = 1; }
        else U32(o + 0x2000) = 0;
    }
    Card_Panel(o);
    Text_Draw(0x726EE8, Text_Left(0x726EE8), 0x8C, 0x64, 0x808080, 0xA, 0);
    Menu_YesNo(0xB4, 0xA0, U8(o + 0x2008), 1);
}

static void card_task(gptr modes)
{
    for (u32 k = 0; k < 0x2020; k += 4) U32(SV + k) = 0;
    for (;;) {
        Card_Back();
        Card_Ring1();
        Card_Ring2();
        U16(SV + 0x200A) = (u16)Card_Repeat(SV);
        Orig_Call1(U32(modes + 4 * (s32)(s8)U8(SV + 0x2000)), SV);
        u32 b = U32(0x87DE0C) << 12;
        Gpu_DrawOT(U32(0x87F94C) + 0x78, b + 0x87CBFC, b + 0x87BC00);
        Task_Sleep(1);
    }
}

/* the load task (modes 0x723A98) */
void DC2CALL Load_Task(void)                                         /* 0x46D5A0 */
{
    Flag_Set(3, 0, 0);
    card_task(0x723A98);
}

/* the save task, from the save room's menu (modes 0x723AB0) */
void DC2CALL Save_Task(void)                                         /* 0x46D690 */
{
    card_task(0x723AB0);
}
