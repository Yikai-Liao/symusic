from typing import TypeVar, Union

from . import core

__all__ = [
    "ControlChange",
    "GeneralTimeUnit",
    "KeySignature",
    "Note",
    "Pedal",
    "PitchBend",
    "PyVec",
    "Score",
    "Tempo",
    "TextMeta",
    "TimeDtype",
    "TimeSignature",
    "TimeUnit",
    "Track",
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

GeneralNoteList = Union[NoteList, list[core.Note]]
GeneralKeySignatureList = Union[KeySignatureList, list[core.KeySignature]]
GeneralTimeSignatureList = Union[TimeSignatureList, list[core.TimeSignature]]
GeneralControlChangeList = Union[ControlChangeList, list[core.ControlChange]]
GeneralTempoList = Union[TempoList, list[core.Tempo]]
GeneralPedalList = Union[PedalList, list[core.Pedal]]
GeneralPitchBendList = Union[PitchBendList, list[core.PitchBend]]
GeneralTextMetaList = Union[TextMetaList, list[core.TextMeta]]
GeneralTrackList = Union[TrackList, list[core.Track]]
