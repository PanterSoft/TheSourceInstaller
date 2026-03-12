# Top-level Makefile for TSI (Rust)

PREFIX ?= $(HOME)/.tsi

.PHONY: help test build clean install lint fmt

help:
	@echo "TSI Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  build         - Build TSI (release)"
	@echo "  test          - Run tests"
	@echo "  lint          - Run clippy"
	@echo "  fmt           - Check code formatting"
	@echo "  clean         - Clean build artifacts"
	@echo "  install       - Install TSI to PREFIX (default: ~/.tsi)"
	@echo ""
	@echo "Usage:"
	@echo "  make build"
	@echo "  make install PREFIX=/opt/tsi"

build:
	@echo "Building TSI..."
	cargo build --release

test:
	@echo "Running tests..."
	cargo test

lint:
	@echo "Running clippy..."
	cargo clippy -- -D warnings

fmt:
	@echo "Checking formatting..."
	cargo fmt --check

clean:
	@echo "Cleaning build artifacts..."
	cargo clean
	rm -rf docker/bin artifacts 2>/dev/null || true

install: build
	@echo "Installing TSI to $(PREFIX)..."
	@mkdir -p $(PREFIX)/bin
	@cp target/release/tsi $(PREFIX)/bin/tsi 2>/dev/null || cp target/release/tsi.exe $(PREFIX)/bin/tsi.exe
	@chmod +x $(PREFIX)/bin/tsi 2>/dev/null || true
	@mkdir -p $(PREFIX)/share/completions
	@test -f completions/tsi.bash && cp completions/tsi.bash $(PREFIX)/share/completions/ || true
	@test -f completions/tsi.zsh && cp completions/tsi.zsh $(PREFIX)/share/completions/ || true
	@echo "Installed. Add to PATH: export PATH=\"$(PREFIX)/bin:\$$PATH\""
