# EquinoxOS: x86_64 Technical Specification & OS Map

<div align="center">

[![License](https://img.shields.io/github/license/ewasion137/EquinoxOS?style=for-the-badge&color=orange)](LICENSE)
[![Status](https://img.shields.io/badge/Kernel-Monolithic-red?style=for-the-badge)]()
[![Arch](https://img.shields.io/badge/Arch-x86__64-blue?style=for-the-badge)]()
[![Boot](https://img.shields.io/badge/Boot-Limine-black?style=for-the-badge)]()

**EquinoxOS** is a hobby monolithic kernel operating system featuring a full graphical user interface, preemptive multitasking, and a custom API for user applications (ELF).
**EquinosOS** is made to be minimally daily usable, and still is in active development.

</div>

---

## 🏗 System Architecture

Visualizing data flow and control within the kernel:

```mermaid
graph TD
    subgraph UserSpace
        App[ELF Executables] --> API[EquinoxAPI / SDK]
    end

    subgraph KernelCore
        API --> Syscalls[Syscall Handler]
        Syscalls --> Task[Task Manager / Scheduler]
        Task --> Memory[PMM / Heap / SSE]
    end

    subgraph VFS_Layer
        VFS[Virtual File System] --> FAT32EXT[FAT32 & EXT2 Driver]
        VFS --> DevFS[Device Nodes]
        FAT32+EXT2 --> ATA[ATA / Disk Driver]
    end

    subgraph Graphics_Input
        GUI[GUI Compositor] --> VESA[VESA LFB Driver]
        GUI --> Mouse[PS/2 Mouse]
        GUI --> KB[PS/2 Keyboard]
    end

    subgraph Network
        NetStack[Basic Network Stack] --> RTL8139[RTL8139 Driver]
    end
```

---

## 🛠 Hardware Support & Features

| Category | Component | Status | Description |
| :--- | :--- | :--- | :--- |
| **Boot** | Limine Protocol | ✅ | Boots in 64-bit mode, retrieves Memory Map and HHDM. |
| **CPU** | x86_64 / SSE | ✅ | SSE initialization for floating-point operations (used in GUI). |
| **Memory** | PMM + Heap | ✅ | Physical page allocator and kernel heap (malloc/kfree). |
| **Multitasking** | Preemptive | ✅ | Round-Robin scheduler. Context switching via IRQ0 timer. |
| **Graphics** | VESA LFB | ✅ | Direct framebuffer access with hardware cursor and double buffering. |
| **Storage** | FAT32 / ATA / EXT2 | ✅ | File Read/Write support. 8.3 filename compliance. |
| **Network** | RTL8139 | 🛠 | Basic PCI driver, raw packet RX/TX (WIP). |

---

## 🖥 User Interface & Built-in Apps

The OS comes with a graphical shell and a set of system utilities:

1.  **Terminal:** Supports command history and system logging.
2.  **Explorer:** Graphical file manager. Reads FAT32 content and launches executables.
3.  **Notepad:** Text editor with the ability to save (`NOTES.TXT`) to disk.
4.  **Paint:** Graphics editor. **Killer feature:** Canvas export to a valid `.BMP` file on disk.
5.  **System Monitor:** Real-time RAM usage monitoring.
6.  +**HTML Viewer** and a music player (NiPlay) as external apps!

---

## ⌨️ Developer API (EquinoxAPI)

To develop applications for EquinoxOS, the `api.h` is used. Key capabilities:

```c
typedef struct {
    void (*draw_buffer)(int x, int y, int w, int h, uint32_t *buffer);
    uint8_t (*get_scancode)();
    uint32_t (*get_time_ms)();
    void (*print)(const char *str);
} EquinoxAPI;
```
*Applications are loaded as ELF modules via `Limine` or executed through the `Explorer`.*

---

## 📂 Project Structure

```text
├── app/               # Userspace application sources (Snake, BMPView)
├── iso_root/          # Bootable image root (configs, fonts, binaries)
├── sdk/               # Libraries for Equinox development (CRT0, Syscalls)
├── src/               
│   ├── boot/          # Boot protocols (Limine)
│   ├── drivers/       # Hardware drivers (Video, Net, Disk, Input)
│   ├── fs/            # VFS, FAT32 implementation, and ELF Loader
│   ├── gui/           # Window manager and compositor
│   ├── libc/          # Minimal standard library (string, stdio)
│   ├── shell/         # Command line interpreter
│   └── system/        # Kernel Core (GDT, IDT, PMM, Scheduler, Syscalls)
└── Makefile           # Build system (GCC / NASM)
```

---

## 🚀 Quick Start

### Prerequisites
*   `gcc-x86_64-elf` (Cross-compiler)
*   `nasm`
*   `make`, `mtools`, `xorriso`
*   `qemu-system-x86_64`

### Build and Run
```bash
# Compile kernel and create ISO
make build
make iso

# Launch in QEMU with diagnostic flags
make run
```

### Debugging
If the kernel panics, use `addr2line` to locate the fault:
```bash
x86_64-elf-addr2line -e kernel.elf <RIP_ADDRESS>
```

---

## 🗺 Roadmap
~~1. Paint saving BMP~~
~~2. BMP viewer external application~~
~~3. Ring 3~~
~~4. Z view (Program behind other programs)~~
~~5. Context Switch + Keyboard ARE NOT HARDLOCKED~~
~~6. Better SDK~~
~~7. *PHYSICAL OS*~~
~~8. Doom~~
9. Run on real hardware
~~10. ***SOUND***~~
11. ***USB***
~~12. Port any new FS (EXT2)~~
13. Make your OWN FS
14. Port any SECOND FS (EXT3/EXT4/UFS/ZFS)
15. Make OS SERIOUS (Fix ANY of the stubs | Polishing)
16. Port any language (AS USERSPACE) - C#, C++, Lua, Python [Better to now implement HTML, JS, CSS (Because htmlview.elf)]
16.1. If needed, write SOMETHING IN THE KERNEL on the language implemented (Like UI on lua)
17. Text browser (Maybe will be deleted soon bcs of htmlview.elf)
18. HTTPS 
19 - VERY…
~~19.1 - Very better VESA | Maybe OWN graphical?~~
~~19.2 - Very better Memory~~
~~19.3 - Very better EID (Equinox Interface Designer)~~
19.4. OS is INSTALLABLE/Archiveable so it’s able to separate
19.5 - General separation. Kernel is separated from anything including GUI (except Drivers). 




***

### Contributors
* **@ewasion137** — Lead Developer
* **@oxtiskz** — Special Thanks (Deleted account)
* **@gobgolaxi** - Special thanks (Contributor)
* **@Offihito** - Special thanks (Contributor)
* **@Lertov2424232** - Special thanks (Contributor)

<img width="1278" height="805" alt="image" src="https://github.com/user-attachments/assets/8ed14d39-b20f-4268-b6cb-b40d0beda5df" />
<img width="1283" height="802" alt="image" src="https://github.com/user-attachments/assets/bf378308-364d-41f6-80ac-3446b5ab0c3a" />
<img width="1277" height="805" alt="image" src="https://github.com/user-attachments/assets/5b0c8d2d-14c2-42a8-b338-527108dcc8fc" />
<img width="644" height="448" alt="image" src="https://github.com/user-attachments/assets/99d3e6b5-459b-4ccc-8976-516775a7bb1c" />
