/* A background of the BGVQ files (bgvq_format.h) back as the picture the
 * game's JPEG decoder makes (320 x 240, the PlayStation's 15-bit colour:
 * red in the low bits): the Dreamcast's loader puts it in the game's memory
 * for whatever reads the picture there, and the tool checks its files with
 * it. stp: the pictures' bit 15 (the file's flag). */
#ifndef DC2_BGVQ_DECODE_H
#define DC2_BGVQ_DECODE_H
#include "bgvq_format.h"

static inline unsigned bgvq_spread7(unsigned i)
{
    unsigned r = 0;
    for (int b = 0; b < 7; b++) r |= ((i >> b) & 1u) << (2 * b);
    return r;
}

/* blob: BGVQ_BLOB bytes; out: 320 x 240 pixels */
static inline void bgvq_decode(const unsigned char *blob, unsigned short *out, int stp)
{
    unsigned sp[128];
    for (unsigned i = 0; i < 128; i++) sp[i] = bgvq_spread7(i);
    for (int h = 0; h < 2; h++) {
        const unsigned char *book = blob + h * BGVQ_PIECE, *idx = book + BGVQ_CODEBOOK;
        unsigned short code[256 * 4];                    /* the codes as 15-bit pixels */
        for (int k = 0; k < 256 * 4; k++) {
            unsigned c = (unsigned)book[2 * k] | (unsigned)book[2 * k + 1] << 8;
            code[k] = (unsigned short)((c >> 11) | (c >> 6 & 31) << 5 | (c & 31) << 10 | (stp ? 0x8000 : 0));
        }
        int bw = h ? 32 : 128;                          /* (blocks across: 256 or 64 columns) */
        for (int by = 0; by < 120; by++) {
            unsigned short *r0 = out + 2 * by * 320 + 256 * h, *r1 = r0 + 320;
            for (int bx = 0; bx < bw; bx++) {
                const unsigned short *c = code + 4 * idx[sp[by] | sp[bx] << 1];
                r0[2 * bx] = c[0]; r1[2 * bx] = c[1]; r0[2 * bx + 1] = c[2]; r1[2 * bx + 1] = c[3];
            }
        }
    }
}
#endif
