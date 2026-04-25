#ifndef _ASSERT_H
#define _ASSERT_H

#include <stdio.h>
#include <stdlib.h>

// Если условие false, печатаем ошибку и выходим
#define assert(expression) \
    if (!(expression)) { \
        printf("Assertion failed: %s, file %s, line %d\n", #expression, __FILE__, __LINE__); \
        exit(1); \
    }

#endif