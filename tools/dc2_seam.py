#!/usr/bin/env python3
"""dc2_seam.py — list the PC-layer functions of Dino2.exe that game code calls.

Region map (Dino2.exe, 2002-02-05 build, MSVC 6.0):
  0x401000-0x46F000  game logic A
  0x46F000-0x471000  Win32 shell (window, dino2.cfg, message loop)
  0x471000-0x4B4000  game logic B
  0x4B4000-0x4DD5C0  PC layer (DirectDraw4/D3D3, DSound, DInput, PS1 GPU/GTE emulation)
  0x4DD5C0-          MSVC CRT
Function starts are found by linear sweep: 16-byte aligned addresses that
follow a ret/jmp and nop padding. That boundary can over-split a function
that has an internal tail jump.

Usage: dc2_seam.py Dino2.exe out.csv [dc2_labels.csv]
Requires: pip install pefile capstone
"""
import bisect, collections, csv, re, struct, sys
import capstone, pefile

REGIONS = [("game_a", 0x401000, 0x46F000), ("win32", 0x46F000, 0x471000),
           ("game_b", 0x471000, 0x4B4000), ("pc_layer", 0x4B4000, 0x4DD5C0),
           ("crt", 0x4DD5C0, 0x4E8000)]


# Identified by hand. The cooperative task scheduler (hand-written asm, no
# padding between entry points): 4 slots of 0x20 bytes at 0x73C870
# {u16 state 0=free 1=sleeping 2=run; u16 wait; u32 saved_esp; ...},
# 0x4000-byte stack per task, cursor at 0x73C8F0, main esp at 0x73C8F4.
KNOWN = {
    0x4C2900: ("Task_RunAll", "per frame: resumes each live slot; sleep counter decrements"),
    0x4C2967: ("Task_InitStackBase", "stack base = esp - 0x4000 -> 0x73C8F8"),
    0x4C2974: ("Task_Start", "(slot, func): state=2, stack = base - slot*0x4000"),
    0x4C29A9: ("Task_Sleep", "(frames): state=1, saves regs+esp, back to scheduler"),
    0x4C29D6: ("Task_Chain", "(func): restarts current slot on fresh stack with func"),
    0x4C2A0D: ("Task_Kill", "(slot): clears slot"),
    # PsyQ-style libgpu/libgte reimplementation (tentative names)
    0x4C05D0: ("AddPrim?", "links prim into OT: *ot=prim, prim tag=next|0x80000000 flag (PC variant of 24-bit tag)"),
    0x4C0FA0: ("RotMatrix?", "copies identity from 0x73C050, then 0x4C0F30/0x4C0EC0/0x4C0E50 = Z/Y/X rotations from SVECTOR"),
    0x4C06F0: ("SetDrawMode?", "fills DR_MODE-like packet, PC code 0xE8, flags dfe/dtd, tpage"),
    0x4C0B20: ("MulMatrix0?", "integer 3x3 matrix multiply on s16 MATRIX"),
}


def region(a):
    for n, lo, hi in REGIONS:
        if lo <= a < hi:
            return n
    return "?"


def main(exe, out, labels=None):
    pe = pefile.PE(exe)
    base = pe.OPTIONAL_HEADER.ImageBase
    t = pe.sections[0]
    code = t.get_data()[:t.Misc_VirtualSize]
    tva = base + t.VirtualAddress
    tend = tva + len(code)
    img = pe.get_memory_mapped_image()
    imports = {i.address: f"{e.dll.decode().split('.')[0]}!{i.name.decode() if i.name else '#%d' % i.ordinal}"
               for e in pe.DIRECTORY_ENTRY_IMPORT for i in e.imports}
    psx_lo, psx_hi = 0x4E8000, 0x4E8000 + 0x21B000

    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.skipdata = True
    insns = list(md.disasm(code, tva))

    starts, prev_term = [], True
    for i in insns:
        if i.mnemonic in ("nop", "int3") or (i.mnemonic == "lea" and i.op_str in ("esi, [esi]", "edi, [edi]")):
            continue
        if prev_term and i.address % 16 == 0:
            starts.append(i.address)
        prev_term = i.mnemonic in ("ret", "jmp", ".byte")
    ends = starts[1:] + [tend]
    fn_of = lambda a: starts[bisect.bisect_right(starts, a) - 1]

    def cstr(va):
        o = va - base
        if not (0 <= o < len(img)):
            return None
        e = img.find(b"\0", o, o + 120)
        s = img[o:e] if e > o else b""
        if len(s) >= 4 and all(32 <= c < 127 for c in s):
            return s.decode()
        return None

    st = collections.defaultdict(lambda: {"x87": 0, "com": 0, "imports": set(), "strings": set(),
                                          "callees": set(), "psx": 0, "insns": 0})
    sites = collections.defaultdict(list)
    for i in insns:
        f = fn_of(i.address)
        s = st[f]
        s["insns"] += 1
        m, ops = i.mnemonic, i.op_str
        if m.startswith("f"):
            s["x87"] += 1
        if m == "call" and re.match(r"dword ptr \[e[a-z]{2} \+ 0x", ops):
            s["com"] += 1
        for tok in re.findall(r"0x[0-9a-f]+", ops):
            v = int(tok, 16)
            if v in imports:
                s["imports"].add(imports[v])
            elif psx_lo <= v < psx_hi:
                s["psx"] += 1
            elif base + 0x303000 <= v < base + 0x482000:
                c = cstr(v)
                if c:
                    s["strings"].add(c)
        if m == "call":
            try:
                x = int(ops, 16)
            except ValueError:
                continue
            if tva <= x < tend:
                s["callees"].add(x)
                if region(i.address).startswith("game") and region(x) == "pc_layer":
                    sites[x].append(i.address)

    by_addr = {i.address: k for k, i in enumerate(insns)}
    rows = []
    for f, cs in sites.items():
        callers = sorted({fn_of(a) for a in cs})
        if f in by_addr and f not in set(starts):
            # mid-block asm entry: measure linearly up to the first ret/jmp
            s = {"x87": 0, "com": 0, "imports": set(), "strings": set(), "callees": set(),
                 "psx": 0, "insns": 0}
            k = by_addr[f]
            while k < len(insns):
                s["insns"] += 1
                if insns[k].mnemonic in ("ret", "jmp"):
                    break
                k += 1
            size = insns[k].address + insns[k].size - f
        else:
            s = st[f]
            size = ends[bisect.bisect_left(starts, f)] - f
        imps = sorted(s["imports"])
        hint = ("task scheduler (asm)" if f in KNOWN else
                "ddraw/d3d" if any(k.startswith("DDRAW") for k in imps) or s["com"] > 3 else
                "math/geometry" if s["x87"] > 20 and s["com"] == 0 else
                "gdi/text" if any(k.startswith("GDI32") for k in imps) else "")
        rows.append({
            "address": f"0x{f:06X}", "call_sites": len(cs), "distinct_callers": len(callers),
            "size_bytes": size, "instructions": s["insns"], "x87_ops": s["x87"],
            "com_vcalls": s["com"], "psxseg_refs": s["psx"], "callees": len(s["callees"]),
            "imports": " ".join(imps), "strings": " | ".join(sorted(s["strings"]))[:200],
            "hint": hint,
            "sample_callers": " ".join(f"0x{c:06X}" for c in callers[:6]),
            "name": KNOWN.get(f, ("", ""))[0], "notes": KNOWN.get(f, ("", ""))[1]})
    if labels:
        lab = {int(r["address"], 16): r for r in csv.DictReader(open(labels))}
        for r in rows:
            l = lab.get(int(r["address"], 16))
            if l:
                r["name"], r["hint"] = l["name"], l["category"]
                r["notes"] = f"[{l['confidence']}] {l['notes']}".strip()
    rows.sort(key=lambda r: -r["call_sites"])
    with open(out, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(rows)
    print(f"{len(rows)} seam functions, {sum(r['call_sites'] for r in rows)} call sites -> {out}")


if __name__ == "__main__":
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    main(*sys.argv[1:])
