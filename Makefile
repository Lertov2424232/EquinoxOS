CC = x86_64-elf-gcc   # Сменил
LD = x86_64-elf-ld    # Сменил
ASM = nasm
OBJCOPY = x86_64-elf-objcopy # Сменил
APP_DIR = app
DOOM_DIR = app/doom
DOOM_C = $(wildcard app/doom/*.c)
DOOM_O = $(DOOM_C:.c=.o)
DOOM_SRC = $(wildcard $(DOOM_DIR)/*.c)
APP_SRC = $(APP_DIR)/app.c $(DOOM_SRC)
APP_OBJ = $(APP_SRC:.c=.o)

CFLAGS = -ffreestanding -O2 -Wall -Wextra -fno-exceptions -std=c11 \
         -Isrc -Isrc/drivers -Isrc/shell -Isrc/boot/limine \
         -mcmodel=kernel -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
         -fno-stack-protector -fno-pic -g
LDFLAGS = -nostdlib -T src/linker.ld -z max-page-size=0x1000
ASMFLAGS = -f elf64

APP_CFLAGS = -ffreestanding -O2 -nostdlib -fno-pic -mno-red-zone \
             -DDOOMGENERIC_RES_WIDTH=320 -DDOOMGENERIC_RES_HEIGHT=200 \
             -Isrc -Iapp/doom -Iapp/include

OBJ_DIR = obj
OBJ = $(OBJ_DIR)/kernel.o $(OBJ_DIR)/io.o $(OBJ_DIR)/keyboard.o \
      $(OBJ_DIR)/gdt_flush.o $(OBJ_DIR)/idt.o $(OBJ_DIR)/stdio.o \
      $(OBJ_DIR)/pic.o $(OBJ_DIR)/interrupt.o $(OBJ_DIR)/timer.o $(OBJ_DIR)/ata.o $(OBJ_DIR)/bmp.o \
      $(OBJ_DIR)/memory.o $(OBJ_DIR)/fs.o $(OBJ_DIR)/vesa.o $(OBJ_DIR)/mouse.o $(OBJ_DIR)/string.o $(OBJ_DIR)/panic.o

all: setup kernel.elf

setup:
	@if not exist $(OBJ_DIR) mkdir $(OBJ_DIR)

kernel.elf: $(OBJ)
	$(LD) $(LDFLAGS) $(OBJ) -o kernel.elf

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

$(OBJ_DIR)/%.o: src/drivers/mouse/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/gdt_flush.o: src/system/gdt_flush.asm
	$(ASM) $(ASMFLAGS) $< -o $@

$(OBJ_DIR)/interrupt.o: src/system/interrupt.asm
	$(ASM) $(ASMFLAGS) $< -o $@

app/doom/%.o: app/doom/%.c
	@echo "Compiling $<..."
	$(CC) $(APP_CFLAGS) -c $< -o $@

app/app.o: app/app.c
	$(CC) $(APP_CFLAGS) -c $< -o $@

clean:
	@if exist $(OBJ_DIR) rmdir /s /q $(OBJ_DIR)
	@if exist kernel.elf del kernel.elf

cleanrun: clean all compile_app copykernel iso run

copykernel:
	copy /Y kernel.elf iso_root\kernel.elf

# Добавь этот таргет
compile_app: app/crt0.o app/app.o $(DOOM_O)
	@echo "Linking DOOM..."
	$(LD) -nostdlib -Ttext=0x400000 --oformat binary -e _entry app/crt0.o app/app.o $(DOOM_O) -o iso_root/doom.bin

iso:
	xorriso -as mkisofs -b limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table --efi-boot limine-bios-cd.bin -efi-boot-part --efi-boot-image -o equos.iso iso_root

limine:
	.\limine.exe bios-install equos.iso

run:
	qemu-system-x86_64 -m 128M -cdrom equos.iso -drive file=hdd.img,format=raw,index=1,media=disk -serial stdio
