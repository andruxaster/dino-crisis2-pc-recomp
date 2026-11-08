/* Room script built-ins that hand their arguments to other game systems
 * (messages, animation, models, sound, menus, fades, stage loading) and the
 * small helpers they use. Arguments are on the thread's stack in push order:
 * S(0) is the last one pushed. Functions that are not rewritten yet are
 * called through orig.h. */
#include "dc2/game.h"
#include "dc2/orig.h"

#define SP_(t)     U32((t) + T_SP)
#define S(t, i)    U32(SP_(t) + 4 * (i))
#define ADV(t)     (U32((t) + T_PC) += 2)
#define SEL(t)     ((s32)S8((t) + T_SEL1))
#define OBJ(i)     (U32(G() + 0x940) + (i) * 0x260)
#define EFF(i)     (U32(G() + 0x93C) + (i) * 0x70)
#define K2(i)      (U32(G() + 0x944) + (i) * 0xAC)
#define K3(i)      (U32(G() + 0x948) + (i) * 0x34)
#define A_OBJ_TYPE_TABLE 0x87DCA0u  /* u32 -> handler per object type */
#define A_OBJ_COUNT      0x87F940u  /* u32 number of object slots */
#define A_EFF_COUNT      0x87DE04u
#define A_MSG_EVENT      0x87FC45u  /* u8, 0xFF = none */

/* ---- helpers ------------------------------------------------------------------ */
void DC2CALL Obj_Init(u32 slot)                                     /* 0x48D820 */
{
    gptr o = OBJ(slot);
    memset(MEM(o), 0, 0x260);
    U8(o + 0xFF) = (u8)slot;
    U32(o + 0x74) = 0x1000; U32(o + 0x70) = 0x1000; U32(o + 0x6C) = 0x1000;
    U32(o + 0x50) = 1;
    U8(o + 0xFA) = 0x80; U8(o + 0xF9) = 0x80; U8(o + 0xF8) = 0x80;
    U8(o + 0xFC) = 5;
}

gptr DC2CALL Obj_Part(gptr obj, u32 n)                              /* 0x48D3F0: n-th part */
{
    while (n--) obj = U32(obj + 0x54);
    return obj;
}

gptr DC2CALL Script_SelObj(gptr t)                                  /* 0x48BE50 */
{
    switch (U8(t + T_SEL0)) {
    case 0: return OBJ(SEL(t));
    case 1: return EFF(SEL(t));
    case 2: return K2(SEL(t));
    case 3: return K3(SEL(t));
    default: return 0;
    }
}

/* sound settings of a room: G + per-area table + room * 4 */
gptr DC2CALL Room_SoundSlot(u32 area, u32 room)                     /* 0x48CC30 */
{
    static const u16 base[10] = {0x138C, 0x13A4, 0x13E8, 0x1400, 0x141C,
                                 0x144C, 0x1460, 0x1484, 0x14A4, 0x14D4};
    area &= 0xFF;
    if (area > 9) return G() + 0x138C;
    return G() + base[area] + (room & 0xFF) * 4;
}

/* 0x474B10 is a lone RET: too short for a hook, so it stays out of hooks.c */
void DC2CALL DrawSync(u32 mode) { (void)mode; }

static void obj_type_call(gptr obj)
{
    Orig_Call1(U32(U32(A_OBJ_TYPE_TABLE) + U8(obj + 0x58) * 4), obj);
}

/* ---- opcodes -------------------------------------------------------------------- */
u32 DC2CALL ScrOp_Message(gptr t)                                  /* 11: message(text offset) */
{
    Message_Show(0x19, 0xB0, S(t, 0) + U32(G() + G_SCRIPT), 2);
    U8(A_MSG_EVENT) = 0xFF;
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_ObjSpawn(gptr t)                                 /* 1A: 10 args */
{
    gptr o = OBJ(S(t, 9));
    if (S(t, 0) && Flag_Check(4, S(t, 0))) { ADV(t); return 0; }   /* already gone */
    Enemy_LoadModel(S(t, 8), S(t, 2));
    Obj_Init(S(t, 9));
    U8(o + 0x58) = U8(SP_(t) + 0x20);
    U16(o + 0xE0) = U16(o + 0x40) = U16(SP_(t) + 0x1C);
    U16(o + 0xE2) = U16(o + 0x42) = U16(SP_(t) + 0x18);
    U16(o + 0xE4) = U16(o + 0x44) = U16(SP_(t) + 0x14);
    U16(o + 0x4A) = U16(SP_(t) + 0x10);
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);         /* floor level */
    U32(o + 0x60) = S(t, 3);
    if (S(t, 2) >= 0x500000) U32(o + 0x88) = S(t, 2);          /* animation data */
    else U16(o + 0x10C) = U16(SP_(t) + 8);                      /* model variant */
    U16(o + 0x1C0) = U16(SP_(t) + 4);
    U8(o + 0x11C) = U8(SP_(t));
    if (U8(o + 0x58) <= 0xF) Flag_Set(2, 0x14, 1);
    obj_type_call(o);
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_ObjCall(gptr t)                                  /* 1B: fn(selected, a) */
{
    gptr p = Script_SelObj(t), objs = U32(G() + 0x940);
    if (p >= objs && p <= objs + U32(A_OBJ_COUNT) * 0x260 - 0x260) {
        gptr e = Obj_Part(U32(p + 0x64), 0);
        gptr c = Tmp_Push(0x60);
        memcpy(MEM(c), MEM(p), 0x60);           /* object header with the root part's position */
        if (e) { U16(c + 0x40) = U16(e + 0x40); U16(c + 0x42) = U16(e + 0x42); U16(c + 0x44) = U16(e + 0x44); }
        Fn_48C0B0(c, S(t, 0));
        Tmp_Pop(0x60);
    } else {
        Fn_48C0B0(p, S(t, 0));
    }
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_WaitEvent(gptr t)                                /* 20: r0 = message event */
{
    u8 ev = U8(A_MSG_EVENT);
    if (ev == 0xFF) return 1;
    S32(t + T_REG) = ev;
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_AnimSet(gptr t)                                  /* 21: anim(obj, a, b, c, d) */
{
    gptr o = OBJ(S(t, 4));
    if (S(t, 0) == 0) U32(o + 0x7C) = 0;
    Anim_Start(o, S(t, 3), S(t, 1), S(t, 2), S(t, 0));
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Set87FC47(gptr t)                                /* 22 */
{
    U8(0x87FC47) = U8(SP_(t));
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_LoadModel(gptr t)                                /* 23: model(type, variant) or (set) */
{
    u32 a = S(t, 1);
    if (a >= 0x500000) Enemy_LoadModel(U8(a + 1), U8(a + 4));    /* enemy_set header */
    else Enemy_LoadModel(a, S(t, 0));
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_WaitB4A(gptr t)                                  /* 24: r0 = G+0xB4A once set */
{
    u8 v = U8(G() + 0xB4A);
    if (v == 0xFF) return 1;
    S32(t + T_REG) = v;
    U8(G() + 0xB4A) = 0xFF;
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Call46FBC0(gptr t)                               /* 25 */
{
    Fn_46FBC0(S(t, 2), S(t, 1), S(t, 0));
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Timer(gptr t)                                    /* 26: timer(seconds) */
{
    Timer_Set(S(t, 0) * 30, 0);
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Fade(gptr t)                                     /* 27: fade(a, b, c) */
{
    Fade_Set(S(t, 2), S(t, 1), S(t, 0));
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_WaitFlag(gptr t)                                 /* 28: wait until flag == v */
{
    if (Flag_Check(S(t, 2), S(t, 1)) != S(t, 0)) return 1;
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Character(gptr t)                                /* 29: switch playable character */
{
    gptr g = G(), o = U32(g + 0x940);
    s32 a = S32(SP_(t));
    int reset = 0;
#define CUR_SLOT(g) ((g) + 0x10B6 + U8((g) + 0x108F) * 2)
    if (a == -1) {                               /* swap between the two characters */
        U16(CUR_SLOT(g)) = U16(g + 0x10B4);
        U8(g + 0x108F) = 1 - U8(g + 0x108F);
        U16(o + 0x108) = 0x4B0; U16(o + 0x10A) = 0x4B0;
        U16(G() + 0x1200) = 0; U16(o + 0x17E) = 0;
        g = G();
        U16(g + 0x10B4) = U16(CUR_SLOT(g));
        for (u32 i = 0x20; i < 0x40; i++) Flag_Set(8, i, 0);
    } else if (a == -2) {
        U16(CUR_SLOT(g)) = U16(g + 0x10B4);
        g = G();
        U8(g + 0x108F) = U8(g + 0x108F) ? 0 : 2;
        g = G();
        U16(g + 0x10B4) = U16(CUR_SLOT(g));
        U16(G() + 0x1200) = 0; U16(o + 0x17E) = 0;
    } else if (a == -3) {
        reset = 1;
        if (U8(g + 0x108F) == 1) {
            U16(g + 0x10B8) = U16(g + 0x10B4);
            U8(G() + 0x108F) = 3;
            U8(o + 0x58) = 0xF;
            U16(G() + 0x10B4) = 0xC;
        } else {
            U8(g + 0x108F) = 1;
            g = G();
            U16(g + 0x10B4) = U16(CUR_SLOT(g));
            U8(o + 0x58) = 0;
        }
        U16(o + 0x108) = 0x4B0; U16(o + 0x10A) = 0x4B0;
        U16(G() + 0x1200) = 0; U16(o + 0x17E) = 0;
    } else {
        U16(g + 0x10B4) = (u16)a;
    }
#undef CUR_SLOT
    g = G();
    if (U8(g + 0x108F) != 3) {
        u32 v = U16(g + 0x10B4);
        Stage_Load(v & 0xFF, v >> 8);
    }
    if (reset) { U8(o + 0x57) = 0; U8(o + 0x56) = 0; U8(o + 0x55) = 0; U8(o + 0x54) = 0; }
    o = U32(G() + 0x940);
    if (U8(o + 0x50) & 1) {
        obj_type_call(o);
        U16(o + 0x4A) &= 0xFFF;
    }
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_ObjMove(gptr t)                                  /* 2B: fn(obj, a, b, c) */
{
    Obj_Set118(OBJ(S(t, 3)), S(t, 2), U16(SP_(t) + 4), U16(SP_(t)));
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Menu(gptr t)                                     /* 2C: menu(mode, ...) */
{
    if (Flag_Check(2, 6)) return 1;
    gptr sp = SP_(t);
    u32 last = S(t, 4) == 1 ? (u32)U8(sp) << 8 | 0xFF : U16(sp);
    Menu_Open(U16(sp + 0x10), U16(sp + 0xC), U16(sp + 8), U16(sp + 4), last);
    ADV(t);
    return 1;
}

u32 DC2CALL ScrOp_RoomReset(gptr t)                                /* 2D: room_reset(area<<8|room) */
{
    u32 v = S(t, 0);
    if (v == 0xFFFF) {
        for (u32 i = 1; i < 0x5D; i++) U8(G() + 0x12CF + i) = 0;   /* all rooms */
    } else {
        u32 i = Room_GlobalIndex((u8)((s32)v >> 8), (u8)v) & 0xFF;
        U8(G() + 0x12D0 + i) = 0;
    }
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Call48C640(gptr t)                               /* 2E */
{
    Fn_48C640(S(t, 2), S(t, 1), S(t, 0));
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Effect2F(gptr t)                                 /* 2F */
{
    s32 a = S32(SP_(t));
    if (a != -1) {
        Eff_KillKind((u8)a);
    } else {
        Eff_KillRoom(0);
        for (u32 i = 0; i < U32(A_EFF_COUNT); i++) {
            gptr e = U32(G() + 0x93C) + i * 0x70;
            s8 k = S8(e + 0x4F);
            if ((U8(e + 0x50) & 1) && U8(e + 0x4E) == 3 && k >= 3 && k != 0x10) Eff_Free(e);
        }
    }
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_ObjAnim(gptr t)                                  /* 46: anim on the selected object */
{
    Anim_Start(OBJ(SEL(t)), S(t, 3), S(t, 1), S(t, 2), S(t, 0));
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_LinkPart(gptr t)                                 /* 47: selected.link = part(src, n) */
{
    gptr dst = 0, g = G();
    switch (U8(t + T_SEL0)) {
    case 1: dst = U32(g + 0x93C) + SEL(t) * 0x70 + 0x594; break;   /* effect (index + 12) +0x54 */
    case 2: dst = U32(g + 0x944) + SEL(t) * 0xAC + 0x60; break;
    default: break;
    }
    switch (S(t, 2)) {
    case 0: U32(dst) = Obj_Part(U32(OBJ(S(t, 1)) + 0x64), S(t, 0)); break;
    case 1: U32(dst) = Obj_Part(EFF(S(t, 1) + 12), S(t, 0)); break;
    default: break;
    }
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Random(gptr t)                                   /* 48: r0 = rand() */
{
    u32 r = Crt_rand() & 0xFFFF;
    S32(t + T_REG) = (s32)r;
    Script_SetCond(t, (s32)r);
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_SoundSlot(gptr t)                                /* 49: room sound slot = (a, b) */
{
    u32 v = U16(SP_(t) + 8);
    gptr p = Room_SoundSlot(v >> 8, v);
    U16(p) = U16(SP_(t) + 4);
    U16(p + 2) = U16(SP_(t));
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_WaitCamera(gptr t)                               /* 4B: r0 = camera once it changes */
{
    u8 cam = U8(G() + 0x7C4);
    if (U8(t + T_A0) == cam) return 1;
    U8(t + T_A0) = cam;
    S32(t + T_REG) = cam;
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Call478B80(gptr t)                               /* 4C */
{
    Fn_478B80(0, S(t, 0));
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Call481D20(gptr t)                               /* 4D */
{
    Door_Load(S(t, 0));
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_WaitFile(gptr t)                                 /* 4E: wait for file loading */
{
    if (File_IsBusy()) return 1;
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_EnemySet(gptr t)                                 /* 4F: enemy_set(header) */
{
    gptr hdr = S(t, 0), g = G();
    u32 room = Room_GlobalIndex(U8(g + 0x1091), U8(g + 0x1090)) & 0xFF;
    g = G();
    if (U32(g + 0xA68) != S(t, 0)) {
        U32(g + 0xA68) = S(t, 0);
        if (hdr && U8(hdr + 7) != U8(G() + 0x132C + room)) {
            U8(G() + 0x132C + room) = U8(hdr + 7);
            U8(G() + 0x12D0 + room) = 0;
        }
        memset(MEM(G() + 0xA80), 0, 0x48);
        U8(G() + 0xA89) = 0xFF;
        U8(G() + 0xAD1) = 0xFF;
        if (hdr) Enemy_LoadModel(U8(hdr + 1), U8(hdr + 4));
    }
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Op50(gptr t)                                     /* 50 */
{
    Fn_473130(S(t, 0));
    U32(0x7593A8) = U32(G() + 0xB18);
    U32(G() + 0xB18) = 0;
    U32(0x7593AC) = U32(G() + 0xB14);
    Flag_Set(1, 0xC, 1);
    Fade_Set(0, 3, 1);
    ADV(t);
    return 1;
}

u32 DC2CALL ScrOp_Op51(gptr t)                                     /* 51 */
{
    Fn_4731F0();
    if (!Flag_Check(2, 0x17)) {
        Fade_Set(1, 1, 1);
        U32(G() + 0xB18) = U32(0x7593A8);
        U32(G() + 0xB14) = U32(0x7593AC);
        ADV(t);
    }
    return 1;
}

u32 DC2CALL ScrOp_Op52(gptr t)                                     /* 52: on the selected effect/k2 */
{
    switch (U8(t + T_SEL0)) {
    case 1: Fn_474B20(EFF(SEL(t) + 12)); break;
    case 2: Fn_448080(K2(SEL(t))); break;
    default: break;
    }
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Call48C600(gptr t)                               /* 53 */
{
    Fn_48C600(S(t, 0));
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_ObjsHold(gptr t)                                 /* 54: 1 restore, else save + clear */
{
    gptr save = 0x87FC4C;
    u32 first = U32(G() + 0xA7C);
    gptr f = OBJ(first + 1) + 0x50;
    if (U32(A_OBJ_COUNT) - first - 1 != 0) {
        u32 i = 0;
        do {
            if (S(t, 0) == 1) U32(f) = U32(save);
            else { U32(save) = U32(f); U32(f) = 0; }
            i++; f += 0x260; save += 4;
        } while (i < U32(A_OBJ_COUNT) - U32(G() + 0xA7C) - 1);
    }
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Call48CE20(gptr t)                               /* 55 */
{
    gptr sp = SP_(t);
    Fn_48CE20(U8(sp + 0xD), U16(sp + 8), U16(sp + 4), U16(sp));
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Op56(gptr t)                                     /* 56 */
{
    U16(G() + 0x680) = 0xFFFF;
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Call4529C0(gptr t)                               /* 57 */
{
    Fn_4529C0(S(t, 0));
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_PartHide(gptr t)                                 /* 58: part(selected, n) visible (1) / hidden */
{
    gptr p = Obj_Part(U32(OBJ(SEL(t)) + 0x64), S(t, 1));
    if (S(t, 0) == 1) U32(p + 0x50) &= ~0x10000000u;
    else U32(p + 0x50) |= 0x10000000u;
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Texture(gptr t)                                  /* 59: upload texture page + CLUT */
{
    if (Flag_Check(9, 0x33)) {
        Armor_Load(S(t, 0));
        gptr r = Tmp_Push(8);
        U16(r) = U16(SP_(t) + 0xC); U16(r + 2) = U16(SP_(t) + 8);
        U16(r + 4) = 0x80; U16(r + 6) = 0x100;
        Gpu_LoadImage(r, 0x6C1500);
        DrawSync(0);
        U16(r) = 0; U16(r + 2) = U16(SP_(t) + 4);
        U16(r + 4) = 0x100; U16(r + 6) = 1;
        Gpu_LoadImageIfChanged(r, 0x6D1500);
        DrawSync(0);
        Tmp_Pop(8);
    }
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_CamMove(gptr t)                                  /* 40: camera/obj move, 5 modes */
{
    gptr o = OBJ(SEL(t)), sp = SP_(t);
    gptr b = Tmp_Push(16);
    memset(MEM(b), 0, 16);
    u32 mode;
    switch (S(t, 4)) {
    case 0: mode = S(t, 2); break;
    case 1: mode = 1; break;
    case 2: mode = (S(t, 2) << 8 | S(t, 3)) << 8 | 2; break;
    case 3: U16(b) = U16(sp + 0x10); mode = S(t, 5);
            U16(b + 2) = U16(sp + 0xC); U16(b + 4) = U16(sp + 8); break;
    case 4: mode = 4; U16(b) = U16(sp + 0xC); U16(b + 0xA) = U16(sp + 8); break;
    default: mode = t; goto call;           /* the original passes its own argument */
    }
    U16(b + 6) = U16(sp + 4);
    U16(b + 0xE) = U16(sp);
call:
    Fn_47A8C0(o, mode, b, b + 8);
    Tmp_Pop(16);
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_WaitCutscene(gptr t)                             /* 41 */
{
    if (S(t, 0)) {
        if (!Flag_Check(2, 0)) return 1;
    } else {
        if (Flag_Check(2, 0) || Flag_Check(2, 2)) return 1;
    }
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Cutscene(gptr t)                                 /* 42: -1 ends, else start(a, b) */
{
    if (Flag_Check(2, 2)) return 1;
    if (S32(SP_(t)) == -1) Cutscene_End();
    else Voice_Play(S(t, 1), S(t, 0));
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Sound(gptr t)                                    /* 43: sound(id, flags) */
{
    u32 id = U16(SP_(t)), fl = U8(SP_(t) + 4);
    if (fl & 1) Fn_48CD70(id >> 8, id & 0xFF);
    if (fl & 2) Fn_48CE60(id >> 8);
    if (fl & 4) {
        gptr c = 0x87FB8C + (id >> 8) * 20;
        U8(c + 4) = 0x80; U8(c + 5) = 3; U16(c) = 0;
    }
    if (fl & 0x10) {
        u32 mask = U16(Room_SoundSlot(U8(G() + 0x1091), U8(G() + 0x1090)) + 2);
        for (u32 i = 0; i < 8; i++)
            if (mask & (1u << i)) {
                gptr c = 0x87FB8C + (i & 3) * 20;
                U8(c + 4) = 0x80; U8(c + 5) = 2; U16(c) = 0;
            }
    }
    if (fl & 8) Fn_48C9C0(U16(Room_SoundSlot(U8(G() + 0x1091), U8(G() + 0x1090)) + 2));
    ADV(t);
    return 0;
}
