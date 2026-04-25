#ifndef _STRINGS_H
#define _STRINGS_H

#include <string.h> // Подтягиваем обычный string.h для базовых функций
#include <stddef.h>

// Doom обязательно потребует эти две функции:
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);

#endif