#ifndef TYPES_H
#define TYPES_H

#include <string.h>

typedef enum {
    INSERT_CHAR_BEF,
    REMOVE_CHAR_BEF,
    INSERT_CHAR_AFT,
    REMOVE_CHAR_AFT,
    INSERT_LINE_BEF,
    REMOVE_LINE_BEF,
    INSERT_LINE_AFT,
    REMOVE_LINE_AFT
} ActionType;

typedef struct {
    size_t length;
    int ax, ay;
    ActionType type;
    char *data;
} Action;

struct Stack;
typedef struct Stack Stack;

struct History;
typedef struct History History;

#endif // !TYPES_H
