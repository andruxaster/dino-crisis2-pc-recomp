/* The title after the game is cleared (task 0x4A63F0, Game_Start(0x16)):
 * five entries - new game, new game with the clear data, the extra crisis
 * (Game_Start(0x19)), load, options - and the difficulty choice. State:
 * G+0x76C mode (table 0x73B2D0), +0x76E screen, +0x770 step; T = 0x759F30
 * {s8 cursor, s8 difficulty cursor, 5 animated labels of 12 bytes at +4
 * (see Title_SetAnim)}. The clear data: 0x87F4E0, 0x460 bytes (G+0x108C). */
#include "dc2/game.h"
#include "dc2/pcinput.h"
#include "dc2/orig.h"
#include "dc2/pcio.h"
#include "dc2/task.h"
#include "dc2/gte.h"

#define TITLE2_T      0x759F30u
#define LBL2_NORMAL   0x73B290u   /* label animations per entry: highlighted */
#define LBL2_IDLE     0x73B2A4u   /* not highlighted */
#define LBL2_DIFF_SEL 0x73B2B8u   /* the difficulty entries 1..2 */
#define LBL2_DIFF     0x73B2C4u
#define CLEAR_DATA    0x87F4E0u
#define G16(o)        U16(G() + (o))

/* every active label one frame on (Title_AnimStep without the result) */
void DC2CALL Title2_AnimAll(gptr t)                                 /* 0x4A6D20 */
{
    for (gptr s = t + 4; s < t + 0x40; s += 12) {
        if (!(U8(s + 9) & 1)) continue;
        if (U8(s + 8)) { U8(s + 8)--; continue; }
        gptr f = U32(s + 4);
        if (U8(f + 4) & 0x20) continue;
        f += (U8(f + 4) & 0x1F) * 8;
        U32(s + 4) = f;
        U8(s + 8) = U8(f + 5);
    }
}

/* the active labels at (0xA0, 0xA0); the difficulty screen on "new game"
   uses another texture page and palette row */
void DC2CALL Title2_DrawAll(gptr t)                                 /* 0x4A6B60 */
{
    u32 tpage, cbase;
    if ((s16)G16(0x76E) == 2 && U8(t) == 0) { tpage = 0x3E; cbase = 0x7F80; }
    else { tpage = 0x2E; cbase = 0x7FC0; }
    for (gptr s = t + 4; s < t + 0x40; s += 12) {
        if (!(U8(s + 9) & 1)) continue;
        gptr f = U32(s + 4);
        u8 h = U8(f + 4);
        u32 n = h & 0x1F;
        u32 bits = (u32)(s32)((s8)h >> 5) >> 1;
        u32 code = (bits & 2) << 24 | 0x64808080;
        u16 clut = (u16)((bits & 1) + cbase);
        for (; n; n--, f += 8) {
            gptr p = U32(0x87DC98);
            Gpu_SetSprt(p);
            U32(p + 4) = code;
            F32(p + 8) = (float)(u32)(0xA0 + (s8)U8(f + 6));
            F32(p + 0xC) = (float)(u32)(0xA0 + (s8)U8(f + 7));
            U8(p + 0x14) = U8(f);
            U8(p + 0x15) = U8(f + 1);
            U16(p + 0x18) = U8(f + 2);
            U16(p + 0x1A) = U8(f + 3);
            U16(p + 0x16) = clut;
            Gpu_AddPrim(U32(0x87F94C) + 0x88, p);
            U32(0x87DC98) += 0x1C;
        }
        gptr p = U32(0x87DC98);
        Gpu_SetDrawMode(p, 1, 0, tpage, 0);
        Gpu_AddPrim(U32(0x87F94C) + 0x88, p);
        U32(0x87DC98) += 0xC;
    }
}

/* a new game's state: the clear data's options kept */
void DC2CALL Title2_NewState(void)                                  /* 0x4A6D70 */
{
    memset(MEM(G() + 0x108C), 0, 0x118 * 4);
    U8(G() + 0x108D) = U8(CLEAR_DATA + 1);
    G16(0x1092) = U16(CLEAR_DATA + 6);
    U8(G() + 0x1094) = U8(CLEAR_DATA + 8);
    if (U8(G() + 0x1094) & 1) Flag_Set(9, 0x35, 1);
    Game_ClearState();
    U8(G() + 0x662) = 0;
}

static void set_all(gptr t, gptr first, gptr rest)
{
    Title_SetAnim(t, 0, U32(first));
    for (u32 i = 1; i < 5; i++) Title_SetAnim(t, i, U32(rest + i * 4));
}

void DC2CALL Title2_NewGame(gptr t)                                 /* 0x4A6730: difficulty */
{
    static const gptr text[3] = {0x724066, 0x7240B4, 0x7240FA};
    switch ((s16)G16(0x770)) {
    case 0: {
        u32 btn = U32(0x87DC00);
        if (btn & 0x40) {                         /* back */
            Se_Play(0, 1);
            G16(0x76E)--;
            set_all(t, LBL2_NORMAL, LBL2_IDLE);
        } else if (btn & 0x5000) {                /* up / down: entries 1..2 */
            s8 c = (s8)U8(t + 1);
            Se_Play(0, 0);
            Title_SetAnim(t, (u32)(s32)c, U32(LBL2_DIFF + c * 4));
            if (btn & 0x1000) { c--; U8(t + 1) = (u8)c; if (c < 1) U8(t + 1) = 2; }
            else { c++; U8(t + 1) = (u8)c; if (c > 2) U8(t + 1) = 1; }
            c = (s8)U8(t + 1);
            Title_SetAnim(t, (u32)(s32)c, U32(LBL2_DIFF_SEL + c * 4));
        } else if (btn & 0x830) {
            Se_Play(0, 0x59);
            G16(0x770)++;
            Fade_Set(1, 0x96, 1);
        }
        Text_Draw(text[(s8)U8(t + 1)], 0x20, 0xB8, 0x64, 0x808080, 1, 0);
        break; }
    case 1:
        if (Flag_Check(2, 6)) break;
        Title2_NewState();
        U8(G() + 0x108E) = (u8)(s8)U8(t + 1);     /* difficulty */
        U32(0x87DE00) = 2;
        Game_Start(0x10);
        break;
    default: break;
    }
}

/* a new game from the clear data (or the room it was saved in) */
void DC2CALL Title2_ClearGame(gptr t)                               /* 0x4A6910 */
{
    (void)t;
    switch ((s16)G16(0x770)) {
    case 0:
        G16(0x770)++;
        Se_Play(0, 0x59);
        Fade_Set(1, 0x96, 1);
        break;
    case 1:
        if (Flag_Check(2, 6)) break;
        U8(G() + 0x662) = 1;
        memcpy(MEM(G() + 0x108C), MEM(CLEAR_DATA), 0x118 * 4);
        if (G16(0x1090) == 0x101) { Game_Start(0x10); break; }
        Game_ClearState();
        Task_Chain(0x4510F0);
        break;
    default: break;
    }
}

void DC2CALL Title2_Extra(gptr t)                                   /* 0x4A69C0: Game_Start(0x19) */
{
    (void)t;
    switch ((s16)G16(0x770)) {
    case 0:
        G16(0x770)++;
        Se_Play(0, 0x59);
        Fade_Set(1, 0xC8, 1);
        break;
    case 1:
        if (Flag_Check(2, 6)) break;
        Flag_Set(2, 0x21, 0);
        PcIn_FixedKeys(0);
        U8(G() + 0xB48) = 0;
        G16(0x76E) = 0; G16(0x770) = 0;
        Game_Start(0x19);
        break;
    default: break;
    }
}

void DC2CALL Title2_Load(gptr t)                                    /* 0x4A6A60 */
{
    (void)t;
    switch ((s16)G16(0x770)) {
    case 0: G16(0x770)++; Fade_Set(1, 0xA, 1); break;
    case 1:
        if (Flag_Check(2, 6)) break;
        G16(0x67A) = 0xFFFF;
        G16(0x76E) = 0; G16(0x770) = 0;
        Game_Start(2);
        break;
    default: break;
    }
}

void DC2CALL Title2_Option(gptr t)                                  /* 0x4A6AE0 */
{
    (void)t;
    switch ((s16)G16(0x770)) {
    case 0: G16(0x770)++; Fade_Set(1, 0xA, 1); break;
    case 1:
        if (Flag_Check(2, 6)) break;
        U8(G() + 0xB48) = 0;
        G16(0x76E) = 0; G16(0x770) = 0;
        Game_Start(0);
        break;
    default: break;
    }
}

void DC2CALL Title2_Run(gptr t)                                     /* 0x4A6460 */
{
    static const gptr help[5] = {0x7292CA, 0x729314, 0x729352, 0x729384, 0x729386};
    switch ((s16)G16(0x76E)) {
    case 0:
        G16(0x76E) = 1;
        Flag_Set(3, 0, 1);
        U32(G() + 8) = 0x6C1500;
        for (u32 i = 0; i < 5; i++) Title_SetAnim(t, i, U32(LBL2_IDLE + i * 4));
        Title_SetAnim(t, (u32)(s32)(s8)U8(t), U32(LBL2_NORMAL + (s8)U8(t) * 4));
        Fade_Set(0, 0xA, 1);
        U32(0x87DE00) = 0;
        break;
    case 1: {
        u32 btn = U32(0x87DC00);
        if (btn & 0x5000) {                       /* up / down */
            s8 c = (s8)U8(t);
            Se_Play(0, 0);
            Title_SetAnim(t, (u32)(s32)c, U32(LBL2_IDLE + c * 4));
            if (btn & 0x1000) { c--; U8(t) = (u8)c; if (c < 0) U8(t) = 4; }
            else { c++; U8(t) = (u8)c; if (c > 4) U8(t) = 0; }
            c = (s8)U8(t);
            Title_SetAnim(t, (u32)(s32)c, U32(LBL2_NORMAL + c * 4));
        } else if (btn & 0x820) {                 /* confirm */
            if (U8(t) != 1 && U8(t) != 2) Se_Play(0, 2);
            G16(0x76E)++;
            U8(t + 1) = 1;
            if (U8(t) == 0) {
                Title_SetAnim(t, 0, U32(LBL2_DIFF));
                Title_SetAnim(t, 1, U32(LBL2_DIFF_SEL + 4));
                Title_SetAnim(t, 2, U32(LBL2_DIFF + 8));
                U8(t + 0xD) = 0; U8(t + 0x3D) = 0; U8(t + 0x31) = 0;   /* labels 0, 4, 3 hidden */
            }
        }
        break; }
    case 2:
        switch ((s8)U8(t)) {
        case 0: Title2_NewGame(t); break;
        case 1: Title2_ClearGame(t); break;
        case 2: Title2_Extra(t); break;
        case 3: Title2_Load(t); break;
        case 4: Title2_Option(t); break;
        default: break;
        }
        break;
    default: break;
    }
    Title2_AnimAll(t);
    Title2_DrawAll(t);
    if (G16(0x76E) != 2) {
        gptr s = help[(s8)U8(t)];
        Text_Draw(s, Text_Left(s) + 2, 0xCB, 0x64, 0x808080, 1, 0);
    }
    u32 k = U32(0x87DE0C) << 12;
    Gpu_DrawOT(U32(0x87F94C) + 0x88, 0x87CBFC + k, 0x87BC00 + k);
}

void DC2CALL Title2_Init(gptr t)                                    /* 0x4A6420 */
{
    memset(MEM(t), 0, 0x40);
    Game_ClearState();
    U8(G() + 0x927) = 0;
    G16(0x76C)++;
    Flag_Set(3, 0, 0);
}

void DC2CALL Title2Task(void)                                       /* 0x4A63F0 */
{
    for (;;) {
        switch ((s16)G16(0x76C)) {
        case 0: Title2_Init(TITLE2_T); break;
        case 1: Title2_Run(TITLE2_T); break;
        default: Orig_Call1(U32(0x73B2D0 + (s16)G16(0x76C) * 4), TITLE2_T); break;
        }
        Task_Sleep(1);
    }
}
