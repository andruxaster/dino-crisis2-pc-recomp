/* What the enemies share: hits on the player, scores, effects they leave,
 * the game over. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/pcio.h"
#include "dc2/gte.h"

/* a flash (data: n, loops, fades, frames...) on an object or the room */
gptr DC2CALL Flash_Play(gptr owner, gptr data)                     /* 0x478B80 */
{
    gptr f = owner ? Flash_AllocObj(owner) : Flash_Alloc();
    if (!f) return 0;
    u8 c = (u8)((U8(f + 0xC) & 0xFD) | 1);
    U32(f) = data;
    U32(f + 4) = data + 4;
    U8(f + 0xC) = c;
    U32(f + 8) = data + U8(data);
    c = (u8)((c & 0xBF) | (U8(data + 1) != 0) << 6);
    U8(f + 0xC) = c;
    U8(f + 0xC) = (u8)((c & 0x7F) | (U8(data + 2) != 0) << 7);
    return f;
}

/* a prop of type 6 following an object (the one it had goes) */
gptr DC2CALL Prop_Attach(gptr obj, u32 arg)                         /* 0x48DAB0 */
{
    gptr p = K2_Alloc();
    if (!p) return 0;
    for (gptr q = U32(G() + 0x944) + U32(G() + 0xA70) * 0xAC; q < U32(G() + 0x944) + U32(0x87DC88) * 0xAC; q += 0xAC)
        if ((U8(q + 0x50) & 1) && U32(q + 0xA8) == obj) K2_Unlink(q);
    U32(p + 0xA0) = obj;
    U32(p + 0x98) = arg;
    U8(p + 0x58) = 6;
    Orig_Call1(U32(U32(0x87F948) + 0x18), p);
    return p;
}

/* a prop of type 0xC at pos */
gptr DC2CALL Prop_SpawnC(u32 kind, gptr pos, u32 a, u32 b, u32 c, u32 d) /* 0x48DC20 */
{
    gptr p = K2_AllocBack();
    if (!p) return 0;
    U32(p + 0x40) = U32(pos);
    U32(p + 0x44) = U32(pos + 4);
    U8(p + 0x7A) = (u8)kind;
    U8(p + 0x58) = 0xC;
    U16(p + 0x98) = (u16)a; U16(p + 0x9A) = (u16)b; U16(p + 0x9C) = (u16)c; U16(p + 0x9E) = (u16)d;
    return p;
}

/* the target within dist and within ang either side of the object's yaw */
u32 DC2CALL Obj_InSight(gptr o, gptr target, u32 dist, u32 ang)     /* 0x47A780 */
{
    if (Dist2_XZ(o + 0x40, target) > dist) return 0;
    u32 d = ((u32)(s32)(s16)Angle_XZ(o + 0x40, target) - U16(o + 0x4A)) & 0xFFF;
    if (ang < d && d < 0x1000 - ang) return 0;
    return 1;
}

/* the points of a kind of event (0x7041B8 by type) */
void DC2CALL Score_Event(gptr o)                                    /* 0x404980 */
{
    gptr g = G();
    u32 s = U16(0x7041B8 + U8(o + 0x58) * 2);
    if (Flag_Check(9, 0x36)) s <<= 1;
    if (!U8(G() + 0x108E)) s <<= 1;
    U8(g + 0x68C + 0xCB) = 1;
    U32(G() + 0x11F4) += s;
    U32(G() + 0xC14) += s;
    U32(G() + 0x120C) += s;
    if (S32(G() + 0x11F4) > 9999999) U32(G() + 0x11F4) = 9999999;
}

/* the points of a kill (0x704178 by type, by the kill's style +0x11D and
 * the combo G+0x68C+0xC4) */
void DC2CALL Score_Kill(gptr o)                                     /* 0x404AF0 */
{
    gptr r = G() + 0x68C;
    u8 t = U8(o + 0x58);
    u32 s = U16(0x704178 + t * 2);
    if (t == 8 && U16(G() + 0x1090) == 0x808) s = 1000;
    if (t == 2) {
        if (U16(G() + 0x1090) == 0x407) s = 0x32;
        if (U16(o + 0x10C) == 5) s = 0x1B58;
    }
    if (Flag_Check(9, 0x36)) s <<= 1;
    if (!U8(G() + 0x108E)) s <<= 1;
    s8 c = S8(o + 0x11D);
    if (c > 100) U8(o + 0x11D) = 100;
    else if (c < -100) U8(o + 0x11D) = 0x9C;
    s = (u32)((S8(o + 0x11D) + 100) * s) / 100;
    Fn_48FDA0();
    if (!U8(r + 0xCD)) { U16(r + 0xC4) = 0; U32(r + 0xB4) = 0; }
    if (!U8(r + 0xCC)) {
        U16(G() + 0xC20)++;
        U16(G() + 0xC22) = U8(o + 0x58);
    }
    if (U8(r + 0xCD)) {                             /* the combo: +20% a kill, triple from the tenth */
        if (S16(r + 0xC4) > 9) s *= 3;
        else s += (u32)((s32)S16(r + 0xC4) * s) / 5;
    }
    U16(r + 0xC4)++;
    if ((s32)S16(r + 0xC4) > (s32)U16(G() + 0xC24)) U16(G() + 0xC24) = U16(r + 0xC4);
    U8(r + 0xCA) = 1;
    U32(G() + 0x11F4) += s;
    U32(r + 0xB4) += s;
    U32(G() + 0xC14) += s;
    U32(G() + 0x120C) += s;
    if (S32(G() + 0x11F4) > 9999999) U32(G() + 0x11F4) = 9999999;
    U8(r + 0xCD) = 1;
}

/* from where the enemy hits the player (front, sides, back by quarter
 * turns): the player's +0x178 */
u32 DC2CALL Enemy_HitDir(gptr o)                                    /* 0x479D00 */
{
    gptr pl = (U8(o + 0x184) & 0x40) ? U32(G() + 0xB50) : U32(G() + 0x940);
    if (!pl) return 0;
    s32 d = (s32)(((U16(o + 0x4A) + 0x200) >> 10) & 3) - (s32)(((U16(pl + 0x4A) + 0x200) >> 10) & 3);
    U16(pl + 0x178) = U8(0x7034A7 + d);
    return U8(0x7034A7 + d);
}

/* the same for the object itself against its +0x178 */
u32 DC2CALL Obj_HitDir(gptr o)                                      /* 0x479CB0 */
{
    s32 d = (s32)(((U16(o + 0x4A) + 0x200) >> 10) & 3) - (s32)(((U16(o + 0x178) - 0x600) >> 10) & 3);
    U16(o + 0x178) = U8(0x70349F + d);
    return U8(0x70349F + d);
}

/* the player starts or keeps bleeding (+0x17E) after a bite of n */
u32 DC2CALL Player_Bleed(u32 n)                                     /* 0x4797D0 */
{
    gptr pl = U32(G() + 0x940);
    if (U8(G() + 0x108F) >= 3) return 0;
    if (U16(pl + 0x17E)) U16(pl + 0x17E) += (u16)((u16)n >> 1);
    if (Rand8() > 0x80) return 0;
    U16(pl + 0x17E) = (u16)(n + 0x2BC);
    return (u16)n;
}

/* a dead object fades (the extra game) or goes when flag 2/0x18 is set */
void DC2CALL Obj_DeathFade(gptr o)                                  /* 0x48FBB0 */
{
    if (U8(G() + 0x927)) {
        if (U8(o + 0xF8)) {
            U8(o + 0xF8) -= 8; U8(o + 0xF9) -= 8; U8(o + 0xFA) -= 8;
            U32(o + 0x6C) -= 0x40; U32(o + 0x70) -= 0x100; U32(o + 0x74) -= 0x40;
            return;
        }
    } else if (!Flag_Check(2, 0x18)) return;
    U32(o + 0x50) |= 0x40000000;
}

/* another enemy of the same type in state 1 / sub */
u32 DC2CALL Enemy_FindState(gptr o, u32 sub)                        /* 0x47AD00 */
{
    gptr q = U32(G() + 0x940);
    for (u32 i = 1; i < U32(0x87F940); i++) {
        q += 0x260;
        if (q == o || !(U8(q + 0x50) & 1) || U8(q + 0x58) != U8(o + 0x58) || U8(q + 0x54) != 1) continue;
        if (U8(q + 0x55) == sub) return 1;
    }
    return 0;
}

/* a short flash of light at an object, and its sound */
void DC2CALL Light_Flash(gptr o)                                    /* 0x414D20 */
{
    gptr l = Light_Alloc();
    if (l) {
        U8(l + 1) = 1;
        U32(l + 0xC) = 0x5EE670;
        U8(l + 2) = 3;
        U32(l + 0x10) = 0x5F0E88;
        U16(l + 0x1A) = U16(o + 0x40);
        U16(l + 0x1C) = 0;
        U16(l + 0x1E) = U16(o + 0x44);
    }
    Se_PlayFlat(0x74, 0x5F, 0x40);
}

/* the objects' drawing layers (+0xFD): each to the emptier of two */
void DC2CALL Layers_Assign(void)                                    /* 0x46B960 */
{
    u32 n[2] = {0, 0};
    gptr pl = U32(G() + 0x940);
    n[U8(pl + 0xFD) & 1]++;
    for (gptr o = pl + 0x260; o < U32(G() + 0x940) + U32(0x87F940) * 0x260; o += 0x260) {
        if (!(U8(o + 0x50) & 1)) continue;
        u8 k = n[0] > n[1];
        U8(o + 0xFD) = k;
        n[k]++;
    }
}

/* the enemy's attack spheres (+0xA0: 0x10 each, active between frames
 * +8 and +9) against the player's hit spheres (+0x134: 0xC each): the
 * hit sphere's index in +0x119; 0xFF none, 0xFE missed, 0xFD the player
 * can't be hit. Bit 3 of +0x184: an attack is on. */
u32 DC2CALL Enemy_AttackHit(gptr o)                                 /* 0x479830 */
{
    gptr pl = U32(G() + 0x940);
    U32(o + 0x184) &= ~8u;
    if (U8(o + 0x184) & 0x40) pl = U32(G() + 0xB50);
    if ((U8(pl + 0x174) & 1) || U8(pl + 0x17B)) { U8(o + 0x119) = 0xFD; return 0xFD; }
    if (!U32(o + 0xA0) || (U16(o + 0x102) & 2)) { U8(o + 0x119) = 0xFF; return 0xFF; }
    gptr hb = U32(pl + 0x134);
    if (!hb) return 0xFF;
    u32 div = (U16(o + 0x102) & 8) ? 1 : (U16(o + 0x102) & 4) ? 4 : 2;
    u32 k = 0;
    u8 res = 0xFF;
    gptr mp = Tmp_Push(0x20), mo = Tmp_Push(0x20), v1 = Tmp_Push(8), v2 = Tmp_Push(8);
    RotMatrix(pl + 0x48, mp);
    RotMatrix(o + 0x48, mo);
    for (;;) {
        gptr b = pl;
        if (U8(hb + 0xA) == 0xFF) ApplyMatrixSV(mp, hb, v1);
        else {
            b = Obj_Part(U32(pl + 0x64), U8(hb + 0xA));
            ApplyMatrixSV(b + 0x20, hb, v1);
        }
        U16(v1) += U16(b + 0x40); U16(v1 + 2) += U16(b + 0x42); U16(v1 + 4) += U16(b + 0x44);
        U16(v1 + 6) = U16(hb + 6);
        u16 h = U16(hb + 8);
        if (!U8(G() + 0x1091) && U8(G() + 0x1090) == 1) U16(v1 + 6) <<= 2;
        if (U8(G() + 0x1091) == 8 && U8(G() + 0x1090) == 0xA) U16(v1 + 6) <<= 2;
        for (gptr a = U32(o + 0xA0);; a += 0x10, k++) {
            u32 f = U16(o + 0x92);
            s16 lo2 = (s16)(U8(a + 8) - 2);
            if (lo2 < 0) lo2 = 0;
            if ((u32)(lo2 * 2) / div <= f && f <= (u32)(U8(a + 9) * 2) / div) U32(o + 0x184) |= 8;
            if ((u32)(U8(a + 8) * 2) / div <= f && f <= (u32)(U8(a + 9) * 2) / div) {
                U16(v2 + 6) = U16(a + 6);
                gptr c = o;
                if (U8(a + 0xA) == 0xFF) ApplyMatrixSV(mo, a, v2);
                else {
                    c = Obj_Part(U32(o + 0x64), U8(a + 0xA));
                    ApplyMatrixSV(c + 0x20, a, v2);
                }
                U16(v2) += U16(c + 0x40); U16(v2 + 2) += U16(c + 0x42); U16(v2 + 4) += U16(c + 0x44);
                if (Sphere_Hit(v2, v1, h, U16(a + 0xC))) {
                    Tmp_Pop(0x50);
                    U8(o + 0x119) = (u8)k;
                    return (u8)k;
                }
                res = 0xFE;
            }
            if (!U8(a + 0xB)) { k++; break; }
        }
        if (!U8(hb + 0xB)) break;
        hb += 0xC;
    }
    Tmp_Pop(0x50);
    U8(o + 0x119) = res;
    return res;
}

/* the player died: the screen fades to red, game mode 4 (continue) */
void DC2CALL GameOver_Start(u32 fade)                               /* 0x4529C0 */
{
    if (U16(G() + 0x664) == 4) return;
    Flag_Set(1, 1, 1);
    U32(0x87DE00) = 4;
    gptr p = U32(0x87DC98);
    U8(p + 4) = 0x20; U8(p + 6) = 0; U8(p + 5) = 0;
    Gpu_SetTile(p);
    Gpu_SetSemiTrans(p, 1);
    U32(p + 8) = 0; U32(p + 0xC) = 0;
    U16(p + 0x14) = 0x140; U16(p + 0x16) = 0xF0;
    Gpu_AddPrim(U32(0x87F94C) + 0x74, p);
    U32(0x87DC98) += 0x18;
    u32 tp = Gpu_GetTPage(1, 1, 0x140, 0x100) & 0xFFFF;
    Gpu_SetDrawMode(U32(0x87DC98), 1, 1, tp, 0);
    Gpu_AddPrim(U32(0x87F94C) + 0x74, U32(0x87DC98));
    U32(0x87DC98) += 0xC;
    Fn_4A7250();
    Task_Sleep(1);
    U8(G() + 0xB58) = 0x80; U8(G() + 0xB59) = 0x80; U8(G() + 0xB5A) = 0x80;
    U8(G() + 0xB5B) = 0; U8(G() + 0xB5C) = 0;
    DrawSync(0);
    Fade_Set(1, fade ? fade : 0x32, 3);
    Cutscene_End();
    Snd_FadeAll();
    u16 rm = U16(G() + 0x1090);
    if (!(rm >= 0x407 && rm <= 0x409) && !Flag_Check(2, 0xB)) Se_Play(0, 4);
    U16(G() + 0x664) = 4;
    U16(G() + 0x666) = 0; U16(G() + 0x668) = 0; U16(G() + 0x66A) = 0;
}

/* two spheres (s16 x, y, z, radius) touch in x/z and are within h + r of
   each other in height */
u32 DC2CALL Sphere_Hit(gptr a, gptr b, u32 h, u32 r)                /* 0x452FF0 */
{
    s32 dx = S16(a) - S16(b), dz = S16(a + 4) - S16(b + 4);
    if ((s32)S16(a + 6) + S16(b + 6) - SquareRoot0((s32)((u32)dz * (u32)dz + (u32)dx * (u32)dx)) < 0) return 0;
    s32 dy = S16(a + 2) - S16(b + 2);
    if (dy < 0) dy = -dy;
    if ((s32)(s16)h + (s16)r - dy < 0) return 0;
    return 1;
}
