/* Game flags: 12 groups of bit arrays, table at 0x723C40 {u32 *bits, u32 count}. */
#include "dc2/game.h"

static gptr flag_word(u32 group, u32 bit)
{
    return U32(A_FLAG_GROUPS + group * 8) + (bit >> 5) * 4;
}

void DC2CALL Flag_Set(u32 group, u32 bit, u32 on)          /* 0x46FAA0 */
{
    gptr w = flag_word(group, bit);
    if (!on) {
        U32(w) &= ~(1u << (bit & 31));
        return;
    }
    U32(w) |= 1u << (bit & 31);
    if (group == 1 && bit == 0) {
        U16(A_FLAG_CLR0) = 0; U16(A_FLAG_CLR1) = 0;
        U16(A_FLAG_CLR2) = 0; U16(A_FLAG_CLR3) = 0;
    }
}

u32 DC2CALL Flag_Check(u32 group, u32 bit)                 /* 0x46FB20 */
{
    return (U32(flag_word(group, bit)) & 1u << (bit & 31)) != 0;
}

void DC2CALL Bits_Set(gptr bits, u32 bit, u32 on)          /* 0x46FB50 */
{
    gptr w = bits + (bit >> 5) * 4;
    if (!on) U32(w) &= ~(1u << (bit & 31));
    else U32(w) |= 1u << (bit & 31);
}

u32 DC2CALL Bits_Check(gptr bits, u32 bit)                 /* 0x46FB90 */
{
    return (U32(bits + (bit >> 5) * 4) & 1u << (bit & 31)) != 0;
}
