# Top-level Makefile for TSI (Rust)

PREFIX ?= $(HOME)/.tsi
export PATH := $(HOME)/.cargo/bin:$(PATH)

.PHONY: help test check build clean install uninstall lint fmt deps dev run dev-packages

help:
	@echo "TSI Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  deps          - Install/verify dependencies (Rust toolchain, crates)"
	@echo "  dev           - Development build (fast, debug)"
	@echo "  dev-packages  - Sync package definitions from in-repo tsi-packages to PREFIX (requires submodule init)"
	@echo "  build         - Build TSI (release)"
	@echo "  run           - Run TSI (development)"
	@echo "  test          - Run tests"
	@echo "  check         - Everything CI checks: fmt + clippy + tests"
	@echo "  lint          - Run clippy"
	@echo "  fmt           - Check code formatting"
	@echo "  clean         - Clean build artifacts"
	@echo "  install       - Install TSI to PREFIX (default: ~/.tsi)"
	@echo "  uninstall     - Remove TSI from PREFIX (default: ~/.tsi)"
	@echo ""
	@echo "Usage:"
	@echo "  make deps     # First-time: install Rust, fetch crates"
	@echo "  make dev      # Development build"
	@echo "  make dev-packages   # After git submodule update --init tsi-packages"
	@echo "  make install PREFIX=/opt/tsi"
	@echo "  make uninstall PREFIX=/opt/tsi"

deps:
	@if command -v cargo >/dev/null 2>&1; then \
		echo "Fetching crate dependencies..."; \
		cargo fetch; \
	else \
		echo "Installing Rust toolchain..."; \
		curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y -q; \
		. "$$HOME/.cargo/env" && echo "Fetching crate dependencies..." && cargo fetch; \
	fi

dev: deps
	@echo "Building TSI (development)..."
	cargo build

run: dev
	cargo run -- $(ARGS)

# Sync package definitions from in-repo tsi-packages (requires submodule init)
dev-packages:
	@test -d tsi-packages/packages || (echo "Run: git submodule update --init tsi-packages"; exit 1)
	cargo run -- update --local tsi-packages/packages --prefix $(PREFIX)

build: deps
	@echo "Building TSI..."
	cargo build --release

test:
	@echo "Running tests..."
	cargo test

# Same gates as .github/workflows/rust-ci.yml — run this before pushing.
check: fmt lint test

lint:
	@echo "Running clippy..."
	cargo clippy --all-targets -- -D warnings

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

uninstall:
	@echo "Uninstalling TSI from $(PREFIX)..."
	@rm -f $(PREFIX)/bin/tsi $(PREFIX)/bin/tsi.exe
	@rm -rf $(PREFIX)/share/completions
	@-rmdir $(PREFIX)/share 2>/dev/null || true
	@-rmdir $(PREFIX)/bin 2>/dev/null || true
	@echo "TSI uninstalled. Remove $(PREFIX)/bin from your PATH if present."
