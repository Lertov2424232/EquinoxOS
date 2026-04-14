***

# eid - Equinox Interface Designer & SDK Documentation

This document describes the architecture and usage of the EquinoxOS Software Development Kit (SDK) and its built-in UI toolkit, EID.

## Overview
EquinoxOS uses a **User-Kernel Separation** model. Applications run in their own memory space and communicate with the kernel via an `int 0x80` interrupt bridge. The SDK provides the necessary headers and wrappers to make development efficient.

### System Call Architecture
All interactions with hardware (disk, screen, keyboard) are performed via system calls.
* **Entry Point:** Applications start at `_start` (defined in `crt0.asm`), which calls `main()`.
* **Registers:**
    * `RAX`: Syscall Number
    * `RDI`, `RSI`, `RDX`, `RCX`, `R8`: Arguments 1-5
    * `RAX`: Return value

---

## SDK System Calls Reference

The SDK provides a low-level `_syscall` wrapper in `<equos.h>`.

| Number | Name | Description | Arguments |
| :--- | :--- | :--- | :--- |
| 1 | `SYS_PRINT` | Prints a string to the kernel terminal. | `rdi`: char* string |
| 2 | `SYS_READ_FILE` | Reads a file from FAT32. | `rdi`: name, `rsi`: size_ptr |
| 3 | `SYS_WRITE_FILE` | Saves/Updates a file on FAT32. | `rdi`: name, `rsi`: buf, `rdx`: size |
| 5 | `SYS_DRAW_BUFFER` | Blits a local buffer to the window. | `rdi`: x, `rsi`: y, `rdx`: w, `rcx`: h, `r8`: buf |
| 6 | `SYS_GET_TIME` | Returns system ticks (1 tick = 10ms). | - |
| 9 | `SYS_GET_SCANCODE` | Pops a scancode from the input buffer. | - |
| 10 | `SYS_EXIT` | Terminates the current process. | - |
| 11 | `SYS_YIELD` | Relinquishes CPU to other tasks. | - |
| 12 | `SYS_GET_FONT` | Returns the kernel-space PSF font pointer. | - |

### Syscall Usage Examples

**Reading a file:**
```c
uint32_t size = 0;
void* buffer = (void*)_syscall(SYS_READ_FILE, (uintptr_t)"LOGO.BMP", (uintptr_t)&size, 0, 0, 0);
if (buffer) {
    // Process data...
}
```

**Exiting an application:**
```c
_syscall(SYS_EXIT, 0, 0, 0, 0, 0);
```

---

## eid - Equinox Interface Designer

EID is a "Buffer-First" UI toolkit. Instead of calling kernel functions for every pixel, the application maintains a local `uint32_t` pixel buffer. EID functions draw into this buffer in user-space, and then the buffer is sent to the kernel compositor once per frame.

### Core Concepts
* **Design Language:** Modern Dark Flat (Tokyonight-inspired).
* **Font Handling:** EID fetches the system PSF font from the kernel during `eid_init()` to ensure UI consistency.
* **Coordinate System:** Relative to the application window.

### Key Functions

| Function | Description |
| :--- | :--- |
| `eid_init()` | Connects to the kernel and maps the system font. |
| `eid_draw_window_frame(...)` | Draws a complete window decoration (title, close btn). |
| `eid_draw_button(...)` | Draws a button with 4 states (Normal, Pressed, Hover, Disabled). |
| `eid_draw_panel(...)` | Draws a surface/card (can be flat or sunken). |
| `eid_draw_text(...)` | Renders PSF text into the buffer. |

### Component Example: Main Menu
```c
#include <equos.h>
#include <eid.h>

static uint32_t win_buf[400 * 300];

int main() {
    eid_init();
    
    while(1) {
        // 1. Render Window UI
        eid_draw_window_frame(win_buf, 400, 300, "App Title");
        
        // 2. Draw a Button
        eid_draw_button(win_buf, 400, 50, 50, 100, 30, "Click Me", EID_STATE_NORMAL);
        
        // 3. Draw a Checkbox
        eid_draw_checkbox(win_buf, 400, 50, 100, "Enable Audio", true);
        
        // 4. Send buffer to Kernel Compositor
        _syscall(SYS_DRAW_BUFFER, 0, 0, 400, 300, (uintptr_t)win_buf);
        
        // 5. Sleep to prevent CPU hogging
        _syscall(SYS_YIELD, 0, 0, 0, 0, 0);
    }
}
```

---

## Technical Specifications
* **Screen Buffer Format:** 32-bit ARGB (8-8-8-8).
* **Font Engine:** PSF1 (PC Screen Font), 8x16 pixels.
* **Linker Requirements:** Applications must be linked with `-Ttext=0x1000000` (User Base Address).
* **Binary Format:** Executable and Linkable Format (ELF64).

***