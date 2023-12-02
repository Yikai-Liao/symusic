from __future__ import annotations
import typing
__all__ = ['ControlChangeQuarter', 'ControlChangeQuarterList', 'ControlChangeSecond', 'ControlChangeSecondList', 'ControlChangeTick', 'ControlChangeTickList', 'KeySignatureQuarter', 'KeySignatureQuarterList', 'KeySignatureSecond', 'KeySignatureSecondList', 'KeySignatureTick', 'KeySignatureTickList', 'NoteQuarter', 'NoteQuarterList', 'NoteSecond', 'NoteSecondList', 'NoteTick', 'NoteTickList', 'PitchBendQuarter', 'PitchBendQuarterList', 'PitchBendSecond', 'PitchBendSecondList', 'PitchBendTick', 'PitchBendTickList', 'ScoreQuarter', 'ScoreTick', 'TempoQuarter', 'TempoQuarterList', 'TempoSecond', 'TempoSecondList', 'TempoTick', 'TempoTickList', 'TextMetaQuarter', 'TextMetaQuarterList', 'TextMetaSecond', 'TextMetaSecondList', 'TextMetaTick', 'TextMetaTickList', 'TimeSignatureQuarter', 'TimeSignatureQuarterList', 'TimeSignatureSecond', 'TimeSignatureSecondList', 'TimeSignatureTick', 'TimeSignatureTickList', 'TrackQuarter', 'TrackQuarterList', 'TrackSecond', 'TrackSecondList', 'TrackTick', 'TrackTickList']
class ControlChangeQuarter:
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
    @typing.overload
    def __init__(self, arg0: float, arg1: int, arg2: int) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: ControlChangeQuarter) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> ControlChangeQuarter:
        """
        Deep copy
        """
    def shift_time(self, arg0: float) -> ControlChangeQuarter:
        ...
    def shift_time_inplace(self, arg0: float) -> ControlChangeQuarter:
        ...
class ControlChangeQuarterList:
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
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
    @typing.overload
    def __getitem__(self, s: slice) -> ControlChangeQuarterList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> ControlChangeQuarter:
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
    def __iter__(self) -> typing.Iterator[ControlChangeQuarter]:
        ...
    def __len__(self) -> int:
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
    def append(self, x: ControlChangeQuarter) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
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
    def sort(self) -> ControlChangeQuarterList:
        ...
    def sort_inplace(self) -> ControlChangeQuarterList:
        ...
class ControlChangeSecond:
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
    @typing.overload
    def __init__(self, arg0: float, arg1: int, arg2: int) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: ControlChangeSecond) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> ControlChangeSecond:
        """
        Deep copy
        """
    def shift_time(self, arg0: float) -> ControlChangeSecond:
        ...
    def shift_time_inplace(self, arg0: float) -> ControlChangeSecond:
        ...
class ControlChangeSecondList:
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
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
    @typing.overload
    def __getitem__(self, s: slice) -> ControlChangeSecondList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> ControlChangeSecond:
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
    def __iter__(self) -> typing.Iterator[ControlChangeSecond]:
        ...
    def __len__(self) -> int:
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
    def append(self, x: ControlChangeSecond) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
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
    def sort(self) -> ControlChangeSecondList:
        ...
    def sort_inplace(self) -> ControlChangeSecondList:
        ...
class ControlChangeTick:
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
    @typing.overload
    def __init__(self, arg0: int, arg1: int, arg2: int) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: ControlChangeTick) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> ControlChangeTick:
        """
        Deep copy
        """
    def shift_time(self, arg0: int) -> ControlChangeTick:
        ...
    def shift_time_inplace(self, arg0: int) -> ControlChangeTick:
        ...
class ControlChangeTickList:
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
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
    @typing.overload
    def __getitem__(self, s: slice) -> ControlChangeTickList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> ControlChangeTick:
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
    def __iter__(self) -> typing.Iterator[ControlChangeTick]:
        ...
    def __len__(self) -> int:
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
    def append(self, x: ControlChangeTick) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
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
    def sort(self) -> ControlChangeTickList:
        ...
    def sort_inplace(self) -> ControlChangeTickList:
        ...
class KeySignatureQuarter:
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
    @typing.overload
    def __init__(self, arg0: float, arg1: int, arg2: int) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: KeySignatureQuarter) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> KeySignatureQuarter:
        """
        Deep copy
        """
    def shift_time(self, arg0: float) -> KeySignatureQuarter:
        ...
    def shift_time_inplace(self, arg0: float) -> KeySignatureQuarter:
        ...
class KeySignatureQuarterList:
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
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
    @typing.overload
    def __getitem__(self, s: slice) -> KeySignatureQuarterList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> KeySignatureQuarter:
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
    def __iter__(self) -> typing.Iterator[KeySignatureQuarter]:
        ...
    def __len__(self) -> int:
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
    def append(self, x: KeySignatureQuarter) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
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
    def sort(self) -> KeySignatureQuarterList:
        ...
    def sort_inplace(self) -> KeySignatureQuarterList:
        ...
class KeySignatureSecond:
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
    @typing.overload
    def __init__(self, arg0: float, arg1: int, arg2: int) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: KeySignatureSecond) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> KeySignatureSecond:
        """
        Deep copy
        """
    def shift_time(self, arg0: float) -> KeySignatureSecond:
        ...
    def shift_time_inplace(self, arg0: float) -> KeySignatureSecond:
        ...
class KeySignatureSecondList:
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
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
    @typing.overload
    def __getitem__(self, s: slice) -> KeySignatureSecondList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> KeySignatureSecond:
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
    def __iter__(self) -> typing.Iterator[KeySignatureSecond]:
        ...
    def __len__(self) -> int:
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
    def append(self, x: KeySignatureSecond) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
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
    def sort(self) -> KeySignatureSecondList:
        ...
    def sort_inplace(self) -> KeySignatureSecondList:
        ...
class KeySignatureTick:
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
    @typing.overload
    def __init__(self, arg0: int, arg1: int, arg2: int) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: KeySignatureTick) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> KeySignatureTick:
        """
        Deep copy
        """
    def shift_time(self, arg0: int) -> KeySignatureTick:
        ...
    def shift_time_inplace(self, arg0: int) -> KeySignatureTick:
        ...
class KeySignatureTickList:
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
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
    @typing.overload
    def __getitem__(self, s: slice) -> KeySignatureTickList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> KeySignatureTick:
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
    def __iter__(self) -> typing.Iterator[KeySignatureTick]:
        ...
    def __len__(self) -> int:
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
    def append(self, x: KeySignatureTick) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
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
    def sort(self) -> KeySignatureTickList:
        ...
    def sort_inplace(self) -> KeySignatureTickList:
        ...
class NoteQuarter:
    duration: float
    pitch: int
    start: float
    time: float
    velocity: int
    def __copy__(self) -> NoteQuarter:
        """
        Deep copy the note
        """
    def __deepcopy__(self) -> NoteQuarter:
        """
        Deep copy the note
        """
    @typing.overload
    def __init__(self, arg0: float, arg1: float, arg2: int, arg3: int) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: NoteQuarter) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> NoteQuarter:
        """
        Deep copy the note
        """
    def empty(self) -> bool:
        """
        duration <= 0 or velocity <= 0
        """
    def end_time(self) -> float:
        ...
    def shift_pitch(self, arg0: float) -> NoteQuarter:
        ...
    def shift_pitch_inplace(self, arg0: float) -> NoteQuarter:
        ...
    def shift_time(self, arg0: float) -> NoteQuarter:
        ...
    def shift_time_inplace(self, arg0: float) -> NoteQuarter:
        ...
    def shift_velocity(self, arg0: float) -> NoteQuarter:
        ...
    def shift_velocity_inplace(self, arg0: float) -> NoteQuarter:
        ...
    @property
    def end(self) -> float:
        ...
class NoteQuarterList:
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
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
    @typing.overload
    def __getitem__(self, s: slice) -> NoteQuarterList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> NoteQuarter:
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
    def __iter__(self) -> typing.Iterator[NoteQuarter]:
        ...
    def __len__(self) -> int:
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
    def append(self, x: NoteQuarter) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
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
    def sort(self) -> NoteQuarterList:
        ...
    def sort_inplace(self) -> NoteQuarterList:
        ...
class NoteSecond:
    duration: float
    pitch: int
    start: float
    time: float
    velocity: int
    def __copy__(self) -> NoteSecond:
        """
        Deep copy the note
        """
    def __deepcopy__(self) -> NoteSecond:
        """
        Deep copy the note
        """
    @typing.overload
    def __init__(self, arg0: float, arg1: float, arg2: int, arg3: int) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: NoteSecond) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> NoteSecond:
        """
        Deep copy the note
        """
    def empty(self) -> bool:
        """
        duration <= 0 or velocity <= 0
        """
    def end_time(self) -> float:
        ...
    def shift_pitch(self, arg0: float) -> NoteSecond:
        ...
    def shift_pitch_inplace(self, arg0: float) -> NoteSecond:
        ...
    def shift_time(self, arg0: float) -> NoteSecond:
        ...
    def shift_time_inplace(self, arg0: float) -> NoteSecond:
        ...
    def shift_velocity(self, arg0: float) -> NoteSecond:
        ...
    def shift_velocity_inplace(self, arg0: float) -> NoteSecond:
        ...
    @property
    def end(self) -> float:
        ...
class NoteSecondList:
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
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
    @typing.overload
    def __getitem__(self, s: slice) -> NoteSecondList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> NoteSecond:
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
    def __iter__(self) -> typing.Iterator[NoteSecond]:
        ...
    def __len__(self) -> int:
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
    def append(self, x: NoteSecond) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
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
    def sort(self) -> NoteSecondList:
        ...
    def sort_inplace(self) -> NoteSecondList:
        ...
class NoteTick:
    duration: int
    pitch: int
    start: int
    time: int
    velocity: int
    def __copy__(self) -> NoteTick:
        """
        Deep copy the note
        """
    def __deepcopy__(self) -> NoteTick:
        """
        Deep copy the note
        """
    @typing.overload
    def __init__(self, arg0: int, arg1: int, arg2: int, arg3: int) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: NoteTick) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> NoteTick:
        """
        Deep copy the note
        """
    def empty(self) -> bool:
        """
        duration <= 0 or velocity <= 0
        """
    def end_time(self) -> int:
        ...
    def shift_pitch(self, arg0: int) -> NoteTick:
        ...
    def shift_pitch_inplace(self, arg0: int) -> NoteTick:
        ...
    def shift_time(self, arg0: int) -> NoteTick:
        ...
    def shift_time_inplace(self, arg0: int) -> NoteTick:
        ...
    def shift_velocity(self, arg0: int) -> NoteTick:
        ...
    def shift_velocity_inplace(self, arg0: int) -> NoteTick:
        ...
    @property
    def end(self) -> int:
        ...
class NoteTickList:
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
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
    @typing.overload
    def __getitem__(self, s: slice) -> NoteTickList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> NoteTick:
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
    def __iter__(self) -> typing.Iterator[NoteTick]:
        ...
    def __len__(self) -> int:
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
    def append(self, x: NoteTick) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
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
    def sort(self) -> NoteTickList:
        ...
    def sort_inplace(self) -> NoteTickList:
        ...
class PitchBendQuarter:
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
    @typing.overload
    def __init__(self, arg0: float, arg1: int) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: PitchBendQuarter) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> PitchBendQuarter:
        """
        Deep copy
        """
    def shift_time(self, arg0: float) -> PitchBendQuarter:
        ...
    def shift_time_inplace(self, arg0: float) -> PitchBendQuarter:
        ...
class PitchBendQuarterList:
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
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
    @typing.overload
    def __getitem__(self, s: slice) -> PitchBendQuarterList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> PitchBendQuarter:
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
    def __iter__(self) -> typing.Iterator[PitchBendQuarter]:
        ...
    def __len__(self) -> int:
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
    def append(self, x: PitchBendQuarter) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
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
    def sort(self) -> PitchBendQuarterList:
        ...
    def sort_inplace(self) -> PitchBendQuarterList:
        ...
class PitchBendSecond:
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
    @typing.overload
    def __init__(self, arg0: float, arg1: int) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: PitchBendSecond) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> PitchBendSecond:
        """
        Deep copy
        """
    def shift_time(self, arg0: float) -> PitchBendSecond:
        ...
    def shift_time_inplace(self, arg0: float) -> PitchBendSecond:
        ...
class PitchBendSecondList:
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
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
    @typing.overload
    def __getitem__(self, s: slice) -> PitchBendSecondList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> PitchBendSecond:
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
    def __iter__(self) -> typing.Iterator[PitchBendSecond]:
        ...
    def __len__(self) -> int:
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
    def append(self, x: PitchBendSecond) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
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
    def sort(self) -> PitchBendSecondList:
        ...
    def sort_inplace(self) -> PitchBendSecondList:
        ...
class PitchBendTick:
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
    @typing.overload
    def __init__(self, arg0: int, arg1: int) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: PitchBendTick) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> PitchBendTick:
        """
        Deep copy
        """
    def shift_time(self, arg0: int) -> PitchBendTick:
        ...
    def shift_time_inplace(self, arg0: int) -> PitchBendTick:
        ...
class PitchBendTickList:
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
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
    @typing.overload
    def __getitem__(self, s: slice) -> PitchBendTickList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> PitchBendTick:
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
    def __iter__(self) -> typing.Iterator[PitchBendTick]:
        ...
    def __len__(self) -> int:
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
    def append(self, x: PitchBendTick) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
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
    def sort(self) -> PitchBendTickList:
        ...
    def sort_inplace(self) -> PitchBendTickList:
        ...
class ScoreQuarter:
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
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: ScoreQuarter) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: str) -> None:
        ...
    def __repr__(self) -> str:
        ...
    def clip(self, start: float, end: float, clip_end: bool = False) -> ScoreQuarter:
        """
        Clip events a given time range
        """
    def copy(self) -> ScoreQuarter:
        """
        Deep copy
        """
    def empty(self) -> bool:
        ...
    def end(self) -> float:
        ...
    def note_num(self) -> int:
        ...
    def shift_pitch(self, arg0: int) -> ScoreQuarter:
        ...
    def shift_pitch_inplace(self, arg0: int) -> ScoreQuarter:
        ...
    def shift_time(self, arg0: float) -> ScoreQuarter:
        ...
    def shift_time_inplace(self, arg0: float) -> ScoreQuarter:
        ...
    def shift_velocity(self, arg0: int) -> ScoreQuarter:
        ...
    def shift_velocity_inplace(self, arg0: int) -> ScoreQuarter:
        ...
    def sort(self) -> ScoreQuarter:
        ...
    def sort_inplace(self) -> ScoreQuarter:
        ...
    def start(self) -> float:
        ...
class ScoreTick:
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
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: ScoreTick) -> None:
        """
        Copy constructor
        """
    @typing.overload
    def __init__(self, arg0: str) -> None:
        ...
    def __repr__(self) -> str:
        ...
    def clip(self, start: int, end: int, clip_end: bool = False) -> ScoreTick:
        """
        Clip events a given time range
        """
    def copy(self) -> ScoreTick:
        """
        Deep copy
        """
    def empty(self) -> bool:
        ...
    def end(self) -> int:
        ...
    def note_num(self) -> int:
        ...
    def shift_pitch(self, arg0: int) -> ScoreTick:
        ...
    def shift_pitch_inplace(self, arg0: int) -> ScoreTick:
        ...
    def shift_time(self, arg0: int) -> ScoreTick:
        ...
    def shift_time_inplace(self, arg0: int) -> ScoreTick:
        ...
    def shift_velocity(self, arg0: int) -> ScoreTick:
        ...
    def shift_velocity_inplace(self, arg0: int) -> ScoreTick:
        ...
    def sort(self) -> ScoreTick:
        ...
    def sort_inplace(self) -> ScoreTick:
        ...
    def start(self) -> int:
        ...
class TempoQuarter:
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
    @typing.overload
    def __init__(self, arg0: float, arg1: float) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TempoQuarter) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> TempoQuarter:
        """
        Deep copy
        """
    def shift_time(self, arg0: float) -> TempoQuarter:
        ...
    def shift_time_inplace(self, arg0: float) -> TempoQuarter:
        ...
class TempoQuarterList:
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
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
    @typing.overload
    def __getitem__(self, s: slice) -> TempoQuarterList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> TempoQuarter:
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
    def __iter__(self) -> typing.Iterator[TempoQuarter]:
        ...
    def __len__(self) -> int:
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
    def append(self, x: TempoQuarter) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
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
    def sort(self) -> TempoQuarterList:
        ...
    def sort_inplace(self) -> TempoQuarterList:
        ...
class TempoSecond:
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
    @typing.overload
    def __init__(self, arg0: float, arg1: float) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TempoSecond) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> TempoSecond:
        """
        Deep copy
        """
    def shift_time(self, arg0: float) -> TempoSecond:
        ...
    def shift_time_inplace(self, arg0: float) -> TempoSecond:
        ...
class TempoSecondList:
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
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
    @typing.overload
    def __getitem__(self, s: slice) -> TempoSecondList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> TempoSecond:
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
    def __iter__(self) -> typing.Iterator[TempoSecond]:
        ...
    def __len__(self) -> int:
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
    def append(self, x: TempoSecond) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
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
    def sort(self) -> TempoSecondList:
        ...
    def sort_inplace(self) -> TempoSecondList:
        ...
class TempoTick:
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
    @typing.overload
    def __init__(self, arg0: int, arg1: float) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TempoTick) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> TempoTick:
        """
        Deep copy
        """
    def shift_time(self, arg0: int) -> TempoTick:
        ...
    def shift_time_inplace(self, arg0: int) -> TempoTick:
        ...
class TempoTickList:
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
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
    @typing.overload
    def __getitem__(self, s: slice) -> TempoTickList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> TempoTick:
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
    def __iter__(self) -> typing.Iterator[TempoTick]:
        ...
    def __len__(self) -> int:
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
    def append(self, x: TempoTick) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
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
    def sort(self) -> TempoTickList:
        ...
    def sort_inplace(self) -> TempoTickList:
        ...
class TextMetaQuarter:
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
    @typing.overload
    def __init__(self, arg0: float, arg1: str) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TextMetaQuarter) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> TextMetaQuarter:
        """
        Deep copy
        """
    def shift_time(self, arg0: float) -> TextMetaQuarter:
        ...
    def shift_time_inplace(self, arg0: float) -> TextMetaQuarter:
        ...
class TextMetaQuarterList:
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
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
    @typing.overload
    def __getitem__(self, s: slice) -> TextMetaQuarterList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> TextMetaQuarter:
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
    def __iter__(self) -> typing.Iterator[TextMetaQuarter]:
        ...
    def __len__(self) -> int:
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
    def append(self, x: TextMetaQuarter) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
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
    def sort(self) -> TextMetaQuarterList:
        ...
    def sort_inplace(self) -> TextMetaQuarterList:
        ...
class TextMetaSecond:
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
    @typing.overload
    def __init__(self, arg0: float, arg1: str) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TextMetaSecond) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> TextMetaSecond:
        """
        Deep copy
        """
    def shift_time(self, arg0: float) -> TextMetaSecond:
        ...
    def shift_time_inplace(self, arg0: float) -> TextMetaSecond:
        ...
class TextMetaSecondList:
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
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
    @typing.overload
    def __getitem__(self, s: slice) -> TextMetaSecondList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> TextMetaSecond:
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
    def __iter__(self) -> typing.Iterator[TextMetaSecond]:
        ...
    def __len__(self) -> int:
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
    def append(self, x: TextMetaSecond) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
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
    def sort(self) -> TextMetaSecondList:
        ...
    def sort_inplace(self) -> TextMetaSecondList:
        ...
class TextMetaTick:
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
    @typing.overload
    def __init__(self, arg0: int, arg1: str) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TextMetaTick) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> TextMetaTick:
        """
        Deep copy
        """
    def shift_time(self, arg0: int) -> TextMetaTick:
        ...
    def shift_time_inplace(self, arg0: int) -> TextMetaTick:
        ...
class TextMetaTickList:
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
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
    @typing.overload
    def __getitem__(self, s: slice) -> TextMetaTickList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> TextMetaTick:
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
    def __iter__(self) -> typing.Iterator[TextMetaTick]:
        ...
    def __len__(self) -> int:
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
    def append(self, x: TextMetaTick) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
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
    def sort(self) -> TextMetaTickList:
        ...
    def sort_inplace(self) -> TextMetaTickList:
        ...
class TimeSignatureQuarter:
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
    @typing.overload
    def __init__(self, arg0: float, arg1: int, arg2: int) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TimeSignatureQuarter) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> TimeSignatureQuarter:
        """
        Deep copy
        """
    def shift_time(self, arg0: float) -> TimeSignatureQuarter:
        ...
    def shift_time_inplace(self, arg0: float) -> TimeSignatureQuarter:
        ...
class TimeSignatureQuarterList:
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
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
    @typing.overload
    def __getitem__(self, s: slice) -> TimeSignatureQuarterList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> TimeSignatureQuarter:
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
    def __iter__(self) -> typing.Iterator[TimeSignatureQuarter]:
        ...
    def __len__(self) -> int:
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
    def append(self, x: TimeSignatureQuarter) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
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
    def sort(self) -> TimeSignatureQuarterList:
        ...
    def sort_inplace(self) -> TimeSignatureQuarterList:
        ...
class TimeSignatureSecond:
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
    @typing.overload
    def __init__(self, arg0: float, arg1: int, arg2: int) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TimeSignatureSecond) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> TimeSignatureSecond:
        """
        Deep copy
        """
    def shift_time(self, arg0: float) -> TimeSignatureSecond:
        ...
    def shift_time_inplace(self, arg0: float) -> TimeSignatureSecond:
        ...
class TimeSignatureSecondList:
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
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
    @typing.overload
    def __getitem__(self, s: slice) -> TimeSignatureSecondList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> TimeSignatureSecond:
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
    def __iter__(self) -> typing.Iterator[TimeSignatureSecond]:
        ...
    def __len__(self) -> int:
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
    def append(self, x: TimeSignatureSecond) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
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
    def sort(self) -> TimeSignatureSecondList:
        ...
    def sort_inplace(self) -> TimeSignatureSecondList:
        ...
class TimeSignatureTick:
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
    @typing.overload
    def __init__(self, arg0: int, arg1: int, arg2: int) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TimeSignatureTick) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> TimeSignatureTick:
        """
        Deep copy
        """
    def shift_time(self, arg0: int) -> TimeSignatureTick:
        ...
    def shift_time_inplace(self, arg0: int) -> TimeSignatureTick:
        ...
class TimeSignatureTickList:
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
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
    @typing.overload
    def __getitem__(self, s: slice) -> TimeSignatureTickList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> TimeSignatureTick:
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
    def __iter__(self) -> typing.Iterator[TimeSignatureTick]:
        ...
    def __len__(self) -> int:
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
    def append(self, x: TimeSignatureTick) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
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
    def sort(self) -> TimeSignatureTickList:
        ...
    def sort_inplace(self) -> TimeSignatureTickList:
        ...
class TrackQuarter:
    controls: ControlChangeQuarterList
    is_drum: bool
    name: str
    notes: NoteQuarterList
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
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TrackQuarter) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def clip(self, arg0: float, arg1: float, arg2: bool) -> TrackQuarter:
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
    def shift_pitch(self, arg0: int) -> TrackQuarter:
        ...
    def shift_pitch_inplace(self, arg0: int) -> TrackQuarter:
        ...
    def shift_time(self, arg0: float) -> TrackQuarter:
        ...
    def shift_time_inplace(self, arg0: float) -> TrackQuarter:
        ...
    def shift_velocity(self, arg0: int) -> TrackQuarter:
        ...
    def shift_velocity_inplace(self, arg0: int) -> TrackQuarter:
        ...
    def sort(self) -> TrackQuarter:
        ...
    def sort_inplace(self) -> TrackQuarter:
        ...
    def start(self) -> float:
        ...
class TrackQuarterList:
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
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
    @typing.overload
    def __getitem__(self, s: slice) -> TrackQuarterList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> TrackQuarter:
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
    def __iter__(self) -> typing.Iterator[TrackQuarter]:
        ...
    def __len__(self) -> int:
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
    def append(self, x: TrackQuarter) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
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
class TrackSecond:
    controls: ControlChangeSecondList
    is_drum: bool
    name: str
    notes: NoteSecondList
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
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TrackSecond) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def clip(self, arg0: float, arg1: float, arg2: bool) -> TrackSecond:
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
    def shift_pitch(self, arg0: int) -> TrackSecond:
        ...
    def shift_pitch_inplace(self, arg0: int) -> TrackSecond:
        ...
    def shift_time(self, arg0: float) -> TrackSecond:
        ...
    def shift_time_inplace(self, arg0: float) -> TrackSecond:
        ...
    def shift_velocity(self, arg0: int) -> TrackSecond:
        ...
    def shift_velocity_inplace(self, arg0: int) -> TrackSecond:
        ...
    def sort(self) -> TrackSecond:
        ...
    def sort_inplace(self) -> TrackSecond:
        ...
    def start(self) -> float:
        ...
class TrackSecondList:
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
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
    @typing.overload
    def __getitem__(self, s: slice) -> TrackSecondList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> TrackSecond:
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
    def __iter__(self) -> typing.Iterator[TrackSecond]:
        ...
    def __len__(self) -> int:
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
    def append(self, x: TrackSecond) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
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
class TrackTick:
    controls: ControlChangeTickList
    is_drum: bool
    name: str
    notes: NoteTickList
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
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TrackTick) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def clip(self, arg0: int, arg1: int, arg2: bool) -> TrackTick:
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
    def shift_pitch(self, arg0: int) -> TrackTick:
        ...
    def shift_pitch_inplace(self, arg0: int) -> TrackTick:
        ...
    def shift_time(self, arg0: int) -> TrackTick:
        ...
    def shift_time_inplace(self, arg0: int) -> TrackTick:
        ...
    def shift_velocity(self, arg0: int) -> TrackTick:
        ...
    def shift_velocity_inplace(self, arg0: int) -> TrackTick:
        ...
    def sort(self) -> TrackTick:
        ...
    def sort_inplace(self) -> TrackTick:
        ...
    def start(self) -> int:
        ...
class TrackTickList:
    def __bool__(self) -> bool:
        """
        Check whether the list is nonempty
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
    @typing.overload
    def __getitem__(self, s: slice) -> TrackTickList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> TrackTick:
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
    def __iter__(self) -> typing.Iterator[TrackTick]:
        ...
    def __len__(self) -> int:
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
    def append(self, x: TrackTick) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
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
