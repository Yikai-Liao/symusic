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
    """Check if the object is nanobind.nb_func or nb_method."""
    return (
        hasattr(type(obj), "__module__") 
        and type(obj).__module__ == "nanobind" 
        and type(obj).__name__ in ("nb_func", "nb_method")
    )


# Patch sphinx.util.inspect.isfunction to recognize nanobind objects
# Keep Sphinx's discovery logic intact while teaching it about nanobind callables.
# Sphinx relies on ``inspect.isfunction`` and ``inspect.ismethoddescriptor`` to decide
# whether something should be handled by FunctionDocumenter or MethodDocumenter.
# nanobind's ``nb_func``/``nb_method`` fail those checks because their types come
# from the C++ binding runtime and expose no Python signature metadata. By short-
# circuiting those helpers we make nanobind functions eligible for signature
# generation in exactly the same code paths as pure Python objects.
_orig_isfunction = sphinx_inspect.isfunction


def isfunction(obj: Any) -> bool:
    """Return True for standard functions and nanobind callables."""

    return _orig_isfunction(obj) or isnanobind(obj)


sphinx_inspect.isfunction = isfunction


# Patch isroutine so nanobind callables attached to classes are treated as methods.
_orig_isroutine = sphinx_inspect.isroutine


def isroutine(obj: Any) -> bool:
    """Return True for Python routines and nanobind callables."""

    return _orig_isroutine(obj) or isnanobind(obj)


sphinx_inspect.isroutine = isroutine


# Patch sphinx.util.inspect.ismethoddescriptor to recognize nanobind objects
_orig_ismethoddescriptor = getattr(sphinx_inspect, "ismethoddescriptor", None)

if _orig_ismethoddescriptor:
    def ismethoddescriptor(obj: Any) -> bool:
        """Return True for Python and nanobind method descriptors."""

        return _orig_ismethoddescriptor(obj) or isnanobind(obj)

    sphinx_inspect.ismethoddescriptor = ismethoddescriptor


def _nanobind_property_return_from_doc(func: Any) -> Optional[str]:
    """Infer the return annotation for a nanobind property getter from its docstring."""

    if not isnanobind(func):
        return None

    doc = (getattr(func, "__doc__", "") or "").strip()
    if not doc:
        return None

    first_line = doc.splitlines()[0]
    if "->" not in first_line:
        return None

    # nanobind docstrings keep ``value(self, /) -> int``.
    # Everything to the right still contains the fully qualified type name, which is
    # sufficient for Sphinx' ``:type:`` option.
    return first_line.split("->", 1)[1].strip()


def _patch_property_documenter() -> None:
    """Augment PropertyDocumenter with the nanobind type fallback."""

    orig_add_directive_header = PropertyDocumenter.add_directive_header

    def add_directive_header(self, sig: str) -> None:  # type: ignore[override]
        orig_add_directive_header(self, sig)

        if self.config.autodoc_typehints == "none":
            return

        func = self._get_property_getter()
        fallback_type = _nanobind_property_return_from_doc(func) if func else None
        if fallback_type:
            # Native Python properties already emit ``:type:`` through Sphinx's default
            # logic because ``inspect.signature`` can inspect their annotations. For
            # nanobind properties the docstring is the only source of truth, so append
            # the parsed type information after Sphinx has written the rest of the
            # directive header.
            self.add_line('   :type: ' + fallback_type, self.get_sourcename())

    PropertyDocumenter.add_directive_header = add_directive_header


_patch_property_documenter()


def _patch_method_documenter() -> None:
    """Teach MethodDocumenter to mark nanobind static factories properly."""

    orig_import_object = MethodDocumenter.import_object
    orig_add_directive_header = MethodDocumenter.add_directive_header

    def import_object(self, raiseerror: bool = False) -> bool:  # type: ignore[override]
        ret = orig_import_object(self, raiseerror)
        self._is_nanobind_static = False  # type: ignore[attr-defined]
        if not ret:
            return ret

        obj = self.parent.__dict__.get(self.object_name)
        # ``nb.def_static`` stores a plain ``nanobind.nb_func`` inside ``__dict__`` while
        # bound instance methods are ``nanobind.nb_method`` descriptors. Checking for
        # ``nb_func`` ensures that only nanobind static factories get tagged as static
        # and regular methods keep their default treatment.
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


_patch_method_documenter()


def _resolve_class_member(documenter: MethodDocumenter) -> tuple[object, str, Any] | None:
    """Resolve the owning class and raw descriptor for a member name."""

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
    except Exception:  # pragma: no cover - defensive, module already imported by autodoc
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
    """Lower member_order for nanobind static factories before sorting."""

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


_patch_member_sorting()


def setup(app):
    return {"version": "0.1", "parallel_read_safe": True}
