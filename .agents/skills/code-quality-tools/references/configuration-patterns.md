# Configuration Patterns — Deep Reference

## Contents

| Section | Lines | Description |
|---------|-------|-------------|
| Pattern 1: Basic Ruff Configuration | 15–33 | Basic ruff setup in pyproject.toml for scientific Python |
| Pattern 2: MyPy Configuration for Scientific Python | 34–50 | Configuring mypy strictness for scientific code |
| Pattern 3: Pre-commit Configuration | 51–66 | Setting up pre-commit hooks for quality enforcement |
| Pattern 4: Ruff Rule Selection for Scientific Python | 67–108 | Comprehensive ruff rule selection for scientific computing |
| Pattern 5: Fixing Common Ruff Warnings | 109–171 | Solutions for frequent ruff warnings with examples |

---

## Pattern 1: Basic Ruff Configuration

Configure ruff in `pyproject.toml` for your scientific Python project. See [assets/pyproject-ruff-mypy.toml](assets/pyproject-ruff-mypy.toml) for complete example.

**Key settings:**
```toml
[tool.ruff]
target-version = "py310"
line-length = 88

[tool.ruff.lint]
select = [
    "E",      # pycodestyle errors
    "F",      # pyflakes
    "I",      # isort
    "NPY",    # NumPy-specific rules
]
```

## Pattern 2: MyPy Configuration for Scientific Python

Configure mypy in `pyproject.toml` with appropriate strictness for scientific code:

```toml
[tool.mypy]
python_version = "3.10"
check_untyped_defs = true
disallow_untyped_defs = false  # Set to true when ready
warn_return_any = true
show_error_codes = true

[[tool.mypy.overrides]]
module = ["scipy.*", "matplotlib.*"]
ignore_missing_imports = true
```

## Pattern 3: Pre-commit Configuration

Set up pre-commit hooks to automatically enforce quality standards. See [assets/pre-commit-config.yaml](assets/pre-commit-config.yaml) for complete example.

**Setup:**
```bash
# Install pre-commit
uv add --dev pre-commit

# Install git hooks
pre-commit install

# Run on all files (first time)
pre-commit run --all-files
```

## Pattern 4: Ruff Rule Selection for Scientific Python

Configure ruff with rules appropriate for scientific computing:

```toml
[tool.ruff.lint]
select = [
    # Essential
    "E",      # pycodestyle errors
    "F",      # pyflakes
    "I",      # isort

    # Code quality
    "B",      # flake8-bugbear (common bugs)
    "C4",     # flake8-comprehensions
    "UP",     # pyupgrade (modern syntax)

    # Scientific Python specific
    "NPY",    # NumPy-specific rules
    "PD",     # pandas-vet

    # Documentation
    "D",      # pydocstyle (docstrings)

    # Type hints
    "ANN",    # flake8-annotations
]

# Customize docstring rules for NumPy style
[tool.ruff.lint.pydocstyle]
convention = "numpy"

# Common rules to ignore in scientific code
ignore = [
    "E501",   # Line too long (formatter handles this)
    "ANN101", # Missing type annotation for self
    "ANN102", # Missing type annotation for cls
    "D100",   # Missing docstring in public module (optional)
    "D104",   # Missing docstring in public package (optional)
]
```

## Pattern 5: Fixing Common Ruff Warnings

**F401: Unused import**
```python
# Before
import numpy as np
import pandas as pd  # Not used

# After
import numpy as np
```

**F841: Unused variable**
```python
# Before
def process_data(data):
    result = expensive_computation(data)
    return data  # Oops, should return result

# After
def process_data(data):
    result = expensive_computation(data)
    return result
```

**E711: Comparison to None**
```python
# Before
if value == None:
    pass

# After
if value is None:
    pass
```

**B006: Mutable default argument**
```python
# Before (dangerous!)
def append_data(value, data=[]):
    data.append(value)
    return data

# After
def append_data(value, data=None):
    if data is None:
        data = []
    data.append(value)
    return data
```

**NPY002: Legacy NumPy random**
```python
# Before (old style)
import numpy as np
data = np.random.rand(100)

# After (new style, better for reproducibility)
import numpy as np
rng = np.random.default_rng(seed=42)
data = rng.random(100)
```
