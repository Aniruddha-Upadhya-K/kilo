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

// TODO: this function should clear the stacks and the actions that were created.
void historyFlush(History *H);

#endif // !HISTORY_H
