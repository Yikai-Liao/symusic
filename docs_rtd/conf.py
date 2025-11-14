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

MOCK_SYMUSIC_CORE = False
try:
    import symusic.core  # noqa: F401  # type: ignore[attr-defined]
except Exception:
    MOCK_SYMUSIC_CORE = True

    class _SymusicCore(types.ModuleType):
        """Lightweight mock of the nanobind extension for documentation builds."""

        def __init__(self) -> None:
            super().__init__("symusic.core")
            self._members: dict[str, object] = {}

        def __getattr__(self, name: str) -> object:
            if name == "dump_wav":  # behaves like a function
                def _placeholder(*_args, **_kwargs):  # noqa: ANN001, ANN002 - placeholder
                    return None

                return _placeholder
            if name not in self._members:
                placeholder = type(name, (), {"__module__": "symusic.core"})
                self._members[name] = placeholder
            return self._members[name]

    sys.modules.setdefault("symusic.core", _SymusicCore())

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
autosummary_generate = True
autodoc_typehints = "description"
autodoc_mock_imports = ["symusic.core"] if MOCK_SYMUSIC_CORE else []
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
