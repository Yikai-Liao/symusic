"""Differential ABC fixtures. The upstream executable is optional and test-only."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import tempfile
from pathlib import Path

import pytest
from symusic import Note, Score, Tempo, Track

from tests.test_abc import HEADER, note_rows, roundtrip_score
from tests.utils import TESTCASES_PATH

SNAPSHOT = TESTCASES_PATH / "abc_oracle.json"
RECOVERED = {
    "rising_sun_stray_coda": (
        TESTCASES_PATH / "abc_invalid" / "rising_sun_stray_coda.abc",
        "ignored-decoration",
    ),
}
CASES = {
    "straight": HEADER + "C D E F | G A B c |",
    "rests": HEADER + "C z2 D | x E2 z |",
    "tuplets": HEADER + "(3CDE (5:4:5 FGABc |",
    "broken_rhythm": HEADER + "C>D E<F G>>A B<<c |",
    "chords_ties": HEADER + "[CEG] [D2F3A]2 | [FA]-[FA] C-C |",
    "grace": HEADER + "{DE}F {G}A B c |",
    "repeat_endings": HEADER + "C |: D E |1 F :|2 G | A |: B :|",
    "final_numbered_ending": HEADER + "|: C2 [1-2 D2 :| [3 E2 |]",
    "broken_dynamic": HEADER + "C>[K:G]!p!F G A |",
    "overlay_tie": HEADER + "C2 & E2- | D2 & E2 |",
    "implicit_repeat": HEADER + "C4 || D4 :| E4 |",
    "named_tempo": HEADER.replace("Q:1/4=120", 'Q:"Largo"').replace("L:1/4", "L:1/8")
    + "CDEFGABc|",
    "relative_tempo": HEADER + "C [L:1/8][Q:C=120] D E F|",
    "inline_tempo": HEADER + "C [Q:1/8=100] D E F |",
    "inline_key": HEADER + "C ^C c C =C c | C [K:G] F ^F f [K:Ddor] B |",
    "voices": HEADER + 'V:one name="Lead"\n%%MIDI program 4\nC D E F |\n'
    'V:two name="Bass"\n%%MIDI program 8\nC,2 D,2 |\n[V:one] G A B c |',
    "overlay": HEADER + "C D & E2 | F G A B |",
    "chord_tie_changes_pitch": HEADER + "[CE]-[DF] GA |",
    "consecutive_naturals": HEADER.replace("K:C", "K:D =F =C clef=treble =G =B")
    + "C F G B |",
    "overlay_midi_channel": HEADER + "(& C D & [I:MIDI channel 10] E F &) G A |",
    "header_midi_state": HEADER.replace(
        "K:C", "%%MIDI program 24\nV:lead\nV:bass\n%%MIDI program 8\nK:C"
    )
    + "[V:lead] CDEF|\n[V:bass] C,D,E,F,|",
    "explicit_overlay": HEADER + "C D & E2 | F (& A B & c2 &) G |",
    "overlay_clocks": HEADER + "C (& D E & c2 &) G | A (& B c & d2 &) d |",
    "directives": HEADER + "%%MIDI program 24\n%%MIDI control 7 50\n"
    "%%MIDI beat 90 80 60 2\nC D E F |\n%%MIDI program 7\n%%MIDI channel 10\nC D E F |",
    "beatstring_positions": HEADER.replace("L:1/4", "L:1/8")
    + "%%MIDI beatstring pFmPffmP\nC D E F G A B c |",
    "rhythm_ratio": HEADER + "%%MIDI ratio 2 1\nC>D E<F| G>>A B<<c|",
    "fermata_modes": HEADER + "%%MIDI fermatafixed\nHC2 D Hz |\n"
    "%%MIDI fermataproportional\nH[EG]2 F G|",
    "trill_beat": HEADER + "TCDE|TC2DE|",
    "expand_after_trim": HEADER + "%%MIDI trim 1/8\nC\n%%MIDI expand 1/8\nD E F z |",
    "pedal_decorations": HEADER + "C !ped!D E !ped-up!F | !ped(!GAB !ped)!c |",
    "breath": HEADER + "C !breath!D !breath![EG] F |",
    "midi_middle_c": HEADER + "%%MIDI C 62\nCDEF |",
    "harmony": HEADER + '"C/E"C D E F | "G7"G A B c |',
    "drums": HEADER.replace("M:4/4", "M:2/4").replace("L:1/4", "L:1/8")
    + "%%MIDI drum dd 36 42 70 80\n%%MIDI drumon\nC D E F |",
    "drum_default_velocity": HEADER
    + "%%MIDI drum dzddd 35 36 37 38 52 53\n%%MIDI drumon\nCDEF|",
    "drum_default_pitch": HEADER + "%%MIDI drum dd\n%%MIDI drumon\nCDEF|",
    "harmony_octave_inversion": HEADER
    + '%%MIDI gchord ghij\n"A/C#"ABcd|"C9/Bb"_BGEC|"C/B"B,CDE|',
    "harmony_arpeggio": HEADER
    + '%%MIDI gchord ghij\n"C5"CDEF|\n%%MIDI gchord GHIJ\n"C"CDEF|',
    "harmony_bass_only": HEADER + '"c"CDEF|"a/E"ABcd|',
    "harmony_custom_intervals": "%%MIDI chordname long 0 1 2 3 4 5 6 7 8 9\n"
    + HEADER
    + '"Clong"CDEF|',
    "harmony_extended_qualities": HEADER
    + '"Caug7"CDEF|"Fdim9"FGAB|"GM9"GABc|"Csus9"CDEF|"C7sus2"CDEF|"C7sus9"CDEF|',
    "parts": HEADER.replace("K:C", "P:ABA\nK:C") + "P:A\nC D E F |\nP:B\nG A B c |",
    "compound_minor": "X:1\nM:6/8\nQ:3/8=90\nK:Gm\nC D E F G A |",
    **{
        path.stem: path.read_text(encoding="utf-8")
        for path in sorted((TESTCASES_PATH / "abc_files").glob("*.abc"))
    },
    **{name: path.read_text(encoding="utf-8") for name, (path, _) in RECOVERED.items()},
}


def performed_track(track: Track) -> dict:
    events = {
        "program": track.program,
        "drums": track.is_drum,
        "notes": sorted([n.time, n.duration, n.pitch, n.velocity] for n in track.notes),
        "controls": [[e.time, e.number, e.value] for e in track.controls],
    }
    if track.pedals:
        events["pedals"] = [[e.time, e.duration] for e in track.pedals]
    if track.pitch_bends:
        events["bends"] = [[e.time, e.value] for e in track.pitch_bends]
    return events


def performed(score: Score) -> dict:
    """Compare music events, not MIDI track titles, ordering or file bytes."""
    return {
        "tpq": score.tpq,
        "tempos": [[event.time, event.mspq] for event in score.tempos],
        "meters": [
            [event.time, event.numerator, event.denominator]
            for event in score.time_signatures
        ],
        "keys": [
            [event.time, event.key, event.tonality] for event in score.key_signatures
        ],
        "tracks": [performed_track(track) for track in score.tracks],
    }


def oracle_score(
    executable: Path, abc: str, directory: Path, *, expected_exit: int = 0
) -> Score:
    source = directory / "sample.abc"
    output = directory / "sample.mid"
    source.write_text(abc + "\n", encoding="utf-8")
    output.unlink(missing_ok=True)
    result = subprocess.run(
        [executable.resolve(), source, "-o", output, "-silent"],
        capture_output=True,
        text=True,
        check=False,
        timeout=15,
    )
    assert result.returncode == expected_exit, result.stdout + result.stderr
    assert output.is_file(), result.stdout + result.stderr
    return Score(output)


@pytest.fixture(scope="module")
def golden() -> dict:
    return json.loads(SNAPSHOT.read_text(encoding="utf-8"))["cases"]


@pytest.fixture(scope="module")
def upstream() -> Path:
    if executable := os.environ.get("SYMUSIC_TEST_ABC2MIDI"):
        return Path(executable)
    pytest.skip("set SYMUSIC_TEST_ABC2MIDI to compare against a local abc2midi")


@pytest.mark.parametrize("name", CASES)
def test_abc_performance_matches_upstream_snapshot(name: str, golden: dict):
    if name in RECOVERED:
        with pytest.warns(UserWarning, match=rf"\[{RECOVERED[name][1]}\]"):
            score = Score.from_abc(CASES[name] + "\n")
    else:
        score = Score.from_abc(CASES[name] + "\n")
    assert performed(score) == golden[name]


@pytest.mark.parametrize("name", CASES)
def test_live_upstream_matches_snapshot(
    name: str, golden: dict, upstream: Path, tmp_path: Path
):
    exit_code = 1 if name in RECOVERED else 0
    score = oracle_score(upstream, CASES[name], tmp_path, expected_exit=exit_code)
    assert performed(score) == golden[name]


@pytest.mark.parametrize("kind", ["single voice", "dense chords", "issue 78"])
def test_upstream_plays_native_exports(kind: str, upstream: Path, tmp_path: Path):
    if kind == "issue 78":
        score = Score(TESTCASES_PATH / "issue78_dump_abc.mid")
    elif kind == "dense chords":
        score = Score(480)
        track = Track("Dense chords")
        track.notes.extend(
            Note(time, 30, pitch, 80) for time in (15, 45) for pitch in range(51)
        )
        score.tracks.append(track)
    else:
        score = roundtrip_score()
    played = oracle_score(upstream, score.dumps_abc(), tmp_path)
    assert note_rows(played) == note_rows(score)
    if kind == "single voice":
        assert played.tracks[0].program == score.tracks[0].program
        assert played.time_signatures == score.time_signatures
        assert played.tempos == score.tempos


def single_note(time: int, duration: int) -> Score:
    score = Score(480)
    track = Track()
    track.notes.append(Note(time, duration, 36, 80))
    score.tracks.append(track)
    return score


def test_export_long_silence(upstream: Path, tmp_path: Path):
    score = single_note(67_108_866, 19)
    abc = score.dumps_abc()
    assert note_rows(Score.from_abc(abc)) == note_rows(score)
    assert note_rows(oracle_score(upstream, abc, tmp_path)) == note_rows(score)


def test_export_long_hold(upstream: Path, tmp_path: Path):
    score = single_note(1, 1_000_001)
    abc = score.dumps_abc()
    assert note_rows(Score.from_abc(abc)) == note_rows(score)
    # abc2midi does not apply %%MIDI expand to a tied note.
    assert note_rows(oracle_score(upstream, abc, tmp_path)) == [(1, 1_000_000, 36, 80)]


def test_export_sustain_range():
    score = single_note(1, 67_108_866)
    with pytest.warns(UserWarning, match=r"\[midi-sustain-range\]"):
        abc = score.dumps_abc(warn=True)
    assert note_rows(Score.from_abc(abc)) == note_rows(score)


@pytest.mark.parametrize(
    ("wanted", "encoded"), [(1, 1), (777_777, 777_777), (16_777_215, 16_777_198)]
)
def test_export_tempos(wanted: int, encoded: int, upstream: Path, tmp_path: Path):
    score = single_note(1, 480)
    score.tempos.append(Tempo(0, mspq=wanted))
    if wanted != encoded:
        with pytest.warns(UserWarning, match=r"\[rounded-tempo\]"):
            abc = score.dumps_abc(warn=True)
    else:
        abc = score.dumps_abc(warn=True)
    assert Score.from_abc(abc).tempos[0].mspq == encoded
    assert oracle_score(upstream, abc, tmp_path).tempos[0].mspq == encoded


def refresh(executable: Path, revision: str) -> None:
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        cases = {
            name: performed(
                oracle_score(
                    executable, abc, root, expected_exit=int(name in RECOVERED)
                )
            )
            for name, abc in CASES.items()
        }
    # One case per line keeps fixture updates compact and independently diffable.
    rows = [
        f"    {json.dumps(name)}: {json.dumps(data, separators=(',', ':'))}"
        for name, data in cases.items()
    ]
    text = '{\n  "revision": ' + json.dumps(revision) + ',\n  "cases": {\n'
    SNAPSHOT.write_text(text + ",\n".join(rows) + "\n  }\n}\n", encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Refresh the checked-in abc2midi snapshot"
    )
    parser.add_argument("abc2midi", type=Path)
    parser.add_argument(
        "revision", help="exact upstream git SHA used to build abc2midi"
    )
    args = parser.parse_args()
    refresh(args.abc2midi, args.revision)
