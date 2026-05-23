
# Contributing to EquinoxOS

EquinoxOS is a monolithic x86_64 operating system with a strict separation between kernel space (Ring 0) and user space (Ring 3).  
Because of its architecture (scheduler, memory manager, filesystem, GUI compositor, and network stack), contributions must follow a consistent and safe development workflow.

To clarify, you **can use AI to generate code**, but **you must review and test it** before submitting it as a PR. **BUT** the generated text **MUST** be written by a human.

---

## 🚧 Before You Start

EquinoxOS is not a typical application project.

- You are working with **bare-metal code**
- Bugs can break boot, memory, or disk image
- Kernel code runs with full hardware access

If you are unsure — open an Issue first.

---

## 🔁 Pull Request Workflow

### 1. Fork & Branch

All changes must go through a feature branch:

```bash
git checkout -b feature/<short-name>
````

Examples:

* `feature/tcp-retransmit`
* `fix/vmm-page-fault`
* `feature/gui-window-focus`

---

### 2. Build Before Anything

Your code must compile and boot in QEMU:

```bash
make clean
make all
make run
```

If it does not boot → PR will be rejected.

---

### 3. Keep PRs Small and Atomic

Each PR must contain **one logical change only**:

✔ Good:

* Fix page fault in VMM
* Add TCP ACK handling
* Improve window dragging logic

❌ Bad:

* “kernel improvements”
* multiple subsystems in one PR
* refactoring + features + bugfix mixed

---

### 4. No Breaking Changes Without Discussion

EquinoxOS has stable internal contracts:

* Syscall ABI (`int 0x80`)
* VFS interface
* Scheduler behavior
* Memory manager API

If you change them:

* Open Issue first
* Explain migration path

---

## 🧠 Kernel Safety Rules

Kernel code (Ring 0) must follow strict rules:

### Memory

* Avoid unnecessary dynamic allocation in early boot
* Never leak kernel heap allocations in drivers
* Always validate user pointers in syscalls

### Interrupts

* Do not block inside IRQ handlers
* Keep ISR execution minimal

### Scheduler

* Do not introduce blocking calls in scheduler context
* Context switch must remain deterministic

---

## 👤 User Mode Rules

Userland (Ring 3):

* Must go through syscalls only
* No direct hardware access
* No kernel symbol dependency
* Must be ELF64 compatible

---

## 📦 Coding Style

### C / Kernel Code

* C11 preferred
* No STL (obviously)
* Avoid recursion in kernel paths
* Prefer explicit memory control

Naming:

```c
pmm_alloc_page()
vmm_map_page()
syscall_handle()
task_switch()
```

### Formatting

* K&R style braces preferred
* 4 spaces indentation
* No tabs
* Keep functions under ~100 lines when possible

---

## 🧪 Testing Requirements

Every PR must be tested in QEMU:

### Required checks:

* [ ] Kernel boots successfully
* [ ] No triple faults
* [ ] Serial output shows initialization complete
* [ ] GUI starts (if GUI-related changes)
* [ ] Filesystem still mounts (FAT32 / EXT2 if affected)

---

## 💥 Debugging Guidelines

If your code breaks boot:

### 1. Use serial output

```
-serial stdio
```

### 2. Use addr2line

```bash
x86_64-elf-addr2line -e kernel.elf <RIP>
```

### 3. Check last successful boot stage:

Kernel prints:

```
PMM initialized
VMM initialized
Scheduler initialized
VFS initialized
GUI initialized
```

Find where it stops.

---

## 🧩 Subsystem Ownership

To avoid conflicts:

| Subsystem   | Responsibility              |
| :---------- | :-------------------------- |
| Kernel Core | memory, scheduler, syscalls |
| Drivers     | hardware interfaces         |
| FS          | FAT32 / EXT2 / VFS          |
| GUI         | compositor, windows, input  |
| Net         | TCP/IP stack                |
| Userland    | ELF apps + SDK              |

Do not mix subsystem logic inside unrelated modules.

---

## 🔄 Commit Style

Use meaningful commits:

✔ Good:

```
vmm: fix page alignment in map routine
tcp: fix retransmission timeout handling
gui: improve z-order window stacking
```

❌ Bad:

```
fix stuff
update
changes
lol it works now
```

---

## 📌 Pull Request Template

When opening a PR, include:

```md
## What this changes
Brief description of the change

## Subsystem
(kernel / gui / fs / net / drivers / userland)

## Why this is needed
Technical explanation

## Testing done
- boot tested in QEMU
- serial output checked
- no crashes observed

## Breaking changes
Yes / No
```

---

## 🚫 What will be rejected

* Random refactors across multiple subsystems
* Untested kernel changes
* Code without explanation
* Breaking ABI without migration plan
* Non-deterministic behavior in kernel core
* "works on my machine" PRs

---

## 👥 Contributors

See README.md for contributors list.

---

## ⚙️ Final Note

If you're unsure whether your change is valid:

Open an Issue first.

Kernel-level mistakes are expensive.
