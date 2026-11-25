import runpy
from pathlib import Path

import pytest
from symusic import Note, Score, Track

SCENARIOS = Path(__file__).resolve().parents[1] / "benchmarks" / "scenarios.py"


@pytest.fixture
def scenario_runs(monkeypatch):
    source = Score()
    source.tracks.append(
        Track(
            notes=[Note(100, 20, 62, 80), Note(60, 20, 60, 80), Note(680, 40, 64, 80)]
        )
    )
    scenarios = runpy.run_path(str(SCENARIOS))
    monkeypatch.setitem(scenarios["_load"].__globals__, "_load", lambda path: source)
    return source, {
        scenario.name: scenario.setup for scenario in scenarios["SCENARIOS"]
    }


def test_midi_export(scenario_runs):
    _, scenarios = scenario_runs
    data = scenarios["midi_dump_bytes"]()()
    assert data.startswith(b"MThd")
    notes = Score.from_midi(data).tracks[0].notes
    assert [note.pitch for note in notes] == [60, 62, 64]


def test_numpy_export(scenario_runs):
    _, scenarios = scenario_runs
    arrays = scenarios["notes_to_numpy"]()()
    assert arrays["time"].tolist() == [100, 60, 680]
    assert arrays["pitch"].tolist() == [62, 60, 64]


def test_numpy_import(scenario_runs):
    source, scenarios = scenario_runs
    assert scenarios["notes_from_numpy"]()() == source.tracks[0].notes


@pytest.mark.parametrize(
    ("name", "expected"),
    [
        ("shift_time", [580, 540, 1160]),
        ("adjust_time", [75, 45, 675]),
        ("sort", [60, 100, 680]),
        ("track_shift_clip_sort", [300, 340]),
    ],
)
def test_editing(scenario_runs, name, expected):
    source, scenarios = scenario_runs
    run = scenarios[name]()
    for _ in range(2):
        result = run()
        track = result.tracks[0] if isinstance(result, Score) else result
        assert [note.time for note in track.notes] == expected
        assert [note.time for note in source.tracks[0].notes] == [100, 60, 680]
