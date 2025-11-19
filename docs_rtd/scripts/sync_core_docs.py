#!/usr/bin/env python3
"""Generate the low-level symusic.core API reference page from docstrings."""
from __future__ import annotations

import inspect
import re
from dataclasses import dataclass
from pathlib import Path

import symusic


@dataclass(frozen=True)
class Entry:
    name: str
    also: tuple[str, ...] = ()
    variant_note: str | None = None


GROUPS: list[tuple[str, list[Entry]]] = [
    ("Time Units", [Entry("Tick"), Entry("Quarter"), Entry("Second")]),
    (
        "Scores and Tracks",
        [
            Entry(
                "ScoreTick",
                ("ScoreQuarter", "ScoreSecond"),
                variant_note=(
                    "{variants} mirror this API on quarter-note or second timelines. "
                    "Convert between them via :py:meth:`symusic.core.ScoreTick.to`."
                ),
            ),
            Entry(
                "TrackTick",
                ("TrackQuarter", "TrackSecond"),
                variant_note=(
                    "{variants} expose identical per-track operations on alternate time bases."
                ),
            ),
        ],
    ),
    (
        "Event Primitives",
        [
            Entry("NoteTick", ("NoteQuarter", "NoteSecond")),
            Entry("ControlChangeTick", ("ControlChangeQuarter", "ControlChangeSecond")),
            Entry("TempoTick", ("TempoQuarter", "TempoSecond")),
            Entry("PedalTick", ("PedalQuarter", "PedalSecond")),
            Entry("PitchBendTick", ("PitchBendQuarter", "PitchBendSecond")),
            Entry("KeySignatureTick", ("KeySignatureQuarter", "KeySignatureSecond")),
            Entry("TimeSignatureTick", ("TimeSignatureQuarter", "TimeSignatureSecond")),
            Entry("TextMetaTick", ("TextMetaQuarter", "TextMetaSecond")),
        ],
    ),
    ("Utilities", [Entry("Synthesizer")]),
]

OUTPUT_PATH = Path("docs_rtd/api/core/index.md")
SECTION_PATTERN = re.compile(r"\n([A-Za-z0-9 _`/]+)\n[-=]{2,}\n")


def heading(level: int, text: str) -> str:
    prefix = "#" * level
    return f"{prefix} {text}".rstrip() + "\n\n"


def split_signature_and_body(doc: str) -> tuple[str | None, str]:
    """Extract the signature-like first line and return the cleaned body."""
    lines = doc.strip().splitlines()
    if not lines:
        return None, ""

    signature: str | None = None
    start = 0
    first = lines[0].strip()
    if "(" in first and ")" in first:
        signature = first
        start = 1
        if start < len(lines) and set(lines[start].strip()) <= {"=", "-"} and lines[start].strip():
            start += 1
        while start < len(lines) and not lines[start].strip():
            start += 1

    body = "\n".join(lines[start:])
    body = SECTION_PATTERN.sub(lambda match: f"\n**{match.group(1).strip()}**\n", body)
    return signature, body.strip()


def render_entry(entry: Entry) -> str:
    obj = getattr(symusic.core, entry.name)
    doc = inspect.getdoc(obj) or "Docstring missing on binding."
    signature, body = split_signature_and_body(doc)

    parts: list[str] = [f"### symusic.core.{entry.name}\n\n"]
    if signature:
        parts.append(f"`{signature}`\n\n")
    if body:
        parts.append(f"{body}\n\n")

    if entry.also:
        variant_names = ", ".join(f"`symusic.core.{name}`" for name in entry.also)
        note = entry.variant_note.format(variants=variant_names) if entry.variant_note else (
            f"{variant_names} share the same API on alternate time units."
        )
        parts.append(f"**Other variants:** {note}\n\n")

    return "".join(parts)


def main() -> None:
    lines: list[str] = []
    lines.append("(core-low-level-api)=\n")
    lines.append(heading(1, "Low-Level symusic.core API"))
    lines.append(
        "Only the canonical tick-based bindings are expanded below so the page stays readable. "
        "Quarter- and second-based variants are called out in each entry when they differ only by "
        "their time axis. Run `python docs_rtd/scripts/sync_core_docs.py` after editing the "
        "docstrings to refresh this content.\n\n"
    )
    for title, entries in GROUPS:
        lines.append(heading(2, title))
        for entry in entries:
            lines.append(render_entry(entry))
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_PATH.write_text("".join(lines), encoding="utf-8")


if __name__ == "__main__":
    main()
