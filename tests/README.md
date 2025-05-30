# SyMusic Test Suite

This directory contains comprehensive tests for both the C++ library and Python bindings of SyMusic.

## Directory Structure

- `cpp/`: C++ tests using the Catch2 framework
- `testcases/`: MIDI files used for testing
- `*.py`: Python test files using pytest

## Running C++ Tests

The C++ tests use the [Catch2](https://github.com/catchorg/Catch2) framework. To build and run the tests:

### Building the Tests

```bash
# From the project root directory
mkdir -p build && cd build
cmake .. -DBUILD_TESTS=ON
make -j
```

### Running the Tests

```bash
# From the build directory
./tests/cpp/symusic_tests
```

You can also run specific test cases or test tags:

```bash
# Run only time event tests
./tests/cpp/symusic_tests "[time_events]"

# Run only score tests
./tests/cpp/symusic_tests "[score]"

# List all available tests
./tests/cpp/symusic_tests --list-tests
```

## Running Python Tests

The Python tests use pytest. To run the Python tests:

### Installing Test Dependencies

```bash
pip install pytest numpy
```

### Running the Tests

```bash
# From the project root directory
pytest tests/
```

You can also run specific test files or test functions:

```bash
# Run a specific test file
pytest tests/test_py_bindings.py

# Run a specific test function
pytest tests/test_py_bindings.py::test_note_creation

# Run with verbose output
pytest -v tests/

# Run with output for each test
pytest -vs tests/
```

## Test Coverage

The test suite covers:

### C++ Tests:
- Time event classes (Note, Tempo, TimeSignature, etc.)
- Track and Score classes
- Time unit conversions
- File I/O operations

### Python Tests:
- Python binding functionality
- Object creation and manipulation
- File I/O operations
- Filtering and transformations
- PianoRoll functionality
- Advanced operations (quantization, transposition, etc.)

## Adding New Tests

### C++ Tests:
1. Create a new header file in the `cpp/` directory
2. Include your test cases using the Catch2 macros
3. Add the header to `test_main.cpp`

### Python Tests:
1. Create a new Python file with a name starting with `test_`
2. Write test functions starting with `test_`
3. Use pytest fixtures as needed

## Continuous Integration

The tests are automatically run as part of the CI pipeline on GitHub. See the GitHub Actions workflow configuration for details.
