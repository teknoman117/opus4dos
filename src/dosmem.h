/* SPDX-License-Identifier: BSD-3-Clause */
/* Conventional (below 1 MB) memory from the DPMI host, for DMA buffers. Under DOS/32A's flat
   model a near pointer is a linear address, and below 1 MB, with no paging manager in the way,
   linear is physical, which is what the ISA DMA controller wants. */
#ifndef OPUS4DOS_DOSMEM_H
#define OPUS4DOS_DOSMEM_H

typedef struct {
    unsigned short selector;  /* the DPMI block, for freeing it */
    unsigned long  linear;    /* where it starts */
    unsigned long  size;      /* bytes */
} dos_block;

/* bytes of DOS memory; 0, or -1 if there isn't that much. */
int  dos_alloc(dos_block *b, unsigned long bytes);
void dos_free(dos_block *b);

/* A buffer of `bytes` in DOS memory that doesn't cross a `boundary` (64 KB for 8-bit ISA DMA,
   128 KB for 16-bit), from a block twice as big; its physical address, or 0 on failure. */
unsigned long dos_alloc_dma(dos_block *b, unsigned long bytes, unsigned long boundary);

#endif
