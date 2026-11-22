"""Deterministic micro-benchmarks for the installed ``symusic`` package.

Usage: python scenarios.py [--samples N] [--json] [--only SUBSTRING]

Each scenario repeats a short operation so that one sample takes roughly
10-50 ms. The reported figure is the sample time divided by the repeat count,
in microseconds per operation. Inputs come from ``tests/testcases/``. The
script uses ``time.perf_counter_ns`` and the standard library, plus NumPy for
the array scenarios.
"""

from __future__ import annotations

import argparse
import gc
import json
import pickle
import statistics
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import TYPE_CHECKING

import symusic

if TYPE_CHECKING:
    from collections.abc import Callable

CASES = Path(__file__).resolve().parent.parent / "tests" / "testcases"
MULTITRACK = CASES / "Multitrack_MIDIs" / "Aicha.mid"
PIANO = CASES / "One_track_MIDIs" / "Maestro_6.mid"
ABC = CASES / "abc_files" / "the_wheels_of_the_world.abc"


@dataclass(frozen=True)
class Scenario:
    name: str
    reps: int
    setup: Callable[[], Callable[[], object]]


SCENARIOS: list[Scenario] = []


def scenario(name: str, reps: int):
    """Register ``setup() -> run``; ``run()`` is one timed operation."""

    def register(setup: Callable[[], Callable[[], object]]):
        SCENARIOS.append(Scenario(name, reps, setup))
        return setup

    return register


def _load(path: Path):
    return symusic.Score(path)


@scenario("midi_parse_path", 3)
def _():
    return lambda: symusic.Score(PIANO)


@scenario("midi_parse_bytes", 3)
def _():
    data = PIANO.read_bytes()
    return lambda: symusic.Score.from_midi(data)


@scenario("note_attribute_access", 60)
def _():
    notes = list(_load(PIANO).tracks[0].notes)[:4096]
    return lambda: sum(note.time for note in notes)


@scenario("note_list_iteration", 60)
def _():
    notes = _load(PIANO).tracks[0].notes[:4096]
    return lambda: list(notes)


@scenario("midi_dump_bytes", 4)
def _():
    score = _load(PIANO)
    return lambda: score.to_midi()


@scenario("abc_parse", 10)
def _():
    text = ABC.read_text(encoding="utf-8")
    return lambda: symusic.Score.from_abc(text)


@scenario("to_quarter", 100)
def _():
    score = _load(PIANO)
    return lambda: score.to(getattr(symusic, "Quarter", "quarter"))


@scenario("to_second", 60)
def _():
    score = _load(PIANO)
    return lambda: score.to(getattr(symusic, "Second", "second"))


@scenario("quarter_to_tick", 20)
def _():
    score = _load(PIANO).to(getattr(symusic, "Quarter", "quarter"))
    return lambda: score.to(getattr(symusic, "Tick", "tick"))


@scenario("second_to_tick", 15)
def _():
    score = _load(PIANO).to(getattr(symusic, "Second", "second"))
    return lambda: score.to(getattr(symusic, "Tick", "tick"))


@scenario("resample", 20)
def _():
    score = _load(PIANO)
    return lambda: score.resample(96)


@scenario("shift_time", 100)
def _():
    score = _load(PIANO)
    return lambda: score.shift_time(480)


@scenario("clip", 100)
def _():
    score = _load(PIANO)
    end = score.end() // 2
    return lambda: score.clip(end // 4, end)


@scenario("trim", 100)
def _():
    track = _load(PIANO).tracks[0]
    end = track.end() // 2
    return lambda: track.trim(
        end // 4, end, min_overlap=1, start_mode="truncate", end_mode="truncate"
    )


@scenario("adjust_time", 20)
def _():
    score = _load(PIANO)
    end = score.end()
    original = [0, end // 3, end]
    adjusted = [0, end // 4, end]
    return lambda: score.copy().adjust_time(original, adjusted)


@scenario("sort", 40)
def _():
    score = _load(PIANO)
    return lambda: score.copy().sort()


@scenario("track_shift_clip_sort", 2000)
def _():
    track = _load(MULTITRACK).tracks[0]
    end = track.end() // 2

    def run():
        shifted = track.copy()
        shifted.shift_time(240)
        clipped = shifted.clip(0, end)
        clipped.sort()
        return clipped

    return run


@scenario("notes_to_numpy", 60)
def _():
    notes = _load(PIANO).tracks[0].notes
    return lambda: notes.to_numpy()


@scenario("notes_from_numpy", 150)
def _():
    arrays = _load(PIANO).tracks[0].notes.to_numpy()
    return lambda: symusic.NoteList.from_numpy(**arrays)


@scenario("pianoroll", 1)
def _():
    score = _load(PIANO).resample(24)
    return lambda: score.pianoroll(modes=["frame", "onset"], encode_velocity=True)


@scenario("pickle_roundtrip", 30)
def _():
    score = _load(PIANO)
    return lambda: pickle.loads(pickle.dumps(score, protocol=pickle.HIGHEST_PROTOCOL))


def measure(sc: Scenario, samples: int) -> list[float]:
    """Return per-operation times in microseconds, one value per sample."""
    run = sc.setup()
    for _ in range(max(2, sc.reps // 5)):  # warm caches and allocators
        run()
    out = []
    gc.collect()
    gc.disable()
    try:
        for _ in range(samples):
            start = time.perf_counter_ns()
            for _ in range(sc.reps):
                run()
            out.append((time.perf_counter_ns() - start) / sc.reps / 1e3)
    finally:
        gc.enable()
    return out


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--samples", type=int, default=7)
    parser.add_argument(
        "--only", default="", help="run scenarios whose name contains this"
    )
    parser.add_argument("--json", action="store_true", help="print raw samples as JSON")
    args = parser.parse_args(argv)

    results = {
        sc.name: measure(sc, args.samples) for sc in SCENARIOS if args.only in sc.name
    }
    if args.json:
        json.dump({"version": symusic.__version__, "samples_us": results}, sys.stdout)
        print()
        return 0
    print(f"symusic {symusic.__version__}; per-operation time, {args.samples} samples")
    print(f"{'scenario':<24}{'median us':>12}{'min us':>12}{'spread':>9}")
    for name, xs in results.items():
        med = statistics.median(xs)
        print(
            f"{name:<24}{med:>12.1f}{min(xs):>12.1f}{(max(xs) - min(xs)) / med:>8.1%}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
