#include "ac97.h"
#include "../../io/io.h"
#include "../../system/pmm.h"
#include "../../libc/string.h"
#include "../../libc/stdio.h"

static uint32_t bar_nam; 
static uint32_t bar_nab; 
static ac97_bdl_t* bdl;  
extern uint64_t hhdm_offset;

void ac97_init(uint32_t nam, uint32_t nab) {
    bar_nam = nam & ~0x1;
    bar_nab = nab & ~0x1;

    // 1. Cold Reset
    outl(bar_nab + 0x2C, 0); 
    for(int i = 0; i < 1000; i++) inb(0x80);
    outl(bar_nab + 0x2C, 1 << 1); 

    // 2. Wait for Ready
    int timeout = 100000;
    while (!(inl(bar_nab + 0x30) & 0x100) && timeout--) inb(0x80);

    // 3. Открываем Master и PCM громкость
    outw(bar_nam + 0x02, 0x0000); 
    outw(bar_nam + 0x04, 0x0000); 
    outw(bar_nam + 0x18, 0x0000); 

    // 4. Настройка частоты (44100 Hz для Doom)
    uint16_t ext_id = inw(bar_nam + 0x28); 
    if (ext_id & 1) { 
        outw(bar_nam + 0x2A, inw(bar_nam + 0x2A) | 1); // VRA On
        outw(bar_nam + 0x2C, 0xAC44); // 44100 Hz
    }

    // 5. Инициализация BDL (32 слота)
    uint64_t bdl_phys = (uintptr_t)pmm_alloc();
    bdl = (ac97_bdl_t*)(bdl_phys + hhdm_offset);
    memset(bdl, 0, 4096);
    
    // Заполняем BDL пустышками, чтобы не было паник
    for(int i=0; i<32; i++) {
        bdl[i].pointer = 0;
        bdl[i].length = 0;
        bdl[i].flags = (1 << 15); // Только прерывание
    }

    outl(bar_nab + 0x10, (uint32_t)bdl_phys);
    outb(bar_nab + 0x15, 0); // LVI = 0
}

void ac97_play_at_idx(int idx, void* phys_addr, uint32_t len) {
    // Сбрасываем статус
    outw(bar_nab + 0x16, 0x1C); 

    bdl[idx].pointer = (uint32_t)(uintptr_t)phys_addr;
    bdl[idx].length = (uint16_t)(len / 2); 
    bdl[idx].flags = (1 << 15); // БЕЗ ФЛАГА ОСТАНОВКИ (1<<14)

    // Двигаем LVI вперед. Железка будет играть до этого индекса.
    outb(bar_nab + 0x15, idx); 

    // Стартуем если стояла
    if (!(inb(bar_nab + 0x1B) & 0x01)) {
        outb(bar_nab + 0x1B, 0x01); 
    }
}

void ac97_stop() {
    outb(bar_nab + 0x1B, 0x00); // Stop DMA
    outw(bar_nam + 0x02, 0x8000); // Mute
}