#ifndef _INTTYPES_H
#define _INTTYPES_H

// Подтягиваем стандартные типы (int32_t, uint32_t и т.д.), которые уже есть в компиляторе
#include <stdint.h>

// Макросы форматирования для printf, которые может захотеть Doom.
// На 32/64-битных системах они обычно мапятся в обычные 'd', 'u', 'x'.
#define PRId8  "d"
#define PRIu8  "u"
#define PRIx8  "x"

#define PRId16 "d"
#define PRIu16 "u"
#define PRIx16 "x"

#define PRId32 "d"
#define PRIu32 "u"
#define PRIx32 "x"
#define PRIX32 "X"

#define PRId64 "lld"
#define PRIu64 "llu"
#define PRIx64 "llx"
#define PRIX64 "llX"

#endif