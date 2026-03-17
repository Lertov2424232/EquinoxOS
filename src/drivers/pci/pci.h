#ifndef PCI_H
#define PCI_H
#include <stdint.h>

void pci_init(void);
uint32_t pci_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

#endif