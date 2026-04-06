---
name: scientific-documentation
description: Set up and maintain documentation for scientific Python packages. Covers Sphinx, MkDocs, NumPy-style docstrings, Diataxis framework, accessibility standards, and documentation hosting with Read the Docs.
metadata:
  assets:
    - assets/sphinx-conf-scientific.py
    - assets/mkdocs-scientific.yml
    - assets/noxfile-docs.py
    - assets/readthedocs.yaml
    - assets/index-template.md
  references:
    - references/accessible-documentation.md
    - references/common-issues.md
    - references/diataxis-framework.md
    - references/docstring-examples.md
    - references/notebook-integration.md
    - references/sphinx-extensions.md
  scripts:
    - scripts/generate-api-docs.py
---

# Scientific Python Documentation

A comprehensive guide to creating documentation for scientific Python packages following community best practices from the [Scientific Python Development Guide](https://learn.scientific-python.org/development/guides/docs/).

## Resources in This Skill

This skill includes extensive supporting materials for documentation tasks:

**References** (detailed guides):
- `references/diataxis-framework.md` - Complete Diátaxis framework guide with examples for all four documentation types
- `references/sphinx-extensions.md` - Detailed Sphinx extension configuration and usage
- `references/docstring-examples.md` - Comprehensive NumPy-style docstring examples for functions, classes, and modules
- `references/notebook-integration.md` - Jupyter notebook integration guide for Sphinx and MkDocs
- `references/common-issues.md` - Troubleshooting documentation build issues
- `references/accessible-documentation.md` - Accessibility guidelines for scientific documentation including images, color contrast, and alt text

**Assets** (ready-to-use templates):
- `assets/sphinx-conf-scientific.py` - Complete Sphinx `conf.py` template for scientific Python
- `assets/readthedocs.yaml` - Read the Docs configuration template
- `assets/mkdocs-scientific.yml` - MkDocs configuration template with Material theme
- `assets/noxfile-docs.py` - Nox automation sessions for documentation builds
- `assets/index-template.md` - Documentation landing page template

**Scripts**:
- `scripts/generate-api-docs.py` - Script to generate API documentation stubs

## Quick Decision Trees

### Documentation Framework Selection

Choose your documentation framework based on project needs:

**Sphinx** - Best for:
- LaTeX/math-heavy documentation
- Established scientific Python ecosystem integration
- Projects similar to NumPy, SciPy, Astropy
- Need for PDF output
- Extensive API documentation with autodoc

**MkDocs** - Best for:
- Markdown-native documentation
- Simpler setup with fewer plugins
- Projects similar to FastAPI, Typer
- Modern, clean aesthetics
- Rapid prototyping

**Jupyter Book** - Best for:
- Notebook-centric documentation
- Computational narratives and tutorials
- Interactive examples and visualizations
- Educational content
- Data science workflows

### Theme Selection

**For Sphinx:**

| Theme | Best For | Pros | Cons |
|-------|----------|------|------|
| `pydata-sphinx-theme` | Scientific Python standard | Dark mode, excellent navigation, ecosystem alignment | Requires configuration |
| `furo` | Modern projects | Clean design, excellent search, fast | Less customization |
| `sphinx_rtd_theme` | Traditional docs | Widely recognized, simple setup | Older design |
| `sphinx-book-theme` | Book-like docs | Beautiful, narrative-focused | Less suitable for API-heavy |

**For MkDocs:**

| Theme | Best For | Pros | Cons |
|-------|----------|------|------|
| `material` | Modern projects | Feature-rich, beautiful, customizable | Can be overwhelming |
| `readthedocs` | Simple needs | Familiar, straightforward | Basic features |

### Hosting Selection

| Platform | Best For | Pros | Cons |
|----------|----------|------|------|
| **Read the Docs** | Open source projects | Free, automatic builds, versioning | Limited customization |
| **GitHub Pages** | Simple static sites | Simple, integrated with GitHub | Manual build process |
| **Netlify** | Advanced needs | Preview deployments, control | Requires setup |

## Diátaxis Framework

The [Diátaxis framework](https://diataxis.fr/) organizes documentation into four categories based on user needs:

| Type | Purpose | Characteristics |
|------|---------|-----------------|
| **Tutorials** | Help newcomers learn by doing | Progressive difficulty, complete examples, encouraging tone |
| **How-to Guides** | Help users accomplish specific goals | Problem-focused, practical recipes, real-world scenarios |
| **Reference** | Provide technical specifications | Comprehensive, consistent, auto-generated from docstrings |
| **Explanation** | Help users understand concepts | Discursive, contextual, theory and design decisions |

**Key principle:** Don't mix documentation types. Keep tutorials separate from reference docs.

> For complete Diátaxis guidance with detailed examples and templates for each documentation type, see `references/diataxis-framework.md`.

## Sphinx Configuration for Scientific Python

### Essential Extensions

Create `docs/conf.py` with these core extensions:

```python
extensions = [
    # Core Sphinx extensions
    "sphinx.ext.autodoc",        # Auto-generate docs from docstrings
    "sphinx.ext.autosummary",    # Generate summary tables
    "sphinx.ext.viewcode",       # Add source code links
    "sphinx.ext.intersphinx",    # Link to other projects

    # Scientific Python extensions
    "sphinx.ext.napoleon",       # NumPy/Google docstrings
    "sphinx.ext.mathjax",        # Math rendering
    "numpydoc",                  # NumPy documentation style
    "sphinx_autodoc_typehints",  # Type hint integration

    # Markdown support
    "myst_parser",               # MyST Markdown parser

    # Notebook integration
    "nbsphinx",                  # Jupyter notebook rendering
]
```

### Extension Purposes

**autodoc** - Automatically generate documentation from docstrings:
```python
.. automodule:: mypackage.analysis
   :members:
   :undoc-members:
   :show-inheritance:
```

**autosummary** - Create summary tables:
```rst
.. autosummary::
   :toctree: generated/

   mypackage.function1
   mypackage.function2
```

**napoleon** - Parse NumPy-style docstrings (configure in `conf.py`):
```python
napoleon_google_docstring = False
napoleon_numpy_docstring = True
napoleon_include_init_with_doc = True
napoleon_use_param = True
napoleon_use_rtype = True
```

**intersphinx** - Link to external documentation:
```python
intersphinx_mapping = {
    "python": ("https://docs.python.org/3", None),
    "numpy": ("https://numpy.org/doc/stable/", None),
    "scipy": ("https://docs.scipy.org/doc/scipy/", None),
    "pandas": ("https://pandas.pydata.org/docs/", None),
    "matplotlib": ("https://matplotlib.org/stable/", None),
    "xarray": ("https://docs.xarray.dev/en/stable/", None),
    "sklearn": ("https://scikit-learn.org/stable/", None),
}
```

**myst_parser** - Use Markdown in Sphinx:
```python
myst_enable_extensions = [
    "colon_fence",    # ::: fences
    "deflist",        # Definition lists
    "dollarmath",     # $...$ and $$...$$ for math
    "fieldlist",      # Field lists
    "substitution",   # Variable substitution
    "tasklist",       # Task lists
]
```

### Theme Configuration

**PyData Sphinx Theme** (recommended for scientific Python):
```python
html_theme = "pydata_sphinx_theme"

html_theme_options = {
    "github_url": "https://github.com/org/package",
    "use_edit_page_button": True,
    "show_toc_level": 2,
    "navigation_with_keys": True,
    "icon_links": [
        {
            "name": "PyPI",
            "url": "https://pypi.org/project/your-package",
            "icon": "fas fa-box",
        },
    ],
}

html_context = {
    "github_user": "org",
    "github_repo": "package",
    "github_version": "main",
    "doc_path": "docs",
}
```

> For complete Sphinx extension configuration including advanced options, autodoc directives, and troubleshooting, see `references/sphinx-extensions.md`. For a complete ready-to-use `conf.py`, see `assets/sphinx-conf-scientific.py`.

## NumPy-style Docstrings

NumPy-style docstrings are the standard for scientific Python projects. Key sections include:

- **Short summary** - One-line description
- **Extended summary** - More detailed explanation
- **Parameters** - Input arguments with types and descriptions
- **Returns** - Return values with types
- **Raises** - Exceptions that may be raised
- **See Also** - Related functions/classes
- **Notes** - Additional information, math formulas
- **Examples** - Runnable code examples

### Concise Function Example

```python
def compute_statistic(data, method="mean", axis=0, weights=None):
    """
    Compute a statistical measure along the specified axis.

    Parameters
    ----------
    data : array_like
        Input data array. Can be any shape.
    method : {'mean', 'median', 'std'}, optional
        Statistical method to compute. Default is 'mean'.
    axis : int or None, optional
        Axis along which to compute. Default is 0.
    weights : array_like, optional
        Weights for each value. If None, equal weights.

    Returns
    -------
    ndarray
        Computed statistic.

    Raises
    ------
    ValueError
        If `method` is not supported.

    Examples
    --------
    >>> import numpy as np
    >>> data = np.array([1, 2, 3, 4, 5])
    >>> compute_statistic(data, method='mean')
    3.0
    """
    pass
```

> For comprehensive NumPy-style docstring examples including classes, modules, generators, and all supported sections, see `references/docstring-examples.md`.

## Read the Docs Integration

### Configuration File: `.readthedocs.yaml`

Place in repository root:

```yaml
version: 2

build:
  os: ubuntu-24.04
  tools:
    python: "3.12"
  jobs:
    post_install:
      # Install package with docs dependencies
      - pip install .[docs]

sphinx:
  configuration: docs/conf.py
  fail_on_warning: true

formats:
  - pdf
  - epub
```

### Dependencies

In `pyproject.toml`, add docs extra:

```toml
[project.optional-dependencies]
docs = [
    "sphinx>=7.0",
    "pydata-sphinx-theme>=0.15",
    "sphinx-autodoc-typehints>=2.0",
    "numpydoc>=1.6",
    "myst-parser>=2.0",
    "nbsphinx>=0.9",
]
```

Or in `setup.py`:

```python
extras_require = {
    "docs": [
        "sphinx>=7.0",
        "pydata-sphinx-theme>=0.15",
        "sphinx-autodoc-typehints>=2.0",
        "numpydoc>=1.6",
        "myst-parser>=2.0",
        "nbsphinx>=0.9",
    ],
}
```

### Setting up RTD

1. Go to https://readthedocs.org/
2. Import your repository
3. Trigger a build
4. Configure advanced settings:
   - Default version
   - Privacy level
   - Build notifications

## MkDocs Configuration for Scientific Python

### `mkdocs.yml`

```yaml
site_name: My Scientific Package
site_description: Description of your scientific package
site_url: https://org.github.io/package
repo_url: https://github.com/org/package
repo_name: org/package

theme:
  name: material
  features:
    - navigation.tabs
    - navigation.sections
    - navigation.expand
    - navigation.top
    - search.suggest
    - search.highlight
    - content.code.copy
    - content.tabs.link
  palette:
    - scheme: default
      primary: indigo
      accent: indigo
      toggle:
        icon: material/brightness-7
        name: Switch to dark mode
    - scheme: slate
      primary: indigo
      accent: indigo
      toggle:
        icon: material/brightness-4
        name: Switch to light mode

plugins:
  - search
  - mkdocstrings:
      handlers:
        python:
          options:
            docstring_style: numpy
            show_source: true
            show_root_heading: true
            separate_signature: true
            merge_init_into_class: true
  - mkdocs-jupyter:
      include_source: true
      execute: false

markdown_extensions:
  - pymdownx.arithmatex:
      generic: true
  - pymdownx.highlight:
      anchor_linenums: true
      line_spans: __span
      pygments_lang_class: true
  - pymdownx.inlinehilite
  - pymdownx.snippets
  - pymdownx.superfences
  - pymdownx.tabbed:
      alternate_style: true
  - admonition
  - pymdownx.details
  - tables
  - attr_list
  - md_in_html
  - def_list

extra_javascript:
  - javascripts/mathjax.js
  - https://cdn.jsdelivr.net/npm/mathjax@3/es5/tex-mml-chtml.js

nav:
  - Home: index.md
  - Getting Started:
      - Installation: getting-started/installation.md
      - Quick Start: getting-started/quickstart.md
  - Tutorials:
      - tutorials/index.md
      - First Analysis: tutorials/first-analysis.md
  - How-to Guides:
      - guides/index.md
      - Handle Large Data: guides/large-data.md
  - Reference:
      - API: reference/api.md
      - Configuration: reference/configuration.md
  - Explanation:
      - explanation/index.md
      - Architecture: explanation/architecture.md
```

### MathJax Configuration

Create `docs/javascripts/mathjax.js`:

```javascript
window.MathJax = {
  tex: {
    inlineMath: [["\\(", "\\)"]],
    displayMath: [["\\[", "\\]"]],
    processEscapes: true,
    processEnvironments: true
  },
  options: {
    ignoreHtmlClass: ".*|",
    processHtmlClass: "arithmatex"
  }
};

document$.subscribe(() => {
  MathJax.typesetPromise()
})
```

## Notebook Integration

### With Sphinx (nbsphinx)

**Configuration in `conf.py`:**
```python
extensions = [
    # ... other extensions
    "nbsphinx",
]

# nbsphinx configuration
nbsphinx_execute = "auto"  # or "always", "never"
nbsphinx_allow_errors = False
nbsphinx_timeout = 60  # seconds

# Exclude patterns
exclude_patterns = [
    "_build",
    "**.ipynb_checkpoints",
]
```

**Include notebooks in documentation:**
```rst
.. toctree::
   :maxdepth: 2

   notebooks/tutorial
   notebooks/advanced_usage
```

### With MkDocs (mkdocs-jupyter)

**Configuration in `mkdocs.yml`:**
```yaml
plugins:
  - mkdocs-jupyter:
      include_source: true
      execute: false
      allow_errors: false
      kernel_name: python3
```

**Reference in navigation:**
```yaml
nav:
  - Tutorials:
      - Getting Started: notebooks/tutorial.ipynb
      - Advanced: notebooks/advanced.ipynb
```

> For advanced notebook integration including execution options, cell tags, and troubleshooting, see `references/notebook-integration.md`.

## Build Automation with Nox

### `noxfile.py`

```python
"""Nox sessions for documentation."""
import nox

@nox.session(python="3.12")
def docs(session):
    """Build the documentation."""
    session.install(".[docs]")
    session.run(
        "sphinx-build",
        "-W",  # Treat warnings as errors
        "-b", "html",
        "docs",
        "docs/_build/html",
    )

@nox.session(python="3.12")
def docs_live(session):
    """Build and serve documentation with live reload."""
    session.install(".[docs]", "sphinx-autobuild")
    session.run(
        "sphinx-autobuild",
        "-W",
        "--open-browser",
        "docs",
        "docs/_build/html",
    )

@nox.session(python="3.12")
def docs_linkcheck(session):
    """Check documentation links."""
    session.install(".[docs]")
    session.run(
        "sphinx-build",
        "-b", "linkcheck",
        "docs",
        "docs/_build/linkcheck",
    )
```

**Usage:**
```bash
nox -s docs        # Build docs
nox -s docs_live   # Live preview
nox -s docs_linkcheck  # Check links
```

> For a complete noxfile with additional sessions (spelling, coverage, doctest, PDF builds), see `assets/noxfile-docs.py`.

## Documentation Structure

### Recommended Directory Layout

```
docs/
├── conf.py                    # Sphinx configuration
├── index.md                   # Landing page
├── getting-started/
│   ├── installation.md
│   └── quickstart.md
├── tutorials/
│   ├── index.md
│   ├── first-analysis.md
│   └── advanced-usage.md
├── guides/
│   ├── index.md
│   ├── large-datasets.md
│   ├── performance.md
│   └── extending.md
├── reference/
│   ├── api.rst
│   ├── cli.md
│   └── configuration.md
├── explanation/
│   ├── index.md
│   ├── architecture.md
│   ├── algorithms.md
│   └── design-decisions.md
├── notebooks/
│   └── *.ipynb
└── _static/
    └── custom.css
```

## Best Practices

### Writing Tips

1. **Start with the user's goal** - What are they trying to accomplish?
2. **Use active voice** - "The function returns..." not "The return value is..."
3. **Include runnable examples** - Every example should work
4. **Cross-reference liberally** - Use `:func:`, `:class:`, `:ref:`
5. **Update docs with code** - Documentation is code

### Common Pitfalls

1. **Mixing documentation types** - Don't put tutorials in reference docs
2. **Outdated examples** - Test your examples in CI
3. **Missing dependencies** - Document all requirements clearly
4. **Broken links** - Use `linkcheck` builder
5. **No search keywords** - Use descriptive titles and headers

> For detailed troubleshooting of common documentation build issues, see `references/common-issues.md`.

## Accessibility Guidelines

Accessible documentation ensures all users can effectively use your documentation regardless of ability. Follow these core principles based on the [Scientific Python Accessible Documentation Guide](https://learn.scientific-python.org/documentation/accessible-documentation/).

### Core Principles

1. **Write it out** - Text is highly adaptable across screen readers, search engines, and different devices
2. **Provide multiple options** - Offer both text explanations and images for concepts
3. **Use semantic structures** - Use HTML/Markdown elements as intended (headings for hierarchy, lists for related items)

### Documentation Structure

- Use headings in hierarchical order without skipping levels (H1 to H2 to H3, not H1 to H3)
- Create descriptive page titles that match navigation labels
- Include table of contents with jump links for long pages
- Provide site search functionality

### Text Accessibility

- Write complete sentences with proper grammar and punctuation
- Use plain language; avoid unnecessary jargon
- Define field-specific terms and expand acronyms on first use
- Write descriptive link text (not "click here")

### Image Accessibility

**Design requirements:**
- Images must remain comprehensible in grayscale
- Avoid red-green color combinations that overlap
- Maintain minimum 4.5:1 color contrast for text
- Ensure annotations are readable (similar size to body text)
- Prevent animated flashing (no more than 3 times per second)

**Context requirements:**
- Provide alt text capturing the information conveyed
- Include detailed image descriptions for complex figures
- All text in images must appear as actual text elsewhere
- Link to source data or notebooks when applicable

### Video Accessibility

- Introduce videos contextually before they appear
- Require explicit play/pause controls (no autoplay)
- Include closed captions and provide linked transcripts
- Ensure video content is duplicated in text elsewhere

### Quick Accessibility Checklist

- [ ] Page titles match navigation labels
- [ ] Headings in proper order (no skipping levels)
- [ ] Color contrast at least 4.5:1
- [ ] Images work in grayscale
- [ ] All images have meaningful alt text
- [ ] Link text describes destination
- [ ] Videos have controls (no autoplay)
- [ ] Text alternatives for all non-text content

> For comprehensive accessibility guidelines including code examples, testing tools, and detailed checklists, see `references/accessible-documentation.md`.

### Version Documentation

For multiple versions:

```python
# conf.py
version = "0.1"  # Short version
release = "0.1.0"  # Full version

# Add version switcher (pydata theme)
html_theme_options = {
    "navbar_end": ["version-switcher", "navbar-icon-links"],
}
```

## Scientific-Specific Sections

### Citation Information

Create `CITATION.cff`:
```yaml
cff-version: 1.2.0
message: "If you use this software, please cite it as below."
authors:
  - family-names: "Doe"
    given-names: "Jane"
    orcid: "https://orcid.org/0000-0000-0000-0000"
title: "My Scientific Package"
version: 0.1.0
doi: 10.5281/zenodo.1234567
date-released: 2024-01-01
url: "https://github.com/org/package"
```

Document in `docs/citation.md`:
```markdown
# Citing This Software

If you use this software in your research, please cite:

## BibTeX
\`\`\`bibtex
@software{doe2024package,
  author = {Doe, Jane},
  title = {My Scientific Package},
  year = {2024},
  version = {0.1.0},
  doi = {10.5281/zenodo.1234567},
  url = {https://github.com/org/package}
}
\`\`\`

## APA
Doe, J. (2024). My Scientific Package (Version 0.1.0) [Computer software]. https://doi.org/10.5281/zenodo.1234567
```

### Reproducibility Guide

```markdown
# Reproducibility

## Environment Setup

Install exact versions:
\`\`\`bash
pip install package==0.1.0
\`\`\`

Or from `requirements.txt`:
\`\`\`bash
pip install -r requirements.txt
\`\`\`

## Random Seeds

For reproducible results:
\`\`\`python
import numpy as np
import random

random.seed(42)
np.random.seed(42)
\`\`\`

## Data Requirements

Sample data available at: [URL]

Expected data format:
- CSV with columns: [...]
- Missing values encoded as NaN
- Date format: ISO 8601

## System Requirements

- Python 3.9+
- 8GB RAM minimum
- 16GB RAM recommended for large datasets
```

## Resources

### Primary References
- [Scientific Python Documentation Guide](https://learn.scientific-python.org/development/guides/docs/)
- [Diátaxis Framework](https://diataxis.fr/)
- [NumPy Documentation Style Guide](https://numpydoc.readthedocs.io/en/latest/format.html)

### Tooling Documentation
- [Sphinx](https://www.sphinx-doc.org/)
- [MyST Parser](https://myst-parser.readthedocs.io/)
- [MkDocs Material](https://squidfunk.github.io/mkdocs-material/)
- [Read the Docs](https://docs.readthedocs.io/)

### Example Projects
- [NumPy](https://numpy.org/doc/stable/)
- [SciPy](https://docs.scipy.org/doc/scipy/)
- [Xarray](https://docs.xarray.dev/)
- [scikit-learn](https://scikit-learn.org/stable/)
