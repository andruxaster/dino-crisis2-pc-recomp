#!/bin/sh
# dc2_movies.sh <movie-dir> - the movies for the Dreamcast build: MPEG-1
# (MOVIE/NAME.DAT, too heavy for the SH-4) -> Cinepak AVI (MOVIE/NAME.AVI)
# with IMA ADPCM sound at 22050 Hz, same size and frame rate. The Dreamcast
# build plays only the .AVI (src/pc/movie_avi.c); the PC builds keep playing
# the .DAT. Work on a copy of your MOVIE folder (the disc is made from it);
# --remove-dat deletes each .DAT once its .AVI is made (disc space).
# Needs ffmpeg with the cinepak encoder.
set -e
DIR=${1:?usage: dc2_movies.sh <movie-dir> [--remove-dat]}
command -v ffmpeg >/dev/null || { echo "ffmpeg not found"; exit 1; }
ffmpeg -hide_banner -encoders 2>/dev/null | grep -q ' cinepak ' || { echo "this ffmpeg has no cinepak encoder"; exit 1; }
for f in "$DIR"/*.DAT "$DIR"/*.dat; do
    [ -f "$f" ] || continue
    out="${f%.*}.AVI"
    echo "$(basename "$f") -> $(basename "$out")"
    ffmpeg -v error -y -i "$f" -map 0:v:0 -map 0:a:0? -c:v cinepak -max_strips 4 \
           -c:a adpcm_ima_wav -ar 22050 -ac 2 "$out"
    [ "$2" = "--remove-dat" ] && rm -f "$f"
done
du -sh "$DIR"
