/*
 * OpenOrbis libc exposes the locale_t APIs from <locale.h>, but not the
 * locale-specific numeric/stdio functions in FreeBSD's xlocale.h. The PS4
 * runtime currently provides the C locale only, so forward these calls to
 * their ordinary C-locale equivalents.
 */
#pragma once
#include <locale.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline long long strtoll_l(const char* value, char** end, int base, locale_t locale)
{
  (void)locale;
  return strtoll(value, end, base);
}

static inline unsigned long long strtoull_l(const char* value, char** end, int base,
                                             locale_t locale)
{
  (void)locale;
  return strtoull(value, end, base);
}

static inline int sscanf_l(const char* input, locale_t locale, const char* format, ...)
{
  (void)locale;
  va_list args;
  va_start(args, format);
  int result = vsscanf(input, format, args);
  va_end(args);
  return result;
}

static inline int snprintf_l(char* output, size_t size, locale_t locale, const char* format, ...)
{
  (void)locale;
  va_list args;
  va_start(args, format);
  int result = vsnprintf(output, size, format, args);
  va_end(args);
  return result;
}

static inline int asprintf_l(char** output, locale_t locale, const char* format, ...)
{
  (void)locale;
  va_list args;
  va_start(args, format);
  va_list copy;
  va_copy(copy, args);
  int size = vsnprintf(NULL, 0, format, copy);
  va_end(copy);
  if (size < 0)
  {
    va_end(args);
    return size;
  }

  *output = (char*)malloc((size_t)size + 1);
  if (!*output)
  {
    va_end(args);
    return -1;
  }
  int result = vsnprintf(*output, (size_t)size + 1, format, args);
  va_end(args);
  if (result < 0)
  {
    free(*output);
    *output = NULL;
  }
  return result;
}

#ifdef __cplusplus
}
#endif
