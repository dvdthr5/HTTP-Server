CC ?= clang
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c11
LDFLAGS = -pthread

SRC_DIR = src
OBJ_DIR = obj
TARGET = httpserver

# Detect whether we have a src/ subdirectory or not
ifneq ($(wildcard $(SRC_DIR)),)
    SRCDIR := $(SRC_DIR)
    CFLAGS += -Isrc
else
    SRCDIR := .
endif

# All .c files in the chosen source directory
SRCS := $(wildcard $(SRCDIR)/*.c)

# Object files live in obj/, names derived from SRCDIR/*.c
OBJS := $(patsubst $(SRCDIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: $(SRCDIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)