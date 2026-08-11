from __future__ import annotations

import ast
from pathlib import Path

import symusic.core as core

TIME_UNITS = ("Tick", "Quarter", "Second")
GENERIC_BASES = (
    "Score",
    "Track",
    "Note",
    "KeySignature",
    "TimeSignature",
    "Tempo",
    "ControlChange",
    "Pedal",
    "PitchBend",
    "TextMeta",
)
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


def test_generic_bases_contain_every_shared_specialization_member() -> None:
    stub_path = Path(core.__file__).with_name("core.pyi")
    raw_stub_path = stub_path.with_name("core.raw.pyi")
    assert stub_path.is_file()
    assert not raw_stub_path.exists()

    module = ast.parse(stub_path.read_text())
    classes = {
        node.name: node for node in module.body if isinstance(node, ast.ClassDef)
    }

    for base in GENERIC_BASES:
        shared = set.intersection(
            *(_method_names(classes[f"{base}{unit}"]) for unit in TIME_UNITS)
        )
        assert shared
        assert shared <= _method_names(classes[base]), base
        assert EXPECTED_METHODS[base] <= _method_names(classes[base]), base

    assert EXPECTED_METHODS["PyVec"] <= _method_names(classes["PyVec"])

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
