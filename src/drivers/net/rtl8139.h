#ifndef RTL8139_H
#define RTL8139_H

#include <stdint.h>
#include "net.h"
#include <stdbool.h>

// 1. Опережающее объявление, чтобы компилятор знал, что такая структура существует
struct vfs_node; 
struct ipv4_header;
struct tcp_header;

void rtl8139_init(uint32_t bar0);
void rtl8139_send_packet(void* data, uint32_t len);
void send_ethernet_frame(uint8_t* dest_mac, uint16_t ethertype, uint8_t* payload, uint32_t payload_len);
void send_arp_request(uint32_t target_ip);
void send_ntp_request();
uint16_t ip_checksum(void* vdata, uint32_t length);
void rtl8139_receive();
void send_arp_reply(uint8_t* dest_mac, uint32_t dest_ip);
void rtl8139_install_vfs(void);
// Теперь компилятор поймет, что это та же самая структура, что и в vfs.h
uint32_t rtl8139_vfs_write(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
// В rtl8139.h
uint16_t tcp_checksum(ipv4_header_t* ip, tcp_header_t* tcp, uint8_t* payload, uint32_t payload_len);
void send_tcp(uint8_t flags, uint8_t* payload, uint32_t payload_len);
void net_wget();
bool rtl8139_has_data();

#endif