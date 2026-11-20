from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import TYPE_CHECKING, Generic, TypeVar

from . import core  # type: ignore
from . import types as smt
from .soundfont import BuiltInSF3

if TYPE_CHECKING:
    from numpy import ndarray

__all__ = [
    "ControlChange",
    "KeySignature",
    "Note",
    "NoteTuning",
    "Pedal",
    "PitchBend",
    "Score",
    "Synthesizer",
    "Tempo",
    "TextMeta",
    "TimeSignature",
    "TimeUnit",
    "Track",
]

# The public names Score, Note, ... are instances of the factory classes below. Calling one
# dispatches on ``ttype`` to the class of that unit in symusic.core, such as ScoreTick, and
# ``__instancecheck__`` makes ``isinstance(x, Score)`` true for all three units.


class TimeUnitFactory:
    """The time units: ``TimeUnit.tick``, ``TimeUnit.quarter`` and ``TimeUnit.second``.

    Every ``ttype`` argument accepts these objects or the strings ``"tick"``,
    ``"quarter"`` and ``"second"`` in any letter case.
    """

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

    def __call__(self, ttype: smt.TimeUnit | str) -> smt.TimeUnit:
        """Return the time unit named by ``ttype``, or ``ttype`` itself if it is one.

        Raises ``ValueError`` for an unknown name and ``TypeError`` for other objects.
        """
        if isinstance(ttype, str):
            return self.from_str(ttype)
        try:
            ttype.is_time_unit()
            return ttype
        except AttributeError as e:
            msg = f"{ttype} is not a TimeUnit object"
            raise TypeError(msg) from e

    def from_str(self, ttype: str) -> smt.TimeUnit:
        ttype = ttype.lower()
        if ttype == "tick":
            return self.tick
        if ttype == "quarter":
            return self.quarter
        if ttype == "second":
            return self.second
        msg = f"Invalid time unit: {ttype}"
        raise ValueError(msg)


TimeUnit = TimeUnitFactory()
T = TypeVar("T")
Q = TypeVar("Q")
S = TypeVar("S")


@dataclass(frozen=True)
class CoreClasses(Generic[T, Q, S]):
    """The Tick, Quarter and Second classes of one symusic.core type."""

    tick: T
    quarter: Q
    second: S

    def dispatch(
        self: CoreClasses[T, Q, S],
        ttype: smt.GeneralTimeUnit,
    ) -> T | Q | S:
        """Return the class for ``ttype``, a time unit or its name."""
        if isinstance(ttype, core.Tick):
            return self.tick
        if isinstance(ttype, core.Quarter):
            return self.quarter
        if isinstance(ttype, core.Second):
            return self.second
        if not isinstance(ttype, str):
            raise ValueError(_ := f"Invalid time unit: {ttype}")
        ttype: str = ttype.lower()  # type: ignore
        if ttype == "tick":
            return self.tick
        if ttype == "quarter":
            return self.quarter
        if ttype == "second":
            return self.second
        msg = f"Invalid time unit: {ttype}"
        raise ValueError(msg)

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, (self.tick, self.quarter, self.second))  # type: ignore


@dataclass(frozen=True)
class NoteFactory:
    """``Note(time, duration, pitch, velocity, ttype="tick")`` creates a note."""

    __core_classes = CoreClasses(core.NoteTick, core.NoteQuarter, core.NoteSecond)
    __core_lists = CoreClasses(
        core.NoteTickList,
        core.NoteQuarterList,
        core.NoteSecondList,
    )

    def __call__(
        self,
        time: smt.TimeDtype,
        duration: smt.TimeDtype,
        pitch: int,
        velocity: int,
        ttype: smt.GeneralTimeUnit = "tick",
    ) -> smt.Note:
        return self.__core_classes.dispatch(ttype)(time, duration, pitch, velocity)  # type: ignore

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, self.__core_classes)  # type: ignore

    def from_numpy(
        self,
        time: ndarray,
        duration: ndarray,
        pitch: ndarray,
        velocity: ndarray,
        ttype: smt.GeneralTimeUnit = "tick",
    ) -> smt.GeneralNoteList:
        """Build a note list from one array per field, all of equal length."""
        return self.__core_lists.dispatch(ttype).from_numpy(
            time,
            duration,
            pitch,
            velocity,
        )


@dataclass(frozen=True)
class KeySignatureFactory:
    """``KeySignature(time, key, tonality, ttype="tick")`` creates a key signature."""

    __core_classes = CoreClasses(
        core.KeySignatureTick,
        core.KeySignatureQuarter,
        core.KeySignatureSecond,
    )
    __core_lists = CoreClasses(
        core.KeySignatureTickList,
        core.KeySignatureQuarterList,
        core.KeySignatureSecondList,
    )

    def __call__(
        self,
        time: smt.TimeDtype,
        key: int,
        tonality: int,
        ttype: smt.GeneralTimeUnit = "tick",
    ) -> smt.KeySignature:
        return self.__core_classes.dispatch(ttype)(time, key, tonality)  # type: ignore

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, self.__core_classes)  # type: ignore

    def from_numpy(
        self,
        time: ndarray,
        key: ndarray,
        tonality: ndarray,
        ttype: smt.GeneralTimeUnit = "tick",
    ) -> smt.GeneralKeySignatureList:
        """Build a key signature list from one array per field."""
        return self.__core_lists.dispatch(ttype).from_numpy(time, key, tonality)


@dataclass(frozen=True)
class TimeSignatureFactory:
    """Create a time signature. Use ``numerator=0, denominator=0`` for free meter."""

    __core_classes = CoreClasses(
        core.TimeSignatureTick,
        core.TimeSignatureQuarter,
        core.TimeSignatureSecond,
    )
    __core_lists = CoreClasses(
        core.TimeSignatureTickList,
        core.TimeSignatureQuarterList,
        core.TimeSignatureSecondList,
    )

    def __call__(
        self,
        time: smt.TimeDtype,
        numerator: int,
        denominator: int,
        ttype: smt.GeneralTimeUnit = "tick",
    ) -> smt.TimeSignature:
        return self.__core_classes.dispatch(ttype)(time, numerator, denominator)  # type: ignore

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, self.__core_classes)  # type: ignore

    def from_numpy(
        self,
        time: ndarray,
        numerator: ndarray,
        denominator: ndarray,
        ttype: smt.GeneralTimeUnit = "tick",
        *,
        beat_groups: ndarray | None = None,
    ) -> smt.GeneralTimeSignatureList:
        """Build time signatures from arrays, with optional zero-padded beat groups."""
        return self.__core_lists.dispatch(ttype).from_numpy(
            time,
            numerator,
            denominator,
            beat_groups=beat_groups,
        )


@dataclass(frozen=True)
class ControlChangeFactory:
    """``ControlChange(time, number, value, ttype="tick")`` creates a control change."""

    __core_classes = CoreClasses(
        core.ControlChangeTick,
        core.ControlChangeQuarter,
        core.ControlChangeSecond,
    )
    __core_lists = CoreClasses(
        core.ControlChangeTickList,
        core.ControlChangeQuarterList,
        core.ControlChangeSecondList,
    )

    def __call__(
        self,
        time: smt.TimeDtype,
        number: int,
        value: int,
        ttype: smt.GeneralTimeUnit = "tick",
    ) -> smt.ControlChange:
        return self.__core_classes.dispatch(ttype)(time, number, value)  # type: ignore

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, self.__core_classes)  # type: ignore

    def from_numpy(
        self,
        time: ndarray,
        number: ndarray,
        value: ndarray,
        ttype: smt.GeneralTimeUnit = "tick",
    ) -> smt.GeneralControlChangeList:
        """Build a control change list from one array per field."""
        return self.__core_lists.dispatch(ttype).from_numpy(time, number, value)


@dataclass(frozen=True)
class TempoFactory:
    """``Tempo(time, qpm=None, mspq=None, ttype="tick")`` creates a tempo change."""

    __core_classes = CoreClasses(core.TempoTick, core.TempoQuarter, core.TempoSecond)
    __core_lists = CoreClasses(
        core.TempoTickList,
        core.TempoQuarterList,
        core.TempoSecondList,
    )

    def __call__(
        self,
        time: smt.TimeDtype,
        qpm: float | None = None,
        mspq: int | None = None,
        ttype: smt.GeneralTimeUnit = "tick",
    ) -> smt.Tempo:
        """Create a tempo change from ``qpm`` or ``mspq``.

        ``qpm`` is quarter notes per minute, the ``bpm`` of pretty_midi and
        miditoolkit. ``mspq`` is microseconds per quarter note, the integer that
        symusic stores; ``qpm`` is rounded to it. ``qpm`` wins when both are given,
        and giving neither raises ``ValueError``.
        """
        return self.__core_classes.dispatch(ttype)(time, qpm, mspq)  # type: ignore

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, self.__core_classes)  # type: ignore

    def from_numpy(
        self,
        time: ndarray,
        mspq: ndarray,
        ttype: smt.GeneralTimeUnit = "tick",
    ) -> smt.GeneralTempoList:
        """Build a tempo list from ``time`` and ``mspq`` arrays."""
        return self.__core_lists.dispatch(ttype).from_numpy(time, mspq)


@dataclass(frozen=True)
class PedalFactory:
    """``Pedal(time, duration, ttype="tick")`` creates a sustain pedal region."""

    __core_classes = CoreClasses(core.PedalTick, core.PedalQuarter, core.PedalSecond)
    __core_lists = CoreClasses(
        core.PedalTickList,
        core.PedalQuarterList,
        core.PedalSecondList,
    )

    def __call__(
        self,
        time: smt.TimeDtype,
        duration: smt.TimeDtype,
        ttype: smt.GeneralTimeUnit = "tick",
    ) -> smt.Pedal:
        return self.__core_classes.dispatch(ttype)(time, duration)  # type: ignore

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, self.__core_classes)  # type: ignore

    def from_numpy(
        self,
        time: ndarray,
        duration: ndarray,
        ttype: smt.GeneralTimeUnit = "tick",
    ) -> smt.GeneralPedalList:
        """Build a pedal list from one array per field."""
        return self.__core_lists.dispatch(ttype).from_numpy(time, duration)


@dataclass(frozen=True)
class PitchBendFactory:
    """``PitchBend(time, value, ttype="tick")`` creates a pitch bend."""

    __core_classes = CoreClasses(
        core.PitchBendTick,
        core.PitchBendQuarter,
        core.PitchBendSecond,
    )
    __core_lists = CoreClasses(
        core.PitchBendTickList,
        core.PitchBendQuarterList,
        core.PitchBendSecondList,
    )

    def __call__(
        self,
        time: smt.TimeDtype,
        value: int,
        ttype: smt.GeneralTimeUnit = "tick",
    ) -> smt.PitchBend:
        return self.__core_classes.dispatch(ttype)(time, value)  # type: ignore

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, self.__core_classes)  # type: ignore

    def from_numpy(
        self,
        time: ndarray,
        value: ndarray,
        ttype: smt.GeneralTimeUnit = "tick",
    ) -> smt.GeneralPitchBendList:
        """Build a pitch bend list from one array per field."""
        return self.__core_lists.dispatch(ttype).from_numpy(time, value)


@dataclass(frozen=True)
class NoteTuningFactory:
    """``NoteTuning(time, key, pitch, ttype="tick")`` retunes one MIDI key."""

    __core_classes = CoreClasses(
        core.NoteTuningTick, core.NoteTuningQuarter, core.NoteTuningSecond
    )

    def __call__(
        self,
        time: smt.TimeDtype,
        key: int,
        pitch: float,
        ttype: smt.GeneralTimeUnit = "tick",
    ) -> smt.NoteTuning:
        return self.__core_classes.dispatch(ttype)(time, key, pitch)  # type: ignore

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, self.__core_classes)  # type: ignore


@dataclass(frozen=True)
class TextMetaFactory:
    """``TextMeta(time, text, ttype="tick")`` creates a lyric or marker."""

    __core_classes = CoreClasses(
        core.TextMetaTick,
        core.TextMetaQuarter,
        core.TextMetaSecond,
    )
    __core_lists = CoreClasses(
        core.TextMetaTickList,
        core.TextMetaQuarterList,
        core.TextMetaSecondList,
    )

    def __call__(
        self,
        time: smt.TimeDtype,
        text: str,
        ttype: smt.GeneralTimeUnit = "tick",
    ) -> smt.TextMeta:
        return self.__core_classes.dispatch(ttype)(time, text)  # type: ignore

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, self.__core_classes)  # type: ignore

    def from_numpy(
        self,
        time: ndarray,
        text: ndarray,
        ttype: smt.GeneralTimeUnit = "tick",
    ) -> smt.GeneralTextMetaList:
        """Not supported for text events; raises ``NotImplementedError``."""
        raise NotImplementedError


@dataclass(frozen=True)
class TrackFactory:
    """``Track(name, program, is_drum, notes, ..., ttype="tick")`` creates a track."""

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
        ttype: smt.GeneralTimeUnit = "tick",
    ) -> smt.Track:
        """Create a track. The event lists are copied into it."""
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

    def empty(self, ttype: smt.GeneralTimeUnit = "tick") -> smt.Track:
        """Return an empty track specialized for the requested time unit."""
        return self.__core_classes.dispatch(ttype)()


@dataclass(frozen=True)
class ScoreFactory:
    """``Score(x=960, ttype="tick", ...)`` loads or creates a score."""

    __core_classes = CoreClasses(core.ScoreTick, core.ScoreQuarter, core.ScoreSecond)

    def __call__(
        self,
        x: int | str | Path | smt.Score = 960,
        ttype: smt.GeneralTimeUnit = "tick",
        fmt: str | None = None,
        sanitize_data: bool = False,
        min_dur: float | None = None,
    ) -> smt.Score:
        """Load or create a score in the unit ``ttype``.

        ``x`` is a path to a MIDI or ABC file (see ``from_file``), the ticks per
        quarter note of an empty score, or a score to copy and convert (see
        ``from_other``).
        """
        if isinstance(x, (str, Path)):
            return self.from_file(x, ttype, fmt, sanitize_data=sanitize_data)
        if sanitize_data:
            msg = "sanitize_data is only supported when loading MIDI files from path"
            raise ValueError(msg)
        if isinstance(x, int):
            return self.from_tpq(x, ttype)
        if isinstance(x, self):  # type: ignore
            return self.from_other(x, ttype, min_dur)
        msg = f"Invalid type: {type(x)}"
        raise TypeError(msg)

    def from_file(
        self,
        path: str | Path,
        ttype: smt.GeneralTimeUnit = "tick",
        fmt: str | None = None,
        sanitize_data: bool = False,
        format: str | None = None,  # noqa: A002  public keyword argument
    ) -> smt.Score:
        """Read a MIDI or ABC file.

        The extension selects the format (``.mid``, ``.midi``, ``.MID``, ``.MIDI``
        or ``.abc`` in any case) unless ``fmt`` or its alias ``format`` names it (``"midi"``
        or ``"abc"``). ``sanitize_data`` applies to MIDI only; see ``from_midi``.
        """
        if format is not None:
            if fmt is not None and fmt != format:
                msg = "fmt and format must match when both are provided"
                raise ValueError(msg)
            fmt = format
        if isinstance(path, str):
            path = Path(path)
        if not path.is_file():
            raise ValueError(_ := f"{path} is not a file")
        return self.__core_classes.dispatch(ttype).from_file(path, fmt, sanitize_data)

    def from_midi(
        self,
        data: bytes,
        ttype: smt.GeneralTimeUnit = "tick",
        sanitize_data: bool = False,
    ) -> smt.Score:
        """Read MIDI bytes.

        A channel message data byte above 127 raises ``RuntimeError``; with
        ``sanitize_data`` it is clamped to 127 instead.
        """
        return self.__core_classes.dispatch(ttype).from_midi(data, sanitize_data)

    def from_abc(
        self,
        abc: str,
        ttype: smt.GeneralTimeUnit = "tick",
    ) -> smt.Score:
        """Read the first tune from ABC text as performed music.

        Parameters
        ----------
        abc : str
            ABC notation, not a file path. Use ``Score(path)`` to load a file.
        ttype : {'tick', 'quarter', 'second'} or TimeUnit, default 'tick'
            Time unit for the returned score. Unit names are case-insensitive.

        Returns
        -------
        Score
            A new score in the requested unit. See the core ``from_abc``
            method for the rendering and diagnostic contract.

        Raises
        ------
        ValueError
            If the time unit is invalid or the tune cannot be rendered.
        """
        return self.__core_classes.dispatch(ttype).from_abc(abc)

    def from_tpq(
        self,
        tpq: int = 960,
        ttype: smt.GeneralTimeUnit = "tick",
    ) -> smt.Score:
        """Create an empty score with ``tpq`` ticks per quarter note."""
        return self.__core_classes.dispatch(ttype)(tpq)

    def from_other(
        self,
        other: smt.Score,
        ttype: smt.GeneralTimeUnit = "tick",
        min_dur: float | None = None,
    ) -> smt.Score:
        """Return a deep copy of ``other`` in ``ttype``; ``min_dur`` works as in ``Score.to``."""
        if other.ticks_per_quarter <= 0:
            msg = (
                f"ticks_per_quarter must be positive, but got {other.ticks_per_quarter}"
            )
            raise ValueError(
                msg,
            )
        return other.to(ttype, min_dur)

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, self.__core_classes)  # type: ignore


class SynthesizerFactory:
    """``Synthesizer(sf_path=None, sample_rate=44100, quality=0)`` loads a SoundFont."""

    def __call__(
        self,
        sf_path: str | Path | None = None,
        sample_rate: int = 44100,
        quality: int = 0,
    ):
        """Create a synthesizer that renders scores with a SoundFont.

        Parameters
        ----------
        sf_path : str | pathlib.Path | None, default None
            Path to an SF2 or SF3 file. When ``None``, MuseScore General
            (``BuiltInSF3.MuseScoreGeneral``, about 38 MB) is downloaded once into
            the user data directory.
        sample_rate : int, default 44100
            Output sample rate in Hz.
        quality : int, default 0
            Resampling of SoundFont samples to ``sample_rate``, 0-4: 0 is linear
            interpolation, 1 zero-order hold, 2-4 sinc from fastest to best.
            Other values raise ``ValueError``.

        Returns
        -------
        symusic.core.Synthesizer
        """
        if sf_path is None:
            sf_path = BuiltInSF3.MuseScoreGeneral().path(download=True)
        sf_path = str(sf_path)
        return core.Synthesizer(sf_path, sample_rate, quality)

    def __instancecheck__(self, instance) -> bool:
        return isinstance(instance, core.Synthesizer)


Note = NoteFactory()
KeySignature = KeySignatureFactory()
TimeSignature = TimeSignatureFactory()
ControlChange = ControlChangeFactory()
Tempo = TempoFactory()
Pedal = PedalFactory()
PitchBend = PitchBendFactory()
NoteTuning = NoteTuningFactory()
TextMeta = TextMetaFactory()
Track = TrackFactory()
Score = ScoreFactory()
Synthesizer = SynthesizerFactory()
