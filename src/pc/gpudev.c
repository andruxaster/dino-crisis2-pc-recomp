/* The PC layer's Direct3D / DirectDraw calls (include/dc2/gpudev.h): COM
 * calls in-process, the standalone renderer (g_gpudev) otherwise. */
#include <stdint.h>
#include "dc2/types.h"
#include "dc2/gpudev.h"

#ifdef DC2_IN_PROCESS
/* method `off` of COM object o, called with o and the arguments (stdcall) */
#define VT(o, off)       (*(uintptr_t *)(uintptr_t)(*(u32 *)(uintptr_t)(o) + (off)))
#define COM0(o, off)     ((u32 (__stdcall *)(u32))VT(o, off))(o)
#define COM(o, off, T, ...) ((u32 (__stdcall *)T)VT(o, off))(o, __VA_ARGS__)
#define DEV U32(A_DEVICE)

u32 Dev_BeginScene(void) { u32 d = DEV; return COM0(d, 0x24); }
u32 Dev_EndScene(void) { u32 d = DEV; return COM0(d, 0x28); }
u32 Dev_SetRenderState(u32 s, u32 v) { u32 d = DEV; return COM(d, 0x58, (u32, u32, u32), s, v); }
u32 Dev_DrawPrimitive(u32 t, u32 fvf, gptr v, u32 n, u32 f)
{ u32 d = DEV; return COM(d, 0x70, (u32, u32, u32, void *, u32, u32), t, fvf, MEM(v), n, f); }
u32 Dev_DrawPrimitiveVB(u32 t, u32 vb, u32 s, u32 n, u32 f)
{ u32 d = DEV; return COM(d, 0x88, (u32, u32, u32, u32, u32, u32), t, vb, s, n, f); }
u32 Dev_SetTexture(u32 st, u32 tex) { u32 d = DEV; return COM(d, 0x98, (u32, u32, u32), st, tex); }
u32 Dev_SetTextureStageState(u32 st, u32 ty, u32 v) { u32 d = DEV; return COM(d, 0xA0, (u32, u32, u32, u32), st, ty, v); }
u32 Vp_Clear(u32 vp, u32 n, gptr r, u32 f) { return COM(vp, 0x30, (u32, u32, void *, u32), n, MEM(r), f); }
u32 Vp_SetBackground(u32 vp, u32 h) { return COM(vp, 0x20, (u32, u32), h); }
u32 Mat_SetMaterial(u32 m, const void *d) { return COM(m, 0x0C, (u32, const void *), d); }
u32 DD_CreateSurface(void *desc, u32 *surf)
{ u32 dd = U32(A_DDRAW); return COM(dd, 0x18, (u32, void *, u32 *, u32), desc, surf, 0); }
u32 Obj_QueryInterface(u32 o, const void *iid, u32 *out) { return COM(o, 0x00, (u32, const void *, u32 *), iid, out); }
u32 Obj_Release(u32 o) { return COM0(o, 0x08); }
u32 Surf_Blt(u32 dst, const void *dr, u32 src, const void *sr, u32 flags, const void *fx)
{ return COM(dst, 0x14, (u32, const void *, u32, const void *, u32, const void *), dr, src, sr, flags, fx); }
u32 Surf_Flip(u32 s, u32 target, u32 flags) { return COM(s, 0x2C, (u32, u32, u32), target, flags); }
u32 Surf_Lock(u32 s, const void *rect, void *desc, u32 flags)
{ return COM(s, 0x64, (u32, const void *, void *, u32, u32), rect, desc, flags, 0); }
u32 Surf_Restore(u32 s) { return COM0(s, 0x6C); }
u32 Surf_SetColorKey(u32 s, u32 flags, const void *key) { return COM(s, 0x74, (u32, u32, const void *), flags, key); }
u32 Surf_Unlock(u32 s, const void *rect) { return COM(s, 0x80, (u32, const void *), rect); }

#else
const GpuDev *g_gpudev;
#define CALL(f, ...) (g_gpudev && g_gpudev->f ? g_gpudev->f(__VA_ARGS__) : 0)
#define CALL0(f) (g_gpudev && g_gpudev->f ? g_gpudev->f() : 0)

u32 Dev_BeginScene(void) { return CALL0(begin_scene); }
u32 Dev_EndScene(void) { return CALL0(end_scene); }
u32 Dev_SetRenderState(u32 s, u32 v) { return CALL(set_render_state, s, v); }
u32 Dev_DrawPrimitive(u32 t, u32 fvf, gptr v, u32 n, u32 f) { return CALL(draw, t, fvf, MEM(v), n, f); }
u32 Dev_DrawPrimitiveVB(u32 t, u32 vb, u32 s, u32 n, u32 f) { return CALL(draw_vb, t, vb, s, n, f); }
u32 Dev_SetTexture(u32 st, u32 tex) { return CALL(set_texture, st, tex); }
u32 Dev_SetTextureStageState(u32 st, u32 ty, u32 v) { return CALL(set_tss, st, ty, v); }
u32 Vp_Clear(u32 vp, u32 n, gptr r, u32 f) { (void)vp; return CALL(vp_clear, n, MEM(r), f); }
u32 Vp_SetBackground(u32 vp, u32 h) { (void)vp; return CALL(vp_set_background, h); }
u32 Mat_SetMaterial(u32 m, const void *d) { return CALL(set_material, m, d); }
u32 DD_CreateSurface(void *desc, u32 *surf) { if (!g_gpudev || !g_gpudev->create_surface) { *surf = 0; return 1; } return g_gpudev->create_surface(desc, surf); }
u32 Obj_QueryInterface(u32 o, const void *iid, u32 *out) { *out = 0; return CALL(query_interface, o, iid, out); }
u32 Obj_Release(u32 o) { return CALL(release, o); }
u32 Surf_Blt(u32 dst, const void *dr, u32 src, const void *sr, u32 flags, const void *fx) { return CALL(blt, dst, dr, src, sr, flags, fx); }
u32 Surf_Flip(u32 s, u32 target, u32 flags) { return CALL(flip, s, target, flags); }
u32 Surf_Lock(u32 s, const void *rect, void *desc, u32 flags) { if (!g_gpudev || !g_gpudev->lock) return 1; return g_gpudev->lock(s, rect, desc, flags); }
u32 Surf_Restore(u32 s) { return CALL(restore, s); }
u32 Surf_SetColorKey(u32 s, u32 flags, const void *key) { return CALL(set_color_key, s, flags, key); }
u32 Surf_Unlock(u32 s, const void *rect) { return CALL(unlock, s, rect); }
#endif
