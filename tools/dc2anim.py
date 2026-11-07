#!/usr/bin/env python3
"""dc2anim.py -- animation sequences of Dino Crisis 2 PC model blocks.

Location: inside the model block (see dc2model.py), right after the quad
list; player blocks (WEP_P*) pad with 0xCD up to offset 0x3800 first. The
game stores the table address in object +0x88 (e.g. 0x665D00 for WEP_P*).
Table: u32 absolute pointers (0 = empty slot), ends where the first sequence
starts.

SEQUENCE (Anim_Set 0x48DDE0, Anim_Start 0x48DEF0)
  +0x00 u16 frame count (u16 at +2 unused here)
  +0x04 ptr -> object +0x9C   +0x08 ptr -> +0xA0   +0x0C ptr -> +0xA4
        (small per-sequence tables, stored after the frames; meaning TBD)
  +0x10 u32
  +0x14 FRAME[count], stride = align4(16 + 6 * nparts)
FRAME (Anim_ApplyFrame 0x48E3D0)
  +0x00 s16 unknown
  +0x02 s16 duration in ticks (t = elapsed / duration)
  +0x04 s16[3] channel 0: root position          -> part0 +0x64
  +0x0A s16[3] channel 1: object rotation        -> object +0xC0
  +0x10 s16[3] per part: rotation (4096 = 360 deg) -> part +0x48
Playback interpolates cubically over 4 keyframes (object +0x78..+0x84);
angles wrap mod 0x1000 along the short way. Part matrix = RotMatrixYXZ(rot)
(0x48FF90); 0x48FEA0 cross-fades two poses with LoadAverageShort0.
Sequences whose frames fit a different part count also exist: e.g. 15-part
(human) sequences stored inside enemy and door files.

Usage: dc2anim.py FILE.DAT [--frames] > anims.json
"""
import json, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dc2model import load_block  # noqa: E402


def anim_table(base, u):
    vp, np_, tp, qp, nt, nq = struct.unpack_from("<4IHH", u, 0)
    o = qp - base + 16 * nq
    while o < len(u) and u[o] == 0xCD:
        o += 1
    o = (o + 3) & ~3
    ents, i, lim = [], o, len(u)
    while i + 4 <= lim:
        v = struct.unpack_from("<I", u, i)[0]
        if v == 0:
            ents.append(None)
        elif base <= v < base + len(u):
            ents.append(v - base)
            lim = min(lim, v - base)
        else:
            break
        i += 4
    return o, ents


def stride(parts):
    return (16 + 6 * parts + 3) & ~3


def best_fit(nf, span, own):
    best = None
    for p in [own] + [x for x in range(1, 32) if x != own]:
        r = span - 0x14 - nf * stride(p)
        if nf and 0 <= r < 0x40 and (best is None or r < best[1] or (r == best[1] and p == own)):
            best = (p, r)
    return best


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    base, u = load_block(sys.argv[1])
    nparts = struct.unpack_from("<I", u, 0x14)[0]
    table_off, ents = anim_table(base, u)
    valid = sorted({e for e in ents if e is not None})
    ends = valid + [len(u)]
    seqs = []
    for idx, a in enumerate(ents):
        if a is None:
            seqs.append({"index": idx, "empty": True})
            continue
        nf = struct.unpack_from("<H", u, a)[0]
        span = next(e for e in ends if e > a) - a
        fit = best_fit(nf, span, nparts)
        s = {"index": idx, "offset": hex(a), "frames": nf, "span": span,
             "tables": [hex(x - base) if x else None for x in struct.unpack_from("<3I", u, a + 4)],
             "parts": fit[0] if fit else None, "tail_bytes": fit[1] if fit else None}
        if fit:
            st, p = stride(fit[0]), fit[0]
            durs, frames = [], []
            for k in range(nf):
                f = a + 0x14 + k * st
                h = struct.unpack_from("<8h", u, f)
                durs.append(h[1])
                if "--frames" in sys.argv:
                    rots = [list(struct.unpack_from("<3h", u, f + 0x10 + 6 * j)) for j in range(p)]
                    frames.append({"h0": h[0], "duration": h[1], "root": list(h[2:5]),
                                   "obj_rot": list(h[5:8]), "rot": rots})
            s["durations"] = durs
            s["total_ticks"] = sum(durs)
            if frames:
                s["frame_data"] = frames
        seqs.append(s)
    out = {"load_address": hex(base), "nparts": nparts, "frame_stride": stride(nparts),
           "table_offset": hex(table_off), "slots": len(ents),
           "sequences_ok": sum(1 for s in seqs if s.get("parts")),
           "sequences_unmatched": sum(1 for s in seqs if not s.get("empty") and not s.get("parts")),
           "sequences": seqs}
    json.dump(out, sys.stdout, indent=1)
    print()


if __name__ == "__main__":
    main()
