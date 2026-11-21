/* The movies as Cinepak AVI (the Dreamcast's: tools/dreamcast/dc2_movies.sh
 * makes MOVIE\NAME.AVI from MOVIE\NAME.DAT): a sequential AVI reader (the
 * 'movi' list read in order, no index needed), a Cinepak decoder into
 * 16-bit ARGB1555 pixels, and the soundtrack as IMA ADPCM (or 16-bit PCM).
 * Built for the Dreamcast only (DC2_MOVIE_AVI): src/pc/movie.c plays its
 * movies this way; the PC builds play the original MPEG-1.
 *
 * Cinepak: a frame is strips of 4x4 blocks, each block one V1 vector (one
 * codebook entry stretched to 4x4) or four V4 vectors (an entry per 2x2);
 * an entry is four Y and one U, V (R = Y + 2V, G = Y - U/2 - V, B = Y + 2U).
 * Each strip keeps a V1 and a V4 codebook of 256 entries across frames; a
 * strip starts from the one above it unless the frame's flags bit 0 is set.
 * Entries are kept as four ready pixels, so decoding is copying. */
#if defined(DC2_MOVIE_AVI) && !defined(DC2_IN_PROCESS)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dc2/types.h"
#include "dc2/pcio.h"
#include "movie_avi.h"

#define MAXSTRIPS 8                      /* (dc2_movies.sh encodes 4) */
typedef struct { u16 v1[256][4], v4[256][4]; } Books;

static struct {
    FILE *f;
    u32 movi_end;                        /* file offset where 'movi' ends */
    u32 w, h, us_per_frame, total;      /* total: frames (avih), 0 unknown */
    Books book[MAXSTRIPS];
    u16 *pix;                            /* w x h, the last frame decoded */
    u8 *chunk; u32 chunk_cap;
    /* sound */
    u32 a_fmt, a_ch, a_rate, a_align, a_spb;
    u8 *aq; u32 aq_len, aq_pos, aq_cap;  /* compressed sound read ahead of the video */
    s16 *apcm; u32 apcm_n, apcm_cap;
    xreal apos;                          /* (float on the SH-4) */
    int ended;
} A;

static u32 rd32(const u8 *p) { return (u32)p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24; }
static u32 rd16(const u8 *p) { return (u32)p[0] | p[1] << 8; }
static u32 be16(const u8 *p) { return (u32)p[0] << 8 | p[1]; }
static u32 be24(const u8 *p) { return (u32)p[0] << 16 | p[1] << 8 | p[2]; }
static u32 be32(const u8 *p) { return (u32)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

static u8 clamp8(s32 v) { return (u8)(v < 0 ? 0 : v > 255 ? 255 : v); }
static u16 px1555(s32 y, s32 u, s32 v)
{
    u32 r = clamp8(y + 2 * v), g = clamp8(y - (u >> 1) - v), b = clamp8(y + 2 * u);
    return (u16)(0x8000 | (r >> 3) << 10 | (g >> 3) << 5 | b >> 3);
}

/* a codebook chunk: full (every entry in turn) or partial (32-bit masks, MSB
   first, say which entries follow); 6 bytes an entry, 4 when grey */
static void load_book(u16 (*bk)[4], const u8 *p, const u8 *end, int partial, int grey)
{
    u32 n = grey ? 4 : 6, mask = 0, bits = 0;
    for (u32 i = 0; i < 256; i++) {
        if (partial) {
            if (!bits) { if (p + 4 > end) return; mask = be32(p); p += 4; bits = 32; }
            bits--;
            if (!(mask & (1u << bits))) continue;
        }
        if (p + n > end) return;
        s32 u = grey ? 0 : (s8)p[4], v = grey ? 0 : (s8)p[5];
        for (int k = 0; k < 4; k++) bk[i][k] = px1555(p[k], u, v);
        p += n;
    }
}

static void put_v1(u32 x, u32 y, const u16 *e)
{
    if (x + 4 > A.w || y + 4 > A.h) return;
    u16 *d = A.pix + y * A.w + x;
    for (int r = 0; r < 4; r++, d += A.w) {
        const u16 *s = e + (r >> 1) * 2;
        d[0] = d[1] = s[0]; d[2] = d[3] = s[1];
    }
}
static void put_v4(u32 x, u32 y, const u16 *a, const u16 *b, const u16 *c, const u16 *e)
{
    if (x + 4 > A.w || y + 4 > A.h) return;
    u16 *d = A.pix + y * A.w + x;
    d[0] = a[0]; d[1] = a[1]; d[2] = b[0]; d[3] = b[1]; d += A.w;
    d[0] = a[2]; d[1] = a[3]; d[2] = b[2]; d[3] = b[3]; d += A.w;
    d[0] = c[0]; d[1] = c[1]; d[2] = e[0]; d[3] = e[1]; d += A.w;
    d[0] = c[2]; d[1] = c[3]; d[2] = e[2]; d[3] = e[3];
}

/* the blocks of a strip [y0, y1): 0x30 every block coded (a flag per block:
   V4 or V1), 0x31 also skipped blocks, 0x32 V1 only */
static void blocks(Books *bk, const u8 *p, const u8 *end, u32 y0, u32 y1, int kind)
{
    u32 mask = 0, bits = 0;
#define FLAG(out) do { if (!bits) { if (p + 4 > end) return; mask = be32(p); p += 4; bits = 32; } bits--; out = (mask >> bits) & 1; } while (0)
    for (u32 y = y0; y + 4 <= y1; y += 4)
        for (u32 x = 0; x + 4 <= A.w; x += 4) {
            u32 f;
            if (kind == 1) { FLAG(f); if (!f) continue; }                  /* skipped: unchanged */
            if (kind == 2) f = 0; else FLAG(f);
            if (f) {
                if (p + 4 > end) return;
                put_v4(x, y, bk->v4[p[0]], bk->v4[p[1]], bk->v4[p[2]], bk->v4[p[3]]);
                p += 4;
            } else {
                if (p >= end) return;
                put_v1(x, y, bk->v1[*p++]);
            }
        }
#undef FLAG
}

static void cinepak(const u8 *p, u32 n)
{
    if (n < 10) return;
    const u8 *end = p + n;
    u32 flags = p[0], strips = be16(p + 8);
    if (be24(p + 1) < n) end = p + be24(p + 1);
    p += 10;
    u32 y = 0;
    for (u32 s = 0; s < strips && s < MAXSTRIPS && p + 12 <= end; s++) {
        u32 size = be16(p + 2), top = be16(p + 4), bottom = be16(p + 8);
        if (size < 12 || p + size > end) break;
        Books *bk = &A.book[s];
        if (s && !(flags & 1)) *bk = A.book[s - 1];
        u32 y1 = y + (bottom - top);
        const u8 *c = p + 12, *se = p + size;
        while (c + 4 <= se) {
            u32 id = be16(c), cs = be16(c + 2);
            if (cs < 4 || c + cs > se) break;
            const u8 *d = c + 4, *de = c + cs;
            switch (id & 0xFF00) {
            case 0x2000: case 0x2100: case 0x2400: case 0x2500:
                load_book(bk->v4, d, de, id & 0x100, id & 0x400); break;
            case 0x2200: case 0x2300: case 0x2600: case 0x2700:
                load_book(bk->v1, d, de, id & 0x100, id & 0x400); break;
            case 0x3000: blocks(bk, d, de, y, y1 > A.h ? A.h : y1, 0); break;
            case 0x3100: blocks(bk, d, de, y, y1 > A.h ? A.h : y1, 1); break;
            case 0x3200: blocks(bk, d, de, y, y1 > A.h ? A.h : y1, 2); break;
            }
            c += cs;
        }
        y = y1;
        p += size;
    }
}

/* ---- the file ---- */

static int read_chunk(u8 tag[4], u32 *size)
{
    u8 h[8];
    if ((u32)ftell(A.f) + 8 > A.movi_end || fread(h, 1, 8, A.f) != 8) return 0;
    memcpy(tag, h, 4); *size = rd32(h + 4);
    return 1;
}

/* the next chunk of 'movi': video (1) into A.chunk, sound queued (2), 0 at the end */
static int next_chunk(u32 *n)
{
    for (;;) {
        u8 tag[4]; u32 size;
        if (!read_chunk(tag, &size)) return 0;
        if (!memcmp(tag, "LIST", 4)) { fseek(A.f, 4, SEEK_CUR); continue; }    /* ('rec ' lists: their chunks follow) */
        u32 pad = size & 1;
        if (tag[2] == 'd' && (tag[3] == 'c' || tag[3] == 'b')) {
            if (size > A.chunk_cap) { A.chunk_cap = size; A.chunk = realloc(A.chunk, size); }
            if (fread(A.chunk, 1, size, A.f) != size) return 0;
            if (pad) fseek(A.f, 1, SEEK_CUR);
            *n = size;
            return 1;
        }
        if (tag[2] == 'w' && tag[3] == 'b' && A.a_ch) {
            if (A.aq_pos > 0x8000) { memmove(A.aq, A.aq + A.aq_pos, A.aq_len - A.aq_pos); A.aq_len -= A.aq_pos; A.aq_pos = 0; }
            if (A.aq_len + size > A.aq_cap) { A.aq_cap = (A.aq_len + size) * 2; A.aq = realloc(A.aq, A.aq_cap); }
            if (fread(A.aq + A.aq_len, 1, size, A.f) != size) return 0;
            A.aq_len += size;
            if (pad) fseek(A.f, 1, SEEK_CUR);
            return 2;
        }
        fseek(A.f, (long)(size + pad), SEEK_CUR);
    }
}

void Avi_Close(void)
{
    if (A.f) fclose(A.f);
    free(A.pix); free(A.chunk); free(A.aq); free(A.apcm);
    memset(&A, 0, sizeof A);
}

int Avi_Open(const char *path)
{
    Avi_Close();
    if (!(A.f = fopen(path, "rb"))) return 0;
    u8 h[12];
    if (fread(h, 1, 12, A.f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "AVI ", 4)) { Avi_Close(); return 0; }
    u32 file_end = 8 + rd32(h + 4), kind = 0, vfourcc = 0;
    while ((u32)ftell(A.f) + 8 <= file_end) {
        u8 c[8];
        if (fread(c, 1, 8, A.f) != 8) break;
        u32 size = rd32(c + 4);
        if (!memcmp(c, "LIST", 4)) {
            u8 t[4];
            if (fread(t, 1, 4, A.f) != 4) break;
            if (!memcmp(t, "movi", 4)) { A.movi_end = (u32)ftell(A.f) - 4 + size; break; }
            continue;                                   /* hdrl, strl: go inside */
        }
        u8 *b = malloc(size ? size : 1);
        if (fread(b, 1, size, A.f) != size) { free(b); break; }
        if (size & 1) fseek(A.f, 1, SEEK_CUR);
        if (!memcmp(c, "avih", 4) && size >= 40) { A.us_per_frame = rd32(b); A.total = rd32(b + 16); }
        else if (!memcmp(c, "strh", 4) && size >= 8) { kind = rd32(b); if (kind == 0x73646976) vfourcc = rd32(b + 4); }  /* 'vids' */
        else if (!memcmp(c, "strf", 4)) {
            if (kind == 0x73646976 && size >= 16) { A.w = rd32(b + 4); A.h = rd32(b + 8); if (!vfourcc) vfourcc = rd32(b + 16); }
            else if (kind == 0x73647561 && size >= 16) {                                                         /* 'auds' */
                A.a_fmt = rd16(b); A.a_ch = rd16(b + 2); A.a_rate = rd32(b + 4); A.a_align = rd16(b + 12);
                if ((A.a_fmt != 0x11 && A.a_fmt != 1) || !A.a_ch || A.a_ch > 2) A.a_ch = 0;
                if (A.a_fmt == 0x11) A.a_spb = (A.a_align / (A.a_ch ? A.a_ch : 1) - 4) * 2 + 1;
            }
        }
        free(b);
    }
    (void)vfourcc;
    if (!A.movi_end || !A.w || !A.h || A.w > 640 || A.h > 480) { Avi_Close(); return 0; }
    if (!A.us_per_frame) A.us_per_frame = 33333;
    A.pix = calloc((size_t)A.w * A.h, 2);
    for (u32 i = 0; i < A.w * A.h; i++) A.pix[i] = 0x8000;
    return 1;
}

u32 Avi_Width(void) { return A.w; }
u32 Avi_Height(void) { return A.h; }
double Avi_FrameTime(void) { return A.us_per_frame / 1e6; }
u32 Avi_Frames(void) { return A.total; }
const u16 *Avi_Pixels(void) { return A.pix; }

int Avi_NextFrame(void)
{
    if (!A.f || A.ended) return 0;
    for (;;) {
        u32 n = 0;
        int k = next_chunk(&n);
        if (!k) { A.ended = 1; return 0; }
        if (k == 1) { cinepak(A.chunk, n); return 1; }
    }
}

/* the next sound block decoded into A.apcm; 0 when there is none */
static int sound_block(void)
{
    u32 need = A.a_fmt == 0x11 ? A.a_align : 4096 * A.a_ch;
    while (A.aq_len - A.aq_pos < need) {
        u32 n;
        int k = next_chunk(&n);
        if (!k) { if (A.aq_len == A.aq_pos) return 0; break; }
        if (k == 1) {                           /* a video chunk on the way: kept for Avi_NextFrame (sound read ahead) */
            fseek(A.f, -(long)(n + 8 + (n & 1)), SEEK_CUR);
            if (A.aq_len - A.aq_pos) break;
            return 0;
        }
    }
    u32 n = A.aq_len - A.aq_pos < need ? A.aq_len - A.aq_pos : need;
    u32 frames = A.a_fmt == 0x11 ? A.a_spb : n / (2 * A.a_ch);
    if (frames * A.a_ch > A.apcm_cap) { A.apcm_cap = frames * A.a_ch; A.apcm = realloc(A.apcm, A.apcm_cap * sizeof(s16)); }
    if (A.a_fmt == 0x11) A.apcm_n = PcAudio_ImaBlock(A.aq + A.aq_pos, n, A.a_ch, A.apcm, A.a_spb);
    else { memcpy(A.apcm, A.aq + A.aq_pos, frames * 2 * A.a_ch); A.apcm_n = frames; }
    A.aq_pos += n;
    return A.apcm_n != 0;
}

u32 Avi_Sound(float *lr, u32 frames, u32 rate)
{
    if (!A.f || !A.a_ch) return 0;
    xreal step = (xreal)A.a_rate / (xreal)rate;
    for (u32 i = 0; i < frames; i++) {
        while (A.apos >= A.apcm_n) {
            A.apos -= A.apcm_n; A.apcm_n = 0;
            if (!sound_block()) return i;
        }
        u32 k = (u32)A.apos;
        float l = A.apcm[A.a_ch == 2 ? 2 * k : k], r = A.apcm[A.a_ch == 2 ? 2 * k + 1 : k];
        lr[2 * i] += l; lr[2 * i + 1] += r;
        A.apos += step;
    }
    return frames;
}
#endif
