from __future__ import annotations

import os
import sys
import types
from datetime import datetime
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
EXTENSIONS_ROOT = Path(__file__).resolve().parent / "_ext"
sys.path.insert(0, str(EXTENSIONS_ROOT))

from sphinx.errors import PycodeError
from sphinx.pycode import ModuleAnalyzer

_original_get_module_source = ModuleAnalyzer.get_module_source


def _safe_get_module_source(modname: str):
    """Wrap ModuleAnalyzer lookups so binary modules don't trigger TypeErrors."""
    try:
        return _original_get_module_source(modname)
    except TypeError as exc:  # pragma: no cover - defensive guard for compiled modules
        raise PycodeError(f"error importing {modname!r}", exc) from exc


ModuleAnalyzer.get_module_source = staticmethod(_safe_get_module_source)


# Try to import installed package first; fallback to local build artifact
import importlib.util
import glob

import symusic.core as core_module  # type: ignore

# Also add to python path for other imports (lower precedence than site-packages)
sys.path.append(str(PROJECT_ROOT / "python"))


author = "symusic developers"
project = "symusic"
copyright = f"{datetime.utcnow():%Y}, {author}"

extensions = [
    "myst_nb",
    # Add sphinx-copybutton to provide a "copy" button on code blocks
    "sphinx_copybutton",
    "sphinx.ext.autosectionlabel",
    "sphinx.ext.todo",
    "sphinx.ext.autodoc",
    "sphinx.ext.autosummary",
    "sphinx.ext.napoleon",
    "sphinx.ext.intersphinx",
    "sphinx.ext.viewcode",
    "remote_images",
    "sphinx_design",
    "format_attributes",
    # "nb_doc",  # disabled to avoid interfering with autosummary stubs
]

myst_enable_extensions = [
    "colon_fence",
    "deflist",
    "attrs_block",
    "substitution",
]

nb_execution_mode = "off"
autosummary_generate = True
autodoc_typehints = "signature"
autodoc_typehints_format = "short"
autodoc_docstring_signature = True
autodoc_type_aliases = {
    "numpy.ndarray[dtype=uint8]": "numpy.ndarray",
    "ndarray": "numpy.ndarray",
}
autosectionlabel_prefix_document = True
todo_include_todos = True

# Enforce consistent and richer API pages
autodoc_member_order = "groupwise"
autodoc_default_options = {
    "show-inheritance": True,
    "members": True,
    "undoc-members": True,
}
# Don't add module names to member signatures
add_module_names = False
nitpicky = True

# Napoleon settings
napoleon_use_param = True
napoleon_use_rtype = True
napoleon_preprocess_types = True

intersphinx_mapping = {
    "python": ("https://docs.python.org/3", None),
    "numpy": ("https://numpy.org/doc/stable/", None),
}

templates_path = ["_templates"]
exclude_patterns: list[str] = [
    "_build",
    "Thumbs.db",
    ".DS_Store",
    "README.md",
]

html_theme = "furo"
html_static_path = ["_static"]
html_title = "Symusic Documentation"

# 使用 Pagefind 自定义搜索页面
html_additional_pages = {
    'search': 'search.html',  # 相对于 docs_rtd/_templates 目录
}

# Tell Sphinx where the package is located without requiring an installed wheel.
os.environ.setdefault("PYTHONPATH", str(PROJECT_ROOT / "python"))
html_css_files = [
    "api-tweaks.css",
]

# Copybutton: strip prompts like >>> and ... when copying
copybutton_prompt_text = ">>> |\\.\\.\\. "
copybutton_prompt_isregexp = True
copybutton_line_continuation = "\\"
copybutton_copy_empty_lines = False
