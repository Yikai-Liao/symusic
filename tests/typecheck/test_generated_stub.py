import ast
import re
from pathlib import Path

import pytest
import symusic
from symusic import _core

EXPECTED_MEMBERS = {
    "Score": {"tracks", "tempos", "copy", "clip", "dump", "pianoroll"},
    "Track": {"notes", "controls", "copy", "clip", "trim"},
    "Note": {"time", "duration", "pitch", "copy", "shift_time"},
    "KeySignature": {"time", "key", "minor", "copy"},
    "TimeSignature": {"time", "numerator", "denominator", "copy"},
    "Tempo": {"time", "qpm", "microseconds_per_quarter", "copy"},
    "ControlChange": {"time", "number", "value", "copy"},
    "Pedal": {"time", "duration", "copy", "shift_time"},
    "PitchBend": {"time", "value", "copy", "shift_time"},
    "TextMeta": {"time", "text", "copy", "shift_time"},
}


EXPECTED_LIST_METHODS = {"append", "__getitem__", "filter", "sort", "adjust_time"}


def _member_names(class_node: ast.ClassDef) -> set[str]:
    methods = {
        node.name for node in class_node.body if isinstance(node, ast.FunctionDef)
    }
    attributes = {
        node.target.id
        for node in class_node.body
        if isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name)
    }
    return methods | attributes


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
        if name.endswith("List"):
            assert [ast.unparse(base) for base in node.bases] == [
                f"MutableSequence[{name.removesuffix('List')}[_Unit]]"
            ]
        elif name in {"Score", "Track", "Note", "Tempo"}:
            assert [ast.unparse(base) for base in node.bases] == ["Generic[_Unit]"]

    for name, members in EXPECTED_MEMBERS.items():
        assert members <= _member_names(classes[name]), name
    for name, node in classes.items():
        if name.endswith("List"):
            assert _member_names(node) >= EXPECTED_LIST_METHODS, name

    score_methods = [
        node for node in classes["Score"].body if isinstance(node, ast.FunctionDef)
    ]
    assert len([node for node in score_methods if node.name == "dump"]) == 1
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
        and any(
            isinstance(target, ast.Name) and target.id == "_TrimMode"
            for target in node.targets
        )
    )
    assert ast.unparse(trim_mode.value) in {"Literal", "typing.Literal"}
    assert ast.literal_eval(trim_mode.slice) == ("remove", "truncate", "retain")
    for name in ("Score", "Track"):
        trim_methods = [
            node
            for node in classes[name].body
            if isinstance(node, ast.FunctionDef) and node.name == "trim"
        ]
        assert len(trim_methods) == 3
        for method in trim_methods:
            annotations = {
                arg.arg: ast.unparse(arg.annotation)
                for arg in method.args.args
                if arg.annotation
            }
            assert annotations["start_mode"] == annotations["end_mode"] == "_TrimMode"


def test_stubgen_patterns_match_without_shadowing():
    pattern_file = (
        Path(__file__).resolve().parents[2] / "scripts" / "core_stubgen.pattern"
    )
    if not pattern_file.is_file():
        pytest.skip("requires source tree pattern file")

    runtime_queries = {_core.__name__}
    for name, obj in vars(_core).items():
        if name.startswith("__") and name.endswith("__"):
            continue
        runtime_queries.add(f"{_core.__name__}.{name}")
        if isinstance(obj, type):
            for attr in obj.__dict__:
                runtime_queries.add(f"{_core.__name__}.{name}.{attr}")

    claimed: set[str] = set()
    for lineno, raw in enumerate(pattern_file.read_text().splitlines(), 1):
        if not raw or raw[0].isspace() or raw.startswith("#"):
            continue
        assert raw.endswith(":"), f"line {lineno}: malformed pattern header {raw!r}"
        expr = raw[:-1]
        if expr in {r"^symusic\._core\.__prefix__$", r"^symusic\._core\.__postfix__$"}:
            continue
        regex = re.compile(expr)
        matched = {query for query in runtime_queries if regex.search(query)}
        assert matched, f"line {lineno}: pattern {expr!r} matches no runtime symbol"
        active = matched - claimed
        assert active, (
            f"line {lineno}: pattern {expr!r} is completely shadowed by an earlier rule"
        )
        claimed |= active
