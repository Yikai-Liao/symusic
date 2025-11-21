"""Sphinx extension for caching remote images as local static assets."""

from __future__ import annotations

import hashlib
import json
import mimetypes
from pathlib import Path
from typing import Any
from urllib.parse import urlparse

import requests
from docutils import nodes
from sphinx.application import Sphinx
from sphinx.util import logging

LOGGER = logging.getLogger(__name__)
_CACHE_NAME = "cache.json"
_REMOTE_ROOT = Path("_static") / "remote_images"
_DOWNLOAD_TIMEOUT = 30  # seconds


def _get_state(app: Sphinx) -> dict[str, Any]:
    state = getattr(app, "_remote_images_state", None)
    if state is not None:
        return state

    target_root = Path(app.outdir) / _REMOTE_ROOT
    target_root.mkdir(parents=True, exist_ok=True)
    cache_path = target_root / _CACHE_NAME
    cache: dict[str, str] = {}
    if cache_path.exists():
        try:
            cache = json.loads(cache_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            LOGGER.warning("[remote-images] cache %s is corrupted; rebuilding", cache_path)

    state = {
        "dir": target_root,
        "cache": cache,
        "cache_path": cache_path,
        "dirty": False,
    }
    app._remote_images_state = state
    return state


def _save_state(app: Sphinx) -> None:
    state = getattr(app, "_remote_images_state", None)
    if not state or not state.get("dirty"):
        return
    cache_path: Path = state["cache_path"]
    cache_data: dict[str, str] = state["cache"]
    cache_path.write_text(json.dumps(cache_data, indent=2, sort_keys=True), encoding="utf-8")
    state["dirty"] = False


def _is_remote(uri: str) -> bool:
    return uri.startswith("http://") or uri.startswith("https://")


def _resolve_extension(uri: str, response: requests.Response) -> str:
    parsed = urlparse(uri)
    suffix = Path(parsed.path).suffix
    if suffix:
        return suffix
    content_type = response.headers.get("Content-Type", "").split(";")[0].strip()
    if content_type:
        guessed = mimetypes.guess_extension(content_type)
        if guessed:
            return guessed
    return ".bin"


def _localize_image(app: Sphinx, uri: str) -> str:
    state = _get_state(app)
    cache: dict[str, str] = state["cache"]
    rel_path = cache.get(uri)
    if rel_path:
        local_path = Path(app.outdir) / rel_path
        if local_path.exists():
            return rel_path.replace("\\", "/")
        LOGGER.info("[remote-images] cache entry missing file for %s; re-downloading", uri)

    try:
        response = requests.get(uri, timeout=_DOWNLOAD_TIMEOUT)
        response.raise_for_status()
    except requests.RequestException as exc:
        LOGGER.warning("[remote-images] failed to fetch %s: %s", uri, exc)
        return uri

    digest = hashlib.sha1(uri.encode("utf-8")).hexdigest()  # nosec - cache key only
    extension = _resolve_extension(uri, response)
    filename = f"{digest}{extension}"
    target = state["dir"] / filename
    target.write_bytes(response.content)

    rel_path = str(_REMOTE_ROOT / filename).replace("\\", "/")
    cache[uri] = rel_path
    state["dirty"] = True
    LOGGER.info("[remote-images] cached %s -> %s", uri, rel_path)
    return rel_path


def _on_doctree_resolved(app: Sphinx, doctree: nodes.document, _docname: str) -> None:
    if app.builder.name != "html":
        return
    for node in doctree.findall(nodes.image):
        uri = node.get("uri")
        if not uri or not _is_remote(uri):
            continue
        node["uri"] = _localize_image(app, uri)


def _on_builder_inited(app: Sphinx) -> None:
    if app.builder.name == "html":
        _get_state(app)


def _on_build_finished(app: Sphinx, _exception: Exception | None) -> None:
    if app.builder.name == "html":
        _save_state(app)


def setup(app: Sphinx) -> dict[str, bool]:
    app.connect("builder-inited", _on_builder_inited)
    app.connect("doctree-resolved", _on_doctree_resolved)
    app.connect("build-finished", _on_build_finished)
    return {
        "parallel_read_safe": True,
        "parallel_write_safe": True,
    }
