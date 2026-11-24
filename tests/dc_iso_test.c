/* The Dreamcast's own /cd reader (src/port/dc/dc_iso.c) on the host, with
 * stand-ins for KallistiOS (tests/dc_iso_fake) reading a Joliet image made by
 * tests/dc_iso_mkiso.py: every file found by path (either slash, any case),
 * the folder read once, the bytes right through small, 1 KB (stdio's) and
 * large reads with seeks back, a missing file missing, the folder listed.
 *   make dc-iso-test */
#include "../src/port/dc/dc_iso.c"
FILE *g_iso; int g_cmds, g_seeks;
int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "dc_iso_test BASE (BASE.iso, BASE.json from tests/dc_iso_mkiso.py)\n"); return 2; }
    char pi[512], pj[512];
    snprintf(pi, sizeof pi, "%s.iso", argv[1]); snprintf(pj, sizeof pj, "%s.json", argv[1]);
    g_iso = fopen(pi, "rb");
    FILE *j = fopen(pj, "r");
    if (!g_iso || !j) { perror(argv[1]); return 2; } static char js[1 << 20]; size_t jn = fread(js, 1, sizeof js - 1, j); js[jn] = 0;
    if (dc_iso_mount()) { printf("mount failed\n"); return 1; }
    int bad = 0, files = 0;
    unsigned seed = 1;
    for (char *p = js; (p = strchr(p, '"')); ) {
        char name[32]; unsigned lba, size;
        if (sscanf(p, "\"%31[^\"]\": [%u, %u]", name, &lba, &size) != 3) { p++; continue; }
        p = strchr(p, ']');
        char path[64]; snprintf(path, sizeof path, (files & 1) ? "DATA\\%s" : "/data/%s", name);
        for (char *q = path; *q; q++) if (files % 3 == 0 && *q >= 'A' && *q <= 'Z') *q += 32;   /* (any case) */
        int c0 = g_cmds;
        CdFile *f = g_vh.open(&g_vh, path, 0);
        if (!f) { printf("%s not found\n", path); bad++; continue; }
        if (files > 1 && g_cmds != c0) { printf("%s: the directory read again\n", path); bad++; }
        static unsigned char want[400000], got[400000];
        fseek(g_iso, (long)lba * 2048, SEEK_SET); if (fread(want, 1, size, g_iso) != size) return 2;
        unsigned at = 0;
        while (at < size) {                       /* reads of mixed sizes, some seeks */
            seed = seed * 1103515245u + 12345u;
            unsigned n = (seed >> 8) % 3 == 0 ? 1 + (seed >> 12) % 100 : (seed >> 8) % 3 == 1 ? 1024 : 1 + (seed >> 10) % 70000;
            if ((seed >> 20) % 7 == 0 && at) { at = (seed >> 3) % at; g_vh.seek(f, at, SEEK_SET); }   /* (back only: every byte read once at least) */
            ssize_t r = g_vh.read(f, got + at, n);
            if (r <= 0) { printf("%s: read %u at %u gave %d\n", path, n, at, (int)r); bad++; break; }
            at += (unsigned)r;
        }
        if (memcmp(got, want, size)) { printf("%s: bytes differ (pieces)\n", path); bad++; }
        g_vh.seek(f, 0, SEEK_SET);
        if (g_vh.read(f, got, size) != (ssize_t)size) bad++;
        if (memcmp(got, want, size)) { printf("%s: bytes differ\n", path); bad++; }
        if (g_vh.total(f) != size) bad++;
        g_vh.close(f);
        files++;
    }
    struct stat st;
    if (g_vh.stat(&g_vh, "data/NOPE.DAT", &st, 0) == 0) { printf("a missing file found\n"); bad++; }
    if (g_vh.stat(&g_vh, "data", &st, 0) != 0 || !S_ISDIR(st.st_mode)) { printf("data not a folder\n"); bad++; }
    CdFile *d = g_vh.open(&g_vh, "data", O_DIR); int n = 0; while (g_vh.readdir(d)) n++;
    printf("%d files checked, %d in the folder listing, %d drive commands, %d failures\n", files, n, g_cmds, bad);
    dc_iso_report();
    return bad != 0;
}
