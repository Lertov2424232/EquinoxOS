#include "rtl8139.h"
#include "../../io/io.h"
#include "../fs/vfs.h"
#include "../../libc/string.h"
#include "net.h"

// Регистры по мануалу
#define REG_MAC         0x00
#define REG_TSD0        0x10
#define REG_TSAD0       0x20
#define REG_RBSTART     0x30
#define REG_COMMAND     0x37
#define REG_CAPR        0x38
#define REG_IMR         0x3C
#define REG_ISR         0x3E
#define REG_TCR         0x40
#define REG_RCR         0x44
#define REG_CONFIG1     0x52

uint32_t rtl_io_base = 0;
uint8_t mac_addr[6];
int tx_cur_desc = 0;
uint8_t* tx_buffer = (uint8_t*)0x80000; // 512 KB mark
uint8_t* rx_buffer = (uint8_t*)0x90000; // 576 KB mark
uint16_t rx_offset = 0;

uint32_t rtl8139_vfs_write(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    rtl8139_send_packet(buffer, size);
    return size;
}

void rtl8139_install_vfs() {
    vfs_node_t* node = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    memset(node, 0, sizeof(vfs_node_t));
    strcpy(node->name, "net");
    node->write = rtl8139_vfs_write;
    vfs_register_device(node);
}


void rtl8139_init(uint32_t bar0) {
    rtl_io_base = bar0 & ~3;

    // 1. Пробуждение (Power ON)
    outb(rtl_io_base + REG_CONFIG1, 0x00);

    outl(rtl_io_base + 0x44, 0x0000000F | (1 << 7));
    // 2. Программный сброс
    outb(rtl_io_base + REG_COMMAND, 0x10);
    while((inb(rtl_io_base + REG_COMMAND) & 0x10) != 0);

    // 3. Настройка буфера приема (обязательно для работы карты)
    // 0x0F = AB+AM+APM+AAP (принимать всё: broadcast, multicast, etc)
    outl(rtl_io_base + REG_RBSTART, (uint32_t)(uintptr_t)rx_buffer);
    outl(rtl_io_base + REG_RCR, 0xf | (1 << 7)); // 8K + 16 bytes buffer

    // 4. Включаем передатчик и приемник
    outb(rtl_io_base + REG_COMMAND, 0x0C); 

    // 5. Настройка параметров передачи (TCR)
    // 0x03000000 = стандартный интервал между пакетами
    outl(rtl_io_base + REG_TCR, 0x03000000);

    // Читаем MAC
    for (int i = 0; i < 6; i++) mac_addr[i] = inb(rtl_io_base + REG_MAC + i);

    term_print("[NET] RTL8139 Hardware Ready.");
}

void rtl8139_send_packet(void* data, uint32_t len) {
    uint32_t send_len = (len < 60) ? 60 : len;
    
    // 1. Вычисляем адрес буфера для текущего слота (0, 1, 2 или 3)
    uint8_t* current_tx_ptr = (uint8_t*)(0x80000 + (tx_cur_desc * 512));
    
    memcpy(current_tx_ptr, data, len);
    if (len < 60) memset(current_tx_ptr + len, 0, 60 - len);

    __asm__ volatile("" : : : "memory");

    // 2. Выбираем регистры в зависимости от слота
    // TSAD0 = 0x20, TSAD1 = 0x24, TSAD2 = 0x28, TSAD3 = 0x2C
    // TSD0  = 0x10, TSD1  = 0x14, TSD2  = 0x18, TSD3  = 0x1C
    uint32_t tsad_reg = 0x20 + (tx_cur_desc * 4);
    uint32_t tsd_reg  = 0x10 + (tx_cur_desc * 4);

    // 3. Загружаем адрес и даем команду на отправку
    outl(rtl_io_base + tsad_reg, (uint32_t)(uintptr_t)current_tx_ptr); 
    outl(rtl_io_base + tsd_reg, send_len | 0x0000); // Threshold 0

    // 4. Переходим к следующему слоту для следующего пакета
    tx_cur_desc = (tx_cur_desc + 1) % 4;

    // ВАЖНО: Мы больше не ждем здесь TX OK! 
    // Карта сама разберется, а мы просто идем дальше.
    // Это и есть настоящая асинхронная работа железа.
}

void rtl8139_receive() {
    uint16_t isr_status = inw(rtl_io_base + 0x3E);
    if (!(isr_status & 0x01)) return; 
    outw(rtl_io_base + 0x3E, 0x01);

    while (!(inb(rtl_io_base + 0x37) & 0x01)) {
        uint16_t* header = (uint16_t*)(rx_buffer + rx_offset);
        uint16_t length = header[1];

        // Защита от мусора и зацикливания
        if (length < 20 || length > 1536) {
            rx_offset = 0;
            return;
        }

        uint8_t* packet = rx_buffer + rx_offset + 4;
        ethernet_header_t* eth = (ethernet_header_t*)packet;
        uint16_t type = HTONS(eth->ethertype);

        if (type == 0x0806) { // ARP
            arp_header_t* arp = (arp_header_t*)(packet + 14);
            if (arp->oper == HTONS(1) && arp->tpa == HTONL(0x0A00020F)) {
                send_arp_reply(arp->sha, arp->spa);
            }
        } 
        else if (type == 0x0800) { // IP
            ipv4_header_t* ip = (ipv4_header_t*)(packet + 14);
            
            if (ip->proto == 17) { // UDP
                // Считаем реальный размер IP заголовка (обычно 20 байт, но может быть больше)
                uint32_t ip_hdr_len = (ip->version_ihl & 0x0F) * 4;
                udp_header_t* udp = (udp_header_t*)(packet + 14 + ip_hdr_len);
                
                uint16_t dest_port = HTONS(udp->dest_port);

                if (dest_port == 1234) {
                    term_print("[NET] NTP ANSWER FOUND!");

                    // NTP данные начинаются после Eth(14) + IP(ip_hdr_len) + UDP(8)
                    uint8_t* ntp_data = packet + 14 + ip_hdr_len + 8;
                    
                    // Читаем Transmit Timestamp (смещение 40 байт внутри NTP)
                    // Используем побайтовый доступ, чтобы не было ошибок выравнивания
                    uint32_t ntp_sec = 
                        ((uint32_t)ntp_data[40] << 24) |
                        ((uint32_t)ntp_data[41] << 16) |
                        ((uint32_t)ntp_data[42] << 8)  |
                        ((uint32_t)ntp_data[43]);

                    if (ntp_sec == 0) {
                        term_print("[ERR] NTP data is empty");
                    } else {
                        // NTP -> UNIX (разница 2208988800)
                        uint32_t unix_timestamp = ntp_sec - 2208988800U;

                        term_print("---------------------------");
                        term_print("TIME SYNCED SUCCESSFULLY!");
                        
                        char timestamp_str[32];
                        itoa(unix_timestamp, 10, timestamp_str);
                        term_print("UNIX:");
                        term_print(timestamp_str);
                        term_print("---------------------------");
                    }
                } else {
                    // Раскомментируй, если хочешь видеть весь мусор в сети:
                    term_print("UDP packet for other port.");
                }
            }
        }

        // Обновляем смещение (длина пакета + 4 байта заголовка RTL + 4 байта CRC)
        // И выравниваем на 4 байта, как того требует карта
        rx_offset = (rx_offset + length + 4 + 3) & ~3;
        
        // Магическое число -16 для регистра CAPR (фикс бага RTL8139)
        outw(rtl_io_base + 0x38, rx_offset - 16);

        if (rx_offset >= 8192) rx_offset = 0;
    }
}

void send_ethernet_frame(uint8_t* dest_mac, uint16_t ethertype, uint8_t* payload, uint32_t payload_len) {
    if (payload_len > 1500) {
        term_print("[NET] ERROR: Frame too large!");
        return;
    }
    uint8_t frame[1514];
    ethernet_header_t* header = (ethernet_header_t*)frame;

    // Копируем реальные адреса
    for(int i=0; i<6; i++) header->dest_mac[i] = dest_mac[i];
    for(int i=0; i<6; i++) header->src_mac[i] = mac_addr[i];
    header->ethertype = HTONS(ethertype);

    // Копируем данные
    for(uint32_t i=0; i<payload_len; i++) frame[14 + i] = payload[i];

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

void send_ntp_request() {
    uint8_t buffer[128];
    memset(buffer, 0, 128);

    uint16_t ntp_payload_len = sizeof(ipv4_header_t) + sizeof(udp_header_t) + sizeof(ntp_packet_t);

    ipv4_header_t* ip = (ipv4_header_t*)buffer;
    udp_header_t* udp = (udp_header_t*)(buffer + sizeof(ipv4_header_t));
    ntp_packet_t* ntp = (ntp_packet_t*)(buffer + sizeof(ipv4_header_t) + sizeof(udp_header_t));

    ntp->mode = 0x23; // NTP Client

    udp->src_port = HTONS(1234); // Наш порт
    udp->dest_port = HTONS(123);  // Порт NTP
    udp->len = HTONS(sizeof(udp_header_t) + sizeof(ntp_packet_t));

    ip->version_ihl = 0x45;
    ip->len = HTONS(ntp_payload_len);
    ip->id = HTONS(1);
    ip->ttl = 64;
    ip->proto = 17; // UDP
    ip->src_ip = HTONL(0x0A00020F); // Наш локальный IP
    ip->dest_ip = HTONL(0xA29FC801); // 162.159.200.1 (Cloudflare NTP)
    
    ip->checksum = 0;
    ip->checksum = ip_checksum(ip, sizeof(ipv4_header_t));

    // ВАЖНО: Пакет идет во внешний мир, но ШЛЕМ МЫ ЕГО РОУТЕРУ!
    // Поэтому Destination MAC — это MAC роутера (Gateway)
    uint8_t router_mac[6] = {0x52, 0x55, 0x0A, 0x00, 0x02, 0x02};
    
    send_ethernet_frame(router_mac, 0x0800, buffer, ntp_payload_len);
    term_print("[NET] NTP Request sent to Cloudflare...");
}

uint16_t ip_checksum(void* vdata, uint32_t length) {
    uint32_t sum = 0;
    uint16_t* ptr = (uint16_t*)vdata;

    while (length > 1) {
        sum += *ptr++;
        length -= 2;
    }

    if (length > 0) {
        sum += *(uint8_t*)ptr;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)(~sum);
}

void send_arp_reply(uint8_t* dest_mac, uint32_t dest_ip) {
    uint8_t arp_payload[28];
    arp_header_t* arp = (arp_header_t*)arp_payload;

    arp->htype = HTONS(1);
    arp->ptype = HTONS(0x0800);
    arp->hlen = 6;
    arp->plen = 4;
    arp->oper = HTONS(2); // REPLY (Ответ)

    memcpy(arp->sha, mac_addr, 6); // Наш MAC
    arp->spa = HTONL(0x0A00020F);  // Наш IP (10.0.2.15)

    memcpy(arp->tha, dest_mac, 6); // MAC того, кто спрашивал
    arp->tpa = HTONL(dest_ip);     // IP того, кто спрашивал

    send_ethernet_frame(dest_mac, 0x0806, arp_payload, 28);
}
// Функция-обертка для VFS