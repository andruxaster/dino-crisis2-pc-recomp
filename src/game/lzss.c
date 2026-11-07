/* LZSS decoder used by the file loader for record types 5/6/8 (0x4041F0).
 * flag byte read LSB first, bit 1 = literal, bit 0 = 2-byte match:
 * offset = b0 | (b1 & 15) << 8 (back from the output position),
 * length = (b1 >> 4) + 2. srclen counts input bytes, loop while > 0. */
#include "dc2/game.h"

void DC2CALL Lzss_Decode(gptr dst, gptr src, s32 srclen)
{
    u8 *d = MEM(dst);
    const u8 *s = MEM(src);
    u32 flags = 0;
    int count = 0;
    do {
        if (count == 0) {
            flags = *s++;
            count = 8;
            srclen--;
        }
        count--;
        srclen--;
        if (flags & 1) {
            *d++ = *s++;
        } else {
            srclen--;
            u32 lo = *s++, hi = *s++;
            u32 n = ((hi >> 4) & 15) + 2, off = (hi & 15) << 8 | lo;
            const u8 *p = d - off;
            while (n--) *d++ = *p++;        /* byte copy, may overlap */
        }
        flags >>= 1;
    } while (srclen > 0);
}
