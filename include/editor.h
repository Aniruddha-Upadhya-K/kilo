#ifndef EDITOR_H
#define EDITOR_H

#include <string.h>
#include <termios.h>
#include <time.h>

struct editorSetting {
    int scrolloff;
    int tabwidth;
    int maxFileNameSize;
    int maxMsgSize;
    size_t maxHistory;
};
extern struct editorSetting S;

typedef struct {
    size_t size;
    char *chars;
    size_t rsize;
    char *render;
} erow;

struct editorMsg {
    char *data;
    int length;
    time_t time;
    int isFocus;

    int cx;
    int cy;
};

struct editorConfig {
    int cx, cy; /* 0 indexed */
    int rx;
    int screenrows;
    int screencols;
    erow *row;
    int rowoff; /* Has the value of first line number in the current view area (0
                                   indexed) */
    int coloff; /* Has the value of first column number in the current view area
                                   (0 indexed) */
    int numrows;
    int max_rx;
    char *filename;
    struct editorMsg message;
    struct termios orig_termios;
};
extern struct editorConfig E;

void editorRemoveChars(int cat, int curline, int clen);

void editorRowInsertCharAfter(erow *row, int cat, const char *s, const int len);

void editorRowInsertCharBefore(erow *row, int cat, const char *s, const int len);

void editorRowInsertAfter(int curline, int cat, int rat);

void editorRowInsertBefore(int curline, int cat, int rat);

#endif // !EDITOR_H
