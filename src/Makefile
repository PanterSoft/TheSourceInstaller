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

# Installation prefix (defaults to ~/.tsi for isolated installation)
PREFIX ?= $(HOME)/.tsi

# Install to local directory (isolated, like bootstrap script)
install: $(TARGET)
	@echo "Installing TSI to $(PREFIX)..."
	@mkdir -p $(PREFIX)/bin
	@install -m 755 $(TARGET) $(PREFIX)/bin/tsi
	@echo "✓ Installed binary: $(PREFIX)/bin/tsi"
	@if [ -d "../completions" ]; then \
		mkdir -p $(PREFIX)/share/completions; \
		if [ -f "../completions/tsi.bash" ]; then \
			install -m 644 ../completions/tsi.bash $(PREFIX)/share/completions/tsi.bash; \
			echo "✓ Installed bash completion"; \
		fi; \
		if [ -f "../completions/tsi.zsh" ]; then \
			install -m 644 ../completions/tsi.zsh $(PREFIX)/share/completions/tsi.zsh; \
			echo "✓ Installed zsh completion"; \
		fi; \
	fi
	@echo ""
	@echo "TSI installed successfully!"
	@echo "Add to your PATH:"
	@echo "  export PATH=\"$(PREFIX)/bin:\$$PATH\""

.PHONY: all clean install

