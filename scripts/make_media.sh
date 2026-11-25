#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Transcodes test clips from ~/Music into Ogg Opus with ffmpeg's libopus, with DOS 8.3 names, into
# media/demo (or the directory given): a minute of each, small enough for a 1.44 MB floppy.
#   GHOSTS.OPU  Ghosts 'N' Stuff, 128 kbit/s stereo: fullband CELT
#   BADAPPLE.OPU Bad Apple!!, 64 kbit/s stereo: CELT
#   FEARHYB.OPU  Fear Inoculum, 20 kbit/s stereo voip: hybrid (SILK + CELT) packets
#   FEARSILK.OPU Fear Inoculum, 12 kbit/s stereo voip: SILK (wideband) packets
#
# Usage: make_media.sh [out dir]
set -e
here="$(cd "$(dirname "$0")/.." && pwd)"
out="${1:-$here/media/demo}"
music="$HOME/Music"
mkdir -p "$out"

clip() { # <source> <start s> <out> <ffmpeg encoder options...>
    src="$1"; ss="$2"; dst="$3"; shift 3
    ffmpeg -loglevel error -y -ss "$ss" -t 60 -i "$src" -map 0:a:0 -vn -ac 2 -ar 48000 -c:a libopus "$@" -f ogg "$out/$dst"
    printf '%-13s %8d bytes  from %s\n' "$dst" "$(wc -c < "$out/$dst")" "$(basename "$src")"
}

clip "$music/02 Ghosts 'N' Stuff (Original Mix).flac" 60 GHOSTS.OPU -b:a 128k -application audio
clip "$music/01. Bad Apple!!.flac" 30 BADAPPLE.OPU -b:a 64k -application audio
clip "$music/01 - Fear Inoculum.flac" 120 FEARHYB.OPU -b:a 20k -application voip
clip "$music/01 - Fear Inoculum.flac" 120 FEARSILK.OPU -b:a 12k -application voip
