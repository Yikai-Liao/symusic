"""Connect Score's ABC methods to the bundled converters."""

from collections.abc import Callable
import os
import stat
from typing import TypeVar

from . import _abc

_Score = TypeVar("_Score")


def read_abc(
    text: str, tune: int | None, parse_midi: Callable[[bytes], _Score]
) -> _Score:
    return _abc.parse_abc(text.encode("utf-8"), tune, parse_midi)


def read_abc_file(
    path: str, tune: int | None, parse_midi: Callable[[bytes], _Score]
) -> _Score:
    mode = os.stat(path).st_mode  # noqa: PTH116
    if not stat.S_ISREG(mode) and not stat.S_ISDIR(mode):
        msg = "Cannot seek to the end of the file"
        raise RuntimeError(msg)
    with open(path, "rb") as abc_file:  # noqa: PTH123
        data = abc_file.read()
    return _abc.parse_abc(data, tune, parse_midi)


def write_abc(midi: bytes, warn: bool) -> str:
    return _abc.write_abc(midi, warn)


def write_abc_file(path: str, midi: bytes, warn: bool) -> None:
    data = write_abc(midi, warn).encode("utf-8")
    # pathlib strips trailing separators before opening a file.
    with open(path, "wb") as file:  # noqa: PTH123
        file.write(data)
