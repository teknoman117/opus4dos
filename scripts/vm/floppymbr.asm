; SPDX-License-Identifier: BSD-3-Clause
; An MBR that boots the floppy: for the test disk of scripts/86box_play.py, which holds the music
; but no DOS, on a machine whose BIOS boots C: before A:. It moves itself out of the way, reads
; drive A:'s boot sector to 0:7C00 and jumps to it, as if the BIOS had booted A:. The partition
; table at 1BEh is sfdisk's; this only fills the code area before it.
;
;   nasm -f bin -o floppymbr.bin floppymbr.asm
        bits    16
        org     0x0600

        cli
        xor     ax, ax
        mov     ss, ax
        mov     sp, 0x7c00
        mov     ds, ax
        mov     es, ax
        sti
        mov     si, 0x7c00              ; this sector, where the BIOS put it
        mov     di, 0x0600
        mov     cx, 256
        cld
        rep movsw
        jmp     0:moved
moved:
        mov     bp, 4                   ; tries: a floppy drive may need a reset first
retry:
        xor     ax, ax                  ; reset drive A:
        xor     dx, dx
        int     0x13
        mov     ax, 0x0201              ; read 1 sector
        mov     bx, 0x7c00
        mov     cx, 0x0001              ; cylinder 0, sector 1
        xor     dx, dx                  ; head 0, drive 0 (A:)
        int     0x13
        jnc     boot
        dec     bp
        jnz     retry
        int     0x18                    ; no floppy: the BIOS's "no boot device"
boot:
        xor     dx, dx                  ; DL = the boot drive, A:
        jmp     0:0x7c00

        times   0x1be - ($ - $$) db 0
