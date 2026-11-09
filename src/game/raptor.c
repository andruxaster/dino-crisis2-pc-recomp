/* The velociraptor (object type handler 0x41F430). Its extension (o+0x1C0,
 * also *0x74AF00 while it runs): +0 first delay, +4 squared distance to its
 * target, +8 angle to it, +0xC state kept from the last frame, +0x19 its
 * kind of attack, +0x1A frames, +0x1C, +0x20/+0x21 may jump / may climb,
 * +0x22/+0x23, +0x24 a countdown.
 * States (o+0x54): 0 init, 1 normal (sub-states 0x720040), 2 hit (by
 * o+0x172, 0x720064), 3 die, 4 (0x4A56D0). */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/task.h"
#include "dc2/gte.h"

#define RAP_EXT   0x74AF00u     /* the extension of the raptor that runs */
#define RAP_MAIN  0x72002Cu     /* state handlers */
#define RAP_NORM  0x720040u     /* state 1: sub-states */
#define RAP_HIT   0x720064u     /* state 2: by the kind of hit (o+0x172) */
#define RAP_ATTACK 0x72007Cu    /* state 1/2: attack steps by o+0x56 */
#define RAP_GRAB  0x720090u     /* holding the player: by o+0x56 (o, player) */
#define RAP_TAB_LEVEL 0x7031E8u /* per level (5 each): health class, attack, ... */
#define RAP_TAB_HEALTH 0x7031B8u
#define RAP_TAB_BLOOD 0x70322Cu /* 4 blood effects, 8 apart */

static void Orig_Call2(gptr fn, u32 a, u32 b)
{
#ifdef DC2_IN_PROCESS
    ((void (DC2CALL *)(u32, u32))(uintptr_t)fn)(a, b);
#else
    void (DC2CALL *f)(u32, u32) = (void (DC2CALL *)(u32, u32))Hook_Find(fn);
    if (f) f(a, b);
    else Orig_Call1(fn, a);                 /* reports it once as not rewritten */
#endif
}

/* each frame: damage taken, the state, then movement, collisions and attacks */
void DC2CALL Raptor_Handler(gptr o)                                 /* 0x41F430 */
{
    Obj_SavePos(o);
    gptr e = o + 0x1C0;
    U32(RAP_EXT) = e;
    if (U8(o + 0x184) & 0x40) Zone_TrackPoint(o, U32(G() + 0xB50) + 0x40);
    if (U8(o + 0x54) == 2 && U8(o + 0x56) == 0) {
        u32 kind = U8(o + 0x10C) & 0xF;
        u8 hit = U8(o + 0x17D);
        int take = 0;
        if (hit <= 2) {
            if (kind == 4) take = !(Rand8() & 0x80);
            else if (kind == 5) { U16(o + 0x176) >>= 1; take = 1; }
        } else if (kind == 5) {
            if (hit == 0x10 || hit == 0x11) { U16(o + 0x176) = 0; U32(o + 0x54) = U32(e + 0xC); }
            else if (hit == 0x1D || hit == 0x1E) U32(o + 0x54) = U32(e + 0xC);
            else {
                u16 k = U16(o + 0x172);
                U16(o + 0x176) >>= 1;
                if (k == 4 || k == 0) take = 1;
                else U16(o + 0x172) = k - 1;
            }
        }
        if (take) {
            U16(o + 0x108) -= U16(o + 0x176);
            if (S16(o + 0x108) >= 0) {
                U16(o + 0x176) = 0;
                U32(o + 0x54) = U32(U32(RAP_EXT) + 0xC);
                if (!U8(o + 0x17B)) {
                    U8(o + 0x17B) = 10;
                    Effect_Attach(o, U32(RAP_TAB_BLOOD + (Rand8() & 3) * 8));
                }
            }
        }
    }
    Orig_Call1(U32(RAP_MAIN + U8(o + 0x54) * 4), o);
    U16(U32(RAP_EXT) + 0x1A)++;
    e = U32(RAP_EXT);
    if (U32(e + 0x24)) U32(e + 0x24)--;
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    Head_Update(o);
    Obj_UpdateMatrix(o);
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    Enemy_AttackHit(o);
    Raptor_Grab(o);
    U32(U32(RAP_EXT) + 0xC) = U32(o + 0x54);
    if (U32(o + 0x50) & 0x40000000) Obj_Disable(o);
}

/* state 0: model, animations, health and abilities by level (o+0x10C) */
void DC2CALL Raptor_Init(gptr o)                                    /* 0x41F660 */
{
    gptr g = G(), e = U32(RAP_EXT);
    if (U8(g + 0x1091) == 4 && U8(g + 0x1090) == 7) U32(o + 0x184) |= 0x40;   /* rides with the part */
    U32(o + 0x50) |= U8(G() + 0x927) ? 0xB : 0xF;
    Model_Setup(o, 0x633000);
    U32(o + 0x88) = 0x635550;
    U32(o + 0x12C) = 0x71FFF0;
    U32(o + 0x128) = 0x71FFE0;
    U32(o + 0x134) = 0x720008;
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    u16 lv = U16(o + 0x10C);
    if (lv & 0x10) { U8(o + 0x18D) = U8(e); U8(o + 0x18C) = (u8)((lv << 4) + 1); }
    if (lv & 0x20) U16(o + 0x10C) = U8(o + 0x18C) >> 4;
    gptr t = RAP_TAB_LEVEL + (U8(o + 0x10C) & 0xF) * 5;
    u8 h = U8(t);
    u16 hp = (u16)((Rand8() & 0x1F) + U16(RAP_TAB_HEALTH + h * 2));
    U16(o + 0x108) = hp;
    if (U8(G() + 0x108E) == 2) U16(o + 0x108) = (u16)(hp * 2);
    if (U8(G() + 0x108E) == 0) S16(o + 0x108) >>= 1;
    U16(o + 0x10A) = U16(o + 0x108);
    u8 a = U8(t + 1);
    U8(e + 0x1C) = (a & 0xF0) ? 0 : 1;
    U8(e + 0x19) = a & 0xF;
    a = U8(t + 2);
    U8(e + 0x22) = a >> 4;
    U8(e + 0x23) = a & 0xF;
    if (!U8(G() + 0x108E)) { U8(e + 0x20) = 0; U8(e + 0x21) = 0; }
    else if (U8(G() + 0x927)) { U8(e + 0x20) = 0; U8(e + 0x21) = 1; }
    else { U8(e + 0x20) = 1; U8(e + 0x21) = 1; }
    if (Flag_Check(2, 0x1A)) U8(e + 0x20) = 0;
    DrawSync(o);
    Obj_Set118(o, 1, 0x170C, 0x1F40);
    U8(o + 0x118) |= 0x20;
    Head_Reset(o);
    U8(o + 0x15F) = 3; U8(o + 0x160) = 2;
    U16(o + 0x164) = 0x40; U16(o + 0x166) = 0x80; U16(o + 0x16A) = 0; U16(o + 0x16E) = 0x2A0;
    U8(o + 0x15D) = 0;
    U8(o + 0x15E) = U8(G() + 0x927) ? 0 : 7;
    U8(o + 0x15C) = 0;
    U16(o + 0x168) = 0x200; U16(o + 0x16C) = 0x200;
    U32(o + 0x188) = 0;
    if (U8(o + 0x184) & 0x40) {
        U8(o + 0x15D) = U8(U32(G() + 0xB50) + 0xFF);
        U8(e + 0x20) = 0; U8(e + 0x21) = 0;
    }
    U32(o + 0x54) = 0x701;
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
}

/* state 1: think, then the sub-state */
void DC2CALL Raptor_Normal(gptr o)                                  /* 0x41F960 */
{
    Raptor_Think(o);
    if (U8(o + 0x55) >= 2) U32(o + 0x158) = 1;
    Orig_Call1(U32(RAP_NORM + U8(o + 0x55) * 4), o);
}

/* state 2: hit, by its kind */
void DC2CALL Raptor_Hit(gptr o)                                     /* 0x41F9A0 */
{
    U8(o + 0x18D) = 0;
    U32(o + 0x158) = 1;
    Orig_Call1(U32(RAP_HIT + U16(o + 0x172) * 4), o);
}

/* state 3: dies, the burning parts darken; scores; fades out */
void DC2CALL Raptor_Die(gptr o)                                     /* 0x41F9D0 */
{
    U32(o + 0x50) |= 0x30;
    if (U8(o + 0x170) & 2)
        for (gptr p = U32(o + 0x64); p; p = U32(p + 0x54))
            for (u32 k = 0x8C; k < 0x8F; k++)
                if (U8(p + k) > 0x30) U8(p + k) -= 4;
    switch (U8(o + 0x55)) {
    case 0: {
        u32 f = U8(o + 0xFF);
        if (f & 1) Anim_Start(o, 0x38, f & 3, 0, 6);
        else Anim_Start(o, 0x39, f & 3, 0, 4);
        if (U8(o + 0x170) & 2) Prop_Attach(o, 0x65AEAC);
        U32(o + 0xF0) = 0;
        U32(o + 0x50) &= ~8u;
        U16(o + 0xF4) = 0;
        U32(o + 0x158) = 1;
        Flag_Set(4, U8(o + 0x11C), 1);
        Score_Kill(o);
        U8(o + 0x55)++;
        break; }
    case 1:
        if (Anim_Step(o)) {
            Layers_Assign();
            Prop_SpawnC(0, o + 0x40, 2, 0x28, 1, 0);
            Obj_DeathFade(o);
            U8(o + 0x55)++;
        }
        break;
    case 2:
        Obj_DeathFade(o);
        break;
    }
}

/* the next state from where the target (the player, or the part it rides)
 * is: o+0x55 0 waits, 1 walks */
#define RAP_DIST (U32(U32(RAP_EXT) + 4))
#define TO(st) do { U32(o + 0x54) = (st); return; } while (0)
void DC2CALL Raptor_Think(gptr o)                                   /* 0x41FB20 */
{
    gptr t = U32(G() + 0x940);
    u8 r1 = Rand8(), r2 = Rand8();
    if (U8(o + 0x18D)) { U8(o + 0x18D)--; return; }
    if (U8(o + 0x184) & 0x40) t = U32(G() + 0xB50);
    U32(U32(RAP_EXT) + 4) = Dist2_XZ(o + 0x40, t + 0x40);
    U16(U32(RAP_EXT) + 8) = (u16)Angle_XZ(o + 0x40, o + 0xD8);
    if (U8(o + 0x17B)) return;
    if (U8(o + 0x55) == 0) {
        if (Wall_Ahead(o, 0x578)) {
            if (!U8(o + 0x13A)) { if (r1 >= 0x28) TO(0x501); return; }
            if (U8(U32(RAP_EXT) + 0x19) == 2) TO(0x301);
            goto wall;
        }
        if (!Obj_InSight(o, t + 0x40, 0xD59F80, 0x40)) {
            if (U16(t + 0x174) || RAP_DIST < 0x17D7840) return;
            TO(0x101);
        }
        if (U16(t + 0x5A) != U16(o + 0x5A)) TO(0x101);
        u8 ts = U8(t + 0x54);
        if (ts == 2) {
            if (RAP_DIST > 0x17D7840) return;
            if ((r2 & 0x80) && (U8(o + 0xFF) & 1)) TO(0x801);
            TO(0x501);
        }
        if (ts == 5) { if (U8(o + 0xFF) & 1) TO(0x801); TO(0x401); }
        if (U16(t + 0x174) && U8(t + 0x55) != 8) {
            if ((r2 & 0x80) && (U8(o + 0xFF) & 1)) TO(0x801);
            return;
        }
        if (Enemy_FindState(o, 2)) { if (RAP_DIST < 0x895440) TO(0x501); TO(0x401); }
        u8 odd = (u8)(U8(o + 0xFF) + r2) & 1;
        if (!U8(G() + 0x108E) && U32(U32(RAP_EXT) + 0x24)) {
            if (RAP_DIST < 0x895440) TO(0x501);
            if (r1 & 1) TO(0x801);
            TO(0x401);
        }
        u32 d = RAP_DIST;
        if (d <= 0x4C4B40) { if (odd) TO(0x201); TO(0x20201); }
        if (d < 0xC65D40 || !odd) return;
        TO(0x40201);
    }
    if (U8(o + 0x55) != 1) return;
    if (Wall_Ahead(o, 0x578)) {
        if (!U8(o + 0x13A)) TO(0x1);
        if (U8(U32(RAP_EXT) + 0x19) == 2) TO(0x301);
    wall:
        if (r1 > 0x80) TO(0x301);
        if (r1 > 0x40) TO(0x401);
        TO(0x501);
    }
    if (Obj_InSight(o, t + 0x40, 0xF42400, 0x80)) {
        if (U16(t + 0x5A) != U16(o + 0x5A)) return;
        if (Enemy_FindState(o, 2)) { if (RAP_DIST < 0x895440) TO(0x501); TO(0x801); }
        if (U16(t + 0x174)) TO(0x801);
        if (!(r1 & 0x10)) return;
        if (!U8(G() + 0x108E) && U32(U32(RAP_EXT) + 0x24)) {
            if (RAP_DIST <= 0x895440) TO(0x501);
            if (r1 & 1) TO(0x801);
            TO(0x401);
        }
        if ((r1 & 1) && r2) TO(0x40201);
        TO(0x30201);
    }
    if (U16(t + 0x174) || RAP_DIST > 0x5F5E10) return;
    TO(0x1);
}
#undef TO

/* holding the player (sub-state 2 of a state, o+0x119 not negative): by o+0x56 */
void DC2CALL Raptor_Grab(gptr o)                                    /* 0x420A00 */
{
    gptr pl = U32(G() + 0x940);
    if (S8(o + 0x119) < 0 || U8(o + 0x55) != 2) return;
    U8(pl + 0x11B) = 0;
    Orig_Call2(U32(RAP_GRAB + U8(o + 0x56) * 4), o, pl);
}

/* ---- state 1: the normal sub-states (o+0x55), each with steps (o+0x56) ------------ */
/* ext +8 the angle to the target, +0xA a turn per frame, +0x28 frames waited */

/* 0: waits, facing the target; turns on the spot when it is far round */
void DC2CALL Raptor_Wait(gptr o)                                    /* 0x41FF30 */
{
    switch (U8(o + 0x56)) {
    case 0:
        Anim_Start(o, 0, U8(o + 0xFF) & 3, 1, 10);
        U32(o + 0xF0) = 0; U16(o + 0x174) = 0; U16(o + 0xF4) = 0;
        U8(o + 0x56)++;
        U16(U32(RAP_EXT) + 0x28) = 0;
        U32(o + 0x158) = 2;
        break;
    case 1: {
        Anim_Step(o);
        if (U16(U32(RAP_EXT) + 0x28) >= 0x1C2) U32(o + 0x54) = 0x30201;
        u16 cur = U16(o + 0x4A), tg = U16(U32(RAP_EXT) + 8);
        u32 d = (u32)(tg - cur) & 0xFFF, ad = d > 0x800 ? 0x1000 - d : d;
        if (ad <= 0x200) { U16(o + 0x4A) += (u16)Angle_Approach(cur, 0x28, tg); U16(o + 0xF4) = 0x28; break; }
        if (ad <= 0x400) { U16(o + 0x4A) += (u16)Angle_Approach(cur, 0x50, tg); U16(o + 0xF4) = 0xA; break; }
        Anim_Start(o, d > 0x800 ? 0x16 : 9, 0, 0, 8);
        if (ad <= 0x600) U16(U32(RAP_EXT) + 0xA) = (u16)(d > 0x800 ? -15 - (s32)ad / 20 : (s32)ad / 20 + 15);
        else U16(U32(RAP_EXT) + 0xA) = d > 0x800 ? 0xFF99 : 0x67;
        U16(o + 0xF4) = 0xA;
        U8(o + 0x56) = 2;
        break; }
    case 2:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        U16(o + 0x4A) = (U16(U32(RAP_EXT) + 0xA) + U16(o + 0x4A)) & 0xFFF;
        break;
    }
    U16(U32(RAP_EXT) + 0x28)++;
}

/* the walk or run (ext+0x1C picks which) */
static void Raptor_WalkStart(gptr o, u32 frame)
{
    Anim_Start(o, U8(0x7032A4 + (U8(U32(RAP_EXT) + 0x1C) & 1)), frame, 1, 4);
    U16(o + 0xF4) = U8(0x7032A8 + (U8(U32(RAP_EXT) + 0x1C) & 1));
}

/* 1: walks toward the target (turns round first when it is behind) */
void DC2CALL Raptor_Walk(gptr o)                                    /* 0x420190 */
{
    switch (U8(o + 0x56)) {
    case 0: {
        U32(o + 0xF0) = 0;
        u32 d = (u16)(U16(U32(RAP_EXT) + 8) - U16(o + 0x4A)) & 0xFFF;
        if (d >= 0x700 && d <= 0x900) {
            Anim_Start(o, 9, 0, 0, 8);
            U16(o + 0xF4) = 0;
            U8(o + 0x56) = 2;
            U16(U32(RAP_EXT) + 0xA) = (u16)(d / 20 + 15);
            return;
        }
        u8 b = U8(o + 0xFF) & 3;
        U32(o + 0x158) = 2;
        if ((Rand8() & 0x20) && (b & 1)) {
            Anim_Start(o, 0xE, b, 0, 8);
            U16(o + 0xF4) = 0xA;
            U8(o + 0x56) = 3;
            return;
        }
        Raptor_WalkStart(o, b);
        U8(o + 0x56)++;
        break; }
    case 1: {
        Anim_Step(o);
        gptr e = U32(RAP_EXT);
        u16 cur = U16(o + 0x4A);
        U16(o + 0x4A) += (u16)Angle_Approach(cur, U8(e + 0x1C) ? 0x60 : 0x80, U16(e + 8));
        break; }
    case 2:
        if (Anim_Step(o)) U32(o + 0x54) = 0x101;
        U16(o + 0x4A) = (U16(U32(RAP_EXT) + 0xA) + U16(o + 0x4A)) & 0xFFF;
        /* fall through */
    case 3:
        if (Anim_Step(o)) {
            Raptor_WalkStart(o, U8(o + 0xFF) & 3);
            U8(o + 0x56) = 1;
        }
        break;
    }
}

/* 2: attacks (the steps in 0x72007C); the player dies at negative health */
void DC2CALL Raptor_Attack(gptr o)                                  /* 0x4203B0 */
{
    gptr pl = U32(G() + 0x940);
    U32(U32(RAP_EXT) + 0x24) = ((U8(o + 0xFF) & 3) + 3) * 30;
    Orig_Call1(U32(RAP_ATTACK + U8(o + 0x56) * 4), o);
    if (S16(pl + 0x108) < 0) GameOver_Start(0);
}

/* 3: jumps (o+0x11A bit 6) or climbs; the level from the floor under it */
void DC2CALL Raptor_Jump(gptr o)                                    /* 0x420410 */
{
    gptr v = Tmp_Push(8);
    U16(v) = U16(o + 0x40); U16(v + 2) = 0x7918; U16(v + 4) = U16(o + 0x44);
    U16(o + 0x5A) = (u16)((s32)(s16)Floor_Height(v, o + 0x40, 0x40, 0) / -1000);
    Tmp_Pop(8);
    if (U8(o + 0x11A) & 0x40) Raptor_JumpOver(o);
    else Raptor_Climb(o);
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
}

/* 4: a cry or a sniff, then back to waiting (sooner when the target is out of sight) */
void DC2CALL Raptor_Cry(gptr o)                                     /* 0x4204A0 */
{
    gptr pl = U32(G() + 0x940);
    switch (U8(o + 0x56)) {
    case 0: {
        u8 b = U8(o + 0xFF) & 3;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        u8 r = Rand8();
        if (r > 0x64) {
            if (r & 1) { Anim_Start(o, 0xE, b, 0, 8); U16(o + 0xF4) = 0xA; }
            else { U32(o + 0x7C) = 0; Anim_Start(o, 0xC, b, 0x40, 8); }
        } else {
            if (r & 1) Anim_Start(o, 7, b, 8, 10);
            else { U32(o + 0x7C) = 0; Anim_Start(o, 0xD, b, 0x40, 4); }
        }
        U8(o + 0x56)++;
        break; }
    case 1:
        if (!Obj_InSight(o, pl + 0x40, 0x17D7840, 0x280)) U32(o + 0x54) = 1;
        /* fall through */
    case 2:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        break;
    }
}

/* 5: backs off, turning to the target */
void DC2CALL Raptor_Back(gptr o)                                    /* 0x4205D0 */
{
    if (!U8(o + 0x56)) {
        Anim_Start(o, 2, U8(o + 0xFF) & 3, 1, 10);
        U8(o + 0x56)++;
        U32(o + 0xF0) = 0;
        U16(o + 0xF4) = 0xFFDC;
        return;
    }
    U16(o + 0x4A) += (u16)Angle_Approach(U16(o + 0x4A), 0x40, U16(U32(RAP_EXT) + 8));
    if (Anim_Step(o)) U32(o + 0x54) = (U8(o + 0xFF) & 1) ? 1 : 0x401;
}

/* 6: eats (o+0x172 more bites), not pushed or colliding meanwhile */
void DC2CALL Raptor_Eat(gptr o)                                     /* 0x420660 */
{
    switch (U8(o + 0x56)) {
    case 0: {
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U32(o + 0x50) |= 0x2020;
        u8 r = Rand8();
        if (r & 0x80) {
            Anim_Start(o, 0x3B, U8(o + 0xFF) & 3, 1, 10);
            U8(o + 0x56)++;
            U16(o + 0x172) = (u8)(U8(o + 0x180) + r) & 1;
        } else {
            U32(o + 0x7C) = 0;
            Anim_Start(o, 0x3C, U8(o + 0x180) & 3, 0x40, (U8(o + 0xFF) & 7) + 4);
            U8(o + 0x56) = 2;
            U16(o + 0x172) = 0;
        }
        break; }
    case 1:
        if (!Anim_Step(o)) break;
        if (U16(o + 0x172)) { U16(o + 0x172)--; break; }
        U32(o + 0x7C) = 0;
        Anim_Start(o, 0x3C, U8(o + 0x180) & 3, 0x40, (U8(o + 0xFF) & 7) + 4);
        U8(o + 0x56)++;
        break;
    case 2:
        if (Anim_Step(o)) {
            U32(o + 0x50) &= 0xFFFFDFDF;
            U16(o + 0x174) &= 0xFFFD;
            U8(U32(RAP_EXT) + 0x18) = 0;
            U32(o + 0x54) = 1;
        } else if (U16(o + 0x92) >= 0x2A) {
            U32(o + 0x50) &= ~0x20u;
            U16(o + 0x174) &= 0xFFFD;
            U8(U32(RAP_EXT) + 0x18) = 0;
        }
        break;
    }
}

/* 7: the entrance the room gave it (o+0x18C low bits) */
void DC2CALL Raptor_Enter(gptr o)                                   /* 0x4207E0 */
{
    if (U8(o + 0x56)) {
        Anim_Step(o);
        if (!U8(o + 0x18D)) { U32(o + 0x54) = 1; U32(o + 0x50) &= 0xFFFFF7EF; }
        return;
    }
    switch (U8(o + 0x18C) & 0xF) {
    case 0:
        U32(o + 0x50) &= 0xFFFFF7EF;
        U8(o + 0x18D) = 0;
        Anim_Start(o, 0, 0, 1, 0);
        U32(o + 0x54) = 1;
        break;
    case 1:
        Anim_Start(o, 0, 0, 1, 0);
        U8(o + 0x56)++;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0x28; U16(o + 0x174) = 0;
        break;
    case 2:
        if (U8(o + 0xFF) & 2) { Anim_Start(o, 0x15, 0, 1, 0); U16(o + 0xF4) = 0x6E; }
        else { Anim_Start(o, 1, 0, 1, 0); U16(o + 0xF4) = 0x96; }
        U8(o + 0x56)++;
        U32(o + 0xF0) = 0;
        break;
    case 3:
        U32(o + 0x54) = 0x40201;
        U8(o + 0x18D) = 0;
        break;
    case 4:
        Anim_Start(o, 0x14, 0, 0, 4);
        U8(o + 0x174) |= 4;
        U32(o + 0x50) |= 0x840;
        U8(o + 0x118) |= 0x20;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0x3C;
        U16(U32(RAP_EXT) + 0x14) = 0xFE5C;
        U32(o + 0x54) = 0x20301;
        break;
    }
}

/* 8: a feint, while the target stays in sight */
void DC2CALL Raptor_Feint(gptr o)                                   /* 0x420960 */
{
    gptr pl = U32(G() + 0x940);
    switch (U8(o + 0x56)) {
    case 0: {
        u8 f = U8(o + 0xFF);
        U32(o + 0x7C) = 0;
        Anim_Start(o, 0x18, f & 1, 0x40, ((f + 2) & 7) + 2);
        U8(o + 0x56)++;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        break; }
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        if (!Obj_InSight(o, pl + 0x40, 0xF42400, 0x200)) U32(o + 0x54) = 1;
        break;
    }
}

/* ---- state 1/2: the attack, by its kind (o+0x56) and step (o+0x57) ----------------- */
/* the player's animations for these, in the raptor's animation table, per
   character (G+0x108F) */
#define PL_ANIM(o, off) U32(U32((o) + 0x88) + U8(G() + 0x108F) * 0x80 + (off))

/* the raptor moves with what the target is pushed by; the target stays at it */
static void Raptor_Hold(gptr o, gptr p, int y)
{
    U16(o + 0x40) += U16(p + 0xE8); U16(o + 0x42) += U16(p + 0xEA); U16(o + 0x44) += U16(p + 0xEC);
    U16(p + 0x4A) = U16(o + 0x4A);
    U16(p + 0x40) = U16(o + 0x40);
    if (y) U16(p + 0x42) = U16(o + 0x42);
    U16(p + 0x44) = U16(o + 0x44);
    gptr s = U32(RAP_EXT) + 2;                      /* struggling: the pad shaken */
    U16(s) += (u16)Pad_Shake();
}

/* the target turned round, its body too, on a new animation */
static void Raptor_TurnTarget(gptr p, gptr anim)
{
    U16(p + 0x4A) = (U16(p + 0x4A) - 0x800) & 0xFFF;
    gptr b = U32(p + 0x64);
    U16(b + 0x4A) = (U16(b + 0x4A) - 0x800) & 0xFFF;
    Anim_Set(p, anim, 0, 0x40, 0);
    Obj_UpdateMatrix(p);
}

static void Raptor_Bite(gptr o, gptr p)
{
    Flash_Play(o, 0x65B09C);
    Effect_Attach(o, 0x659E0C);
    U16(p + 0x108) -= (u16)Damage_Scale(U16(p + 0x176));
}

/* the leap that pins the target down (steps 0..0xC) */
void DC2CALL Raptor_Pounce(gptr o)                                  /* 0x420A40 */
{
    gptr p = U32(G() + 0x940);
    if (U8(o + 0x184) & 0x40) p = U32(G() + 0xB50);
    u8 st = U8(o + 0x57);
    switch (st) {
    case 0:
        Anim_Start(o, 0x1A, 0, 0x40, 10);
        U8(o + 0x57)++;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0x14;
        break;
    case 1:
        if (Anim_Step(o)) { U32(o + 0x54) = 1; break; }
        if (U16(o + 0x92) != 0x12 || Obj_InSight(o, p + 0x40, 0x5F5E10, 0x100)) break;
        Anim_Start(o, 0x20, 0, 0, 10);
        U8(o + 0x57) = 0xC;
        U16(o + 0xF4) = 0;
        break;
    case 2: case 3:                                 /* pinned down */
        Raptor_Hold(o, p, 1);
        if (!Anim_Step(o)) break;
        if (U8(o + 0x57) == 2) {
            Flash_Play(o, 0x65B09C);
            Anim_Start(p, PL_ANIM(o, 0x120), 0, 0x41, 0);
            Anim_Start(o, 0x1C, 0, 0x41, 0);
            U16(p + 0x108) -= (u16)Damage_Scale(U16(p + 0x176));
            U8(o + 0x57)++;
            break;
        }
        Flash_Play(o, 0x65B090);
        if (U16(U32(RAP_EXT) + 2) > 0xA) {
            Anim_Start(p, PL_ANIM(o, 0x130), 0, 0x40, 0);
            Anim_Start(o, 0x1F, 0, 0x40, 0);
            U8(o + 0x57)++;
        } else {
            Anim_Start(p, PL_ANIM(o, 0x128), 0, 0x40, 0);
            Anim_Start(o, 0x1D, 0, 0x40, 0);
            U16(p + 0x108) -= (u16)Damage_Scale(U16(p + 0x176));
            U8(o + 0x57) = 5;
        }
        break;
    case 4:                                         /* shaken off */
        if (!Anim_Step(o)) break;
        Raptor_TurnTarget(p, PL_ANIM(o, 0x16C));
        U32(p + 0x50) &= ~0x900u;
        U16(p + 0x174) = 0;
        Player_Bleed(0x1F4);
        U32(p + 0x1DC) = 8;
        U32(p + 0x54) = 1;
        U32(o + 0x50) = (U32(o + 0x50) & ~0x800u) | 8;
        U32(o + 0x54) = 1;
        break;
    case 5:                                         /* thrown off, hurt */
        if (!Anim_Step(o)) break;
        Raptor_TurnTarget(p, PL_ANIM(o, 0x164));
        U32(p + 0x50) &= ~0x900u;
        Player_Bleed(0x1F4);
        U32(p + 0x54) = 0x30202;
        Player_Weapon(p);
        U32(o + 0x50) = (U32(o + 0x50) & ~0x800u) | 8;
        U32(o + 0x54) = 1;
        break;
    case 6: case 7:                                 /* bites, from the front */
        Raptor_Hold(o, p, 0);
        if (!Anim_Step(o)) break;
        if (U8(o + 0x57) == 6) {
            Raptor_Bite(o, p);
            Anim_Start(p, PL_ANIM(o, 0x140), 0, 0x40, 0);
            Anim_Start(o, 0x23, 0, 0x40, 8);
            U8(o + 0x57)++;
            break;
        }
        U16(U32(RAP_EXT) + 2) += 6;
        if (U16(U32(RAP_EXT) + 2) > 0xA) {
            Flash_Play(o, 0x65B090);
            Raptor_TurnTarget(p, PL_ANIM(o, 0x148));
            U32(p + 0x54) = 0x30202;
            Anim_Start(o, 0x23, 0, 0x40, 8);
            U8(o + 0x57) = 0xA;
        } else {
            Raptor_Bite(o, p);
            Flash_Play(o, 0x65B09C);
            Anim_Start(p, PL_ANIM(o, 0x140), 0, 0x40, 0);
            Anim_Start(o, 0x23, 0, 0x40, 0);
            U16(U32(RAP_EXT) + 2) += 6;
        }
        break;
    case 8: case 9:                                 /* bites, from behind */
        Raptor_Hold(o, p, 0);
        if (!Anim_Step(o)) break;
        if (U8(o + 0x57) == 8) {
            Raptor_Bite(o, p);
            Anim_Start(p, PL_ANIM(o, 0x144), 0, 0x40, 0);
            Anim_Start(o, 0x23, 0, 0x40, 8);
            U16(U32(RAP_EXT) + 2) = 0;
            U8(o + 0x57)++;
            break;
        }
        U16(U32(RAP_EXT) + 2) += 6;
        if (U16(U32(RAP_EXT) + 2) > 0xA) {
            Flash_Play(o, 0x65B090);
            Anim_Set(p, PL_ANIM(o, 0x14C), 0, 0x40, 0);
            Obj_UpdateMatrix(p);
            U32(p + 0x54) = 0x30302;
            Anim_Start(o, 0x23, 0, 0x40, 8);
            U8(o + 0x57) = 0xA;
        } else {
            Raptor_Bite(o, p);
            Flash_Play(o, 0x65B09C);
            Anim_Start(p, PL_ANIM(o, 0x144), 0, 0x40, 0);
            Anim_Start(o, 0x23, 0, 0x40, 8);
            U16(U32(RAP_EXT) + 2) += 6;
        }
        break;
    case 0xA:                                       /* lets go */
        Anim_Step(o);
        if (U16(o + 0x92) <= 5) break;
        Player_Bleed(0x1F4);
        Anim_Start(o, 0x32, 0, 0x40, 8);
        U32(o + 0x50) = (U32(o + 0x50) & ~0x10u) | 8;
        U8(o + 0x57)++;
        break;
    case 0xB:                                       /* then eats */
        if (Anim_Step(o)) {
            U16(o + 0x174) = 2;
            U32(o + 0x50) = (U32(o + 0x50) & 0xFFFFF7AF) | 0x2020;
            U16(o + 0x172) = 0;
            Anim_Start(o, 0x3B, U8(o + 0xFF) & 3, 1, 10);
            U32(o + 0x54) = 0x10601;
        } else if (U16(o + 0x92) >= 0xE) U8(o + 0x174) |= 2;
        break;
    case 0xC:                                       /* missed */
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        break;
    }
}

/* the target of an attack: the player, or the part it rides on */
static gptr Raptor_Target(gptr o)
{
    return (U8(o + 0x184) & 0x40) ? U32(G() + 0xB50) : U32(G() + 0x940);
}

/* the target hit, with this flash unless it rides (pl: the player) */
static gptr Raptor_HitTarget(gptr o, gptr pl)
{
    if (U8(o + 0x184) & 0x40) return U32(G() + 0xB50);
    Flash_Play(o, 0x65B090);
    return pl;
}

/* damage per level (ext+0x23), 9 per row: +0 claw, +1 lunge, +2 its push,
   +3 tail, +4 jump, +6 bite from the front, +7 from behind, +8 */
#define RAP_DMG(k) U8(0x7031C4 + U8(U32(RAP_EXT) + 0x23) * 9 + (k))

/* the target knocked back (0x202, stronger) or hit (0x102) */
static void Raptor_Knock(gptr p, u32 k, int strong)
{
    U16(p + 0x176) = (u16)(RAP_DMG(k) + (strong ? 0x28 : 0));
    U32(p + 0x54) = strong ? 0x202 : 0x102;
}

/* the target at the raptor, held (state 5 of the player) */
static void Raptor_Seize(gptr o, gptr p, u16 y)
{
    U16(p + 0x4A) = U16(o + 0x4A);
    U16(p + 0x40) = U16(o + 0x40);
    U16(p + 0x42) = y;
    U16(p + 0x44) = U16(o + 0x44);
    U32(p + 0x1D8) = o;
}

/* lunge (attack 0/1): seizes the target from the front when it may */
void DC2CALL Raptor_LungeHit(gptr o, gptr pl)                       /* 0x421290 */
{
    if (U8(o + 0x57) != 1) return;
    gptr p = Raptor_HitTarget(o, pl);
    Effect_SpawnAt(o, 0x659A40, 0);
    U16(p + 0x174) = 1;
    if (Enemy_HitDir(o) == 2) {
        gptr e = U32(RAP_EXT);
        if (U8(e + 0x22) && U8(e + 0x20)) {
            Anim_Start(o, 0x1B, 0, 0x40, 0);
            U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
            U32(o + 0x50) = (U32(o + 0x50) & ~8u) | 0x800;
            U8(o + 0x57)++;
            U16(U32(RAP_EXT) + 2) = 0;
            U32(G() + 0x92C) = 0x4A56D0;
            Anim_Start(p, PL_ANIM(o, 0x118), 0, 0x40, 0);
            U32(p + 0xF0) = 0; U16(p + 0xF4) = 0;
            U16(p + 0x108) -= (u16)Damage_Scale(RAP_DMG(1));
            U16(p + 0x176) = RAP_DMG(2);
            U32(p + 0x50) |= 0x900;
            Raptor_Seize(o, p, U16(o + 0x42));
            U32(p + 0x54) = 0x205;
            return;
        }
    } else if (!U16(p + 0x178) && U8(U32(RAP_EXT) + 0x21)) {
        Raptor_Knock(p, 1, 1);
        return;
    }
    U16(p + 0x176) = RAP_DMG(1);
    U32(p + 0x54) = 2;
}

/* attack 2: a bite, to the side of the target's angle (ext+8) */
void DC2CALL Raptor_SideBite(gptr o)                                /* 0x421460 */
{
    gptr p = Raptor_Target(o);
    switch (U8(o + 0x57)) {
    case 0:
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        if (S16(U32(RAP_EXT) + 8) >= 0x800) { Anim_Start(o, 0x24, 0, 0, 8); U8(o + 0x57) = 1; }
        else { Anim_Start(o, 0x25, 0, 0, 8); U8(o + 0x57) = 2; }
        break;
    case 1: case 2:
        if (Anim_Step(o)) { U32(o + 0x54) = 1; break; }
        if (U16(o + 0x92) != 0x16 || Obj_InSight(o, p + 0x40, 0x5F5E10, 0x200)) break;
        Anim_Start(o, U8(o + 0x57) == 1 ? 0x26 : 0x27, 0, 0, 10);
        U8(o + 0x57) = 3;
        break;
    case 3:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        break;
    }
}

/* its hit: turns the target aside (o+0x119 3: in the air) */
void DC2CALL Raptor_SideBiteHit(gptr o, gptr pl)                    /* 0x4215A0 */
{
    gptr p = Raptor_HitTarget(o, pl);
    if (U8(o + 0x119) == 3 && U8(U32(RAP_EXT) + 0x21)) {
        if (U8(o + 0x57) == 1) {
            Raptor_Knock(p, 6, 1);
            U16(p + 0x4A) = (U16(o + 0x4A) - 0x400) & 0xFFF;
        } else if (U8(o + 0x57) == 2) {
            Raptor_Knock(p, 6, 1);
            U16(p + 0x4A) = (U16(o + 0x4A) + 0x400) & 0xFFF;
        }
    } else Raptor_Knock(p, 6, 0);
    U16(p + 0x174) = 1;
    Enemy_HitDir(o);
    Fn_48C0B0(o, 0x3B);
}

/* attack 3: the jump at the target (harder raptors run in first) */
void DC2CALL Raptor_Leap(gptr o)                                    /* 0x4216B0 */
{
    gptr p = Raptor_Target(o);
    switch (U8(o + 0x57)) {
    case 0:
        Anim_Start(o, 0x21, 0, 0x40, 4);
        U32(o + 0xF0) = 0;
        if (U8(G() + 0x108E) && (U8(o + 0x10C) & 0xF) > 2) { U16(o + 0xF4) = 0x50; U8(o + 0x57)++; break; }
        U16(o + 0xF4) = 0; U8(o + 0x57)++;
        break;
    case 1:
        U16(o + 0x4A) += (u16)Angle_Approach(U16(o + 0x4A), 0x10, U16(U32(RAP_EXT) + 8));
        if (!Anim_Step(o)) break;
        if (U16(p + 0x174)) { U32(o + 0x54) = 1; U32(o + 0x50) &= ~0x800u; break; }
        if (U8(p + 0x55) == 2 && (Rand8() & 0x3F) > 0x20) { U32(o + 0x54) = 0x401; break; }
        Anim_Start(o, 3, 0, 0, 8);
        U16(o + 0xF4) = 0; U8(o + 0x57)++;
        break;
    case 2:
        if (Anim_Step(o)) { U32(o + 0x54) = 1; U32(o + 0x50) = (U32(o + 0x50) & ~0x800u) | 8; }
        break;
    case 3:                                         /* on the target: rides its push */
        if (U16(o + 0x92) <= 0xC) {
            U16(o + 0x40) += U16(p + 0xE8); U16(o + 0x42) += U16(p + 0xEA); U16(o + 0x44) += U16(p + 0xEC);
        }
        if (!Anim_Step(o)) break;
        U32(o + 0x50) = (U32(o + 0x50) & ~0x800u) | 8;
        U16(o + 0x4A) = (U16(o + 0x4A) - 0x440) & 0xFFF;
        Anim_Set(o, 0, 0, 1, 0);
        U32(o + 0x54) = 0x10001;
        break;
    }
}

/* the target down under the raptor (state 3 of the player), or dead */
static void Raptor_PinDown(gptr o, gptr p, u32 dmg, gptr anim, u32 st)
{
    U16(p + 0x108) -= (u16)Damage_Scale(dmg);
    if (S16(p + 0x108) < 0) { U32(p + 0x54) = 3; return; }
    Anim_Start(p, anim, 0, 0x40, 0);
    U32(p + 0x50) |= 0x800;
    U32(p + 0xF0) = 0; U16(p + 0xF4) = 0;
    U16(p + 0x174) = 1;
    if (st == 0x10302) U16(p + 0x4A) = U16(o + 0x4A);
    U32(p + 0x1D8) = o;
    U16(p + 0x1E8) = 0x64;
    U32(p + 0x54) = st;
    Player_Weapon(p);
}

/* the leap's hit: lands on the target's back, or on its front */
void DC2CALL Raptor_LeapHit(gptr o, gptr pl)                        /* 0x421870 */
{
    if (U8(o + 0x57) != 1) return;
    gptr p = Raptor_HitTarget(o, pl);
    Effect_SpawnAt(o, 0x659ABC, 0);
    Enemy_HitDir(o);
    U16(p + 0x174) = 1;
    u32 dp = ((U16(p + 0x4A) + 0x200) >> 10) & 3, dr = ((U16(o + 0x4A) + 0x200) >> 10) & 3;
    gptr e = U32(RAP_EXT);
    if (dp == dr) {                                 /* from behind */
        if (U8(e + 0x22) || !U8(e + 0x20)) {
            if (U8(e + 0x21)) Raptor_Knock(p, 0, 1);
            else Raptor_Knock(p, 0, 0);
            return;
        }
        Anim_Start(o, 0x2D, 0, 0x40, 2);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U16(o + 0x40) += U16(p + 0xE8); U16(o + 0x42) += U16(p + 0xEA); U16(o + 0x44) += U16(p + 0xEC);
        gptr m = Tmp_Push(0x20), v = Tmp_Push(8);
        RotMatrix(o + 0x48, m);
        U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = 0xFDA8;
        ApplyMatrixSV(m, v, v);
        U16(o + 0x40) += U16(v); U16(o + 0x42) += U16(v + 2); U16(o + 0x44) += U16(v + 4);
        Tmp_Pop(8); Tmp_Pop(0x20);
        U32(o + 0x50) = (U32(o + 0x50) & ~8u) | 0x800;
        U8(o + 0x57) = 3;
        Raptor_PinDown(o, p, RAP_DMG(8), PL_ANIM(o, 0x180), 0x10302);
        return;
    }
    if (dp != ((dr - 2) & 3) || U8(e + 0x22)) { Raptor_Knock(p, 0, 0); return; }
    if ((U8(o + 0x10C) & 0xF) <= 2 || !U8(e + 0x20)) { Raptor_Knock(p, 0, 0); return; }
    U16(o + 0x40) += U16(p + 0xE8); U16(o + 0x42) += U16(p + 0xEA); U16(o + 0x44) += U16(p + 0xEC);
    U32(o + 0x50) &= ~8u;
    U8(o + 0x57) = 2;
    Raptor_PinDown(o, p, RAP_DMG(7), PL_ANIM(o, 0x134), 0x10202);
}

/* attack 4: the tail */
void DC2CALL Raptor_Tail(gptr o)                                    /* 0x421C10 */
{
    switch (U8(o + 0x57)) {
    case 0: {
        gptr e = U32(RAP_EXT);
        if (U8(e + 0xC) == 1 && U8(e + 0xD) == 1) Anim_Start(o, 4, 4, 0x40, 3);
        else Anim_Start(o, 4, 0, 0x40, 6);
        U8(o + 0x57)++;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        break; }
    case 1: {
        if (Anim_Step(o)) {
            U32(o + 0x54) = 1;
            U32(o + 0x50) &= 0xFFFFF7CF;
            U16(o + 0x174) &= 0xFFFB;
        }
        u16 f = U16(o + 0x92);
        if (f >= 0x18) U16(o + 0x174) &= 0xFFFB;
        else if (f >= 0xD) U8(o + 0x174) |= 4;
        break; }
    }
}

/* the tail's hit; when the target is not down, the raptor catches it */
void DC2CALL Raptor_TailHit(gptr o, gptr pl)                        /* 0x421CC0 */
{
    if (U8(o + 0x57) != 1) return;
    gptr p = Raptor_HitTarget(o, pl);
    U16(p + 0x174) = 1;
    Effect_SpawnAt(o, 0x659CC8, 0);
    Enemy_HitDir(o);
    gptr e = U32(RAP_EXT);
    if (!(U8(p + 0x178) & 1) && !U8(o + 0x119)) {
        if (!U8(e + 0x20)) {
            if (U8(e + 0x21) && !U16(p + 0x178)) Raptor_Knock(p, 3, 1);
            else Raptor_Knock(p, 3, 0);
            return;
        }
        U16(o + 0x40) += U16(p + 0xE8); U16(o + 0x42) += U16(p + 0xEA); U16(o + 0x44) += U16(p + 0xEC);
        Anim_Start(o, 0x22, 0, 0x40, 0);
        U16(o + 0x174) &= 0xFFFB;
        U32(o + 0x50) = (U32(o + 0x50) & ~0x18u) | 0x800;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U16(U32(RAP_EXT) + 2) = 0;
        u32 dmg;
        if (U16(p + 0x178)) {
            U32(o + 0x54) = 0x6000201;
            Anim_Start(p, PL_ANIM(o, 0x138), 0, 0x40, 0);
            dmg = RAP_DMG(3);
        } else {
            U32(o + 0x54) = 0x8000201;
            Anim_Start(p, PL_ANIM(o, 0x13C), 0, 0x40, 0);
            dmg = RAP_DMG(3) + 0x28;
        }
        U16(p + 0x108) -= (u16)Damage_Scale(dmg);
        U16(p + 0x176) = RAP_DMG(4);
        U32(p + 0x50) |= 0x800;
        U32(p + 0xF0) = 0; U16(p + 0xF4) = 0;
        Raptor_Seize(o, p, (u16)(S16(p + 0x5A) * -1000));
        U32(G() + 0x92C) = 0x4A56D0;
        U32(p + 0x54) = 0x205;
        return;
    }
    if (U8(e + 0x21) && !U16(p + 0x178)) Raptor_Knock(p, 3, 1);
    else Raptor_Knock(p, 3, 0);
}

/* ---- state 1/3: jumps; ext +0x10 the angle, +0x14 the vertical speed, +0x16 the
   level it jumps to ------------------------------------------------------------------ */

/* in the air: faster forward, falling */
static void Raptor_Fall(gptr o)
{
    if (S16(o + 0xF4) < 0xC8) U16(o + 0xF4) += 0x1E;
    U16(U32(RAP_EXT) + 0x14) += 0x3C;
    U16(o + 0x42) += U16(U32(RAP_EXT) + 0x14);
}

/* landed on level o+0x5A */
static void Raptor_Land(gptr o, u32 anim)
{
    Anim_Start(o, anim, 0, 0x40, 4);
    U16(o + 0x174) &= 0xFFFB;
    U16(o + 0x42) = (u16)(S16(o + 0x5A) * -1000);
    U32(o + 0x50) &= 0xFFFFF7AF;
    U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
    U8(o + 0x118) &= 0xDF;
    U8(o + 0x56)++;
    Fn_48C0B0(o, 0x36);
}

/* up onto a ledge two levels higher */
void DC2CALL Raptor_Climb(gptr o)                                   /* 0x421F50 */
{
    switch (U8(o + 0x56)) {
    case 0:
        Anim_Start(o, 7, 0, 0, U8(o + 0xFF));
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U16(U32(RAP_EXT) + 0x10) = U16(o + 0x4A);
        U32(o + 0x50) |= 0x840;
        U16(U32(RAP_EXT) + 0x16) = U16(o + 0x5A) + 2;
        U8(U32(RAP_EXT) + 0x1E) = 0;
        U8(o + 0x56)++;
        break;
    case 1:
        U16(o + 0x4A) += (u16)Angle_Approach(U16(o + 0x4A), 0x40, U16(U32(RAP_EXT) + 0x10));
        if (!Anim_Step(o)) break;
        Anim_Start(o, 0x13, 0, 0, 0);
        U8(o + 0x174) |= 4;
        U32(o + 0x50) |= 0x840;
        U8(o + 0x118) |= 0x20;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0x3C;
        U16(U32(RAP_EXT) + 0x14) = 0xFD6C;
        U8(o + 0x56)++;
        break;
    case 2:
        Raptor_Fall(o);
        if (S16(U32(RAP_EXT) + 0x14) < 0) { Anim_Step(o); break; }
        Anim_Start(o, 0xF, 0, 0, 2);
        U8(o + 0x56)++;
        break;
    case 3:
        Raptor_Fall(o);
        if (-1000 * (s32)U16(U32(RAP_EXT) + 0x16) < S16(o + 0x42)) U32(o + 0x50) &= ~0x40u;   /* above the ledge: no walls */
        if (-1000 * S16(o + 0x5A) > S16(o + 0x42)) { Anim_Step(o); break; }
        Raptor_Land(o, 0x10);
        break;
    case 4:
        if (Anim_Step(o)) U32(o + 0x54) = 0xA000001;
        break;
    }
}

/* across, or down */
void DC2CALL Raptor_JumpOver(gptr o)                                /* 0x4221E0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        Anim_Start(o, 0x17, 0, 4, U8(o + 0xFF));
        U32(o + 0x50) |= 0x840;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U8(o + 0x56)++;
        break;
    case 1:
        U16(o + 0x4A) += (u16)Angle_Approach(U16(o + 0x4A), 0x20, U16(U32(RAP_EXT) + 8));
        if (!Anim_Step(o)) break;
        Anim_Start(o, 0x14, 0, 0, 4);
        U8(o + 0x174) |= 4;
        U32(o + 0x50) |= 0x840;
        U8(o + 0x118) |= 0x20;
        U16(o + 0xF4) = 0x3C;
        U16(U32(RAP_EXT) + 0x14) = 0xFE5C;
        U8(o + 0x56)++;
        break;
    case 2:
        Raptor_Fall(o);
        if (S16(U32(RAP_EXT) + 0x14) < 0) { Anim_Step(o); break; }
        Anim_Start(o, 0x11, 0, 0, 2);
        U8(o + 0x56)++;
        break;
    case 3:
        Raptor_Fall(o);
        if (-1000 * S16(o + 0x5A) > S16(o + 0x42)) { Anim_Step(o); break; }
        Raptor_Land(o, 0x12);
        break;
    case 4:
        if (Anim_Step(o)) U32(o + 0x54) = 0xA000001;
        break;
    }
}
