# Scientific Python Testing Patterns — Deep Reference

## Contents

| Section | Lines | Description |
|---------|-------|-------------|
| Pattern 9: Testing Numerical Algorithms | 15–52 | Testing stability, convergence, and conservation laws |
| Pattern 10: Testing with Different NumPy dtypes | 53–79 | Parametrized tests across float and integer dtypes |
| Pattern 11: Testing Random/Stochastic Code | 80–112 | Fixed seeds and statistical property verification |
| Pattern 12: Testing Data Pipelines | 113–152 | End-to-end and stage-by-stage pipeline testing |
| Pattern 13: Property-Based Testing with Hypothesis | 153–182 | Using hypothesis for generative scientific testing |

---

## Pattern 9: Testing Numerical Algorithms

```python
import numpy as np
from pytest import approx

def test_numerical_stability():
    """Test algorithm is numerically stable."""
    data = np.array([1e10, 1.0, -1e10])
    result = stable_sum(data)
    assert result == approx(1.0)

def test_convergence():
    """Test iterative algorithm converges."""
    x0 = np.array([1.0, 1.0, 1.0])
    result = iterative_solver(x0, tol=1e-8, max_iter=1000)

    assert result.converged
    assert result.iterations < 1000
    assert result.residual < 1e-8

def test_against_analytical_solution():
    """Test against known analytical result."""
    x = np.linspace(0, 1, 100)
    numerical = compute_integral(lambda t: t**2, x)
    analytical = x**3 / 3
    assert numerical == approx(analytical, rel=1e-6)

def test_conservation_law():
    """Test that physical conservation law holds."""
    initial_energy = compute_energy(system)
    system.evolve(dt=0.01, steps=1000)
    final_energy = compute_energy(system)

    # Energy should be conserved (within numerical error)
    assert final_energy == approx(initial_energy, rel=1e-10)
```

## Pattern 10: Testing with Different NumPy dtypes

```python
@pytest.mark.parametrize("dtype", [
    np.float32,
    np.float64,
    np.complex64,
    np.complex128,
])
def test_computation_dtypes(dtype):
    """Test function works with different dtypes."""
    data = np.array([1, 2, 3, 4, 5], dtype=dtype)
    result = compute_transform(data)

    assert result.dtype == dtype
    assert result.shape == data.shape

@pytest.mark.parametrize("dtype", [np.int32, np.int64, np.float32, np.float64])
def test_integer_and_float_types(dtype):
    """Test handling of integer and float types."""
    arr = np.array([1, 2, 3], dtype=dtype)
    result = safe_divide(arr, 2)

    # Result should be floating point
    assert result.dtype in [np.float32, np.float64]
```

## Pattern 11: Testing Random/Stochastic Code

```python
def test_random_with_seed():
    """Test random code with fixed seed for reproducibility."""
    np.random.seed(42)
    result1 = generate_random_samples(n=100)

    np.random.seed(42)
    result2 = generate_random_samples(n=100)

    # Should get identical results with same seed
    assert np.array_equal(result1, result2)

def test_statistical_properties():
    """Test statistical properties of random output."""
    np.random.seed(123)
    samples = generate_normal_samples(n=100000, mean=0, std=1)

    # Test mean and std are close to expected (not exact due to randomness)
    assert np.mean(samples) == approx(0, abs=0.01)
    assert np.std(samples) == approx(1, abs=0.01)

@pytest.mark.parametrize("seed", [42, 123, 456])
def test_reproducibility_with_seeds(seed):
    """Test reproducibility with different seeds."""
    np.random.seed(seed)
    result = stochastic_algorithm()

    # Should complete successfully regardless of seed
    assert result.success
```

## Pattern 12: Testing Data Pipelines

```python
def test_pipeline_end_to_end(tmp_path):
    """Test complete data pipeline."""
    # Arrange - Create input data
    input_file = tmp_path / "input.csv"
    input_file.write_text("x,y\n1,2\n3,4\n5,6\n")

    output_file = tmp_path / "output.csv"

    # Act - Run pipeline
    result = run_pipeline(input_file, output_file)

    # Assert - Check results
    assert result.success
    assert output_file.exists()

    output_data = np.loadtxt(output_file, delimiter=",", skiprows=1)
    assert len(output_data) == 3

def test_pipeline_stages_independently():
    """Test each pipeline stage separately."""
    # Test stage 1
    raw_data = load_data("input.csv")
    assert len(raw_data) > 0

    # Test stage 2
    cleaned = clean_data(raw_data)
    assert not np.any(np.isnan(cleaned))

    # Test stage 3
    transformed = transform_data(cleaned)
    assert transformed.shape == cleaned.shape

    # Test stage 4
    result = analyze_data(transformed)
    assert result.metrics["r2"] > 0.9
```

## Pattern 13: Property-Based Testing with Hypothesis

For complex scientific code, consider property-based testing:

```python
from hypothesis import given, strategies as st
from hypothesis.extra.numpy import arrays
import numpy as np

@given(arrays(np.float64, shape=st.integers(1, 100)))
def test_mean_is_bounded(arr):
    """Mean should be between min and max."""
    if len(arr) > 0 and not np.any(np.isnan(arr)):
        mean = np.mean(arr)
        assert np.min(arr) <= mean <= np.max(arr)

@given(
    x=arrays(np.float64, shape=10, elements=st.floats(-100, 100)),
    y=arrays(np.float64, shape=10, elements=st.floats(-100, 100))
)
def test_linear_fit_properties(x, y):
    """Test properties of linear regression."""
    if not (np.any(np.isnan(x)) or np.any(np.isnan(y))):
        slope, intercept = fit_linear(x, y)

        # Predictions should be finite
        predictions = slope * x + intercept
        assert np.all(np.isfinite(predictions))
```
