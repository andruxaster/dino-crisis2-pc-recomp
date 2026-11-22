#!/usr/bin/env python3
"""Name the profiler's hot spots in a Dreamcast log (src/port/dc/prof.c).

    tools/dreamcast/dc2_prof.py LOG [ELF]

ELF: the build the log came from (default build/dc/dinocrisis2.elf); its
symbols come from sh-elf-nm (on PATH, or $KOS_CC_BASE/bin). The last report
of the log is used (the counts add up over the run). For each histogram the
functions are listed with their share of the samples, with the addresses
in them, so that a hot loop can be found with sh-elf-objdump."""
import bisect, os, re, shutil, subprocess, sys
from collections import defaultdict


def symbols(elf):
    nm = shutil.which("sh-elf-nm") or os.path.join(os.environ.get("KOS_CC_BASE", "/opt/toolchains/dc/sh-elf"), "bin", "sh-elf-nm")
    out = subprocess.run([nm, "-n", elf], capture_output=True, text=True, check=True).stdout
    addrs, names = [], []
    for line in out.splitlines():
        p = line.split()
        if len(p) == 3 and p[1] in "tTwW":
            a = int(p[0], 16)
            if addrs and addrs[-1] == a:
                continue
            addrs.append(a); names.append(p[2][1:] if p[2].startswith("_") else p[2])
    return addrs, names


def main():
    if len(sys.argv) < 2:
        print(__doc__); return 2
    log = open(sys.argv[1], errors="replace").read()
    elf = sys.argv[2] if len(sys.argv) > 2 else "build/dc/dinocrisis2.elf"
    addrs, names = symbols(elf)
    last = {}
    for m in re.finditer(r"^prof (\S+) (\d+):(.*)$", log, re.M):
        last[m.group(1)] = (int(m.group(2)), [(int(a, 16), int(c)) for a, c in re.findall(r"([0-9A-F]+):(\d+)", m.group(3))])
    if not last:
        print("no profiler report in the log (it comes every 1800 frames)"); return 1
    for what in ("all", "slow", "slow-callers", "stall", "stall-callers"):
        if what not in last:
            continue
        total, buckets = last[what]
        fn = defaultdict(int); where = defaultdict(list)
        for a, c in buckets:
            i = bisect.bisect_right(addrs, a) - 1
            n = names[i] if i >= 0 else "?"
            fn[n] += c; where[n].append("%X" % a)
        print("%s: %d samples (%.1f s)%s" % (what, total, total / 1000.0,
              "; who called the code that was running" if what.endswith("-callers") else ""))
        for n, c in sorted(fn.items(), key=lambda x: -x[1]):
            print("  %5.1f%%  %-40s %s" % (100.0 * c / max(total, 1), n, " ".join(where[n][:6])))
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
