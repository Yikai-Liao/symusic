from __future__ import annotations

import pickle

import pytest
from symusic import (
    ControlChange,
    KeySignature,
    Note,
    Pedal,
    PitchBend,
    Score,
    Tempo,
    TextMeta,
    TimeSignature,
    Track,
)


@pytest.fixture(params=["tick", "quarter", "second"])
def ttype(request: pytest.FixtureRequest) -> str:
    return request.param


def _time_pair(ttype: str) -> tuple[int | float, int | float]:
    if ttype == "tick":
        return 480, 240
    return 1.25, 0.5


def _event_cases(ttype: str) -> list[tuple[str, object]]:
    time, duration = _time_pair(ttype)
    return [
        ("note", Note(time, duration, 60, 96, ttype)),
        ("control_change", ControlChange(time, 64, 127, ttype)),
        ("pitch_bend", PitchBend(time, 4096, ttype)),
        ("pedal", Pedal(time, duration, ttype)),
        ("text_meta", TextMeta(time, "marker", ttype)),
        ("tempo", Tempo(time, mspq=500000, ttype=ttype)),
        ("time_signature", TimeSignature(time, 7, 8, ttype)),
        ("key_signature", KeySignature(time, -3, 1, ttype)),
    ]


def _build_track(ttype: str):
    time, duration = _time_pair(ttype)
    track = Track("Pickle Track", 8, True, ttype=ttype)
    track.notes.append(Note(time, duration, 60, 96, ttype))
    track.notes.append(Note(time + duration, duration, 64, 88, ttype))
    track.controls.append(ControlChange(time, 64, 127, ttype))
    track.pitch_bends.append(PitchBend(time + duration, 2048, ttype))
    track.pedals.append(Pedal(time, duration * 2, ttype))
    track.lyrics.append(TextMeta(time + duration, "lyric", ttype))
    return track


def _build_score(ttype: str):
    score = Score(960, ttype=ttype)
    score.tracks.append(_build_track(ttype))

    other_track = Track("Backing", 32, False, ttype=ttype)
    time, duration = _time_pair(ttype)
    other_track.notes.append(Note(time * 2, duration, 67, 72, ttype))
    score.tracks.append(other_track)

    score.tempos.append(Tempo(time, mspq=500000, ttype=ttype))
    score.time_signatures.append(TimeSignature(time, 7, 8, ttype))
    score.key_signatures.append(KeySignature(time, -3, 1, ttype))
    score.markers.append(TextMeta(time + duration, "section-a", ttype))
    return score


def _assert_pickle_roundtrip(obj: object) -> object:
    restored = pickle.loads(pickle.dumps(obj, protocol=pickle.HIGHEST_PROTOCOL))
    assert type(restored) is type(obj)
    assert restored == obj
    return restored


@pytest.mark.parametrize("label,event", _event_cases("tick"))
def test_pickle_event_roundtrip_tick(label: str, event: object):
    del label
    restored = _assert_pickle_roundtrip(event)
    assert restored == event
    assert restored.ttype == event.ttype


@pytest.mark.parametrize("ttype", ["quarter", "second"])
def test_pickle_event_roundtrip_float_ttypes(ttype: str):
    for _, event in _event_cases(ttype):
        restored = _assert_pickle_roundtrip(event)
        assert restored == event
        assert restored.ttype == event.ttype


def test_pickle_event_lists_roundtrip(ttype: str):
    track = _build_track(ttype)
    event_lists = [
        track.notes,
        track.controls,
        track.pitch_bends,
        track.pedals,
        track.lyrics,
    ]

    for event_list in event_lists:
        restored = _assert_pickle_roundtrip(event_list)
        assert restored == event_list
        assert restored.ttype == event_list.ttype


def test_pickle_track_roundtrip(ttype: str):
    track = _build_track(ttype)
    restored = _assert_pickle_roundtrip(track)

    assert restored == track
    assert restored.ttype == track.ttype
    assert restored.notes == track.notes
    assert restored.controls == track.controls
    assert restored.pitch_bends == track.pitch_bends
    assert restored.pedals == track.pedals
    assert restored.lyrics == track.lyrics


def test_pickle_track_list_roundtrip(ttype: str):
    track_a = _build_track(ttype)
    track_b = Track("Harmony", 24, False, ttype=ttype)
    time, duration = _time_pair(ttype)
    track_b.notes.append(Note(time + duration, duration, 69, 80, ttype))

    score = Score(960, ttype=ttype)
    score.tracks.extend([track_a, track_b])

    restored = _assert_pickle_roundtrip(score.tracks)
    assert restored == score.tracks
    assert restored.ttype == score.tracks.ttype


def test_pickle_score_roundtrip(ttype: str):
    score = _build_score(ttype)
    restored = _assert_pickle_roundtrip(score)

    assert restored == score
    assert restored.ttype == score.ttype
    assert restored.tracks == score.tracks
    assert restored.tempos == score.tempos
    assert restored.time_signatures == score.time_signatures
    assert restored.key_signatures == score.key_signatures
    assert restored.markers == score.markers
