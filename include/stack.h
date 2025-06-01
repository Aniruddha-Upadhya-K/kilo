#ifndef STACK_H
#define STACK_H

#include <string.h>
#include "types.h"

Stack* stackInit(void);

const Action* stackPeek(const Stack *s);

void stackDelete(Stack *s);

void actionFlush(Action *act);

void actionDelete(Action *act);

void actionSet(Action *act, const size_t length, const int ax, const int ay, const ActionType type, const char *data);

void actionAppend(Action *act, const char *s, const size_t length, const int dax, const int day);

void actionCommit(Action *act, Stack *s);

Action* actionPop(Stack *s);

#endif // !STACK_H
