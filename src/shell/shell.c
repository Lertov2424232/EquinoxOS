#include "shell.h"
#include "../libc/string.h"
#include "../libc/stdio.h"
#include "../api.h" 
#include "../fs/fat32.h"
#include "../gui/gui.h"
#include "../system/task.h"
#include "../system/memory.h"
#include "../drivers/vga/bmp.h"
#include <stdbool.h>
#include <stdint.h>

extern void net_wget();
extern void term_print(const char* str);
extern char term_history[8][64];
extern void init_fs();
extern void list_files();
extern void create_file(char* name, char* content);
extern void read_file(char* name);
extern void exec_module_elf();
extern bool should_run_app;
extern void send_arp_request(uint32_t target_ip);
extern void send_ntp_request();
extern void show();

// Буфер ввода теперь живет здесь, а не в kernel.c!
char shell_buffer[64] = {0};
int shell_idx = 0;

void shell_init() {
    memset(shell_buffer, 0, 64);
    shell_idx = 0;
}

// Эту функцию будет вызывать клавиатура при нажатии
void shell_handle_char(char c) {
    if (c == '\b') {
        if (shell_idx > 0) {
            shell_idx--;
            shell_buffer[shell_idx] = '\0';
        }
    } 
    else if (c == '\n') {
        term_print(shell_buffer);
        term_print("\n");
        // Обрабатываем команды
        if (strcmp(shell_buffer, "panic") == 0) {
            __asm__ volatile("ud2");
        }
        else if (strcmp(shell_buffer, "ls") == 0) {
            fat32_list_files();
        }
        else if (strcmp(shell_buffer, "clear") == 0) {
            for(int i=0; i<8; i++) memset(term_history[i], 0, 64);
        }
        else if (strcmp(shell_buffer, "wget") == 0) {
            net_wget();
        }
        else if (strcmp(shell_buffer, "malloc") == 0) {
            kmalloc(1024 * 1024);
        }
        else if (memcmp(shell_buffer, "run ", 4) == 0) {
    char* filename = shell_buffer + 4;
    
    // Убираем пробелы и переносы строк в конце
    int len = strlen(filename);
    while(len > 0 && (filename[len-1] == ' ' || filename[len-1] == '\r' || filename[len-1] == '\n')) {
        filename[len-1] = '\0';
        len--;
    }
    
    term_print("Attempting to run: ");
    term_print(filename);
    term_print("\n");
    task_exec(filename);
}
        else if (strcmp(shell_buffer, "run") == 0) {
            term_print("Usage: run [filename.elf]\n");
        }
        else if (strcmp(shell_buffer, "nettest") == 0) {
            send_arp_request(0x0A000202); 
            term_print("[NET] ARP Request sent!\n");
        }
        else if (strcmp(shell_buffer, "gettime") == 0) {
            send_ntp_request();
        }
        else if (memcmp(shell_buffer, "show ", 5) == 0) {
            if (strlen(shell_buffer) <= 5) {
                term_print("Usage: show [filename]\n");
            } else {
                char* filename = shell_buffer + 5;
                uint32_t size = 0;
                uint8_t* data = fat32_read_file(filename, &size);
                if (data) {
                    bmp_draw_to_window(term_win, data, 10, 50); 
                    kfree(data);
                } else {
                    term_print("File not found!\n");
                }
            }
        }
        else if (shell_buffer[0] != '\0') {
            term_print("Unknown command: ");
            term_print(shell_buffer);
            term_print("\n");
        }

        memset(shell_buffer, 0, 64);
        shell_idx = 0;
    }
    else if (shell_idx < 62) {
        shell_buffer[shell_idx++] = c;
        shell_buffer[shell_idx] = '\0';
    }
}