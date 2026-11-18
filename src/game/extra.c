/* The extra crisis menu (task 0x46E3F0, Game_Start(0x19)): extra crisis or
 * the dino duel (G+0x1092 bits unlock it and its characters), then one or
 * two characters chosen (the duel: a player each, pads 0x87DC10 / 0x87DC20)
 * turning on a stand between the pictures of the characters beside them,
 * then the game (GameTask). T = 0x74D6A8 (0x60 bytes) {u8 mode (table
 * 0x723C20), u8 step, u8 sub, u8 turn time +3, u8 load step +4, u8 turned
 * +5, s8 menu cursor +6, u8 character +7, u8 palette +8, s8 player +9,
 * u16 characters unlocked +0xA, labels of 20 bytes at +0xC..+0x5C, pad
 * +0x5C}. The characters (0x723B30, 12 of 20 bytes): {model, u16 file,
 * u16 id, label, distance, name}. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/pcio.h"
#include "dc2/task.h"
#include "dc2/gte.h"

#define EXTRA_T       0x74D6A8u
#define CHARS         0x723B30u
#define CH(i, o)      (CHARS + (i) * 0x14 + (o))
#define G16(o)        U16(G() + (o))

/* a label {first frame, frame, palette add, time, flags (1 animated, 2 drawn), x, y} */
void DC2CALL Extra_SetLabel(gptr l, gptr a, u32 pal)                  /* 0x46FA30 */
{
    U8(l + 0xD) = 3;
    U32(l) = a;
    U32(l + 4) = a;
    U8(l + 0xC) = U8(a + 5);
    U32(l + 8) = pal;
}

void DC2CALL Extra_AnimLabels(gptr t)                                 /* 0x46FA50 */
{
    for (gptr l = t + 0xC; l < t + 0x5C; l += 0x14) {
        if (!(U8(l + 0xD) & 1)) continue;
        if (U8(l + 0xC)) { U8(l + 0xC)--; continue; }
        gptr f = U32(l + 4);
        if (U8(f + 4) & 0x20) continue;
        f += (U8(f + 4) & 0x1F) * 8;
        U32(l + 4) = f;
        U8(l + 0xC) = U8(f + 5);
    }
}

void DC2CALL Extra_DrawLabels(gptr t)                                 /* 0x46F880 */
{
    u32 tpage, cbase;
    if (U8(t) == 0) { tpage = 0x25; cbase = 0x7E80; }
    else { tpage = 0x35; cbase = 0x7E40; }
    for (gptr l = t + 0xC; l < t + 0x5C; l += 0x14) {
        if (!(U8(l + 0xD) & 2)) continue;
        gptr f = U32(l + 4);
        u8 h = U8(f + 4);
        u32 n = h & 0x1F;
        u32 bits = (u32)(s32)((s8)h >> 5) >> 1;
        u32 code = (bits & 2) << 24 | 0x64808080;
        u16 clut = (u16)((bits & 1) + U32(l + 8) + cbase);
        for (; n; n--, f += 8) {
            gptr p = U32(0x87DC98);
            Gpu_SetSprt(p);
            U32(p + 4) = code;
            F32(p + 8) = (float)(s32)(U16(l + 0x10) + (s8)U8(f + 6));
            F32(p + 0xC) = (float)(s32)(U16(l + 0x12) + (s8)U8(f + 7));
            U8(p + 0x14) = U8(f);
            U8(p + 0x15) = U8(f + 1);
            U16(p + 0x18) = U8(f + 2);
            U16(p + 0x1A) = U8(f + 3);
            U16(p + 0x16) = clut;
            Gpu_AddPrim(U32(0x87F94C) + 0x8C, p);
            U32(0x87DC98) += 0x1C;
        }
        gptr p = U32(0x87DC98);
        Gpu_SetDrawMode(p, 1, 0, tpage, 0);
        Gpu_AddPrim(U32(0x87F94C) + 0x8C, p);
        U32(0x87DC98) += 0xC;
    }
}

/* the next character bit set in mask from cur in direction dir (31 tries);
   cur when there is none */
u32 DC2CALL Extra_NextChar(u32 mask, u32 cur, s32 dir)                /* 0x46E850 */
{
    u32 b = 1u << (cur & 31), i = cur;
    for (u32 n = 31; n; n--) {
        if (dir >= 0) { b <<= 1; i++; if ((s32)i > 0x1F) { i = 0; b = 1; } }
        else { b >>= 1; i--; if ((s32)i < 0) { i = 0x1F; b = 0x80000000u; } }
        if (mask & b) return i;
    }
    return cur;
}

/* ---- the menu: extra crisis, the dino duel, back --------------------------------------- */
void DC2CALL Extra_Menu(gptr t)                                       /* 0x46E440 */
{
    static const gptr help[3] = {0x729388, 0x7293F2, 0x729452};
    static const gptr sel[3] = {0x5FFFBC, 0x60000C, 0x60005C};
    static const gptr idle[3] = {0x5FFFA4, 0x5FFFAC, 0x5FFFB4};
    switch (U8(t + 1)) {
    case 0:
        Fn_4BF130(0);
        U8(t + 1)++;
        U32(0x87DE00) = 0;
        Strm_Start(0, 5);
        Screen_LoadImage(6);
        memset(MEM(t + 0xC), 0, 0x50);
        U8(t + 6) = Flag_Check(2, 0x21) != 0;
        Extra_SetLabel(t + 0x0C, idle[0], 0);
        Extra_SetLabel(t + 0x20, idle[1], 0);
        Extra_SetLabel(t + 0x34, idle[2], 0);
        Extra_SetLabel(t + 0xC + (s8)U8(t + 6) * 0x14, sel[(s8)U8(t + 6)], 0);
        U16(t + 0x44) = 0xA0; U16(t + 0x30) = 0xA0; U16(t + 0x1C) = 0xA0;
        U16(t + 0x46) = 0x74; U16(t + 0x32) = 0x74; U16(t + 0x1E) = 0x74;
        if (!(G16(0x1092) & 0xFFE0)) {            /* no duel characters: no duel entry */
            U16(t + 0x46) = 0x62;
            U8(t + 0x2D) &= ~2;
        }
        Fade_Set(0, 0xA, 1);
        break;
    case 1: {
        u32 trig = U32(0x87DC12);
        s8 c = (s8)U8(t + 6);
        if (trig & 0x5000) {                      /* up / down */
            Se_Play(0, 0);
            if (trig & 0x1000) { U8(t + 6) = (u8)(c - 1); if ((s8)U8(t + 6) < 0) U8(t + 6) = 2;
                                 if (!(G16(0x1092) & 0xFFE0) && U8(t + 6) == 1) U8(t + 6) = 0; }
            else { U8(t + 6) = (u8)(c + 1); if ((s8)U8(t + 6) > 2) U8(t + 6) = 0;
                   if (!(G16(0x1092) & 0xFFE0) && U8(t + 6) == 1) U8(t + 6) = 2; }
            s8 n = (s8)U8(t + 6);
            Extra_SetLabel(t + 0xC + c * 0x14, idle[c], 0);
            Extra_SetLabel(t + 0xC + n * 0x14, sel[n], 0);
            break;
        }
        u32 btn = U32(0x87DC00);
        if (btn & 0x40) { Se_Play(0, 1); U8(t + 1) = 3; }
        else if (btn & 0x820) {
            Se_Play(0, 2);
            U8(t + 1)++;
            if (U8(t + 6) == 2) U8(t + 1) = 3;    /* back */
            else Fade_Set(1, 0xA, 1);
        }
        break; }
    case 2:
        if (Flag_Check(2, 6)) break;
        if (U8(t + 6)) {                          /* the duel: two players */
            U8(G() + 0x927) = 2;
            U8(t)++;
            U8(t + 1) = 0;
            U8(G() + 0x84C) = 5;
            Flag_Set(2, 0x21, 1);
            Fn_4BF130(1);
            Flag_Set(1, 0, 0);
        } else {
            U8(G() + 0x927) = 1;
            U8(t + 1) = 0;
            U8(t)++;
            U8(G() + 0x84C) = 0;
            Flag_Set(2, 0x21, 0);
            Fn_4BF130(0);
        }
        U8(G() + 0x108E) = 1;
        U8(t + 9) = 0;
        break;
    case 3:
        switch (U8(t + 2)) {
        case 0:
            U8(t + 2)++;
            Fade_Set(1, 0x5A, 1);
            Strm_SetFade(0, 3, 0, 0x80);
            break;
        case 1:
            if (Flag_Check(2, 6)) break;
            G16(0x772) = 0; G16(0x770) = 0; G16(0x76E) = 0; G16(0x76C) = 0;
            Room_Reset();
            Game_Start(0x16);                     /* the title */
            break;
        default: break;
        }
        break;
    default: break;
    }
    Extra_AnimLabels(t);
    Extra_DrawLabels(t);
    gptr s = help[(s8)U8(t + 6)];
    Text_Draw(s, Text_Left(s), 0xBC, 0x64, 0x808080, 1, 0);
    u32 k = U32(0x87DE0C) << 12;
    Gpu_DrawOT(U32(0x87F94C) + 0x88, 0x87CBFC + k, 0x87BC00 + k);
}

/* ---- the character choice ------------------------------------------------------------- */
static void rect(gptr r, u16 x, u16 y, u16 w, u16 h) { U16(r) = x; U16(r + 2) = y; U16(r + 4) = w; U16(r + 6) = h; }

/* a character's picture: loaded to 0x200, 0x100 and copied to x (its palette row 0x1FC to row) */
static void load_picture(gptr r, u32 ch, s32 x, s32 row)
{
    while (File_IsBusy()) Task_Sleep(1);
    File_RequestLoad(U16(CH(ch, 4)));
    while (File_IsBusy()) Task_Sleep(1);
    rect(r, 0x200, 0x100, 0x80, 0x100);
    Gpu_VramCopy(r, x, 0x100);
    rect(r, 0, 0x1FC, 0x100, 1);
    Gpu_MoveImage(r, 0, row);
}

void DC2CALL Extra_SelInit(gptr t)                                    /* 0x46EA10 */
{
    gptr g;
    U8(t + 1)++;
    memset(MEM(0x60011C), 0, 0x98E * 4);
    memset(MEM(t + 0xC), 0, 0x50);
    Room_Reset();
    U32(G() + 0x938) = 0x60011C;
    U32(0x87DC0C) = 0x2A;
    U32(G() + 0x940) = 0x602294;
    U32(0x87F940) = 2;
    if (U8(G() + 0x927) == 1) {                   /* extra crisis */
        Screen_LoadImage(2);
        U8(t + 7) = 0;
        U16(t + 0xA) = G16(0x1092);
        U8(t + 8) = 2;
    } else {                                      /* the duel */
        Screen_LoadImage((u32)((s8)U8(t + 9) + 3));
        U16(t + 0xA) = G16(0x1092) & 0xFFE0;
        U8(t + 7) = (u8)Extra_NextChar(U16(t + 0xA), 4, 1);
        U8(t + 8) = (u8)(U8(t + 9) + 7);
    }
    U32(t + 0x5C) = U8(t + 9) ? 0x87DC20 : 0x87DC10;
    gptr r = Tmp_Push(8);
    load_picture(r, Extra_NextChar(U16(t + 0xA), U8(t + 7), -1), 0x280, 0x1FD);
    load_picture(r, U8(t + 7), 0x300, 0x1FE);
    load_picture(r, Extra_NextChar(U16(t + 0xA), U8(t + 7), 1), 0x380, 0x1FF);
    g = G();
    for (gptr o = U32(g + 0x940); o < U32(G() + 0x940) + 0x4C0; o += 0x260) {
        Model_Setup(o, U32(CH(U8(t + 7), 0)));
        U32(o + 0x50) = 3;
        u32 d = U32(CH(U8(t + 7), 0xC));
        U32(o + 0x74) = d; U32(o + 0x70) = d; U32(o + 0x6C) = d;
        U16(o + 0x4A) = 0x800;
        U16(o + 0x104) = 0x9C; U16(o + 0x106) = 0x7F80;
    }
    gptr c = G() + 0x778;
    U16(c + 0x2C) = 0; U16(c + 0x2A) = 0; U16(c + 0x28) = 0; U16(c + 0x30) = 0; U16(c + 0x32) = 0;
    U16(c + 0x34) = 0x1194; U16(c + 0x20) = 0xFD0; U16(c + 0x24) = 0; U16(c + 0x22) = 0;
    SetGeomScreen(0x140);
    SetGeomOffset(0xA0, 0x96);
    U16(r) = (u16)-U16(c + 0x20); U16(r + 2) = (u16)-U16(c + 0x22); U16(r + 4) = (u16)-U16(c + 0x24);
    RotMatrix(r, G() + 0x778);
    Tmp_Pop(8);
    U8(G() + 0xB4B) = 4;
    SetDepthScale((float)(s32)(0x400 << (U8(G() + 0xB4B) & 31)));
    Flag_Set(3, 1, 1); Flag_Set(3, 2, 0); Flag_Set(3, 3, 0); Flag_Set(3, 4, 0); Flag_Set(3, 5, 0); Flag_Set(3, 8, 0);
    U32(G() + 8) = 0x6C1500;
    Flag_Set(3, 0, 1);
    Fade_Set(0, 0xA, 1);
}

/* the pictures moved one step (dir 1: the next one comes from the right) and the stand's
   two models: the one shown moves off, the new one comes in */
static void turn(gptr t, u32 ch, int right)
{
    gptr r = Tmp_Push(8);
    Se_Play(0, 0x56);
    U8(t + 7) = (u8)ch;
    if (right) {
        rect(r, 0, 0x1FE, 0x100, 1); Gpu_MoveImage(r, 0, 0x1FD);
        rect(r, 0x300, 0x100, 0x80, 0x100); Gpu_VramCopy(r, 0x280, 0x100);
        rect(r, 0, 0x1FF, 0x100, 1); Gpu_MoveImage(r, 0, 0x1FE);
        rect(r, 0x380, 0x100, 0x80, 0x100); Gpu_VramCopy(r, 0x300, 0x100);
        U8(t + 1) = 2; U8(t + 2) = 0; U8(t + 3) = 0;
    } else {
        rect(r, 0, 0x1FE, 0x100, 1); Gpu_MoveImage(r, 0, 0x1FF);
        rect(r, 0x300, 0x100, 0x80, 0x100); Gpu_VramCopy(r, 0x380, 0x100);
        rect(r, 0, 0x1FD, 0x100, 1); Gpu_MoveImage(r, 0, 0x1FE);
        rect(r, 0x280, 0x100, 0x80, 0x100); Gpu_VramCopy(r, 0x300, 0x100);
        U8(t + 1) = 2; U8(t + 2) = 1; U8(t + 3) = 0;
    }
    Tmp_Pop(8);
    Obj_Free(U32(G() + 0x940));
    Obj_Free(U32(G() + 0x940) + 0x260);
    gptr o = U32(G() + 0x940);
    Model_Setup(o + 0x260, U32(o + 0x60));        /* the old model on the second object */
    o = U32(G() + 0x940);
    U16(o + 0x2A0) = 0;
    U16(o + 0x364) = right ? 0x9A : 0x9E; U16(o + 0x366) = right ? 0x7F40 : 0x7FC0;
    o = U32(G() + 0x940);
    U32(o + 0x2D4) = U32(o + 0x6C);
    U32(o + 0x2D0) = U32(o + 0x2D4);
    U32(o + 0x2CC) = U32(o + 0x2D0);
    Model_Setup(U32(G() + 0x940), U32(CH(U8(t + 7), 0)));
    U16(U32(G() + 0x940) + 0x40) = right ? 0x29AF : 0xD651;
    U16(U32(G() + 0x940) + 0x104) = 0x9C;
    U16(U32(G() + 0x940) + 0x106) = 0x7F80;
    o = U32(G() + 0x940);
    U32(o + 0x74) = U32(CH(U8(t + 7), 0xC));
    o = U32(G() + 0x940);
    U32(o + 0x70) = U32(o + 0x74);
    o = U32(G() + 0x940);
    U32(o + 0x6C) = U32(o + 0x70);
}

void DC2CALL Extra_SelChoose(gptr t)                                  /* 0x46EED0 */
{
    static const u8 duel[2][13] = {               /* the duel's characters per player */
        {0, 0, 0, 0, 0x1D, 0, 0, 0x1F, 0x21, 0x23, 0x25, 0x27, 0x29},
        {0, 0, 0, 0, 0x1E, 0, 0, 0x20, 0x22, 0x24, 0x26, 0x28, 0x2A}};
    gptr pad = U32(t + 0x5C);
    u16 key = U8(t + 9) ? U16(0x87DC92) : U16(0x87DC00);
    Extra_SetLabel(t + 0xC, U32(CH(U8(t + 7), 8)), 0);
    U16(t + 0x1C) = 0x43; U16(t + 0x1E) = 0x8F;
    Text_Draw(U32(CH(U8(t + 7), 0x10)), 0x20, 0xA0, 0x64, 0x808080, 4, 0);
    u16 held = U16(pad);
    if (held & 0x2000) {                          /* right */
        u32 n = Extra_NextChar(U16(t + 0xA), U8(t + 7), 1);
        if (n != U8(t + 7)) turn(t, n, 1);
    } else if (held & 0x8000) {                   /* left */
        u32 n = Extra_NextChar(U16(t + 0xA), U8(t + 7), -1);
        if (n != U8(t + 7)) turn(t, n, 0);
    } else if (key & 0x40) {                      /* back: the player before, or the menu */
        Se_Play(0, 0x57);
        U8(t + 9)--;
        U8(t + 2) = (s8)U8(t + 9) < 0 ? 2 : 1;
        U8(t + 1) = 3;
        Fade_Set(1, 0x14, 1);
    } else if (key & 0x820) {                     /* chosen */
        Se_Play(0, 0x58);
        gptr g = G();
        s8 pl = (s8)U8(t + 9);
        if (U8(g + 0x927) == 1) U8(g + 0x850 + pl) = U8(CH(U8(t + 7), 6));
        else U8(g + 0x850 + pl) = ((const u8 *)duel)[pl * 13 + U16(CH(U8(t + 7), 6))];
        U8(t + 1) = 3;
        U8(t + 9)++;
        U8(t + 2) = (s32)(s8)U8(t + 9) != (s32)U8(G() + 0x927);
        if (!U8(t + 2)) { Strm_SetFade(0, 3, 0, 0x80); Fade_Set(1, 0x5A, 1); }
        else Fade_Set(1, 0x14, 1);
    }
    Extra_DrawLabels(t);
}

/* the stand turning a step (both models), the next character's picture loading */
void DC2CALL Extra_SelTurn(gptr t)                                    /* 0x46F540 */
{
    switch (U8(t + 4)) {
    case 0: {
        if (File_IsBusy()) break;
        U8(t + 4)++;
        u32 n = Extra_NextChar(U16(t + 0xA), U8(t + 7), U8(t + 2) ? -1 : 1);
        if ((s32)n >= 0 && n < 0xC) File_RequestLoad(U16(CH(n, 4)));
    }   /* fall through */
    case 1:
        if (!File_IsBusy()) U8(t + 4)++;
        break;
    default: break;
    }
    if (!U8(t + 5)) {
        s32 dir = U8(t + 2) ? 1 : -1;
        gptr o = U32(G() + 0x940);
        U16(o + 0x2A0) += (u16)((rsin((s32)U8(t + 3) << 5) / 8) * dir);
        o = U32(G() + 0x940);
        U16(o + 0x40) += (u16)((rsin((s32)((u32)(0x20 - U8(t + 3)) << 5)) / 8) * dir);
        U8(t + 3)++;
        if (U8(t + 3) > 0x20) U8(t + 5)++;
    }
    if (U8(t + 4) == 2 && U8(t + 5) == 1) {       /* the new picture beside */
        gptr r = Tmp_Push(8);
        rect(r, 0, 0x1FC, 0x100, 1);
        if (!U8(t + 2)) { Gpu_MoveImage(r, 0, 0x1FF); rect(r, 0x200, 0x100, 0x80, 0x100); Gpu_VramCopy(r, 0x380, 0x100); }
        else { Gpu_MoveImage(r, 0, 0x1FD); rect(r, 0x200, 0x100, 0x80, 0x100); Gpu_VramCopy(r, 0x280, 0x100); }
        Tmp_Pop(8);
        U8(t + 1) = 1; U8(t + 5) = 0; U8(t + 4) = 0; U8(t + 3) = 0; U8(t + 2) = 0;
    }
}

void DC2CALL Extra_SelLeave(gptr t)                                   /* 0x46F730 */
{
    Text_Draw(U32(CH(U8(t + 7), 0x10)), 0x20, 0xA0, 0x64, 0x808080, 4, 0);
    if (Flag_Check(2, 6)) return;
    switch (U8(t + 2)) {
    case 0: U8(t)++; break;                       /* the game */
    case 1: break;                                /* the next player */
    default: U8(t)--; break;                      /* the menu */
    }
    U8(t + 2) = 0; U8(t + 1) = 0;
    Obj_Free(U32(G() + 0x940));
    Obj_Free(U32(G() + 0x940) + 0x260);
    U32(U32(G() + 0x940) + 0x50) &= ~2u;
    U32(U32(G() + 0x940) + 0x2B0) &= ~2u;
    SetGeomOffset(0xA0, 0x78);
}

void DC2CALL Extra_Select(gptr t)                                     /* 0x46E8C0 */
{
    switch (U8(t + 1)) {
    case 0: Extra_SelInit(t); break;
    case 1: Extra_SelChoose(t); break;
    case 2: Extra_SelTurn(t); break;
    case 3: Extra_SelLeave(t); break;
    default: Orig_Call1(U32(0x723C2C + U8(t + 1) * 4), t); break;
    }
    SetBackColor(0x40, 0x40, 0x40);
    for (gptr o = U32(G() + 0x940); o < U32(G() + 0x940) + 0x4C0; o += 0x260) {
        if (!(U8(o + 0x50) & 2)) continue;
        U16(U32(o + 0x64) + 0x76) = 0xD8F0;
        U16(U32(o + 0x64) + 0x78) = 0xEC78;
        U16(o + 0x4A) = (U16(o + 0x4A) + 0x10) & 0xFFF;
        Obj_UpdateMatrix(o);
    }
    Frame_Draw();
    u32 n = Extra_NextChar(U16(t + 0xA), U8(t + 7), -1);
    U8(t + 0x2D) &= ~2;
    if (n != U8(t + 7) && (U8(G() + 4) & 0x10)) {          /* the arrows (blinking) */
        Extra_SetLabel(t + 0x20, 0x60010C, U8(t + 8));
        U16(t + 0x30) = 0; U16(t + 0x32) = 0x60;
    }
    n = Extra_NextChar(U16(t + 0xA), U8(t + 7), 1);
    U8(t + 0x41) &= ~2;
    if (n != U8(t + 7) && (U8(G() + 4) & 0x10)) {
        Extra_SetLabel(t + 0x34, 0x600114, U8(t + 8));
        U16(t + 0x44) = 0x120; U16(t + 0x46) = 0x60;
    }
    Extra_DrawLabels(t);
}

/* the game: a new state keeping the options */
void DC2CALL Extra_Start(gptr t)                                      /* 0x46F800 */
{
    U8(t + 1)++;
    gptr g = G();
    u16 opt = U16(g + 0x1092);
    u8 b = U8(g + 0x1094) & 2;
    memset(MEM(g + 0x108C), 0, 0x118 * 4);
    G16(0x1092) = opt;
    U8(G() + 0x1094) = b;
    U8(G() + 0x662) = 0;
    Game_ClearState();
    Task_Chain(0x4510F0);
}

void DC2CALL ExtraTask(void)                                          /* 0x46E3F0 */
{
    memset(MEM(EXTRA_T), 0, 0x18 * 4);
    memcpy(MEM(G() + 0x108C), MEM(0x87F4E0), 0x118 * 4);
    for (;;) {
        switch (U8(EXTRA_T)) {
        case 0: Extra_Menu(EXTRA_T); break;
        case 1: Extra_Select(EXTRA_T); break;
        case 2: Extra_Start(EXTRA_T); break;
        default: Orig_Call1(U32(0x723C20 + U8(EXTRA_T) * 4), EXTRA_T); break;
        }
        Task_Sleep(1);
    }
}
