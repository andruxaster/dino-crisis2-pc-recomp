#!/usr/bin/env python3
"""dc2cutscene.py -- structure of Dino Crisis 2 PC cutscene files (SC*.DAT).

Every SC*.DAT is one container whose main record is an LZSS type-5 block
loaded at a per-scene address (0x5FE000..0x620000), sometimes with texture
pages + CLUTs. Three block layouts were found (48 files):

  chain       (most files) a chain of animation SEQUENCEs (same format as
              dc2anim.py: u32 frames, u32 events*, 3 x u32 0, frames at
              +0x14, stride align4(16 + 6*parts)). events* points right after
              the frames: u32 count, count x {u16 frame, u16 code}; the next
              sequence follows. events* = 0 -> no table. Parts: 15 = human,
              14/20/21 = other characters / dinosaurs, 1 = a moving object
              (root = path near ground level, mostly yaw rotation).
              Event codes 0x50/0x51 dominate (alternating; likely footsteps).
  model-first block starts with a skeletal model (dc2model.py) followed by a
              4-pointer table of other data (format not decoded yet).
  objects     u32 count + pointers to objects drawn by the env-mapped/static
              mesh routines 0x4851E0 / 0x4854B0 / 0x486320 / 0x485F00
              (object = 2 pointers + 2 x u16 + data; format not decoded yet).
No camera tracks were found in the blocks: in-engine scenes appear to use the
room cameras (pre-rendered DBS backgrounds).

Usage: dc2cutscene.py SC01.DAT [SC02.DAT ...] > cutscenes.json
"""
import json, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dc2dat import parse_container   # noqa: E402
from dc2lzss import unpack           # noqa: E402
from dc2model import looks_like_model  # noqa: E402


def is_seq_header(u, off, base):
    if off + 0x18 > len(u):
        return False
    nf, p, a, b, c = struct.unpack_from("<5I", u, off)
    return 0 < nf < 5000 and (p == 0 or base <= p < base + len(u)) and a == b == c == 0


def walk_chain(u, base, off=0):
    seqs = []
    while is_seq_header(u, off, base):
        nf, p = struct.unpack_from("<II", u, off)
        fit = None
        for parts in range(1, 40):
            fe = off + 0x14 + nf * ((16 + 6 * parts + 3) & ~3)
            if (p and p - base == fe) or (not p and (fe == len(u) or is_seq_header(u, fe, base))):
                fit = (parts, fe)
                break
        if not fit:
            break
        parts, fe = fit
        events, nxt = [], fe
        if p:
            n = struct.unpack_from("<I", u, fe)[0]
            events = [list(struct.unpack_from("<HH", u, fe + 4 + 4 * i)) for i in range(n)]
            nxt = fe + 4 + 4 * n
        seqs.append({"offset": hex(off), "frames": nf, "parts": parts,
                     "events": [{"frame": f, "code": hex(c)} for f, c in events]})
        off = nxt
    return seqs, off


def analyse(path):
    d = open(path, "rb").read()
    recs, _ = parse_container(d)
    blocks = [r for r in recs if r["type"] == 5]
    if not blocks:
        return {"file": os.path.basename(path), "layout": "no type-5 block"}
    r = blocks[0]
    base = r["arg"]
    u = unpack(d[r["file_offset"]:r["file_offset"] + r["size"]])
    out = {"file": os.path.basename(path), "load_address": hex(base), "block_size": len(u),
           "other_records": [{"type": x["type"], "size": x["size"]} for x in recs if x is not r]}
    if looks_like_model(base, u):
        out["layout"] = "model-first"
        out["model_parts"] = struct.unpack_from("<I", u, 0x14)[0]
        return out
    seqs, end = walk_chain(u, base)
    if seqs:
        out["layout"] = "chain"
        out["sequences"] = seqs
        out["parsed_to"] = hex(end)
        out["complete"] = end == len(u)
        return out
    cnt = struct.unpack_from("<I", u, 0)[0]
    ptrs = struct.unpack_from(f"<{min(cnt, 64)}I", u, 4) if 0 < cnt < 64 else ()
    if ptrs and all(base <= p < base + len(u) for p in ptrs):
        out["layout"] = "objects"
        out["objects"] = [hex(p - base) for p in ptrs]
    else:
        out["layout"] = "unknown"
    return out


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    json.dump([analyse(p) for p in sys.argv[1:]], sys.stdout, indent=1)
    print()


if __name__ == "__main__":
    main()
