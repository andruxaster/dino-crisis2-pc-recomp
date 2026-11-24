#pragma once
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#define ERR_OK 0
#define CD_GDROM 0x80
typedef struct { int x; } cd_toc_t;
extern FILE *g_iso; extern int g_cmds, g_seeks;
static inline int cdrom_get_status(int *s, int *t) { *s = 0; *t = 0x20; return 0; }
static inline int cdrom_read_toc(cd_toc_t *t, bool hd) { (void)t; (void)hd; return 0; }
static inline uint32_t cdrom_locate_data_track(cd_toc_t *t) { (void)t; return 150; }
static inline int cdrom_read_sectors_ex(void *b, uint32_t sec, size_t n, bool dma) { (void)dma; g_cmds++; g_seeks++; fseek(g_iso, (long)(sec - 150) * 2048, SEEK_SET); return fread(b, 2048, n, g_iso) == n ? 0 : -1; }
static long g_spos; static int g_sleft;
static inline int cdrom_stream_start(int sec, int cnt, bool dma) { (void)dma; g_cmds++; g_seeks++; g_spos = (long)(sec - 150) * 2048; g_sleft = cnt; return 0; }
static inline int cdrom_stream_request(void *b, size_t n, bool blk) { (void)blk; if ((uintptr_t)b & 31 || n % 2048 || (int)(n / 2048) > g_sleft) return -1; fseek(g_iso, g_spos, SEEK_SET); if (fread(b, 1, n, g_iso) != n) return -1; g_spos += n; g_sleft -= n / 2048; return 0; }
static inline int cdrom_stream_stop(bool a) { (void)a; return 0; }
