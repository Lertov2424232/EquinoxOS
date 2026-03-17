#include "rtl8139.h"
#include "net.h"
#include "../../io/io.h"
#include "../../libc/string.h"

// Регистры
#define RTL_REG_COMMAND 0x37
#define RTL_REG_IMR     0x3C 

extern void term_print(const char* str);

// Переменные
uint32_t rtl_io_base = 0;
uint8_t mac_addr[6];
uint8_t tx_buffer[2048] __attribute__((aligned(4)));

void rtl8139_init(uint32_t bar0) {
    rtl_io_base = bar0 & ~3;
    outb(rtl_io_base + RTL_REG_COMMAND, 0x0C); 
    outw(rtl_io_base + RTL_REG_IMR, 0x0005); 

    for (int i = 0; i < 6; i++) {
        mac_addr[i] = inb(rtl_io_base + i);
    }

    char msg[64] = "[ETH] MAC Address: ";
    const char* digits = "0123456789ABCDEF";
    int pos = 19;
    for (int i = 0; i < 6; i++) {
        msg[pos++] = digits[(mac_addr[i] >> 4) & 0x0F];
        msg[pos++] = digits[mac_addr[i] & 0x0F];
        if (i < 5) msg[pos++] = ':';
    }
    msg[pos] = '\0';
    
    term_print("[ETH] RTL8139 Initialized");
    term_print(msg);
}

void rtl8139_send_packet(void* data, uint32_t len) {
    memcpy(tx_buffer, data, len);
    outl(rtl_io_base + 0x20, (uint32_t)tx_buffer); 
    outl(rtl_io_base + 0x10, len); 
}

void send_ethernet_frame(uint8_t* dest_mac, uint16_t ethertype, uint8_t* payload, uint32_t payload_len) {
    uint8_t frame[1514];
    ethernet_header_t* header = (ethernet_header_t*)frame;

    // --- ВРУЧНУЮ ЗАПОЛНЯЕМ ЗАГОЛОВОК (БЕЗ MEMCPY) ---
    for(int i=0; i<6; i++) header->dest_mac[i] = dest_mac[i];
    for(int i=0; i<6; i++) header->src_mac[i] = mac_addr[i];
    header->ethertype = HTONS(ethertype);

    // --- ВРУЧНУЮ КОПИРУЕМ ПЕЙЛОАД ---
    for(uint32_t i=0; i<payload_len; i++) frame[14 + i] = payload[i];

    // --- ОТЛАДОЧНЫЙ ВЫВОД (ЧТОБЫ УВИДЕТЬ ДАННЫЕ В ТЕРМИНАЛЕ) ---
    // Если тут увидишь правильные цифры, значит дело было в memcpy
    char debug[30];
    debug[0] = (header->dest_mac[0] >> 4) + '0'; // (упрощенно)
    term_print("Dest MAC start...");
    // Debug: печатаем, что мы реально шлем
    if (mac_addr[0] == 0) {
        term_print("[ERROR] MAC is ZERO! Check rtl8139.c!");
    }
    rtl8139_send_packet(frame, 14 + payload_len);
}

void send_arp_request(uint32_t target_ip) {
    uint8_t arp_payload[28]; // Размер ARP пакета 28 байт
    arp_header_t* arp = (arp_header_t*)arp_payload;

    arp->htype = HTONS(1);
    arp->ptype = HTONS(0x0800);
    arp->hlen = 6;
    arp->plen = 4;
    arp->oper = HTONS(1); // REQUEST
    
    // Твой MAC (mac_addr)
    memcpy(arp->sha, mac_addr, 6);
    
    // Твой IP (хардкодом для начала: 10.0.2.15)
    // 10.0.2.15 в HEX = 0x0A00020F
    arp->spa = HTONL(0x0A00020F); 
    
    // Target MAC (пустой в запросе)
    memset(arp->tha, 0, 6);
    
    // Target IP (роутер: 10.0.2.2 = 0x0A000202)
    arp->tpa = HTONL(target_ip);

    // Шлем на Broadcast MAC
    uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    
    // Используем функцию из прошлого шага!
    send_ethernet_frame(bcast, 0x0806, arp_payload, 28);
}