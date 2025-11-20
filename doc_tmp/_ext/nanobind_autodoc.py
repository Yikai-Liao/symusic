import sphinx.util.inspect as sphinx_inspect
from typing import Any


def isnanobind(obj) -> bool:
    """Check if the object is nanobind.nb_func or nb_method."""
    return (
        hasattr(type(obj), "__module__") 
        and type(obj).__module__ == "nanobind" 
        and type(obj).__name__ in ("nb_func", "nb_method")
    )


# Patch sphinx.util.inspect.isfunction to recognize nanobind objects
_orig_isfunction = sphinx_inspect.isfunction

def isfunction(obj: Any) -> bool:
    """Check if the object is a user-defined function.
    
    This patches the original Sphinx isfunction to also recognize nanobind.nb_func
    as a function.
    """
    return _orig_isfunction(obj) or isnanobind(obj)

sphinx_inspect.isfunction = isfunction


# Patch sphinx.util.inspect.ismethoddescriptor to recognize nanobind objects
_orig_ismethoddescriptor = getattr(sphinx_inspect, "ismethoddescriptor", None)

if _orig_ismethoddescriptor:
    def ismethoddescriptor(obj: Any) -> bool:
        """Check if the object is a method descriptor.
        
        This patches the original Sphinx ismethoddescriptor to also recognize 
        nanobind.nb_method as a method descriptor.
        """
        return _orig_ismethoddescriptor(obj) or isnanobind(obj)
    
    sphinx_inspect.ismethoddescriptor = ismethoddescriptor


def setup(app):
    return {"version": "0.1", "parallel_read_safe": True}