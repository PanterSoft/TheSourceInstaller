# TSI Testing Guide

## Test Infrastructure

TSI has comprehensive testing infrastructure for the C implementation.

## Running Tests

### Quick Start

```bash
# Run all tests
make test

# Test C version only
make test-c
```

### Manual Testing

```bash
cd docker
./run-tests.sh
```

## Test Scenarios

### C Version Tests

1. **alpine-minimal**: Minimal system (should fail gracefully)
2. **alpine-c-only**: C compiler only (should build and work)
3. **ubuntu-minimal**: Minimal Ubuntu (should fail gracefully)

## CI/CD Integration

### GitHub Actions

Tests run automatically on:
- Push to main/develop branches
- Pull requests
- Manual trigger (workflow_dispatch)

**Jobs:**
- `test`: Tests C version on all scenarios
- `build`: Multi-architecture builds (amd64, arm64)
- `lint`: Code linting
- `test-all`: Complete test suite

### GitLab CI

Similar structure with GitLab CI syntax.

## Known Issues and Fixes

### C Version Build Issues

**Problem**: Static linking may fail on some systems (especially macOS).

**Fix**:
- Made static linking optional (falls back to dynamic)
- Makefile detects OS and only uses static linking on Linux
- Makefile respects `CC` environment variable
- Better error messages

### Type Safety Warnings

**Problem**: Type mismatch warnings when passing `char **` to functions expecting `const char **`.

**Fix**:
- Added explicit casts in `main.c`
- Suppressed unused parameter warnings with `(void)` casts

### Test Failures

**Problem**: Tests failing due to missing dependencies.

**Fix**:
- Improved error handling in test scripts
- Better detection of minimal systems
- Proper cleanup between tests

## Test Results

Test logs are saved to `/tmp/tsi-test-*.log` and uploaded as artifacts in CI.

## Adding New Tests

1. Create Dockerfile in `docker/`
2. Add to `docker-compose.yml`
3. Add to test scenarios in `run-tests.sh`
4. Update CI configuration if needed

## Debugging Failed Tests

1. Check test logs: `/tmp/tsi-test-<scenario>.log`
2. Run test manually:
   ```bash
   docker-compose run --rm <scenario> /bin/sh
   ```
3. Check container state:
   ```bash
   docker-compose ps
   ```

## Best Practices

- Always test locally before pushing
- Check CI results after PR
- Update tests when adding features
- Keep test scenarios minimal and focused
- Document expected behavior for each scenario
