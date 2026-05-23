#ifndef DNS_H
#define DNS_H

#include "system/drivers/hardware/net/net.h"
#include <stdint.h>

typedef struct {
    uint16_t id;
    uint16_t flags;
    uint16_t questions;
    uint16_t answers;
    uint16_t authority;
    uint16_t additional;
} __attribute__((packed)) dns_header_t;

void dns_query(net_interface_t* iface, const char* hostname);
uint32_t dns_get_result(const char* hostname); // Returns 0 if not resolved yet

#endif
