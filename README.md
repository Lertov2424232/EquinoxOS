# EquinoxOS
### hobby OS proj

## Current Features:
- Limine Bootloader
- VESA Screen Driver with Hardware Cursor
- Keyboard Driver (ASCII, Shift support)
- Windows
- Command Shell
- External ELF runner

## Stack:
- **Language:** C, x86 Assembly
- **Compiler:** GCC (MinGW-w64), i686, x86_64-elf
- **Assembler:** NASM
- **Emulator:** QEMU

to build:

xorriso -as mkisofs -b limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table -o equos.iso iso_root
.\limine.exe bios-install equos.iso
qemu-system-x86_64 -cdrom equos.iso

Debug:
qemu-system-x86_64.exe -cdrom equos.iso -d int,cpu_reset -no-reboot -no-shutdown 2> qemu_log.txt
OR
x86_64-elf-addr2line -e kernel.elf FFFFFFFF?????????

Made with help of AI. IDGAF what you say about AI. I do it like i want it to be.

Trying to finish it.