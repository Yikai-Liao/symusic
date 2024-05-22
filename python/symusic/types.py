try:
    from typing import Protocol  # type: ignore
except ImportError:
    from typing_extensions import Protocol  # type: ignore

from typing import List, Union

from . import core  # type: ignore

__all__ = [
    "TimeUnit",
    "GeneralTimeUnit",
    "TimeDtype",
    "Note",
    "KeySignature",
    "TimeSignature",
    "ControlChange",
    "Tempo",
    "Pedal",
    "PitchBend",
    "TextMeta",
    "Track",
    "Score",
]


class TimeUnit(Protocol):
    def __repr__(self) -> str:
        ...

    def is_time_unit(self) -> bool:
        ...


class ScoreSecond:
    def __init__(self, *args, **kwargs):
        raise NotImplementedError

    @classmethod
    def from_file(cls, *args, **kwargs):
        raise NotImplementedError


GeneralTimeUnit = Union[TimeUnit, str]
TimeDtype = Union[int, float]
Note = Union[core.NoteTick, core.NoteQuarter, core.NoteSecond]
KeySignature = Union[
    core.KeySignatureTick, core.KeySignatureQuarter, core.KeySignatureSecond
]
TimeSignature = Union[
    core.TimeSignatureTick, core.TimeSignatureQuarter, core.TimeSignatureSecond
]
ControlChange = Union[
    core.ControlChangeTick, core.ControlChangeQuarter, core.ControlChangeSecond
]
Tempo = Union[core.TempoTick, core.TempoQuarter, core.TempoSecond]
Pedal = Union[core.PedalTick, core.PedalQuarter, core.PedalSecond]
PitchBend = Union[core.PitchBendTick, core.PitchBendQuarter, core.PitchBendSecond]
TextMeta = Union[core.TextMetaTick, core.TextMetaQuarter, core.TextMetaSecond]
Track = Union[core.TrackTick, core.TrackQuarter, core.TrackSecond]
Score = Union[core.ScoreTick, core.ScoreQuarter, ScoreSecond]

NoteList = Union[core.NoteTickList, core.NoteQuarterList, core.NoteSecondList]
KeySignatureList = Union[
    core.KeySignatureTickList, core.KeySignatureQuarterList, core.KeySignatureSecondList
]
TimeSignatureList = Union[
    core.TimeSignatureTickList,
    core.TimeSignatureQuarterList,
    core.TimeSignatureSecondList,
]
ControlChangeList = Union[
    core.ControlChangeTickList,
    core.ControlChangeQuarterList,
    core.ControlChangeSecondList,
]
TempoList = Union[core.TempoTickList, core.TempoQuarterList, core.TempoSecondList]
PedalList = Union[core.PedalTickList, core.PedalQuarterList, core.PedalSecondList]
PitchBendList = Union[
    core.PitchBendTickList, core.PitchBendQuarterList, core.PitchBendSecondList
]
TextMetaList = Union[
    core.TextMetaTickList, core.TextMetaQuarterList, core.TextMetaSecondList
]
TrackList = Union[core.TrackTickList, core.TrackQuarterList, core.TrackSecondList]

GeneralNoteList = Union[NoteList, List[Note]]
GeneralKeySignatureList = Union[KeySignatureList, List[KeySignature]]
GeneralTimeSignatureList = Union[TimeSignatureList, List[TimeSignature]]
GeneralControlChangeList = Union[ControlChangeList, List[ControlChange]]
GeneralTempoList = Union[TempoList, List[Tempo]]
GeneralPedalList = Union[PedalList, List[Pedal]]
GeneralPitchBendList = Union[PitchBendList, List[PitchBend]]
GeneralTextMetaList = Union[TextMetaList, List[TextMeta]]
GeneralTrackList = Union[TrackList, List[Track]]


# TimeDtype = Union[int, float]
# TimeDtype = TypeVar('TimeDtype', bound=Union[int, float])

# class Note(Protocol[TimeDtype]):
#     # Note that start points to the same object as time
#     time: TimeDtype
#     start: TimeDtype
#     duration: TimeDtype
#     # pitch and velocity are actually stored in i8
#     pitch: int
#     velocity: int
#
#     @overload
#     def __init__(self, time: TimeDtype, duration: TimeDtype, pitch: int, velocity: int) -> None: ...
#     @overload
#     def __init__(self, other: Self) -> None: """Copy constructor"""
#
#     def __repr__(self) -> str: ...
#     def __copy__(self) -> Self: """Deep copy"""
#     def __deepcopy__(self) -> Self: """Deep copy"""
#
#     def copy(self) -> Self: """Deep copy"""
#     def empty(self) -> bool:  """duration <= 0 or velocity <= 0"""
#
#     @property
#     def end(self) -> TimeDtype: """time + duration"""
#     def end_time(self) -> TimeDtype: """time + duration"""
#
#     def shift_time_inplace(self, offset: TimeDtype) -> Self: """time += time"""
#     def shift_time(self, offset: TimeDtype) -> Self: """ copy().shift_time_inplace(offset) """
#     def shift_pitch_inplace(self, offset: int) -> Self: """pitch += offset"""
#     def shift_pitch(self, offset: int) -> Self: """ copy().shift_pitch_inplace(offset) """
#     def shift_velocity_inplace(self, offset: int) -> Self: """velocity += offset"""
#     def shift_velocity(self, offset: int) -> Self: """ copy().shift_velocity_inplace(offset) """
#
# class KeySignature(Protocol[TimeDtype]):
#     time: TimeDtype
#     # key is actually stored in i8
#     key: int
#     # tonalities is actually stored in i8
#     tonality: int
#
#     @overload
#     def __init__(self, time: TimeDtype, key: int, tonalities: int) -> None: ...
#     @overload
#     def __init__(self, other: Self) -> None: """Copy constructor"""
#
#     def __repr__(self) -> str: ...
#     def __copy__(self) -> Self: """Deep copy"""
#     def __deepcopy__(self) -> Self: """Deep copy"""
#
#     def copy(self) -> Self: """Deep copy"""
#
#     def shift_time_inplace(self, offset: TimeDtype) -> Self: """time += time"""
#     def shift_time(self, offset: TimeDtype) -> Self: """ copy().time_shift_inplace(offset) """
#
# class TimeSignature(Protocol[TimeDtype]):
#     time: TimeDtype
#     # numerator and denominator are actually stored in i8
#     numerator: int
#     denominator: int
#
#     @overload
#     def __init__(self, time: TimeDtype, numerator: int, denominator: int) -> None: ...
#     @overload
#     def __init__(self, other: Self) -> None: """Copy constructor"""
#
#     def __repr__(self) -> str: ...
#     def __copy__(self) -> Self: """Deep copy"""
#     def __deepcopy__(self) -> Self: """Deep copy"""
#
#     def copy(self) -> Self: """Deep copy"""
#
#     def shift_time_inplace(self, offset: TimeDtype) -> Self: """time += time"""
#     def shift_time(self, offset: TimeDtype) -> Self: """ copy().time_shift_inplace(offset) """
#
# class ControlChange(Protocol[TimeDtype]):
#     time: TimeDtype
#     # control is actually stored in u8
#     number: int
#     value: int
#
#     @overload
#     def __init__(self, time: TimeDtype, number: int, value: int) -> None: ...
#     @overload
#     def __init__(self, other: Self) -> None: """Copy constructor"""
#
#     def __repr__(self) -> str: ...
#     def __copy__(self) -> Self: """Deep copy"""
#     def __deepcopy__(self) -> Self: """Deep copy"""
#
#     def copy(self) -> Self: """Deep copy"""
#
#     def shift_time_inplace(self, offset: TimeDtype) -> Self: """time += time"""
#     def shift_time(self, offset: TimeDtype) -> Self: """ copy().time_shift_inplace(offset) """
#
# class Tempo(Protocol[TimeDtype]):
#     time: TimeDtype
#     # tempo is actually stored in u8
#     tempo: float
#
#     @overload
#     def __init__(self, time: TimeDtype, tempo: float) -> None: ...
#     @overload
#     def __init__(self, other: Self) -> None: """Copy constructor"""
#
#     def __repr__(self) -> str: ...
#     def __copy__(self) -> Self: """Deep copy"""
#     def __deepcopy__(self) -> Self: """Deep copy"""
#
#     def copy(self) -> Self: """Deep copy"""
#
#     def shift_time_inplace(self, offset: TimeDtype) -> Self: """time += time"""
#     def shift_time(self, offset: TimeDtype) -> Self: """ copy().time_shift_inplace(offset) """
#
# class PitchBend(Protocol[TimeDtype]):
#     time: TimeDtype
#     # value is actually stored in i32
#     value: int
#
#     @overload
#     def __init__(self, time: TimeDtype, value: int) -> None: ...
#     @overload
#     def __init__(self, other: Self) -> None: """Copy constructor"""
#
#     def __repr__(self) -> str: ...
#     def __copy__(self) -> Self: """Deep copy"""
#     def __deepcopy__(self) -> Self: """Deep copy"""
#
#     def copy(self) -> Self: """Deep copy"""
#
#     def shift_time_inplace(self, offset: TimeDtype) -> Self: """time += time"""
#     def shift_time(self, offset: TimeDtype) -> Self: """ copy().time_shift_inplace(offset) """
#
# class TextMeta(Protocol[TimeDtype]):
#     time: TimeDtype
#     text: str
#
#     @overload
#     def __init__(self, time: TimeDtype, text: str) -> None: ...
#     @overload
#     def __init__(self, other: Self) -> None: """Copy constructor"""
#
#     def __repr__(self) -> str: ...
#     def __copy__(self) -> Self: """Deep copy"""
#     def __deepcopy__(self) -> Self: """Deep copy"""
#
#     def copy(self) -> Self: """Deep copy"""
#
#     def shift_time_inplace(self, offset: TimeDtype) -> Self: """time += time"""
#     def shift_time(self, offset: TimeDtype) -> Self: """ copy().time_shift_inplace(offset) """

# T = TypeVar('T')
# class List(Protocol[T]):
#     def __bool__(self) -> bool:
#         """
#         Check whether the list is nonempty
#         """
#     @overload
#     def __delitem__(self, arg0: int) -> None:
#         """
#         Delete the list elements at index ``i``
#         """
#     @overload
#     def __delitem__(self, arg0: slice) -> None:
#         """
#         Delete list elements using a slice object
#         """
#     @overload
#     def __getitem__(self, s: slice) -> Self:
#         """
#         Retrieve list elements using a slice object
#         """
#     @overload
#     def __getitem__(self, arg0: int) -> T:
#         ...
#     @overload
#     def __init__(self) -> None:
#         ...
#     @overload
#     def __init__(self, arg0: Self) -> None:
#         """
#         Copy constructor
#         """
#     @overload
#     def __init__(self, arg0: Iterable) -> None:
#         ...
#     def __iter__(self) -> Iterator:
#         ...
#     def __len__(self) -> int:
#         ...
#     def __repr__(self) -> str:
#         """
#         Return the canonical string representation of this list.
#         """
#     @overload
#     def __setitem__(self, arg0: int, arg1: T) -> None:
#         ...
#     @overload
#     def __setitem__(self, arg0: slice, arg1: Self) -> None:
#         """
#         Assign list elements using a slice object
#         """
#     def append(self, x: T) -> None:
#         """
#         Add an item to the end of the list
#         """
#     def clear(self) -> None:
#         """
#         Clear the contents
#         """
#     @overload
#     def extend(self, L: Self) -> None:
#         """
#         Extend the list by appending all the items in the given list
#         """
#     @overload
#     def extend(self, L: Iterable) -> None:
#         """
#         Extend the list by appending all the items in the given list
#         """
#     def insert(self, i: int, x: T) -> None:
#         """
#         Insert an item at a given position.
#         """
#     @overload
#     def pop(self) -> T:
#         """
#         Remove and return the last item
#         """
#     @overload
#     def pop(self, i: int) -> T:
#         """
#         Remove and return the item at index ``i``
#         """
#     def sort(self) -> Self:
#         ...
#     def sort_inplace(self) -> Self:
#         ...


# class Track(Protocol[TimeDtype]):
#     name: str
#     program: int    # [0, 128)
#     is_drum: bool
#     notes: List
#     controls: List
#     pitch_bends: List
#
#     @overload
#     def __init__(self) -> None: """Empty constructor"""
#     @overload
#     def __init__(self, other: Self) -> None: """Copy constructor"""
#
#     def __repr__(self) -> str: ...
#     def __copy__(self) -> Self: """Deep copy"""
#     def __deepcopy__(self) -> Self: """Deep copy"""
#
#     def copy(self) -> Self: """Deep copy"""
#     def empty(self) -> bool: ...
#     def start(self) -> TimeDtype: ...
#     def end(self) -> TimeDtype: ...
#
#     def clip(self, start: TimeDtype, end: TimeDtype, clip_end: bool) -> Self: ...
#
#     def sort_inplace(self) -> Self: ...
#     def sort(self) -> Self: ...
#
#     def shift_time_inplace(self, offset: TimeDtype) -> Self: """time += time"""
#     def shift_time(self, offset: TimeDtype) -> Self: """ copy().time_shift_inplace(offset) """
#     def shift_pitch_inplace(self, offset: int) -> Self: """pitch += offset"""
#     def shift_pitch(self, offset: int) -> Self: """ copy().pitch_shift_inplace(offset) """
#     def shift_velocity_inplace(self, offset: int) -> Self: """velocity += offset"""
#     def shift_velocity(self, offset: int) -> Self: """ copy().velocity_shift_inplace(offset) """
#
# class Score(Protocol[TimeDtype]):
#     ticks_per_quarter: int # actually stored in i32
#     tracks: List
#     key_signatures: List
#     time_signatures: List
#     tempos: List
#     lyrics: List
#     markers: List
#
#     @overload
#     def __init__(self) -> None: """Empty constructor"""
#     @overload
#     def __init__(self, other: Self) -> None: """Copy constructor"""
#     @overload
#     def __init__(self, path: str) -> None: """Load from a MIDI file"""
#     @staticmethod
#     def from_file(path: str) -> 'Score[TimeDtype]': """Load from a MIDI file"""
#
#     def __repr__(self) -> str: ...
#     def __copy__(self) -> Self: """Deep copy"""
#     def __deepcopy__(self) -> Self: """Deep copy"""
#
#     def copy(self) -> Self: """Deep copy"""
#     def empty(self) -> bool: ...
#     def start(self) -> TimeDtype: ...
#     def end(self) -> TimeDtype: ...
#
#     def clip(self, start: TimeDtype, end: TimeDtype, clip_end: bool) -> Self: ...
#
#     def sort_inplace(self) -> Self: ...
#     def sort(self) -> Self: ...
#
#     def shift_time_inplace(self, offset: TimeDtype) -> Self: """time += time"""
#     def shift_time(self, offset: TimeDtype) -> Self: """ copy().time_shift_inplace(offset) """
#     def shift_pitch_inplace(self, offset: int) -> Self: """pitch += offset"""
#     def shift_pitch(self, offset: int) -> Self: """ copy().pitch_shift_inplace(offset) """
#     def shift_velocity_inplace(self, offset: int) -> Self: """velocity += offset"""
#     def shift_velocity(self, offset: int) -> Self: """ copy().velocity_shift_inplace(offset) """
