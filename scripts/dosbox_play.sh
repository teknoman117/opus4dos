#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Plays Opus files with OPUS4DOS in DOSBox (SB16, dynamic core, max cycles), headless, and records
# what DOSBox sends to the sound card's output: SDL's "disk" audio driver writes the mix to a raw
# file, which becomes <out>/dosbox.wav (44.1 kHz stereo). OPUS4DOS's console output goes to
# <out>/OPUS4DOS.TXT, and each file's /W copy of what it sent to the card to <out>/<NAME>.WAV.
#
# Usage: [DOSBOX_CYCLES=n] dosbox_play.sh <OPUS4DOS.EXE> <out dir> <file.opu>...   (default: max cycles)
set -e
exe="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
out="$2"
shift 2
mkdir -p "$out"
out="$(cd "$out" && pwd)"
work="$(mktemp -d)"
cp "$exe" "$work/OPUS4DOS.EXE"
{
    echo "@ECHO OFF"
    echo "ECHO %BLASTER% > OPUS4DOS.TXT"
    for f in "$@"; do
        b="$(basename "$f")"
        cp "$f" "$work/$b"
        echo "OPUS4DOS $b /W:${b%.*}.WAV >> OPUS4DOS.TXT"
    done
    echo "EXIT"
} | sed 's/$/\r/' > "$work/RUN.BAT"
cat > "$work/dosbox.conf" <<EOF
[sdl]
output=surface
[dosbox]
machine=svga_s3
memsize=32
[cpu]
core=dynamic
cputype=auto
cycles=${DOSBOX_CYCLES:-max}
[mixer]
rate=44100
blocksize=1024
prebuffer=40
[sblaster]
sbtype=sb16
sbbase=220
irq=7
dma=1
hdma=5
oplmode=none
[speaker]
pcspeaker=false
tandy=off
[autoexec]
EOF
# SDL 1.2's disk driver: write each 1024-frame mixer block, then sleep its length (23 ms), so
# DOSBox's mixer runs at about real time.
(cd "$work" && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE="$work/dosbox.raw" \
    SDL_DISKAUDIODELAY=23 dosbox -noconsole -conf "$work/dosbox.conf" -c "mount c $work" -c "c:" \
    -c "RUN.BAT" -c "exit" > /dev/null 2>&1) || true
ffmpeg -loglevel error -y -f s16le -ar 44100 -ac 2 -i "$work/dosbox.raw" "$out/dosbox.wav"
tr -d '\r' < "$work/OPUS4DOS.TXT" | sed 's/.*\r//' > "$out/OPUS4DOS.TXT" || true
for f in "$@"; do
    b="$(basename "$f")"
    [ -f "$work/${b%.*}.WAV" ] && cp "$work/${b%.*}.WAV" "$out/"
done
rm -rf "$work"
ls -l "$out"
