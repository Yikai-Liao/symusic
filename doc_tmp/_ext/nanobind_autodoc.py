from inspect import Parameter
from typing import Any, Optional

import sphinx.util.inspect as sphinx_inspect
from sphinx.ext.autodoc import PropertyDocumenter
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

    # nanobind docstrings keep ``value(self, /) -> symusic.core.Quarter``.
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


def setup(app):
    return {"version": "0.1", "parallel_read_safe": True}
