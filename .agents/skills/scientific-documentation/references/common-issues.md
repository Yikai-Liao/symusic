# Common Documentation Issues and Solutions — Deep Reference

Troubleshooting guide for common problems when building scientific Python documentation.

## Contents

| Section | Lines | Description |
|---------|-------|-------------|
| Build Errors | 26–81 | ImportError, toctree warnings, and extension import failures |
| Autodoc Issues | 82–131 | Failed imports, missing docstrings, and type hint problems |
| Math Rendering Issues | 132–169 | MathJax configuration and LaTeX syntax fixes |
| Intersphinx Issues | 170–206 | External link resolution and ambiguous references |
| Theme Issues | 207–240 | Theme loading failures and custom CSS issues |
| Link Checking | 241–272 | False positive link checks and local file links |
| Notebook Issues | 273–327 | Rendering, execution, and output size problems |
| Read the Docs Issues | 328–377 | RTD build failures and missing dependencies |
| Performance Issues | 378–437 | Slow builds and large documentation optimization |
| Version-Specific Issues | 438–461 | numpy.typing and Sphinx 7.x compatibility |
| Debugging Tips | 462–498 | Verbose output, validation, and clean builds |
| Quick Reference | 499–538 | Common commands and useful conf.py settings |
| Getting Help | 539–549 | Documentation, forums, and community resources |
| Resources | 550–554 | Links to Sphinx FAQ, RTD, and nbsphinx troubleshooting |

---

## Build Errors

### ImportError: No module named 'package'

**Problem**: Sphinx cannot import your package during autodoc

**Solution 1**: Install package in editable mode
```bash
pip install -e .
```

**Solution 2**: Add to `conf.py`
```python
import sys
from pathlib import Path

sys.path.insert(0, str(Path("..").resolve() / "src"))
```

**Solution 3**: Mock imports
```python
autodoc_mock_imports = ["expensive_dependency", "optional_package"]
```

### WARNING: document isn't included in any toctree

**Problem**: File exists but not referenced in table of contents

**Solution**: Add to `toctree` directive
```rst
.. toctree::

   missing-page
   another-page
```

Or suppress warning:
```python
# conf.py
suppress_warnings = ["toc.not_readable"]
```

### Extension error: Could not import extension

**Problem**: Required extension not installed

**Solution**:
```bash
pip install sphinx-extension-name
```

Check installed extensions:
```bash
python -c "import sphinx_extension_name; print(sphinx_extension_name.__version__)"
```

## Autodoc Issues

### WARNING: autodoc: failed to import module

**Problem**: Module cannot be imported

**Diagnosis**:
```bash
python -c "import mypackage.module"
```

**Solutions**:
1. Check module path in `conf.py`
2. Verify package is installed
3. Check for circular imports
4. Mock dependencies: `autodoc_mock_imports = ["dependency"]`

### No docstrings appear in documentation

**Problem**: Autodoc not generating content

**Check**:
1. Docstrings exist in source code
2. Napoleon extension enabled (for NumPy-style)
3. Correct autodoc directive syntax

**Solution**:
```rst
.. automodule:: mypackage
   :members:          # Add this
   :undoc-members:    # Show members without docstrings
```

### Type hints break documentation

**Problem**: Complex type hints cause errors

**Solution**: Use `sphinx-autodoc-typehints`
```bash
pip install sphinx-autodoc-typehints
```

```python
# conf.py
extensions = ["sphinx_autodoc_typehints"]

typehints_fully_qualified = False
always_document_param_types = True
```

## Math Rendering Issues

### Math not rendering

**Problem**: LaTeX equations don't display

**Solution 1**: Check MathJax configuration
```python
# conf.py
extensions = ["sphinx.ext.mathjax"]

mathjax_path = "https://cdn.jsdelivr.net/npm/mathjax@3/es5/tex-mml-chtml.js"
```

**Solution 2**: Verify syntax
```rst
Inline: :math:`E = mc^2`

Display:

.. math::

   E = mc^2
```

### LaTeX errors in math

**Problem**: `! Undefined control sequence`

**Common fixes**:
```rst
Use double backslash:
:math:`\\alpha`  # Not \alpha

Escape underscores:
:math:`x\_i`  # Not x_i in text mode
```

## Intersphinx Issues

### External links not working

**Problem**: `:class:`numpy.ndarray`` doesn't link

**Diagnosis**:
```bash
python -m sphinx.ext.intersphinx https://numpy.org/doc/stable/objects.inv
```

**Solution**: Add to intersphinx mapping
```python
intersphinx_mapping = {
    "numpy": ("https://numpy.org/doc/stable/", None),
}
```

Increase timeout:
```python
intersphinx_timeout = 30
```

### Ambiguous references

**Problem**: Multiple objects with same name

**Solution**: Use fully qualified names
```rst
:class:`mypackage.submodule.ClassName`
```

Or use explicit inventory:
```rst
:class:`numpy:numpy.ndarray`
```

## Theme Issues

### Theme not loading

**Problem**: `html_theme = "pydata_sphinx_theme"` fails

**Solution**:
```bash
pip install pydata-sphinx-theme
```

Verify installation:
```python
import pydata_sphinx_theme
print(pydata_sphinx_theme.get_html_theme_path())
```

### Custom CSS not applied

**Problem**: Changes to `_static/custom.css` don't appear

**Solution**: Add to `conf.py`
```python
html_static_path = ["_static"]

def setup(app):
    app.add_css_file("custom.css")
```

Force rebuild:
```bash
sphinx-build -E -a docs docs/_build/html
```

## Link Checking

### linkcheck reports false positives

**Problem**: Valid links marked as broken

**Solution**: Ignore specific patterns
```python
# conf.py
linkcheck_ignore = [
    r"http://localhost",
    r"https://doi.org/.*",  # DOI links often block bots
    "https://example.com/requires-auth",
]

linkcheck_timeout = 30
linkcheck_retries = 3
```

### Links to local files broken

**Problem**: `file://` links don't work

**Solution**: Use relative paths instead
```rst
# Bad
file:///path/to/file.txt

# Good
:download:`file.txt <../../path/to/file.txt>`
```

## Notebook Issues

### Notebooks not rendering

**Problem**: `.ipynb` files don't appear

**Solution**: Check nbsphinx configuration
```python
# conf.py
extensions = ["nbsphinx"]

# Exclude checkpoints
exclude_patterns = ["_build", "**.ipynb_checkpoints"]
```

### Notebook execution fails

**Problem**: `nbsphinx` can't execute notebooks

**Solution 1**: Use existing outputs
```python
nbsphinx_execute = "never"
```

**Solution 2**: Fix kernel
```bash
python -m ipykernel install --user --name myenv
```

```python
nbsphinx_kernel_name = "myenv"
```

**Solution 3**: Increase timeout
```python
nbsphinx_timeout = 300  # 5 minutes
```

### Notebook outputs too large

**Problem**: Build process slow/fails

**Solution**: Limit output size
```python
nbsphinx_execute_arguments = [
    "--InlineBackend.figure_formats={'svg'}",
    "--InlineBackend.rc={'figure.dpi': 96}",
]
```

Or clear outputs before committing:
```bash
jupyter nbconvert --clear-output --inplace notebook.ipynb
```

## Read the Docs Issues

### Build failing on RTD but works locally

**Problem**: Different environments

**Solution 1**: Check Python version
```yaml
# .readthedocs.yaml
build:
  os: ubuntu-24.04
  tools:
    python: "3.12"
```

**Solution 2**: Pin dependencies
```toml
# pyproject.toml
[project.optional-dependencies]
docs = [
    "sphinx>=7.0,<8.0",
    "sphinx-theme>=0.15",
]
```

**Solution 3**: Check build logs
- Go to readthedocs.org → Your Project → Builds
- Click on failed build
- Read full log for errors

### Missing dependencies on RTD

**Problem**: `ModuleNotFoundError` on RTD

**Solution**: Install package with docs extras
```yaml
# .readthedocs.yaml
build:
  jobs:
    post_install:
      - pip install .[docs]
```

Or use requirements file:
```yaml
python:
  install:
    - requirements: docs/requirements.txt
```

## Performance Issues

### Slow documentation builds

**Problem**: `sphinx-build` takes too long

**Solutions**:

**1. Disable notebook execution**:
```python
nbsphinx_execute = "never"
```

**2. Use autosummary sparingly**:
```python
autosummary_generate = False  # Generate manually when needed
```

**3. Mock expensive imports**:
```python
autodoc_mock_imports = ["tensorflow", "torch", "expensive_lib"]
```

**4. Reduce API docs scope**:
```rst
.. automodule:: mypackage
   :members: public_function  # Only specific members
   :no-inherited-members:     # Skip inherited
```

**5. Use parallel builds**:
```bash
sphinx-build -j auto docs docs/_build/html
```

### Large documentation size

**Problem**: `_build` directory too large

**Solutions**:

**1. Optimize images**:
```python
# For matplotlib in notebooks
import matplotlib.pyplot as plt
plt.savefig("plot.png", dpi=96, bbox_inches="tight")
```

**2. Use SVG for plots**:
```python
%config InlineBackend.figure_formats = ['svg']
```

**3. Minify HTML** (MkDocs):
```yaml
plugins:
  - minify:
      minify_html: true
```

## Version-Specific Issues

### numpy.typing not recognized (Python < 3.9)

**Problem**: `import numpy.typing` fails

**Solution**: Conditional import
```python
try:
    from numpy.typing import ArrayLike
except ImportError:
    from numpy import ndarray as ArrayLike
```

### Sphinx 7.x compatibility

**Problem**: Extensions break with Sphinx 7

**Solution**: Check compatibility
```bash
pip install "sphinx>=7.0"
pip install --upgrade sphinx-extension-name
```

## Debugging Tips

### Enable verbose output

```bash
sphinx-build -v -v docs docs/_build/html  # Very verbose
```

### Check specific builder

```bash
sphinx-build -b dummy docs docs/_build/dummy  # Check parsing only
```

### Validate reStructuredText

```bash
pip install restructuredtext-lint
rst-lint docs/*.rst
```

### Clean builds

```bash
# Remove build directory
rm -rf docs/_build

# Full rebuild
sphinx-build -E -a docs docs/_build/html
```

### Test autosummary generation

```bash
sphinx-autogen docs/*.rst
```

## Quick Reference

### Common Commands

```bash
# Build docs
sphinx-build -W docs docs/_build/html

# Build with Nox
nox -s docs

# Serve locally
python -m http.server -d docs/_build/html

# Check links
sphinx-build -b linkcheck docs docs/_build/linkcheck

# Run doctests
sphinx-build -b doctest docs docs/_build/doctest

# Live reload
sphinx-autobuild docs docs/_build/html
```

### Useful conf.py settings

```python
# Fail on any warning
nitpicky = True
nitpick_ignore = [
    ("py:class", "optional"),  # Ignore specific warnings
]

# Show full traceback on error
show_warning_types = True

# Keep going on errors
keep_going = True
```

## Getting Help

If issues persist:

1. **Check Sphinx documentation**: https://www.sphinx-doc.org/
2. **Search Stack Overflow**: Tag `python-sphinx`
3. **GitHub Issues**: Check extension repositories
4. **Community forums**:
   - https://discuss.python.org/
   - https://discourse.sphinx-doc.org/

## Resources

- [Sphinx FAQ](https://www.sphinx-doc.org/en/master/faq.html)
- [Read the Docs Troubleshooting](https://docs.readthedocs.io/en/stable/guides/troubleshooting.html)
- [nbsphinx Troubleshooting](https://nbsphinx.readthedocs.io/en/latest/troubleshooting.html)
