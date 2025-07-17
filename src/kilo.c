/*** includes ***/
#define _DEFAULT_SOURCE
#define _GNU_SOURCE
#define _BSD_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "lib.h"
#include "types.h"
#include "editor.h"
#include "stack.h"
#include "history.h"

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)

#define KILO_VERSION "0.0.1"

enum editorKey {
    BACKSPACE = 127,
    ARROW_UP = 1000,
    ARROW_DOWN,
    ARROW_LEFT,
    ARROW_RIGHT,
    PAGE_UP,
    PAGE_DOWN,
    HOME_KEY,
    END_KEY,
    DELETE_KEY,
    EOL
};

/*** terminal ***/
void disableRawMode(void) {
    for (int i=0; i < E.numrows; i++) {
        free(E.row[i].chars);
        free(E.row[i].render);
    }
    free(E.row);
    free(E.filename);

    if (E.message.length) free(E.message.data);

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("In function: %s\r\nAt line: %d\r\ntcsetattr", __func__, __LINE__);
}

void enableRawMode(void) {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        die("In function: %s\r\nAt line: %d\r\ntcsetattr", __func__, __LINE__);
    atexit(disableRawMode);

    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(ICRNL | IXON | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cc[VTIME] = 1;
    raw.c_cc[VMIN] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("In function: %s\r\nAt line: %d\r\ntcsetattr", __func__, __LINE__);
}

int editorReadKey(void) {
    char c;
    int nread;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && nread != EAGAIN)
            die("In function: %s\r\nAt line: %d", __func__, __LINE__);
    }

    if (c == '\x1b') {
        char seq[3];

        if (read(STDIN_FILENO, &seq[0], 1) != 1)
            return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
            return '\x1b';

        if (seq[0] != '[') {
            if (seq[0] == 'O') {
                switch (seq[1]) {
                    case 'H':
                        return HOME_KEY;
                    case 'F':
                        return END_KEY;
                }
            } else
            return '\x1b';
        }

        if (seq[1] >= '0' && seq[1] < '9') {
            if (read(STDIN_FILENO, &seq[2], 1) != 1)
                return '\x1b';
            if (seq[2] == '~') {
                switch (seq[1]) {
                    case '1':
                        return HOME_KEY;
                    case '3':
                        return DELETE_KEY;
                    case '4':
                        return END_KEY;
                    case '5':
                        return PAGE_UP;
                    case '6':
                        return PAGE_DOWN;
                    case '7':
                        return HOME_KEY;
                    case '8':
                        return END_KEY;
                }
            }
        }

        switch (seq[1]) {
            case 'A':
                return ARROW_UP;
            case 'B':
                return ARROW_DOWN;
            case 'C':
                return ARROW_RIGHT;
            case 'D':
                return ARROW_LEFT;
            case 'F':
                return END_KEY;
            case 'H':
                return HOME_KEY;
        }
    }
    return c;
}

int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    size_t i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) !=
        4) /* Writes current cursor position */
        return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1)
            break;
        if (buf[i] == 'R')
            break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[')
        return -1;
    sscanf(&buf[2], "%d;%d", rows, cols);

    return 0;
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
            return -1;
        }
        return getCursorPosition(rows, cols);
    } else {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
        return 0;
    }
}

/*** row operations ***/
int editorRowCxToRx(const erow *row, int cx) {
    int rx = 0;
    for (int i = 0; i < cx; i++) {
        if (row->chars[i] == '\t')
            rx += S.tabwidth - (rx % S.tabwidth);
        else
            rx++;
    }

    return rx;
}

int editorRowRxToCx(const erow *row, int rx) {
    int cx = 0;
    int i = 0;
    while (cx < (int)row->size && i < rx) {
        if (row->chars[cx] == '\t') {
            if (i + S.tabwidth - (i % S.tabwidth) - 1 >= rx)
                break;
            i += S.tabwidth - (i % S.tabwidth) - 1;
        }
        cx++;
        i++;
    }

    return cx;
}

void editorUpdateRow(erow *row) {
    int tabcount = 0;
    for (size_t i = 0; i < row->size; i++)
        if (row->chars[i] == '\t')
            tabcount++;

    row->render = realloc(row->render, row->size + tabcount * (S.tabwidth - 1) + 1);
    if (!row->render) die("In function: %s\r\nAt line: %d\r\nrealloc", __func__, __LINE__);

    size_t idx = 0;
    for (size_t j = 0; j < row->size; j++) {
        if (row->chars[j] == '\t') {
            row->render[idx++] = ' ';
            while (idx % S.tabwidth != 0) {
                row->render[idx++] = ' ';
            }
        } else {
            row->render[idx++] = row->chars[j];
        }
    }

    row->render[idx] = '\0';
    row->rsize = idx;
}

void editorRowAppend(const char *s, size_t len) {
    E.row = (erow *) realloc(E.row, sizeof(erow) * (E.numrows + 1));
    if (!E.row) die("In function: %s\r\nAt line: %d\r\nrealloc", __func__, __LINE__);

    int at = E.numrows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    E.numrows++;

    E.row[at].render = NULL;
    E.row[at].rsize = 0;
    editorUpdateRow(&E.row[at]);
}

/*
 * Description:
 * Inserts characters at the position of the cursor but does not modify the cursor position
 * Simulates opposite of DELETE key function
 */
void editorRowInsertCharAfter(int curline, int cat, const char *s, const int len) {
    erow *row = &E.row[curline];
    if (cat < 0 || cat > (int) row->size) cat = row->size;

    row->chars = (char *) realloc(row->chars, row->size + len + 1);
    if (!row->chars) die("In function: %s\r\nAt line: %d\r\nrealloc", __func__, __LINE__);
    memmove(row->chars + cat + len, row->chars + cat, row->size - cat + 1);
    memcpy(row->chars + cat, s, len);

    row->size += len;

    editorUpdateRow(row);
}

/*
 * Description:
 * Inserts characters at the position of the cursor and modifies the cursor position
 * Default behaviour
 * Exactly opposite of BACKSPACE key function
 */
void editorRowInsertCharBefore(int curline, int cat, const char *s, const int len) {
    editorRowInsertCharAfter(curline, cat, s, len);

    E.cx += len;
    E.rx = editorRowCxToRx(&E.row[curline], E.cx);
    if (E.rx > E.max_rx) E.max_rx = E.rx;
}

/*
 * Description:
 * Inserts line at the cursor position but cursor position remains unchanged
 * Simulates opposite of DELETE key function at the end of a line
 */
void editorRowInsertAfter(int curline, int cat) {
    if (curline < 0 || curline >= E.numrows) curline = E.numrows - 1;

    E.row = (erow *) realloc(E.row, sizeof(erow) * (E.numrows + 1));
    if (!E.row) die("In function: %s\r\nAt line: %d\r\nrealloc", __func__, __LINE__);

    erow *currow = E.row + curline;

    if (cat < 0 || cat > (int) currow->size) cat = currow->size;

    if (curline < E.numrows - 1)
        memmove(currow + 2, currow + 1, sizeof(erow) * (E.numrows - curline - 1));

    int size = currow->size - cat;
    erow nextrow = {.size = size};

    nextrow.chars = (char *) malloc(size + 1);
    if (!nextrow.chars) die("In function: %s\r\nAt line: %d\r\nmalloc", __func__, __LINE__);

    memcpy(nextrow.chars, &currow->chars[cat], currow->size + 1 - cat);
    currow->chars[cat] = '\0';

    currow->chars = (char *) realloc(currow->chars, cat + 1);
    if (!currow->chars) die("In function: %s\r\nAt line: %d\r\nrealloc", __func__, __LINE__);

    currow->size = cat;
    E.numrows++;
    E.row[curline + 1] = nextrow;
    editorUpdateRow(&E.row[curline]);
    editorUpdateRow(&E.row[curline + 1]);
}

/*
 * Description:
 * Inserts line at the cursor position and puts cursor to new line
 * Default behaviour
 * Simulates opposite of BACKSPACE key function at the end of a line
 */
void editorRowInsertBefore(int curline, int cat) {
    editorRowInsertAfter(curline, cat);

    E.cy++;
    E.max_rx = 0;
    E.cx = 0;
    E.rx = 0;
}

/*
* Description: 
* +ve clen => BACKSPACE functionality
* -ve clen => DELETE functionality
*
* BACKSPACE functionality:
* from one character before the position 'cat' in current row to a total of 'clen' characters will be removed from 'chars' array(s)
*
* DELETE functionality:
* characters after current character 'cat', in current row (or after) will be removed to a total of absolute value of 'clen' characters from 'chars' array(s)
*/
void editorRemoveChars(int curline, int cat, int clen) {
    if (curline > E.numrows - 1 && curline < 0) curline = E.numrows - 1;
    erow *currow = &E.row[curline];
    if (cat > (int) currow->size && cat < 0) cat = currow->size;

    if (clen > cat && curline == 0) {
        clen = cat;
    } else if (clen < 0 && cat + abs(clen) > (int) currow->size && curline == E.numrows - 1) {
        clen = -(currow->size - cat);
    }


    if (clen > cat) {
        clen -= cat + 1;

        erow *prevrow = &E.row[curline - 1];

        int prevRowSize = prevrow->size;
        int prevRowRsize = prevrow->rsize;
        int curRowSize = currow->size;

        prevrow->chars = (char *) realloc(prevrow->chars, prevRowSize + curRowSize + 1);
        if (!prevrow->chars) die("In function: %s\r\nAt line: %d\r\nrealloc", __func__, __LINE__);

        memmove(&prevrow->chars[prevRowSize], currow->chars, curRowSize + 1);
        prevrow->size += curRowSize;
        editorUpdateRow(prevrow);

        if (curline + 1 < E.numrows) {
            free(currow->chars);
            free(currow->render);

            memmove(E.row + curline, E.row + curline + 1, sizeof(erow) * (E.numrows - curline - 2));

            erow *lastrow = &E.row[E.numrows - 1];

            E.row[E.numrows - 2] = (erow) {lastrow->size, strdup(lastrow->chars), lastrow->rsize, strdup(lastrow->render)};
        }
        E.numrows--;
        free(E.row[E.numrows].chars);
        free(E.row[E.numrows].render);
        E.row = (erow *) realloc(E.row, E.numrows * sizeof(erow));
        if (!E.row) die("In function: %s\r\nAt line: %d\r\nrealloc", __func__, __LINE__);

        E.cy--;
        E.cx = prevRowSize;
        E.rx = prevRowRsize;
        E.max_rx = E.rx;

        if (clen > 0) editorRemoveChars(E.cy, E.cx, clen);
    } else if (clen < 0 && abs(clen) > (int) currow->size - cat) {
        clen += currow->size - cat + 1;

        erow *nextrow = &E.row[curline + 1];

        currow->chars = (char*) realloc(currow->chars, cat + nextrow->size + 1);
        memcpy(currow->chars + cat, nextrow->chars, nextrow->size + 1);
        currow->size = cat + nextrow->size;

        editorUpdateRow(currow);

        erow *lastrow = &E.row[E.numrows - 1];
        if (curline + 1 < E.numrows - 1) {
            free(nextrow->chars);
            free(nextrow->render);

            if (curline + 2 < E.numrows - 1)
                memmove(nextrow, &E.row[curline + 2], sizeof(erow) * (E.numrows - (curline + 3)));

            E.row[E.numrows - 2] = (erow) {lastrow->size, strdup(lastrow->chars), lastrow->rsize, strdup(lastrow->render)};
        }

        free(lastrow->chars);
        free(lastrow->render);
        E.numrows--;
        E.row = (erow*) realloc(E.row, sizeof(erow) * E.numrows);

        if (clen < 0) editorRemoveChars(curline, cat, clen);
    } else {
        char *src = NULL, *dest = NULL;
        int movesize = 0;
        if (clen < 0) {
            // DELETE
            src = currow->chars + cat - clen;
            dest = currow->chars + cat;
            movesize = currow->size - (cat - clen) + 1;
        } else {
            // BACKSPACE
            src = currow->chars + cat;
            dest = currow->chars + cat - clen;
            movesize = currow->size - cat + 1;
        }

        memmove(dest, src, movesize);

        currow->size -= abs(clen);

        currow->chars = (char *) realloc(currow->chars, currow->size + 1);
        if (!E.row) die("In function: %s\r\nAt line: %d\r\nrealloc", __func__, __LINE__);

        editorUpdateRow(currow);

        if (clen > 0) {
            E.cx -= clen;
            E.rx = editorRowCxToRx(currow, E.cx);
            E.max_rx = E.rx;
        }
    }
}

/*** append buffer ***/
struct abuf {
    char *b;
    size_t len;
};

void abAppend(struct abuf *ab, const char *s, size_t len) {
    char *new = (char *) realloc(ab->b, ab->len + len + 1);
    if (!new) die("In function: %s\r\nAt line: %d\r\nrealloc", __func__, __LINE__);

    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
    ab->b[ab->len] = '\0';
}

void abFree(struct abuf *ab) { 
    free(ab->b); 
}

/*** output ***/
void welcome(struct abuf *ab) {
    char welcome[80];
    int welcomelen = snprintf(welcome, sizeof(welcome),
                              "Kilo Editor --- version %s", KILO_VERSION);
    if (welcomelen > E.screencols)
        welcomelen = E.screencols;

    int padding = (E.screencols - welcomelen) / 2;
    if (padding) {
        abAppend(ab, "~", 1);
        padding--;
    }
    while (padding--)
        abAppend(ab, " ", 1);
    abAppend(ab, welcome, welcomelen);
}

void editorScroll(void) {
    if (E.numrows < E.screenrows);
    else if (E.cy < E.rowoff + S.scrolloff) {
        if (E.cy > S.scrolloff)
            E.rowoff = E.cy - S.scrolloff;
        else
            E.rowoff = 0;
    } else if (E.cy > E.rowoff + E.screenrows - S.scrolloff - 1) {
        if (E.cy + S.scrolloff < E.numrows)
            E.rowoff = E.cy - E.screenrows + S.scrolloff + 1;
        else
            E.rowoff = E.numrows - E.screenrows;
    }
    if (E.rx < E.coloff) {
        E.coloff = E.rx;
    }
    if (E.rx > E.coloff + E.screencols - 1) {
        E.coloff = E.rx - E.screencols + 1;
    }
}

void editorDrawRows(struct abuf *ab) {
    editorScroll();

    for (int y = 0; y < E.screenrows; y++) {
        if (y >= E.numrows) {
            if ((E.numrows == 1 && E.row[0].size == 0) && y == 2 * E.screenrows / 3) {
                welcome(ab);
            } else
            abAppend(ab, "~", 1);
        } else {
            int currow = y + E.rowoff;
            int len = E.row[currow].rsize - E.coloff;
            if (len < 0)
                len = 0;
            if (E.screencols < len)
                len = E.screencols;
            abAppend(ab, &E.row[currow].render[E.coloff], len);
        }

        abAppend(ab, "\x1b[K", 3); /* Clear line to the right of cursor */
        abAppend(ab, "\r\n", 2);
    }
}

/*** output: status & message bar ***/
void editorDrawStatusBar(struct abuf *ab) {
    abAppend(ab, "\x1b[7m", 4); /* Inverts the fg and bg colors */
    int idx = 0;

    char status[13];
    ssize_t statusLength;
    statusLength = snprintf(status, sizeof(status), "%d,%d", E.cy + 1, E.rx + 1);

    char name[50];
    ssize_t nameLength;
    nameLength =
        snprintf(name, sizeof(name), "%.*s - %d lines", S.maxFileNameSize,
                 E.filename ? E.filename : "[No File]", E.numrows);

    if (nameLength + statusLength + 1 > E.screencols) {
        if (statusLength + 1 > E.screencols) {
            statusLength = E.screencols;
            nameLength = 0;
        } else {
            nameLength = E.screencols - statusLength - 1;
        }
    }
    idx = nameLength;

    abAppend(ab, name, nameLength);

    while (idx < E.screencols) {
        if (E.screencols - idx == statusLength) {
            abAppend(ab, status, statusLength);
            break;
        }
        idx++;
        abAppend(ab, " ", 1);
    }
    abAppend(ab, "\x1b[m", 3); /* Switches back to normal formatting */
    abAppend(ab, "\r\n", 2);
}

void editorClearMessage (void) {
    if (E.message.length) free(E.message.data);
    E.message.length = 0;
}

void editorSetMessage(char *fmt, ...) {
    if (E.message.length) editorClearMessage();
    E.message.data = (char *) malloc(S.maxMsgSize);

    va_list ap;
    va_start(ap, fmt);
    E.message.length = vsnprintf(E.message.data, S.maxMsgSize, fmt, ap);
    va_end(ap);

    if (E.message.length < 0)
        die("In function: %s\r\nAt line: %d\r\nvsnprintf", __func__, __LINE__);
    else if (E.message.length >= S.maxMsgSize) {
        E.message.length = S.maxMsgSize - 1;
    }

    if (E.message.isFocus) {
        E.message.cy = E.screenrows + 1;
        E.message.cx = E.message.length;
    }

    E.message.time = time(NULL);
}

void editorAppendMessage (const char *s, const int length) {
    char *new = realloc(E.message.data, E.message.length + length + 1);
    if (!new) die("In function: %s\r\nAt line: %d\r\nrealloc", __func__, __LINE__);

    memcpy(new + E.message.length, s, length);
    E.message.data = new;
    E.message.length += length;
    E.message.data[E.message.length] = '\0';

    if (E.message.length >= S.maxMsgSize) {
        E.message.length = S.maxMsgSize - 1;
    }

    if (E.message.cx < S.maxMsgSize) E.message.cx++;
}

void editorDrawMessageBar(struct abuf *ab) {
    abAppend(ab, "\x1b[K", 3);
    if (E.message.length > E.screencols) E.message.length = E.screencols;
    if (E.message.isFocus || (E.message.length && time(NULL) - E.message.time < 5))
        abAppend(ab, E.message.data, E.message.length);
    if (!E.message.isFocus && time(NULL) - E.message.time > 5) 
        editorClearMessage();
}

void editorRefreshScreen(void) {
    struct abuf ab = {NULL, 0};

    abAppend(&ab, "\x1b[?25l", 6); /* Hide cursor */
    abAppend(&ab, "\x1b[H", 3);    /* Move cursor to top-left */

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);

    char cursorpos[15];
    size_t cursorposLen;
    if (!E.message.isFocus)
        cursorposLen = snprintf(cursorpos, sizeof(cursorpos), "\x1b[%d;%dH",
                                    (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);
    else {
        cursorposLen = snprintf(cursorpos, sizeof(cursorpos), "\x1b[%d;%dH",
                                    (E.message.cy) + 1, (E.message.cx) + 1);
    }
    abAppend(
        &ab, cursorpos,
        cursorposLen); /* Move cursor position back to its actual position */
    abAppend(&ab, "\x1b[?25h", 6); /* Unhide the cursor */

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** file i/o ***/
void editorOpenEmpty(void) {
    int linelen = 0;
    char line = '\0';
    editorRowAppend(&line, linelen);
}

void editorOpen(const char *filename) {
    free(E.filename);
    E.filename = strdup(filename);
    if (!E.filename)
        die("In function: %s\r\nAt line: %d\r\nNo file name given", __func__, __LINE__);

    FILE *fp = fopen(E.filename, "r");
    if (!fp)
        fp = fopen(E.filename, "w");

    ssize_t linelen = 0;
    size_t linecap = 0;
    char *line = NULL;

    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        while (linelen > 0 &&
            (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) {
            linelen--;
        }
        editorRowAppend(line, linelen);
    }

    if (E.row == NULL) editorOpenEmpty(); /* Condition to make sure empty files are opened correctly */

    free(line);
    fclose(fp);
}

void editorSave(const char *filename) {
    if (!filename) {
        editorSetMessage("File name is not set!");
        return;
    }

    size_t writeSize = 0;
    for (int curline = 0; curline < E.numrows - 1; curline++) {
        writeSize += E.row[curline].size + 2;
    }
    writeSize += E.row[E.numrows - 1].size + 1;

    FILE *file;
    if (!E.message.isFocus) {
        if (!(file = fopen(filename, "r+"))) {
            editorSetMessage("File does not exist");
            return;
        }
    } else {
        file = fopen(filename, "w");
    }

    char *buf = (char *) malloc(writeSize);
    if (buf == NULL) die("In function: %s\r\nAt line: %d\r\nmalloc", __func__, __LINE__);

    size_t offset = 0;
    for (int curline = 0; curline < E.numrows; curline++) {
        const erow *row = &E.row[curline];
        memcpy(buf + offset, row->chars, row->size);

        if (curline != E.numrows - 1) {
            buf[offset + row->size] = '\r';
            buf[offset + row->size + 1] = '\n';
            offset += row->size + 2;
        } else 
            buf[offset + row->size] = '\0';
    }
    
    if (writeSize > 1) {
        size_t bytes = fwrite(buf, sizeof(char), writeSize - 1, file);
        if (bytes < writeSize - 1) die("In function: %s\r\nAt line: %d\r\nfwrite", __func__, __LINE__);
    }

    fclose(file);
    free(buf);
    editorSetMessage("Total of %ld bytes have been written to disk", writeSize - 1);
}

void editorSaveAs(void) {
    E.message.isFocus = 1;
    int filenamesize = 0;
    char *filename = NULL;

    editorSetMessage("Enter file name: ");
    editorRefreshScreen();

    int c;
    while ((c = editorReadKey()) != '\r') {
        if (filenamesize > S.maxFileNameSize) {
            editorSetMessage("File name can not exceed %d characters", S.maxFileNameSize);
            E.message.isFocus = 0;
            E.message.time = time(NULL);
            return;
        }

        switch (c) {
            case '\x1b' :
                free(filename);
                E.message.isFocus = 0;
                editorClearMessage();
                return;
            case CTRL_KEY('q'): 
                free(filename);
                E.message.isFocus = 0;
                editorClearMessage();
                write(STDOUT_FILENO, "\x1b[2J", 4);
                write(STDOUT_FILENO, "\x1b[H", 3);
                exit(0);
            default:
                if (isprint(c)) {
                    filename = (char *) realloc(filename, filenamesize + 2);
                    if (!filename) die("In function: %s\r\nAt line: %d\r\nrealloc", __func__, __LINE__);
                    filename[filenamesize] = c;
                    filenamesize++;
                    filename[filenamesize] = '\0';
                    editorAppendMessage((char *) &c, 1);

                    editorRefreshScreen();
                }
        }
    }

    editorSave(filename);
    if (!E.filename) E.filename = strdup(filename);
    free(filename);

    E.message.isFocus = 0;
}

/*** input ***/
void editorMoveCursor(int c) {
    switch (c) {
        case ARROW_UP: {
            if (E.cy == 0) {
                break;
            }

            E.cy--;
            const erow *row = &E.row[E.cy];
            E.cx = editorRowRxToCx(row, E.max_rx);
            E.rx = editorRowCxToRx(row, E.cx);

            if (E.cx > (int)row->size) {
                E.cx = row->size - 1;
                E.rx = row->rsize - 1;
            }
            break;
        }
        case ARROW_DOWN: {
            if (E.cy == E.numrows - 1) {
                break;
            }

            E.cy++;
            const erow *row = &E.row[E.cy];
            E.cx = editorRowRxToCx(row, E.max_rx);
            E.rx = editorRowCxToRx(row, E.cx);

            if (E.cx > (int)row->size) {
                E.cx = row->size - 1;
                E.rx = row->rsize - 1;
            }
            break;
        }
        case ARROW_RIGHT: {
            const erow *row = &E.row[E.cy];
            if (E.rx == (int)row->rsize && E.cy == E.numrows - 1)
                break;
            if (E.rx == (int)row->rsize) {
                E.cy++;
                E.cx = 0;
                E.rx = 0;
                E.max_rx = 0;
            } else if (row->chars[E.cx] == '\t') {
                E.rx += S.tabwidth - (E.rx % S.tabwidth);
                E.cx++;
                E.max_rx = E.rx;
            } else {
                E.cx++;
                E.rx++;
                E.max_rx++;
            }
            break;
        }
        case ARROW_LEFT: {
            const erow *curRow = &E.row[E.cy];
            const char *chars = curRow->chars;

            if (E.cx == 0 && E.cy == 0)
                break;
            if (E.cx == 0) {
                E.cy--;

                const erow *prevRow = &E.row[E.cy];

                E.cx = prevRow->size;
                E.rx = prevRow->rsize;
                E.max_rx = E.rx;
            } else if (chars[E.cx - 1] == '\t') {
                int spaceCount = 0; /* Number of spaces behind '\t' */
                int deltaRx = 0;
                int cx = 2;
                while (cx <= E.cx && chars[E.cx - cx] == ' ') {
                    cx++;
                    spaceCount++;
                }
                if (spaceCount == E.cx - 1 || chars[E.cx - cx] == '\t') {
                    /* Either all previous chars to '\t' are ' 's till first char of line
                    * or after all the continuous spaces behind '\t' there is another '\t'
                    */
                    int extraSpaces = spaceCount % S.tabwidth;
                    deltaRx = S.tabwidth - extraSpaces;
                } else {
                    const char *render = curRow->render;
                    int rx = cx;
                    while (render[E.rx - rx] != chars[E.cx - cx]) {
                        rx++;
                    }
                    deltaRx = rx - cx + 1;
                }
                E.rx -= deltaRx;
                E.cx--;
                E.max_rx = E.rx;

            } else {
                E.cx--;
                E.rx--;
                E.max_rx = E.rx;
            }
            break;
        }
        case PAGE_UP:
        case PAGE_DOWN: {
            // if (c == PAGE_UP) {
            //     E.cy = E.rowoff + S.scrolloff;
            // } else {
            //     E.cy = E.rowoff + E.screenrows - 1 - S.scrolloff;
            //     if (E.cy > E.numrows - 1)
            //         E.cy = E.numrows - 1;
            // }
            int temp = E.screenrows;
            while (temp--)
                editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            break;
        }
        case HOME_KEY:
            E.cx = 0;
            E.rx = 0;
            E.cy = 0;
            E.max_rx = 0;
            break;
        case END_KEY:
            E.cy = E.numrows - 1;
            editorMoveCursor(EOL);
            break;
        case EOL: {
            const erow *row = &E.row[E.cy];
            E.cx = row->size;
            E.rx = row->rsize;
            for (int i = 0; i < E.numrows; i++) {
                if (E.max_rx < (int) E.row[i].rsize) E.max_rx = E.row[i].rsize;
            }
            break;
        }
    }
}

void editorProcessKeyPress(void) {
    int c = editorReadKey();

    // TODO: undo time check
    // if time > threshold
    // commit action

    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        case CTRL_KEY('o'):
            editorSave(E.filename);
            break;
        case CTRL_KEY('w'):
            editorSaveAs();
            break;
        case CTRL_KEY('u'):
            H.undo();
            E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
            break;
        case CTRL_KEY('r'):
            H.redo();
            E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
        case PAGE_UP:
        case PAGE_DOWN:
        case HOME_KEY:
        case END_KEY:
            // TODO: commit action
            H.commit();

            editorMoveCursor(c);
            break;
        case '\r':
            // TODO: commit action
            // set another action
            // commit action
            H.record(INSERT_LINE_BEF, "\n", 1, E.cx, E.cy);

            editorRowInsertBefore(E.cy, E.cx);
            break;
        case DELETE_KEY: {
            char charRemoved;
            int length = -1; // Since it is Delete key
            // TODO: if action type change
            // commit action
            if (E.cx == (int) E.row[E.cy].size && E.cy < E.numrows) {
                charRemoved = '\n';
                H.record(REMOVE_LINE_AFT, &charRemoved, length, E.cx, E.cy);
            } else {
                charRemoved = E.row[E.cy].chars[E.cx];
                H.record(REMOVE_CHAR_AFT, &charRemoved, length, E.cx, E.cy);
            }

            editorRemoveChars(E.cy, E.cx, -1);
            break;
        }
        case BACKSPACE: {
            char charRemoved[2] = "\0";
            int length = 1;
            // TODO: if action type change
            // commit action
            // bit extra to do here
            if (E.cx == 0) {
                if (E.cy > 1) {
                    charRemoved[0] = '\n';

                    // action x-position is given as previous line's last char pos, 
                    // as that is where new line character need to be inserted, 
                    // not in 0th position of next line while undoing
                    // otherwise that context will be lost!
                    H.record(REMOVE_LINE_BEF, charRemoved, length, E.row[E.cy - 1].size, E.cy); 
                }
            } else {
                charRemoved[0] = E.row[E.cy].chars[E.cx - 1];
                H.record(REMOVE_CHAR_BEF, charRemoved, length, E.cx, E.cy);
            }

            editorRemoveChars(E.cy, E.cx, 1);
            break;
        }
        default:
            if (isprint(c) || c == '\t') {
                // TODO: if action type change
                // commit action
                H.record(INSERT_CHAR_BEF, (char *) &c, 1, E.cx, E.cy);

                editorRowInsertCharBefore(E.cy, E.cx, (char *) &c, 1);
            }
    }
}

struct editorSetting S;
struct editorConfig E;
struct History H;

/*** init ***/
void initEditor(void) {
    /* Editor Configs */
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.row = NULL;
    E.numrows = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.max_rx = 0;
    E.filename = NULL;

    E.message = (struct editorMsg) { NULL, 0, 0, 0, 0, 0 };

    /* Editor Settings */
    S.scrolloff = 8;
    S.tabwidth = 4;
    S.maxFileNameSize = 40;
    S.maxMsgSize = 80;
    S.maxHistory = 10;
    S.maxActionTime = 10;

    /* Editor History */
    historyInit();

    enableRawMode();
    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("In function: %s\r\nAt line: %d", __func__, __LINE__);
    E.screenrows -= 2;
}

int main(int argc, char *argv[]) {
    initEditor();
    if (argc >= 2) {
        editorOpen(argv[1]);
    } else {
        editorOpenEmpty();
    }

    editorSetMessage("Help:\tCtrl+Q=Quit\tCtrl+O=Save\tCtrl+W=Save As");

    while (1) {
        editorRefreshScreen();
        editorProcessKeyPress();
    }

    return 0;
}
