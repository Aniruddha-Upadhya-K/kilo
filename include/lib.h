#ifndef LIB_H
#define LIB_H

#include <stdarg.h>
#include <string.h>

void die(char *s, ...);

void stringSlice(char *s, size_t slen, size_t rlen, int from);

#endif // !LIB_H
