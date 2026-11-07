/* Keyframe animation of objects (0x48DDE0..0x490118).
 *
 * Animation data: u32 header (low 16 bits = number of frames), +4/+8/+0xC
 * optional tables (+4: events {u32 n; {u16 time, u16 arg}[n]}), frames at
 * +0x14, each stride = align4(16 + 6 * parts) bytes: +2 s16 duration, +4
 * s16[3] per channel (channel 0 root position, 1 object rotation, 2.. part
 * rotations, 4096 = 360 degrees).
 * Object fields: +0x78..+0x84 four key frames around the current one,
 * +0x8C first frame, +0x90 frames, +0x92 play time, +0x94 current frame,
 * +0x98 time inside the frame, +0x9A blend frames left, +0x9C..+0xA4
 * tables, +0xAA half-step carry, +0xAC stride, +0x102 flags (1 loop,
 * 4/8 speed, 0x10 no events, 0x20 finished, 0x40 move by root, 0x200 ...).
 * Values are interpolated with a cubic (Hermite) curve over four keys. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

#define BLEND_BUF 0x6F0000u        /* target pose while blending, s16[4] per channel */

static u32 speed_div(u32 fl) { return fl & 8 ? 1 : fl & 4 ? 4 : 2; }

/* run the events {u16 time, u16 arg} that fall on the current time */
void DC2CALL Anim_Events(gptr o, s32 m)                             /* 0x48DE80 */
{
    gptr tab = U32(o + 0x9C);
    if (!tab || (U8(o + 0x102) & 0x10)) return;
    u32 n = U32(tab);
    for (gptr e = tab + n * 4; n; n--, e -= 4) {             /* last entry first */
        s32 cur = (s32)(U16(o + 0x92) * 2) / m + S16(o + 0xAA);
        if ((s32)(U16(e) * 2) / m == cur) Fn_48C0B0(o, U16(e + 2));
    }
}

/* wrap three angles of b to within half a turn of a, then blend linearly */
void DC2CALL Anim_BlendLinear(gptr a, gptr b, gptr out, s32 w)      /* 0x48FEA0 */
{
    for (int i = 0; i < 3; i++) U16(a + 2 * i) &= 0xFFF;
    for (int i = 0; i < 3; i++) {
        U16(b + 2 * i) &= 0xFFF;
    }
    for (int i = 0; i < 3; i++) {
        s32 d = S16(b + 2 * i) - S16(a + 2 * i);
        if ((d < 0 ? -d : d) > 0x800) U16(b + 2 * i) += d > 0 ? 0xF000 : 0x1000;
    }
    LoadAverageShort0(a, b, 0x1000 - w, w, out);
    for (int i = 0; i < 3; i++) U16(out + 2 * i) &= 0xFFF;
}

/* angles (YXZ order) of a rotation matrix */
gptr DC2CALL Matrix_ToAngles(gptr m, gptr out)                      /* 0x48F1D0 */
{
    S16(out + 2) = (s16)-ratan2((float)-S16(m + 4), (float)S16(m + 0x10));
    S16(out + 4) = (s16)-ratan2((float)-S16(m + 6), (float)S16(m + 8));
    gptr t = Tmp_Push(0x28), r = t, mm = t + 8;
    U16(r) = 0; U16(r + 2) = U16(out + 2); U16(r + 4) = 0;
    RotMatrix(r, mm);
    TransposeMatrix(mm, mm);
    MulMatrix0(m, mm, mm);
    S16(out) = (s16)-ratan2((float)S16(mm + 0xA), (float)S16(mm + 0x10));
    Tmp_Pop(0x28);
    return out;
}

/* blend two rotations through their matrices */
void DC2CALL Anim_BlendMatrix(gptr a, gptr b, gptr out, s32 w)      /* 0x48FF90 */
{
    for (int i = 0; i < 3; i++) U16(a + 2 * i) &= 0xFFF;
    for (int i = 0; i < 3; i++) U16(b + 2 * i) &= 0xFFF;
    gptr t = Tmp_Push(0x40), ma = t, mb = t + 0x20;
    RotMatrixYXZ(a, ma);
    RotMatrixYXZ(b, mb);
    s32 wa = (s16)(0x1000 - w), wb = (s16)w;
    for (int i = 0; i < 9; i++) {
        s32 v = (s32)((u32)(S16(ma + 2 * i) * wa) + (u32)(S16(mb + 2 * i) * wb));
        S16(ma + 2 * i) = (s16)(v / 4096);
    }
    Matrix_ToAngles(ma, out);
    Tmp_Pop(0x40);
    for (int i = 0; i < 3; i++) U16(out + 2 * i) &= 0xFFF;
}

static s32 unwrap(s32 v, s32 ref)
{
    s32 d = v - ref;
    if ((d < 0 ? -d : d) > 0x800) v += d > 0 ? -0x1000 : 0x1000;
    return v;
}

u32 DC2CALL Anim_ApplyFrame(gptr o, u32 step)                       /* 0x48E3D0 */
{
    gptr k0 = U32(o + 0x78), k1 = U32(o + 0x7C), k2 = U32(o + 0x80), k3 = U32(o + 0x84);
    gptr p0 = k0 + 4, p1 = k1 + 4, p2 = k2 + 4, p3 = k3 + 4;
    s32 d0 = S16(k0 + 2), d1 = S16(k1 + 2), d2 = S16(k2 + 2);
    s32 t = (s32)((u32)S16(o + 0x98) << 14) / d1;
    s32 t2 = (s32)((u32)t * (u32)t) >> 14;
    s32 t3 = (s32)((u32)t2 * (u32)t) >> 14;
    gptr part = U32(o + 0x64);
    for (u32 ch = 0; ch < U32(o + 0x68) + 2; ch++) {
        gptr dst;
        if (U16(o + 0x9A)) dst = BLEND_BUF + ch * 8;
        else if (ch == 0) dst = part + 0x64;
        else if (ch == 1) dst = o + 0xC0;
        else { dst = part + 0x48; part = U32(part + 0x54); }
        for (int j = 0; j < 3; j++, dst += 2, p0 += 2, p1 += 2, p2 += 2, p3 += 2) {
            s32 v0 = S16(p0), v1 = S16(p1), v2 = S16(p2), v3 = S16(p3);
            if (ch >= 2) {                                  /* angles: shortest way */
                v0 &= 0xFFF; v1 &= 0xFFF; v2 &= 0xFFF; v3 &= 0xFFF;
                v2 = unwrap(v2, v1);
                v0 = unwrap(v0, v2);
                v3 = unwrap(v3, v0);
            }
            s32 dv = v2 - v1, m1, m2;
            if (S16(o + 0x94) == 0) m1 = dv;
            else m1 = (s32)((u32)(v1 - v0) * (u32)d1 + (u32)dv * (u32)d0) / (d0 + d1);
            if ((s32)S16(o + 0x94) == (s32)U16(o + 0x90) - 1) m2 = v3 - v2;
            else m2 = (s32)((u32)(v3 - v2) * (u32)d1 + (u32)dv * (u32)d2) / (d1 + d2);
            u32 a = (u32)(3 * dv - 2 * m1 - m2) * (u32)t2;
            u32 b = (u32)(m2 + 2 * (v1 - v2) + m1) * (u32)t3;
            u32 c = (u32)m1 * (u32)t;
            S16(dst) = (s16)(((s32)(a + b + c) >> 14) + v1);
        }
    }
    if (U16(o + 0x9A)) {                                    /* blend into the new animation */
        s32 w = 0x1000 / (s32)U16(o + 0x9A);
        gptr first = U32(o + 0x64), tmp = Tmp_Push(8);
        LoadAverageShort0(first + 0x64, BLEND_BUF, 0x1000 - w, w, tmp);
        U16(first + 0x64) = U16(tmp); U16(first + 0x66) = U16(tmp + 2); U16(first + 0x68) = U16(tmp + 4);
        Tmp_Pop(8);
        U16(o + 0xC0) = U16(BLEND_BUF + 8); U16(o + 0xC2) = U16(BLEND_BUF + 10); U16(o + 0xC4) = U16(BLEND_BUF + 12);
        gptr src = BLEND_BUF + 0x10;
        for (gptr p = first; p; p = U32(p + 0x54), src += 8) {
            if (U8(o + 0x103) & 1) Anim_BlendMatrix(p + 0x48, src, p + 0x48, w);
            else Anim_BlendLinear(p + 0x48, src, p + 0x48, w);
        }
        U16(o + 0x9A)--;
    }
    U16(o + 0x98) += (u16)step;
    return 0;
}

void DC2CALL Anim_Start(gptr o, gptr anim, u32 frame, u32 fl, u32 blend)  /* 0x48DEF0 */
{
    fl = fl & 0x80000000u ? fl | 0x200 : fl & ~0x200u;
    if (anim < 0x500000) anim = U32(U32(o + 0x88) + anim * 4);
    U32(o + 0x9C) = U32(anim + 4);
    U32(o + 0xA0) = U32(anim + 8);
    U32(o + 0xA4) = U32(anim + 0xC);
    if (fl & 0x80) {                    /* keep the play position across speeds */
        u32 c = speed_div(U16(o + 0x102)), n = speed_div(fl);
        u32 a = U32(anim) * 2 / c * U16(o + 0x92);
        u32 b = U16(o + 0x90) * 2u / n;
        frame = a / b + 2 / n;
    }
    U16(o + 0x102) = (u16)fl;
    gptr frames = anim + 0x14;
    u32 stride = (u8)(((U32(o + 0x68) * 6 + 0x13) >> 2) << 2);
    U8(o + 0xAC) = (u8)stride;
    U16(o + 0x90) = U16(anim);
    U32(o + 0x78) = U32(o + 0x7C);
    u32 last = (u32)U16(o + 0x90) - 1;
    U32(o + 0x8C) = frames;
    gptr k1 = frames + (frame < last ? frame : last) * stride;
    U32(o + 0x7C) = k1;
    U32(o + 0x80) = frames + (frame + 1 < last ? frame + 1 : last) * stride;
    U32(o + 0x84) = frames + (frame + 2 < last ? frame + 2 : last) * stride;
    if (!U32(o + 0x78)) U32(o + 0x78) = k1;
    U16(o + 0x98) = 0;
    U16(o + 0xAA) = 0;
    U16(o + 0x92) = (u16)(((u32)S16(k1 + 2) * frame) >> 1);
    u8 keep = U8(o + 0x102) & 0x40;
    U16(o + 0x94) = (u16)frame;
    U16(o + 0x9A) = (u16)blend;
    U16(o + 0x102) = (u16)(fl & 0xFFBF);
    u32 m = speed_div(fl & 0xFFBF);
    Anim_Events(o, (s32)m);
    Anim_ApplyFrame(o, m);
    U16(o + 0x102) |= keep;
}

void DC2CALL Anim_Set(gptr o, gptr anim, u32 frame, u32 fl, u32 blend)    /* 0x48DDE0 */
{
    gptr p = U32(o + 0x64);
    U16(o + 0x40) = U16(p + 0x40);
    U16(o + 0x42) = (u16)(U16(o + 0x5A) * 0xFC18u);           /* floor level * -1000 */
    U16(o + 0x44) = U16(p + 0x44);
    U16(o + 0xE0) = U16(p + 0x6C);
    U16(o + 0xE2) = U16(p + 0x6E);
    U16(o + 0xE4) = U16(p + 0x70);
    U32(o + 0x7C) = 0;
    gptr a = anim >= 0x500000 ? anim : U32(U32(o + 0x88) + anim * 4);
    U16(p + 0x64) = U16(a + 0x18);
    U16(p + 0x66) = (u16)(U16(p + 0x42) - U16(o + 0x42));
    U16(p + 0x68) = U16(a + 0x1C);
    Anim_Start(o, anim, frame, fl, blend);
}

/* advance by one game frame; 1 when a non-looping animation reached its end */
u32 DC2CALL Anim_Step(gptr o)                                       /* 0x48E100 */
{
    u32 done = 0;
    u16 fl = U16(o + 0x102);
    if (fl & 0x20) return 0;
    u32 m = speed_div(fl);
    u16 h = (u16)(U16(o + 0xAA) + m);
    U16(o + 0x92) += (u16)((s32)(s16)h / 2);
    U16(o + 0xAA) = h & 1;
    Anim_Events(o, (s32)m);
    U16(o + 0xC8) = U16(o + 0xC0); U16(o + 0xCA) = U16(o + 0xC2); U16(o + 0xCC) = U16(o + 0xC4);
    gptr k1 = U32(o + 0x7C);
    if (S16(o + 0x98) >= S16(k1 + 2)) {                     /* next key frame */
        U16(o + 0x94)++;
        U16(o + 0x98) -= U16(k1 + 2);
        s32 nfr = U16(o + 0x90), idx;
        if (U8(o + 0x102) & 1) {                            /* looping */
            if (S16(o + 0x94) >= nfr) {
                done = 1;
                U16(o + 0x94) = 0; U16(o + 0x92) = 0; U16(o + 0x98) = 0; U16(o + 0xAA) = 0;
            }
            idx = S16(o + 0x94) + 2;
            if (idx >= nfr) idx -= nfr;
        } else {
            if (S16(o + 0x94) >= nfr) {
                U16(o + 0x92)--;
                U16(o + 0x94)--;
                done = 1;
            }
            idx = S16(o + 0x94) + 2;
            if (idx >= nfr) idx = nfr - 1;
        }
        if (nfr == 1) idx = 0;
        gptr k3 = U32(o + 0x84);
        U32(o + 0x7C) = U32(o + 0x80);
        U32(o + 0x78) = k1;
        U32(o + 0x80) = k3;
        U32(o + 0x84) = U8(o + 0xAC) * (u32)idx + U32(o + 0x8C);
    }
    Anim_ApplyFrame(o, m);
    fl = U16(o + 0x102);
    if ((fl & 0x200) || done) {
        if (done && !(fl & 1)) U16(o + 0x102) = fl | 0x20;
        return done;
    }
    if (!(fl & 0x40)) return 0;
    /* root motion: rotate the change of the root position into the world */
    gptr sv = Tmp_Push(0x28), mm = sv + 8;
    U16(sv) = U16(o + 0xD0) = (u16)(U16(o + 0xC0) - U16(o + 0xC8));
    U16(sv + 2) = U16(o + 0xD2) = (u16)(U16(o + 0xC2) - U16(o + 0xCA));
    U16(sv + 4) = U16(o + 0xD4) = (u16)(U16(o + 0xC4) - U16(o + 0xCC));
    RotMatrix(o + 0x48, mm);
    ApplyMatrixSV(mm, sv, sv);
    if (U8(G() + 0x1091) == 9 && U8(G() + 0x1090) == 3) {   /* room 903: scaled objects */
        U16(o + 0x40) += (u16)((s32)((u32)(S32(o + 0x6C) >> 8) * (u32)S16(sv)) >> 4);
        U16(o + 0x42) += (u16)((s32)((u32)(S32(o + 0x70) >> 8) * (u32)S16(sv + 2)) >> 4);
        U16(o + 0x44) += (u16)((s32)((u32)(S32(o + 0x74) >> 8) * (u32)S16(sv + 4)) >> 4);
    } else {
        U16(o + 0x40) += U16(sv); U16(o + 0x42) += U16(sv + 2); U16(o + 0x44) += U16(sv + 4);
    }
    Tmp_Pop(0x28);
    return 0;
}
