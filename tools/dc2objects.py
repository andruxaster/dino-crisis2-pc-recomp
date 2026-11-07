#!/usr/bin/env python3
"""dc2objects.py -- object types of Dino Crisis 2 PC and what each room places.

Object type = obj+0x58 (objects: G+0x940, 0x260 bytes each, slot 0 = player).
Per-area handler table *0x87DCA0 (28 entries, set by the area functions
0x48D170 + 0x40*area) is called with the object every frame.

Model loading, Enemy_LoadModel 0x482070(type, variant):
  types 2..15: file id = u16 at 0x703520 + 8*type (E*.DAT), plus a texture
    file chosen by variant for type 2 (0x703E48, E00/E01/E02/E03/E00_0x.TEX),
    type 7 (0x703E70, E90/E91/E90_91.TEX), type 6 (0x703E94, E40/E41.TEX);
  types >= 0x1D: KOF_P*.DAT (0x703598 + 8*(type-0x1D)), playable dinosaurs.
  Other E*.DAT carry their texture themselves.
  E<k>0.DAT is dinosaur k of the in-game dinosaur file list (text at file
  offset 0x10B988): sizes of the models agree with the species (below).
Other types: 0/1 player characters (handlers 0x47CE40 / 0x490120),
  16 room prop: a skeletal model stored in the room block (obj+0x60 = model
  pointer, handler 0x446170), 17..28 area-specific objects.

Where objects come from (all through the room script, dc2script.py):
  obj_spawn  (op 0x1A): slot, type, x, y, z, dir, data ptr, variant, ?, flag
  enemy_set  (op 0x4F): pointer to an ENEMY SET in the room block, kept in
    G+0xA68 and run by the spawn manager 0x472230 (random dinosaurs):
    +0 u8 zone count, +1 u8 enemy type, +2 u8 cap (raised on hard mode),
    +3 u8 room limit (0xFF none; compared with the per-room counter
    G+0x12D0[room]), +4 u8 variant (texture), +6 u8, +7 u8 state stored in
    G+0x132C[room]; zones from +0x10, 32 bytes each: s16 quad[4][2] (area
    the player is in, picked by 0x472C60), then spawn parameters
    (+0x14 u8, +0x16 u8 random range, +0x18 u16 delay; not fully decoded).
Room names: ROOM.p10 (g_areas) points to the room name in game text.

Usage: dc2objects.py DATA_DIR [--json]   (all rooms of the area table)
"""
import json, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dc2room import room_block, BASE        # noqa: E402
from dc2script import Script, decode_text   # noqa: E402

SPECIES = ["Velociraptor", "Tyrannosaurus Rex", "Pteranodon", "Plesiosaurus", "Giganotosaurus",
           "Inostrancevia", "Allosaurus", "Triceratops", "Mosasaurus", "Oviraptor", "Compsognathus"]
# type -> (file, note); file ids from 0x703520 / 0x703598
TYPES = {
    0: ("-", "player character"), 1: ("-", "second player character"),
    2: ("E00.DAT", "Velociraptor (textures E00..E03.TEX by variant)"),
    3: ("E10.DAT", "Tyrannosaurus Rex"), 4: ("E20.DAT", "Pteranodon"),
    5: ("E30.DAT", "Plesiosaurus"), 6: ("E40.DAT", "Giganotosaurus (E40/E41.TEX)"),
    7: ("E90.DAT", "Oviraptor (E90/E91.TEX)"), 8: ("E60.DAT", "Allosaurus"),
    9: ("E70.DAT", "Triceratops"), 10: ("E80.DAT", "Mosasaurus"),
    11: ("E31.DAT", "Plesiosaurus part/variant"), 12: ("E32.DAT", "Plesiosaurus part/variant"),
    13: ("EA0.DAT", "Compsognathus"), 14: ("E50.DAT", "Inostrancevia"),
    15: ("KOF_P00P.DAT", "playable Velociraptor"),
    16: ("room block", "room prop (model in the room block)"),
}
for i, (f, n) in enumerate([("KOF_P00P", "Velociraptor"), ("KOF_P01P", "Velociraptor"),
                            ("KOF_P90P", "Oviraptor"), ("KOF_P91P", "Oviraptor"),
                            ("KOF_P60P", "Allosaurus"), ("KOF_P61P", "Allosaurus"),
                            ("KOF_P10P", "Tyrannosaurus Rex"), ("KOF_P11P", "Tyrannosaurus Rex"),
                            ("KOF_P70P", "Triceratops"), ("KOF_P71P", "Triceratops"),
                            ("KOF_PA0P", "Compsognathus"), ("KOF_PA1P", "Compsognathus"),
                            ("KOF_P50P", "Inostrancevia"), ("KOF_P51P", "Inostrancevia")]):
    TYPES[0x1D + i] = (f + ".DAT", "playable " + n)


def type_name(t):
    if t is None:
        return "?"
    return TYPES.get(t, ("?", "area object %d" % t))[1]


def enemy_set(blk, ptr):
    """ENEMY SET (script op 0x4F), run by EnemySet_Tick 0x472230"""
    o = ptr - BASE
    if not 0 <= o < len(blk) - 16:
        return None
    h = blk[o:o + 16]
    zones = []
    for k in range(h[0]):
        z = o + 0x10 + 32 * k
        q = struct.unpack_from("<8h", blk, z)
        off, npt, b13, alive, total, rnd = struct.unpack_from("<HBBBBB", blk, z + 16)
        delay = struct.unpack_from("<H", blk, z + 24)[0]
        pts = []
        for i in range(npt):
            p = o + off + 16 * i
            x, y, zz, d = struct.unpack_from("<4h", blk, p)
            pts.append({"pos": [x, y, zz], "dir": d, "enter": blk[p + 8], "b9": blk[p + 9],
                        "ai": blk[p + 10], "timer": blk[p + 12]})
        zones.append({"quad": [list(q[i:i + 2]) for i in range(0, 8, 2)], "b13": b13,
                      "max_alive": alive, "max_spawns": total, "random_alive": rnd,
                      "delay": delay, "points": pts})
    return {"ptr": hex(ptr), "type": h[1], "enemy": type_name(h[1]), "cap": h[2],
            "room_limit": None if h[3] == 0xFF else h[3], "variant": h[4], "pick": h[6], "b7": h[7],
            "zones": zones}


TEX = {2: ["E00.TEX", "E00.TEX", "E00.TEX", "E02.TEX", "E00_02.TEX", "E02.TEX", "E03.TEX", "E00_03.TEX", "E03.TEX", "E01.TEX"],
       7: ["E90.TEX", "E90_91.TEX", "E91.TEX", "E90.TEX", "E90_91.TEX", "E91.TEX", "E90.TEX", "E90_91.TEX", "E91.TEX"],
       6: ["E40.TEX", "E41.TEX"]}


def viewer_args(t, variant, pos=None):
    """dc2view arguments to look at this enemy (model file + texture file)"""
    if t not in TYPES or not 2 <= t <= 15:
        return None
    a = "--model %s" % TYPES[t][0][:-4]
    if t in TEX:
        v = variant if isinstance(variant, int) and 0 <= variant < len(TEX[t]) else 0
        a += " --vram %s" % TEX[t][v]
    if pos and None not in pos:
        a += " --pos %d %d" % (pos[0], pos[2])
    return a


def room_list(exe=None):
    """(area, room, name) from dc2_rooms.csv"""
    import csv
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "dc2_rooms.csv")
    return [(int(r["area"]), int(r["room"]), r["dat"][:-4], r.get("name", "")) for r in csv.DictReader(open(path))]


def analyse(data_dir, name):
    blk = room_block(open(os.path.join(data_dir, name + ".DAT"), "rb").read())
    sc = Script(blk)
    _, calls = sc.listing()
    out = {"room": name, "spawns": [], "enemy_sets": [], "file_loads": []}
    seen = set()
    for pc, op, a in calls:
        if op == 0x1A:
            out["spawns"].append({"pc": hex(pc), "slot": a[0], "type": a[1], "what": type_name(a[1]),
                                  "pos": a[2:5], "dir": a[5],
                                  "data": hex(a[6]) if isinstance(a[6], int) and a[6] > 0xFFFF else a[6],
                                  "variant": a[7], "flag": a[9], "dc2view": viewer_args(a[1], a[7], a[2:5])})
        elif op == 0x4F and a[0] and a[0] not in seen:
            seen.add(a[0])
            e = enemy_set(blk, a[0])
            if e:
                e["dc2view"] = viewer_args(e["type"], e["variant"])
                out["enemy_sets"].append(e)
        elif op == 0x23:
            out["file_loads"].append({"type": a[0], "what": type_name(a[0]) if isinstance(a[0], int) and a[0] < 0x500000 else "?",
                                      "variant": a[1]})
    return out


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    d = sys.argv[1]
    res = []
    for area, room, name, title in room_list():
        if os.path.exists(os.path.join(d, name + ".DAT")):
            r = analyse(d, name)
            r["area"] = area
            r["name"] = title
            res.append(r)
    if "--json" in sys.argv:
        json.dump({"types": {str(k): v for k, v in sorted(TYPES.items())}, "rooms": res}, sys.stdout, indent=1)
        print()
        return
    for r in res:
        en = sorted({e["enemy"] for e in r["enemy_sets"]} | {s["what"] for s in r["spawns"] if s["type"] in range(2, 16)})
        props = sum(1 for s in r["spawns"] if s["type"] == 16)
        en = [e.split(" (")[0] for e in en]
        print("%-6s %-42s enemies: %-36s props %2d  other: %s" % (
            r["room"], r["name"][:42], ", ".join(en) or "-", props,
            ", ".join(sorted({str(s["type"]) for s in r["spawns"] if s["type"] not in range(2, 17)})) or "-"))


if __name__ == "__main__":
    main()
