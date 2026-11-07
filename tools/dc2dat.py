#!/usr/bin/env python3
"""dc2dat.py -- unpacker for Dino Crisis 2 PC (2002) data containers.

Container (CORE*.DAT, DOOR*.DAT, E*.DAT, ST*.DAT, KOF_*.DAT, ...):
  0x000..0x7FF  header: up to 64 records of 32 bytes; unused slots are zero
                or start with "dummy header".
  0x800..       payloads in record order, each padded to a 0x800 sector.
ST*.DBS is a chain of such containers, one per camera angle.

record: +0 u32 type  +4 u32 size  +8 type-specific (16 bytes)
  (handlers from File_Process 0x404290 and the camera loader 0x403AC0)
  type 0  copy to RAM          +8 u32 dest (absolute VA in the exe's .psxseg)
  type 1  VRAM image (raw)     +8 u16 x, y, w, h  (16-bit VRAM units)
  type 2  CLUT                 +8 u16 x, y, w, h
  type 3  sound-effect bank    +8 u32 bank slot (0x87FBE0[slot]); +0x000 32 x 8-byte
          effect descriptors, +0x100 32 x {u32 offset, u32 size} of RIFF WAVE
          (PCM16 mono 22050 Hz) -> extracted as rec_NN_seKK.wav
  type 4  streamed sound       +8 u32 slot (0x87FB84, Strm_Open); dumped raw
  type 5  LZSS -> RAM          +8 u32 dest
  type 6  LZSS VRAM image      +8 u16 x, y, w, h; unpacked data is 32x32-halfword
          tiles of 0x800 bytes, left to right, then next 32 rows
  type 7  buffer               dumped raw
  type 8  LZSS VRAM image      +8 u16 x, y, w, h; one rectangle (DBS backgrounds)
  DBS record 3 may instead be type 0 with arg = unpacked size: LZSS mask
          pixels, 18x18 BGR555 per mask (16x16 + 1-pixel border), see dc2room.py
LZSS: see dc2lzss.py.

Headerless archives (the loader synthesizes the records, File_Process flags):
  FILE.DAT 0x404100: 48 x 0x3800 = 128x96 8bpp (VRAM 384,256) + CLUT 256 (row 497)
  ITEM.DAT 0x403EE0: 128 x 0x2800 = 128x64 8bpp (VRAM 768,192) + CLUT 256 (row 497)
  MAP.DAT  0x403FD0: 87 x 0x10800 = 2 x 256x256 4bpp (VRAM 448,0 and 384,0)
                     + CLUT 16 (row 500)
*.PXL: raw 320x240 BGR555 screen (153600 bytes) -> image.png.

Usage:  dc2dat.py OUT_DIR FILE [FILE ...]
Per input: OUT_DIR/<name>/[camNN/] rec_NN_typeT.bin (packed records also as
.unpacked.bin), manifest.json, vram.png (15-bit view of the 1024x512 VRAM
with all uploads placed), rec_NN_8bpp.png / rec_NN_4bpp.png using the first
CLUT of the container (pairing is a guess; the game picks the CLUT per
packet). Room texture pages (type 6 in ST*.DAT) are 4bpp; DBS backgrounds 8bpp.
"""
import json, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dc2lzss import unpack  # noqa: E402

SECTOR = 0x800
PSXSEG_VA, PSXSEG_SIZE = 0x4E8000, 0x21B000
TYPE_NAMES = {0: "ram_copy", 1: "vram_image", 2: "vram_clut", 3: "sound_bank", 4: "sound_stream",
              5: "lzss_ram", 6: "lzss_vram_tiles", 7: "buffer", 8: "lzss_vram_rect"}
VRAM_TYPES = (1, 2, 6, 8)

try:
    from PIL import Image
except ImportError:
    Image = None


def bgr555(v):
    r, g, b = v & 31, (v >> 5) & 31, (v >> 10) & 31
    return (r << 3 | r >> 2, g << 3 | g >> 2, b << 3 | b >> 2)


def parse_container(d, base=0):
    recs, off = [], base + SECTOR
    for i in range(SECTOR // 32):
        r = d[base + i * 32:base + i * 32 + 32]
        if len(r) < 32 or r.startswith(b"dummy") or r == bytes(32):
            continue
        t, size = struct.unpack_from("<II", r)
        if t > 0x20 or size > len(d):
            return [], base
        rec = {"index": i, "type": t, "type_name": TYPE_NAMES.get(t, "unknown"),
               "size": size, "file_offset": off}
        if t in VRAM_TYPES:
            rec["x"], rec["y"], rec["w"], rec["h"] = struct.unpack_from("<4H", r, 8)
        else:
            a = struct.unpack_from("<I", r, 8)[0]
            rec["arg"] = a
            if PSXSEG_VA <= a < PSXSEG_VA + PSXSEG_SIZE:
                rec["psxseg_offset"] = a - PSXSEG_VA
        rec["raw_header"] = r.hex()
        recs.append(rec)
        off += (size + SECTOR - 1) & ~(SECTOR - 1)
    return recs, off


def split_chain(d):
    """Return [(base, recs)] if d is one container or a chain of them."""
    out, base = [], 0
    while base < len(d):
        recs, end = parse_container(d, base)
        if not recs or end <= base or end > len(d):
            return None
        out.append((base, recs))
        base = end
    return out


def vram_put(vram, x0, y0, w, h, data):
    hw = struct.unpack_from(f"<{min(len(data) // 2, w * h)}H", data)
    for j, v in enumerate(hw):
        vram[(x0 + j % w, y0 + j // w)] = v


def save_png(path, w, h, pixels):
    if Image is not None:
        im = Image.new("RGB", (w, h))
        im.putdata(pixels)
        im.save(path)


def unpack_container(d, recs, out):
    vram, clut = {}, None
    for r in recs:
        p = d[r["file_offset"]:r["file_offset"] + r["size"]]
        fn = f"rec_{r['index']:02d}_type{r['type']}.bin"
        open(os.path.join(out, fn), "wb").write(p)
        r["file"] = fn
        t = r["type"]
        lzss_type0 = (t == 0 and "arg" in r and "psxseg_offset" not in r and r["arg"] > r["size"])
        if lzss_type0:  # DBS mask pixels: arg = unpacked size, LZSS payload
            r["type_name"] = "lzss_mask_pixels"
        if t in (5, 6, 8) or lzss_type0:
            u = unpack(p)
            r["unpacked_size"] = len(u)
            open(os.path.join(out, fn.replace(".bin", ".unpacked.bin")), "wb").write(u)
            p = u
        if t == 3 and len(p) >= 0x200:
            r["sounds"] = []
            for k in range(32):
                o, n = struct.unpack_from("<II", p, 0x100 + 8 * k)
                if o and p[o:o + 4] == b"RIFF":
                    wn = f"rec_{r['index']:02d}_se{k:02d}.wav"
                    open(os.path.join(out, wn), "wb").write(p[o:o + n])
                    r["sounds"].append({"slot": k, "file": wn, "size": n,
                                        "descriptor": p[8 * k:8 * k + 8].hex()})
        if t in (1, 2, 8):
            vram_put(vram, r["x"], r["y"], r["w"], r["h"], p)
            if t == 2 and clut is None:
                clut = [bgr555(v) for v in struct.unpack_from("<256H", p)]
        elif t == 6:
            tiles_x = (r["w"] + 31) // 32
            for k in range(len(p) // 0x800):
                tx, ty = (k % tiles_x) * 32, (k // tiles_x) * 32
                vram_put(vram, r["x"] + tx, r["y"] + ty, 32, 32, p[k * 0x800:(k + 1) * 0x800])
        if t in (1, 6, 8):
            r["pixels_8bpp"] = [r["w"] * 2, r["h"]]
    if Image is not None and vram:
        save_png(os.path.join(out, "vram.png"), 1024, 512,
                 [bgr555(vram.get((x, y), 0)) for y in range(512) for x in range(1024)])
        if clut:
            for r in recs:
                if r["type"] not in (1, 6, 8):
                    continue
                w2, h = r["w"] * 2, r["h"]
                px = []
                for y in range(h):
                    for x in range(r["w"]):
                        v = vram.get((r["x"] + x, r["y"] + y), 0)
                        px += [clut[v & 0xFF], clut[v >> 8]]
                save_png(os.path.join(out, f"rec_{r['index']:02d}_8bpp.png"), w2, h, px)
                px4 = []
                for y in range(h):
                    for x in range(r["w"]):
                        v = vram.get((r["x"] + x, r["y"] + y), 0)
                        px4 += [clut[(v >> s) & 15] for s in (0, 4, 8, 12)]
                save_png(os.path.join(out, f"rec_{r['index']:02d}_4bpp.png"), r["w"] * 4, h, px4)
    return recs


def unpack_filedat(d, out):
    chunk, recs = 0x3800, []
    for k in range(len(d) // chunk):
        c = d[k * chunk:(k + 1) * chunk]
        fn = f"img_{k:03d}"
        open(os.path.join(out, fn + ".bin"), "wb").write(c)
        pal = [bgr555(v) for v in struct.unpack_from("<256H", c, 0x3000)]
        save_png(os.path.join(out, fn + ".png"), 128, 96, [pal[b] for b in c[:0x3000]])
        recs.append({"index": k, "file_offset": k * chunk, "file": fn + ".bin"})
    return recs


def unpack_fixed_archive(d, out, kind):
    ents = []
    if kind == "ITEM.DAT":
        for k in range(len(d) // 0x2800):
            e = d[k * 0x2800:(k + 1) * 0x2800]
            pal = [bgr555(v) for v in struct.unpack_from("<256H", e, 0x2000)]
            save_png(os.path.join(out, f"item_{k:03d}.png"), 128, 64, [pal[b] for b in e[:0x2000]])
            ents.append({"index": k, "offset": k * 0x2800, "file": f"item_{k:03d}.png"})
    else:
        for k in range(len(d) // 0x10800):
            e = d[k * 0x10800:(k + 1) * 0x10800]
            pal = [bgr555(v) for v in struct.unpack_from("<16H", e, 0x10000)]
            for half in range(2):
                px = e[half * 0x8000:(half + 1) * 0x8000]
                save_png(os.path.join(out, f"map_{k:02d}_{half}.png"), 256, 256,
                         [pal[(b >> s) & 15] for b in px for s in (0, 4)])
            ents.append({"index": k, "offset": k * 0x10800,
                         "files": [f"map_{k:02d}_0.png", f"map_{k:02d}_1.png"],
                         "note": "half 0 -> VRAM x=448, half 1 -> x=384"})
    return ents


def main(argv):
    if len(argv) < 3:
        sys.exit(__doc__)
    for path in argv[2:]:
        d = open(path, "rb").read()
        name = os.path.splitext(os.path.basename(path))[0] + os.path.splitext(path)[1].replace(".", "_")
        out = os.path.join(argv[1], name)
        os.makedirs(out, exist_ok=True)
        chain = split_chain(d)
        if chain:
            manifest = {"name": name, "containers": []}
            for n, (base, recs) in enumerate(chain):
                sub = out if len(chain) == 1 else os.path.join(out, f"cam{n:02d}")
                os.makedirs(sub, exist_ok=True)
                manifest["containers"].append({"offset": base, "dir": os.path.relpath(sub, out),
                                               "records": unpack_container(d, recs, sub)})
            summary = f"{len(chain)} container(s), " + ", ".join(
                f"{r['type_name']}:{r['size']:#x}" for r in chain[0][1])
        elif path.upper().endswith(".PXL") and len(d) == 320 * 240 * 2:
            px = [bgr555(v) for v in struct.unpack(f"<{320 * 240}H", d)]
            save_png(os.path.join(out, "image.png"), 320, 240, px)
            manifest = {"name": name, "format": "pxl", "width": 320, "height": 240}
            summary = "PXL 320x240 BGR555"
        elif os.path.basename(path).upper() in ("ITEM.DAT", "MAP.DAT"):
            manifest = {"name": name, "format": os.path.basename(path).upper(),
                        "entries": unpack_fixed_archive(d, out, os.path.basename(path).upper())}
            summary = f"{len(manifest['entries'])} entries"
        elif len(d) % 0x3800 == 0:
            manifest = {"name": name, "format": "filedat", "images": unpack_filedat(d, out)}
            summary = f"{len(manifest['images'])} images 128x96 8bpp"
        else:
            print(f"{path}: unknown layout, skipped")
            continue
        json.dump(manifest, open(os.path.join(out, "manifest.json"), "w"), indent=1)
        print(f"{path}: {summary}")


if __name__ == "__main__":
    main(sys.argv)
