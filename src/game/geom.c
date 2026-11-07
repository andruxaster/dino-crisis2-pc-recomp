/* Small geometry helpers. */
#include "dc2/game.h"

/* 0x48F790: is pos (SVECTOR: x at +0, z at +4) inside quad s16 {x,z}[4]?
 * Four edge-side tests in the order and with the operands of the original
 * (32-bit products of 16-bit differences). */
u32 DC2CALL PointInQuad(gptr pos, gptr quad)
{
    s32 px = S16(pos), pz = S16(pos + 4);
    s32 x0 = S16(quad), z0 = S16(quad + 2), x1 = S16(quad + 4), z1 = S16(quad + 6);
    s32 x2 = S16(quad + 8), z2 = S16(quad + 10), x3 = S16(quad + 12), z3 = S16(quad + 14);
    if ((x1 - x0) * (pz - z0) > (z1 - z0) * (px - x0)) return 0;
    if ((x3 - x0) * (pz - z0) < (z3 - z0) * (px - x0)) return 0;
    if ((x1 - x2) * (pz - z2) < (z1 - z2) * (px - x2)) return 0;
    if ((x3 - x2) * (pz - z2) > (z3 - z2) * (px - x2)) return 0;
    return 1;
}

/* 0x472210: global room index = first index of the area + room (low byte
 * is what callers use) */
u32 DC2CALL Room_GlobalIndex(u32 area, u32 room)
{
    return (u8)(U8(A_ROOM_BASE_IDX + (area & 0xFF)) + (u8)room);
}
