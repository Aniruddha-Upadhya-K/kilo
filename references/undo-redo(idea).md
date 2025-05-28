# Undo Redo
This is reference for Undo-Redo stack (undo-redo tree will follow similar types, but 2 stacks are replaced with a tree, mostly a B-tree).

To perform Undo or Redo, we have to store every action that is performed, in this case in an Undo stack.

When the user hits Undo, the top action should be popped and moved to redo stack.

If the user performs another action, after Undoing, then Redo stack has to be cleared.

So each element in the Undo stack should look like this

```c
stack<Action> undo, redo;
```

And each action is defined as

```c
typedef enum {
    UNDO, REDO
} ActionType;

typedef struct Action {
    int cx, rx;
    int cy;
    char *action;
    ActionType actionType; 
    int actionSize;
} Action;
```

In this case (cx, cy) will be the initial cursor position where the action began, in the plain text buffer (where TAB is denoted as \t)
and (rx, cy) will be the initial cursor poition where the action began, in the rendered text buffer (where TAB is expanded as _user-defined_ amount of spaces)

## How to group action?
what i mean is if you enter 10 characters, should all 10 characters be removed on next Undo action, or should i group it into multiple actions?

GNU Nano seems to group actions until user performs grouping event (pressing enter key, or arrow keys).

other editors might perform differently (excluding editors like vim, modal based editing is beyond the scope of this project atleast, even though i like it more than traditional editors).

what seems more intuitive for me is, (other editor might be doing it, but i dint really observe it, and took it all for granted) start a timer between each keypress. If the next key takes longer than some threshold then split the action into two groups. Also include Nano like event based splitting.

So if the user presses enter or arrow keys, then one action is completed, but even if user doesnt press those keys, the action will end if user takes long time to press next key (may be like 5sec, just a random number that seemed OK).
