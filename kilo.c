/*** includes ***/
#define _DEFAULT_SOURCE
#define _GNU_SOURCE
#define _BSD_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)

#define KILO_VERSION "0.0.1"

enum editorKey {
  ARROW_UP = 1000,
  ARROW_DOWN,
  ARROW_LEFT,
  ARROW_RIGHT,
  PAGE_UP,
  PAGE_DOWN,
  HOME_KEY,
  END_KEY,
  DELETE_KEY
};

/*** data ***/
typedef struct erow {
  size_t size;
  char *chars;
} erow;

struct editorConfig {
  int cx, cy;
  int screenrows;
  int screencols;
  erow *row;
  int rowoff;
  int numrows;
  struct termios orig_termios;
};

struct editorConfig E;

/*** terminal ***/
void die(char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    die("tcsetattr");
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
    die("tcgetattr");
  atexit(disableRawMode);

  struct termios raw = E.orig_termios;
  raw.c_iflag &= ~(ICRNL | IXON | BRKINT | INPCK | ISTRIP);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  raw.c_cc[VTIME] = 1;
  raw.c_cc[VMIN] = 0;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    die("tcsetattr");
}

int editorReadKey() {
  char c;
  int nread;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && nread != EAGAIN)
      die("read");
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

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) // Writes current cursor position
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
void editorRowAppend(char *s, size_t len) {
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

  int at = E.numrows;
  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';
  E.numrows++;
}

/*** file i/o ***/
void editorOpen(char *filename) {
  FILE *fp = fopen(filename, "r");
  if (!fp)
    die("fopen");

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
  char *new = realloc(ab->b, ab->len + len);

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

void editorDrawRows(struct abuf *ab) {
  for (int y = 0; y < E.screenrows; y++) {
    if (y >= E.numrows) {
      if (y == 2 * E.screenrows / 3 && E.numrows == 0) {
        welcome(ab);
      } else
        abAppend(ab, "~", 1);
    } else {
      int currow;
      int len = E.row[currow].size;
      if (E.screencols < len)
        len = E.screencols;
      abAppend(ab, E.row[currow].chars, len);
    }

    abAppend(ab, "\x1b[K", 3); // Clear line to the right of cursor
    if (y < E.screenrows - 1)
      abAppend(ab, "\r\n", 2);
  }
}

void editorRefreshScreen() {
  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6); // Hide cursor
  abAppend(&ab, "\x1b[H", 3);    // Move cursor to top-left

  editorDrawRows(&ab);

  char cursorpos[15];
  size_t cursorpos_len =
      snprintf(cursorpos, sizeof(cursorpos), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
  abAppend(&ab, cursorpos,
           cursorpos_len); // Move cursor position back to its actual position
  abAppend(&ab, "\x1b[?25h", 6); // Unhide the cursor

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

/*** input ***/
void editorMoveCursor(int c) {
  switch (c) {
  case ARROW_UP:
    if (E.cy > 0)
      E.cy--;
    if (E.rowoff > 0)
      E.rowoff--;
    break;
  case ARROW_DOWN:
    if (E.cy < E.screenrows - 1)
      E.cy++;
    if (E.rowoff < E.numrows - 1)
      E.rowoff++;
    break;
  case ARROW_RIGHT:
    if (E.cx < E.screencols - 1)
      E.cx++;
    break;
  case ARROW_LEFT:
    if (E.cx > 0)
      E.cx--;
    break;
  case PAGE_UP:
  case PAGE_DOWN: {
    int temp = E.screenrows;
    while (--temp)
      editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
    E.cy = E.screenrows / 2;
    E.rowoff -= E.screenrows / 2;
    break;
  }
  case HOME_KEY:
    E.cx = 0;
    break;
  case END_KEY:
    E.cx = E.screencols - 1;
    break;
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
  case DELETE_KEY:

  default:
    printf("%d\r\n", c);
  }
}

/*** init ***/
void initEditor() {
  E.cx = 0;
  E.cy = 0;
  E.row = NULL;
  E.numrows = 0;
  E.rowoff = 0;
  enableRawMode();
  if (getWindowSize(&E.screenrows, &E.screencols) == -1)
    die("getWindowSize");
}

int main(int argc, char *argv[]) {
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  while (1) {
    editorRefreshScreen();
    editorProcessKeyPress();
  }

  return 0;
}
