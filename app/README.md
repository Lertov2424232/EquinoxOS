# EquinoxOS — Userspace Applications

[![ELF64](https://img.shields.io/badge/Format-ELF64-5c6bc0?style=flat-square)](../EID_SDK.md)
[![Base](https://img.shields.io/badge/Load%20Base-0x1000000-00897b?style=flat-square)]()
[![Ring](https://img.shields.io/badge/Ring-3%20User%20Mode-8e24aa?style=flat-square)]()

All apps here are pre-installed userspace **ELF64** binaries that ship with EquinoxOS.
They are built with the [EquinoxOS SDK](../EID_SDK.md) and run in isolated Ring 3 address spaces.

---

## 🌐 htmlview — HTTP Browser

A minimal HTTP browser. Opens any plain HTTP site by fetching it through the kernel's TCP/IP stack.

> ⚠️ HTTPS is not supported — the kernel has no TLS layer yet.

```bash
# From the Terminal shell:
run htmlview.elf
```

- Parses basic HTML tags for layout
- Uses `SYS_DRAW_BUFFER` to render the page into its own window
- Network traffic goes through the RTL8139 driver → IPv4 → TCP stack

---

## 🎵 niplay — WAV Music Player

Plays `.WAV` audio files through the AC97 driver via the `SYS_AUDIO_PLAY` syscall.

```bash
run niplay.elf terry.wav
```

**Bundled tracks:**
- `terry.wav` — TempleOS "God's Music" by Terry A. Davis

---

## 🌙 luagui — Lua Script Runner

Runs stripped Lua 5.x scripts inside a graphical window using EID primitives.

```bash
run luagui.elf app.lua
```

- Lua interpreter is statically linked from `sdk/lua/`
- Scripts can draw to the window buffer via EID bindings
- Very limited standard library (no OS I/O) — graphics focused

---

## 🖼 bmpview — BMP Image Viewer

Opens and displays 24-bit BMP images from the disk (EXT2 or FAT32).

```bash
run bmpview.elf BG.BMP
```

> Still somewhat buggy with non-standard BMP variants (RLE, 16-bit, etc.)

---

## 🐍 snake — Classic Snake

A classic Snake game rendered entirely via `SYS_DRAW_BUFFER`.

```bash
run snake.elf
```

- Keyboard input via `SYS_GET_SCANCODE`
- Speed increases as the snake grows

---

## 💀 doom — DOOM Port

A port of **doomgeneric** for EquinoxOS. Renders at 640×400, with AC97 audio.

```bash
# Launched from Explorer by clicking doom.elf
run doom.elf
```

- Built with `-DDOOMGENERIC_RESX=640 -DDOOMGENERIC_RESY=400 -DFEATURE_SOUND`
- Uses `SYS_DRAW_BUFFER` for video and `SYS_AUDIO_PLAY` for sound
- WAD file must be present on the disk

---

## 🔧 Building the Apps

```bash
# Build all userspace apps + SDK
make apps

# Build DOOM separately
make doom.elf

# Full build (kernel + apps + ISO + HDD)
make all
```

Apps are placed in `iso_root/bin/` and copied into the bootable image automatically.

---

> 📖 For the SDK API, EID toolkit reference, and syscall table — see [EID_SDK.md](../EID_SDK.md)