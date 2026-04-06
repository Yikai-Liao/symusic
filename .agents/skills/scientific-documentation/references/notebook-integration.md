# Jupyter Notebook Integration in Documentation — Deep Reference

Guide to integrating Jupyter notebooks into Sphinx and MkDocs documentation for scientific Python packages.

## Contents

| Section | Lines | Description |
|---------|-------|-------------|
| Why Include Notebooks? | 20–27 | Benefits of including notebooks in documentation |
| Sphinx Integration | 28–159 | nbsphinx and MyST-NB setup, execution control, custom rendering |
| MkDocs Integration | 160–218 | mkdocs-jupyter plugin setup and notebook execution |
| Best Practices | 219–378 | Notebook structure, code cells, data management, dependencies |
| Gallery Integration | 379–430 | Using sphinx-gallery with Python scripts |
| CI/CD Integration | 431–488 | Testing notebooks in GitHub Actions and pytest |
| Common Issues | 489–540 | Troubleshooting execution, output, and build failures |
| Resources | 541–547 | Links to nbsphinx, MyST-NB, mkdocs-jupyter, and more |

---

## Why Include Notebooks?

Jupyter notebooks are ideal for:
- **Interactive tutorials**: Step-by-step learning experiences
- **Computational narratives**: Combining code, text, and visualizations
- **Reproducible examples**: Complete working examples with outputs
- **Gallery of applications**: Showcase package capabilities

## Sphinx Integration

### Using nbsphinx

**Installation**:
```bash
pip install nbsphinx
```

**Configuration in `conf.py`**:
```python
extensions = [
    # ... other extensions
    "nbsphinx",
]

# Execution settings
nbsphinx_execute = "auto"  # Options: "auto", "always", "never"
nbsphinx_allow_errors = False
nbsphinx_timeout = 60  # seconds

# Kernel settings
nbsphinx_kernel_name = "python3"

# Custom CSS/JS
nbsphinx_prolog = """
{% set docname = env.doc2path(env.docname, base=None) %}

.. note::
   This page was generated from `{{ docname }}`_.
   Download it :download:`here <{{ docname }}>`.
"""

# Exclude checkpoint files
exclude_patterns = [
    "_build",
    "**.ipynb_checkpoints",
]
```

**Include notebooks in documentation**:
```rst
.. toctree::
   :maxdepth: 2

   notebooks/tutorial
   notebooks/advanced_analysis
   notebooks/visualization
```

### Execution Control

**Never execute** (use existing outputs):
```python
nbsphinx_execute = "never"
```

**Always execute** (regenerate all outputs):
```python
nbsphinx_execute = "always"
```

**Auto-execute** (execute only if no outputs):
```python
nbsphinx_execute = "auto"
```

**Per-notebook control** (in notebook metadata):
```json
{
  "nbsphinx": {
    "execute": "never"
  }
}
```

### Custom Rendering

**Hide input cells**:
Add to notebook cell metadata:
```json
{
  "nbsphinx": "hidden"
}
```

**Hide output cells**:
```json
{
  "tags": ["hide-output"]
}
```

**Custom cell classes**:
```json
{
  "nbsphinx": {
    "cell_class": "warning"
  }
}
```

### Using MyST-NB

**Alternative: MyST-NB for MyST Markdown**:

```bash
pip install myst-nb
```

**Configuration**:
```python
extensions = [
    "myst_nb",
]

nb_execution_mode = "auto"  # or "cache", "off"
nb_execution_timeout = 60

# Merge stderr/stdout streams
nb_merge_streams = True

# Execute notebooks in temporary directory
nb_execution_in_temp = True
```

**Advantages over nbsphinx**:
- MyST Markdown support
- Better caching
- More execution control
- Integrated with Jupyter Book

## MkDocs Integration

### Using mkdocs-jupyter

**Installation**:
```bash
pip install mkdocs-jupyter
```

**Configuration in `mkdocs.yml`**:
```yaml
plugins:
  - mkdocs-jupyter:
      include_source: true  # Show download link
      execute: false  # Don't execute (use existing outputs)
      allow_errors: false
      kernel_name: python3
      theme: light  # or "dark"
      include_requirejs: true
```

**Include notebooks**:
```yaml
nav:
  - Tutorials:
      - Getting Started: notebooks/tutorial.ipynb
      - Advanced: notebooks/advanced.ipynb
```

### Execution in MkDocs

**Execute notebooks before build**:
```bash
# Execute all notebooks
jupyter nbconvert --to notebook --execute notebooks/*.ipynb --inplace

# Then build docs
mkdocs build
```

**Or use a script**:
```python
# scripts/execute_notebooks.py
from pathlib import Path
import nbformat
from nbconvert.preprocessors import ExecutePreprocessor

notebooks_dir = Path("docs/notebooks")
for nb_path in notebooks_dir.glob("*.ipynb"):
    with open(nb_path) as f:
        nb = nbformat.read(f, as_version=4)

    ep = ExecutePreprocessor(timeout=600, kernel_name="python3")
    ep.preprocess(nb, {"metadata": {"path": notebooks_dir}})

    with open(nb_path, "w") as f:
        nbformat.write(nb, f)
```

## Best Practices

### Notebook Structure

**Good notebook structure**:
```markdown
# Tutorial Title

## Learning Objectives
- Objective 1
- Objective 2

## Prerequisites
- Prerequisite 1
- Prerequisite 2

## Setup

[Installation and import code]

## Step 1: [Task]

[Explanation]

[Code cell]

[Output/visualization]

## Step 2: [Task]

...

## Summary

- What we learned
- Next steps
```

### Code Cells

**Do**:
- Keep cells focused on single concepts
- Add comments for complex code
- Show intermediate outputs
- Use meaningful variable names

**Don't**:
- Put too much code in one cell
- Use absolute file paths
- Rely on external files not in repo
- Leave debug/scratch code

### Data for Notebooks

**Option 1: Generate synthetic data**:
```python
import numpy as np
import pandas as pd

# Generate reproducible sample data
np.random.seed(42)
data = pd.DataFrame({
    'temperature': np.random.normal(20, 5, 100),
    'pressure': np.random.normal(1013, 10, 100),
})
```

**Option 2: Include small data files**:
```
docs/
├── notebooks/
│   ├── tutorial.ipynb
│   └── data/
│       └── sample.csv  # < 1MB
```

**Option 3: Download from URL**:
```python
from urllib.request import urlretrieve

url = "https://example.com/data.csv"
filename = "data.csv"

if not Path(filename).exists():
    urlretrieve(url, filename)

data = pd.read_csv(filename)
```

### Managing Dependencies

**List requirements in notebook**:
```python
# This notebook requires:
# - numpy >= 1.20
# - pandas >= 1.3
# - matplotlib >= 3.4
```

**Or check programmatically**:
```python
import importlib.metadata

required = {
    "numpy": "1.20",
    "pandas": "1.3",
    "matplotlib": "3.4",
}

for package, min_version in required.items():
    version = importlib.metadata.version(package)
    print(f"{package}: {version} (required: >={min_version})")
```

### Execution Time

**For long-running notebooks**:
```python
# Set longer timeout
nbsphinx_timeout = 300  # 5 minutes

# Or disable execution
nbsphinx_execute = "never"
```

**Add progress indicators**:
```python
from tqdm.notebook import tqdm

for i in tqdm(range(1000)):
    # Long computation
    pass
```

### Memory Management

**Clear memory between cells**:
```python
import gc

# After large computation
del large_array
gc.collect()
```

**Monitor memory**:
```python
import tracemalloc

tracemalloc.start()

# Your code here

current, peak = tracemalloc.get_traced_memory()
print(f"Current memory: {current / 10**6:.1f} MB")
print(f"Peak memory: {peak / 10**6:.1f} MB")

tracemalloc.stop()
```

## Gallery Integration

### Using sphinx-gallery

**Convert Python scripts to notebooks**:

```python
# examples/plot_example.py
"""
Example Analysis
================

This example demonstrates basic data analysis.
"""

import numpy as np
import matplotlib.pyplot as plt

# %%
# Generate data
x = np.linspace(0, 2*np.pi, 100)
y = np.sin(x)

# %%
# Create plot
plt.figure(figsize=(10, 6))
plt.plot(x, y)
plt.xlabel('x')
plt.ylabel('sin(x)')
plt.title('Sine Wave')
plt.show()
```

**Configuration**:
```python
from sphinx_gallery.sorting import FileNameSortKey

extensions = ["sphinx_gallery.gen_gallery"]

sphinx_gallery_conf = {
    "examples_dirs": "../examples",
    "gallery_dirs": "auto_examples",
    "filename_pattern": "/plot_",
    "backreferences_dir": "gen_modules/backreferences",
    "doc_module": "mypackage",
    "reference_url": {"mypackage": None},
    "download_all_examples": False,
    "notebook_images": True,  # Generate Jupyter notebooks
    "within_subsection_order": FileNameSortKey,
}
```

## CI/CD Integration

### Execute Notebooks in CI

**GitHub Actions example**:
```yaml
name: Test Notebooks

on: [push, pull_request]

jobs:
  test-notebooks:
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v3

      - name: Set up Python
        uses: actions/setup-python@v4
        with:
          python-version: "3.11"

      - name: Install dependencies
        run: |
          pip install .[docs]
          pip install pytest nbmake

      - name: Test notebooks
        run: |
          pytest --nbmake docs/notebooks/*.ipynb
```

### Using pytest-notebook

**Install**:
```bash
pip install pytest-notebook
```

**Test notebooks**:
```python
# tests/test_notebooks.py
import pytest
from pathlib import Path

NOTEBOOKS_DIR = Path("docs/notebooks")

@pytest.mark.parametrize(
    "notebook",
    list(NOTEBOOKS_DIR.glob("*.ipynb")),
    ids=lambda x: x.stem,
)
def test_notebook(notebook):
    """Test that notebook runs without errors."""
    # pytest-notebook will execute the notebook
    pass
```

## Common Issues

### Issue: Notebooks Don't Execute

**Cause**: Missing kernel or dependencies

**Solution**:
```bash
# Install ipykernel
pip install ipykernel

# Create kernel
python -m ipykernel install --user --name myenv

# Update nbsphinx config
nbsphinx_kernel_name = "myenv"
```

### Issue: Large Outputs

**Cause**: Notebook outputs are too large

**Solution**:
```python
# Limit output
import warnings
warnings.filterwarnings("ignore")

# Or in nbsphinx config
nbsphinx_execute_arguments = [
    "--InlineBackend.figure_formats={'svg', 'pdf'}",
    "--InlineBackend.rc={'figure.dpi': 96}",
]
```

### Issue: Notebooks Break Documentation Build

**Cause**: Notebook has errors

**Solution**:
```python
# Allow errors for specific notebooks
nbsphinx_allow_errors = True

# Or per-notebook in metadata
{
  "nbsphinx": {
    "allow_errors": true
  }
}
```

## Resources

- [nbsphinx Documentation](https://nbsphinx.readthedocs.io/)
- [MyST-NB Documentation](https://myst-nb.readthedocs.io/)
- [mkdocs-jupyter Plugin](https://github.com/danielfrg/mkdocs-jupyter)
- [Jupyter Book](https://jupyterbook.org/)
- [sphinx-gallery](https://sphinx-gallery.github.io/)
