/* Calls into code that is not rewritten yet, see include/dc2/orig.h. */
#include "dc2/orig.h"
#include <stdio.h>

#ifdef DC2_IN_PROCESS
#define ORIG(addr, type) ((type)(uintptr_t)(addr))
void DC2CALL Timer_Set(u32 f, u32 b) { ORIG(0x405DE0, void (DC2CALL *)(u32, u32))(f, b); }

static u8 g_tmp[0x4000];
static u32 g_tmp_used;
gptr Tmp_Push(u32 n) { g_tmp_used += (n + 3) & ~3u; return (gptr)(uintptr_t)(g_tmp + sizeof g_tmp - g_tmp_used); }
void Tmp_Pop(u32 n) { g_tmp_used -= (n + 3) & ~3u; }
#else
static void missing(const char *name)
{
    static const char *seen[64]; static int n;
    for (int i = 0; i < n; i++) if (seen[i] == name) return;
    if (n < 64) seen[n++] = name;
    fprintf(stderr, "not rewritten yet: %s (skipped)\n", name);
}
void DC2CALL Timer_Set(u32 f, u32 b) { (void)f; (void)b; missing("Timer_Set 0x405DE0"); }

/* standalone: the image buffer has TMP_SIZE spare bytes after IMAGE_SIZE */
static u32 g_tmp_used;
gptr Tmp_Push(u32 n) { g_tmp_used += (n + 3) & ~3u; return IMAGE_BASE + IMAGE_SIZE + TMP_SIZE - g_tmp_used; }
void Tmp_Pop(u32 n) { g_tmp_used -= (n + 3) & ~3u; }
#endif

