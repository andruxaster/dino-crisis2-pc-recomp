/* Effect slots G+0x944 (0xAC each, *0x87DC88 of them): sparks, smoke,
 * splashes, pick-up glows and their emitters. Handler by type through
 * *0x87F948; +0x7B a motion step (0x7215E4), +0x54 the handler's step,
 * +0x60 what it is attached to (object part, effect slot or itself),
 * +0x8C the emitter record that counts it (+1 kind, +0x18 count),
 * +0x90/+0x94 its sprite animation, +0x82 frames left of the current one. */
#include <string.h>
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

#define K2(i) (U32(G() + 0x944) + (u32)(i) * 0xAC)

/* the emitter record +0x8C counts one less */
static void k2_src_dec(gptr s)
{
    if (s && U8(s + 1) == 1) U8(s + 0x18)--;
}

/* a point of the slot (+0x40) in the world, through the matrix it follows */
void DC2CALL K2_ToWorld(gptr k, gptr out)                           /* 0x448100 */
{
    if (!U32(k + 0x60)) return;
    ApplyMatrixSV(k + 0x20, k + 0x40, out);
    U16(out) += U16(k + 0x34); U16(out + 2) += U16(k + 0x38); U16(out + 4) += U16(k + 0x3C);
}

/* the object whose part the slot follows (0 if not a part of the pool) */
gptr DC2CALL K2_OwnerObj(gptr k)                                    /* 0x448140 */
{
    gptr p = U32(k + 0x60), pool = U32(G() + 0x938);
    if (p < pool || p > pool + U32(0x87DC0C) * 0xCC) return 0;
    for (;;) {
        gptr objs = U32(G() + 0x940);
        if (p >= objs && p <= objs + U32(0x87F940) * 0x260) return p;
        p = U32(p + 0x58);
    }
}

/* let go of what it follows: its place, speed (unless +0x84 bit 1) and, for
   motion step 3, its spin into the world */
void DC2CALL K2_Detach(gptr k)                                      /* 0x448080 */
{
    if (!U32(k + 0x60)) return;
    ApplyMatrixSV(k + 0x20, k + 0x40, k + 0x40);
    U16(k + 0x40) += U16(k + 0x34); U16(k + 0x42) += U16(k + 0x38); U16(k + 0x44) += U16(k + 0x3C);
    if (!(U8(k + 0x84) & 2)) ApplyMatrixSV(k + 0x20, k + 0x7C, k + 0x7C);
    if (U8(k + 0x7B) == 3) ApplyMatrixSV(k + 0x20, k + 0x98, k + 0x98);
    U32(k + 0x60) = 0;
}

/* the matrix of what it follows; itself once that is gone */
void DC2CALL K2_SyncParent(gptr k)                                  /* 0x4481A0 */
{
    gptr p = U32(k + 0x60);
    if (!p || p == k) return;
    if (!(U8(p + 0x50) & 1)) { U32(k + 0x60) = k; return; }
    memmove(MEM(k + 0x20), MEM(p + 0x20), 0x20);
    U32(k + 0x34) = (u32)(s32)S16(p + 0x40);
    U32(k + 0x38) = (u32)(s32)S16(p + 0x42);
    U32(k + 0x3C) = (u32)(s32)S16(p + 0x44);
}

/* to the front of the chain G+0x852 (+0x86 next, +0x87 previous) */
void DC2CALL K2_PushFront(gptr k)                                   /* 0x4481E0 */
{
    U8(k + 0x86) = U8(G() + 0x852);
    U8(G() + 0x852) = U8(k + 0x87);
    u8 n = U8(k + 0x86);
    if (n != 0xFF) U8(K2(n) + 0x87) = U8(k + 0x87);
}

/* the room light G+0xC at the slot, colour by c (0x7033A8) */
void DC2CALL K2_Light(gptr k, u32 c)                                /* 0x448240 */
{
    gptr l = G() + 0xC;
    K2_SyncParent(k);
    K2_Detach(k);
    U8(l + 3) = 1;
    U16(l + 4) = U16(k + 0x40); U16(l + 6) = U16(k + 0x42); U16(l + 8) = U16(k + 0x44);
    U16(l + 0xE) = U16(0x7033A8 + (c & 0xFF) * 2);
    U16(l + 0xA) = 0x600;
    U8(l) = 0x80; U8(l + 1) = 0x80; U8(l + 2) = 0x80;
}

/* ---- motion steps (+0x7B, table 0x7215E4); +0x98..+0x9E their values ---- */

void DC2CALL K2Step_SetAngles(gptr k)                               /* 0x4482B0 */
{
    U16(k + 0x48) = U16(k + 0x98); U16(k + 0x4A) = U16(k + 0x9A); U16(k + 0x4C) = U16(k + 0x9C);
    U8(k + 0x7B) = 0;
}

void DC2CALL K2Step_SetYaw(gptr k)                                  /* 0x4482E0 */
{
    U8(k + 0x7B) = 0;
    U16(k + 0x4A) = U16(k + 0x98);
}

void DC2CALL K2Step_Accel(gptr k)                                   /* 0x448300 */
{
    U16(k + 0x7C) += U16(k + 0x98); U16(k + 0x7E) += U16(k + 0x9A); U16(k + 0x80) += U16(k + 0x9C);
}

void DC2CALL K2Step_SpinAccel(gptr k)                               /* 0x448330 */
{
    U16(k + 0x4C) += U16(k + 0x9E);
    K2Step_Accel(k);
}

/* rising and turning for +0x9C frames */
void DC2CALL K2Step_Rise(gptr k)                                    /* 0x448350 */
{
    gptr s = U32(k + 0x8C);
    U16(k + 0x7E) += U16(k + 0x98);
    U16(k + 0x4C) += U16(k + 0x9A);
    if (--U16(k + 0x9C)) return;
    K2_Unlink(k);
    k2_src_dec(s);
}

/* growing (+0x6C, +0x6E by +0x9E) up to 0xFF; gone below 0 */
void DC2CALL K2Step_Grow(gptr k)                                    /* 0x4483A0 */
{
    gptr s = U32(k + 0x8C);
    U16(k + 0x6E) += U16(k + 0x9E);
    U16(k + 0x6C) += U16(k + 0x9E);
    if (S16(k + 0x6E) < 0) {
        U16(k + 0x6E) = 0;
        K2_Unlink(k);
        k2_src_dec(s);
    } else if (S16(k + 0x6C) < 0) {
        U16(k + 0x6C) = 0;
        K2_Unlink(k);
        k2_src_dec(s);
    }
    if (U16(k + 0x6E) > 0xFF) U16(k + 0x6E) = 0xFF;
    if (U16(k + 0x6C) > 0xFF) U16(k + 0x6C) = 0xFF;
    K2Step_Accel(k);
}

void DC2CALL K2Step_GrowLaterInit(gptr k)                           /* 0x448430 */
{
    K2Step_SetAngles(k);
    U16(k + 0x9A) = U16(k + 0x9E);
    U8(k + 0x7B) = 4;
    U16(k + 0x9E) = 0; U16(k + 0x9C) = 0; U16(k + 0x98) = 0;
}

/* after +0x98 frames, the size changes by +0x9A / +0x9C a frame */
void DC2CALL K2Step_GrowLater(gptr k)                               /* 0x448470 */
{
    gptr s = U32(k + 0x8C);
    U16(k + 0x98)--;
    if (S16(k + 0x98) >= 0) return;
    U16(k + 0x6E) += U16(k + 0x9A);
    if (S16(k + 0x6E) < 0) { K2_Unlink(k); k2_src_dec(s); return; }
    if (S16(k + 0x6E) > 0xFF) { U16(k + 0x6E) = 0xFF; U8(k + 0x7B) = 0; }
    U16(k + 0x6C) += U16(k + 0x9C);
    if (S16(k + 0x6C) < 0) { K2_Unlink(k); k2_src_dec(s); return; }
    if (S16(k + 0x6C) > 0xFF) { U16(k + 0x6C) = 0xFF; U8(k + 0x7B) = 0; }
}

/* fixed angles for +0x9E frames */
void DC2CALL K2Step_Hold(gptr k)                                    /* 0x448520 */
{
    gptr s = U32(k + 0x8C);
    U16(k + 0x9E)--;
    U16(k + 0x48) = U16(k + 0x98); U16(k + 0x4C) = U16(k + 0x9C); U16(k + 0x4A) = U16(k + 0x9A);
    if (U16(k + 0x9E)) return;
    K2_Unlink(k);
    k2_src_dec(s);
}

/* the angles of a matrix (y, x, z order through its axes) */
void DC2CALL K2_MatrixAngles(gptr m, gptr out)                      /* 0x448760 */
{
    gptr t = Tmp_Push(0x30), M = t, sv = t + 0x20, v = t + 0x28;
    memmove(MEM(M), MEM(m), 0x20);
    U32(M + 0x14) = 0; U32(M + 0x18) = 0; U32(M + 0x1C) = 0;
    U32(sv) = 0; U16(sv + 4) = 0x1000;
    ApplyMatrixSV(M, sv, v);
    U16(out) -= (u16)ratan2((float)S16(v + 2), (float)S16(v + 4));
    s32 r = SquareRoot12((s32)((u32)(S16(v + 2) * S16(v + 2)) + (u32)(S16(v + 4) * S16(v + 4))) >> 12);
    U16(out + 2) += (u16)ratan2((float)S16(v), (float)(s16)r);
    U16(sv) = 0; U16(sv + 2) = 0x1000; U16(sv + 4) = 0;
    ApplyMatrixSV(M, sv, v);
    U16(sv) = (u16)-U16(out); U16(sv + 2) = (u16)-U16(out + 2); U16(sv + 4) = 0;
    RotMatrixZYX(sv, M);
    ApplyMatrixSV(M, v, sv);
    U16(out + 4) -= (u16)ratan2((float)S16(sv), (float)S16(sv + 2));
    Tmp_Pop(0x30);
}

/* ---- the sprite animation of a slot: frames of 8 bytes (+4 op/jump, +5 length) ---- */

/* one frame: at the last count of a frame its +4 >> 5 (signed); ends,
   jumps (op 0) or restarts (op 0x40) the list */
s32 DC2CALL K2_AnimStep(gptr k)                                     /* 0x48DD10 */
{
    u16 c = U16(k + 0x82);
    if (c == 1) { U16(k + 0x82)--; return (s8)U8(U32(k + 0x94) + 4) >> 5; }
    if (c) { U16(k + 0x82)--; return 0; }
    u8 f = U8(U32(k + 0x94) + 4);
    gptr e;
    switch (f & 0xE0) {
    case 0: e = U32(k + 0x94) + (f & 0x1F) * 8; break;
    case 0x40: e = U32(k + 0x90); break;
    default: U16(k + 0x82)--; return 0;
    }
    U32(k + 0x94) = e;
    U16(k + 0x82) = U8(e + 5);
    if (U16(k + 0x82) == 1) { U16(k + 0x82)--; return (s8)U8(e + 4) >> 5; }
    U16(k + 0x82)--;
    return 0;
}

void DC2CALL K2_SetAnim(gptr k, gptr list)                          /* 0x48DCE0 */
{
    U32(k + 0x94) = list; U32(k + 0x90) = list;
    U16(k + 0x82) = U8(list + 5);
    K2_AnimStep(k);
}

/* hidden: flag 3/0xA, or inside the camera zone's mask quad */
u32 DC2CALL Pos_Hidden(gptr pos)                                    /* 0x48F8C0 */
{
    if (Flag_Check(3, 0xA)) return 1;
    gptr z = U32(G() + 0x7BC);
    if (z && PointInQuad(pos, z + 0x1C)) return 1;
    return 0;
}

/* +0x50 bit 1: where it is, in the world, is hidden */
void DC2CALL K2_UpdateHidden(gptr k)                                /* 0x48F9B0 */
{
    gptr v = Tmp_Push(8);
    U32(v) = U32(k + 0x40); U32(v + 4) = U32(k + 0x44);
    if (U8(k + 0x50) & 1) {
        gptr p = U32(k + 0x60);
        if (p == k) {
            ApplyMatrixSV(p + 0x20, v, v);
            U16(v) += U16(k + 0x34); U16(v + 2) += U16(k + 0x38); U16(v + 4) += U16(k + 0x3C);
        } else if (p) {
            ApplyMatrixSV(p + 0x20, v, v);
            U16(v) += U16(p + 0x40); U16(v + 2) += U16(p + 0x42); U16(v + 4) += U16(p + 0x44);
        }
        if (Pos_Hidden(v)) U32(k + 0x50) |= 2;
        else U32(k + 0x50) &= ~2u;
    }
    Tmp_Pop(8);
}

/* ---- the plain sprite slot (0x448900): steps 0x7216E8 ---- */

void DC2CALL K2_Handler(gptr k)                                     /* 0x448900 */
{
    gptr s = U32(k + 0x8C);
    Orig_Call1(U32(0x7215E4 + U8(k + 0x7B) * 4), k);
    Orig_Call1(U32(0x7216E8 + U8(k + 0x54) * 4), k);
    if (U8(k + 0x7A) == 0xF && U32(k + 0x60) && !(U8(U32(k + 0x60) + 0x50) & 1)) {
        K2_Unlink(k);
        if (!s) goto vis;
        if (U8(s + 1) == 1) U8(s + 0x18)--;
    } else K2_SyncParent(k);
    if (s && !((U8(k + 0x84) & 8) + (U8(s) & 1)) && U8(s + 1) == 1) K2_Unlink(k);
vis:
    if (U8(k + 0x50) & 4) K2_UpdateHidden(k);
    if (U8(k + 0x46) && !--U8(k + 0x46)) K2_Detach(k);
}

/* its kind (+0x7A, 8 bytes at 0x721668: drawing bits, size, palette, animation) */
void DC2CALL K2_Init(gptr k)                                        /* 0x4489B0 */
{
    gptr s = U32(k + 0x8C), r = Tmp_Push(8);
    U32(r) = 0; U32(r + 4) = 0;
    U8(k + 0x54)++;
    RotMatrix(r, k + 0x20);
    Tmp_Pop(8);
    u32 kind = U8(k + 0x7A);
    K2_SetAnim(k, U32(0x72166C + kind * 8));
    U32(k + 0x50) |= 2;
    U8(k + 0x67) |= U8(0x721668 + U8(k + 0x7A) * 8);
    if ((U8(k + 0x84) & 8) && s && U8(s + 1) == 1) U8(s + 0x18)--;
    kind = U8(k + 0x7A);
    u32 sel = kind > 0xF ? 5 : (u8[]){0, 0, 5, 5, 1, 5, 5, 5, 5, 5, 5, 2, 2, 3, 3, 4}[kind];
    switch (sel) {
    case 0:
        U16(k + 0x6A) = (u16)(U8(0x721669 + kind * 8) << 5 | 0xA);
        U16(k + 0x68) = (u16)(U8(0x72166A + kind * 8) + 0x7E08);
        K2_PushFront(k);
        return;
    case 1:
        return;
    case 3: {
        gptr p = U32(k + 0x60);
        if (p && !(U8(p + 0x50) & 1)) { K2_Unlink(k); k2_src_dec(s); return; }
        U32(k + 0x50) |= 4;
        U16(k + 0x6A) = (u16)(U8(0x721669 + kind * 8) << 5 | 0xA);
        U16(k + 0x68) = (u16)(U8(0x72166A + kind * 8) + 0x7E08);
        return; }
    case 4:
        U8(k + 0x54) = 3;
        U32(k + 0xA8) = K2_OwnerObj(k);
        /* fall through */
    case 2:
        K2_PushFront(k);
        /* fall through */
    default:
        kind = U8(k + 0x7A);
        U16(k + 0x6A) = (u16)(U8(0x721669 + kind * 8) << 5 | 0xB);
        U16(k + 0x68) = (u16)(U8(0x72166A + kind * 8) + 0x7DC0);
        return;
    }
}

/* moves by its speed (+0x7C..+0x80), the last position kept in +0x74;
   the next step once its animation says so */
void DC2CALL K2_Move(gptr k)                                        /* 0x448B70 */
{
    u32 old = U32(k + 0x40);
    gptr s = U32(k + 0x8C);
    U16(k + 0x40) += U16(k + 0x7C);
    U16(k + 0x42) += U16(k + 0x7E);
    U32(k + 0x74) = old;
    u16 z = U16(k + 0x44);
    U16(k + 0x78) = z;
    U16(k + 0x44) = (u16)(U16(k + 0x80) + z);
    if (U8(k + 0x84) & 0x10) return;
    if (!K2_AnimStep(k) || (U8(k + 0x84) & 8)) return;
    U8(k + 0x54)++;
    k2_src_dec(s);
}

/* fading out after +0x98 frames: darker, smaller, then gone */
void DC2CALL K2_FadeOut(gptr k)                                     /* 0x448BE0 */
{
    U16(k + 0x98)--;
    if (S16(k + 0x98) < 0) {
        U8(k + 0x65) -= 2;
        U8(k + 0x64) -= 2;
        U8(k + 0x66) -= 2;
        if (!U8(k + 0x64)) K2_Unlink(k);
        if (U16(k + 0x6C)) U16(k + 0x6C)--;
        if (U16(k + 0x6E)) U16(k + 0x6E)--;
        if ((u32)U16(k + 0x6E) * U16(k + 0x6C) == 0) K2_Unlink(k);
    }
    K2_AnimStep(k);
}

/* ---- splashes and emitters (0x448C70): steps 0x72170C ---- */

void DC2CALL K2_Handler2(gptr k)                                    /* 0x448C70 */
{
    gptr s = U32(k + 0x8C);
    Orig_Call1(U32(0x7215E4 + U8(k + 0x7B) * 4), k);
    Orig_Call1(U32(0x72170C + U8(k + 0x54) * 4), k);
    K2_SyncParent(k);
    if (s && !(U8(s) & 1)) K2_Unlink(k);
}

/* a splash drop: thrown up at random; +0xA2 bit 0 only stores its sprite
   (0x721708) for the next ones */
void DC2CALL K2_SplashInit(gptr k)                                  /* 0x448CC0 */
{
    gptr s = U32(k + 0x8C);
    if (U8(k + 0xA2) & 1) {
        U16(0x721708) = U16(k + 0x9A);
        U16(0x72170A) = U16(k + 0x98);
        K2_Unlink(k);
        return;
    }
    gptr r = Tmp_Push(8);
    U32(r) = 0; U32(r + 4) = 0;
    U8(k + 0x54)++;
    RotMatrix(r, k + 0x20);
    Tmp_Pop(8);
    K2_SetAnim(k, U32(0x7216FC + U8(k + 0x7A) * 8));
    U32(k + 0x50) |= 2;
    U8(k + 0x67) |= U8(0x7216F8 + U8(k + 0x7A) * 8);
    if (s && (U8(k + 0x84) & 8)) U8(s + 0x18)--;
    U16(k + 0x6A) = U16(0x721708);
    U16(k + 0x68) = U16(0x72170A);
    U16(k + 0x7C) += (u16)(Rand8() - 0x80);
    U16(k + 0x7E) += (u16)(Rand8() - 0x80);
    u8 z = Rand8();
    U8(k + 0x7B) = 3;
    U16(k + 0x80) += (u16)(z - 0x80);
    U16(k + 0x98) = 0; U16(k + 0x9C) = 0; U16(k + 0x9A) = 0x28;
    U16(k + 0x9E) = (u16)(Rand8() + 0x80);
    U16(k + 0xA4) = U16(k + 0x42);
}

/* falling (+0x55: 0 falls, then floats one way or the other) */
void DC2CALL K2_SplashFall(gptr k)                                  /* 0x448E10 */
{
    U16(k + 0x9E) -= 0x14;
    s16 sp = S16(k + 0x9E);
    U16(k + 0x9A)--;
    if (sp < 0x1E) U16(k + 0x9E) = 0x1E;
    if (S16(k + 0x7E) <= 0x28) return;
    U16(k + 0x48) &= 0xFFF;
    S16(k + 0x7C) >>= 1;
    S16(k + 0x80) >>= 1;
    U8(k + 0x7B) = 0;
    U16(k + 0x7E) = 0x50;
    U8(k + 0x55)++;
    if (S16(k + 0x48) > 0x800) U8(k + 0x55)++;
    U16(k + 0x98) = 0x30;
    U16(k + 0x9A) = Rand8();
    U16(k + 0x9E) = 0;
    U16(k + 0x9C) = Rand8();
}

/* floating down, swaying (dir 1: +, -1: -; tilt between lo and hi) */
static void k2_float(gptr k, int dir, s16 lo, s16 hi)
{
    gptr s = U32(k + 0x8C);
    U16(k + 0x7C) = (u16)((s32)(s16)(U16(k + 0x7C) << 1) / 3);
    U16(k + 0x80) = (u16)((s32)(s16)(U16(k + 0x80) << 1) / 3);
    U16(k + 0x40) += (u16)(dir * (rsin(S16(k + 0x9A)) >> 8));
    U16(k + 0x44) += (u16)(dir * (rcos(S16(k + 0x9C)) >> 8));
    U16(k + 0x9A) = (u16)((U16(k + 0x9A) + (Rand8() >> 3) + 0x10) & 0xFFF);
    U16(k + 0x9C) = (u16)(((Rand8() >> 3) + U16(k + 0x9C) + 0x10) & 0xFFF);
    U16(k + 0x48) += U16(k + 0x98);
    if (S16(k + 0x48) < lo) U16(k + 0x98) = 0x30;
    else if (S16(k + 0x48) > hi) U16(k + 0x98) = 0xFFD0;
    if (S16(k + 0x42) > S16(k + 0xA4)) {
        U8(k + 0x54)++;
        if (s) U8(s + 0x18)--;
    }
}

void DC2CALL K2_FloatA(gptr k) { k2_float(k, 1, 0x300, 0x500); }  /* 0x448EB0 */
void DC2CALL K2_FloatB(gptr k) { k2_float(k, -1, 0xB00, 0xD00); } /* 0x448FC0 */

/* moves, spins on x, then its fall/float step (+0x55, 0x721718) */
void DC2CALL K2_Splash(gptr k)                                      /* 0x4490D0 */
{
    u32 old = U32(k + 0x40);
    U16(k + 0x40) += U16(k + 0x7C);
    U16(k + 0x42) += U16(k + 0x7E);
    U32(k + 0x74) = old;
    u16 z = U16(k + 0x44);
    U16(k + 0x78) = z;
    U16(k + 0x44) = (u16)(U16(k + 0x80) + z);
    U16(k + 0x48) += U16(k + 0x9E);
    Orig_Call1(U32(0x721718 + U8(k + 0x55) * 4), k);
}

/* ---- emitters: records of 0x28 (+0xF the time they come out) ---- */

void DC2CALL K2_Emitter(gptr k)                                     /* 0x449120 */
{
    Orig_Call1(U32(0x721724 + U8(k + 0x54) * 4), k);
}

void DC2CALL K2_EmitterStart(gptr k)                                /* 0x449140 */
{
    U8(k + 0xA2) = 0; U8(k + 0xA4) = 0;
    U8(k + 0xA3) = U8(k + 0xA1);
    U8(k) = 1;
    U8(k + 0x54)++;
    K2_EmitterStep(k);
}

/* the records of this time (+0xA2) into slots (+0xA3 the next one, or from
   the back when +0x7A bit 0); +0x7A bit 3 loops */
void DC2CALL K2_EmitterStep(gptr k)                                 /* 0x449180 */
{
    gptr rec = U32(k + 0x9C), base = U32(k + 0x98);
    s32 t = S8(k + 0xA2);
    if ((s32)U8(rec + 0xF) > t) goto next_time;
    for (;;) {
        if (U32(rec) == 0xFFFFFFFF) {
            if (!(U8(k + 0x7A) & 8)) { U8(k + 0x54)++; return; }
            u8 a1 = U8(k + 0xA1);
            U32(k + 0x9C) += (u32)((s32)a1 - (s32)U8(k + 0xA3)) * 0x28;
            U8(k + 0xA3) = a1;
            U8(k + 0xA2) = 0;
            return;
        }
        if ((s32)U8(rec + 0xF) == t) {
            gptr n;
            if (U8(k + 0x7A) & 1) {
                n = K2_AllocBack();
                if (!n) goto next_time;
                U8(k + 0xA4)++; U8(k + 0xA3)++;
            } else {
                n = K2(U8(k + 0xA3));
                if (U8(n + 0x50) & 1) goto advance;
                memset(MEM(n), 0, 0xAC);
                U32(n + 0x50) = 3;
                U8(n + 0x67) = 0x2C;
                U8(k + 0xA4)++; U8(k + 0xA3)++;
                if (U8(k + 0xA3) >= U32(0x87DC88)) { U8(k + 0x54)++; return; }
            }
            U8(n + 0x58) = U8(rec);
            U16(n + 0x6C) = U8(rec + 1); U16(n + 0x6E) = U8(rec + 2);
            U16(n + 0x64) = U16(rec + 4); U8(n + 0x66) = U8(rec + 6);
            U32(n + 0x40) = U32(rec + 8); U32(n + 0x44) = U32(rec + 0xC);
            U32(n + 0x7C) = U32(rec + 0x10); U32(n + 0x80) = U32(rec + 0x14);
            U16(n + 0x7A) = U16(rec + 0x1C);
            U8(n + 0x84) = U8(rec + 3);
            U32(n + 0x98) = U32(rec + 0x20); U32(n + 0x9C) = U32(rec + 0x24);
            if (U8(n + 0x84) & 0x40) U8(n + 0x67) |= 2;
            u8 at = U8(rec + 0x1E) >> 4, f = U8(k + 0x7A);
            u32 anim = U32(rec + 0x18);
            if (f & 2) {
                at = 1;
                U16(n + 0x40) += U16(k + 0xA6); U16(n + 0x42) += U16(k + 0xA8); U16(n + 0x44) += U16(k + 0xAA);
            } else if (f & 4) {
                U16(n + 0x64) = U16(k + 0xA8); U8(n + 0x66) = U8(k + 0xAA);
            } else if (U16(k + 0xA8)) anim = U32(k + 0xA8);
            U32(n + 0x68) = anim;
            U32(n + 0x70) = U16(rec + 0x16) + base;
            if (at & 4) {
                s32 i = S8(k + 0xA0);
                gptr e = U32(G() + 0x93C);
                if (!(U8(e + i * 0x70 + 0x590) & 1)) { K2_Unlink(n); U8(k + 0x54)++; return; }
                U32(n + 0x60) = e + (i + 0xC) * 0x70;
            } else if (!at) {
                gptr o = U32(G() + 0x940) + S8(k + 0xA0) * 0x260;
                if (!(U8(o + 0x50) & 1)) { K2_Unlink(n); U8(k + 0x54)++; return; }
                U32(n + 0x60) = Obj_Part(U32(o + 0x64), U8(rec + 0x1F));
            }
        }
    advance:
        rec += 0x28;
        U32(k + 0x9C) += 0x28;
        t = S8(k + 0xA2);
        if ((s32)U8(rec + 0xF) > t) break;
    }
next_time:
    U8(k + 0xA2)++;
    if (U32(U32(k + 0x9C)) != 0xFFFFFFFF) return;
    if (!(U8(k + 0x7A) & 8)) { U8(k + 0x54)++; return; }
    {
        u8 a1 = U8(k + 0xA1);
        U32(k + 0x9C) += (u32)((s32)a1 - (s32)U8(k + 0xA3)) * 0x28;
        U8(k + 0xA3) = a1;
        U8(k + 0xA2) = 0xFF;
    }
}

/* the other emitter (0x4494C0): steps 0x721730; +0xA4 its options */
void DC2CALL K2_Emitter2(gptr k)                                    /* 0x4494C0 */
{
    Orig_Call1(U32(0x721730 + U8(k + 0x54) * 4), k);
    if (U32(U32(k + 0x98)) != 0xFFFFFFFF) return;
    if (!(U8(k + 0xA4) & 8)) { U8(k + 0x54) = 3; return; }
    u8 c = U8(k + 0x9C);
    U32(k + 0x98) += (u32)((s32)c - (s32)U8(k + 0x9F)) * 0x28;
    U8(k + 0x9F) = c;
    U8(k + 0x9E) = 0xFF;
}

/* where it starts in its list (+0x9C), and what it follows (+0xA0) */
void DC2CALL K2_Emitter2Start(gptr k)                               /* 0x449530 */
{
    u32 f = U32(k + 0xA4);
    if (f & 1) U8(k + 0x9E) = U8(U32(k + 0x98) + 0xF);
    else if (f & 0x100) {
        u8 c = U8(k + 0x9C);
        U8(k + 0x9C) = 0;
        U8(k + 0x9E) = c;
        while ((s32)U8(U32(k + 0x98) + 0xF) < (s32)(s8)c) {
            U32(k + 0x98) += 0x28;
            U8(k + 0x9C)++;
            if (U32(U32(k + 0x98)) == 0xFFFFFFFF) { U8(k + 0x54) = 3; return; }
        }
    } else {
        U8(k + 0x9E) = 0;
        U32(k + 0x98) += U8(k + 0x9C) * 0x28;
    }
    U8(k + 0x9F) = U8(k + 0x9C);
    if (U8(k + 0x9C) >= U8(k + 0x9D)) U8(k + 0x9D) = 0xFF;
    if (f & 0x20) {
        gptr p = U32(k + 0xA0);
        U32(k + 0x40) = U32(p); U32(k + 0x44) = U32(p + 4);
        U32(k + 0xA0) = 0;
    } else if (f & 0x200) {
        gptr p = U32(k + 0xA0);
        memmove(MEM(k + 0x20), MEM(p + 0x20), 0x20);
        U32(k + 0x34) = (u32)(s32)S16(p + 0x40);
        U32(k + 0x38) = (u32)(s32)S16(p + 0x42);
        U32(k + 0x3C) = (u32)(s32)S16(p + 0x44);
    }
    U8(k + 0x54)++;
    K2_Emitter2Step(k);
}

void DC2CALL K2_StepNext(gptr k)                                    /* 0x449650 */
{
    U8(k + 0x54)++;
}

/* the records of this time (+0x9E) between +0x9C and +0x9D into slots from
   the back, following +0xA0 the way +0xA4 says */
void DC2CALL K2_Emitter2Step(gptr k)                                /* 0x449660 */
{
    gptr par = U32(k + 0xA0), rec = U32(k + 0x98);
    if (par && !(U8(par + 0x50) & 1)) { U8(k + 0x54) = 3; return; }
    s32 t = S8(k + 0x9E);
    if ((s32)U8(rec + 0xF) > t) { U8(k + 0x9E)++; return; }
    for (;;) {
        if (U32(rec) == 0xFFFFFFFF) {
            if (U8(k + 0xA4) & 8) {
                u8 c = U8(k + 0x9C);
                U32(k + 0x98) += (u32)((s32)c - (s32)U8(k + 0x9F)) * 0x28;
                U8(k + 0x9F) = c;
                U8(k + 0x9E) = 0;
            } else {
                U8(k + 0x54) = 3;
                U8(k + 0x9E)++;
            }
            return;
        }
        if ((s32)U8(rec + 0xF) == t) {
            if (U8(k + 0x9C) <= U8(k + 0x9F) && U8(k + 0x9D) >= U8(k + 0x9F)) {
                gptr n = K2_AllocBack();
                if (!n) return;
                U8(n + 0x58) = U8(rec);
                U8(k + 0x7A) = U8(rec) == 1 || U8(rec) == 2;
                if (U8(k + 0xA4) & 0x80) U32(n + 0x50) |= 4;
                U32(n + 0x64) = (U8(k + 0xA4) & 0x40) ? U32(k + 0xA8) : U32(rec + 4);
                U8(n + 0x67) = 0x2C;
                U32(n + 0x40) = U32(rec + 8); U32(n + 0x44) = U32(rec + 0xC);
                U32(n + 0x7C) = U32(rec + 0x10); U32(n + 0x80) = U32(rec + 0x14);
                U16(n + 0x7A) = U16(rec + 0x1C);
                U8(n + 0x84) = U8(rec + 3);
                if (U16(k + 0xA8)) U32(n + 0x68) = U32(k + 0xA8);
                u32 f = U32(k + 0xA4);
                if (f & 2) U32(n + 0x60) = U32(k + 0xA0);
                else if (f & 4) {
                    gptr p = U32(k + 0xA0);
                    U16(n + 0x40) += U16(p + 0x40); U16(n + 0x42) += U16(p + 0x42); U16(n + 0x44) += U16(p + 0x44);
                } else if (f & 0x20) {
                    U32(n + 0x34) = (u32)(s32)S16(k + 0x40);
                    U32(n + 0x38) = (u32)(s32)S16(k + 0x42);
                    U32(n + 0x3C) = (u32)(s32)S16(k + 0x44);
                    U16(n + 0xA0) = U16(k + 0x46);
                    U32(n + 0x60) = n;
                } else if (f & 0x200) {
                    memmove(MEM(n + 0x20), MEM(k + 0x20), 0x20);
                    U32(n + 0x60) = n;
                } else U32(n + 0x60) = Obj_Part(U32(U32(k + 0xA0) + 0x64), U8(rec + 0x1F));
                if (U32(n + 0x60) && !(U32(k + 0xA4) & 0x222)) {
                    s32 sc = (s32)U32(U32(k + 0xA0) + 0x6C);
                    U16(n + 0x6C) = (u16)((s32)((u32)U8(rec + 1) * (u32)sc) / 4096);
                    U16(n + 0x6E) = (u16)((s32)((u32)U8(rec + 2) * (u32)sc) / 4096);
                } else {
                    U16(n + 0x6C) = U8(rec + 1);
                    U16(n + 0x6E) = U8(rec + 2);
                }
                U32(n + 0x98) = U32(rec + 0x20); U32(n + 0x9C) = U32(rec + 0x24);
            }
            U8(k + 0x9F)++;
        }
        rec += 0x28;
        U32(k + 0x98) += 0x28;
        t = S8(k + 0x9E);
        if ((s32)U8(rec + 0xF) > t) break;
    }
    U8(k + 0x9E)++;
}

/* ---- a slot that runs a falling-drops show (*0x87F948 [18], steps 0x7219E8) and
   one sprite stuck to an effect (*0x87F948 [19], steps 0x721A40) ------------------- */

/* step 0: gone once flag 8:+0xA2 is set, else on with animation +0x98 */
void DC2CALL K2Drops_Start(gptr k)                                  /* 0x44B4C0 */
{
    if (Flag_Check(8, U16(k + 0xA2))) { K2_Unlink(k); return; }
    U8(k + 0x54)++;
    U16(k + 0x6A) = 0x18; U16(k + 0x68) = 0x7E40;
    U16(k + 0x6E) = 0xA; U16(k + 0x6C) = 0xA;
    U16(k + 0x4A) = U16(k + 0xA0);
    U8(k + 0x84) |= 1;
    U32(k + 0x50) |= 2;
    K2_SetAnim(k, U32(k + 0x98));
}

/* step 2: back to step 1, on animation +0x9C */
void DC2CALL K2Drops_Back(gptr k)                                   /* 0x44B540 */
{
    U16(k + 0x6A) = 0x18; U16(k + 0x68) = 0x7E40;
    U8(k + 0x54)--;
    U16(k + 0x6E) = 0xA; U16(k + 0x6C) = 0xA;
    U16(k + 0x4A) = U16(k + 0xA0);
    U32(k + 0x50) |= 2;
    U8(k + 0x84) |= 1;
    K2_SetAnim(k, U32(k + 0x9C));
}

/* a drop anew: somewhere in front of the camera, high up, white */
void DC2CALL K2Drops_Spawn(gptr pos, gptr col)                      /* 0x44B660 */
{
    gptr t = Tmp_Push(0x30), eye = t, v = t + 8, m = t + 0x10;
    Camera_LookAt(G() + 0x7A0, G() + 0x7A8, G() + 0x798, eye);
    U16(v) = (u16)((Rand8() - 0x80) << 5);
    U16(v + 2) = (u16)((Rand8() - 0x80) << 5);
    u32 a = Rand8(), b = Rand8();
    U16(v + 4) = (u16)((b + a * 4) << 4);
    RotMatrixYXZ(G() + 0x798, m);
    ApplyMatrixSV(m, v, pos);
    U16(pos) += U16(eye); U16(pos + 4) += U16(eye + 4);
    U16(pos + 2) = 0xD8F0;
    U8(col + 2) = 0x80; U8(col + 1) = 0x80; U8(col) = 0x80;
    Tmp_Pop(0x30);
}

/* step 4: four drops (the second's colour is the slot's own step bytes) */
void DC2CALL K2Drops_Init(gptr k)                                   /* 0x44B5A0 */
{
    U8(k + 0x57)++;
    U32(k + 0x50) &= ~2u;
    K2Drops_Spawn(k + 0x40, k + 0x64); U16(k + 0x42) += (u16)(Rand8() << 5);
    K2Drops_Spawn(k + 0x48, k + 0x54); U16(k + 0x4A) += (u16)(Rand8() << 5);
    K2Drops_Spawn(k + 0x7C, k + 0x68); U16(k + 0x7E) += (u16)(Rand8() << 5);
    K2Drops_Spawn(k + 0x6C, k + 0x70); U16(k + 0x6E) += (u16)(Rand8() << 5);
    U16(k + 0x98) = Rand8() & 0xFFF;
    U16(k + 0x9A) = Rand8() & 0xFFF;
}

/* a drop sways down to the player's height, then fades there; drawn as a tile
   sized by its depth, anew once off screen or too far */
void DC2CALL K2Drops_Drop(gptr k, gptr pos, gptr col, gptr ang)     /* 0x44B790 */
{
    s16 py = S16(U32(G() + 0x940) + 0x42);
    if (S16(pos + 2) < py) {
        U16(ang) = (u16)((U16(ang) + (Rand8() & 0x1F) + 0x14) & 0xFFF);
        U16(pos) += (u16)(rsin(S16(ang)) / 512);
        U16(pos + 2) += (u16)((Rand8() & 0x2F) + 0x1F);
        U16(pos + 4) += (u16)(rsin(S16(ang)) / 512);
    } else {
        U16(pos + 2) = (u16)py;
        U8(col + 1) -= 8; U8(col) -= 8; U8(col + 2) -= 8;
        if (U8(col) < 8) { K2Drops_Spawn(pos, col); return; }
    }
    u32 sh = (U8(G() + 0xB4B) - 2) & 31;
    SetRotMatrix(G() + 0x778);
    gptr t = Tmp_Push(0x10);
    s32 z = RotTransPers(pos, t, t + 8, t + 8) >> sh;
    s32 sz = (300 - z) / 70;
    float sx = F32(t);
    u32 sxy0 = U32(t), sxy1 = U32(t + 4);
    Tmp_Pop(0x10);
    if (z <= 300 && (s16)sz >= 1) {
        gptr p = U32(0x87DC98);
        U8(p + 4) = U8(col); U8(p + 5) = U8(col + 1); U8(p + 6) = U8(col + 2); U8(p + 7) = 0x62;
        U32(p + 8) = sxy0; U32(p + 0xC) = sxy1; U32(p + 0x10) = 0x3C23D70A;   /* 0.01f */
        U16(p + 0x14) = U16(p + 0x16) = (u16)(sz + 2);
        gptr ot = 0x87BC00 + 4 * ((U32(0x87DE0C) << 10) + (u32)z);
        U32(p) = U32(ot) | 0x3000000; U32(ot) = p & 0xFFFFFF;
        U32(0x87DC98) += 0x18;
        u32 tp = Gpu_GetTPage(0, 1, 0x140, 0x100) & 0xFFFF;
        Gpu_SetDrawMode(U32(0x87DC98), 1, 0, tp, 0);
        Gpu_AddPrim(U32(0x87F94C) + 0x74, U32(0x87DC98));
        U32(0x87DC98) += 0xC;
        if (sx >= 0.0f && !(sx > 320.0f)) return;
    }
    u16 y = U16(pos + 2);
    K2Drops_Spawn(pos, col);
    U16(pos + 2) = y;
}

/* step 5: the four drops */
void DC2CALL K2Drops_Run(gptr k)                                    /* 0x44B730 */
{
    K2Drops_Drop(k, k + 0x40, k + 0x64, k + 0x98);
    K2Drops_Drop(k, k + 0x48, k + 0x54, k + 0x9A);
    K2Drops_Drop(k, k + 0x7C, k + 0x68, k + 0x98);
    K2Drops_Drop(k, k + 0x6C, k + 0x70, k + 0x9A);
}

/* how many bubbles by the player's distance from (-0x3CF0, 0x36B0) */
static s32 k2_bubbles_dist(void)
{
    gptr pl = U32(G() + 0x940);
    s32 dx = S16(pl + 0x40) + 0x3CF0, dz = S16(pl + 0x44) - 0x36B0;
    return SquareRoot0((s32)((u32)dz * (u32)dz + (u32)dx * (u32)dx));
}

/* step 7: twenty bubble slots (type 10), the nearer the more of them on */
void DC2CALL K2Bubbles_Start(gptr k)                                /* 0x44B9E0 */
{
    U8(k + 0x54)++;
    s32 n = 20 - k2_bubbles_dist() / 20;
    for (s32 i = 1; i <= 20; i++) {
        gptr s = K2_AllocBack();
        U8(s + 0x58) = 0xA;
        U8(s + 0x64) = Rand8() & 7;
        U8(s + 0x65) = (u8)((Rand8() & 0x1F) + 0x70);
        U8(s + 0x66) = (u8)((Rand8() & 0x1F) + 0x70);
        if (i > n) U8(s + 0x84) = 0;
    }
}

/* step 8: switch on idle bubble slots up to the count for the distance (3 at least) */
void DC2CALL K2Bubbles_Keep(gptr k)                                 /* 0x44BA90 */
{
    s32 n = 20 - (s32)((u32)k2_bubbles_dist() * 20) / 40000, on = 0;
    if (n < 3) n = 3;
    gptr s = U32(G() + 0x944);
    u32 cnt = U32(0x87DC88);
    for (u32 i = 0; i < cnt; i++)
        if (U8(s + i * 0xAC + 0x58) == 0xA && U32(s + i * 0xAC + 0x50)) on++;
    for (u32 i = 0; i < U32(0x87DC88); i++) {
        gptr e = s + i * 0xAC;
        if (on < n && U8(e + 0x58) == 0xA && !U32(e + 0x50)) {
            U32(e + 0x50) = 1; U8(e + 0x54) = 0; on++;
        }
    }
}

/* step 9: a bubble: random heading, rising speed, size and time */
void DC2CALL K2Bubble_Init(gptr k)                                  /* 0x44BB60 */
{
    U32(k + 0x50) |= 2;
    U8(k + 0x54)++;
    K2_SetAnim(k, U32(k + 0x70));
    gptr t = Tmp_Push(0x30), ang = t, v = t + 8, m = t + 0x10;
    u32 r = Rand8() & 0x1F;
    U16(v + 2) = 0; U16(v + 4) = (u16)(r + 0xF0); U16(v) = 0;
    u32 r2 = Rand8();
    U16(ang + 2) = (u16)(r2 << 4); U16(ang + 4) = 0; U16(ang) = 0;
    RotMatrix(ang, m);
    ApplyMatrixSV(m, v, k + 0x7C);
    U16(k + 0x9C) = 0;
    U16(k + 0x98) = 0xFB00;
    U8(k + 0xA4) = S16(k + 0x7C) >= 0 ? 1 : 0xFF;
    U16(k + 0x9A) = (u16)(U16(ang + 2) + 0x800);
    U8(k + 0xA5) = S16(k + 0x80) >= 0 ? 1 : 0xFF;
    RotMatrixYXZ(k + 0x98, m);
    K2_MatrixAngles(m, k + 0x48);
    Tmp_Pop(0x30);
    U16(k + 0x7E) = (u16)(-600 - (s32)(Rand8() & 0x7F));
    U16(k + 0xA2) = (u16)((Rand8() & 0x3F) + 0x64);
    U16(k + 0xA0) = U16(k + 0x42);
    U16(k + 0x46) = (u16)((Rand8() & 0x3F) + 0x1E);
    U16(k + 0x9E) = (u16)((Rand8() & 7) + 8);
    U8(k + 0x56) = (u8)((Rand8() & 7) + 8);
    U8(k + 0x84) = 1;
    U32(k + 0x60) = G() + 0x778;
    U8(k + 0x7B) = Rand8() & 3;
}

/* step 10: up, slowing; wobbling out sideways */
void DC2CALL K2Bubble_Rise(gptr k)                                  /* 0x44BCE0 */
{
    u16 vy = U16(k + 0x7E);
    U16(k + 0x40) += U16(k + 0x7C); U16(k + 0x42) += vy; U16(k + 0x44) += U16(k + 0x80);
    U16(k + 0x7E) = (u16)(U16(k + 0xA2) + vy);
    U16(k + 0x7C) += (u16)((Rand8() & 7) * S8(k + 0xA4));
    U16(k + 0x80) += (u16)((Rand8() & 7) * S8(k + 0xA5));
    if (S16(k + 0x7E) > 0) { U8(k + 0x54)++; U16(k + 0xA2) = (u16)(Rand8() << 5); }
    K2_AnimStep(k);
}

/* step 11: down, swaying, to where it started or until its time is up */
void DC2CALL K2Bubble_Sink(gptr k)                                  /* 0x44BD70 */
{
    U16(k + 0xA2) = (u16)((U16(k + 0xA2) + (Rand8() & 0x1F) + 0x14) & 0xFFF);
    U16(k + 0x40) += (u16)(rsin(S16(k + 0xA2)) / 256);
    U16(k + 0x44) += (u16)(rsin(S16(k + 0xA2)) / 256);
    U16(k + 0x42) += (u16)((Rand8() & 0x1F) * 2);
    K2_AnimStep(k);
    if (S16(k + 0x42) > S16(k + 0xA0) || !U16(k + 0x46)) U8(k + 0x54)++;
}

/* step 12: on once +0x46 went below 0 */
void DC2CALL K2Bubble_Wait(gptr k)                                  /* 0x44BE00 */
{
    if (S16(k + 0x46) < 0) U8(k + 0x54)++;
}

/* step 13: shrinking while +0x55 */
void DC2CALL K2Bubble_Shrink(gptr k)                                /* 0x44BE10 */
{
    if (!U8(k + 0x55)) return;
    U16(k + 0x6E)--; U16(k + 0x6C)--;
    if (!U16(k + 0x6C)) U8(k + 0x54)++;
}

void DC2CALL K2_Drops(gptr k)                                       /* 0x44B4A0 */
{
    Orig_Call1(U32(0x7219E8 + U8(k + 0x54) * 4), k);
    K2_UpdateHidden(k);
}

/* where the sprite is: its offset from the effect +0x98 (unless it moves itself) */
void DC2CALL K2Stuck_Place(gptr k)                                  /* 0x44BEF0 */
{
    gptr e = U32(k + 0x98);
    if (U8(k + 0x7A) || !e) return;
    U16(k + 0x40) = (u16)(U16(k + 0x9C) + U16(e + 0xC));
    U16(k + 0x42) = (u16)(U16(e + 0xE) + U16(k + 0x9E));
}

/* step 0: stuck to the first effect of kind 0x14 (only for characters 0 and 2) */
void DC2CALL K2Stuck_Init(gptr k)                                   /* 0x44BE30 */
{
    U32(k + 0x98) = 0;
    u8 c = U8(G() + 0x1091);
    if (c == 0 || c == 2) {
        gptr e = U32(G() + 0x948), end = e + U32(0x87DE14) * 0x34;
        for (; e < end; e += 0x34)
            if ((U8(e) & 1) && U8(e + 1) == 0x14) { U32(k + 0x98) = e; break; }
    }
    U16(k + 0x9C) = U16(k + 0x40); U16(k + 0x9E) = U16(k + 0x42); U16(k + 0xA0) = U16(k + 0x68);
    K2_SetAnim(k, U32(k + 0x70));
    U32(k + 0x50) |= 2;
    U8(k + 0x84) |= 0x80;
    if (U8(k + 0x84) & 0x40) U8(k + 0x67) |= 2;
    K2Stuck_Place(k);
    U8(k + 0x54)++;
}

/* step 1: animated; in mode 2 its +0x68 pulses with the frame (0x721A24) */
void DC2CALL K2Stuck_Run(gptr k)                                    /* 0x44BF30 */
{
    K2_AnimStep(k);
    if (U8(k + 0x7A) == 2)
        U16(k + 0x68) = (u16)(U8(0x721A24 + ((U32(k + 0x94) - U32(k + 0x90)) >> 3)) + U16(k + 0xA0));
    K2Stuck_Place(k);
}

void DC2CALL K2_Stuck(gptr k)                                       /* 0x44BF80 */
{
    Orig_Call1(U32(0x721A40 + U8(k + 0x54) * 4), k);
}

/* ---- effects (G+0x948) that fill effect slots ------------------------------------- */

/* a list of slot records (+0x10, 0x28 each, +0xF the time each starts; ends at -1)
   spawns slots, from +0x17 on (or anywhere, flag 1), each following an effect
   (flag 4 of the record), the effect's own offset (flag 2) or an object's part
   (+0x14); +0x16 the time, +0x18 how many it has alive; flag 8 repeats */
void DC2CALL EffSlots_Step(gptr e)                                  /* 0x40EF20 */
{
    gptr rec = U32(e + 0x10), list = U32(e + 0xC);
    s32 t = S8(e + 0x16);
    if ((s32)U8(rec + 0xF) <= t) {
        for (;;) {
            if (U32(rec) == 0xFFFFFFFF) {
                if (!(U8(e + 2) & 8)) { U8(e + 3)++; return; }
                s32 n = (s32)U8(e + 0x15) - (s32)U8(e + 0x17);
                U8(e + 0x17) = U8(e + 0x15); U8(e + 0x16) = 0;
                U32(e + 0x10) += (u32)(n * 0x28);
                return;
            }
            if ((s32)U8(rec + 0xF) == t) {
                gptr s;
                if (U8(e + 2) & 1) {
                    s = K2_AllocBack();
                    if (!s) break;
                    U32(s + 0x8C) = e;
                    U8(e + 0x18)++; U8(e + 0x17)++;
                } else {
                    s = K2(U8(e + 0x17));
                    if (U8(s + 0x50) & 1) goto next;
                    memset(MEM(s), 0, 0xAC);
                    U32(s + 0x50) = 3; U8(s + 0x67) = 0x2C; U32(s + 0x8C) = e;
                    U8(s + 0x87) = U8(e + 0x17); U8(s + 0x86) = U8(e + 0x17);
                    U8(e + 0x18)++; U8(e + 0x17)++;
                    if (U8(e + 0x17) >= U32(0x87DC88)) { U8(e + 3)++; return; }
                }
                U8(s + 0x58) = U8(rec);
                U16(s + 0x6C) = U8(rec + 1); U16(s + 0x6E) = U8(rec + 2);
                U16(s + 0x64) = U16(rec + 4); U8(s + 0x66) = U8(rec + 6);
                U32(s + 0x40) = U32(rec + 8); U32(s + 0x44) = U32(rec + 0xC);
                U32(s + 0x7C) = U32(rec + 0x10); U32(s + 0x80) = U32(rec + 0x14);
                U16(s + 0x7A) = U16(rec + 0x1C); U8(s + 0x84) = U8(rec + 3);
                U32(s + 0x98) = U32(rec + 0x20); U32(s + 0x9C) = U32(rec + 0x24);
                if (U8(s + 0x84) & 0x40) U8(s + 0x67) |= 2;
                u8 f = U8(rec + 0x1E) >> 4, m = U8(e + 2);
                u32 c = U32(rec + 0x18);
                if (m & 2) {
                    f = 1;
                    U16(s + 0x40) += U16(e + 0x1A); U16(s + 0x42) += U16(e + 0x1C); U16(s + 0x44) += U16(e + 0x1E);
                } else if (m & 4) {
                    U16(s + 0x64) = U16(e + 0x1C); U8(s + 0x66) = U8(e + 0x1E);
                } else if (U16(e + 0x1C)) c = U32(e + 0x1C);
                U32(s + 0x68) = c;
                U32(s + 0x70) = U16(rec + 0x16) + list;
                if (f & 4) {
                    s32 i = S8(e + 0x14);
                    gptr ef = U32(G() + 0x93C);
                    if (!(U8(ef + (i + 0xC) * 0x70 + 0x50) & 1)) goto kill;
                    U32(s + 0x60) = ef + (i + 0xC) * 0x70;
                } else if (!f) {
                    gptr o = U32(G() + 0x940) + S8(e + 0x14) * 0x260;
                    if (!(U8(o + 0x50) & 1)) goto kill;
                    U32(s + 0x60) = Obj_Part(U32(o + 0x64), U8(rec + 0x1F));
                }
                goto next;
            kill:
                K2_Unlink(s);
                U8(e + 0x18)--;
                U8(e + 3)++;
                return;
            }
        next:
            U32(e + 0x10) += 0x28;
            rec += 0x28;
            t = S8(e + 0x16);
            if ((s32)U8(rec + 0xF) > t) break;
        }
    }
    U8(e + 0x16)++;
    if (U32(U32(e + 0x10)) != 0xFFFFFFFF) return;
    if (!(U8(e + 2) & 8)) { U8(e + 3)++; return; }
    s32 n = (s32)U8(e + 0x15) - (s32)U8(e + 0x17);
    U8(e + 0x17) = U8(e + 0x15); U8(e + 0x16) = 0xFF;
    U32(e + 0x10) += (u32)(n * 0x28);
}

void DC2CALL EffSlots_Start(gptr e)                                 /* 0x40EEF0 */
{
    U8(e + 0x16) = 0; U8(e + 0x18) = 0;
    U8(e + 0x17) = U8(e + 0x15);
    U8(e + 3)++;
    EffSlots_Step(e);
}

/* until none of its slots is left */
void DC2CALL EffSlots_Wait(gptr e)                                  /* 0x40F210 */
{
    if (!U8(e + 0x18)) U8(e + 3)++;
}

/* step 3: off */
void DC2CALL Eff_Off(gptr e)                                        /* 0x48D470 */
{
    U8(e) = 0;
}

void DC2CALL EffSlots(gptr e)                                       /* 0x40EED0 */
{
    Orig_Call1(U32(0x71E17C + S8(e + 3) * 4), e);
}

/* slots +0xC.. (+0xD of them) of the area's own type (0x71E75C), each given
   +0xE..+0x10; from then on run in step 2 */
void DC2CALL EffArea_Start(gptr e)                                  /* 0x4140E0 */
{
    if (U8(e + 3)) return;
    U8(e + 3) = 1;
    u32 off = U8(e + 0xC) * 0xAC;
    for (u32 n = U8(e + 0xD); n; n--, off += 0xAC) {
        gptr s = U32(G() + 0x944) + off;
        memset(MEM(s), 0, 0xAC);
        U32(s + 0x50) = 1;
        U8(s + 0x58) = U8(0x71E75C + U16(G() + 0x67C) * 4);
        U16(s + 0x98) = U8(e + 0xE); U16(s + 0x9A) = U8(e + 0xF); U16(s + 0x9C) = U8(e + 0x10);
    }
    U32(e + 4) = 0x4141A0;
    Flag_Set(3, 8, 1);
}

void DC2CALL EffArea_Run(gptr e)                                    /* 0x4141A0 */
{
    u32 off = U8(e + 0xC) * 0xAC;
    for (u32 n = U8(e + 0xD); n; n--, off += 0xAC) {
        gptr s = U32(G() + 0x944) + off;
        U8(s + 0x54) = 2;
        Orig_Call1(U32(U32(0x87F948) + U32(0x71E75C + U16(G() + 0x67C) * 4) * 4), s);
    }
}

/* ---- puffs (0x449F90): steps 0x721998; kind +0x7A (8 bytes at 0x721950: drawing
   bits, size, palette, animation) -------------------------------------------------- */

void DC2CALL K2Puff_Init(gptr k)                                    /* 0x449FD0 */
{
    U8(k + 0x54)++;
    gptr a = Tmp_Push(8);
    U32(a) = 0; U32(a + 4) = 0;
    RotMatrix(a, k + 0x20);
    Tmp_Pop(8);
    gptr t = 0x721950 + U8(k + 0x7A) * 8;
    U16(k + 0x6A) = (u16)((U8(t + 1) << 5) | 0xB);
    U16(k + 0x68) = (u16)(U8(t + 2) + 0x7DC0);
    K2_SetAnim(k, U32(t + 4));
    U32(k + 0x50) |= 2;
    U8(k + 0x67) |= U8(0x721950 + U8(k + 0x7A) * 8);
    U16(k + 0xA0) = 0x32;
    if (U8(k + 0x7A) == 5) {                        /* on the floor under a part of +0xA4 */
        U8(k + 0x7B) = 1;
        U8(k + 0x84) |= 1;
        U16(k + 0x9E) += 0x1E; U16(k + 0x98) = 0x400; U16(k + 0x9C) = 0; U16(k + 0x9A) = 0;
        gptr p = Obj_Part(U32(U32(k + 0xA4) + 0x64), 0);
        u16 sz = (u16)(U16(k + 0x6E) + (Rand8() & 3));
        U16(k + 0x6C) = sz; U16(k + 0x6E) = sz;
        U16(k + 0x40) = (u16)(U16(p + 0x40) - Rand8() * 2 + 0x100);
        U16(k + 0x44) = (u16)(U16(p + 0x44) - Rand8() * 2 + 0x100);
        U16(k + 0x42) = U16(p + 0x42);
        gptr v = Tmp_Push(8);
        U16(v) = U16(k + 0x40); U16(v + 2) = 0x3E8; U16(v + 4) = U16(k + 0x44);
        U16(k + 0x42) = (u16)Floor_Height(v, k + 0x40, 0, 0);
        Tmp_Pop(8);
        U8(k + 0x54)++;
    }
    K2_PushFront(k);
}

/* moving; kinds 3 and 4 shrink away after +0xA0 frames; gone at the animation's end */
void DC2CALL K2Puff_Move(gptr k)                                    /* 0x44A160 */
{
    U16(k + 0x40) += U16(k + 0x7C); U16(k + 0x42) += U16(k + 0x7E); U16(k + 0x44) += U16(k + 0x80);
    gptr s = U32(k + 0x8C);
    if (U8(k + 0x7A) == 3 || U8(k + 0x7A) == 4) {
        if (U16(k + 0xA0)) U16(k + 0xA0)--;
        else {
            if (U16(k + 0x6E)) U16(k + 0x6E)--;
            if (U16(k + 0x6C)) U16(k + 0x6C)--;
            else { k2_src_dec(s); K2_Unlink(k); }
        }
    }
    if (K2_AnimStep(k)) { k2_src_dec(s); K2_Unlink(k); }
}

/* +0x9E frames, then shrinks */
void DC2CALL K2Puff_Wait(gptr k)                                    /* 0x44A210 */
{
    K2_AnimStep(k);
    if (--U16(k + 0x9E)) return;
    U16(k + 0xA2) = (u16)(U16(k + 0x6E) << 4);
    U8(k + 0x54)++;
}

void DC2CALL K2Puff_Shrink(gptr k)                                  /* 0x44A250 */
{
    K2_AnimStep(k);
    U16(k + 0xA2)--;
    u16 s = (u16)(S16(k + 0xA2) >> 4);
    U16(k + 0x6C) = s; U16(k + 0x6E) = s;
    if (!s) U8(k + 0x54)++;
}

void DC2CALL K2_Puff(gptr k)                                        /* 0x449F90 */
{
    Orig_Call1(U32(0x7215E4 + U8(k + 0x7B) * 4), k);
    Orig_Call1(U32(0x721998 + U8(k + 0x54) * 4), k);
    K2_SyncParent(k);
    if (!--U8(k + 0x46)) K2_Detach(k);
}

/* ---- a glow at a fixed place (0x44A280): steps 0x7219AC ---------------------------- */

void DC2CALL K2Glow_Init(gptr k)                                    /* 0x44A2A0 */
{
    U16(k + 0x6A) = 0x2B;
    U8(k + 0x54)++;
    U16(k + 0x68) = 0x7DC3;
    K2_SetAnim(k, 0x681E48);
    U32(k + 0x50) |= 2;
    U8(k + 0x67) |= 2;
    U8(k + 0x84) |= 0x80;
    U16(k + 0x6C) = 1; U16(k + 0x6E) = 1;
    U32(k + 0x60) = 0;
}

void DC2CALL K2Glow_Dim(gptr k)                                     /* 0x44A300 */
{
    U8(k + 0x65) -= 8; U8(k + 0x64) -= 8; U8(k + 0x66) -= 8;
    if (!U8(k + 0x64)) U8(k + 0x54)++;
}

void DC2CALL K2Glow_Brighten(gptr k)                                /* 0x44A330 */
{
    U8(k + 0x65) += 0x10; U8(k + 0x64) += 0x10; U8(k + 0x66) += 0x10;
    if (U8(k + 0x64) > 0x50) U8(k + 0x54)--;
}

void DC2CALL K2Glow_Place(gptr k)                                   /* 0x44A360 */
{
    U16(k + 0x40) = 0xA3; U16(k + 0x42) = 0x7C; U16(k + 0x44) = 1;
}

void DC2CALL K2_Glow(gptr k)                                        /* 0x44A280 */
{
    Orig_Call1(U32(0x7219AC + U8(k + 0x54) * 4), k);
    K2Glow_Place(k);
}

/* ---- a sprite on its own (0x44A380): steps 0x7219B8 -------------------------------- */

void DC2CALL K2Lone_Init(gptr k)                                    /* 0x44A400 */
{
    gptr s = U32(k + 0x8C);
    U8(k + 0x54)++;
    gptr a = Tmp_Push(8);
    U32(a) = 0; U32(a + 4) = 0;
    RotMatrix(a, k + 0x20);
    Tmp_Pop(8);
    K2_SetAnim(k, U32(k + 0x70));
    U32(k + 0x50) |= 2;
    if (s && (U8(k + 0x84) & 8)) { U8(s + 0x18)--; U32(k + 0x8C) = 0; }
}

void DC2CALL K2Lone_Move(gptr k)                                    /* 0x44C0A0 */
{
    U16(k + 0x40) += U16(k + 0x7C); U16(k + 0x42) += U16(k + 0x7E); U16(k + 0x44) += U16(k + 0x80);
    if (K2_AnimStep(k) && !(U8(k + 0x84) & 8)) U8(k + 0x54)++;
}

void DC2CALL K2Lone_End(gptr k)                                     /* 0x44CAF0 */
{
    gptr s = U32(k + 0x8C);
    K2_Unlink(k);
    if (s) U8(s + 0x18)--;
}

void DC2CALL K2_Lone(gptr k)                                        /* 0x44A380 */
{
    gptr s = U32(k + 0x8C);
    Orig_Call1(U32(0x7215E4 + U8(k + 0x7B) * 4), k);
    Orig_Call1(U32(0x7219B8 + U8(k + 0x54) * 4), k);
    K2_SyncParent(k);
    if (U8(k + 0x50) & 4) K2_UpdateHidden(k);
    if (s && !(U8(s) & 1) && !(U8(k + 0x84) & 8)) K2_Unlink(k);
    if (U8(k + 0x46) && !--U8(k + 0x46) && U32(k + 0x60)) K2_Detach(k);
}

/* ---- screen sparkles and bursts (0x44A480): steps 0x7219C4; the slot holds nine
   (sparkles) or seven (bursts) little ones, each a position (+6 an angle), three
   colour bytes and a timer --------------------------------------------------------- */
static const u8 k2_sparks[9][5] = {
    {0x20, 0x74, 0x75, 0x76, 0x92}, {0x28, 0x78, 0x79, 0x77, 0x94}, {0x30, 0x7A, 0x7B, 0x7C, 0x96},
    {0x38, 0x7E, 0x7F, 0x7D, 0x98}, {0x40, 0x80, 0x81, 0x82, 0x9A}, {0x48, 0x84, 0x85, 0x83, 0x9C},
    {0x5C, 0x88, 0x89, 0x8A, 0x9E}, {0x64, 0x8C, 0x8D, 0x8B, 0xA0}, {0x6C, 0x8E, 0x8F, 0x90, 0x5A}};

/* skips a random count of random numbers */
static void k2_rand_skip(void)
{
    s32 i = 0, r = Rand8() % 10;
    if (r > 0) do { Rand8(); i++; r = Rand8() % 10; } while (i < r);
}

/* a sparkle anew: somewhere on screen, dark, for a while */
void DC2CALL K2Spark_New(gptr pos, gptr c0, gptr c1, gptr c2, gptr w) /* 0x44A9E0 */
{
    U16(pos) = (u16)(Rand8() + 0x20);
    k2_rand_skip();
    U16(pos + 2) = Rand8();
    k2_rand_skip();
    U16(pos + 4) = (u16)(Rand8() * 2 + 1);
    U16(pos + 6) = (u16)(Rand8() << 3);
    U16(c0) = 0;                                    /* c0 and c1 side by side */
    U8(c2) = 0;
    U16(w) = (u16)(((Rand8() & 0x1F) + 1) * 30);
    if (S16(pos + 2) > 0xF0) U16(pos + 2) -= 0xF0;
}

/* drawn as a 1x1 tile circling its point; brightens to the slot's colour
   (+0x55..+0x57), then after its time fades and starts anew */
void DC2CALL K2Spark_Draw(gptr k, gptr pos, gptr c0, gptr c1, gptr c2, gptr w) /* 0x44AAD0 */
{
    u8 r = U8(c0), g = U8(c1), b = U8(c2);
    s32 x = rcos((U16(pos + 6) * 2) & 0xFFF) / 1024 + S16(pos);
    s32 y = rsin(S16(pos + 6)) / 512 + S16(pos + 2);
    gptr p = U32(0x87DC98);
    U8(p + 4) = r; U8(p + 5) = g; U8(p + 6) = b; U8(p + 7) = 0x62;
    F32(p + 8) = (float)x; F32(p + 0xC) = (float)y;
    U32(p + 0x10) = 0x3C23D70A; U16(p + 0x14) = 1; U16(p + 0x16) = 1;
    gptr ot = 0x87BC00 + 4 * ((S16(pos + 4) / 4) + (U32(0x87DE0C) << 10));
    U32(p) = U32(ot) | 0x3000000; U32(ot) = p & 0xFFFFFF;
    U32(0x87DC98) += 0x18;
    u32 tp = Gpu_GetTPage(0, 1, 0x140, 0x100) & 0xFFFF;
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, tp, 0);
    Gpu_AddPrim(0x87BC00 + 4 * ((S16(pos + 4) / 4) + (U32(0x87DE0C) << 10)), U32(0x87DC98));
    U32(0x87DC98) += 0xC;
    if (Flag_Check(2, 0x10)) return;
    s16 t = S16(w);
    U16(w) = (u16)(t - 1);
    if (t >= 0) {
        U8(c0) += 8; if (U8(c0) > U8(k + 0x55)) U8(c0) = U8(k + 0x55);
        U8(c1) += 8; if (U8(c1) > U8(k + 0x56)) U8(c1) = U8(k + 0x56);
        U8(c2) += 8; if (U8(c2) > U8(k + 0x57)) U8(c2) = U8(k + 0x57);
        return;
    }
    s16 a = (s16)(U8(c0) - 8), d = (s16)(U8(c1) - 8), c = (s16)(U8(c2) - 8);
    if (a < 0) a = 0;
    if (d < 0) d = 0;
    if (c < 0) c = 0;
    if (d + c + a < 1) { K2Spark_New(pos, c0, c1, c2, w); return; }
    U8(c0) = (u8)a; U8(c1) = (u8)d; U8(c2) = (u8)c;
}

/* step 0: colour from +0x98/+0x9A/+0x9C (else 0x60, 0x30, 0x30), nine sparkles */
void DC2CALL K2Sparks_Init(gptr k)                                  /* 0x44A4A0 */
{
    U8(k + 0x54)++;
    U32(k + 0x50) &= ~2u;
    U8(k + 0x55) = U8(k + 0x98); U8(k + 0x56) = U8(k + 0x9A); U8(k + 0x57) = U8(k + 0x9C);
    if (!(U8(k + 0x56) + U8(k + 0x55) + U8(k + 0x57))) { U8(k + 0x55) = 0x60; U8(k + 0x56) = 0x30; U8(k + 0x57) = 0x30; }
    for (int i = 0; i < 9; i++) {
        const u8 *t = k2_sparks[i];
        K2Spark_New(k + t[0], k + t[1], k + t[2], k + t[3], k + t[4]);
    }
}

/* step 1: turning; all anew when the camera changed (G+0x7C4) */
void DC2CALL K2Sparks_Turn(gptr k)                                  /* 0x44A620 */
{
    for (int i = 0; i < 9; i++) {
        gptr a = k + k2_sparks[i][0] + 6;
        U16(a) = (u16)((U16(a) + (Rand8() & 0xF) + 0x10) & 0xFFF);
    }
    if ((s32)S8(k + 0x59) != (s32)U8(G() + 0x7C4))
        for (int i = 0; i < 9; i++) {
            const u8 *t = k2_sparks[i];
            K2Spark_New(k + t[0], k + t[1], k + t[2], k + t[3], k + t[4]);
        }
    U8(k + 0x59) = U8(G() + 0x7C4);
}

/* step 2: drawn, back to step 1 */
void DC2CALL K2Sparks_Draw(gptr k)                                  /* 0x44A890 */
{
    for (int i = 0; i < 9; i++) {
        const u8 *t = k2_sparks[i];
        K2Spark_Draw(k, k + t[0], k + t[1], k + t[2], k + t[3], k + t[4]);
    }
    U8(k + 0x54) = 1;
}

/* steps 3 and 6: off */
void DC2CALL K2Sparks_Off(gptr k)                                   /* 0x44AFC0 */
{
    U32(k + 0x50) = 0;
}

/* the seven of a burst: speed (+0 down, +2 heading), colour (+3 its spread) and time */
static const u8 k2_burst[7][4] = {
    {0x20, 0x54, 0x76, 0x26}, {0x28, 0x66, 0x7A, 0x2E}, {0x30, 0x6A, 0x7E, 0x36}, {0x38, 0x6E, 0x82, 0x3E},
    {0x40, 0x72, 0x86, 0x46}, {0x48, 0x98, 0x8A, 0x4E}, {0x5C, 0x9C, 0x8E, 0x62}};

void DC2CALL K2Burst_New(gptr vel, gptr col, gptr t)                /* 0x44AFD0 */
{
    U8(col + 3) = (u8)((Rand8() & 0xF) + 0x64);
    U16(vel) = (u16)(-50 - (s32)(Rand8() & 0x3F));
    U16(vel + 2) += (u16)(Rand8() * 4 - 0x200);
    U16(t) = (u16)(((Rand8() & 0x1F) + 10) * 30);
}

/* step 4: seven bits at +0x40/+0x44, colour +0x98.. (else grey); the first one's
   speed is the slot's step bytes */
void DC2CALL K2Bursts_Init(gptr k)                                  /* 0x44AD30 */
{
    U8(k + 0x59)++;
    u16 c = U16(k + 0x98);
    U32(k + 0x50) &= ~2u;
    u32 x = U32(k + 0x40), z = U32(k + 0x44);
    U8(k + 0x5A) = (u8)c; U8(k + 0x5B) = U8(k + 0x9A); U8(k + 0x64) = U8(k + 0x9C);
    U32(k + 0x20) = x; U32(k + 0x24) = z; U32(k + 0x28) = x; U32(k + 0x2C) = z;
    U32(k + 0x30) = x; U32(k + 0x34) = z; U32(k + 0x38) = x; U32(k + 0x3C) = z;
    U32(k + 0x40) = x; U32(k + 0x48) = x; U32(k + 0x5C) = x;
    u32 sum = U8(k + 0x5B) + (c & 0xFF) + U8(k + 0x64);
    U32(k + 0x44) = z; U32(k + 0x4C) = z; U32(k + 0x60) = z;
    if (!sum) { U8(k + 0x5A) = 0x80; U8(k + 0x5B) = 0x80; U8(k + 0x64) = 0x80; }
    U16(k + 0x9E) = c; U16(k + 0x9A) = c; U16(k + 0x74) = c; U16(k + 0x70) = c;
    U16(k + 0x6C) = c; U16(k + 0x68) = c; U16(k + 0x56) = c;
    for (int i = 0; i < 7; i++) K2Burst_New(k + k2_burst[i][1], k + k2_burst[i][2], k + k2_burst[i][3]);
}

/* one bit: falls and drifts, drawn as a grey dot (depth by its distance), fades
   after its time; +0x59 counts those gone */
void DC2CALL K2Burst_Draw(gptr k, gptr pos, gptr vel, gptr col, gptr tm) /* 0x44B030 */
{
    s8 sp = (s8)(U8(col + 3) - 3);
    U8(col + 3) = (u8)sp;
    if (sp < 0) { U8(col + 3) = 0; U16(vel) = 1; }
    else {
        U16(vel) += 4;
        if (S16(vel) > -10) U16(vel) = 1;
    }
    gptr t = Tmp_Push(0x40), v = t, ang = t + 8, sxy = t + 0x10, m = t + 0x18;
    U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = U8(col + 3);
    U16(ang) = 0; U16(ang + 2) = U16(vel + 2); U16(ang + 4) = 0; U16(ang + 6) = 0;
    RotMatrix(ang, m);
    ApplyMatrixSV(m, v, v);
    U16(pos) += U16(v); U16(pos + 4) += U16(v + 4);
    U16(pos + 2) += U16(vel);
    S32(m + 0x14) = S16(k + 0x92); S32(m + 0x18) = S16(k + 0x94); S32(m + 0x1C) = S16(k + 0x96);
    SetTransMatrix(m);
    SetRotMatrix(G() + 0x778);
    gte_ldv0(pos);
    gte_rtps();
    gte_stsxy(sxy);
    gte_stsz(t + 0x38);
    s32 z = S32(t + 0x38) >> (U8(G() + 0xB4B) & 31);
    if (z >= 0x3FF) z = 0x3FF;
    u32 sx = U32(sxy), sy = U32(sxy + 4);
    s32 div = (s32)(0x400u << ((U8(G() + 0xB4B) + 1) & 31));
    Tmp_Pop(0x40);
    gptr p = U32(0x87DC98);
    U8(p + 4) = 0x80; U8(p + 5) = 0x80; U8(p + 6) = 0x80; U8(p + 7) = 0x62;
    U32(p + 8) = sx; U32(p + 0xC) = sy;
    F32(p + 0x10) = (float)((xreal)z / (xreal)div);
    U16(p + 0x14) = 1; U16(p + 0x16) = 1;
    gptr ot = 0x87BC00 + 4 * ((U32(0x87DE0C) << 10) + (u32)z);
    U32(p) = U32(ot) | 0x3000000; U32(ot) = p & 0xFFFFFF;
    U32(0x87DC98) += 0x18;
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0x35, 0);
    Gpu_AddPrim(0x87BC00 + 4 * ((U32(0x87DE0C) << 10) + (u32)z), U32(0x87DC98));
    U32(0x87DC98) += 0xC;
    s16 w = S16(tm);
    U16(tm) = (u16)(w - 1);
    if (w >= 0) return;
    s16 a = (s16)(U8(col) - 8), d = (s16)(U8(col + 1) - 8), c = (s16)(U8(col + 2) - 8);
    if (a < 0) a = 0;
    if (d < 0) d = 0;
    if (c < 0) c = 0;
    if (c + d + a < 1) { U8(k + 0x59)++; return; }
    U8(col) = (u8)a; U8(col + 1) = (u8)d; U8(col + 2) = (u8)c;
}

/* step 5: the seven, seen from the camera's eye (+0x92..) */
void DC2CALL K2Bursts_Run(gptr k)                                   /* 0x44AE80 */
{
    gptr c = G() + 0x778, v = Tmp_Push(8);
    U16(v) = (u16)-U16(G() + 0x7A0); U16(v + 2) = (u16)-U16(c + 0x2A); U16(v + 4) = (u16)-U16(c + 0x2C);
    ApplyMatrixSV(c, v, v);
    U16(k + 0x92) = (u16)(U16(c + 0x30) + U16(v));
    U16(k + 0x94) = (u16)(U16(c + 0x32) + U16(v + 2));
    U16(k + 0x96) = (u16)(U16(c + 0x34) + U16(v + 4));
    Tmp_Pop(8);
    for (int i = 0; i < 7; i++)
        K2Burst_Draw(k, k + k2_burst[i][0], k + k2_burst[i][1], k + k2_burst[i][2], k + k2_burst[i][3]);
}

void DC2CALL K2_Sparks(gptr k)                                      /* 0x44A480 */
{
    Orig_Call1(U32(0x7219C4 + U8(k + 0x54) * 4), k);
}
