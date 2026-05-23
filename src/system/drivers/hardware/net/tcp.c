#include "tcp.h"
#include "../libc/stdio.h"
#include "../libc/string.h"
#include "../system/memory.h"
#include "ipv4.h"

extern void term_print(const char *str);

#define MAX_TCP_SOCKETS 32
static tcp_socket_t tcp_sockets[MAX_TCP_SOCKETS];

uint16_t tcp_checksum(uint32_t src_ip, uint32_t dest_ip, tcp_header_t *tcp,
                      uint8_t *payload, uint32_t payload_len) {
  uint32_t sum = 0;
  uint16_t tcp_len = sizeof(tcp_header_t) + payload_len;

  // Pseudo Header
  uint16_t s_ip[2] = {(uint16_t)(src_ip >> 16), (uint16_t)(src_ip & 0xFFFF)};
  uint16_t d_ip[2] = {(uint16_t)(dest_ip >> 16), (uint16_t)(dest_ip & 0xFFFF)};

  sum += HTONS(s_ip[0]);
  sum += HTONS(s_ip[1]);
  sum += HTONS(d_ip[0]);
  sum += HTONS(d_ip[1]);
  sum += HTONS(6); // Protocol
  sum += HTONS(tcp_len);

  uint16_t *ptr = (uint16_t *)tcp;
  for (int i = 0; i < (int)sizeof(tcp_header_t) / 2; i++)
    sum += ptr[i];

  uint16_t *p_ptr = (uint16_t *)payload;
  for (int i = 0; i < (int)payload_len / 2; i++)
    sum += p_ptr[i];
  if (payload_len % 2)
    sum += (uint16_t)((uint8_t *)payload)[payload_len - 1];

  while (sum >> 16)
    sum = (sum & 0xFFFF) + (sum >> 16);
  return (uint16_t)~sum;
}

void tcp_send_packet(net_interface_t *iface, tcp_socket_t *sock, uint8_t flags,
                     uint8_t *payload, uint32_t payload_len) {
  if (!iface || !sock)
    return;

  uint32_t total_len = sizeof(tcp_header_t) + payload_len;
  uint8_t *buffer = kmalloc(total_len);
  memset(buffer, 0, total_len);

  tcp_header_t *tcp = (tcp_header_t *)buffer;
  tcp->src_port = HTONS(sock->local_port);
  tcp->dest_port = HTONS(sock->remote_port);
  tcp->seq = HTONL(sock->seq);
  tcp->ack = HTONL(sock->ack);
  tcp->data_offset = 0x50; // 5 * 4 = 20 bytes
  tcp->flags = flags;
  tcp->window_size = HTONS(8192);

  if (payload && payload_len > 0) {
    memcpy(buffer + sizeof(tcp_header_t), payload, payload_len);
  }

  tcp->checksum = 0;
  tcp->checksum =
      tcp_checksum(iface->ip, sock->remote_ip, tcp, payload, payload_len);

  ipv4_send_packet(iface, sock->remote_ip, 6, buffer, total_len);
  kfree(buffer);
}

void handle_tcp(net_interface_t *iface, uint8_t *packet, uint32_t ip_hdr_len) {
  ipv4_header_t *ip = (ipv4_header_t *)(packet + sizeof(ethernet_header_t));
  tcp_header_t *tcp =
      (tcp_header_t *)(packet + sizeof(ethernet_header_t) + ip_hdr_len);

  uint16_t dest_port = HTONS(tcp->dest_port);
  uint16_t src_port = HTONS(tcp->src_port);
  uint32_t src_ip = HTONL(ip->src_ip);

  // Find socket
  tcp_socket_t *sock = NULL;
  for (int i = 0; i < MAX_TCP_SOCKETS; i++) {
    if (tcp_sockets[i].active && tcp_sockets[i].local_port == dest_port) {
      sock = &tcp_sockets[i];
      break;
    }
  }

  if (!sock) {
    // Log missed packets to see if we're receiving anything
    char mbuf[80];
    sprintf(mbuf,
            "[TCP] Packet for unknown port: %d (looking for local sockets)\n",
            dest_port);
    term_print(mbuf);
    return;
  }

  char lbuf[128];
  sprintf(lbuf, "[TCP] RX: Flags=%02x Seq=%u Ack=%u\n", tcp->flags,
          HTONL(tcp->seq), HTONL(tcp->ack));
  term_print(lbuf);

  uint32_t hdr_len = (tcp->data_offset >> 4) * 4;
  uint32_t payload_len = HTONS(ip->len) - ip_hdr_len - hdr_len;
  uint32_t seq = HTONL(tcp->seq);
  uint32_t ack = HTONL(tcp->ack);

  if (tcp->flags & TCP_RST) {
    term_print("[TCP] Connection Reset by Remote Host.\n");
    sock->state = TCP_CLOSED;
    sock->active = false;
    return;
  }

  switch (sock->state) {
  case TCP_SYN_SENT:
    if ((tcp->flags & TCP_SYN) && (tcp->flags & TCP_ACK)) {
      term_print(
          "[TCP] Handshake: SYN+ACK Received. Connection Established.\n");
      sock->ack = seq + 1;
      sock->seq = ack;
      sock->state = TCP_ESTABLISHED;
      tcp_send_packet(iface, sock, TCP_ACK, NULL, 0);

      // If this is port 8080 or has on_data, assume it's wget and send GET
      if (sock->remote_port == 8080 || sock->remote_port == 80) {
        extern char last_queried_name[];
        char get[512];
        sprintf(get,
                "GET / HTTP/1.1\r\n"
                "Host: %s\r\n"
                "User-Agent: EquinoxBrowser/1.0\r\n"
                "Accept: text/html\r\n"
                "Connection: close\r\n\r\n",
                last_queried_name[0] ? last_queried_name : "10.0.2.2");

        tcp_send_packet(iface, sock, TCP_PSH | TCP_ACK, (uint8_t *)get,
                        strlen(get));
      }
    }
    break;

  case TCP_ESTABLISHED:
    if (payload_len > 0) {
      sock->ack = seq + payload_len;
      sock->seq = ack;
      if (sock->on_data) {
        sock->on_data((uint8_t *)tcp + hdr_len, payload_len);
      }
      tcp_send_packet(iface, sock, TCP_ACK, NULL, 0);
    }
    if (tcp->flags & TCP_FIN) {
      sock->ack = seq + 1;
      sock->seq = ack;
      tcp_send_packet(iface, sock, TCP_ACK | TCP_FIN, NULL, 0);
      sock->state = TCP_CLOSED;
      sock->active = false;
      term_print("[TCP] Connection closed by Remote Host.\n");

      extern bool http_finished;
      http_finished = true;
    }
    break;

  default:
    break;
  }
}

tcp_socket_t *tcp_connect(net_interface_t *iface, uint32_t dest_ip,
                          uint16_t port, tcp_callback_t callback) {
  if (!iface)
    return NULL;

  for (int i = 0; i < MAX_TCP_SOCKETS; i++) {
    if (!tcp_sockets[i].active) {
      tcp_sockets[i].remote_ip = dest_ip;
      tcp_sockets[i].remote_port = port;
      tcp_sockets[i].local_port = 50000 + i; // Ephemeral port
      tcp_sockets[i].seq = 1000;
      tcp_sockets[i].ack = 0;
      tcp_sockets[i].state = TCP_SYN_SENT;
      tcp_sockets[i].on_data = callback;
      tcp_sockets[i].active = true;

      tcp_send_packet(iface, &tcp_sockets[i], TCP_SYN, NULL, 0);
      return &tcp_sockets[i];
    }
  }
  return NULL;
}

// --- WGET Implementation ---

uint8_t *http_response_buf = NULL;
uint32_t http_response_len = 0;
bool http_finished = false;

static void wget_on_data(uint8_t *data, uint32_t len) {
  if (!http_response_buf) {
    http_response_buf = kmalloc(65536); // Max 64KB for now
    memset(http_response_buf, 0, 65536);
  }

  if (http_response_len + len < 65536) {
    memcpy(http_response_buf + http_response_len, data, len);
    http_response_len += len;
  }

  // Basic search for end of stream or just let it time out in syscall
  // In a real browser we'd check Content-Length or wait for FIN
  char *body = strstr((char *)data, "\r\n\r\n");
  if (body) {
    // In this simple wget, we consider it "finished" if we got some data
    // and maybe after a short delay or FIN. For now, let's just mark it.
  }

  // If we get FIN, it will be handled in handle_tcp which sets active=false.
}

void net_wget(net_interface_t *iface, uint32_t dest_ip) {
  term_print("[TCP] Initiating HTTP GET (Port 80)...\n");
  tcp_connect(iface, dest_ip, 80, wget_on_data);
}
