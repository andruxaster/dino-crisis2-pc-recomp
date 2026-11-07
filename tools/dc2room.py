#!/usr/bin/env python3
"""dc2room.py -- parse the room block of Dino Crisis 2 PC (ST*.DAT, record type 5).

The type-5 record of STxyy.DAT unpacks (LZSS) to the room block that the game
places at 0x5E0000 in .psxseg; all pointers inside are absolute addresses.
Room_SetPointers 0x481E10 copies its header into the game work struct G:

  hdr+0x00 -> G+0xA3C   hdr+0x04 -> G+0xA50   hdr+0x08 -> G+0xA4C
  hdr+0x0C -> G+0xA44   (camera-switch data, read by 0x4034D0 / 0x4035F0)
  hdr+0x10 -> G+0xA58   hdr+0x14 -> G+0xA54   hdr+0x18 -> G+0xA64
  hdr+0x24 -> G+0x7B8 = cameras + 0x10   (16-byte list header, then 16-byte cameras)
  hdr+0x28 -> G+0xA6C   DBS table: per camera {u32 sector, u32 byte size}
  hdr+0x2C   end of block

CAMERA (16 bytes), used by Camera_Load 0x403AC0:
  s16 eye[3]; u16 h (-> SetGeomScreen); s16 target[3]; s16 roll
  angles = LookAt 0x48F620(eye, target); matrix = RotMatrixZXY(-ax, -ay, roll)

DBS container per camera (Camera_Load 0x403AC0):
  rec 0 type 0 -> 0x6B9500  masks: u16 count, u16 0, then count x
        {u8 x/16, u8 y/16 (low nibble), s16 z}; each mask is a 16x16 screen
        tile of the background drawn at depth z (Masks_Setup 0x4036F0 builds
        6 pre-transformed vertices per mask; drawn where the 0x88 marker
        packet sits in the OT)
  rec 1 type 0 -> 0x6C1500  JPEG 320x240 background
  rec 2 type 2              CLUT
  rec 3 type 8              LZSS 8bpp PS1-style background to VRAM, or
        type 0 with arg = unpacked size: LZSS mask pixels, count x 648 bytes
        = 18x18 BGR555 per mask (16x16 + 1-pixel border); kept at CAM+0x54

Collision and floor (G+0xA4C, checked by 0x486FB0 / edge test 0x487160):
  u8 count, 3 pad, u32 0, u32 offset[count] (from the section start), then
  POLY: u8 flag (0 = off, 0x40 = walkable area, else obstacle), u8 n,
        u8 index, u8 ignore_kinds (object kinds 0x80 player / 0x40 / 0x20 that
        skip it), u16 levels (bit k = polygon exists on level k), u16 0,
        u32 edge[n], s16 {x, z}[n + 1] (closed).
        edge: byte0 type (0 = no wall, bit 1 = passable, bit 7 = passable in
        some moves), byte1 = kinds that ignore this edge.
  Level = trunc(-y / 1000) (obj+0x5A); there is no stored floor height: the
  floor of a level k area is y = -1000 * k (lowest bit of the mask).
Camera-switch zones (G+0xA44, 0x4035F0): u32 count, 0x2C-byte entries:
  u8 flags (0x80 off), 3, u8 camera, 1, u8 level, u8 extra levels, 4,
  s16 {x, z}[4] (zone quad, point test 0x48F790), 16 more bytes (second quad).
Footstep zones (G+0xA64, 0x48C3B0, event codes 0x30/0x31/0x50/0x51):
  u32 count, 20-byte {u8 ?, u8 sound, s8 level, u8 material, s16 {x, z}[4]};
  footstep sound = code + 2 * material (3 = silent).
Lighting (G+0xA3C, 0x46B8C0): bytes 0..2 = ambient colour (SetBackColor).

Usage: dc2room.py ST000.DAT [ST000.DBS] [--masks-png OUTDIR] [--map-svg OUT.svg] > room.json
  --masks-png writes, per camera with masks, the masks pasted at their screen
  positions (camNN_masks.png, magenta = no mask) and a depth view
  (camNN_maskz.png, brighter = nearer).
"""
import json, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dc2lzss import unpack          # noqa: E402
from dc2dat import parse_container  # noqa: E402

BASE = 0x5E0000
FIELDS = ["p00_G_A3C", "p04_G_A50", "p08_G_A4C", "p0C_camswitch_G_A44", "p10_G_A58",
          "p14_G_A54", "p18_G_A64", "p1C", "p20", "p24_cameras", "p28_dbs_table", "p2C_end"]


def room_block(dat):
    recs, _ = parse_container(dat)
    for r in recs:
        if r["type"] == 5 and r.get("arg") == BASE:
            return unpack(dat[r["file_offset"]:r["file_offset"] + r["size"]])
    raise SystemExit("no type-5 room block at 0x5E0000 in this file")


def parse_collision(blk, ptr):
    o = ptr - BASE
    polys = []
    for po in struct.unpack_from("<%dI" % blk[o], blk, o + 8):
        p = o + po
        fl, n, idx, ign, lv = struct.unpack_from("<BBBBH", blk, p)
        edges = struct.unpack_from("<%dI" % n, blk, p + 8)
        pts = struct.unpack_from("<%dh" % (2 * n + 2), blk, p + 8 + 4 * n)
        levels = [k for k in range(16) if lv >> k & 1]
        polys.append({"flag": fl, "walkable": bool(fl & 0x40), "index": idx, "ignore_kinds": ign,
                      "levels": levels, "floor_y": -1000 * levels[0] if levels else None,
                      "edges": [{"type": e & 0xFF, "ignore_kinds": e >> 8 & 0xFF, "hi": e >> 16} for e in edges],
                      "points": [list(t) for t in zip(pts[0::2], pts[1::2])]})
    return polys


def parse_zones(blk, ptr):
    o = ptr - BASE
    zones = []
    for k in range(struct.unpack_from("<I", blk, o)[0]):
        e = o + 4 + 0x2C * k
        q = struct.unpack_from("<8h", blk, e + 12)
        r = struct.unpack_from("<8h", blk, e + 28)
        zones.append({"flags": blk[e], "camera": blk[e + 4], "level": blk[e + 6], "extra_levels": blk[e + 7],
                      "quad": [list(t) for t in zip(q[0::2], q[1::2])],
                      "quad2": [list(t) for t in zip(r[0::2], r[1::2])]})
    return zones


def parse_footsteps(blk, ptr):
    o = ptr - BASE
    out = []
    for k in range(struct.unpack_from("<I", blk, o)[0]):
        e = o + 4 + 20 * k
        q = struct.unpack_from("<8h", blk, e + 4)
        out.append({"b0": blk[e], "sound": blk[e + 1], "level": struct.unpack_from("<b", blk, e + 2)[0],
                    "material": blk[e + 3], "quad": [list(t) for t in zip(q[0::2], q[1::2])]})
    return out


def write_map_svg(path, cams, polys, zones):
    xs = [p[0] for q in polys for p in q["points"]] + [c["eye"][0] for c in cams]
    zs = [p[1] for q in polys for p in q["points"]] + [c["eye"][2] for c in cams]
    x0, x1, z0, z1 = min(xs) - 1000, max(xs) + 1000, min(zs) - 1000, max(zs) + 1000
    sc = 900.0 / max(x1 - x0, z1 - z0)
    W, H = (x1 - x0) * sc, (z1 - z0) * sc
    P = lambda x, z: f"{(x - x0) * sc:.1f},{(z1 - z) * sc:.1f}"   # +z up
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.0f}" height="{H + 40:.0f}" '
           f'font-family="sans-serif" font-size="11"><rect width="100%" height="100%" fill="#16181d"/>']
    hues = [(i * 137) % 360 for i in range(64)]
    for z in zones:
        out.append(f'<polygon points="{" ".join(P(*p) for p in z["quad"])}" fill="hsla({hues[z["camera"]]},70%,55%,0.12)" '
                   f'stroke="hsl({hues[z["camera"]]},70%,60%)" stroke-dasharray="4 3" stroke-width="1"/>')
    for q in polys:
        if not q["flag"]:
            continue
        pts = q["points"]
        for i, e in enumerate(q["edges"]):
            col = "#4ade80" if (not e["type"] or e["type"] & 2) else ("#facc15" if q["walkable"] else "#f87171")
            out.append(f'<line x1="{P(*pts[i]).split(",")[0]}" y1="{P(*pts[i]).split(",")[1]}" '
                       f'x2="{P(*pts[i+1]).split(",")[0]}" y2="{P(*pts[i+1]).split(",")[1]}" stroke="{col}" stroke-width="2"/>')
        cx = sum(p[0] for p in pts[:-1]) / (len(pts) - 1); cz = sum(p[1] for p in pts[:-1]) / (len(pts) - 1)
        lv = q["levels"]
        lab = f"L{lv[0]}" + (f"-{lv[-1]}" if len(lv) > 1 else "") if lv else "-"
        out.append(f'<text x="{P(cx, cz).split(",")[0]}" y="{P(cx, cz).split(",")[1]}" fill="#e5e7eb" text-anchor="middle">{lab}</text>')
    for i, c in enumerate(cams):
        ex, ez, tx, tz = c["eye"][0], c["eye"][2], c["target"][0], c["target"][2]
        col = f"hsl({hues[i]},70%,65%)"
        a, b = P(ex, ez).split(","), P(tx, tz).split(",")
        out.append(f'<line x1="{a[0]}" y1="{a[1]}" x2="{b[0]}" y2="{b[1]}" stroke="{col}" stroke-width="1" opacity="0.7"/>')
        out.append(f'<circle cx="{a[0]}" cy="{a[1]}" r="4" fill="{col}"/><text x="{float(a[0])+6:.1f}" y="{float(a[1])-4:.1f}" fill="{col}">cam {i}</text>')
    out.append(f'<text x="8" y="{H + 25:.0f}" fill="#9ca3af">yellow: walkable area edge, red: obstacle, green: passable edge, '
               f'dashed: camera-switch zones, dots: camera eye -> target; +z up</text></svg>')
    open(path, "w").write("\n".join(out))


def write_mask_pngs(out_dir, cam, ents, pixels):
    try:
        from PIL import Image
    except ImportError:
        return
    col = Image.new("RGB", (320, 240), (255, 0, 255))
    dep = Image.new("L", (320, 240), 0)
    zs = [e[2] for e in ents]
    zmin, zmax = min(zs), max(zs)
    for k, (x, y, z) in enumerate(ents):
        hw = struct.unpack_from("<324H", pixels, k * 648)
        tile = Image.new("RGB", (18, 18))
        tile.putdata([((v & 31) << 3, ((v >> 5) & 31) << 3, ((v >> 10) & 31) << 3) for v in hw])
        pos = (x * 16, (y & 15) * 16)
        col.paste(tile.crop((1, 1, 17, 17)), pos)
        g = 255 - int(200 * (z - zmin) / max(1, zmax - zmin))
        dep.paste(g, pos + (pos[0] + 16, pos[1] + 16))
    col.save(os.path.join(out_dir, f"cam{cam:02d}_masks.png"))
    dep.save(os.path.join(out_dir, f"cam{cam:02d}_maskz.png"))


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    blk = room_block(open(sys.argv[1], "rb").read())
    off = lambda a: a - BASE
    hdr = dict(zip(FIELDS, struct.unpack_from("<12I", blk, 0)))
    ncam_hint = struct.unpack_from("<H", blk, off(hdr["p24_cameras"]) + 4)[0]
    dbs = []
    for i in range(ncam_hint):
        sec, size = struct.unpack_from("<II", blk, off(hdr["p28_dbs_table"]) + 8 * i)
        dbs.append({"sector": sec, "offset": sec * 0x800, "size": size})
    cams = []
    for i in range(ncam_hint):
        e = struct.unpack_from("<3hH3hh", blk, off(hdr["p24_cameras"]) + 16 + 16 * i)
        cams.append({"eye": e[0:3], "h": e[3], "target": e[4:7], "roll": e[7]})
    polys = parse_collision(blk, hdr["p08_G_A4C"])
    zones = parse_zones(blk, hdr["p0C_camswitch_G_A44"])
    out = {"block_size": len(blk), "header": {k: hex(v) for k, v in hdr.items()},
           "camera_list_header": blk[off(hdr["p24_cameras"]):off(hdr["p24_cameras"]) + 16].hex(),
           "cameras": cams, "dbs_table": dbs,
           "ambient_rgb": list(blk[off(hdr["p00_G_A3C"]):off(hdr["p00_G_A3C"]) + 3]),
           "collision": polys, "camera_zones": zones,
           "footstep_zones": parse_footsteps(blk, hdr["p18_G_A64"])}
    args = sys.argv[2:]
    if "--map-svg" in args:
        k = args.index("--map-svg")
        write_map_svg(args[k + 1], cams, polys, zones)
        del args[k:k + 2]
    png_dir = None
    if "--masks-png" in args:
        k = args.index("--masks-png")
        png_dir = args[k + 1]
        del args[k:k + 2]
        os.makedirs(png_dir, exist_ok=True)
    if args:
        dbsdata = open(args[0], "rb").read()
        for i, t in enumerate(dbs):
            recs, end = parse_container(dbsdata, t["offset"])
            t["container_ok"] = end - t["offset"] == t["size"]
            masks = [r for r in recs if r["type"] == 0 and r.get("arg") == 0x6B9500]
            ents = []
            if masks:
                m = masks[0]
                cnt = struct.unpack_from("<H", dbsdata, m["file_offset"])[0]
                t["mask_count"] = cnt
                ents = [struct.unpack_from("<BBh", dbsdata, m["file_offset"] + 4 + 4 * k)
                        for k in range(cnt)]
                if ents:
                    t["mask_z_range"] = [min(e[2] for e in ents), max(e[2] for e in ents)]
            pix = [r for r in recs if r["index"] == 3 and r["type"] == 0]
            if pix and ents:
                r3 = pix[0]
                u = unpack(dbsdata[r3["file_offset"]:r3["file_offset"] + r3["size"]])
                t["mask_pixels_ok"] = len(u) == r3["arg"] == 648 * len(ents)
                if png_dir:
                    write_mask_pngs(png_dir, i, ents, u)
            jpg = [r for r in recs if r["type"] == 0 and r.get("arg") == 0x6C1500]
            if jpg:
                j = dbsdata[jpg[0]["file_offset"]:jpg[0]["file_offset"] + jpg[0]["size"]]
                t["jpeg"] = j[:2] == b"\xff\xd8"
                t["jpeg_bytes"] = len(j)
    json.dump(out, sys.stdout, indent=1)
    print()


if __name__ == "__main__":
    main()
