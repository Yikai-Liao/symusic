import os
import sys
from pathlib import Path

project = "symusic"
author = "symusic developers"

root = Path(__file__).resolve().parent.parent
sys.path.append(str(root / "python"))
# Ensure local Sphinx extensions are discoverable
ext_path = Path(__file__).resolve().parent / "_ext"
sys.path.insert(0, str(ext_path))

extensions = [
    "sphinx.ext.autodoc",
    "sphinx.ext.autosummary",
    "sphinx.ext.napoleon",
    "sphinx.ext.viewcode",
    # Treat nanobind callables (nb_method/nb_func) as methods to show signatures
    "nanobind_autodoc",
]

autosummary_generate = True
autodoc_member_order = "groupwise"
autodoc_typehints = "signature"
autodoc_typehints_format = "short"
autodoc_docstring_signature = True
autodoc_default_options = {
    "show-inheritance": True,
    "members": True,
    "undoc-members": True,
}
add_module_names = False

templates_path = ["_templates"]
exclude_patterns = ["_build"]
html_static_path = ["_static"]
html_theme = "furo"