# Libraries included, and its purpose in this editor

## Contents

- [[#ctype.h]]
- [[#errno.h]]
- [[#stdio.h]]
- [[#stdlib.h]]
- [[#string.h]]
- [[#sys/ioctl.h]]
- [[#sys/types.h]]
- [[#termios.h]]
- [[#unistd.h]]

## ctype.h 

> [!info]
> This header declares a set of functions to classify and transform individual characters.

| Methods/Types Used | Description                         |
| ------------------ | ----------------------------------- |
| `iscntrl()`        | Checks if a character is control    |
|                    | character (Non printable character) |


## errno.h

> [!info] > `errno` is set to zero at program startup, and any function of the standard C library
> can modify its value to some value different from zero, generally to signal specific
> categories of error (no library function sets its value back to zero once changed).

## stdio.h

> [!info]
> This library uses what are called streams to operate with physical devices such as
> keyboards, printers, terminals or with any other type of files supported by the system.

| Methods/Types Used | Description                                                           |
| ------------------ | --------------------------------------------------------------------- |
| `perror()`         | Prints error code stored in global `errno` and passed message with it |
|                    | character (Non printable character)                                   |
| `snprintf()`       | Write formatted output to sized buffer                                |
| `printf()`         | print formatted output to std out                                     |
| `getline()`        | Reads an entire line from stream                                      |
|                    |                                                                       |
| `size_t` (T)       | Unsigned integral type                                                |
| `FILE` (T)         | Object containing information to control a stream                     |


## stdlib.h

> [!info]
> This header is a part of program support utilities library, in particular,
> it provides functions for program termination, memory management,
> string conversions, random numbers generation.

| Methods/Types Used | Description                         |
| ------------------ | ----------------------------------- |
| `malloc()`         | Allocate memory block               |
| `realloc()`        | Rellocate memory block              |
| `free()`           | Deallocate memory block             |
| `atexit()`         | Set function to be executed on exit |
| `exit()`           | Exit calling process                |


## string.h

> [!info]
> This library provides methds that allows to manipulate strings

| Methods/Types Used | Description                                                    |
| ------------------ | -------------------------------------------------------------- |
| `memcpy()`         | Copy block of memory                                           |
| `strdup()`         | Returns a pointer to a null-terminated byte string, which is a |
|                    | duplicate of the string pointed to by src string               |


## sys/ioctl.h

> [!info]
> The ioctl() system call manipulates the underlying device parameters of special files.
> In particular, many operating characteristics of character special files (e.g., terminals)
> may be controlled with ioctl() operations.

| Methods/Types Used   | Description                       |
| -------------------- | --------------------------------- |
| `ioctl()`            | To get details of terminal window |
|                      |                                   |
| `struct winsize` (T) | Has the window length and width   |


## sys/types.h

> [!info]
> Defines some of the types

| Methods/Types Used | Description                                                |
| ------------------ | ---------------------------------------------------------- |
| `ssize_t` (T)      | Singned integer type                                       |
|                    | Capable of storing values in the range \[-1, {SSIZE_MAX}\] |


## termios.h

> [!info]
> This header shall contain the definitions used by the terminal I/O interfaces

| Methods/Definitions Used | Description                                                         |
| ------------------------ | ------------------------------------------------------------------- |
| `tcgetattr()`            | Gets terminal attributes                                            |
| `tcsetattr()`            | Sets terminal attributes                                            |
|                          |                                                                     |
| ==Input Modes==          |                                                                     |
| `ICRNL` (D)              | Map CR to NL on input                                               |
| `IXON` (D)               | Enable start/stop output control                                    |
| `BRKINT` (D)             | Signal interrupt on break                                           |
| `INPCK` (D)              | Enable input parity check                                           |
| `ISTRIP` (D)             | Strip character                                                     |
|                          |                                                                     |
| ==Output Modes==         |                                                                     |
| `OPOST` (D)              | Post-process output                                                 |
| `CS8` (D)                | Character size mask                                                 |
|                          |                                                                     |
| ==Local Modes==          |                                                                     |
| `ECHO` (D)               | Enable echo                                                         |
| `ICANON` (D)             | Canonical input (erase and kill processing)                         |
| `ISIG` (D)               | Enable signals                                                      |
| `IEXTEN` (D)             | Enable extended input character processing                          |
|                          |                                                                     |
| ==Line Control==         |                                                                     |
| `TCSAFLUSH` (D)          | Change attributes when output has drained; also flush pending input |
|                          |                                                                     |
| ==Subscripts for array== |                                                                     |
| `VTIME` (D)              | Timeout in deciseconds for noncanonical read (TIME)                 |
| `VMIN` (D)               | Minimum number of characters for noncanonical read (MIN)            |


## unistd.h

> [!info]
> The <unistd.h> header defines miscellaneous symbolic constants and types,
> and declares miscellaneous functions

| Methods Used | Description                                                  |
| ------------ | ------------------------------------------------------------ |
| `read()`     | Attempts to read up to `count` bytes from file descriptor    |
|              | `fd` into the buffer starting at `buf`                       |
|              | `fd` can be `STDIN_FILENO` to read from stdin                |
| `write()`    | Writes up to `count` bytes from the buffer starting at `buf` |
|              | to the file referred to by the file descriptor `fd`          |
|              | `fd` can be `STDOUT_FILENO` to write into stdout             |

