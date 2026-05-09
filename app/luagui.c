#include "../sdk/lua/lauxlib.h"
#include "../sdk/lua/lua.h"
#include "../sdk/lua/lualib.h"
#include <eid.h>
#include <equos.h>
#include <stdio.h>
#include <stdlib.h>


static eid_ctx_t ctx;
static uint32_t *fb;
#define W 640
#define H 480

// --- Функции, которые мы отдаем в Lua ---

// draw_rect(x, y, w, h, color)
static int l_draw_rect(lua_State *L) {
  int x = luaL_checkinteger(L, 1);
  int y = luaL_checkinteger(L, 2);
  int w = luaL_checkinteger(L, 3);
  int h = luaL_checkinteger(L, 4);
  uint32_t color = (uint32_t)luaL_checkinteger(L, 5);
  eid_draw_rect(fb, W, H, x, y, w, h, color);
  return 0;
}

// button(label, x, y, w, h) -> returns true/false
static int l_button(lua_State *L) {
  const char *label = luaL_checkstring(L, 1);
  int x = luaL_checkinteger(L, 2);
  int y = luaL_checkinteger(L, 3);
  int w = luaL_checkinteger(L, 4);
  int h = luaL_checkinteger(L, 5);

  uint32_t id = eid_get_id(label, x, y);
  uint32_t state = eid_process_interaction(&ctx, id, x, y, w, h);

  // Отрисовка кнопки (упрощенно)
  uint32_t color = (state & EID_STATE_HOVER) ? 0x555555 : 0x333333;
  eid_draw_rect(fb, W, H, x, y, w, h, color);
  eid_draw_text(fb, W, H, x + 5, y + 5, label, 0xFFFFFF);

  lua_pushboolean(L, (state & EID_STATE_CLICKED));
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

  // Регистрируем команды EID в Lua
  lua_register(L, "draw_rect", l_draw_rect);
  lua_register(L, "button", l_button);

  // 1. ЗАГРУЖАЕМ И ВЫПОЛНЯЕМ ФАЙЛ ТОЛЬКО ОДИН РАЗ
  if (luaL_dofile(L, argv[1])) {
    printf("Lua Error: %s\n", lua_tostring(L, -1));
    return 1;
  }

  while (1) {
    eid_begin(&ctx, fb, W, H);
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
    // Мы можем добавить рамку окна вокруг нашего Lua-интерфейса!
    // Либо просто eid_end(&ctx, 100, 100);
    eid_end(&ctx, 100, 100);

    if (ctx.last_key == 0x01)
      break; // ESC
    sys_yield();
  }

  lua_close(L);
  return 0;
}