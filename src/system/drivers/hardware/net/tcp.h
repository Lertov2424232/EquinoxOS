#ifndef TCP_H
#define TCP_H

#include "net.h"
#include <stdbool.h>

typedef enum {
  TCP_CLOSED,
  TCP_LISTEN,
  TCP_SYN_SENT,
  TCP_SYN_RECEIVED,
  TCP_ESTABLISHED,
  TCP_FIN_WAIT_1,
  TCP_FIN_WAIT_2,
  TCP_CLOSE_WAIT,
  TCP_CLOSING,
  TCP_LAST_ACK,
  TCP_TIME_WAIT
} tcp_state_t;

typedef void (*tcp_callback_t)(uint8_t *data, uint32_t len);

typedef struct {
  uint32_t remote_ip;
  uint16_t local_port;
  uint16_t remote_port;

  tcp_state_t state;

  uint32_t seq;
  uint32_t ack;

  tcp_callback_t on_data;
  bool active;
} tcp_socket_t;

void handle_tcp(net_interface_t *iface, uint8_t *packet, uint32_t ip_hdr_len);
void tcp_send_packet(net_interface_t *iface, tcp_socket_t *sock, uint8_t flags,
                     uint8_t *payload, uint32_t payload_len);

// Application-level shortcuts
void net_wget(net_interface_t *iface, uint32_t dest_ip);
tcp_socket_t *tcp_connect(net_interface_t *iface, uint32_t dest_ip,
                          uint16_t port, tcp_callback_t callback);

#endif
