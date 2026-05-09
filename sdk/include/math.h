#ifndef _MATH_H
#define _MATH_H

// Константы
#define HUGE_VAL (__builtin_huge_val())
#define INFINITY (__builtin_inff())
#define NAN (__builtin_nan(""))

#define M_PI 3.14159265358979323846

// Округление
double floor(double x);
double ceil(double x);

// Экспоненты и логарифмы
double pow(double x, double y);
double log(double x);
double log10(double x);
double exp(double x);
double sqrt(double x);

// Тригонометрия
double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);
double atan2(double y, double x);

// Манипуляции с числами (критично для Lua)
double frexp(double x, int *exp);
double ldexp(double x, int exp);
double modf(double x, double *iptr);
double fmod(double x, double y);
double fabs(double x);

#endif