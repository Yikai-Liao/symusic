---
name: python-packaging
description: Create and publish distributable scientific Python packages following Scientific Python community best practices. Covers pyproject.toml, src layout, Hatchling, metadata, CLI entry points, and PyPI publishing.
metadata:
  assets:
    - assets/pyproject-minimal.toml
    - assets/pyproject-full-featured.toml
    - assets/readme-template.md
    - assets/github-actions-publish.yml
    - assets/sphinx-conf.py
    - assets/.gitignore
  references:
    - references/common-issues.md
    - references/docstrings.md
    - references/metadata.md
    - references/patterns.md
  scripts:
    - scripts/cli-example.py
---

# Scientific Python Packaging

A comprehensive guide to creating, structuring, and distributing Python packages for scientific computing, following the [Scientific Python Community guidelines](https://learn.scientific-python.org/development/guides/packaging-simple/). This skill focuses on modern packaging standards using `pyproject.toml`, PEP 621 metadata, and the Hatchling build backend.

## Quick Decision Tree

**Package Structure Selection:**
```
START
  ├─ Pure Python scientific package (most common) → Pattern 1 (src/ layout)
  ├─ Need data files with package → Pattern 2 (data/ subdirectory)
  ├─ CLI tool → Pattern 5 (add [project.scripts])
  └─ Complex multi-feature package → Pattern 3 (full-featured)
```

**Build Backend Choice:**
```
START → Use Hatchling (recommended for scientific Python)
  ├─ Need VCS versioning? → Add hatch-vcs plugin
  ├─ Simple manual versioning? → version = "X.Y.Z" in pyproject.toml
  └─ Dynamic from __init__.py? → [tool.hatch.version] path
```

**Dependency Management:**
```
START
  ├─ Runtime dependencies → [project] dependencies
  ├─ Optional features → [project.optional-dependencies]
  ├─ Development tools → [dependency-groups] (PEP 735)
  └─ Version constraints → Use >= for minimum, avoid upper caps
```

**Publishing Workflow:**
```
1. Build: python -m build
2. Check: twine check dist/*
3. Test: twine upload --repository testpypi dist/*
4. Verify: pip install --index-url https://test.pypi.org/simple/ pkg
5. Publish: twine upload dist/*
```

**Common Task Quick Reference:**
```bash
# Setup new package
mkdir -p my-pkg/src/my_pkg && cd my-pkg
# Create pyproject.toml with [build-system] and [project] sections

# Development install
pip install -e . --group dev

# Build distributions
python -m build

# Test installation
pip install dist/*.whl

# Publish
twine upload dist/*
```

## When to Use This Skill

- Creating scientific Python libraries for distribution
- Building research software packages with proper structure
- Publishing scientific packages to PyPI
- Setting up reproducible scientific Python projects
- Creating installable packages with scientific dependencies
- Implementing command-line tools for scientific workflows
- Following community standards for scientific Python development
- Preparing packages for peer review and publication

## Core Concepts

### 1. Modern Build Systems

Python packages now use standardized build systems instead of classic `setup.py`:

- **PEP 621**: Standardized project metadata in `pyproject.toml`
- **PEP 517/518**: Build system independence
- **Build backend**: Hatchling
- **No classic files**: No `setup.py`, `setup.cfg`, or `MANIFEST.in`

### 2. Build Backend: Hatchling

- **Hatchling**: Excellent balance of speed, configurability, and extendability
- Modern, standards-compliant build backend
- Automatic package discovery in `src/` layout
- VCS-aware file inclusion for SDists
- Extensible through plugins

### 3. Package Structure

- **src/ layout**: Required for proper isolation (prevents importing uninstalled code)
- **Automatic discovery**: Hatchling auto-detects packages in `src/`
- **Standard structure**: Consistent organization for testing and documentation

### 4. Scientific Python Standards

- **Dependency management**: Careful version constraints
- **Python version support**: Minimum version without upper caps
- **Development dependencies**: Use dependency-groups (PEP 735)
- **Documentation**: Include README, LICENSE, and docs folder
- **Testing**: Dedicated tests folder

## Quick Start

### Minimal Scientific Package Structure

```
my-sci-package/
├── pyproject.toml
├── README.md
├── LICENSE
├── src/
│   └── my_sci_package/
│       ├── __init__.py
│       ├── analysis.py
│       └── utils.py
├── tests/
│   ├── test_analysis.py
│   └── test_utils.py
└── docs/
    └── index.md
```

See [assets/pyproject-minimal.toml](assets/pyproject-minimal.toml) for a complete minimal `pyproject.toml` template.

## Package Structure Patterns

See [references/patterns.md](references/patterns.md) for detailed package structure patterns including:
- Pure Python scientific package (recommended)
- Scientific package with data files
- Versioning strategies
- Building and publishing workflows
- Testing installation

## Project Metadata

See [references/metadata.md](references/metadata.md) for detailed information on:
- License configuration (SPDX format)
- Python version requirements
- Dependency management
- Classifiers
- Optional dependencies (extras)
- Development dependencies (dependency groups)

## Command-Line Interface

For CLI tool implementation, see [scripts/cli-example.py](scripts/cli-example.py) for a complete example using Click.

**Register in pyproject.toml:**
```toml
[project.scripts]
sci-analyze = "my_sci_package.cli:main"
```

## File Templates

Ready-to-use templates are available in the `assets/` directory:

- **[assets/.gitignore](assets/.gitignore)** - `.gitignore` for scientific Python packages
- **[assets/pyproject-minimal.toml](assets/pyproject-minimal.toml)** - Minimal `pyproject.toml` template
- **[assets/pyproject-full-featured.toml](assets/pyproject-full-featured.toml)** - Full-featured `pyproject.toml` with all options
- **[assets/readme-template.md](assets/readme-template.md)** - README template for scientific packages
- **[assets/sphinx-conf.py](assets/sphinx-conf.py)** - Sphinx documentation configuration
- **[assets/github-actions-publish.yml](assets/github-actions-publish.yml)** - GitHub Actions workflow for publishing

## Documentation

### NumPy-style Docstrings

See [references/docstrings.md](references/docstrings.md) for examples of NumPy-style docstrings and documentation best practices.

## Checklist for Publishing Scientific Packages

- [ ] Code is tested with pytest (>90% coverage recommended)
- [ ] Documentation is complete (README, docstrings, Sphinx docs)
- [ ] Version number follows semantic versioning
- [ ] CHANGELOG.md or NEWS.md updated
- [ ] LICENSE file included with appropriate license
- [ ] pyproject.toml has complete metadata
- [ ] Package uses src/ layout
- [ ] Package builds without errors (`python -m build`)
- [ ] SDist contents verified (`tar -tvf dist/*.tar.gz`)
- [ ] Installation tested in clean environment
- [ ] CLI tools work if applicable
- [ ] All classifiers are appropriate
- [ ] Python version constraint is correct (no upper bound)
- [ ] Dependencies have appropriate version constraints
- [ ] Repository is linked in project.urls
- [ ] Tested on TestPyPI first
- [ ] GitHub release created (if using)
- [ ] Documentation published (ReadTheDocs, GitHub Pages)
- [ ] Citation information included (CITATION.cff or README)

## Best Practices for Scientific Python Packages

1. **Use src/ layout** - Prevents importing uninstalled code, ensures proper testing
2. **Use pyproject.toml** - Modern standard, tool-independent configuration
3. **Use Hatchling** - Modern, fast, and configurable build backend
4. **No classic files** - Avoid setup.py, setup.cfg, MANIFEST.in
5. **Version constraints** - Minimum versions for dependencies, no upper cap for Python
6. **Test SDist contents** - Always verify what files are included/excluded
7. **Use TestPyPI** - Always test publishing before going to production
8. **Document thoroughly** - README, docstrings, Sphinx documentation
9. **Include LICENSE** - Use SPDX identifiers, choose appropriate scientific license
10. **Use dependency-groups** - For development dependencies (PEP 735)
11. **Semantic versioning** - Clear versioning strategy
12. **Automate CI/CD** - GitHub Actions for testing and publishing
13. **Type hints** - Include py.typed marker for typed packages
14. **Citation information** - Make it easy for users to cite your work
15. **Community standards** - Follow Scientific Python guidelines

## Common Issues and Solutions

See [references/common-issues.md](references/common-issues.md) for solutions to:
- Import errors in tests
- Missing files in distribution
- Dependency conflicts
- Python version incompatibility

## Resources

- **Scientific Python Development Guide**: https://learn.scientific-python.org/development/
- **Simple Packaging Guide**: https://learn.scientific-python.org/development/guides/packaging-simple/
- **Python Packaging Guide**: https://packaging.python.org/
- **PyPI**: https://pypi.org/
- **TestPyPI**: https://test.pypi.org/
- **Hatchling documentation**: https://hatch.pypa.io/latest/
- **build**: https://pypa-build.readthedocs.io/
- **twine**: https://twine.readthedocs.io/
- **Scientific Python Cookie**: https://github.com/scientific-python/cookie
- **NumPy documentation style**: https://numpydoc.readthedocs.io/
