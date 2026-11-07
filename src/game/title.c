/* Title screen (task 0x4A5730): menu (new game, load, options), the opening
 * movie and the attract demo that alternate when nobody presses a button,
 * and the difficulty choice. State: G+0x76C mode, +0x76E screen, +0x770
 * step, +0x772 sub-step; T = 0x759EF0 {s16 timer, s8 cursor, 3 animated
 * labels of 12 bytes at +4, u8 movie/demo +0x28, demo number +0x29,
 * difficulty cursor +0x2A, back-from-menu +0x2B, demo input +0x2C,
 * saved G+0x14EC at +0x30}. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/pcio.h"
#include "dc2/gte.h"

#define TITLE_T       0x759EF0u
#define LBL_NORMAL    0x73B248u   /* label animations per entry: highlighted */
#define LBL_IDLE      0x73B254u   /* not highlighted */
#define LBL_DIFF_SEL  0x73B260u
#define LBL_DIFF      0x73B26Cu
#define G16(o)        U16(G() + (o))

static void load_wait_raw(u32 id, gptr dest)
{
    while (File_IsBusy()) Task_Sleep(1);
    File_RequestRaw(id, dest);
    while (File_IsBusy()) Task_Sleep(1);
}

/* ---- animated labels: frames of 8 bytes {u, v, w, h, next|flags, time, dx, dy} */
void DC2CALL Title_SetAnim(gptr t, u32 i, gptr a)                   /* 0x4A6370 */
{
    gptr s = t + 4 + i * 12;
    U8(s + 9) = 1;
    U32(s) = a;
    U32(s + 4) = a;
    U8(s + 8) = U8(a + 5);
}

u32 DC2CALL Title_AnimStep(gptr t, u32 i)                           /* 0x4A63A0 */
{
    gptr s = t + 4 + i * 12;
    if (U8(s + 8)) { U8(s + 8)--; return 0; }
    gptr f = U32(s + 4);
    u8 b = U8(f + 4);
    if (b & 0x20) return 0;                       /* last frame */
    f += (b & 0x1F) * 8;
    U32(s + 4) = f;
    U8(s + 8) = U8(f + 5);
    if (U8(f + 5) != 1) return 0;
    return (u32)(s32)((s8)U8(f + 4) >> 5);
}

/* one label: n sprites (n = byte 4 of the first frame) and a draw-mode prim */
void DC2CALL Title_Draw(s32 x, s32 y, u32 color, gptr f)            /* 0x4A6210 */
{
    u8 h = U8(f + 4);
    u32 n = h & 0x1F;
    u32 bits = (u32)(s32)((s8)h >> 5) >> 1;
    u32 code = ((bits & 2) | 0x64) << 24 | color;
    u16 clut = (u16)((bits & 1) + 0x7E40);
    for (; n; n--, f += 8) {
        gptr p = U32(0x87DC98);
        Gpu_SetSprt(p);
        U32(p + 4) = code;
        F32(p + 8) = (float)(u32)(x + (s8)U8(f + 6));
        F32(p + 0xC) = (float)(u32)(y + (s8)U8(f + 7));
        U8(p + 0x14) = U8(f);
        U8(p + 0x15) = U8(f + 1);
        U16(p + 0x18) = U8(f + 2);
        U16(p + 0x1A) = U8(f + 3);
        U16(p + 0x16) = clut;
        Gpu_AddPrim(U32(0x87F94C) + 0x88, p);
        U32(0x87DC98) += 0x1C;
    }
    gptr p = U32(0x87DC98);
    Gpu_SetDrawMode(p, 1, 0, 0x37, 0);
    Gpu_AddPrim(U32(0x87F94C) + 0x88, p);
    U32(0x87DC98) += 0xC;
}

/* ---- the menu ----------------------------------------------------------------------- */
static void menu_show(gptr t)
{
    G16(0x770)++;
    Flag_Set(3, 0, 1);
    U32(G() + 8) = 0x600000;
    U16(t) = 0x384;                               /* idle frames before the demo */
    Title_SetAnim(t, 0, U32(LBL_IDLE));
    Title_SetAnim(t, 1, U32(LBL_IDLE + 4));
    Title_SetAnim(t, 2, U32(LBL_IDLE + 8));
    u8 c = U8(0x87DC90);                          /* a save exists: start on "load" */
    U8(t + 2) = c;
    Title_SetAnim(t, (u32)(s32)(s8)c, U32(LBL_NORMAL + (s8)c * 4));
    U32(0x87DE00) = 0;
}

void DC2CALL Title_NewGame(gptr t)                                  /* 0x4A5EC0: difficulty */
{
    static const gptr text[3] = {0x724066, 0x7240B4, 0x7240FA};
    switch ((s16)G16(0x772)) {
    case 0: {
        u32 btn = U32(0x87DC00);
        if (btn & 0x40) {                         /* back */
            Fn_48C0B0(0, 1);
            G16(0x770)--;
            Title_SetAnim(t, 0, U32(LBL_NORMAL));
            Title_SetAnim(t, 1, U32(LBL_IDLE + 4));
            Title_SetAnim(t, 2, U32(LBL_IDLE + 8));
        } else if (btn & 0x5000) {                /* up / down: entries 1..2 */
            s8 c = (s8)U8(t + 0x2A);
            Fn_48C0B0(0, 0);
            Title_SetAnim(t, (u32)(s32)c, U32(LBL_DIFF + c * 4));
            if (btn & 0x1000) { c--; U8(t + 0x2A) = (u8)c; if (c < 1) U8(t + 0x2A) = 2; }
            else { c++; U8(t + 0x2A) = (u8)c; if (c > 2) U8(t + 0x2A) = 1; }
            c = (s8)U8(t + 0x2A);
            Title_SetAnim(t, (u32)(s32)c, U32(LBL_DIFF_SEL + c * 4));
        } else if (btn & 0x820) {
            Fn_48C0B0(0, 0x59);
            G16(0x772)++;
            Fade_Set(1, 0x96, 1);
        }
        Text_Draw(text[(s8)U8(t + 0x2A)], 0x20, 0xB8, 0x64, 0x808080, 1, 0);
        u32 k = U32(0x87DE0C) << 12;
        Gpu_DrawOT(U32(0x87F94C) + 0x88, 0x87CBFC + k, 0x87BC00 + k);
        break; }
    case 1:
        if (Flag_Check(2, 6)) break;
        memset(MEM(G() + 0x108C), 0, 0x118 * 4);   /* new game state */
        G16(0x66A) = 0; G16(0x668) = 0; G16(0x666) = 0; G16(0x664) = 0;
        U8(G() + 0x108E) = (u8)(s8)U8(t + 0x2A);  /* difficulty */
        U32(0x87DE00) = 2;
        Game_Start(0x10);
        break;
    default: break;
    }
}

static void to_game(gptr t, u32 mode)
{
    U8(t + 0x2B) = 1;
    G16(0x76E) = 0; G16(0x770) = 0; G16(0x772) = 0;
    Game_Start(mode);
}

void DC2CALL Title_Load(gptr t)                                     /* 0x4A60F0 */
{
    if ((s16)G16(0x772) == 0) { G16(0x772)++; Fade_Set(1, 0xA, 1); return; }
    if ((s16)G16(0x772) == 1 && !Flag_Check(2, 6)) { G16(0x67A) = 0xFFFF; to_game(t, 2); }
}

void DC2CALL Title_Option(gptr t)                                   /* 0x4A6180 */
{
    if ((s16)G16(0x772) == 0) { G16(0x772)++; Fade_Set(1, 0xA, 1); return; }
    if ((s16)G16(0x772) == 1 && !Flag_Check(2, 6)) { U8(G() + 0xB48) = 0; to_game(t, 0); }
}

void DC2CALL Title_Menu(gptr t)                                     /* 0x4A57B0 */
{
    switch ((s16)G16(0x770)) {
    case 0:
        G16(0x770)++;
        while (File_IsBusy()) Task_Sleep(1);
        File_RequestLoad(0x1E0);                  /* title graphics */
        while (File_IsBusy()) Task_Sleep(1);
        if (U8(t + 0x2B)) {                       /* back from a menu: no movie */
            G16(0x770) = 3;
            Fade_Set(0, 0xA, 1);
            menu_show(t);
            break;
        }
        /* fall through */
    case 1:
        G16(0x770)++;
        Flag_Set(3, 0, 0);
        Fn_473130(0);
        break;
    case 2:
        Fn_4731F0();
        if (Flag_Check(2, 0x17)) break;
        G16(0x770)++;
        /* fall through */
    case 3:
        menu_show(t);
        break;
    case 4: {
        U16(t)--;
        u32 btn = U32(0x87DC00);
        u16 tm = U16(t);
        if (btn & 0x5000) {                       /* up / down */
            s8 c = (s8)U8(t + 2);
            Fn_48C0B0(0, 0);
            Title_SetAnim(t, (u32)(s32)c, U32(LBL_IDLE + c * 4));
            if (btn & 0x1000) { U8(t + 2) = (u8)(c - 1); if ((s8)U8(t + 2) < 0) U8(t + 2) = 2; }
            else { U8(t + 2) = (u8)(c + 1); if ((s8)U8(t + 2) > 2) U8(t + 2) = 0; }
            c = (s8)U8(t + 2);
            Title_SetAnim(t, (u32)(s32)c, U32(LBL_NORMAL + c * 4));
            U16(t) = 0x384;
        } else if (btn & 0x820) {                 /* confirm */
            Fn_48C0B0(0, 2);
            G16(0x770)++;
            U8(t + 0x2A) = 1;
            if (!U8(t + 2)) {
                Title_SetAnim(t, 0, U32(LBL_DIFF));
                Title_SetAnim(t, 1, U32(LBL_DIFF_SEL + 4));
                Title_SetAnim(t, 2, U32(LBL_DIFF + 8));
            }
        } else if (!tm) {                         /* idle: movie or demo */
            G16(0x770) = 6;
            Fade_Set(1, 0xA, 1);
        }
        break; }
    case 5:
        switch ((s8)U8(t + 2)) {
        case 0: Title_NewGame(t); break;
        case 1: Title_Load(t); break;
        case 2: Title_Option(t); break;
        default: break;
        }
        break;
    case 6:
        if (!Flag_Check(2, 6)) { G16(0x76E)++; G16(0x770) = 0; }
        break;
    default: break;
    }
    if ((s16)G16(0x770) >= 4) {
        Title_AnimStep(t, 0); Title_AnimStep(t, 1); Title_AnimStep(t, 2);
        if ((s16)G16(0x770) == 4) Title_Draw(0xA0, 0x88, 0x808080, U32(t + 8));
        Title_Draw(0xA0, 0x88, 0x808080, U32(t + 0x14));
        Title_Draw(0xA0, 0x88, 0x808080, U32(t + 0x20));
    }
}

void DC2CALL Title_Movie(gptr t)                                    /* 0x4A5B50 */
{
    switch ((s16)G16(0x770)) {
    case 0:
        Flag_Set(3, 0, 0);
        G16(0x770)++;
        Fn_473130(U8(t + 3) + 1u);
        break;
    case 1:
        Fn_4731F0();
        if (!Flag_Check(2, 0x17)) G16(0x770)++;
        break;
    case 2:
        U8(t + 2) = 0; U8(t + 0x2B) = 0;
        G16(0x76E) = 0; G16(0x770) = 0;
        U8(t + 0x28) ^= 1;
        break;
    default: break;
    }
}

void DC2CALL Title_Demo(gptr t)                                     /* 0x4A5BF0 */
{
    switch ((s16)G16(0x770)) {
    case 0:
        G16(0x770)++;
        Demo_Init();
        memcpy(MEM(t + 0x30), MEM(G() + 0x14EC), 16);
        load_wait_raw(U8(t + 0x29) + 0x23u, 0x65F500);    /* DEMO0x.TRG */
        memcpy(MEM(G() + 0x108C), MEM(0x65F504), 0x118 * 4);
        U16(0x87DE10) = U16(0x65F500);
        U32(t + 0x2C) = 0x65F964;
        Flag_Set(0, 0xA, 1);
        U8(G() + 0x14F0) = 1;
        G16(0x840) = G16(0x10AC); G16(0x842) = G16(0x10AE); G16(0x844) = G16(0x10B0); G16(0x846) = G16(0x10B2);
        U16(t) = 0;
        G16(0x664) = 0;
        U8(G() + 0x662) = 1;
        Fn_451120();
        Fn_4512F0();
        break;
    case 1:
        G16(0x770)++;
        U16(0x87DE10) = U16(0x65F500);
        Fn_451330();
        Flag_Set(2, 0xF, 1);
        break;
    case 2: {
        U16(t)++;
        gptr e = U32(t + 0x2C);                    /* recorded pad state per frame */
        U32(t + 0x2C) = e + 0x10;
        U16(0x87DC08) = U16(e);
        U16(0x87DC94) = U16(e + 2);
        Demo_Input();
        u16 tm = U16(t);
        if (tm >= 0x244 || (tm > 0x14 && (U32(0x87DC12) & 0x970))) Fade_Set(1, 0xA, 1);
        if (U8(G() + 0x814) == 1 && !Flag_Check(2, 6)) G16(0x770)++;
        break; }
    case 3:
        U8(t + 2) = 0;
        Room_Reset();
        U8(t + 0x2B) = 0;
        G16(0x76E) = 0; G16(0x770) = 0;
        U8(t + 0x28) ^= 1;
        U8(t + 0x29) = (U8(t + 0x29) + 1) & 3;
        memcpy(MEM(G() + 0x14EC), MEM(t + 0x30), 16);
        Flag_Set(0, 0xA, 0);
        break;
    default: break;
    }
}

void DC2CALL Title_Attract(gptr t)                                  /* 0x4A5B30 */
{
    if (U8(t + 0x28)) Title_Demo(t); else Title_Movie(t);
}

void DC2CALL Title_Init(gptr t)                                     /* 0x4A5760 */
{
    memset(MEM(t), 0, 0x40);
    G16(0x76C)++;
    Flag_Set(3, 0, 0);
}

void DC2CALL Title_Run(gptr t)                                      /* 0x4A5790 */
{
    switch ((s16)G16(0x76E)) {
    case 0: Title_Menu(t); break;
    case 1: Title_Attract(t); break;
    case 2: Title_Movie(t); break;
    case 3: Title_Demo(t); break;
    default: Orig_Call1(U32(0x73B280 + (s16)G16(0x76E) * 4), t); break;
    }
}

void DC2CALL TitleTask(void)                                        /* 0x4A5730 */
{
    for (;;) {
        switch ((s16)G16(0x76C)) {
        case 0: Title_Init(TITLE_T); break;
        case 1: Title_Run(TITLE_T); break;
        case 2: Title_Menu(TITLE_T); break;
        case 3: Title_Attract(TITLE_T); break;
        case 4: Title_Movie(TITLE_T); break;
        case 5: Title_Demo(TITLE_T); break;
        default: Orig_Call1(U32(0x73B278 + (s16)G16(0x76C) * 4), TITLE_T); break;
        }
        Task_Sleep(1);
    }
}
