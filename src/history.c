#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include "lib.h"
#include "types.h"
#include "editor.h"
#include "stack.h"
#include "history.h"

static void historyPerform(Action *act) {
    switch (act->type) {
        case INSERT_CHAR_BEF:
            editorRemoveChars(act->ay, act->ax + act->length, act->length);
            actionTypeConv(act, REMOVE_CHAR_BEF);
            actionAppend(act, "", 0, act->length, 0);
            break;
        case REMOVE_CHAR_BEF:
            editorRowInsertCharBefore(act->ay, act->ax - act->length, act->data, act->length);
            actionTypeConv(act, INSERT_CHAR_BEF);
            actionAppend(act, "", 0, -act->length, 0);
            break;
        case INSERT_CHAR_AFT:
            editorRemoveChars(act->ay, act->ax, (-1 * act->length));
            actionTypeConv(act, REMOVE_CHAR_AFT);
            break;
        case REMOVE_CHAR_AFT:
            editorRowInsertCharAfter(act->ay, act->ax, act->data, act->length);
            actionTypeConv(act, INSERT_CHAR_AFT);
            break;
        case INSERT_LINE_BEF:
            editorRemoveChars(act->ay + 1, 0, 1);
            actionTypeConv(act, REMOVE_LINE_BEF);
            actionAppend(act, "", 0, 0, 1);
            break;
        case REMOVE_LINE_BEF:
            editorRowInsertBefore(act->ay - 1, act->ax);
            actionTypeConv(act, INSERT_LINE_BEF);
            actionAppend(act, "", 0, 0, -1);
            break;
        case INSERT_LINE_AFT:
            editorRemoveChars(act->ay, act->ax, -1);
            actionTypeConv(act, REMOVE_LINE_AFT);
            break;
        case REMOVE_LINE_AFT:
            editorRowInsertAfter(act->ay, act->ax);
            actionTypeConv(act, INSERT_LINE_AFT);
            break;
    }
}

static void historyCommit(void) {
    if (actionIsEmpty(&H.action)) return;
    if (H.action.type == REMOVE_CHAR_BEF || H.action.type == INSERT_CHAR_AFT) {
        strRev(H.action.data);
    }

    actionCommit(&H.action, H.undoStack);
}

static void editorUndo(void) {
    if (!actionIsEmpty(&H.action)) {
        historyCommit();
    }

    Action *act = actionPop(H.undoStack);
    if (!act) {
        return;
    }

    historyPerform(act);

    actionCommit(act, H.redoStack);
}

void historyFlushRedo(void) {
    while(stackPeek(H.redoStack)) {
        Action *act = actionPop(H.redoStack);
        actionDelete(act);
    }
}

static void editorRedo(void) {
    if (H.action.length) {
        historyFlushRedo();
        return;
    }

    Action *act = actionPop(H.redoStack);
    if (!act) {
        return;
    }

    historyPerform(act);
    actionCommit(act, H.undoStack);
}

void historyFlush(void) {
    stackDelete(H.undoStack);
    stackDelete(H.redoStack);
}

/*
 * Description:
 * This function records every action that takes place in the editor and appends to Undo stack
 *
 * type => is the type of operation just performed
 * data => the character(s) that is just appended or deleted
 * length => length of data
 *    +ve when the operation is anything but DELETE keypress 
 *    -ve when the operation is DELETE keypress
 * ax => x-coordinate of the cursor at the time of action (0-indexed)
 * ay => y-coordinate of the cursor at the time of action (0-indexed)
 */
static void historyRecord(const ActionType type, const char *data, const ssize_t length, const int ax, const int ay) {

    if (S.maxActionTime > 0) {
        if (actionIsEmpty(&H.action)) {
            H.time = time(NULL);
        } else if (difftime(time(NULL), H.time) > S.maxActionTime) {
            historyCommit();
        }
    }

    historyFlushRedo();

    switch (type) {
        case INSERT_LINE_BEF:
        case INSERT_LINE_AFT:
            if (!actionIsEmpty(&H.action)) historyCommit();
            actionSet(&H.action, 1, ax, ay, type, "\n");
            historyCommit();
            break;
        
        case REMOVE_LINE_BEF:
        case REMOVE_LINE_AFT:
            if (!actionIsEmpty(&H.action)) historyCommit();
            actionSet(&H.action, 1, ax, ay, type, "\b");
            historyCommit();
            break;

        case INSERT_CHAR_AFT:
            break;

        case INSERT_CHAR_BEF:
        case REMOVE_CHAR_BEF:
            if (!actionIsEmpty(&H.action) && H.action.type != type) {
                historyCommit();
            }
            if (actionIsEmpty(&H.action)) {
                actionSet(&H.action, length, ax, ay, type, data);
            } else {
                actionAppend(&H.action, data, length, 0, 0);
            }
            break;

        case REMOVE_CHAR_AFT:
            if (!actionIsEmpty(&H.action) && H.action.type != type) {
                historyCommit();
            }
            if (actionIsEmpty(&H.action)) {
                actionSet(&H.action, -length, ax, ay, type, data);
            } else {
                actionAppend(&H.action, data, -length, 0, 0);
            }
            break;
    }
}

void historyInit(void) {
    H.undoStack = stackInit();
    H.redoStack = stackInit();
    H.action = (Action) {.length = 0, .ax = 0, .ay = 0, .data = NULL};
    H.time = time(NULL);
    H.undo = editorUndo;
    H.redo = editorRedo;
    H.record = historyRecord;
    H.commit = historyCommit;
}
