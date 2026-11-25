/* SPDX-License-Identifier: BSD-3-Clause */
/* Hardware interrupt handlers in protected mode. Under DOS/32A an IRQ that comes while the CPU
   is in real mode (in DOS or the BIOS) is passed up to the protected-mode handler too. */
#ifndef OPUS4DOS_IRQ_H
#define OPUS4DOS_IRQ_H

typedef void(__interrupt __far *irq_handler)(void);

/* Installs handler for IRQ 0-15 and unmasks the IRQ at the PIC; 0, or -1. */
int irq_hook(int irq, irq_handler handler);

/* Masks the IRQ again (if it was masked before) and puts the old handler back. */
void irq_unhook(int irq);

/* End of interrupt, from the handler. */
void irq_eoi(int irq);

#endif
