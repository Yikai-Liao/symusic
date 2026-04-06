---
name: code-quality-tools
description: Configure and use automated code quality tools (ruff, mypy, pre-commit) for scientific Python projects. Covers linting rules, type checking configuration, formatting, and CI integration.
metadata:
  assets:
    - assets/pre-commit-config.yaml
    - assets/pyproject-ruff-mypy.toml
  references:
    - references/common-issues.md
    - references/configuration-patterns.md
    - references/type-hints.md
---

# Code Quality Tools for Scientific Python

Master the essential code quality tools that keep scientific Python projects maintainable, consistent, and error-free. Learn how to configure **ruff** for lightning-fast linting and formatting, **mypy** for static type checking, and **pre-commit** hooks for automated quality gates. These tools help catch bugs early, enforce consistent style across teams, and make code reviews focus on logic rather than formatting.

**Key Tools:**
- **Ruff**: Ultra-fast Python linter and formatter (replaces flake8, black, isort, and more)
- **MyPy**: Static type checker for Python
- **Pre-commit**: Git hook framework for automated checks

## Quick Reference Card

### Installation & Setup
```bash
# Using uv (recommended for scientific projects)
uv add --dev ruff mypy pre-commit

# Or install into the active virtual environment
uv pip install ruff mypy pre-commit

# Initialize pre-commit
pre-commit install
```

### Essential Ruff Commands
```bash
# Check code (linting)
ruff check .

# Fix auto-fixable issues
ruff check --fix .

# Format code
ruff format .

# Check and format together
ruff check --fix . && ruff format .
```

### Essential MyPy Commands
```bash
# Type check entire project
mypy src/

# Type check with strict mode
mypy --strict src/

# Type check specific file
mypy src/mymodule/analysis.py

# Generate type coverage report
mypy --html-report mypy-report src/
```

### Essential Pre-commit Commands
```bash
# Run all hooks on all files
pre-commit run --all-files

# Run hooks on staged files only
pre-commit run

# Update hook versions
pre-commit autoupdate

# Skip hooks temporarily (not recommended)
git commit --no-verify
```

### Quick Decision Tree

```
Need to enforce code style and catch common errors?
  YES → Use Ruff (linting + formatting)
  NO → Skip to type checking

Want to catch type-related bugs before runtime?
  YES → Add MyPy
  NO → Ruff alone is sufficient

Need to ensure checks run automatically?
  YES → Set up pre-commit hooks
  NO → Run tools manually (not recommended for teams)

Working with legacy code without type hints?
  YES → Start with Ruff only, add MyPy gradually
  NO → Use both Ruff and MyPy from the start
```

## When to Use This Skill

Use this skill to establish or improve code quality practices in scientific Python projects:

- Starting a new scientific Python project and want to establish code quality standards from day one
- Maintaining existing research code that needs consistency and error prevention
- Collaborating with multiple contributors who need automated style enforcement
- Preparing code for publication or package distribution
- Catching bugs early through static type checking before runtime
- Automating code reviews to focus on logic rather than style
- Integrating with CI/CD for automated quality checks
- Migrating from older tools like black, flake8, or isort to modern alternatives

## Core Concepts

### 1. Ruff: The All-in-One Linter and Formatter

**Ruff** is a blazingly fast Python linter and formatter written in Rust that replaces multiple tools you might be using today.

**What Ruff Replaces:**
- flake8 (linting)
- black (formatting)
- isort (import sorting)
- pyupgrade (syntax modernization)
- pydocstyle (docstring linting)
- And 50+ other tools

**Why Ruff for Scientific Python:**

Ruff is 10-100x faster than traditional tools, which matters when you have large codebases with thousands of lines of numerical code. Instead of managing multiple configuration files and tool versions, you get a single tool that handles everything. Ruff can auto-fix most issues automatically, saving time during development. It includes NumPy-aware docstring checking, understanding the conventions used throughout the scientific Python ecosystem. Best of all, it's compatible with existing black and flake8 configurations, making migration straightforward.

**Example:**
```python
# Before ruff format
import sys
import os
import numpy as np

def calculate_mean(data):
    return np.mean(data)

# After ruff format
import os
import sys

import numpy as np


def calculate_mean(data):
    return np.mean(data)
```

Ruff automatically organizes imports (standard library, third party, local) and applies consistent formatting.

### 2. MyPy: Static Type Checking

**MyPy** analyzes type hints to catch errors before your code ever runs. This is especially valuable in scientific computing where dimension mismatches and type errors can lead to subtle bugs in numerical calculations.

**Example of what MyPy catches:**

```python
import numpy as np
from numpy.typing import NDArray

def calculate_mean(data: NDArray[np.float64]) -> float:
    """Calculate mean of array."""
    return float(np.mean(data))

# MyPy catches this error at type-check time:
result: int = calculate_mean(np.array([1.0, 2.0, 3.0]))
# Error: Incompatible types (expression has type "float", variable has type "int")
```

**Benefits for Scientific Code:**

Type hints catch dimension mismatches in array operations before you run expensive computations. They validate function signatures, ensuring you pass the right types to numerical functions. Type hints serve as documentation, making it clear what types functions expect and return. They prevent None-related bugs that can crash long-running simulations. Modern IDEs use type hints to provide better autocomplete and inline documentation.

### 3. Pre-commit: Automated Quality Gates

**Pre-commit** runs checks automatically before each commit, ensuring code quality standards are maintained without manual intervention.

**Workflow:**
1. Developer runs `git commit`
2. Pre-commit automatically runs ruff, mypy, and other checks
3. If checks fail, commit is blocked
4. Developer fixes issues and commits again
5. Once all checks pass, commit succeeds

This ensures that code quality issues are caught immediately, before they enter the codebase.

## Configuration

See [assets/pyproject-ruff-mypy.toml](assets/pyproject-ruff-mypy.toml) for complete Ruff and MyPy configuration examples.

See [assets/pre-commit-config.yaml](assets/pre-commit-config.yaml) for pre-commit hook configuration.

## Configuration Patterns

See [references/configuration-patterns.md](references/configuration-patterns.md) for detailed patterns including:
- Basic Ruff configuration
- MyPy configuration for scientific Python
- Pre-commit configuration
- Ruff rule selection for scientific Python
- Fixing common Ruff warnings

## Type Hints

See [references/type-hints.md](references/type-hints.md) for type hint patterns including:
- Gradual type hint adoption
- NumPy array type hints
- Handling optional and union types

## Common Issues and Solutions

See [references/common-issues.md](references/common-issues.md) for solutions to:
- Ruff and Black formatting conflicts
- MyPy can't find imports
- Pre-commit hooks too slow
- Too many Ruff errors on legacy code
- Type hints break at runtime
- MyPy errors in test files
- Ruff conflicts with project style
- Pre-commit fails in CI

## Best Practices Checklist

### Initial Setup
- Install ruff, mypy, and pre-commit in dev environment
- Create `pyproject.toml` with tool configurations
- Create `.pre-commit-config.yaml`
- Run `pre-commit install` to enable git hooks
- Run `pre-commit run --all-files` to check existing code
- Document common quality commands with `uv run` or in project automation

### Configuration
- Set appropriate Python version target
- Enable NumPy-specific rules (NPY) for scientific code
- Configure NumPy-style docstring checking
- Set up per-file ignores for special cases (__init__.py, scripts)
- Configure mypy strictness appropriate for project maturity
- Install type stubs for third-party libraries

### Workflow Integration
- Add quality checks to CI/CD pipeline
- Document quality standards in CONTRIBUTING.md
- Create documented `uv run` workflows for common quality checks
- Set up IDE integration (VS Code, PyCharm)
- Configure editor to run ruff on save
- Add quality check badge to README

### Team Practices
- Run `ruff check --fix` before committing
- Run `ruff format` before committing
- Address mypy errors (don't use `# type: ignore` without reason)
- Review pre-commit failures before using `--no-verify`
- Keep pre-commit hooks updated (`pre-commit autoupdate`)
- Add type hints to new functions
- Gradually add types to existing code

### Maintenance
- Update ruff regularly (fast-moving project)
- Update pre-commit hook versions monthly
- Review and adjust ignored rules as project matures
- Increase mypy strictness gradually
- Monitor CI/CD for quality check failures
- Refactor code flagged by quality tools

## Resources and References

### Official Documentation
- **Ruff**: https://docs.astral.sh/ruff/
- **MyPy**: https://mypy.readthedocs.io/
- **Pre-commit**: https://pre-commit.com/
- **NumPy Typing**: https://numpy.org/devdocs/reference/typing.html

### Ruff Resources
- Rule reference: https://docs.astral.sh/ruff/rules/
- Configuration: https://docs.astral.sh/ruff/configuration/
- Editor integrations: https://docs.astral.sh/ruff/integrations/
- Migration guide: https://docs.astral.sh/ruff/faq/#how-does-ruff-compare-to-flake8

### MyPy Resources
- Type hints cheat sheet: https://mypy.readthedocs.io/en/stable/cheat_sheet_py3.html
- Common issues: https://mypy.readthedocs.io/en/stable/common_issues.html
- Running mypy: https://mypy.readthedocs.io/en/stable/running_mypy.html
- Type stubs: https://mypy.readthedocs.io/en/stable/stubs.html

### Pre-commit Resources
- Supported hooks: https://pre-commit.com/hooks.html
- Creating hooks: https://pre-commit.com/index.html#creating-new-hooks
- CI integration: https://pre-commit.ci/

### Scientific Python Resources
- Scientific Python Development Guide: https://learn.scientific-python.org/development/
- NumPy documentation style: https://numpydoc.readthedocs.io/
- Type hints for scientific code: https://numpy.org/devdocs/reference/typing.html

## Summary

Code quality tools are essential for maintaining scientific Python projects. Ruff provides fast, comprehensive linting and formatting. MyPy catches type errors before runtime. Pre-commit automates quality checks in your workflow.

**Key takeaways:**

Start with ruff for immediate impact as it replaces multiple tools with a single fast solution. Add mypy gradually as you add type hints to catch bugs early. Use pre-commit to enforce standards automatically without manual intervention. Integrate with uv for reproducible development environments. Configure tools in pyproject.toml for centralized management. Run quality checks in CI/CD to maintain standards across the team.

**Next steps:**

Set up ruff and pre-commit in your project today. Add type hints to new functions you write. Gradually increase mypy strictness as your codebase matures. Share configurations with your team for consistency. Integrate quality checks into your development workflow.

Quality tools save time by catching errors early and maintaining consistency across your scientific codebase. They make code reviews more productive by automating style discussions, allowing reviewers to focus on scientific correctness and algorithmic choices rather than formatting details.
