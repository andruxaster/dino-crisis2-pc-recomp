#!/usr/bin/env python3
"""dc2script.py -- room script (G+0xA54) of Dino Crisis 2 PC: disassembler,
pseudo-C listing and extraction of doors / triggers / items / spawns.

Section layout (Script_Init 0x48BB60):
  +0x00  header, 7 x u32: [3] = offset of the global table (from +0x1C),
         copied to 0x87FC40 (value type 6)
  +0x1C  code base B: u32 routine offset table (offsets from B; the first
         entry also gives its length), then bytecode, then data (texts,
         tables), then the global table (s32 each)
VM (Script_Run 0x48BC50): 10 threads x 0xA4 bytes at *0x87FC20:
  +0x00 pc, +0x04..+0x7F stack (grows down from +0x80), +0x84 sp,
  +0x88 r0..r3, +0x98 wait counter, +0x9B state (1 run, 2 wait, bit7 paused),
  +0x9C condition flags, +0x9D call depth, +0x9E/+0x9F selected object.
  Opcode table 0x738C40 (96 entries); handler returns 0 = next opcode,
  1 = yield until next frame.
Encoding (little endian, all instructions 16-bit aligned):
  01 d t v16      r[d] = val(t, v)                    (sets flags)
  02 d t v16      val(t, v) = r[d]                    (sets flags)
  03 op d t v16   r[d] = r[d] OP val(t, v); d = 0xFF -> sp (pop: add sp, n)
  04 00           return (end of thread at depth 0)
  05 t v16        push val(t, v)
  06 t v16        pop -> val(t, v)
  07 c s16        if (flags & (1<<c)) or c == 0: pc += s16 (from this opcode)
  08 00 s16       call routine s16
  xx 00           other opcodes: built-ins, arguments on the stack in push
                  order (the script then pops them with 03 00 FF 00 n 00);
                  results go to r0.
  val types: 0 immediate, 1 register, 2 stack slot sp[v], 4 immediate
  (routine/thread ids), 6 global[v].
  flags after 01/02/03 and built-ins with a result:
  ==0 -> bits 1,3,5; >0 -> 2,3,6; <0 -> 4,5,6  (1 eq, 2 gt, 3 ge, 4 lt, 5 le, 6 ne).
  OP: 0 add 1 sub 2 mul 3 div 4 mod 5 or 6 and 7 xor 8 not 9/11 shl 10/12 sar
      13 gt 14 lt 15 ge 16 le 17 eq 18 ne 19 lnot 20 inc 21 dec 24 lor 25 land

AOT table (G+0xA5C, 32 x 40 bytes; checked each frame by 0x48B040):
  +0 flags (1 on, 2 test point in front of the player, bits 2-3 trigger:
  0 action button, 8 touch, 0xC special), +1 kind, +2 level, +4 quad
  s16 {x,z}[4], +0x14.. kind data. Kind handlers at 0x738B68:
  0 door (0x48B390): +0x14 x, y, z, dir, u16 room (stage<<8 | room),
    u16 door animation, +0x20 lock message id, +0x21 key flag (group 8)
  1 event: start routine +0x18 on thread +0x14
  2 message: text at script+0x14            3 flag: Flag_Set(+14,+15,+16)
  5 item pickup (menu 0x495E90 mode 1)      6 effect object on
  7 save/typewriter mode (script mode 3)    8 item box (menu mode 2)
  9 use item +0x19 here -> routine +0x18 on thread +0x14
  10, 11 weapon/other (not decoded)

Usage: dc2script.py ST100.DAT [--json] [--lst out.txt]
"""
import json, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dc2room import room_block, BASE  # noqa: E402

# name, argc, returns value in r0
OPS = {
    0x00: ("nop", 0, 0), 0x04: ("return", 0, 0),
    0x10: ("wait", 1, 0), 0x11: ("message", 1, 0), 0x12: ("obj_select", 2, 0),
    0x13: ("effect_init", 15, 0), 0x14: ("obj14_init", 11, 0),
    0x15: ("thread_start", 2, 0), 0x16: ("thread_kill", 1, 0),
    0x17: ("thread_pause", 1, 0), 0x18: ("thread_resume", 1, 0),
    0x19: ("thread_goto", 1, 0), 0x1A: ("obj_spawn", 10, 0), 0x1B: ("op1B", 1, 0),
    0x1C: ("flag_set", 3, 0), 0x1D: ("flag_check", 2, 1),
    0x1E: ("obj_set", 2, 0), 0x1F: ("obj_get", 1, 1),
    0x20: ("wait_event", 0, 1), 0x21: ("anim_start", 5, 0), 0x22: ("set_skip", 1, 0),
    0x23: ("load_file", 2, 0), 0x24: ("wait_item_used", 0, 1),
    0x25: ("flag_range", 3, 0), 0x26: ("timer", 1, 0), 0x27: ("sound", 3, 0),
    0x28: ("wait_flag", 3, 0), 0x29: ("player_change", 1, 0), 0x2A: ("wait_frame", 1, 0),
    0x2B: ("obj_op2B", 4, 0), 0x2C: ("menu", 5, 0), 0x2D: ("item_op2D", 1, 0),
    0x2E: ("op2E", 3, 0), 0x2F: ("op2F", 1, 0),
    0x30: ("aot_door", 19, 0), 0x31: ("aot_event", 13, 0), 0x32: ("aot_message", 13, 0),
    0x33: ("aot_flag", 14, 0), 0x35: ("aot_item", 17, 0), 0x36: ("aot_effect", 13, 0),
    0x37: ("aot_save", 12, 0), 0x38: ("aot_box", 12, 0), 0x39: ("aot_use_item", 14, 0),
    0x3A: ("aot_kind10", 15, 0), 0x3B: ("aot_kind11", 12, 0),
    0x3E: ("aot_trigger", 1, 0), 0x40: ("op40", 5, 0), 0x41: ("op41", 1, 0),
    0x42: ("op42", 2, 0), 0x43: ("op43", 2, 0), 0x44: ("obj44_init", 8, 0),
    0x45: ("obj_enable", 3, 0), 0x46: ("anim_start2", 4, 0), 0x47: ("op47", 3, 0),
    0x48: ("random", 0, 1), 0x49: ("op49", 3, 0), 0x4A: ("op4A", 1, 0), 0x4B: ("op4B", 0, 0),
    0x4C: ("op4C", 1, 0), 0x4D: ("op4D", 1, 0), 0x4E: ("wait_file", 0, 0),
    0x4F: ("enemy_set", 1, 0), 0x50: ("op50", 1, 0), 0x51: ("op51", 0, 0), 0x52: ("op52", 0, 0),
    0x53: ("op53", 1, 0), 0x54: ("op54", 1, 0), 0x55: ("op55", 4, 0), 0x56: ("op56", 0, 0),
    0x57: ("op57", 1, 0), 0x58: ("op58", 2, 0), 0x59: ("vram_load", 4, 0),
}
LEN = {1: 6, 2: 6, 3: 6, 5: 4, 6: 4, 7: 4, 8: 4}
BINOP = ["+", "-", "*", "/", "%", "|", "&", "^", "~", "<<", ">>", "<<", ">>", ">", "<", ">=",
         "<=", "==", "!=", "!", "++", "--", "?", "?", "||", "&&"]
COND = {0: "", 1: "==0", 2: ">0", 3: ">=0", 4: "<0", 5: "<=0", 6: "!=0"}


def decode_text(buf, off, limit=2000):
    """game text: u16 per character, ASCII in the low byte; 0x0000 space,
    0x0005 '.', 0x14..0x1D digits, 0x4000 new line, 0x5000 new page, 0x6000 wait for a key,
    0xF000 end; other 0x2000/0x70xx codes are kept as {xxxx}"""
    out = []
    for i in range(off, min(len(buf) - 1, off + limit), 2):
        c = buf[i] | buf[i + 1] << 8
        if c == 0xF000:
            break
        out.append(chr(c) if 32 <= c < 127 else " " if c == 0 else "." if c == 5 else
                   str(c - 0x14) if 0x14 <= c <= 0x1D else '"' if c in (8, 9) else
                   "\\n" if c == 0x4000 else "\\p" if c == 0x5000 else "\\w" if c == 0x6000 else "{%04x}" % c)
    return "".join(out)


class Script:
    def __init__(self, blk):
        self.blk = blk
        a54 = struct.unpack_from("<I", blk, 0x14)[0] - BASE
        self.hdr = struct.unpack_from("<7I", blk, a54)
        self.base = a54 + 0x1C
        ptrs = [x - BASE for x in struct.unpack_from("<12I", blk, 0)]
        end = min([x for x in ptrs if x > a54] + [len(blk)])     # next section
        n = struct.unpack_from("<I", blk, self.base)[0] // 4
        self.routines = list(struct.unpack_from("<%dI" % n, blk, self.base))
        g = self.base + self.hdr[3]
        self.globals = list(struct.unpack_from("<%di" % max(0, (end - g) // 4), blk, g))
        self.code = blk[self.base:end]
        self.insns = {}
        self.labels = set()
        for r in self.routines:
            self._walk(r)

    def _walk(self, pc):
        work = [pc]
        while work:
            pc = work.pop()
            while pc not in self.insns and 0 <= pc and pc + 2 <= len(self.code):
                op = self.code[pc]
                if op > 0x5F:
                    break
                ln = LEN.get(op, 2)
                self.insns[pc] = self.code[pc:pc + ln]
                if op == 7:
                    c, off = self.code[pc + 1], struct.unpack_from("<h", self.code, pc + 2)[0]
                    self.labels.add(pc + off)
                    work.append(pc + off)
                    if c == 0:
                        break
                if op in (4, 0x19):
                    break
                pc += ln

    def val(self, t, v):
        if t == 0 or t == 4:
            return str(v)
        if t == 1:
            return "r%d" % v
        if t == 2:
            return "sp[%d]" % v
        if t == 6:
            g = self.globals[v] if v < len(self.globals) else None
            if g is not None and 0x8000 <= g < 0x10000: g -= 0x10000
            return "g[%d]" % v + ("{%d}" % g if g is not None else "")
        return "?%d:%d" % (t, v)

    def gval(self, t, v):
        """numeric value of a push (None when not constant)"""
        if t in (0, 4):
            return v
        if t == 6 and v < len(self.globals):
            g = self.globals[v]
            return g - 0x10000 if 0x8000 <= g < 0x10000 else g
        return None

    def listing(self):
        out, stack, calls = [], [], []
        for r, start in enumerate(self.routines):
            self.labels.add(start)
        rstart = {s: i for i, s in enumerate(self.routines)}
        for pc in sorted(self.insns):
            ins = self.insns[pc]
            op = ins[0]
            if pc in rstart:
                out.append("\nroutine_%d:   /* %#x */" % (rstart[pc], pc))
                stack = []
            elif pc in self.labels:
                out.append("L_%04x:" % pc)
                stack = []
            line = None
            if op == 5:
                t, v = ins[1], struct.unpack_from("<h", ins, 2)[0]
                stack.append((self.val(t, v), self.gval(t, v)))
                continue
            if op == 1:
                line = "r%d = %s" % (ins[1], self.val(ins[2], struct.unpack_from("<h", ins, 4)[0]))
            elif op == 2:
                line = "%s = r%d" % (self.val(ins[2], struct.unpack_from("<h", ins, 4)[0]), ins[1])
            elif op == 3:
                o, d, t, v = ins[1], ins[2], ins[3], struct.unpack_from("<h", ins, 4)[0]
                if d == 0xFF and o == 0 and t == 0:
                    if len(stack) >= v:
                        del stack[len(stack) - v:]
                        continue
                    line = "pop %d" % v
                else:
                    dst = "sp" if d == 0xFF else "r%d" % d
                    sym = BINOP[o] if o < len(BINOP) else "op%d" % o
                    line = "%s = %s %s %s" % (dst, dst, sym, self.val(t, v))
            elif op == 6:
                line = "pop -> %s" % self.val(ins[1], struct.unpack_from("<h", ins, 2)[0])
            elif op == 7:
                c, off = ins[1], struct.unpack_from("<h", ins, 2)[0]
                line = ("goto L_%04x" % (pc + off)) if c == 0 else \
                       "if (flags %s) goto L_%04x" % (COND.get(c, "bit%d" % c), pc + off)
            elif op == 8:
                line = "call routine_%d" % struct.unpack_from("<h", ins, 2)[0]
            else:
                name, argc, ret = OPS.get(op, ("op%02X" % op, 0, 0))
                args = stack[len(stack) - argc:] if argc <= len(stack) else None
                if args is None:
                    line = "%s(<%d args from stack>)" % (name, argc)
                else:
                    line = "%s(%s)" % (name, ", ".join(a[0] for a in args))
                    calls.append((pc, op, [a[1] for a in args]))
                    if op == 0x19 or op == 0x04:
                        pass
                if ret:
                    line = "r0 = " + line
                if args and op in (0x11, 0x32) and args[-2 if op == 0x32 else 0][1] is not None:
                    toff = args[11][1] if op == 0x32 else args[0][1]
                    if 0 <= toff < len(self.code):
                        line += '   /* "%s" */' % self.text(toff)[:200]
            used = OPS.get(op, ("", 0, 0))[1] if op not in (1, 2, 3, 6, 7, 8) else 0
            rest = stack[:len(stack) - used] if op not in (1, 2, 3, 6, 7) else []
            if rest:
                out.append("    /* also on stack: %s */" % ", ".join(a[0] for a in rest))
            out.append("    %s" % line)
            if op in (4, 0x19) or (op == 7 and ins[1] == 0):
                stack = []
        return "\n".join(out), calls

    def text(self, off):
        return decode_text(self.code, off)


def extract(sc, calls):
    doors, aots, spawns, items = [], [], [], []
    for pc, op, a in calls:
        if op == 0x30:
            idx, fl, lvl = a[0], a[1], a[2]
            q = a[3:11]
            doors.append({"pc": hex(pc), "aot": idx, "flags": fl, "level": lvl,
                          "quad": [[q[0], q[1]], [q[2], q[3]], [q[4], q[5]], [q[6], q[7]]],
                          "dest_pos": a[11:14], "dest_dir": a[14],
                          "dest_room": None if a[15] is None else "ST%X%02X" % (a[15] >> 8, a[15] & 0xFF),
                          "door_anim": a[16], "lock_msg": a[17], "key_flag": a[18]})
        elif 0x31 <= op <= 0x3B and op in OPS:
            q = a[3:11]
            aots.append({"pc": hex(pc), "op": OPS[op][0], "aot": a[0], "flags": a[1], "level": a[2],
                         "quad": [[q[0], q[1]], [q[2], q[3]], [q[4], q[5]], [q[6], q[7]]],
                         "data": a[11:]})
        elif op == 0x1A:
            spawns.append({"pc": hex(pc), "slot": a[0], "type": a[1], "pos": a[2:5], "dir": a[5],
                           "p6": a[6], "anim_or_file": a[7], "p8": a[8], "flag": a[9]})
    return {"doors": doors, "aots": aots, "spawns": spawns}


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    sc = Script(room_block(open(sys.argv[1], "rb").read()))
    lst, calls = sc.listing()
    args = sys.argv[2:]
    if "--lst" in args:
        open(args[args.index("--lst") + 1], "w").write(lst + "\n")
    info = {"routines": [hex(r) for r in sc.routines], "code_bytes": len(sc.code),
            "decoded_bytes": sum(len(i) for i in sc.insns.values()), "globals": sc.globals}
    info.update(extract(sc, calls))
    if "--json" in args or "--lst" in args:
        json.dump(info, sys.stdout, indent=1)
        print()
    else:
        print(lst)


if __name__ == "__main__":
    main()
