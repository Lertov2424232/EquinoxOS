CC = x86_64-elf-gcc
LD = x86_64-elf-ld

CFLAGS = -ffreestanding -fno-stack-protector -fno-pie -mno-red-zone -I../../sdk/include -I../../sdk/lua
LDFLAGS = -T ../../sdk/lib/link.ld -L../../sdk/lib -lequos_sdk -L../../sdk/lua -llua

SRCS = main.c api_gui.c
OBJS = $(SRCS:.c=.o)

all: sysgui.elf

sysgui.elf: $(OBJS)
	$(CC) $(OBJS) -o $@ $(CFLAGS) $(LDFLAGS)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -f *.o *.elf