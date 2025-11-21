# Nanobind autodoc patch cheat sheet

## Method detection

- `sphinx.util.inspect.isfunction`, `isroutine`, and `ismethoddescriptor` are monkey patched to return `True` for `nanobind.nb_func` / `nanobind.nb_method`.  
- Reason: nanobind callables do not inherit Python's pure function descriptors, so the default autodoc discovery treated them as attributes and skipped signature extraction.  
- The helper `isnanobind(obj)` (module check + type name check) centralizes the detection.

```python
def isfunction(obj: Any) -> bool:
    return _orig_isfunction(obj) or isnanobind(obj)

def isroutine(obj: Any) -> bool:
    return _orig_isroutine(obj) or isnanobind(obj)

def ismethoddescriptor(obj: Any) -> bool:
    return _orig_ismethoddescriptor(obj) or isnanobind(obj)
```

## Property type hints

- Nanobind property getters expose docstrings like `value(self, /) -> symusic.core.TrackTickList` but do not carry `__annotations__`.  
- `_nanobind_property_return_from_doc()` parses the first line of the docstring and extracts the post-`->` portion.  
- `_patch_property_documenter()` appends the parsed type as `:type:` in the directive header unless `autodoc_typehints == "none"`.

```python
def add_directive_header(self, sig: str) -> None:
    orig_add_directive_header(self, sig)
    if self.config.autodoc_typehints == "none":
        return

    func = self._get_property_getter()
    fallback = _nanobind_property_return_from_doc(func) if func else None
    if fallback:
        self.add_line('   :type: ' + fallback, self.get_sourcename())
```

## Static factories and groupwise ordering

- `_patch_method_documenter()` inspects `parent.__dict__[name]`; if the descriptor is a `nanobind.nb_func`, mark it as static and emit `:staticmethod:`.  
- The same hook lowers `member_order` by one so static factories lead their group.  
- `_patch_member_sorting()` hooks `Documenter.sort_members()` to ensure any nanobind static factory gets its `member_order` adjusted before Sphinx performs the groupwise sort. It relies on `_resolve_class_member()` to recover the raw descriptor via the fully qualified name (e.g., `symusic.core::ScoreTick.from_file`).

```python
def import_object(self, raiseerror: bool = False) -> bool:
    ret = orig_import_object(self, raiseerror)
    if not ret:
        return ret

    obj = self.parent.__dict__.get(self.object_name)
    if isinstance(obj, type(self.object)) and type(obj).__name__ == "nb_func" and isnanobind(obj):
        self._is_nanobind_static = True
        self.member_order -= 1
    return ret

def add_directive_header(self, sig: str) -> None:
    orig_add_directive_header(self, sig)
    if getattr(self, "_is_nanobind_static", False):
        self.add_line('   :staticmethod:', self.get_sourcename())
```

```python
def sort_members(self, documenters, order):
    if order == "groupwise" and isinstance(self, ClassDocumenter):
        for documenter, _ in documenters:
            if isinstance(documenter, MethodDocumenter):
                resolved = _resolve_class_member(documenter)
                if resolved:
                    owner, _, raw = resolved
                    if type(raw).__name__ == "nb_func" and isnanobind(raw):
                        documenter.member_order = MethodDocumenter.member_order - 1
    return orig_sort_members(self, documenters, order)
```

## Full patch reference

```python
from __future__ import annotations

import importlib
from inspect import Parameter
from typing import Any, Optional

import sphinx.util.inspect as sphinx_inspect
from sphinx.ext.autodoc import (
    ClassDocumenter,
    Documenter,
    MethodDocumenter,
    PropertyDocumenter,
)
from sphinx.locale import __
from sphinx.util import logging
from sphinx.util.typing import stringify_annotation

logger = logging.getLogger(__name__)


def isnanobind(obj) -> bool:
    return (
        hasattr(type(obj), "__module__")
        and type(obj).__module__ == "nanobind"
        and type(obj).__name__ in ("nb_func", "nb_method")
    )


_orig_isfunction = sphinx_inspect.isfunction
def isfunction(obj: Any) -> bool:
    return _orig_isfunction(obj) or isnanobind(obj)
sphinx_inspect.isfunction = isfunction

_orig_isroutine = sphinx_inspect.isroutine
def isroutine(obj: Any) -> bool:
    return _orig_isroutine(obj) or isnanobind(obj)
sphinx_inspect.isroutine = isroutine

_orig_ismethoddescriptor = getattr(sphinx_inspect, "ismethoddescriptor", None)
if _orig_ismethoddescriptor:
    def ismethoddescriptor(obj: Any) -> bool:
        return _orig_ismethoddescriptor(obj) or isnanobind(obj)
    sphinx_inspect.ismethoddescriptor = ismethoddescriptor


def _nanobind_property_return_from_doc(func: Any) -> Optional[str]:
    if not isnanobind(func):
        return None

    doc = (getattr(func, "__doc__", "") or "").strip()
    if not doc:
        return None

    first_line = doc.splitlines()[0]
    if "->" not in first_line:
        return None

    return first_line.split("->", 1)[1].strip()


def _patch_property_documenter() -> None:
    orig_add_directive_header = PropertyDocumenter.add_directive_header

    def add_directive_header(self, sig: str) -> None:  # type: ignore[override]
        orig_add_directive_header(self, sig)

        if self.config.autodoc_typehints == "none":
            return

        func = self._get_property_getter()
        fallback_type = _nanobind_property_return_from_doc(func) if func else None
        if fallback_type:
            self.add_line('   :type: ' + fallback_type, self.get_sourcename())

    PropertyDocumenter.add_directive_header = add_directive_header


def _patch_method_documenter() -> None:
    orig_import_object = MethodDocumenter.import_object
    orig_add_directive_header = MethodDocumenter.add_directive_header

    def import_object(self, raiseerror: bool = False) -> bool:  # type: ignore[override]
        ret = orig_import_object(self, raiseerror)
        self._is_nanobind_static = False  # type: ignore[attr-defined]
        if not ret:
            return ret

        obj = self.parent.__dict__.get(self.object_name)
        if (
            isinstance(obj, type(self.object))
            and type(obj).__name__ == "nb_func"
            and isnanobind(obj)
        ):
            self._is_nanobind_static = True  # type: ignore[attr-defined]
            self.member_order -= 1
        return ret

    def add_directive_header(self, sig: str) -> None:  # type: ignore[override]
        orig_add_directive_header(self, sig)

        if getattr(self, "_is_nanobind_static", False):  # type: ignore[attr-defined]
            self.add_line('   :staticmethod:', self.get_sourcename())

    MethodDocumenter.import_object = import_object
    MethodDocumenter.add_directive_header = add_directive_header


def _resolve_class_member(documenter: MethodDocumenter) -> tuple[object, str, Any] | None:
    if "::" not in documenter.name:
        return None

    modname, qualname = documenter.name.split("::", 1)
    if not qualname:
        return None

    chunks = qualname.split(".")
    if len(chunks) < 2:
        return None

    try:
        module = importlib.import_module(modname)
    except Exception:  # pragma: no cover
        return None

    owner: Any = module
    for chunk in chunks[:-1]:
        owner = getattr(owner, chunk, None)
        if owner is None:
            return None

    member_name = chunks[-1]
    owner_dict = getattr(owner, "__dict__", {})
    raw = owner_dict.get(member_name)
    if raw is None:
        raw = getattr(owner, member_name, None)

    if raw is None:
        return None

    return owner, member_name, raw


def _patch_member_sorting() -> None:
    orig_sort_members = Documenter.sort_members

    def sort_members(self, documenters, order):  # type: ignore[override]
        if order == "groupwise" and isinstance(self, ClassDocumenter):
            for documenter, _ in documenters:
                if isinstance(documenter, MethodDocumenter):
                    resolved = _resolve_class_member(documenter)
                    if not resolved:
                        continue

                    _, _, raw = resolved
                    if type(raw).__name__ == "nb_func" and isnanobind(raw):
                        documenter.member_order = MethodDocumenter.member_order - 1

        return orig_sort_members(self, documenters, order)

    Documenter.sort_members = sort_members


_patch_property_documenter()
_patch_method_documenter()
_patch_member_sorting()
```
