"""Differential ABC fixtures. The upstream executable is optional and test-only."""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import tempfile
from pathlib import Path

import pytest
from symusic import ControlChange, Note, Pedal, Score, Tempo, Track

from tests.test_abc import HEADER, note_rows, roundtrip_score
from tests.utils import TESTCASES_PATH

SNAPSHOT = TESTCASES_PATH / "abc_oracle.json"
RECOVERED = {
    "decoration_line_end": "orphan-decoration",
    "rising_sun_stray_coda": "ignored-decoration",
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
    "conductor_tempo": HEADER + "V:1\nCDEF|[Q:1/4=90] GABc| [Q:1/4=60] cBAG|"
    "\nV:2\nC,4| G,4| C,4|",
    "lyric_tempo": HEADER
    + "CDEF|[Q:1/4=90] GABc|\nw: one two three four five six seven eight",
    "overlay_tempo": HEADER + "CDEF & C4|[Q:90]GABc|",
    "harmony_tempo": HEADER + '"C"CDEF|[Q:90] "G"GABc|',
    "drum_tempo": HEADER
    + "%%MIDI drum dd 36 42 70 80\n%%MIDI drumon\nCDEF|[Q:90]GABc|",
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
    "roll_octaves_and_key": HEADER.replace("K:C", "K:G") + "RB RC RE RF |",
    "ornament_continuation": HEADER + "CDE !fermata!\\\n F |",
    "trill_beat": HEADER + "TCDE|TC2DE|",
    "expand_after_trim": HEADER + "%%MIDI trim 1/8\nC\n%%MIDI expand 1/8\nD E F z |",
    "pedal_decorations": HEADER + "C !ped!D E !ped-up!F | !ped(!GAB !ped)!c |",
    "breath": HEADER + "C !breath!D !breath![EG] F |",
    "transpose_tune": HEADER.replace("K:C", "%%MIDI transpose 2\nK:C")
    + "V:one\nCDEF|\nV:two\nC,D,E,F,|",
    "transpose_key_and_voice": HEADER.replace(
        "K:C", "%%MIDI transpose 2\nK:C transpose=3"
    )
    + "V:one transpose=4\nCDEF|\nV:two\nC,D,E,F,|",
    "transpose_scoped_header": HEADER.replace(
        "K:C", "V:one\nV:two\n%%MIDI transpose 2\nK:C"
    )
    + "[V:one]CDEF|\n[V:two]C,D,E,F,|",
    "relative_transpose": HEADER.replace("K:C", "K:C transpose=3")
    + "C\n%%MIDI transpose 2\nD\n%%MIDI rtranspose -1\nE [K:G transpose=5] F|",
    "transpose_percussion": HEADER + "%%MIDI channel 10\n%%MIDI transpose 2\nCDEF|",
    "channel_setup": (
        HEADER.replace("K:C", "%%MIDI pitchbendrange 5\n%%MIDI portamento 12\nK:C")
        + "CDEF|"
    ),
    "microtone_carry": HEADER + "^/F F _3/4E E | F E ^/4F =F |",
    "microtone_wide": HEADER.replace("M:4/4", "M:1/4")
    + "^9/4F|_9/4F|^3/2F|^2F|^3F|_3F|",
    "microtone_tie_wheel": HEADER + "^/C- _/C D E|",
    "microtone_tie": HEADER + "^/C4- | C4 | D4 |",
    "microtone_key": HEADER.replace("K:C", "K:G _/B _3/4e _3/4A")
    + "B e A G|_/B _3/4e _3/4A G|",
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
    "part_pickup": HEADER + 'P:A\nD |:"C"CDEF| GABc:|\nP:B\nE |:"G"GABc| CDEF:|',
    "part_order_pickup": HEADER.replace("K:C", "P:ABA\nK:C")
    + "P:A\nD |:CDEF| GABc:|\nP:B\nE |:GABc| CDEF:|",
    "compound_minor": "X:1\nM:6/8\nQ:3/8=90\nK:Gm\nC D E F G A |",
    **{
        path.stem: path.read_text(encoding="utf-8")
        for path in sorted((TESTCASES_PATH / "abc_files").glob("*.abc"))
    },
    "rising_sun_stray_coda": (
        TESTCASES_PATH / "abc_invalid" / "rising_sun_stray_coda.abc"
    ).read_text(encoding="utf-8"),
    "decoration_line_end": HEADER + "C D E F |H\nG A B c |]",
}


# Compare all events for these upstream samples without redistributing them.
# Keep feature gaps out of the passing list; pin intentional differences in a
# separate focused test, rather than removing fields from every comparison.
UPSTREAM_SAMPLES = (
    "araber",
    "barcheck_long_warning",
    "barfly_aliases",
    "barfly_barcheck",
    "barfly_missing_bar_lines",
    "barfly_option",
    "barfly_rhythm",
    "barfly_split_bar",
    "barloc_long",
    "beatstring",
    "daramud",
    "demo",
    "decoration_bar_line",
    "drums",
    "fermata_bars",
    "gchord_codes",
    "gchord_inversion",
    "key_microtone",
    "key_unknown_word",
    "microtone_wide",
    "partdemo",
    "part_pickup",
    "tempo_tracks",
    "text_fields",
    "words",
    "words_tempo",
)


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
        with pytest.warns(UserWarning, match=rf"\[{RECOVERED[name]}\]"):
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


def test_bad_key_accidental(upstream: Path, tmp_path: Path):
    abc = HEADER + "C [K:G ^xF] D E F|"
    with pytest.warns(
        UserWarning, match=r"<string>:7:4: ABC \[invalid-accidental\].*\^x"
    ):
        native = Score.from_abc(abc)
    assert performed(native) == performed(oracle_score(upstream, abc, tmp_path))


def test_mixed_pitch_bends(upstream: Path, tmp_path: Path):
    abc = HEADER + "^/C [I:MIDI pitchbend 0 64] ^/D E F|"
    native = performed_track(Score.from_abc(abc).tracks[0])["bends"]
    upstream_bends = performed_track(oracle_score(upstream, abc, tmp_path).tracks[0])[
        "bends"
    ]
    # Neither restores the accidental wheel on D; abc2midi moves E a tick early.
    assert native == [[1, 2048], [480, 0], [961, 0]]
    assert upstream_bends == [[1, 2048], [480, 0], [960, 0]]


def test_pitchbend_does_not_shift_note_clock(upstream: Path, tmp_path: Path):
    abc = HEADER + "C [I:MIDI pitchbend 12 69] D E F |"
    native = Score.from_abc(abc)
    actual = oracle_score(upstream, abc, tmp_path)
    assert native.tracks[0].pitch_bends == actual.tracks[0].pitch_bends
    assert [(bend.time, bend.value) for bend in native.tracks[0].pitch_bends] == [
        (480, 652)
    ]
    assert [note.time for note in native.tracks[0].notes] == [1, 481, 961, 1441]
    assert [note.time for note in actual.tracks[0].notes] == [1, 480, 960, 1440]


@pytest.fixture(scope="module")
def upstream_samples(upstream: Path) -> Path:
    if source := os.environ.get("SYMUSIC_TEST_ABCMIDI_SOURCE"):
        samples = Path(source) / "samples"
        assert samples.is_dir(), f"upstream samples not found: {samples}"
        return samples
    pytest.skip("set SYMUSIC_TEST_ABCMIDI_SOURCE to use the upstream sample corpus")


@pytest.mark.parametrize("name", UPSTREAM_SAMPLES)
def test_upstream_sample(
    name: str, upstream: Path, upstream_samples: Path, tmp_path: Path
):
    abc = (upstream_samples / f"{name}.abc").read_text(encoding="utf-8-sig")
    if len(re.findall(r"^X\s*:", abc, re.MULTILINE)) > 1:
        with pytest.warns(UserWarning, match=r"\[extra-tunes\]"):
            native = Score.from_abc(abc)
    else:
        native = Score.from_abc(abc)
    assert performed(native) == performed(oracle_score(upstream, abc, tmp_path))


@pytest.mark.parametrize("index", range(7))
def test_upstream_part_pickups(
    index: int, upstream: Path, upstream_samples: Path, tmp_path: Path
):
    abc = (upstream_samples / "part_pickup.abc").read_text(encoding="utf-8")
    tune = re.split(r"(?=^X:)", abc, flags=re.MULTILINE)[index + 1]
    assert performed(Score.from_abc(tune)) == performed(
        oracle_score(upstream, tune, tmp_path)
    )


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


@pytest.mark.parametrize(
    "kind",
    ["irregular_lengths", "overlap_pitch", "simultaneous_velocities", "multitrack"],
)
def test_export_playback_events(kind: str, upstream: Path, tmp_path: Path):
    score = roundtrip_score()
    track = score.tracks[0]
    if kind == "irregular_lengths":
        track.notes[:] = [
            Note(1, 1, 60, 81),
            Note(15, 53, 61, 91),
            Note(1081, 240, 62, 77),
        ]
    elif kind == "overlap_pitch":
        track.notes.append(Note(97, 600, track.notes[0].pitch, 92))
    elif kind == "simultaneous_velocities":
        track.notes.append(Note(track.notes[0].time, 480, 103, 109))
    else:
        other = Track("Other instrument", 41)
        other.notes.append(Note(241, 240, 54, 94))
        score.tracks.append(other)
    abc = score.dumps_abc()
    native = Score.from_abc(abc)
    actual = oracle_score(upstream, abc, tmp_path)
    assert performed(native) == performed(actual)
    assert note_rows(actual) == note_rows(score)
    assert {track.program for track in actual.tracks} == {
        track.program for track in score.tracks
    }


def test_export_conductor_tempo(upstream: Path, tmp_path: Path):
    score = Score(480)
    score.tempos.extend([Tempo(0, mspq=500_000), Tempo(720, mspq=625_000)])
    for index in range(2):
        track = Track(str(index), 24 + index)
        track.notes.extend(
            [Note(1, 240, 60 + index, 90), Note(961, 240, 62 + index, 90)]
        )
        score.tracks.append(track)
    abc = score.dumps_abc()
    native = Score.from_abc(abc)
    actual = oracle_score(upstream, abc, tmp_path)
    assert native.tempos == actual.tempos == score.tempos
    assert performed(native) == performed(actual)


def test_export_drum_release(upstream: Path, tmp_path: Path):
    score = roundtrip_score()
    score.tracks[0].is_drum = True
    score.tracks[0].program = 0
    abc = score.dumps_abc()
    assert performed_track(Score.from_abc(abc).tracks[0]) == performed_track(
        score.tracks[0]
    )
    actual = oracle_score(upstream, abc, tmp_path)
    assert actual.tracks[0].is_drum
    assert note_rows(actual) == [(t, d - 1, p, v) for t, d, p, v in note_rows(score)]


def test_export_controls_inside_sustains(upstream: Path, tmp_path: Path):
    score = roundtrip_score()
    track = score.tracks[0]
    track.controls.extend([ControlChange(0, 7, 101), ControlChange(35, 7, 62)])
    track.pedals.append(Pedal(45, 500))
    abc = score.dumps_abc()
    native = Score.from_abc(abc)
    assert list(native.tracks[0].controls) == [
        *track.controls,
        ControlChange(45, 64, 127),
        ControlChange(545, 64, 0),
    ]
    assert native.tracks[0].pedals == track.pedals
    actual = oracle_score(upstream, abc, tmp_path)
    assert list(actual.tracks[0].controls) == [
        ControlChange(1, 7, 101),
        ControlChange(1, 7, 62),
        ControlChange(1, 64, 127),
        ControlChange(481, 64, 0),
    ]
    assert list(actual.tracks[0].pedals) == [Pedal(1, 480)]
    assert [(n.time, n.pitch, n.velocity) for n in actual.tracks[0].notes] == [
        (n.time, n.pitch, n.velocity) for n in track.notes
    ]


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
