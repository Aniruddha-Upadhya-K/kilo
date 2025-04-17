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

/*** data ***/
typedef struct erow {
    size_t size;
    char *chars;
    size_t rsize;
    char *render;
} erow;

struct editorSetting {
    int scrolloff;
    int tabwidth;
    int maxFileNameSize;
};

struct editorSetting S;

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
    int numcols;
    int max_rx;
    struct termios orig_termios;
    char *filename;
    char statusmsg[80];
    time_t statusmsg_time;
};

struct editorConfig E;

/*** terminal ***/
void die(char *s, ...) {
    va_list ap;
    char err[100];
    va_start(ap, s);
    vsnprintf(err, sizeof(err), s, ap);
    va_end(ap);

    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(err);
    exit(1);
}

void disableRawMode() {
    for (int i=0; i < E.numrows; i++) {
        free(E.row[i].chars);
        free(E.row[i].render);
    }
    free(E.row);
    free(E.filename);

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("In function: %s\r\nAt line: %d\r\ntcsetattr", __func__, __LINE__);
}

void enableRawMode() {
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

int editorReadKey() {
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

    while (i < sizeof(buf)) {
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
int editorRowCxToRx(erow *row, int cx) {
    int rx = 0;
    for (int i = 0; i < cx; i++) {
        if (row->chars[i] == '\t')
            rx += S.tabwidth - (rx % S.tabwidth);
        else
            rx++;
    }

    return rx;
}

int editorRowRxToCx(erow *row, int rx) {
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
    free(row->render);

    int tabcount = 0;
    for (size_t i = 0; i < row->size; i++)
        if (row->chars[i] == '\t')
            tabcount++;

    row->render = malloc(row->size + tabcount * (S.tabwidth - 1) + 1);

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

void editorRowAppend(char *s, size_t len) {
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

    if (E.numcols < (int)E.row[at].rsize)
        E.numcols = len + 1;
}

void editorRowInsertChar(erow *row, int cat, int rat, int c) {
    if (cat < 0 || cat > (int) row->size) cat = row->size;
    if (rat < 0 || rat > (int) row->rsize) rat = row->rsize;

    int clen = 1, rlen = 1;
    if (c == '\t') 
        rlen = S.tabwidth - (rat % S.tabwidth);

    row->chars = (char *) realloc(row->chars, row->size + clen + 1);
    if (!E.row) die("In function: %s\r\nAt line: %d\r\nrealloc", __func__, __LINE__);
    memmove(row->chars + cat + clen, row->chars + cat, row->size - cat + 1);
    row->chars[cat] = c;

    row->size += clen;

    editorUpdateRow(row);

    E.cx += clen;
    E.rx += rlen;
}

void editorRowInsert(int curline, int cat, int rat) {
    if (curline < 0 || curline >= E.numrows) curline = E.numrows - 1;

    E.row = (erow *) realloc(E.row, sizeof(erow) * (E.numrows + 1));
    if (!E.row) die("In function: %s\r\nAt line: %d\r\nrealloc", __func__, __LINE__);

    erow *currow = E.row + curline;
    erow *nextrow = E.row + curline + 1;

    if (cat < 0 || cat > (int) currow->size) cat = currow->size;
    if (rat < 0 || rat > (int) currow->rsize) rat = currow->rsize;

    int size = currow->size - cat;

    if (curline < E.numrows - 1)
        memmove(nextrow + 1, nextrow, sizeof(erow) * (E.numrows - curline - 1));

    nextrow->chars = (char *) malloc(size + 1);
    if (!nextrow->chars) die("In function: %s\r\nAt line: %d\r\nmalloc", __func__, __LINE__);
    nextrow->render = (char *) malloc(1);

    memcpy(nextrow->chars, &currow->chars[cat], currow->size + 1 - cat);
    currow->chars[cat] = '\0';

    currow->chars = (char *) realloc(currow->chars, cat + 1);
    if (!currow->chars) die("In function: %s\r\nAt line: %d\r\nrealloc", __func__, __LINE__);

    E.numrows++;
    E.cy++;
    E.max_rx = 0;
    E.cx = 0;
    E.rx = 0;

    currow->size = cat;
    currow->rsize = rat;
    nextrow->size = size;
    editorUpdateRow(nextrow);
}

/*
* from one character before the position 'cat' in current row to a total of 'clen' characters will be removed from 'chars' array(s)
* from one character before the position 'rat' in current row to a total of 'rlen' characters will be removed from 'render' array(s)
*/
void editorRemoveChars(int cat, int rat, int curline, int clen, int rlen) {
    if (curline > E.numrows - 1 && curline < 0) curline = E.numrows - 1;
    erow *currow = &E.row[curline];
    if (cat > (int) currow->size && cat < 0) cat = currow->size;
    if (rat > (int) currow->rsize && rat < 0) rat = currow->rsize;


    if (clen > cat && rlen > rat && curline == 0);
    else if (clen > cat && rlen > rat) {
        clen -= cat + 1;
        rlen -= rat + 1;

        erow *prevrow = &E.row[curline - 1];

        int prevRowSize = prevrow->size;
        int prevRowRsize = prevrow->rsize;
        int curRowSize = currow->size;
        int curRowRsize = currow->rsize;

        prevrow->chars = (char *) realloc(prevrow->chars, prevRowSize + curRowSize + 1);
        if (!prevrow->chars) die("In function: %s\r\nAt line: %d\r\nrealloc", __func__, __LINE__);
        prevrow->render = (char *) realloc(prevrow->render, prevRowRsize + curRowRsize + 1);
        if (!prevrow->render) die("In function: %s\r\nAt line: %d\r\nrealloc", __func__, __LINE__);

        memmove(&prevrow->chars[prevRowSize], currow->chars, curRowSize + 1);
        memmove(&prevrow->render[prevRowRsize], currow->render, curRowRsize + 1);

        prevrow->size += curRowSize;
        prevrow->rsize += curRowRsize;
        editorUpdateRow(prevrow);

        if (curline + 1 < E.numrows) {
            memmove(E.row + curline, E.row + curline + 1, sizeof(erow) * (E.numrows - curline - 2));

            erow *lastrow = &E.row[E.numrows - 1];
            char *chars = strdup(lastrow->chars);
            char *render = strdup(lastrow->render);

            E.row[E.numrows - 2] = (erow) {lastrow->size, chars, lastrow->rsize, render};
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

        if (clen > 0 && rlen > 0) editorRemoveChars(E.cx, E.rx, E.cy, clen, rlen);
    } else {
        memmove(currow->chars + cat - clen, currow->chars + cat, currow->size - cat + 1);
        memmove(currow->render + rat - rlen, currow->render + rat, currow->rsize - rat + 1);

        currow->size -= clen;
        currow->rsize -= rlen;

        currow->chars = (char *) realloc(currow->chars, currow->size + 1);
        if (!E.row) die("In function: %s\r\nAt line: %d\r\nrealloc", __func__, __LINE__);

        editorUpdateRow(currow);

        E.cx -= clen;
        E.rx -= rlen;
        E.max_rx = E.rx;
    }
}

/*** file i/o ***/
void editorOpenEmpty() {
    int linelen = 0;
    char line;
    editorRowAppend(&line, linelen);
}

void editorOpen(char *filename) {
    free(E.filename);
    E.filename = strdup(filename);
    if (!E.filename)
        die("In function: %s\r\nAt line: %d\r\nNo file name given", __func__, __LINE__);

    FILE *fp = fopen(E.filename, "r");
    if (!fp)
        die("In function: %s\r\nAt line: %d\r\nNo file exists with the name %s", __func__, __LINE__, E.filename);

    ssize_t linelen = 0;
    size_t linecap = 0;
    char *line = NULL;

    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        if (linelen != -1) {
            while (linelen > 0 &&
                (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) {
                linelen--;
            }
            editorRowAppend(line, linelen);
        }
    }
    free(line);
    fclose(fp);
}

/*** append buffer ***/
struct abuf {
    char *b;
    size_t len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, char *s, size_t len) {
    char *new = (char *) realloc(ab->b, ab->len + len);
    if (!E.row) die("In function: %s\r\nAt line: %d\r\nrealloc", __func__, __LINE__);

    if (!new)
        return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab) { free(ab->b); }

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

void editorScroll() {
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
            if (y == 2 * E.screenrows / 3 && E.numrows == 1 && E.numcols < 1) {
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

void editorSetStatusMessage(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}

void editorDrawMessageBar(struct abuf *ab) {
    abAppend(ab, "\x1b[K", 3);
    int msglen = strlen(E.statusmsg);
    if (msglen > E.screencols) msglen = E.screencols;
    if (msglen && time(NULL) - E.statusmsg_time < 5)
        abAppend(ab, E.statusmsg, msglen);
}

void editorRefreshScreen() {
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6); /* Hide cursor */
    abAppend(&ab, "\x1b[H", 3);    /* Move cursor to top-left */

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);

    char cursorpos[15];
    size_t cursorpos_len = snprintf(cursorpos, sizeof(cursorpos), "\x1b[%d;%dH",
                                    (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);
    abAppend(
        &ab, cursorpos,
        cursorpos_len); /* Move cursor position back to its actual position */
    abAppend(&ab, "\x1b[?25h", 6); /* Unhide the cursor */

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** input ***/
void getPreviousChar(int *dcx, int *drx, int *dcy, int *dmax_rx) {
    erow *curRow = &E.row[E.cy];
    char *chars = curRow->chars;
    char *render = curRow->render;

    if (E.cx == 0 && E.cy == 0);
    else if (E.cx == 0) {
        *dcy += 1;

        erow *prevRow = &E.row[E.cy - *dcy];

        *dcx = -prevRow->size;
        *drx = -prevRow->rsize;
        *dmax_rx = -prevRow->rsize + E.max_rx;
    } else if (chars[E.cx - 1] == '\t') {
        int spaceCount = 0; /* Number of spaces behind '\t' */
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
            *drx = S.tabwidth - extraSpaces;
        } else {
            int rx = cx;
            while (render[E.rx - rx] != chars[E.cx - cx]) {
                rx++;
            }
            *drx = rx - cx + 1;
        }
        *dcx += 1;
        *dmax_rx = -(E.rx - *drx) + E.max_rx;
    } else {
        *dcx += 1;
        *drx += 1;
        *dmax_rx = -(E.rx - *drx) + E.max_rx;
    }
}

void editorMoveCursor(int c) {
    switch (c) {
        case ARROW_UP: {
            if (E.cy == 0) {
                break;
            }

            E.cy--;
            erow *row = &E.row[E.cy];
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
            erow *row = &E.row[E.cy];
            E.cx = editorRowRxToCx(row, E.max_rx);
            E.rx = editorRowCxToRx(row, E.cx);

            if (E.cx > (int)row->size) {
                E.cx = row->size - 1;
                E.rx = row->rsize - 1;
            }
            break;
        }
        case ARROW_RIGHT: {
            erow *row = &E.row[E.cy];
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
            } else if (row->chars[E.cx] != '\t') {
                E.cx++;
                E.rx++;
                E.max_rx++;
            }
            break;
        }
        case ARROW_LEFT: {
            int drx = 0;
            int dcx = 0;
            int dcy = 0;
            int dmax_rx = 0;
            getPreviousChar(&dcx, &drx, &dcy, &dmax_rx);
            E.cx -= dcx;
            E.rx -= drx;
            E.cy -= dcy;
            E.max_rx -= dmax_rx;
            break;
        }
        case PAGE_UP:
        case PAGE_DOWN: {
            if (c == PAGE_UP) {
                E.cy = E.rowoff + S.scrolloff;
            } else {
                E.cy = E.rowoff + E.screenrows - 1 - S.scrolloff;
                if (E.cy > E.numrows - 1)
                    E.cy = E.numrows - 1;
            }
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
            struct erow *row = &E.row[E.cy];
            E.cx = row->size;
            E.rx = row->rsize;
            E.max_rx = E.numcols;
            break;
        }
    }
}

void editorProcessKeyPress() {
    int c = editorReadKey();

    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
        case PAGE_UP:
        case PAGE_DOWN:
        case HOME_KEY:
        case END_KEY:
            editorMoveCursor(c);
            break;
        case '\r':
            editorRowInsert(E.cy, E.cx, E.rx);
            break;
        case DELETE_KEY:
        case BACKSPACE: {
            if (E.cx > 0 && E.row[E.cy].chars[E.cx - 1] == '\t') {
                int drx = 0;
                int dcx = 0;
                int dcy = 0;
                int dmax_rx = 0;
                getPreviousChar(&dcx, &drx, &dcy, &dmax_rx);
                if (dcy) editorRemoveChars(E.cx, E.rx, E.cy, 1, 1);
                else editorRemoveChars(E.cx, E.rx, E.cy, 1, drx);
            } else {
                editorRemoveChars(E.cx, E.rx, E.cy, 1, 1);
            }
            break;
        }

        default:
            editorRowInsertChar(&E.row[E.cy], E.cx, E.rx, c);
    }
}

/*** init ***/
void initEditor() {
    /* Editor Configs */
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.row = NULL;
    E.numrows = 0;
    E.numcols = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.max_rx = 0;
    E.filename = NULL;
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;

    /* Editor Settings */
    S.scrolloff = 8;
    S.tabwidth = 4;
    S.maxFileNameSize = 20;

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

    editorSetStatusMessage("Help: Ctrl Q=Quit");

    while (1) {
        editorRefreshScreen();
        editorProcessKeyPress();
    }

    return 0;
}
