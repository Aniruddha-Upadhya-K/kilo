#define _DEFAULT_SOURCE
#define _GNU_SOURCE
#define _BSD_SOURCE

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "lib.h"
#include "types.h"
#include "stack.h"
#include "editor.h"

/*** node type and methods ***/
typedef struct Node {
    Action action;
    struct Node *next, *prev;
} Node;

// CAUTION: The pointer to Node that is returned should be freed by the caller by calling nodeDelete(Node *)
static Node* nodeCreate(const Action *act) {
    Node *node = malloc(sizeof(Node));
    if (!node) die("In function: %s\r\nAt line: %d\r\nmalloc", __func__, __LINE__);

    node->action = (Action) {act->length, act->ax, act->ay, act->type, strdup(act->data) };
    node->next = node->prev = NULL;
    return node;
}

static void nodeDelete(Node *node) {
    if (node) {
        if (node->action.data) free(node->action.data);
        free(node);
    }
}

/*** stack type and methods ***/
struct Stack {
    Node *top, *bottom;
    size_t size;
};

// CAUTION: The pointer to Stack that is returned should be freed by the caller by calling stackDelete(Stack *)
Stack* stackInit(void) {
    Stack *s = malloc(sizeof(Stack));
    if (!s) die("In function: %s\r\nAt line: %d\r\nmalloc", __func__, __LINE__);
    s->top = s->bottom = NULL;
    s->size = 0;
    return s;
}

const Action* stackPeek(const Stack* s) {
    return s->size ? &s->top->action : NULL;
}

// static void deleteStackRecursive(Node *node) {
//     if (!node) {
//         return;
//     }
//     deleteStackRecursive(node->next);
//     nodeDelete(node);
// }

void stackClear(Stack *s) {
    // deleteStackRecursive(s->bottom);
    while (s->top) {
        Node *node = s->top;
        s->top = node->prev;
        nodeDelete(node);
    }

    s->top = s->bottom = NULL;
    s->size = 0;
}

void stackDelete(Stack *s) {
    if (s->size) stackClear(s);
    free(s);
}

static void stackPush(Stack *s, const Action *act) {
    if (!s->size) {
        s->top = s->bottom = nodeCreate(act);
        s->size = 1;
        return;
    }

    if (s->size == S.maxHistory) {
        s->bottom = s->bottom->next;
        nodeDelete(s->bottom->prev);
        s->size--;
        s->bottom->prev = NULL;
    }
    
    Node *node = nodeCreate(act);
    s->top->next = node;
    node->prev = s->top;
    s->top = node;
    s->size++;
}

// CAUTION: The pointer to Action that is returned should be freed by the caller with actionDelete(Action *act)
static Action* stackPop(Stack *s) {
    if (!s->size) {
        return NULL;
    }

    Action* act = malloc(sizeof(Action));
    if (!act) die("In function: %s\r\nAt line: %d\r\nmalloc", __func__, __LINE__);

    Node *node = s->top;
    *act = (Action) {node->action.length, node->action.ax, node->action.ay, node->action.type, strdup(node->action.data) };

    if (s->size == 1) {
        s->top = s->bottom = NULL;
    } else {
        s->top = node->prev;
        s->top->next = NULL;
    }

    nodeDelete(node);
    s->size--;

    return act;
}

/*** action methods ***/
void actionFlush(Action *act) {
    free(act->data);
    act->data = NULL;
    act->length = 0;
    act->ax = act->ay = 0;
}

void actionDelete(Action *act) {
    if (act->length) {
        actionFlush(act);
    }
    free(act);
}

int actionIsEmpty(const Action *act) {
    if (act->length) {
        return 0;
    } else return 1;
}

void actionSet(Action *act, const ssize_t length, const int ax, const int ay, const ActionType type, const char *data) {
    *act = (Action) { length, ax, ay, type, strdup(data) };
}

void actionAppend(Action *act, const char *s, const ssize_t dlength, const int dax, const int day) {
    if (dlength > 0) {
        act->data = (char *) realloc(act->data, act->length + dlength + 1);
        if (!act->data) die("In function: %s\r\nAt line: %d\r\nrealloc", __func__, __LINE__);

        memcpy(act->data + act->length, s, dlength);
        act->length += dlength;
        act->data[act->length] = '\0';
    }

    act->ax += dax;
    act->ay += day;
}

void actionTypeConv(Action *act, ActionType newType) {
    act->type = newType;
}

void actionCommit(Action *act, Stack *s) {
    if (!actionIsEmpty(act)) {
        stackPush(s, act);
        actionFlush(act);
    }
}

Action* actionPop(Stack *s) {
    return stackPop(s);
}
