#!/usr/bin/env python3
"""Map functions of the PC version (Dino2.exe) to the PS1 version
(SLUS_012.79 and its code overlays *.BIN, all in one folder).

The PC game code is a port of the PS1 code: the functions correspond almost
one to one and use the same structure layouts. Starting from anchors (the
per-area function tables, which have the same shape on both), callee lists
of paired functions are aligned and the pairs propagated through the call
graph. Pairs are scored by size and by the structure offsets / constants the
two functions use.

  usage: dc2_psx_map.py game/Dino2.exe game/PSX [-o tools/dc2_psx_map.csv]

PS1 overlays (load addresses in PSX.BASES): ST0..9 areas (0x800D5800),
E00..EA0 one enemy kind each (0x800D0000; the area tables' type slots
2.. point at fixed entry offsets, the loaded E file supplies the code),
WEPxx weapons, menus (SAVE, OPTION, SUBSCRx, RESxx, LOAD), title/result.

Output CSV: pc address, PS1 address, where the PS1 code lives (exe or the
overlay's name), score (0..1: shared structure offsets and constants,
size), callees (paired callees that are also callees of the partner /
paired callees), name (from dc2_labels.csv). Needs capstone.
Only addresses are written; nothing of the game itself."""
import argparse, csv, os, struct, sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_32, CS_ARCH_MIPS, CS_MODE_MIPS32, CS_MODE_LITTLE_ENDIAN
from capstone.x86 import X86_OP_IMM, X86_OP_MEM

# ---------------------------------------------------------------- PC side
class PC:
    LO, HI = 0x401000, 0x4E8000
    GAME = ((0x401000, 0x46F000), (0x471000, 0x4B4000))
    PCL = (0x4B4000, 0x4CD000)

    def __init__(self, path):
        d = open(path, 'rb').read()
        pe = struct.unpack_from('<I', d, 0x3C)[0]
        nsec, optsz = struct.unpack_from('<HH', d, pe + 6)[0], struct.unpack_from('<H', d, pe + 20)[0]
        self.base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
        self.secs = []
        for i in range(nsec):
            s = pe + 24 + optsz + 40 * i
            va, vs = struct.unpack_from('<II', d, s + 12)[0], struct.unpack_from('<I', d, s + 8)[0]
            rs, ro = struct.unpack_from('<II', d, s + 16)
            self.secs.append((self.base + va, max(vs, rs), d[ro:ro + rs]))
        self.md = Cs(CS_ARCH_X86, CS_MODE_32); self.md.detail = True
        self.cache = {}

    def rd(self, a, n):
        for va, vs, raw in self.secs:
            if va <= a < va + vs:
                o = a - va
                return (raw[o:o + n] + b'\0' * n)[:n]
        return b'\0' * n

    def u32(self, a): return struct.unpack('<I', self.rd(a, 4))[0]
    def is_code(self, a): return self.LO <= a < self.HI

    def func(self, addr):
        """(size, ordered direct callees, features)"""
        if addr in self.cache: return self.cache[addr]
        todo, seen, calls, feat, hi = [addr], set(), [], set(), addr
        refs = []
        while todo:
            a = todo.pop()
            while self.is_code(a) and a not in seen:
                ins = next(self.md.disasm(self.rd(a, 16), a), None)
                if ins is None: break
                seen.add(a); hi = max(hi, a + ins.size)
                m = ins.mnemonic
                for op in ins.operands:
                    if op.type == X86_OP_MEM and op.mem.disp >= 0x4E8000 and op.mem.disp < 0x900000: refs.append((a, op.mem.disp))
                    elif op.type == X86_OP_IMM and 0x4E8000 <= op.imm < 0x900000: refs.append((a, op.imm))
                    if op.type == X86_OP_MEM and op.mem.base and ins.reg_name(op.mem.base) not in ('esp', 'ebp') \
                            and 0 < op.mem.disp < 0x2000: feat.add(('o', op.mem.disp))
                    elif op.type == X86_OP_IMM and 0x10 < op.imm < 0x10000 and m not in ('call', 'push') and not m.startswith('j'):
                        feat.add(('i', op.imm))
                if m == 'call':
                    op = ins.operands[0]
                    if op.type == X86_OP_IMM: calls.append((a, op.imm))
                elif m.startswith('j'):
                    op = ins.operands[0]
                    if op.type == X86_OP_IMM:
                        t = op.imm
                        if m == 'jmp' and not (addr <= t < addr + 0x4000): calls.append((a, t)); break
                        todo.append(t)
                        if m == 'jmp': break
                    elif op.type == X86_OP_MEM and op.mem.disp and op.mem.index:
                        for k in range(64):
                            v = self.u32(op.mem.disp + 4 * k)
                            if not (addr <= v < addr + 0x4000): break
                            todo.append(v)
                        break
                    else: break
                elif m in ('ret', 'int3'): break
                a += ins.size
        r = (len(seen), [t for _, t in sorted(calls)], feat, [t for _, t in sorted(refs)])
        self.cache[addr] = r
        return r

# --------------------------------------------------------------- PS1 side
class PSX:
    # overlays and where the PS1 version loads them (file table at 0x80019460
    # and jump tables inside the overlays)
    BASES = [('ST%d' % n, 0x800D5800) for n in range(10)] + \
            [(n, 0x800D0000) for n in ('E00', 'E10', 'E20', 'E30', 'E31', 'E32', 'E40', 'E50', 'E60', 'E70',
                                       'E80', 'E90', 'EA0')] + \
            [(n, 0x800D5800) for n in ('LOGO', 'M_TITLE', 'TITLE2', 'OPENING', 'ENDING', 'M_RESULT')] + \
            [(n, 0x801C1500) for n in ('LOAD', 'SAVE', 'OPTION', 'SUBSCR3', 'SUBSCR6')] + \
            [(n, 0x80150000) for n in ('RES00', 'RES01', 'RES02')] + \
            [('WEP%02X' % n, 0x8017E500) for n in range(0x0D)] + \
            [('WEP%02X' % n, 0x80120000) for n in range(0x0D, 0x14)]

    def __init__(self, folder):
        d = open(os.path.join(folder, 'SLUS_012.79'), 'rb').read()
        load, size = struct.unpack_from('<I', d, 0x18)[0], struct.unpack_from('<I', d, 0x1C)[0]
        self.exe = (load, d[0x800:0x800 + size])
        self.ovl = {}
        for name, base in self.BASES:
            p = os.path.join(folder, name + '.BIN')
            if os.path.exists(p): self.ovl[name] = (base, open(p, 'rb').read())
        self.md = Cs(CS_ARCH_MIPS, CS_MODE_MIPS32 + CS_MODE_LITTLE_ENDIAN); self.md.detail = True
        self.cache = {}

    def mem(self, where):
        return self.exe if where == 'exe' else self.ovl[where]

    def where(self, a, ctx=None):
        b, d = self.exe
        if b <= a < b + len(d): return 'exe'
        if ctx in self.ovl:
            b, d = self.ovl[ctx]
            if b <= a < b + len(d): return ctx
        return None

    def candidates(self, a):
        """overlays that have a function start at a"""
        out = []
        for name, (b, d) in self.ovl.items():
            o = a - b
            if 0 <= o <= len(d) - 4 and (struct.unpack_from('<I', d, o)[0] >> 16) == 0x27BD: out.append(name)
        return out

    def u32(self, where, a):
        b, d = self.mem(where)
        o = a - b
        return struct.unpack_from('<I', d, o)[0] if 0 <= o <= len(d) - 4 else 0

    def func(self, where, addr):
        key = (where, addr)
        if key in self.cache: return self.cache[key]
        b, d = self.mem(where)
        todo, seen, calls, feat, refs = [addr], set(), [], set(), []
        while todo:
            a = todo.pop()
            stop = None
            hi = {}
            while b <= a < b + len(d) - 4 and a not in seen:
                ins = next(self.md.disasm(d[a - b:a - b + 4], a), None)
                seen.add(a)
                if ins is None:                 # GTE (cop2) instructions are not decoded
                    a += 4
                    if stop is not None and a >= stop: break
                    continue
                m, ops = ins.mnemonic, ins.operands
                w = struct.unpack_from('<I', d, a - b)[0]
                op6, rs, rt, imm = w >> 26, (w >> 21) & 31, (w >> 16) & 31, w & 0xFFFF
                simm = imm - 0x10000 if imm & 0x8000 else imm
                if op6 == 0xF: hi[rt] = imm << 16
                elif op6 in (9, 0x23, 0x21, 0x25, 0x20, 0x24) and rs in hi:
                    v = (hi[rs] + simm) & 0xFFFFFFFF
                    if 0x80010000 <= v < 0x80200000: refs.append((a, v))
                    hi.pop(rt, None)
                elif op6 == 0 and (w & 0x3F) == 0x21:       # addu rd, rs, rt: keep the table base
                    rd = (w >> 11) & 31
                    if rs in hi: hi[rd] = hi[rs]
                    elif rt in hi: hi[rd] = hi[rt]
                    else: hi.pop(rd, None)
                elif op6 not in (0x2B, 0x29, 0x28):           # other writes to rt clear it
                    hi.pop(rt, None)
                if m in ('lw', 'sw', 'lh', 'lhu', 'sh', 'lb', 'lbu', 'sb', 'lwl', 'lwr', 'swl', 'swr') and ops[-1].type == 3:
                    disp = ops[-1].mem.disp
                    if 0 < disp < 0x2000 and ins.reg_name(ops[-1].mem.base) not in ('sp', 'fp'):
                        feat.add(('o', disp))
                elif m in ('addiu', 'slti', 'sltiu', 'andi', 'ori', 'xori', 'li') and ops and ops[-1].type == 2:
                    v = ops[-1].imm & 0xFFFF
                    if 0x10 < v < 0x8000 and not (m == 'addiu' and ins.op_str.startswith('$sp')): feat.add(('i', v))
                if m == 'jal': calls.append((a, ops[0].imm))
                elif m == 'j':
                    t = ops[0].imm
                    if addr <= t < addr + 0x4000: todo.append(t)
                    else: calls.append((a, t))
                    stop = a + 8
                elif m.startswith('b') and ops and ops[-1].type == 2 and m not in ('break',):
                    todo.append(ops[-1].imm)
                    if m == 'b' or (m == 'beq' and ins.op_str.startswith('$zero, $zero')): stop = a + 8
                elif m == 'jr':
                    stop = a + 8
                if stop is not None and a + 4 >= stop: break
                a += 4
                if stop is not None and a >= stop: break
        r = (len(seen), [t for _, t in sorted(calls)], feat, [t for _, t in sorted(refs)])
        self.cache[key] = r
        return r

# ------------------------------------------------------------------ match
PC_AREA_TABS = [  # Area0..9_SetTables (0x48D170 + 0x40 n): 0x87DCA0, 0x87F948, 0x87DC8C, 0x87F320
    (0x738DDC, 0x738E4C, 0x738E94, 0x738EE8), (0x738F08, 0x738F78, 0x738FCC, 0x739020),
    (0x739064, 0x7390D4, 0x73911C, 0x739174), (0x7391B8, 0x739228, 0x739274, 0x7392C8),
    (0x73930C, 0x739380, 0x7393CC, 0x739424), (0x739468, 0x7394D8, 0x73951C, 0x739568),
    (0x7395B0, 0x73961C, 0x739660, 0x7396AC), (0x7396CC, 0x73973C, 0x739784, 0x7397DC),
    (0x739800, 0x739870, 0x7398C8, 0x739924), (0x739964, 0x739A14, 0x739A60, 0x739ABC)]

def psx_area_tables(px, n):
    """pointers stored by the overlay's area set-up (G+0x21C = area)"""
    d = px.ovl[n][1]; w = struct.unpack_from('<%dI' % (len(d) // 4), d, 0)
    st = next(i for i in range(len(w)) if (w[i] & 0xFFFF) == 0x21C and (w[i] >> 26) == 0x29)
    out = []
    for i in range(max(0, st - 8), min(st + 40, len(w) - 2)):
        if (w[i] >> 26) == 0xF and (w[i] & 0xFFFF) in (0x800D, 0x800E):
            rt = (w[i] >> 16) & 31
            for j in (i + 1, i + 2):
                if (w[j] >> 26) == 9 and ((w[j] >> 21) & 31) == rt:
                    lo = w[j] & 0xFFFF
                    out.append((((w[i] & 0xFFFF) << 16) + lo - (0x10000 if lo & 0x8000 else 0)) & 0xFFFFFFFF)
                    break
        if w[i] == 0x03E00008 and i > st: break
    return out[1:5]          # the first is a data table the PC version keeps elsewhere

def jacc(a, b):
    return len(a & b) / max(1, len(a | b))

def score(pcf, psf):
    n1, _, f1 = pcf[:3]; n2, _, f2 = psf[:3]
    ratio = min(n1, n2) / max(1, max(n1, n2))       # instruction counts (x86 ~ MIPS within 2x)
    return 0.5 * jacc(f1, f2) + 0.5 * ratio

def align(a, b, sim):
    """Needleman-Wunsch over two callee lists; returns matched index pairs"""
    n, m = len(a), len(b)
    S = [[0.0] * (m + 1) for _ in range(n + 1)]
    for i in range(n):
        for j in range(m):
            S[i + 1][j + 1] = max(S[i][j + 1], S[i + 1][j], S[i][j] + sim(a[i], b[j]))
    i, j, out = n, m, []
    while i and j:
        s = sim(a[i - 1], b[j - 1])
        if S[i][j] == S[i - 1][j - 1] + s and s > 0: out.append((i - 1, j - 1)); i -= 1; j -= 1
        elif S[i][j] == S[i - 1][j]: i -= 1
        else: j -= 1
    return out[::-1]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe'); ap.add_argument('psx')
    ap.add_argument('-o', default=os.path.join(os.path.dirname(__file__), 'dc2_psx_map.csv'))
    a = ap.parse_args()
    pc, px = PC(a.exe), PSX(a.psx)
    names = {}
    lab = os.path.join(os.path.dirname(__file__), 'dc2_labels.csv')
    if os.path.exists(lab):
        for r in csv.DictReader(open(lab)):
            try: names[int(r['address'], 16)] = r['name']
            except ValueError: pass

    pairs, rev, todo = {}, {}, []           # pc -> (where, ps, score)

    def add(p, where, s, sc):
        if p in pairs or (where, s) in rev: return
        if not pc.is_code(p): return
        pairs[p] = (where, s, sc); rev[(where, s)] = p; todo.append(p)

    def best_cand(x, t):
        c = [(score(pc.func(x), px.func(w, t)), w) for w in px.candidates(t)]
        return max(c) if c else (0, None)

    flat = sorted(t for tt in PC_AREA_TABS for t in tt)
    for n, tabs in enumerate(PC_AREA_TABS):
        ov = 'ST%d' % n
        if ov not in px.ovl: continue
        for pt, st in zip(tabs, psx_area_tables(px, ov)):
            nxt = next((t for t in flat if t > pt), pt + 0x80)
            for k in range((nxt - pt) // 4):
                v1 = pc.u32(pt + 4 * k)
                where = px.where(st + 4 * k, ov)
                if where is None: break
                v2 = px.u32(where, st + 4 * k)
                if not pc.is_code(v1): continue
                w2 = px.where(v2, ov)
                if w2 is not None: add(v1, w2, v2, 1.0)
                else:                                       # enemy code: one of the E overlays
                    sc, w2 = best_cand(v1, v2)
                    if w2 is not None and sc >= 0.3: add(v1, w2, v2, sc)

    # whole-program call graphs for the upward step
    pc_funcs = set(names) | {p for p in pairs}
    pc_funcs = {f for f in pc_funcs if any(lo <= f < hi for lo, hi in PC.GAME)}
    def in_game(t): return any(lo <= t < hi for lo, hi in PC.GAME)
    for va, vs, raw in pc.secs:                       # call targets and code pointers in data
        for i in range(0, len(raw) - 4):
            if va < 0x4E8000 and raw[i] == 0xE8:
                t = (va + i + 5 + struct.unpack_from('<i', raw, i + 1)[0]) & 0xFFFFFFFF
                if in_game(t) and t & 0xF == 0: pc_funcs.add(t)
            elif va >= 0x4E8000 and i % 4 == 0:
                t = struct.unpack_from('<I', raw, i)[0]
                if in_game(t) and t & 0xF == 0: pc_funcs.add(t)
    work = list(pc_funcs)
    while work:
        f = work.pop()
        for t in pc.func(f)[1]:
            if any(lo <= t < hi for lo, hi in PC.GAME) and t not in pc_funcs: pc_funcs.add(t); work.append(t)
    pc_callers = {}
    for f in pc_funcs:
        for t in pc.func(f)[1]: pc_callers.setdefault(t, set()).add(f)
    ps_funcs = set()
    for where in ['exe'] + sorted(px.ovl):
        b, d = px.mem(where)
        for i in range(0, len(d) - 4, 4):
            w = struct.unpack_from('<I', d, i)[0]
            if (w >> 16) == 0x27BD and where != 'exe': ps_funcs.add((where, b + i))
            if w >> 26 == 3:
                t = 0x80000000 | ((w & 0x3FFFFFF) << 2)
                wt = px.where(t, where if where != 'exe' else None)
                if wt is not None: ps_funcs.add((wt, t))
    ps_funcs |= set(rev)
    for where in ['exe'] + sorted(px.ovl):            # code pointers in tables
        b, d = px.mem(where)
        for i in range(0, len(d) - 4, 4):
            t = struct.unpack_from('<I', d, i)[0]
            wt = px.where(t, where if where != 'exe' else None)
            if wt is not None and t & 3 == 0:
                bb, dd = px.mem(wt)
                if (struct.unpack_from('<I', dd, t - bb)[0] >> 16) == 0x27BD: ps_funcs.add((wt, t))
    ps_callers = {}
    for g in ps_funcs:
        area = g[0] if g[0] != 'exe' else None
        for t in px.func(*g)[1]:
            wt = px.where(t, area)
            if wt is not None: ps_callers.setdefault((wt, t), set()).add(g)

    def upward():
        found = 0
        for f, (where, s, _) in list(pairs.items()):
            cf = [c for c in pc_callers.get(f, ()) if c not in pairs]
            cg = [c for c in ps_callers.get((where, s), ()) if c not in rev]
            if not cf or not cg: continue
            sc = {(x, y): score(pc.func(x), px.func(*y)) for x in cf for y in cg}
            for x in cf:
                best = sorted(((v, y) for (xx, y), v in sc.items() if xx == x), key=lambda t: -t[0])
                if not best or best[0][0] < 0.45: continue
                if len(best) > 1 and best[0][0] - best[1][0] < 0.1: continue
                y = best[0][1]
                if max(v for (xx, yy), v in sc.items() if yy == y) > best[0][0]: continue     # mutual best
                if x not in pairs and y not in rev:
                    add(x, y[0], y[1], best[0][0]); found += 1
        return found

    def global_pass():
        """unique, mutual best pairs among the rest (functions with enough features)"""
        A = [f for f in pc_funcs if f not in pairs and len(pc.func(f)[2]) >= 6]
        B = [g for g in ps_funcs if g not in rev and len(px.func(*g)[2]) >= 6]
        best_a, best_b = {}, {}
        for x in A:
            fx = pc.func(x)
            for y in B:
                fy = px.func(*y)
                if not (0.4 <= fx[0] / max(1, fy[0]) <= 2.5): continue
                v = score(fx, fy)
                if v < 0.55: continue
                best_a.setdefault(x, []).append((v, y)); best_b.setdefault(y, []).append((v, x))
        found = 0
        for x, l in best_a.items():
            l.sort(key=lambda t: -t[0])
            if len(l) > 1 and l[0][0] - l[1][0] < 0.15: continue
            v, y = l[0]
            m = sorted(best_b[y], key=lambda t: -t[0])
            if m[0][1] != x or (len(m) > 1 and m[0][0] - m[1][0] < 0.15): continue
            if x not in pairs and y not in rev: add(x, y[0], y[1], v); found += 1
        return found

    def ps_resolve(t, ctx):
        return px.where(t, ctx)

    while True:
      while todo:
          p = todo.pop(0)
          where, s, _ = pairs[p]
          if not pc.is_code(p): continue
          pf = pc.func(p); sf = px.func(where, s)
          area = where if where != 'exe' else None
          ca = [t for t in pf[1] if pc.is_code(t)]
          cb = [(ps_resolve(t, area), t) for t in sf[1]]
          cb = [x for x in cb if x[0] is not None or px.candidates(x[1])]   # None: one of the overlays

          def resolve(x, y):
              return (best_cand(x, y[1])[1], y[1]) if y[0] is None else y

          def sim(x, y):
              y = resolve(x, y)
              if y[0] is None: return 0.0
              if x in pairs: return 3.0 if pairs[x][:2] == y else 0.0
              if y in rev: return 0.0
              return score(pc.func(x), px.func(*y))

          for i, j in align(ca, cb, sim):
              x, y = ca[i], resolve(ca[i], cb[j])
              if y[0] is None: continue
              sc = score(pc.func(x), px.func(*y))
              if sc >= 0.35 or (sc >= 0.2 and len(ca) == len(cb)): add(x, y[0], y[1], sc)
          # function tables used by both (state tables, dispatch tables)
          def pc_tab(t):
              n = 0
              while n < 128 and pc.is_code(pc.u32(t + 4 * n)): n += 1
              return n
          def ps_tab(t):
              w0 = px.where(t, area)
              n = 0
              while w0 and n < 128 and px.where(t + 4 * n, area) == w0 and px.where(px.u32(w0, t + 4 * n), area) is not None \
                      and px.u32(w0, t + 4 * n) & 3 == 0: n += 1
              return n
          ta = [t for t in dict.fromkeys(pf[3]) if pc_tab(t) >= 2]
          tb = [t for t in dict.fromkeys(sf[3]) if ps_tab(t) >= 2]
          for i, j in align(ta, tb, lambda x, y: 1.0 if abs(pc_tab(x) - ps_tab(y)) <= 2 else 0.0):
              x, y = ta[i], tb[j]
              w0 = px.where(y, area)
              for k in range(min(pc_tab(x), ps_tab(y))):
                  v1, v2 = pc.u32(x + 4 * k), px.u32(w0, y + 4 * k)
                  w2 = px.where(v2, area)
                  if w2 is not None: add(v1, w2, v2, 0.9)
      if upward(): continue
      if not global_pass(): break

    def consistency(p):
        """paired callees of p whose partner is a callee of p's partner: 'n/m'"""
        where, s, _ = pairs[p]
        area = where if where != 'exe' else None
        cb = {(px.where(t, area), t) for t in px.func(where, s)[1]}
        ca = [t for t in dict.fromkeys(pc.func(p)[1]) if t in pairs]
        return '%d/%d' % (sum(1 for t in ca if pairs[t][:2] in cb), len(ca))

    rows = sorted(pairs.items())
    with open(a.o, 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['pc', 'psx', 'where', 'score', 'callees', 'name'])
        for p, (where, s, sc) in rows:
            w.writerow(['0x%06X' % p, '0x%08X' % s, where, '%.2f' % sc,
                        consistency(p), names.get(p, '')])
    game = [p for p in pairs if any(lo <= p < hi for lo, hi in PC.GAME)]
    lib = [p for p in pairs if PC.PCL[0] <= p < PC.PCL[1]]
    ovl = [p for p in pairs if pairs[p][0] != 'exe']
    print('PC game functions seen %d, PS1 functions seen %d' % (len(pc_funcs), len(ps_funcs)))
    print('%d pairs: %d game, %d PC layer, %d in area overlays -> %s' % (len(pairs), len(game), len(lib), len(ovl), a.o))

if __name__ == '__main__':
    main()
