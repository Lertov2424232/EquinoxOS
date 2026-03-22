#include "drivers/net/rtl8139.h"
#include "drivers/net/net.h"
#include "io/io.h"
#include "fs/vfs.h"
#include "libc/string.h"
#include "libc/stdio.h"
#include "api.h"

uint32_t tcp_seq = 1000;
uint32_t tcp_ack = 0;


extern void term_print(const char* str); // Из ядра

// --- РЕГИСТРЫ RTL8139 ---
#define REG_MAC         0x00
#define REG_TSD0        0x10 // Transmit Status (4 дескриптора)
#define REG_TSAD0       0x20 // Transmit Address (4 дескриптора)
#define REG_RBSTART     0x30 // Receive Buffer Start
#define REG_COMMAND     0x37
#define REG_CAPR        0x38 // Current Address of Packet Read
#define REG_IMR         0x3C // Interrupt Mask
#define REG_ISR         0x3E // Interrupt Status
#define REG_TCR         0x40 // Transmit Configuration
#define REG_RCR         0x44 // Receive Configuration
#define REG_CONFIG1     0x52

// --- ЖЕСТКО ЗАДАННЫЕ АДРЕСА ПАМЯТИ ---
// TODO: В будущем выделять через kmalloc()!
#define TX_BUFFER_BASE  0x80000 // 512 KB
#define RX_BUFFER_BASE  0x90000 // 576 KB
#define RX_BUFFER_SIZE  8192

// --- ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ДРАЙВЕРА ---
uint32_t rtl_io_base = 0;
uint8_t mac_addr[6];
int tx_cur_desc = 0;
uint8_t* tx_buffer = (uint8_t*)TX_BUFFER_BASE; 
uint8_t* rx_buffer = (uint8_t*)RX_BUFFER_BASE; 
uint16_t rx_offset = 0;


// =========================================================================
//                   ВРЕМЕННЫЙ СЕТЕВОЙ СТЕК (NETWORK STACK)
// TODO: Вынести логику ARP, IP, UDP и NTP в отдельный файл network.c
// =========================================================================

uint16_t ip_checksum(void* vdata, uint32_t length) {
    uint32_t sum = 0;
    uint16_t* ptr = (uint16_t*)vdata;

    while (length > 1) { sum += *ptr++; length -= 2; }
    if (length > 0) { sum += *(uint8_t*)ptr; }
    while (sum >> 16) { sum = (sum & 0xFFFF) + (sum >> 16); }

    return (uint16_t)(~sum);
}

static void handle_arp(uint8_t* packet) {
    arp_header_t* arp = (arp_header_t*)(packet + sizeof(ethernet_header_t));
    
    // Если это ARP Запрос (REQUEST) и ищут наш IP (10.0.2.15 = 0x0A00020F)
    if (arp->oper == HTONS(1) && arp->tpa == HTONL(0x0A00020F)) {
        send_arp_reply(arp->sha, arp->spa);
    }
}

static void handle_udp_ntp(uint8_t* packet, uint32_t ip_hdr_len) {
    udp_header_t* udp = (udp_header_t*)(packet + sizeof(ethernet_header_t) + ip_hdr_len);
    uint16_t dest_port = HTONS(udp->dest_port);

    if (dest_port == 1234) {
        term_print("[NET] NTP ANSWER FOUND!");

        // NTP данные лежат после Eth + IP + UDP
        uint8_t* ntp_data = packet + sizeof(ethernet_header_t) + ip_hdr_len + sizeof(udp_header_t);
        
        // Читаем Transmit Timestamp (смещение 40 байт внутри NTP).
        // Читаем побайтово для защиты от невыровненного доступа к памяти.
        uint32_t ntp_sec = ((uint32_t)ntp_data[40] << 24) | ((uint32_t)ntp_data[41] << 16) |
                           ((uint32_t)ntp_data[42] << 8)  | ((uint32_t)ntp_data[43]);

        if (ntp_sec == 0) {
            term_print("[ERR] NTP data is empty");
        } else {
            // NTP -> UNIX время (сдвиг 2208988800 секунд с 1900 до 1970 года)
            uint32_t unix_timestamp = ntp_sec - 2208988800U;

            term_print("---------------------------");
            term_print("TIME SYNCED SUCCESSFULLY!");
            
            char timestamp_str[32];
            itoa(unix_timestamp, 10, timestamp_str);
            term_print("UNIX:");
            term_print(timestamp_str);
            term_print("---------------------------");
        }
    }
}

static void handle_ipv4(uint8_t* packet) {
    ipv4_header_t* ip = (ipv4_header_t*)(packet + sizeof(ethernet_header_t));
    uint32_t ip_hdr_len = (ip->version_ihl & 0x0F) * 4;
    
    if (ip->proto == 17) { // UDP
        handle_udp_ntp(packet, ip_hdr_len);
    } 
    else if (ip->proto == 6) { // TCP (ПРОТОКОЛ 6)
        // 1. Сначала определяем, где в пакете лежит TCP заголовок
        tcp_header_t* tcp = (tcp_header_t*)(packet + sizeof(ethernet_header_t) + ip_hdr_len);
        
        // 2. Проверяем, что пакет пришел на наш порт (wget)
        if (HTONS(tcp->dest_port) == 49152) {
            
            // Если сервер сбросил соединение
            if (tcp->flags & TCP_RST) {
                term_print("[TCP] Connection reset.");
                return;
            }

            // 3. Считаем длины и достаем Seq/Ack (объявляем те самые переменные!)
            uint32_t tcp_hdr_len = (tcp->data_offset >> 4) * 4;
            uint32_t payload_len = HTONS(ip->len) - ip_hdr_len - tcp_hdr_len;
            uint32_t incoming_seq = HTONL(tcp->seq);
            uint32_t incoming_ack = HTONL(tcp->ack);

            // 4. Логика рукопожатия (SYN-ACK)
            if ((tcp->flags & TCP_SYN) && (tcp->flags & TCP_ACK)) {
                term_print("[TCP] Handshake OK. Sending GET...");
                tcp_ack = incoming_seq + 1;
                tcp_seq = incoming_ack;
                
                send_tcp(TCP_ACK, NULL, 0); 
                
                char* http_req = "GET / HTTP/1.1\r\n"
                 "Host: 10.0.2.2\r\n"
                 "Connection: close\r\n\r\n";
                send_tcp(TCP_PSH | TCP_ACK, (uint8_t*)http_req, strlen(http_req));
            }
            // 5. ЛОГИКА ПРИЕМА ДАННЫХ (ТО, ЧТО ТЫ ВСТАВЛЯЛ)
            else if (payload_len > 0) {
                if (incoming_seq >= tcp_ack) {
                    tcp_ack = incoming_seq + payload_len;
                    tcp_seq = incoming_ack;

                    term_print("[HTTP] Received data packet!");
                    
                    // Выведем только первую строчку для теста
                    uint8_t* data = (uint8_t*)tcp + tcp_hdr_len;
                    char preview[40];
                    uint32_t len = (payload_len > 30) ? 30 : payload_len;
                    memcpy(preview, data, len);
                    preview[len] = '\0';
                    term_print(preview);

                    send_tcp(TCP_ACK, NULL, 0); // Подтверждаем
                }
            }
            // 6. Закрытие соединения
            else if (tcp->flags & TCP_FIN) {
                term_print("[TCP] Closing connection.");
                tcp_ack = incoming_seq + 1;
                send_tcp(TCP_ACK | TCP_FIN, NULL, 0);
            }
        }
    }
}

// Главный распределитель входящих пакетов
static void net_handle_packet(uint8_t* packet, uint16_t length) {
    (void)length; // Пока не используем жесткий контроль длины
    
    ethernet_header_t* eth = (ethernet_header_t*)packet;
    uint16_t type = HTONS(eth->ethertype);

    if (type == 0x0806) {
        handle_arp(packet);
    } else if (type == 0x0800) {
        handle_ipv4(packet);
    }
}


// =========================================================================
//                       ОТПРАВКА СЕТЕВЫХ ПАКЕТОВ
// =========================================================================

void send_ethernet_frame(uint8_t* dest_mac, uint16_t ethertype, uint8_t* payload, uint32_t payload_len) {
    if (payload_len > 1500) {
        term_print("[NET] ERROR: Frame too large!");
        return;
    }
    
    uint8_t frame[1514];
    ethernet_header_t* header = (ethernet_header_t*)frame;

    memcpy(header->dest_mac, dest_mac, 6);
    memcpy(header->src_mac, mac_addr, 6);
    header->ethertype = HTONS(ethertype);

    memcpy(frame + sizeof(ethernet_header_t), payload, payload_len);

    rtl8139_send_packet(frame, sizeof(ethernet_header_t) + payload_len);
}

void send_arp_request(uint32_t target_ip) {
    uint8_t arp_payload[sizeof(arp_header_t)]; 
    arp_header_t* arp = (arp_header_t*)arp_payload;

    arp->htype = HTONS(1);
    arp->ptype = HTONS(0x0800);
    arp->hlen = 6;
    arp->plen = 4;
    arp->oper = HTONS(1); // 1 = REQUEST
    
    memcpy(arp->sha, mac_addr, 6);
    arp->spa = HTONL(0x0A00020F); // Наш IP (10.0.2.15)
    
    memset(arp->tha, 0, 6);
    arp->tpa = HTONL(target_ip);

    uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    send_ethernet_frame(bcast, 0x0806, arp_payload, sizeof(arp_header_t));
}

void send_arp_reply(uint8_t* dest_mac, uint32_t dest_ip) {
    uint8_t arp_payload[sizeof(arp_header_t)];
    arp_header_t* arp = (arp_header_t*)arp_payload;

    arp->htype = HTONS(1);
    arp->ptype = HTONS(0x0800);
    arp->hlen = 6;
    arp->plen = 4;
    arp->oper = HTONS(2); // 2 = REPLY 

    memcpy(arp->sha, mac_addr, 6); 
    arp->spa = HTONL(0x0A00020F);  

    memcpy(arp->tha, dest_mac, 6); 
    arp->tpa = HTONL(dest_ip);     

    send_ethernet_frame(dest_mac, 0x0806, arp_payload, sizeof(arp_header_t));
}

void send_ntp_request() {
    uint8_t buffer[128];
    memset(buffer, 0, 128);

    uint16_t ntp_payload_len = sizeof(ipv4_header_t) + sizeof(udp_header_t) + sizeof(ntp_packet_t);

    ipv4_header_t* ip = (ipv4_header_t*)buffer;
    udp_header_t* udp = (udp_header_t*)(buffer + sizeof(ipv4_header_t));
    ntp_packet_t* ntp = (ntp_packet_t*)(buffer + sizeof(ipv4_header_t) + sizeof(udp_header_t));

    ntp->mode = 0x23; // NTP Client

    udp->src_port = HTONS(1234); 
    udp->dest_port = HTONS(123);  // NTP Port
    udp->len = HTONS(sizeof(udp_header_t) + sizeof(ntp_packet_t));

    ip->version_ihl = 0x45;
    ip->len = HTONS(ntp_payload_len);
    ip->id = HTONS(1);
    ip->ttl = 64;
    ip->proto = 17; // UDP
    ip->src_ip = HTONL(0x0A00020F);  // Локальный IP
    ip->dest_ip = HTONL(0xA29FC801); // 162.159.200.1 (Cloudflare)
    
    ip->checksum = 0;
    ip->checksum = ip_checksum(ip, sizeof(ipv4_header_t));

    // Шлем пакет на MAC-адрес роутера (Gateway)
    uint8_t router_mac[6] = {0x52, 0x55, 0x0A, 0x00, 0x02, 0x02};
    
    send_ethernet_frame(router_mac, 0x0800, buffer, ntp_payload_len);
    term_print("[NET] NTP Request sent to Cloudflare...");
}


// =========================================================================
//                   АППАРАТНЫЙ ДРАЙВЕР КАРТЫ RTL8139
// =========================================================================

void rtl8139_init(uint32_t bar0) {
    rtl_io_base = bar0 & ~3;

    outb(rtl_io_base + REG_CONFIG1, 0x00);         // Power ON
    outl(rtl_io_base + REG_RCR, 0x0000000F | (1 << 7)); // Сброс
    
    outb(rtl_io_base + REG_COMMAND, 0x10);         // Программный Reset
    while((inb(rtl_io_base + REG_COMMAND) & 0x10) != 0);

    // Настройка буфера приема (AB+AM+APM+AAP)
    outl(rtl_io_base + REG_RBSTART, (uint32_t)(uintptr_t)rx_buffer);
    outl(rtl_io_base + REG_RCR, 0x0F | (1 << 7));  // 8K буфер

    outb(rtl_io_base + REG_COMMAND, 0x0C);         // Включаем TX и RX
    outl(rtl_io_base + REG_TCR, 0x03000000);       // Стандартный интервал пакетов

    for (int i = 0; i < 6; i++) {
        mac_addr[i] = inb(rtl_io_base + REG_MAC + i);
    }

    term_print("[NET] RTL8139 Hardware Ready.");
}

void rtl8139_send_packet(void* data, uint32_t len) {
    uint32_t send_len = (len < 60) ? 60 : len;
    uint8_t* current_tx_ptr = (uint8_t*)(TX_BUFFER_BASE + (tx_cur_desc * 512));
    
    memcpy(current_tx_ptr, data, len);
    if (len < 60) memset(current_tx_ptr + len, 0, 60 - len);

    // Барьер памяти компилятора
    __asm__ volatile("" : : : "memory");

    uint32_t tsad_reg = REG_TSAD0 + (tx_cur_desc * 4);
    uint32_t tsd_reg  = REG_TSD0  + (tx_cur_desc * 4);

    outl(rtl_io_base + tsad_reg, (uint32_t)(uintptr_t)current_tx_ptr); 
    outl(rtl_io_base + tsd_reg, send_len | 0x0000); // 0x0000 = Threshold 0

    tx_cur_desc = (tx_cur_desc + 1) % 4; // Двигаем указатель кольцевого буфера
}

void rtl8139_receive() {
    uint16_t isr_status = inw(rtl_io_base + REG_ISR);
    if (!(isr_status & 0x01)) return; 
    
    outw(rtl_io_base + REG_ISR, 0x01); // Очищаем флаг прерывания RX OK

    // Пока буфер не пуст (бит 0 в COMMAND сброшен = есть данные)
    while (!(inb(rtl_io_base + REG_COMMAND) & 0x01)) {
        uint16_t* header = (uint16_t*)(rx_buffer + rx_offset);
        uint16_t length = header[1];

        // Защита от битых пакетов и зацикливания
        if (length < 20 || length > 1536) {
            rx_offset = 0;
            return;
        }

        uint8_t* packet = rx_buffer + rx_offset + 4; // Пропускаем 4 байта (статус + длина)
        
        // --- ПЕРЕДАЕМ ПАКЕТ В СЕТЕВОЙ СТЕК ---
        net_handle_packet(packet, length - 4);

        // Смещение: длина пакета + 4 (заголовок RTL) + 3 (выравнивание на 4 байта)
        rx_offset = (rx_offset + length + 4 + 3) & ~3;
        
        // Магическое число -16 для регистра CAPR (официальный фикс бага RTL8139)
        outw(rtl_io_base + REG_CAPR, rx_offset - 16);

        if (rx_offset >= RX_BUFFER_SIZE) rx_offset = 0;
    }
}


// =========================================================================
//                           ИНТЕГРАЦИЯ В VFS
// =========================================================================

uint32_t rtl8139_vfs_write(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)node; (void)offset; // Подавляем warnings
    rtl8139_send_packet(buffer, size);
    return size;
}

void rtl8139_install_vfs() {
    vfs_node_t* node = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    memset(node, 0, sizeof(vfs_node_t));
    
    strcpy(node->name, "net");
    node->write = rtl8139_vfs_write;
    node->flags = 2; // Как у устройства
    
    vfs_register_device(node);
}


// В rtl8139.c
uint16_t tcp_checksum(ipv4_header_t* ip, tcp_header_t* tcp, uint8_t* payload, uint32_t payload_len) {
    uint32_t sum = 0;
    uint16_t tcp_len = sizeof(tcp_header_t) + payload_len;

    // 1. Псевдозаголовок
    uint32_t src_ip = HTONL(ip->src_ip);
    uint32_t dest_ip = HTONL(ip->dest_ip);

    sum += (src_ip >> 16) & 0xFFFF;
    sum += src_ip & 0xFFFF;
    sum += (dest_ip >> 16) & 0xFFFF;
    sum += dest_ip & 0xFFFF;
    sum += 6; 
    sum += tcp_len;

    // 2. Заголовок TCP
    uint16_t* ptr = (uint16_t*)tcp;
    for (int i = 0; i < (int)sizeof(tcp_header_t) / 2; i++) {
        sum += HTONS(ptr[i]);
    }

    // 3. Данные
    uint16_t* p_ptr = (uint16_t*)payload;
    for (int i = 0; i < (int)payload_len / 2; i++) {
        sum += HTONS(p_ptr[i]);
    }

    if (payload_len % 2) {
        sum += (uint16_t)payload[payload_len - 1] << 8;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return HTONS((uint16_t)~sum);
}

void send_tcp(uint8_t flags, uint8_t* payload, uint32_t payload_len) {
    uint8_t buffer[1500];
    memset(buffer, 0, 1500);

    ipv4_header_t* ip = (ipv4_header_t*)buffer;
    tcp_header_t* tcp = (tcp_header_t*)(buffer + sizeof(ipv4_header_t));
    uint8_t* data = buffer + sizeof(ipv4_header_t) + sizeof(tcp_header_t);

    if (payload_len > 0) memcpy(data, payload, payload_len);

    tcp->src_port = HTONS(49152); // Наш порт
    tcp->dest_port = HTONS(8080); // Порт прокси-сервера (Python)
    tcp->seq = HTONL(tcp_seq);
    tcp->ack = HTONL(tcp_ack);
    tcp->data_offset = 0x50; // Размер заголовка TCP (20 байт)
    tcp->flags = flags;
    tcp->window_size = HTONS(8192);

    ip->version_ihl = 0x45;
    ip->len = HTONS(sizeof(ipv4_header_t) + sizeof(tcp_header_t) + payload_len);
    ip->id = HTONS(1);
    ip->ttl = 64;
    ip->proto = 6; // ПРОТОКОЛ TCP!
    ip->src_ip = HTONL(0x0A00020F); // Наш IP (10.0.2.15)
    ip->dest_ip = HTONL(0x0A000202); // IP Хоста (10.0.2.2)

    ip->checksum = ip_checksum(ip, sizeof(ipv4_header_t));
    tcp->checksum = 0;
    tcp->checksum = tcp_checksum(ip, tcp, payload, payload_len);
    

    uint8_t router_mac[6] = {0x52, 0x55, 0x0A, 0x00, 0x02, 0x02};
    send_ethernet_frame(router_mac, 0x0800, buffer, sizeof(ipv4_header_t) + sizeof(tcp_header_t) + payload_len);
}

// Вызываем это из shell.c !
void net_wget() {
    term_print("[TCP] Initiating Connection (SYN)...");
    tcp_seq = 1000;
    tcp_ack = 0;
    send_tcp(TCP_SYN, NULL, 0); // Шаг 1: Привет!
}