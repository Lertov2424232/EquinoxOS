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

    memcpy(header->dest_mac, dest_mac, 6);
    memcpy(header->src_mac, mac_addr, 6);
    header->ethertype = HTONS(ethertype);

    memcpy(frame + 14, payload, payload_len);
    rtl8139_send_packet(frame, 14 + payload_len);
}