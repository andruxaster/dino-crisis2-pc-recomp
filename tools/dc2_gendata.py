#!/usr/bin/env python3
"""dc2_gendata.py -- the initial contents of Dino2.exe's tables as C.

Reads the reference executable (2002-02-05) and writes src/data/*.c: every
table and initialised variable of the game and of the PC layer in .rdata and
.data, at its original address, as a typed C array, plus the list Data_Init
copies into the memory image.  What only the C runtime, libjpeg, the Win32
glue or DirectX use (the import table, locale tables, Huffman tables, GUIDs)
is left out.

Objects start at every address the original code refers to (and at the
labels of tools/dc2_labels.csv); the element type comes from how the code
accesses the object (byte/word/dword, x87 float/double), else from the
contents (strings, pointers, words).  Pointer values are commented with what
they point to (function or label names, strings).

    usage: dc2_gendata.py Dino2.exe [OUTDIR]     (default: src/data)
"""
import collections
import csv
import hashlib
import os
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

RDATA, DATA = 0x703000, 0x71B000
DATA_END = 0x747000                 # end of the initialised part of .data
IAT_END = 0x7031A4                  # the import address table
GUIDS = (0x719820, 0x719870)        # DirectX interface ids
REFERENCE_SHA1 = 'c14c2b630b3044fee207bd7a65b3fc8139a49d2c'


def region(a):
    if a < 0x46F000 or 0x471000 <= a < 0x4B4000:
        return 'game'
    if a < 0x471000:
        return 'w32'
    if a < 0x4CD000:
        return 'pc'
    if a < 0x4DD5C0:
        return 'jpeg'
    return 'crt'


def load_exe(path):
    d = open(path, 'rb').read()
    if hashlib.sha1(d).hexdigest() != REFERENCE_SHA1:
        sys.exit('%s: not the reference Dino2.exe (2002-02-05)' % path)
    pe = struct.unpack_from('<I', d, 0x3C)[0]
    nsec, opt = struct.unpack_from('<H', d, pe + 6)[0], struct.unpack_from('<H', d, pe + 20)[0]
    img = bytearray(DATA_END)
    for i in range(nsec):
        va, rs, ro = struct.unpack_from('<III', d, pe + 24 + opt + 40 * i + 12)
        va += 0x400000
        if va in (RDATA, DATA):
            img[va:va + min(rs, DATA_END - va)] = d[ro:ro + min(rs, DATA_END - va)]
    return img


def scan_code(exe):
    """addresses of .rdata/.data the code refers to: users (regions,
    functions) and access kinds"""
    dis = subprocess.run(['objdump', '-d', '-M', 'intel', '--no-show-raw-insn', exe],
                         capture_output=True, text=True, check=True).stdout
    users = collections.defaultdict(set)
    calls = set(int(h, 16) for h in re.findall(r'\tcall\s+0x([0-9a-f]+)\b', dis))
    regs = collections.defaultdict(set)
    acc = collections.defaultdict(set)
    for line in dis.splitlines():
        m = re.match(r'\s+([0-9a-f]+):\t(\S+)\s*(.*)', line)
        if not m:
            continue
        pc, mn, ops = int(m.group(1), 16), m.group(2), m.group(3)
        for h in re.findall(r'0x([0-9a-f]{6})\b', ops):
            v = int(h, 16)
            if not RDATA <= v < DATA_END:
                continue
            regs[v].add(region(pc))
            users[v].add(pc)
            mm = re.search(r'(BYTE|WORD|DWORD|QWORD|TBYTE) PTR \[[^\]]*0x' + h, ops)
            if mm:
                k = mm.group(1)
                if mn.startswith('f') and k in ('DWORD', 'QWORD'):
                    k = 'float' if k == 'DWORD' else 'double'
                acc[v].add(k)
            else:
                acc[v].add('addr')
    return users, regs, acc, calls


def load_names():
    funcs, labels, hooked = {}, {}, set()
    for r in csv.reader(open(os.path.join(HERE, 'dc2_labels.csv'))):
        try:
            a = int(r[0], 16)
        except ValueError:
            continue
        if r[1] == 'func':
            funcs[a] = r[2]
        else:
            labels.setdefault(a, r[2])
    for m in re.finditer(r'\{0x([0-9A-F]+),\s*\(void \*\)\w+,\s*"(\w+)"\}', open(os.path.join(ROOT, 'src/hook/hooks.c')).read()):
        funcs[int(m.group(1), 16)] = m.group(2)
        hooked.add(int(m.group(1), 16))
    return funcs, labels, hooked


def func_of(funcs, starts, pc):
    import bisect
    i = bisect.bisect_right(starts, pc) - 1
    return funcs[starts[i]] if i >= 0 else None


def printable(b):
    return all(32 <= c < 127 or c in (9, 10, 13) for c in b)


def c_string(b):
    out = []
    for c in b:
        if c == 0x22 or c == 0x5C:
            out.append('\\' + chr(c))
        elif 32 <= c < 127:
            out.append(chr(c))
        elif c == 10:
            out.append('\\n')
        elif c == 13:
            out.append('\\r')
        elif c == 9:
            out.append('\\t')
        else:
            out.append('\\%03o' % c)
    return ''.join(out)


def as_strings(b):
    """the object as NUL-terminated strings (and the zero padding after):
    the strings, or None"""
    end = len(b)
    while end and b[end - 1] == 0:
        end -= 1
    if end == 0:
        return None
    parts = bytes(b[:end]).split(b'\0')
    if not all(len(p) >= 3 and printable(p) and any(chr(c).isalpha() for c in p) for p in parts):
        return None
    if sum(len(p) for p in parts) < 2 * len(parts):
        return None
    return parts


def fmt_float(v, bits):
    if bits == 32:
        f = struct.unpack('<f', struct.pack('<I', v))[0]
        if f != f or f in (float('inf'), float('-inf')):
            return None
        s = '%.9g' % f
        if struct.unpack('<I', struct.pack('<f', float(s)))[0] != v:
            return None
        if 'e' not in s and '.' not in s:
            s += '.0'
        return s + 'f'
    f = struct.unpack('<d', struct.pack('<Q', v))[0]
    if f != f or f in (float('inf'), float('-inf')):
        return None
    s = '%.17g' % f
    if 'e' not in s and '.' not in s:
        s += '.0'
    return s


def main():
    exe = sys.argv[1]
    outdir = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, 'src/data')
    img = load_exe(exe)
    users, regs, acc, calls = scan_code(exe)
    funcs, labels, hooked = load_names()
    for a in range(RDATA, DATA_END - 3, 4):  # and so do code addresses in the tables
        v = struct.unpack_from('<I', img, a)[0]
        if 0x401000 <= v < 0x46F000 and v % 16 == 0:
            calls.add(v)
    for c in calls:                     # functions without a name bound the named ones
        if 0x401000 <= c < 0x4E8000:
            funcs.setdefault(c, 'sub_%06X' % c)
    starts = sorted(funcs)

    def keep_users(a):
        return regs.get(a, set()) & {'game', 'pc'}

    # the address ranges looked at: .rdata after the import table, .data
    spans = [(IAT_END, 0x71A000), (DATA, DATA_END)]
    bounds = set(a for a in regs if RDATA <= a < DATA_END) | set(a for a in labels if RDATA <= a < DATA_END)
    for lo, hi in spans:
        bounds.add(lo)
    objs = []
    for lo, hi in spans:
        bs = sorted(b for b in bounds if lo <= b < hi) + [hi]
        for a, b in zip(bs, bs[1:]):
            objs.append([a, b])

    # an object nobody of the game or the PC layer refers to belongs to the
    # one before (bytes reached by an index), unless that one is dropped
    def wanted(a):
        if GUIDS[0] <= a < GUIDS[1]:
            return False
        if keep_users(a) or a in labels:
            return True
        # the Win32 glue's region also holds game functions rewritten in C
        import bisect
        for pc in users.get(a, ()):
            i = bisect.bisect_right(starts, pc) - 1
            if i >= 0 and starts[i] in hooked:
                return True
        return False
    # addresses the C code uses (the ports' start-up code included)
    c_addrs = set()
    for d in ('src/game', 'src/pc', 'src/port', 'src/port/dc', 'include/dc2'):
        for f in os.listdir(os.path.join(ROOT, d)):
            if f.endswith(('.c', '.h')):
                for h in re.findall(r'0x([0-9A-Fa-f]{6})\b', open(os.path.join(ROOT, d, f)).read()):
                    if RDATA <= int(h, 16) < DATA_END:
                        c_addrs.add(int(h, 16))
    kept = []
    for o in objs:
        a = o[0]
        if a in regs or a in labels or a in (IAT_END, DATA):
            o.append(wanted(a))
            kept.append(o)
        elif kept:
            kept[-1][1] = o[1]
    for o in kept:
        if not o[2] and any(o[0] <= c < o[1] for c in c_addrs) and not GUIDS[0] <= o[0] < GUIDS[1]:
            o[2] = True
    # fields of one table the compiler addressed one by one (base + constant):
    # a run of objects accessed the same way by the same functions is one
    # table (a label starts a new one)
    def funcs_of(a):
        return set(filter(None, (func_of(funcs, starts, pc) for pc in users.get(a, ()))))
    merged = []
    for o in kept:
        p = merged[-1] if merged else None
        k = acc.get(o[0], set())
        if (p and p[1] == o[0] and p[2] == o[2] and o[0] not in labels and len(k) == 1
                and k == acc.get(p[0], set()) and k <= {'BYTE', 'WORD', 'DWORD', 'float'}
                and funcs_of(o[0]) & p[3]):
            p[1] = o[1]
            p[3] |= funcs_of(o[0])
            p[4] |= users.get(o[0], set())
        else:
            merged.append([o[0], o[1], o[2], funcs_of(o[0]), set(users.get(o[0], ()))])
    kept = [(m[0], m[1], m[2], m[4]) for m in merged]
    out = collections.defaultdict(list)
    seen_names = set()
    nbytes = 0
    for a, b, want, uses in kept:
        if not want:
            continue
        body = img[a:b]
        end = len(body)
        while end and body[end - 1] == 0:
            end -= 1
        if end == 0:
            continue
        name = labels.get(a) or 'd_%06X' % a
        if name in seen_names:
            name = '%s_%06X' % (name, a)
        seen_names.add(name)
        who = sorted(set(filter(None, (func_of(funcs, starts, pc) for pc in uses))))
        kinds = acc.get(a, set())
        size = b - a
        lines = []
        text = None
        if 'float' in kinds and not (kinds & {'BYTE', 'WORD', 'double'}):
            ctype, unit = 'float', 4
        elif 'double' in kinds and not (kinds & {'BYTE', 'WORD', 'float'}):
            ctype, unit = 'double', 8
        elif kinds & {'DWORD', 'float'} and not kinds & {'BYTE', 'WORD'}:
            ctype, unit = 'u32', 4
        elif 'WORD' in kinds and not kinds & {'BYTE', 'DWORD'}:
            ctype, unit = 'u16', 2
        elif 'BYTE' in kinds and not kinds & {'WORD', 'DWORD'}:
            ctype, unit = 'u8', 1
        else:
            ctype, unit = None, 0
        strs = as_strings(body) if ctype is None and kinds <= {'addr'} else None
        if strs is not None:
            ctype, unit = 'char', 1
            text = strs
        elif ctype is None:
            # by contents: words if aligned, else bytes
            ctype, unit = ('u32', 4) if a % 4 == 0 and size % 4 == 0 else ('u16', 2) if a % 2 == 0 and size % 2 == 0 else ('u8', 1)
        n = (end + unit - 1) // unit
        if a % unit:
            ctype, unit, n = 'u8', 1, end
        if a + n * unit > b:
            ctype, unit, n = 'u8', 1, end
        nbytes += n * unit
        cmt = '/* 0x%06X' % a
        if who:
            cmt += ': ' + ', '.join(who[:6]) + (', ...' if len(who) > 6 else '')
        cmt += ' */'
        if text is not None:
            lit = '\\0'.join(c_string(p) for p in text)
            decl = 'DATA(0x%06X, char, %s, %d) = "%s";' % (a, name, n, lit)
            lines.append(cmt)
            lines.append(decl)
        else:
            vals = []
            for i in range(n):
                if unit == 4:
                    v = struct.unpack_from('<I', body, i * 4)[0]
                elif unit == 2:
                    v = struct.unpack_from('<H', body, i * 2)[0] if i * 2 + 2 <= len(body) else body[i * 2]
                elif unit == 8:
                    v = struct.unpack_from('<Q', body, i * 8)[0]
                else:
                    v = body[i]
                vals.append(v)
            items = []
            bad_float = False
            for v in vals:
                if ctype == 'float':
                    s = fmt_float(v, 32)
                    if s is None:
                        bad_float = True
                        break
                    items.append(s)
                elif ctype == 'double':
                    s = fmt_float(v, 64)
                    if s is None:
                        bad_float = True
                        break
                    items.append(s)
                else:
                    items.append(None)
            if bad_float:
                ctype = 'u32' if unit == 4 else 'u32'
                n = n * unit // 4
                vals = [struct.unpack_from('<I', body, i * 4)[0] for i in range(n)]
                unit = 4
                items = [None] * n
            notes = [None] * n
            for i, v in enumerate(vals):
                if items[i] is not None:
                    continue
                if unit == 4:
                    s = '0x%X' % v if v > 9 else str(v)
                    if v in funcs and 0x401000 <= v < 0x4E8000:
                        notes[i] = funcs[v]
                    elif RDATA <= v < DATA_END and v in labels:
                        notes[i] = labels[v]
                    elif RDATA <= v < DATA_END:
                        st = img[v:v + 40].split(b'\0')[0]
                        if len(st) >= 3 and printable(st):
                            notes[i] = '"%s"' % c_string(st[:32]).replace('*/', '*\\/')
                elif unit == 2:
                    s = '0x%X' % v if v > 9 else str(v)
                elif unit == 8:
                    s = '0x%XULL' % v
                else:
                    s = '0x%X' % v if v > 9 else str(v)
                items[i] = s
            per = {1: 16, 2: 12, 4: 8, 8: 4}[unit] if ctype not in ('float', 'double') else 6
            lines.append(cmt)
            lines.append('DATA(0x%06X, %s, %s, %d) = {' % (a, ctype, name, n))
            if any(notes):                  # pointers: one a line, with what they point to
                w = max(len(x) for x in items) + 1
                for i in range(n):
                    lines.append(('    %-*s /* %s */' % (w, items[i] + ',', notes[i])) if notes[i] else '    ' + items[i] + ',')
            else:
                for i in range(0, n, per):
                    lines.append('    ' + ', '.join(items[i:i + per]) + ',')
            lines.append('};')
        f = 'rodata' if a < DATA else 'data_game' if a < 0x73B600 or a >= 0x746C00 else 'data_pc'
        out[f].append((a, name, '\n'.join(lines)))

    os.makedirs(outdir, exist_ok=True)
    titles = {'rodata': 'read-only tables (.rdata): constants, strings, tables of the game and of the PC layer',
              'data_game': 'initialised data of the game (.data 0x71B000..0x73B600, 0x746C00..)',
              'data_pc': 'initialised data of the PC layer (.data 0x73B600..0x746C00)'}
    for f, items in out.items():
        with open(os.path.join(outdir, f + '.c'), 'w') as fp:
            fp.write('/* %s.c -- %s.\n' % (f, titles[f]))
            fp.write(' * Generated by tools/dc2_gendata.py from Dino2.exe (2002-02-05); each\n')
            fp.write(' * table at its original address, copied into the memory image by Data_Init.\n')
            fp.write(' * The comments name the original functions using a table and what its\n')
            fp.write(' * pointers point to. */\n#include "dc2/data.h"\n\n')
            for a, name, text in items:
                fp.write(text + '\n\n')
            fp.write('const DataBlock g_%s[] = {\n' % f)
            for a, name, text in items:
                fp.write('    DATA_BLOCK(0x%06X, %s),\n' % (a, name))
            fp.write('    {0, 0, 0}\n};\n')
    print('%d objects, %d bytes' % (sum(len(v) for v in out.values()), nbytes))


if __name__ == '__main__':
    main()
