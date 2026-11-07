#!/usr/bin/env python3
"""dc2inventory.py -- one CSV row per file of the Dino Crisis 2 PC data folder.

Usage: dc2inventory.py DATA_DIR > inventory.csv
Columns: file, size, layout (container / chain:N / pxl / other / empty),
record types (type:count), model (load address, parts, tris, quads, verts),
type-5 load addresses, notes.
"""
import collections, csv, glob, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dc2dat import split_chain      # noqa: E402
from dc2lzss import unpack          # noqa: E402
from dc2model import looks_like_model  # noqa: E402


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    w = csv.writer(sys.stdout)
    w.writerow(["file", "size", "layout", "record_types", "model", "type5_addresses", "notes"])
    for path in sorted(glob.glob(os.path.join(sys.argv[1], "*"))):
        name = os.path.basename(path).upper()
        d = open(path, "rb").read()
        layout, rtypes, model, addrs, notes = "other", "", "", set(), ""
        if not d:
            layout = "empty"
        elif name.endswith(".PXL") and len(d) == 320 * 240 * 2:
            layout = "pxl"
            notes = "raw 320x240 BGR555"
        else:
            chain = split_chain(d)
            if chain:
                layout = "container" if len(chain) == 1 else f"chain:{len(chain)}"
                c = collections.Counter()
                for base, recs in chain:
                    for r in recs:
                        c[r["type"]] += 1
                        if r["type"] == 5:
                            addrs.add(hex(r.get("arg", 0)))
                            if base == 0 and not model:
                                u = unpack(d[r["file_offset"]:r["file_offset"] + r["size"]])
                                if len(u) > 0x1C and looks_like_model(r["arg"], u):
                                    vp, np_, tp, qp, nt, nq = struct.unpack_from("<4IHH", u, 0)
                                    n = struct.unpack_from("<I", u, 0x14)[0]
                                    model = f"{r['arg']:#x} parts={n} tris={nt} quads={nq} verts={(np_ - vp) // 8}"
                rtypes = " ".join(f"{k}:{v}" for k, v in sorted(c.items()))
            elif name == "FILE.DAT":
                notes = "48 x (128x96 8bpp + CLUT)"
        w.writerow([name, len(d), layout, rtypes, model, " ".join(sorted(addrs)), notes])


if __name__ == "__main__":
    main()
