#include "rtl8139.h"
#include "net.h"
#include "../../system/memory.h"
#include "../../io/io.h"
#include "../../fs/vfs.h"
#include "../../libc/string.h"
#include "../../libc/stdio.h"
#include "../../api.h"
#include "../../system/vmm.h"

extern void term_print(const char* str);

// --- RTL8139 REGISTERS ---
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

#define TX_BUFFER_BASE  0x80000 
#define RX_BUFFER_BASE  0x90000 
#define RX_BUFFER_SIZE  8192

static uint32_t rtl_io_base = 0;
static int tx_cur_desc = 0;
static uint8_t* rx_buffer_phys = (uint8_t*)RX_BUFFER_BASE; 
static uint16_t rx_offset = 0;

static net_interface_t rtl_iface;

void rtl8139_init(uint32_t bar0) {
    rtl_io_base = bar0 & ~3;

    outb(rtl_io_base + REG_CONFIG1, 0x00);         
    outl(rtl_io_base + REG_RCR, 0x0000000F | (1 << 7)); 
    
    outb(rtl_io_base + REG_COMMAND, 0x10);         
    while((inb(rtl_io_base + REG_COMMAND) & 0x10) != 0);

    outl(rtl_io_base + REG_RBSTART, (uint32_t)(uintptr_t)rx_buffer_phys);
    outl(rtl_io_base + REG_RCR, 0x0F | (1 << 7));  

    outb(rtl_io_base + REG_COMMAND, 0x0C);         
    outl(rtl_io_base + REG_TCR, 0x03000000);       
    outw(rtl_io_base + REG_IMR, 0x0005); // Enable ROK and TOK bits

    memset(&rtl_iface, 0, sizeof(net_interface_t));
    // ... rest of init ...
    strcpy(rtl_iface.name, "eth0");
    for (int i = 0; i < 6; i++) {
        rtl_iface.mac[i] = inb(rtl_io_base + REG_MAC + i);
    }
    rtl_iface.ip = 0x0A00020F; // 10.0.2.15
    rtl_iface.subnet_mask = 0xFFFFFF00;
    rtl_iface.gateway_ip = 0x0A000202;
    rtl_iface.send = rtl8139_send_packet;

    net_register_interface(&rtl_iface);

    term_print("[NET] RTL8139 Hardware Ready (eth0).\n");
}

void rtl8139_send_packet(void* data, uint32_t len) {
    uint32_t send_len = (len < 60) ? 60 : len;
    
    // CPU access via VIRT
    uint8_t* current_tx_virt = (uint8_t*)VIRT(TX_BUFFER_BASE + (tx_cur_desc * 512));
    uint32_t current_tx_phys = TX_BUFFER_BASE + (tx_cur_desc * 512);

    memcpy(current_tx_virt, data, len);
    if (len < 60) memset(current_tx_virt + len, 0, 60 - len);

    __asm__ volatile("" : : : "memory");

    uint32_t tsad_reg = REG_TSAD0 + (tx_cur_desc * 4);
    uint32_t tsd_reg  = REG_TSD0  + (tx_cur_desc * 4);

    outl(rtl_io_base + tsad_reg, current_tx_phys); 
    outl(rtl_io_base + tsd_reg, send_len | 0x0000); 

    tx_cur_desc = (tx_cur_desc + 1) % 4; 
}

void rtl8139_receive() {
    uint16_t isr_status = inw(rtl_io_base + REG_ISR);
    if (!(isr_status & 0x01)) return; 
    
    outw(rtl_io_base + REG_ISR, 0x01); 

    while (!(inb(rtl_io_base + REG_COMMAND) & 0x01)) {
        uint8_t* rx_buffer_virt = (uint8_t*)VIRT(rx_buffer_phys);
        uint16_t* header = (uint16_t*)(rx_buffer_virt + rx_offset);
        uint16_t length = header[1];

        if (length < 20 || length > 1536) {
            rx_offset = 0;
            return;
        }

        uint8_t* packet = rx_buffer_virt + rx_offset + 4; 
        
        net_handle_packet(&rtl_iface, packet, length - 4);

        rx_offset = (rx_offset + length + 4 + 3) & ~3;
        outw(rtl_io_base + REG_CAPR, rx_offset - 16);

        if (rx_offset >= RX_BUFFER_SIZE) rx_offset = 0;
    }
}

uint32_t rtl8139_vfs_write(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)node; (void)offset; 
    rtl8139_send_packet(buffer, size);
    return size;
}

void rtl8139_install_vfs() {
    vfs_node_t* node = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    memset(node, 0, sizeof(vfs_node_t));
    
    strcpy(node->name, "net");
    node->write = rtl8139_vfs_write;
    node->flags = 2; 
    
    vfs_register_device(node);
}

bool rtl8139_has_data() {
    uint16_t status = inw(rtl_io_base + 0x3E); 
    return (status & 0x01); 
}