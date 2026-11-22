"""Score file methods installed on the existing extension types."""

from __future__ import annotations

import pathlib
from typing import TYPE_CHECKING

from . import _abc, core

if TYPE_CHECKING:
    import typing_extensions

_MIDI_EXTENSIONS = {".mid", ".midi", ".MID", ".MIDI"}


class _FileMethods:
    @classmethod
    def from_file(
        cls,
        path: str | pathlib.Path,
        format: str | None = None,  # noqa: A002
        sanitize_data: bool = False,
        *,
        tune: int | None = None,
    ) -> typing_extensions.Self:
        """Read MIDI or ABC, selecting the format by extension or explicit name."""
        path = pathlib.Path(path)
        if format is not None and not isinstance(format, str):
            msg = "format must be a string or None"
            raise TypeError(msg)
        if format:
            file_format = format.lower()
        elif path.suffix in _MIDI_EXTENSIONS:
            file_format = "midi"
        else:
            file_format = "abc" if path.suffix == ".abc" else ""

        if file_format in {"midi", "mid"}:
            if tune is not None:
                msg = "tune is only supported for ABC input"
                raise ValueError(msg)
            return cls.from_midi(path.read_bytes(), sanitize_data)
        if file_format == "abc":
            if sanitize_data:
                msg = "sanitize_data is only supported for MIDI input"
                raise ValueError(msg)
            return cls.from_abc_file(path, tune=tune)
        msg = "Unknown file format"
        raise ValueError(msg)

    @classmethod
    def from_abc(cls, abc: str, *, tune: int | None = None) -> typing_extensions.Self:
        """Read the given ABC tune by its ``X:`` number. Default to the first tune."""
        if not isinstance(abc, str):
            msg = "abc must be a string"
            raise TypeError(msg)
        return _abc.parse_abc(abc.encode("utf-8"), tune, cls.from_midi)

    @classmethod
    def from_abc_file(
        cls,
        path: str | pathlib.Path,
        *,
        tune: int | None = None,
    ) -> typing_extensions.Self:
        """Read an ABC file, selecting an ``X:`` number or defaulting to the first tune."""
        return _abc.parse_abc(pathlib.Path(path).read_bytes(), tune, cls.from_midi)

    def dump_abc(self, path: str | pathlib.Path, warn: bool = False) -> None:
        """Write ABC to a file. With ``warn=True``, report converter diagnostics."""
        pathlib.Path(path).write_bytes(self.dumps_abc(warn).encode("utf-8"))

    def dumps_abc(self, warn: bool = False) -> str:
        """Return ABC text. With ``warn=True``, report converter diagnostics."""
        return _abc.write_abc(self.dumps_midi(), warn)


def install_score_file_methods() -> None:
    """Keep the same score types for construction, copying, conversion and pickle."""
    for name, method in vars(_FileMethods).items():
        if name.startswith("_"):
            continue
        for score_type in (core.ScoreTick, core.ScoreQuarter, core.ScoreSecond):
            setattr(score_type, name, method)
