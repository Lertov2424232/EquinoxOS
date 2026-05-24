#include "pci.h"
#include "../../../core/io.h"
#include "../../../drivers/hardware/net/rtl8139.h"
#include "../../../drivers/devices/audio/ac97.h"


extern void term_print(const char* str); 

// --- ПОРТЫ PCI ---
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

// --- СМЕЩЕНИЯ РЕГИСТРОВ PCI ---
#define PCI_REG_VENDOR_DEVICE 0x00
#define PCI_REG_COMMAND       0x04
#define PCI_REG_BAR0          0x10

// --- ФЛАГИ COMMAND REGISTER ---
#define PCI_CMD_IO_SPACE      (1 << 0) // Разрешить доступ через IN/OUT (порты)
#define PCI_CMD_BUS_MASTER    (1 << 2) // Разрешить устройству DMA (прямой доступ к памяти)


// =========================================================================
//                   БАЗОВЫЕ ФУНКЦИИ ЧТЕНИЯ/ЗАПИСИ
// =========================================================================

uint32_t pci_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) |
                                  (func << 8) | (offset & 0xFC) | 0x80000000);
              
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | 
                                  (func << 8) | (offset & 0xFC) | 0x80000000);
                                  
    outl(PCI_CONFIG_ADDRESS, address);
    outw(PCI_CONFIG_DATA + (offset & 2), val);
}

// =========================================================================
//                   ПОИСК И ИНИЦИАЛИЗАЦИЯ УСТРОЙСТВ
// =========================================================================

// Вспомогательная функция для проверки одного слота
static void pci_check_device(uint8_t bus, uint8_t slot) {
    uint32_t vendor_device = pci_read_dword(bus, slot, 0, PCI_REG_VENDOR_DEVICE);
    uint16_t vendor = vendor_device & 0xFFFF;
    uint16_t device = (vendor_device >> 16) & 0xFFFF;

    // Если Vendor ID = 0xFFFF, значит устройство в слоте отсутствует
    if (vendor == 0xFFFF) return; 

    // --- 1. Realtek RTL8139 (Сетевая карта) ---
    if (vendor == 0x10EC && device == 0x8139) {
        term_print("[PCI] Found Realtek RTL8139 Network Card!\n");
        
        // Читаем Command Register
        uint16_t command = pci_read_dword(bus, slot, 0, PCI_REG_COMMAND) & 0xFFFF;
        
        // Включаем Bus Mastering (для DMA) и I/O Space (для портов) разом
        command |= (PCI_CMD_IO_SPACE | PCI_CMD_BUS_MASTER); 
        pci_write_word(bus, slot, 0, PCI_REG_COMMAND, command);
        term_print("[PCI] RTL8139 Bus Mastering and I/O Enabled.\n");

        // Читаем базовый адрес (BAR0) и передаем в драйвер
        uint32_t bar0 = pci_read_dword(bus, slot, 0, PCI_REG_BAR0);
        rtl8139_init(bar0);
        return;
    }
    if (vendor == 0x8086 && (device == 0x2415 || device == 0x2425)) {
    term_print("[PCI] Found Intel AC'97 Audio!\n");
    
    // Включаем Bus Mastering (ОБЯЗАТЕЛЬНО для DMA)
    uint16_t command = pci_read_dword(bus, slot, 0, 0x04) & 0xFFFF;
    command |= (1 << 0) | (1 << 2); // IO Space + Bus Master
    pci_write_word(bus, slot, 0, 0x04, command);

    uint32_t bar0 = pci_read_dword(bus, slot, 0, 0x10); // NAM
    uint32_t bar1 = pci_read_dword(bus, slot, 0, 0x14); // NAB
    
    ac97_init(bar0, bar1);
    return;
}
}

void pci_init() {
    term_print("[PCI] Scanning buses...\n");
    
    // Перебираем все шины (0-255) и все слоты (0-31)
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            pci_check_device(bus, slot);
        }
    }
    
    term_print("[PCI] Scan complete.\n");
}
