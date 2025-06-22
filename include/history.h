#ifndef HISTORY_H
#define HISTORY_H

#include "types.h"

struct History {
    Stack *undoStack, *redoStack;
    Action *action;
    void (*undo)(void);
    void (*redo)(void);
} H;

void historyInit(void);

void historyFlush(void);

void historyFlushRedo(void);

#endif // !HISTORY_H
