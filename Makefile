CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -MMD -MP
TARGET = ocsh

SOURCES = code/main.c \
          code/execution.c \
          code/parser.c \
          code/pipes.c \
          code/jobs.c \
          code/script.c

OBJECTS = $(SOURCES:.c=.o)
DEPENDS = $(OBJECTS:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

code/%.o: code/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(DEPENDS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

-include $(DEPENDS)

.PHONY: all clean run
