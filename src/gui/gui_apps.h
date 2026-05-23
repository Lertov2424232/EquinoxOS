#ifndef GUI_APPS_H
#define GUI_APPS_H

#include "gui/gui.h"
#include <stdint.h>

// Notepad limits
#define NOTEPAD_MAX_LINES 16
#define NOTEPAD_LINE_LEN 48

// ВАЖНО: используем правильное имя структуры
struct vfs_node;

typedef struct {
  char name[128];
  uint32_t size;
  struct vfs_node *dev; // Исправлено здесь
  uint32_t inode;
} explorer_file_t;

void draw_cursor(int x, int y);
void update_gui();
void notepad_load_content(const char *data, uint32_t size);
void notepad_handle_char(char c);

#endif