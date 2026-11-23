/* Small helpers of the frame of play: the camera block's reset and zone check,
 * background masks, fades, the screen shake, the message slots and box, the
 * sound slot bookkeeping, hidden flags, movies. */
#include <string.h>
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"
#include "dc2/pcio.h"

/* ---- the camera block G+0x778 ------------------------------------------------- */

void DC2CALL Camera_Reset(void)                                     /* 0x403480 */
{
    gptr c = G() + 0x778;
    memset(MEM(c), 0, 0x8C);
    U16(c + 0x20) = 0xFF00; U16(c + 0x22) = 0; U16(c + 0x24) = 0;
    U16(c + 0x28) = 0; U16(c + 0x2A) = 0xFA24; U16(c + 0x2C) = 0;
    U16(c + 0x30) = 0; U16(c + 0x32) = 0; U16(c + 0x34) = 0x1000;
}

/* the camera zone the player is in (unless flag 1:0xA); +0x4E the one before */
void DC2CALL Camera_ZoneTick(void)                                  /* 0x403520 */
{
    gptr c = G() + 0x778;
    if (!Flag_Check(1, 0xA)) CamZones_Check(c, U32(G() + 0xA44));
    U8(c + 0x4E) = U8(c + 0x4C);
}

/* the room's background masks: n quads (x, y in 16s, depth) over pixels */
void DC2CALL Masks_Setup(gptr data, u32 n, gptr pixels)            /* 0x4036F0 */
{
    if (!n) return;
    Masks_CreateVB(n * 6, pixels);
    for (u32 i = 0; i < n; i++) {
        gptr e = data + 4 * i;
        Masks_SetQuad(i, U8(e) << 4, (U8(e + 1) & 0xF) << 4, (u32)(s32)S16(e + 2));
    }
    Masks_BuildTexture();
    Masks_Upload(0);
    U32(0x746C9C) = 0;
}

void DC2CALL Cutscene_End(void)                                     /* 0x403A30 */
{
    U16(0x87FFEA) = 0;
    if (Flag_Check(2, 0)) Voice_Stop();
}

/* ---- the damage gauge block G+0x68C ------------------------------------------ */

void DC2CALL Gauge_Clear(void)                                      /* 0x404940 */
{
    gptr b = G() + 0x68C;
    U8(b + 0xCA) = 0; U16(b + 0xC0) = 0; U8(b + 0xCB) = 0; U16(b + 0xC2) = 0;
    U8(b + 0xCE) = 0; U8(b + 0xCF) = 0;
}

void DC2CALL Gauge_Show(void)                                       /* 0x406020 */
{
    gptr b = G() + 0x68C;
    U16(b + 0xB8) = 0x78; U8(b + 0xC8) = 1;
    Flag_Set(3, 7, 1);
}

/* shown for 60 frames from the player's health */
void DC2CALL Gauge_Reset(void)                                      /* 0x407170 */
{
    gptr b = G() + 0x68C;
    Gauge_Show();
    Gauge_Clear();
    U8(b + 0xC8) = 1;
    U16(b + 0xBC) = U16(U32(G() + 0x940) + 0x108);
    U16(b + 0xB8) = 0x3C;
    U8(b + 0xC9) = 0; U16(b + 0xBA) = 0; U8(b + 0xCC) = 0; U16(b + 0xC4) = 0;
    U32(b + 0xB4) = 0; U8(b + 0xCF) = 0; U8(b + 0xCE) = 0;
}

void DC2CALL Room_SetLighting(void)                                 /* 0x46B8C0 */
{
    gptr l = U32(G() + 0xA3C);
    SetBackColor(U8(l), U8(l + 1), U8(l + 2));
}

/* ---- the screen shake (G+0xB4C: from, to, step, steps; G+0xC27 the offset) ---- */

/* a < 0: off; else shake between a and b over c frames (0: for ever) */
void DC2CALL Shake_Set(u32 a, u32 b, u32 c)                         /* 0x46FBC0 */
{
    gptr g = G();
    if (a & 0x80000000) {
        Flag_Set(2, 0xC, 0);
        U8(g + 0xC27) = 0; U8(g + 0xB73) = 1;
        SetGeomOffset(0xA0, 0x78);
        return;
    }
    U8(g + 0xB4C) = (u8)a; U8(g + 0xB4E) = 0;
    U8(g + 0xB4D) = (u8)b; U8(g + 0xB4F) = (u8)c;
    U8(g + 0xB72) = 1;
    Flag_Set(2, 0xC, 1);
}

void DC2CALL Shake_Tick(void)                                       /* 0x452370 */
{
    if (!Flag_Check(2, 0xC)) return;
    gptr g = G(), s = g + 0xB4C;
    U8(g + 0xB72) ^= 1;
    U8(g + 0xC26) = U8(g + 0xC27);
    if (U8(g + 0xB72)) U8(g + 0xC27) = 0;
    else {
        u32 n = U8(s + 3) ? U8(s + 3) : 1, c = U8(s + 2);
        U8(g + 0xC27) = (u8)(((((n - c) * U8(s) + U8(s + 1) * c) << 16) / n) >> 16);
    }
    SetGeomOffset(0xA0, U8(G() + 0xC27) + 0x78);
    if (U8(s + 3)) {
        U8(s + 2)++;
        if (U8(s + 2) == U8(s + 3)) Shake_Set(0xFFFFFFFF, 0, 0);
    }
}

void DC2CALL Shake_Off(void)                                        /* 0x452BA0 */
{
    U8(G() + 0xC27) = 0; U8(G() + 0xC26) = 0;
    SetGeomOffset(0xA0, 0x78);
}

/* ---- G+0xB10: a level rising while flag 2:7, falling after ------------------- */
void DC2CALL Dim_Tick(void)                                         /* 0x4509C0 */
{
    gptr s = G() + 0xB10;
    s32 v;
    switch (U8(s + 2)) {
    case 0:
        if (!Flag_Check(2, 7)) return;
        U16(s) = 0; U8(s + 2)++;
        /* fall through */
    case 1:
        v = S16(s) + 6;
        if (v >= 0xFF) v = 0xFF;
        U16(s) = (u16)v;
        if ((s16)v >= 0xFF) U8(s + 2)++;
        return;
    case 2:
        if (Flag_Check(2, 7)) return;
        U16(s) = 0xFF; U8(s + 2)++;
        /* fall through */
    case 3:
        v = S16(s) - 20;
        if (v <= 0) v = 0;
        U16(s) = (u16)v;
        if ((s16)v <= 0) U8(s + 2) = 0;
        return;
    }
}

/* ---- the screen fade G+0x804: rectangle, step, level (<< 16), in/out, kind ---- */

void DC2CALL Fade_Set(u32 a, u32 b, u32 c)                          /* 0x450A70 */
{
    gptr s = G() + 0x804;
    if ((s32)S8(G() + 0x814) == (s32)a) return;
    if (a == 0) U32(s + 0xC) = 0xFF0000;
    else if (a == 1) U32(s + 0xC) = 0;
    Flag_Set(2, 6, 1);
    U8(s + 0x10) = (u8)a; U8(s + 0x11) = (u8)(b - 1);
    U16(s) = 0; U16(s + 2) = 0; U16(s + 4) = 0x140; U16(s + 6) = 0xF0;
    U32(s + 8) = 0x1000000u / b;
    U8(s + 0x12) = (u8)c;
}

/* a tile over the rectangle, its level by the kind: 1 subtracts, 2 adds, 3 red */
static void fade_tile(gptr s, int red)
{
    u8 v = (u8)(U32(s + 0xC) >> 16);
    gptr p = U32(0x87DC98);
    if (red) { U8(p + 4) = v; U8(p + 6) = 0; U8(p + 5) = 0; }
    else { U8(p + 6) = v; U8(p + 5) = v; U8(p + 4) = v; }
    Gpu_SetTile(U32(0x87DC98));
    Gpu_SetSemiTrans(U32(0x87DC98), 1);
    F32(U32(0x87DC98) + 8) = (float)S16(s);
    F32(U32(0x87DC98) + 0xC) = (float)S16(s + 2);
    U16(U32(0x87DC98) + 0x14) = U16(s + 4);
    U16(U32(0x87DC98) + 0x16) = U16(s + 6);
    Gpu_AddPrim(U32(0x87F94C) + 0x78, U32(0x87DC98));
    U32(0x87DC98) += 0x18;
}

void DC2CALL Fade_Draw(gptr s)                                      /* 0x450B10 */
{
    u32 kind = U8(s + 0x12);
    if (!Flag_Check(2, 0x1D) && !Flag_Check(2, 0x16)) {
        u8 n = U8(s + 0x11);
        U8(s + 0x11) = (u8)(n - 1);
        if (!n) Flag_Set(2, 6, 0);
        if (U8(s + 0x10) & 1) {
            if (Flag_Check(2, 6)) U32(s + 0xC) += U32(s + 8);
            else U32(s + 0xC) = 0xFF0000;
        } else {
            U32(s + 0xC) -= U32(s + 8);
            if (!Flag_Check(2, 6)) { U8(s + 0x12) = 0; U32(s + 0xC) = 0; }
        }
    }
    fade_tile(s, kind == 3);
    if (kind == 1) {
        u32 tp = Gpu_GetTPage(0, 2, 0x140, 0x100) & 0xFFFF;
        Gpu_SetDrawMode(U32(0x87DC98), 1, 0, tp, 0);
    } else if (kind == 2 || kind == 3) {
        u32 tp = Gpu_GetTPage(0, 1, 0x140, 0x100) & 0xFFFF;
        Gpu_SetDrawMode(U32(0x87DC98), 1, 0, tp, 0);
    }
    Gpu_AddPrim(U32(0x87F94C) + 0x78, U32(0x87DC98));
    U32(0x87DC98) += 0xC;
    if (kind != 3) return;
    fade_tile(s, 0);                                /* and darker over it */
    u32 tp = Gpu_GetTPage(0, 2, 0x140, 0x100) & 0xFFFF;
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, tp, 0);
    Gpu_AddPrim(U32(0x87F94C) + 0x78, U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}

void DC2CALL Fade_Tick(void)                                        /* 0x450AF0 */
{
    gptr s = G() + 0x804;
    Orig_Call1(U32(0x7224A4 + U8(s + 0x12) * 4), s);
}

/* ---- effect 0's trail: its last four matrices and places (0x74AFB0, 0x20 each) */
void DC2CALL Eff0_Trail(void)                                       /* 0x445D10 */
{
    gptr e = U32(G() + 0x93C);
    switch (S8(e + 0x46)) {
    case 0:
        if (!U32(0x87DE04) || !(U32(e + 0x50) & 1) || U8(e + 0x4E) != 3 || S8(e + 0x4F) > 1 || !(U32(e + 0x50) & 0x8000)) return;
        U8(e + 0x46)++;
        U16(e + 0x66) = 0xFFFF;
        /* fall through */
    case 1:
        for (gptr d = 0x74B010; d > 0x74AFB0; d -= 0x20) memmove(MEM(d), MEM(d - 0x20), 0x20);
        if (S16(e + 0x66) < 3) U16(e + 0x66)++;
        {
            gptr p = U32(e + 0x54);
            if (p) {
                MulMatrix0(p + 0x20, e + 0x20, 0x74AFB0);
                ApplyMatrix(p + 0x20, e + 0x40, 0x74AFC4);
                S32(0x74AFC4) += S16(p + 0x40);
                S32(0x74AFC8) += S16(p + 0x42);
                S32(0x74AFCC) += S16(p + 0x44);
            }
        }
        if (!(U32(e + 0x50) & 0x8000)) U8(e + 0x46)--;
        return;
    }
}

/* the light slots' "lit this frame" bits: the objects' (+0x190, two) and G+0x94C.. */
void DC2CALL Lights_Unmark(void)                                    /* 0x478790 */
{
    gptr g = G();
    for (gptr o = U32(g + 0x940); o < U32(G() + 0x940) + U32(0x87F940) * 0x260; o += 0x260) {
        if (!(U8(o + 0x50) & 1)) continue;
        for (gptr l = o + 0x190; l < o + 0x1C0; l += 0x18) U8(l + 0xC) &= 0xFE;
    }
    for (gptr l = G() + 0x94C; l < G() + 0xA3C; l += 0x18) U8(l + 0xC) &= 0xFE;
}

/* ---- sound slots: 32 one-shot (0x87F980, 0x10 each), 4 streams (0x87FB80, 0x14) */

void DC2CALL Snd_ReleaseAll(void)                                   /* 0x48C080 */
{
    for (gptr s = 0x87F980; s < 0x87F980 + 0x200; s += 0x10)
        if (U8(s + 8)) { Snd_Stop(U32(s)); U8(s + 8) = 0; }
}

void DC2CALL Snd_SetFade(u32 slot, u32 a, u32 b, u32 c)             /* 0x48C5D0 */
{
    gptr s = 0x87F980 + (slot & 0xFFFF) * 0x10;
    U8(s + 0xA) = (u8)c; U8(s + 0xB) = (u8)a; U16(s + 0xE) = (u16)b;
}

void DC2CALL Strm_SetFade(u32 slot, u32 a, u32 b, u32 c)            /* 0x48CE20 */
{
    gptr s = 0x87FB80 + (slot & 0xFFFF) * 0x14;
    if (!U32(s + 4) || U8(s + 8) != 1) return;
    U8(s + 0x10) = (u8)c; U8(s + 0x11) = (u8)a; U16(s + 0xC) = (u16)b;
}

/* stream slot n starts (or, while fading out, fades back in) */
void DC2CALL Strm_Start(u32 n, u32 unused)                          /* 0x48CD70 */
{
    gptr s = 0x87FB80 + n * 0x14;
    if (!U32(s + 4) || Flag_Check(0, 0xA)) return;
    if (U8(s + 8) == 1) {
        if (U8(s + 0x10) & 0x80) { Strm_SetFade(n, 5, 0x7F, 2); return; }
        return;
    }
    U8(s + 0x10) = 0; U8(s + 0x11) = 0;
    U16(s + 0xA) = 0x7F; U16(s + 0xC) = 0x7F;
    U8(s + 8) = 1;
    Strm_Play(U32(s));
    Strm_SetVolume(U32(s), (u32)((s32)S8(G() + 0x14ED) * S16(s + 0xA) / 15));
}

/* the streams of bits set in mask (8 bits: slot bit & 3, kind 5 + bit / 4) */
void DC2CALL Strm_StartMask(u32 mask)                               /* 0x48C9C0 */
{
    mask &= 0xFFFF;
    for (u32 i = 0; i < 8; i++)
        if (mask & (1u << i)) Strm_Start((u32)((s32)i % 4), (i >> 2) + 5);
}

/* leaving the room: the streams the next room does not keep, and the sounds
   still playing that do not stay (+0xA bit 7), fade out */
void DC2CALL Snd_LeaveRoom(void)                                    /* 0x48CA40 */
{
    gptr d = Door_Dest();
    u16 cur = U16(Room_SoundSlot(U8(d + 9), U8(d + 8)));
    if ((cur ^ U16(0x87FBD4)) & 0xFF00) {
        for (u32 i = 0; i < 4; i++) Strm_SetFade(i & 3, 4, 0, 0x80);
    } else {
        u32 keep = U16(Room_SoundSlot(U8(d + 9), U8(d + 8)) + 2);
        for (u32 i = 0; i < 8; i++)
            if (!(keep & (1u << i)) && U8(0x87FB88 + (i & 3) * 0x14)) Strm_SetFade(i & 3, 4, 0, 0x80);
    }
    for (u32 i = 0; i < 0x20; i++) {
        gptr s = 0x87F980 + i * 0x10;
        if (U8(s + 8) && Snd_IsPlaying(U32(s)) && !(U8(s + 0xA) & 0x80)) Snd_SetFade(i, 4, 0, 0x80);
    }
}

/* everything fades out */
void DC2CALL Snd_FadeAll(void)                                      /* 0x48CB40 */
{
    for (u32 i = 0; i < 4; i++) if (U8(0x87FB88 + i * 0x14)) Strm_SetFade(i, 4, 0, 0x80);
    for (u32 i = 0; i < 0x20; i++) if (U8(0x87F988 + i * 0x10)) Snd_SetFade(i, 4, 0, 0x80);
}

/* sound bank n (0x87FBE0, 8 each: the bank's memory, its 32 sounds) */
void DC2CALL Snd_FreeBank(u32 n)                                    /* 0x48CEB0 */
{
    gptr b = 0x87FBE0 + n * 8;
    if (!U32(b)) return;
    for (u32 k = 0; k < 0x100; k += 8) {
        gptr e = U32(b + 4) + k;
        if (U32(e)) Snd_Free(U32(e + 4));
    }
    Crt_free(U32(b));
    U32(b) = 0; U32(b + 4) = 0;
}

void DC2CALL Snd_FreeStream(u32 n)                                  /* 0x48CF10 */
{
    gptr s = 0x87FB80 + n * 0x14;
    if (!U32(s + 4)) return;
    Strm_Close(U32(s));
    Crt_free(U32(s + 4));
    U32(s) = 0; U32(s + 4) = 0; U32(s + 8) = 0; U32(s + 0xC) = 0; U32(s + 0x10) = 0;
}

/* ---- hidden behind the camera zone's mask (+0x50 bit 1) ----------------------- */

/* an object, and its parts the same */
void DC2CALL Obj_UpdateHidden(gptr o)                               /* 0x48F910 */
{
    u32 f = U32(o + 0x50), h;
    if ((f & 5) == 5) {
        h = Pos_Hidden(o + 0x40);
        s16 z = S16(o + 0x44);
        if (z > 0x7D00 || z < -0x7D00) { h = 0; U32(o + 0x50) &= ~2u; }
        else if (h) U32(o + 0x50) |= 2;
        else U32(o + 0x50) &= ~2u;
    } else h = (f >> 1) & 1;
    for (gptr p = U32(o + 0x64); p; p = U32(p + 0x54)) {
        if (h) U32(p + 0x50) |= 2;
        else U32(p + 0x50) &= ~2u;
    }
}

/* an effect not following anything */
void DC2CALL Eff_UpdateHidden(gptr e)                               /* 0x48F980 */
{
    if (U32(e + 0x54)) return;
    U32(e + 0x50) &= ~2u;
    if (Pos_Hidden(e + 0x40)) U32(e + 0x50) |= 2;
}

void DC2CALL Hidden_UpdateAll(void)                                 /* 0x48FA70 */
{
    gptr o = U32(G() + 0x940);
    for (u32 n = U32(0x87F940); n; n--, o += 0x260) Obj_UpdateHidden(o);
    gptr e = U32(G() + 0x93C);
    for (u32 n = U32(0x87DE04); n; n--, e += 0x70)
        if ((U32(e + 0x50) & 5) == 5) Eff_UpdateHidden(e);
}

/* the far colour (both copies of it) */
void DC2CALL Light_SetFar(u32 c)                                    /* 0x48FB50 */
{
    U8(0x87DD7D) = (u8)c; U8(0x87DCED) = (u8)c;
    U8(0x87DD7E) = (u8)(c >> 8); U8(0x87DCEE) = (u8)(c >> 8);
    U8(0x87DD7F) = (u8)(c >> 16); U8(0x87DCEF) = (u8)(c >> 16);
}

/* the sound area of an area and room (area 4 is three) */
u32 DC2CALL Area_SoundArea(u32 area, u32 room)                      /* 0x491B10 */
{
    u8 a = (u8)area, r = (u8)room;
    if (a < 4) return a;
    if (a > 4) return (u8)(a + 1);
    if (r <= 6) return 4;
    if (r <= 10) return 5;
    return 4;
}

/* where a text of (kind << 12 | index) codes starts so it is centred: the widest
   line (0x723D4C widths; kind 4 a new line, 0xF the end) */
u32 DC2CALL Text_Left(gptr t)                                       /* 0x494DE0 */
{
    float m = F32(0x7033B4);
    xreal x = m, y = F32(0x7033B4);
    for (;; t += 2) {
        u16 w = U16(t);
        u32 c = w >> 12;
        if (c == 4) {
            if (x < y) m = (float)y;
            x = m; y = F32(0x7033B4);
            continue;
        }
        if (c == 0xF) break;
        y += F32(0x723D4C + (w & 0xFFF) * 4);
    }
    if (x < y) m = (float)y;
    return (u32)(s32)((xreal)F32(0x704620) - (xreal)m * F32(0x704624));
}

/* ---- message slots (0x74D980, 32 of 0x10) and the message box (0x87FF80) ------ */

void DC2CALL Msg_Clear(void)                                        /* 0x471A30 */
{
    for (gptr s = 0x74D980; s < 0x74DB80; s += 0x10) U8(s) = 0;
    U8(0x87FF80) = 0;
}

void DC2CALL Msg_ClearBox(void)                                     /* 0x471A50 */
{
    memset(MEM(0x74D750), 0, 0x230);
}

void DC2CALL Msg_SetScale(u32 x, u32 y)                             /* 0x471A70 */
{
    U8(0x87FF90) = (u8)(x >> 1); U8(0x87FF91) = (u8)(y >> 1);
}

void DC2CALL Msg_Reset(void)                                        /* 0x470CB0 */
{
    Msg_Clear();
    memset(MEM(0x87FF80), 0, 0x1C);
    Msg_SetScale(0x19, 0xB0);
    Msg_ClearBox();
    Flag_Set(2, 0x1F, 0);
}

void DC2CALL Msg_Tick(void)                                         /* 0x470CF0 */
{
    for (gptr s = 0x74D980; s < 0x74DB80; s += 0x10)
        if (U8(s) & 1) Orig_Call1(U32(0x723F70 + U8(s + 1) * 4), s);
    if (U8(0x87FF80) & 1) Orig_Call1(U32(0x723F8C + U8(0x87FF82) * 4), 0);
}

/* kinds 0, 1, 4: a free slot at (x, y); 2: the box; 3: the box, pausing the
   timer (G+0xB14) and the shake */
gptr DC2CALL Message_Show(u32 x, u32 y, gptr text, u32 kind)        /* 0x4718F0 */
{
    switch (kind & 0xFF) {
    case 0: case 1: case 4:
        for (gptr s = 0x74D980; s < 0x74DB80; s += 0x10) {
            if (U8(s) & 1) continue;
            U8(s + 1) = (u8)kind; U16(s + 0xC) = (u16)x; U8(s) = 1; U8(s + 2) = 0;
            U32(s + 8) = 0x808080; U16(s + 0xE) = (u16)y; U32(s + 4) = text;
            return s;
        }
        return 0;
    case 2: case 3:
        if (U8(0x87FF80) & 1) Msg_ClearBox();
        U8(0x87FF80) = 1; U32(0x87FF84) = text;
        U8(0x87FF83) = 0; U8(0x87FF8E) = 0;
        U16(0x87FF8A) = 0x20; U16(0x87FF88) = 0x18; U8(0x87FF8F) = 0xFF;
        Flag_Set(2, 0x1F, 1);
        if ((kind & 0xFF) == 2) { U8(0x87FF82) = 0; return 0x87FF80; }
        U32(0x87FF94) = U32(G() + 0xB14);
        U32(G() + 0xB14) = 0xFFFFFFFF;
        if (Flag_Check(2, 0xC)) {
            Shake_Off();
            Flag_Set(2, 0xC, 0);
            U8(0x87FF98) = 1;
        } else U8(0x87FF98) = 0;
        U8(0x87FF82) = 7;
        return 0x87FF80;
    }
    return 0;
}

/* a slot shown for two frames (0x723F70) */
void DC2CALL MsgSlot_Step(gptr s)                                   /* 0x470D30 */
{
    u8 v = U8(s + 2);
    U8(s + 2) = (u8)(v + 1);
    if (v == 1) U8(s) = 0;
}

/* the box's steps (0x723F8C by +2): +4 the text, +8 the speed (1/32 characters a
   frame) and +0xA what is left of it, +0xC a timer, +0xE characters so far
   (0x74D750), +0xF the choice */
#define BOX 0x87FF80u

/* step 0: the text comes out; codes (kind << 12): 0 a character, 1 colour,
   2 speed (0: all at once), 3 a question, 4 skip, 5 a new page, 6 wait for a
   button, 7 wait a while, 8 a choice mark, 0xF the end */
void DC2CALL Box_Type(void)                                         /* 0x470D50 */
{
    u16 n = (u16)(U16(BOX + 0xA) + U16(BOX + 8));
    U16(BOX + 0xA) = n;
    if ((U8(0x87DC00) & 0x60) && U8(BOX + 0xE)) { n = 0x1180; U16(BOX + 0xA) = n; }
    while (n & 0xFFE0) {
        u16 w = U16(U32(BOX + 4));
        switch (w >> 12) {
        case 0:
            U32(0x74D750 + U8(BOX + 0xE) * 4) = (u32)(U8(BOX + 3) | 0x100) << 16 | w;
            U8(BOX + 0xE)++;
            if (w) { n -= 0x20; U16(BOX + 0xA) = n; }
            break;
        case 1: U8(BOX + 3) = (u8)w; break;
        case 2:
            if (w & 0xFFF) U16(BOX + 8) = w & 0xFFF;
            else { n = 0x1180; U16(BOX + 0xA) = n; }
            break;
        case 3:
            U8(BOX + 0xF) = 0;
            if ((w & 0xFF) == 0) U8(BOX + 2) += 4;
            else if ((w & 0xFF) == 1) U8(BOX + 2) += 5;
            n = 0; U16(BOX + 0xA) = 0;
            break;
        case 4: U8(BOX + 0xE) = 0x46; break;
        case 5:
            Msg_ClearBox();
            U8(BOX + 0xE) = 0; U16(BOX + 0xC) = 1; U8(BOX + 2)++;
            n = 0; U16(BOX + 0xA) = 0;
            break;
        case 6:
            U16(BOX + 0xC) = 0; U8(BOX + 2) += 2;
            n = 0; U16(BOX + 0xA) = 0;
            break;
        case 7:
            U16(BOX + 0xC) = w & 0xFFF; U8(BOX + 2)++;
            n = 0; U16(BOX + 0xA) = 0;
            break;
        case 8:
            U32(0x74D750 + U8(BOX + 0xE) * 4) = (w & 0xFF) | 0x2000000;
            U8(BOX + 0xE)++;
            break;
        case 0xF:
            U8(BOX + 2) += 6;
            n = 0; U16(BOX + 0xA) = 0;
            break;
        }
        U32(BOX + 4) += 2;
    }
}

/* step 1: a while, then back */
void DC2CALL Box_Wait(void)                                         /* 0x470F40 */
{
    if (U16(BOX + 0xC) && !--U16(BOX + 0xC)) { U8(BOX + 0xF) = 0; U8(BOX + 2)--; }
}

/* the blinking 14x14 sprite at (x, y), u from the choice */
static void box_sprite(float x, float y, u8 u)
{
    gptr p = U32(0x87DC98);
    U8(p + 4) = 0xFF; U8(p + 5) = 0xFF; U8(p + 6) = 0xFF; U8(p + 7) = 0x64;
    F32(p + 8) = x; F32(p + 0xC) = y; U32(p + 0x10) = 0x3C23D70A;
    U8(p + 0x14) = u; U8(p + 0x15) = 0xF0; U16(p + 0x16) = 0x7DCD; U16(p + 0x18) = 0xE; U16(p + 0x1A) = 0xE;
    gptr ot = U32(0x87F94C) + 0x74;
    U32(p) = U32(ot) | 0x4000000; U32(ot) = p & 0xFFFFFF;
    U32(0x87DC98) += 0x1C;
    Gpu_SetDrawMode(U32(0x87DC98), 1, 1, 0x2B, 0);
    Gpu_AddPrim(U32(0x87F94C) + 0x74, U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}

/* step 2: the page's end, the button goes on (or ends, with +0 bit 1) */
void DC2CALL Box_Prompt(void)                                       /* 0x470F70 */
{
    gptr t = U32(BOX + 4);
    if (U8(0x87DC00) & 0x60) {
        if ((U16(t) & 0xF000) == 0xF000 && (U8(BOX) & 2)) U8(BOX + 2)++;
        else { U8(BOX + 0xF) = 0; U8(BOX + 2) -= 2; }
    }
    if ((U16(t) & 0xF000) == 0xF000) return;
    u16 c = (u16)(U16(BOX + 0xC) + 1);
    U16(BOX + 0xC) = c;
    if (c > 0x14) { U16(BOX + 0xC) = 0; return; }
    if (c >= 0xA) return;
    box_sprite(152.0f, 217.0f, 0);
}

/* step 3: once the button is let go */
void DC2CALL Box_Release(void)                                      /* 0x4710D0 */
{
    if (U8(0x87DDFC) & 0x20) return;
    U8(BOX + 0xF) = 0; U8(BOX + 2) -= 3;
}

/* step 4: yes / no, left and right */
void DC2CALL Box_YesNo(void)                                        /* 0x4710F0 */
{
    u32 b = U32(0x87DC00);
    u8 cur = U8(BOX + 0xF);
    int moved = 0;
    if ((b & 0x8000) && cur) { U8(BOX + 0xF) = 0; moved = 1; }
    else if ((b & 0x2000) && !cur) { U8(BOX + 0xF) = 1; moved = 1; }
    if (moved) { U16(BOX + 0xC) = 0; Se_Play(0, 0); }
    Message_Show(U8(BOX + 0x10) * 2 + 0xB2, U8(BOX + 0x11) * 2 + 0x13, 0x728658, 0);
    Message_Show(U8(BOX + 0x10) * 2 + 0xE4, U8(BOX + 0x11) * 2 + 0x13, 0x728660, 0);
    u16 c = (u16)(U16(BOX + 0xC) + 1);
    U16(BOX + 0xC) = c;
    if (c > 0x14) U16(BOX + 0xC) = 0;
    else if (c < 0xA) box_sprite((float)S16(0x723FC4 + U8(BOX + 0xF) * 2), 196.0f, 0x10);
    u8 b0 = U8(0x87DC00);
    if (b0 & 0x20) U8(BOX + 2) -= 4;
    else if (b0 & 0x40) { U8(BOX + 0xF) = 1; U8(BOX + 2) -= 4; }
}

/* step 5: one of the choice marks, left and right */
void DC2CALL Box_Choose(void)                                       /* 0x4712B0 */
{
    u32 b = U32(0x87DC00);
    if (b & 0x8000 || b & 0x2000) {
        u32 want = (u32)U8(BOX + 0xF) + (b & 0x8000 ? (u32)-1 : 1u);
        for (u32 i = 0; i < 0x8C; i++) {
            u32 v = U32(0x74D750 + i * 4);
            if ((v & 0xFF000000) == 0x2000000 && (v & 0xFF) == want) {
                Se_Play(0, 0);
                U8(BOX + 0xF) = (u8)(U8(BOX + 0xF) + (b & 0x8000 ? -1 : 1));
                U16(BOX + 0xC) = 0;
                break;
            }
        }
    }
    u16 c = (u16)(U16(BOX + 0xC) + 1);
    U16(BOX + 0xC) = c;
    if (c > 0x14) U16(BOX + 0xC) = 0;
    if (U8(0x87DC00) & 0x20) U8(BOX + 2) -= 5;
}

/* step 6: closed; the choice kept at 0x87FC45 */
void DC2CALL Box_Close(void)                                        /* 0x4713C0 */
{
    U8(0x87FC45) = U8(BOX + 0xF);
    Msg_ClearBox();
    Flag_Set(2, 0x1F, 0);
    U8(BOX) = 0;
}

/* the box's last mode in its second table (0x723F70 [20]): closed, its
   result (BOX+0x14) kept in G+0xB14, flag 2:0xC set when BOX+0x18 bit 0 asks */
void DC2CALL Box_CloseResult(void)                                  /* 0x4713F0 */
{
    Msg_ClearBox();
    gptr g = G();
    U8(BOX) = 0;
    U32(g + 0xB14) = U32(BOX + 0x14);
    Flag_Set(2, 0x1F, 0);
    if (U8(BOX + 0x18) & 1) Flag_Set(2, 0xC, 1);
}

/* the message layer's own draw mode and drawing of its ordering table */
void DC2CALL Msg_Flush(void)                                        /* 0x471AC0 */
{
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0x28, 0);
    Gpu_AddPrim(0x87DDE8 + U32(0x87DE0C) * 0xC, U32(0x87DC98));
    U32(0x87DC98) += 0xC;
    u32 o = U32(0x87DE0C) * 0xC;
    Gpu_DrawOT(U32(0x87F94C) + 0x70, 0x87DDE8 + o, 0x87DDE0 + o);
}

/* ---- movies (0x7375A8, 12 each: name, size, the timer after) ---------------- */

void DC2CALL Movie_Start(u32 n)                                     /* 0x473130 */
{
    gptr e = 0x7375A8 + n * 0xC, name = Tmp_Push(0x28);
    Crt_sprintf1(name, 0x7376A8, U32(e));
    u32 r = Movie_Open(name, U16(e + 4), U16(e + 6));
    Tmp_Pop(0x28);
    if (r) { U32(0x74DB84) = 0xFFFFFFFF; return; }
    U32(0x74DB80) = U32(0x87DE00);
    U32(0x74DB84) = n;
    U32(0x87DE00) = U16(e + 8);
    U32(0x74DB88) = 0;
    U32(0x87DC9C)++;
    Flag_Set(2, 0x17, 1);
    Flag_Set(2, 0x1E, 0);
    Flag_Set(3, 0, 0);
}

void DC2CALL Movie_Tick(void)                                       /* 0x4731F0 */
{
    if (U32(0x74DB84) == 0xFFFFFFFF) return;
    Orig_Call1(U32(0x73762C + U32(0x74DB88) * 4), 0);
}

/* the movie's steps (Movie_Tick by 0x74DB88): start (the screen cleared,
   faded in at once; 0x87DCEC/0x87DD7C off for those without +6), play until it ends
   or (+0xA) a button skips it, then the screen cleared and faded out (not
   movie 0, the title's, nor 10) and the root mode back */
void DC2CALL Movie_StepStart(void)                                  /* 0x473210 */
{
    gptr e = 0x7375A8 + U32(0x74DB84) * 0xC;
    U32(0x74DB88)++;
    if (!U16(e + 6)) { U8(0x87DCEC) = 0; U8(0x87DD7C) = 0; }
    Movie_Clear(1);
    Movie_Ctl1();
    Fade_Set(0, 1, 1);
}

void DC2CALL Movie_StepPlay(void)                                   /* 0x473260 */
{
    if (!Movie_Poll()) {
        if (!U16(0x7375A8 + U32(0x74DB84) * 0xC + 0xA) || !U16(0x87DC00)) return;
        Movie_Ctl0();
        Flag_Set(2, 0x1E, 1);
    }
    U32(0x74DB88)++;
}

void DC2CALL Movie_StepEnd(void)                                    /* 0x4732A0 */
{
    int fade = 1;
    if (Flag_Check(2, 0x1E)) { U8(0x87DCEC) = 1; U8(0x87DD7C) = 1; }
    else {
        Movie_Close();
        u32 n = U32(0x74DB84);
        if (n == 0xA) fade = 0;
        else { U8(0x87DCEC) = 1; U8(0x87DD7C) = 1; fade = n != 0; }
    }
    if (fade) { Movie_Clear(1); Fade_Set(1, 1, 1); }
    Flag_Set(2, 0x17, 0);
    U32(0x87DE00) = U32(0x74DB80);
}

/* ---- sound effects: 0x6EED00 the table of sounds (8 bytes), banks 0x87FBE0 ------ */

/* a sound's record unpacked into 13 bytes: bank, group, first, ..., count */
void DC2CALL Se_Info(u32 id, gptr r)                                /* 0x48C220 */
{
    gptr t = 0x6EED00 + id * 8;
    U8(r) = U8(t + 1);
    U8(r + 1) = U8(t + 2) & 0x7F;
    U8(r + 2) = U8(t + 3) >> 4;
    U8(r + 4) = U8(t + 3) & 7;
    U8(r + 3) = (U8(t + 3) >> 3) & 1;
    U8(r + 5) = (U8(t) >> 1) & 1;
    U8(r + 6) = (U8(t) >> 2) & 3;
    U8(r + 7) = U8(t) & 1;
    U8(r + 8) = U8(t + 4) & 0x1F;
    U8(r + 9) = (U8(t + 4) >> 5) & 1;
    U8(r + 0xA) = U8(t + 5); U8(r + 0xB) = U8(t + 6); U8(r + 0xC) = U8(t + 7);
}

/* pan and volume of a sound at o heard from the camera (G+0x7B0) within range */
void DC2CALL Se_Calc(gptr o, u32 vol, gptr pan, gptr pvol, u32 range) /* 0x48C2D0 */
{
    if (!o) { U32(pan) = 0x40; U32(pvol) = vol; return; }
    u32 d = Camera_Distance(G() + 0x7B0, o + 0x40);
    if (d > range) { U32(pan) = 0x40; U32(pvol) = 0; return; }
    U32(pvol) = (range - d) * vol / range;
    u32 a = ((u32)(s32)(s16)Angle_XZ(G() + 0x7B0, o + 0x40) - U16(G() + 0x79A)) & 0xFFF;
    if (a >= 0x400 && a < 0xC00) U32(pan) = 0x7F - (((a - 0x400) >> 4) & 0xFF);
    else U32(pan) = ((a + 0x400) >> 4) & 0xFF;
}

/* a sound by where o stands: the room's sound areas (G+0xA64) change it */
u32 DC2CALL Se_Remap(gptr o, u32 id)                                /* 0x48C3B0 */
{
    if (!o) return id;
    if (id == 0x50 || id == 0x51) {
        if (Flag_Check(2, 0xF)) return id;
        gptr t = U32(G() + 0xA64);
        u32 n = U32(t);
        for (u32 i = 0; i < n; i++) {
            gptr r = t + 4 + i * 0x14;
            if ((s16)S8(r + 2) != S16(o + 0x5A) || !PointInQuad(o + 0x40, r + 4)) continue;
            if (U8(r + 1)) Latch_74B16C((u8)(id - 0x4F));
            if (U8(r + 3) != 3) id = id + (u32)(s32)S8(r + 3) * 2 + 2;
            return id;
        }
        return id;
    }
    if (id == 0x30 || id == 0x31) {
        gptr t = U32(G() + 0xA64);
        u32 n = U32(t);
        for (u32 i = 0; i < n; i++) {
            gptr r = t + 4 + i * 0x14;
            if (!U8(r + 1) || (s16)S8(r + 2) != S16(o + 0x5A) || !PointInQuad(o + 0x40, r + 4)) continue;
            Latch_74B16C((u8)(id - 0x4F));
            return id + 2;
        }
    }
    return id;
}

/* into a free sound slot (or the one with this buffer) and played */
void DC2CALL Se_Start(u32 h, gptr o, gptr e, u32 pan, u32 vol, u32 id) /* 0x48C4F0 */
{
    u32 loop = U32(e + 4);
    (void)o;
    if (id >= 0x10 && id < 0x30 && id != 0x11 && id != 0x21 && id != 0x1A) Flag_Set(2, 0x19, 1);
    gptr s = 0x87F980;
    u32 i = 0;
    for (; i < 0x20; i++, s += 0x10) if (!U8(s + 8) || U32(s) == h) break;
    if (i == 0x20) return;
    U8(s + 0xA) = 0; U16(s + 0xC) = 0x7F; U8(s + 9) = (u8)id; U32(s) = h; U8(s + 8) = 1; U32(s + 4) = 0;
    s32 v = (s32)S8(G() + 0x14EE) * (s32)vol / 15;
    if (U8(G() + 0x14EC)) Snd_SetPan(h, pan);
    else if (g_gog_fixes) Snd_SetPan(h, 0x40);      /* (GOG: mono centred, not the buffer's last pan) */
    Snd_SetVolume(h, v);
    Snd_Play(h, loop, U8(e + 1));
}

/* the bank's entry for record r (bank, group, index) */
static gptr se_entry(gptr r)
{
    return U32(0x87FBE0 + (s32)S8(r) * 8) + ((s32)S8(r + 1) * 16 + S8(r + 2)) * 8;
}

int g_gog_fixes;

/* sound id at object o (0: no place): every buffer of the sound. Sound 0x38
   (the tyrannosaur's roar) is never played by the original; the GOG release
   plays it but for the objects at 0x5F2C6C and 0x5ED1BC and without a place
   ("Restored missing T-Rex roar sound") */
void DC2CALL Se_Play(gptr o, u32 id)                                /* 0x48C0B0 */
{
    if (id == 0x38 && (!g_gog_fixes || o == 0x5F2C6C || o == 0x5ED1BC || (s32)o < 0x200000)) return;
    if (U8(G() + 0x927) && o) {
        u8 k = U8(o + 0x58);
        if (U8(0x87E747) == 2) { if (k & 1) id += 0x20; }
        else if (k >= 0x14 && k <= 0x1A) id += 0x20;
    }
    u32 snd = Se_Remap(o, id);
    gptr r = Tmp_Push(0x18);
    Se_Info(snd, r);
    if (U8(r) == 0xFF) { Tmp_Pop(0x18); return; }
    gptr e = se_entry(r);
    u32 pan = (u32)o, vol = id;                     /* (the argument slots they reuse) */
    while (U8(e)) {
        if (U8(r + 5)) { pan = U8(e + 3); vol = U8(e + 2); }
        else {
            u32 range = 1000000;
            if (o) { u8 k = U8(o + 0x58); if (k == 3 || k == 8 || k == 6) range = 5000000; }
            gptr pv = Tmp_Push(8);
            U32(pv) = pan; U32(pv + 4) = vol;
            Se_Calc(o, U8(e + 2), pv, pv + 4, range);
            pan = U32(pv); vol = U32(pv + 4);
            Tmp_Pop(8);
        }
        Se_Start(U32(U32(0x87FBE4 + (s32)S8(r) * 8) + U8(e) * 8 + 4), o, e, pan, vol, snd);
        U8(r + 2)++;
        u8 c = U8(r + 6);
        U8(r + 6) = (u8)(c - 1);
        if (!c) break;
        e = se_entry(r);
    }
    Tmp_Pop(0x18);
}

/* sound id without a place, at pan and volume */
void DC2CALL Se_PlayFlat(u32 id, u32 vol, u32 pan)                  /* 0x48C640 */
{
    gptr r = Tmp_Push(0x18);
    Se_Info(id, r);
    if (U8(r) != 0xFF) {
        gptr e = se_entry(r);
        while (U8(e)) {
            Se_Start(U32(U32(0x87FBE4 + (s32)S8(r) * 8) + U8(e) * 8 + 4), 0, e, pan, vol, id);
            U8(r + 2)++;
            u8 c = U8(r + 6);
            U8(r + 6) = (u8)(c - 1);
            if (!c) break;
            e = se_entry(r);
        }
    }
    Tmp_Pop(0x18);
}

/* the sounds of group g fade out */
void DC2CALL Se_StopGroup(u32 g)                                    /* 0x48C600 */
{
    for (u32 i = 0; i < 0x20; i++) if ((s32)S8(0x87F989 + i * 0x10) == (s32)g) Snd_SetFade(i, 4, 0, 0x80);
}

/* stream slot n stops */
void DC2CALL Strm_Halt(u32 n)                                       /* 0x48CE60 */
{
    gptr s = 0x87FB80 + (s32)(s16)n * 0x14;
    if (!U32(s + 4) || U8(s + 8) != 1) return;
    u32 h = U32(s);
    U8(s + 8) = 0; U8(s + 0x11) = 0; U8(s + 0x10) = 0; U16(s + 0xA) = 0; U16(s + 0xC) = 0;
    Strm_Stop(h);
    Strm_SetVolume(U32(s), 0);
}

/* ---- text: 14x14 characters from the font page (0x723D4C their widths) -------- */

/* n codes of s at (x, y), depth z; kind 4 a new line (15 down, 14 in mode 2,
   where the characters also sit closer); 1 when it reached the end (0xF) */
u32 DC2CALL Text_Draw(gptr s, u32 x, u32 y, u32 n, u32 col, u32 z, u32 mode) /* 0x494BA0 */
{
    xreal X = (s32)(x & 0xFFFF);
    u32 c24 = (col & 0xFFFFFF) | 0x6C000000;
    for (;;) {
        u16 cnt = (u16)n;
        n--;
        if (!cnt) return 0;
        u16 w = U16(s);
        s += 2;
        u32 c = w >> 12, idx = w & 0xFFF;
        if (c == 0xF) return 1;
        gptr p = U32(0x87DC98);
        U32(p + 4) = c24;
        F32(p + 8) = (float)X;
        s32 yi = (s32)(y & 0xFFFF);
        float x2 = (float)(X + F32(0x703454));
        F32(p + 0xC) = (float)yi;
        F32(p + 0x14) = x2;
        F32(p + 0x18) = (float)yi;
        F32(p + 0x20) = (float)X;
        F32(p + 0x24) = (float)(yi + 14);
        F32(p + 0x2C) = x2;
        F32(p + 0x30) = (float)(yi + 14);
        U8(p + 0x10) = 0; U8(p + 0x11) = 0; U8(p + 0x1C) = 14; U8(p + 0x1D) = 0;
        U8(p + 0x28) = 0; U8(p + 0x29) = 14; U8(p + 0x34) = 14; U8(p + 0x35) = 14;
        U16(p + 0x1E) = (u16)idx; U16(p + 0x12) = 0x7DCC;
        gptr ot = 0x87BC00 + 4 * ((U32(0x87DE0C) << 10) + (z & 0xFFFF));
        U32(p) = U32(ot) | 0x2000000; U32(ot) = p & 0xFFFFFF;
        U32(0x87DC98) += 0x38;
        if ((u16)mode == 2) X += (xreal)F32(0x723D4C + idx * 4) - (xreal)F32(0x703498);
        else X += F32(0x723D4C + idx * 4);
        if ((u16)c == 4) {
            X = (s32)(x & 0xFFFF);
            y += (u16)mode == 2 ? 14 : 15;
        }
    }
}

/* ---- every frame: lights and pads ---------------------------------------------- */

/* the lights marked this frame run (0x737A50 by +0xC bit 1): the objects' two and
   G+0x94C.. ; 0x74E350/51 their counts */
void DC2CALL Lights_Run(void)                                       /* 0x478CF0 */
{
    U8(0x74E351) = 0; U8(0x74E350) = 0;
    gptr objs = U32(G() + 0x940);
    if (objs)
        for (gptr o = objs; o < U32(G() + 0x940) + U32(0x87F940) * 0x260; o += 0x260) {
            if (!(U8(o + 0x50) & 1)) continue;
            for (gptr l = o + 0x190; l < o + 0x1C0; l += 0x18)
                if (U8(l + 0xC) & 1) Orig_Call1(U32(0x737A50 + ((U8(l + 0xC) >> 1) & 1) * 4), l);
        }
    for (gptr l = G() + 0x94C; l < G() + 0xA3C; l += 0x18)
        if (U8(l + 0xC) & 1) Orig_Call1(U32(0x737A50 + ((U8(l + 0xC) >> 1) & 1) * 4), l);
    if (Flag_Check(2, 4)) { U8(0x74E351) = 0; U8(0x74E350) = 0; }
}

/* ---- the marked lights' two kinds (0x737A50, by a light's +0xC bit 1) ----------
 * A light record (0x18 bytes): +0 a blink program (+3 the first state, +1 its
 * repeats) and +4 its run lengths (one byte a run), +8 a fade program (pairs
 * {frames, level}); +0xC flags (1 running, 2 the kind, 4 which count it sets,
 * 0x40 the blink running, 0x80 the fade running), +0xD the blink's state,
 * +0xE frames left in the run, +0xF frames left in the fade step, +0x10 the
 * level's step (8.8), +0x12 the level (8.8), +0x14 / +0x15 the blink's and the
 * fade's stage, +0x16 the blink's repeats left, +0x17 the fade's steps left.
 * Their result: 0x74E350 the blink's state, 0x74E351 the level. */

/* the blink: runs of frames that flip the state, the program's runs repeated */
static void Light_Blink(gptr l)                                     /* 0x4788B0 */
{
    gptr prog = U32(l), run = U32(l + 4);
    switch (U8(l + 0x14)) {
    case 0:
        U8(l + 0x14)++;
        U8(l + 0xD) = U8(prog + 3);
        U8(l + 0x16) = U8(prog + 1);
        U8(l + 0xE) = U8(run);
        /* fall through */
    case 1:
        if (!U8(l + 0xE)) {
            if (!--U8(l + 0x16)) { U8(l + 0x14)++; return; }
            U8(l + 0xE) = U8(run + 1);
            U8(l + 0xD) ^= 1;
            U32(l + 4) = run + 1;
        }
        U8(l + 0xE)--;
        U8(0x74E350) = U8(l + 0xD);
        return;
    case 2:
        U8(l + 0xC) &= 0xBF;
        return;
    }
}

/* the fade: the level moved by steps toward each next {frames, level} pair */
static void Light_Fade(gptr l)                                      /* 0x478940 */
{
    gptr p = U32(l + 8);
    switch (U8(l + 0x15)) {
    case 0: {
        U8(l + 0x15)++;
        U8(l + 0x17) = U8(U32(l) + 2);
        u8 n = U8(p);
        U8(l + 0xF) = n;
        if (n) U16(l + 0x10) = (u16)((s32)(U8(p + 1) << 8) / (s32)n);
        else { U8(l + 0x12) = 0; U8(l + 0x13) = U8(p + 1); }
        U16(l + 0x12) = 0;                      /* (the original clears the level either way) */
    }
        /* fall through */
    case 1:
        if (!U8(l + 0xF)) {
            if (!--U8(l + 0x17)) { U8(l + 0x15)++; return; }
            p += 2;
            U32(l + 8) = p;
            u8 n = U8(p);
            U8(l + 0xF) = n;
            if (n) U16(l + 0x10) = (u16)((((s32)U8(p + 1) - (s32)U8(p - 1)) << 8) / (s32)n);
            else { U16(l + 0x10) = 0; U8(l + 0xF) = 1; }
        }
        U8(l + 0xF)--;
        U8(0x74E351) = U8(l + 0x13);
        if (U8(l + 0xF)) U16(l + 0x12) += U16(l + 0x10);
        else { U8(l + 0x12) = 0; U8(l + 0x13) = U8(p + 1); }
        return;
    case 2:
        U8(l + 0xC) &= 0x7F;
        return;
    }
}

/* kind 0: the blink and the fade, the light off when neither runs */
void DC2CALL Light_Run0(gptr l)                                      /* 0x478870 */
{
    if (U8(l + 0xC) & 0x40) Light_Blink(l);
    if (U8(l + 0xC) & 0x80) Light_Fade(l);
    if (!(U8(l + 0xC) & 0xC0)) U8(l + 0xC) &= 0xFE;
}

/* kind 1: a level for +0xE frames more, into the count +0xC bit 2 picks */
void DC2CALL Light_Run1(gptr l)                                      /* 0x478A30 */
{
    U8(0x74E350 + ((U8(l + 0xC) >> 2) & 1)) = U8(l + 0x13);
    if (!U8(l + 0xE)--) U8(l + 0xC) &= 0xFE;
}

/* held, newly pressed, the held before */
void DC2CALL Pad_Map(gptr src, gptr dst)                            /* 0x478DF0 */
{
    U16(dst + 4) = U16(dst);
    U16(dst) = U16(src);
    U16(dst + 2) = (u16)(~U16(dst + 4) & U16(src));
}

/* both pads read (0x87DC42 + 0x24 each), into the game's button words */
void DC2CALL Input_Frame(void)                                      /* 0x478BF0 */
{
    Lights_Run();
    for (u32 i = 0; i < 2; i++) {
        gptr s = i ? 0x87DC20 : 0x87DC10;
        Input_ReadPad(i);
        u16 old = U16(s), now = U16(0x87DC42 + i * 0x24);
        U16(s + 6) = 0;
        U16(s + 0xE) = 0; U16(s + 0xC) = 0; U16(s + 0xA) = 0; U16(s + 8) = 0;
        U16(s + 4) = old; U16(s) = now; U16(s + 2) = (u16)(~old & now);
    }
    Pad_Map(0x87DC10, 0x74E358);
    U16(0x87DDFC) = U16(0x74E358); U16(0x87DC00) = U16(0x74E35A);
    Pad_Map(0x87DC20, 0x74E368);
    U16(0x87DE0A) = U16(0x74E368); U16(0x87DC92) = U16(0x74E36A);
    if (Flag_Check(1, 0)) { U16(0x87DC92) = 0; U16(0x87DE0A) = 0; U16(0x87DC94) = 0; U16(0x87DC08) = 0; }
    else { U16(0x87DC08) = U16(0x87DDFC); U16(0x87DC94) = U16(0x87DC00); }
}

/* ---- the lights a model part uses: the camera's set (G+0xA3C, 0x298 each by
   G+0x7C4) or the nearest of its lists, and the room light G+0xC ------------------ */

/* light l (colour, place, radius, intensity +0xE) on part p in slot 0..2: its
   colour and direction scaled by distance / radius; 0 when out of reach */
u32 DC2CALL Light_Try(gptr p, gptr l, u32 slot)                     /* 0x46BC20 */
{
    s32 dx = (s16)(U16(l + 4) - U16(p + 0x40)), dy = (s16)(U16(l + 6) - U16(p + 0x42)), dz = (s16)(U16(l + 8) - U16(p + 0x44));
    u32 d2 = (u32)(dx * dx) + (u32)(dy * dy) + (u32)(dz * dz);
    s32 r = S16(l + 0xA);
    if ((u32)(r * r) < d2) return 0;
    U16(p + slot * 2 + 0xAC) = (u16)(U8(l) << 7);
    U16(p + slot * 2 + 0xB2) = (u16)(U8(l + 1) << 7);
    U16(p + slot * 2 + 0xB8) = (u16)(U8(l + 2) << 7);
    u32 s = (u32)SquareRoot0((s32)d2);
    gptr t = Tmp_Push(0x30), ang = t, v = t + 8, m = t + 0x10;
    Angles_LookAt(l + 4, p + 0x40, ang);
    RotMatrixYXZ(ang, m);
    U32(v) = 0; U16(v + 4) = U16(l + 0xE); U16(v + 6) = U16(ang + 6);
    ApplyMatrixSV(m, v, v);
    xreal k = (xreal)s / (xreal)r;
    s16 x = (s16)(s32)((xreal)S16(v) * k), y = (s16)(s32)((xreal)S16(v + 2) * k), z = (s16)(s32)((xreal)S16(v + 4) * k);
    Tmp_Pop(0x30);
    U16(p + slot * 8 + 0x78) = (u16)z;
    U16(p + slot * 8 + 0x74) = (u16)x; U16(p + slot * 8 + 0x76) = (u16)y;
    return 1;
}

void DC2CALL Part_Lights(gptr p, u32 keep)                          /* 0x46BAC0 */
{
    if (!keep) {
        memset(MEM(p + 0xAC), 0, 0x14);
        gptr s = U32(G() + 0xA3C) + U8(G() + 0x7C4) * 0x298 + 8, l = s + 0x18;
        if (U8(s + 0xB) & 1) {
            U16(p + 0xAC) = (u16)(U8(s + 8) << 7); U16(p + 0xB2) = (u16)(U8(s + 9) << 7); U16(p + 0xB8) = (u16)(U8(s + 0xA) << 7);
            U32(p + 0x74) = U32(s); U32(p + 0x78) = U32(s + 4);
        } else {
            u32 n = U8(s + 6);
            for (u32 i = 0; i < n; i++)
                if ((U8(l + i * 0x14 + 3) & 1) && Light_Try(p, l + i * 0x14, 0)) break;
            l += n * 0x14;
        }
        if (U8(s + 0xB) & 2) {
            gptr b = s + 0xC;
            U16(p + 0xAE) = (u16)(U8(b + 8) << 7); U16(p + 0xB4) = (u16)(U8(b + 9) << 7); U16(p + 0xBA) = (u16)(U8(b + 0xA) << 7);
            U32(p + 0x7C) = U32(b); U32(p + 0x80) = U32(b + 4);
        } else {
            for (u32 n = U8(s + 7); n; n--, l += 0x14)
                if ((U8(l + 3) & 1) && Light_Try(p, l, 1)) break;
        }
    }
    if (U8(G() + 0xF) & 1) Light_Try(p, G() + 0xC, 2);
}

/* every object to update (+0x50 bit 1): its parts, every other frame (+0xFD) */
void DC2CALL Obj_PickLights(gptr unused)                            /* 0x46BA00 */
{
    (void)unused;
    for (gptr o = U32(G() + 0x940); o < U32(G() + 0x940) + U32(0x87F940) * 0x260; o += 0x260) {
        if (!(U8(o + 0x50) & 2)) continue;
        u8 b = U8(o + 0xFD) ^ 1;
        U8(o + 0xFD) = b;
        u32 keep = b;
        if (U8(o + 0xFE) & 2) { U8(o + 0xFE) &= ~2; keep = 0; }
        gptr p = U32(o + 0x64);
        for (u32 n = (U8(o + 0xFE) & 1) ? U32(o + 0x68) : 1; n; n--) { Part_Lights(p, keep); p = U32(p + 0x54); }
    }
}

/* ---- the health gauge (G+0x68C: +0xB8 timer, +0xBC the health shown, +0xC8 its
   step); drawn in the top layer (*0x87F94C + 0x7C) ------------------------------- */

/* a colour between a and b that goes back and forth over 2 << sh frames */
u32 DC2CALL Color_Pulse(u32 a, u32 b, u32 sh)                       /* 0x48FAE0 */
{
    u32 m = 1u << (sh & 31), t = U32(G() + 4), f = (m - 1) & t;
    if (t & m) f = m - f;
    u32 w = f << ((12 - sh) & 31);
    gptr v = Tmp_Push(8);
    U32(v) = a; U32(v + 4) = b;
    LoadAverageCol(v, v + 4, (s32)w, (s32)(0x1000 - w), v);
    u32 r = U32(v);
    Tmp_Pop(8);
    return r;
}

/* the drawing area (x, y [+0x100 on the second buffer], w, h) */
void DC2CALL Hud_DrawArea(u32 x, u32 y, u32 w, u32 h)               /* 0x406810 */
{
    gptr r = Tmp_Push(8);
    U16(r) = (u16)x;
    U16(r + 2) = (u16)(U32(0x87DE0C) ? y + 0x100 : y);
    U16(r + 4) = (u16)w; U16(r + 6) = (u16)h;
    Gpu_SetDrawArea(U32(0x87DC98), r);
    Tmp_Pop(8);
    Gpu_AddPrim(U32(0x87F94C) + 0x7C, U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}

static void hud_link(u32 len)
{
    gptr p = U32(0x87DC98), ot = U32(0x87F94C) + 0x7C;
    U32(p) = U32(ot) | len << 24; U32(ot) = p & 0xFFFFFF;
}

/* a tile over rect (x, y, w, h) */
void DC2CALL Hud_Tile(gptr r, u32 col, u32 semi)                    /* 0x406C90 */
{
    gptr p = U32(0x87DC98);
    F32(p + 8) = (float)S16(r); F32(p + 0xC) = (float)S16(r + 2);
    U32(p + 0x10) = 0x3C23D70A;
    U16(p + 0x14) = U16(r + 4); U16(p + 0x16) = U16(r + 6);
    U32(p + 4) = col | ((u16)semi ? 0x62000000 : 0x60000000);
    hud_link(3);
    U32(0x87DC98) += 0x18;
}

/* a sprite at rect from (u, v) of clut */
void DC2CALL Hud_Sprite(gptr r, u32 u, u32 v, u32 clut, u32 col, u32 semi) /* 0x406E30 */
{
    gptr p = U32(0x87DC98);
    F32(p + 8) = (float)S16(r); F32(p + 0xC) = (float)S16(r + 2);
    U32(p + 0x10) = 0x3C23D70A;
    U16(p + 0x18) = U16(r + 4); U16(p + 0x1A) = U16(r + 6);
    U8(p + 0x14) = (u8)u; U8(p + 0x15) = (u8)v; U16(p + 0x16) = (u16)clut;
    U32(p + 4) = col | ((u16)semi ? 0x66000000 : 0x64000000);
    hud_link(4);
    U32(0x87DC98) += 0x1C;
}

/* a picture of 8-byte pieces (u, v, w, h, dx, dy, count in the first) at x, y;
   col bit 31 semi-transparent; then the texture page tp */
void DC2CALL Hud_Picture(gptr list, u32 x, u32 y, u32 tp, u32 clut, u32 col) /* 0x406D50 */
{
    u32 n = U8(list + 6), semi = col >> 31, c24 = col & 0xFFFFFF;
    gptr r = Tmp_Push(8);
    for (gptr e = list; n; n--, e += 8) {
        U16(r) = (u16)(x + (s32)S8(e + 4)); U16(r + 2) = (u16)(y + (s32)S8(e + 5));
        U16(r + 4) = U8(e + 2); U16(r + 6) = U8(e + 3);
        Hud_Sprite(r, U8(e), U8(e + 1), clut, c24, semi);
    }
    Tmp_Pop(8);
    gptr p = U32(0x87DC98);
    U32(p + 4) = tp | 0xE1000400; U32(p + 8) = 0;
    hud_link(2);
    U32(0x87DC98) += 0xC;
}

/* the gauge: frame (blinking when low), bar by health (green, yellow, red;
   pulsing when poisoned), the health lost going down behind it */
gptr DC2CALL Gauge_Bar(u32 x, u32 y)                                /* 0x4062C0 */
{
    gptr pl = U32(G() + 0x940), b = G() + 0x68C;
    s32 mx = S16(pl + 0x10A), hp = S16(pl + 0x108);
    if (!(hp > mx / 4) && (U8(G() + 4) & 0x10)) Hud_Picture(0x685020, x - 2, y - 2, 0xC, 0x7C8F, 0x80);
    u32 col = U16(pl + 0x17E) ? Color_Pulse(0x80, 0x30, 3) | 0x2000000 : 0x808080;
    Hud_Picture(0x6850A8, x + 0x40, y + 8, 0xC, 0x7C8F, col);
    Hud_Picture(0x683620, x, y, 0xD, 0x7C8F, col);
    hp = S16(pl + 0x108); mx = S16(pl + 0x10A);
    s16 len = (s16)(hp * 0x70 / mx);
    if (len < 0) len = 0;
    else if (len > 0x70) len = 0x70;
    u32 c;
    if (hp > mx / 4 && !U16(pl + 0x17E)) c = hp > mx / 2 ? Color_Pulse(0x8000, 0x4000, 5) : Color_Pulse(0x8080, 0x4040, 4);
    else c = Color_Pulse(0x80, 0x40, 3);
    gptr r = Tmp_Push(8);
    U16(r) = (u16)x; U16(r + 2) = (u16)y; U16(r + 4) = (u16)len; U16(r + 6) = 0x10;
    Hud_Sprite(r, 0x70, 0x50, 0x7C8F, c, 0);
    U16(b + 0xBC) -= 6;
    if (S16(pl + 0x108) >= S16(b + 0xBC)) U16(b + 0xBC) = U16(pl + 0x108);
    else {
        s16 l2 = (s16)(S16(b + 0xBC) * 0x70 / S16(pl + 0x10A));
        if (l2 < 0) l2 = 0;
        else if (l2 > 0x70) l2 = 0x70;
        U16(r) = (u16)x; U16(r + 2) = (u16)y; U16(r + 4) = (u16)l2; U16(r + 6) = 0x10;
        Hud_Sprite(r, 0x70, 0x50, 0x7C8F, c >> 1, 0);
    }
    Tmp_Pop(8);
    gptr p = U32(0x87DC98);
    U32(p + 4) = 0xE100040C; U32(p + 8) = 0;
    hud_link(2);
    U32(0x87DC98) += 0xC;
    return U32(0x87DC98);
}

/* the gauge's steps (+0xC8): 1 shown, 2 closing, 3/4 a line shrinking; shown
   again when the health is low, poisoned, in the extra mode or just hit */
void DC2CALL Gauge_Draw(u32 x, u32 y)                               /* 0x406050 */
{
    gptr pl = U32(G() + 0x940), b = G() + 0x68C;
    if (Flag_Check(2, 7) || Flag_Check(2, 0xF) || !Flag_Check(3, 7)) return;
    u8 st = U8(b + 0xC8);
    switch ((s32)(s8)st) {
    case 1:
        U16(b + 0xB8)--;
        if (S16(b + 0xB8) < 0) { U8(b + 0xC8) = (u8)(st + 1); U16(b + 0xB8) = 8; }
        Gauge_Bar(x, y);
        break;
    case 2: {
        U16(b + 0xB8)--;
        if (S16(b + 0xB8) < 0) { U8(b + 0xC8) = (u8)(st + 1); U16(b + 0xB8) = 0; }
        gptr p = U32(0x87DC98);
        U32(p + 4) = 0xE100040C; U32(p + 8) = 0;
        hud_link(2);
        U32(0x87DC98) += 0xC;
        Hud_DrawArea(0, 0, 0x140, 0xF0);
        Gauge_Bar(x, y);
        u16 t = U16(b + 0xB8);
        Hud_DrawArea(x, y - t + 8, 0x70, (u32)t * 2);
        break; }
    case 3:
        U8(b + 0xC8) = (u8)(st + 1);
        U16(b + 0xB8) = 0x38;
        /* fall through */
    case 4: {
        U16(b + 0xB8) -= 5;
        if (S16(b + 0xB8) < 0) { U8(b + 0xC8) = 0; U16(b + 0xB8) = 0; }
        u16 t = U16(b + 0xB8);
        gptr r = Tmp_Push(8);
        U16(r) = (u16)(x - t + 0x38); U16(r + 2) = (u16)(y + 8); U16(r + 4) = (u16)(t * 2); U16(r + 6) = 1;
        Hud_Tile(r, 0x80F080, 0);
        Tmp_Pop(8);
        break; }
    }
    if (S16(pl + 0x108) < S16(pl + 0x10A) / 4 || U16(pl + 0x17E) || U8(G() + 0x108E) == 2) {
        U8(b + 0xC8) = 1; U16(b + 0xB8) = 0x78;
    }
    if (U16(pl + 0x174) && U16(pl + 0x108) != U16(b + 0xBC)) {
        U8(b + 0xC8) = 1; U16(b + 0xB8) = 0x78; U8(b + 0xCC) = 1;
        U16(G() + 0xC20) = 0; U16(G() + 0xC22) = 0;
    }
}

/* ---- starting inventory / shop tables ---------------------------------- */

static void stock_fill(gptr dst, const u8 *ids, const u16 *ab, int n)
{
    for (int i = 0; i < n; i++, dst += 12) {
        U8(dst) = 1; U8(dst + 1) = ids[i]; U16(dst + 2) = 0; U32(dst + 4) = 0;
        U16(dst + 8) = ab[i * 2]; U16(dst + 10) = ab[i * 2 + 1];
    }
}

/* 0x496410: fills the two 12-byte-record tables at G+0x10BC (0x3C dwords)
   and G+0x11AC (0x12 dwords) by difficulty G+0x108E, sets flag 9:0x74. */
void DC2CALL Game_InitStock(void)                                    /* 0x496410 */
{
    static const u8  i0[] = {1, 2, 6, 0xA, 0x11, 0x12, 0x15};
    static const u16 a0[] = {200,200, 200,200, 1000,1000, 9999,9999, 9999,9999, 9999,9999, 9999,9999};
    static const u8  i1[] = {1, 2, 0xA, 0x11, 0x12, 0x15};
    static const u16 a1[] = {100,100, 200,200, 9999,9999, 9999,9999, 9999,9999, 9999,9999};
    static const u16 a2[] = {100,100, 100,100, 9999,9999, 9999,9999, 9999,9999, 9999,9999};
    static const u8  j0[] = {0x1B, 0x1C, 0x1D};
    static const u16 b0[] = {3,5, 3,5, 3,5};
    static const u8  j1[] = {0x1A, 0x1B, 0x1C, 0x1E};
    static const u16 b1[] = {1,2, 1,2, 1,2, 1,2};
    static const u16 b2[] = {1,1, 1,1, 1,1, 1,1};
    gptr g = G();
    for (u32 i = 0; i < 0x3C; i++) U32(g + 0x10BC + i * 4) = 0;
    u8 m = U8(g + 0x108E);
    if (m == 0)      stock_fill(g + 0x10BC, i0, a0, 7);
    else if (m == 1) stock_fill(g + 0x10BC, i1, a1, 6);
    else             stock_fill(g + 0x10BC, i1, a2, 6);
    for (u32 i = 0; i < 0x12; i++) U32(g + 0x11AC + i * 4) = 0;
    if (m == 0)      stock_fill(g + 0x11AC, j0, b0, 3);
    else if (m == 1) stock_fill(g + 0x11AC, j1, b1, 4);
    else             stock_fill(g + 0x11AC, j1, b2, 4);
    Flag_Set(9, 0x74, 1);
}

/* ---- weapon change and ammo pop-ups --------------------------------------- */

/* the draw-mode primitive closing a HUD group */
static void hud_mode(void)
{
    gptr p = U32(0x87DC98);
    U32(p + 4) = 0xE100040C; U32(p + 8) = 0;
    hud_link(2);
    U32(0x87DC98) += 0xC;
}

/* an 8x11 textured quad at the rect's corner (its size unused) */
void DC2CALL Hud_Glyph(gptr r, u32 u, u32 v, u32 clut, u32 col, u32 semi) /* 0x406F10 */
{
    gptr p = U32(0x87DC98);
    s32 x = S16(r), y = S16(r + 2);
    F32(p + 0x28) = (float)x;       U32(p + 8) = U32(p + 0x28);
    F32(p + 0x38) = (float)(x + 8); U32(p + 0x18) = U32(p + 0x38);
    F32(p + 0x1C) = (float)y;       U32(p + 0xC) = U32(p + 0x1C);
    F32(p + 0x3C) = (float)(y + 0xB); U32(p + 0x2C) = U32(p + 0x3C);
    U32(p + 0x40) = U32(p + 0x30) = U32(p + 0x20) = U32(p + 0x10) = 0x3C23D70A;
    U8(p + 0x34) = U8(p + 0x14) = (u8)u; U8(p + 0x44) = U8(p + 0x24) = (u8)(u + 8);
    U8(p + 0x25) = U8(p + 0x15) = (u8)v; U8(p + 0x45) = U8(p + 0x35) = (u8)(v + 0xB);
    U16(p + 0x16) = (u16)clut; U16(p + 0x26) = 0xC;
    U32(p + 4) = col | ((u16)semi ? 0x2E000000 : 0x2C000000);
    hud_link(9);
    U32(0x87DC98) += 0x48;
}

/* a line (x0, y0) - (x1, y1) */
void DC2CALL Hud_Line(u32 x0, u32 y0, u32 x1, u32 y1, u32 col, u32 semi) /* 0x4070A0 */
{
    gptr p = U32(0x87DC98);
    F32(p + 8) = (float)(s16)x0; F32(p + 0xC) = (float)(s16)y0;
    F32(p + 0x14) = (float)(s16)x1; F32(p + 0x18) = (float)(s16)y1;
    U32(p + 0x1C) = U32(p + 0x10) = 0x3C23D70A;
    U32(p + 4) = col | ((u16)semi ? 0x42000000 : 0x40000000);
    hud_link(3);
    U32(0x87DC98) += 0x20;
}

/* n digits of v at x, y (leading zeros shown once lead is set or a digit is
   nonzero; the last always), col bit 31 semi-transparent */
static void hud_digits(u32 x, u32 y, u32 n, u32 lead, u32 col, u32 v, u32 dx, int kind)
{
    u32 rgb = col & 0xFFFFFF, semi = col >> 31, p10 = 1, x0 = x;
    if ((u16)n > 1) for (u32 i = (u16)n - 1; i; i--) p10 *= 10;
    gptr r = Tmp_Push(8);
    for (u32 i = (u16)n; i; i--, x += dx) {
        u32 q = v / p10;
        if ((u16)q) lead = 1;
        v -= (q & 0xFFFF) * p10;
        p10 /= 10;
        if (!p10) lead = 1;
        else if (!(u16)lead && !(u16)q) continue;
        U16(r) = (u16)x; U16(r + 2) = (u16)y;
        if (kind == 0) { U16(r + 4) = 0x10; U16(r + 6) = 0xB; Hud_Sprite(r, (q + 6) << 4, 0xB0, 0x7C8F, rgb, semi); }
        else if (kind == 1) { U16(r + 4) = 8; U16(r + 6) = 0xC; Hud_Sprite(r, q << 3, 0x18, 0x7C8F, rgb, semi); }
        else { U16(r + 4) = 0x10; U16(r + 6) = 0x14; Hud_Glyph(r, q << 3, 0, 0x7C8F, rgb, semi); }
    }
    if (kind == 0) {                                 /* the unit after them */
        U16(r) = (u16)(x0 + 0x68); U16(r + 2) = (u16)y; U16(r + 4) = 0x18; U16(r + 6) = 0xB;
        Hud_Sprite(r, 0x48, 0xB0, 0x7C8F, rgb, semi);
    }
    Tmp_Pop(8);
    hud_mode();
}

void DC2CALL Hud_Number(u32 x, u32 y, u32 n, u32 lead, u32 col, u32 v)     /* 0x406890 */
{ hud_digits(x, y, n, lead, col, v, 0xF, 0); }
void DC2CALL Hud_SmallNum(u32 x, u32 y, u32 n, u32 lead, u32 col, u32 v)   /* 0x406A10 */
{ hud_digits(x, y, n, lead, col, v, 7, 1); }
void DC2CALL Hud_GlyphNum(u32 x, u32 y, u32 n, u32 lead, u32 col, u32 v)   /* 0x406B50 */
{ hud_digits(x, y, n, lead, col, v, 7, 2); }

/* the frame, count and picture of a weapon (id) changed to */
static void weapon_box(u32 id, u32 lcol, int sub)
{
    gptr g = G();
    s8 idx = (s8)Item_Find(g + 0x10BC, 0x14, id);
    Hud_Line(0xC, 0xC0, 0x4C, 0xC0, lcol, 0);
    Hud_Line(0xC, 0xC0, 0xC, 0xD8, lcol, 0);
    Hud_Line(0x4C, 0xC0, 0x4C, 0xD8, lcol, 0);
    Hud_Line(0xC, 0xD8, 0x4C, 0xD8, lcol, 0);
    gptr t = 0x704268 + 12 * (id & 0xFF);
    if (U8(g + 0x1094) & 1) Hud_Picture(0x685CC8, 0x30, 0xCC, 0xC, 0x7C8F, 0x707070);
    else if (!(U8(t + 2) & 0x80)) Hud_SmallNum(0x28, 0xC8, 4, 0, 0x707070, (u32)(s32)S16(g + idx * 12 + 0x10C4));
    gptr p = U32(0x87DC98);
    u32 x0 = 0x41A00000, x1 = 0x42880000;                    /* 20, 68 */
    if (!sub && (U8(t + 2) & 0x20)) { x0 = 0xC0800000; x1 = 0x42B80000; }   /* -4, 92 */
    U32(p + 0x28) = U32(p + 8) = x0; U32(p + 0x38) = U32(p + 0x18) = x1;
    U32(p + 0x1C) = U32(p + 0xC) = 0x43400000; U32(p + 0x3C) = U32(p + 0x2C) = 0x43580000;   /* 192, 216 */
    U32(p + 0x40) = U32(p + 0x30) = U32(p + 0x20) = U32(p + 0x10) = 0x3C23D70A;
    if (sub && (u8)id == 0x19) {
        U8(p + 0x34) = U8(p + 0x14) = 0x40; U8(p + 0x44) = U8(p + 0x24) = 0x7F;
        U8(p + 0x25) = U8(p + 0x15) = 0x40; U8(p + 0x45) = U8(p + 0x35) = 0x5F;
        U16(p + 0x16) = 0x7F00; U16(p + 0x26) = 0x9B;
    } else {
        U8(p + 0x34) = U8(p + 0x14) = U8(t);
        U8(p + 0x44) = U8(p + 0x24) = (u8)(U8(t) + (!sub && (U8(t + 2) & 0x20) ? 0x7F : 0x3F));
        U8(p + 0x25) = U8(p + 0x15) = U8(t + 1); U8(p + 0x45) = U8(p + 0x35) = (u8)(U8(t + 1) + 0x1F);
        U16(p + 0x16) = 0x7C80; U16(p + 0x26) = 0x8D;
    }
    U32(p + 4) = sub ? 0x2CC02000 : 0x2C208020;
    hud_link(9);
    U32(0x87DC98) += 0x48;
    gptr r = Tmp_Push(8);
    U16(r) = 0xC; U16(r + 2) = 0xC0; U16(r + 4) = 0x40; U16(r + 6) = 0x18;
    Hud_Tile(r, 0, 1);
    Tmp_Pop(8);
    hud_mode();
}

/* the two pop-up bars at y 0x1C: picture (0x7C, w 0x40) and tile (0x84, w 0x30) */
static void ammo_bars(u32 y, u32 scol, u32 tcol)
{
    gptr r = Tmp_Push(0x10), a = r + 8;
    U16(a) = 0x7C; U16(a + 2) = (u16)y; U16(a + 4) = 0x40; U16(a + 6) = 8;
    U16(r) = 0x84; U16(r + 2) = (u16)y; U16(r + 4) = 0x30; U16(r + 6) = 8;
    Hud_Sprite(a, 0x98, 0xA8, 0x7C8F, scol, 0);
    Hud_Tile(r, tcol, 0);
    Tmp_Pop(0x10);
    hud_mode();
}

/* the weapon change boxes (main and sub weapon), the ammo pop-up (count,
   rounds) and the bar fading in under the gauge */
void DC2CALL Hud_Weapon(void)                                         /* 0x404CC0 */
{
    gptr g = G(), b = g + 0x68C, pl = U32(g + 0x940);
    u8 md = U8(g + 0x108F);
    if (md <= 2 || md == 5 || md == 6) {
        if (U8(b + 0xCE)) {
            weapon_box(U8(g + 0x10B4), 0x108020, 0);
            U8(b + 0xCE)--;
        }
        g = G();
        if (U8(b + 0xCF)) {
            weapon_box((u8)(U8(g + 0x10B5) + 0x10), 0x802010, 1);
            U8(b + 0xCF)--;
            gptr q = U32(G() + 0x940);
            u16 s = U16(q + 0x54);
            if (s == 0x501 || (s == 0x901 && (U8(0x87DC08) & 8))) { U8(b + 0xCF) = 0; U8(b + 0xCE) = 1; }
            else if (U8(q + 0x54) != 1) { U8(b + 0xCF) = 0; U8(b + 0xCE) = 0; }
            g = G();
        }
        u16 s = U16(U32(g + 0x940) + 0x54);
        if (s == 0x501 || (s == 0x901 && (U8(0x87DC08) & 8))) { U8(b + 0xCF) = 0; U8(b + 0xCE) = 1; }
    }
    if (Flag_Check(9, 0x34)) U16(pl + 0x17E) = 0;
    if (U8(pl + 0x54) != 1 || (Flag_Check(3, 0xB) && U16(G() + 0x928) != U16(b + 0xBE))) {
        U8(b + 0xCA) = 0; U16(b + 0xC0) = 0; U8(b + 0xCD) = 0;
    }
    switch (S8(b + 0xCA)) {
    case 1:
        if (S16(b + 0xC4) > 1) { U8(b + 0xCA) = 3; U16(b + 0xC0) = 0; break; }
        { u32 t = Flag_Check(9, 0x39) ? 0x60 : 0x20;
          if (!U8(G() + 0x108E)) t *= 3;
          U8(b + 0xCA) = 2; U16(b + 0xC0) = (u16)(t + 0xB4); }
        break;
    case 2: {
        u32 col = 0x808080;
        if (S16(b + 0xC0) < 0x80) { u32 c = (u32)(s32)S16(b + 0xC0); col = ((((c | 0xFFFF8000u) << 8) | c) << 8) | c; }
        Hud_Number(0x50, 0x1C, 7, 0, col, U32(b + 0xB4));
        if (--U16(b + 0xC0), S16(b + 0xC0) < 0) goto done;
        break; }
    case 3: case 4: {
        u32 e = (u16)(U16(b + 0xC0) * 3);
        ammo_bars(0x1C, (e + 0x30) | (e + 0x30) << 8 | (e + 0x60) << 16,
                  (0xD000 - (e << 9)) | (0xD0 - 2 * e) | (0xD00000 - (e << 17)));
        Hud_GlyphNum(0x94, 0x25, 2, 1, (e + 0x80) << 16 | (e + 0x60) << 8 | (e + 0x60), (u32)(s32)S16(b + 0xC4));
        if (S8(b + 0xCA) == 3) {
            if (++U16(b + 0xC0), S16(b + 0xC0) >= 0x20) { U8(b + 0xCA) = 4; U16(b + 0xC0) = 0x20; }
        } else if (--U16(b + 0xC0), S16(b + 0xC0) <= 0) {
            U8(b + 0xCA) = 5; U16(b + 0xC0) = 0x20;
            if (Flag_Check(9, 0x39)) U16(b + 0xC0) = (u16)(U16(b + 0xC0) * 3);
            if (!U8(G() + 0x108E)) U16(b + 0xC0) = (u16)(U16(b + 0xC0) * 3);
        }
        break; }
    case 5:
        ammo_bars(0x1C, 0x603030, 0xD0D0D0);
        Hud_GlyphNum(0x94, 0x25, 2, 1, 0x806060, (u32)(s32)S16(b + 0xC4));
        if (--U16(b + 0xC0), S16(b + 0xC0) <= 0) { U8(b + 0xCA) = 6; U16(b + 0xC0) = 0; }
        break;
    case 6: {
        Hud_DrawArea(0, 0, 0x140, 0xF0);
        s32 c = S16(b + 0xC0);
        ammo_bars((u16)(0x1C - U16(b + 0xC0)), 0x603030, 0x808080);
        Hud_GlyphNum(0x94, (u32)(0x25 - c), 2, 1, 0x806060, (u32)(s32)S16(b + 0xC4));
        Hud_Number(0x50, (u32)(0x32 - S16(b + 0xC0)), 7, 0, 0x808080, U32(b + 0xB4));
        Hud_DrawArea(0, 0x1C, 0x140, 0x16);
        if (++U16(b + 0xC0), S16(b + 0xC0) > 0x16) { U8(b + 0xCA) = 7; U16(b + 0xC0) = 0x50; }
        break; }
    case 7: {
        u32 c = (u32)(s32)S16(b + 0xC0);
        Hud_Number(0x50, 0x1C, 7, 0, c << 9 | c << 17 | (c + c) | 0x80000000u, U32(b + 0xB4));
        if (--U16(b + 0xC0), S16(b + 0xC0) < 0) goto done;
        break; }
    }
    goto bar;
done:
    U8(b + 0xCA) = 0; U16(b + 0xC0) = 0; U8(b + 0xCD) = 0;
bar:
    switch (S8(b + 0xCB)) {
    case 1:
        U8(b + 0xCB) = 2; U16(b + 0xC2) = 0;
        /* fall through */
    case 2: {
        gptr r = Tmp_Push(8);
        for (u32 i = 0; i < 8; i++) {
            s16 x = (s16)(U16(b + 0xC2) - 8 * i - 0x64);
            U16(r) = (u16)(x > 0x18 ? 0x18 : x); U16(r + 2) = (u16)(0x1C + i); U16(r + 4) = 0x40; U16(r + 6) = 1;
            Hud_Sprite(r, 0x98, 0xA0 + i, 0x7C8F, 0xA0E0, 0);
        }
        Tmp_Pop(8);
        hud_mode();
        U16(b + 0xC2) += 8;
        if (S16(b + 0xC2) > 0xC0) { U8(b + 0xCB) = 3; U16(b + 0xC2) = 0x20; }
        break; }
    case 3: case 4: {
        gptr r = Tmp_Push(8);
        u32 e = (u16)(U16(b + 0xC2) * 3);
        U16(r) = 0x18; U16(r + 2) = 0x1C; U16(r + 4) = 0x40; U16(r + 6) = 8;
        Hud_Sprite(r, 0x98, 0xA0, 0x7C8F, S8(b + 0xCB) == 3 ? (e + 0x40) << 8 | (e + 0x80) : 0x4080, 0);
        U16(r) = 0x20; U16(r + 4) = 0x30;
        u32 c = (u16)(0xE0 - (u16)(U16(b + 0xC2) * 7)), tc = ((c << 8 | c) << 8) | c;
        Hud_Tile(r, S8(b + 0xCB) == 3 ? tc : 0xFFFFFF, 0);
        Tmp_Pop(8);
        hud_mode();
        if (--U16(b + 0xC2), S16(b + 0xC2) < 0) {
            if (S8(b + 0xCB) == 3) { U16(b + 0xC2) = 0x1E; U8(b + 0xCB)++; }
            else { U8(b + 0xCB) = 0; U16(b + 0xC2) = 0; }
        }
        break; }
    }
}

/* ---- small steps the script, the game task and enemies call ------------------------ */

/* the event timer: G+0x12C8/0x12CC = frames (flag 3:9 while it runs) */
void DC2CALL Timer_Set(u32 frames)                                    /* 0x405DE0 */
{
    gptr g = G();
    U32(g + 0x12CC) = frames; U32(g + 0x12C8) = frames;
    Flag_Set(3, 9, frames != 0);
    Flag_Set(2, 0xB, 0);
}

/* an effect let go of its parent (+0x54): position, angles and the +0x60
   vector carried into the world */
void DC2CALL Eff_Detach(gptr e)                                       /* 0x474B20 */
{
    gptr par = U32(e + 0x54);
    if (!par) return;
    gptr t = Tmp_Push(8);
    ApplyMatrixSV(par + 0x20, e + 0x40, t);
    U16(e + 0x40) = (u16)(U16(par + 0x40) + U16(t));
    U16(e + 0x42) = (u16)(U16(par + 0x42) + U16(t + 2));
    U16(e + 0x44) = (u16)(U16(par + 0x44) + U16(t + 4));
    U16(e + 0x48) += U16(par + 0x48); U16(e + 0x4A) += U16(par + 0x4A); U16(e + 0x4C) += U16(par + 0x4C);
    ApplyMatrixSV(par + 0x20, e + 0x60, e + 0x60);
    U32(e + 0x54) = 0;
    Tmp_Pop(8);
}

/* an object's movement block reset with n + 1 steps */
void DC2CALL Obj_MoveReset(gptr o, u32 n)                             /* 0x47A8C0 */
{
    U32(o + 0x150) = 0; U32(o + 0x154) = 0; U32(o + 0x140) = 0; U32(o + 0x144) = 0;
    U16(o + 0x168) = 0; U16(o + 0x16C) = 0; U8(o + 0x160) = 0;
    U32(o + 0x158) = (n & 0xFF) + 1; U8(o + 0x15F) = 7;
    U16(o + 0x164) = 0x50; U16(o + 0x166) = 0x50; U16(o + 0x16A) = 0x200; U16(o + 0x16E) = 0x300;
}

/* the current room's counter (G+0x12D0 by the room's global index) + 1 */
u32 DC2CALL Room_CountUp(void)                                        /* 0x48FDA0 */
{
    gptr a = G() + 0x12D0 + (Room_GlobalIndex(U8(G() + 0x1091), U8(G() + 0x1090)) & 0xFF);
    return ++U8(a);
}

/* a 12-byte item list closed up: each empty entry takes the next used one */
void DC2CALL Item_Compact(gptr list, u32 n)                           /* 0x495D70 */
{
    s32 last = (s32)(n & 0xFF) - 1;
    for (u32 i = 0; (s32)i < last; i++) {
        gptr e = list + 12 * i;
        if (U8(e)) continue;
        for (u8 j = (u8)(i + 1); j < (u8)n; j++) {
            gptr f = list + 12 * j;
            if (!U8(f)) continue;
            U32(e) = U32(f); U32(e + 4) = U32(f + 4); U32(e + 8) = U32(f + 8);
            U32(f) = 0; U32(f + 4) = 0; U32(f + 8) = 0;
            break;
        }
    }
}

/* the 6 carried items (G+0x11AC): one unit of a later stack of the same item
   moved onto an earlier one that is not full; an emptied stack removed */
void DC2CALL Item_Merge(void)                                         /* 0x495DF0 */
{
    for (u16 i = 0; i < 5; i++) {
        gptr e = G() + 0x11AC + 12 * i;
        if (!U8(e)) continue;
        for (u16 k = 5; k > i; k--) {
            gptr t = G() + 0x11AC + 12 * k;
            if (!U8(t) || U8(t + 1) != U8(e + 1) || S16(e + 8) >= S16(e + 0xA)) continue;
            U16(e + 8)++; U16(t + 8)--;
            if (S16(t + 8) <= 0) {
                U32(t) = 0; U32(t + 4) = 0; U32(t + 8) = 0;
                Item_Compact(G() + 0x11AC, 6);
            }
            break;
        }
    }
}

/* the event timer on screen (8 glyphs "mm:ss:ff" at 0x64.., y 50), counting
   down unless flag 1:E; red under a tenth, yellow under half; at 0 flag 3:9
   off, 2:B on */
void DC2CALL Timer_Draw(void)                                         /* 0x405E40 */
{
    gptr g = G();
    if (!Flag_Check(1, 0xE)) U32(g + 0x12C8)--;
    u32 t = U32(g + 0x12C8);
    s32 tot = S32(g + 0x12CC);
    u32 col = tot / 10 > (s32)t ? 0x101080 : tot / 2 <= (s32)t ? 0x108000 : 0x108080;
    gptr T = Tmp_Push(0x34), buf = Tmp_Push(0x40);    /* the glyph quad and the text (stack blocks of the original) */
    memset(MEM(T), 0, 0x34);
    U32(T) = col | 0x6C000000;
    U32(T + 8) = 0x42480000; U32(T + 0x14) = 0x42480000; U32(T + 0x20) = 0x42800000; U32(T + 0x2C) = 0x42800000;   /* 50, 64 */
    U16(T + 0xE) = 0x7DCC; U8(T + 0x18) = 0xE; U8(T + 0x25) = 0xE; U8(T + 0x30) = 0xE; U8(T + 0x31) = 0xE;
    u32 m = t / 1800, r = t - m * 1800, s = r / 30;
    Crt_sprintf3(buf, 0x71D5C4, m, s, (r - s * 30) * 3);
    for (u32 i = 0, x = 0x72; x < 0xE2; i++, x += 0xE) {
        U16(T + 0x1A) = U8(buf + i);
        F32(T + 4) = (float)(x - 0xE); F32(T + 0x10) = (float)x; F32(T + 0x1C) = (float)(x - 0xE); F32(T + 0x28) = (float)x;
        gptr p = U32(0x87DC98);
        memcpy(MEM(p + 4), MEM(T), 0x34);
        hud_link(2);
        U32(0x87DC98) += 0x38;
    }
    Tmp_Pop(0x40); Tmp_Pop(0x34);
    if (S32(G() + 0x12C8) <= 0) { Flag_Set(3, 9, 0); Flag_Set(2, 0xB, 1); }
}

/* the boss's gauge at x, y: frame, bar by G+0x928 of G+0x92A (pulsing green,
   yellow, red) and the health lost going down behind it */
void DC2CALL Boss_Bar(u32 x, u32 y)                                   /* 0x4065A0 */
{
    gptr g = G(), b = g + 0x68C;
    s32 w = S16(g + 0x928) * 0x70 / S16(g + 0x92A);
    if ((s16)w < 0) w = 0; else if ((s16)w > 0x70) w = 0x70;
    gptr r = Tmp_Push(8);
    U16(r) = (u16)x; U16(r + 2) = (u16)y; U16(r + 4) = 0x40; U16(r + 6) = 8;
    Hud_Sprite(r, 0, 0xF8, 0x7E8F, 0x808080, 0);
    gptr p = U32(0x87DC98);
    U32(p + 4) = 0xE100041A; U32(p + 8) = 0;
    hud_link(2);
    U32(0x87DC98) += 0xC;
    Hud_Picture(0x6850A8, x + 0x40, y + 8, 0xC, 0x7C8F, 0x808080);
    Hud_Picture(0x683620, x, y, 0xD, 0x7C8F, 0x808080);
    s32 hp = S16(G() + 0x928), mx = S16(G() + 0x92A);
    u32 col = hp <= mx / 4 ? Color_Pulse(0x80, 0x40, 3) : hp <= mx / 2 ? Color_Pulse(0x8080, 0x4040, 4) : Color_Pulse(0x8000, 0x4000, 5);
    U16(r) = (u16)x; U16(r + 2) = (u16)y; U16(r + 4) = (u16)w; U16(r + 6) = 0x10;
    Hud_Sprite(r, 0x70, 0x50, 0x7C8F, col, 0);
    U16(b + 0xBE) -= 6;
    if (S16(G() + 0x928) >= S16(b + 0xBE)) U16(b + 0xBE) = U16(G() + 0x928);
    else {
        s32 w2 = S16(b + 0xBE) * 0x70 / S16(G() + 0x92A);
        if ((s16)w2 >= 0 && (s16)w2 <= 0x70) {
            U16(r) = (u16)x; U16(r + 2) = (u16)y; U16(r + 4) = (u16)w2; U16(r + 6) = 0x10;
            Hud_Sprite(r, 0x70, 0x50, 0x7C8F, col >> 1, 0);
        }
    }
    Tmp_Pop(8);
    hud_mode();
}

/* the boss's gauge (flag 3:7, not while 2:7); a hit resets the combo */
void DC2CALL Boss_Gauge(u32 x, u32 y)                                 /* 0x406520 */
{
    gptr b = G() + 0x68C;
    if (Flag_Check(2, 7) || !Flag_Check(3, 7)) return;
    Boss_Bar(x, y);
    if (U16(G() + 0x928) != U16(b + 0xBE)) { U8(b + 0xCC) = 1; U16(G() + 0xC20) = 0; U16(G() + 0xC22) = 0; }
}

/* a menu or screen started from the game (flag 2:8; G+0x68C+0x38.. its four
   arguments): 1 the item box, 2 the save / pause screen, 3 and 6 the map
   (area, room in b), 4 the file, 5 the card key used up */
void DC2CALL Menu_Open(u32 a, u32 b, u32 c, u32 d, u32 e)              /* 0x495E90 */
{
    gptr m = G() + 0x68C;
    Flag_Set(2, 8, 1);
    U16(m + 0x3A) = (u16)c; U16(m + 0x3E) = (u16)e; U16(m + 0x38) = (u16)b; U16(m + 0x3C) = (u16)d;
    switch (a & 0xFFFF) {
    case 1: {
        static const u8 fl[] = {1, 2, 3, 4, 6, 7, 8, 0xA, 0xD, 0xE};
        U32(m + 0x68) = U32(G() + 0xB14);
        for (unsigned k = 0; k < sizeof fl; k++) Flag_Set(1, fl[k], 1);
        U8(m + 0x64) = 1;
        if (U16(m + 0x38) == 0x1E && !U8(G() + 0x108E)) U16(m + 0x38) = 0x1D;
        break; }
    case 2:
        Flag_Set(2, 8, 0); Flag_Set(2, 0x11, 1);
        U8(G() + 0x663) = 0;
        break;
    case 3: {
        U8(m + 0x64) = 3; U8(m + 0x63) = (u8)b;
        U8(G() + 0x1220) = (u8)b;
        u8 s = (u8)Area_SoundArea((u8)(b >> 8), b);
        U8(m + 0x62) = s;
        U8(G() + 0x1221) = s; U8(G() + 0x1222) = (u8)c; U8(G() + 0x1223) = (u8)d;
        Flag_Set(0xB, U8(m + 0x62) + 0x75, 1);
        U8(m + 0x60) = (u8)(U8(m + 0x62) + 5);
        break; }
    case 4:
        U8(m + 0x64) = 4;
        break;
    case 5: {
        s8 i = (s8)Item_Find(G() + 0x10BC, 0x14, 0x19);
        gptr it = G() + (i + 0x165) * 12;
        U8(it) = 0; U8(it + 1) = 0; U16(it + 2) = 0; U32(it + 4) = 0; U16(it + 8) = 0; U16(it + 0xA) = 0;
        Flag_Set(2, 8, 0);
        U16(m + 0x38) = 0; U16(m + 0x3A) = 0; U16(m + 0x3C) = 0; U16(m + 0x3E) = 0;
        gptr g = G();
        if ((U16(g + 0x10B4) & 0xFF00) == 0x900) {
            U16(g + 0x10B4) = (u16)(U8(g + 0x10B4) | (Flag_Check(9, 0x38) ? 0x800 : 0x200));
            U16(g + 0x10B6) = U16(g + 0x10B4);
        }
        if ((U16(g + 0x10B8) & 0xFF00) == 0x900)
            U16(g + 0x10B8) = (u16)(U8(g + 0x10B8) | (Flag_Check(9, 0x37) ? 0x700 : 0x100));
        break; }
    case 6: {
        u32 area = (c & ~0xFFu) | ((b >> 8) & 0xFF);
        Flag_Set(0xB, Room_GlobalIndex(area, b) & 0xFF, 1);
        U8(m + 0x64) = 3; U8(m + 0x63) = (u8)b;
        u8 s = (u8)Area_SoundArea(area, b);
        U8(m + 0x62) = s; U8(m + 0x60) = (u8)(s + 5);
        break; }
    }
}

/* the stock by the game's mode G+0x108F (0, 1: the main game's two
   characters, 3, 5, 6: the extra modes; 4, 7..12: one item) after both
   tables are cleared */
void DC2CALL Game_ModeStock(void)                                     /* 0x452450 */
{
    static const u8  i0[] = {6, 7, 0x12, 0x16};
    static const u16 a0[] = {9999,9999, 300,300, 9999,9999, 20,20};
    static const u8  i1[] = {1, 3, 0x11, 0x13, 0x16};
    static const u16 a1[] = {9999,9999, 5,5, 9999,9999, 10,10, 10,10};
    static const u8  i5[] = {4, 0x16, 0x13};
    static const u16 a5[] = {9999,9999, 20,20, 10,10};
    static const u8  i6[] = {5, 9, 0x16};
    static const u16 a6[] = {9999,9999, 20,20, 10,10};
    static const u8  j0[] = {0x1A, 0x1B, 0x1C};
    static const u16 b0[] = {2,2, 2,2, 1,2};
    static const u8  jk[] = {0x1F};
    static const u16 bk[] = {1,2};
    gptr g = G();
    for (u32 i = 0; i < 0x3C; i++) U32(g + 0x10BC + i * 4) = 0;
    for (u32 i = 0; i < 0x12; i++) U32(g + 0x11AC + i * 4) = 0;
    switch (U8(g + 0x108F)) {
    case 0: stock_fill(g + 0x10BC, i0, a0, 4); stock_fill(g + 0x11AC, j0, b0, 3); break;
    case 1: stock_fill(g + 0x10BC, i1, a1, 5); stock_fill(g + 0x11AC, j0, b0, 3); break;
    case 2: break;
    case 3: stock_fill(g + 0x11AC, j0 + 1, b0 + 2, 2); break;
    case 5: stock_fill(g + 0x10BC, i5, a5, 3); stock_fill(g + 0x11AC, j0, b0, 3); break;
    case 6: stock_fill(g + 0x10BC, i6, a6, 3); stock_fill(g + 0x11AC, j0, b0, 3); break;
    case 4: case 7: case 8: case 9: case 10: case 11: case 12: stock_fill(g + 0x11AC, jk, bk, 1); break;
    }
}
