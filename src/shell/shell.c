#include "shell.h"
#include "../api.h"
#include "../drivers/pcspeaker/pcspeaker.h"
#include "../drivers/vga/bmp.h"
#include "../fs/fat32.h"
#include "../fs/vfs.h"
#include "../gui/gui.h"
#include "../gui/terminal.h"
#include "../libc/stdio.h"
#include "../libc/string.h"
#include "../system/memory.h"
#include "../system/task.h"
#include "arp.h"
#include "dns.h"
#include "icmp.h"
#include "net.h"
#include "tcp.h"
#include "udp.h"
#include <stdbool.h>
#include <stdint.h>

static uint32_t parse_ip(const char* s) {
    uint32_t res = 0;
    for (int i = 0; i < 4; i++) {
        uint32_t val = 0;
        while (*s >= '0' && *s <= '9') {
            val = val * 10 + (*s - '0');
            s++;
        }
        res = (res << 8) | val;
        if (*s == '.') s++;
    }
    return res;
}

extern void term_print(const char *str);
extern void init_fs();
extern void list_files();
extern void create_file(char *name, char *content);
extern void read_file(char *name);
extern void exec_module_elf();
extern bool should_run_app;
extern void show();

char shell_buffer[64] = {0};
int shell_idx = 0;

void shell_init() {
  memset(shell_buffer, 0, 64);
  shell_idx = 0;
}

void shell_handle_char(char c) {
  if (c == '\b') {
    if (shell_idx > 0) {
      shell_idx--;
      shell_buffer[shell_idx] = '\0';
    }
  } else if (c == '\n') {
    term_print(shell_buffer);
    term_print("\n");

    net_interface_t *iface = net_get_primary_interface();

    if (strcmp(shell_buffer, "panic") == 0) {
      __asm__ volatile("ud2");
    } else if (strcmp(shell_buffer, "ls") == 0) {
      vfs_ls();
    } else if (memcmp(shell_buffer, "ping ", 5) == 0) {
      if (iface) {
        uint32_t ip = parse_ip(shell_buffer + 5);
        icmp_send_echo_request(iface, ip);
      } else {
        term_print("No network interface!\n");
      }
    } else if (memcmp(shell_buffer, "dns ", 4) == 0) {
      if (iface) {
          dns_query(iface, shell_buffer + 4);
      } else {
          term_print("No network interface!\n");
      }
    } else if (strcmp(shell_buffer, "clear") == 0) {
      terminal_clear();
    } else if (memcmp(shell_buffer, "wget ", 5) == 0) {
      if (iface) {
        uint32_t ip = parse_ip(shell_buffer + 5);
        net_wget(iface, ip);
      } else {
        term_print("No network interface!\n");
      }
    } else if (strcmp(shell_buffer, "wget") == 0) {
      if (iface)
        net_wget(iface, 0x0A000202); // 10.0.2.2
      else
        term_print("No network interface!\n");
    } else if (strcmp(shell_buffer, "arp") == 0) {
      arp_print_cache();
    } else if (strcmp(shell_buffer, "nettest") == 0) {
      if (iface) {
        send_arp_request(iface, 0x0A000202);
        term_print("[NET] ARP Request sent!\n");
      } else
        term_print("No network interface!\n");
    } else if (strcmp(shell_buffer, "gettime") == 0) {
      if (iface)
        send_ntp_request(iface);
      else
        term_print("No network interface!\n");
    } else if (strcmp(shell_buffer, "netif") == 0) {
      if (iface) {
        char buf[128];
        sprintf(buf,
                "Interface: %s\nMAC: %02x:%02x:%02x:%02x:%02x:%02x\nIP: "
                "%d.%d.%d.%d\n",
                iface->name, iface->mac[0], iface->mac[1], iface->mac[2],
                iface->mac[3], iface->mac[4], iface->mac[5],
                (iface->ip >> 24) & 0xFF, (iface->ip >> 16) & 0xFF,
                (iface->ip >> 8) & 0xFF, iface->ip & 0xFF);
        term_print(buf);
      } else
        term_print("No network interface!\n");
    } else if (strcmp(shell_buffer, "malloc") == 0) {
      kmalloc(1024 * 1024);
    } else if (memcmp(shell_buffer, "run ", 4) == 0) {
      char *filename = shell_buffer + 4;
      int len = strlen(filename);
      while (len > 0 &&
             (filename[len - 1] == ' ' || filename[len - 1] == '\r' ||
              filename[len - 1] == '\n')) {
        filename[len - 1] = '\0';
        len--;
      }
      task_exec(filename);
    } else if (strcmp(shell_buffer, "beep") == 0) {
      pcspeaker_beep(1000, 1000);
    } else if (strcmp(shell_buffer, "speakeron") == 0) {
      pcspeaker_play(1000);
      term_print("Speaker ON (1000Hz). Type 'speakeroff' to stop.\n");
    } else if (strcmp(shell_buffer, "speakeroff") == 0) {
      pcspeaker_stop();
      term_print("Speaker OFF.\n");
    } else if (strcmp(shell_buffer, "melody") == 0) {
      term_print("Playing melody...\n");
      pcspeaker_test_melody();
      term_print("Done!\n");
    } else if (memcmp(shell_buffer, "tone ", 5) == 0) {
      char *freq_str = shell_buffer + 5;
      uint32_t freq = 0;
      uint32_t duration = 500;
      int i = 0;
      while (freq_str[i] >= '0' && freq_str[i] <= '9') {
        freq = freq * 10 + (freq_str[i] - '0');
        i++;
      }
      if (freq_str[i] == ' ') {
        i++;
        duration = 0;
        while (freq_str[i] >= '0' && freq_str[i] <= '9') {
          duration = duration * 10 + (freq_str[i] - '0');
          i++;
        }
      }
      if (freq > 0 && freq < 20000) {
        pcspeaker_beep(freq, duration);
        term_print("Played tone.\n");
      } else {
        term_print("Invalid frequency (1-19999 Hz)\n");
      }
    } else if (memcmp(shell_buffer, "show ", 5) == 0) {
      if (strlen(shell_buffer) <= 5) {
        term_print("Usage: show [filename]\n");
      } else {
        char *filename = shell_buffer + 5;
        uint32_t size = 0;
        uint8_t *data = vfs_read_file(filename, &size);
        if (data) {
          bmp_draw_to_window(term_win, data, 10, 50);
          kfree(data);
        } else {
          term_print("File not found!\n");
        }
      }
    } else if (shell_buffer[0] != '\0') {
      term_print("Unknown command: ");
      term_print(shell_buffer);
      term_print("\n");
    }

    memset(shell_buffer, 0, 64);
    shell_idx = 0;
  } else if (shell_idx < 62) {
    shell_buffer[shell_idx++] = c;
    shell_buffer[shell_idx] = '\0';
  }
}
