# Makefile for the store project

CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wshadow -Wpedantic -O2 -g
LDFLAGS :=
LDLIBS :=

SRC := store.c tomlc99/toml.c
OBJ := $(SRC:.c=.o)
TARGET := store

.PHONY: all clean fmt

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c store.h
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

-include $(OBJ:.o=.d)

clean:
	rm -f $(OBJ) $(OBJ:.o=.d) $(TARGET)

fmt:
	command -v clang-format >/dev/null 2>&1 && clang-format -i store.c store.h || true