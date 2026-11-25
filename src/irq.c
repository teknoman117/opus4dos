/* SPDX-License-Identifier: BSD-3-Clause */
#include <conio.h>
#include <dos.h>

#include "irq.h"

static irq_handler old_handler[16];
static int         was_masked[16];

static int
vector(int irq)
{
    return (irq < 8) ? (0x08 + irq) : (0x70 + irq - 8);
}

static unsigned
pic_mask_port(int irq)
{
    return (irq < 8) ? 0x21 : 0xa1;
}

int
irq_hook(int irq, irq_handler handler)
{
    unsigned port = pic_mask_port(irq);
    unsigned bit  = 1u << (irq & 7);

    if ((irq < 0) || (irq > 15) || old_handler[irq])
        return -1;
    _disable();
    old_handler[irq] = _dos_getvect(vector(irq));
    _dos_setvect(vector(irq), handler);
    was_masked[irq] = (inp(port) & bit) != 0;
    outp(port, inp(port) & ~bit);
    if (irq >= 8)
        outp(0x21, inp(0x21) & ~0x04); /* the cascade, IRQ 2 */
    _enable();
    return 0;
}

void
irq_unhook(int irq)
{
    unsigned port = pic_mask_port(irq);
    unsigned bit  = 1u << (irq & 7);

    if ((irq < 0) || (irq > 15) || !old_handler[irq])
        return;
    _disable();
    if (was_masked[irq])
        outp(port, inp(port) | bit);
    _dos_setvect(vector(irq), old_handler[irq]);
    old_handler[irq] = 0;
    _enable();
}

void
irq_eoi(int irq)
{
    if (irq >= 8)
        outp(0xa0, 0x20);
    outp(0x20, 0x20);
}
