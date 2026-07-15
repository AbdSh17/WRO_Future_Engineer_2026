# ==============================================================================
# General Purpose Makefile
# ==============================================================================

# --- Project Settings ---------------------------------------------------------
TARGET   := program
CC       := gcc
CFLAGS   := -Wall -Wextra -g
LDFLAGS  :=
LIBS     :=

# --- Directories --------------------------------------------------------------
SRC_DIR  := src
OBJ_DIR  := obj
BIN_DIR  := bin

# --- Sources & Objects --------------------------------------------------------
SRCS     := $(wildcard $(SRC_DIR)/*.c)
OBJS     := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# ==============================================================================
# Rules
# ==============================================================================

# Default target
all: dirs $(BIN_DIR)/$(TARGET)

# Create required directories
dirs:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

# Link
$(BIN_DIR)/$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# Compile each .c → .o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# ==============================================================================
# Utility Targets
# ==============================================================================

run: all
	./$(BIN_DIR)/$(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

rebuild: clean all

# Show variables — useful for debugging the Makefile itself
info:
	@echo "TARGET  : $(TARGET)"
	@echo "SRCS    : $(SRCS)"
	@echo "OBJS    : $(OBJS)"
	@echo "CFLAGS  : $(CFLAGS)"
	@echo "LIBS    : $(LIBS)"

.PHONY: all dirs run clean rebuild info
