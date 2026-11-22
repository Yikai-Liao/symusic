"""Score file methods installed on the existing extension types."""

from __future__ import annotations

import pathlib
from typing import TYPE_CHECKING

from . import core

if TYPE_CHECKING:
    import typing_extensions


class _FileMethods:
    @classmethod
    def from_file(
        cls,
        path: str | pathlib.Path,
        format: str | None = None,  # noqa: A002
        sanitize_data: bool = False,
    ) -> typing_extensions.Self:
        """Read MIDI or ABC, selecting the format by extension or explicit name."""
        path = pathlib.Path(path)
        file_format = format.lower() if format else path.suffix
        if file_format in {"midi", "mid", ".mid", ".midi", ".MID", ".MIDI"}:
            return cls.from_midi(path.read_bytes(), sanitize_data)
        if file_format in {"abc", ".abc"}:
            if sanitize_data:
                msg = "sanitize_data is only supported for MIDI input"
                raise ValueError(msg)
            return cls._from_abc_file(path)
        msg = "Unknown file format"
        raise ValueError(msg)


def install_score_file_methods() -> None:
    """Keep the same score types for construction, copying, conversion and pickle."""
    for name, method in vars(_FileMethods).items():
        if name.startswith("_"):
            continue
        # Nanobind's stub generator emits signatures for methods from its own module.
        method.__module__ = core.__name__
        for score_type in (core.ScoreTick, core.ScoreQuarter, core.ScoreSecond):
            setattr(score_type, name, method)
