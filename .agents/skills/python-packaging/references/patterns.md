# Package Structure Patterns — Deep Reference

## Contents

| Section | Lines | Description |
|---------|-------|-------------|
| Pattern 1: Pure Python Scientific Package (Recommended) | 15–47 | Standard src layout for scientific Python packages |
| Pattern 2: Scientific Package with Data Files | 48–92 | Including and accessing data files in packages |
| Pattern 3: Versioning Strategies | 93–143 | Manual, dynamic, and git-based versioning approaches |
| Pattern 4: Building and Publishing | 144–203 | Building distributions and publishing to PyPI |
| Pattern 5: Testing Installation | 204–241 | Editable installs and isolated environment testing |

---

## Pattern 1: Pure Python Scientific Package (Recommended)

```
my-sci-package/
├── pyproject.toml
├── README.md
├── LICENSE
├── .gitignore
├── src/
│   └── my_sci_package/
│       ├── __init__.py
│       ├── analysis.py
│       ├── preprocessing.py
│       ├── visualization.py
│       ├── utils.py
│       └── py.typed          # For type hints
├── tests/
│   ├── __init__.py
│   ├── test_analysis.py
│   ├── test_preprocessing.py
│   └── test_visualization.py
└── docs/
    ├── conf.py
    ├── index.md
    └── api.md
```

**Key advantages:**
- Prevents accidental imports from source
- Forces proper installation for testing
- Professional structure for scientific libraries
- Clear separation of concerns

## Pattern 2: Scientific Package with Data Files

```
my-sci-package/
├── pyproject.toml
├── README.md
├── LICENSE
├── src/
│   └── my_sci_package/
│       ├── __init__.py
│       ├── analysis.py
│       └── data/
│           ├── reference.csv
│           ├── constants.json
│           └── coefficients.dat
├── tests/
│   └── test_analysis.py
└── docs/
    └── index.md
```

**Include data files in pyproject.toml (if needed):**

```toml
[tool.hatch.build.targets.wheel]
packages = ["src/my_sci_package"]

# Only if you need to explicitly include data
[tool.hatch.build.targets.wheel.force-include]
"src/my_sci_package/data" = "my_sci_package/data"
```

**Access data files in code:**

```python
from importlib.resources import files
import json

def load_constants():
    """Load constants from package data."""
    data_file = files("my_sci_package").joinpath("data/constants.json")
    with data_file.open() as f:
        return json.load(f)
```

## Pattern 3: Versioning Strategies

### Manual Versioning

```toml
[project]
version = "1.2.3"
```

```python
# src/my_sci_package/__init__.py
__version__ = "1.2.3"
```

### Dynamic Versioning with Hatchling

```toml
[project]
dynamic = ["version"]

[tool.hatch.version]
path = "src/my_sci_package/__init__.py"
```

```python
# src/my_sci_package/__init__.py
__version__ = "1.2.3"
```

### Git-Based Versioning with Hatchling

```toml
[build-system]
requires = ["hatchling", "hatch-vcs"]
build-backend = "hatchling.build"

[project]
dynamic = ["version"]

[tool.hatch.version]
source = "vcs"

[tool.hatch.build.hooks.vcs]
version-file = "src/my_sci_package/_version.py"
```

**Semantic versioning for scientific software:**
- `MAJOR`: Breaking API changes
- `MINOR`: New features, backward compatible
- `PATCH`: Bug fixes

## Pattern 4: Building and Publishing

### Build Package Locally

```bash
# Install build tools
pip install build

# Build distribution
python -m build

# Creates:
# dist/my-sci-package-1.0.0.tar.gz (source distribution)
# dist/my_sci_package-1.0.0-py3-none-any.whl (wheel)

# Verify the distribution
pip install twine
twine check dist/*

# Inspect contents
tar -tvf dist/*.tar.gz
unzip -l dist/*.whl
```

**Critical**: Test the SDist contents to ensure all necessary files are included.

### Publishing to PyPI

```bash
# Install publishing tools
pip install twine

# Test on TestPyPI first (always!)
twine upload --repository testpypi dist/*

# Install and test from TestPyPI
pip install --index-url https://test.pypi.org/simple/ my-sci-package

# If everything works, publish to PyPI
twine upload dist/*
```

**Using API tokens (recommended):**

Create `~/.pypirc`:
```ini
[distutils]
index-servers =
    pypi
    testpypi

[pypi]
username = __token__
password = pypi-...your-token...

[testpypi]
username = __token__
password = pypi-...your-test-token...
```

## Pattern 5: Testing Installation

### Editable Install for Development

```bash
# Install in development mode
pip install -e .

# With dependency groups
pip install -e . --group dev

# Using uv (recommended for scientific workflows)
uv pip install -e . --group dev

# Now changes to source code are immediately reflected
```

### Testing in Isolated Environment

```bash
# Create and activate virtual environment
python -m venv test-env
source test-env/bin/activate  # Linux/Mac

# Install from wheel
pip install dist/my_sci_package-1.0.0-py3-none-any.whl

# Test import and version
python -c "import my_sci_package; print(my_sci_package.__version__)"

# Test CLI
sci-analyze --help

# Cleanup
deactivate
rm -rf test-env
```
