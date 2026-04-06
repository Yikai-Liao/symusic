# Common Testing Pitfalls and Solutions — Deep Reference

## Contents

| Section | Lines | Description |
|---------|-------|-------------|
| Pitfall 1: Testing Implementation Instead of Behavior | 14–31 | Testing behavior and outcomes rather than internal details |
| Pitfall 2: Non-Deterministic Tests | 32–49 | Fixing random seed issues for reproducible tests |
| Pitfall 3: Exact Floating-Point Comparisons | 50–65 | Using approx for floating-point assertions |
| Pitfall 4: Testing Too Much in One Test | 66–111 | Splitting monolithic tests into focused test functions |

---

## Pitfall 1: Testing Implementation Instead of Behavior

**Bad:**
```python
def test_uses_numpy_mean():
    """Test that function uses np.mean."""  # Testing implementation!
    # This is fragile - breaks if implementation changes
    pass
```

**Good:**
```python
def test_computes_correct_average():
    """Test that function returns correct average."""
    data = np.array([1, 2, 3, 4, 5])
    result = compute_average(data)
    assert result == approx(3.0)
```

## Pitfall 2: Non-Deterministic Tests

**Bad:**
```python
def test_random_sampling():
    samples = generate_samples()  # Uses random seed from system time!
    assert samples[0] > 0  # Might fail randomly
```

**Good:**
```python
def test_random_sampling():
    np.random.seed(42)  # Fixed seed
    samples = generate_samples()
    assert samples[0] == approx(0.4967, rel=1e-4)
```

## Pitfall 3: Exact Floating-Point Comparisons

**Bad:**
```python
def test_computation():
    result = 0.1 + 0.2
    assert result == 0.3  # Fails due to floating-point error!
```

**Good:**
```python
def test_computation():
    result = 0.1 + 0.2
    assert result == approx(0.3)
```

## Pitfall 4: Testing Too Much in One Test

**Bad:**
```python
def test_entire_analysis():
    # Load data
    data = load_data()
    assert data is not None

    # Process
    processed = process(data)
    assert len(processed) > 0

    # Analyze
    result = analyze(processed)
    assert result.score > 0.8

    # Save
    save_results(result, "output.txt")
    assert Path("output.txt").exists()
```

**Good:**
```python
def test_load_data_succeeds():
    data = load_data()
    assert data is not None

def test_process_returns_nonempty():
    data = load_data()
    processed = process(data)
    assert len(processed) > 0

def test_analyze_gives_good_score():
    data = load_data()
    processed = process(data)
    result = analyze(processed)
    assert result.score > 0.8

def test_save_results_creates_file(tmp_path):
    output_file = tmp_path / "output.txt"
    result = create_mock_result()
    save_results(result, output_file)
    assert output_file.exists()
```
