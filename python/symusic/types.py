try:
    from typing import Protocol, TypeVar
except ImportError:
    from typing_extensions import Protocol, TypeVar

from typing import List, Union

from . import core

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
    "PyVec",
]

TimeUnit = TypeVar("TimeUnit", core.Tick, core.Quarter, core.Second)
GeneralTimeUnit = Union[TimeUnit, str]
TimeDtype = Union[int, float]

Note = core.Note
KeySignature = core.KeySignature
TimeSignature = core.TimeSignature
ControlChange = core.ControlChange
Tempo = core.Tempo
Pedal = core.Pedal
PitchBend = core.PitchBend
TextMeta = core.TextMeta
Track = core.Track
Score = core.Score
PyVec = core.PyVec

NoteList = Union[core.NoteTickList, core.NoteQuarterList, core.NoteSecondList]
KeySignatureList = Union[
    core.KeySignatureTickList,
    core.KeySignatureQuarterList,
    core.KeySignatureSecondList,
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
    core.PitchBendTickList,
    core.PitchBendQuarterList,
    core.PitchBendSecondList,
]
TextMetaList = Union[
    core.TextMetaTickList,
    core.TextMetaQuarterList,
    core.TextMetaSecondList,
]
TrackList = Union[core.TrackTickList, core.TrackQuarterList, core.TrackSecondList]

GeneralNoteList = Union[NoteList, List[core.Note]]
GeneralKeySignatureList = Union[KeySignatureList, List[core.KeySignature]]
GeneralTimeSignatureList = Union[TimeSignatureList, List[core.TimeSignature]]
GeneralControlChangeList = Union[ControlChangeList, List[core.ControlChange]]
GeneralTempoList = Union[TempoList, List[core.Tempo]]
GeneralPedalList = Union[PedalList, List[core.Pedal]]
GeneralPitchBendList = Union[PitchBendList, List[core.PitchBend]]
GeneralTextMetaList = Union[TextMetaList, List[core.TextMeta]]
GeneralTrackList = Union[TrackList, List[core.Track]]
