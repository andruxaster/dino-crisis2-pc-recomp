#!/usr/bin/env python3
"""dc2_music.py -- the room music for the Dreamcast build: MP3 -> IMA ADPCM.

The PC game keeps each area's music as MP3 streams (record type 4) inside
Data/MS_*.DAT and loads them whole into memory (8 MB and more). The Dreamcast
build (DC2_LOWMEM) leaves those records on the disc and plays the stream from
Data/BGM/ instead: each distinct MP3 once, as a WAV with IMA ADPCM (4 bits a
sample) at 22050 Hz stereo, about 22 KB a second, read from the disc as it
plays (src/pc/audio.c, Strm_OpenFile).

    dc2_music.py DATA_DIR            # writes DATA_DIR/BGM/*.WAV and BGM/INDEX.TXT
    dc2_music.py DATA_DIR --strip    # also cuts MS_*.DAT down to their 2 KB header
                                     # (the originals are moved to DATA_DIR/../MS_ORIG/)

BGM/INDEX.TXT has one line per stream record: "MS_0001 3 1A2B3C4D" (file,
record number in its header, the WAV's name). Needs ffmpeg. Work on a copy
of your Data folder: the disc image is made from it.
"""
import hashlib, os, shutil, struct, subprocess, sys

RATE = 22050
DUMM = 0x6D6D7564      # "dumm": the end of a container's record list


def records(path):
    """the container's records: (index, type, size, offset of the data)"""
    with open(path, 'rb') as f:
        hdr = f.read(0x800)
    out, off = [], 0x800
    for i in range(0x800 // 0x20):
        t, size = struct.unpack_from('<II', hdr, i * 0x20)
        if t == DUMM or (t == 0 and size == 0):
            break
        out.append((i, t, size, off))
        off += (size + 0x7FF) & ~0x7FF
    return out


def convert(mp3, dst):
    r = subprocess.run(['ffmpeg', '-v', 'error', '-y', '-f', 'mp3', '-i', 'pipe:0', '-ar', str(RATE), '-ac', '2',
                        '-c:a', 'adpcm_ima_wav', '-f', 'wav', dst], input=mp3)
    if r.returncode:
        sys.exit('ffmpeg failed on %s' % dst)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    if len(args) != 1:
        sys.exit(__doc__)
    data = args[0]
    strip = '--strip' in sys.argv
    names = sorted(n for n in os.listdir(data) if n.upper().startswith('MS_') and n.upper().endswith('.DAT'))
    if not names:
        sys.exit('no MS_*.DAT in %s' % data)
    bgm = os.path.join(data, 'BGM')
    os.makedirs(bgm, exist_ok=True)
    index, done = [], {}
    for n in names:
        p = os.path.join(data, n)
        if os.path.getsize(p) <= 0x800:
            sys.exit('%s is already cut down to its header: run on the original files' % n)
        with open(p, 'rb') as f:
            for i, t, size, off in records(p):
                if t != 4:
                    continue
                f.seek(off)
                mp3 = f.read(size)
                h = hashlib.sha1(mp3).hexdigest()[:8].upper()
                if h not in done:
                    dst = os.path.join(bgm, h + '.WAV')
                    print('%s record %d -> BGM/%s.WAV' % (n, i, h))
                    convert(mp3, dst)
                    done[h] = os.path.getsize(dst)
                index.append('%s %d %s' % (os.path.splitext(n)[0].upper(), i, h))
    with open(os.path.join(bgm, 'INDEX.TXT'), 'w', newline='\n') as f:
        f.write('\n'.join(index) + '\n')
    print('%d streams, %d distinct, %.1f MB of ADPCM' % (len(index), len(done), sum(done.values()) / 1e6))
    if strip:
        keep = os.path.join(os.path.dirname(os.path.abspath(data)), 'MS_ORIG')
        os.makedirs(keep, exist_ok=True)
        for n in names:
            p = os.path.join(data, n)
            shutil.move(p, os.path.join(keep, n))
            with open(os.path.join(keep, n), 'rb') as f, open(p, 'wb') as g:
                g.write(f.read(0x800))
        print('MS_*.DAT cut to their headers; the originals are in %s' % keep)


if __name__ == '__main__':
    main()
