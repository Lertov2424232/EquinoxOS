#ifndef NET_H
#define NET_H

#include <stdint.h>

#define HTONS(x) ((x << 8) | (x >> 8))

typedef struct {
    uint8_t  dest_mac[6];
    uint8_t  src_mac[6];
    uint16_t ethertype; 
} __attribute__((packed)) ethernet_header_t;

// Добавь это в net.h
typedef struct {
    uint16_t htype;    // Hardware type (1 = Ethernet)
    uint16_t ptype;    // Protocol type (0x0800 = IPv4)
    uint8_t  hlen;     // Hardware size (6)
    uint8_t  plen;     // Protocol size (4)
    uint16_t oper;     // Opcode (1 = Request, 2 = Reply)
    uint8_t  sha[6];   // Sender MAC
    uint32_t spa;      // Sender IP
    uint8_t  tha[6];   // Target MAC
    uint32_t tpa;      // Target IP
} __attribute__((packed)) arp_header_t;

// И добавь функцию для IP (Host to Network Long - для 32 бит)
#define HTONL(x) (((x << 24) & 0xFF000000) | \
                  ((x << 8)  & 0x00FF0000) | \
                  ((x >> 8)  & 0x0000FF00) | \
                  ((x >> 24) & 0x000000FF))

#endif