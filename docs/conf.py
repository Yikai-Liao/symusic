from __future__ import annotations

import os
import sys
from datetime import datetime, timezone
from pathlib import Path

# Local extensions live in _ext.
sys.path.insert(0, str(Path(__file__).resolve().parent / "_ext"))

author = "symusic developers"
project = "symusic"
copyright = f"{datetime.now(timezone.utc):%Y}, {author}"

extensions = [
    "myst_parser",
    "sphinx_copybutton",
    "remote_images",
]

# GitHub-style heading anchors, so `page.md#section` links work on GitHub, in
# the agent skill and on the site.
myst_heading_anchors = 3

linkcheck_timeout = 15
linkcheck_retries = 2
linkcheck_rate_limit_timeout = 30
linkcheck_report_timeouts_as_broken = True

templates_path = ["_templates"]
exclude_patterns: list[str] = [
    "_build",
    "Thumbs.db",
    ".DS_Store",
    "README.md",
    "v07-proposal",
]

html_theme = "furo"
html_static_path = ["_static"]
html_title = "symusic"
html_baseurl = os.environ.get(
    "READTHEDOCS_CANONICAL_URL",
    "https://symusic.readthedocs.io/en/stable/",
)

# Custom search page using Pagefind
html_additional_pages = {
    "search": "search.html",  # relative to docs/_templates directory
}
html_js_files = [
    "sidebar-scroll.js",
]

# Copybutton: strip prompts like >>> and ... when copying
copybutton_prompt_text = ">>> |\\.\\.\\. "
copybutton_prompt_isregexp = True
copybutton_line_continuation = "\\"
copybutton_copy_empty_lines = False
