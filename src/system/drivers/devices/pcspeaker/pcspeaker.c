#include "pcspeaker.h"
#include "../../../core/io.h"

// PIT and Speaker ports
#define PIT_CHAN2_DATA   0x42
#define PIT_CMD          0x43
#define SPEAKER_PORT     0x61

// PIT command: channel 2, mode 3 (square wave), lo/hi byte access
#define PIT_CHAN2_MODE3  0xB6

// Speaker control bits
#define SPEAKER_GATE     0x01  // Bit 0: Timer 2 gate enable
#define SPEAKER_DATA     0x02  // Bit 1: Speaker data (AND with timer output)
#define SPEAKER_ON       0x03  // Bits 0 and 1: Enable PIT to drive speaker

// Simple delay - needs large multiplier for QEMU
static void delay(int ms) {
    while (ms-- > 0) {
        for (volatile int i = 0; i < 50000; i++) {
            __asm__ __volatile__("nop");
        }
    }
}

void pcspeaker_init(void) {
    outb(SPEAKER_PORT, inb(SPEAKER_PORT) & ~SPEAKER_ON);
}

void pcspeaker_play(uint32_t freq) {
    if (freq == 0) {
        pcspeaker_stop();
        return;
    }
    
    uint32_t div = 1193180 / freq;
    if (div < 1) div = 1;
    if (div > 65535) div = 65535;
    
    outb(PIT_CMD, 0xB6);
    outb(PIT_CHAN2_DATA, div & 0xFF);
    outb(PIT_CHAN2_DATA, (div >> 8) & 0xFF);
    outb(SPEAKER_PORT, inb(SPEAKER_PORT) | SPEAKER_ON);
}

void pcspeaker_stop(void) {
    outb(SPEAKER_PORT, inb(SPEAKER_PORT) & ~SPEAKER_ON);
}

void pcspeaker_beep(uint32_t freq, uint32_t ms) {
    pcspeaker_play(freq);
    delay(ms);
    pcspeaker_stop();
}

void pcspeaker_test_melody(void) {
    uint32_t notes[] = {262, 294, 330, 262, 294, 330, 392, 0};
    for (int i = 0; notes[i]; i++) {
        pcspeaker_beep(notes[i], 200);
        delay(50);
    }
}
