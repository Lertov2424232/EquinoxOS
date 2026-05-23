#include "dns.h"
#include "udp.h"
#include "../libc/string.h"
#include "../libc/stdio.h"
#include "../system/memory.h"

extern void term_print(const char* str);

uint32_t resolved_ip = 0;
char last_queried_name[128];

// Converts "google.com" to "\x06google\x03com\x00"
static void dns_format_name(uint8_t* qname, const char* host) {
    int lock = 0;
    char temp[256];
    
    if (strlen(host) > 250) return;
    
    strcpy(temp, host);
    strcat(temp, ".");
    
    for (int i = 0; i < (int)strlen(temp); i++) {
        if (temp[i] == '.') {
            *qname++ = i - lock;
            for (; lock < i; lock++) {
                *qname++ = temp[lock];
            }
            lock++;
        }
    }
    *qname++ = '\0';
}

static void dns_callback(uint32_t src_ip, uint16_t src_port, uint8_t* data, uint32_t len) {
    (void)src_ip; (void)src_port; (void)len;
    dns_header_t* dns = (dns_header_t*)data;
    
    if (HTONS(dns->answers) > 0) {
        // Skip Header
        uint8_t* ptr = data + sizeof(dns_header_t);
        // Skip Question
        while (*ptr != 0) ptr++;
        ptr += 5; // Skip final 0, Type(2), Class(2)
        
        // --- Answer Section ---
        // Answers usually start with a Name Pointer (0xC0XX)
        if ((*ptr & 0xC0) == 0xC0) ptr += 2; else while (*ptr != 0) ptr++;
        
        uint16_t type = (ptr[0] << 8) | ptr[1];
        uint16_t rdlen = (ptr[8] << 8) | ptr[9];
        ptr += 10;
        
        if (type == 1 && rdlen == 4) { // A Record (IPv4)
            resolved_ip = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
            char buf[128];
            sprintf(buf, "[DNS] Resolved %s to %d.%d.%d.%d\n", last_queried_name,
                    (resolved_ip >> 24) & 0xFF, (resolved_ip >> 16) & 0xFF,
                    (resolved_ip >> 8) & 0xFF, resolved_ip & 0xFF);
            term_print(buf);
        }
    } else {
        term_print("[DNS] Error: No answers in response.\n");
    }
}

void dns_query(net_interface_t* iface, const char* hostname) {
    if (!iface) return;
    
    static bool bound = false;
    if (!bound) {
        udp_bind(53535, dns_callback); // Ephemeral port for DNS responses
        bound = true;
    }
    
    resolved_ip = 0;
    strcpy(last_queried_name, hostname);
    
    uint8_t buffer[512];
    memset(buffer, 0, 512);
    
    dns_header_t* dns = (dns_header_t*)buffer;
    dns->id = HTONS(0xBEEF);
    dns->flags = HTONS(0x0100); // Standard Query, Recursion Desired
    dns->questions = HTONS(1);
    
    uint8_t* qname = buffer + sizeof(dns_header_t);
    dns_format_name(qname, hostname);
    
    int qname_len = strlen((char*)qname) + 1;
    uint8_t* qinfo = qname + qname_len;
    qinfo[1] = 1; // Type A
    qinfo[3] = 1; // Class IN
    
    uint32_t total_len = sizeof(dns_header_t) + qname_len + 4;
    
    // Send to Google DNS (8.8.8.8)
    udp_send_packet(iface, 0x08080808, 53535, 53, buffer, total_len);
    
    char buf[128];
    sprintf(buf, "[DNS] Querying 8.8.8.8 for %s...\n", hostname);
    term_print(buf);
}

uint32_t dns_get_result(const char* hostname) {
    if (strcmp(last_queried_name, hostname) == 0) return resolved_ip;
    return 0;
}
