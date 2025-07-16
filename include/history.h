#ifndef HISTORY_H
#define HISTORY_H

#include <time.h>
#include "types.h"

struct History {
    Stack *undoStack, *redoStack;
    Action action;
    time_t time;
    void (*undo)(void);
    void (*redo)(void);
    void (*record)(const ActionType type, const char *data, const ssize_t length, const int ax, const int ay);
    void (*commit)(void);
};
extern struct History H;

void historyInit(void);

void historyFlush(void);

void historyFlushRedo(void);

#endif // !HISTORY_H
