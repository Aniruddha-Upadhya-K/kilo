# Kilo: Terminal Based Text Editor

This is Kilo, a memory safe (atleast as far as I tested) nano like terminal based text editor

https://github.com/user-attachments/assets/9addb366-5d16-4e4a-bafb-fed66dcc85dc

## Features
- Opening/editing/creating files (ofc)
- Stack based undo/redo capabilities
- Supports ASCII characters
- Scrolling offset (cursor does not go till bottom of screen while scrolling)
- Supported keys
    - Navigation with arrow, page up/page down, home, end keys
    - Backspace/Delete keys
    - TAB:      writes a '\t' character, that renders 4 spaces
    - ENTER:    write a new line
    - Ctrl-O:   save the file (why not Ctrl-S? well I use tmux with Ctrl-S as prefix)
    - Ctrl-W:   save as, and you have to enter file name 
    - ESC:      exit save as menu
    - Ctrl-U:   undo
    - Ctrl-R:   redo
    - Ctrl-Q:   quit

## Tech Stack
1. c99

## Requirements

- Linux/Mac machine (haven't tested in windows)
- gcc (compiler used)
- make
- bear and clngd (only for development)

## Installation

clone and cd to the repo

``` bash
git clone https://github.com/Aniruddha-Upadhya-K/kilo.git && cd kilo
```

create ./bin for the executable

``` bash
mkdir bin
```

build with make

``` bash
make
```

usage

``` bash
./bin/kilo                  # opens a blank buffer

./bin/kilo filename.txt     # opens the contents of the file if it exists, else creates one on save
```

## Development

for lsp support in neovim (clangd), create compile_commands.json file using `bear`

``` bash
bear -- make
```
