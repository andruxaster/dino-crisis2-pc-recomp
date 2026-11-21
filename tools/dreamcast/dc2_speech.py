#!/usr/bin/env python3
"""dc2_speech.py -- the voices for the Dreamcast build: PCM -> IMA ADPCM 22050 Hz.

The game's SPEECH/NNNN.DAT are PCM waves: the lines at 18900 Hz mono, the
films' sound effects (1000.DAT on, a tyrannosaur's roar among them) at
37800 Hz stereo, 151 KB a second. The Dreamcast build plays each from the
disc as it goes, read in the sound thread under the mixer's lock (src/pc/
audio.c, Snd_OpenFileVoice): at that rate the disc holds up the sound and
the game, and every frame is resampled to the mix's 22050 Hz. Made IMA
ADPCM (4 bits a sample) at 22050 Hz, channels kept, the same as the music:
11 KB a second mono, 22 KB stereo, nothing to resample; 43 MB become ~10.

    dc2_speech.py SPEECH_DIR     # converts in place; the originals are moved
                                 # to SPEECH_DIR/../SPEECH_ORIG/

Needs ffmpeg. Work on a copy: the disc image is made from it. A file
already IMA ADPCM at 22050 Hz is left as it is (run it again safely).
"""
import os, shutil, struct, subprocess, sys

RATE = 22050


def wave_format(path):
    """(format, channels, rate) of a RIFF WAVE file, or None"""
    with open(path, 'rb') as f:
        d = f.read(4096)
    if d[:4] != b'RIFF' or d[8:12] != b'WAVE':
        return None
    i = 12
    while i + 8 <= len(d):
        tag, n = d[i:i + 4], struct.unpack_from('<I', d, i + 4)[0]
        if tag == b'fmt ':
            fmt, ch, rate = struct.unpack_from('<HHI', d, i + 8)
            return fmt, ch, rate
        i += 8 + n + (n & 1)
    return None


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    src = sys.argv[1]
    names = sorted(n for n in os.listdir(src) if n.upper().endswith('.DAT'))
    if not names:
        sys.exit('no NNNN.DAT in %s' % src)
    orig = os.path.join(os.path.dirname(os.path.abspath(src.rstrip('/'))), 'SPEECH_ORIG')
    os.makedirs(orig, exist_ok=True)
    before = after = done = 0
    for n in names:
        path = os.path.join(src, n)
        f = wave_format(path)
        if not f:
            print('skipped (not a wave):', n)
            continue
        if f[0] == 0x11 and f[2] == RATE:
            continue
        keep = os.path.join(orig, n)
        if not os.path.exists(keep):
            shutil.copy2(path, keep)
        tmp = path + '.tmp.wav'
        r = subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', keep, '-ar', str(RATE), '-ac', str(f[1]),
                            '-c:a', 'adpcm_ima_wav', '-f', 'wav', tmp])
        if r.returncode:
            sys.exit('ffmpeg failed on %s' % n)
        os.replace(tmp, path)
        before += os.path.getsize(keep)
        after += os.path.getsize(path)
        done += 1
    print('%d voices converted: %.1f MB -> %.1f MB (originals in %s)' % (done, before / 1e6, after / 1e6, orig))


if __name__ == '__main__':
    main()
