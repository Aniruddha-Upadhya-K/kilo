#ifndef HISTORY_H
#define HISTORY_H

#include <time.h>
#include "types.h"

struct History {
    Stack *undoStack, *redoStack;
    Action action;
    void (*undo)(void);
    void (*redo)(void);
    time_t time;
    void (*record)(ActionType type, char *data, ssize_t length, int ax, int ay);
};
extern struct History H;

void historyInit(void);

void historyFlush(void);

void historyFlushRedo(void);

#endif // !HISTORY_H
