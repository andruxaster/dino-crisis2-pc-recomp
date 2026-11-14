/* The playable dinosaurs of the extra game (area 9 object types 29..42,
 * KOF_P*.DAT): what all of them share. Each kind has two copies of its code,
 * one per player of the duel; they differ only in their tables, variables,
 * model and which object is the other player (see kof_ino.c).
 * The attacks: the records of o+0xA0 (16 bytes: SVECTOR offset, u16 radius
 * +6, u8 first/last frame +8/+9 (halved twice when o+0x102 has 4, kept when
 * it has 8), u8 part +0xA (0xFF the origin), u8 more +0xB, s16 height +0xC)
 * become the weapon record of slot `slot` (0x737A90, 0x24 bytes each) and go
 * through Weapon_Hit (the computer's dinosaurs) or Kof_HitFoe (the other
 * player). */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

#define HITS          0x74E380u     /* hit list of Weapon_Hit */
#define A_WEAPON_HITS 0x737A90u     /* weapon records */

/* a button sequence: c+0 the step reached, c+2 frames left for the next
 * button (10), c+4 the masks of each step, 0xFFFF after the last; 1 when
 * the last step is reached */
u32 DC2CALL Combo_Input(gptr c, u32 pad)                            /* 0x418F40 */
{
    if (U16(c + 2)) U16(c + 2)--;
    if (!U16(c + 2)) U16(c) = 0;
    u16 i = U16(c);
    if (U16(c + 4 + i * 2) & (u16)pad) {
        i++;
        U16(c + 2) = 10; U16(c) = i;
        if (U16(c + 4 + i * 2) == 0xFFFF) return 1;
    }
    return 0;
}

/* the super move starts: the camera block 0x747A00 on o, event flag 2/0xF,
 * the shout (its kind 0x747A02: 1 for the small kinds, 2 for the big ones) */
void DC2CALL Kof_Super(gptr o)                                      /* 0x418FA0 */
{
    U16(0x747A20) = 1; U16(0x747A1E) = 0;
    U32(0x748088) = o;
    U16(0x747A08) = U16(o + 0x40); U16(0x747A0A) = U16(o + 0x42); U16(0x747A0C) = U16(o + 0x44);
    U32(0x747A10) = 0x19; U32(0x747A18) = 0;
    Flag_Set(2, 0xF, 1);
    u32 k;
    if (U8(0x87E747) == 1) {
        u32 s = (u32)U8(0x87EEAF) - 4;
        static const u8 by_stage[9] = {1, 0, 0, 1, 2, 2, 2, 1, 1};
        k = s <= 8 ? by_stage[s] : 0;
    } else {
        u32 t = (u32)U8(o + 0x58) - 0x1D;
        k = t <= 0xD ? (t >= 4 && t <= 9 ? 2 : 1) : 0;
    }
    if (k) U16(0x747A02) = (u16)k;
    Voice_Play(1, 0x3F5);
}

/* the other player's dinosaur (object `foe`) hit by a weapon record: its
 * spheres seen from the frame m at pos, nearest first; 1 at the first hit */
u32 DC2CALL Kof_HitFoe(gptr m, gptr pos, u32 slot, u32 foe)         /* 0x47B9E0 */
{
    gptr e = A_WEAPON_HITS + slot * 0x24;
    if (slot == 5) {
        s16 r = S16(U32(G() + 0x948) + 0x18);
        U16(e + 2) = (u16)(r > 0x898 ? 0x898 : r);
    }
    gptr f = Tmp_Push(0x40), v = f, a1 = f + 8, a2 = f + 0x10, t = f + 0x18;
    TransposeMatrix(m, t);
    u32 n = 0, ret = 0;
    gptr ob = U32(G() + 0x940) + foe * 0x260;
    gptr d = U32(ob + 0x134);
    if (!d || !(U8(ob + 0x50) & 8) || U8(ob + 0x17B)) goto out;
    gptr r = HITS;
    u8 k = 0;
    d -= 0xC;
    do {
        d += 0xC;
        gptr p = Obj_Part(U32(ob + 0x64), U8(d + 0xA));
        ApplyMatrixSV(p + 0x20, d, v);
        U16(v) += (u16)(U16(p + 0x40) - U16(pos));
        U16(v + 2) += (u16)(U16(p + 0x42) - U16(pos + 2));
        U16(v + 4) += (u16)(U16(p + 0x44) - U16(pos + 4));
        ApplyMatrixSV(t, v, v);
        U32(r) = (u32)((s32)S16(v + 2) - (s32)U16(d + 6));
        U16(r + 4) = U16(v); U16(r + 6) = U16(v + 2); U16(r + 8) = U16(v + 4);
        U16(r + 0xA) = U16(d + 6); U16(r + 0xC) = U16(d + 8);
        U8(r + 0xE) = (u8)foe; U8(r + 0xF) = k;
        n++; r += 0x10; k++;
    } while (U8(d + 0xB) & 1);
    if (!n) goto out;
    Hit_Sort(HITS, 0, n - 1);
    u32 ca = (u32)S16(e + 2) << 16 | (u32)(s32)S16(e);
    u32 cx = (u32)S16(e + 6) << 16 | (u32)(s32)S16(e + 4);
    u32 cz = (u32)S16(e + 0xA) << 16 | (u32)(s32)S16(e + 8);
    for (u32 j = 0; j < n; j++) {
        gptr c = HITS + j * 0x10, q = c + 4;
        if (!U32(c)) continue;
        if (!Cone_HitX(ca, cx, e + 0x1C, q)) continue;
        U32(a1) = U32(q); U16(a1 + 4) = (u16)(U16(q + 8) + U16(q + 4));    /* top */
        if (!Cone_HitZ(ca, cz, e + 0x1C, a1)) {
            U32(a2) = U32(q); U16(a2 + 4) = (u16)(U16(q + 4) - U16(q + 8)); /* bottom */
            if (!Cone_HitZ(ca, cz, e + 0x1C, a2) && (S16(a1 + 4) <= 0 || S16(a2 + 4) >= 0)) continue;
        }
        gptr o = U32(G() + 0x940) + U8(c + 0xE) * 0x260;
        U8(o + 0x174) |= 1;
        U8(o + 0x180) = 0;
        /* the direction from the last sphere of the list, not the one hit */
        U16(o + 0x178) = (u16)Angle_XZ(pos, Obj_Part(U32(o + 0x64), U8(d + 0xA)) + 0x40);
        u32 fl = U32(e + 0xC);
        if (fl & 0x800) U8(o + 0x17A) = (u8)fl;
        else U8(o + 0x17A) = U32(c) <= U16(e + 0x10) ? 0 : U32(c) <= U16(e + 0x12) ? 1 : 2;
        U16(o + 0x170) = (u16)(U32(e + 0xC) >> 16);
        fl = U32(e + 0xC);
        U16(o + 0x172) = fl & 0x400 ? 4 : fl & 0x100 ? 3 : (u16)(2 - U8(o + 0x17A));
        U16(o + 0x176) = U16(e + 0x16 + U8(o + 0x17A) * 2);
        if (slot == 2 && Rand8() > 0xC0) { U16(o + 0x176) <<= 1; U16(o + 0x172) = 2; }   /* critical */
        U8(o + 0x17D) = (u8)slot;
        U8(o + 0x17B) = 5;
        U8(o + 0x17C) = U8(c + 0xF);
        U16(o + 0x1C0) = 1;
        U16(o + 0x1C2) = U16(e + 0x22);
        ret = 1;
        break;
    }
out:
    Tmp_Pop(0x40);
    return ret;
}

/* o's attack spheres of this frame against the computer's dinosaurs (foe 0)
 * or the other player (foe 1/2): o+0x184 bit 8 while a sphere is active,
 * o+0x119 the sphere that hit, 0xFE none of them, 0xFF none active */
void DC2CALL Kof_Attack(gptr o, u32 foe, u32 dmg, u32 react, u32 slot) /* 0x47CB20 */
{
    U32(o + 0x184) &= ~8u;
    if (!U32(o + 0xA0) || (U16(o + 0x102) & 2) || !(u16)dmg) { U8(o + 0x119) = 0xFF; return; }
    u32 div = U16(o + 0x102) & 8 ? 1 : U16(o + 0x102) & 4 ? 4 : 2;
    gptr f = Tmp_Push(0x50), P = f, A = f + 8, m2 = f + 0x10, m = f + 0x30;
    RotMatrix(o + 0x48, m);
    u32 add = 0, i = 0;
    u8 res = 0xFF;
    gptr s = U32(o + 0xA0);
    for (;;) {
        s32 lo = (s16)(U8(s + 8) - 2);
        if (lo < 0) lo = 0;
        u32 fr = U16(o + 0x92);
        if ((u32)(lo * 2) / div <= fr && fr <= U8(s + 9) * 2u / div) U32(o + 0x184) |= 8;
        if (U8(s + 8) * 2u / div <= fr && fr <= U8(s + 9) * 2u / div) {
            U16(P + 6) = U16(s + 6);
            if (U8(s + 0xA) == 0xFF) {
                ApplyMatrixSV(m, s, P);
                U16(P) += U16(o + 0x40); U16(P + 2) += U16(o + 0x42); U16(P + 4) += U16(o + 0x44);
                U16(A) = 0; U16(A + 2) = U16(o + 0x4A);
            } else {
                gptr p = Obj_Part(U32(o + 0x64), U8(s + 0xA));
                ApplyMatrixSV(p + 0x20, s, P);
                U16(P) += U16(p + 0x40); U16(P + 2) += U16(p + 0x42); U16(P + 4) += U16(p + 0x44);
                U16(A) = 0; U16(A + 2) = U16(p + 0x4A);
            }
            U16(A + 4) = 0;
            RotMatrix(A, m2);
            switch ((s16)react) {
            case 0: add = 2; break;
            case 1: add = 1; break;
            case 2: add = 0; break;
            case 3: add = 0x100; break;
            }
            gptr e = A_WEAPON_HITS + (slot & 0xFF) * 0x24;
            U16(e) = 0;
            U16(e + 2) = (u16)(U16(s + 0xC) * 2);
            U16(e + 4) = U16(s + 6); U16(e + 6) = U16(s + 6); U16(e + 8) = U16(s + 6); U16(e + 0xA) = U16(s + 6);
            U16(e + 0x10) = 0;
            U32(e + 0xC) = add + 0x800;
            U16(e + 0x12) = 0; U16(e + 0x14) = 0;
            U16(e + 0x16) = (u16)dmg; U16(e + 0x18) = (u16)dmg; U16(e + 0x1A) = (u16)dmg;
            U16(e + 0x1C) = 0; U16(e + 0x1E) = 0; U16(e + 0x20) = 0;
            U16(e + 0x22) = (u16)react;
            U16(A) = U16(P); U16(A + 4) = U16(P + 4);
            U16(A + 2) = (u16)(U16(P + 2) - U16(s + 0xC));
            u32 hit = (u8)foe == 0 ? Weapon_Hit(m2, A, slot & 0xFF) : Kof_HitFoe(m2, A, slot & 0xFF, foe & 0xFF);
            if (hit) { U8(o + 0x119) = (u8)i; Tmp_Pop(0x50); return; }
            res = 0xFE;
        }
        i++;
        s += 0x10;
        if (!U8(s - 5)) break;
    }
    U8(o + 0x119) = res;
    Tmp_Pop(0x50);
}
