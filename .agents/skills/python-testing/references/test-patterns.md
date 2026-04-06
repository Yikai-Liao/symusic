# Testing Patterns — Deep Reference

## Contents

| Section | Lines | Description |
|---------|-------|-------------|
| Pattern 1: Writing Simple, Focused Tests | 19–66 | Arrange-Act-Assert pattern and focused test design |
| Pattern 2: Testing for Failures | 67–94 | Testing exceptions, warnings, and deprecations |
| Pattern 3: Approximate Comparisons | 95–152 | Floating-point comparisons with pytest.approx and numpy.testing |
| Pattern 4: Using Fixtures | 153–239 | Reusable test setup with scopes and auto-use |
| Pattern 5: Parametrized Tests | 240–311 | Testing with multiple inputs and custom IDs |
| Pattern 6: Test Organization with Markers | 312–440 | Markers, skip/xfail, and directory-based test organization |
| Pattern 7: Mocking and Monkeypatching | 441–514 | Mocking expensive operations and external dependencies |
| Pattern 8: Testing Against Installed Version | 515–555 | Using src layout for proper installed package testing |
| Pattern 8b: Import Best Practices in Tests | 556–620 | Proper import patterns for maintainable tests |

---

## Pattern 1: Writing Simple, Focused Tests

**Bad - Multiple assertions testing different things:**
```python
def test_everything():
    data = load_data("input.csv")
    assert len(data) > 0
    processed = process_data(data)
    assert processed.mean() > 0
    result = analyze(processed)
    assert result.success
```

**Good - Separate tests for each behavior:**
```python
def test_load_data_returns_nonempty():
    """Data loading should return at least one row."""
    data = load_data("input.csv")
    assert len(data) > 0

def test_process_data_positive_mean():
    """Processed data should have positive mean."""
    data = load_data("input.csv")
    processed = process_data(data)
    assert processed.mean() > 0

def test_analyze_succeeds():
    """Analysis should complete successfully."""
    data = load_data("input.csv")
    processed = process_data(data)
    result = analyze(processed)
    assert result.success
```

**Arrange-Act-Assert pattern:**
```python
def test_computation():
    # Arrange - Set up test data
    data = np.array([1, 2, 3, 4, 5])
    expected = 3.0

    # Act - Execute the function
    result = compute_mean(data)

    # Assert - Check the result
    assert result == approx(expected)
```

## Pattern 2: Testing for Failures

Always test that your code raises appropriate exceptions:

```python
import pytest

def test_zero_division_raises():
    """Division by zero should raise ZeroDivisionError."""
    with pytest.raises(ZeroDivisionError):
        result = 1 / 0

def test_invalid_input_raises():
    """Invalid input should raise ValueError."""
    with pytest.raises(ValueError, match="must be positive"):
        result = compute_sqrt(-1)

def test_deprecation_warning():
    """Deprecated function should warn."""
    with pytest.warns(DeprecationWarning):
        result = old_function()

def test_deprecated_call():
    """Check for deprecated API usage."""
    with pytest.deprecated_call():
        result = legacy_api()
```

## Pattern 3: Approximate Comparisons

Scientific computing often involves floating-point arithmetic that cannot be tested for exact equality:

**For scalars:**
```python
from pytest import approx

def test_approximate_scalar():
    """Test with approximate comparison."""
    result = 1 / 3
    assert result == approx(0.33333333333, rel=1e-10)

    # Default relative tolerance is 1e-6
    assert 0.3 + 0.3 == approx(0.6)

def test_approximate_with_absolute_tolerance():
    """Test with absolute tolerance."""
    result = compute_small_value()
    assert result == approx(0.0, abs=1e-10)
```

**For NumPy arrays (preferred over numpy.testing):**
```python
import numpy as np
from pytest import approx

def test_array_approximate():
    """Test NumPy arrays with approx."""
    result = np.array([0.1, 0.2, 0.3])
    expected = np.array([0.10001, 0.20001, 0.30001])
    assert result == approx(expected)

def test_array_with_nan():
    """Handle NaN values in arrays."""
    result = np.array([1.0, np.nan, 3.0])
    expected = np.array([1.0, np.nan, 3.0])
    assert result == approx(expected, nan_ok=True)
```

**When to use numpy.testing:**
```python
import numpy as np
from numpy.testing import assert_allclose, assert_array_equal

def test_exact_integer_array():
    """Use numpy.testing for exact integer comparisons."""
    result = np.array([1, 2, 3])
    expected = np.array([1, 2, 3])
    assert_array_equal(result, expected)

def test_complex_array_tolerances():
    """Use numpy.testing for complex tolerance requirements."""
    result = compute_result()
    expected = load_reference()
    assert_allclose(result, expected, rtol=1e-7, atol=1e-10)
```

## Pattern 4: Using Fixtures

Fixtures provide reusable test setup and teardown:

**Basic fixtures:**
```python
import pytest
import numpy as np

@pytest.fixture
def sample_data():
    """Provide sample data for tests."""
    return np.array([1.0, 2.0, 3.0, 4.0, 5.0])

@pytest.fixture
def empty_array():
    """Provide empty array for edge case tests."""
    return np.array([])

def test_mean_with_fixture(sample_data):
    """Test using fixture."""
    result = np.mean(sample_data)
    assert result == approx(3.0)

def test_empty_array(empty_array):
    """Test edge case with empty array."""
    with pytest.warns(RuntimeWarning):
        result = np.mean(empty_array)
        assert np.isnan(result)
```

**Fixtures with setup and teardown:**
```python
import pytest
import tempfile
from pathlib import Path

@pytest.fixture
def temp_datafile():
    """Create temporary data file for tests."""
    # Setup
    tmpfile = tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.txt')
    tmpfile.write("1.0\n2.0\n3.0\n")
    tmpfile.close()

    # Provide to test
    yield Path(tmpfile.name)

    # Teardown
    Path(tmpfile.name).unlink()

def test_load_data(temp_datafile):
    """Test data loading from file."""
    data = np.loadtxt(temp_datafile)
    assert len(data) == 3
    assert data[0] == approx(1.0)
```

**Fixture scopes:**
```python
@pytest.fixture(scope="function")  # Default, run for each test
def data_per_test():
    return create_data()

@pytest.fixture(scope="class")  # Run once per test class
def data_per_class():
    return create_data()

@pytest.fixture(scope="module")  # Run once per module
def data_per_module():
    return load_large_dataset()

@pytest.fixture(scope="session")  # Run once per test session
def database_connection():
    conn = create_connection()
    yield conn
    conn.close()
```

**Auto-use fixtures:**
```python
@pytest.fixture(autouse=True)
def reset_random_seed():
    """Reset random seed before each test for reproducibility."""
    np.random.seed(42)
```

## Pattern 5: Parametrized Tests

Test the same function with multiple inputs:

**Basic parametrization:**
```python
import pytest

@pytest.mark.parametrize("input_val,expected", [
    (0, 0),
    (1, 1),
    (2, 4),
    (3, 9),
    (-2, 4),
])
def test_square(input_val, expected):
    """Test squaring with multiple inputs."""
    assert input_val**2 == expected

@pytest.mark.parametrize("angle", [0, np.pi/6, np.pi/4, np.pi/3, np.pi/2])
def test_sine_range(angle):
    """Test sine function returns values in [0, 1] for first quadrant."""
    result = np.sin(angle)
    assert 0 <= result <= 1
```

**Multiple parameters:**
```python
@pytest.mark.parametrize("n_air,n_water", [
    (1.0, 1.33),
    (1.0, 1.5),
    (1.5, 1.0),
])
def test_refraction(n_air, n_water):
    """Test Snell's law with different refractive indices."""
    angle_in = np.pi / 4
    angle_out = snell(angle_in, n_air, n_water)
    assert angle_out >= 0
    assert angle_out <= np.pi / 2
```

**Parametrized fixtures:**
```python
@pytest.fixture(params=[1, 2, 3], ids=["one", "two", "three"])
def dimension(request):
    """Parametrized fixture for different dimensions."""
    return request.param

def test_array_creation(dimension):
    """Test array creation in different dimensions."""
    shape = tuple([10] * dimension)
    arr = np.zeros(shape)
    assert arr.ndim == dimension
    assert arr.shape == shape
```

**Combining parametrization with custom IDs:**
```python
@pytest.mark.parametrize(
    "data,expected",
    [
        (np.array([1, 2, 3]), 2.0),
        (np.array([1, 1, 1]), 1.0),
        (np.array([0, 10]), 5.0),
    ],
    ids=["sequential", "constant", "extremes"]
)
def test_mean_with_ids(data, expected):
    """Test mean with descriptive test IDs."""
    assert np.mean(data) == approx(expected)
```

## Pattern 6: Test Organization with Markers

Use markers to organize and selectively run tests:

**Basic markers:**
```python
import pytest

@pytest.mark.slow
def test_expensive_computation():
    """Test that takes a long time."""
    result = run_simulation(n_iterations=1000000)
    assert result.converged

@pytest.mark.requires_gpu
def test_gpu_acceleration():
    """Test that requires GPU hardware."""
    result = compute_on_gpu(large_array)
    assert result.success

@pytest.mark.integration
def test_full_pipeline():
    """Integration test for complete workflow."""
    data = load_data()
    processed = preprocess(data)
    result = analyze(processed)
    output = save_results(result)
    assert output.exists()
```

**Running specific markers:**
```bash
pytest -m slow              # Run only slow tests
pytest -m "not slow"        # Skip slow tests
pytest -m "slow or gpu"     # Run slow OR gpu tests
pytest -m "slow and integration"  # Run slow AND integration tests
```

**Skip and xfail markers:**
```python
@pytest.mark.skip(reason="Feature not implemented yet")
def test_future_feature():
    """Test for feature under development."""
    result = future_function()
    assert result.success

@pytest.mark.skipif(sys.version_info < (3, 10), reason="Requires Python 3.10+")
def test_pattern_matching():
    """Test using Python 3.10+ features."""
    match value:
        case 0:
            result = "zero"
        case _:
            result = "other"
    assert result == "zero"

@pytest.mark.xfail(reason="Known bug in upstream library")
def test_known_failure():
    """Test that currently fails due to known issue."""
    result = buggy_function()
    assert result == expected

@pytest.mark.xfail(strict=True)
def test_must_fail():
    """Test that MUST fail (test will fail if it passes)."""
    with pytest.raises(NotImplementedError):
        unimplemented_function()
```

## Pattern 6b: Organizing Test Suites by Directory

Following [Scientific Python recommendations](https://learn.scientific-python.org/development/principles/testing/#test-suites), organize tests into separate directories by type and execution time:

```text
tests/
├── unit/               # Fast, isolated unit tests
│   ├── conftest.py
│   ├── test_analysis.py
│   └── test_utils.py
├── integration/        # Integration tests
│   ├── conftest.py
│   └── test_pipeline.py
├── e2e/               # End-to-end tests
│   └── test_workflows.py
└── conftest.py        # Shared fixtures
```

**Run specific test suites:**
```bash
# Run only unit tests (fast)
pytest tests/unit/

# Run integration tests after unit tests pass
pytest tests/integration/

# Run all tests
pytest
```

**Auto-mark all tests in a directory using conftest.py:**
```python
# tests/unit/conftest.py
import pytest

def pytest_collection_modifyitems(session, config, items):
    """Automatically mark all tests in this directory as unit tests."""
    for item in items:
        item.add_marker(pytest.mark.unit)
```

**Benefits of organized test suites:**
- Run fast tests first ("fail fast" principle)
- Developers can run relevant tests quickly
- Clear separation of test types
- Avoid false positives from slow/flaky tests
- Better CI/CD optimization

**Example test runner strategy:**
```bash
# Run fast unit tests first, stop on failure
pytest tests/unit/ -x || exit 1

# If unit tests pass, run integration tests
pytest tests/integration/ -x || exit 1

# Finally run slow end-to-end tests
pytest tests/e2e/
```

## Pattern 7: Mocking and Monkeypatching

Mock expensive operations or external dependencies:

**Basic monkeypatching:**
```python
import platform

def test_platform_specific_behavior(monkeypatch):
    """Test behavior on different platforms."""
    # Mock platform.system() to return "Linux"
    monkeypatch.setattr(platform, "system", lambda: "Linux")
    result = get_platform_specific_path()
    assert result == "/usr/local/data"

    # Change mock to return "Windows"
    monkeypatch.setattr(platform, "system", lambda: "Windows")
    result = get_platform_specific_path()
    assert result == r"C:\Users\data"
```

**Mocking with pytest-mock:**
```python
import pytest
from unittest.mock import Mock

def test_expensive_computation(mocker):
    """Mock expensive computation."""
    # Mock the expensive function
    mock_compute = mocker.patch("my_package.analysis.expensive_compute")
    mock_compute.return_value = 42

    result = run_analysis()

    # Verify the mock was called
    mock_compute.assert_called_once()
    assert result == 42

def test_matplotlib_plotting(mocker):
    """Test plotting without creating actual plots."""
    mock_plt = mocker.patch("matplotlib.pyplot")

    create_plot(data)

    # Verify plot was created
    mock_plt.figure.assert_called_once()
    mock_plt.plot.assert_called_once()
    mock_plt.savefig.assert_called_once_with("output.png")
```

**Fixture for repeated mocking:**
```python
@pytest.fixture
def mock_matplotlib(mocker):
    """Mock matplotlib for testing plots."""
    fig = mocker.Mock(spec=plt.Figure)
    ax = mocker.Mock(spec=plt.Axes)
    line2d = mocker.Mock(name="plot", spec=plt.Line2D)
    ax.plot.return_value = (line2d,)

    mpl = mocker.patch("matplotlib.pyplot", autospec=True)
    mocker.patch("matplotlib.pyplot.subplots", return_value=(fig, ax))

    return {"fig": fig, "ax": ax, "mpl": mpl}

def test_my_plot(mock_matplotlib):
    """Test plotting function."""
    ax = mock_matplotlib["ax"]
    my_plotting_function(ax=ax)

    ax.plot.assert_called_once()
    ax.set_xlabel.assert_called_once()
```

## Pattern 8: Testing Against Installed Version

Always test the installed package, not local source:

**Why this matters:**
```
my-package/
├── src/
│   └── my_package/
│       ├── __init__.py
│       ├── data/            # Data files
│       │   └── reference.csv
│       └── analysis.py
└── tests/
    └── test_analysis.py
```

**Use src/ layout + editable install:**
```bash
# Install in editable mode
pip install -e .

# Run tests against installed version
pytest
```

**Benefits:**
- Tests ensure package installs correctly
- Catches missing files (like data files)
- Tests work in CI/CD environments
- Validates package structure and imports

**In tests, import from package:**
```python
# Good - imports installed package
from my_package.analysis import compute_mean

# Bad - would import from local src/ if not using src/ layout
# from analysis import compute_mean
```

## Pattern 8b: Import Best Practices in Tests

Following [Scientific Python unit testing guidelines](https://learn.scientific-python.org/development/principles/testing/#unit-tests), proper import patterns make tests more maintainable:

**Keep imports local to file under test:**
```python
# Good - Import from the file being tested
from my_package.analysis import MyClass, compute_mean

def test_compute_mean():
    """Test imports from module under test."""
    data = MyClass()
    result = compute_mean(data)
    assert result > 0
```

**Why this matters:**
- When code is refactored and symbols move, tests don't break
- Tests only care about symbols used in the file under test
- Reduces coupling between tests and internal code organization

**Import specific symbols, not entire modules:**
```python
# Good - Specific imports, easy to mock
from numpy import mean as np_mean, ndarray as NpArray

def my_function(data: NpArray) -> float:
    return np_mean(data)

# Good - Easy to patch in tests
def test_my_function(mocker):
    mock_mean = mocker.patch("my_package.analysis.np_mean")
    # ...
```

```python
# Less ideal - Harder to mock effectively
import numpy as np

def my_function(data: np.ndarray) -> float:
    return np.mean(data)

# Less ideal - Complex patching required
def test_my_function(mocker):
    # Must patch through the aliased namespace
    mock_mean = mocker.patch("my_package.analysis.np.mean")
    # ...
```

**Consider meaningful aliases:**
```python
# Make imports meaningful to your domain
from numpy import sum as numeric_sum
from scipy.stats import ttest_ind as statistical_test

# Easy to understand and replace
result = numeric_sum(values)
p_value = statistical_test(group1, group2)
```

This approach makes it easier to:
- Replace implementations without changing tests
- Mock dependencies effectively
- Understand code purpose from import names
