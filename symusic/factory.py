import os.path
from typing import Union, TypeVar, Generic
from dataclasses import dataclass
from pathlib import Path
from . import core  # type: ignore
from . import types as smt

__all__ = [
    "TimeUnit",
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

"""
All the Factory classes are initialized when the module is imported.
And the objects are created when the factory is called.
Note that the factory can not be created again by users,
because the factory is not exposed to the users by setting __all__ manually.

The __isinstancecheck__ method is overrided to make isinstance() work.
"""


class TimeUnitFactory:
    def __init__(self) -> None:
        self._tick = core.Tick()
        self._quarter = core.Quarter()
        self._second = core.Second()

    @property
    def tick(self) -> core.Tick:
        return self._tick

    @property
    def quarter(self) -> core.Quarter:
        return self._quarter

    @property
    def second(self) -> core.Second:
        return self._second

    def __call__(self, ttype: Union[smt.TimeUnit, str]) -> smt.TimeUnit:
        """
        Create a TimeUnit object from a str, e.g. 'tick', 'quarter', 'second'
        It is used to dispatch the correct TimeUnit object.
        However, it is recommended to use the `TimeUnit.tick`, `TimeUnit.quarter`, `TimeUnit.second`
        for better performance.
        """
        if isinstance(ttype, str):
            return self.from_str(ttype)
        try:
            ttype.is_time_unit()
            return ttype
        except AttributeError:
            raise TypeError(f"{ttype} is not a TimeUnit object")

    def from_str(self, ttype: str) -> smt.TimeUnit:
        ttype = ttype.lower()
        if ttype == "tick":
            return self.tick
        elif ttype == "quarter":
            return self.quarter
        elif ttype == "second":
            return self.second
        else:
            raise ValueError(f"Invalid time unit: {ttype}")


TimeUnit = TimeUnitFactory()
T = TypeVar("T")
Q = TypeVar("Q")
S = TypeVar("S")


@dataclass(frozen=True)
class CoreClasses(Generic[T, Q, S]):
    tick: T
    quarter: Q
    second: S

    def dispatch(
            self: "CoreClasses[T, Q, S]", ttype: smt.GeneralTimeUnit
    ) -> Union[T, Q, S]:
        """
        Dispatch the correct Core class according to the ttype.
        """
        if isinstance(ttype, core.Tick):
            return self.tick
        if isinstance(ttype, core.Quarter):
            return self.quarter
        if isinstance(ttype, core.Second):
            return self.second
        assert isinstance(ttype, str), f"Invalid time unit: {ttype}"
        # ttype can only be str now, while the type checker does not know it.
        ttype: str = ttype.lower()  # type: ignore
        if ttype == "tick":
            return self.tick
        elif ttype == "quarter":
            return self.quarter
        elif ttype == "second":
            return self.second
        else:
            raise ValueError(f"Invalid time unit: {ttype}")

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, (self.tick, self.quarter, self.second))  # type: ignore


@dataclass(frozen=True)
class NoteFactory:
    __core_classes = CoreClasses(core.NoteTick, core.NoteQuarter, core.NoteSecond)

    def __call__(
            self,
            time: smt.TimeDtype,
            duration: smt.TimeDtype,
            pitch: int,
            velocity: int,
            ttype: smt.GeneralTimeUnit = TimeUnit.tick,
    ) -> smt.Note:
        """
        Note that `smt.TimeDtype = Union[int, float]`, and Note constructor requires `int` or `float` as time.
        So Type Checker like MyPy will complain about the type of `time` argument.
        However, float and int can be converted to each other implicitly.
        So I just add a `# type: ignore` to ignore the type checking.
        """
        return self.__core_classes.dispatch(ttype)(time, duration, pitch, velocity)  # type: ignore

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, self.__core_classes)  #type: ignore


@dataclass(frozen=True)
class KeySignatureFactory:
    __core_classes = CoreClasses(
        core.KeySignatureTick, core.KeySignatureQuarter, core.KeySignatureSecond
    )

    def __call__(
            self,
            time: smt.TimeDtype,
            key: int,
            tonality: int,
            ttype: smt.GeneralTimeUnit = TimeUnit.tick,
    ) -> smt.KeySignature:
        return self.__core_classes.dispatch(ttype)(time, key, tonality)  # type: ignore

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, self.__core_classes)  #type: ignore


@dataclass(frozen=True)
class TimeSignatureFactory:
    __core_classes = CoreClasses(
        core.TimeSignatureTick, core.TimeSignatureQuarter, core.TimeSignatureSecond
    )

    def __call__(
            self,
            time: smt.TimeDtype,
            numerator: int,
            denominator: int,
            ttype: smt.GeneralTimeUnit = TimeUnit.tick,
    ) -> smt.TimeSignature:
        return self.__core_classes.dispatch(ttype)(time, numerator, denominator)  # type: ignore

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, self.__core_classes)  #type: ignore


@dataclass(frozen=True)
class ControlChangeFactory:
    __core_classes = CoreClasses(
        core.ControlChangeTick, core.ControlChangeQuarter, core.ControlChangeSecond
    )

    def __call__(
            self,
            time: smt.TimeDtype,
            number: int,
            value: int,
            ttype: smt.GeneralTimeUnit = TimeUnit.tick,
    ) -> smt.ControlChange:
        return self.__core_classes.dispatch(ttype)(time, number, value)  # type: ignore

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, self.__core_classes)  #type: ignore


@dataclass(frozen=True)
class TempoFactory:
    __core_classes = CoreClasses(core.TempoTick, core.TempoQuarter, core.TempoSecond)

    def __call__(
            self,
            time: smt.TimeDtype,
            tempo: float,
            ttype: smt.GeneralTimeUnit = TimeUnit.tick,
    ) -> smt.Tempo:
        return self.__core_classes.dispatch(ttype)(time, tempo)  # type: ignore

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, self.__core_classes)  #type: ignore


@dataclass(frozen=True)
class PedalFactory:
    __core_classes = CoreClasses(core.PedalTick, core.PedalQuarter, core.PedalSecond)

    def __call__(
            self,
            time: smt.TimeDtype,
            duration: smt.TimeDtype,
            ttype: smt.GeneralTimeUnit = TimeUnit.tick,
    ) -> smt.Pedal:
        return self.__core_classes.dispatch(ttype)(time, duration)  # type: ignore

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, self.__core_classes)  #type: ignore


@dataclass(frozen=True)
class PitchBendFactory:
    __core_classes = CoreClasses(
        core.PitchBendTick, core.PitchBendQuarter, core.PitchBendSecond
    )

    def __call__(
            self,
            time: smt.TimeDtype,
            value: int,
            ttype: smt.GeneralTimeUnit = TimeUnit.tick,
    ) -> smt.PitchBend:
        return self.__core_classes.dispatch(ttype)(time, value)  # type: ignore

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, self.__core_classes)  #type: ignore


@dataclass(frozen=True)
class TextMetaFactory:
    __core_classes = CoreClasses(
        core.TextMetaTick, core.TextMetaQuarter, core.TextMetaSecond
    )

    def __call__(
            self, time: smt.TimeDtype, text: str, ttype: smt.GeneralTimeUnit = TimeUnit.tick
    ) -> smt.TextMeta:
        return self.__core_classes.dispatch(ttype)(time, text)  # type: ignore

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, self.__core_classes)  #type: ignore


@dataclass(frozen=True)
class TrackFactory:
    __core_classes = CoreClasses(core.TrackTick, core.TrackQuarter, core.TrackSecond)

    def __call__(
        self, name: str = "", program: int = 0, is_drum: bool = False, 
        notes: smt.GeneralNoteList = [],
        controls: smt.GeneralControlChangeList = [],
        pitch_bends: smt.GeneralPitchBendList = [],
        pedals: smt.GeneralPedalList = [],
        ttype: smt.GeneralTimeUnit = TimeUnit.tick
    ) -> smt.Track:
        r"""
        Create a Track object with the given parameters.
        Note that all of this parameters are optional,
        and they will be copied to the new Track object.
        So it is safe to use `[]` in the default value.

        Of couse, copy will cause a little overhead, but it is acceptable.
        And create a `Note` object (binded by pybind11) is much more expensive.
        """
        new_track = self.empty(ttype)
        new_track.name = name
        new_track.program = program
        new_track.is_drum = is_drum
        new_track.notes = notes                 # type: ignore
        new_track.controls = controls           # type: ignore
        new_track.pitch_bends = pitch_bends     # type: ignore
        new_track.pedals = pedals               # type: ignore
        return new_track

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, self.__core_classes)  #type: ignore
    
    def empty(self, ttype: smt.GeneralTimeUnit = TimeUnit.tick) -> smt.Track:
        # create a empty track
        return self.__core_classes.dispatch(ttype)()


@dataclass(frozen=True)
class ScoreFactory:
    __core_classes = CoreClasses(core.ScoreTick, core.ScoreQuarter, smt.ScoreSecond)

    def __call__(
            self, x: Union[int, str, Path, smt.Score] = 0, 
            ttype: smt.GeneralTimeUnit = TimeUnit.tick
    ) -> smt.Score:  
        if isinstance(x, str) or isinstance(x, Path):
            return self.from_file(x, ttype)
        elif isinstance(x, int):
            return self.from_tpq(x, ttype)
        elif isinstance(x, self):   # type: ignore
            return self.from_other(x, ttype)
        else:
            raise TypeError(f"Invalid type: {type(x)}")

    def from_file(
            self, path: Union[str, Path], ttype: smt.GeneralTimeUnit = TimeUnit.tick
    ) -> smt.Score:
        assert os.path.isfile(path), f"{path} is not a file"
        return self.__core_classes.dispatch(ttype).from_file(str(path))

    def from_tpq(self, tpq: int = 960, ttype: smt.GeneralTimeUnit = TimeUnit.tick) -> smt.Score:
        return self.__core_classes.dispatch(ttype)(tpq)
    
    def from_other(self, other: smt.Score, ttype: smt.GeneralTimeUnit = TimeUnit.tick) -> smt.Score:
        assert other.ticks_per_quarter > 0, f"ticks_per_quarter must be positive, but got {other.ticks_per_quarter}"
        return self.__core_classes.dispatch(ttype)(other)

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, self.__core_classes)  #type: ignore


Note = NoteFactory()
KeySignature = KeySignatureFactory()
TimeSignature = TimeSignatureFactory()
ControlChange = ControlChangeFactory()
Tempo = TempoFactory()
Pedal = PedalFactory()
PitchBend = PitchBendFactory()
TextMeta = TextMetaFactory()
Track = TrackFactory()
Score = ScoreFactory()
