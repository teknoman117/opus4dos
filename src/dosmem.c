/* SPDX-License-Identifier: BSD-3-Clause */
#include <i86.h>
#include <string.h>

#include "dosmem.h"

int
dos_alloc(dos_block *b, unsigned long bytes)
{
    union REGS r;

    memset(b, 0, sizeof(*b));
    memset(&r, 0, sizeof(r));
    r.w.ax = 0x0100; /* DPMI: allocate DOS memory block, BX paragraphs */
    r.w.bx = (unsigned short) ((bytes + 15) >> 4);
    int386(0x31, &r, &r);
    if (r.w.cflag)
        return -1;
    b->selector = r.w.dx;
    b->linear   = (unsigned long) r.w.ax << 4;
    b->size     = bytes;
    return 0;
}

void
dos_free(dos_block *b)
{
    union REGS r;

    if (!b->selector)
        return;
    memset(&r, 0, sizeof(r));
    r.w.ax = 0x0101; /* DPMI: free DOS memory block DX */
    r.w.dx = b->selector;
    int386(0x31, &r, &r);
    memset(b, 0, sizeof(*b));
}

unsigned long
dos_alloc_dma(dos_block *b, unsigned long bytes, unsigned long boundary)
{
    unsigned long p;

    if ((bytes > boundary) || dos_alloc(b, 2 * bytes))
        return 0;
    p = b->linear;
    if ((p / boundary) != ((p + bytes - 1) / boundary))
        p = (p / boundary + 1) * boundary; /* the next boundary, which is inside the block */
    return p;
}
