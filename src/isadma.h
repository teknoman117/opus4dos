/* SPDX-License-Identifier: BSD-3-Clause */
/* The PC's two 8237 DMA controllers: channels 0-3 move bytes, 4-7 move 16-bit words. */
#ifndef OPUS4DOS_ISADMA_H
#define OPUS4DOS_ISADMA_H

/* Sets channel ch up to read `bytes` from memory at physical address phys to the device, over
   and over (auto-init), and unmasks it. The buffer mustn't cross a 64 KB (channels 0-3) or
   128 KB (4-7) boundary, and for 4-7 phys and bytes must be even. */
void isa_dma_play(int ch, unsigned long phys, unsigned long bytes);

/* Masks channel ch, stopping its transfers. */
void isa_dma_stop(int ch);

#endif
