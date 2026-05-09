#include <errno.h>
#include <math.h>
#include <time.h>


// --- Округление ---
double floor(double x) { return __builtin_floor(x); }
double ceil(double x) { return __builtin_ceil(x); }
double fabs(double x) { return __builtin_fabs(x); }

// --- Степень и корни ---
double sqrt(double x) {
  if (x < 0) {
    errno = EDOM;
    return NAN;
  }
  return __builtin_sqrt(x);
}

double pow(double x, double y) {
  // Базовая обработка специфичных случаев для Lua
  if (x < 0 && __builtin_floor(y) != y) {
    errno = EDOM;
    return NAN;
  }
  return __builtin_pow(x, y);
}

// --- Экспоненты и логарифмы ---
double log(double x) {
  if (x < 0) {
    errno = EDOM;
    return NAN;
  }
  if (x == 0) {
    errno = ERANGE;
    return -HUGE_VAL;
  }
  return __builtin_log(x);
}

double log10(double x) {
  if (x < 0) {
    errno = EDOM;
    return NAN;
  }
  if (x == 0) {
    errno = ERANGE;
    return -HUGE_VAL;
  }
  return __builtin_log10(x);
}

double exp(double x) { return __builtin_exp(x); }

// --- Тригонометрия ---
double sin(double x) { return __builtin_sin(x); }
double cos(double x) { return __builtin_cos(x); }
double tan(double x) { return __builtin_tan(x); }

double asin(double x) {
  if (x < -1.0 || x > 1.0) {
    errno = EDOM;
    return NAN;
  }
  return __builtin_asin(x);
}

double acos(double x) {
  if (x < -1.0 || x > 1.0) {
    errno = EDOM;
    return NAN;
  }
  return __builtin_acos(x);
}

double atan(double x) { return __builtin_atan(x); }
double atan2(double y, double x) { return __builtin_atan2(y, x); }

// --- Расщепление числа ---

// Разделяет число на мантиссу (0.5..1) и степень двойки
// Нужно Lua для сохранения чисел в байт-код или дебага
double frexp(double x, int *exp) { return __builtin_frexp(x, exp); }

// Обратная функция к frexp
double ldexp(double x, int exp) { return __builtin_ldexp(x, exp); }

// Разделяет на целую и дробную часть
double modf(double x, double *iptr) { return __builtin_modf(x, iptr); }

// Остаток от деления
double fmod(double x, double y) {
  if (y == 0)
    return NAN;
  return __builtin_fmod(x, y);
}

// difftime считает разницу между временем
double difftime(time_t t1, time_t t0) { return (double)(t1 - t0); }