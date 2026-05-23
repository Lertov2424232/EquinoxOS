#ifndef UDP_H
#define UDP_H

#include "system/drivers/hardware/net/net.h"
#include <stdbool.h>

typedef void (*udp_callback_t)(uint32_t src_ip, uint16_t src_port,
                               uint8_t *data, uint32_t len);

typedef struct {
  uint16_t local_port;
  udp_callback_t callback;
  bool active;
} udp_socket_t;

void handle_udp(net_interface_t *iface, uint8_t *packet, uint32_t ip_hdr_len);
void udp_send_packet(net_interface_t *iface, uint32_t dest_ip,
                     uint16_t src_port, uint16_t dest_port, uint8_t *data,
                     uint32_t len);

// Application-level shortcuts
void send_ntp_request(net_interface_t *iface);
bool udp_bind(uint16_t port, udp_callback_t callback);

#endif
