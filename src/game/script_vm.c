/* Room script virtual machine (G+0xA54), see tools/dc2script.py.
 * 10 threads of 0xA4 bytes at *0x87FC20; handler table 0x738C40. */
#include "dc2/game.h"
#include <stdio.h>

#define THR(t, off)   ((t) + (off))
#define PC(t)         U32(THR(t, T_PC))
#define SP(t)         U32(THR(t, T_SP))
#define REG(t, i)     S32(THR(t, T_REG) + (i) * 4)

s32 DC2CALL Script_GetValue(gptr thr, u32 type, s32 v)            /* 0x48B8E0 */
{
    switch (type & 0xFF) {
    case 1: return REG(thr, v);
    case 2: return S32(SP(thr) + v * 4);
    case 6: return S32(U32(A_SCR_GLOBALS) + v * 4);
    default: return v;
    }
}

void DC2CALL Script_SetValue(gptr thr, u32 type, s32 v, s32 x)    /* 0x48B930 */
{
    switch (type & 0xFF) {
    case 1: REG(thr, v) = x; break;
    case 2: S32(SP(thr) + v * 4) = x; break;
    case 6: S32(U32(A_SCR_GLOBALS) + v * 4) = x; break;
    default: break;
    }
}

s32 DC2CALL Script_BinOp(s32 a, s32 b, u32 op)                    /* 0x48B980 */
{
    switch (op) {
    case 0: return (s32)((u32)a + (u32)b);
    case 1: return (s32)((u32)a - (u32)b);
    case 2: return (s32)((u32)a * (u32)b);
    case 3: return a / b;
    case 4: return a % b;
    case 5: return a | b;
    case 6: return a & b;
    case 7: return a ^ b;
    case 8: return ~a;
    case 9: case 11: return (s32)((u32)a << (b & 31));   /* x86 shl uses cl & 31 */
    case 10: case 12: return a >> (b & 31);
    case 13: return a > b;
    case 14: return a < b;
    case 15: return a >= b;
    case 16: return a <= b;
    case 17: return a == b;
    case 18: return a != b;
    case 19: return a == 0;
    case 20: return (s32)((u32)a + 1);
    case 21: return (s32)((u32)a - 1);
    case 24: return a || b;
    case 25: return a && b;
    default: return a;                                     /* 22, 23, > 25 */
    }
}

void DC2CALL Script_SetCond(gptr thr, s32 v)                       /* 0x48BB20 */
{
    if (v == 0) U8(THR(thr, T_COND)) = 0x2A;        /* eq, ge, le */
    else if (v > 0) U8(THR(thr, T_COND)) = 0x4C;    /* gt, ge, ne */
    else U8(THR(thr, T_COND)) = 0x70;               /* lt, le, ne */
}

static gptr thread_addr(s32 id) { return U32(A_SCR_THREADS) + id * SCR_THREAD_SIZE; }

void DC2CALL Script_StartThread(u32 id, u32 routine)               /* 0x48BDB0 */
{
    gptr scr = U32(G() + G_SCRIPT);
    gptr ent = scr + (s16)routine * 4;
    gptr t = thread_addr((s8)id);
    memset(MEM(t), 0, SCR_THREAD_SIZE);
    U8(THR(t, T_STATE)) = 1;
    PC(t) = scr + U32(ent);
    SP(t) = t + T_STACK;
    U8(THR(t, T_A0)) = 0xFF;
}

void DC2CALL Script_KillThread(u32 id)                             /* 0x48BE20 */
{
    memset(MEM(thread_addr((s8)id)), 0, SCR_THREAD_SIZE);
}

/* ---- opcodes ------------------------------------------------------------ */
u32 DC2CALL ScrOp_Nop(gptr t) { PC(t) += 2; return 0; }            /* 0x489990 */

u32 DC2CALL ScrOp_Load(gptr t)                                     /* 01 d t v16 */
{
    gptr pc = PC(t);
    s32 v = Script_GetValue(t, U8(pc + 2), S16(pc + 4));
    REG(t, U8(pc + 1)) = v;
    Script_SetCond(t, v);
    PC(t) += 6;
    return 0;
}

u32 DC2CALL ScrOp_Store(gptr t)                                    /* 02 d t v16 */
{
    gptr pc = PC(t);
    s32 x = REG(t, U8(pc + 1));
    Script_SetValue(t, U8(pc + 2), S16(pc + 4), x);
    Script_SetCond(t, x);
    PC(t) += 6;
    return 0;
}

u32 DC2CALL ScrOp_Arith(gptr t)                                    /* 03 op d t v16 */
{
    gptr pc = PC(t);
    u8 d = U8(pc + 2);
    s32 x = Script_GetValue(t, U8(pc + 3), S16(pc + 4)), r;
    if (d == 0xFF) {                                 /* stack pointer, in slots */
        r = Script_BinOp((s32)SP(t), x << 2, U8(PC(t) + 1));
        SP(t) = (u32)r;
    } else {
        r = Script_BinOp(REG(t, d), x, U8(PC(t) + 1));
        REG(t, d) = r;
    }
    Script_SetCond(t, r);
    PC(t) += 6;
    return 0;
}

u32 DC2CALL ScrOp_Return(gptr t)                                   /* 04 */
{
    u8 depth = U8(THR(t, T_DEPTH));
    if (depth) {
        U8(THR(t, T_DEPTH)) = depth - 1;
        gptr sp = SP(t);
        PC(t) = U32(sp);
        SP(t) = sp + 4;
        return 0;
    }
    U8(THR(t, T_STATE)) = 0;
    return 1;
}

u32 DC2CALL ScrOp_Push(gptr t)                                     /* 05 t v16 */
{
    gptr pc = PC(t);
    s32 x = Script_GetValue(t, U8(pc + 1), S16(pc + 2));
    SP(t) -= 4;
    S32(SP(t)) = x;
    PC(t) += 4;
    return 0;
}

u32 DC2CALL ScrOp_Pop(gptr t)                                      /* 06 t v16 */
{
    gptr sp = SP(t);
    s32 x = S32(sp);
    SP(t) = sp + 4;
    gptr pc = PC(t);
    Script_SetValue(t, U8(pc + 1), S16(pc + 2), x);
    PC(t) += 4;
    return 0;
}

u32 DC2CALL ScrOp_Branch(gptr t)                                   /* 07 c s16 */
{
    gptr pc = PC(t);
    u8 c = U8(pc + 1);
    u8 bit = (u8)(1u << (c & 31));                   /* only the low byte is tested */
    if ((bit & U8(THR(t, T_COND))) || c == 0)
        PC(t) = pc + S16(pc + 2);
    else
        PC(t) = pc + 4;
    return 0;
}

u32 DC2CALL ScrOp_Call(gptr t)                                     /* 08 00 s16 */
{
    gptr sp = SP(t) - 4;
    SP(t) = sp;
    U32(sp) = PC(t) + 4;
    gptr scr = U32(G() + G_SCRIPT);
    PC(t) = scr + U32(scr + S16(PC(t) + 2) * 4);
    U8(THR(t, T_DEPTH))++;
    return 0;
}

u32 DC2CALL ScrOp_Wait(gptr t)                                     /* 10: wait(frames) */
{
    U8(THR(t, T_STATE)) = 2;
    U16(THR(t, T_WAIT)) = U16(SP(t));
    PC(t) += 2;
    return 1;
}

u32 DC2CALL ScrOp_FlagSet(gptr t)                                  /* 1C: flag_set(g, bit, on) */
{
    gptr sp = SP(t);
    if (U32(sp + 8) == 1 && U32(sp + 4) == 0xFF) {
        U32(G() + 0xB14) = U32(sp) == 1 ? 0xFFFFFFBFu : 0;
    } else {
        Flag_Set(U32(sp + 8), U32(sp + 4), U32(sp));
    }
    PC(t) += 2;
    return 0;
}

u32 DC2CALL ScrOp_FlagCheck(gptr t)                                /* 1D: r0 = flag_check(g, bit) */
{
    gptr sp = SP(t);
    if (Flag_Check(U32(sp + 4), U32(sp))) {
        U8(THR(t, T_COND)) = 0x40;
        REG(t, 0) = 1;
    } else {
        U8(THR(t, T_COND)) = 0x02;
        REG(t, 0) = 0;
    }
    PC(t) += 2;
    return 0;
}

/* ---- dispatcher ----------------------------------------------------------- */
u32 DC2CALL ScrOp_Original(gptr t)
{
    u8 op = U8(PC(t));
#ifdef DC2_IN_PROCESS
    /* hook build / harness: the original handler is in this process */
    ScrOpFn orig = (ScrOpFn)(uintptr_t)U32(A_SCR_OPTABLE + op * 4);
    return orig(t);
#else
    fprintf(stderr, "script: opcode %02X at %08X not reimplemented, thread stopped\n", (unsigned)op, (unsigned)PC(t));
    U8(THR(t, T_STATE)) = 0;
    return 1;
#endif
}

ScrOpFn g_scr_ops[96] = {
    [0x00] = ScrOp_Nop, [0x01] = ScrOp_Load, [0x02] = ScrOp_Store, [0x03] = ScrOp_Arith,
    [0x04] = ScrOp_Return, [0x05] = ScrOp_Push, [0x06] = ScrOp_Pop, [0x07] = ScrOp_Branch,
    [0x08] = ScrOp_Call, [0x09 ... 0x0F] = ScrOp_Nop, [0x10] = ScrOp_Wait,
    [0x12] = ScrOp_ObjSelect, [0x13] = ScrOp_EffectInit, [0x14] = ScrOp_Obj14Init,
    [0x15] = ScrOp_ThreadStart, [0x16] = ScrOp_ThreadKill, [0x17] = ScrOp_ThreadPause,
    [0x18] = ScrOp_ThreadResume, [0x19] = ScrOp_ThreadGoto, [0x1C] = ScrOp_FlagSet, [0x1D] = ScrOp_FlagCheck,
    [0x1E] = ScrOp_ObjSet, [0x1F] = ScrOp_ObjGet, [0x2A] = ScrOp_WaitFrame,
    [0x44] = ScrOp_Obj44Init, [0x45] = ScrOp_ObjEnable, [0x4A] = ScrOp_CutsceneCamera,
    [0x34] = ScrOp_Nop, [0x3C] = ScrOp_Nop, [0x3D] = ScrOp_Nop, [0x3F] = ScrOp_Nop,
    [0x5A ... 0x5F] = ScrOp_Nop,
    [0x11] = ScrOp_Message,
    [0x1A] = ScrOp_ObjSpawn,
    [0x1B] = ScrOp_ObjCall,
    [0x20] = ScrOp_WaitEvent,
    [0x21] = ScrOp_AnimSet,
    [0x22] = ScrOp_Set87FC47,
    [0x23] = ScrOp_LoadModel,
    [0x24] = ScrOp_WaitB4A,
    [0x25] = ScrOp_Call46FBC0,
    [0x26] = ScrOp_Timer,
    [0x27] = ScrOp_Fade,
    [0x28] = ScrOp_WaitFlag,
    [0x29] = ScrOp_Character,
    [0x2B] = ScrOp_ObjMove,
    [0x2C] = ScrOp_Menu,
    [0x2D] = ScrOp_RoomReset,
    [0x2E] = ScrOp_Call48C640,
    [0x2F] = ScrOp_Effect2F,
    [0x30] = ScrOp_AotDoor,
    [0x31] = ScrOp_Aot31,
    [0x32] = ScrOp_Aot32,
    [0x33] = ScrOp_Aot33,
    [0x35] = ScrOp_AotItem,
    [0x36] = ScrOp_Aot36,
    [0x37] = ScrOp_Aot37,
    [0x38] = ScrOp_Aot38,
    [0x39] = ScrOp_Aot39,
    [0x3A] = ScrOp_Aot3A,
    [0x3B] = ScrOp_Aot3B,
    [0x3E] = ScrOp_AotRun,
    [0x40] = ScrOp_CamMove,
    [0x41] = ScrOp_WaitCutscene,
    [0x42] = ScrOp_Cutscene,
    [0x43] = ScrOp_Sound,
    [0x46] = ScrOp_ObjAnim,
    [0x47] = ScrOp_LinkPart,
    [0x48] = ScrOp_Random,
    [0x49] = ScrOp_SoundSlot,
    [0x4B] = ScrOp_WaitCamera,
    [0x4C] = ScrOp_Call478B80,
    [0x4D] = ScrOp_Call481D20,
    [0x4E] = ScrOp_WaitFile,
    [0x4F] = ScrOp_EnemySet,
    [0x50] = ScrOp_Op50,
    [0x51] = ScrOp_Op51,
    [0x52] = ScrOp_Op52,
    [0x53] = ScrOp_Call48C600,
    [0x54] = ScrOp_ObjsHold,
    [0x55] = ScrOp_Call48CE20,
    [0x56] = ScrOp_Op56,
    [0x57] = ScrOp_Call4529C0,
    [0x58] = ScrOp_PartHide,
    [0x59] = ScrOp_Texture,
};

void DC2CALL Script_Run(void)                                      /* 0x48BC50 */
{
    gptr t = U32(A_SCR_THREADS);
    for (int n = SCR_NTHREADS; n; n--, t += SCR_THREAD_SIZE) {
        u8 st = U8(THR(t, T_STATE));
        if (st == 2) {
            if (--U16(THR(t, T_WAIT)) != 0) continue;
            U8(THR(t, T_STATE)) = 1;
        } else if (st != 1) {
            continue;
        }
        for (;;) {
            u8 op = U8(PC(t));
            ScrOpFn f = g_scr_ops[op] ? g_scr_ops[op] : ScrOp_Original;
            if (f(t)) break;
        }
    }
}

/* ---- thread control built-ins (arguments on the stack) ---------------------- */
u32 DC2CALL ScrOp_ThreadStart(gptr t)                              /* 15: thread_start(id, routine) */
{
    if (!Flag_Check(0, 9)) {
        gptr sp = SP(t);
        Script_StartThread(U8(sp + 4), U16(sp));
    }
    PC(t) += 2;
    return 0;
}

u32 DC2CALL ScrOp_ThreadKill(gptr t)                               /* 16: thread_kill(id) */
{
    Script_KillThread(U8(SP(t)));
    if (!U8(THR(t, T_STATE))) return 1;            /* the thread killed itself */
    PC(t) += 2;
    return 0;
}

u32 DC2CALL ScrOp_ThreadPause(gptr t)                              /* 17: thread_pause(id) */
{
    U8(thread_addr(S32(SP(t))) + T_STATE) |= 0x80;
    PC(t) += 2;
    return 0;
}

u32 DC2CALL ScrOp_ThreadResume(gptr t)                             /* 18: thread_resume(id) */
{
    gptr st = thread_addr(S32(SP(t))) + T_STATE;
    if (U8(st) & 0x80) U8(st) &= 0x7F;
    PC(t) += 2;
    return 0;
}

u32 DC2CALL ScrOp_ThreadGoto(gptr t)                               /* 19: thread_goto(routine) */
{
    gptr scr = U32(G() + G_SCRIPT);
    gptr ent = scr + S32(SP(t)) * 4;
    memset(MEM(t), 0, 0xA4);
    U8(THR(t, T_STATE)) = 1;
    PC(t) = U32(G() + G_SCRIPT) + U32(ent);
    SP(t) = t + T_STACK;
    U8(THR(t, T_DEPTH)) = 0;
    return 0;
}
