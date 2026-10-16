CC      = gcc
CFLAGS  = -Wall -Wextra -g -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
# -Wall -Wextra : turn on most warnings (catches common bugs)
# -g            : include debug info (so you can use gdb if something crashes)

# Source and object files
SRC_DIR = src
SRCS    = $(SRC_DIR)/main.c $(SRC_DIR)/parser.c $(SRC_DIR)/executor.c $(SRC_DIR)/builtins.c $(SRC_DIR)/jobs.c $(SRC_DIR)/signals.c $(SRC_DIR)/history.c
OBJS    = $(SRCS:.c=.o)

# Output binary name
TARGET  = mini-shell

# Default rule: build the shell
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Pattern rule: compile any .c file into a .o file
$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up compiled files
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
