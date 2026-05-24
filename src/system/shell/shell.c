#include "shell.h"
#include "../drivers/devices/pcspeaker/pcspeaker.h"
#include "../drivers/vesa/bmp.h"
#include "../fs/vfs.h"
#include "../../gui/gui.h"
#include "../../gui/terminal.h"
#include "../../syslibc/stdio.h"
#include "../../syslibc/string.h"
#include "../mem/memory.h"
#include "../usr/task.h"
#include "../mem/pmm.h"
#include "../drivers/hardware/net/arp.h"
#include "../drivers/hardware/net/dns.h"
#include "../drivers/hardware/net/icmp.h"
#include "../drivers/hardware/net/net.h"
#include "../drivers/hardware/net/tcp.h"
#include "../drivers/hardware/net/udp.h"
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
#define MAX_HISTORY 10
char history[MAX_HISTORY][64];
int history_count = 0;
int history_browse_idx = -1;

void print_logo() {
  term_print("\e[36m        eeeeeeee        \n");
  term_print("\e[36m      eee      eee      \n");
  term_print("\e[36m     eee      eee      \e[37mEquinox\e[0m OS\n");
  term_print("\e[36m    eeeeeeeeeee         \e[37mCore: \e[32mx86_64\n");
  term_print("\e[36m    eee                 \e[37mShell: \e[32meqsh 1.0\n");
  term_print("\e[36m     eee      eee      \n");
  term_print("\e[36m      eeeeeeeeee       \n");
  term_print("\e[0m\n");
}

void shell_prompt() { term_print("\e[32muser@equos\e[0m:\e[34m/\e[0m$ "); }

void shell_init() {
  terminal_clear();
  print_logo();
  shell_prompt();
}

void shell_execute(char *cmd) {
  if (cmd[0] == '\0')
    return;

  // Сохраняем в историю
  if (history_count < MAX_HISTORY) {
    strcpy(history[history_count++], cmd);
  } else {
    for (int i = 0; i < MAX_HISTORY - 1; i++)
      strcpy(history[i], history[i + 1]);
    strcpy(history[MAX_HISTORY - 1], cmd);
  }
  history_browse_idx = history_count;

  term_print("\n");

  // ЛОГИКА КОМАНД
  if (strcmp(cmd, "ls") == 0) {
    // Цветной вывод: папки синим, файлы белым
    vfs_node_t *dev = vfs_root->next;
    while (dev) {
      term_print("\e[34m[");
      term_print(dev->name);
      term_print("]\e[0m  ");
      dev = dev->next;
    }
    term_print("\n");
  } else if (strcmp(cmd, "fetch") == 0) {
    print_logo();
    char mem[64];
    sprintf(mem, "Memory: %d MB / %d MB\n", pmm_get_used_memory() / 1024 / 1024,
            pmm_get_total_memory() / 1024 / 1024);
    term_print(mem);
  } else if (strcmp(cmd, "clear") == 0) {
    terminal_clear();
  } else if (strcmp(cmd, "gui") == 0) {
    term_print("Starting Equinox GUI...\n");
    // Здесь будет вызов переключения в GUI
  } else if (memcmp(cmd, "run ", 4) == 0) {
    task_exec(cmd + 4);
  } else {
    term_print("\e[31mCommand not found: \e[0m");
    term_print(cmd);
    term_print("\n");
  }

  shell_prompt();
}

void shell_handle_char(char c) {
  if (c == '\n') {
    shell_execute(shell_buffer);
    memset(shell_buffer, 0, 64);
    shell_idx = 0;
  } else if (c == '\b') { // Backspace
    if (shell_idx > 0) {
      shell_idx--;
      shell_buffer[shell_idx] = '\0';
    }
  } else if (c == '\t') { // Tab Completion (Простейшее)
    if (shell_idx > 0) {
      // Если начали писать run, дописываем .elf
      if (strstr(shell_buffer, "run ") == shell_buffer) {
        strcat(shell_buffer, ".elf");
        shell_idx = strlen(shell_buffer);
      }
    }
  } else if (c == '\x11') { // UP Arrow (История)
    if (history_count > 0 && history_browse_idx > 0) {
      history_browse_idx--;
      strcpy(shell_buffer, history[history_browse_idx]);
      shell_idx = strlen(shell_buffer);
    }
  } else if (c == '\x12') { // DOWN Arrow
    if (history_browse_idx < history_count - 1) {
      history_browse_idx++;
      strcpy(shell_buffer, history[history_browse_idx]);
      shell_idx = strlen(shell_buffer);
    } else {
      history_browse_idx = history_count;
      memset(shell_buffer, 0, 64);
      shell_idx = 0;
    }
  } else if (shell_idx < 62 && c >= 32 && c <= 126) {
    shell_buffer[shell_idx++] = c;
    shell_buffer[shell_idx] = '\0';
  }
}