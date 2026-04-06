# Project Metadata Reference — Deep Reference

## Contents

| Section | Lines | Description |
|---------|-------|-------------|
| License (Modern SPDX Format) | 16–33 | Using SPDX license identifiers with hatchling |
| Python Version Requirements | 34–43 | Best practices for requires-python without upper caps |
| Dependencies | 44–58 | Appropriate version constraints for dependencies |
| Classifiers | 59–81 | Important PyPI classifiers for scientific packages |
| Optional Dependencies (Extras) | 82–111 | Defining and installing optional scientific features |
| Development Dependencies (Dependency Groups) | 112–153 | Using PEP 735 dependency groups for dev tools |

---

## License (Modern SPDX Format)

Use SPDX identifiers (supported by hatchling>=1.26):

```toml
[project]
license = "BSD-3-Clause"
license-files = ["LICENSE"]
```

Common scientific licenses:
- `MIT` - Permissive, simple
- `BSD-3-Clause` - Permissive, commonly used in science
- `Apache-2.0` - Permissive, explicit patent grant
- `GPL-3.0-or-later` - Copyleft

**Do not include License classifiers if using the `license` field.**

## Python Version Requirements

**Best practice**: Specify minimum version only, no upper cap:

```toml
requires-python = ">=3.9"
```

This allows pip to back-solve for old package versions when needed.

## Dependencies

**Use appropriate version constraints:**

```toml
dependencies = [
    "numpy>=1.20",              # Minimum version
    "scipy>=1.7,<2.0",          # Compatible range (use sparingly)
    "pandas>=1.3",              # Open-ended (preferred)
    "matplotlib>=3.4",          # Minimum version
]
```

**Avoid pinning exact versions unless absolutely necessary.**

## Classifiers

Important classifiers for scientific packages:

```toml
classifiers = [
    "Development Status :: 4 - Beta",
    "Intended Audience :: Science/Research",
    "License :: OSI Approved :: BSD License",
    "Programming Language :: Python :: 3",
    "Programming Language :: Python :: 3.9",
    "Programming Language :: Python :: 3.10",
    "Programming Language :: Python :: 3.11",
    "Programming Language :: Python :: 3.12",
    "Programming Language :: Python :: 3.13",
    "Topic :: Scientific/Engineering",
    "Topic :: Scientific/Engineering :: Physics",
    "Typing :: Typed",
]
```

[Browse all classifiers](https://pypi.org/classifiers/)

## Optional Dependencies (Extras)

Use extras for optional scientific features:

```toml
[project.optional-dependencies]
plotting = [
    "matplotlib>=3.4",
    "seaborn>=0.11",
]
ml = [
    "scikit-learn>=1.0",
    "xgboost>=1.5",
]
parallel = [
    "dask[array]>=2021.0",
    "joblib>=1.0",
]
all = [
    "my-sci-package[plotting,ml,parallel]",
]
```

**Install with extras:**
```bash
pip install my-sci-package[plotting]
pip install my-sci-package[plotting,ml]
pip install my-sci-package[all]
```

## Development Dependencies (Dependency Groups)

Use `dependency-groups` (PEP 735) instead of extras for development tools:

```toml
[dependency-groups]
test = [
    "pytest>=7.0",
    "pytest-cov>=4.0",
    "hypothesis>=6.0",
]
docs = [
    "sphinx>=5.0",
    "numpydoc>=1.5",
    "sphinx-gallery>=0.11",
]
dev = [
    {include-group = "test"},
    {include-group = "docs"},
    "ruff>=0.1",
    "mypy>=1.0",
]
```

**Install dependency groups:**
```bash
# Using uv (recommended)
uv pip install --group dev

# Using pip 25.1+
pip install --group dev

# Traditional approach with editable install
pip install -e ".[dev]"  # if using extras
```

**Advantages over extras:**
- Formally standardized
- More composable
- Not available on PyPI (development-only)
- Installed by default with `uv`
