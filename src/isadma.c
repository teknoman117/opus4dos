/* SPDX-License-Identifier: BSD-3-Clause */
#include <conio.h>

#include "isadma.h"

/* Per channel: address, count and page registers. */
static const unsigned char addr_port[8]  = { 0x00, 0x02, 0x04, 0x06, 0xc0, 0xc4, 0xc8, 0xcc };
static const unsigned char count_port[8] = { 0x01, 0x03, 0x05, 0x07, 0xc2, 0xc6, 0xca, 0xce };
static const unsigned char page_port[8]  = { 0x87, 0x83, 0x81, 0x82, 0x8f, 0x8b, 0x89, 0x8a };

void
isa_dma_play(int ch, unsigned long phys, unsigned long bytes)
{
    int           wide  = (ch >= 4);
    unsigned      mask  = wide ? 0xd4 : 0x0a; /* single-channel mask register */
    unsigned      mode  = wide ? 0xd6 : 0x0b;
    unsigned      flip  = wide ? 0xd8 : 0x0c; /* clear the byte pointer flip-flop */
    unsigned long addr  = wide ? (phys >> 1) : phys;  /* words for 4-7 */
    unsigned long count = (wide ? (bytes >> 1) : bytes) - 1;

    outp(mask, 0x04 | (ch & 3));
    outp(flip, 0);
    /* Single transfers, auto-init, read (memory to device). */
    outp(mode, 0x40 | 0x10 | 0x08 | (ch & 3));
    outp(addr_port[ch], addr & 0xff);
    outp(addr_port[ch], (addr >> 8) & 0xff);
    /* 4-7's page register holds address bits 23:17; bit 16 is in the word address. */
    outp(page_port[ch], (phys >> 16) & (wide ? 0xfe : 0xff));
    outp(flip, 0);
    outp(count_port[ch], count & 0xff);
    outp(count_port[ch], (count >> 8) & 0xff);
    outp(mask, ch & 3);
}

void
isa_dma_stop(int ch)
{
    outp((ch >= 4) ? 0xd4 : 0x0a, 0x04 | (ch & 3));
}
