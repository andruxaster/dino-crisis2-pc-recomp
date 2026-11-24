/* The camera backgrounds made ahead of time for the Dreamcast
 * (tools/dreamcast/dc2_bgvq.c writes them, src/port/dc/bgvq.c reads them):
 * one file per room DBS file, Data\BGVQ\<name>.VQ, holding for each camera
 * record of the DBS (by its sector) the 320 x 240 picture the game's JPEG
 * decoder makes of it, as two 256 x 256 VQ textures of the PVR (columns 0-255
 * and 256-319; the edges repeated past the picture): no JPEG to decode and
 * nothing to twiddle on the console, 36 KB of VRAM instead of 256.
 *
 *   "DC2BGVQ1"  u32 count  u32 flags (bit 0: the pictures' bit 15 set)
 *   count x { u32 sector, u32 offset }  (by sector)
 *   at each offset: two pieces of BGVQ_PIECE bytes: the codebook (256
 *   entries of 4 RGB565 texels: top left, bottom left, top right, bottom
 *   right) and 128 x 128 index bytes in the twiddled order
 * (all little-endian) */
#ifndef DC2_BGVQ_FORMAT_H
#define DC2_BGVQ_FORMAT_H
#define BGVQ_MAGIC "DC2BGVQ1"
#define BGVQ_CODEBOOK 2048
#define BGVQ_PIECE (BGVQ_CODEBOOK + 128 * 128)
#define BGVQ_BLOB (2 * BGVQ_PIECE)
#endif
