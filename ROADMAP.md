

## 📍 Current Status (Phase 0: The Foundation)
- [x] **Kernel:** x86_64 Higher Half (HHDM) via Limine.
- [x] **Memory:** Bitmap PMM, Dynamic Heap (kmalloc/kfree).
- [x] **Graphics:** VESA LFB with a Compositing Window Manager (CWM), Alpha-blending shadows, and Z-order management.
- [x] **Network:** RTL8139 Driver, ARP, IPv4, UDP, TCP (3-way handshake), HTTP GET, and NTP sync.
- [x] **Filesystem:** FAT32 Read-only support via ATA PIO.
- [x] **UI:** Desktop Environment with Terminal, Paint (Bresenham), Notepad, and System Monitor.

---

## 🛠 Phase 1: Architectural Integrity (HAL & Rings)
*Goal: Moving away from "everything in Ring 0" and creating a hardware abstraction layer.*

- [ ] **Hardware Abstraction Layer (HAL):** Abstract display and input drivers to support multiple backends (VGA/VESA/VirtIO).
- [ ] **GDT/TSS Refactoring:** Proper implementation of Task State Segments for user-mode switching.
- [ ] **User Mode (Ring 3):** Isolated address spaces for applications using Paging (ML4/PDP/PD/PT).
- [ ] **System Calls:** Implement a `syscall` interface (int 0x80 or `syscall` instruction) for applications to request kernel services (I/O, Memory, UI).

## 📁 Phase 2: Advanced Storage & VFS
*Goal: Making the system truly data-driven.*

- [ ] **FAT32 Write Support:** Allow saving files from Notepad and creating directories.
- [ ] **VFS Enhancements:** Proper file descriptors (`open`, `read`, `write`, `close`) and pipe support.
- [ ] **AHCI Driver:** Implementation of SATA/AHCI support to replace the aging ATA PIO mode for faster disk I/O.
- [ ] **Partition Parsing:** Support for GPT (GUID Partition Table) alongside MBR.

## 🎨 Phase 3: The "Insane Visuals" API
*Goal: Building a high-performance UI toolkit.*

- [ ] **SSE/AVX Optimized Blitting:** Using SIMD instructions to accelerate window compositing and transparency.
- [ ] **Advanced UI Toolkit:**
    *   Standardized Widget Library (Buttons, Checkboxes, Text Inputs).
    *   Window Animations (Fade-in, slide-out).
    *   Real-time Blur (Gaussian/Box blur) using SSE shaders.
- [ ] **Dynamic Font Rendering:** Transition from 8x8 bitmap fonts to PSF or basic TrueType (TTF) support.

## ⚙️ Phase 4: Multitasking & IPC
*Goal: A responsive, multi-threaded environment.*

- [ ] **Preemptive Multitasking:** Stable scheduler based on APIC/HPET timers.
- [ ] **Inter-Process Communication (IPC):** Shared memory and message passing for GUI events.
- [ ] **Thread Safety:** Implementing Mutexes and Semaphores in the kernel.

## 🏗 Phase 5: The Self-Hosting Milestone
*Goal: The ultimate test—EquinoxOS compiling EquinoxOS.*

- [ ] **C Standard Library (Libc):** Porting `newlib` or building a custom `mlibc` implementation.
- [ ] **Shell Improvements:** Support for scripts, environment variables, and piping.
- [ ] **Porting Toolchains:**
    *   Porting `TinyCC` (TCC) or a basic `GCC` cross-compiler.
    *   Porting `make` or a similar build system.
- [ ] **Native Development:** Writing and compiling a "Hello World" application entirely within EquinoxOS.

---

## 🌠 Long-Term Vision
* **Audio Stack:** AC97 or Intel HD Audio driver support.
* **USB Stack:** UHCI/EHCI/XHCI support for external peripherals.
* **Porting Games:** Running a native port of DOOM or Quake.
