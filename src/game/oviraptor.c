/* The oviraptor (object type handler 0x441FB0, type 7, E90.DAT). Its
 * extension (o+0x1C0, also *0x74AFA8 while it runs): +0 first delay, +4
 * squared distance to its target, +8 angle to it, +0xC state kept from the
 * last frame, +0x10 the angle it jumps at, +0x14 vertical speed, +0x16 the
 * level it climbs to, +0x19 its kind of attack, +0x1A frames, +0x1C, +0x1E
 * the climb's crouch done, +0x20/+0x21 may jump / may climb, +0x22/+0x23,
 * +0x28, +0x2E.
 * States (o+0x54): 0 init, 1 normal (sub-states 0x7212DC), 2 hit (by
 * o+0x172, 0x721300), 3 die, 4 (0x4A56D0). */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/task.h"
#include "dc2/gte.h"

#define OVI_EXT    0x74AFA8u    /* the extension of the oviraptor that runs */
#define OVI_MAIN   0x7212C8u    /* state handlers */
#define OVI_NORM   0x7212DCu    /* state 1: sub-states */
#define OVI_HIT    0x721300u    /* state 2: by the kind of hit (o+0x172) */
#define OVI_GRAB   0x721314u    /* holding the player: by o+0x56 (o, player) */
#define OVI_ATTACK 0x72132Cu    /* sub-state 2: attack steps by o+0x56 (o, player) */
#define OVI_TAB_LEVEL 0x7032E0u /* per level (5 each): health class, attack, ... */
#define OVI_TAB_HEALTH 0x7032D0u
#define EXT        U32(OVI_EXT)

static void Call2(gptr fn, u32 a, u32 b)
{
#ifdef DC2_IN_PROCESS
    ((void (DC2CALL *)(u32, u32))(uintptr_t)fn)(a, b);
#else
    void (DC2CALL *f)(u32, u32) = (void (DC2CALL *)(u32, u32))Hook_Find(fn);
    if (f) f(a, b);
    else Orig_Call1(fn, a);                 /* reports it once as not rewritten */
#endif
}

void DC2CALL Ovi_Handler(gptr o)                                    /* 0x441FB0 */
{
    Obj_SavePos(o);
    U32(OVI_EXT) = o + 0x1C0;
    if (U8(o + 0x184) & 0x40) Zone_TrackPoint(o, U32(G() + 0xB50) + 0x40);
    Orig_Call1(U32(OVI_MAIN + U8(o + 0x54) * 4), o);
    U16(EXT + 0x1A)++;
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    Head_Update(o);
    Obj_UpdateMatrix(o);
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    Enemy_AttackHit(o);
    Ovi_Grab(o);
    U32(EXT + 0xC) = U32(o + 0x54);
    if (U32(o + 0x50) & 0x40000000) Obj_Disable(o);
}

/* state 0: model, animations, health and abilities by level (o+0x10C) */
void DC2CALL Ovi_Init(gptr o)                                       /* 0x442080 */
{
    gptr g = G();
    if (U8(g + 0x1091) == 4 && U32(g + 0xB50) && (U8(g + 0x1090) == 8 || U8(g + 0x1090) == 9))
        U32(o + 0x184) |= 0x40;                   /* rides with the part */
    else U16(EXT + 0x2E) = 0xFFFF;
    U32(o + 0x50) |= U8(G() + 0x927) ? 0xB : 0xF;
    Model_Setup(o, 0x63D000);
    U32(o + 0x88) = 0x63F078;
    U32(o + 0x12C) = 0x7212A0;
    U32(o + 0x128) = 0x721290;
    U32(o + 0x134) = 0x7212B0;
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    u16 lv = U16(o + 0x10C);
    if (lv & 0x10) { U8(o + 0x18D) = U8(EXT); U8(o + 0x18C) = (u8)((lv << 4) + 1); }
    if (lv & 0x20) U16(o + 0x10C) = U8(o + 0x18C) >> 4;
    gptr t = OVI_TAB_LEVEL + (U8(o + 0x10C) & 0xF) * 5;
    u8 h = U8(t);
    u16 hp = (u16)((Rand8() & 0x1F) + U16(OVI_TAB_HEALTH + h * 2));
    U16(o + 0x108) = hp;
    if (U8(G() + 0x108E) == 2) U16(o + 0x108) = (u16)(hp * 2);
    if (U8(G() + 0x108E) == 0) S16(o + 0x108) >>= 1;
    U16(o + 0x10A) = U16(o + 0x108);
    u8 a = U8(t + 1);
    U8(EXT + 0x1C) = (a & 0xF0) ? 0 : 1;
    U8(EXT + 0x19) = a & 0xF;
    U8(EXT + 0x19) = 2;
    a = U8(t + 2);
    U8(EXT + 0x22) = a >> 4;
    U8(EXT + 0x23) = a & 0xF;
    if (U8(G() + 0x108E) == 2) Flag_Set(2, 0x26, 1);
    if (!U8(G() + 0x108E)) { U8(EXT + 0x20) = 0; U8(EXT + 0x21) = 0; }
    else if (U8(G() + 0x927)) { U8(EXT + 0x20) = 0; U8(EXT + 0x21) = 1; }
    else { U8(EXT + 0x20) = 1; U8(EXT + 0x21) = 1; }
    if (Flag_Check(2, 0x1A)) U8(EXT + 0x20) = 0;
    DrawSync(o);
    Obj_Set118(o, 1, 0x12C0, 0x1518);
    U8(o + 0x118) |= 0x20;
    Head_Reset(o);
    U8(o + 0x15F) = 3; U8(o + 0x160) = 1;
    U16(o + 0x164) = 0x80; U16(o + 0x166) = 0x80; U16(o + 0x16A) = 0x400; U16(o + 0x16E) = 0x400;
    U8(o + 0x15D) = 0;
    U8(o + 0x15E) = U8(G() + 0x927) ? 0 : 7;
    U16(o + 0x168) = 0x400; U16(o + 0x16C) = 0x600;
    U8(o + 0x15C) = 0;
    U32(o + 0x188) = 0;
    if (U8(o + 0x184) & 0x40) U8(o + 0x15D) = U8(U32(G() + 0xB50) + 0xFF);
    U32(o + 0x54) = 0x701;
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
}

/* state 1: think, then the sub-state */
void DC2CALL Ovi_Normal(gptr o)                                     /* 0x4423D0 */
{
    Ovi_Think(o);
    if (U8(o + 0x55) >= 2) U32(o + 0x158) = 1;
    Orig_Call1(U32(OVI_NORM + U8(o + 0x55) * 4), o);
}

/* state 2: hit, by its kind */
void DC2CALL Ovi_Hit(gptr o)                                        /* 0x442410 */
{
    U8(o + 0x18D) = 0;
    U32(o + 0x158) = 1;
    Orig_Call1(U32(OVI_HIT + U16(o + 0x172) * 4), o);
    U16(EXT + 0x28) = 0;
}

/* state 3: dies, the burning parts darken; scores; fades out */
void DC2CALL Ovi_Die(gptr o)                                        /* 0x442450 */
{
    if (U8(o + 0x170) & 2)
        for (gptr p = U32(o + 0x64); p; p = U32(p + 0x54))
            for (u32 k = 0x8C; k < 0x8F; k++)
                if (U8(p + k) > 0x30) U8(p + k) -= 4;
    switch (U8(o + 0x55)) {
    case 0: {
        u8 b = (u8)((U8(o + 0xFF) & 3) + (Rand8() & 3) + 3);
        Anim_Start(o, b & 1 ? 0x2C : 0xF, (u32)U16(o + 0x170) << 1, 0, b);
        U32(o + 0x50) = (U32(o + 0x50) & ~8u) | 0x30;
        U32(o + 0xF0) = 0;
        U16(o + 0xF4) = 0;
        U32(o + 0x158) = 1;
        if (U8(o + 0x184) & 8) U32(o + 0x50) |= 0x4000;
        Flag_Set(4, U8(o + 0x11C), 1);
        Score_Kill(o);
        U8(o + 0x55)++;
        break; }
    case 1:
        if (Anim_Step(o)) {
            U32(o + 0xF0) = 0;
            U32(o + 0x50) |= 0x30;
            U16(o + 0xF4) = 0;
            Layers_Assign();
            Prop_SpawnC(0, o + 0x40, 1, 0x19, 1, 0);
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
 * is, while it waits (o+0x55 0) */
#define TO(st) do { U32(o + 0x54) = (st); return; } while (0)
void DC2CALL Ovi_Think(gptr o)                                      /* 0x4425D0 */
{
    gptr t = U32(G() + 0x940);
    if (U8(o + 0x18D)) { U8(o + 0x18D)--; return; }
    if (U8(o + 0x184) & 0x40) t = U32(G() + 0xB50);
    U32(EXT + 4) = Dist2_XZ(o + 0x40, t + 0x40);
    U16(EXT + 8) = (u16)Angle_XZ(o + 0x40, o + 0xD8);
    if (U8(o + 0x17B) || U8(o + 0x55) >= 1 || U8(o + 0x56) == 2) return;
    u32 r = Rand8();
    if (Wall_Ahead(o, 0x578) && U8(o + 0x13A)) {
        if (Step_Ahead(o, 0x7D0, 0x19A, U16(o + 0x4A))) TO(0x301);
        if (r > 0x90) TO(0x601);
        if (r > 0x30) TO(0x401);
        return;
    }
    if (Obj_InSight(o, t + 0x40, 0x895440, 0x30)) {
        if (U16(t + 0x174) && U8(t + 0x55) != 8) { if (r & 0x10) TO(0x401); TO(0x601); }
        if (U16(t + 0x5A) != U16(o + 0x5A)) return;
        r += U8(o + 0xFF);
        if (U8(o + 0x184) & 0x40) {
            r &= 1;
            U32(o + 0x54) = r << 16 | 0x201;
            if (U8(EXT + 0x22) && r == 1) U32(o + 0x54) = 0x30201;
        } else {
            r &= 3;
            U32(o + 0x54) = r << 16 | 0x201;
            if (U8(EXT + 0x22) && r == 1) U32(o + 0x54) = 0x20201;
        }
        return;
    }
    if (!U16(t + 0x174)) return;
    u32 d = U32(EXT + 4);
    if (d > 0x2255100 || U8(o + 0x13A)) return;
    if (d <= 0x3D0900) TO(0x601);
    if (r > 0x40) TO(0x401);
}
#undef TO

/* holding the player (sub-state 2 of a state, o+0x119 not negative): by o+0x56 */
void DC2CALL Ovi_Grab(gptr o)                                       /* 0x4427E0 */
{
    gptr p = U8(o + 0x184) & 0x40 ? U32(G() + 0xB50) : U32(G() + 0x940);
    if (S8(o + 0x119) < 0 || U8(o + 0x55) != 2 || (U8(p + 0x174) & 1)) return;
    U8(p + 0x11B) = 0;
    Call2(U32(OVI_GRAB + U8(o + 0x56) * 4), o, p);
}

/* sub-states 0/1: walks towards the target; turns about when it is behind */
void DC2CALL Ovi_Walk(gptr o)                                       /* 0x442840 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U32(o + 0xF0) = 0;
        Anim_Start(o, 0xA, U8(o + 0xFF) & 3, 1, 4);
        U16(o + 0xF4) = 0;
        U32(o + 0x158) = 2;
        U8(o + 0x56)++;
        break;
    case 1: {
        u16 tg = U16(EXT + 8), cur = U16(o + 0x4A);
        u32 d = (u32)(tg - cur) & 0xFFF;
        if (d >= 0x700 && d <= 0x900) {
            U32(o + 0x7C) = 0;
            Anim_Start(o, Rand8() & 1 ? 4 : 3, 0, 0x40, 8);
            U16(o + 0xF4) = 0;
            U32(o + 0x158) = 1;
            U8(o + 0x56)++;
            break;
        }
        if (d >= 0xE00 && d <= 0x200) U32(o + 0x54) = 1;          /* (never) */
        else U16(o + 0x4A) += (u16)Angle_Approach(cur, U32(EXT + 4) < 0x3D0900 ? 0xA0 : 0x60, tg);
        Anim_Step(o);
        U16(o + 0xF4) = 0xA0;
        break; }
    case 2:
        if (Anim_Step(o)) {
            U16(o + 0x4A) = (U16(o + 0x4A) - 0x800) & 0xFFF;
            gptr p = U32(o + 0x64);
            U16(p + 0x4A) = (U16(p + 0x4A) - 0x800) & 0xFFF;
            Anim_Set(o, 0xA, 0, 1, 4);
            U32(o + 0xF0) = 0;
            U16(o + 0xF4) = 0;
            U32(o + 0x54) = 0x10001;
        }
        break;
    }
}

/* sub-state 2: an attack on the player, by its step (o+0x56); the game ends
   when the player's health runs out */
void DC2CALL Ovi_Attack(gptr o)                                     /* 0x442B20 */
{
    gptr p = U32(G() + 0x940);
    Call2(U32(OVI_ATTACK + U8(o + 0x56) * 4), o, p);
    if (S16(p + 0x108) < 0) GameOver_Start(0);
}

/* ---- sub-state 3: jumps; ext +0x10 the angle, +0x14 the vertical speed, +0x16 the
   level it climbs to ----------------------------------------------------------------- */

/* in the air: faster forward, falling by g */
static void Ovi_Fall(gptr o, u16 g)
{
    if (S16(o + 0xF4) < 0xB4) U16(o + 0xF4) += 0x1E;
    U16(EXT + 0x14) += g;
    U16(o + 0x42) += U16(EXT + 0x14);
}

/* up onto a ledge two levels higher */
void DC2CALL Ovi_Climb(gptr o)                                      /* 0x442BF0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        Anim_Start(o, 0xB, 0, 8, U8(o + 0xFF) & 7);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U16(EXT + 0x10) = U16(o + 0x4A);
        U16(EXT + 0x16) = U16(o + 0x5A) + 2;
        U8(EXT + 0x1E) = 0;
        U8(o + 0x56)++;
        break;
    case 1: {
        if (Anim_Step(o)) U8(EXT + 0x1E) = 1;
        u32 a = Angle_Approach(U16(o + 0x4A), 0x40, U16(EXT + 0x10)) & 0xFFFF;
        U16(o + 0x4A) += (u16)a;
        if (a || !U8(EXT + 0x1E)) break;
        Anim_Start(o, 0x3B, 0, 0, 2);
        U8(o + 0x174) |= 4;
        U32(o + 0x50) |= 0x840;
        U8(o + 0x118) |= 0x20;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0x28;
        U16(EXT + 0x14) = 0xFDBC;
        U8(o + 0x56)++;
        break; }
    case 2:
        Ovi_Fall(o, 0x32);
        if (S16(EXT + 0x14) < 0) { Anim_Step(o); break; }
        Anim_Start(o, 0x37, 0, 0, 2);
        U8(o + 0x56)++;
        break;
    case 3:
        Ovi_Fall(o, 0x3C);
        if (-1000 * (s32)U16(EXT + 0x16) < S16(o + 0x42)) U32(o + 0x50) &= ~0x40u;   /* above the ledge: no walls */
        if (-1000 * S16(o + 0x5A) > S16(o + 0x42)) { Anim_Step(o); break; }
        U32(o + 0x7C) = 0;
        Anim_Start(o, 0x38, 0, 0x40, 2);
        U16(o + 0x174) &= 0xFFFB;
        U16(o + 0x42) = (u16)(S16(o + 0x5A) * -1000);
        U32(o + 0x50) &= 0xFFFFF7AF;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U8(o + 0x118) &= 0xDF;
        U8(o + 0x56)++;
        Se_Play(o, 0x36);
        break;
    case 4:
        if (Anim_Step(o)) U32(o + 0x54) = 0xA000001;
        break;
    }
}

/* across, or down */
void DC2CALL Ovi_JumpOver(gptr o)                                   /* 0x442E90 */
{
    switch (U8(o + 0x56)) {
    case 0:
        Anim_Start(o, 0xC, 0, 4, (U8(o + 0xFF) & 3) + 3);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U16(EXT + 0x10) = U16(o + 0x4A);
        U8(o + 0x56)++;
        break;
    case 1:
        if (!Anim_Step(o)) break;
        Anim_Start(o, 0x3C, 0, 0, 2);
        U8(o + 0x174) |= 4;
        U32(o + 0x50) |= 0x840;
        U8(o + 0x118) |= 0x20;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0x28;
        U16(EXT + 0x14) = 0xFE5C;
        U8(o + 0x56)++;
        break;
    case 2:
        Ovi_Fall(o, 0x3C);
        if (S16(EXT + 0x14) < 0) { Anim_Step(o); break; }
        Anim_Start(o, 0x39, 0, 0, 2);
        U8(o + 0x56)++;
        break;
    case 3:
        Ovi_Fall(o, 0x3C);
        if (-1000 * S16(o + 0x5A) > S16(o + 0x42)) { Anim_Step(o); break; }
        U32(o + 0x7C) = 0;
        Anim_Start(o, 0x3A, 0, 0x40, 2);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U16(o + 0x42) = (u16)(S16(o + 0x5A) * -1000);
        U32(o + 0x50) &= 0xFFFFF7AF;
        U8(o + 0x56)++;
        Se_Play(o, 0x36);
        break;
    case 4:
        if (Anim_Step(o)) U32(o + 0x54) = 0xA000001;
        break;
    }
}

/* sub-state 3: the level of the floor under it, then climbs (o+0x11A bit 6:
   jumps over instead); its level from its height after */
void DC2CALL Ovi_Jump(gptr o)                                       /* 0x442B60 */
{
    gptr tmp = Tmp_Push(8);
    U16(tmp) = U16(o + 0x40); U16(tmp + 2) = 0x7918; U16(tmp + 4) = U16(o + 0x44);
    U16(o + 0x5A) = (u16)((s32)(s16)Floor_Height(tmp, o + 0x40, 0x40, 0) / -1000);
    Tmp_Pop(8);
    if (U8(o + 0x11A) & 0x40) Ovi_JumpOver(o);
    else Ovi_Climb(o);
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
}

/* sub-state 7: comes in (o+0x18C low bits: how) */
void DC2CALL Ovi_Enter(gptr o)                                      /* 0x4429B0 */
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
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0x32;
        U16(o + 0x174) = 0;
        U8(o + 0x56)++;
        break;
    case 2:
        U32(o + 0xF0) = 0;
        Anim_Start(o, 0xA, 0, 1, 0);
        U16(o + 0xF4) = 0xA0;
        U8(o + 0x56)++;
        break;
    case 3:
        U8(o + 0x18D) = 0;
        U32(o + 0x54) = 0x40201;
        break;
    case 4:
        Anim_Start(o, 0x3C, 0, 0, 4);
        U8(o + 0x174) |= 4;
        U32(o + 0x50) |= 0x840;
        U8(o + 0x118) |= 0x20;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0x28;
        U16(EXT + 0x14) = 0xFE5C;
        U32(o + 0x54) = 0x20301;
        break;
    default:
        U8(o + 0x18D) = 0;
        U32(o + 0x54) = 1;
        U32(o + 0x50) &= ~0x10u;
        break;
    }
}

/* sub-state 4: faces the target, then looks about; turns back to walking or
   goes on to sub-state 6 */
void DC2CALL Ovi_Look(gptr o)                                       /* 0x4430A0 */
{
    gptr p = U32(G() + 0x940);
    switch (U8(o + 0x56)) {
    case 0: {
        U16(EXT + 8) = (u16)Angle_XZ(o + 0x40, o + 0xD8);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0; U32(o + 0x7C) = 0;
        s16 d = (s16)(U16(EXT + 8) - U16(o + 0x4A));
        if (d >= -0x100 && d <= 0x100) {
            u32 f = U8(o + 0xFF) & 3;
            if (f & 1) Anim_Start(o, 6, f, 0x40, 8);
            else Anim_Start(o, 7, f, 0x40, f + 6);
            U8(o + 0x56) = 2;
            return;
        }
        Anim_Start(o, 0, 0, 0x40, 8);
        U8(o + 0x56)++;
        break; }
    case 1: {
        U16(o + 0x4A) += (u16)Angle_Approach(U16(o + 0x4A), 0x60, U16(EXT + 8));
        if (!Anim_Step(o)) break;
        u32 f = U8(o + 0xFF) & 3;
        U32(o + 0x7C) = 0;
        if (f & 1) Anim_Start(o, 6, f, 0x40, f + 6);
        else Anim_Start(o, 7, f, 0x40, 8);
        U8(o + 0x56)++;
        break; }
    case 2:
        if (!Anim_Step(o)) break;
        if (!(Rand8() & 1)) { U32(o + 0x54) = 0x601; break; }
        {
            s16 a = (s16)((Angle_XZ(o + 0x40, p + 0x40) - U16(o + 0x4A)) & 0xFFF);
            if (a < 0xE00 && a > 0x200) { U32(o + 0x54) = 1; break; }
        }
        U32(o + 0x7C) = 0;
        if (U8(o + 0xFF) & 1) Anim_Start(o, 2, 1, 0x40, 5);
        else Anim_Start(o, 9, 0, 0x40, 3);
        if (U32(EXT + 4) >= 0x7A1200 && U32(EXT + 4) <= 0x989680) U8(o + 0x56)++;
        break;
    case 3:
        Obj_TurnToPoint(o, 0x20, p + 0x40);
        if (Anim_Step(o)) { Anim_Set(o, 0xA, 0, 1, 4); U32(o + 0x54) = 0x10001; }
        break;
    }
}

/* sub-state 5: hides (ext +0x2A frames), comes out */
void DC2CALL Ovi_Hide(gptr o)                                       /* 0x4432D0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        if (U16(EXT + 0x2A)) U16(EXT + 0x2A)--;
        else { U32(o + 0x50) |= 0x20; U8(o + 0x56)++; }
        if (Anim_Step(o)) { U32(o + 0xF0) = 0; U32(o + 0x50) |= 0x20; U16(o + 0xF4) = 0; }
        break;
    case 1:
        U32(o + 0x7C) = 0;
        Anim_Start(o, 5, U8(o + 0x180) & 1, 0x40, 6);
        U8(o + 0x56)++;
        U32(o + 0x50) |= 0x2000;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        break;
    case 2:
        if (Anim_Step(o)) {
            U32(o + 0x54) = 1;
            U32(o + 0x50) &= 0xFFFFDFDF;
            U16(o + 0x174) &= 0xFFFD;
        } else if (U16(o + 0x92) >= 0x2F) {
            U32(o + 0x50) &= ~0x20u;
            U16(o + 0x174) &= 0xFFFD;
        }
        break;
    }
}

/* sub-state 6: turns to the target in place, now and then again */
void DC2CALL Ovi_Turn(gptr o)                                       /* 0x4433D0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U32(o + 0x7C) = 0;
        Anim_Start(o, 0xD, 0, 0x40, (U8(o + 0xFF) & 3) + 5);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U16(EXT + 8) = (u16)Angle_XZ(o + 0x40, o + 0xD8);
        U8(o + 0x56)++;
        break;
    case 1: {
        U16(o + 0x4A) += (u16)Angle_Approach(U16(o + 0x4A), 0x60, U16(EXT + 8));
        if (!Anim_Step(o)) break;
        u8 r = Rand8();
        if (r > 0xF0) U8(o + 0x56) = 0;
        else if (r & 1) { Anim_Start(o, 0xC, 0, 0, 8); U8(o + 0x56)++; }
        else U32(o + 0x54) = 1;
        break; }
    case 2:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        break;
    }
}

/* ---- attacks (sub-state 2, steps by o+0x56, each by o+0x57; called with the player) --- */
#define OVI_DMG(k) U8(0x7032D4 + U8(EXT + 0x23) * 6 + (k))

/* a leap back from the target */
void DC2CALL Ovi_LeapBack(gptr o, gptr p)                           /* 0x4434B0 */
{
    (void)p;
    switch (U8(o + 0x57)) {
    case 0:
        U32(o + 0x7C) = 0;
        Anim_Start(o, 0x10, 0, 0x40, 8);
        U8(o + 0x57)++;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        break;
    case 1:
        if (Anim_Step(o)) {
            U16(o + 0x174) &= 0xFFFB;
            U32(o + 0x50) &= ~0x10u;
            U16(o + 0x42) = (u16)(S16(o + 0x5A) * -1000);
            U32(o + 0xF0) = 0;
        } else {
            u16 f = U16(o + 0x92);
            if (f < 4) break;
            if (f >= 0x1B) U16(o + 0x174) &= 0xFFFB;
            else U8(o + 0x174) |= 4;
            break;
        }
        Anim_Set(o, 0xA, 0, 1, 4);
        U32(o + 0x54) = 0x10001;
        break;
    case 2: {
        if (!Anim_Step(o)) break;
        U16(o + 0x174) &= 0xFFFB;
        U16(o + 0x42) = (u16)(S16(o + 0x5A) * -1000);
        U32(o + 0x50) &= ~0x10u;
        U16(o + 0x4A) = (U16(o + 0x4A) - 0x800) & 0xFFF;
        gptr q = U32(o + 0x64);
        U16(q + 0x4A) = (U16(q + 0x4A) - 0x800) & 0xFFF;
        Anim_Set(o, 0xA, 0, 1, 4);
        U32(o + 0x54) = 0x10001;
        break; }
    }
}

/* a peck */
void DC2CALL Ovi_Peck(gptr o, gptr p)                               /* 0x4436B0 */
{
    (void)p;
    switch (U8(o + 0x57)) {
    case 0:
        U32(o + 0x7C) = 0;
        Anim_Start(o, 0x1A, 0, 0x40, 8);
        U8(o + 0x57)++;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        break;
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        if (U16(o + 0x92) == 0x18) Effect_SpawnAt(o, 0x65D738, 0);
        break;
    }
}

/* holding, step 0: the bite lands; the player is thrown down or back */
void DC2CALL Ovi_GrabBite(gptr o, gptr p)                           /* 0x4435D0 */
{
    if (U8(o + 0x57) != 1) return;
    U32(o + 0x7C) = 0;
    Anim_Start(o, 0x1D, 0, 0x40, 2);
    U8(o + 0x57)++;
    U32(o + 0x50) &= ~0x10u;
    u32 r = Enemy_HitDir(o);
    U16(p + 0x174) = 1;
    if (!r && U8(EXT + 0x21)) { U32(p + 0x54) = 0x202; U16(p + 0x176) = (u16)(OVI_DMG(0) + 0x28); }
    else { U16(p + 0x176) = OVI_DMG(0); U32(p + 0x54) = 0x102; }
    if (U8(o + 0x184) & 0x40) U32(p + 0x54) = 2;
    else Flash_Play(o, 0x65EDAC);
    Se_Play(o, 0x45);
    Effect_SpawnAt(o, 0x65CAF8, 0);
}

/* the player's animation for the oviraptor's moves (off: 0xFC..) */
#define PL_ANIM(o, off) U32(U32((o) + 0x88) + U8(G() + 0x108F) * 0x40 + (off))

/* holding the player down: it moves with the player, faces as the oviraptor */
static void Ovi_Ride(gptr o, gptr p)
{
    U16(o + 0x40) += U16(p + 0xE8);
    U16(o + 0x42) += U16(p + 0xEA);
    U16(o + 0x44) += U16(p + 0xEC);
    U16(p + 0x4A) = U16(o + 0x4A);
    U16(p + 0x40) = U16(o + 0x40); U16(p + 0x42) = U16(o + 0x42); U16(p + 0x44) = U16(o + 0x44);
}

/* a bite while holding: the player hurt (ext anim, oviraptor's anim, effect) */
static void Ovi_RideBite(gptr o, gptr p, u32 anim, u32 fl, gptr panim, gptr fx)
{
    Effect_Attach(o, fx);
    Anim_Start(o, anim, 0, fl, 0);
    Anim_Start(p, panim, 0, fl, 0);
}

/* shaken off: the player gets up (and turns about when 0x104) */
static void Ovi_ShakenOff(gptr o, gptr p, u32 anim, u32 poff, int turn)
{
    U32(o + 0x7C) = 0;
    Anim_Start(o, anim, 0, 0x40, 0);
    U32(o + 0x50) |= 8;
    U8(o + 0x57) = 6;
    Player_Bleed(0x1F4);
    if (turn) {
        U16(p + 0x4A) = (U16(p + 0x4A) - 0x800) & 0xFFF;
        gptr q = U32(p + 0x64);
        U16(q + 0x4A) = (U16(q + 0x4A) - 0x800) & 0xFFF;
    }
    Anim_Set(p, PL_ANIM(o, poff), 0, 0x40, 0);
    Obj_UpdateMatrix(p);
    U32(p + 0x54) = 0x10102;
}

/* a pounce: on the player, bites while the player does not shake it off
   (o+0x1E8 the shaking, more than 10 throws it off) */
void DC2CALL Ovi_Pounce(gptr o, gptr p)                             /* 0x443730 */
{
    switch (U8(o + 0x57)) {
    case 0:
        U32(o + 0x7C) = 0;
        Anim_Start(o, 0x11, 0, 0x40, 1);
        U8(o + 0x174) |= 4;
        U8(o + 0x57)++;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        break;
    case 1:
        if (Anim_Step(o)) { U16(o + 0x174) &= 0xFFFB; U32(o + 0x54) = 1; break; }
        if (U16(o + 0x92) < 9) break;
        if (U16(o + 0x92) >= 0x1A) U16(o + 0x174) &= 0xFFFB;
        else U8(o + 0x174) |= 4;
        break;
    case 2:
    case 4: {
        int front = U8(o + 0x57) == 2;
        Ovi_Ride(o, p);
        if (!Anim_Step(o)) break;
        Ovi_RideBite(o, p, front ? 0x13 : 0x14, 1, PL_ANIM(o, front ? 0xFC : 0x100), front ? 0x65CBEC : 0x65CF88);
        U16(o + 0x1E8) = 0;
        U8(o + 0x57)++;
        U16(p + 0x108) -= (u16)Damage_Scale(U16(p + 0x176));
        Flash_Play(o, 0x65EDC8);
        break; }
    case 3:
    case 5: {
        int front = U8(o + 0x57) == 3;
        Ovi_Ride(o, p);
        U16(o + 0x1E8) += (u16)Pad_Shake();
        if (!Anim_Step(o)) break;
        if (S16(o + 0x1E8) > 10) {
            if (front) Ovi_ShakenOff(o, p, 0x15, 0x104, 1);
            else Ovi_ShakenOff(o, p, 0x16, 0x120, 0);
            break;
        }
        if (front) Ovi_RideBite(o, p, 0x13, 0, PL_ANIM(o, 0xFC), 0x65CBEC);
        else Ovi_RideBite(o, p, 0x14, 1, PL_ANIM(o, 0x100), 0x65CF88);
        U16(p + 0x108) -= (u16)Damage_Scale(U16(p + 0x176));
        U16(o + 0x1E8) += 0xB;
        Flash_Play(o, 0x65EDC8);
        break; }
    case 6: {
        if (!Anim_Step(o)) break;
        U32(o + 0x50) &= ~0x800u;
        U16(o + 0x174) &= 0xFFFB;
        U16(o + 0x42) = (u16)(S16(o + 0x5A) * -1000);
        U16(o + 0x4A) = (U16(o + 0x4A) - 0x800) & 0xFFF;
        gptr q = U32(o + 0x64);
        U16(q + 0x4A) = (U16(q + 0x4A) - 0x800) & 0xFFF;
        Anim_Set(o, 0xA, 0, 1, 4);
        U32(o + 0x54) = 1;
        break; }
    }
}

/* holding, step 2: the pounce lands on the player (from the front, o+0x57 2,
   or behind, 4) unless the player dodges or it may not jump */
void DC2CALL Ovi_PounceCatch(gptr o, gptr p)                        /* 0x443C70 */
{
    if (U8(o + 0x57) != 1) return;
    Flash_Play(o, 0x65EDAC);
    u32 r = Enemy_HitDir(o);
    U16(p + 0x174) = 1;
    if ((r & 1) || U8(o + 0x119) || !U8(EXT + 0x20)) {
        U16(p + 0x176) = OVI_DMG(1);
        U32(p + 0x54) = 2;
        return;
    }
    U16(p + 0x176) = OVI_DMG(2);
    if (r) {
        Anim_Start(o, 0x12, 0, 0, 0);
        Anim_Start(p, PL_ANIM(o, 0xF4), 0, 0, 0);
        U16(p + 0x176) += 0x28;
        U8(o + 0x57) = 2;
    } else {
        Anim_Start(o, 0x1B, 0, 0, 0);
        Anim_Start(p, PL_ANIM(o, 0xF8), 0, 0, 0);
        U8(o + 0x57) = 4;
    }
    Se_Play(p, 3);
    U16(o + 0x42) = (u16)(S16(o + 0x5A) * -1000);
    U32(o + 0x50) = (U32(o + 0x50) & ~8u) | 0x800;
    U32(p + 0xF0) = 0; U16(p + 0xF4) = 0;
    U32(G() + 0x92C) = 0x4A56D0;
    U32(p + 0x50) |= 0x900;
    U16(p + 0x108) -= (u16)Damage_Scale(OVI_DMG(1));
    U16(p + 0x4A) = U16(o + 0x4A);
    U16(p + 0x40) = U16(o + 0x40); U16(p + 0x42) = U16(o + 0x42);
    U32(p + 0x1D8) = o;
    U16(p + 0x44) = U16(o + 0x44);
    U32(p + 0x54) = 0x205;
}

/* a lunge (faster when it may and its level is 2 or more) */
void DC2CALL Ovi_Lunge(gptr o, gptr p)                              /* 0x443E30 */
{
    (void)p;
    switch (U8(o + 0x57)) {
    case 0:
        U32(o + 0x7C) = 0;
        Anim_Start(o, 0xE, 0, 0x40, 6);
        U32(o + 0xF0) = 0;
        U16(o + 0xF4) = U8(EXT + 0x22) && (U8(o + 0x10C) & 0xF) >= 2 ? 0x14 : 0;
        U8(o + 0x57)++;
        break;
    case 1:
        U16(o + 0x4A) += (u16)Angle_Approach(U16(o + 0x4A), U8(EXT + 0x22) ? 0x20 : 8, U16(EXT + 8));
        if (Anim_Step(o)) { U16(o + 0xF4) = 0; U32(o + 0x54) = 1; }
        break;
    }
}

/* the lunge lands */
void DC2CALL Ovi_LungeHit(gptr o, gptr p)                           /* 0x443EF0 */
{
    if (U8(o + 0x57) != 1) return;
    u32 r = Enemy_HitDir(o);
    U16(p + 0x176) = OVI_DMG(5);
    U16(p + 0x174) = 1;
    if (!(r & 0xFFFF) && U8(EXT + 0x21)) { U16(p + 0x176) += 0x28; U32(p + 0x54) = 0x202; }
    else U32(p + 0x54) = 0x102;
    if (U8(o + 0x184) & 0x40) U32(p + 0x54) = 2;
    else Flash_Play(o, 0x65EDAC);
    Se_Play(o, 0x45);
}

/* a kick, jumping */
void DC2CALL Ovi_Kick(gptr o, gptr p)                               /* 0x443FA0 */
{
    (void)p;
    switch (U8(o + 0x57)) {
    case 0:
        U32(o + 0x7C) = 0;
        Anim_Start(o, 0x1E, 0, 0x40, 8);
        U8(o + 0x57)++;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        break;
    case 1: {
        if (!Anim_Step(o)) {
            u16 f = U16(o + 0x92);
            if (f < 8) break;
            if (f >= 0x24) U16(o + 0x174) &= 0xFFFB;
            else U8(o + 0x174) |= 4;
            break;
        }
        U16(o + 0x174) &= 0xFFFB;
        U32(o + 0x50) &= 0xFFFFF7EF;
        U16(o + 0x42) = (u16)(S16(o + 0x5A) * -1000);
        U16(o + 0x4A) = (U16(o + 0x4A) - 0x800) & 0xFFF;
        gptr q = U32(o + 0x64);
        U16(q + 0x4A) = (U16(q + 0x4A) - 0x800) & 0xFFF;
        U32(o + 0xF0) = 0;
        Anim_Set(o, 0xA, 0, 1, 0);
        U32(o + 0x54) = 0x10001;
        break; }
    case 2:
        if (!Anim_Step(o)) break;
        U16(o + 0x174) &= 0xFFFB;
        U32(o + 0x50) &= 0xFFFFF7EF;
        U16(o + 0x42) = (u16)(S16(o + 0x5A) * -1000);
        U32(o + 0x54) = 1;
        break;
    }
}

/* the kick lands */
void DC2CALL Ovi_KickHit(gptr o, gptr p)                            /* 0x4440D0 */
{
    if (U8(o + 0x57) != 1) return;
    U32(o + 0x7C) = 0;
    Anim_Start(o, 0x1F, 0, 0x40, 2);
    U8(o + 0x57)++;
    U32(o + 0x50) &= ~0x10u;
    U16(p + 0x174) = 1;
    U16(p + 0x176) = OVI_DMG(0);
    u32 r = Enemy_HitDir(o);
    if (!(r & 0xFFFF) && U8(EXT + 0x21)) { U16(p + 0x176) += 0x28; U32(p + 0x54) = 0x202; }
    else U32(p + 0x54) = 0x102;
    if (U8(o + 0x184) & 0x40) U32(p + 0x54) = 2;
    else Flash_Play(o, 0x65EDAC);
    Se_Play(o, 0x45);
    Effect_SpawnAt(o, 0x65CAF8, 0);
}

/* a jump on the player's back */
void DC2CALL Ovi_BackJump(gptr o, gptr p)                           /* 0x4441A0 */
{
    switch (U8(o + 0x57)) {
    case 0:
        U32(o + 0x7C) = 0;
        Anim_Start(o, 0x17, 0, 0x40, 1);
        U8(o + 0x57)++;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        break;
    case 1:
        if (!Anim_Step(o)) break;
        U32(o + 0x54) = 1;
        U32(o + 0x50) &= ~0x10u;
        U16(o + 0x42) = (u16)(S16(o + 0x5A) * -1000);
        break;
    case 2:
        if (!Anim_Step(o)) break;
        Anim_Start(o, 0x18, 0, 0, 0);
        U8(o + 0x57)++;
        Anim_Start(p, PL_ANIM(o, 0x110), 0, 0, 0);
        U16(p + 0x108) -= (u16)Damage_Scale(U16(p + 0x176));
        break;
    case 3:
        if (!Anim_Step(o)) break;
        U32(o + 0x50) &= ~0x10u;
        U32(o + 0x7C) = 0;
        Anim_Start(o, 0x19, 0, 0x40, 0);
        U32(o + 0x50) &= ~0x800u;
        U8(o + 0x57)++;
        Anim_Start(p, PL_ANIM(o, 0x118), 0, 0x40, 0);
        U32(p + 0x54) = 0x10102;
        break;
    case 4:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        break;
    }
}

/* it lands on the player's back unless the player dodges */
void DC2CALL Ovi_BackCatch(gptr o, gptr p)                          /* 0x444330 */
{
    if (U8(o + 0x57) != 1 || (Enemy_HitDir(o) & 1)) return;
    U32(o + 0x50) |= 0x800;
    U16(o + 0x42) = (u16)(S16(o + 0x5A) * -1000);
    U8(o + 0x57)++;
    Anim_Start(p, U16(p + 0x172) == 2 ? PL_ANIM(o, 0x108) : PL_ANIM(o, 0x10C), 0, 0, 0);
    Se_Play(p, 3);
    U32(G() + 0x92C) = 0x4A56D0;
    U32(p + 0xF0) = 0; U16(p + 0xF4) = 0;
    U32(p + 0x50) |= 0x900;
    U16(p + 0x174) = 1;
    U16(p + 0x176) = OVI_DMG(3);
    U16(p + 0x4A) = (U16(o + 0x4A) - 0x800) & 0xFFF;
    U16(p + 0x40) = U16(o + 0x40); U16(p + 0x42) = U16(o + 0x42); U16(p + 0x44) = U16(o + 0x44);
    U32(p + 0x1D8) = o;
    U32(p + 0x54) = 0x205;
}

/* ---- hits (state 2, by o+0x172) ------------------------------------------------------ */

/* the damage of the hit (o+0x176); half more on a weak point (o+0x184 bit 3) */
static void Ovi_TakeDamage(gptr o)
{
    if (U8(o + 0x184) & 8) {
        Score_Event(o);
        u16 d = U16(o + 0x176);
        U16(o + 0x108) -= (u16)((d >> 1) + d);
    } else U16(o + 0x108) -= U16(o + 0x176);
}

/* hit standing: staggers (ext +0x1D counts the hits), or is knocked down
   (low health, by chance), or dies */
void DC2CALL Ovi_HurtStand(gptr o)                                  /* 0x4444A0 */
{
    switch (U8(o + 0x56)) {
    case 0: {
        u8 c = U8(EXT + 0x1D);
        U8(EXT + 0x1D) = c + 1;
        c &= 7;
        u32 b = U8(o + 0x180) & 3;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        s32 lv = (s32)(s16)Floor_Height(o + 0xE0, o + 0x40, 0x40, 0) / -1000;
        U8(o + 0x17B) = 4;
        U16(o + 0x5A) = (u16)lv;
        U16(o + 0x42) = (u16)(lv * -1000);
        U32(o + 0x50) &= 0xFFFFD78F;
        Ovi_TakeDamage(o);
        int knock = 0;
        if (S16(o + 0x108) < (S16(o + 0x10A) >> 2) && (Rand8() & 0x40)) knock = 1;
        if (S16(o + 0x108) < 0) {
            U32(o + 0x7C) = 0;
            Anim_Start(o, 0x28, b * 2, 0x40, c);
            U32(o + 0x50) &= ~8u;
            U8(o + 0x56) = 3;
            Se_Play(o, 0x41);
        } else if (knock) {
            U32(o + 0x7C) = 0;
            Anim_Start(o, 0x28, b * 2, 0x40, c);
            U8(o + 0x17B) = 0x14;
            U8(o + 0x56) = 3;
            Se_Play(o, 0x41);
        } else {
            if (U16(o + 0x172) == 4) {
                if (U8(EXT + 0x1D) & 1) Se_Play(o, 0x41);
                c = 0;
            } else {
                c &= 3;
                Se_Play(o, 0x41);
            }
            Anim_Start(o, U8(0x7032F4 + c + b * 8), b, 0, c);
            U16(EXT + 0x2A) = (u16)(b << 2);
            U8(o + 0x56)++;
        }
        if (U8(o + 0x170) & 2) Prop_Attach(o, 0x65EC18);
        else if (Rand8() & 0x10) Effect_Attach(o, U32(0x703314 + b * 8));
        else Effect_Attach(o, U32(0x703318 + b * 8));
        break; }
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        break;
    case 2:
        if (!Anim_Step(o)) break;
        U32(o + 0x7C) = 0;
        Anim_Start(o, 0x28, 0, 0x40, 2);
        U8(o + 0x56)++;
        break;
    case 3:
        if (Anim_Step(o)) {
            U16(o + 0x42) = (u16)(S16(o + 0x5A) * -1000);
            U16(o + 0x174) = 2;
            U32(o + 0x50) |= 0x20;
            if (S16(o + 0x108) < 0) { U32(o + 0x54) = 3; U32(o + 0x50) &= ~8u; }
            else U32(o + 0x54) = 0x501;
        } else if (U16(o + 0x92) >= 0x13) {
            U16(o + 0x174) = 2;
            U8(o + 0x56) = 1;
        }
        break;
    }
}

/* hit while down */
void DC2CALL Ovi_HurtDown(gptr o)                                   /* 0x4447F0 */
{
    switch (U8(o + 0x56)) {
    case 0: {
        u32 b = U8(o + 0x180) & 3, c = U8(EXT + 0x1A) & 3;
        U32(o + 0x50) &= 0xFFFFD78F;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U8(o + 0x17B) = 5;
        U16(o + 0x42) = (u16)(S16(o + 0x5A) * -1000);
        U16(o + 0x5A) = (u16)((s32)(s16)Floor_Height(o + 0xE0, o + 0x40, 0x40, 0) / -1000);
        Ovi_TakeDamage(o);
        if (S16(o + 0x108) < 0) { U32(o + 0x54) = 3; U32(o + 0x50) &= ~8u; }
        else {
            Anim_Start(o, U8(0x703334 + c + b * 4), 0, 0, 0);
            U8(o + 0x56)++;
            U16(EXT + 0x2A) = (u16)(b * 4);
        }
        Se_Play(o, 0x41);
        if (U8(o + 0x170) & 2) Prop_Attach(o, 0x65EC18);
        else Effect_Attach(o, U32(0x703344 + (c + b * 4) * 4));
        break; }
    case 1:
        if (!Anim_Step(o)) break;
        U32(o + 0x50) |= 0x20;
        U32(o + 0x54) = S16(o + 0x108) < 0 ? 3 : 0x501;
        break;
    }
}

/* hit kinds 0 and 4: in the air, down, or standing */
void DC2CALL Ovi_Hurt(gptr o)                                       /* 0x444470 */
{
    u16 f = U16(o + 0x174);
    if (f & 4) Ovi_HurtAir(o);
    else if (f & 2) Ovi_HurtDown(o);
    else Ovi_HurtStand(o);
}

/* knocked back: up after, or dead */
static void Ovi_KnockedUp(gptr o)
{
    U16(o + 0x42) = (u16)(S16(o + 0x5A) * -1000);
    U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
    U16(o + 0x174) = 2;
    U32(o + 0x50) |= 0x20;
    U32(o + 0x54) = S16(o + 0x108) < 0 ? 3 : 0x501;
}

/* hit kind 1 standing: knocked back the way the hit came (o+0x178) */
void DC2CALL Ovi_KnockStand(gptr o)                                 /* 0x4449B0 */
{
    switch (U8(o + 0x56)) {
    case 0: {
        u32 b = U8(o + 0x180) & 3, r = Rand8() & 1;
        U32(o + 0x7C) = 0;
        Anim_Start(o, U8(0x703394 + b), r, 0x40, r + 3);
        U16(o + 0x42) = (u16)(S16(o + 0x5A) * -1000);
        U16(o + 0x4A) = U16(o + 0x178) + 0x800;
        U32(o + 0x50) &= 0xFFFFD78F;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U8(o + 0x17B) = 5;
        Ovi_TakeDamage(o);
        if (S16(o + 0x108) < 0) U32(o + 0x50) &= ~8u;
        U16(EXT + 0x2A) = (u16)((b << 4) + 4);
        Se_Play(o, 0x42);
        if (U8(o + 0x170) & 2) Prop_Attach(o, 0x65EC18);
        else {
            Obj_HitDir(o);
            Effect_Attach(o, U32(0x703384 + S16(o + 0x178) * 4));
        }
        U8(o + 0x56)++;
        break; }
    case 1:
        if (Anim_Step(o)) Ovi_KnockedUp(o);
        else if (U16(o + 0x92) >= 0x1C) U16(o + 0x174) = 2;
        break;
    }
}

/* hit kind 1 while down */
void DC2CALL Ovi_KnockDown(gptr o)                                  /* 0x444B80 */
{
    switch (U8(o + 0x56)) {
    case 0: {
        u32 b = U8(o + 0x180) & 3;
        Anim_Start(o, 0x31, b, 0, b + 3);
        U16(o + 0x42) = (u16)(S16(o + 0x5A) * -1000);
        U32(o + 0x50) &= 0xFFFFD78F;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U8(o + 0x17B) = 4;
        U8(o + 0x56)++;
        Ovi_TakeDamage(o);
        if (S16(o + 0x108) < 0) U32(o + 0x50) &= ~8u;
        U16(EXT + 0x2A) = (u16)((b << 4) + 4);
        if (U8(o + 0x170) & 2) Prop_Attach(o, 0x65EC18);
        else Effect_Attach(o, 0x65E9AC);
        Se_Play(o, 0x42);
        break; }
    case 1:
        if (Anim_Step(o)) Ovi_KnockedUp(o);
        break;
    }
}

/* hit kind 1: in the air, down, or standing */
void DC2CALL Ovi_Knock(gptr o)                                      /* 0x444980 */
{
    u16 f = U16(o + 0x174);
    if (f & 4) Ovi_HurtAir(o);
    else if (f & 2) Ovi_KnockDown(o);
    else Ovi_KnockStand(o);
}

/* hit kinds 2 and 3: in the air, down, or standing */
void DC2CALL Ovi_Blow(gptr o)                                       /* 0x444D00 */
{
    u16 f = U16(o + 0x174);
    if (f & 4) Ovi_HurtAir(o);
    else if (f & 2) Ovi_BlowDown(o);
    else Ovi_BlowStand(o);
}

/* the burnt parts go dark */
static void Ovi_Char(gptr p)
{
    for (; p; p = U32(p + 0x54)) U32(p + 0x8C) = 0x404040;
}

/* hit kinds 2/3 standing: thrown about; dies (kind 3 always) */
void DC2CALL Ovi_BlowStand(gptr o)                                  /* 0x444D30 */
{
    gptr parts = U32(o + 0x64);
    switch (U8(o + 0x56)) {
    case 0: {
        u32 b = U8(o + 0x180) & 3;
        U32(o + 0x7C) = 0;
        Anim_Start(o, 0x21, b, 0x40, 2);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U16(o + 0x4A) = U16(o + 0x178) + 0x800;
        U16(o + 0x4A) += (u16)Rand8();
        U16(o + 0x42) = (u16)(S16(o + 0x5A) * -1000);
        U32(o + 0x50) &= 0xFFFFD78F;
        U8(o + 0x17B) = 0x28;
        Ovi_TakeDamage(o);
        if (U16(o + 0x172) == 3) U16(o + 0x108) = 0xFFFF;
        if (S16(o + 0x108) < 0) {
            U32(o + 0x50) &= ~8u;
            Flag_Set(4, U8(o + 0x11C), 1);
            Score_Kill(o);
            if (U8(o + 0x170) & 2) Ovi_Char(parts);
        }
        U16(EXT + 0x2A) = (u16)(b * 8 + 0x10);
        if (U8(o + 0x170) & 2) Prop_Attach(o, 0x65EC18);
        else {
            Obj_HitDir(o);
            Effect_Attach(o, U32(0x703398 + S16(o + 0x178) * 4));
        }
        Se_Play(o, 0x43);
        U8(o + 0x56)++;
        break; }
    case 1:
        if (!Anim_Step(o)) break;
        U16(o + 0x42) = (u16)(S16(o + 0x5A) * -1000);
        if (S16(o + 0x108) < 0) {
            U32(o + 0x50) |= 0x20;
            u8 c = (u8)((U8(o + 0xFF) & 7) + 1);
            if (Rand8() > 0x40) {
                if (c & 1) Anim_Start(o, 0x2C, (u32)U8(o + 0x180) << 1, 0, c);
                else Anim_Start(o, 0xF, (u32)U8(o + 0x180) << 1, 0, c);
            } else Anim_Start(o, 0x2C, c, 0, 0xA);
            U32(o + 0x54) = 0x103;
        } else {
            U16(o + 0x174) = 2;
            U32(o + 0x50) |= 0x20;
            U32(o + 0x54) = 0x501;
        }
        break;
    }
}

/* hit kinds 2/3 while down: pushed along the ground */
void DC2CALL Ovi_BlowDown(gptr o)                                   /* 0x444F90 */
{
    gptr parts = U32(o + 0x64);
    switch (U8(o + 0x56)) {
    case 0: {
        u32 b = U8(o + 0x180) & 3;
        Anim_Start(o, 0x32, b, 0, 3);
        u16 a = U16(o + 0x178);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        gptr m = Tmp_Push(0x20), v = Tmp_Push(8);
        U16(v) = 0; U16(v + 2) = (u16)(a - (u16)(a + 0x800)); U16(v + 4) = 0;
        U16(o + 0x4A) = a + 0x800;
        RotMatrix(v, m);
        U32(v) = 0; U16(v + 4) = 0x50;
        ApplyMatrixSV(m, v, o + 0xF0);
        Tmp_Pop(8); Tmp_Pop(0x20);
        U16(o + 0x42) = (u16)(S16(o + 0x5A) * -1000);
        U32(o + 0x50) &= 0xFFFFD78F;
        U8(o + 0x17B) = 0x14;
        Ovi_TakeDamage(o);
        if (U16(o + 0x172) == 3) U16(o + 0x108) = 0xFFFF;
        if (S16(o + 0x108) < 0) {
            U32(o + 0x50) &= ~8u;
            U8(o + 0x56)++;
            if (U8(o + 0x170) & 2) Ovi_Char(parts);
        } else {
            U16(o + 0x174) = 2;
            U32(o + 0x50) |= 0x20;
            U32(o + 0x54) = 0x501;
        }
        U16(EXT + 0x2A) = (u16)(b * 8 + 0x10);
        Se_Play(o, 0x43);
        if (U8(o + 0x170) & 2) Prop_Attach(o, 0x65EC18);
        else Effect_Attach(o, 0x65EAA0);
        break; }
    case 1:
        if (!Anim_Step(o)) break;
        U32(o + 0xF0) = 0;
        U16(o + 0x42) = (u16)(S16(o + 0x5A) * -1000);
        U32(o + 0x50) |= 0x20;
        U16(o + 0xF4) = 0;
        U32(o + 0x54) = 3;
        break;
    }
}

/* hit in the air (or thrown up, kinds 0..3): flies back, falls to the floor */
void DC2CALL Ovi_HurtAir(gptr o)                                    /* 0x445190 */
{
    gptr parts = U32(o + 0x64);
    u32 h = Floor_Height(o + 0xE0, o + 0x40, 0x40, 0);
    U16(o + 0x5A) = (u16)((s32)(s16)h / -1000);
    u16 k = U16(o + 0x172);
    switch (U8(o + 0x56)) {
    case 0: {
        gptr m = Tmp_Push(0x20), v = Tmp_Push(8);
        U16(v) = 0; U16(v + 2) = (u16)(U16(o + 0x178) - U16(o + 0x4A)); U16(v + 4) = 0;
        RotMatrix(v, m);
        U32(v) = 0; U16(v + 4) = k == 3 ? 0xF0 : k == 2 ? 0x78 : 0x3C;
        ApplyMatrixSV(m, v, o + 0xF0);
        Tmp_Pop(8); Tmp_Pop(0x20);
        U32(o + 0x50) &= 0xFFFFD78F;
        U8(o + 0x17B) = 0x1E;
        U16(o + 0x4A) += (u16)Rand8();
        Ovi_TakeDamage(o);
        if (U16(o + 0x172) == 3) U16(o + 0x108) = 0xFFFF;
        if (S16(o + 0x108) < 0) {
            U32(o + 0x50) &= ~8u;
            Flag_Set(4, U8(o + 0x11C), 1);
            Score_Kill(o);
            if (U8(o + 0x170) & 2) Ovi_Char(parts);
        }
        Obj_HitDir(o);
        if (U16(o + 0x172) < 2) {
            Anim_Start(o, 0x26, 0, 0, 2);
            if (U8(o + 0x170) & 2) Prop_Attach(o, 0x65EC18);
            else Effect_Attach(o, 0x65E9AC);
            Se_Play(o, U16(o + 0x172) == 0 ? 0x41 : 0x42);
        } else {
            Anim_Start(o, 0x29, U8(o + 0x180), 0, 2);
            if (U8(o + 0x170) & 2) Prop_Attach(o, 0x65EC18);
            else Effect_Attach(o, 0x65EAA0);
            Se_Play(o, 0x43);
        }
        U16(EXT + 0x2A) = (u16)((U8(o + 0x180) + (u16)(U16(o + 0x172) << 1)) << 2);
        U8(o + 0x56)++;
        break; }
    case 1:
        if (Anim_Step(o)) Anim_Start(o, U16(o + 0x172) < 2 ? 0x2B : 0x36, 0, 1, 1);
        U16(o + 0x42) += 0xF0;
        if (S16(o + 0x42) < (s16)h) break;
        U16(o + 0x174) &= 0xFFFB;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U16(o + 0x42) = (u16)(S16(o + 0x5A) * -1000);
        if (S16(o + 0x108) < 0) {
            U32(o + 0x7C) = 0;
            Anim_Start(o, U16(o + 0x172) < 2 ? 0x27 : 0x2A, 0, 0x40, 2);
            U32(o + 0x54) = 0x103;
            break;
        }
        U16(o + 0x174) = 2;
        U16(EXT + 0x2A) = (u16)(((u16)U8(o + 0x180) + (u16)(U16(o + 0x172) << 1)) << 2);
        U16(EXT + 0x2A) += 0x22;
        U32(o + 0x7C) = 0;
        Anim_Start(o, U16(o + 0x172) < 2 ? 0x27 : 0x2A, 0, 0x40, 2);
        U32(o + 0x50) |= 0x20;
        U32(o + 0x54) = 0x501;
        break;
    }
}
