/* The extra crisis result screen (task 0x46D990, Game_Start(0x18)): five
 * grades (ranks 0..4 against the thresholds below; their points add up to
 * the total's grade), the total grade's model turning in the light, then
 * back to the extra crisis menu (Game_Start(0x19)). T = 0x74D610 (0x98
 * bytes) {u8 mode (table 0x723B08), u8 step, u32 timer +4, labels of 16
 * bytes at +8..+0x48, colour matrix +0x58, light vectors +0x78, ranks
 * +0x90..+0x94}. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/pcio.h"
#include "dc2/task.h"
#include "dc2/gte.h"

#define RESULT_T      0x74D610u
#define RANK_LBL      0x723AD8u   /* rank labels, 5 */
#define GRADE_LBL     0x723AECu   /* total grade labels, 5 */
#define RANK_PTS      0x723B00u   /* points per rank, u8[5] */
#define RES_ANGLE     0x723B28u   /* the model's rotation, SVECTOR */

/* a label {first frame, frame, palette add, time, active} */
void DC2CALL Res_SetLabel(gptr l, gptr a, u32 pal)                   /* 0x46DE90 */
{
    U8(l + 0xD) = 1;
    U32(l) = a;
    U32(l + 4) = a;
    U8(l + 0xC) = U8(a + 5);
    U32(l + 8) = pal;
}

/* the label's frame: n sprites at (x, y) and a draw-mode prim */
void DC2CALL Res_DrawLabel(s32 x, s32 y, gptr l)                     /* 0x46DD40 */
{
    gptr f = U32(l + 4);
    u8 h = U8(f + 4);
    u32 n = h & 0x1F;
    u16 clut = (u16)(((h >> 6) & 1) + U32(l + 8) + 0x7C00);
    for (; n; n--, f += 8) {
        gptr p = U32(0x87DC98);
        Gpu_SetSprt(p);
        U32(p + 4) = 0x64808080;
        F32(p + 8) = (float)(u32)(x + (s8)U8(f + 6));
        F32(p + 0xC) = (float)(u32)(y + (s8)U8(f + 7));
        U8(p + 0x14) = U8(f);
        U8(p + 0x15) = U8(f + 1);
        U16(p + 0x18) = U8(f + 2);
        U16(p + 0x1A) = U8(f + 3);
        U16(p + 0x16) = clut;
        Gpu_AddPrim(U32(0x87F94C) + 0x8C, p);
        U32(0x87DC98) += 0x1C;
    }
    gptr p = U32(0x87DC98);
    Gpu_SetDrawMode(p, 1, 0, 0xA5, 0);
    Gpu_AddPrim(U32(0x87F94C) + 0x8C, p);
    U32(0x87DC98) += 0xC;
}

/* the ranks: 0 best .. 4 (no threshold reached); the fifth is the total */
void DC2CALL Result_Grade(gptr t)                                   /* 0x46DEB0 */
{
    static const u32 thr[5][4] = {
        {0x50, 0x3C, 0x28, 0x14}, {0x3138, 0x2A30, 0x1C20, 0x708}, {0x46, 0x3C, 0x32, 0x28},
        {0x64C8, 0x62D4, 0x6144, 0x5FB4}, {0x10, 0xB, 7, 3}};
    gptr g = G();
    u32 sum = 0;
    for (u32 i = 0; i < 5; i++) {
        u32 v;
        switch (i) {
        case 0: v = U16(g + 0x84E); break;
        case 1: v = U32(g + 0x12C8); break;
        case 2: { u32 d = U32(g + 0x684); v = d ? U32(g + 0x688) * 100u / d : 0; break; }
        case 3: v = U32(g + 0x11F4); break;
        default: v = sum; break;
        }
        u32 r = 4;
        for (u32 c = 0; c < 4; c++) if (thr[i][c] <= v) { r = c; break; }
        U8(t + 0x90 + i) = (u8)r;
        sum += U8(RANK_PTS + r);
    }
}

void DC2CALL Result_Init(gptr t)                                    /* 0x46D9D0 */
{
    memset(MEM(0x5EE968), 0, 0x296 * 4);
    Snd_FadeAll();
    Strm_Start(0, 5);
    U8(t)++;
    Screen_LoadImage(0);
    Flag_Set(3, 0, 1);
    U32(G() + 8) = 0x6C1500;
    Shake_Set(0xFFFFFFFF, 0, 0);
    Room_Reset();
    gptr g = G(), c = g + 0x778;
    U16(g + 0x7A4) = 0; U16(g + 0x7A2) = 0;
    U16(c + 0x28) = 0; U16(c + 0x30) = 0; U16(c + 0x32) = 0;
    U16(c + 0x34) = 0x1194; U16(c + 0x20) = 0xFD0; U16(c + 0x24) = 0; U16(c + 0x22) = 0;
    SetGeomScreen(0x140);
    gptr v = Tmp_Push(8);
    U16(v) = (u16)-U16(c + 0x20); U16(v + 2) = (u16)-U16(c + 0x22); U16(v + 4) = (u16)-U16(c + 0x24);
    RotMatrix(v, G() + 0x778);
    Tmp_Pop(8);
    Result_Grade(t);
    U16(t + 0x78) = 1000; U16(t + 0x7A) = (u16)-1000; U16(t + 0x7C) = (u16)-1000;
    U16(t + 0x80) = (u16)-1000; U16(t + 0x82) = (u16)-1000; U16(t + 0x84) = (u16)-1000;
    U16(t + 0x88) = 0; U16(t + 0x8A) = (u16)-1000; U16(t + 0x8C) = 1000;
    switch (U8(t + 0x94)) {                       /* the colour matrix by grade */
    case 0: U16(t + 0x5E) = 0x4E20; U16(t + 0x58) = 0x7530; U16(t + 0x64) = 0x7530;
            U16(t + 0x5A) = 0x2710; U16(t + 0x60) = 0x1770; break;
    case 1: U16(t + 0x5A) = 1000; U16(t + 0x58) = 0x7530; U16(t + 0x5E) = 0x7530;
            U16(t + 0x64) = 0x7530; U16(t + 0x60) = 0; break;
    case 2: U16(t + 0x5A) = 1000; U16(t + 0x58) = 0x3A98; U16(t + 0x5E) = 0x3A98;
            U16(t + 0x64) = 0x3A98; U16(t + 0x60) = 0; break;
    default: U16(t + 0x5E) = 0x2AF8; U16(t + 0x58) = 0x2710; U16(t + 0x64) = 0x2710;
             U16(t + 0x5A) = 0; U16(t + 0x60) = 0; break;
    }
    U16(t + 0x66) = 0; U16(t + 0x5C) = 0; U16(t + 0x62) = 0; U16(t + 0x68) = 0;
    Res_SetLabel(t + 0x08, U32(RANK_LBL + U8(t + 0x90) * 4), 0);
    Res_SetLabel(t + 0x18, U32(RANK_LBL + U8(t + 0x91) * 4), 0);
    Res_SetLabel(t + 0x38, U32(RANK_LBL + U8(t + 0x93) * 4), 0);
    Res_SetLabel(t + 0x28, U32(RANK_LBL + U8(t + 0x92) * 4), 0);
    Res_SetLabel(t + 0x48, U32(GRADE_LBL + U8(t + 0x94) * 4), 0);
    U32(0x87DE00) = 2;
    Fade_Set(0, 0x14, 1);
}

/* the grade models (table 0x723B14 by the total grade) */
static void tex(gptr m, gptr p, u16 tpage, u16 clut, u8 mode)
{
    U16(p) = tpage; U16(p + 2) = clut; U8(p + 8) = mode;
    Model_SetTexture(m, p);
}

void DC2CALL Result_Model0(void)                                    /* 0x46E010 */
{
    gptr p = Tmp_Push(0xC);
    EnvMapDraw(0x5E0000);
    tex(0x5E0000, p, 0x8E, 0x7FC0, 3);
    tex(0x5E0000, p, 0x9E, 0x7F80, 2);
    Tmp_Pop(0xC);
}

void DC2CALL Result_Model1(void)                                    /* 0x46E070 */
{
    gptr p = Tmp_Push(0xC);
    EnvMapDraw(0x5E3758);
    tex(0x5E3758, p, 0x8E, 0x7FC0, 3);
    tex(0x5E3758, p, 0x9C, 0x7F40, 2);
    Model_DrawTex(0x5E62F0, 0x95, 0x7EC0);
    Tmp_Pop(0xC);
}

void DC2CALL Result_Model2(void)                                    /* 0x46E0E0 */
{
    gptr p = Tmp_Push(0xC);
    EnvMapDraw(0x5E6DE0);
    tex(0x5E6DE0, p, 0x8E, 0x7FC0, 3);
    Model_DrawTex(0x5E6DE0, 5, 0x7C0E);
    Model_DrawTex(0x5E9118, 0x86, 0x7E80);
    Tmp_Pop(0xC);
}

void DC2CALL Result_Model3(void)                                    /* 0x46E140 */
{
    gptr p = Tmp_Push(0xC);
    EnvMapDraw(0x5E9988);
    tex(0x5E9988, p, 0x9A, 0x7F00, 2);
    Model_DrawTex(0x5EB948, 0x87, 0x7D80);
    Tmp_Pop(0xC);
}

void DC2CALL Result_Model4(void)                                    /* 0x46E190 */
{
    gptr p = Tmp_Push(0xC);
    EnvMapDraw(0x5EBC58);
    U32(p + 4) = 0x808080;
    tex(0x5EBC58, p, 0x8E, 0x7FC0, 1);
    Model_DrawTex(0x5EBC58, 5, 0x7C0F);
    Tmp_Pop(0xC);
}

/* the model: turning (the second pad turns it by hand), lit by T+0x78 in
   view space with the colour matrix T+0x58 */
void DC2CALL Result_Draw(gptr t)                                    /* 0x46E1E0 */
{
    static const u32 dist[5] = {0x1CC5, 0x1702, 0x1702, 0x14C3, 0xED8};
    gptr L = Tmp_Push(0x58), m = L + 0x18, lm = L + 0x38;
    U16(RES_ANGLE + 2) = (U16(RES_ANGLE + 2) + 0x10) & 0xFFF;
    RotMatrix(RES_ANGLE, m);
    u32 pad = U32(0x87DC10);
    if (pad & 0x2000) U16(RES_ANGLE + 2) -= 0x50;
    if (pad & 0x8000) U16(RES_ANGLE + 2) += 0x40;
    u16 ax = U16(RES_ANGLE);
    if (pad & 0x1000) ax -= 0x40;
    if (pad & 0x4000) ax += 0x40;
    if (U8(0x87DDFC) & 0x40) ax = 0x64;
    U16(RES_ANGLE) = ax & 0xFFF;
    SetGeomScreen(0x140);
    SetGeomOffset(0xFA, 0x78);
    gptr c = G() + 0x778;
    U16(c + 0x34) = (u16)dist[U8(t + 0x94)];
    MulMatrix0(c, m, m);
    U32(L + 4) = (u32)-(s32)S16(c + 0x28); U32(L + 8) = (u32)-(s32)S16(c + 0x2A); U32(L + 0xC) = (u32)-(s32)S16(c + 0x2C);
    ApplyMatrixLV(c, L + 4, m + 0x14);
    U32(m + 0x14) += (u32)(s32)S16(c + 0x30);
    U32(m + 0x18) += (u32)(s32)S16(c + 0x32);
    U32(m + 0x1C) += U16(c + 0x34);
    U32(L) = 0x606060;
    gte_ldrgb(L);
    TransposeMatrix(m, lm);
    SetRotMatrix(lm);
    gte_ldv3c(t + 0x78);                          /* the lights into view space */
    gte_rtv0(); gte_stsv(lm);
    gte_rtv1(); gte_stsv(lm + 6);
    gte_rtv2(); gte_stsv(lm + 0xC);
    SetLightMatrix(lm);
    SetColorMatrix(t + 0x58);
    SetRotMatrix(m);
    SetTransMatrix(m);
    Tmp_Pop(0x58);
    switch (U8(t + 0x94)) {
    case 0: Result_Model0(); break;
    case 1: Result_Model1(); break;
    case 2: Result_Model2(); break;
    case 3: Result_Model3(); break;
    case 4: Result_Model4(); break;
    default: ((void (DC2CALL *)(void))(uintptr_t)U32(0x723B14 + U8(t + 0x94) * 4))(); break;
    }
    u32 k = U32(0x87DE0C) << 12;
    Gpu_DrawOT(U32(0x87F94C) + 0x88, 0x87CBFC + k, 0x87BC00 + k);
}

void DC2CALL Result_Run(gptr t)                                     /* 0x46DC20 */
{
    SetBackColor(0x40, 0x40, 0x40);
    Result_Draw(t);
    Res_DrawLabel(0x88, 0x40, t + 0x08);
    Res_DrawLabel(0x88, 0x58, t + 0x18);
    Res_DrawLabel(0x88, 0x70, t + 0x28);
    Res_DrawLabel(0x88, 0x88, t + 0x38);
    Res_DrawLabel(0x88, 0xA0, t + 0x48);
    switch (U8(t + 1)) {
    case 0:
        if (Flag_Check(2, 6)) break;
        U32(t + 4) = 0xD2;                        /* frames before a button counts */
        U8(t + 1)++;
        break;
    case 1:
        if (U32(t + 4)) { U32(t + 4)--; break; }
        if (!(U32(0x87DC00) & 0x920)) break;
        U8(t + 1)++;
        Fade_Set(1, 0x14, 1);
        break;
    case 2:
        if (Flag_Check(2, 6)) break;
        U8(t + 1) = 0;
        U8(t)++;
        break;
    default: break;
    }
}

void DC2CALL Result_End(gptr t)                                     /* 0x46DD20 */
{
    (void)t;
    SetGeomOffset(0xA0, 0x78);
    Game_Start(0x19);
}

void DC2CALL ResultTask(void)                                       /* 0x46D990 */
{
    memset(MEM(RESULT_T), 0, 0x26 * 4);
    for (;;) {
        switch (U8(RESULT_T)) {
        case 0: Result_Init(RESULT_T); break;
        case 1: Result_Run(RESULT_T); break;
        case 2: Result_End(RESULT_T); break;
        default: Orig_Call1(U32(0x723B08 + U8(RESULT_T) * 4), RESULT_T); break;
        }
        Task_Sleep(1);
    }
}
