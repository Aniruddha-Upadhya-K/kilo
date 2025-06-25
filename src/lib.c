#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "lib.h"

void die(char *s, ...) {
    va_list ap;
    char err[100];
    va_start(ap, s);
    vsnprintf(err, sizeof(err), s, ap);
    va_end(ap);

    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(err);
    exit(1);
}

/*
 * All lengths are coints except last null character
 */
void stringSlice(char *s, size_t slen, size_t rlen, int from) {
    if (rlen >= slen) {
        s = realloc(s, 1);
        if (!s) die("In function: %s\r\nAt line: %d\r\nrealloc", __func__, __LINE__);
        s[0] = '\0';
        return;
    }

    if (from > (int) slen - 1) from = slen - 1;
    if (from < 0) from = 0;
    if (from + rlen > slen) rlen = slen - from;

    size_t nlen = slen - rlen;
    memmove(s + from, s + from + rlen, rlen);
    s = realloc(s, nlen + 1);
    if (!s) die("In function: %s\r\nAt line: %d\r\nrealloc", __func__, __LINE__);
    s[nlen] = '\0';
}
