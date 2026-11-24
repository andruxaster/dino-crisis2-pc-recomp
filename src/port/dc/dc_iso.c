/* /cd read by the port itself (after the Resident Evil port's dc_iso.cpp,
 * andruxaster/resident-evil-dreamcast-port), in place of KallistiOS's
 * fs_iso9660.
 *
 * Why: fs_iso9660 looks a name up on every open by scanning the directory's
 * sectors through a 16-block cache. The game's Data folder has some 500
 * files, ~20 sectors of directory, so every open misses that cache and the
 * drive reads the whole directory again: on a real drive (a CD-R) a seek
 * and many commands for each room file, camera, sound bank or speech line.
 * GDEMU answers from its SD card at once, which hid it.
 *
 * Here each directory is read once (one command for all its sectors) into
 * a table of name -> (sector, size) sorted for a binary search; a file's
 * data is read with one CD stream per read (one seek, then sectors by DMA),
 * whole sectors only, into 32-byte aligned buffers; small reads go through
 * a 16 KB cache per open file. A failed stream falls back to sector
 * commands. Sector numbers as KallistiOS has them (LBA + 150).
 *
 * dc_iso_progress counts the reads done, so the watchdog (prof.c) does not
 * take a long load from a slow drive for a hang. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/stat.h>
#include <kos/fs.h>
#include <kos/nmmgr.h>
#include <kos/mutex.h>
#include <dc/cdrom.h>
#include <dc/fs_iso9660.h>
#include "dc_iso.h"

volatile unsigned dc_iso_progress;

typedef struct { unsigned lba, size; unsigned char dir; char name[64], key[64]; } Ent;
typedef struct { unsigned lba; Ent *e; int n; } Dir;

#define NDIRS 32
#define BOUNCE_SECTORS 32                 /* 64 KB */
#define FILE_CACHE 16384                  /* 8 sectors read ahead for small reads */

static mutex_t g_lock = RECURSIVE_MUTEX_INITIALIZER;
static int g_tried, g_ok, g_joliet, g_gd, g_stream_broken;
static unsigned g_root_lba, g_root_size;
static Dir g_dirs[NDIRS];
static int g_ndirs;
static unsigned char *g_bounce;
static unsigned g_reads, g_dir_loads;

static unsigned le32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24; }

static int read_sectors(unsigned lba, unsigned n, void *dst)
{
    int ok = cdrom_read_sectors_ex(dst, lba + 150, n, true) == ERR_OK;
    dc_iso_progress++;
    return ok;
}

static int init(void)
{
    if (g_tried) return g_ok;
    g_tried = 1;
    if (!(g_bounce = memalign(32, BOUNCE_SECTORS * 2048))) return 0;
    /* a GD-ROM: the high-density area's table (the data track 3); a CD-R:
       the disc's one */
    int status = 0, type = 0;
    cdrom_get_status(&status, &type);
    g_gd = type == CD_GDROM;
    cd_toc_t toc;
    if (cdrom_read_toc(&toc, g_gd) != ERR_OK) { printf("disc: no table of contents\n"); return 0; }
    unsigned base = cdrom_locate_data_track(&toc);
    if (!base) { printf("disc: no data track\n"); return 0; }
    unsigned vd = base - 150 + 16;
    for (unsigned i = 1; i <= 3 && !g_ok; i++) {      /* Joliet first (the long names, as KallistiOS) */
        if (!read_sectors(vd + i, 1, g_bounce)) break;
        if (!memcmp(g_bounce, "\x02" "CD001", 6) && g_bounce[88] == '%' && g_bounce[89] == '/' &&
            (g_bounce[90] == '@' || g_bounce[90] == 'C' || g_bounce[90] == 'E')) {
            g_root_lba = le32(g_bounce + 158); g_root_size = le32(g_bounce + 166);
            g_joliet = g_ok = 1;
        }
    }
    if (!g_ok) {
        if (!read_sectors(vd, 1, g_bounce) || memcmp(g_bounce, "\x01" "CD001", 6)) { printf("disc: not ISO 9660\n"); return 0; }
        g_root_lba = le32(g_bounce + 158); g_root_size = le32(g_bounce + 166);
        g_ok = 1;
    }
    printf("disc: read directly (%s, %s), the root at sector %u\n", g_gd ? "GD-ROM" : "CD", g_joliet ? "Joliet" : "ISO 9660", g_root_lba);
    return 1;
}

/* a record's name as KallistiOS compares it: Joliet's UCS-2 to ASCII, ";1"
   and a last '.' dropped; key: the same in lower case */
static void record_name(const unsigned char *s, int len, Ent *e)
{
    int k = 0;
    if (g_joliet) for (int i = 0; i + 1 < len && k < 63; i += 2) { unsigned c = s[i] << 8 | s[i + 1]; e->name[k++] = c < 128 ? (char)c : '?'; }
    else for (int i = 0; i < len && k < 63; i++) e->name[k++] = (char)s[i];
    e->name[k] = 0;
    char *semi = strchr(e->name, ';');
    if (semi) *semi = 0;
    k = (int)strlen(e->name);
    if (k && e->name[k - 1] == '.') e->name[--k] = 0;
    for (int i = 0; i <= k; i++) e->key[i] = (char)(e->name[i] >= 'A' && e->name[i] <= 'Z' ? e->name[i] + 32 : e->name[i]);
}

static int cmp_ent(const void *a, const void *b) { return strcmp(((const Ent *)a)->key, ((const Ent *)b)->key); }

static Dir *load_dir(unsigned lba, unsigned size)
{
    for (int i = 0; i < g_ndirs; i++) if (g_dirs[i].lba == lba) return &g_dirs[i];
    unsigned secs = (size + 2047) / 2048;
    unsigned char *raw = memalign(32, secs * 2048);
    if (!raw) return NULL;
    if (!read_sectors(lba, secs, raw)) { free(raw); return NULL; }
    int cap = 64, n = 0;
    Ent *e = malloc(sizeof(Ent) * cap);
    for (unsigned s = 0; s < secs && e; s++) {
        const unsigned char *sec = raw + s * 2048;
        for (unsigned off = 0; off < 2048;) {
            const unsigned char *r = sec + off;
            unsigned len = r[0];
            if (!len || off + len > 2048 || len < 34) break;   /* (the rest of the sector is padding) */
            int nlen = r[32];
            if (!(nlen == 1 && (r[33] == 0 || r[33] == 1))) {   /* (not "." or "..") */
                if (n == cap) { cap *= 2; Ent *m = realloc(e, sizeof(Ent) * cap); if (!m) break; e = m; }
                e[n].lba = le32(r + 2); e[n].size = le32(r + 10); e[n].dir = (r[25] & 2) != 0;
                record_name(r + 33, nlen, &e[n]);
                n++;
            }
            off += len;
        }
    }
    free(raw);
    if (!e) return NULL;
    qsort(e, n, sizeof(Ent), cmp_ent);
    Dir *d;
    if (g_ndirs < NDIRS) d = &g_dirs[g_ndirs++];
    else { d = &g_dirs[NDIRS - 1]; free(d->e); }          /* (more folders than kept: the last slot reused) */
    d->lba = lba; d->e = e; d->n = n;
    g_dir_loads++;
    return d;
}

/* a path from the disc's root ("data/ST100.DBS", either slash, any case) */
static int lookup(const char *path, Ent *out)
{
    if (!init()) return 0;
    Ent cur = {g_root_lba, g_root_size, 1, "", ""};
    const char *p = path;
    while (*p) {
        while (*p == '/' || *p == '\\') p++;
        if (!*p) break;
        char comp[64];
        int k = 0;
        for (; *p && *p != '/' && *p != '\\'; p++) if (k < 63) comp[k++] = (char)(*p >= 'A' && *p <= 'Z' ? *p + 32 : *p);
        comp[k] = 0;
        if (!strcmp(comp, ".")) continue;
        if (!cur.dir) return 0;
        Dir *d = load_dir(cur.lba, cur.size);
        if (!d) return 0;
        Ent key;
        strcpy(key.key, comp);
        Ent *hit = bsearch(&key, d->e, d->n, sizeof(Ent), cmp_ent);
        if (!hit) return 0;
        cur = *hit;
    }
    *out = cur;
    return 1;
}

/* [off, off + len) of the file at lba by sector commands (the fallback) */
static int read_range_sectors(unsigned lba, unsigned off, unsigned len, unsigned char *dst)
{
    while (len) {
        unsigned sec = off / 2048, in = off % 2048;
        if (!in && len >= 2048 && !((unsigned)dst & 31)) {
            unsigned n = len / 2048;
            if (n > 256) n = 256;
            if (!read_sectors(lba + sec, n, dst)) return 0;
            dst += n * 2048; off += n * 2048; len -= n * 2048;
            continue;
        }
        unsigned n = (in + len + 2047) / 2048;
        if (n > BOUNCE_SECTORS) n = BOUNCE_SECTORS;
        if (!read_sectors(lba + sec, n, g_bounce)) return 0;
        unsigned take = n * 2048 - in;
        if (take > len) take = len;
        memcpy(dst, g_bounce + in, take);
        dst += take; off += take; len -= take;
    }
    return 1;
}

/* the same through one CD stream: one seek, then the sectors by DMA, whole
   sectors only (a request under 32 bytes hangs the stream) */
static int read_range_stream(unsigned lba, unsigned off, unsigned len, unsigned char *dst)
{
    unsigned first = off / 2048, last = (off + len - 1) / 2048, sec = first;
    if (cdrom_stream_start((int)(lba + first + 150), (int)(last - first + 1), true) != ERR_OK) return 0;
    int ok = 1;
    while (ok && len) {
        unsigned in = off % 2048;
        if (!in && len >= 2048 && !((unsigned)dst & 31)) {
            unsigned n = len / 2048;
            if (n > 512) n = 512;
            ok = cdrom_stream_request(dst, n * 2048, true) == ERR_OK;
            dc_iso_progress++;
            dst += n * 2048; off += n * 2048; len -= n * 2048; sec += n;
            continue;
        }
        unsigned n = (in + len + 2047) / 2048;
        if (n > BOUNCE_SECTORS) n = BOUNCE_SECTORS;
        if (n > last - sec + 1) n = last - sec + 1;
        ok = cdrom_stream_request(g_bounce, n * 2048, true) == ERR_OK;
        dc_iso_progress++;
        unsigned take = n * 2048 - in;
        if (take > len) take = len;
        if (ok) memcpy(dst, g_bounce + in, take);
        dst += take; off += take; len -= take; sec += n;
    }
    cdrom_stream_stop(false);
    return ok;
}

static int read_range(unsigned lba, unsigned off, unsigned len, unsigned char *dst)
{
    if (!len) return 1;
    g_reads++;
    if (!g_stream_broken) {
        if (read_range_stream(lba, off, len, dst)) return 1;
        printf("disc: a stream read failed, sector reads from now on\n");
        g_stream_broken = 1;
    }
    return read_range_sectors(lba, off, len, dst);
}

/* ---- /cd ---- */

typedef struct {
    Ent e;
    unsigned pos;
    unsigned char *cache;                 /* small reads (stdio's buffer): FILE_CACHE, aligned */
    unsigned cstart, clen;
    Dir *d;                               /* a directory: its entries, the next one */
    int next;
    dirent_t de;
} CdFile;

static void *vfs_open(vfs_handler_t *vfs, const char *fn, int mode)
{
    (void)vfs;
    mutex_lock(&g_lock);
    Ent e;
    CdFile *f = NULL;
    if ((mode & O_MODE_MASK) != O_RDONLY) errno = EROFS;
    else if (!lookup(fn ? fn : "", &e)) errno = ENOENT;
    else if ((mode & O_DIR) && !e.dir) errno = ENOTDIR;
    else if (!(mode & O_DIR) && e.dir) errno = EISDIR;
    else if (!(f = calloc(1, sizeof *f))) errno = ENOMEM;
    else {
        f->e = e;
        if (e.dir && !(f->d = load_dir(e.lba, e.size))) { free(f); f = NULL; errno = EIO; }
    }
    mutex_unlock(&g_lock);
    return f;
}

static int vfs_close(void *h)
{
    CdFile *f = h;
    if (!f) return -1;
    free(f->cache);
    free(f);
    return 0;
}

static ssize_t vfs_read(void *h, void *buf, size_t cnt)
{
    CdFile *f = h;
    if (!f || f->e.dir) { errno = EBADF; return -1; }
    if (f->pos >= f->e.size || !cnt) return 0;
    unsigned n = f->e.size - f->pos;
    if (cnt < n) n = (unsigned)cnt;
    unsigned char *out = buf;
    mutex_lock(&g_lock);
    ssize_t r = (ssize_t)n;
    if (n >= FILE_CACHE) {                                /* a large read: straight in */
        if (read_range(f->e.lba, f->pos, n, out)) f->pos += n;
        else { errno = EIO; r = -1; }
        mutex_unlock(&g_lock);
        return r;
    }
    for (unsigned done = 0; done < n;) {
        if (f->pos < f->cstart || f->pos >= f->cstart + f->clen) {
            if (!f->cache && !(f->cache = memalign(32, FILE_CACHE))) { errno = ENOMEM; r = -1; break; }
            unsigned start = f->pos & ~2047u, len = f->e.size - start;
            if (len > FILE_CACHE) len = FILE_CACHE;
            if (!read_range(f->e.lba, start, len, f->cache)) { errno = EIO; r = -1; break; }
            f->cstart = start; f->clen = len;
        }
        unsigned take = f->cstart + f->clen - f->pos;
        if (take > n - done) take = n - done;
        memcpy(out + done, f->cache + (f->pos - f->cstart), take);
        f->pos += take; done += take;
    }
    mutex_unlock(&g_lock);
    return r;
}

static off_t vfs_seek(void *h, off_t off, int whence)
{
    CdFile *f = h;
    if (!f || f->e.dir) { errno = EBADF; return -1; }
    off_t base = whence == SEEK_SET ? 0 : whence == SEEK_CUR ? (off_t)f->pos : whence == SEEK_END ? (off_t)f->e.size : -1;
    if (base < 0 || base + off < 0) { errno = EINVAL; return -1; }
    off_t p = base + off;
    if (p > (off_t)f->e.size) p = (off_t)f->e.size;
    f->pos = (unsigned)p;
    return p;
}

static off_t vfs_tell(void *h) { CdFile *f = h; if (!f) { errno = EBADF; return -1; } return (off_t)f->pos; }
static size_t vfs_total(void *h) { CdFile *f = h; if (!f) { errno = EBADF; return (size_t)-1; } return f->e.size; }

static const dirent_t *vfs_readdir(void *h)
{
    CdFile *f = h;
    if (!f || !f->e.dir) { errno = EBADF; return NULL; }
    if (f->next >= f->d->n) return NULL;
    const Ent *e = &f->d->e[f->next++];
    memset(&f->de, 0, sizeof f->de);
    strncpy(f->de.name, e->name, sizeof f->de.name - 1);
    f->de.size = e->dir ? -1 : (int)e->size;
    f->de.attr = e->dir ? O_DIR : 0;
    return &f->de;
}

static int vfs_rewinddir(void *h) { CdFile *f = h; if (!f || !f->e.dir) { errno = EBADF; return -1; } f->next = 0; return 0; }

static void fill_stat(const Ent *e, struct stat *st)
{
    memset(st, 0, sizeof *st);
    st->st_dev = (dev_t)('c' | ('d' << 8));
    st->st_mode = S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH | (e->dir ? S_IFDIR : S_IFREG);
    st->st_size = e->dir ? -1 : (off_t)e->size;
    st->st_nlink = e->dir ? 2 : 1;
    st->st_blksize = 2048;
}

static int vfs_stat(vfs_handler_t *vfs, const char *path, struct stat *st, int flag)
{
    (void)vfs; (void)flag;
    mutex_lock(&g_lock);
    Ent e;
    int ok = lookup(path ? path : "", &e);
    mutex_unlock(&g_lock);
    if (!ok) { errno = ENOENT; return -1; }
    fill_stat(&e, st);
    return 0;
}

static int vfs_fstat(void *h, struct stat *st) { CdFile *f = h; if (!f) { errno = EBADF; return -1; } fill_stat(&f->e, st); return 0; }

static int vfs_fcntl(void *h, int cmd, va_list ap)
{
    (void)ap;
    CdFile *f = h;
    if (!f) { errno = EBADF; return -1; }
    switch (cmd) {
    case F_GETFL: return O_RDONLY | (f->e.dir ? O_DIR : 0);
    case F_SETFL: case F_GETFD: case F_SETFD: return 0;
    default: errno = EINVAL; return -1;
    }
}

static vfs_handler_t g_vh = {
    .nmmgr = {"/cd", 0, 0x00010000, 0, NMMGR_TYPE_VFS, NMMGR_LIST_INIT},
    .open = vfs_open, .close = vfs_close, .read = vfs_read, .seek = vfs_seek, .tell = vfs_tell, .total = vfs_total,
    .readdir = vfs_readdir, .stat = vfs_stat, .fcntl = vfs_fcntl, .rewinddir = vfs_rewinddir, .fstat = vfs_fstat,
};

int dc_iso_mount(void)
{
    mutex_lock(&g_lock);
    int r = -1;
    if (init()) {
        fs_iso9660_shutdown();
        if (nmmgr_handler_add(&g_vh.nmmgr) == 0) r = 0;
        else { printf("disc: /cd could not be taken over: KallistiOS's ISO 9660 kept\n"); fs_iso9660_init(); }
    }
    mutex_unlock(&g_lock);
    return r;
}

void dc_iso_report(void)
{
    if (g_ok) printf("disc: %u reads, %u folders read\n", g_reads, g_dir_loads);
}
