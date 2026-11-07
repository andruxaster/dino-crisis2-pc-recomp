/* Calls into code that is not rewritten yet, see include/dc2/orig.h. */
#include "dc2/orig.h"
#include <stdio.h>

#ifdef DC2_IN_PROCESS
#define ORIG_F(ret, name, addr, params, args) \
    ret DC2CALL name params { return ((ret (DC2CALL *) params)(uintptr_t)(addr)) args; }
#define ORIG_V(name, addr, params, args) \
    void DC2CALL name params { ((void (DC2CALL *) params)(uintptr_t)(addr)) args; }
ORIG_LIST(ORIG_F, ORIG_V)

u32 DC2CALL Crt_rand(void) { return ((u32 (DC2CALL *)(void))0x4DDDA2)(); }
void DC2CALL Orig_Call1(gptr fn, u32 a) { ((void (DC2CALL *)(u32))(uintptr_t)fn)(a); }

static u8 g_tmp[0x4000];
static u32 g_tmp_used;
gptr Tmp_Push(u32 n) { g_tmp_used += (n + 3) & ~3u; return (gptr)(uintptr_t)(g_tmp + sizeof g_tmp - g_tmp_used); }
void Tmp_Pop(u32 n) { g_tmp_used -= (n + 3) & ~3u; }
#else
#pragma GCC diagnostic ignored "-Wunused-parameter"
static void missing(const char *name)
{
    static const char *seen[128]; static int n;
    for (int i = 0; i < n; i++) if (seen[i] == name) return;
    if (n < 128) seen[n++] = name;
    fprintf(stderr, "not rewritten yet: %s (skipped)\n", name);
}
#define ORIG_F(ret, name, addr, params, args) \
    ret DC2CALL name params { missing(#name " " #addr); return (ret)0; }
#define ORIG_V(name, addr, params, args) \
    void DC2CALL name params { missing(#name " " #addr); }
ORIG_LIST(ORIG_F, ORIG_V)

static u32 g_rand_seed = 1;
u32 DC2CALL Crt_rand(void) { g_rand_seed = g_rand_seed * 214013u + 2531011u; return (g_rand_seed >> 16) & 0x7FFF; }

void (*g_orig_call1)(gptr fn, u32 a);

void DC2CALL Orig_Call1(gptr fn, u32 a)
{
    if (g_orig_call1) { g_orig_call1(fn, a); return; }
    static char buf[64][24]; static gptr seen[64]; static int n;
    for (int i = 0; i < n; i++) if (seen[i] == fn) { missing(buf[i]); return; }
    if (n < 64) { seen[n] = fn; snprintf(buf[n], sizeof buf[n], "handler 0x%06X", fn); missing(buf[n++]); }
}

/* standalone: the image buffer has TMP_SIZE spare bytes after IMAGE_SIZE */
static u32 g_tmp_used;
gptr Tmp_Push(u32 n) { g_tmp_used += (n + 3) & ~3u; return IMAGE_BASE + IMAGE_SIZE + TMP_SIZE - g_tmp_used; }
void Tmp_Pop(u32 n) { g_tmp_used -= (n + 3) & ~3u; }
#endif
