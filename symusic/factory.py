import os.path
from dataclasses import dataclass
from pathlib import Path
from typing import Generic, Optional, TypeVar, Union

from numpy import ndarray

from . import core  # type: ignore
from . import types as smt

# import subprocess
# from random import randint

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
    "Synthesizer",
]

_HERE = Path(__file__).parent
_BIN = _HERE / "bin"
# for win
if os.name == "nt":
    _MIDI2ABC = _BIN / "midi2abc.exe"
    _ABC2MIDI = _BIN / "abc2midi.exe"
# for linux
else:
    _MIDI2ABC = _BIN / "midi2abc"
    _ABC2MIDI = _BIN / "abc2midi"

if not _MIDI2ABC.exists():
    raise FileNotFoundError(f"{_MIDI2ABC} does not exist")
if not _ABC2MIDI.exists():
    raise FileNotFoundError(f"{_ABC2MIDI} does not exist")
# set env var SYMUSIC_MIDI2ABC
os.environ["SYMUSIC_MIDI2ABC"] = str(_MIDI2ABC)
os.environ["SYMUSIC_ABC2MIDI"] = str(_ABC2MIDI)

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
        except AttributeError as e:
            raise TypeError(f"{ttype} is not a TimeUnit object") from e

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
        Note that `smt.TimeDtype = Union[int, float]`, and Note constructor requires
        `int` or `float` as time. So Type Checker like MyPy will complain about the
        type of `time` argument. However, float and int can be converted to each other
        implicitly. So I just add a `# type: ignore` to ignore the type checking.
        """
        return self.__core_classes.dispatch(ttype)(time, duration, pitch, velocity)  # type: ignore

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, self.__core_classes)  # type: ignore

    def from_numpy(
        self,
        time: ndarray,
        duration: ndarray,
        pitch: ndarray,
        velocity: ndarray,
        ttype: smt.GeneralTimeUnit = TimeUnit.tick,
    ) -> smt.GeneralNoteList:
        return self.__core_classes.dispatch(ttype).from_numpy(
            time, duration, pitch, velocity
        )


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
        return isinstance(instance, self.__core_classes)  # type: ignore

    def from_numpy(
        self,
        time: ndarray,
        key: ndarray,
        tonality: ndarray,
        ttype: smt.GeneralTimeUnit = TimeUnit.tick,
    ) -> smt.GeneralKeySignatureList:
        return self.__core_classes.dispatch(ttype).from_numpy(time, key, tonality)


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
        return isinstance(instance, self.__core_classes)  # type: ignore

    def from_numpy(
        self,
        time: ndarray,
        numerator: ndarray,
        denominator: ndarray,
        ttype: smt.GeneralTimeUnit = TimeUnit.tick,
    ) -> smt.GeneralTimeSignatureList:
        return self.__core_classes.dispatch(ttype).from_numpy(
            time, numerator, denominator
        )


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
        return isinstance(instance, self.__core_classes)  # type: ignore

    def from_numpy(
        self,
        time: ndarray,
        number: ndarray,
        value: ndarray,
        ttype: smt.GeneralTimeUnit = TimeUnit.tick,
    ) -> smt.GeneralControlChangeList:
        return self.__core_classes.dispatch(ttype).from_numpy(time, number, value)


@dataclass(frozen=True)
class TempoFactory:
    __core_classes = CoreClasses(core.TempoTick, core.TempoQuarter, core.TempoSecond)

    def __call__(
        self,
        time: smt.TimeDtype,
        qpm: Optional[float] = None,
        mspq: Optional[int] = None,
        ttype: smt.GeneralTimeUnit = TimeUnit.tick,
    ) -> smt.Tempo:
        """
        :param time: the time of the tempo change, in the unit of `ttype`
        :param qpm: quarter per minute. The `bpm` in miditoolkit is actually quarter per minute, not beat per minute.
        :param mspq: microsecond per quarter. We store mspq instead of qpm to avoid float precision problem.
        :param ttype: the time unit of `time`
        :return:
        """
        return self.__core_classes.dispatch(ttype)(time, qpm, mspq)  # type: ignore

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, self.__core_classes)  # type: ignore

    def from_numpy(
        self, time: ndarray, mspq: ndarray, ttype: smt.GeneralTimeUnit = TimeUnit.tick
    ) -> smt.GeneralTempoList:
        return self.__core_classes.dispatch(ttype).from_numpy(time, mspq)


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
        return isinstance(instance, self.__core_classes)  # type: ignore

    def from_numpy(
        self,
        time: ndarray,
        duration: ndarray,
        ttype: smt.GeneralTimeUnit = TimeUnit.tick,
    ) -> smt.GeneralPedalList:
        return self.__core_classes.dispatch(ttype).from_numpy(time, duration)


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
        return isinstance(instance, self.__core_classes)  # type: ignore

    def from_numpy(
        self, time: ndarray, value: ndarray, ttype: smt.GeneralTimeUnit = TimeUnit.tick
    ) -> smt.GeneralPitchBendList:
        return self.__core_classes.dispatch(ttype).from_numpy(time, value)


@dataclass(frozen=True)
class TextMetaFactory:
    __core_classes = CoreClasses(
        core.TextMetaTick, core.TextMetaQuarter, core.TextMetaSecond
    )

    def __call__(
        self,
        time: smt.TimeDtype,
        text: str,
        ttype: smt.GeneralTimeUnit = TimeUnit.tick,
    ) -> smt.TextMeta:
        return self.__core_classes.dispatch(ttype)(time, text)  # type: ignore

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, self.__core_classes)  # type: ignore

    def from_numpy(
        self, time: ndarray, text: ndarray, ttype: smt.GeneralTimeUnit = TimeUnit.tick
    ) -> smt.GeneralTextMetaList:
        raise NotImplementedError
        # return self.__core_classes.dispatch(ttype).from_numpy(time, text)


@dataclass(frozen=True)
class TrackFactory:
    __core_classes = CoreClasses(core.TrackTick, core.TrackQuarter, core.TrackSecond)

    def __call__(
        self,
        name: str = "",
        program: int = 0,
        is_drum: bool = False,
        notes: smt.GeneralNoteList = None,
        controls: smt.GeneralControlChangeList = None,
        pitch_bends: smt.GeneralPitchBendList = None,
        pedals: smt.GeneralPedalList = None,
        ttype: smt.GeneralTimeUnit = TimeUnit.tick,
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
        new_track.notes = notes if notes else []
        new_track.controls = controls if controls else []
        new_track.pitch_bends = pitch_bends if pitch_bends else []
        new_track.pedals = pedals if pedals else []
        return new_track

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, self.__core_classes)  # type: ignore

    def empty(self, ttype: smt.GeneralTimeUnit = TimeUnit.tick) -> smt.Track:
        # create a empty track
        return self.__core_classes.dispatch(ttype)()


@dataclass(frozen=True)
class ScoreFactory:
    __core_classes = CoreClasses(core.ScoreTick, core.ScoreQuarter, core.ScoreSecond)

    def __call__(
        self,
        x: Union[int, str, Path, smt.Score] = 0,
        ttype: smt.GeneralTimeUnit = TimeUnit.tick,
        fmt: Optional[str] = None,
    ) -> smt.Score:
        if isinstance(x, str) or isinstance(x, Path):
            return self.from_file(x, ttype, fmt)
        elif isinstance(x, int):
            return self.from_tpq(x, ttype)
        elif isinstance(x, self):  # type: ignore
            return self.from_other(x, ttype)
        else:
            raise TypeError(f"Invalid type: {type(x)}")

    def from_file(
        self,
        path: Union[str, Path],
        ttype: smt.GeneralTimeUnit = TimeUnit.tick,
        fmt: Optional[str] = None,
    ) -> smt.Score:
        assert os.path.isfile(path), f"{path} is not a file"
        return self.__core_classes.dispatch(ttype).from_file(path, fmt)

    def from_abc(
        self,
        abc: str,
        ttype: smt.GeneralTimeUnit = TimeUnit.tick,
    ) -> smt.Score:
        return self.__core_classes.dispatch(ttype).from_abc(abc)

    def from_tpq(
        self, tpq: int = 960, ttype: smt.GeneralTimeUnit = TimeUnit.tick
    ) -> smt.Score:
        return self.__core_classes.dispatch(ttype)(tpq)

    def from_other(
        self,
        other: smt.Score,
        ttype: smt.GeneralTimeUnit = TimeUnit.tick,
        min_dur: Optional[int] = None,
    ) -> smt.Score:
        if other.ticks_per_quarter <= 0:
            raise ValueError(
                f"ticks_per_quarter must be positive, but got {other.ticks_per_quarter}"
            )
        return self.__core_classes.dispatch(ttype)(other, min_dur)

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, self.__core_classes)  # type: ignore


class SynthesizerFactory:
    def __call__(self, sf_path: str, sample_rate: int, quality: int, worker_num: int):
        return core.Synthesizer(sf_path, sample_rate, quality, worker_num)


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
Synthesizer = SynthesizerFactory()
