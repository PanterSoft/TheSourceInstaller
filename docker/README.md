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
docker-compose build alpine-minimal
docker-compose run --rm alpine-minimal /bin/sh /root/tsi/docker/test-install.sh

# Or enter the container interactively
docker-compose run --rm alpine-minimal /bin/sh
```

## Test Scenarios

### Alpine Linux

1. **alpine-minimal**: Absolutely minimal system
   - No Python
   - No build tools
   - No package manager
   - Tests: Should fail gracefully with helpful error

2. **alpine-python**: Python available, no build tools
   - Python 3.x installed
   - No gcc/make
   - Tests: Should install TSI directly using Python

3. **alpine-build**: Build tools available, no Python
   - gcc, make, wget, curl available
   - No Python
   - Tests: Should bootstrap Python from source, then install TSI

### Ubuntu/Debian

4. **ubuntu-minimal**: Minimal Ubuntu system
   - No Python
   - No build tools
   - No package manager
   - Tests: Should fail gracefully

5. **ubuntu-python**: Python available, no build tools
   - Python 3.x installed
   - No gcc/make
   - Tests: Should install TSI directly

6. **ubuntu-build**: Build tools available, no Python
   - gcc, make, wget, curl available
   - No Python
   - Tests: Should bootstrap Python from source

## Manual Testing

### Enter a Container

```bash
cd docker
docker-compose run --rm alpine-python /bin/sh
```

Inside the container:

```sh
# Check available tools
which python3
which gcc
which make

# Run the bootstrap installer
cd /root/tsi
./bootstrap-default.sh

# Test TSI
~/.tsi/bin/tsi --help
```

### Test One-Line Install

```bash
docker-compose run --rm alpine-python /bin/sh -c "
cd /root/tsi
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/bootstrap-default.sh | sh
"
```

## Container Details

### Minimal Containers

The minimal containers have:
- Package managers removed (simulating minimal systems)
- Only essential POSIX tools
- No Python, no build tools

### Python Containers

Containers with Python have:
- Python 3.x installed
- pip available
- No build tools (gcc, make)

### Build Tool Containers

Containers with build tools have:
- gcc/g++ compiler
- make
- wget/curl
- tar/gzip
- No Python

## Test Script

The `test-install.sh` script:
1. Shows system information
2. Lists available tools
3. Runs the bootstrap installer
4. Verifies TSI installation
5. Tests TSI command

## Continuous Integration

These Docker containers can be used in CI/CD pipelines:

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
chmod +x docker/test-install.sh
```

## Adding New Test Scenarios

1. Create a new Dockerfile in `docker/`:
   ```dockerfile
   FROM <base-image>
   # Install specific tools
   # Remove package manager
   COPY . /root/tsi/
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
- May have Python or build tools depending on SDK

Use the appropriate container:
- `ubuntu-minimal` - Bare Xilinx system
- `ubuntu-python` - Xilinx with Python SDK
- `ubuntu-build` - Xilinx with build tools

## See Also

- [INSTALL.md](../INSTALL.md) - Installation guide
- [BOOTSTRAP.md](../BOOTSTRAP.md) - Bootstrap reference
- [INTELLIGENT-INSTALLER.md](../INTELLIGENT-INSTALLER.md) - Installer details

