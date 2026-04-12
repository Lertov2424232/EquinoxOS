#include <equos.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>


int vsprintf(char *buffer, const char *format, va_list args) {
  char *ptr = buffer;
  const char *f = format;
  char temp_buf[64];

  while (*f) {
    if (*f != '%') {
      *ptr++ = *f++;
      continue;
    }
    f++;
    switch (*f) {
    case 'c': {
      *ptr++ = (char)va_arg(args, int);
      break;
    }
    case 's': {
      char *s = va_arg(args, char *);
      if (!s)
        s = "(null)";
      while (*s)
        *ptr++ = *s++;
      break;
    }
    case 'd': {
      int val = va_arg(args, int);
      itoa(val, 10, temp_buf);
      char *t = temp_buf;
      while (*t)
        *ptr++ = *t++;
      break;
    }
    case 'x': {
      unsigned long long val = va_arg(args, unsigned long long);
      itoa_hex(val, temp_buf);
      char *t = temp_buf;
      while (*t)
        *ptr++ = *t++;
      break;
    }
    case '%':
      *ptr++ = '%';
      break;
    }
    f++;
  }
  *ptr = '\0';
  return ptr - buffer;
}

void printf(const char *format, ...) {
  char buffer[1024];
  va_list args;
  va_start(args, format);
  vsprintf(buffer, format, args);
  va_end(args);

  // ВОТ ОН - МОМЕНТ ИСТИНЫ
  // Мы вызываем прерывание 0x80 (SYS_PRINT = 1)
  syscall(SYS_PRINT, (uint64_t)buffer, 0, 0, 0, 0);
}