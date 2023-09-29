from __future__ import annotations
import numpy
import typing
__all__ = ['ControlChange', 'ControlChangeList', 'ControlMap', 'KeySignature', 'KeySignatureList', 'Note', 'NoteArray', 'NoteList', 'Score', 'Tempo', 'TempoList', 'TimeSignature', 'TimeSignatureList', 'Track', 'TrackList']
class ControlChange:
    time: float
    value: int
    @typing.overload
    def __init__(self, arg0: float, arg1: int) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: ControlChange) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> ControlChange:
        """
        Deep copy
        """
class ControlChangeList:
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
    def __getitem__(self, s: slice) -> ControlChangeList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> ControlChange:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: ControlChangeList) -> None:
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
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: ControlChange) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: ControlChangeList) -> None:
        """
        Assign list elements using a slice object
        """
    def append(self, x: ControlChange) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    @typing.overload
    def extend(self, L: ControlChangeList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: ControlChange) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> ControlChange:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> ControlChange:
        """
        Remove and return the item at index ``i``
        """
class ControlMap:
    def __bool__(self) -> bool:
        """
        Check whether the map is nonempty
        """
    @typing.overload
    def __contains__(self, arg0: int) -> bool:
        ...
    @typing.overload
    def __contains__(self, arg0: typing.Any) -> bool:
        ...
    def __delitem__(self, arg0: int) -> None:
        ...
    def __getitem__(self, arg0: int) -> ControlChangeList:
        ...
    def __init__(self) -> None:
        ...
    def __iter__(self) -> typing.Iterator:
        ...
    def __len__(self) -> int:
        ...
    def __repr__(self) -> str:
        ...
    def __setitem__(self, arg0: int, arg1: ControlChangeList) -> None:
        ...
    def items(self) -> typing.ItemsView[int, ControlChangeList]:
        ...
    def keys(self) -> typing.KeysView[int]:
        ...
    def values(self) -> typing.ValuesView[ControlChangeList]:
        ...
class KeySignature:
    key: int
    time: float
    tonality: int
    @typing.overload
    def __init__(self, arg0: float, arg1: int, arg2: int) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: KeySignature) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> KeySignature:
        """
        Deep copy
        """
class KeySignatureList:
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
    def __getitem__(self, s: slice) -> KeySignatureList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> KeySignature:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: KeySignatureList) -> None:
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
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: KeySignature) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: KeySignatureList) -> None:
        """
        Assign list elements using a slice object
        """
    def append(self, x: KeySignature) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    @typing.overload
    def extend(self, L: KeySignatureList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: KeySignature) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> KeySignature:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> KeySignature:
        """
        Remove and return the item at index ``i``
        """
class Note:
    duration: float
    pitch: int
    start: float
    velocity: int
    @typing.overload
    def __init__(self, arg0: float, arg1: float, arg2: int, arg3: int) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: Note) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> Note:
        """
        Deep copy the note
        """
    def empty(self) -> bool:
        """
        duration <= 0 or velocity <= 0
        """
    def end_time(self) -> float:
        ...
    def shift_pitch(self, arg0: int) -> Note:
        ...
    def shift_time(self, arg0: float) -> Note:
        ...
    def shift_velocity(self, arg0: int) -> Note:
        ...
    @property
    def end(self) -> float:
        ...
class NoteArray:
    duration: numpy.ndarray
    name: str
    pitch: numpy.ndarray
    program: int
    start: numpy.ndarray
    velocity: numpy.ndarray
    @typing.overload
    def __init__(self, arg0: Track) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: NoteArray) -> None:
        ...
    def __repr__(self) -> str:
        ...
    def copy(self) -> NoteArray:
        """
        Deep copy
        """
class NoteList:
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
    def __getitem__(self, s: slice) -> NoteList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> Note:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: NoteList) -> None:
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
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: Note) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: NoteList) -> None:
        """
        Assign list elements using a slice object
        """
    def append(self, x: Note) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    @typing.overload
    def extend(self, L: NoteList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: Note) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> Note:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> Note:
        """
        Remove and return the item at index ``i``
        """
class Score:
    key_signatures: KeySignatureList
    tempos: TempoList
    time_signatures: TimeSignatureList
    tracks: TrackList
    @staticmethod
    def from_file(*args, **kwargs):
        """
        (arg0: str) -> symusic.Score
        """
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: str) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: Score) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def clip_time_sorted(self, arg0: float, arg1: float) -> Score:
        """
        Clip sorted notes to a given time range
        """
    def clip_time_unsorted(self, arg0: float, arg1: float) -> Score:
        """
        Clip unordered notes to a given time range
        """
    def copy(self) -> Score:
        ...
    def end_time(self) -> float:
        ...
    def filter_notes(self, arg0: typing.Callable[[symusic.Note], bool]) -> Score:
        """
        Filter notes by a given function
        """
    def note_num(self) -> int:
        ...
    def shift_pitch(self, arg0: int) -> Score:
        ...
    def shift_time(self, arg0: float) -> Score:
        ...
    def shift_velocity(self, arg0: int) -> Score:
        ...
    def sort(self) -> Score:
        ...
class Tempo:
    tempo: float
    time: float
    @typing.overload
    def __init__(self, arg0: float, arg1: float) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: Tempo) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> Tempo:
        """
        Deep copy
        """
class TempoList:
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
    def __getitem__(self, s: slice) -> TempoList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> Tempo:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TempoList) -> None:
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
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: Tempo) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: TempoList) -> None:
        """
        Assign list elements using a slice object
        """
    def append(self, x: Tempo) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    @typing.overload
    def extend(self, L: TempoList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: Tempo) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> Tempo:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> Tempo:
        """
        Remove and return the item at index ``i``
        """
class TimeSignature:
    denominator: int
    numerator: int
    time: float
    @typing.overload
    def __init__(self, arg0: float, arg1: int, arg2: int) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TimeSignature) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def copy(self) -> TimeSignature:
        """
        Deep copy
        """
class TimeSignatureList:
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
    def __getitem__(self, s: slice) -> TimeSignatureList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> TimeSignature:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TimeSignatureList) -> None:
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
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: TimeSignature) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: TimeSignatureList) -> None:
        """
        Assign list elements using a slice object
        """
    def append(self, x: TimeSignature) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    @typing.overload
    def extend(self, L: TimeSignatureList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: TimeSignature) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> TimeSignature:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> TimeSignature:
        """
        Remove and return the item at index ``i``
        """
class Track:
    controls: ControlMap
    is_drum: bool
    name: str
    notes: NoteList
    program: int
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: Track) -> None:
        """
        Copy constructor
        """
    def __repr__(self) -> str:
        ...
    def clip_time_sorted(self, arg0: float, arg1: float) -> Track:
        """
        Clip sorted notes to a given time range
        """
    def clip_time_unsorted(self, arg0: float, arg1: float) -> Track:
        """
        Clip unordered notes to a given time range
        """
    def copy(self) -> Track:
        """
        Deep copy
        """
    def end_time(self) -> float:
        ...
    def filter_notes(self, arg0: typing.Callable[[symusic.Note], bool]) -> Track:
        """
        Filter notes by a given function
        """
    def frame_pianoroll(self, quantization: int = 16) -> numpy.ndarray:
        ...
    def note_array(self) -> NoteArray:
        ...
    def note_num(self) -> int:
        ...
    def onset_pianoroll(self, quantization: int = 16) -> numpy.ndarray:
        ...
    def shift_pitch(self, arg0: int) -> Track:
        ...
    def shift_time(self, arg0: float) -> Track:
        ...
    def shift_velocity(self, arg0: int) -> Track:
        ...
    def sort(self) -> Track:
        ...
class TrackList:
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
    def __getitem__(self, s: slice) -> TrackList:
        """
        Retrieve list elements using a slice object
        """
    @typing.overload
    def __getitem__(self, arg0: int) -> Track:
        ...
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: TrackList) -> None:
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
    def __repr__(self) -> str:
        ...
    @typing.overload
    def __setitem__(self, arg0: int, arg1: Track) -> None:
        ...
    @typing.overload
    def __setitem__(self, arg0: slice, arg1: TrackList) -> None:
        """
        Assign list elements using a slice object
        """
    def append(self, x: Track) -> None:
        """
        Add an item to the end of the list
        """
    def clear(self) -> None:
        """
        Clear the contents
        """
    @typing.overload
    def extend(self, L: TrackList) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    @typing.overload
    def extend(self, L: typing.Iterable) -> None:
        """
        Extend the list by appending all the items in the given list
        """
    def insert(self, i: int, x: Track) -> None:
        """
        Insert an item at a given position.
        """
    @typing.overload
    def pop(self) -> Track:
        """
        Remove and return the last item
        """
    @typing.overload
    def pop(self, i: int) -> Track:
        """
        Remove and return the item at index ``i``
        """
__version__: str = 'dev'
