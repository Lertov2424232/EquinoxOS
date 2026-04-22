CC = x86_64-elf-gcc
LD = x86_64-elf-ld
ASM = nasm

# --- НАСТРОЙКИ ПУТЕЙ ---
OBJ_DIR = obj
SDK_LIB_DIR = sdk/lib
ISO_ROOT = iso_root
APP_BINARY = $(ISO_ROOT)/app.elf

# --- ФЛАГИ ЯДРА ---
CFLAGS = -ffreestanding -O0 -Wall -Wextra -fno-exceptions -std=c11 \
         -Isrc -Isrc/drivers -Isrc/shell -Isrc/boot/limine \
         -mcmodel=kernel -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
         -fno-stack-protector -fno-pic -g

LDFLAGS = -nostdlib -T src/linker.ld -z max-page-size=0x1000
ASMFLAGS = -f elf64

# --- ФЛАГИ SDK (ПРИЛОЖЕНИЯ) ---
SDK_INC = -I./sdk/include
USER_CFLAGS = -ffreestanding -mcmodel=small -mno-red-zone -fno-stack-protector -fno-pic -D__clang__ $(SDK_INC)

# --- ФЛАГИ MLIBC ---
MLIBC_CFLAGS = -ffreestanding -nostdinc -D__clang__ -I./sdk/include
MLIBC_LDFLAGS = -nostdlib -L./sdk/lib -lc /usr/lib/gcc/x86_64-elf/15.2.0/libgcc.a

# Объекты SDK и Ядра
# Note: stdio.o and string.o removed - mlibc provides these
SDK_OBJS = $(SDK_LIB_DIR)/crt0.o $(SDK_LIB_DIR)/eid.o
MLIBC_CRT0 = $(SDK_LIB_DIR)/crt0.o
OBJ = $(OBJ_DIR)/kernel.o $(OBJ_DIR)/io.o $(OBJ_DIR)/keyboard.o $(OBJ_DIR)/rtl8139.o $(OBJ_DIR)/vfs.o $(OBJ_DIR)/gui.o $(OBJ_DIR)/syscall.o \
      $(OBJ_DIR)/gdt_flush.o $(OBJ_DIR)/idt.o $(OBJ_DIR)/stdio.o $(OBJ_DIR)/pci.o $(OBJ_DIR)/pmm.o $(OBJ_DIR)/shell.o $(OBJ_DIR)/eqstart.o \
      $(OBJ_DIR)/pic.o $(OBJ_DIR)/interrupt.o $(OBJ_DIR)/timer.o $(OBJ_DIR)/ata.o $(OBJ_DIR)/bmp.o $(OBJ_DIR)/task.o $(OBJ_DIR)/fat32.o \
      $(OBJ_DIR)/memory.o $(OBJ_DIR)/fs.o $(OBJ_DIR)/vesa.o $(OBJ_DIR)/mouse.o $(OBJ_DIR)/string.o $(OBJ_DIR)/panic.o $(OBJ_DIR)/vmm.o $(OBJ_DIR)/gdt.o \
      $(OBJ_DIR)/serial.o

all: setup kernel.elf compile_app compile_mlibc_test copykernel

setup:
	mkdir -p $(OBJ_DIR)

kernel.elf: $(OBJ)
	$(LD) $(LDFLAGS) $(OBJ) -o kernel.elf

copykernel:
	cp -f kernel.elf $(ISO_ROOT)/kernel.elf

# --- СБОРКА ЯДРА ---
$(OBJ_DIR)/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJ_DIR)/%.o: src/system/%.c
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJ_DIR)/%.o: src/drivers/screen/%.c
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJ_DIR)/%.o: src/drivers/keyboard/%.c
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJ_DIR)/%.o: src/shell/%.c
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJ_DIR)/%.o: src/drivers/disk/%.c
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJ_DIR)/%.o: src/fs/%.c
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJ_DIR)/%.o: src/drivers/vga/%.c
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJ_DIR)/%.o: src/libc/%.c
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJ_DIR)/%.o: src/io/%.c
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJ_DIR)/%.o: src/gui/%.c
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJ_DIR)/%.o: src/drivers/mouse/%.c
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJ_DIR)/%.o: src/drivers/pci/%.c
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJ_DIR)/%.o: src/drivers/net/%.c
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJ_DIR)/%.o: src/drivers/serial/%.c
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJ_DIR)/gdt_flush.o: src/system/gdt_flush.asm
	$(ASM) $(ASMFLAGS) $< -o $@
$(OBJ_DIR)/interrupt.o: src/system/interrupt.asm
	$(ASM) $(ASMFLAGS) $< -o $@

# --- СБОРКА SDK ---
$(SDK_LIB_DIR)/%.o: $(SDK_LIB_DIR)/%.c
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(SDK_LIB_DIR)/%.o: $(SDK_LIB_DIR)/%.asm
	$(ASM) -f elf64 $< -o $@

# --- СБОРКА ПРИЛОЖЕНИЯ ---
compile_app: $(SDK_OBJS)
	$(CC) $(USER_CFLAGS) -nostdinc -c app/snake.c -o app/snake.o
	$(LD) -nostdlib -Ttext=0x1000000 -e _start $(SDK_OBJS) app/snake.o -L./sdk/lib -lc /usr/lib/gcc/x86_64-elf/15.2.0/libgcc.a -o $(ISO_ROOT)/snake.elf
	
	$(CC) $(USER_CFLAGS) -nostdinc -c app/bmpview.c -o app/bmpview.o
	$(LD) -nostdlib -Ttext=0x1000000 -e _start $(SDK_OBJS) app/bmpview.o -L./sdk/lib -lc /usr/lib/gcc/x86_64-elf/15.2.0/libgcc.a -o $(ISO_ROOT)/bmpview.elf

# --- СБОРКА MLIBC TEST ---
compile_mlibc_test: $(MLIBC_CRT0)
	$(CC) -o $(ISO_ROOT)/test.elf $(MLIBC_CRT0) test_mlibc.c $(MLIBC_CFLAGS) $(MLIBC_LDFLAGS)
# --- ОЧИСТКА ---
clean:
	rm -rf $(OBJ_DIR)
	rm -f app/snake.o app/bmpview.o
	rm -f sdk/lib/*.o
	rm -f kernel.elf
	rm -f iso_root/kernel.elf
	rm -f equos.iso
	rm -f packets.pcap

cleanrun: clean all iso run

iso:
	xorriso -as mkisofs -b limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table --efi-boot limine-bios-cd.bin -efi-boot-part --efi-boot-image -o equos.iso $(ISO_ROOT)

run:
	qemu-system-x86_64 -m 128M -boot d -drive file=hdd.img,format=raw,index=0,media=disk -cdrom equos.iso -serial stdio -netdev user,id=n0,hostfwd=tcp::2222-:22 -device rtl8139,netdev=n0 -d int,guest_errors,mmu -D qemu.log