#include <eid.h>
#include <equos.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WIN_W 640
#define WIN_H 420
#define CONTENT_X 18
#define CONTENT_Y 56
#define CONTENT_W (WIN_W - 36)
#define LINE_H 18
#define MAX_LINES 256
#define LINE_CHARS 74

#define CLR_BG 0xF6F7F9
#define CLR_CHROME 0x20242C
#define CLR_CHROME_2 0x2E3542
#define CLR_TEXT 0x1B1F24
#define CLR_MUTED 0x657080
#define CLR_H1 0x0A5C7A
#define CLR_H2 0x22624A
#define CLR_LINK 0x135CC8
#define CLR_CODE_BG 0xE9EEF3
#define CLR_CODE 0x7A2E12
#define CLR_BORDER 0xC9D1D9

typedef enum {
  STYLE_NORMAL,
  STYLE_H1,
  STYLE_H2,
  STYLE_LINK,
  STYLE_CODE,
  STYLE_MUTED,
  STYLE_BULLET
} line_style_t;

typedef struct {
  char text[LINE_CHARS + 1];
  line_style_t style;
  bool indent;
} line_t;

static uint32_t fb[WIN_W * WIN_H];
static eid_ctx_t ui;
static line_t lines[MAX_LINES];
static int line_count = 0;
static int scroll_line = 0;
static char page_title[64] = "index.html";

static void print(const char *s) {
  _syscall(SYS_PRINT, (uint64_t)s, 0, 0, 0, 0);
}

static bool ascii_isspace(char c) {
  return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

static char ascii_lower(char c) {
  if (c >= 'A' && c <= 'Z')
    return c + ('a' - 'A');
  return c;
}

static bool tag_eq(const char *tag, const char *name) {
  int i = 0;
  while (name[i] != '\0') {
    if (ascii_lower(tag[i]) != name[i])
      return false;
    i++;
  }
  return tag[i] == '\0' || tag[i] == ' ' || tag[i] == '/' || tag[i] == '>';
}

static int visible_lines(void) { return (WIN_H - CONTENT_Y - 20) / LINE_H; }

static void push_line(const char *text, int len, line_style_t style, bool indent) {
  if (line_count >= MAX_LINES)
    return;

  if (len < 0)
    len = 0;
  if (len > LINE_CHARS)
    len = LINE_CHARS;

  for (int i = 0; i < len; i++)
    lines[line_count].text[i] = text[i];
  lines[line_count].text[len] = '\0';
  lines[line_count].style = style;
  lines[line_count].indent = indent;
  line_count++;
}

static void blank_line(void) {
  if (line_count == 0)
    return;
  if (line_count > 0 && lines[line_count - 1].text[0] == '\0')
    return;
  push_line("", 0, STYLE_NORMAL, false);
}

static void append_word(char *line, int *len, const char *word, int word_len,
                        line_style_t style, bool indent) {
  int max_chars = indent ? (LINE_CHARS - 4) : LINE_CHARS;
  if (word_len <= 0)
    return;

  if (*len > 0 && *len + 1 + word_len > max_chars) {
    push_line(line, *len, style, indent);
    *len = 0;
  }

  if (*len > 0)
    line[(*len)++] = ' ';

  while (word_len > max_chars) {
    int room = max_chars - *len;
    if (room <= 0) {
      push_line(line, *len, style, indent);
      *len = 0;
      room = max_chars;
    }
    for (int i = 0; i < room; i++)
      line[(*len)++] = *word++;
    word_len -= room;
    push_line(line, *len, style, indent);
    *len = 0;
  }

  for (int i = 0; i < word_len && *len < max_chars; i++)
    line[(*len)++] = word[i];
}

static void flush_current(char *line, int *len, line_style_t style, bool indent) {
  if (*len > 0) {
    push_line(line, *len, style, indent);
    *len = 0;
  }
}

static void read_tag(const char *html, uint32_t size, uint32_t *pos, char *tag,
                     int tag_size) {
  int len = 0;
  (*pos)++;

  while (*pos < size && html[*pos] != '>' && len < tag_size - 1) {
    tag[len++] = ascii_lower(html[*pos]);
    (*pos)++;
  }
  while (*pos < size && html[*pos] != '>')
    (*pos)++;
  if (*pos < size && html[*pos] == '>')
    (*pos)++;

  tag[len] = '\0';
}

static void copy_title_from_html(const char *html, uint32_t size) {
  for (uint32_t i = 0; i + 7 < size; i++) {
    if (html[i] == '<' && tag_eq(html + i + 1, "title")) {
      uint32_t start = i + 1;
      while (start < size && html[start] != '>')
        start++;
      if (start >= size)
        return;
      start++;

      uint32_t end = start;
      while (end + 8 < size) {
        if (html[end] == '<' && html[end + 1] == '/' &&
            tag_eq(html + end + 2, "title"))
          break;
        end++;
      }

      int out = 0;
      for (uint32_t j = start; j < end && out < 63; j++) {
        if (!ascii_isspace(html[j]) || (out > 0 && page_title[out - 1] != ' '))
          page_title[out++] = ascii_isspace(html[j]) ? ' ' : html[j];
      }
      page_title[out] = '\0';
      return;
    }
  }
}

static void parse_html(const char *html, uint32_t size) {
  line_count = 0;
  scroll_line = 0;
  line_style_t style = STYLE_NORMAL;
  bool in_body = false;
  bool in_pre = false;
  bool in_list = false;
  bool skipping_head = false;
  bool at_li_start = false;

  char current[LINE_CHARS + 1];
  char word[LINE_CHARS + 1];
  int current_len = 0;
  int word_len = 0;

  copy_title_from_html(html, size);

  for (uint32_t i = 0; i < size;) {
    char c = html[i];

    if (c == '<') {
      char tag[64];
      read_tag(html, size, &i, tag, sizeof(tag));

      if (word_len > 0) {
        append_word(current, &current_len, word, word_len, style, in_list);
        word_len = 0;
      }

      if (tag_eq(tag, "head")) {
        skipping_head = true;
      } else if (tag_eq(tag, "/head")) {
        skipping_head = false;
      } else if (tag_eq(tag, "body")) {
        in_body = true;
      } else if (tag_eq(tag, "/body")) {
        in_body = false;
      } else if (!skipping_head) {
        if (tag_eq(tag, "h1")) {
          flush_current(current, &current_len, style, in_list);
          blank_line();
          style = STYLE_H1;
        } else if (tag_eq(tag, "/h1")) {
          flush_current(current, &current_len, style, false);
          style = STYLE_NORMAL;
          blank_line();
        } else if (tag_eq(tag, "h2") || tag_eq(tag, "h3")) {
          flush_current(current, &current_len, style, in_list);
          blank_line();
          style = STYLE_H2;
        } else if (tag_eq(tag, "/h2") || tag_eq(tag, "/h3")) {
          flush_current(current, &current_len, style, false);
          style = STYLE_NORMAL;
          blank_line();
        } else if (tag_eq(tag, "p") || tag_eq(tag, "div") || tag_eq(tag, "section")) {
          flush_current(current, &current_len, style, in_list);
          blank_line();
        } else if (tag_eq(tag, "/p") || tag_eq(tag, "/div") || tag_eq(tag, "/section")) {
          flush_current(current, &current_len, style, in_list);
          blank_line();
        } else if (tag_eq(tag, "br")) {
          flush_current(current, &current_len, style, in_list);
        } else if (tag_eq(tag, "ul") || tag_eq(tag, "ol")) {
          flush_current(current, &current_len, style, in_list);
          in_list = true;
        } else if (tag_eq(tag, "/ul") || tag_eq(tag, "/ol")) {
          flush_current(current, &current_len, style, in_list);
          in_list = false;
          blank_line();
        } else if (tag_eq(tag, "li")) {
          flush_current(current, &current_len, style, in_list);
          style = STYLE_BULLET;
          at_li_start = true;
        } else if (tag_eq(tag, "/li")) {
          flush_current(current, &current_len, style, true);
          style = STYLE_NORMAL;
          at_li_start = false;
        } else if (tag_eq(tag, "a")) {
          style = STYLE_LINK;
        } else if (tag_eq(tag, "/a")) {
          style = in_list ? STYLE_BULLET : STYLE_NORMAL;
        } else if (tag_eq(tag, "code") || tag_eq(tag, "pre")) {
          style = STYLE_CODE;
          in_pre = true;
        } else if (tag_eq(tag, "/code") || tag_eq(tag, "/pre")) {
          flush_current(current, &current_len, style, in_list);
          style = in_list ? STYLE_BULLET : STYLE_NORMAL;
          in_pre = false;
        }
      }
      continue;
    }

    if (!in_body && !skipping_head && line_count == 0) {
      in_body = true;
    }

    if (skipping_head) {
      i++;
      continue;
    }

    if (at_li_start) {
      append_word(current, &current_len, "*", 1, STYLE_BULLET, true);
      at_li_start = false;
    }

    if (in_pre && (c == '\n' || c == '\r')) {
      if (word_len > 0) {
        append_word(current, &current_len, word, word_len, style, in_list);
        word_len = 0;
      }
      flush_current(current, &current_len, style, in_list);
      i++;
      continue;
    }

    if (ascii_isspace(c)) {
      if (word_len > 0) {
        append_word(current, &current_len, word, word_len, style, in_list);
        word_len = 0;
      }
    } else if (word_len < LINE_CHARS) {
      word[word_len++] = c;
    }

    i++;
  }

  if (word_len > 0)
    append_word(current, &current_len, word, word_len, style, in_list);
  flush_current(current, &current_len, style, in_list);

  if (line_count == 0)
    push_line("(empty HTML document)", 21, STYLE_MUTED, false);
}

static uint32_t color_for_style(line_style_t style) {
  switch (style) {
  case STYLE_H1:
    return CLR_H1;
  case STYLE_H2:
    return CLR_H2;
  case STYLE_LINK:
    return CLR_LINK;
  case STYLE_CODE:
    return CLR_CODE;
  case STYLE_MUTED:
    return CLR_MUTED;
  case STYLE_BULLET:
    return CLR_TEXT;
  default:
    return CLR_TEXT;
  }
}

static void draw_text_line(int x, int y, const char *text, line_style_t style) {
  if (style == STYLE_CODE) {
    int w = strlen(text) * 8 + 8;
    if (w < 24)
      w = 24;
    if (w > CONTENT_W)
      w = CONTENT_W;
    // ИЗМЕНЕНИЕ: Добавлен WIN_H
    eid_draw_rect(fb, WIN_W, WIN_H, x - 4, y - 2, w, LINE_H, CLR_CODE_BG);
  }

  // ИЗМЕНЕНИЕ: Добавлен WIN_H
  eid_draw_text(fb, WIN_W, WIN_H, x, y, text, color_for_style(style));

  if (style == STYLE_H1)
    // ИЗМЕНЕНИЕ: Добавлен WIN_H
    eid_draw_line(fb, WIN_W, WIN_H, x, y + 17, x + 220, y + 17, 0x7EB9CC);

  if (style == STYLE_LINK)
    // ИЗМЕНЕНИЕ: Добавлен WIN_H
    eid_draw_line(fb, WIN_W, WIN_H, x, y + 15, x + strlen(text) * 8, y + 15,
                  CLR_LINK);
}

static void render(const char *filename) {
  // 1. Очистка фона и отрисовка "шапки" (добавлен WIN_H)
  eid_draw_rect(fb, WIN_W, WIN_H, 0, 0, WIN_W, WIN_H, CLR_BG);
  eid_draw_rect(fb, WIN_W, WIN_H, 0, 0, WIN_W, 42, CLR_CHROME);
  eid_draw_rect(fb, WIN_W, WIN_H, 0, 42, WIN_W, 1, CLR_BORDER);

  eid_draw_text(fb, WIN_W, WIN_H, 14, 12, "Equinox HTML Viewer", 0xFFFFFF);
  eid_draw_rect(fb, WIN_W, WIN_H, 210, 9, WIN_W - 230, 24, CLR_CHROME_2);
  eid_draw_text(fb, WIN_W, WIN_H, 220, 15, filename, 0xDCE6EF);

  eid_draw_text(fb, WIN_W, WIN_H, CONTENT_X, 48, page_title, CLR_MUTED);

  // 2. Расчет скролла (объявляем переменную здесь, чтобы она была видна всей
  // функции)
  int v_lines = visible_lines();
  int max_scroll = line_count - v_lines;
  if (max_scroll < 0)
    max_scroll = 0;
  if (scroll_line > max_scroll)
    scroll_line = max_scroll;

  // 3. Цикл отрисовки текста
  int cur_y = CONTENT_Y;
  for (int i = 0; i < v_lines; i++) {
    int idx = scroll_line + i; // Объявляем idx внутри цикла
    if (idx >= line_count)
      break;

    // Смещение для списков (indent)
    int cur_x = CONTENT_X + (lines[idx].indent ? 18 : 0);

    // Вызов отрисовки строки (убедись, что draw_text_line принимает нужные
    // аргументы)
    draw_text_line(cur_x, cur_y, lines[idx].text, lines[idx].style);
    cur_y += LINE_H;
  }

  // 4. Отрисовка скроллбара (используем max_scroll, который объявили выше)
  eid_draw_rect(fb, WIN_W, WIN_H, WIN_W - 12, CONTENT_Y, 4,
                WIN_H - CONTENT_Y - 18, 0xD5DCE4);

  if (line_count > v_lines) {
    int track_h = WIN_H - CONTENT_Y - 18;
    int knob_h = (v_lines * track_h) / line_count;
    if (knob_h < 16)
      knob_h = 16;

    int denom = (max_scroll > 0) ? max_scroll : 1;
    int knob_y = CONTENT_Y + (scroll_line * (track_h - knob_h)) / denom;

    eid_draw_rect(fb, WIN_W, WIN_H, WIN_W - 12, knob_y, 4, knob_h, CLR_LINK);
  }

  // 5. Футер
  eid_draw_text(fb, WIN_W, WIN_H, CONTENT_X, WIN_H - 16,
                "Up/Down scroll  Esc close", CLR_MUTED);
}

int main(int argc, char **argv) {
  eid_init();

  const char *filename = "index.html";
  if (argc > 1 && argv[1] != 0)
    filename = argv[1];

  print("[HTMLVIEW] Loading ");
  print(filename);
  print("\n");

  uint32_t size = 0;
  char *html = (char *)_syscall(SYS_READ_FILE, (uint64_t)filename,
                                (uint64_t)&size, 0, 0, 0);

  for (int i = 0; i < WIN_W * WIN_H; i++)
    fb[i] = CLR_BG;

  if (!html) {
    line_count = 0;
    push_line("Could not open HTML file.", 25, STYLE_H2, false);
    push_line(filename, strlen(filename), STYLE_MUTED, false);
    print("[HTMLVIEW] File not found\n");
  } else {
    parse_html(html, size);
    print("[HTMLVIEW] Parsed document\n");
  }

  while (1) {
    eid_begin(&ui, fb, WIN_W, WIN_H);
    ui.mx -= 120; // 120 - это WIN_X из вызова eid_end
    ui.my -= 90;

    uint8_t key = ui.last_key;
    int max_scroll = line_count - visible_lines();
    if (max_scroll < 0)
      max_scroll = 0;

    if (key == 0x01)
      break;
    if ((key == 0x50 || key == 0x1F) && scroll_line < max_scroll)
      scroll_line++;
    if ((key == 0x48 || key == 0x11) && scroll_line > 0)
      scroll_line--;
    if (key == 0x51) {
      scroll_line += visible_lines();
      if (scroll_line > max_scroll)
        scroll_line = max_scroll;
    }
    if (key == 0x49) {
      scroll_line -= visible_lines();
      if (scroll_line < 0)
        scroll_line = 0;
    }
    if (key == 0x47)
      scroll_line = 0;
    if (key == 0x4F)
      scroll_line = max_scroll;

    render(filename);
    eid_end(&ui, 120, 90);
    sleep(20);
  }

  _syscall(SYS_EXIT, 0, 0, 0, 0, 0);
  return 0;
}
