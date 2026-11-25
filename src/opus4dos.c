/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * OPUS4DOS - plays Ogg Opus files on DOS, through a DMA-driven sound card.
 *
 * Usage: OPUS4DOS file.opus [/D:driver] [/R:rate] [/M] [/W:file.wav] [/L] [/Q]
 *
 * The whole of Opus (SILK, CELT and hybrid, from libopus, unmodified) is decoded a packet at a
 * time as the file is read, and written to the card's DMA buffer ahead of where it's playing.
 * The card's rate is the highest it can do up to 48 kHz; if that isn't one the decoder gives
 * (48, 24, 16, 12, 8 kHz), the decoder's 48 kHz is resampled to it.
 */
#include <conio.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "audio.h"
#include "ogg.h"
#include "opus.h"
#include "resample.h"

#define VERSION   "0.1"
#define MAX_FRAME 5760 /* 120 ms at 48 kHz */
#define BLOCK     2048 /* frames per DMA block: AUDIO_BLOCKS of them, ~190 ms at 44.1 kHz */

static short     pcm[MAX_FRAME * 2];
static short     rs_out[(MAX_FRAME + 4) * 2];
static int       quit, paused, quiet;
static clock_t   busy_clocks; /* time spent decoding, for the CPU load */
static FILE     *wav;
static unsigned long wav_bytes;

static void
usage(void)
{
    int i;

    printf("OPUS4DOS " VERSION " - Ogg Opus player for DOS (%s)\n", opus_get_version_string());
    printf("BSD 3-clause licensed, as libopus is. This product uses DOS/32 Advanced DOS Extender\n"
           "technology. See NOTICE.\n\n");
    printf("Usage: OPUS4DOS file.opus [options]\n"
           "  /D:name   the sound card's driver (default: the first that finds its card)\n"
           "  /R:rate   output rate (default: the card's highest, up to 48000)\n"
           "  /M        mono\n"
           "  /W:file   also write what goes to the card to a WAV file\n"
           "  /L        list the drivers\n"
           "  /Q        quiet: no status line\n"
           "Keys while playing: Space pauses, Esc or Q stops.\n\nDrivers:");
    for (i = 0; audio_drivers[i]; i++)
        printf(" %s (%s)", audio_drivers[i]->name, audio_drivers[i]->desc);
    printf("\n");
}

static void
put16(FILE *f, unsigned v)
{
    fputc(v & 0xff, f);
    fputc((v >> 8) & 0xff, f);
}

static void
put32(FILE *f, unsigned long v)
{
    put16(f, (unsigned) (v & 0xffff));
    put16(f, (unsigned) (v >> 16));
}

static void
wav_header(FILE *f, long rate, int channels, unsigned long bytes)
{
    fwrite("RIFF", 1, 4, f);
    put32(f, 36 + bytes);
    fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16);
    put16(f, 1);
    put16(f, channels);
    put32(f, rate);
    put32(f, rate * channels * 2);
    put16(f, channels * 2);
    put16(f, 16);
    fwrite("data", 1, 4, f);
    put32(f, bytes);
}

/* While waiting for the card, and between packets: the keyboard. */
static void
idle(void)
{
    while (kbhit()) {
        int c = getch();
        if ((c == 0) || (c == 0xe0)) {
            getch(); /* an extended key's second byte */
        } else if ((c == 27) || (c == 'q') || (c == 'Q')) {
            quit = 1;
        } else if (c == ' ') {
            audio_pause(1);
            paused = 1;
            while (paused && !quit) {
                c = getch();
                if ((c == 27) || (c == 'q') || (c == 'Q'))
                    quit = 1;
                else if (c == ' ')
                    paused = 0;
            }
            audio_pause(0);
        }
    }
}

static void
output(const short *frames, long n, int channels)
{
    audio_write(frames, n, idle);
    if (wav) {
        fwrite(frames, 2 * channels, n, wav);
        wav_bytes += (unsigned long) n * 2 * channels;
    }
}

static void
print_tags(const unsigned char *p, unsigned long len)
{
    unsigned long off = 8, vendor, count, i;

    if ((len < 16) || memcmp(p, "OpusTags", 8))
        return;
    vendor = p[off] | ((unsigned long) p[off + 1] << 8) | ((unsigned long) p[off + 2] << 16) | ((unsigned long) p[off + 3] << 24);
    off += 4 + vendor;
    if (off + 4 > len)
        return;
    count = p[off] | ((unsigned long) p[off + 1] << 8) | ((unsigned long) p[off + 2] << 16) | ((unsigned long) p[off + 3] << 24);
    off += 4;
    for (i = 0; (i < count) && (off + 4 <= len); i++) {
        unsigned long n = p[off] | ((unsigned long) p[off + 1] << 8) | ((unsigned long) p[off + 2] << 16) | ((unsigned long) p[off + 3] << 24);
        static const char *const tags[][2] = { { "TITLE=", "Title" }, { "ARTIST=", "Artist" }, { "ALBUM=", "Album" } };
        int                      t;
        off += 4;
        if (off + n > len)
            break;
        for (t = 0; t < 3; t++) {
            size_t k = strlen(tags[t][0]);
            if ((n > k) && !strnicmp((const char *) p + off, tags[t][0], k)) {
                printf("  %s: %.*s\n", tags[t][1], (int) (n - k), p + off + k);
                break;
            }
        }
        off += n;
    }
}

static void
status(long long frames, long rate, long long total, long long decoded48)
{
    long s  = (long) (frames / rate), ts = (total > 0) ? (long) (total / 48000) : 0;
    long ms = (long) (decoded48 / 48);
    int  cpu;

    /* The time spent decoding against the audio decoded, in ms. */
    cpu = (ms > 0) ? (int) ((long long) busy_clocks * 1000 / CLOCKS_PER_SEC * 100 / ms) : 0;
    if (ts)
        printf("\r  %02ld:%02ld / %02ld:%02ld   CPU %3d%%   underruns %lu  ", s / 60, s % 60, ts / 60, ts % 60, cpu,
               audio_underruns());
    else
        printf("\r  %02ld:%02ld   CPU %3d%%   underruns %lu  ", s / 60, s % 60, cpu, audio_underruns());
    fflush(stdout);
}

int
main(int argc, char **argv)
{
    const char      *file = NULL, *driver = NULL, *wav_name = NULL;
    long             want_rate = 48000;
    int              mono = 0, i, err, channels, preskip, gain;
    ogg_reader       ogg;
    audio_format     fmt;
    OpusDecoder     *dec;
    long             dec_rate;
    int              resampling;
    resampler        rs;
    long long        total48, decoded48 = 0;
    long long        skip;
    clock_t          last_status = 0;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if ((a[0] == '/') || (a[0] == '-')) {
            char o = (char) toupper((unsigned char) a[1]);
            const char *v = (a[2] == ':') ? a + 3 : a + 2;
            if (o == 'D')
                driver = v;
            else if (o == 'R')
                want_rate = atol(v);
            else if (o == 'M')
                mono = 1;
            else if (o == 'W')
                wav_name = v;
            else if (o == 'Q')
                quiet = 1;
            else if (o == 'L') {
                usage();
                return 0;
            } else {
                usage();
                return 1;
            }
        } else {
            file = a;
        }
    }
    if (!file) {
        usage();
        return 1;
    }

    if (ogg_open(&ogg, file)) {
        printf("OPUS4DOS: can't open %s\n", file);
        return 1;
    }
    if (ogg_packet(&ogg) || (ogg.pkt_len < 19) || memcmp(ogg.pkt, "OpusHead", 8)) {
        printf("OPUS4DOS: %s isn't an Ogg Opus file\n", file);
        return 1;
    }
    channels = ogg.pkt[9];
    preskip  = ogg.pkt[10] | (ogg.pkt[11] << 8);
    gain     = (short) (ogg.pkt[16] | (ogg.pkt[17] << 8));
    if ((ogg.pkt[18] != 0) || (channels < 1) || (channels > 2)) {
        printf("OPUS4DOS: %s has %d channels (mapping family %d); only mono and stereo are played\n", file,
               channels, ogg.pkt[18]);
        return 1;
    }
    total48 = ogg_last_granule(file, ogg.serial);
    if (total48 > 0)
        total48 -= preskip;

    printf("OPUS4DOS " VERSION " (%s; uses DOS/32 Advanced DOS Extender technology)\n", opus_get_version_string());
    printf("%s: %s, %ld Hz source\n", file, (channels == 2) ? "stereo" : "mono",
           (long) (ogg.pkt[12] | ((unsigned long) ogg.pkt[13] << 8) | ((unsigned long) ogg.pkt[14] << 16) |
                   ((unsigned long) ogg.pkt[15] << 24)));
    if (ogg_packet(&ogg) == 0)
        print_tags(ogg.pkt, ogg.pkt_len);

    fmt.rate     = want_rate;
    fmt.channels = mono ? 1 : 2;
    fmt.bits     = 16;
    if (audio_open(driver, &fmt, BLOCK)) {
        printf("OPUS4DOS: %s\n", audio_error());
        return 1;
    }
    printf("Card: %s\n", audio_describe());

    switch (fmt.rate) {
        case 48000: case 24000: case 16000: case 12000: case 8000:
            dec_rate   = fmt.rate;
            resampling = 0;
            break;
        default:
            dec_rate   = 48000;
            resampling = 1;
            resample_init(&rs, 48000, fmt.rate, fmt.channels);
            printf("Decoding at 48000 Hz, resampled to %ld Hz\n", fmt.rate);
            break;
    }
    dec = opus_decoder_create((opus_int32) dec_rate, fmt.channels, &err);
    if (err != OPUS_OK) {
        printf("OPUS4DOS: opus_decoder_create: %s\n", opus_strerror(err));
        audio_close();
        return 1;
    }
    if (gain)
        opus_decoder_ctl(dec, OPUS_SET_GAIN(gain));
    if (wav_name) {
        wav = fopen(wav_name, "wb");
        if (wav)
            wav_header(wav, fmt.rate, fmt.channels, 0);
        else
            printf("OPUS4DOS: can't write %s\n", wav_name);
    }

    skip = (long long) preskip * dec_rate / 48000;
    while (!quit && (ogg_packet(&ogg) == 0)) {
        clock_t   t0 = clock();
        int       n  = opus_decode(dec, ogg.pkt, (opus_int32) ogg.pkt_len, pcm, MAX_FRAME, 0);
        long      keep;
        long long n48;

        if (n < 0) {
            printf("\nOPUS4DOS: a damaged packet: %s\n", opus_strerror(n));
            continue;
        }
        keep = n;
        n48  = (long long) n * 48000 / dec_rate;
        /* The last page's granule position says where the stream ends. */
        if (ogg.eos && (ogg.granule >= 0) && (decoded48 + n48 > ogg.granule)) {
            long long over = decoded48 + n48 - ogg.granule;
            keep -= (long) (over * dec_rate / 48000);
            if (keep < 0)
                keep = 0;
        }
        decoded48 += n48;
        {
            const short *p = pcm;
            if (skip > 0) {
                long s = (skip > keep) ? keep : (long) skip;
                p += s * fmt.channels;
                keep -= s;
                skip -= s;
            }
            if (keep > 0) {
                if (resampling) {
                    long m = resample(&rs, p, keep, rs_out);
                    busy_clocks += clock() - t0;
                    output(rs_out, m, fmt.channels);
                } else {
                    busy_clocks += clock() - t0;
                    output(p, keep, fmt.channels);
                }
            }
        }
        if (!quiet && (clock() - last_status >= CLOCKS_PER_SEC / 4)) {
            status((long long) audio_frames_played(), fmt.rate, total48, decoded48);
            last_status = clock();
        }
        if (kbhit())
            idle();
    }
    if (!quit)
        audio_drain(idle);
    if (!quiet) {
        status((long long) audio_frames_played(), fmt.rate, total48, decoded48);
        printf("\n");
    }
    audio_close();
    opus_decoder_destroy(dec);
    ogg_close(&ogg);
    if (wav) {
        fseek(wav, 0, SEEK_SET);
        wav_header(wav, fmt.rate, fmt.channels, wav_bytes);
        fclose(wav);
    }
    return 0;
}
