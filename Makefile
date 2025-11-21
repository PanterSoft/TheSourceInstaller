# Top-level Makefile for TSI

.PHONY: help test build clean install

help:
	@echo "TSI Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  test          - Run all tests"
	@echo "  build         - Build TSI"
	@echo "  clean         - Clean build artifacts"
	@echo "  install       - Install TSI"

test:
	@echo "Running tests..."
	cd docker && ./run-tests.sh

build:
	@echo "Building TSI..."
	cd src && make

clean:
	@echo "Cleaning build artifacts..."
	cd src && make clean
	rm -rf docker/bin artifacts

install: build
	@echo "Installing TSI..."
	cd src && make install

