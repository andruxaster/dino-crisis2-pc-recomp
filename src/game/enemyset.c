/* Enemy sets: the room's spawner (G+0xA68) brings enemies in while the
 * player is in one of its groups' areas.
 *
 * set: +0 groups, +1 enemy type, +2 most alive, +3 most per room (0xFF:
 * no limit), +4 the enemies' level, +6 how a spawn point is picked (2 farthest from the others,
 * 3 not behind the way the player went, 1 not the last), +8 the small
 * dinosaurs (type 0x1B) too; +0x10 groups of 0x20: +0 area (quad), +0x10 spawn points (offset in the set), +0x12
 * their count, +0x13 floor, +0x14 most alive here, +0x15 most spawned
 * here, +0x16 chance of one fewer, +0x18 frames between spawns. A spawn
 * point (0x10): +0 position, +6 yaw, +8 kind (4: comes in roaring), +9
 * light, +0xA cry, +0xB, +0xC, +0xE.
 *
 * state G+0xA80: +0 frames to the next spawn, +4 spawned, +5 most alive
 * now, +6, +8 last point, +9 group, +0xA previous group, +0xB, +0xC out
 * of every area, +0xE, +0x10 the way the player went, +0x18 spawned per
 * group; G+0xAB8 4 cries {object, kind, step, delay}; G+0xAC8 the small
 * dinosaurs' state. */
#include "dc2/game.h"
#include "dc2/orig.h"

#define ST  (G() + 0xA80)
#define ST2 (G() + 0xAC8)
#define SET U32(G() + 0xA68)

/* a free object slot from the end down to the first enemy slot (G+0xA7C) */
gptr DC2CALL Obj_AllocEnemy(void)                                   /* 0x48D7B0 */
{
    u32 i = U32(0x87F940) - 1, lo = U32(G() + 0xA7C);
    if (i <= lo) return 0;
    for (gptr o = U32(G() + 0x940) + i * 0x260; i > lo; i--, o -= 0x260)
        if (!(U8(o + 0x50) & 1)) { Obj_Init(i); return o; }
    return 0;
}

/* the cries in progress: each step a sound (by the crier), then the next
 * after its delay (0x73758C: per kind {sound, delay} pairs) */
void DC2CALL EnemySet_Cries(gptr s)                                 /* 0x472130 */
{
    for (int k = 0; k < 4; k++, s += 4) {
        if (!U8(s)) continue;
        if (U8(s + 3)--) continue;
        gptr e = U32(0x73758C + U8(s + 1) * 4) + U8(s + 2) * 2;
        Se_Play(U32(G() + 0x940) + U8(s) * 0x260, U8(e));
        if (U8(e + 1)) { U8(s + 2)++; U8(s + 3) = U8(e + 1); }
        else U8(s) = 0;
    }
}

/* a new enemy's cry (kind 1..6) if the first slot is free */
void DC2CALL EnemySet_Cry(u32 kind, u32 obj)                        /* 0x4720B0 */
{
    gptr s = G() + 0xAB8;
    u8 k = (u8)kind;
    if (!k) return;
    k--;
    if (k >= 6) return;
    if (U8(s)) return;                              /* (only the first slot is looked at) */
    U8(s + 1) = k;
    U8(s + 2) = 0;
    gptr t = U32(0x73758C + k * 4);
    Se_Play(U32(G() + 0x940), U8(t));
    if (!U8(t + 1)) return;
    U8(s) = (u8)obj;
    U8(s + 2)++;
    U8(s + 3) = U8(t + 1);
}

/* a spawn point's light */
void DC2CALL EnemySet_Light(gptr p)                                 /* 0x4721B0 */
{
    if (!U8(p + 9)) return;
    gptr l = Light_Alloc();
    if (!l) return;
    U8(l + 1) = 4;
    U8(l + 2) = U8(p + 9) - 1;
    U16(l + 0xC) = U16(p); U16(l + 0xE) = U16(p + 2); U16(l + 0x10) = U16(p + 4);
    U16(l + 0x12) = 0; U16(l + 0x14) = 0;
    U16(l + 0x16) = U16(p + 6);
    U16(l + 0x18) = 0;
}

/* from the centre of area a to that of area b (x, z) */
void DC2CALL EnemySet_Way(gptr a, gptr b, gptr out)                 /* 0x472BB0 */
{
    s32 ax = (S16(a + 0xC) + S16(a + 8) + S16(a + 4) + S16(a)) / 4;
    s32 az = (S16(a + 0xE) + S16(a + 0xA) + S16(a + 6) + S16(a + 2)) / 4;
    s32 bx = (S16(b + 8) + S16(b + 0xC) + S16(b + 4) + S16(b)) / 4;
    s32 bz = (S16(b + 0xE) + S16(b + 0xA) + S16(b + 6) + S16(b + 2)) / 4;
    U16(out + 4) = (u16)(bz - az);
    U16(out) = (u16)(bx - ax);
    U16(out + 6) = 0; U16(out + 2) = 0;
}

/* the group whose area (on the player's floor) holds the player, or 0xFF;
 * the player is G+0xB50 when set (riding) */
u32 DC2CALL EnemySet_FindGroup(gptr grp, s32 n)                     /* 0x472C60 */
{
    gptr pl = U32(G() + 0xB50) ? U32(G() + 0xB50) : U32(G() + 0x940), src = pl;
    if (U32(pl + 0x50) & 0x100) src = Obj_Part(U32(pl + 0x64), 8);
    gptr pos = Tmp_Push(8);
    U16(pos) = U16(src + 0x40); U16(pos + 2) = U16(src + 0x42); U16(pos + 4) = U16(src + 0x44);
    u32 r = 0xFF;
    for (u8 i = 0; (s32)i < n; i++, grp += 0x20)
        if (U8(grp + 0x13) == U16(pl + 0x5A) && PointInQuad(pos, grp)) { r = i; break; }
    Tmp_Pop(8);
    return r;
}

static u32 count_type(u32 type)
{
    u32 n = 0;
    gptr o = U32(G() + 0x940) + 0x260;
    for (u32 k = U32(0x87F940); k > 1; k--, o += 0x260)
        if ((U8(o + 0x50) & 1) && U8(o + 0x58) == (u8)type) n++;
    return n;
}

static u32 rnd2(void)
{
    u8 a = Rand8();
    return (u8)(a + Rand8());
}

/* each frame: count down, then bring an enemy in at one of the group's
 * spawn points (its level from the difficulty and the room) */
void DC2CALL EnemySet_Tick(void)                                    /* 0x472230 */
{
    gptr st = ST;
    u32 two = 0;
    if (U32(G() + 0xB50)) { Flag_Set(7, 0x1F, 0); Flag_Set(7, 0x1E, 0); }
    EnemySet_Cries(G() + 0xAB8);
    gptr set = SET;
    if (!set) return;
    u32 n = U8(set);
    if (!n) return;
    if (U8(set + 1) == 2 && U16(G() + 0x138A)) {      /* the scripted raptors */
        U8(st + 6) = 1;
        U8(set + 2) = U16(G() + 0x138A) == 2 && !U8(st + 4) ? 1 : 2;
    }
    u32 room = Room_GlobalIndex(U8(G() + 0x1091), U8(G() + 0x1090)) & 0xFF;
    if (U8(set + 3) == 0xFF) U8(G() + 0x12D0 + room) = 0;
    else {
        u16 most = U8(set + 3);
        if (U8(G() + 0x108E) == 2) most = most < 0xE6 ? most + 0x14 : 0xFA;
        if (U8(G() + 0x12D0 + room) >= most) { Flag_Set(2, 0x15, 1); return; }
    }
    u16 alive = U8(set + 2);
    if (!(U8(set + 1) == 2 && U8(st + 6) == 1) && U8(G() + 0x108E) == 2) alive = alive >= 0xF0 ? 0xFA : alive + 10;
    if ((s16)U8(st + 4) >= (s16)alive) return;
    gptr grp = SET + 0x10;
    if (Flag_Check(1, 0xA)) {
        if (U16(st)) U16(st)--;
        return;
    }
    u32 k = EnemySet_FindGroup(grp, n) & 0xFF;
    if (k == 0xFF) { U8(st + 0xC) = 1; return; }
    grp += k * 0x20;
    u8 cur = U8(st + 9);
    if (k != cur) {
        if (k == U8(st + 0xA)) {                  /* back to the previous group */
            if (!U8(st + 0xB)) {
                U8(st + 0xB) = 1;
                U16(st + 0xE) = U16(st + 0xE) ? U16(st + 0xE) + 0x3C : 10;
                U16(st) = U16(st + 0xE);
            }
        } else U16(st + 0xE) = 0;
        U8(st + 0xA) = cur;
        U8(st + 9) = (u8)k;
        U8(st + 0xC) = 0;
        EnemySet_Way(SET + U8(st + 0xA) * 0x20 + 0x10, grp, st + 0x10);
        if (U8(st + 0xA) == 0xFF) U16(st) = (u16)((Rand8() & 0xF) + U16(grp + 0x18));
        U16(st) -= 7;
        if (U16(st) > 0x8000) U16(st) = Rand8() & 3;
        if (!U8(grp + 0x16)) U8(st + 5) = U8(grp + 0x14);
        else {
            s32 r = rnd2();
            U8(st + 5) = r % (5 - (s32)U8(grp + 0x16)) ? U8(grp + 0x14) - 1 : U8(grp + 0x14);
        }
        if (U8(ST2 + 5) && U8(st + 5) >= 3) U8(st + 5) = 2;
    } else if (U8(st + 0xC) == 1 && !U8(st + 0xB)) {  /* back into the area */
        U8(st + 0xB) = 1;
        U16(st + 0xE) = U16(st + 0xE) ? U16(st + 0xE) + 0x3C : 10;
        U16(st) = U16(st + 0xE);
    }
    if (!U8(grp + 0x12)) return;
    if (U8(st + 0x18 + U8(st + 9)) >= U8(grp + 0x15)) {
        if (U32(G() + 0xB50)) Flag_Set(7, 0x1E, 1);
        return;
    }
    if (count_type(U8(set + 1)) >= U8(st + 5)) { Rand8(); Rand8(); return; }
    if (U16(st)) U16(st)--;
    if (!U8(G() + 0x1091) && U8(G() + 0x1090) == 1) U8(st + 5) = U8(grp + 0x14);
    if (U16(st)) return;
    U8(st + 0xB) = 0;
    if (U32(G() + 0xB50)) Flag_Set(7, 0x1F, 1);
    U16(st) = (u16)((Rand8() & 0xF) + U16(grp + 0x18) - 3);
    if (U16(st) > 0x8000) U16(st) = Rand8() & 3;
    if (U8(G() + 0x108E) == 2) U16(st) >>= 2;
    if (U16(st) < 5) U16(st) = 5;
    s32 cnt = U8(grp + 0x12);
    s32 pt = rnd2() % cnt;
    u8 mode = U8(set + 6);
    if (mode == 2 && cnt != 1) {                    /* the point farthest from the others */
        s32 d[256];
        for (s32 i = 0; i < cnt; i++) {
            d[i] = 0xFFFFFFF;
            gptr p = U16(grp + 0x10) + SET + i * 0x10;
            gptr o = U32(G() + 0x940) + 0x260;
            for (u32 m = U32(0x87F940); m > 1; m--, o += 0x260) {
                if (!(U8(o + 0x50) & 1) || U8(o + 0x58) != U8(set + 1)) continue;
                s32 dz = S16(o + 0x44) - S16(p + 4), dx = S16(o + 0x40) - S16(p);
                s32 v = (dz < 0 ? -dz : dz) + (dx < 0 ? -dx : dx);
                if (v < d[i]) d[i] = v;
            }
        }
        s32 best = d[0];
        pt = 0;
        for (s32 i = 1; i < cnt; i++) if (best < d[i]) { best = d[i]; pt = i; }
    }
    if (mode == 3 && cnt != 1) {                    /* not behind the way the player went */
        gptr p = U16(grp + 0x10) + SET + pt * 0x10, pl = U32(G() + 0x940);
        s32 v = (S16(p + 4) - S16(pl + 0x44)) * S16(st + 0x14) + (S16(p) - S16(pl + 0x40)) * S16(st + 0x10);
        if (v < 0) { if (pt == cnt - 1) pt--; else pt++; }
    }
    if ((mode == 3 || mode == 1) && cnt != 1 && U8(st + 8) == pt) {
        if (pt == cnt - 1) pt--; else pt++;
    }
    U8(st + 8) = (u8)pt;
    gptr sp = U16(grp + 0x10) + SET + pt * 0x10;
    gptr o = Obj_AllocEnemy();
    if (!o) return;
    U8(st + 4)++;
    U8(st + 0x18 + U8(st + 9))++;
    Flag_Set(2, 0x14, 1);
    U16(o + 0xE0) = U16(o + 0x40) = U16(sp);
    U16(o + 0xE2) = U16(o + 0x42) = U16(sp + 2);
    U16(o + 0xE4) = U16(o + 0x44) = U16(sp + 4);
    U16(o + 0x4A) = U16(sp + 6);
    s32 idx = (s32)(o - U32(G() + 0x940)) / 0x260;
    EnemySet_Cry(U8(sp + 0xA), (u32)idx);
    EnemySet_Light(sp);
    u32 lvl = U8(set + 4) + S16(G() + 0x1388) * 3;
    u8 type = U8(set + 1), level;
    if (type == 2) {
        if (U16(G() + 0x138A)) {
            two = 0;
            U16(G() + 0x138A) = (u16)(U16(G() + 0x138A) - 1) ? 0 : 2;
            level = 5;
        } else {
            if (lvl > 9) lvl = 9;
            two = Rand8() & 1;
            level = U8(0x703E9C + lvl * 4 + (two ? 1 : 0));
        }
    } else if (type == 7) {
        if (lvl > 8) lvl = 8;
        two = Rand8() & 1;
        level = U8(0x703EC4 + lvl * 4 + (two ? 1 : 0));
    } else level = U8(set + 4);
    U8(o + 0x18C) = (u8)(level << 4);
    U16(o + 0x10C) = 0x20;
    U8(o + 0x18C) = (u8)((U8(sp + 8) & 0xF) | (u8)(level << 4));
    U8(o + 0x18D) = U8(sp + 0xB);
    U8(o + 0x17B) = U8(sp + 0xC);
    U8(o + 0x18E) = U8(sp + 0xE);
    U8(o + 0x18F) = U8(st + 9);
    U8(o + 0x58) = U8(set + 1);
    if ((U8(set + 1) == 2 || U8(set + 1) == 7) && U8(sp + 8)) U32(o + 0x50) |= 0x810;
    Orig_Call1(U32(U32(0x87DCA0) + U8(o + 0x58) * 4), o);
    if (U8(set + 1) == 2 && two && !U8(0x703E9E + lvl * 4)) { U16(o + 0x104) = 0x9C; U16(o + 0x106) = 0x7F80; }
    if (U8(set + 1) == 7 && two && !U8(0x703EC6 + lvl * 4)) { U16(o + 0x104) = 0x9C; U16(o + 0x106) = 0x7F80; }
    if ((U8(set + 1) == 2 || U8(set + 1) == 7) && U8(sp + 8) == 4) U16(o + 0x5A) -= 2;
    if (U8(set + 1) == 7 && U8(sp + 8) == 4) {
        u32 s = Rand8() & 1 ? 0x3A : 0x3B;
        Se_Play(U32(G() + 0x940) + idx * 0x260, s);
    }
    if (U8(set + 1) == 2 && U8(sp + 8) == 4) {
        u32 s = Rand8() & 1 ? 0x42 : 0x43;
        Se_Play(U32(G() + 0x940) + idx * 0x260, s);
    }
}

/* each frame while the player is hurt (+0x17E) the small dinosaurs (type
 * 0x1B) come, more the weaker it is */
void DC2CALL EnemySet_Small(void)                                   /* 0x472D20 */
{
    gptr st = ST2;
    if (!U16(U32(G() + 0x940) + 0x17E)) return;
    if (U32(G() + 0xB50)) {
        Flag_Set(7, 0x1F, 0); Flag_Set(7, 0x1E, 0);
        if (U32(G() + 0xB50)) return;
    }
    gptr set = SET;
    if (!set || !U8(set) || !U8(set + 8)) return;
    s32 n = U8(set);
    gptr grp = set + 0x10;
    u32 k = Flag_Check(1, 0xA) ? U8(st + 9) : EnemySet_FindGroup(grp, n) & 0xFF;
    if (k == 0xFF) return;
    grp += k * 0x20;
    if (!U8(grp + 0x12)) return;
    U8(st + 5) = count_type(U8(set + 1)) > 2 ? 1 : 5;
    u16 hp = U16(U32(G() + 0x940) + 0x17E);
    s32 most = 5 - hp / 210;
    if (hp > 0x41A) return;
    if (most < 0) most = 0;
    if ((s32)U8(st + 5) > most) U8(st + 5) = (u8)most;
    if (count_type(0x1B) >= U8(st + 5)) { Rand8(); Rand8(); return; }
    U16(st)--;
    if (U16(st) > 0x8000) U16(st) = 0;
    if (U16(st)) return;
    U16(st) = (u16)((Rand8() & 0x1F) + 0xF);
    if (U8(G() + 0x108E) == 2) {
        U16(st) >>= 1;
        if (U16(st) < 5) U16(st) = 5;
    }
    s32 pt = rnd2() % U8(grp + 0x12);
    U8(st + 8) = (u8)pt;
    gptr sp = U16(grp + 0x10) + SET + pt * 0x10;
    gptr o = Obj_AllocEnemy();
    if (!o) return;
    U16(o + 0xE0) = U16(o + 0x40) = U16(sp);
    U16(o + 0xE2) = U16(o + 0x42) = U16(sp + 2);
    U16(o + 0xE4) = U16(o + 0x44) = U16(sp + 4);
    u8 r = Rand8();
    U16(o + 0x10C) = 0;
    U8(o + 0x58) = 0x1B;
    U16(o + 0x4A) = (u16)(r + U16(sp + 6) - 0x1FF);
    Orig_Call1(U32(U32(0x87DCA0) + 0x6C), o);
}
