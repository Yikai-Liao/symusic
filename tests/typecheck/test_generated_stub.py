import ast
from pathlib import Path

import symusic
from symusic import _core


def test_stub_exposes_runtime_classes():
    stub = Path(_core.__file__).with_name("_core.pyi")
    classes = {
        node.name: node
        for node in ast.parse(stub.read_text()).body
        if isinstance(node, ast.ClassDef) and not node.name.startswith("_")
    }
    assert classes.keys() == {
        name
        for name in symusic.__all__
        if getattr(symusic, name).__module__ == _core.__name__
    }
    for name, node in classes.items():
        if name.endswith("List"):
            assert [ast.unparse(base) for base in node.bases] == [
                f"MutableSequence[{name.removesuffix('List')}[_Unit]]"
            ]
        elif name in {"Score", "Track", "Note", "Tempo"}:
            assert [ast.unparse(base) for base in node.bases] == ["Generic[_Unit]"]


def test_event_list_copy_has_no_options():
    stub = Path(_core.__file__).with_name("_core.pyi")
    for node in ast.parse(stub.read_text()).body:
        if isinstance(node, ast.ClassDef) and node.name.endswith("List"):
            copy_method = next(
                method
                for method in node.body
                if isinstance(method, ast.FunctionDef) and method.name == "__copy__"
            )
            assert ast.unparse(copy_method.args) in {"self", "self, /"}, node.name
