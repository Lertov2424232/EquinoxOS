#include "../sdk/lua/lauxlib.h"
#include "../sdk/lua/lua.h"
#include "../sdk/lua/lualib.h"
#include <eid.h>
#include <equos.h>
#include <stdio.h>
#include <stdlib.h>

static eid_font_t *current_lua_font = NULL;

static eid_ctx_t ctx;
static uint32_t *fb;
#define W 640
#define H 480

int win_x = 100;
int win_y = 100;

// --- Функции, которые мы отдаем в Lua ---

// draw_rect(x, y, w, h, color)
static int l_draw_rect(lua_State *L) {
  int x = (int)luaL_checknumber(L, 1);
  int y = (int)luaL_checknumber(L, 2);
  int w = (int)luaL_checknumber(L, 3);
  int h = (int)luaL_checknumber(L, 4);
  uint32_t color = (uint32_t)luaL_checknumber(L, 5);
  eid_draw_rect(fb, W, H, x, y, w, h, color);
  return 0;
}

// button(label, x, y, w, h) -> returns true/false
static int l_button(lua_State *L) {
  const char *label = luaL_checkstring(L, 1);
  int x = (int)luaL_checknumber(L, 2);
  int y = (int)luaL_checknumber(L, 3);
  int w = (int)luaL_checknumber(L, 4);
  int h = (int)luaL_checknumber(L, 5);

  uint32_t id = eid_get_id(label, x, y);
  uint32_t state = eid_process_interaction(&ctx, id, x, y, w, h);

  // Отрисовка кнопки (упрощенно)
  uint32_t color = (state & EID_STATE_HOVER) ? 0x555555 : 0x333333;
  eid_draw_rect(fb, W, H, x, y, w, h, color);
  eid_draw_text(fb, W, H, x + 5, y + 5, label, 0xFFFFFF);

  lua_pushboolean(L, (state & EID_STATE_CLICKED));
  return 1;
}

static int l_load_font(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  float size = luaL_checknumber(L, 2);

  FILE *f = fopen(path, "r");
  if (f) {
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *data = malloc(fsize);
    fread(data, 1, fsize, f);
    fclose(f);
    current_lua_font = eid_load_font(data, size);
    lua_pushboolean(L, true);
  } else {
    lua_pushboolean(L, false);
  }
  return 1;
}

static int l_draw_text(lua_State *L) {
  const char *str = luaL_checkstring(L, 1);
  int x = (int)luaL_checknumber(L, 2);
  int y = (int)luaL_checknumber(L, 3);
  uint32_t color = (uint32_t)luaL_checknumber(L, 4);

  if (current_lua_font) {
    eid_draw_text_ttf(&ctx, current_lua_font, x, y, str, color);
  } else {
    eid_draw_text(fb, W, H, x, y, str, color);
  }
  return 0;
}

static int l_draw_line(lua_State *L) {
  int x1 = (int)luaL_checknumber(L, 1);
  int y1 = (int)luaL_checknumber(L, 2);
  int x2 = (int)luaL_checknumber(L, 3);
  int y2 = (int)luaL_checknumber(L, 4);
  uint32_t color = (uint32_t)luaL_checknumber(L, 5);
  eid_draw_line(fb, W, H, x1, y1, x2, y2, color);
  return 0;
}

static int l_draw_gradient(lua_State *L) {
  int x = (int)luaL_checknumber(L, 1);
  int y = (int)luaL_checknumber(L, 2);
  int w = (int)luaL_checknumber(L, 3);
  int h = (int)luaL_checknumber(L, 4);
  uint32_t c1 = (uint32_t)luaL_checknumber(L, 5);
  uint32_t c2 = (uint32_t)luaL_checknumber(L, 6);
  bool vertical = lua_toboolean(L, 7);
  eid_draw_gradient_rect(fb, W, H, x, y, w, h, c1, c2, vertical);
  return 0;
}

static int l_get_mouse(lua_State *L) {
  lua_pushinteger(L, ctx.mx);
  lua_pushinteger(L, ctx.my);
  lua_pushboolean(L, ctx.m_down);
  return 3;
}

static int l_get_time(lua_State *L) {
  uint64_t t = _syscall(6, 0, 0, 0, 0, 0); // SYS_GET_TIME
  lua_pushnumber(L, (double)t);
  return 1;
}

static int l_sys_exec(lua_State *L) {
  const char *cmd = luaL_checkstring(L, 1);
  int ret = sys_exec(cmd);
  lua_pushboolean(L, ret == 1);
  return 1;
}

static int l_get_used_mem(lua_State *L) {
  lua_pushnumber(L, (double)sys_get_used_mem());
  return 1;
}

static int l_get_total_mem(lua_State *L) {
  lua_pushnumber(L, (double)sys_get_total_mem());
  return 1;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: luagui.elf script.lua\n");
    return 1;
  }

  eid_init();
  fb = malloc(W * H * 4);
  lua_State *L = luaL_newstate();
  luaL_openlibs(L);

  lua_register(L, "draw_rect", l_draw_rect);
  lua_register(L, "button", l_button);
  lua_register(L, "load_font", l_load_font);
  lua_register(L, "draw_text", l_draw_text);
  lua_register(L, "draw_line", l_draw_line);
  lua_register(L, "draw_gradient", l_draw_gradient);
  lua_register(L, "get_mouse", l_get_mouse);
  lua_register(L, "get_time", l_get_time);
  lua_register(L, "sys_exec", l_sys_exec);
  lua_register(L, "get_used_mem", l_get_used_mem);
  lua_register(L, "get_total_mem", l_get_total_mem);
  // 1. ЗАГРУЖАЕМ И ВЫПОЛНЯЕМ ФАЙЛ ТОЛЬКО ОДИН РАЗ
  if (luaL_dofile(L, argv[1])) {
    printf("Lua Error: %s\n", lua_tostring(L, -1));
    return 1;
  }

  while (1) {
    eid_begin(&ctx, fb, W, H);

    // Спрашиваем у ядра, где сейчас наше окно (сисколл 33)
    uint64_t rx = 0, ry = 0;
    __asm__ volatile("mov $33, %%rax\n int $0x80" : "=a"(rx), "=b"(ry));
    win_x = (int)rx;
    win_y = (int)ry;

    // Синхронизируем мышь: вычитаем АКТУАЛЬНУЮ позицию окна из ядра
    ctx.mx -= win_x;
    ctx.my -= win_y;

    eid_draw_rect(fb, W, H, 0, 0, W, H, 0x1a1a1a); // Фон

    // 2. ИЩЕМ ФУНКЦИЮ on_update И ВЫЗЫВАЕМ ЕЁ КАЖДЫЙ КАДР
    lua_getglobal(L, "on_update");
    if (lua_isfunction(L, -1)) {
      if (lua_pcall(L, 0, 0, 0) != LUA_OK) { // 0 аргументов, 0 возвратов
        printf("Lua Update Error: %s\n", lua_tostring(L, -1));
        break;
      }
    } else {
      lua_pop(L, 1); // Снимаем со стека, если функции нет
    }

    // Отрисовка
    bool open = true;
    eid_end(&ctx, win_x, win_y);

    // Force Lua garbage collection step to prevent memory leaks from temp variables
    lua_gc(L, LUA_GCSTEP, 0);

    if (ctx.last_key == 0x01)
      break; // ESC
    sys_yield();
  }

  lua_close(L);
  return 0;
}