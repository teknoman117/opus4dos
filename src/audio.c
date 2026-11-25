/* SPDX-License-Identifier: BSD-3-Clause */
#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "dosmem.h"

extern const audio_driver sb16_driver;

const audio_driver *const audio_drivers[] = {
    &sb16_driver,
    NULL,
};

static const audio_driver *drv;
static audio_format        fmt;
static dos_block           block;
static unsigned char      *buf;          /* the DMA buffer */
static long                block_frames;
static unsigned long       block_bytes;
static int                 frame_bytes;
static int                 started;
static char                error[128];
static char                description[160];

/* Blocks the card has finished, counted by its interrupt; blocks the player has filled. The
   card is playing block number `played` (mod AUDIO_BLOCKS); the player may fill the blocks
   after it, up to AUDIO_BLOCKS - 1 ahead. */
static volatile unsigned long played;
static volatile unsigned long filled;
static long                   fill_pos; /* frames already in block number `filled` */
static unsigned long          underruns;

void
audio_block_done(void)
{
    played++;
    /* The block the card goes on to hasn't been written yet (the player is behind): silence it,
       so the card plays a gap rather than what was there a buffer ago. wait_room() then puts
       the music in the block after it, so none is skipped. */
    if (played >= filled)
        memset(buf + (played % AUDIO_BLOCKS) * block_bytes, (fmt.bits == 8) ? 0x80 : 0, block_bytes);
}

const char *
audio_error(void)
{
    return error;
}

const char *
audio_describe(void)
{
    return description;
}

int
audio_open(const char *name, audio_format *want, long frames_per_block)
{
    int  i, dma_bits;
    char info[96];

    drv = NULL;
    for (i = 0; audio_drivers[i]; i++) {
        if (name && strcmp(name, audio_drivers[i]->name))
            continue;
        info[0] = 0;
        if (audio_drivers[i]->probe(info, sizeof(info)) == 0) {
            drv = audio_drivers[i];
            break;
        }
        if (name) {
            snprintf(error, sizeof(error), "%s: %s", audio_drivers[i]->name, info[0] ? info : "no card found");
            return -1;
        }
    }
    if (!drv) {
        snprintf(error, sizeof(error), name ? "no driver called %s" : "no sound card found", name);
        return -1;
    }
    fmt = *want;
    if (drv->open(&fmt, &dma_bits)) {
        snprintf(error, sizeof(error), "%s: can't open the card", drv->name);
        return -1;
    }
    block_frames = frames_per_block;
    frame_bytes  = fmt.channels * (fmt.bits / 8);
    block_bytes  = (unsigned long) block_frames * frame_bytes;
    {
        unsigned long phys = dos_alloc_dma(&block, block_bytes * AUDIO_BLOCKS,
                                           (dma_bits == 16) ? 0x20000ul : 0x10000ul);
        if (!phys) {
            drv->close();
            snprintf(error, sizeof(error), "no DOS memory for a %lu byte DMA buffer", block_bytes * AUDIO_BLOCKS);
            return -1;
        }
        buf = (unsigned char *) phys;
    }
    memset(buf, (fmt.bits == 8) ? 0x80 : 0, block_bytes * AUDIO_BLOCKS);
    played = filled = underruns = 0;
    fill_pos = 0;
    started  = 0;
    snprintf(description, sizeof(description), "%s (%s): %ld Hz, %d-bit %s", drv->desc, info, fmt.rate, fmt.bits,
             (fmt.channels == 2) ? "stereo" : "mono");
    *want = fmt;
    return 0;
}

static void
start(void)
{
    if (!started) {
        drv->start((unsigned long) buf, block_bytes * AUDIO_BLOCKS, block_bytes);
        started = 1;
    }
}

long
audio_space(void)
{
    long blocks = started ? (long) (played + AUDIO_BLOCKS - filled) : (long) (AUDIO_BLOCKS - filled);

    return (blocks > 0) ? blocks * block_frames - fill_pos : 0;
}

/* Waits until block number `filled` may be written. */
static void
wait_room(void (*idle)(void))
{
    if (!started)
        return;
    while (filled > played + AUDIO_BLOCKS - 1) {
        if (idle)
            idle();
    }
    if (filled <= played) {
        /* The card got to this block before it was written, and is playing it (as silence):
           start again in the next one, with what's been written of this one. */
        unsigned long now = played;
        if (fill_pos)
            memmove(buf + ((now + 1) % AUDIO_BLOCKS) * block_bytes, buf + (filled % AUDIO_BLOCKS) * block_bytes,
                    (unsigned long) fill_pos * frame_bytes);
        underruns += now - filled + 1;
        filled = now + 1;
    }
}

static void
put(const short *pcm, long n)
{
    unsigned char *p = buf + (filled % AUDIO_BLOCKS) * block_bytes + (unsigned long) fill_pos * frame_bytes;
    long           i, samples = n * fmt.channels;

    if (fmt.bits == 16) {
        memcpy(p, pcm, samples * 2);
    } else {
        for (i = 0; i < samples; i++)
            p[i] = (unsigned char) ((pcm[i] >> 8) + 128);
    }
}

static void
next_block(void)
{
    fill_pos = 0;
    filled++;
    if (!started && (filled == AUDIO_BLOCKS))
        start();
}

void
audio_write(const short *pcm, long n, void (*idle)(void))
{
    while (n > 0) {
        long k;

        wait_room(idle);
        k = block_frames - fill_pos;
        if (k > n)
            k = n;
        put(pcm, k);
        pcm += k * fmt.channels;
        n -= k;
        fill_pos += k;
        if (fill_pos == block_frames)
            next_block();
    }
}

void
audio_drain(void (*idle)(void))
{
    static const short silence[256 * 2];
    unsigned long      last;
    int                i;

    /* The rest of the last block, then one more block of silence, so that once the card has
       finished the last block it plays silence while it's being stopped. */
    for (i = 0; i < 2; i++) {
        long n = block_frames - fill_pos;
        while (n > 0) {
            long k = (n > 256) ? 256 : n;
            audio_write(silence, k, idle);
            n -= k;
        }
    }
    last = filled - 1; /* the silent block */
    while (!started)
        audio_write(silence, (block_frames < 256) ? block_frames : 256, idle);
    while (played < last) {
        if (idle)
            idle();
    }
    drv->stop();
    started = 0;
}

void
audio_pause(int paused)
{
    if (started)
        drv->pause(paused);
}

unsigned long
audio_frames_played(void)
{
    /* The blocks the card played as silence while the player was behind aren't music. */
    unsigned long p = played, music = (p > underruns) ? p - underruns : 0;

    return music * (unsigned long) block_frames;
}

unsigned long
audio_underruns(void)
{
    return underruns;
}

void
audio_close(void)
{
    if (!drv)
        return;
    if (started)
        drv->stop();
    drv->close();
    dos_free(&block);
    drv     = NULL;
    started = 0;
}
