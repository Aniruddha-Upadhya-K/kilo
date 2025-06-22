#include "types.h"
#include "editor.h"
#include "stack.h"
#include "history.h"

static void historyPerform(const Action *act) {
    switch (act->type) {
        case INSERT_CHAR_BEF:
            editorRemoveChars(act->ay, act->ax, act->length);
            actionTypeConv(H.action, REMOVE_CHAR_BEF);
            break;
        case REMOVE_CHAR_BEF:
            editorRowInsertCharBefore(act->ay, act->ax, act->data, act->length);
            actionTypeConv(H.action, INSERT_CHAR_BEF);
            break;
        case INSERT_CHAR_AFT:
            editorRemoveChars(act->ay, act->ax, (-1 * act->length));
            actionTypeConv(H.action, REMOVE_CHAR_AFT);
            break;
        case REMOVE_CHAR_AFT:
            editorRowInsertCharAfter(act->ay, act->ax, act->data, act->length);
            actionTypeConv(H.action, INSERT_CHAR_AFT);
            break;
        case INSERT_LINE_BEF:
            editorRemoveChars(act->ay, act->ax, act->length);
            actionTypeConv(H.action, REMOVE_LINE_BEF);
            break;
        case REMOVE_LINE_BEF:
            editorRowInsertBefore(act->ay, act->ax);
            actionTypeConv(H.action, INSERT_LINE_BEF);
            break;
        case INSERT_LINE_AFT:
            editorRemoveChars(act->ay, act->ax, (-1 * act->length));
            actionTypeConv(H.action, REMOVE_LINE_AFT);
            break;
        case REMOVE_LINE_AFT:
            editorRowInsertAfter(act->ay, act->ax);
            actionTypeConv(H.action, INSERT_LINE_AFT);
            break;
    }
}

static void editorUndo(void) {
    if (!actionIsEmpty(H.action)) {
        actionCommit(H.action, H.undoStack);
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
    if (H.action->length) {
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

void historyInit(void) {
    H.undoStack = stackInit();
    H.redoStack = stackInit();
    H.action = NULL;
    H.undo = editorUndo;
    H.redo = editorRedo;
}
