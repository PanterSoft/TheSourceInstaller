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
docker-compose run --rm alpine-c-only /bin/sh /root/tsi-source/docker/test-install-c.sh

# Or enter the container interactively
docker-compose run --rm alpine-c-only /bin/sh
```

## Test Scenarios

### C Version Tests

1. **alpine-minimal**: Absolutely minimal system
   - No C compiler
   - No build tools
   - No package manager
   - Tests: Should fail gracefully with helpful error

2. **alpine-c-only**: C compiler only
   - gcc, make available
   - No Python
   - Tests: Should build C version successfully
   - Tests: Should run basic CLI commands

3. **ubuntu-minimal**: Minimal Ubuntu system
   - No C compiler
   - No build tools
   - No package manager
   - Tests: Should fail gracefully

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

# Build TSI
cd /root/tsi-source/src
make

# Test TSI
./bin/tsi --help
```

### Test Bootstrap Install

```bash
docker-compose run --rm alpine-c-only /bin/sh -c "
cd /root/tsi-source
./bootstrap-c.sh
"
```

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
- No Python

## Test Script

The `test-install-c.sh` script:
1. Shows system information
2. Lists available tools
3. Builds TSI from source
4. Verifies TSI installation
5. Tests TSI command

## Continuous Integration

TSI includes CI/CD configurations for automated testing:

### GitHub Actions

Located in `.github/workflows/test.yml`:
- Tests C version build and functionality
- Builds C version for multiple architectures
- Lints code
- Runs on push, PR, and manual trigger

### GitLab CI

Located in `.gitlab-ci.yml`:
- Similar test structure
- Builds static binaries
- Uploads artifacts

### Running in CI

```yaml
# Example GitHub Actions
- name: Test TSI Installation
  run: |
    cd docker
    ./run-tests.sh
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
chmod +x docker/test-install-c.sh
```

## Adding New Test Scenarios

1. Create a new Dockerfile in `docker/`:
   ```dockerfile
   FROM <base-image>
   # Install specific tools
   # Remove package manager
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

## Simulating Xilinx Systems

Xilinx systems are typically:
- Minimal Linux (often based on Debian/Ubuntu)
- No package manager
- May have C compiler and build tools depending on SDK

Use the appropriate container:
- `ubuntu-minimal` - Bare Xilinx system
- `alpine-c-only` - Xilinx with build tools
