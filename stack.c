#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STACK_SIZE 10

/*** action types ***/

typedef enum ActionType {
    INSERT, REMOVE
} ActionType;

typedef struct Action {
    size_t length;
    int ax, ay;
    ActionType type;
    char *data;
} Action;

/*** node type and methods ***/

typedef struct Node {
    Action action;
    struct Node *next, *prev;
} Node;

static Node* nodeCreate(const Action *act) {
    Node *node = malloc(sizeof(Node));
    if (!node) exit(1);

    node->action = (Action) {act->length, act->ax, act->ay, act->type, strdup(act->data)};
    node->next = node->prev = NULL;
    return node;
}

static void nodeDelete(Node *node) {
    free(node->action.data);
    free(node);
}

/*** stack type and methods ***/

typedef struct Stack {
    Node *top, *bottom;
    size_t size;
} Stack;

Stack stackInit() {
    Stack s;
    s.top = s.bottom = NULL;
    s.size = 0;

    return s;
}

static void stackPush(Stack *s, const Action *act) {
    if (!s->size) {
        s->top = s->bottom = nodeCreate(act);
        s->size = 1;
        return;
    }

    if (s->size == MAX_STACK_SIZE) {
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

static Action* stackPop(Stack *s) {
    if (!s->size) {
        return NULL;
    }

    if (s->size == 1) {
        nodeDelete(s->top);
        s->top = s->bottom = NULL;
        s->size = 0;
    }

    Action* act = malloc(sizeof(Action));
    if (!act) exit(1);

    const Node *node = s->top;
    *act = (Action) {node->action.length, node->action.ax, node->action.ay, node->action.type, strdup(node->action.data)};

    s->top = s->top->prev;
    nodeDelete(s->top->next);
    s->top->next = NULL;
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
    if (act) {
        if (act->length) {
            actionFlush(act);
        }
        free(act);
    }
}

void actionSet(Action *act, const size_t length, const int ax, const int ay, const ActionType type, const char *data) {
    *act = (Action) { length, ax, ay, type, strdup(data) };
}

void actionAppend(Action *act, const char *s, const size_t length) {
    act->data = (char *) realloc(act->data, act->length + length + 1);
    if (!act->data) exit(1);

    memcpy(act->data + act->length, s, length);
    act->length += length;
    act->data[act->length] = '\0';
}

void actionCommit(const Action *act, Stack *s) {
    stackPush(s, act);
}

Action* actionPop(Stack *s) {
    return stackPop(s);
}
