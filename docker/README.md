# TSI Docker Testing Environment

Docker containers for testing TSI installation on various minimal system configurations.

## Quick Start

### Run All Tests

```bash
cd docker
./run-tests.sh
```

This will test TSI installation on all configured minimal system scenarios.

### Test Individual Scenarios

```bash
cd docker

# Build and run a specific test
docker-compose build alpine-c-only
docker-compose run --rm alpine-c-only /bin/sh /root/tsi-source/docker/test-install.sh

# Or enter the container interactively
docker-compose run --rm alpine-c-only /bin/sh
```

## Test Scenarios

### Rust Version Tests

1. **alpine-minimal**: Absolutely minimal system
   - No C compiler
   - No build tools
   - No package manager
   - Tests: Should fail gracefully with helpful error (or use pre-built binary if available)

2. **alpine-c-only**: C compiler only (for building packages)
   - gcc, make available
   - Rust toolchain or pre-built TSI binary
   - Tests: Should run TSI binary successfully
   - Tests: Should run basic CLI commands (tsi --help, tsi list, tsi update, tsi doctor)

3. **ubuntu-minimal**: Minimal Ubuntu system
   - No C compiler
   - No build tools
   - No package manager
   - Tests: Should fail gracefully (or use pre-built binary)

## Manual Testing

### Enter a Container

```bash
cd docker
docker-compose run --rm alpine-c-only /bin/sh
```

Inside the container:

```sh
# Check available tools
which gcc
which make

# If TSI is pre-built or built from Rust source:
tsi --help
tsi list
tsi update
tsi doctor
```

### Test Bootstrap Install

```bash
docker-compose run --rm alpine-c-only /bin/sh -c "
cd /root/tsi-source
./tsi-bootstrap.sh
"
```

The bootstrap script will try to download a pre-built binary first, then fall back to `cargo build --release` if Rust is available.

## Container Details

### Minimal Containers

The minimal containers have:
- Package managers removed (simulating minimal systems)
- Only essential POSIX tools
- No C compiler, no build tools

### C-Only Container

The C-only container has:
- gcc/g++ compiler
- make
- wget/curl (for downloading sources)
- tar/gzip
- May include Rust toolchain for building TSI from source

## Test Script

The `test-install.sh` script:
1. Shows system information
2. Lists available tools
3. Builds or downloads TSI
4. Verifies TSI installation
5. Tests TSI commands: `--help`, `--version`, `list`, `update`, `info`, `doctor`, `search`

## Continuous Integration

TSI includes CI/CD configurations for automated testing:

### GitHub Actions

Located in `.github/workflows/test.yml`:
- Tests Rust build and functionality
- Runs `cargo build`, `cargo test`, `cargo clippy`, `cargo fmt`
- Builds for multiple platforms (Linux, macOS, Windows)

### GitLab CI

Located in `.gitlab-ci.yml`:
- Similar test structure
- Rust build and test

### Running in CI

```yaml
# Example GitHub Actions
- name: Test TSI
  run: |
    cargo build --release
    cargo test
```

## Troubleshooting

### Container Build Fails

```bash
# Clean and rebuild
docker-compose down
docker-compose build --no-cache
```

### Test Fails

Check the log file:
```bash
cat /tmp/tsi-test-<scenario>.log
```

### Permission Issues

```bash
chmod +x docker/run-tests.sh
chmod +x docker/test-install.sh
```

## Adding New Test Scenarios

1. Create a new Dockerfile in `docker/`:
   ```dockerfile
   FROM <base-image>
   # Install specific tools (Rust, or use pre-built binary)
   COPY . /root/tsi-source/
   ```

2. Add to `docker-compose.yml`:
   ```yaml
   new-scenario:
     build:
       context: ..
       dockerfile: docker/Dockerfile.new-scenario
   ```

3. Add to test scenarios in `run-tests.sh`
