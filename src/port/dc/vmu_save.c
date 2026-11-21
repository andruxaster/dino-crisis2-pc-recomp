/* Saves on the VMU (Dreamcast build).
 *
 * The game writes and reads SAVE00.DAT..SAVE99.DAT (0x5F4 bytes each) with
 * plain file calls. On the Dreamcast those go to the RAM disk (/ram, set as
 * g_pc_save_root), and this file keeps the VMU in step with it:
 *   at start: every DC2_SAVEnn on the first VMU is unpacked into /ram/SAVEnn.DAT;
 *   after each save (g_pc_save_written): the file is packed with a VMS header
 *   (names for the BIOS's memory manager, the icon) and written to the VMU.
 * The VMS header keeps the payload's exact length, so a save reads back as
 * the 0x5F4 bytes the game checks, not rounded up to the VMU's 512-byte blocks.
 * The icon is the save's own: the PS1 memory card header the game puts at the
 * start of the file (16 colours at +0x60, a 16x16 picture at +0x80), doubled
 * to the VMU's 32x32. */
#include <kos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dc/maple.h>
#include <dc/vmu_pkg.h>
#include <dc/vmufs.h>
#include "dc2/pcio.h"
#include "vmu_save.h"

#define SAVE_SIZE 0x5F4
#define SAVE_ROOT "/ram"

static maple_device_t *first_vmu(void) { return maple_enum_type(0, MAPLE_FUNC_MEMCARD); }

/* "SAVE07.DAT" -> 7; -1 for any other name */
static int slot_of(const char *name)
{
    const char *b = strrchr(name, '/');
    b = b ? b + 1 : name;
    if (strncasecmp(b, "SAVE", 4) || b[4] < '0' || b[4] > '9' || b[5] < '0' || b[5] > '9' || strcasecmp(b + 6, ".DAT")) return -1;
    return (b[4] - '0') * 10 + (b[5] - '0');
}

/* the PS1 icon (CLUT of 15-bit colours, 16x16 at 4 bits, the left pixel in
   the low nibble) as the VMU's: ARGB4444 palette, 32x32, the left pixel in the
   high nibble; colour 0000 (the PS1's transparent black) left transparent */
static void make_icon(const u8 *save, vmu_pkg_t *pkg, u8 *icon)
{
    int ps1 = save[0] == 'S' && save[1] == 'C';
    for (int i = 0; i < 16; i++) {
        u32 c = ps1 ? (u32)(save[0x60 + 2 * i] | save[0x61 + 2 * i] << 8) : (i ? 0x7FFF : 0);
        u32 r = c & 31, g = c >> 5 & 31, b = c >> 10 & 31;
        pkg->icon_pal[i] = (u16)(((c & 0x7FFF) ? 0xF000 : 0) | (r >> 1) << 8 | (g >> 1) << 4 | (b >> 1));
    }
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x += 2) {
            u32 p = 1;
            if (ps1) { u8 v = save[0x80 + (y >> 1) * 8 + (x >> 2)]; p = (x >> 1) & 1 ? v >> 4 : v & 15; }
            icon[y * 16 + x / 2] = (u8)(p << 4 | p);    /* (each PS1 pixel twice across) */
        }
}

void DcSave_Written(const char *host, const char *name)
{
    int slot = slot_of(name);
    if (slot < 0) return;
    maple_device_t *vmu = first_vmu();
    if (!vmu) { printf("save: no VMU, SAVE%02d.DAT stays in %s only\n", slot, SAVE_ROOT); return; }
    FILE *f = fopen(host, "rb");
    if (!f) return;
    static u8 data[SAVE_SIZE];
    size_t n = fread(data, 1, sizeof data, f);
    fclose(f);
    if (n != SAVE_SIZE) return;

    static u8 icon[512];
    vmu_pkg_t pkg;
    memset(&pkg, 0, sizeof pkg);
    u32 t = (u32)(data[0x10C] | data[0x10D] << 8 | data[0x10E] << 16 | (u32)data[0x10F] << 24);   /* play time, 1/60 s */
    u32 h = t / 216000, m = t / 3600 % 60, s = t / 60 % 60;
    snprintf(pkg.desc_short, sizeof pkg.desc_short, "DINO CRISIS 2");
    snprintf(pkg.desc_long, sizeof pkg.desc_long, "No.%02d %02u:%02u:%02u", slot, (unsigned)(h > 99 ? 99 : h), (unsigned)m, (unsigned)s);
    snprintf(pkg.app_id, sizeof pkg.app_id, "DC2RECOMP");
    pkg.icon_cnt = 1;
    pkg.icon_anim_speed = 0;
    pkg.eyecatch_type = VMUPKG_EC_NONE;
    pkg.data_len = SAVE_SIZE;
    pkg.data = data;
    make_icon(data, &pkg, icon);
    pkg.icon_data = icon;

    u8 *out = NULL;
    int out_n = 0;
    if (vmu_pkg_build(&pkg, &out, &out_n) < 0 || !out) { printf("save: VMS packing failed\n"); return; }
    char vname[16];
    snprintf(vname, sizeof vname, "DC2_SAVE%02d", slot);
    if (vmufs_write(vmu, vname, out, out_n, VMUFS_OVERWRITE) < 0)
        printf("save: writing %s to the VMU failed (full?)\n", vname);
    else
        printf("save: %s on the VMU (%d blocks)\n", vname, (out_n + 511) / 512);
    free(out);
}

void DcSave_Import(void)
{
    g_pc_save_root = SAVE_ROOT;
    g_pc_save_written = DcSave_Written;
    maple_device_t *vmu = first_vmu();
    if (!vmu) { printf("save: no VMU found, saves last until power-off\n"); return; }
    vmu_dir_t *dir = NULL;
    int cnt = 0;
    if (vmufs_readdir(vmu, &dir, &cnt) < 0 || !dir) return;
    int got = 0;
    for (int i = 0; i < cnt; i++) {
        char fn[16];
        memcpy(fn, dir[i].filename, 12); fn[12] = 0;
        if (strncmp(fn, "DC2_SAVE", 8) || fn[8] < '0' || fn[8] > '9' || fn[9] < '0' || fn[9] > '9') continue;
        void *raw = NULL;
        int raw_n = 0;
        if (vmufs_read_dirent(vmu, &dir[i], &raw, &raw_n) < 0 || !raw) continue;
        vmu_pkg_t pkg;
        if (vmu_pkg_parse(raw, (size_t)raw_n, &pkg) >= 0 && pkg.data_len == SAVE_SIZE && pkg.data) {
            char path[32];
            snprintf(path, sizeof path, SAVE_ROOT "/SAVE%c%c.DAT", fn[8], fn[9]);
            FILE *f = fopen(path, "wb");
            if (f) { fwrite(pkg.data, 1, SAVE_SIZE, f); fclose(f); got++; }
        }
        free(raw);
    }
    free(dir);
    printf("save: %d save(s) read from the VMU\n", got);
}
