/* SPDX-License-Identifier: BSD-3-Clause */
/* PCM output through DMA-driven sound cards.
 *
 * The cards share one scheme: a buffer in DOS memory of AUDIO_BLOCKS blocks, which the card's
 * DMA channel plays round and round (auto-init), the card interrupting at the end of each block.
 * The player writes 16-bit samples ahead of the card a block at a time; this layer converts them
 * to what the card takes (8-bit unsigned, mono) and does the bookkeeping, so a driver has only
 * its card to look after. Adding a card is a struct audio_driver and a line in audio.c.
 */
#ifndef OPUS4DOS_AUDIO_H
#define OPUS4DOS_AUDIO_H

#define AUDIO_BLOCKS 4

typedef struct {
    long rate;      /* frames per second */
    int  channels;  /* 1 or 2 */
    int  bits;      /* 8 (unsigned) or 16 (signed) */
} audio_format;

typedef struct audio_driver {
    const char *name; /* for /D: */
    const char *desc;
    /* Is the card there? 0 if so, with what was found in info. */
    int (*probe)(char *info, int info_len);
    /* Takes the card, and makes *fmt (what the player would like) what the card will play;
       *dma_bits is 8 or 16, the width of the DMA channel the buffer goes through. 0, or -1. */
    int (*open)(audio_format *fmt, int *dma_bits);
    /* Starts the card on the buffer at phys, `bytes` long, interrupting every block_bytes,
       each time calling audio_block_done(). */
    int (*start)(unsigned long phys, unsigned long bytes, unsigned long block_bytes);
    void (*pause)(int paused);
    void (*stop)(void);   /* stops the card; start may be called again */
    void (*close)(void);  /* lets the card go */
} audio_driver;

/* From a driver's interrupt handler: the card has finished a block. */
void audio_block_done(void);

/* The drivers, by name; NULL ends the list. */
extern const audio_driver *const audio_drivers[];

/* Opens a card: the driver called name, or the first that finds its card, for *fmt (adjusted to
   what it will play), with a buffer of AUDIO_BLOCKS blocks of block_frames frames. 0, or -1 with
   why in audio_error(). */
int         audio_open(const char *name, audio_format *fmt, long block_frames);
const char *audio_error(void);
const char *audio_describe(void); /* the driver and what it found */

/* How many frames can be written now without waiting. */
long audio_space(void);

/* Writes n frames of 16-bit samples (fmt->channels interleaved), waiting for room as needed,
   calling idle() while it waits if that isn't NULL. Playing starts once the buffer is full. */
void audio_write(const short *pcm, long n, void (*idle)(void));

/* Pads the last block with silence, starts the card if it hasn't, and waits until it has played
   everything written, calling idle() while it waits. */
void audio_drain(void (*idle)(void));

void audio_pause(int paused);

/* Frames of music the card has played; blocks it found unwritten (and played as silence). */
unsigned long audio_frames_played(void);
unsigned long audio_underruns(void);

void audio_close(void);

#endif
