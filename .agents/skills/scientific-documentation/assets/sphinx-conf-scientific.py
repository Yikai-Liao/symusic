# Configuration file for the Sphinx documentation builder.
# Scientific Python Documentation Template
#
# This file contains the configuration for building documentation using Sphinx
# with extensions optimized for scientific Python packages.

import sys
from pathlib import Path
from datetime import datetime

# -- Path setup --------------------------------------------------------------

# Add package to path for autodoc
# Adjust this path based on your project structure
sys.path.insert(0, str(Path("..").resolve() / "src"))

# -- Project information -----------------------------------------------------

project = "My Scientific Package"
copyright = f"{datetime.now().year}, Your Name"
author = "Your Name"

# The short X.Y version
version = "0.1"
# The full version, including alpha/beta/rc tags
release = "0.1.0"

# -- General configuration ---------------------------------------------------

# Add any Sphinx extension module names here, as strings
extensions = [
    # Core Sphinx extensions
    "sphinx.ext.autodoc",        # Auto-generate docs from docstrings
    "sphinx.ext.autosummary",    # Generate summary tables
    "sphinx.ext.viewcode",       # Add source code links
    "sphinx.ext.intersphinx",    # Link to other projects' documentation
    "sphinx.ext.todo",           # Support for todo items
    "sphinx.ext.coverage",       # Check documentation coverage
    "sphinx.ext.doctest",        # Test code examples

    # Scientific Python extensions
    "sphinx.ext.napoleon",       # NumPy/Google docstrings support
    "sphinx.ext.mathjax",        # Math rendering
    "numpydoc",                  # NumPy documentation style
    "sphinx_autodoc_typehints",  # Type hint integration

    # Markdown support
    "myst_parser",               # MyST Markdown parser

    # Notebook integration
    "nbsphinx",                  # Jupyter notebook rendering
]

# Add any paths that contain templates here, relative to this directory
templates_path = ["_templates"]

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files
exclude_patterns = [
    "_build",
    "Thumbs.db",
    ".DS_Store",
    "**.ipynb_checkpoints",
]

# The suffix(es) of source filenames
source_suffix = {
    ".rst": "restructuredtext",
    ".md": "markdown",
}

# The master toctree document
master_doc = "index"

# -- MyST Configuration ------------------------------------------------------

myst_enable_extensions = [
    "colon_fence",      # Support ::: fences
    "deflist",          # Definition lists
    "dollarmath",       # Support $...$ and $$...$$ for math
    "fieldlist",        # Field lists
    "substitution",     # Variable substitution
    "tasklist",         # Task lists with checkboxes
]

# Allow headings in Markdown to be used for cross-references
myst_heading_anchors = 3

# -- Autodoc Configuration ---------------------------------------------------

# Automatically generate stub pages for autosummary
autosummary_generate = True

# Include both class docstring and __init__ docstring
autoclass_content = "both"

# Document __init__, __repr__, and other special methods
autodoc_default_options = {
    "members": True,
    "member-order": "bysource",
    "special-members": "__init__",
    "undoc-members": True,
    "exclude-members": "__weakref__",
    "show-inheritance": True,
}

# -- Napoleon Configuration --------------------------------------------------

# Configure support for NumPy-style docstrings
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
napoleon_type_aliases = None
napoleon_attr_annotations = True

# -- Intersphinx Configuration -----------------------------------------------

# Mapping to external documentation
intersphinx_mapping = {
    "python": ("https://docs.python.org/3", None),
    "numpy": ("https://numpy.org/doc/stable/", None),
    "scipy": ("https://docs.scipy.org/doc/scipy/", None),
    "pandas": ("https://pandas.pydata.org/docs/", None),
    "matplotlib": ("https://matplotlib.org/stable/", None),
    "xarray": ("https://docs.xarray.dev/en/stable/", None),
    "sklearn": ("https://scikit-learn.org/stable/", None),
    "dask": ("https://docs.dask.org/en/stable/", None),
    "numba": ("https://numba.readthedocs.io/en/stable/", None),
    "zarr": ("https://zarr.readthedocs.io/en/stable/", None),
}

# Timeout for retrieving intersphinx inventories (in seconds)
intersphinx_timeout = 30

# -- nbsphinx Configuration --------------------------------------------------

# Execute notebooks before converting them
nbsphinx_execute = "auto"  # Options: "auto", "always", "never"

# Allow errors in notebook execution
nbsphinx_allow_errors = False

# Timeout for notebook execution (in seconds)
nbsphinx_timeout = 60

# Custom formats for notebook input/output prompts
nbsphinx_prompt_width = "0"

# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages
html_theme = "pydata_sphinx_theme"

# Theme options are theme-specific and customize the look and feel of a theme
html_theme_options = {
    # External links
    "github_url": "https://github.com/org/package",
    "twitter_url": "https://twitter.com/org",

    # Navigation and sidebar
    "use_edit_page_button": True,
    "show_toc_level": 2,
    "navigation_with_keys": True,
    "navbar_align": "left",
    "show_nav_level": 2,

    # Icons and links
    "icon_links": [
        {
            "name": "PyPI",
            "url": "https://pypi.org/project/your-package",
            "icon": "fas fa-box",
        },
        {
            "name": "Conda",
            "url": "https://anaconda.org/conda-forge/your-package",
            "icon": "fas fa-archive",
        },
    ],

    # Footer
    "footer_items": ["copyright", "sphinx-version"],
}

# Context for template variables
html_context = {
    "github_user": "org",
    "github_repo": "package",
    "github_version": "main",
    "doc_path": "docs",
}

# Add any paths that contain custom static files (such as style sheets)
html_static_path = ["_static"]

# Custom sidebar templates
html_sidebars = {
    "**": ["search-field", "sidebar-nav-bs"],
}

# Logo and favicon
# html_logo = "_static/logo.png"
# html_favicon = "_static/favicon.ico"

# -- Options for LaTeX/PDF output --------------------------------------------

latex_elements = {
    "papersize": "letterpaper",
    "pointsize": "10pt",
    "preamble": r"""
        \usepackage{amsmath}
        \usepackage{amssymb}
    """,
}

# Grouping the document tree into LaTeX files
latex_documents = [
    (master_doc, "package.tex", "My Scientific Package Documentation",
     "Your Name", "manual"),
]

# -- Options for manual page output ------------------------------------------

# One entry per manual page
man_pages = [
    (master_doc, "package", "My Scientific Package Documentation",
     [author], 1)
]

# -- Options for Texinfo output ----------------------------------------------

# Grouping the document tree into Texinfo files
texinfo_documents = [
    (master_doc, "package", "My Scientific Package Documentation",
     author, "package", "One line description of project.",
     "Miscellaneous"),
]

# -- Options for Epub output -------------------------------------------------

# Bibliographic Dublin Core info
epub_title = project
epub_author = author
epub_publisher = author
epub_copyright = copyright

# -- Extension configuration -------------------------------------------------

# sphinx.ext.todo
todo_include_todos = True

# sphinx.ext.coverage
coverage_write_headline = False
coverage_show_missing_items = True

# sphinx_autodoc_typehints
typehints_fully_qualified = False
always_document_param_types = True
typehints_document_rtype = True

# -- Custom Configuration ----------------------------------------------------

# If you need to add custom CSS
def setup(app):
    """Custom setup function."""
    # app.add_css_file("custom.css")
    pass
