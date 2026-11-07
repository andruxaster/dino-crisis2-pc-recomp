#!/usr/bin/env python3
"""dc2model.py -- skeletal model blocks of Dino Crisis 2 PC.

Where: the LZSS type-5 record of E*.DAT (-> 0x633000) and KOF_P*.DAT
(-> 0x662500); Object_SetModel 0x48D8B0(obj, block) binds one to an object.
All pointers are absolute (record arg = load address).

MODEL header
  +0x00 SVECTOR *verts      (8 bytes: s16 x, y, z, pad), part-local coordinates
  +0x04 SVECTOR *normals    (same count/indices as verts, |n| = 4096)
  +0x08 TRI     *tris       +0x0C QUAD *quads
  +0x10 u16 ntris           +0x12 u16 nquads
  +0x14 u32 nparts          +0x18 u16 tpage  +0x1A u16 clut
  +0x1C PART[nparts]        20 bytes each:
        s16 joint[3] (offset from parent), s8 parent (-1 = root), u8 flag,
        TRI *tris, QUAD *quads, u16 ntris, u16 nquads  (contiguous slices)
TRI  (12 bytes): u16 i0, i1, i2; u8 u2, v2, u0, v0, u1, v1   (MeshDrawGT3 0x4834F0)
QUAD (16 bytes): u16 i0, i1, i2, i3; u8 u0, v0, u1, v1, u2, v2, u3, v3 (0x4832C0)
Unused vertex slots hold a filler pattern and are never indexed. Polygons on
part seams reference vertices of the neighbouring part.

Usage:
  dc2model.py FILE.DAT                 -> JSON summary + checks on stdout
  dc2model.py FILE.DAT --obj out.obj   -> also write a bind-pose OBJ
     (vertex = local + sum of joint offsets along the parent chain of the
      first part that uses it; no rotations; UVs in texture-page pixels / 256)
"""
import json, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dc2lzss import unpack          # noqa: E402
from dc2dat import parse_container  # noqa: E402


def looks_like_model(base, u):
    end = base + len(u)
    vp, np_, tp, qp, nt, nq = struct.unpack_from("<4IHH", u, 0)
    nparts = struct.unpack_from("<I", u, 0x14)[0]
    if not all(base <= p < end for p in (vp, np_, tp, qp)) or not (0 < nparts <= 64):
        return False
    if not (vp < np_ < tp <= qp) or tp - base + 12 * nt != qp - base:
        return False
    if 0x1C + 20 * nparts > len(u) or qp - base + 16 * nq > len(u):
        return False
    return True


def load_block(path):
    d = open(path, "rb").read()
    recs, _ = parse_container(d)
    for r in recs:
        if r["type"] == 5 and "psxseg_offset" in r:
            u = unpack(d[r["file_offset"]:r["file_offset"] + r["size"]])
            if len(u) > 0x1C and looks_like_model(r["arg"], u):
                return r["arg"], u
    raise SystemExit("no model block (type 5 with in-block pointers) found")


def parse(base, u):
    o = lambda a: a - base
    vp, np_, tp, qp, nt, nq = struct.unpack_from("<4IHH", u, 0)
    nparts = struct.unpack_from("<I", u, 0x14)[0]
    tpage, clut = struct.unpack_from("<HH", u, 0x18)
    nv = (o(np_) - o(vp)) // 8
    verts = [struct.unpack_from("<3h", u, o(vp) + 8 * i) for i in range(nv)]
    normals = [struct.unpack_from("<3h", u, o(np_) + 8 * i) for i in range(nv)]
    parts = []
    for i in range(nparts):
        x, y, z, par, fl, pt, pq, a, b = struct.unpack_from("<3hbBIIHH", u, 0x1C + 20 * i)
        tris = []
        for k in range(a):
            i0, i1, i2, u2, v2, u0, v0, u1, v1 = struct.unpack_from("<3H6B", u, o(pt) + 12 * k)
            tris.append({"idx": [i0, i1, i2], "uv": [[u0, v0], [u1, v1], [u2, v2]]})
        quads = []
        for k in range(b):
            q = struct.unpack_from("<4H8B", u, o(pq) + 16 * k)
            quads.append({"idx": list(q[:4]), "uv": [list(q[4 + 2 * j:6 + 2 * j]) for j in range(4)]})
        parts.append({"joint": [x, y, z], "parent": par, "flag": fl,
                      "tri_offset": hex(o(pt)), "quad_offset": hex(o(pq)),
                      "tris": tris, "quads": quads})
    used = sorted({i for p in parts for f in p["tris"] + p["quads"] for i in f["idx"]})
    checks = {
        "sum_tris_ok": sum(len(p["tris"]) for p in parts) == nt,
        "sum_quads_ok": sum(len(p["quads"]) for p in parts) == nq,
        "tri_list_ends_at_quads": o(tp) + 12 * nt == o(qp),
        "indices_in_range": all(i < nv for i in used),
        "used_vertices": len(used), "vertex_slots": nv,
    }
    return {"load_address": hex(base), "block_size": len(u), "tpage": hex(tpage), "clut": hex(clut),
            "ntris": nt, "nquads": nq, "parts": parts, "checks": checks,
            "_verts": verts, "_normals": normals, "_used": used}


def write_obj(m, path):
    world = []
    for p in m["parts"]:
        j = list(p["joint"])
        if p["parent"] >= 0:
            j = [j[k] + world[p["parent"]][k] for k in range(3)]
        world.append(j)
    owner = {}
    for pi, p in enumerate(m["parts"]):
        for f in p["tris"] + p["quads"]:
            for i in f["idx"]:
                owner.setdefault(i, pi)
    with open(path, "w") as fh:
        fh.write("# Dino Crisis 2 PC model, bind pose without rotations (unverified)\n")
        for i, v in enumerate(m["_verts"]):
            w = world[owner[i]] if i in owner else [0, 0, 0]
            fh.write(f"v {v[0] + w[0]} {-(v[1] + w[1])} {v[2] + w[2]}\n")
        for f in (f for p in m["parts"] for f in p["tris"] + p["quads"]):
            for uv in f["uv"]:
                fh.write(f"vt {uv[0] / 256:.5f} {1 - uv[1] / 256:.5f}\n")
        vt = 1
        for pi, p in enumerate(m["parts"]):
            fh.write(f"g part{pi:02d}\n")
            for f in p["tris"]:
                a, b, c = (i + 1 for i in f["idx"])
                fh.write(f"f {a}/{vt} {b}/{vt + 1} {c}/{vt + 2}\n")
                vt += 3
            for f in p["quads"]:  # PS1 quad order 0,1,2,3 = Z shape -> 0,1,3,2
                a, b, c, d = (i + 1 for i in f["idx"])
                fh.write(f"f {a}/{vt} {b}/{vt + 1} {d}/{vt + 3} {c}/{vt + 2}\n")
                vt += 4


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    base, u = load_block(sys.argv[1])
    m = parse(base, u)
    if "--obj" in sys.argv:
        write_obj(m, sys.argv[sys.argv.index("--obj") + 1])
    out = {k: v for k, v in m.items() if not k.startswith("_")}
    out["parts"] = [{k: (len(v) if k in ("tris", "quads") else v) for k, v in p.items()} for p in m["parts"]]
    json.dump(out, sys.stdout, indent=1)
    print()


if __name__ == "__main__":
    main()
