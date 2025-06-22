#include <stdarg.h>
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
