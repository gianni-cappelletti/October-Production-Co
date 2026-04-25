# Test Fixtures

Shared test data (WAV files and NAM models) used by all test suites:
- `libs/octobir-core/tests/` -- octobir-core library tests
- `libs/octobass-core/tests/` -- octobass-core library tests
- `plugins/octobir/juce/tests/` -- OctobIR JUCE plugin tests
- `plugins/octobir/vcv-rack/tests/` -- OctobIR VCV Rack plugin tests
- `plugins/octobass/juce/tests/` -- OctoBASS JUCE plugin tests

Defined as `TEST_DATA_DIR` in each suite's `CMakeLists.txt`.

## Running Tests

```bash
make test              # Run all test suites
make test-octobir      # Run all OctobIR tests (core + JUCE + VCV)
make test-octobass     # Run all OctoBASS tests (core + JUCE)
```
