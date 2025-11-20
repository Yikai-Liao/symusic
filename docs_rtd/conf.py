from __future__ import annotations

import os
import sys
import types
from datetime import datetime
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "python"))
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


# Load the compiled extension module directly
import glob
import importlib.util

# Find the compiled extension
core_so_pattern = str(PROJECT_ROOT / "build" / "core.cpython-*.so")
core_so_files = glob.glob(core_so_pattern)

if not core_so_files:
    raise RuntimeError(
        f"Could not find compiled extension at {core_so_pattern}. "
        "Please build the extension first with: cmake --build build"
    )

# Load the extension module
core_so_path = core_so_files[0]
spec = importlib.util.spec_from_file_location("core", core_so_path)
if spec is None or spec.loader is None:
    raise RuntimeError(f"Failed to create module spec from {core_so_path}")

core_module = importlib.util.module_from_spec(spec)
sys.modules["symusic.core"] = core_module
spec.loader.exec_module(core_module)

# Also add to python path for other imports
sys.path.insert(0, str(PROJECT_ROOT / "python"))


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
]

myst_enable_extensions = [
    "colon_fence",
    "deflist",
    "attrs_block",
    "substitution",
]

nb_execution_mode = "off"
autosummary_generate = False
autodoc_typehints = "description"
autosectionlabel_prefix_document = True
todo_include_todos = True

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
