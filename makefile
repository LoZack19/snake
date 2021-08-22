SH	= /bin/bash

CC	= gcc
CFLAGS	= -Wall -Wextra -std=gnu11
CFLAGS	+= -O3 -march=native -Werror
#CFLAGS	+= -g3 -fanalyzer
#CFLAGS	+= -ggdb -fno-omit-frame-pointer -fsanitize=address
CLIBS	= -lncurses

OBJ	= main.o
TARGET	= main

.PHONY=clean, debug

%.o: %.c
	$(CC) $(CFLAGS) -c $<

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(CLIBS) $^ -o $@

clean:
	@rm *.o $(TARGET)
