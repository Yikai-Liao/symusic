# Common Issues and Solutions — Deep Reference

## Contents

| Section | Lines | Description |
|---------|-------|-------------|
| Issue: Import errors in tests | 14–19 | Fixing test imports with src layout and editable installs |
| Issue: Missing files in distribution | 20–28 | Ensuring data files and docs are included in SDist/wheel |
| Issue: Dependency conflicts | 29–34 | Avoiding incompatible dependency versions for users |
| Issue: Python version incompatibility | 35–40 | Supporting multiple Python versions without capping |

---

## Issue: Import errors in tests

**Problem**: Tests import the source code instead of installed package.

**Solution**: Use src/ layout and install package with `pip install -e .`

## Issue: Missing files in distribution

**Problem**: Data files or documentation not included in SDist/wheel.

**Solution**:
- For Hatchling: VCS ignore file controls SDist contents
- Check with: `tar -tvf dist/*.tar.gz`
- Explicitly configure if needed in `[tool.hatch.build]`

## Issue: Dependency conflicts

**Problem**: Users cannot install due to incompatible dependency versions.

**Solution**: Use minimal version constraints, avoid upper bounds on dependencies.

## Issue: Python version incompatibility

**Problem**: Package doesn't work on newer Python versions.

**Solution**: Don't cap `requires-python`, test on multiple Python versions with CI.
