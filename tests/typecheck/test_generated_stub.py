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
        if name.endswith("List") or name in {"Score", "Track", "Note", "Tempo"}:
            assert [ast.unparse(base) for base in node.bases] == ["Generic[_Unit]"]
