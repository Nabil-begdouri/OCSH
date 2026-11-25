CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -MMD -MP
LDLIBS = -lreadline

TARGET = ocsh

SOURCES = code/main.c \
          code/execution.c \
          code/parser.c \
          code/variables.c \
          code/pipes.c \
          code/jobs.c \
          code/script.c \
          code/builtins.c \
          code/redirections.c \
          code/history.c \
          code/command_substitution.c \
          code/completion.c \
          code/ocshrc.c

OBJECTS = $(SOURCES:.c=.o)
DEPENDS = $(OBJECTS:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDLIBS)

code/%.o: code/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(DEPENDS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

-include $(DEPENDS)

.PHONY: all clean run