from typing import TypeVar

from . import core

__all__ = [
    "ControlChange",
    "GeneralTimeUnit",
    "KeySignature",
    "Note",
    "NoteTuning",
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
GeneralTimeUnit = TimeUnit | str
TimeDtype = int | float

Note = core.Note
KeySignature = core.KeySignature
TimeSignature = core.TimeSignature
ControlChange = core.ControlChange
Tempo = core.Tempo
Pedal = core.Pedal
PitchBend = core.PitchBend
NoteTuning = core.NoteTuning
TextMeta = core.TextMeta
Track = core.Track
Score = core.Score
PyVec = core.PyVec

NoteList = core.NoteTickList | core.NoteQuarterList | core.NoteSecondList
KeySignatureList = (
    core.KeySignatureTickList
    | core.KeySignatureQuarterList
    | core.KeySignatureSecondList
)
TimeSignatureList = (
    core.TimeSignatureTickList
    | core.TimeSignatureQuarterList
    | core.TimeSignatureSecondList
)
ControlChangeList = (
    core.ControlChangeTickList
    | core.ControlChangeQuarterList
    | core.ControlChangeSecondList
)
TempoList = core.TempoTickList | core.TempoQuarterList | core.TempoSecondList
PedalList = core.PedalTickList | core.PedalQuarterList | core.PedalSecondList
PitchBendList = (
    core.PitchBendTickList | core.PitchBendQuarterList | core.PitchBendSecondList
)
TextMetaList = (
    core.TextMetaTickList | core.TextMetaQuarterList | core.TextMetaSecondList
)
TrackList = core.TrackTickList | core.TrackQuarterList | core.TrackSecondList

GeneralNoteList = NoteList | list[core.Note]
GeneralKeySignatureList = KeySignatureList | list[core.KeySignature]
GeneralTimeSignatureList = TimeSignatureList | list[core.TimeSignature]
GeneralControlChangeList = ControlChangeList | list[core.ControlChange]
GeneralTempoList = TempoList | list[core.Tempo]
GeneralPedalList = PedalList | list[core.Pedal]
GeneralPitchBendList = PitchBendList | list[core.PitchBend]
GeneralTextMetaList = TextMetaList | list[core.TextMeta]
GeneralTrackList = TrackList | list[core.Track]
