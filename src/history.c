#include "types.h"
#include "stack.h"
#include "history.h"

static void editorUndo(void) {

}

static void editorRedo(void) {

}

void historyInit(void) {
    H.undoStack = stackInit();
    H.redoStack = stackInit();
    H.action = NULL;
    H.undo = editorUndo;
    H.redo = editorRedo;
}
