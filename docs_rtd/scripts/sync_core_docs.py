#!/usr/bin/env python3
"""Generate the low-level symusic.core API reference page from docstrings."""
from __future__ import annotations

import inspect
from pathlib import Path

import symusic

GROUPS: list[tuple[str, list[str]]] = [
    ("Time Units", ["Tick", "Quarter", "Second"]),
    (
        "Scores and Tracks",
        [
            "ScoreTick",
            "ScoreQuarter",
            "ScoreSecond",
            "TrackTick",
            "TrackQuarter",
            "TrackSecond",
        ],
    ),
    (
        "Event Primitives",
        [
            "NoteTick",
            "ControlChangeTick",
            "TempoTick",
            "PedalTick",
            "PitchBendTick",
            "KeySignatureTick",
            "TimeSignatureTick",
            "TextMetaTick",
        ],
    ),
    ("Utilities", ["Synthesizer"]),
]

OUTPUT_PATH = Path("docs_rtd/api/core/index.md")


def heading(level: int, text: str) -> str:
    prefix = "#" * level
    return f"{prefix} {text}\n\n"


def render_entry(name: str) -> str:
    obj = getattr(symusic.core, name)
    doc = inspect.getdoc(obj) or "Docstring missing on binding."
    return f"### symusic.core.{name}\n\n{doc}\n\n"


def main() -> None:
    lines: list[str] = []
    lines.append("(core-low-level-api)=\n")
    lines.append(heading(1, "Low-Level symusic.core API"))
    lines.append(
        "This page is generated directly from the docstrings that live next to the nanobind "
        "bindings under `py_src/bindings`. Run `python docs_rtd/scripts/sync_core_docs.py` "
        "after editing those docstrings to refresh the content.\n\n"
    )
    for title, names in GROUPS:
        lines.append(heading(2, title))
        for name in names:
            lines.append(render_entry(name))
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_PATH.write_text("".join(lines), encoding="utf-8")


if __name__ == "__main__":
    main()
