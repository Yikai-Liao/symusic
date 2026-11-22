from __future__ import annotations

import pickle

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


@pytest.fixture(params=[Tick, Quarter, Second])
def ttype(request: pytest.FixtureRequest) -> type[Tick] | type[Quarter] | type[Second]:
    return request.param


def _time_pair(
    ttype: type[Tick] | type[Quarter] | type[Second],
) -> tuple[int | float, int | float]:
    if ttype == Tick:
        return 480, 240
    return 1.25, 0.5


def _event_cases(
    ttype: type[Tick] | type[Quarter] | type[Second],
) -> list[tuple[str, object]]:
    time, duration = _time_pair(ttype)
    return [
        ("note", Note(time, duration, 60, 96, ttype=ttype)),
        ("control_change", ControlChange(time, 64, 127, ttype=ttype)),
        ("pitch_bend", PitchBend(time, 4096, ttype=ttype)),
        ("pedal", Pedal(time, duration, ttype=ttype)),
        ("text_meta", TextMeta(time, "marker", ttype=ttype)),
        ("tempo", Tempo(time, mspq=500000, ttype=ttype)),
        ("time_signature", TimeSignature(time, 7, 8, ttype=ttype)),
        ("key_signature", KeySignature(time, -3, 1, ttype=ttype)),
    ]


def _build_track(ttype: type[Tick] | type[Quarter] | type[Second]):
    time, duration = _time_pair(ttype)
    track = Track("Pickle Track", 8, True, ttype=ttype)
    track.notes.append(Note(time, duration, 60, 96, ttype=ttype))
    track.notes.append(Note(time + duration, duration, 64, 88, ttype=ttype))
    track.controls.append(ControlChange(time, 64, 127, ttype=ttype))
    track.pitch_bends.append(PitchBend(time + duration, 2048, ttype=ttype))
    track.pedals.append(Pedal(time, duration * 2, ttype=ttype))
    track.lyrics.append(TextMeta(time + duration, "lyric", ttype=ttype))
    return track


def _build_score(ttype: type[Tick] | type[Quarter] | type[Second]):
    score = Score(960, ttype=ttype)
    score.tracks.append(_build_track(ttype))

    other_track = Track("Backing", 32, False, ttype=ttype)
    time, duration = _time_pair(ttype)
    other_track.notes.append(Note(time * 2, duration, 67, 72, ttype=ttype))
    score.tracks.append(other_track)

    score.tempos.append(Tempo(time, mspq=500000, ttype=ttype))
    score.time_signatures.append(TimeSignature(time, 7, 8, ttype=ttype))
    score.key_signatures.append(KeySignature(time, -3, 1, ttype=ttype))
    score.markers.append(TextMeta(time + duration, "section-a", ttype=ttype))
    return score


def _assert_pickle_roundtrip(obj: object) -> object:
    restored = pickle.loads(
        pickle.dumps(obj, protocol=pickle.HIGHEST_PROTOCOL),
    )
    assert type(restored) is type(obj)
    assert restored == obj
    return restored


@pytest.mark.parametrize(("label", "event"), _event_cases(Tick))
def test_pickle_event_roundtrip_tick(label: str, event: object):
    del label
    restored = _assert_pickle_roundtrip(event)
    assert restored == event
    assert restored.ttype == event.ttype


@pytest.mark.parametrize("ttype", [Quarter, Second])
def test_pickle_event_roundtrip_float_ttypes(
    ttype: type[Tick] | type[Quarter] | type[Second],
):
    for _, event in _event_cases(ttype):
        restored = _assert_pickle_roundtrip(event)
        assert restored == event
        assert restored.ttype == event.ttype


def test_pickle_event_lists_roundtrip(ttype: type[Tick] | type[Quarter] | type[Second]):
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


def test_pickle_track_roundtrip(ttype: type[Tick] | type[Quarter] | type[Second]):
    track = _build_track(ttype)
    restored = _assert_pickle_roundtrip(track)

    assert restored == track
    assert restored.ttype == track.ttype
    assert restored.notes == track.notes
    assert restored.controls == track.controls
    assert restored.pitch_bends == track.pitch_bends
    assert restored.pedals == track.pedals
    assert restored.lyrics == track.lyrics


def test_pickle_track_list_roundtrip(ttype: type[Tick] | type[Quarter] | type[Second]):
    track_a = _build_track(ttype)
    track_b = Track("Harmony", 24, False, ttype=ttype)
    time, duration = _time_pair(ttype)
    track_b.notes.append(Note(time + duration, duration, 69, 80, ttype=ttype))

    score = Score(960, ttype=ttype)
    score.tracks.extend([track_a, track_b])

    restored = _assert_pickle_roundtrip(score.tracks)
    assert restored == score.tracks
    assert restored.ttype == score.tracks.ttype


def test_pickle_score_roundtrip(ttype: type[Tick] | type[Quarter] | type[Second]):
    score = _build_score(ttype)
    restored = _assert_pickle_roundtrip(score)

    assert restored == score
    assert restored.ttype == score.ttype
    assert restored.tracks == score.tracks
    assert restored.tempos == score.tempos
    assert restored.time_signatures == score.time_signatures
    assert restored.key_signatures == score.key_signatures
    assert restored.markers == score.markers


def _restore_state(obj, state):
    cls = type(obj)
    cls.__new__(cls).__setstate__(state)


@pytest.mark.parametrize(
    "build", [_build_score, _build_track, lambda t: _build_track(t).notes]
)
def test_pickle_rejects_other_format_version(ttype, build):
    obj = build(ttype)
    marker, state = obj.__getstate__()
    assert state.startswith(b"SYMZ\x01")
    changed = state[:4] + b"\x02" + state[5:]
    with pytest.raises(ValueError, match=r"version 2 .*version 1"):
        _restore_state(obj, (marker, changed))


def test_pickle_rejects_invalid_state(ttype):
    obj = _build_score(ttype)
    marker, state = obj.__getstate__()
    for cut in range(len(state)):
        with pytest.raises(ValueError, match=r"Invalid ZPP pickle (header|payload)"):
            _restore_state(obj, (marker, state[:cut]))
    for invalid in (b"garbage", state + b"extra"):
        with pytest.raises(ValueError, match="Invalid ZPP pickle"):
            _restore_state(obj, (marker, invalid))
