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

Node* nodeCreate(const Action *act) {
    Node *node = malloc(sizeof(Node));
    if (!node) exit(1);

    node->action = (Action) {act->length, act->ax, act->ay, act->type, strdup(act->data)};
    node->next = node->prev = NULL;
    return node;
}

void nodeDelete(Node *node) {
    free(node->action.data);
    free(node);
}

/*** stack type and methods ***/

typedef struct Stack {
    Node *top, *bottom;
    size_t size;
} Stack;

void stackInit(Stack *s) {
    s->top = s->bottom = NULL;
    s->size = 0;
}

void stackPush(Stack *s, const Action *act) {
    if (!s->size) {
        s->top = s->bottom = nodeCreate(act);
        s->size = 1;
        return;
    }

    if (s->size == MAX_STACK_SIZE) {
        s->bottom = s->bottom->next;
        nodeDelete(s->bottom->prev);
        s->bottom->prev = NULL;
    }
    
    Node *node = nodeCreate(act);
    s->top->next = node;
    node->prev = s->top;
    s->top = node;
    s->size++;
}

Action* stackPop(Stack *s) {
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
void actionSet(Action *act, size_t length, int ax, int ay, ActionType type, char *data) {}

void actionAppend() {}

void actionFlush() {}

void actionCommit() {}
