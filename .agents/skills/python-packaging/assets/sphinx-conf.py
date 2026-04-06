# docs/conf.py
import sys
from pathlib import Path

# Add package to path
sys.path.insert(0, str(Path("..").resolve() / "src"))

# Project information
project = "My Scientific Package"
copyright = "2025, Your Name"
author = "Your Name"

# Extensions
extensions = [
    "sphinx.ext.autodoc",
    "sphinx.ext.napoleon",  # NumPy/Google style docstrings
    "sphinx.ext.viewcode",
    "sphinx.ext.mathjax",   # Math rendering
    "sphinx.ext.intersphinx",
    "numpydoc",             # NumPy documentation style
]

# Intersphinx mapping
intersphinx_mapping = {
    "python": ("https://docs.python.org/3", None),
    "numpy": ("https://numpy.org/doc/stable/", None),
    "scipy": ("https://docs.scipy.org/doc/scipy/", None),
    "pandas": ("https://pandas.pydata.org/docs/", None),
}

# Theme
html_theme = "sphinx_rtd_theme"
