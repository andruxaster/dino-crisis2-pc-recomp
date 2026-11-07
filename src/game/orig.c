/* Calls into code that is not rewritten yet, see include/dc2/orig.h. */
#include "dc2/orig.h"
#include <stdio.h>

#ifdef DC2_IN_PROCESS
#define ORIG(addr, type) ((type)(uintptr_t)(addr))
void DC2CALL Camera_LookAt(gptr t, gptr d, gptr a, gptr e)
{ ORIG(0x403840, void (DC2CALL *)(gptr, gptr, gptr, gptr))(t, d, a, e); }
void DC2CALL Timer_Set(u32 f, u32 b) { ORIG(0x405DE0, void (DC2CALL *)(u32, u32))(f, b); }
void DC2CALL Obj_UpdateMatrix(gptr o) { ORIG(0x48FDE0, void (DC2CALL *)(gptr))(o); }
void DC2CALL Gte_SetGeomScreen(u32 h) { ORIG(0x4C0A10, void (DC2CALL *)(u32))(h); }
void DC2CALL Gte_RotMatrix(gptr r, gptr m) { ORIG(0x4C0FA0, void (DC2CALL *)(gptr, gptr))(r, m); }
void DC2CALL Gte_SetFogFar(float f) { ORIG(0x4C1B90, void (DC2CALL *)(float))(f); }

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
void DC2CALL Camera_LookAt(gptr t, gptr d, gptr a, gptr e) { (void)t; (void)d; (void)a; (void)e; missing("Camera_LookAt 0x403840"); }
void DC2CALL Timer_Set(u32 f, u32 b) { (void)f; (void)b; missing("Timer_Set 0x405DE0"); }
void DC2CALL Obj_UpdateMatrix(gptr o) { (void)o; missing("Obj_UpdateMatrix 0x48FDE0"); }
void DC2CALL Gte_SetGeomScreen(u32 h) { (void)h; missing("SetGeomScreen 0x4C0A10"); }
void DC2CALL Gte_RotMatrix(gptr r, gptr m) { (void)r; (void)m; missing("RotMatrix 0x4C0FA0"); }
void DC2CALL Gte_SetFogFar(float f) { (void)f; missing("SetFogFar 0x4C1B90"); }

/* standalone: the image buffer has TMP_SIZE spare bytes after IMAGE_SIZE */
static u32 g_tmp_used;
gptr Tmp_Push(u32 n) { g_tmp_used += (n + 3) & ~3u; return IMAGE_BASE + IMAGE_SIZE + TMP_SIZE - g_tmp_used; }
void Tmp_Pop(u32 n) { g_tmp_used -= (n + 3) & ~3u; }
#endif
