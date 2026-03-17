#ifndef NET_H
#define NET_H

#include <stdint.h>

#define HTONS(x) ((x << 8) | (x >> 8))

typedef struct {
    uint8_t  dest_mac[6];
    uint8_t  src_mac[6];
    uint16_t ethertype; 
} __attribute__((packed)) ethernet_header_t;

#endif