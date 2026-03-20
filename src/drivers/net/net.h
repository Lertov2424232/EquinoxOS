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

typedef struct {
    uint8_t  version_ihl;
    uint8_t  tos;
    uint16_t len;
    uint16_t id;
    uint16_t flags_offset;
    uint8_t  ttl;
    uint8_t  proto; // 17 = UDP
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dest_ip;
} __attribute__((packed)) ipv4_header_t;

typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t len;
    uint16_t checksum;
} __attribute__((packed)) udp_header_t;

// Структура NTP (48 байт)
typedef struct {
    uint8_t  mode; // 0x23 для клиента
    uint8_t  stratum;
    uint8_t  poll;
    uint8_t  precision;
    uint32_t root_delay;
    uint32_t root_dispersion;
    uint32_t ref_id;
    uint64_t ref_ts;
    uint64_t orig_ts;
    uint64_t recv_ts;
    uint64_t trans_ts; // Отсюда мы возьмем время
} __attribute__((packed)) ntp_packet_t;