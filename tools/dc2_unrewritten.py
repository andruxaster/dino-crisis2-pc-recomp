#!/usr/bin/env python3
"""Game functions of Dino2.exe not rewritten yet (not in src/hook/hooks.c).

    tools/dc2_unrewritten.py [game/Dino2.exe]

Function entries are found in the exe itself: the targets of direct calls in
the game code (0x401000-0x46F000, 0x471000-0x4B4000), the code addresses in
tables of .rdata/.data (16-byte aligned, next to another code address) and
code addresses pushed as constants (tasks started or chained). Those not in
hooks.c are printed by the table they are found in (named after
src/data's DATA blocks):

  [called by C]  the C code dispatches that table through Orig_Call*, so the
                 standalone builds reach the entry and print "not rewritten
                 yet: handler 0x..." (or stop the task) when it comes up;
  [check]        a table the C code reads in its own way (a switch, a C
                 table of rewritten static functions, a sound id...): often
                 not a function to rewrite, look at it by hand.

Direct calls to functions not rewritten would show as a list of their own;
there are none now (every function called directly is rewritten).
Needs objdump (binutils) on PATH."""
import os, re, struct, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
exe_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'game', 'Dino2.exe')
exe = open(exe_path, 'rb').read()

def game(a): return 0x401000 <= a < 0x46F000 or 0x471000 <= a < 0x4B4000

pe = struct.unpack_from('<I', exe, 0x3C)[0]
nsec = struct.unpack_from('<H', exe, pe + 6)[0]
opt = struct.unpack_from('<H', exe, pe + 20)[0]
secs = [struct.unpack_from('<8sIIII', exe, pe + 24 + opt + 40 * i) for i in range(nsec)]

dis = subprocess.run(['objdump', '-d', '-M', 'intel', '--start-address=0x401000', '--stop-address=0x4B4000', exe_path],
                     capture_output=True, text=True, check=True).stdout
calls = {int(x, 16) for x in re.findall(r'\tcall\s+([0-9a-f]{6})\b', dis)}
pushes = {int(x, 16) for x in re.findall(r'\tpush\s+0x([0-9a-f]{6})\n', dis)}

ptrs = {}                                   # entry -> table addresses holding it
for name, vs, va, rs, ro in secs:
    if name.rstrip(b'\0') not in (b'.rdata', b'.data'):
        continue
    w = [struct.unpack_from('<I', exe, ro + o)[0] for o in range(0, min(vs, rs) - 3, 4)]
    for i, v in enumerate(w):
        if game(v) and v % 16 == 0 and ((i and game(w[i - 1])) or (i + 1 < len(w) and game(w[i + 1]))):
            ptrs.setdefault(v, []).append(0x400000 + va + 4 * i)

text = next(s for s in secs if s[0].rstrip(b'\0') == b'.text')
def byte_at(a): return exe[text[4] + a - 0x400000 - text[2]]
def entry_like(a):
    """after the padding or the return of the function before (not a case
    label inside a function, not a constant such as an RGB 0x404040)"""
    return a & 0xFFF and byte_at(a - 1) in (0x90, 0xCC, 0xC3) or byte_at(a - 3) == 0xC2
cands = {a for a in calls if game(a)} | {a for a in ptrs if a & 0xFFF} | \
        {a for a in pushes if game(a) and a % 16 == 0 and entry_like(a)}

src = open(os.path.join(ROOT, 'src', 'hook', 'hooks.c')).read()
done = {int(x, 16) for x in re.findall(r'\{0x([0-9A-Fa-f]+), \(void \*\)', src)}

blocks = []                                  # src/data's tables: (start, end, name)
for f in os.listdir(os.path.join(ROOT, 'src', 'data')):
    if f.endswith('.c'):
        for a, t, nm, n in re.findall(r'DATA\((0x[0-9A-F]+), (\w+), (\w+), (\d+)\)', open(os.path.join(ROOT, 'src', 'data', f)).read()):
            size = {'u8': 1, 's8': 1, 'u16': 2, 's16': 2}.get(t, 4)
            blocks.append((int(a, 16), int(a, 16) + size * int(n), nm))
blocks.sort()
def block_of(a):
    for s, e, nm in blocks:
        if s <= a < e: return s, nm
    return None, None

csrc = ''
for d in ('src/game', 'src/pc', 'src/port'):
    for f in os.listdir(os.path.join(ROOT, d)):
        if f.endswith('.c'): csrc += open(os.path.join(ROOT, d, f)).read()
via_orig = {int(x, 16) for x in re.findall(r'Orig_Call\w*\(U32\((0x[0-9A-F]{6})', csrc)}

labels = {}
for l in open(os.path.join(ROOT, 'tools', 'dc2_labels.csv')):
    p = l.split(',')
    if len(p) > 2 and p[0].startswith('0x'):
        labels[int(p[0], 16)] = p[2]

left = sorted(cands - done)
direct = [a for a in left if a in calls]
groups = {}
for a in left:
    if a in calls: continue
    t = min(ptrs[a]) if a in ptrs else None
    base, nm = block_of(t) if t else (None, None)
    groups.setdefault((base or t or 0, nm), []).append(a)

def fmt(a): return '0x%06X' % a + (' ' + labels[a] if a in labels else '')
print('%d function entries found in the game code, %d rewritten, %d not:' % (len(cands), len(cands & done), len(left)))
if direct:
    print('\ncalled directly:\n  ' + '\n  '.join(fmt(a) for a in direct))
for called in (True, False):
    rows = [(k, v) for k, v in sorted(groups.items()) if (k[0] in via_orig) == called]
    if not rows: continue
    print('\n[called by C]' if called else '\n[check]')
    for (base, nm), v in rows:
        where = ('table 0x%06X %s' % (base, nm or '')) if base else 'pushed as a task'
        print('  %s: %s' % (where, ', '.join(fmt(a) for a in v)))
