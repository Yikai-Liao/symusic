import ast
from pathlib import Path

import symusic
from symusic import _core


EXPECTED_METHODS = {
    "Score": {"tracks", "tempos", "copy", "clip", "dump_midi", "pianoroll"},
    "Track": {"notes", "controls", "copy", "clip", "trim"},
    "Note": {"time", "duration", "pitch", "copy", "shift_time"},
    "KeySignature": {"time", "key", "tonality", "copy"},
    "TimeSignature": {"time", "numerator", "denominator", "copy"},
    "Tempo": {"time", "qpm", "mspq", "copy"},
    "ControlChange": {"time", "number", "value", "copy"},
    "Pedal": {"time", "duration", "copy", "shift_time"},
    "PitchBend": {"time", "value", "copy", "shift_time"},
    "TextMeta": {"time", "text", "copy", "shift_time"},
    "PyVec": {"append", "__getitem__", "filter", "sort", "adjust_time"},
}


def _method_names(class_node: ast.ClassDef) -> set[str]:
    return {
        node.name
        for node in class_node.body
        if isinstance(node, ast.FunctionDef) and node.name != "__init__"
    }


def test_stub_exposes_runtime_classes():
    stub = Path(_core.__file__).with_name("_core.pyi")
    module = ast.parse(stub.read_text())
    classes = {
        node.name: node
        for node in module.body
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

    for name, methods in EXPECTED_METHODS.items():
        if name == "PyVec":
            for list_name, node in classes.items():
                if list_name.endswith("List"):
                    assert methods <= _method_names(node), list_name
        else:
            assert methods <= _method_names(classes[name]), name

    score_methods = [
        node for node in classes["Score"].body if isinstance(node, ast.FunctionDef)
    ]
    assert len([node for node in score_methods if node.name == "dump_midi"]) == 1
    assert len([node for node in score_methods if node.name == "__repr__"]) == 1

    pianoroll = [node for node in score_methods if node.name == "pianoroll"]
    assert len(pianoroll) == 1
    assert ast.unparse(pianoroll[0].args.args[0].annotation) == "Score[Tick]"

    clips = [node for node in score_methods if node.name == "clip"]
    assert [ast.unparse(node.args.args[0].annotation) for node in clips] == [
        "Score[Tick]",
        "Score[Quarter]",
        "Score[Second]",
    ]
    assert [ast.unparse(node.args.args[1].annotation) for node in clips] == [
        "int",
        "float",
        "float",
    ]
    assert [ast.unparse(node.returns) for node in clips] == [
        "Score[Tick]",
        "Score[Quarter]",
        "Score[Second]",
    ]

    get_beats = [node for node in score_methods if node.name == "get_beats"]
    assert [ast.literal_eval(node.args.defaults[0]) for node in get_beats] == [
        0,
        0.0,
        0.0,
    ]


    trim_mode = next(
        node.value
        for node in module.body
        if isinstance(node, ast.Assign)
        and any(isinstance(target, ast.Name) and target.id == "_TrimMode" for target in node.targets)
    )
    assert ast.unparse(trim_mode.value) in {"Literal", "typing.Literal"}
    assert ast.literal_eval(trim_mode.slice) == ("remove", "truncate", "retain")
    for name in ("Score", "Track"):
        trim_methods = [
            node for node in classes[name].body
            if isinstance(node, ast.FunctionDef) and node.name == "trim"
        ]
        assert len(trim_methods) == 3
        for method in trim_methods:
            annotations = {arg.arg: ast.unparse(arg.annotation) for arg in method.args.args if arg.annotation}
            assert annotations["start_mode"] == annotations["end_mode"] == "_TrimMode"
