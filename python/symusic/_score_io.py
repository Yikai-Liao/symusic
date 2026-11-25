"""Connect Score's ABC methods to the bundled converters."""

from collections.abc import Callable
from typing import TypeVar

from . import _abc

_Score = TypeVar("_Score")


def read_abc(
    data: bytes, tune: int | None, parse_midi: Callable[[bytes], _Score]
) -> _Score:
    return _abc.parse_abc(data, tune, parse_midi)


def write_abc(midi: bytes, warn: bool) -> str:
    return _abc.write_abc(midi, warn)


def write_abc_file(path: str, midi: bytes, warn: bool) -> None:
    data = write_abc(midi, warn).encode("utf-8")
    # pathlib strips trailing separators before opening a file.
    with open(path, "wb") as file:  # noqa: PTH123
        file.write(data)
