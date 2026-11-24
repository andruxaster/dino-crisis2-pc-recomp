/* Files streamed as they play (the music, the speech) with their read-ahead
 * in the AICA's sound RAM rather than in main RAM (aica_ra.h).
 *
 * The disc is still read in large pieces (one piece of `ring` bytes, ~6 s
 * of the music, as seldom as before: a real drive seeks for each read of
 * another file), but a piece passes through main RAM 32 KB at a time and
 * is kept in sound RAM, where the stream's reads fetch it as they go (the
 * music needs ~22 KB a second, nothing for the G2 bus). Main RAM holds the
 * 32 KB bounce, shared by every such file, instead of 128 KB per file.
 * When sound RAM has no room the file gets a 32 KB stdio buffer in main
 * RAM instead. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <kos.h>
#include <kos/fs.h>
#include <kos/mutex.h>
#include <dc/spu.h>
#include <dc/sound/sound.h>
#include "aica_ra.h"

#define BOUNCE (32 << 10)

typedef struct {
    file_t fd;
    u32 aica, ring;                       /* sound RAM: the piece read ahead, its size */
    u32 at, have;                         /* the piece's place in the file, the bytes in it */
    u32 pos, size;                        /* the reader's place, the file's size */
} Ra;

static u8 *g_bounce;                      /* (32-byte aligned; shared, under g_lock) */
static mutex_t g_lock = MUTEX_INITIALIZER;
static u32 g_open, g_pieces;              /* (the log) */

/* the piece from the file at `from` into sound RAM; 0 at a read error */
static int refill(Ra *r, u32 from)
{
    r->at = from; r->have = 0;
    if (fs_seek(r->fd, (off_t)from, SEEK_SET) < 0) return 0;
    u32 want = r->size - from < r->ring ? r->size - from : r->ring;
    mutex_lock(&g_lock);
    while (r->have < want) {
        u32 n = want - r->have < BOUNCE ? want - r->have : BOUNCE;
        ssize_t got = fs_read(r->fd, g_bounce, n);
        if (got <= 0) break;
        spu_memload(r->aica + r->have, g_bounce, ((u32)got + 3) & ~3u);
        r->have += (u32)got;
        if ((u32)got < n) break;
    }
    mutex_unlock(&g_lock);
    g_pieces++;
    return r->have > 0;
}

static int ra_read(void *c, char *buf, int n)
{
    Ra *r = c;
    int done = 0;
    while (done < n && r->pos < r->size) {
        if (r->pos < r->at || r->pos >= r->at + r->have)
            if (!refill(r, r->pos)) break;
        u32 off = r->pos - r->at, take = r->have - off;
        if (take > (u32)(n - done)) take = (u32)(n - done);
        /* (sound RAM read 4 bytes at a time: the piece's 4-aligned span through a small buffer) */
        u32 tmp32[65];
        u8 *tmp = (u8 *)tmp32;
        while (take) {
            u32 a0 = off & ~3u, lead = off - a0, k = take < 256 - lead ? take : 256 - lead;
            spu_memread(tmp, r->aica + a0, (lead + k + 3) & ~3u);
            memcpy(buf + done, tmp + lead, k);
            done += (int)k; off += k; take -= k; r->pos += k;
        }
    }
    return done;
}

static fpos_t ra_seek(void *c, fpos_t off, int whence)
{
    Ra *r = c;
    long p = whence == SEEK_SET ? (long)off : whence == SEEK_CUR ? (long)r->pos + (long)off : (long)r->size + (long)off;
    if (p < 0) return -1;
    r->pos = (u32)p > r->size ? r->size : (u32)p;
    return (fpos_t)r->pos;
}

static int ra_close(void *c)
{
    Ra *r = c;
    fs_close(r->fd);
    snd_mem_free(r->aica);
    free(r);
    g_open--;
    return 0;
}

void *AicaRa_Open(const char *path, size_t ring)
{
    if (!g_bounce && !(g_bounce = memalign(32, BOUNCE))) return NULL;
    ring = (ring + 31) & ~(size_t)31;
    u32 aica = snd_mem_malloc(ring);
    if (!aica) return NULL;                       /* (no room in sound RAM: the caller's plain buffer) */
    file_t fd = fs_open(path, O_RDONLY);
    if (fd < 0) { snd_mem_free(aica); return NULL; }
    Ra *r = calloc(1, sizeof *r);
    FILE *f = r ? funopen(r, ra_read, NULL, ra_seek, ra_close) : NULL;
    if (!f) { free(r); fs_close(fd); snd_mem_free(aica); return NULL; }
    r->fd = fd; r->aica = aica; r->ring = (u32)ring; r->size = (u32)fs_total(fd);
    g_open++;
    return f;
}

u32 AicaRa_Open_Count(void) { return g_open; }
u32 AicaRa_Pieces(void) { return g_pieces; }
