# Sphinx Extensions for Scientific Python — Deep Reference

A comprehensive guide to Sphinx extensions commonly used in scientific Python documentation.

## Contents

| Section | Lines | Description |
|---------|-------|-------------|
| Core Sphinx Extensions | 19–274 | autodoc, autosummary, viewcode, intersphinx, napoleon, mathjax, todo, doctest |
| Scientific Python Extensions | 275–519 | numpydoc, sphinx_autodoc_typehints, myst_parser, nbsphinx, sphinx-gallery |
| Theme Extensions | 520–587 | pydata_sphinx_theme and furo configuration |
| Utility Extensions | 588–666 | sphinx-copybutton, sphinx-design, sphinxcontrib-bibtex |
| Debugging and Quality | 667–705 | coverage and linkcheck extensions |
| Extension Compatibility | 706–777 | Common combinations and troubleshooting conflicts |
| Resources | 778–783 | Links to Sphinx, PyData theme, MyST, and nbsphinx docs |

---

## Core Sphinx Extensions

### sphinx.ext.autodoc

**Purpose**: Automatically generate documentation from Python docstrings.

**Installation**: Built-in to Sphinx

**Configuration**:
```python
extensions = ["sphinx.ext.autodoc"]

autodoc_default_options = {
    "members": True,
    "member-order": "bysource",  # or "alphabetical", "groupwise"
    "special-members": "__init__",
    "undoc-members": True,
    "exclude-members": "__weakref__",
    "show-inheritance": True,
}

# Include both class docstring and __init__ docstring
autoclass_content = "both"  # or "class", "init"
```

**Usage**:
```rst
.. automodule:: mypackage.analysis
   :members:
   :undoc-members:
   :show-inheritance:

.. autoclass:: mypackage.DataAnalyzer
   :members:
   :special-members: __init__

.. autofunction:: mypackage.compute_statistic
```

**Tips**:
- Use `:private-members:` to document private members
- Use `:inherited-members:` to show inherited methods
- Use `:exclude-members:` to skip specific members

### sphinx.ext.autosummary

**Purpose**: Generate summary tables and stub pages for modules.

**Installation**: Built-in to Sphinx

**Configuration**:
```python
extensions = ["sphinx.ext.autosummary"]

autosummary_generate = True  # Generate stub pages
autosummary_imported_members = False  # Exclude imported members
```

**Usage**:
```rst
.. autosummary::
   :toctree: generated/
   :recursive:

   mypackage.analysis
   mypackage.visualization
   mypackage.io
```

**Generated Structure**:
```
docs/
└── generated/
    ├── mypackage.analysis.rst
    ├── mypackage.visualization.rst
    └── mypackage.io.rst
```

### sphinx.ext.viewcode

**Purpose**: Add links to highlighted source code.

**Installation**: Built-in to Sphinx

**Configuration**:
```python
extensions = ["sphinx.ext.viewcode"]

# No additional configuration needed
```

**Result**: Adds "[source]" links to documentation.

### sphinx.ext.intersphinx

**Purpose**: Link to other projects' documentation.

**Installation**: Built-in to Sphinx

**Configuration**:
```python
extensions = ["sphinx.ext.intersphinx"]

intersphinx_mapping = {
    "python": ("https://docs.python.org/3", None),
    "numpy": ("https://numpy.org/doc/stable/", None),
    "scipy": ("https://docs.scipy.org/doc/scipy/", None),
    "pandas": ("https://pandas.pydata.org/docs/", None),
    "matplotlib": ("https://matplotlib.org/stable/", None),
    "xarray": ("https://docs.xarray.dev/en/stable/", None),
    "sklearn": ("https://scikit-learn.org/stable/", None),
    "dask": ("https://docs.dask.org/en/stable/", None),
}

intersphinx_timeout = 30  # seconds
```

**Usage**:
```rst
:func:`numpy.mean`
:class:`pandas.DataFrame`
:mod:`scipy.stats`
```

**Tips**:
- Find inventory file: `python -m sphinx.ext.intersphinx https://numpy.org/doc/stable/objects.inv`
- Use `:external:` prefix for explicit external links

### sphinx.ext.napoleon

**Purpose**: Support NumPy and Google style docstrings.

**Installation**: Built-in to Sphinx

**Configuration**:
```python
extensions = ["sphinx.ext.napoleon"]

# NumPy style configuration
napoleon_google_docstring = False
napoleon_numpy_docstring = True
napoleon_include_init_with_doc = True
napoleon_include_private_with_doc = False
napoleon_include_special_with_doc = True
napoleon_use_admonition_for_examples = False
napoleon_use_admonition_for_notes = False
napoleon_use_admonition_for_references = False
napoleon_use_ivar = False
napoleon_use_param = True
napoleon_use_rtype = True
napoleon_use_keyword = True
napoleon_attr_annotations = True
```

**Supported Sections**:
- Parameters
- Returns
- Raises
- Yields
- Receives
- See Also
- Notes
- References
- Examples
- Attributes

### sphinx.ext.mathjax

**Purpose**: Render mathematical formulas using MathJax.

**Installation**: Built-in to Sphinx

**Configuration**:
```python
extensions = ["sphinx.ext.mathjax"]

mathjax_path = "https://cdn.jsdelivr.net/npm/mathjax@3/es5/tex-mml-chtml.js"

mathjax3_config = {
    "tex": {
        "inlineMath": [["$", "$"], ["\\(", "\\)"]],
        "displayMath": [["$$", "$$"], ["\\[", "\\]"]],
    }
}
```

**Usage**:
```rst
Inline math: :math:`E = mc^2`

Display math:

.. math::
   \frac{\partial f}{\partial x} = \lim_{h \to 0} \frac{f(x+h) - f(x)}{h}
```

**In Docstrings**:
```python
"""
The Gaussian function:

.. math::
    f(x) = \frac{1}{\sigma\sqrt{2\pi}} e^{-\frac{1}{2}\left(\frac{x-\mu}{\sigma}\right)^2}
"""
```

### sphinx.ext.todo

**Purpose**: Track TODO items in documentation.

**Installation**: Built-in to Sphinx

**Configuration**:
```python
extensions = ["sphinx.ext.todo"]

todo_include_todos = True  # Show TODOs in output
todo_emit_warnings = True  # Warn about TODOs
```

**Usage**:
```rst
.. todo::
   Add example for edge case
```

### sphinx.ext.doctest

**Purpose**: Test code examples in documentation.

**Installation**: Built-in to Sphinx

**Configuration**:
```python
extensions = ["sphinx.ext.doctest"]

doctest_global_setup = """
import numpy as np
import pandas as pd
"""
```

**Usage**:
```rst
.. doctest::

   >>> import numpy as np
   >>> np.array([1, 2, 3]).mean()
   2.0
```

**Run tests**:
```bash
sphinx-build -b doctest docs docs/_build/doctest
```

## Scientific Python Extensions

### numpydoc

**Purpose**: Enhanced NumPy-style docstring processing.

**Installation**:
```bash
pip install numpydoc
```

**Configuration**:
```python
extensions = ["numpydoc"]

numpydoc_show_class_members = True
numpydoc_class_members_toctree = True
numpydoc_use_plots = True  # Render plots in examples
numpydoc_xref_param_type = True  # Cross-reference parameter types
numpydoc_xref_aliases = {
    "DataFrame": "pandas.DataFrame",
    "Series": "pandas.Series",
    "ndarray": "numpy.ndarray",
}
numpydoc_xref_ignore = {"optional", "default", "of", "or"}
```

**Features**:
- Better NumPy-style docstring rendering
- Automatic cross-referencing of types
- Plot rendering in examples
- Validation of docstring formatting

### sphinx_autodoc_typehints

**Purpose**: Integrate type hints into documentation.

**Installation**:
```bash
pip install sphinx-autodoc-typehints
```

**Configuration**:
```python
extensions = ["sphinx_autodoc_typehints"]

typehints_fully_qualified = False
always_document_param_types = True
typehints_document_rtype = True
typehints_use_signature = False  # Show in description instead
typehints_use_signature_return = False
```

**Example**:
```python
def process(data: np.ndarray, method: str = "mean") -> float:
    """Process data with specified method.

    Parameters
    ----------
    data
        Input data array
    method
        Processing method

    Returns
    -------
    result
        Processed value
    """
```

Renders with automatic type information from annotations.

### myst_parser

**Purpose**: Use Markdown in Sphinx documentation.

**Installation**:
```bash
pip install myst-parser
```

**Configuration**:
```python
extensions = ["myst_parser"]

source_suffix = {
    ".rst": "restructuredtext",
    ".md": "markdown",
}

myst_enable_extensions = [
    "colon_fence",      # ::: directive syntax
    "deflist",          # Definition lists
    "dollarmath",       # $...$ for math
    "fieldlist",        # Field lists
    "html_admonition",  # HTML-style admonitions
    "html_image",       # HTML image syntax
    "linkify",          # Auto-convert URLs to links
    "replacements",     # Text replacements
    "smartquotes",      # Smart quotes
    "strikethrough",    # ~~text~~
    "substitution",     # Variable substitution
    "tasklist",         # - [ ] task lists
]

myst_heading_anchors = 3  # Generate anchors for h1, h2, h3
myst_url_schemes = ["http", "https", "mailto", "ftp"]
```

**MyST Syntax Examples**:
```markdown
# Heading

## Directives

:::{note}
This is a note
:::

## Math

Inline: $E = mc^2$

Display:
$$
\int_{-\infty}^{\infty} e^{-x^2} dx = \sqrt{\pi}
$$

## Cross-references

See {ref}`my-label` or {doc}`other-page`

## Code

```python
import numpy as np
```

## Task List

- [x] Completed task
- [ ] Pending task
```

### nbsphinx

**Purpose**: Include Jupyter notebooks in documentation.

**Installation**:
```bash
pip install nbsphinx
```

**Configuration**:
```python
extensions = ["nbsphinx"]

nbsphinx_execute = "auto"  # "auto", "always", "never"
nbsphinx_allow_errors = False
nbsphinx_timeout = 60  # Execution timeout in seconds
nbsphinx_kernel_name = "python3"

# Custom CSS
nbsphinx_prolog = """
{% set docname = env.doc2path(env.docname, base=None) %}

.. note::
   This page was generated from `{{ docname }}`_
"""

exclude_patterns = ["_build", "**.ipynb_checkpoints"]
```

**Usage**:
```rst
.. toctree::

   notebooks/tutorial
   notebooks/advanced
```

**Tips**:
- Use `nbsphinx-toctree` directive for dynamic notebook lists
- Add `:nbsphinx-execute: never` to skip execution for specific notebooks
- Use raw cells for Sphinx directives within notebooks

### sphinx-gallery

**Purpose**: Build example galleries from Python scripts.

**Installation**:
```bash
pip install sphinx-gallery
```

**Configuration**:
```python
extensions = ["sphinx_gallery.gen_gallery"]

sphinx_gallery_conf = {
    "examples_dirs": "../examples",  # Path to example scripts
    "gallery_dirs": "auto_examples",  # Output path
    "filename_pattern": "/plot_",     # Pattern for scripts
    "ignore_pattern": r"__init__\.py",
    "plot_gallery": True,             # Execute and plot
    "download_all_examples": False,
    "remove_config_comments": True,
    "backreferences_dir": "gen_modules/backreferences",
    "doc_module": "mypackage",
    "reference_url": {
        "mypackage": None,
        "numpy": "https://numpy.org/doc/stable/",
    },
}
```

**Example Script Format**:
```python
"""
Plot Sine Wave
==============

This example plots a sine wave.
"""

import numpy as np
import matplotlib.pyplot as plt

# %%
# Generate data
x = np.linspace(0, 2*np.pi, 100)
y = np.sin(x)

# %%
# Create plot
plt.figure()
plt.plot(x, y)
plt.xlabel('x')
plt.ylabel('sin(x)')
plt.title('Sine Wave')
plt.show()
```

## Theme Extensions

### pydata_sphinx_theme

**Purpose**: Modern theme for scientific Python projects.

**Installation**:
```bash
pip install pydata-sphinx-theme
```

**Configuration**:
```python
html_theme = "pydata_sphinx_theme"

html_theme_options = {
    "github_url": "https://github.com/org/package",
    "twitter_url": "https://twitter.com/org",
    "use_edit_page_button": True,
    "show_toc_level": 2,
    "navigation_with_keys": True,
    "navbar_align": "left",
    "navbar_end": ["navbar-icon-links", "version-switcher"],
    "icon_links": [
        {
            "name": "PyPI",
            "url": "https://pypi.org/project/package",
            "icon": "fas fa-box",
        },
    ],
    "analytics": {
        "google_analytics_id": "UA-XXXXXXXXX-X",
    },
}

html_context = {
    "github_user": "org",
    "github_repo": "package",
    "github_version": "main",
    "doc_path": "docs",
}
```

### furo

**Purpose**: Clean, modern Sphinx theme.

**Installation**:
```bash
pip install furo
```

**Configuration**:
```python
html_theme = "furo"

html_theme_options = {
    "light_css_variables": {
        "color-brand-primary": "#0072B2",
        "color-brand-content": "#0072B2",
    },
    "dark_css_variables": {
        "color-brand-primary": "#56B4E9",
        "color-brand-content": "#56B4E9",
    },
}
```

## Utility Extensions

### sphinx-copybutton

**Purpose**: Add copy button to code blocks.

**Installation**:
```bash
pip install sphinx-copybutton
```

**Configuration**:
```python
extensions = ["sphinx_copybutton"]

copybutton_prompt_text = r">>> |\.\.\. |\$ "
copybutton_prompt_is_regexp = True
copybutton_remove_prompts = True
```

### sphinx-design

**Purpose**: Design elements (cards, grids, tabs).

**Installation**:
```bash
pip install sphinx-design
```

**Usage**:
```rst
.. card:: Card Title

   Card content

.. grid:: 2

   .. grid-item::
      Column 1

   .. grid-item::
      Column 2

.. tab-set::

   .. tab-item:: Tab 1

      Content 1

   .. tab-item:: Tab 2

      Content 2
```

### sphinxcontrib-bibtex

**Purpose**: Citations and bibliography.

**Installation**:
```bash
pip install sphinxcontrib-bibtex
```

**Configuration**:
```python
extensions = ["sphinxcontrib.bibtex"]

bibtex_bibfiles = ["refs.bib"]
bibtex_default_style = "plain"
bibtex_reference_style = "author_year"
```

**Usage**:
```rst
See :cite:`author2020paper`

.. bibliography::
```

## Debugging and Quality

### sphinx.ext.coverage

**Purpose**: Check documentation coverage.

**Configuration**:
```python
extensions = ["sphinx.ext.coverage"]

coverage_write_headline = False
coverage_show_missing_items = True
```

**Run**:
```bash
sphinx-build -b coverage docs docs/_build/coverage
```

### sphinx.ext.linkcheck

**Purpose**: Check external links.

**Configuration**:
```python
extensions = ["sphinx.ext.linkcheck"]

linkcheck_ignore = [
    r"http://localhost",
    r"https://doi.org/.*",  # DOIs often block bots
]
linkcheck_timeout = 30
```

**Run**:
```bash
sphinx-build -b linkcheck docs docs/_build/linkcheck
```

## Extension Compatibility

### Common Combinations

**Minimal Scientific Python**:
```python
extensions = [
    "sphinx.ext.autodoc",
    "sphinx.ext.napoleon",
    "sphinx.ext.mathjax",
    "sphinx.ext.viewcode",
    "sphinx.ext.intersphinx",
]
```

**Full-Featured Scientific Python**:
```python
extensions = [
    # Core
    "sphinx.ext.autodoc",
    "sphinx.ext.autosummary",
    "sphinx.ext.viewcode",
    "sphinx.ext.intersphinx",

    # Scientific
    "sphinx.ext.napoleon",
    "sphinx.ext.mathjax",
    "numpydoc",
    "sphinx_autodoc_typehints",

    # Markdown
    "myst_parser",

    # Notebooks
    "nbsphinx",

    # Utilities
    "sphinx_copybutton",
    "sphinx_design",
]
```

## Troubleshooting

### Extension Conflicts

**Napoleon + numpydoc**: Use one or the other, not both.
```python
# Choose one:
extensions = ["sphinx.ext.napoleon"]  # Lighter
# OR
extensions = ["numpydoc"]  # More features
```

**MathJax + KaTeX**: Don't use both simultaneously.

### Performance Issues

**Slow autosummary generation**:
```python
autosummary_generate = True
autodoc_mock_imports = ["expensive_dependency"]
```

**Slow notebook execution**:
```python
nbsphinx_execute = "never"  # Execute manually
nbsphinx_execute_arguments = [
    "--InlineBackend.figure_formats={'svg'}",
]
```

## Resources

- [Sphinx Extensions Documentation](https://www.sphinx-doc.org/en/master/usage/extensions/index.html)
- [PyData Sphinx Theme](https://pydata-sphinx-theme.readthedocs.io/)
- [MyST Parser Guide](https://myst-parser.readthedocs.io/)
- [nbsphinx Documentation](https://nbsphinx.readthedocs.io/)
