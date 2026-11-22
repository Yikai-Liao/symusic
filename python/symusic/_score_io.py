"""Connect Score's ABC methods to the bundled converters."""

from collections.abc import Callable
from pathlib import Path
from typing import TypeVar

from . import _abc

_Score = TypeVar("_Score")


def read_abc(
    text: str, tune: int | None, parse_midi: Callable[[bytes], _Score]
) -> _Score:
    return _abc.parse_abc(text.encode("utf-8"), tune, parse_midi)


def read_abc_file(
    path: Path, tune: int | None, parse_midi: Callable[[bytes], _Score]
) -> _Score:
    return _abc.parse_abc(path.read_bytes(), tune, parse_midi)


def write_abc(midi: bytes, warn: bool) -> str:
    return _abc.write_abc(midi, warn)


def write_abc_file(path: Path, midi: bytes, warn: bool) -> None:
    path.write_bytes(write_abc(midi, warn).encode("utf-8"))
