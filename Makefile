CC = gcc
CFLAGS = -Wall -Wextra -std=c11
TARGET = ocsh

SOURCES = code/main.c \
          code/execution.c \
          code/parser.c \
          code/pipes.c \
          code/jobs.c

OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

code/%.o: code/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run