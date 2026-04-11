CC = x86_64-elf-gcc
LD = x86_64-elf-ld
ASM = nasm
OBJCOPY = x86_64-elf-objcopy

# --- НАСТРОЙКИ ПРИЛОЖЕНИЯ (ЗМЕЙКА) ---
APP_SRC = app/snake.c
APP_OBJ = app/snake.o

# Флаги ядра
CFLAGS = -ffreestanding -O0 -Wall -Wextra -fno-exceptions -std=c11 \
         -Isrc -Isrc/drivers -Isrc/shell -Isrc/boot/limine \
         -mcmodel=kernel -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
         -fno-stack-protector -fno-pic -g

LDFLAGS = -nostdlib -T src/linker.ld -z max-page-size=0x1000
ASMFLAGS = -f elf64

# Флаги приложения (ELF)
# Мы линкуем по адресу 0x1000000 (16 МБ), чтобы не мешать ядру
APP_CFLAGS = -ffreestanding -O2 -nostdlib -fno-pic -mno-red-zone -Isrc

OBJ_DIR = obj
OBJ = $(OBJ_DIR)/kernel.o $(OBJ_DIR)/io.o $(OBJ_DIR)/keyboard.o $(OBJ_DIR)/rtl8139.o $(OBJ_DIR)/vfs.o $(OBJ_DIR)/gui.o \
      $(OBJ_DIR)/gdt_flush.o $(OBJ_DIR)/idt.o $(OBJ_DIR)/stdio.o $(OBJ_DIR)/pci.o $(OBJ_DIR)/pmm.o $(OBJ_DIR)/shell.o $(OBJ_DIR)/eqstart.o \
      $(OBJ_DIR)/pic.o $(OBJ_DIR)/interrupt.o $(OBJ_DIR)/timer.o $(OBJ_DIR)/ata.o $(OBJ_DIR)/bmp.o $(OBJ_DIR)/task.o $(OBJ_DIR)/fat32.o \
      $(OBJ_DIR)/memory.o $(OBJ_DIR)/fs.o $(OBJ_DIR)/vesa.o $(OBJ_DIR)/mouse.o $(OBJ_DIR)/string.o $(OBJ_DIR)/panic.o

all: setup kernel.elf

setup:
	@if not exist $(OBJ_DIR) mkdir $(OBJ_DIR)

kernel.elf: $(OBJ)
	$(LD) $(LDFLAGS) $(OBJ) -o kernel.elf

# Компиляция файлов ядра
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
$(OBJ_DIR)/gdt_flush.o: src/system/gdt_flush.asm
	$(ASM) $(ASMFLAGS) $< -o $@
$(OBJ_DIR)/interrupt.o: src/system/interrupt.asm
	$(ASM) $(ASMFLAGS) $< -o $@
$(OBJ_DIR)/%.o: src/drivers/net/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# --- КОМПИЛЯЦИЯ ПРИЛОЖЕНИЯ (SNAKE) ---
app/snake.o: app/snake.c
	$(CC) $(APP_CFLAGS) -c $< -o $@

# Собираем app.elf (точка входа _start, адрес 0x1000000)
compile_app: app/snake.o
	@echo "Linking Snake app.elf..."
	$(LD) -nostdlib -z max-page-size=0x1000 -Ttext=0x1000000 -e _start app/snake.o -o iso_root/app.elf

clean:
	@if exist $(OBJ_DIR) rmdir /s /q $(OBJ_DIR)
	@if exist kernel.elf del kernel.elf
	@if exist app\snake.o del app\snake.o
	@if exist packets.pcap del packets.pcap

cleanrun: clean all compile_app copykernel iso run

copykernel:
	copy /Y kernel.elf iso_root\kernel.elf

iso:
	xorriso -as mkisofs -b limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table --efi-boot limine-bios-cd.bin -efi-boot-part --efi-boot-image -o equos.iso iso_root

limine:
	.\limine.exe bios-install equos.iso

run:
	qemu-system-x86_64 -m 128M -boot d -drive file=hdd.img,format=raw,index=0,media=disk -cdrom equos.iso -serial stdio -netdev user,id=n0,hostfwd=tcp::2222-:22 -device rtl8139,netdev=n0 -object filter-dump,id=f1,netdev=n0,file=packets.pcap