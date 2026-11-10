/* Zones: the room's navigation graph G+0xA50 (s16 count, then 0x14-byte
 * zones from +4: +4 u32 neighbour bits, +8 point x/y/z (+0xE 0x8000 = any
 * floor, else the floor id), +0x10 u16 offset from the graph of the edge
 * lengths, one u16 per neighbour from the highest index down).
 * An object's zone state: +0x138 zone, +0x13A 1 = heading for a zone point,
 * +0x13C distance to its goal, +0xD8 point it heads for.
 * Each frame the player's zone is found, the shortest paths to it are laid
 * out in 0x87FCE0 (16 x {u16 done, u16 next zone, u32 length}), and every
 * live object gets the point it should walk to. Walls are the polylines of
 * the collision blocks G+0xA4C. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

#define ZONE_PATHS 0x87FCE0u
#define zone_at(i) (U32(G() + 0xA50) + 4 + (u32)(s32)(i) * 0x14)

static int overlap(s16 a0, s16 a1, s16 b0, s16 b1)
{
    s16 lo1 = a0 > a1 ? a1 : a0, hi1 = a0 > a1 ? a0 : a1;
    s16 lo2 = b1 > b0 ? b0 : b1, hi2 = b1 > b0 ? b1 : b0;
    return lo2 <= hi1 && hi2 >= lo1;
}

/* do segments s (x1,z1,x2,z2) and w cross; *den gets the cross product of
 * their directions (its sign: which way s crosses w) */
static int seg_cross(gptr s, gptr w, s32 *den)
{
    if (!overlap(S16(s), S16(s + 4), S16(w), S16(w + 4))) return 0;
    if (!overlap(S16(s + 2), S16(s + 6), S16(w + 2), S16(w + 6))) return 0;
    s32 x1 = S16(s), z1 = S16(s + 2), x3 = S16(w), z3 = S16(w + 2);
    u32 dx1 = (u32)(S16(s + 4) - x1), dz1 = (u32)(S16(s + 6) - z1);
    u32 dx2 = (u32)(S16(w + 4) - x3), dz2 = (u32)(S16(w + 6) - z3);
    s32 d = (s32)(dx1 * dz2 - dx2 * dz1);
    if (!d) return 0;
    u32 ez = (u32)(z1 - z3), ex = (u32)(x1 - x3);
    s32 n1 = (s32)(ez * dx2 - ex * dz2), n2 = (s32)(ez * dx1 - ex * dz1);
    if (d > 0) { if (n1 < 0 || n1 > d) return 0; }
    else if (d > n1 || n1 > 0) return 0;
    if (d > 0) { if (n2 < 0 || n2 > d) return 0; }
    else if (d > n2 || n2 > 0) return 0;
    *den = d;
    return 1;
}

/* only crossings of one turning direction count (the walls are one-sided) */
u32 DC2CALL Wall_Cross(gptr s, gptr w)                              /* 0x487160 */
{
    s32 d;
    return seg_cross(s, w, &d) && (s32)(0u - (u32)d) < 0;
}

/* both directions */
u32 DC2CALL Seg_Hit(gptr s, gptr w)                                 /* 0x47C6C0 */
{
    s32 d;
    return (u32)seg_cross(s, w, &d);
}

/* a wall (flag 1 polygons and edges, not 4) between points a and b on
 * their floors: shots */
u32 DC2CALL Wall_Between(gptr a, gptr b)                           /* 0x47C550 */
{
    if (!U8(U32(G() + 0xA4C))) return 0;
    gptr seg = Tmp_Push(0x10), w = seg + 8;
    u32 r = 0;
    U16(seg) = U16(a); U16(seg + 2) = U16(a + 4); U16(seg + 4) = U16(b); U16(seg + 6) = U16(b + 4);
    for (u32 k = 0; k < U8(U32(G() + 0xA4C)); k++) {
        gptr c = U32(G() + 0xA4C), blk = c + U32(c + 8 + 4 * k);
        if (!(U8(blk) & 1)) continue;
        u16 fl = U16(blk + 4);
        if (!(fl & (1u << ((S16(a + 2) / -1000) & 31))) && !(fl & (1u << ((S16(b + 2) / -1000) & 31)))) continue;
        if (U8(blk + 3) & 4) continue;
        gptr e = blk + 8, pt = blk + 8 + U8(blk + 1) * 4;
        for (u32 j = 0; j < U8(blk + 1); j++, e += 4, pt += 4) {
            U32(w) = U32(pt); U32(w + 4) = U32(pt + 4);
            if ((U8(e) & 1) && !(U8(e + 1) & 4) && Seg_Hit(seg, w)) { r = 1; goto done; }
        }
    }
done:
    Tmp_Pop(0x10);
    return r;
}

/* is the way from a to b (points) walled for object o; low walls (flag
 * 0x80) are ignored when low is set */
u32 DC2CALL Line_Blocked(gptr o, gptr a, gptr b, u32 low)          /* 0x486FB0 */
{
    gptr g = G();
    u32 who = 0x80;
    if (o != U32(g + 0x940)) who = o == U32(g + 0xB50) ? 0x80 : 0x40;
    if (U32(o + 0x50) & 0x1000) who = 0x20;
    gptr seg = Tmp_Push(8);
    U16(seg) = U16(a); U16(seg + 2) = U16(a + 4);
    U16(seg + 4) = U16(b); U16(seg + 6) = U16(b + 4);
    gptr c = U32(g + 0xA4C);
    u32 r = 0;
    for (s32 k = U8(c) - 1; ; k--) {
        gptr blk = c + U32(c + 8 + k * 4);
        if (U8(blk)) {
            u16 floors = U16(blk + 4);
            s32 fa = S16(a + 2) / -1000, fb = S16(b + 2) / -1000;
            if (((floors & (1u << (fa & 31))) || (floors & (1u << (fb & 31)))) && !((u8)who & U8(blk + 3))) {
                u32 m = U8(blk + 1);
                gptr e = blk + 8, pt = blk + 8 + m * 4;
                for (; m; m--, e += 4, pt += 4) {
                    u8 f = U8(e);
                    if (!f || ((u8)who & U8(e + 1)) || (f & 2) || ((f & 0x80) && (u8)low)) continue;
                    if (Wall_Cross(seg, pt)) { r = 1; goto done; }
                }
            }
        }
        if (!k) break;
    }
    if ((U8(o + 0x184) & 0x10) && (u16)Floor_Cross(b, a, seg, who) != 0xFFFF && U16(seg + 2) != U16(b + 2)) r = 1;
done:
    Tmp_Pop(8);
    return r;
}

/* the zone point o heads for */
static void head_for(gptr o, s16 z)
{
    gptr r = zone_at(z);
    U16(o + 0xD8) = U16(r + 8); U16(o + 0xDA) = U16(r + 0xA); U16(o + 0xDC) = U16(r + 0xC);
    if (U16(r + 0xE) == 0x8000) U16(o + 0xDA) = U16(o + 0x42);
}

/* nearest zone point of o's floor in sight; 1 if none was (then the nearest) */
u32 DC2CALL Zone_Find(gptr o)                                      /* 0x486D00 */
{
    u32 d[16], id[16];
    gptr base = U32(G() + 0xA50);
    s16 i = S16(base);
    do { i--; d[i] = Dist2_XZ(o + 0x40, base + i * 0x14 + 0xC); id[i] = (u32)i; } while (i);
    s32 n = S16(U32(G() + 0xA50));
    int swapped = 0;
    for (s16 pass = 0; pass < n - 1; ) {
        for (s16 j = 0; j < n - pass - 1; j++)
            if (d[j] > d[j + 1]) {
                u32 t = d[j]; d[j] = d[j + 1]; d[j + 1] = t;
                t = id[j]; id[j] = id[j + 1]; id[j + 1] = t;
                swapped = 1;
            }
        if (!swapped) break;
        pass++;
    }
    U16(o + 0x138) = 0;
    U32(o + 0x13C) = 0x7FFFFFFF;
    for (s16 k = 0; k < S16(U32(G() + 0xA50)); k++) {
        gptr r = base + id[k] * 0x14 + 4;
        u16 fl = U16(r + 0xE);
        if (U16(o + 0x5A) != fl && fl != 0x8000) continue;
        if (!(u8)Line_Blocked(o, o + 0x40, r + 8, 1)) {
            U16(o + 0x138) = (u16)id[k]; U32(o + 0x13C) = d[k];
            break;
        }
    }
    if (U32(o + 0x13C) != 0x7FFFFFFF) return 0;
    U16(o + 0x138) = (u16)id[0]; U32(o + 0x13C) = d[0];
    return 1;
}

/* shortest paths over the graph from o's zone (Dijkstra); tab gets for
 * each zone the next zone toward o and the length. Clears one entry before
 * the table too, as the original does */
void DC2CALL Zone_Paths(gptr o, gptr tab)                          /* 0x486EC0 */
{
    gptr base = U32(G() + 0xA50);
    s32 n = S16(base), left = n;
    gptr e = tab + 0x80;
    do { e -= 8; U16(e) = 0; U16(e + 2) = 0; U32(e + 4) = 0x7FFFFFFF; } while (e >= tab);
    U32(tab + S16(o + 0x138) * 8 + 4) = 0;
    u32 cur = 0;
    do {
        u32 best = 0x7FFFFFFF;
        for (s32 i = S16(base); i-- > 0; )
            if (!U16(tab + i * 8) && U32(tab + i * 8 + 4) < best) { best = U32(tab + i * 8 + 4); cur = (u32)i; }
        U16(tab + cur * 8) = 1;
        left--;
        gptr r = base + 4 + cur * 0x14;
        gptr len = base + U16(r + 0x10);
        for (s32 j = S16(base); j-- > 0; )
            if (U32(r + 4) & (1u << (j & 31))) {
                if (!U16(tab + j * 8)) {
                    u32 l = U32(tab + cur * 8 + 4) + U16(len);
                    if (U32(tab + j * 8 + 4) > l) { U32(tab + j * 8 + 4) = l; U16(tab + j * 8 + 2) = (u16)cur; }
                }
                len += 2;
            }
    } while (left);
}

/* o's goal toward t: t itself when in sight, else the next zone point on
 * the path (o+0x184: 4 = straight to the next zone, 0x20 = see over low walls) */
void DC2CALL Zone_Track(gptr o, gptr t)                            /* 0x4869F0 */
{
    gptr tp = t + 0x40;
    u16 z;
    if (!(u8)Line_Blocked(o, o + 0x40, tp, U8(o + 0x184) & 0x20)) {
        U8(o + 0x13A) = 0;
        U16(o + 0x138) = U16(t + 0x138);
        U32(o + 0x13C) = (u32)SquareRoot0((s32)Dist2_XZ(o + 0x40, tp));
        U16(o + 0xD8) = U16(tp); U16(o + 0xDA) = U16(t + 0x42); U16(o + 0xDC) = U16(t + 0x44);
        return;
    }
    if ((u8)Zone_Find(o)) { head_for(o, S16(o + 0x138)); return; }
    if (U8(o + 0x184) & 4) z = U16(ZONE_PATHS + S16(o + 0x138) * 8 + 2);
    else {
        gptr pt = Tmp_Push(8);
        z = U16(o + 0x138);
        for (;;) {
            gptr r = zone_at(S16(o + 0x138));
            U16(pt) = U16(r + 8); U16(pt + 2) = U16(r + 0xA); U16(pt + 4) = U16(r + 0xC);
            if (U16(r + 0xE) == 0x8000) U16(pt + 2) = U16(o + 0x42);
            if ((u8)Line_Blocked(o, o + 0x40, pt, 1)) break;
            z = U16(o + 0x138);
            if (z == U16(t + 0x138)) break;
            U16(o + 0x138) = U16(ZONE_PATHS + (s16)z * 8 + 2);
        }
        Tmp_Pop(8);
    }
    U16(o + 0x138) = z;
    U8(o + 0x13A) = 1;
    U32(o + 0x13C) = (u32)SquareRoot0((s32)Dist2_XZ(o + 0x40, tp)) + U32(ZONE_PATHS + (s16)z * 8 + 4);
    head_for(o, (s16)z);
}

/* room 903 has scaled objects: distances in their units */
static void scale_903(gptr o)
{
    if (U8(G() + 0x1091) == 9 && U8(G() + 0x1090) == 3)
        U32(o + 0x13C) = (U32(o + 0x13C) << 4) / (u32)(S32(o + 0x6C) >> 8);
}

/* o's goal toward a point (through the stand-in object 0x759148) */
u32 DC2CALL Zone_TrackPoint(gptr o, gptr p)                        /* 0x486C00 */
{
    gptr s = 0x759148;
    U16(s + 0x40) = U16(p); U16(s + 0x42) = U16(p + 2); U16(s + 0x44) = U16(p + 4);
    U16(s + 0x5A) = (u16)(S16(s + 0x42) / -1000);
    Zone_Find(s);
    head_for(s, S16(s + 0x138));
    Zone_Paths(s, ZONE_PATHS);
    Zone_Track(o, s);
    scale_903(o);
    return 0;
}

/* each frame, for the player o: its zone, the paths to it, and every live
 * object's goal (from the player's part +0x188 when o+0x50 has 0x100) */
u32 DC2CALL Zone_Update(gptr o)                                    /* 0x486870 */
{
    u16 x = 0, y = 0, z = 0;
    int moved = (U32(o + 0x50) & 0x100) != 0;
    if (moved) {
        gptr p = Obj_Part(U32(o + 0x64), U32(o + 0x188));
        x = U16(o + 0x40); y = U16(o + 0x42); z = U16(o + 0x44);
        U16(o + 0x40) = U16(p + 0x40); U16(o + 0x42) = U16(p + 0x42); U16(o + 0x44) = U16(p + 0x44);
    }
    Zone_Find(o);
    head_for(o, S16(o + 0x138));
    Zone_Paths(o, ZONE_PATHS);
    for (gptr e = U32(G() + 0x940) + U32(0x87F940) * 0x260; ; ) {
        u32 fl = U32(e - 0x210);
        e -= 0x260;
        if ((fl & 1) && !(fl & 0x20000) && e != U32(G() + 0xB50)) { Zone_Track(e, o); scale_903(e); }
        if (!(e > U32(G() + 0x940) + 0x260)) break;
    }
    if (U32(o + 0x50) & 0x100) { U16(o + 0x40) = x; U16(o + 0x42) = y; U16(o + 0x44) = z; }
    return 0;
}
