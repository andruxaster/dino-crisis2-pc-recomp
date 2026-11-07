/* Room script built-ins 30..3B: define an AOT entry (G+0xA5C, 32 x 40 bytes;
 * trigger areas: doors, items, messages, ...) and 3E: run an entry's kind
 * handler (table 0x738B68).
 * Entry: +0 flags (bit 0 active), +1 kind (= opcode - 0x30), +2 level,
 * +4.. area (s16 x/z pairs) and kind-specific data.
 * Every opcode takes its entry index first; S(i) is stack slot i (S(0) is
 * the last argument pushed). */
#include "dc2/game.h"
#include "dc2/orig.h"

#define SP_(t)    U32((t) + T_SP)
#define S(t, i)   U32(SP_(t) + 4 * (i))
#define ADV(t)    (U32((t) + T_PC) += 2)
#define A_AOT_KIND_TABLE 0x738B68u

/* common head: index in slot n, flags slot n-1 (| or), level slot n-2,
 * then nw halfwords from slots n-3 downwards to +4 */
static gptr aot_head(gptr t, int n, u8 kind, u8 orf, int nw)
{
    gptr sp = SP_(t);
    gptr e = U32(G() + 0xA5C) + S(t, n) * 0x28;
    U8(e + 1) = kind;
    U8(e) = U8(sp + 4 * (n - 1)) | orf;
    U8(e + 2) = U8(sp + 4 * (n - 2));
    for (int i = 0; i < nw; i++) U16(e + 4 + 2 * i) = U16(sp + 4 * (n - 3 - i));
    return e;
}

u32 DC2CALL ScrOp_AotDoor(gptr t)                                  /* 30: kind 0, door */
{
    gptr e = aot_head(t, 18, 0, 9, 14);
    U8(e + 0x20) = U8(SP_(t) + 4);
    U8(e + 0x21) = U8(SP_(t));
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Aot31(gptr t)                                    /* 31: kind 1 */
{
    gptr e = aot_head(t, 12, 1, 1, 8);
    U8(e + 0x14) = U8(SP_(t) + 4);
    U32(e + 0x18) = S(t, 0);
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Aot32(gptr t)                                    /* 32: kind 2 */
{
    gptr e = aot_head(t, 12, 2, 1, 8);
    U32(e + 0x14) = S(t, 1);
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Aot33(gptr t)                                    /* 33: kind 3 */
{
    gptr e = aot_head(t, 13, 3, 1, 8);
    U8(e + 0x14) = U8(SP_(t) + 8);
    U8(e + 0x15) = U8(SP_(t) + 4);
    U8(e + 0x16) = U8(SP_(t));
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_AotItem(gptr t)                                  /* 35: kind 5, item pickup */
{
    gptr sp = SP_(t);
    if (Flag_Check(5, S(t, 5)) && S(t, 5)) { ADV(t); return 0; }     /* already taken */
    gptr e = aot_head(t, 16, 5, 1, 8);
    U16(e + 0x18) = U16(sp + 0x14);                                 /* item flag */
    U32(e + 0x14) = S(t, 4);
    U8(e + 0x1A) = U8(sp + 0xC);
    U8(e + 0x1B) = U8(sp + 4) + 12;                                 /* effect slot of the model */
    u32 slot = S(t, 1);
    if (slot != 0xFF) {                                             /* show the item model */
        gptr m = U32(G() + 0x93C) + (slot + 12) * 0x70;
        U32(m + 0x50) = 7;
        U8(m + 0x4E) = 1;
        U16(m + 0x6C) = 0;
        U8(m + 0x46) = U8(sp + 8);
        U16(m + 0x6E) = 10;
        U32(m + 0x68) = S(t, 0);
        U16(m + 0x40) = (u16)((s32)(U32(sp + 0x34) + U32(sp + 0x2C) + U32(sp + 0x24) + U32(sp + 0x1C)) / 4);
        U16(m + 0x42) = (u16)(0u - 100u - (u32)U16(sp + 0x38) * 1000u);
        U16(m + 0x44) = (u16)((s32)(U32(sp + 0x30) + U32(sp + 0x28) + U32(sp + 0x20) + U32(sp + 0x18)) / 4);
    }
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Aot36(gptr t)                                    /* 36: kind 6 */
{
    gptr e = aot_head(t, 11, 6, 1, 8);
    U32(e + 0x14) = S(t, 0) + 12;
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Aot37(gptr t)                                    /* 37: kind 7 */
{
    aot_head(t, 10, 7, 1, 8);
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Aot38(gptr t)                                    /* 38: kind 8 */
{
    aot_head(t, 10, 8, 1, 8);
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Aot39(gptr t)                                    /* 39: kind 9 */
{
    gptr e = aot_head(t, 13, 9, 1, 8);
    U8(e + 0x19) = U8(SP_(t) + 8);
    U8(e + 0x18) = U8(SP_(t) + 4);
    U32(e + 0x14) = S(t, 0);
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Aot3A(gptr t)                                    /* 3A: kind 10, unless flag(8, n) */
{
    gptr sp = SP_(t);
    if (!Flag_Check(8, S(t, 2))) {
        gptr e = aot_head(t, 14, 10, 1, 8);
        U8(e + 0x14) = U8(sp + 0xC);
        U8(e + 0x15) = U8(sp + 8);
        U32(e + 0x18) = S(t, 1);
        U8(e + 0x16) = 3;
        U8(e + 0x17) = U8(sp);
    } else if (S(t, 3) == 1) {                                      /* already done: reset k2 entry */
        gptr k = U32(G() + 0x944) + S(t, 1) * 0xAC;
        U8(k + 0x64) = 0; U8(k + 0x65) = 0xFF; U8(k + 0x66) = 0;
    }
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_Aot3B(gptr t)                                    /* 3B: kind 11 */
{
    gptr e = aot_head(t, 11, 11, 1, 8);
    U8(e + 0x14) = U8(SP_(t));
    ADV(t);
    return 0;
}

u32 DC2CALL ScrOp_AotRun(gptr t)                                   /* 3E: run entry; 1 = item */
{
    gptr e = U32(G() + 0xA5C) + S(t, 0) * 0x28;
    Orig_Call1(U32(A_AOT_KIND_TABLE + U8(e + 1) * 4), e);
    ADV(t);
    return U8(e + 1) == 5;
}
