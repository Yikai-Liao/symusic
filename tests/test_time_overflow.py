"""Tick arithmetic and float-to-tick conversion at the edges of the int32 range."""

from __future__ import annotations

import math

import pytest
from symusic import (
    ControlChange,
    KeySignature,
    Note,
    Pedal,
    PitchBend,
    Quarter,
    Score,
    Second,
    Tempo,
    TextMeta,
    Tick,
    TimeSignature,
    Track,
)

from tests.test_malformed_midi import build_single_track_midi

INT_MAX = 2**31 - 1
INT_MIN = -(2**31)


def tick_score(*notes: Note) -> Score:
    score = Score(ticks_per_quarter=480)
    score.tracks.append(Track(notes=list(notes), ttype=Tick))
    return score


def test_end_overflow():
    assert Note(INT_MAX - 5, 5, 60, 80, ttype=Tick).end == INT_MAX
    with pytest.raises(OverflowError):
        _ = Note(INT_MAX - 5, 6, 60, 80, ttype=Tick).end
    with pytest.raises(OverflowError):
        _ = Pedal(INT_MAX, 1, ttype=Tick).end


@pytest.mark.parametrize("ttype", [Quarter, Second])
def test_end_float_units_do_not_raise(ttype: str):
    assert Note(3.0e38, 3.0e38, 60, 80, ttype=ttype).end == math.inf


def test_end_setter_overflow():
    note = Note(INT_MIN, 1, 60, 80, ttype=Tick)
    with pytest.raises(OverflowError):
        note.end = INT_MAX
    assert note.duration == 1


@pytest.mark.parametrize("make", [Note, Pedal])
def test_shift_time_overflow(make):
    args = (INT_MAX, 1, 60, 80) if make is Note else (INT_MAX, 1)
    event = make(*args, ttype=Tick)
    with pytest.raises(OverflowError):
        event.shift_time(1)
    assert event.time == INT_MAX
    assert event.shift_time(-1) is None
    assert event.time == INT_MAX - 1


def test_shift_time_event_is_applied():
    note = Note(10, 5, 60, 80, ttype=Tick)
    assert note.shift_time(5) is None
    assert note.time == 15


def test_track_score_overflow():
    note = Note(INT_MAX, 1, 60, 80, ttype=Tick)
    score = tick_score(note)
    with pytest.raises(OverflowError):
        score.tracks[0].end()
    with pytest.raises(OverflowError):
        score.end()
    with pytest.raises(OverflowError):
        score.shift_time(1)
    with pytest.raises(OverflowError):
        score.to(Second)
    with pytest.raises(OverflowError):
        score.to_midi()


def test_clip_trim_extreme():
    track = Track(
        notes=[
            Note(INT_MAX - 10, 100, 60, 80, ttype=Tick),
            Note(0, 10, 60, 80, ttype=Tick),
        ],
        ttype=Tick,
    )
    clipped = track.clip(0, INT_MAX, clip_end=True)
    assert [n.time for n in clipped.notes] == [0]

    trimmed = track.trim(0, INT_MAX, start_mode="retain", end_mode="truncate")
    assert sorted((n.time, n.duration) for n in trimmed.notes) == [
        (0, 10),
        (INT_MAX - 10, 10),
    ]

    assert track.trim(0, INT_MAX).notes[0].time == 0
    assert len(track.trim(0, INT_MAX).notes) == 1


def test_adjust_time_overflow():
    track = Track(notes=[Note(0, 1, 60, 80, ttype=Tick)], ttype=Tick)
    with pytest.raises(OverflowError):
        track.adjust_time([0, 1], [INT_MIN, INT_MAX])


def test_adjust_time_anchors_at_tick_limits():
    track = Track(notes=[Note(0, 0, 60), Note(1, 0, 62)])
    track.adjust_time([0, 1], [INT_MIN, INT_MAX])
    assert [(note.time, note.duration) for note in track.notes] == [
        (INT_MIN, 0),
        (INT_MAX, 0),
    ]


@pytest.mark.parametrize("ttype", [Quarter, Second])
def test_adjust_time_wide_float_anchors(ttype):
    score = Score(ticks_per_quarter=480, ttype=ttype)
    control = ControlChange(2e38, 1, 64, ttype=ttype)
    score.tracks.append(Track(controls=[control], ttype=ttype))

    anchors = [-3e38, 3e38]
    expected = control.time
    score.adjust_time(anchors, anchors)
    assert score.tracks[0].controls[0].time == expected


@pytest.mark.parametrize("ttype", [Quarter, Second])
@pytest.mark.parametrize("target", ["score", "tracks"])
def test_adjust_time_float_duration_overflow(ttype, target):
    score = Score(ticks_per_quarter=480, ttype=ttype)
    score.tracks.append(
        Track(controls=[ControlChange(0.25, 1, 64, ttype=ttype)], ttype=ttype)
    )
    score.tracks.append(Track(notes=[Note(0, 1, 60, 80, ttype=ttype)], ttype=ttype))
    receiver = score if target == "score" else score.tracks
    control = score.tracks[0].controls[0]
    note = score.tracks[1].notes[0]

    with pytest.raises(
        ValueError, match="adjusted duration must be finite and in range, got "
    ):
        receiver.adjust_time([0, 1], [-3e38, 3e38])
    assert [control.time for control in score.tracks[0].controls] == [0.25]
    assert [(note.time, note.duration) for note in score.tracks[1].notes] == [(0, 1)]

    assert receiver.adjust_time([0, 1], [2, 4]) is None
    adjusted = score.tracks
    assert [control.time for control in adjusted[0].controls] == [2.5]
    assert [(note.time, note.duration) for note in adjusted[1].notes] == [(2, 2)]
    assert control.time == 2.5
    assert (note.time, note.duration) == (2, 2)


def test_adjust_time_shared_track():
    score = tick_score(Note(2, 1, 60, 80))
    track = score.tracks[0]
    score.tracks.append(track)
    note = track.notes[0]

    score.tracks.adjust_time([0, 10], [1, 11])

    assert [track.notes[0].time for track in score.tracks] == [3, 3]
    assert note.time == 3


def _spacer() -> bytes:
    # Maximal 28-bit delta (0x0FFFFFFF) followed by an empty text meta event.
    return bytes([0xFF, 0xFF, 0xFF, 0x7F, 0xFF, 0x01, 0x00])


def test_midi_event_time_overflow():
    # Nine maximal deltas pass INT32_MAX.
    body = _spacer() * 9 + bytes([0x00, 0xFF, 0x2F, 0x00])
    with pytest.raises(RuntimeError, match="32-bit tick range"):
        Score.from_midi(build_single_track_midi(body))


def test_midi_duration_in_range():
    # 7 * 0x0FFFFFFF < INT32_MAX: a note spanning most of the range still loads.
    body = (
        bytes([0x00, 0x90, 0x3C, 0x40])
        + _spacer() * 7
        + bytes([0x00, 0x80, 0x3C, 0x40, 0x00, 0xFF, 0x2F, 0x00])
    )
    score = Score.from_midi(build_single_track_midi(body))
    assert score.tracks[0].notes[0].duration == 7 * 0x0FFFFFFF


def test_second_conversion_at_range_edges():
    score = tick_score(
        Note(INT_MAX, 0, 60, 80, ttype=Tick), Note(-480, 480, 60, 80, ttype=Tick)
    )
    score.tempos.append(Tempo(0, microseconds_per_quarter=500000))
    notes = score.to(Second).tracks[0].notes
    assert notes[1].time == pytest.approx(-0.5)
    assert notes[1].duration == pytest.approx(0.5)
    assert notes[0].duration == 0


@pytest.mark.parametrize("ttype", [Quarter, Second])
@pytest.mark.parametrize("bad", [1.0e10, -1.0e10])
def test_to_tick_out_of_range(ttype: str, bad: float):
    score = Score(ticks_per_quarter=480, ttype=ttype)
    score.tracks.append(Track(notes=[Note(bad, 1.0, 60, 80, ttype=ttype)], ttype=ttype))
    with pytest.raises(OverflowError):
        score.to(Tick)


@pytest.mark.parametrize("ttype", [Quarter, Second])
@pytest.mark.parametrize("time", [math.nan, math.inf, -math.inf])
def test_event_rejects_nonfinite_time(ttype, time):
    with pytest.raises(ValueError, match="time"):
        Note(time, 1.0, 60, 80, ttype=ttype)


def test_tempo_rejects_zero_microseconds_per_quarter():
    with pytest.raises(ValueError, match="microseconds_per_quarter"):
        Tempo(0.0, microseconds_per_quarter=0, ttype=Second)


@pytest.mark.parametrize("qpm", [0, -1, -math.inf, math.inf, math.nan, 1e300, 1e9])
def test_tempo_qpm_invalid(qpm: float):
    with pytest.raises(ValueError, match="qpm"):
        Tempo(0, qpm)
    tempo = Tempo(0, 120)
    with pytest.raises(ValueError, match="qpm"):
        tempo.qpm = qpm
    assert tempo.microseconds_per_quarter == 500000


@pytest.mark.parametrize("qpm", [1e-3, 1e-300])
def test_tempo_qpm_tiny(qpm: float):
    with pytest.raises(OverflowError):
        Tempo(0, qpm)


def test_tempo_qpm_bounds():
    assert Tempo(0, 120).microseconds_per_quarter == 500000
    assert Tempo(0, 60_000_000).microseconds_per_quarter == 1


@pytest.mark.parametrize("time", [1_000_000, -1_000_000])
def test_resample_extreme_factor(time: int):
    score = tick_score(Note(time, 10, 60, 80, ttype=Tick))
    with pytest.raises(OverflowError):
        score.resample(INT_MAX)
    assert (
        tick_score(Note(2, 480, 60, 80, ttype=Tick))
        .resample(960)
        .tracks[0]
        .notes[0]
        .time
        == 4
    )


@pytest.mark.parametrize(
    ("collection", "kind", "event"),
    [
        ("time_signatures", "time signature", TimeSignature(-2, 4, 4)),
        ("key_signatures", "key signature", KeySignature(-2, 0, False)),
        ("tempos", "tempo", Tempo(-2, 120)),
        ("markers", "marker", TextMeta(-2, "marker")),
        ("notes", "note", Note(-2, 5, 60, 80)),
        ("controls", "control change", ControlChange(-2, 64, 0)),
        ("pitch_bends", "pitch bend", PitchBend(-2, 0)),
        ("lyrics", "lyric", TextMeta(-2, "lyric")),
    ],
)
def test_midi_rejects_negative_start(collection, kind, event):
    score = tick_score()
    target = score if hasattr(score, collection) else score.tracks[0]
    getattr(target, collection).append(event)
    with pytest.raises(ValueError, match=rf"{kind} time.*-2"):
        score.to_midi()


@pytest.mark.parametrize("ttype", [Quarter, Second])
def test_midi_rejects_negative_start_before_rounding(ttype):
    score = Score(ticks_per_quarter=480, ttype=ttype)
    score.tracks.append(
        Track(notes=[Note(-0.0001, 1, 60, 80, ttype=ttype)], ttype=ttype)
    )
    with pytest.raises(ValueError, match="note time"):
        score.to_midi()


@pytest.mark.parametrize("ttype", [Tick, Quarter, Second])
def test_midi_duration_overflow(ttype):
    # The note-off follows nine valid deltas but exceeds the signed tick range.
    body = bytes.fromhex("00 90 3c 40") + _spacer() * 8
    body += bytes.fromhex("ff ff ff 7f 80 3c 40 00 ff 2f 00")
    with pytest.raises(RuntimeError, match="32-bit tick range"):
        Score.from_midi(build_single_track_midi(body), ttype=ttype)


@pytest.mark.parametrize("ttype", [Tick, Quarter, Second])
@pytest.mark.parametrize("ticks_per_quarter", [-1, 0, 32768, 65536])
def test_midi_rejects_invalid_division(ttype, ticks_per_quarter):
    score = Score(ticks_per_quarter=480, ttype=ttype)
    if ticks_per_quarter <= 0:
        with pytest.raises(ValueError, match="ticks_per_quarter"):
            score.ticks_per_quarter = ticks_per_quarter
    else:
        score.ticks_per_quarter = ticks_per_quarter
        with pytest.raises(ValueError, match=r"ticks_per_quarter.*1\.\.32767"):
            score.to_midi()


@pytest.mark.parametrize("ticks_per_quarter", [1, 32767])
def test_midi_preserves_division_limits(ticks_per_quarter):
    score = Score(ticks_per_quarter=ticks_per_quarter)
    score.tracks.append(Track(notes=[Note(0, 1, 60, 80)]))
    assert Score.from_midi(score.to_midi()).ticks_per_quarter == ticks_per_quarter


@pytest.mark.parametrize("value", [-8193, 8192, 65536, 2**80])
def test_pitch_bend_rejects_out_of_range_value(value):
    with pytest.raises(ValueError, match="value"):
        PitchBend(0, value)


def test_midi_rejects_tempo_above_24_bits():
    score = Score(ticks_per_quarter=480)
    score.tempos.append(Tempo(0, microseconds_per_quarter=1 << 24))
    with pytest.raises(ValueError, match=r"microseconds_per_quarter.*24 bits"):
        score.to_midi()


@pytest.mark.parametrize("denominator", [0, 3, 256, 2**80])
def test_denominator_requires_power_of_two(denominator):
    with pytest.raises(ValueError, match="denominator"):
        TimeSignature(0, 4, denominator)


def test_midi_rejects_unrepresentable_delta():
    score = tick_score(Note(1 << 28, 1, 60, 80))
    with pytest.raises(ValueError, match=r"event delta.*28 bits"):
        score.to_midi()


@pytest.mark.parametrize(("pitch", "velocity"), [(-1, 80), (60, -1)])
def test_midi_rejects_negative_note_data(pitch, velocity):
    with pytest.raises(ValueError, match=r"(pitch|velocity).*-1"):
        Note(0, 1, pitch, velocity)


@pytest.mark.parametrize("ttype", [Tick, Quarter, Second])
@pytest.mark.parametrize("offset", [math.nan, math.inf, -math.inf])
@pytest.mark.parametrize("kind", ["score", "tracks", "track", "notes", "note"])
def test_shift_rejects_nonfinite_offset(ttype, offset, kind):
    score = Score(ttype=ttype)
    score.tracks.append(Track(notes=[Note(1, 2, 60, 80, ttype=ttype)], ttype=ttype))
    score.tempos.append(Tempo(2, 120, ttype=ttype))
    track = score.tracks[0]
    notes = track.notes
    note = notes[0]
    tempo = score.tempos[0]
    targets = {
        "score": score,
        "tracks": score.tracks,
        "track": track,
        "notes": notes,
        "note": note,
    }
    target = targets[kind]

    with pytest.raises(ValueError, match="offset must be finite"):
        target.shift_time(offset)
    assert (note.time, tempo.time) == (1, 2)

    assert target.shift_time(3) is None
    assert note.time == notes[0].time == track.notes[0].time == 4
    assert tempo.time == (5 if kind == "score" else 2)

    if kind != "note":
        if kind in {"tracks", "notes"}:
            target.clear()
        else:
            track.notes.clear()
            score.tempos.clear()
        with pytest.raises(ValueError, match="offset must be finite"):
            target.shift_time(offset)


@pytest.mark.parametrize("ttype", [Quarter, Second])
@pytest.mark.parametrize("kind", ["score", "tracks", "track", "notes", "note"])
def test_shift_rejects_float_overflow_atomically(ttype, kind):
    score = Score(ttype=ttype)
    track = Track(
        notes=[Note(1, 2, 60, 80, ttype=ttype), Note(3e38, 2, 61, 80, ttype=ttype)],
        ttype=ttype,
    )
    score.tracks.append(track)
    notes = track.notes
    first, last = notes
    original = last.time
    targets = {
        "score": score,
        "tracks": score.tracks,
        "track": track,
        "notes": notes,
        "note": last,
    }

    with pytest.raises(ValueError, match="Shifted time must be finite"):
        targets[kind].shift_time(3e38)
    assert (first.time, last.time) == (1, original)
