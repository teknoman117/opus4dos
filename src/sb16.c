/* SPDX-License-Identifier: BSD-3-Clause */
/* Creative Sound Blaster 16 (DSP 4.x): 16-bit signed PCM, mono or stereo, 5000-44100 Hz, played
   with the DSP's auto-init 16-bit output (B6h) through the high (16-bit) DMA channel. The card is
   found from the BLASTER variable (A220 I5 D1 H5 T6), else at those defaults. */
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "irq.h"
#include "isadma.h"

static unsigned base = 0x220;
static int      irq = 5, dma8 = 1, dma16 = 5;
static int      stereo;
static long     rate;
static int      dsp_major, dsp_minor;

static void
blaster(void)
{
    const char *s = getenv("BLASTER");

    while (s && *s) {
        char c = *s++;
        switch (c) {
            case 'A': case 'a': base = (unsigned) strtoul(s, NULL, 16); break;
            case 'I': case 'i': irq = (int) strtol(s, NULL, 10); break;
            case 'D': case 'd': dma8 = (int) strtol(s, NULL, 10); break;
            case 'H': case 'h': dma16 = (int) strtol(s, NULL, 10); break;
        }
        while (*s && (*s != ' '))
            s++;
        while (*s == ' ')
            s++;
    }
}

static int
dsp_write(unsigned char v)
{
    long n;

    for (n = 0; n < 100000L; n++) {
        if (!(inp(base + 0xc) & 0x80)) {
            outp(base + 0xc, v);
            return 0;
        }
    }
    return -1;
}

static int
dsp_read(void)
{
    long n;

    for (n = 0; n < 100000L; n++) {
        if (inp(base + 0xe) & 0x80)
            return inp(base + 0xa);
    }
    return -1;
}

static int
dsp_reset(void)
{
    int i;

    outp(base + 6, 1);
    for (i = 0; i < 100; i++) /* at least 3 us */
        inp(base + 6);
    outp(base + 6, 0);
    for (i = 0; i < 100; i++) {
        if (dsp_read() == 0xaa)
            return 0;
    }
    return -1;
}

static int
sb16_probe(char *info, int info_len)
{
    blaster();
    if (dsp_reset()) {
        snprintf(info, info_len, "no DSP at %Xh", base);
        return -1;
    }
    if (dsp_write(0xe1) || ((dsp_major = dsp_read()) < 0) || ((dsp_minor = dsp_read()) < 0)) {
        snprintf(info, info_len, "the DSP at %Xh doesn't answer", base);
        return -1;
    }
    if (dsp_major < 4) {
        snprintf(info, info_len, "DSP %d.%02d at %Xh is older than a Sound Blaster 16's", dsp_major, dsp_minor, base);
        return -1;
    }
    snprintf(info, info_len, "DSP %d.%02d at %Xh, IRQ %d, DMA %d", dsp_major, dsp_minor, base, irq,
             (dma16 >= 4) ? dma16 : dma8);
    return 0;
}

static int
sb16_open(audio_format *fmt, int *dma_bits)
{
    if (fmt->rate < 5000)
        fmt->rate = 5000;
    if (fmt->rate > 44100)
        fmt->rate = 44100;
    fmt->channels = (fmt->channels >= 2) ? 2 : 1;
    fmt->bits     = 16;
    stereo        = (fmt->channels == 2);
    rate          = fmt->rate;
    /* 16-bit samples go through the high DMA channel; a card set up without one sends them
       through the low channel a byte at a time. */
    *dma_bits = (dma16 >= 4) ? 16 : 8;
    return 0;
}

static void __interrupt __far
sb16_isr(void)
{
    unsigned char status;

    outp(base + 4, 0x82); /* the mixer's interrupt status */
    status = (unsigned char) inp(base + 5);
    if (status & 0x02) { /* 16-bit DMA */
        inp(base + 0xf);
        audio_block_done();
    }
    if (status & 0x01) /* 8-bit DMA or DSP: acknowledge */
        inp(base + 0xe);
    irq_eoi(irq);
}

static int
sb16_start(unsigned long phys, unsigned long bytes, unsigned long block_bytes)
{
    unsigned long words = block_bytes / 2 - 1; /* the DSP counts 16-bit samples */

    if (irq_hook(irq, sb16_isr))
        return -1;
    isa_dma_play((dma16 >= 4) ? dma16 : dma8, phys, bytes);
    dsp_write(0x41); /* output rate, high byte first */
    dsp_write((unsigned char) (rate >> 8));
    dsp_write((unsigned char) rate);
    dsp_write(0xb6); /* 16-bit output, auto-init, FIFO on */
    dsp_write(stereo ? 0x30 : 0x10); /* signed, stereo or mono */
    dsp_write((unsigned char) words);
    dsp_write((unsigned char) (words >> 8));
    return 0;
}

static void
sb16_pause(int paused)
{
    dsp_write(paused ? 0xd5 : 0xd6); /* pause/continue 16-bit DMA output */
}

static void
sb16_stop(void)
{
    dsp_write(0xd5); /* pause 16-bit output */
    dsp_write(0xd9); /* and end auto-init */
    isa_dma_stop((dma16 >= 4) ? dma16 : dma8);
    dsp_reset();
    irq_unhook(irq);
}

static void
sb16_close(void)
{
}

const audio_driver sb16_driver = {
    "sb16",
    "Sound Blaster 16",
    sb16_probe,
    sb16_open,
    sb16_start,
    sb16_pause,
    sb16_stop,
    sb16_close,
};
