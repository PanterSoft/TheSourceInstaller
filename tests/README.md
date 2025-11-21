# TSI Test Suite

Comprehensive test suite for TSI package manager (C implementation).

## Test Structure

```
tests/
├── unit/              # Unit tests (future)
├── integration/       # Integration tests (future)
└── README.md         # This file

docker/
├── test-install-c.sh # C version installation test
└── run-tests.sh      # Test runner
```

## Running Tests

### Local Testing

```bash
# Run all tests
cd docker
./run-tests.sh

# Test specific scenario
docker-compose run --rm alpine-c-only /bin/sh /root/tsi-source/docker/test-install-c.sh

# Or use Makefile
make test
```

### CI/CD Pipeline

Tests run automatically on:
- **GitHub Actions**: `.github/workflows/test.yml`
- **GitLab CI**: `.gitlab-ci.yml`

## Test Scenarios

### C Version Tests

1. **alpine-minimal**: Absolutely minimal system (should fail gracefully)
2. **alpine-c-only**: C compiler only (should build and work)
3. **ubuntu-minimal**: Minimal Ubuntu (should fail gracefully)

## Test Coverage

### Current Coverage

- ✅ Installation on various system configurations
- ✅ Bootstrap installer functionality
- ✅ Tool detection
- ✅ C version compilation
- ✅ Basic CLI commands
- ✅ Package installation workflow

### Planned Coverage

- ⏳ Unit tests for core components
- ⏳ Integration tests for package installation
- ⏳ Dependency resolution tests
- ⏳ Build system integration tests
- ⏳ Error handling tests
- ⏳ Performance tests

## Writing Tests

### Adding a New Test Scenario

1. Create Dockerfile in `docker/`:
   ```dockerfile
   FROM alpine:latest
   # Install specific tools
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

### Writing Unit Tests

Future unit tests should be placed in `tests/unit/`:
- C: Use a C testing framework (e.g., Unity, Check)

## Continuous Integration

### GitHub Actions

Tests run on:
- Push to main/develop
- Pull requests
- Manual trigger (workflow_dispatch)

Jobs:
- `test`: Tests C version on all scenarios
- `build`: Builds C version for multiple architectures
- `lint`: Lints C code
- `test-all`: Runs complete test suite

### GitLab CI

Similar structure with GitLab CI syntax.

## Test Results

Test logs are saved to `/tmp/tsi-test-*.log` and uploaded as artifacts on failure.

## Contributing

When adding new features:
1. Add corresponding tests
2. Ensure tests pass locally
3. Update test documentation
4. Tests will run automatically in CI
