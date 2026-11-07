/*
 * dc2_render_states.h -- blend / depth / fog behaviour of the Direct3D path
 * of Dino Crisis 2 PC (Dino2.exe, 2002-02-05). Documentation only: the
 * tables below describe what the binary does, as reference for a new backend.
 *
 * Per textured/untextured packet, every PC_Draw_* handler calls, in order:
 *   Tex_Bind(tpage, clut)                 0x4B6470   (textured only)
 *   PC_SetDepthMode(g_drawmode_tpage)     0x4B61C0
 *   PC_SetFogForPrim(on)                  0x4B5E60
 *   PC_CalcVertexColour(r,g,b,code,tpage,&argb)  0x4B5F20  (per vertex)
 *   PC_SetBlendMode(code, tpage)          0x4B60A0
 * All state setters cache the last value (0x73B684 colorop, 0x73B688 abr,
 * 0x73B690 depth flags) and only call SetRenderState on change.
 *
 * ---- semi-transparency (code bit 1), abr = (tpage >> 5) & 3 ----------------
 *  abr  PS1 meaning     vertex alpha   SRCBLEND     DESTBLEND      result
 *   0   B/2 + F/2       0x80           SRCALPHA     INVSRCALPHA    0.5B + 0.5F
 *   1   B + F           0xFF           ONE          ONE            B + F
 *   2   B - F           0xFF           ZERO         INVSRCCOLOR    B * (1 - F)  (approximation)
 *   3   B + F/4         0x40           SRCALPHA     ONE            B + 0.25F
 *  opaque packet:       0xFF           SRCALPHA     INVSRCALPHA    (blending stays on)
 *  The semi alpha is only applied if 0x73B67C != 0 (else alpha 0xFF).
 *  Texel 0x0000 has alpha 0 (see dc2_texcache.h) and is cut by the alpha test,
 *  so it neither draws nor writes Z. The PS1 STP bit is ignored: semi applies
 *  to every non-zero texel of a semi packet.
 *
 * ---- texture colour (code bit 2 = textured, bit 0 = raw) --------------------
 *  if (g_render_flags 0x75DEF0 & 4) && (tpage & 0x1000):
 *      COLOROP = MODULATE2X; vertex colour as given (0x80 = 1.0), raw -> 0x80
 *  else:
 *      COLOROP = MODULATE;   vertex colour * 2 clamped to 0xFF, raw -> 0xFF
 *  untextured: COLOROP = MODULATE, colour as given.
 *  Vertex colour is D3DCOLOR (alpha<<24 | r<<16 | g<<8 | b).
 *
 * ---- depth: PC-only flags in tpage bits 10..11 (from the DR_MODE packet) ------
 *  tpage & 0x400  -> ZENABLE 0, ZWRITEENABLE 0, ZFUNC ALWAYS   (pure 2D overlay)
 *  otherwise      -> ZENABLE 1, ZWRITEENABLE 1, ZFUNC LESSEQUAL
 *  tpage & 0x800  -> ZFUNC ALWAYS (draw over, still writes Z unless 0x400)
 *  Vertex z comes from the PC_stsz* helpers: SZ / (2 * depth_scale).
 *
 * ---- fixed states set once in D3D init 0x4BBA30 -----------------------------
 *  CULLMODE NONE, TEXTUREPERSPECTIVE on, ZENABLE on, ZFUNC LESSEQUAL,
 *  ALPHABLENDENABLE on, ALPHATESTENABLE on, ALPHAREF 8, ALPHAFUNC GREATER,
 *  stage 0: COLORARG1 TEXTURE, ALPHAOP MODULATE / ALPHAARG1 TEXTURE,
 *  MAGFILTER LINEAR, MINFILTER LINEAR (bilinear), ADDRESS CLAMP.
 *
 * ---- fog --------------------------------------------------------------------
 *  SetFarColor 0x4B4E60: FOGCOLOR, FOGTABLEMODE LINEAR, FOGTABLESTART/END.
 *  PC_SetFogEnable 0x4B4F50 global switch; PC_SetFogForPrim(0/1) per handler,
 *  skipped when 0x865EEC == 0 (fog off) or in DrawOTag path A.
 */
#ifndef DC2_RENDER_STATES_H
#define DC2_RENDER_STATES_H

enum { PC_ABR_HALF = 0, PC_ABR_ADD = 1, PC_ABR_SUB = 2, PC_ABR_ADD_QUARTER = 3 };
enum { PC_TPAGE_NO_DEPTH = 0x400, PC_TPAGE_DEPTH_ALWAYS = 0x800, PC_TPAGE_MOD2X = 0x1000 };

#endif
