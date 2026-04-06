# Common Issues and Solutions — Deep Reference

## Contents

| Section | Lines | Description |
|---------|-------|-------------|
| Issue 1: Ruff and Black Formatting Conflicts | 18–32 | Resolving conflicts between ruff format and black |
| Issue 2: MyPy Can't Find Imports | 33–48 | Fixing missing import errors in mypy |
| Issue 3: Pre-commit Hooks Too Slow | 49–56 | Speeding up pre-commit on large codebases |
| Issue 4: Too Many Ruff Errors on Legacy Code | 57–72 | Gradual adoption strategy for ruff on existing code |
| Issue 5: Type Hints Break at Runtime | 73–87 | Using future annotations for Python < 3.10 |
| Issue 6: MyPy Errors in Test Files | 88–98 | Configuring mypy to be lenient with test files |
| Issue 7: Ruff Conflicts with Project Style | 99–108 | Customizing ruff formatting preferences |
| Issue 8: Pre-commit Fails in CI | 109–124 | Ensuring consistent pre-commit environments in CI |

---

## Issue 1: Ruff and Black Formatting Conflicts

**Problem:** Using both ruff format and black causes conflicts.

**Solution:** Choose one formatter. Ruff format is compatible with black's style:
```toml
[tool.ruff.format]
# Use black-compatible formatting
quote-style = "double"
indent-style = "space"
line-ending = "auto"
```

Remove black from dependencies and pre-commit hooks.

## Issue 2: MyPy Can't Find Imports

**Problem:** `error: Cannot find implementation or library stub for module named 'scipy'`

**Solution:** Install type stubs or ignore missing imports:
```toml
[[tool.mypy.overrides]]
module = ["scipy.*", "matplotlib.*"]
ignore_missing_imports = true
```

Or install stubs:
```bash
uv add --dev types-requests types-PyYAML
```

## Issue 3: Pre-commit Hooks Too Slow

**Problem:** Pre-commit takes too long on large codebases.

**Solution:**

Use ruff instead of multiple tools (much faster). Limit hooks to staged files only (default behavior). Skip expensive checks in pre-commit, run in CI instead by removing mypy from `.pre-commit-config.yaml` and keeping it in CI workflow.

## Issue 4: Too Many Ruff Errors on Legacy Code

**Problem:** Hundreds of ruff errors on existing codebase.

**Solution:** Gradual adoption strategy:
```bash
# 1. Start with auto-fixable issues only
ruff check --fix .

# 2. Add baseline to ignore existing issues
ruff check --add-noqa .

# 3. Fix new code going forward
# 4. Gradually remove # noqa comments
```

## Issue 5: Type Hints Break at Runtime

**Problem:** Code with type hints fails with `NameError` in Python < 3.10.

**Solution:** Use `from __future__ import annotations`:
```python
from __future__ import annotations  # Must be first import

import numpy as np

def process(data: np.ndarray) -> np.ndarray:
    """This works in Python 3.7+"""
    return data * 2
```

## Issue 6: MyPy Errors in Test Files

**Problem:** MyPy complains about pytest fixtures and dynamic test generation.

**Solution:** Configure mypy to be lenient with tests:
```toml
[[tool.mypy.overrides]]
module = "tests.*"
disallow_untyped_defs = false
```

## Issue 7: Ruff Conflicts with Project Style

**Problem:** Team prefers single quotes, but ruff uses double quotes.

**Solution:** Configure ruff to match team preferences:
```toml
[tool.ruff.format]
quote-style = "single"
```

## Issue 8: Pre-commit Fails in CI

**Problem:** Pre-commit hooks pass locally but fail in CI.

**Solution:** Ensure consistent environments:
```yaml
# In CI, use same Python version and dependencies
- name: Set up Python
  uses: actions/setup-python@v5
  with:
    python-version: "3.11"  # Match local version

# Or use pre-commit's CI action
- uses: pre-commit/action@v3.0.0
```
