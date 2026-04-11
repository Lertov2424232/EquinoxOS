#include "ata.h"
#include "../../io/io.h"

#define ATA_PRIMARY_DATA         0x1F0
#define ATA_PRIMARY_ERR          0x1F1
#define ATA_PRIMARY_SECCOUNT     0x1F2
#define ATA_PRIMARY_LBA_LOW      0x1F3
#define ATA_PRIMARY_LBA_MID      0x1F4
#define ATA_PRIMARY_LBA_HIGH     0x1F5
#define ATA_PRIMARY_DRIVE_SEL    0x1F6
#define ATA_PRIMARY_COMMAND      0x1F7
#define ATA_PRIMARY_STATUS       0x1F7

// Бит 6 (0x40) - LBA режим, Бит 4 (0x10) - Slave, Бит 7 и 5 - всегда 1
#define ATA_DRIVE_MASTER 0xE0 
#define ATA_DRIVE_SLAVE  0xF0

static void ata_400ns_delay() {
    for(int i = 0; i < 4; i++) inb(ATA_PRIMARY_STATUS);
}

static int ata_wait_bsy() {
    uint32_t timeout = 1000000;
    while ((inb(ATA_PRIMARY_STATUS) & 0x80) && --timeout);
    return timeout > 0;
}

static int ata_wait_drq() {
    uint32_t timeout = 1000000;
    while (!(inb(ATA_PRIMARY_STATUS) & 0x08) && --timeout);
    return timeout > 0;
}

void ata_identify() {
    outb(ATA_PRIMARY_DRIVE_SEL, 0xA0);
    outb(ATA_PRIMARY_SECCOUNT, 0);
    outb(ATA_PRIMARY_LBA_LOW, 0);
    outb(ATA_PRIMARY_LBA_MID, 0);
    outb(ATA_PRIMARY_LBA_HIGH, 0);
    outb(ATA_PRIMARY_COMMAND, 0xEC); // Identify command

    uint8_t status = inb(ATA_PRIMARY_STATUS);
    if (status == 0) {
        term_print("ATA: No drive found on Primary Master!\n");
        return;
    }

    if (!ata_wait_bsy() || !ata_wait_drq()) {
        term_print("ATA: Drive IDENTIFY timeout or error.\n");
        return;
    }

    uint16_t info[256];
    for (int i = 0; i < 256; i++) {
        info[i] = inw(ATA_PRIMARY_DATA);
    }

    // Модель диска в словах 27-46
    char model[41];
    for (int i = 0; i < 20; i++) {
        model[i*2] = (info[27+i] >> 8) & 0xFF;
        model[i*2+1] = info[27+i] & 0xFF;
    }
    model[40] = '\0';
    
    term_print("ATA: Drive found: ");
    term_print(model);
    term_print("\n");
}

void read_sectors_ata_pio(uintptr_t target_address, uint64_t LBA, uint8_t sector_count) {
    if (!ata_wait_bsy()) return;

    outb(ATA_PRIMARY_DRIVE_SEL, 0xE0 | ((LBA >> 24) & 0x0F));
    ata_400ns_delay();

    outb(ATA_PRIMARY_SECCOUNT, sector_count);
    outb(ATA_PRIMARY_LBA_LOW,  (uint8_t)LBA);
    outb(ATA_PRIMARY_LBA_MID,  (uint8_t)(LBA >> 8));
    outb(ATA_PRIMARY_LBA_HIGH, (uint8_t)(LBA >> 16));
    outb(ATA_PRIMARY_COMMAND,  0x20); // Read

    uint16_t* target = (uint16_t*)target_address;

    for (int j = 0; j < sector_count; j++) {
        if (!ata_wait_bsy() || !ata_wait_drq()) return;
        for (int i = 0; i < 256; i++) {
            target[i] = inw(ATA_PRIMARY_DATA);
        }
        target += 256;
    }
}

void write_sectors_ata_pio(uint64_t LBA, uint8_t sector_count, uint16_t* buffer) {
    if (!ata_wait_bsy()) return;

    outb(ATA_PRIMARY_DRIVE_SEL, ATA_DRIVE_MASTER | ((LBA >> 24) & 0x0F));
    ata_400ns_delay();

    outb(ATA_PRIMARY_SECCOUNT, sector_count);
    outb(ATA_PRIMARY_LBA_LOW,  (uint8_t)LBA);
    outb(ATA_PRIMARY_LBA_MID,  (uint8_t)(LBA >> 8));
    outb(ATA_PRIMARY_LBA_HIGH, (uint8_t)(LBA >> 16));
    outb(ATA_PRIMARY_COMMAND,  0x30); // Write Sectors

    for (int j = 0; j < sector_count; j++) {
        if (!ata_wait_bsy() || !ata_wait_drq()) return;
        for (int i = 0; i < 256; i++) {
            outw(ATA_PRIMARY_DATA, buffer[i]);
        }
    }
    
    outb(ATA_PRIMARY_COMMAND, 0xE7); // Cache Flush
    ata_wait_bsy();
}