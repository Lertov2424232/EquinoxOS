#include "stdio.h"
#include "string.h"

// Нам нужно знать про функцию терминала (она в kernel.c)
extern void term_print(const char *str);

int vsprintf(char *buffer, const char *format, va_list args) {
  char *ptr = buffer;
  const char *f = format;
  char temp_buf[64]; // Буфер для чисел

  while (*f) {
    if (*f != '%') {
      *ptr++ = *f++;
      continue;
    }

    f++; // Пропускаем '%'

    switch (*f) {
    case 'c': { // Символ
      char c = (char)va_arg(args, int);
      *ptr++ = c;
      break;
    }
    case 's': { // Строка
      char *s = va_arg(args, char *);
      while (*s)
        *ptr++ = *s++;
      break;
    }
    case 'd': { // Десятичное число (int)
      int val = va_arg(args, int);
      itoa(val, 10, temp_buf);
      char *t = temp_buf;
      while (*t)
        *ptr++ = *t++;
      break;
    }
    case 'x': { // Hex число (long long)
      unsigned long long val = va_arg(args, unsigned long long);
      itoa_hex(val, temp_buf); // Используем твою функцию
      // itoa_hex не добавляет "0x", добавим руками для красоты?
      // *ptr++ = '0'; *ptr++ = 'x';
      char *t = temp_buf;
      while (*t)
        *ptr++ = *t++;
      break;
    }
    case '%': { // Просто процент
      *ptr++ = '%';
      break;
    }
    default: { // Неизвестный формат - просто печатаем как есть
      *ptr++ = '%';
      *ptr++ = *f;
      break;
    }
    }
    f++;
  }

  *ptr = '\0';         // Завершаем строку нулем
  return ptr - buffer; // Возвращаем длину
}

int sprintf(char *buffer, const char *format, ...) {
  va_list args;
  va_start(args, format);
  int len = vsprintf(buffer, format, args);
  va_end(args);
  return len;
}

// ГЛАВНЫЙ БОСС
void printf(const char *format, ...) {
  char buffer[1024];
  va_list args;
  va_start(args, format);
  vsprintf(buffer, format, args);
  va_end(args);

  // Вместо term_print(buffer) мы могли бы делать:
  // vfs_node_t* tty = vfs_find("tty0");
  // vfs_write(tty, 0, strlen(buffer), (uint8_t*)buffer);

  // Но пока для простоты оставим мост, но назовем его правильно:
  term_print(buffer);
}