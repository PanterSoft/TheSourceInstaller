# Makefile for TSI C/C++ implementation

CC ?= gcc
CFLAGS = -Wall -Wextra -O2 -std=c11 -D_POSIX_C_SOURCE=200809L

# Detect OS for static linking (only works on Linux)
UNAME_S := $(shell uname -s 2>/dev/null || echo "Unknown")
ifeq ($(UNAME_S),Linux)
    LDFLAGS = -static
else
    LDFLAGS =
endif

# Directories
SRCDIR = .
OBJDIR = build
BINDIR = bin

# Source files (exclude test files)
C_SOURCES = $(filter-out %_test.c, $(wildcard $(SRCDIR)/*.c))
OBJECTS = $(C_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Target
TARGET = $(BINDIR)/tsi

# Default target
all: $(TARGET)

# Create directories
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# Compile C files
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link
$(TARGET): $(OBJECTS) | $(BINDIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

# Clean
clean:
	rm -rf $(OBJDIR) $(BINDIR)

# Install
install: $(TARGET)
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(TARGET) $(DESTDIR)/usr/local/bin/

.PHONY: all clean install

