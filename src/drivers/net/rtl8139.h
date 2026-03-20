#ifndef RTL8139_H
#define RTL8139_H

#include <stdint.h>

void rtl8139_init(uint32_t bar0);
void rtl8139_send_packet(void* data, uint32_t len);
void send_ethernet_frame(uint8_t* dest_mac, uint16_t ethertype, uint8_t* payload, uint32_t payload_len);
void send_arp_request(uint32_t target_ip);
void send_ntp_request();
void rtl8139_receive();

#endif