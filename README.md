# OPUS4DOS

An Ogg Opus player for DOS: the whole of Opus (SILK, CELT and hybrid) from
[libopus](https://opus-codec.org/) 1.6.1, in 32-bit protected mode under the DOS/32A extender,
played through a DMA-driven sound card. The first card is the Sound Blaster 16.

libopus is a git submodule (`external/opus`), built unmodified by its own CMake build
(`add_subdirectory`): OPUS4DOS only sets its options (fixed point, so no FPU is needed; no float
API, intrinsics, programs or tests).

## Building

You need OpenWatcom 2 and CMake 3.21 or later. Source OpenWatcom's environment script first;
the `dos` preset (`CMakePresets.json`) makes it a 32-bit DOS build with `wcl386` and the Watcom
WMake generator.

    git submodule update --init
    . ~/bin/watcom/owsetenv.sh
    cmake --preset dos
    cmake --build --preset dos

That gives `build/dos/OPUS4DOS.EXE`, with DOS/32A bound into it.

## Playing

    OPUS4DOS file.opus [/D:driver] [/R:rate] [/M] [/W:file.wav] [/L] [/Q]

| Option | |
|---|---|
| `/D:name` | the sound card's driver; by default, the first that finds its card |
| `/R:rate` | the output rate; by default the card's highest, up to 48000 |
| `/M` | mono |
| `/W:file` | also write what goes to the card to a WAV file (for checking) |
| `/L` | list the drivers |
| `/Q` | no status line |

Space pauses, Esc or Q stops. The status line shows the time, how much of the CPU decoding
takes, and how many DMA blocks the card found empty (underruns).

The player decodes at 48 kHz, or at the card's rate when that's one Opus gives (24, 16, 12 or
8 kHz); otherwise (the Sound Blaster 16 stops at 44.1 kHz) it resamples, by linear
interpolation. It plays mono and stereo files (channel mapping family 0), applies the header's
output gain, skips the pre-skip, and trims the end to the last page's granule position.

## Sound cards

`src/audio.c` is the layer the player writes to. Every card it drives works the same way: a
buffer of `AUDIO_BLOCKS` blocks in DOS memory, played round and round by the card's DMA channel
(auto-init), the card interrupting at the end of each block. That layer allocates the buffer (not
crossing a 64 or 128 KB DMA boundary), converts the player's 16-bit samples to what the card
takes (8-bit unsigned, mono), and keeps the player ahead of the card. A driver is a
`struct audio_driver` (probe, open, start, pause, stop, close) and a line in `audio_drivers[]`;
its interrupt handler acknowledges the card and calls `audio_block_done()`. The 8237 DMA
controllers (`isadma.c`), IRQ hooking (`irq.c`) and DOS memory (`dosmem.c`) are shared.

| Driver | Card | |
|---|---|---|
| `sb16` | Sound Blaster 16 (DSP 4.x) | 16-bit stereo, 5000-44100 Hz, high DMA, from `BLASTER` |

## Testing in emulators

`scripts/make_media.sh` makes test clips from `~/Music` into `media/demo`: a minute each of
fullband CELT (128 and 64 kbit/s), hybrid (20 kbit/s) and SILK (12 kbit/s).

    scripts/dosbox_play.sh build/dos/OPUS4DOS.EXE build/play/dosbox media/demo/*.OPU
    scripts/86box_play.py <86Box> build/dos/OPUS4DOS.EXE build/play/86box media/demo/GHOSTS.OPU
    scripts/compare_capture.py build/play/dosbox/dosbox.wav build/play/dosbox/GHOSTS.WAV

Each records what the emulator's sound card played: DOSBox through SDL's `disk` audio driver,
86Box (built with `AUTOMATION=ON`) through its automation's `audio start`, in emulated time.
`compare_capture.py` finds the music OPUS4DOS sent (its `/W` copy) in the recording and prints
the offset, the mixer's gain, the correlation and the SNR.

## Other things here

- `scripts/d32cfg.py` sets DOS/32A's built-in configuration in an executable (object loading
  scheme #1 runs a program on a machine without extended memory).
- `scripts/vm/floppymbr.asm` is the MBR of `86box_play.py`'s test disk.
- `media/` isn't in git: the test audio is made from commercial recordings, by
  `scripts/make_media.sh` from a local music library.

Decoders tuned for particular CPUs (a SILK-only 386 decoder, later 486 and Pentium ones) are
worked on as branches of a libopus clone in `~/Projects/opus-x86`, with its test bench (OPUSDOS),
profiling scripts, notes and measurements in its `x86-work/`; OPUS4DOS builds stock libopus.

## License

OPUS4DOS is BSD 3-clause licensed, as libopus is: `LICENSE`. `OPUS4DOS.EXE` also contains libopus (BSD 3-clause,
`external/opus/COPYING`), the DOS/32 Advanced DOS Extender (BSD-style) and OpenWatcom's C
library: `NOTICE` has their terms, which ask that their notices go with any copy of the
executable that is passed on. This product uses DOS/32 Advanced DOS Extender technology.
