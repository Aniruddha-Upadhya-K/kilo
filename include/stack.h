#ifndef STACK_H
#define STACK_H

#include <sys/types.h>
#include "types.h"

Stack* stackInit(void);

const Action* stackPeek(const Stack *s);

void stackClear(Stack *s);

void stackDelete(Stack *s);

void actionFlush(Action *act);

void actionDelete(Action *act);

int actionIsEmpty(const Action *act);

void actionSet(Action *act, const ssize_t length, const int ax, const int ay, const ActionType type, const char *data);

void actionAppend(Action *act, const char *s, const ssize_t dlength, const int dax, const int day);

void actionTypeConv(Action *act, ActionType newType);

void actionCommit(Action *act, Stack *s);

Action* actionPop(Stack *s);

#endif // !STACK_H
