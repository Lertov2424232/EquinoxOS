#include "rtl8139.h"
#include "../../io/io.h"
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

// ХАК: Используем фиксированный адрес в первой мегабайте памяти, 
// который в 99.9% случаев свободен и доступен для DMA (Identity mapped)
uint8_t* tx_buffer = (uint8_t*)0x80000; // 512 KB mark
uint8_t* rx_buffer = (uint8_t*)0x90000; // 576 KB mark

void rtl8139_init(uint32_t bar0) {
    rtl_io_base = bar0 & ~3;

    // 1. Пробуждение (Power ON)
    outb(rtl_io_base + REG_CONFIG1, 0x00);

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
    
    // Копируем данные в наш "низкий" буфер
    memcpy(tx_buffer, data, len);
    if (len < 60) memset(tx_buffer + len, 0, 60 - len); // Padding

    // Память должна "осесть" перед тем как карта её заберет
    __asm__ volatile("" : : : "memory");

    // TSAD0 - физический адрес
    outl(rtl_io_base + REG_TSAD0, (uint32_t)(uintptr_t)tx_buffer); 

    // TSD0 - размер и команда "Взлёт!"
    // Бит 13 (OWN) сбрасывается в 0 при записи, карта поставит его в 1 когда закончит
    outl(rtl_io_base + REG_TSD0, send_len | 0x0000); // 0 threshold

    // Ждем подтверждения от карты (Polling)
    // В реальной ОС тут нужны прерывания, но для теста - цикл
    int timeout = 100000;
    while (!(inl(rtl_io_base + REG_TSD0) & (1 << 13)) && timeout--) {
        __asm__("pause");
    }

    if (timeout <= 0) term_print("[NET] TX Timeout!");
    else if (!(inl(rtl_io_base + REG_TSD0) & (1 << 15))) term_print("[NET] TX Error!");
}

void send_ethernet_frame(uint8_t* dest_mac, uint16_t ethertype, uint8_t* payload, uint32_t payload_len) {
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