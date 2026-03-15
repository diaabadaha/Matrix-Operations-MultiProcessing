# Compile flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -fopenmp -Iinclude
LDFLAGS = -fopenmp -lm

# Source files
SRCS = src/parent.c src/menu.c src/child.c src/matrix_ops.c src/pipes.c src/signals.c src/functions.c src/pool_child.c src/main.c src/pool_manager.c
# object files
OBJS = $(SRCS:.c=.o)

# executable name
EXEC = run_project

# build rules
all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(EXEC)

run: $(EXEC)
	./$(EXEC)