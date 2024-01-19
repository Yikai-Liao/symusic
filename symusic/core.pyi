import os
from typing import Callable, Iterable, Optional, overload

import numpy

import symusic.core

class ControlChangeQuarter:
    """
    None
    """

    def __init__(self, other: symusic.core.ControlChangeQuarter) -> None:
        """
        __init__(self, time: float, number: int, value: int) -> None

        Copy constructor
        """
        ...

    def copy(self) -> symusic.core.ControlChangeQuarter:
        """
        Deep copy
        """
        ...

    def from_numpy(
        self, arg0: numpy.typing.NDArray, arg1: numpy.typing.NDArray, /
    ) -> symusic.core.ControlChangeQuarterList: ...
    @property
    def number(self) -> int: ...
    @number.setter
    def number(self, arg: int, /) -> None: ...
    def shift_time(
        self, offset: float, inplace: bool = False
    ) -> symusic.core.ControlChangeQuarter:
        """
        Shift the event time by offset
        """
        ...

    @property
    def time(self) -> float: ...
    @time.setter
    def time(self, arg: float, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Quarter: ...
    @property
    def value(self) -> int: ...
    @value.setter
    def value(self, arg: int, /) -> None: ...

class ControlChangeQuarterList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.ControlChangeQuarter], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.ControlChangeQuarterList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.ControlChangeQuarter, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.ControlChangeQuarterList: ...
    def count(self, arg: symusic.core.ControlChangeQuarter, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.ControlChangeQuarterList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.ControlChangeQuarter, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def numpy(self) -> dict: ...
    def pop(self, index: int = -1) -> symusic.core.ControlChangeQuarter:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.ControlChangeQuarter, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Quarter: ...

class ControlChangeSecond:
    """
    None
    """

    def __init__(self, other: symusic.core.ControlChangeSecond) -> None:
        """
        __init__(self, time: float, number: int, value: int) -> None

        Copy constructor
        """
        ...

    def copy(self) -> symusic.core.ControlChangeSecond:
        """
        Deep copy
        """
        ...

    def from_numpy(
        self, arg0: numpy.typing.NDArray, arg1: numpy.typing.NDArray, /
    ) -> symusic.core.ControlChangeSecondList: ...
    @property
    def number(self) -> int: ...
    @number.setter
    def number(self, arg: int, /) -> None: ...
    def shift_time(
        self, offset: float, inplace: bool = False
    ) -> symusic.core.ControlChangeSecond:
        """
        Shift the event time by offset
        """
        ...

    @property
    def time(self) -> float: ...
    @time.setter
    def time(self, arg: float, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Second: ...
    @property
    def value(self) -> int: ...
    @value.setter
    def value(self, arg: int, /) -> None: ...

class ControlChangeSecondList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.ControlChangeSecond], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.ControlChangeSecondList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.ControlChangeSecond, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.ControlChangeSecondList: ...
    def count(self, arg: symusic.core.ControlChangeSecond, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.ControlChangeSecondList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.ControlChangeSecond, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def numpy(self) -> dict: ...
    def pop(self, index: int = -1) -> symusic.core.ControlChangeSecond:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.ControlChangeSecond, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Second: ...

class ControlChangeTick:
    """
    None
    """

    def __init__(self, other: symusic.core.ControlChangeTick) -> None:
        """
        __init__(self, time: int, number: int, value: int) -> None

        Copy constructor
        """
        ...

    def copy(self) -> symusic.core.ControlChangeTick:
        """
        Deep copy
        """
        ...

    def from_numpy(
        self, arg0: numpy.typing.NDArray, arg1: numpy.typing.NDArray, /
    ) -> symusic.core.ControlChangeTickList: ...
    @property
    def number(self) -> int: ...
    @number.setter
    def number(self, arg: int, /) -> None: ...
    def shift_time(
        self, offset: int, inplace: bool = False
    ) -> symusic.core.ControlChangeTick:
        """
        Shift the event time by offset
        """
        ...

    @property
    def time(self) -> int: ...
    @time.setter
    def time(self, arg: int, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Tick: ...
    @property
    def value(self) -> int: ...
    @value.setter
    def value(self, arg: int, /) -> None: ...

class ControlChangeTickList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.ControlChangeTick], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.ControlChangeTickList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.ControlChangeTick, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.ControlChangeTickList: ...
    def count(self, arg: symusic.core.ControlChangeTick, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.ControlChangeTickList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.ControlChangeTick, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def numpy(self) -> dict: ...
    def pop(self, index: int = -1) -> symusic.core.ControlChangeTick:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.ControlChangeTick, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Tick: ...

class KeySignatureQuarter:
    """
    None
    """

    def __init__(self, other: symusic.core.KeySignatureQuarter) -> None:
        """
        __init__(self, time: float, key: int, tonality: int) -> None

        Copy constructor
        """
        ...

    def copy(self) -> symusic.core.KeySignatureQuarter:
        """
        Deep copy
        """
        ...

    @property
    def degree(self) -> int: ...
    def from_numpy(
        self, arg0: numpy.typing.NDArray, arg1: numpy.typing.NDArray, /
    ) -> symusic.core.KeySignatureQuarterList: ...
    @property
    def key(self) -> int: ...
    @key.setter
    def key(self, arg: int, /) -> None: ...
    def shift_time(
        self, offset: float, inplace: bool = False
    ) -> symusic.core.KeySignatureQuarter:
        """
        Shift the event time by offset
        """
        ...

    @property
    def time(self) -> float: ...
    @time.setter
    def time(self, arg: float, /) -> None: ...
    @property
    def tonality(self) -> int: ...
    @tonality.setter
    def tonality(self, arg: int, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Quarter: ...

class KeySignatureQuarterList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.KeySignatureQuarter], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.KeySignatureQuarterList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.KeySignatureQuarter, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.KeySignatureQuarterList: ...
    def count(self, arg: symusic.core.KeySignatureQuarter, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.KeySignatureQuarterList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.KeySignatureQuarter, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def numpy(self) -> dict: ...
    def pop(self, index: int = -1) -> symusic.core.KeySignatureQuarter:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.KeySignatureQuarter, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Quarter: ...

class KeySignatureSecond:
    """
    None
    """

    def __init__(self, other: symusic.core.KeySignatureSecond) -> None:
        """
        __init__(self, time: float, key: int, tonality: int) -> None

        Copy constructor
        """
        ...

    def copy(self) -> symusic.core.KeySignatureSecond:
        """
        Deep copy
        """
        ...

    @property
    def degree(self) -> int: ...
    def from_numpy(
        self, arg0: numpy.typing.NDArray, arg1: numpy.typing.NDArray, /
    ) -> symusic.core.KeySignatureSecondList: ...
    @property
    def key(self) -> int: ...
    @key.setter
    def key(self, arg: int, /) -> None: ...
    def shift_time(
        self, offset: float, inplace: bool = False
    ) -> symusic.core.KeySignatureSecond:
        """
        Shift the event time by offset
        """
        ...

    @property
    def time(self) -> float: ...
    @time.setter
    def time(self, arg: float, /) -> None: ...
    @property
    def tonality(self) -> int: ...
    @tonality.setter
    def tonality(self, arg: int, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Second: ...

class KeySignatureSecondList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.KeySignatureSecond], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.KeySignatureSecondList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.KeySignatureSecond, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.KeySignatureSecondList: ...
    def count(self, arg: symusic.core.KeySignatureSecond, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.KeySignatureSecondList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.KeySignatureSecond, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def numpy(self) -> dict: ...
    def pop(self, index: int = -1) -> symusic.core.KeySignatureSecond:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.KeySignatureSecond, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Second: ...

class KeySignatureTick:
    """
    None
    """

    def __init__(self, other: symusic.core.KeySignatureTick) -> None:
        """
        __init__(self, time: int, key: int, tonality: int) -> None

        Copy constructor
        """
        ...

    def copy(self) -> symusic.core.KeySignatureTick:
        """
        Deep copy
        """
        ...

    @property
    def degree(self) -> int: ...
    def from_numpy(
        self, arg0: numpy.typing.NDArray, arg1: numpy.typing.NDArray, /
    ) -> symusic.core.KeySignatureTickList: ...
    @property
    def key(self) -> int: ...
    @key.setter
    def key(self, arg: int, /) -> None: ...
    def shift_time(
        self, offset: int, inplace: bool = False
    ) -> symusic.core.KeySignatureTick:
        """
        Shift the event time by offset
        """
        ...

    @property
    def time(self) -> int: ...
    @time.setter
    def time(self, arg: int, /) -> None: ...
    @property
    def tonality(self) -> int: ...
    @tonality.setter
    def tonality(self, arg: int, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Tick: ...

class KeySignatureTickList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.KeySignatureTick], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.KeySignatureTickList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.KeySignatureTick, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.KeySignatureTickList: ...
    def count(self, arg: symusic.core.KeySignatureTick, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.KeySignatureTickList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.KeySignatureTick, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def numpy(self) -> dict: ...
    def pop(self, index: int = -1) -> symusic.core.KeySignatureTick:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.KeySignatureTick, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Tick: ...

class NoteQuarter:
    """
    None
    """

    def __init__(self, other: symusic.core.NoteQuarter) -> None:
        """
        __init__(self, start: float, duration: float, pitch: int, velocity: int) -> None

        Copy constructor
        """
        ...

    def copy(self) -> symusic.core.NoteQuarter:
        """
        Deep copy
        """
        ...

    @property
    def duration(self) -> float: ...
    @duration.setter
    def duration(self, arg: float, /) -> None: ...
    def empty(self) -> bool:
        """
        duration <= 0 or velocity <= 0
        """
        ...

    @property
    def end(self) -> float: ...
    def end_time(self) -> float: ...
    def from_numpy(
        self,
        arg0: numpy.typing.NDArray,
        arg1: numpy.typing.NDArray,
        arg2: numpy.typing.NDArray,
        /,
    ) -> symusic.core.NoteQuarterList: ...
    @property
    def pitch(self) -> int: ...
    @pitch.setter
    def pitch(self, arg: int, /) -> None: ...
    def shift_pitch(self, offset: int, inplace: bool = False) -> object:
        """
        Shift the pitch by offset
        """
        ...

    def shift_time(
        self, offset: float, inplace: bool = False
    ) -> symusic.core.NoteQuarter:
        """
        Shift the event time by offset
        """
        ...

    def shift_velocity(self, offset: int, inplace: bool = False) -> object:
        """
        Shift the velocity by offset
        """
        ...

    @property
    def start(self) -> float: ...
    @start.setter
    def start(self, arg: float, /) -> None: ...
    @property
    def time(self) -> float: ...
    @time.setter
    def time(self, arg: float, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Quarter: ...
    @property
    def velocity(self) -> int: ...
    @velocity.setter
    def velocity(self, arg: int, /) -> None: ...

class NoteQuarterList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.NoteQuarter], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.NoteQuarterList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.NoteQuarter, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.NoteQuarterList: ...
    def count(self, arg: symusic.core.NoteQuarter, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.NoteQuarterList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.NoteQuarter, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def numpy(self) -> dict: ...
    def pop(self, index: int = -1) -> symusic.core.NoteQuarter:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.NoteQuarter, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Quarter: ...

class NoteSecond:
    """
    None
    """

    def __init__(self, other: symusic.core.NoteSecond) -> None:
        """
        __init__(self, start: float, duration: float, pitch: int, velocity: int) -> None

        Copy constructor
        """
        ...

    def copy(self) -> symusic.core.NoteSecond:
        """
        Deep copy
        """
        ...

    @property
    def duration(self) -> float: ...
    @duration.setter
    def duration(self, arg: float, /) -> None: ...
    def empty(self) -> bool:
        """
        duration <= 0 or velocity <= 0
        """
        ...

    @property
    def end(self) -> float: ...
    def end_time(self) -> float: ...
    def from_numpy(
        self,
        arg0: numpy.typing.NDArray,
        arg1: numpy.typing.NDArray,
        arg2: numpy.typing.NDArray,
        /,
    ) -> symusic.core.NoteSecondList: ...
    @property
    def pitch(self) -> int: ...
    @pitch.setter
    def pitch(self, arg: int, /) -> None: ...
    def shift_pitch(self, offset: int, inplace: bool = False) -> object:
        """
        Shift the pitch by offset
        """
        ...

    def shift_time(
        self, offset: float, inplace: bool = False
    ) -> symusic.core.NoteSecond:
        """
        Shift the event time by offset
        """
        ...

    def shift_velocity(self, offset: int, inplace: bool = False) -> object:
        """
        Shift the velocity by offset
        """
        ...

    @property
    def start(self) -> float: ...
    @start.setter
    def start(self, arg: float, /) -> None: ...
    @property
    def time(self) -> float: ...
    @time.setter
    def time(self, arg: float, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Second: ...
    @property
    def velocity(self) -> int: ...
    @velocity.setter
    def velocity(self, arg: int, /) -> None: ...

class NoteSecondList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.NoteSecond], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.NoteSecondList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.NoteSecond, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.NoteSecondList: ...
    def count(self, arg: symusic.core.NoteSecond, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.NoteSecondList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.NoteSecond, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def numpy(self) -> dict: ...
    def pop(self, index: int = -1) -> symusic.core.NoteSecond:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.NoteSecond, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Second: ...

class NoteTick:
    """
    None
    """

    def __init__(self, other: symusic.core.NoteTick) -> None:
        """
        __init__(self, start: int, duration: int, pitch: int, velocity: int) -> None

        Copy constructor
        """
        ...

    def copy(self) -> symusic.core.NoteTick:
        """
        Deep copy
        """
        ...

    @property
    def duration(self) -> int: ...
    @duration.setter
    def duration(self, arg: int, /) -> None: ...
    def empty(self) -> bool:
        """
        duration <= 0 or velocity <= 0
        """
        ...

    @property
    def end(self) -> int: ...
    def end_time(self) -> int: ...
    def from_numpy(
        self,
        arg0: numpy.typing.NDArray,
        arg1: numpy.typing.NDArray,
        arg2: numpy.typing.NDArray,
        /,
    ) -> symusic.core.NoteTickList: ...
    @property
    def pitch(self) -> int: ...
    @pitch.setter
    def pitch(self, arg: int, /) -> None: ...
    def shift_pitch(self, offset: int, inplace: bool = False) -> object:
        """
        Shift the pitch by offset
        """
        ...

    def shift_time(self, offset: int, inplace: bool = False) -> symusic.core.NoteTick:
        """
        Shift the event time by offset
        """
        ...

    def shift_velocity(self, offset: int, inplace: bool = False) -> object:
        """
        Shift the velocity by offset
        """
        ...

    @property
    def start(self) -> int: ...
    @start.setter
    def start(self, arg: int, /) -> None: ...
    @property
    def time(self) -> int: ...
    @time.setter
    def time(self, arg: int, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Tick: ...
    @property
    def velocity(self) -> int: ...
    @velocity.setter
    def velocity(self, arg: int, /) -> None: ...

class NoteTickList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.NoteTick], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.NoteTickList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.NoteTick, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.NoteTickList: ...
    def count(self, arg: symusic.core.NoteTick, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.NoteTickList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.NoteTick, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def numpy(self) -> dict: ...
    def pop(self, index: int = -1) -> symusic.core.NoteTick:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.NoteTick, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Tick: ...

class PedalQuarter:
    """
    None
    """

    def __init__(self, other: symusic.core.PedalQuarter) -> None:
        """
        __init__(self, time: float, duration: float) -> None

        Copy constructor
        """
        ...

    def copy(self) -> symusic.core.PedalQuarter:
        """
        Deep copy
        """
        ...

    @property
    def duration(self) -> float: ...
    @duration.setter
    def duration(self, arg: float, /) -> None: ...
    @property
    def end(self) -> float: ...
    def from_numpy(
        self, arg: numpy.typing.NDArray, /
    ) -> symusic.core.PedalQuarterList: ...
    def shift_time(
        self, offset: float, inplace: bool = False
    ) -> symusic.core.PedalQuarter:
        """
        Shift the event time by offset
        """
        ...

    @property
    def time(self) -> float: ...
    @time.setter
    def time(self, arg: float, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Quarter: ...

class PedalQuarterList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.PedalQuarter], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.PedalQuarterList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.PedalQuarter, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.PedalQuarterList: ...
    def count(self, arg: symusic.core.PedalQuarter, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.PedalQuarterList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.PedalQuarter, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def numpy(self) -> dict: ...
    def pop(self, index: int = -1) -> symusic.core.PedalQuarter:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.PedalQuarter, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Quarter: ...

class PedalSecond:
    """
    None
    """

    def __init__(self, other: symusic.core.PedalSecond) -> None:
        """
        __init__(self, time: float, duration: float) -> None

        Copy constructor
        """
        ...

    def copy(self) -> symusic.core.PedalSecond:
        """
        Deep copy
        """
        ...

    @property
    def duration(self) -> float: ...
    @duration.setter
    def duration(self, arg: float, /) -> None: ...
    @property
    def end(self) -> float: ...
    def from_numpy(
        self, arg: numpy.typing.NDArray, /
    ) -> symusic.core.PedalSecondList: ...
    def shift_time(
        self, offset: float, inplace: bool = False
    ) -> symusic.core.PedalSecond:
        """
        Shift the event time by offset
        """
        ...

    @property
    def time(self) -> float: ...
    @time.setter
    def time(self, arg: float, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Second: ...

class PedalSecondList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.PedalSecond], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.PedalSecondList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.PedalSecond, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.PedalSecondList: ...
    def count(self, arg: symusic.core.PedalSecond, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.PedalSecondList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.PedalSecond, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def numpy(self) -> dict: ...
    def pop(self, index: int = -1) -> symusic.core.PedalSecond:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.PedalSecond, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Second: ...

class PedalTick:
    """
    None
    """

    def __init__(self, other: symusic.core.PedalTick) -> None:
        """
        __init__(self, time: int, duration: int) -> None

        Copy constructor
        """
        ...

    def copy(self) -> symusic.core.PedalTick:
        """
        Deep copy
        """
        ...

    @property
    def duration(self) -> int: ...
    @duration.setter
    def duration(self, arg: int, /) -> None: ...
    @property
    def end(self) -> int: ...
    def from_numpy(
        self, arg: numpy.typing.NDArray, /
    ) -> symusic.core.PedalTickList: ...
    def shift_time(self, offset: int, inplace: bool = False) -> symusic.core.PedalTick:
        """
        Shift the event time by offset
        """
        ...

    @property
    def time(self) -> int: ...
    @time.setter
    def time(self, arg: int, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Tick: ...

class PedalTickList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.PedalTick], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.PedalTickList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.PedalTick, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.PedalTickList: ...
    def count(self, arg: symusic.core.PedalTick, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.PedalTickList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.PedalTick, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def numpy(self) -> dict: ...
    def pop(self, index: int = -1) -> symusic.core.PedalTick:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.PedalTick, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Tick: ...

class PitchBendQuarter:
    """
    None
    """

    def __init__(self, other: symusic.core.PitchBendQuarter) -> None:
        """
        __init__(self, time: float, value: int) -> None

        Copy constructor
        """
        ...

    def copy(self) -> symusic.core.PitchBendQuarter:
        """
        Deep copy
        """
        ...

    def from_numpy(
        self, arg: numpy.typing.NDArray, /
    ) -> symusic.core.PitchBendQuarterList: ...
    def shift_time(
        self, offset: float, inplace: bool = False
    ) -> symusic.core.PitchBendQuarter:
        """
        Shift the event time by offset
        """
        ...

    @property
    def time(self) -> float: ...
    @time.setter
    def time(self, arg: float, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Quarter: ...
    @property
    def value(self) -> int: ...
    @value.setter
    def value(self, arg: int, /) -> None: ...

class PitchBendQuarterList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.PitchBendQuarter], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.PitchBendQuarterList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.PitchBendQuarter, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.PitchBendQuarterList: ...
    def count(self, arg: symusic.core.PitchBendQuarter, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.PitchBendQuarterList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.PitchBendQuarter, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def numpy(self) -> dict: ...
    def pop(self, index: int = -1) -> symusic.core.PitchBendQuarter:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.PitchBendQuarter, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Quarter: ...

class PitchBendSecond:
    """
    None
    """

    def __init__(self, other: symusic.core.PitchBendSecond) -> None:
        """
        __init__(self, time: float, value: int) -> None

        Copy constructor
        """
        ...

    def copy(self) -> symusic.core.PitchBendSecond:
        """
        Deep copy
        """
        ...

    def from_numpy(
        self, arg: numpy.typing.NDArray, /
    ) -> symusic.core.PitchBendSecondList: ...
    def shift_time(
        self, offset: float, inplace: bool = False
    ) -> symusic.core.PitchBendSecond:
        """
        Shift the event time by offset
        """
        ...

    @property
    def time(self) -> float: ...
    @time.setter
    def time(self, arg: float, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Second: ...
    @property
    def value(self) -> int: ...
    @value.setter
    def value(self, arg: int, /) -> None: ...

class PitchBendSecondList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.PitchBendSecond], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.PitchBendSecondList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.PitchBendSecond, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.PitchBendSecondList: ...
    def count(self, arg: symusic.core.PitchBendSecond, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.PitchBendSecondList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.PitchBendSecond, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def numpy(self) -> dict: ...
    def pop(self, index: int = -1) -> symusic.core.PitchBendSecond:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.PitchBendSecond, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Second: ...

class PitchBendTick:
    """
    None
    """

    def __init__(self, other: symusic.core.PitchBendTick) -> None:
        """
        __init__(self, time: int, value: int) -> None

        Copy constructor
        """
        ...

    def copy(self) -> symusic.core.PitchBendTick:
        """
        Deep copy
        """
        ...

    def from_numpy(
        self, arg: numpy.typing.NDArray, /
    ) -> symusic.core.PitchBendTickList: ...
    def shift_time(
        self, offset: int, inplace: bool = False
    ) -> symusic.core.PitchBendTick:
        """
        Shift the event time by offset
        """
        ...

    @property
    def time(self) -> int: ...
    @time.setter
    def time(self, arg: int, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Tick: ...
    @property
    def value(self) -> int: ...
    @value.setter
    def value(self, arg: int, /) -> None: ...

class PitchBendTickList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.PitchBendTick], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.PitchBendTickList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.PitchBendTick, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.PitchBendTickList: ...
    def count(self, arg: symusic.core.PitchBendTick, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.PitchBendTickList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.PitchBendTick, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def numpy(self) -> dict: ...
    def pop(self, index: int = -1) -> symusic.core.PitchBendTick:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.PitchBendTick, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Tick: ...

class Quarter:
    """
    None
    """

    def __init__(self) -> None: ...
    def is_time_unit(self) -> bool: ...

class ScoreQuarter:
    """
    None
    """

    def __init__(self, path: os.PathLike) -> None:
        """
        Load from midi file
        """
        ...

    @overload
    def __init__(self, other: symusic.core.ScoreQuarter) -> None:
        """
        Copy constructor
        """
        ...

    @overload
    def __init__(self, path: str) -> None:
        """
        Load from midi file
        """
        ...

    def clip(
        self, start: float, end: float, clip_end: bool = False
    ) -> symusic.core.ScoreQuarter:
        """
        Clip events a given time range
        """
        ...

    def copy(self) -> symusic.core.ScoreQuarter:
        """
        Deep copy
        """
        ...

    def dump_abc(self, path: os.PathLike, warn: bool = True) -> None:
        """
        Dump to abc file
        """
        ...

    @overload
    def dump_abc(self, path: str, warn: bool = True) -> None:
        """
        Dump to abc file
        """
        ...

    def dump_midi(self, path: os.PathLike) -> None:
        """
        Dump to midi file
        """
        ...

    @overload
    def dump_midi(self, path: str) -> None:
        """
        Dump to midi file
        """
        ...

    def empty(self) -> bool: ...
    def end(self) -> float: ...
    def from_abc(abc: str) -> symusic.core.ScoreQuarter:
        """
        Load from abc string
        """
        ...

    def from_file(
        path: os.PathLike, fmt: Optional[str] = None
    ) -> symusic.core.ScoreQuarter:
        """
        Load from midi file
        """
        ...

    @overload
    def from_file(path: str, fmt: Optional[str] = None) -> symusic.core.ScoreQuarter:
        """
        Load from midi file
        """
        ...

    @property
    def key_signatures(self) -> symusic.core.KeySignatureQuarterList: ...
    @key_signatures.setter
    def key_signatures(self, arg: symusic.core.KeySignatureQuarterList, /) -> None: ...
    @property
    def lyrics(self) -> symusic.core.TextMetaQuarterList: ...
    @lyrics.setter
    def lyrics(self, arg: symusic.core.TextMetaQuarterList, /) -> None: ...
    @property
    def markers(self) -> symusic.core.TextMetaQuarterList: ...
    @markers.setter
    def markers(self, arg: symusic.core.TextMetaQuarterList, /) -> None: ...
    def note_num(self) -> int: ...
    def pianoroll(
        self,
        modes: list[str],
        pitchRange: tuple[int, int] = (0, 127),
        encodeVelocity: bool = False,
    ) -> numpy.typing.NDArray: ...
    def resample(
        self, tpq: int, min_dur: Optional[int] = None
    ) -> symusic.core.ScoreTick:
        """
        Resample to another ticks per quarter
        """
        ...

    def shift_pitch(self, offset: int, inplace: bool = False) -> object: ...
    def shift_time(self, offset: float, inplace: bool = False) -> object: ...
    def shift_velocity(self, offset: int, inplace: bool = False) -> object: ...
    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    def start(self) -> float: ...
    @property
    def tempos(self) -> symusic.core.TempoQuarterList: ...
    @tempos.setter
    def tempos(self, arg: symusic.core.TempoQuarterList, /) -> None: ...
    @property
    def ticks_per_quarter(self) -> int: ...
    @ticks_per_quarter.setter
    def ticks_per_quarter(self, arg: int, /) -> None: ...
    @property
    def time_signatures(self) -> symusic.core.TimeSignatureQuarterList: ...
    @time_signatures.setter
    def time_signatures(
        self, arg: symusic.core.TimeSignatureQuarterList, /
    ) -> None: ...
    def to(self, ttype: object, min_dur: Optional[object] = None) -> object:
        """
        Convert to another time unit
        """
        ...

    @property
    def tracks(self) -> symusic.core.TrackQuarterList: ...
    @tracks.setter
    def tracks(self, arg: symusic.core.TrackQuarterList, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Quarter: ...

class ScoreSecond:
    """
    None
    """

    def __init__(self, path: os.PathLike) -> None:
        """
        Load from midi file
        """
        ...

    @overload
    def __init__(self, other: symusic.core.ScoreSecond) -> None:
        """
        Copy constructor
        """
        ...

    @overload
    def __init__(self, path: str) -> None:
        """
        Load from midi file
        """
        ...

    def clip(
        self, start: float, end: float, clip_end: bool = False
    ) -> symusic.core.ScoreSecond:
        """
        Clip events a given time range
        """
        ...

    def copy(self) -> symusic.core.ScoreSecond:
        """
        Deep copy
        """
        ...

    def dump_abc(self, path: os.PathLike, warn: bool = True) -> None:
        """
        Dump to abc file
        """
        ...

    @overload
    def dump_abc(self, path: str, warn: bool = True) -> None:
        """
        Dump to abc file
        """
        ...

    def dump_midi(self, path: os.PathLike) -> None:
        """
        Dump to midi file
        """
        ...

    @overload
    def dump_midi(self, path: str) -> None:
        """
        Dump to midi file
        """
        ...

    def empty(self) -> bool: ...
    def end(self) -> float: ...
    def from_abc(abc: str) -> symusic.core.ScoreSecond:
        """
        Load from abc string
        """
        ...

    def from_file(
        path: os.PathLike, fmt: Optional[str] = None
    ) -> symusic.core.ScoreSecond:
        """
        Load from midi file
        """
        ...

    @overload
    def from_file(path: str, fmt: Optional[str] = None) -> symusic.core.ScoreSecond:
        """
        Load from midi file
        """
        ...

    @property
    def key_signatures(self) -> symusic.core.KeySignatureSecondList: ...
    @key_signatures.setter
    def key_signatures(self, arg: symusic.core.KeySignatureSecondList, /) -> None: ...
    @property
    def lyrics(self) -> symusic.core.TextMetaSecondList: ...
    @lyrics.setter
    def lyrics(self, arg: symusic.core.TextMetaSecondList, /) -> None: ...
    @property
    def markers(self) -> symusic.core.TextMetaSecondList: ...
    @markers.setter
    def markers(self, arg: symusic.core.TextMetaSecondList, /) -> None: ...
    def note_num(self) -> int: ...
    def pianoroll(
        self,
        modes: list[str],
        pitchRange: tuple[int, int] = (0, 127),
        encodeVelocity: bool = False,
    ) -> numpy.typing.NDArray: ...
    def resample(
        self, tpq: int, min_dur: Optional[int] = None
    ) -> symusic.core.ScoreTick:
        """
        Resample to another ticks per quarter
        """
        ...

    def shift_pitch(self, offset: int, inplace: bool = False) -> object: ...
    def shift_time(self, offset: float, inplace: bool = False) -> object: ...
    def shift_velocity(self, offset: int, inplace: bool = False) -> object: ...
    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    def start(self) -> float: ...
    @property
    def tempos(self) -> symusic.core.TempoSecondList: ...
    @tempos.setter
    def tempos(self, arg: symusic.core.TempoSecondList, /) -> None: ...
    @property
    def ticks_per_quarter(self) -> int: ...
    @ticks_per_quarter.setter
    def ticks_per_quarter(self, arg: int, /) -> None: ...
    @property
    def time_signatures(self) -> symusic.core.TimeSignatureSecondList: ...
    @time_signatures.setter
    def time_signatures(self, arg: symusic.core.TimeSignatureSecondList, /) -> None: ...
    def to(self, ttype: object, min_dur: Optional[object] = None) -> object:
        """
        Convert to another time unit
        """
        ...

    @property
    def tracks(self) -> symusic.core.TrackSecondList: ...
    @tracks.setter
    def tracks(self, arg: symusic.core.TrackSecondList, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Second: ...

class ScoreTick:
    """
    None
    """

    def __init__(self, path: os.PathLike) -> None:
        """
        Load from midi file
        """
        ...

    @overload
    def __init__(self, other: symusic.core.ScoreTick) -> None:
        """
        Copy constructor
        """
        ...

    @overload
    def __init__(self, path: str) -> None:
        """
        Load from midi file
        """
        ...

    def clip(
        self, start: int, end: int, clip_end: bool = False
    ) -> symusic.core.ScoreTick:
        """
        Clip events a given time range
        """
        ...

    def copy(self) -> symusic.core.ScoreTick:
        """
        Deep copy
        """
        ...

    def dump_abc(self, path: os.PathLike, warn: bool = True) -> None:
        """
        Dump to abc file
        """
        ...

    @overload
    def dump_abc(self, path: str, warn: bool = True) -> None:
        """
        Dump to abc file
        """
        ...

    def dump_midi(self, path: os.PathLike) -> None:
        """
        Dump to midi file
        """
        ...

    @overload
    def dump_midi(self, path: str) -> None:
        """
        Dump to midi file
        """
        ...

    def empty(self) -> bool: ...
    def end(self) -> int: ...
    def from_abc(abc: str) -> symusic.core.ScoreTick:
        """
        Load from abc string
        """
        ...

    def from_file(
        path: os.PathLike, fmt: Optional[str] = None
    ) -> symusic.core.ScoreTick:
        """
        Load from midi file
        """
        ...

    @overload
    def from_file(path: str, fmt: Optional[str] = None) -> symusic.core.ScoreTick:
        """
        Load from midi file
        """
        ...

    @property
    def key_signatures(self) -> symusic.core.KeySignatureTickList: ...
    @key_signatures.setter
    def key_signatures(self, arg: symusic.core.KeySignatureTickList, /) -> None: ...
    @property
    def lyrics(self) -> symusic.core.TextMetaTickList: ...
    @lyrics.setter
    def lyrics(self, arg: symusic.core.TextMetaTickList, /) -> None: ...
    @property
    def markers(self) -> symusic.core.TextMetaTickList: ...
    @markers.setter
    def markers(self, arg: symusic.core.TextMetaTickList, /) -> None: ...
    def note_num(self) -> int: ...
    def pianoroll(
        self,
        modes: list[str],
        pitchRange: tuple[int, int] = (0, 127),
        encodeVelocity: bool = False,
    ) -> numpy.typing.NDArray: ...
    def resample(
        self, tpq: int, min_dur: Optional[int] = None
    ) -> symusic.core.ScoreTick:
        """
        Resample to another ticks per quarter
        """
        ...

    def shift_pitch(self, offset: int, inplace: bool = False) -> object: ...
    def shift_time(self, offset: int, inplace: bool = False) -> object: ...
    def shift_velocity(self, offset: int, inplace: bool = False) -> object: ...
    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    def start(self) -> int: ...
    @property
    def tempos(self) -> symusic.core.TempoTickList: ...
    @tempos.setter
    def tempos(self, arg: symusic.core.TempoTickList, /) -> None: ...
    @property
    def ticks_per_quarter(self) -> int: ...
    @ticks_per_quarter.setter
    def ticks_per_quarter(self, arg: int, /) -> None: ...
    @property
    def time_signatures(self) -> symusic.core.TimeSignatureTickList: ...
    @time_signatures.setter
    def time_signatures(self, arg: symusic.core.TimeSignatureTickList, /) -> None: ...
    def to(self, ttype: object, min_dur: Optional[object] = None) -> object:
        """
        Convert to another time unit
        """
        ...

    @property
    def tracks(self) -> symusic.core.TrackTickList: ...
    @tracks.setter
    def tracks(self, arg: symusic.core.TrackTickList, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Tick: ...

class Second:
    """
    None
    """

    def __init__(self) -> None: ...
    def is_time_unit(self) -> bool: ...

class TempoQuarter:
    """
    None
    """

    def __init__(self, other: symusic.core.TempoQuarter) -> None:
        """
        __init__(self, time: float, qpm: Optional[float] = None, mspq: Optional[int] = None) -> None

        Copy constructor
        """
        ...

    def copy(self) -> symusic.core.TempoQuarter:
        """
        Deep copy
        """
        ...

    def from_numpy(
        self, arg: numpy.typing.NDArray, /
    ) -> symusic.core.TempoQuarterList: ...
    @property
    def mspq(self) -> int:
        """
        Microseconds per quarter note
        """
        ...
    @mspq.setter
    def mspq(self, arg: int, /) -> None:
        """
        Microseconds per quarter note
        """
        ...

    @property
    def qpm(self) -> float:
        """
        Quarter per minute, the same as tempo
        """
        ...
    @qpm.setter
    def qpm(self, arg: float, /) -> symusic.core.TempoQuarter:
        """
        Quarter per minute, the same as tempo
        """
        ...

    def shift_time(
        self, offset: float, inplace: bool = False
    ) -> symusic.core.TempoQuarter:
        """
        Shift the event time by offset
        """
        ...

    @property
    def tempo(self) -> float:
        """
        The same as qpm
        """
        ...
    @tempo.setter
    def tempo(self, arg: float, /) -> symusic.core.TempoQuarter:
        """
        The same as qpm
        """
        ...

    @property
    def time(self) -> float: ...
    @time.setter
    def time(self, arg: float, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Quarter: ...

class TempoQuarterList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.TempoQuarter], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.TempoQuarterList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.TempoQuarter, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.TempoQuarterList: ...
    def count(self, arg: symusic.core.TempoQuarter, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.TempoQuarterList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.TempoQuarter, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def numpy(self) -> dict: ...
    def pop(self, index: int = -1) -> symusic.core.TempoQuarter:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.TempoQuarter, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Quarter: ...

class TempoSecond:
    """
    None
    """

    def __init__(self, other: symusic.core.TempoSecond) -> None:
        """
        __init__(self, time: float, qpm: Optional[float] = None, mspq: Optional[int] = None) -> None

        Copy constructor
        """
        ...

    def copy(self) -> symusic.core.TempoSecond:
        """
        Deep copy
        """
        ...

    def from_numpy(
        self, arg: numpy.typing.NDArray, /
    ) -> symusic.core.TempoSecondList: ...
    @property
    def mspq(self) -> int:
        """
        Microseconds per quarter note
        """
        ...
    @mspq.setter
    def mspq(self, arg: int, /) -> None:
        """
        Microseconds per quarter note
        """
        ...

    @property
    def qpm(self) -> float:
        """
        Quarter per minute, the same as tempo
        """
        ...
    @qpm.setter
    def qpm(self, arg: float, /) -> symusic.core.TempoSecond:
        """
        Quarter per minute, the same as tempo
        """
        ...

    def shift_time(
        self, offset: float, inplace: bool = False
    ) -> symusic.core.TempoSecond:
        """
        Shift the event time by offset
        """
        ...

    @property
    def tempo(self) -> float:
        """
        The same as qpm
        """
        ...
    @tempo.setter
    def tempo(self, arg: float, /) -> symusic.core.TempoSecond:
        """
        The same as qpm
        """
        ...

    @property
    def time(self) -> float: ...
    @time.setter
    def time(self, arg: float, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Second: ...

class TempoSecondList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.TempoSecond], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.TempoSecondList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.TempoSecond, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.TempoSecondList: ...
    def count(self, arg: symusic.core.TempoSecond, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.TempoSecondList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.TempoSecond, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def numpy(self) -> dict: ...
    def pop(self, index: int = -1) -> symusic.core.TempoSecond:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.TempoSecond, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Second: ...

class TempoTick:
    """
    None
    """

    def __init__(self, other: symusic.core.TempoTick) -> None:
        """
        __init__(self, time: int, qpm: Optional[float] = None, mspq: Optional[int] = None) -> None

        Copy constructor
        """
        ...

    def copy(self) -> symusic.core.TempoTick:
        """
        Deep copy
        """
        ...

    def from_numpy(
        self, arg: numpy.typing.NDArray, /
    ) -> symusic.core.TempoTickList: ...
    @property
    def mspq(self) -> int:
        """
        Microseconds per quarter note
        """
        ...
    @mspq.setter
    def mspq(self, arg: int, /) -> None:
        """
        Microseconds per quarter note
        """
        ...

    @property
    def qpm(self) -> float:
        """
        Quarter per minute, the same as tempo
        """
        ...
    @qpm.setter
    def qpm(self, arg: float, /) -> symusic.core.TempoTick:
        """
        Quarter per minute, the same as tempo
        """
        ...

    def shift_time(self, offset: int, inplace: bool = False) -> symusic.core.TempoTick:
        """
        Shift the event time by offset
        """
        ...

    @property
    def tempo(self) -> float:
        """
        The same as qpm
        """
        ...
    @tempo.setter
    def tempo(self, arg: float, /) -> symusic.core.TempoTick:
        """
        The same as qpm
        """
        ...

    @property
    def time(self) -> int: ...
    @time.setter
    def time(self, arg: int, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Tick: ...

class TempoTickList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.TempoTick], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.TempoTickList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.TempoTick, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.TempoTickList: ...
    def count(self, arg: symusic.core.TempoTick, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.TempoTickList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.TempoTick, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def numpy(self) -> dict: ...
    def pop(self, index: int = -1) -> symusic.core.TempoTick:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.TempoTick, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Tick: ...

class TextMetaQuarter:
    """
    None
    """

    def __init__(self, other: symusic.core.TextMetaQuarter) -> None:
        """
        __init__(self, time: float, text: str) -> None

        Copy constructor
        """
        ...

    def copy(self) -> symusic.core.TextMetaQuarter:
        """
        Deep copy
        """
        ...

    def from_numpy(self, arg: numpy.typing.NDArray, /) -> None: ...
    def shift_time(
        self, offset: float, inplace: bool = False
    ) -> symusic.core.TextMetaQuarter:
        """
        Shift the event time by offset
        """
        ...

    @property
    def text(self) -> str: ...
    @text.setter
    def text(self, arg: str, /) -> None: ...
    @property
    def time(self) -> float: ...
    @time.setter
    def time(self, arg: float, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Quarter: ...

class TextMetaQuarterList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.TextMetaQuarter], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.TextMetaQuarterList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.TextMetaQuarter, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.TextMetaQuarterList: ...
    def count(self, arg: symusic.core.TextMetaQuarter, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.TextMetaQuarterList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.TextMetaQuarter, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def numpy(self) -> None: ...
    def pop(self, index: int = -1) -> symusic.core.TextMetaQuarter:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.TextMetaQuarter, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Quarter: ...

class TextMetaSecond:
    """
    None
    """

    def __init__(self, other: symusic.core.TextMetaSecond) -> None:
        """
        __init__(self, time: float, text: str) -> None

        Copy constructor
        """
        ...

    def copy(self) -> symusic.core.TextMetaSecond:
        """
        Deep copy
        """
        ...

    def from_numpy(self, arg: numpy.typing.NDArray, /) -> None: ...
    def shift_time(
        self, offset: float, inplace: bool = False
    ) -> symusic.core.TextMetaSecond:
        """
        Shift the event time by offset
        """
        ...

    @property
    def text(self) -> str: ...
    @text.setter
    def text(self, arg: str, /) -> None: ...
    @property
    def time(self) -> float: ...
    @time.setter
    def time(self, arg: float, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Second: ...

class TextMetaSecondList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.TextMetaSecond], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.TextMetaSecondList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.TextMetaSecond, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.TextMetaSecondList: ...
    def count(self, arg: symusic.core.TextMetaSecond, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.TextMetaSecondList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.TextMetaSecond, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def numpy(self) -> None: ...
    def pop(self, index: int = -1) -> symusic.core.TextMetaSecond:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.TextMetaSecond, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Second: ...

class TextMetaTick:
    """
    None
    """

    def __init__(self, other: symusic.core.TextMetaTick) -> None:
        """
        __init__(self, time: int, text: str) -> None

        Copy constructor
        """
        ...

    def copy(self) -> symusic.core.TextMetaTick:
        """
        Deep copy
        """
        ...

    def from_numpy(self, arg: numpy.typing.NDArray, /) -> None: ...
    def shift_time(
        self, offset: int, inplace: bool = False
    ) -> symusic.core.TextMetaTick:
        """
        Shift the event time by offset
        """
        ...

    @property
    def text(self) -> str: ...
    @text.setter
    def text(self, arg: str, /) -> None: ...
    @property
    def time(self) -> int: ...
    @time.setter
    def time(self, arg: int, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Tick: ...

class TextMetaTickList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.TextMetaTick], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.TextMetaTickList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.TextMetaTick, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.TextMetaTickList: ...
    def count(self, arg: symusic.core.TextMetaTick, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.TextMetaTickList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.TextMetaTick, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def numpy(self) -> None: ...
    def pop(self, index: int = -1) -> symusic.core.TextMetaTick:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.TextMetaTick, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Tick: ...

class Tick:
    """
    None
    """

    def __init__(self) -> None: ...
    def is_time_unit(self) -> bool: ...

class TimeSignatureQuarter:
    """
    None
    """

    def __init__(self, other: symusic.core.TimeSignatureQuarter) -> None:
        """
        __init__(self, arg0: float, arg1: int, arg2: int, /) -> None

        Copy constructor
        """
        ...

    def copy(self) -> symusic.core.TimeSignatureQuarter:
        """
        Deep copy
        """
        ...

    @property
    def denominator(self) -> int: ...
    @denominator.setter
    def denominator(self, arg: int, /) -> None: ...
    def from_numpy(
        self, arg0: numpy.typing.NDArray, arg1: numpy.typing.NDArray, /
    ) -> symusic.core.TimeSignatureQuarterList: ...
    @property
    def numerator(self) -> int: ...
    @numerator.setter
    def numerator(self, arg: int, /) -> None: ...
    def shift_time(
        self, offset: float, inplace: bool = False
    ) -> symusic.core.TimeSignatureQuarter:
        """
        Shift the event time by offset
        """
        ...

    @property
    def time(self) -> float: ...
    @time.setter
    def time(self, arg: float, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Quarter: ...

class TimeSignatureQuarterList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.TimeSignatureQuarter], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.TimeSignatureQuarterList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.TimeSignatureQuarter, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.TimeSignatureQuarterList: ...
    def count(self, arg: symusic.core.TimeSignatureQuarter, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.TimeSignatureQuarterList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.TimeSignatureQuarter, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def numpy(self) -> dict: ...
    def pop(self, index: int = -1) -> symusic.core.TimeSignatureQuarter:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.TimeSignatureQuarter, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Quarter: ...

class TimeSignatureSecond:
    """
    None
    """

    def __init__(self, other: symusic.core.TimeSignatureSecond) -> None:
        """
        __init__(self, arg0: float, arg1: int, arg2: int, /) -> None

        Copy constructor
        """
        ...

    def copy(self) -> symusic.core.TimeSignatureSecond:
        """
        Deep copy
        """
        ...

    @property
    def denominator(self) -> int: ...
    @denominator.setter
    def denominator(self, arg: int, /) -> None: ...
    def from_numpy(
        self, arg0: numpy.typing.NDArray, arg1: numpy.typing.NDArray, /
    ) -> symusic.core.TimeSignatureSecondList: ...
    @property
    def numerator(self) -> int: ...
    @numerator.setter
    def numerator(self, arg: int, /) -> None: ...
    def shift_time(
        self, offset: float, inplace: bool = False
    ) -> symusic.core.TimeSignatureSecond:
        """
        Shift the event time by offset
        """
        ...

    @property
    def time(self) -> float: ...
    @time.setter
    def time(self, arg: float, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Second: ...

class TimeSignatureSecondList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.TimeSignatureSecond], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.TimeSignatureSecondList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.TimeSignatureSecond, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.TimeSignatureSecondList: ...
    def count(self, arg: symusic.core.TimeSignatureSecond, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.TimeSignatureSecondList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.TimeSignatureSecond, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def numpy(self) -> dict: ...
    def pop(self, index: int = -1) -> symusic.core.TimeSignatureSecond:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.TimeSignatureSecond, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Second: ...

class TimeSignatureTick:
    """
    None
    """

    def __init__(self, other: symusic.core.TimeSignatureTick) -> None:
        """
        __init__(self, arg0: int, arg1: int, arg2: int, /) -> None

        Copy constructor
        """
        ...

    def copy(self) -> symusic.core.TimeSignatureTick:
        """
        Deep copy
        """
        ...

    @property
    def denominator(self) -> int: ...
    @denominator.setter
    def denominator(self, arg: int, /) -> None: ...
    def from_numpy(
        self, arg0: numpy.typing.NDArray, arg1: numpy.typing.NDArray, /
    ) -> symusic.core.TimeSignatureTickList: ...
    @property
    def numerator(self) -> int: ...
    @numerator.setter
    def numerator(self, arg: int, /) -> None: ...
    def shift_time(
        self, offset: int, inplace: bool = False
    ) -> symusic.core.TimeSignatureTick:
        """
        Shift the event time by offset
        """
        ...

    @property
    def time(self) -> int: ...
    @time.setter
    def time(self, arg: int, /) -> None: ...
    @property
    def ttype(self) -> symusic.core.Tick: ...

class TimeSignatureTickList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.TimeSignatureTick], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.TimeSignatureTickList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.TimeSignatureTick, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.TimeSignatureTickList: ...
    def count(self, arg: symusic.core.TimeSignatureTick, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.TimeSignatureTickList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.TimeSignatureTick, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def numpy(self) -> dict: ...
    def pop(self, index: int = -1) -> symusic.core.TimeSignatureTick:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.TimeSignatureTick, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Tick: ...

class TrackQuarter:
    """
    None
    """

    def __init__(self) -> None:
        """
        __init__(self, other: symusic.core.TrackQuarter) -> None

        Copy constructor
        """
        ...

    def clip(
        self, start: float, end: float, clip_end: bool = False
    ) -> symusic.core.TrackQuarter:
        """
        Clip notes and controls to a given time range
        """
        ...

    @property
    def controls(self) -> symusic.core.ControlChangeQuarterList: ...
    @controls.setter
    def controls(self, arg: symusic.core.ControlChangeQuarterList, /) -> None: ...
    def copy(self) -> symusic.core.TrackQuarter:
        """
        Deep copy
        """
        ...

    def empty(self) -> bool: ...
    def end(self) -> float: ...
    @property
    def is_drum(self) -> bool: ...
    @is_drum.setter
    def is_drum(self, arg: bool, /) -> None: ...
    @property
    def name(self) -> str: ...
    @name.setter
    def name(self, arg: str, /) -> None: ...
    def note_num(self) -> int: ...
    @property
    def notes(self) -> symusic.core.NoteQuarterList: ...
    @notes.setter
    def notes(self, arg: symusic.core.NoteQuarterList, /) -> None: ...
    @property
    def pedals(self) -> symusic.core.PedalQuarterList: ...
    @pedals.setter
    def pedals(self, arg: symusic.core.PedalQuarterList, /) -> None: ...
    def pianoroll(
        self,
        modes: list[str],
        pitchRange: tuple[int, int] = (0, 127),
        encodeVelocity: bool = False,
    ) -> numpy.typing.NDArray: ...
    @property
    def pitch_bends(self) -> symusic.core.PitchBendQuarterList: ...
    @pitch_bends.setter
    def pitch_bends(self, arg: symusic.core.PitchBendQuarterList, /) -> None: ...
    @property
    def program(self) -> int: ...
    @program.setter
    def program(self, arg: int, /) -> None: ...
    def shift_pitch(self, offset: int, inplace: bool = False) -> object: ...
    def shift_time(self, offset: float, inplace: bool = False) -> object: ...
    def shift_velocity(self, offset: int, inplace: bool = False) -> object: ...
    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    def start(self) -> float: ...
    @property
    def ttype(self) -> symusic.core.Quarter: ...

class TrackQuarterList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.TrackQuarter], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.TrackQuarterList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.TrackQuarter, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.TrackQuarterList: ...
    def count(self, arg: symusic.core.TrackQuarter, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.TrackQuarterList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.TrackQuarter, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def pop(self, index: int = -1) -> symusic.core.TrackQuarter:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.TrackQuarter, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Quarter: ...

class TrackSecond:
    """
    None
    """

    def __init__(self) -> None:
        """
        __init__(self, other: symusic.core.TrackSecond) -> None

        Copy constructor
        """
        ...

    def clip(
        self, start: float, end: float, clip_end: bool = False
    ) -> symusic.core.TrackSecond:
        """
        Clip notes and controls to a given time range
        """
        ...

    @property
    def controls(self) -> symusic.core.ControlChangeSecondList: ...
    @controls.setter
    def controls(self, arg: symusic.core.ControlChangeSecondList, /) -> None: ...
    def copy(self) -> symusic.core.TrackSecond:
        """
        Deep copy
        """
        ...

    def empty(self) -> bool: ...
    def end(self) -> float: ...
    @property
    def is_drum(self) -> bool: ...
    @is_drum.setter
    def is_drum(self, arg: bool, /) -> None: ...
    @property
    def name(self) -> str: ...
    @name.setter
    def name(self, arg: str, /) -> None: ...
    def note_num(self) -> int: ...
    @property
    def notes(self) -> symusic.core.NoteSecondList: ...
    @notes.setter
    def notes(self, arg: symusic.core.NoteSecondList, /) -> None: ...
    @property
    def pedals(self) -> symusic.core.PedalSecondList: ...
    @pedals.setter
    def pedals(self, arg: symusic.core.PedalSecondList, /) -> None: ...
    def pianoroll(
        self,
        modes: list[str],
        pitchRange: tuple[int, int] = (0, 127),
        encodeVelocity: bool = False,
    ) -> numpy.typing.NDArray: ...
    @property
    def pitch_bends(self) -> symusic.core.PitchBendSecondList: ...
    @pitch_bends.setter
    def pitch_bends(self, arg: symusic.core.PitchBendSecondList, /) -> None: ...
    @property
    def program(self) -> int: ...
    @program.setter
    def program(self, arg: int, /) -> None: ...
    def shift_pitch(self, offset: int, inplace: bool = False) -> object: ...
    def shift_time(self, offset: float, inplace: bool = False) -> object: ...
    def shift_velocity(self, offset: int, inplace: bool = False) -> object: ...
    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    def start(self) -> float: ...
    @property
    def ttype(self) -> symusic.core.Second: ...

class TrackSecondList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.TrackSecond], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.TrackSecondList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.TrackSecond, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.TrackSecondList: ...
    def count(self, arg: symusic.core.TrackSecond, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.TrackSecondList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.TrackSecond, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def pop(self, index: int = -1) -> symusic.core.TrackSecond:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.TrackSecond, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Second: ...

class TrackTick:
    """
    None
    """

    def __init__(self) -> None:
        """
        __init__(self, other: symusic.core.TrackTick) -> None

        Copy constructor
        """
        ...

    def clip(
        self, start: int, end: int, clip_end: bool = False
    ) -> symusic.core.TrackTick:
        """
        Clip notes and controls to a given time range
        """
        ...

    @property
    def controls(self) -> symusic.core.ControlChangeTickList: ...
    @controls.setter
    def controls(self, arg: symusic.core.ControlChangeTickList, /) -> None: ...
    def copy(self) -> symusic.core.TrackTick:
        """
        Deep copy
        """
        ...

    def empty(self) -> bool: ...
    def end(self) -> int: ...
    @property
    def is_drum(self) -> bool: ...
    @is_drum.setter
    def is_drum(self, arg: bool, /) -> None: ...
    @property
    def name(self) -> str: ...
    @name.setter
    def name(self, arg: str, /) -> None: ...
    def note_num(self) -> int: ...
    @property
    def notes(self) -> symusic.core.NoteTickList: ...
    @notes.setter
    def notes(self, arg: symusic.core.NoteTickList, /) -> None: ...
    @property
    def pedals(self) -> symusic.core.PedalTickList: ...
    @pedals.setter
    def pedals(self, arg: symusic.core.PedalTickList, /) -> None: ...
    def pianoroll(
        self,
        modes: list[str],
        pitchRange: tuple[int, int] = (0, 127),
        encodeVelocity: bool = False,
    ) -> numpy.typing.NDArray: ...
    @property
    def pitch_bends(self) -> symusic.core.PitchBendTickList: ...
    @pitch_bends.setter
    def pitch_bends(self, arg: symusic.core.PitchBendTickList, /) -> None: ...
    @property
    def program(self) -> int: ...
    @program.setter
    def program(self, arg: int, /) -> None: ...
    def shift_pitch(self, offset: int, inplace: bool = False) -> object: ...
    def shift_time(self, offset: int, inplace: bool = False) -> object: ...
    def shift_velocity(self, offset: int, inplace: bool = False) -> object: ...
    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    def start(self) -> int: ...
    @property
    def ttype(self) -> symusic.core.Tick: ...

class TrackTickList:
    """
    None
    """

    def __init__(self, arg: Iterable[symusic.core.TrackTick], /) -> None:
        """
        Construct from an iterable object
        """
        ...

    @overload
    def __init__(self) -> None:
        """
        Default constructor
        """
        ...

    @overload
    def __init__(self, arg: symusic.core.TrackTickList) -> None:
        """
        Copy constructor
        """
        ...

    def append(self, arg: symusic.core.TrackTick, /) -> None:
        """
        Append `arg` to the end of the list.
        """
        ...

    def clear(self) -> None:
        """
        Remove all items from list.
        """
        ...

    def copy(self) -> symusic.core.TrackTickList: ...
    def count(self, arg: symusic.core.TrackTick, /) -> int:
        """
        Return number of occurrences of `arg`.
        """
        ...

    def extend(self, arg: symusic.core.TrackTickList, /) -> None:
        """
        Extend `self` by appending elements from `arg`.
        """
        ...

    def filter(self, func: Callable[..., object], inplace: bool = False) -> object: ...
    def insert(self, arg0: int, arg1: symusic.core.TrackTick, /) -> None:
        """
        Insert object `arg1` before index `arg0`.
        """
        ...

    def pop(self, index: int = -1) -> symusic.core.TrackTick:
        """
        Remove and return item at `index` (default last).
        """
        ...

    def remove(self, arg: symusic.core.TrackTick, /) -> None:
        """
        Remove first occurrence of `arg`.
        """
        ...

    def sort(
        self, key: Optional[object] = None, reverse: bool = False, inplace: bool = True
    ) -> object: ...
    @property
    def ttype(self) -> symusic.core.Tick: ...
