from __future__ import annotations
from . import core
from . import typing
__all__ = ['ControlChange', 'KeySignature', 'Note', 'PitchBend', 'Quarter', 'Score', 'Second', 'Tempo', 'TextMeta', 'Tick', 'TimeSignature', 'Track', 'core', 'typing']
class Quarter:
    def __init__(self) -> None:
        ...
    def __repr__(self) -> str:
        ...
class Second:
    def __init__(self) -> None:
        ...
    def __repr__(self) -> str:
        ...
class Tick:
    def __init__(self) -> None:
        ...
    def __repr__(self) -> str:
        ...
@typing.overload
def ControlChange(time: int, number: int, value: int, time_unit: Tick) -> core.ControlChangeTick:
    ...
@typing.overload
def ControlChange(time: float, number: int, value: int, time_unit: Quarter) -> core.ControlChangeQuarter:
    ...
@typing.overload
def ControlChange(time: float, number: int, value: int, time_unit: Second) -> core.ControlChangeSecond:
    ...
@typing.overload
def ControlChange(time: typing.Any, number: int, value: int, time_unit: str) -> typing.Any:
    """
    Create a ControlChange object from time, number, value and time_unit
    """
@typing.overload
def KeySignature(time: int, key: int, tonality: int, time_unit: Tick) -> core.KeySignatureTick:
    ...
@typing.overload
def KeySignature(time: float, key: int, tonality: int, time_unit: Quarter) -> core.KeySignatureQuarter:
    ...
@typing.overload
def KeySignature(time: float, key: int, tonality: int, time_unit: Second) -> core.KeySignatureSecond:
    ...
@typing.overload
def KeySignature(time: typing.Any, key: int, tonality: int, time_unit: str) -> typing.Any:
    """
    Create a KeySignature object from time, key, tonality and time_unit
    """
@typing.overload
def Note(time: int, duration: int, pitch: int, velocity: int, time_unit: Tick) -> core.NoteTick:
    ...
@typing.overload
def Note(time: float, duration: float, pitch: int, velocity: int, time_unit: Quarter) -> core.NoteQuarter:
    ...
@typing.overload
def Note(time: float, duration: float, pitch: int, velocity: int, time_unit: Second) -> core.NoteSecond:
    ...
@typing.overload
def Note(time: typing.Any, duration: typing.Any, pitch: int, velocity: int, time_unit: str) -> typing.Any:
    """
    Create a Note object from time, duration, pitch, velocity and time_unit
    """
@typing.overload
def PitchBend(time: int, value: int, time_unit: Tick) -> core.PitchBendTick:
    ...
@typing.overload
def PitchBend(time: float, value: int, time_unit: Quarter) -> core.PitchBendQuarter:
    ...
@typing.overload
def PitchBend(time: float, value: int, time_unit: Second) -> core.PitchBendSecond:
    ...
@typing.overload
def PitchBend(time: typing.Any, value: int, time_unit: str) -> typing.Any:
    """
    Create a PitchBend object from time, value and time_unit
    """
@typing.overload
def Score(filename: str) -> core.ScoreTick:
    ...
@typing.overload
def Score(filename: str, time_unit: Tick) -> core.ScoreTick:
    ...
@typing.overload
def Score(filename: str, time_unit: Quarter) -> core.ScoreQuarter:
    ...
@typing.overload
def Score(filename: str, time_unit: str) -> typing.Any:
    """
    Create a Score object from filename and time_unit
    """
@typing.overload
def Score() -> core.ScoreTick:
    ...
@typing.overload
def Score(time_unit: Tick) -> core.ScoreTick:
    ...
@typing.overload
def Score(time_unit: Quarter) -> core.ScoreQuarter:
    ...
@typing.overload
def Tempo(time: int, qpm: float, time_unit: Tick) -> core.TempoTick:
    ...
@typing.overload
def Tempo(time: float, qpm: float, time_unit: Quarter) -> core.TempoQuarter:
    ...
@typing.overload
def Tempo(time: float, qpm: float, time_unit: Second) -> core.TempoSecond:
    ...
@typing.overload
def Tempo(time: typing.Any, qpm: float, time_unit: str) -> typing.Any:
    """
    Create a Tempo object from time, qpm and time_unit
    """
@typing.overload
def TextMeta(time: int, text: str, time_unit: Tick) -> core.TextMetaTick:
    ...
@typing.overload
def TextMeta(time: float, text: str, time_unit: Quarter) -> core.TextMetaQuarter:
    ...
@typing.overload
def TextMeta(time: float, text: str, time_unit: Second) -> core.TextMetaSecond:
    ...
@typing.overload
def TextMeta(time: typing.Any, text: str, time_unit: str) -> typing.Any:
    """
    Create a TextMeta object from time, text and time_unit
    """
@typing.overload
def TimeSignature(time: int, numerator: int, denominator: int, time_unit: Tick) -> core.TimeSignatureTick:
    ...
@typing.overload
def TimeSignature(time: float, numerator: int, denominator: int, time_unit: Quarter) -> core.TimeSignatureQuarter:
    ...
@typing.overload
def TimeSignature(time: float, numerator: int, denominator: int, time_unit: Second) -> core.TimeSignatureSecond:
    ...
@typing.overload
def TimeSignature(time: typing.Any, numerator: int, denominator: int, time_unit: str) -> typing.Any:
    """
    Create a TimeSignature object from time, numerator, denominator and time_unit
    """
@typing.overload
def Track(name: str, program: int, is_drum: bool, time_unit: Tick) -> core.TrackTick:
    ...
@typing.overload
def Track(name: str, program: int, is_drum: bool, time_unit: Quarter) -> core.TrackQuarter:
    ...
@typing.overload
def Track(name: str, program: int, is_drum: bool, time_unit: Second) -> core.TrackSecond:
    ...
@typing.overload
def Track(name: str, program: int, is_drum: bool, time_unit: str) -> typing.Any:
    """
    Create a Track object from name, program, is_drum and time_unit
    """
__version__: str = '0.1.0'
