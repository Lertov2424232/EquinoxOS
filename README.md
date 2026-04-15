<div align="center">
  
# EquinoxOS: x86-based Hobby Operating System

[![License](https://img.shields.io/github/license/ewasion137/EquinoxOS?style=for-the-badge&color=orange)](LICENSE)
[![Status](https://img.shields.io/badge/Status-Active%20Development-green?style=for-the-badge)]()
[![Arch](https://img.shields.io/badge/Architecture-x86__64%20%2F%20i686-blue?style=for-the-badge)]()
[![Toolchain](https://img.shields.io/badge/Toolchain-GCC%20%2F%20NASM-lightgrey?style=for-the-badge)]()

*EquinoxOS is a monolithic-oriented hobby operating system targeting the x86 architecture. This repository contains the kernel source, drivers, and basic userspace environment.*

English • [Русский](README_RU.md)

</div>

## Technical Specifications

### Core Architecture
* **Target:** x86_64 / i686
* **Bootloader:** Limine
* **Executable Format:** ELF (External runner implemented as command 'run'. Currently runs broken snake.)
* **Toolchain:** GCC (MinGW-w64 / x86_64-elf-gcc), NASM

### Implemented Subsystems
* **Video:** VESA Framebuffer driver with hardware cursor support.
* **Input:** PS/2 Keyboard driver (supporting ASCII mapping and Shift modifiers).
* **Environment:** Basic Command Shell with windowing support.
* **Networking:** Initial internet stack testing (WIP).

## Build and Deployment

### Development Environment
The project requires a cross-compiler toolchain and the NASM assembler. Emulation is performed via QEMU.

### Build Instructions
Clean and full rebuild + run:
```bash
make cleanrun
```
Standard build:
```bash
make build
make iso
```

### Execution
Fast launch in QEMU:
```bash
make run
```

## Debugging and Diagnostics
For kernel-level debugging, use the following QEMU diagnostic flags:
```bash
qemu-system-x86_64.exe -cdrom equos.iso -d int,cpu_reset -no-reboot -no-shutdown 2> qemu_log.txt
```
To resolve symbols from the instruction pointer:
```bash
x86_64-elf-addr2line -e kernel.elf <address>
```


## Project Status
The OS is under active development. Development involves the use of LLM-assisted coding as part of the author's workflow. 

<img width="1278" height="801" alt="image" src="https://github.com/user-attachments/assets/5c9ab047-cd60-42a9-904e-8b5c63db58eb" />
<img width="464" height="581" alt="image" src="https://github.com/user-attachments/assets/52dd9b4d-a635-423a-b7bc-64052a4fc246" />
<img width="455" height="565" alt="image" src="https://github.com/user-attachments/assets/9e21c077-ce9d-4bbc-8865-e1cd939073c6" />


***

Addictional thanks to @oxtiskz