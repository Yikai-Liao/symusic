from __future__ import annotations
import numpy
import typing
__all__ = ['ControlChangeQuarter', 'ControlChangeQuarterList', 'ControlChangeSecond', 'ControlChangeSecondList', 'ControlChangeTick', 'ControlChangeTickList', 'KeySignatureQuarter', 'KeySignatureQuarterList', 'KeySignatureSecond', 'KeySignatureSecondList', 'KeySignatureTick', 'KeySignatureTickList', 'NoteQuarter', 'NoteQuarterList', 'NoteSecond', 'NoteSecondList', 'NoteTick', 'NoteTickList', 'PedalQuarter', 'PedalQuarterList', 'PedalSecond', 'PedalSecondList', 'PedalTick', 'PedalTickList', 'PitchBendQuarter', 'PitchBendQuarterList', 'PitchBendSecond', 'PitchBendSecondList', 'PitchBendTick', 'PitchBendTickList', 'Quarter', 'ScoreQuarter', 'ScoreTick', 'Second', 'TempoQuarter', 'TempoQuarterList', 'TempoSecond', 'TempoSecondList', 'TempoTick', 'TempoTickList', 'TextMetaQuarter', 'TextMetaQuarterList', 'TextMetaSecond', 'TextMetaSecondList', 'TextMetaTick', 'TextMetaTickList', 'Tick', 'TimeSignatureQuarter', 'TimeSignatureQuarterList', 'TimeSignatureSecond', 'TimeSignatureSecondList', 'TimeSignatureTick', 'TimeSignatureTickList', 'TrackQuarter', 'TrackQuarterList', 'TrackSecond', 'TrackSecondList', 'TrackTick', 'TrackTickList']
class ControlChangeQuarter:
    __hash__: typing.ClassVar[None] = None
    number: int
    time: float
    value: int
    def __copy__(self) -> ControlChangeQuarter:
        """
        Deep copy
        """
    def __deepcopy__(self) -> ControlChangeQuarter:
        """
        Deep copy
        """
    def __eq__(self, arg0: ControlChangeQuarter) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, other: ControlChangeQuarter) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, time: float, number: int, value: int) -> None:
        ...
    def __ne__(self, arg0: ControlChangeQuarter) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def copy(self) -> ControlChangeQuarter:
        """
        Deep copy
        """
    def shift_time(self, offset: float, inplace: bool = False) -> ControlChangeQuarter:
        """
        Shift the event time by offset
        """
    @property
    def ttype(self) -> Quarter:
        ...
class ControlChangeQuarterList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: ControlChangeQuarter) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: ControlChangeQuarterList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> ControlChangeQuarterList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> ControlChangeQuarter:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: ControlChangeQuarterList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: ControlChangeQuarterList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: ControlChangeQuarter) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: ControlChangeQuarterList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: ControlChangeQuarter) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: ControlChangeQuarter) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: ControlChangeQuarterList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: ControlChangeQuarter) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> ControlChangeQuarter:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> ControlChangeQuarter:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: ControlChangeQuarter) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype(self) -> Quarter:
        ...
class ControlChangeSecond:
    __hash__: typing.ClassVar[None] = None
    number: int
    time: float
    value: int
    def __copy__(self) -> ControlChangeSecond:
        """
        Deep copy
        """
    def __deepcopy__(self) -> ControlChangeSecond:
        """
        Deep copy
        """
    def __eq__(self, arg0: ControlChangeSecond) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, other: ControlChangeSecond) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, time: float, number: int, value: int) -> None:
        ...
    def __ne__(self, arg0: ControlChangeSecond) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def copy(self) -> ControlChangeSecond:
        """
        Deep copy
        """
    def shift_time(self, offset: float, inplace: bool = False) -> ControlChangeSecond:
        """
        Shift the event time by offset
        """
    @property
    def ttype(self) -> Second:
        ...
class ControlChangeSecondList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: ControlChangeSecond) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: ControlChangeSecondList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> ControlChangeSecondList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> ControlChangeSecond:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: ControlChangeSecondList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: ControlChangeSecondList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: ControlChangeSecond) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: ControlChangeSecondList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: ControlChangeSecond) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: ControlChangeSecond) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: ControlChangeSecondList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: ControlChangeSecond) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> ControlChangeSecond:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> ControlChangeSecond:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: ControlChangeSecond) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype(self) -> Second:
        ...
class ControlChangeTick:
    __hash__: typing.ClassVar[None] = None
    number: int
    time: int
    value: int
    def __copy__(self) -> ControlChangeTick:
        """
        Deep copy
        """
    def __deepcopy__(self) -> ControlChangeTick:
        """
        Deep copy
        """
    def __eq__(self, arg0: ControlChangeTick) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, other: ControlChangeTick) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, time: int, number: int, value: int) -> None:
        ...
    def __ne__(self, arg0: ControlChangeTick) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def copy(self) -> ControlChangeTick:
        """
        Deep copy
        """
    def shift_time(self, offset: int, inplace: bool = False) -> ControlChangeTick:
        """
        Shift the event time by offset
        """
    @property
    def ttype(self) -> Tick:
        ...
class ControlChangeTickList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: ControlChangeTick) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: ControlChangeTickList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> ControlChangeTickList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> ControlChangeTick:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: ControlChangeTickList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: ControlChangeTickList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: ControlChangeTick) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: ControlChangeTickList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: ControlChangeTick) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: ControlChangeTick) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: ControlChangeTickList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: ControlChangeTick) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> ControlChangeTick:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> ControlChangeTick:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: ControlChangeTick) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype(self) -> Tick:
        ...
class KeySignatureQuarter:
    __hash__: typing.ClassVar[None] = None
    key: int
    time: float
    tonality: int
    def __copy__(self) -> KeySignatureQuarter:
        """
        Deep copy
        """
    def __deepcopy__(self) -> KeySignatureQuarter:
        """
        Deep copy
        """
    def __eq__(self, arg0: KeySignatureQuarter) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, other: KeySignatureQuarter) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, time: float, key: int, tonality: int) -> None:
        ...
    def __ne__(self, arg0: KeySignatureQuarter) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def copy(self) -> KeySignatureQuarter:
        """
        Deep copy
        """
    def shift_time(self, offset: float, inplace: bool = False) -> KeySignatureQuarter:
        """
        Shift the event time by offset
        """
    @property
    def degree(self) -> int:
        ...
    @property
    def ttype(self) -> Quarter:
        ...
class KeySignatureQuarterList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: KeySignatureQuarter) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: KeySignatureQuarterList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> KeySignatureQuarterList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> KeySignatureQuarter:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: KeySignatureQuarterList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: KeySignatureQuarterList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: KeySignatureQuarter) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: KeySignatureQuarterList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: KeySignatureQuarter) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: KeySignatureQuarter) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: KeySignatureQuarterList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: KeySignatureQuarter) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> KeySignatureQuarter:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> KeySignatureQuarter:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: KeySignatureQuarter) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype(self) -> Quarter:
        ...
class KeySignatureSecond:
    __hash__: typing.ClassVar[None] = None
    key: int
    time: float
    tonality: int
    def __copy__(self) -> KeySignatureSecond:
        """
        Deep copy
        """
    def __deepcopy__(self) -> KeySignatureSecond:
        """
        Deep copy
        """
    def __eq__(self, arg0: KeySignatureSecond) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, other: KeySignatureSecond) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, time: float, key: int, tonality: int) -> None:
        ...
    def __ne__(self, arg0: KeySignatureSecond) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def copy(self) -> KeySignatureSecond:
        """
        Deep copy
        """
    def shift_time(self, offset: float, inplace: bool = False) -> KeySignatureSecond:
        """
        Shift the event time by offset
        """
    @property
    def degree(self) -> int:
        ...
    @property
    def ttype(self) -> Second:
        ...
class KeySignatureSecondList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: KeySignatureSecond) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: KeySignatureSecondList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> KeySignatureSecondList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> KeySignatureSecond:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: KeySignatureSecondList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: KeySignatureSecondList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: KeySignatureSecond) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: KeySignatureSecondList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: KeySignatureSecond) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: KeySignatureSecond) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: KeySignatureSecondList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: KeySignatureSecond) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> KeySignatureSecond:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> KeySignatureSecond:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: KeySignatureSecond) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype(self) -> Second:
        ...
class KeySignatureTick:
    __hash__: typing.ClassVar[None] = None
    key: int
    time: int
    tonality: int
    def __copy__(self) -> KeySignatureTick:
        """
        Deep copy
        """
    def __deepcopy__(self) -> KeySignatureTick:
        """
        Deep copy
        """
    def __eq__(self, arg0: KeySignatureTick) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, other: KeySignatureTick) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, time: int, key: int, tonality: int) -> None:
        ...
    def __ne__(self, arg0: KeySignatureTick) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def copy(self) -> KeySignatureTick:
        """
        Deep copy
        """
    def shift_time(self, offset: int, inplace: bool = False) -> KeySignatureTick:
        """
        Shift the event time by offset
        """
    @property
    def degree(self) -> int:
        ...
    @property
    def ttype(self) -> Tick:
        ...
class KeySignatureTickList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: KeySignatureTick) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: KeySignatureTickList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> KeySignatureTickList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> KeySignatureTick:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: KeySignatureTickList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: KeySignatureTickList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: KeySignatureTick) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: KeySignatureTickList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: KeySignatureTick) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: KeySignatureTick) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: KeySignatureTickList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: KeySignatureTick) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> KeySignatureTick:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> KeySignatureTick:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: KeySignatureTick) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype(self) -> Tick:
        ...
class NoteQuarter:
    __hash__: typing.ClassVar[None] = None
    duration: float
    pitch: int
    start: float
    time: float
    velocity: int
    def __copy__(self) -> NoteQuarter:
        """
        Deep copy
        """
    def __deepcopy__(self) -> NoteQuarter:
        """
        Deep copy
        """
    def __eq__(self, arg0: NoteQuarter) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, other: NoteQuarter) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, start: float, duration: float, pitch: int, velocity: int) -> None:
        ...
    def __ne__(self, arg0: NoteQuarter) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def copy(self) -> NoteQuarter:
        """
        Deep copy
        """
    def empty(self) -> bool:
        """
        duration <= 0 or velocity <= 0
        """
    def end_time(self) -> float:
        ...
    def shift_pitch(self, offset: int, inplace: bool = False) -> typing.Any:
        """
        Shift the pitch by offset
        """
    def shift_time(self, offset: float, inplace: bool = False) -> NoteQuarter:
        """
        Shift the event time by offset
        """
    def shift_velocity(self, offset: int, inplace: bool = False) -> typing.Any:
        """
        Shift the velocity by offset
        """
    @property
    def end(self) -> float:
        ...
    @property
    def ttype(self) -> Quarter:
        ...
class NoteQuarterList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: NoteQuarter) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: NoteQuarterList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> NoteQuarterList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> NoteQuarter:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: NoteQuarterList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: NoteQuarterList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: NoteQuarter) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: NoteQuarterList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: NoteQuarter) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: NoteQuarter) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: NoteQuarterList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: NoteQuarter) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> NoteQuarter:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> NoteQuarter:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: NoteQuarter) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype(self) -> Quarter:
        ...
class NoteSecond:
    __hash__: typing.ClassVar[None] = None
    duration: float
    pitch: int
    start: float
    time: float
    velocity: int
    def __copy__(self) -> NoteSecond:
        """
        Deep copy
        """
    def __deepcopy__(self) -> NoteSecond:
        """
        Deep copy
        """
    def __eq__(self, arg0: NoteSecond) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, other: NoteSecond) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, start: float, duration: float, pitch: int, velocity: int) -> None:
        ...
    def __ne__(self, arg0: NoteSecond) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def copy(self) -> NoteSecond:
        """
        Deep copy
        """
    def empty(self) -> bool:
        """
        duration <= 0 or velocity <= 0
        """
    def end_time(self) -> float:
        ...
    def shift_pitch(self, offset: int, inplace: bool = False) -> typing.Any:
        """
        Shift the pitch by offset
        """
    def shift_time(self, offset: float, inplace: bool = False) -> NoteSecond:
        """
        Shift the event time by offset
        """
    def shift_velocity(self, offset: int, inplace: bool = False) -> typing.Any:
        """
        Shift the velocity by offset
        """
    @property
    def end(self) -> float:
        ...
    @property
    def ttype(self) -> Second:
        ...
class NoteSecondList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: NoteSecond) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: NoteSecondList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> NoteSecondList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> NoteSecond:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: NoteSecondList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: NoteSecondList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: NoteSecond) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: NoteSecondList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: NoteSecond) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: NoteSecond) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: NoteSecondList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: NoteSecond) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> NoteSecond:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> NoteSecond:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: NoteSecond) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype(self) -> Second:
        ...
class NoteTick:
    __hash__: typing.ClassVar[None] = None
    duration: int
    pitch: int
    start: int
    time: int
    velocity: int
    def __copy__(self) -> NoteTick:
        """
        Deep copy
        """
    def __deepcopy__(self) -> NoteTick:
        """
        Deep copy
        """
    def __eq__(self, arg0: NoteTick) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, other: NoteTick) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, start: int, duration: int, pitch: int, velocity: int) -> None:
        ...
    def __ne__(self, arg0: NoteTick) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def copy(self) -> NoteTick:
        """
        Deep copy
        """
    def empty(self) -> bool:
        """
        duration <= 0 or velocity <= 0
        """
    def end_time(self) -> int:
        ...
    def shift_pitch(self, offset: int, inplace: bool = False) -> typing.Any:
        """
        Shift the pitch by offset
        """
    def shift_time(self, offset: int, inplace: bool = False) -> NoteTick:
        """
        Shift the event time by offset
        """
    def shift_velocity(self, offset: int, inplace: bool = False) -> typing.Any:
        """
        Shift the velocity by offset
        """
    @property
    def end(self) -> int:
        ...
    @property
    def ttype(self) -> Tick:
        ...
class NoteTickList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: NoteTick) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: NoteTickList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> NoteTickList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> NoteTick:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: NoteTickList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: NoteTickList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: NoteTick) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: NoteTickList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: NoteTick) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: NoteTick) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: NoteTickList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: NoteTick) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> NoteTick:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> NoteTick:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: NoteTick) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype(self) -> Tick:
        ...
class PedalQuarter:
    __hash__: typing.ClassVar[None] = None
    duration: float
    time: float
    def __copy__(self) -> PedalQuarter:
        """
        Deep copy
        """
    def __deepcopy__(self) -> PedalQuarter:
        """
        Deep copy
        """
    def __eq__(self, arg0: PedalQuarter) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, other: PedalQuarter) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, time: float, duration: float) -> None:
        ...
    def __ne__(self, arg0: PedalQuarter) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def copy(self) -> PedalQuarter:
        """
        Deep copy
        """
    def shift_time(self, offset: float, inplace: bool = False) -> PedalQuarter:
        """
        Shift the event time by offset
        """
    @property
    def ttype(self) -> Quarter:
        ...
class PedalQuarterList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: PedalQuarter) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: PedalQuarterList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> PedalQuarterList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> PedalQuarter:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: PedalQuarterList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: PedalQuarterList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: PedalQuarter) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: PedalQuarterList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: PedalQuarter) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: PedalQuarter) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: PedalQuarterList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: PedalQuarter) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> PedalQuarter:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> PedalQuarter:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: PedalQuarter) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype(self) -> Quarter:
        ...
class PedalSecond:
    __hash__: typing.ClassVar[None] = None
    duration: float
    time: float
    def __copy__(self) -> PedalSecond:
        """
        Deep copy
        """
    def __deepcopy__(self) -> PedalSecond:
        """
        Deep copy
        """
    def __eq__(self, arg0: PedalSecond) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, other: PedalSecond) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, time: float, duration: float) -> None:
        ...
    def __ne__(self, arg0: PedalSecond) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def copy(self) -> PedalSecond:
        """
        Deep copy
        """
    def shift_time(self, offset: float, inplace: bool = False) -> PedalSecond:
        """
        Shift the event time by offset
        """
    @property
    def ttype(self) -> Second:
        ...
class PedalSecondList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: PedalSecond) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: PedalSecondList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> PedalSecondList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> PedalSecond:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: PedalSecondList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: PedalSecondList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: PedalSecond) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: PedalSecondList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: PedalSecond) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: PedalSecond) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: PedalSecondList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: PedalSecond) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> PedalSecond:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> PedalSecond:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: PedalSecond) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype(self) -> Second:
        ...
class PedalTick:
    __hash__: typing.ClassVar[None] = None
    duration: int
    time: int
    def __copy__(self) -> PedalTick:
        """
        Deep copy
        """
    def __deepcopy__(self) -> PedalTick:
        """
        Deep copy
        """
    def __eq__(self, arg0: PedalTick) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, other: PedalTick) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, time: int, duration: int) -> None:
        ...
    def __ne__(self, arg0: PedalTick) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def copy(self) -> PedalTick:
        """
        Deep copy
        """
    def shift_time(self, offset: int, inplace: bool = False) -> PedalTick:
        """
        Shift the event time by offset
        """
    @property
    def ttype(self) -> Tick:
        ...
class PedalTickList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: PedalTick) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: PedalTickList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> PedalTickList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> PedalTick:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: PedalTickList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: PedalTickList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: PedalTick) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: PedalTickList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: PedalTick) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: PedalTick) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: PedalTickList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: PedalTick) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> PedalTick:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> PedalTick:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: PedalTick) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype(self) -> Tick:
        ...
class PitchBendQuarter:
    __hash__: typing.ClassVar[None] = None
    time: float
    value: int
    def __copy__(self) -> PitchBendQuarter:
        """
        Deep copy
        """
    def __deepcopy__(self) -> PitchBendQuarter:
        """
        Deep copy
        """
    def __eq__(self, arg0: PitchBendQuarter) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, other: PitchBendQuarter) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, time: float, value: int) -> None:
        ...
    def __ne__(self, arg0: PitchBendQuarter) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def copy(self) -> PitchBendQuarter:
        """
        Deep copy
        """
    def shift_time(self, offset: float, inplace: bool = False) -> PitchBendQuarter:
        """
        Shift the event time by offset
        """
    @property
    def ttype(self) -> Quarter:
        ...
class PitchBendQuarterList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: PitchBendQuarter) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: PitchBendQuarterList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> PitchBendQuarterList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> PitchBendQuarter:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: PitchBendQuarterList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: PitchBendQuarterList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: PitchBendQuarter) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: PitchBendQuarterList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: PitchBendQuarter) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: PitchBendQuarter) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: PitchBendQuarterList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: PitchBendQuarter) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> PitchBendQuarter:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> PitchBendQuarter:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: PitchBendQuarter) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype(self) -> Quarter:
        ...
class PitchBendSecond:
    __hash__: typing.ClassVar[None] = None
    time: float
    value: int
    def __copy__(self) -> PitchBendSecond:
        """
        Deep copy
        """
    def __deepcopy__(self) -> PitchBendSecond:
        """
        Deep copy
        """
    def __eq__(self, arg0: PitchBendSecond) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, other: PitchBendSecond) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, time: float, value: int) -> None:
        ...
    def __ne__(self, arg0: PitchBendSecond) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def copy(self) -> PitchBendSecond:
        """
        Deep copy
        """
    def shift_time(self, offset: float, inplace: bool = False) -> PitchBendSecond:
        """
        Shift the event time by offset
        """
    @property
    def ttype(self) -> Second:
        ...
class PitchBendSecondList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: PitchBendSecond) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: PitchBendSecondList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> PitchBendSecondList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> PitchBendSecond:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: PitchBendSecondList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: PitchBendSecondList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: PitchBendSecond) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: PitchBendSecondList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: PitchBendSecond) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: PitchBendSecond) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: PitchBendSecondList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: PitchBendSecond) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> PitchBendSecond:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> PitchBendSecond:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: PitchBendSecond) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype(self) -> Second:
        ...
class PitchBendTick:
    __hash__: typing.ClassVar[None] = None
    time: int
    value: int
    def __copy__(self) -> PitchBendTick:
        """
        Deep copy
        """
    def __deepcopy__(self) -> PitchBendTick:
        """
        Deep copy
        """
    def __eq__(self, arg0: PitchBendTick) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, other: PitchBendTick) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, time: int, value: int) -> None:
        ...
    def __ne__(self, arg0: PitchBendTick) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def copy(self) -> PitchBendTick:
        """
        Deep copy
        """
    def shift_time(self, offset: int, inplace: bool = False) -> PitchBendTick:
        """
        Shift the event time by offset
        """
    @property
    def ttype(self) -> Tick:
        ...
class PitchBendTickList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: PitchBendTick) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: PitchBendTickList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> PitchBendTickList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> PitchBendTick:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: PitchBendTickList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: PitchBendTickList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: PitchBendTick) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: PitchBendTickList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: PitchBendTick) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: PitchBendTick) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: PitchBendTickList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: PitchBendTick) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> PitchBendTick:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> PitchBendTick:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: PitchBendTick) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype(self) -> Tick:
        ...
class Quarter:
    __hash__: typing.ClassVar[None] = None
    def __eq__(self, arg0: typing.Any) -> bool:
        ...
    def __init__(self) -> None:
        ...
    def __repr__(self) -> str:
        ...
    def is_time_unit(self) -> bool:
        ...
class ScoreQuarter:
    __hash__: typing.ClassVar[None] = None
    key_signatures: KeySignatureQuarterList
    lyrics: TextMetaQuarterList
    markers: TextMetaQuarterList
    tempos: TempoQuarterList
    ticks_per_quarter: int
    time_signatures: TimeSignatureQuarterList
    tracks: TrackQuarterList
    @staticmethod
    def from_file(*args, **kwargs):
        """
        (arg0: str) -> symusic.core.ScoreQuarter
        """
    def __copy__(self) -> ScoreQuarter:
        """
        Deep copy
        """
    def __deepcopy__(self) -> ScoreQuarter:
        """
        Deep copy
        """
    def __eq__(self, arg0: ScoreQuarter) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, tpq: int) -> None:
        ...
    @typing.overload
    def __init__(self, other: ScoreQuarter) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, path: str) -> None:
        """
        Load from midi file
        """
    @typing.overload
    def __init__(self, other: ScoreTick) -> None:
        """
        Convert Tick to Quarter
        """
    def __ne__(self, arg0: ScoreQuarter) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def clip(self, start: float, end: float, clip_end: bool = False) -> ScoreQuarter:
        """
        Clip events a given time range
        """
    def copy(self) -> ScoreQuarter:
        """
        Deep copy
        """
    @typing.overload
    def dump_midi(self, path: str) -> None:
        """
        Dump to midi file
        """
    @typing.overload
    def dump_midi(self, path: typing.Any) -> None:
        ...
    def empty(self) -> bool:
        ...
    def end(self) -> float:
        ...
    def note_num(self) -> int:
        ...
    def pianoroll(self, quantization: float, mode: str) -> numpy.ndarray:
        ...
    def shift_pitch(self, offset: int, inplace: bool = False) -> typing.Any:
        ...
    def shift_time(self, offset: float, inplace: bool = False) -> typing.Any:
        ...
    def shift_velocity(self, offset: int, inplace: bool = False) -> typing.Any:
        ...
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = False) -> typing.Any:
        ...
    def start(self) -> float:
        ...
    def to(self, ttype: typing.Any) -> typing.Any:
        """
        Convert to another time unit
        """
    @property
    def ttype() -> Quarter:
        ...
class ScoreTick:
    __hash__: typing.ClassVar[None] = None
    key_signatures: KeySignatureTickList
    lyrics: TextMetaTickList
    markers: TextMetaTickList
    tempos: TempoTickList
    ticks_per_quarter: int
    time_signatures: TimeSignatureTickList
    tracks: TrackTickList
    @staticmethod
    def from_file(*args, **kwargs):
        """
        (arg0: str) -> symusic.core.ScoreTick
        """
    def __copy__(self) -> ScoreTick:
        """
        Deep copy
        """
    def __deepcopy__(self) -> ScoreTick:
        """
        Deep copy
        """
    def __eq__(self, arg0: ScoreTick) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, tpq: int) -> None:
        ...
    @typing.overload
    def __init__(self, other: ScoreTick) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, path: str) -> None:
        """
        Load from midi file
        """
    @typing.overload
    def __init__(self, other: ScoreQuarter) -> None:
        """
        Convert Quarter to Tick
        """
    def __ne__(self, arg0: ScoreTick) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def clip(self, start: int, end: int, clip_end: bool = False) -> ScoreTick:
        """
        Clip events a given time range
        """
    def copy(self) -> ScoreTick:
        """
        Deep copy
        """
    @typing.overload
    def dump_midi(self, path: str) -> None:
        """
        Dump to midi file
        """
    @typing.overload
    def dump_midi(self, path: typing.Any) -> None:
        ...
    def empty(self) -> bool:
        ...
    def end(self) -> int:
        ...
    def note_num(self) -> int:
        ...
    def pianoroll(self, quantization: float, mode: str) -> numpy.ndarray:
        ...
    def shift_pitch(self, offset: int, inplace: bool = False) -> typing.Any:
        ...
    def shift_time(self, offset: int, inplace: bool = False) -> typing.Any:
        ...
    def shift_velocity(self, offset: int, inplace: bool = False) -> typing.Any:
        ...
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = False) -> typing.Any:
        ...
    def start(self) -> int:
        ...
    def to(self, ttype: typing.Any) -> typing.Any:
        """
        Convert to another time unit
        """
    @property
    def ttype() -> Tick:
        ...
class Second:
    __hash__: typing.ClassVar[None] = None
    def __eq__(self, arg0: typing.Any) -> bool:
        ...
    def __init__(self) -> None:
        ...
    def __repr__(self) -> str:
        ...
    def is_time_unit(self) -> bool:
        ...
class TempoQuarter:
    __hash__: typing.ClassVar[None] = None
    tempo: float
    time: float
    def __copy__(self) -> TempoQuarter:
        """
        Deep copy
        """
    def __deepcopy__(self) -> TempoQuarter:
        """
        Deep copy
        """
    def __eq__(self, arg0: TempoQuarter) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, other: TempoQuarter) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, time: float, qpm: float) -> None:
        ...
    def __ne__(self, arg0: TempoQuarter) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def copy(self) -> TempoQuarter:
        """
        Deep copy
        """
    def shift_time(self, offset: float, inplace: bool = False) -> TempoQuarter:
        """
        Shift the event time by offset
        """
    @property
    def ttype(self) -> Quarter:
        ...
class TempoQuarterList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: TempoQuarter) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: TempoQuarterList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> TempoQuarterList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> TempoQuarter:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TempoQuarterList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: TempoQuarterList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: TempoQuarter) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: TempoQuarterList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: TempoQuarter) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: TempoQuarter) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: TempoQuarterList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: TempoQuarter) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> TempoQuarter:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> TempoQuarter:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: TempoQuarter) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype(self) -> Quarter:
        ...
class TempoSecond:
    __hash__: typing.ClassVar[None] = None
    tempo: float
    time: float
    def __copy__(self) -> TempoSecond:
        """
        Deep copy
        """
    def __deepcopy__(self) -> TempoSecond:
        """
        Deep copy
        """
    def __eq__(self, arg0: TempoSecond) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, other: TempoSecond) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, time: float, qpm: float) -> None:
        ...
    def __ne__(self, arg0: TempoSecond) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def copy(self) -> TempoSecond:
        """
        Deep copy
        """
    def shift_time(self, offset: float, inplace: bool = False) -> TempoSecond:
        """
        Shift the event time by offset
        """
    @property
    def ttype(self) -> Second:
        ...
class TempoSecondList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: TempoSecond) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: TempoSecondList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> TempoSecondList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> TempoSecond:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TempoSecondList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: TempoSecondList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: TempoSecond) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: TempoSecondList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: TempoSecond) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: TempoSecond) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: TempoSecondList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: TempoSecond) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> TempoSecond:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> TempoSecond:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: TempoSecond) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype(self) -> Second:
        ...
class TempoTick:
    __hash__: typing.ClassVar[None] = None
    tempo: float
    time: int
    def __copy__(self) -> TempoTick:
        """
        Deep copy
        """
    def __deepcopy__(self) -> TempoTick:
        """
        Deep copy
        """
    def __eq__(self, arg0: TempoTick) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, other: TempoTick) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, time: int, qpm: float) -> None:
        ...
    def __ne__(self, arg0: TempoTick) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def copy(self) -> TempoTick:
        """
        Deep copy
        """
    def shift_time(self, offset: int, inplace: bool = False) -> TempoTick:
        """
        Shift the event time by offset
        """
    @property
    def ttype(self) -> Tick:
        ...
class TempoTickList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: TempoTick) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: TempoTickList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> TempoTickList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> TempoTick:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TempoTickList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: TempoTickList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: TempoTick) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: TempoTickList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: TempoTick) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: TempoTick) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: TempoTickList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: TempoTick) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> TempoTick:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> TempoTick:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: TempoTick) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype(self) -> Tick:
        ...
class TextMetaQuarter:
    __hash__: typing.ClassVar[None] = None
    text: str
    time: float
    def __copy__(self) -> TextMetaQuarter:
        """
        Deep copy
        """
    def __deepcopy__(self) -> TextMetaQuarter:
        """
        Deep copy
        """
    def __eq__(self, arg0: TextMetaQuarter) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, other: TextMetaQuarter) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, time: float, text: str) -> None:
        ...
    def __ne__(self, arg0: TextMetaQuarter) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def copy(self) -> TextMetaQuarter:
        """
        Deep copy
        """
    def shift_time(self, offset: float, inplace: bool = False) -> TextMetaQuarter:
        """
        Shift the event time by offset
        """
    @property
    def ttype(self) -> Quarter:
        ...
class TextMetaQuarterList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: TextMetaQuarter) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: TextMetaQuarterList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> TextMetaQuarterList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> TextMetaQuarter:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TextMetaQuarterList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: TextMetaQuarterList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: TextMetaQuarter) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: TextMetaQuarterList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: TextMetaQuarter) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: TextMetaQuarter) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: TextMetaQuarterList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: TextMetaQuarter) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> TextMetaQuarter:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> TextMetaQuarter:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: TextMetaQuarter) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype(self) -> Quarter:
        ...
class TextMetaSecond:
    __hash__: typing.ClassVar[None] = None
    text: str
    time: float
    def __copy__(self) -> TextMetaSecond:
        """
        Deep copy
        """
    def __deepcopy__(self) -> TextMetaSecond:
        """
        Deep copy
        """
    def __eq__(self, arg0: TextMetaSecond) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, other: TextMetaSecond) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, time: float, text: str) -> None:
        ...
    def __ne__(self, arg0: TextMetaSecond) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def copy(self) -> TextMetaSecond:
        """
        Deep copy
        """
    def shift_time(self, offset: float, inplace: bool = False) -> TextMetaSecond:
        """
        Shift the event time by offset
        """
    @property
    def ttype(self) -> Second:
        ...
class TextMetaSecondList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: TextMetaSecond) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: TextMetaSecondList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> TextMetaSecondList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> TextMetaSecond:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TextMetaSecondList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: TextMetaSecondList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: TextMetaSecond) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: TextMetaSecondList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: TextMetaSecond) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: TextMetaSecond) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: TextMetaSecondList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: TextMetaSecond) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> TextMetaSecond:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> TextMetaSecond:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: TextMetaSecond) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype(self) -> Second:
        ...
class TextMetaTick:
    __hash__: typing.ClassVar[None] = None
    text: str
    time: int
    def __copy__(self) -> TextMetaTick:
        """
        Deep copy
        """
    def __deepcopy__(self) -> TextMetaTick:
        """
        Deep copy
        """
    def __eq__(self, arg0: TextMetaTick) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, other: TextMetaTick) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, time: int, text: str) -> None:
        ...
    def __ne__(self, arg0: TextMetaTick) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def copy(self) -> TextMetaTick:
        """
        Deep copy
        """
    def shift_time(self, offset: int, inplace: bool = False) -> TextMetaTick:
        """
        Shift the event time by offset
        """
    @property
    def ttype(self) -> Tick:
        ...
class TextMetaTickList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: TextMetaTick) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: TextMetaTickList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> TextMetaTickList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> TextMetaTick:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TextMetaTickList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: TextMetaTickList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: TextMetaTick) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: TextMetaTickList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: TextMetaTick) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: TextMetaTick) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: TextMetaTickList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: TextMetaTick) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> TextMetaTick:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> TextMetaTick:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: TextMetaTick) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype(self) -> Tick:
        ...
class Tick:
    __hash__: typing.ClassVar[None] = None
    def __eq__(self, arg0: typing.Any) -> bool:
        ...
    def __init__(self) -> None:
        ...
    def __repr__(self) -> str:
        ...
    def is_time_unit(self) -> bool:
        ...
class TimeSignatureQuarter:
    __hash__: typing.ClassVar[None] = None
    denominator: int
    numerator: int
    time: float
    def __copy__(self) -> TimeSignatureQuarter:
        """
        Deep copy
        """
    def __deepcopy__(self) -> TimeSignatureQuarter:
        """
        Deep copy
        """
    def __eq__(self, arg0: TimeSignatureQuarter) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, other: TimeSignatureQuarter) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: float, arg1: int, arg2: int) -> None:
        ...
    def __ne__(self, arg0: TimeSignatureQuarter) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def copy(self) -> TimeSignatureQuarter:
        """
        Deep copy
        """
    def shift_time(self, offset: float, inplace: bool = False) -> TimeSignatureQuarter:
        """
        Shift the event time by offset
        """
    @property
    def ttype(self) -> Quarter:
        ...
class TimeSignatureQuarterList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: TimeSignatureQuarter) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: TimeSignatureQuarterList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> TimeSignatureQuarterList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> TimeSignatureQuarter:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TimeSignatureQuarterList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: TimeSignatureQuarterList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: TimeSignatureQuarter) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: TimeSignatureQuarterList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: TimeSignatureQuarter) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: TimeSignatureQuarter) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: TimeSignatureQuarterList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: TimeSignatureQuarter) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> TimeSignatureQuarter:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> TimeSignatureQuarter:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: TimeSignatureQuarter) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype(self) -> Quarter:
        ...
class TimeSignatureSecond:
    __hash__: typing.ClassVar[None] = None
    denominator: int
    numerator: int
    time: float
    def __copy__(self) -> TimeSignatureSecond:
        """
        Deep copy
        """
    def __deepcopy__(self) -> TimeSignatureSecond:
        """
        Deep copy
        """
    def __eq__(self, arg0: TimeSignatureSecond) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, other: TimeSignatureSecond) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: float, arg1: int, arg2: int) -> None:
        ...
    def __ne__(self, arg0: TimeSignatureSecond) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def copy(self) -> TimeSignatureSecond:
        """
        Deep copy
        """
    def shift_time(self, offset: float, inplace: bool = False) -> TimeSignatureSecond:
        """
        Shift the event time by offset
        """
    @property
    def ttype(self) -> Second:
        ...
class TimeSignatureSecondList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: TimeSignatureSecond) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: TimeSignatureSecondList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> TimeSignatureSecondList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> TimeSignatureSecond:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TimeSignatureSecondList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: TimeSignatureSecondList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: TimeSignatureSecond) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: TimeSignatureSecondList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: TimeSignatureSecond) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: TimeSignatureSecond) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: TimeSignatureSecondList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: TimeSignatureSecond) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> TimeSignatureSecond:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> TimeSignatureSecond:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: TimeSignatureSecond) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype(self) -> Second:
        ...
class TimeSignatureTick:
    __hash__: typing.ClassVar[None] = None
    denominator: int
    numerator: int
    time: int
    def __copy__(self) -> TimeSignatureTick:
        """
        Deep copy
        """
    def __deepcopy__(self) -> TimeSignatureTick:
        """
        Deep copy
        """
    def __eq__(self, arg0: TimeSignatureTick) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self, other: TimeSignatureTick) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: int, arg1: int, arg2: int) -> None:
        ...
    def __ne__(self, arg0: TimeSignatureTick) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def copy(self) -> TimeSignatureTick:
        """
        Deep copy
        """
    def shift_time(self, offset: int, inplace: bool = False) -> TimeSignatureTick:
        """
        Shift the event time by offset
        """
    @property
    def ttype(self) -> Tick:
        ...
class TimeSignatureTickList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: TimeSignatureTick) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: TimeSignatureTickList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> TimeSignatureTickList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> TimeSignatureTick:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TimeSignatureTickList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: TimeSignatureTickList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: TimeSignatureTick) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: TimeSignatureTickList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: TimeSignatureTick) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: TimeSignatureTick) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: TimeSignatureTickList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: TimeSignatureTick) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> TimeSignatureTick:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> TimeSignatureTick:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: TimeSignatureTick) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype(self) -> Tick:
        ...
class TrackQuarter:
    __hash__: typing.ClassVar[None] = None
    controls: ControlChangeQuarterList
    is_drum: bool
    name: str
    notes: NoteQuarterList
    pedals: PedalQuarterList
    pitch_bends: PitchBendQuarterList
    program: int
    def __copy__(self) -> TrackQuarter:
        """
        Deep copy
        """
    def __deepcopy__(self) -> TrackQuarter:
        """
        Deep copy
        """
    def __eq__(self, arg0: TrackQuarter) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, other: TrackQuarter) -> None:
        """
        Copy constructor
        """
    def __ne__(self, arg0: TrackQuarter) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def clip(self, start: float, end: float, clip_end: bool = False) -> TrackQuarter:
        """
        Clip notes and controls to a given time range
        """
    def copy(self) -> TrackQuarter:
        """
        Deep copy
        """
    def empty(self) -> bool:
        ...
    def end(self) -> float:
        ...
    def note_num(self) -> int:
        ...
    def pianoroll(self, quantization: float, mode: str) -> numpy.ndarray:
        ...
    def shift_pitch(self, offset: int, inplace: bool = False) -> typing.Any:
        ...
    def shift_time(self, offset: float, inplace: bool = False) -> typing.Any:
        ...
    def shift_velocity(self, offset: int, inplace: bool = False) -> typing.Any:
        ...
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = False) -> typing.Any:
        ...
    def start(self) -> float:
        ...
    @property
    def ttype() -> Quarter:
        ...
class TrackQuarterList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: TrackQuarter) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: TrackQuarterList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> TrackQuarterList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> TrackQuarter:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TrackQuarterList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: TrackQuarterList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: TrackQuarter) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: TrackQuarterList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: TrackQuarter) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: TrackQuarter) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: TrackQuarterList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: TrackQuarter) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> TrackQuarter:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> TrackQuarter:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: TrackQuarter) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype() -> Quarter:
        ...
class TrackSecond:
    __hash__: typing.ClassVar[None] = None
    controls: ControlChangeSecondList
    is_drum: bool
    name: str
    notes: NoteSecondList
    pedals: PedalSecondList
    pitch_bends: PitchBendSecondList
    program: int
    def __copy__(self) -> TrackSecond:
        """
        Deep copy
        """
    def __deepcopy__(self) -> TrackSecond:
        """
        Deep copy
        """
    def __eq__(self, arg0: TrackSecond) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, other: TrackSecond) -> None:
        """
        Copy constructor
        """
    def __ne__(self, arg0: TrackSecond) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def clip(self, start: float, end: float, clip_end: bool = False) -> TrackSecond:
        """
        Clip notes and controls to a given time range
        """
    def copy(self) -> TrackSecond:
        """
        Deep copy
        """
    def empty(self) -> bool:
        ...
    def end(self) -> float:
        ...
    def note_num(self) -> int:
        ...
    def pianoroll(self, quantization: float, mode: str) -> numpy.ndarray:
        ...
    def shift_pitch(self, offset: int, inplace: bool = False) -> typing.Any:
        ...
    def shift_time(self, offset: float, inplace: bool = False) -> typing.Any:
        ...
    def shift_velocity(self, offset: int, inplace: bool = False) -> typing.Any:
        ...
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = False) -> typing.Any:
        ...
    def start(self) -> float:
        ...
    @property
    def ttype() -> Second:
        ...
class TrackSecondList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: TrackSecond) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: TrackSecondList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> TrackSecondList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> TrackSecond:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TrackSecondList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: TrackSecondList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: TrackSecond) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: TrackSecondList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: TrackSecond) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: TrackSecond) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: TrackSecondList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: TrackSecond) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> TrackSecond:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> TrackSecond:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: TrackSecond) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype() -> Second:
        ...
class TrackTick:
    __hash__: typing.ClassVar[None] = None
    controls: ControlChangeTickList
    is_drum: bool
    name: str
    notes: NoteTickList
    pedals: PedalTickList
    pitch_bends: PitchBendTickList
    program: int
    def __copy__(self) -> TrackTick:
        """
        Deep copy
        """
    def __deepcopy__(self) -> TrackTick:
        """
        Deep copy
        """
    def __eq__(self, arg0: TrackTick) -> bool:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, other: TrackTick) -> None:
        """
        Copy constructor
        """
    def __ne__(self, arg0: TrackTick) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def clip(self, start: int, end: int, clip_end: bool = False) -> TrackTick:
        """
        Clip notes and controls to a given time range
        """
    def copy(self) -> TrackTick:
        """
        Deep copy
        """
    def empty(self) -> bool:
        ...
    def end(self) -> int:
        ...
    def note_num(self) -> int:
        ...
    def pianoroll(self, quantization: float, mode: str) -> numpy.ndarray:
        ...
    def shift_pitch(self, offset: int, inplace: bool = False) -> typing.Any:
        ...
    def shift_time(self, offset: int, inplace: bool = False) -> typing.Any:
        ...
    def shift_velocity(self, offset: int, inplace: bool = False) -> typing.Any:
        ...
    def sort(self, key: typing.Any = None, reverse: bool = False, inplace: bool = False) -> typing.Any:
        ...
    def start(self) -> int:
        ...
    @property
    def ttype() -> Tick:
        ...
class TrackTickList:
    __hash__: typing.ClassVar[None] = None
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
        """
    def __contains__(self, x: TrackTick) -> bool:
        """
        Return true the container contains ``x``
        """
    @typing.overload
    def __delitem__(self, arg0: int) -> None:
        """
        Delete the list elements at index ``i``
        """
    @typing.overload
    def __delitem__(self, arg0: slice) -> None:
        """
        Delete list elements using a slice object
        """
    def __eq__(self, arg0: TrackTickList) -> bool:
        ...
    @typing.overload
    def __getitem__(self, s: slice) -> TrackTickList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> TrackTick:
        ...
    def __getstate__(self) -> bytes:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TrackTickList) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: typing.Iterable) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __ne__(self, arg0: TrackTickList) -> bool:
        ...
    @typing.overload
    def __repr__(self) -> str:
        """
        Return the canonical string representation of this list.
        """
    @typing.overload
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: TrackTick) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: TrackTickList) -> None:
        """
        Assign list elements using a slice object
        """
    def __setstate__(self, arg0: bytes) -> None:
        ...
    def append(self, x: TrackTick) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    def count(self, x: TrackTick) -> int:
        """
        Return the number of times ``x`` appears in the list
        """
    @typing.overload
    def extend(self, L: TrackTickList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: TrackTick) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> TrackTick:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> TrackTick:
        """
        Remove and return the item at index ``i``
        """
    def remove(self, x: TrackTick) -> None:
        """
        Remove the first item from the list whose value is x. It is an error if there is no such item.
        """
    def sort(self, key: typing.Any, reverse: bool = False, inplace: bool = True) -> typing.Any:
        ...
    @property
    def ttype() -> Tick:
        ...
