#include "pci.h"
#include "../../io/io.h"
#include "../../libc/stdio.h"
#include "../../api.h" // Для term_print (если нужно вытащить функцию наружу)
#include "../net/rtl8139.h"


// Внешняя функция из твоего ядра для печати в терминал
extern void term_print(const char* str); 

uint32_t pci_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    
    // Формируем адрес для порта 0xCF8
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
              
    outl(0xCF8, address);
    return inl(0xCFC);
}

void pci_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
    outl(0xCF8, address);
    outw(0xCFC + (offset & 2), val);
}

void pci_init() {
    term_print("[PCI] Scanning buses...");
    
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            // Читаем Vendor ID (кто произвел) и Device ID (что за устройство)
            uint32_t vendor_device = pci_read_dword(bus, slot, 0, 0);
            uint16_t vendor = vendor_device & 0xFFFF;
            uint16_t device = (vendor_device >> 16) & 0xFFFF;

            if (vendor == 0xFFFF) continue; // Пустой слот

            // Ищем нашу сетевую карту Realtek RTL8139
            if (vendor == 0x10EC && device == 0x8139) {
                uint32_t command = pci_read_dword(bus, slot, 0, 0x04);
                pci_write_word(bus, slot, 0, 0x04, (uint16_t)(command | 0x4)); 
                term_print("[PCI] Bus Mastering Enabled!");
                term_print("[PCI] FOUND REALTEK RTL8139 NETWORK CARD!");
                uint16_t pci_cmd = pci_read_dword(bus, slot, 0, 0x04) & 0xFFFF;
                pci_cmd |= (1 << 0) | (1 << 2); 
                pci_write_word(bus, slot, 0, 0x04, pci_cmd);
                term_print("[PCI] Bus Mastering and I/O Enabled!");
                uint32_t bar0 = pci_read_dword(bus, slot, 0, 0x10);
                rtl8139_init(bar0);
            }
        }
    }
}

