# Project Settings
debug ?= 1
SRC_DIR := src
INCLUDE_DIR := include
BIN_DIR := bin
EXE := kilo

# source files
SRCS := $(wildcard $(SRC_DIR)/*.c)

# flags
CFLAGS := -std=c99 -Wall -Wextra -pedantic -I$(INCLUDE_DIR)
ifeq ($(debug), 1)
	CFLAGS := $(CFLAGS) -g -O0
else
	CFLAGS := $(CFLAGS) -Oz
endif

# build
$(EXE): $(SRCS)
	$(CC) $(SRCS) $(CFLAGS) -o $(BIN_DIR)/$(EXE)
