/* The camera: the zones that pick it each frame and loading its view (the
 * room's DBS record: masks, background, projection). */
#include <string.h>
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/pcio.h"
#include "dc2/gte.h"

/* ---- the camera: zones that pick it, and loading its view -------------------------- */
/* camera block C = G+0x778: +0 matrix, +0x20 angles, +0x28 target, +0x30
 * distance, +0x38 eye, +0x40 the room's camera records (0x10 each), +0x44
 * the zone record, +0x48 masks, +0x4C camera, +0x4D zone, +0x4F, +0x50
 * mask count, +0x54 mask pixels. Zones (G+0xA44, G+0xA48 of them, 0x2C
 * each): +0 flags (bit 7 off), +4 camera, +5, +6 lowest level, +7 levels,
 * +0xC area (quad) */

/* the zone record of camera C+0x4C (or past the last) */
gptr DC2CALL CamZones_FindForCamera(gptr c, gptr zones)            /* 0x4035B0 */
{
    u32 n = U32(G() + 0xA48);
    if (!n) return zones;
    gptr e = zones;
    for (u32 i = 0; (i & 0xFFFF) < n; i++, e += 0x2C)
        if (U8(e + 4) == U8(c + 0x4C)) return e;
    return e;
}

/* the zone the player (or the part 8 it rides on) is in picks the camera */
void DC2CALL CamZones_Check(gptr c, gptr zones)                    /* 0x4035F0 */
{
    gptr pl = U32(G() + 0x940), pos = Tmp_Push(8), src = pl;
    if (U32(pl + 0x50) & 0x100) src = Obj_Part(U32(pl + 0x64), 8);
    U16(pos) = U16(src + 0x40); U16(pos + 2) = U16(src + 0x42); U16(pos + 4) = U16(src + 0x44);
    gptr e = zones;
    for (u32 i = 0; (i & 0xFFFF) < U32(G() + 0xA48); i++, e += 0x2C) {
        s16 lv = S16(pl + 0x5A);
        if ((s16)U8(e + 6) > lv) continue;
        if ((s32)U8(e + 7) + U8(e + 6) < lv) continue;
        if (U8(e) & 0x80) continue;
        if (!PointInQuad(pos, e + 0xC)) continue;
        U8(c + 0x4D) = (u8)i;
        U8(c + 0x4F) = U8(e + 5);
        if (U8(e + 4) != U8(c + 0x4C)) Camera_Switch(U8(e + 4));
        break;
    }
    Tmp_Pop(8);
}

/* each frame: the camera zones, unless flag 1/0xA */
void DC2CALL Camera_Tick(void)                                     /* 0x403570 */
{
    if (!Flag_Check(1, 0xA)) CamZones_Check(G() + 0x778, U32(G() + 0xA44));
}

/* the camera's view from the room's DBS file (G+0xA6C: sector, size per
 * camera): masks to 0x6B9500, the JPEG background to 0x6C1500, its CLUT or
 * mask pixels; the eye, target and projection from the camera record */
void DC2CALL Camera_Load(void)                                     /* 0x403AC0 */
{
    gptr g = G(), c = g + 0x778;
    gptr rm = U32(0x73825C + U8(g + 0x1091) * 0x18) + U8(g + 0x1090) * 0x18;
    u32 cam = U8(g + 0x84A);
    gptr rec = U32(c + 0x40) + U8(c + 0x4C) * 0x10;
    if (U8(g + 0x927)) { SetGeomScreen(0xE6); return; }
    gptr tab = U32(g + 0xA6C);
    U32(0x87DC9C)++;
    u32 sec = U32(tab + cam * 8), size = U32(tab + cam * 8 + 4);
    gptr name = Tmp_Push(0x28);
    Crt_sprintf1(name, 0x71D5BC, U32(0x71B230 + U16(rm + 4) * 4));
    s32 fd = Pc_Open(name, 0, 0);
    Tmp_Pop(0x28);
    if (fd == -1) return;
    gptr p = Crt_malloc(size);
    Pc_Seek(fd, (s32)(sec << 11), 1);
    Pc_Read(fd, p, size);
    Pc_Close(fd);
    memmove(MEM(0x6B9500), MEM(p + 0x800), U32(p + 4));
    gptr jpg = p + 0x1000;
    JPEG_Decode(0x6C1500, jpg, U32(p + 0x24), 2);
    if (U16(G() + 0x664) != 4) Flag_Set(3, 0, 1);
    U32(G() + 8) = 0x6C1500;
    gptr q = jpg + (((U32(p + 0x24) + 0x7FF) >> 2) & 0x3FFFFE00) * 4;
    if (U32(p + 0x40) != 0x6D6D7564) {                  /* "dumm": none */
        gptr r = Tmp_Push(8);
        U16(r) = U16(p + 0x48); U16(r + 2) = U16(p + 0x4A); U16(r + 4) = U16(p + 0x4C); U16(r + 6) = U16(p + 0x4E);
        Gpu_LoadImageIfChanged(r, q);
        q += 0x800;
        if (!U32(p + 0x60)) {
            gptr m = Crt_malloc(U32(p + 0x68));
            U32(c + 0x54) = m;
            Lzss_Decode(m, q, U32(p + 0x64));
        } else {
            U32(c + 0x54) = 0;
            U16(r) = U16(p + 0x68); U16(r + 2) = U16(p + 0x6A); U16(r + 4) = U16(p + 0x6C); U16(r + 6) = U16(p + 0x6E);
            Lzss_Decode(0x5A0000, q, U32(p + 0x64));
            Gpu_LoadImage(r, 0x5A0000);
        }
        Tmp_Pop(8);
    }
    Crt_free(p);
    U32(c + 0x44) = CamZones_FindForCamera(c, U32(G() + 0xA44));
    U16(c + 0x38) = U16(rec); U16(c + 0x3A) = U16(rec + 2); U16(c + 0x3C) = U16(rec + 4);
    U16(c + 0x28) = U16(rec + 8); U16(c + 0x2A) = U16(rec + 0xA); U16(c + 0x2C) = U16(rec + 0xC);
    SetGeomScreen(U16(rec + 6));
    Angles_LookAt(c + 0x28, c + 0x38, c + 0x20);
    gptr v = Tmp_Push(8);
    U16(v) = (u16)-U16(c + 0x20); U16(v + 2) = (u16)-U16(c + 0x22);
    U16(c + 0x24) = U16(rec + 0xE); U16(v + 4) = U16(rec + 0xE);
    RotMatrixZXY(v, c);
    Tmp_Pop(8);
    U16(c + 0x32) = 0; U16(c + 0x30) = 0;
    U16(c + 0x34) = (u16)Camera_Distance(c + 0x38, c + 0x28);
    U16(c + 0x50) = U16(0x6B9500);
    U32(c + 0x48) = 0x6B9504;
    Masks_Setup(0x6B9504, U16(0x6B9500), U32(c + 0x54));
    for (gptr o = U32(G() + 0x940); o < U32(G() + 0x940) + U32(0x87F940) * 0x260; o += 0x260)
        if (U8(o + 0x50) & 1) U8(o + 0xFE) |= 2;
    Flag_Set(2, 0x18, 0);
    Flag_Set(2, 2, 0);
    U8(G() + 0xB49) = 0;
}
