#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include "types.h"
#include "editor.h"
#include "stack.h"
#include "history.h"

static void historyPerform(const Action *act) {
    switch (act->type) {
        case INSERT_CHAR_BEF:
            editorRemoveChars(act->ay, act->ax, act->length);
            actionTypeConv(&H.action, REMOVE_CHAR_BEF);
            break;
        case REMOVE_CHAR_BEF:
            editorRowInsertCharBefore(act->ay, act->ax, act->data, act->length);
            actionTypeConv(&H.action, INSERT_CHAR_BEF);
            break;
        case INSERT_CHAR_AFT:
            editorRemoveChars(act->ay, act->ax, (-1 * act->length));
            actionTypeConv(&H.action, REMOVE_CHAR_AFT);
            break;
        case REMOVE_CHAR_AFT:
            editorRowInsertCharAfter(act->ay, act->ax, act->data, act->length);
            actionTypeConv(&H.action, INSERT_CHAR_AFT);
            break;
        case INSERT_LINE_BEF:
            editorRemoveChars(act->ay, act->ax, act->length);
            actionTypeConv(&H.action, REMOVE_LINE_BEF);
            break;
        case REMOVE_LINE_BEF:
            editorRowInsertBefore(act->ay, act->ax);
            actionTypeConv(&H.action, INSERT_LINE_BEF);
            break;
        case INSERT_LINE_AFT:
            editorRemoveChars(act->ay, act->ax, (-1 * act->length));
            actionTypeConv(&H.action, REMOVE_LINE_AFT);
            break;
        case REMOVE_LINE_AFT:
            editorRowInsertAfter(act->ay, act->ax);
            actionTypeConv(&H.action, INSERT_LINE_AFT);
            break;
    }
}

static void editorUndo(void) {
    if (!actionIsEmpty(&H.action)) {
        actionCommit(&H.action, H.undoStack);
    }

    const Action *uact = stackPeek(H.undoStack);
    if (!uact) {
        return;
    }

    historyPerform(uact);

    Action *act = actionPop(H.undoStack);
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

    const Action *ract = stackPeek(H.redoStack);
    if (!ract) {
        return;
    }

    historyPerform(ract);
    Action *act = actionPop(H.redoStack);
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
static void historyRecord(ActionType type, char *data, ssize_t length, int ax, int ay) {

    if (S.maxActionTime > 0) {
        if (actionIsEmpty(&H.action)) {
            H.time = time(NULL);
        } else if (difftime(time(NULL), H.time) > S.maxActionTime) {
            actionCommit(&H.action, H.undoStack);
        }
    }

    switch (type) {
        case INSERT_LINE_BEF:
        case INSERT_LINE_AFT:
            if (!actionIsEmpty(&H.action)) actionCommit(&H.action, H.undoStack);
            actionSet(&H.action, 1, ax, ay, type, "\n");
            actionCommit(&H.action, H.undoStack);
            break;
        
        case REMOVE_LINE_BEF:
        case REMOVE_LINE_AFT:
            if (!actionIsEmpty(&H.action)) actionCommit(&H.action, H.undoStack);
            actionSet(&H.action, 1, ax, ay, type, "\b");
            actionCommit(&H.action, H.undoStack);
            break;

        case INSERT_CHAR_AFT:
            break;

        case INSERT_CHAR_BEF:
        case REMOVE_CHAR_BEF:
            if (!actionIsEmpty(&H.action) && H.action.type != type) {
                actionCommit(&H.action, H.undoStack);
            }
            if (actionIsEmpty(&H.action)) {
                actionSet(&H.action, length, ax, ay, type, data);
            } else {
                actionAppend(&H.action, data, length, 0, 0);
            }
            break;

        case REMOVE_CHAR_AFT:
            if (!actionIsEmpty(&H.action) && H.action.type != type) {
                actionCommit(&H.action, H.undoStack);
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
}
