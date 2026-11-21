"""Differential ABC fixtures. The upstream executable is optional and test-only."""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import tempfile
import warnings
from contextlib import nullcontext
from itertools import groupby
from pathlib import Path

import pytest
from mido import MidiFile, merge_tracks
from symusic import (
    ControlChange,
    KeySignature,
    Note,
    Pedal,
    PitchBend,
    Score,
    Tempo,
    TimeSignature,
    Track,
)

from tests.test_abc import (
    HEADER as ABC_HEADER,
)
from tests.test_abc import (
    note_rows,
    roundtrip_score,
    score_with_constrained_onset,
)
from tests.utils import TESTCASES_PATH

# An explicit beat gives both players the same velocities for the melody.
COMPARISON_BEAT = "%%MIDI beat 105 95 80 2"
HEADER = ABC_HEADER.replace("K:C", COMPARISON_BEAT + "\nK:C")

SNAPSHOT = TESTCASES_PATH / "abc_oracle.json"
CASES = json.loads((TESTCASES_PATH / "abc_inputs.json").read_text(encoding="utf-8"))


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
    """Compare performed events, not track titles or redundant meter copies."""
    return {
        "tpq": score.tpq,
        "tempos": [[event.time, event.mspq] for event in score.tempos],
        # abc2midi can copy identical time signatures into the conductor
        # and backing tracks. Keep real changes, including changes at tick 0.
        "meters": [
            list(meter)
            for meter, _ in groupby(
                (event.time, event.numerator, event.denominator)
                for event in score.time_signatures
            )
        ],
        "keys": [
            [event.time, event.key, event.tonality] for event in score.key_signatures
        ],
        "tracks": [performed_track(track) for track in score.tracks],
    }


def oracle_midi(
    executable: Path, abc: str, directory: Path, *options: str, expected_exit: int = 0
) -> Path:
    source = directory / "sample.abc"
    output = directory / "sample.mid"
    source.write_text(abc + "\n", encoding="utf-8")
    output.unlink(missing_ok=True)
    result = subprocess.run(
        [executable.resolve(), source, "-o", output, "-silent", *options],
        capture_output=True,
        text=True,
        check=False,
        timeout=15,
    )
    assert result.returncode == expected_exit, result.stdout + result.stderr
    assert output.is_file(), result.stdout + result.stderr
    return output


def oracle_score(
    executable: Path, abc: str, directory: Path, *options: str, expected_exit: int = 0
) -> Score:
    return Score(
        oracle_midi(executable, abc, directory, *options, expected_exit=expected_exit)
    )


@pytest.mark.parametrize(
    "music",
    [
        "[I:MIDI snt 60 61.5] C [I:MIDI snt 60 62.125] D E F |]",
        "z [I:MIDI snt 60 61] C z [I:MIDI snt 60 62] D |]",
        "z [I:MIDI program 0] [I:MIDI snt 60 61.5] C z2 |]",
        "z [Q:1/4=90] [I:MIDI snt 60 61.5] C z2 |]",
        '"C"z [I:MIDI snt 60 61.5] D z2 |]',
        "z [I:MIDI droneon] [I:MIDI snt 60 61.5] C D [I:MIDI droneoff] z |]",
    ],
)
def test_live_tuning_leaves_no_trace(upstream, tmp_path, music):
    # The MIDI reader skips the tuning SysEx that abc2midi writes.
    abc = HEADER + music
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


@pytest.mark.parametrize("gap", [0, 4, 10])
@pytest.mark.parametrize("music", ["C [DF] E G", "TC [DF] E [GB]", "RC-C [DF] E"])
def test_live_expanded_release_queue(
    upstream: Path, tmp_path: Path, gap: int, music: str
):
    abc = HEADER + f"%%MIDI chordattack {gap}\n%%MIDI expand 1/24\n{music} | z4 |]"
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


def test_live_numeric_voice_ids(upstream: Path, tmp_path: Path):
    abc = (
        "%abc-2.1\nX:1\nT:Voice numbers\nM:4/4\nL:1/4\nV:01\nV:2\nK:C\n"
        "[V:1] C D [V:001] E F |]\n[V:02] A, B, [V:2] C D |]"
    )

    def gates(score: Score) -> list[list[tuple[int, int, int]]]:
        return [
            [(note.time, note.duration, note.pitch) for note in track.notes]
            for track in score.tracks
        ]

    assert gates(Score.from_abc(abc)) == gates(oracle_score(upstream, abc, tmp_path))


def test_live_text_tempo_keeps_previous_header_tempo(upstream: Path, tmp_path: Path):
    abc = (
        "%abc-2.1\nX:1\nT:Tempo label\nM:4/4\nL:1/4\nQ:1/4=90\n"
        'Q:"Slowly with feeling"\nK:C\nCDEF |]'
    )
    with pytest.warns(UserWarning, match=r"\[text-tempo\]"):
        native = Score.from_abc(abc)
    reference = oracle_score(upstream, abc, tmp_path, expected_exit=1)
    assert native.tempos == reference.tempos == [Tempo(0, mspq=666666)]


@pytest.fixture(scope="module")
def golden() -> dict:
    cases = json.loads(SNAPSHOT.read_text(encoding="utf-8"))["cases"]
    assert cases.keys() == CASES.keys()
    return cases


@pytest.fixture(scope="module")
def upstream() -> Path:
    if executable := os.environ.get("SYMUSIC_TEST_ABC2MIDI"):
        return Path(executable)
    pytest.skip("set SYMUSIC_TEST_ABC2MIDI to compare against a local abc2midi")


@pytest.mark.parametrize("name", CASES)
def test_native_matches_snapshot(name: str, golden: dict):
    sample = CASES[name]
    with warnings.catch_warnings(record=True) as caught:
        warnings.simplefilter("always")
        score = Score.from_abc(sample["abc"])
    messages = [str(w.message) for w in caught]
    if code := sample.get("warning"):
        assert any(f"[{code}]" in message for message in messages), messages
    else:
        assert not messages
    assert performed(score) == golden[name]


@pytest.mark.parametrize("name", CASES)
def test_live_upstream_matches_snapshot(
    name: str, golden: dict, upstream: Path, tmp_path: Path
):
    sample = CASES[name]
    score = oracle_score(
        upstream, sample["abc"], tmp_path, expected_exit=sample.get("exit", 0)
    )
    assert performed(score) == golden[name]


def test_abc_lyric_bar_alignment(upstream: Path, tmp_path: Path):
    abc = HEADER + "CDEF | GABc | cBAG |\nw: one | two\\-three | four"
    native = Score.from_abc(abc)
    actual = oracle_score(upstream, abc, tmp_path, "-lyrics")
    # abc2midi stores lyrics in a Words track and prefixes inter-word spaces.
    assert [(e.time, e.text) for e in native.tracks[0].lyrics] == [
        (e.time, e.text.lstrip(" ")) for track in actual.tracks for e in track.lyrics
    ]


@pytest.mark.parametrize("rest", ["Z2", "X2"])
def test_multimeasure_rest_backing(rest: str, upstream: Path, tmp_path: Path):
    abc = (
        HEADER + "%%MIDI drum dddd 36 37 38 39 91 92 93 94\n%%MIDI drumon\n"
        f'"C"{rest} | D4 |]'
    )
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


@pytest.mark.parametrize(
    "body",
    [
        "C2 & [I:MIDI drumon] !p!E- & G2 | F2 & E2 & B | C2 |",
        (
            "C4 & [I:MIDI drumon] E4 & [I:MIDI drum dd 37 43] [I:MIDI drumon] G4 |\n"
            "[I:MIDI drumoff] D4 & [I:MIDI drumoff] F4 & A4 | B4 |]"
        ),
        "|: C4 & [I:MIDI drumon] E4 | D4 & [I:MIDI drumoff] F4 :| G4 |]",
        "C4 & [I:MIDI drumon] E4 | D4 | F4 & G4 | A4 |]",
    ],
)
def test_overlay_drum_tracks(body: str, upstream: Path, tmp_path: Path):
    abc = HEADER + "%%MIDI drum dd 36 42\n" + body
    native = Score.from_abc(abc)
    reference = oracle_score(upstream, abc, tmp_path)
    assert [track.is_drum for track in native.tracks] == [
        track.is_drum for track in reference.tracks
    ]
    assert [track.notes for track in native.tracks if track.is_drum] == [
        track.notes for track in reference.tracks if track.is_drum
    ]


def test_overlay_harmony_tracks(upstream: Path, tmp_path: Path):
    abc = (
        HEADER
        + '%%MIDI programbase 0\n"C"C4 & !mf![I:MIDI gchordoff] [I:MIDI chordprog 5] [I:MIDI bassvol 31] "Am"E4 |\n'
        "[I:MIDI gchordoff] D4 & [I:MIDI gchordon] F4 | B4 |]"
    )
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


def test_overlay_drones(upstream: Path, tmp_path: Path):
    abc = (
        HEADER + "%%MIDI drone 40 47 35 61 62\n"
        "C4 & !mf![I:MIDI droneon] E4 & !mf![I:MIDI droneon] G4 |\n"
        "D4 & [I:MIDI droneoff] F4 & A4 | B4 & G4 & [I:MIDI droneoff] F4 |]"
    )
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


@pytest.mark.parametrize(
    "body",
    [
        "[V:lead] C>[V:lead]DEF GABC | [V:side] !mf!C8|]",
        "[V:lead] C>[V:side] !mf!C8 | [V:lead] DEF GABC |]",
        "[V:lead] C>[V:lead octave=0]DEF GABC | [V:side] !mf!C8|]",
        "[V:lead] {c}[V:lead octave=0] CDEF GABC | [V:side] !mf!C8|]",
    ],
)
def test_voice_fields_in_broken_rhythm_and_grace(
    body: str, upstream: Path, tmp_path: Path
):
    abc = (
        HEADER.replace("L:1/4", "L:1/8").replace(
            "K:C", "V:lead octave=1\nV:side transpose=-2\nK:C"
        )
        + body
    )
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


@pytest.mark.parametrize(
    "abc",
    [
        HEADER.replace("K:C\n", "%%MIDI tuningsystem comma53\nK:C\n")
        + "CDEF | GABc |]",
        HEADER + "CDEF | [I:MIDI tuningsystem comma53] CDEF | ^/C _/D ^3/4C _3/4D |]",
        HEADER + "|: CDEF | [I:MIDI tuningsystem comma53] CDEF :| GABc |]",
        HEADER + "%%MIDI tuningsystem comma53\n"
        "CDEF | [I:MIDI C 61] CDEF | [I:MIDI temperamentnormal] CDEF |]",
    ],
)
def test_comma53_tuning(abc: str, upstream: Path, tmp_path: Path):
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


def test_chord_accidental_register(upstream: Path, tmp_path: Path):
    abc = (
        HEADER + '%%MIDI gchord fc\n"Cb"C4 | "B#"D4 | "Cbb"E4 | "B##"F4 | "D/cbb"G4 |]'
    )
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


def test_clef_change_preserves_measure_accidentals(upstream: Path, tmp_path: Path):
    abc = "%abc-2.1\n" + HEADER + "^F [K:clef=bass] F F [K:C] F | F4 |]"
    native = Score.from_abc(abc)
    reference = oracle_score(upstream, abc, tmp_path)
    assert native.key_signatures == reference.key_signatures
    assert [n.pitch for n in native.tracks[0].notes] == [
        n.pitch for n in reference.tracks[0].notes
    ]


def lyric_rows(score: Score) -> list[tuple[int, str]]:
    return sorted(
        (event.time, event.text.lstrip(" ")) for t in score.tracks for event in t.lyrics
    )


def test_abc_exported_lyrics_follow_attacks(upstream: Path, tmp_path: Path):
    source = Score.from_abc(
        "%abc-2.1\n"
        + HEADER
        + "C z D [EG] F G A B c d e f g a b c' d' e' f' g' a' b' c'' d''\n"
        "w: first * three * later * * * * * * * * * * * * * * * * last\n"
    )
    written = source.dumps_abc()
    actual = lyric_rows(oracle_score(upstream, written, tmp_path, "-lyrics"))
    # Upstream can emit a blank lyric for '*' and duplicate the opening lyric at one tick.
    assert sorted({(tick, text) for tick, text in actual if text}) == lyric_rows(source)


def test_numbered_stanza_lyric_alignment(upstream: Path, tmp_path: Path):
    abc = "%abc-2.1\n" + HEADER + "C D E F |]\nw: 1.~Three blind mice last"
    expected = [(1, "Three"), (481, "blind"), (961, "mice"), (1441, "last")]
    assert lyric_rows(Score.from_abc(abc)) == expected
    assert lyric_rows(oracle_score(upstream, abc, tmp_path, "-lyrics")) == expected


@pytest.mark.parametrize("width", [197, 198])
def test_export_lyric_karaoke_limit(width: int, upstream: Path, tmp_path: Path):
    syllable = "a" * width
    source = Score.from_abc(
        "%abc-2.1\n" + HEADER + f"C D E |]\nw: first {syllable} last"
    )
    report = (
        pytest.warns(UserWarning, match=r"\[midi-lyric-width\]")
        if width > 197
        else nullcontext()
    )
    with report:
        abc = source.dumps_abc(warn=True)
    assert lyric_rows(Score.from_abc(abc)) == lyric_rows(source)
    midi = MidiFile(oracle_midi(upstream, abc, tmp_path))
    karaoke = [
        message.text.lstrip("/\\ ")
        for t in midi.tracks
        for message in t
        if message.type == "text"
    ]
    assert (syllable in karaoke) == (width == 197)


@pytest.mark.parametrize(
    "abc",
    [
        HEADER + "C [I:MIDI vol 0] D E F|\nw: one two three four",
        HEADER + "CDEF|\nw: one two three four\nw: five six seven eight",
        HEADER.replace("K:C", "P:(AB)3\nK:C")
        + "P:A\nCDEF|\nw: one two three four\nw: five six seven eight\n"
        + "P:B\nGABc|\nw: An-swer be-low",
    ],
    ids=["muted-note", "first-verse", "ordered-verses"],
)
def test_abc_lyrics_follow_notation(abc: str, upstream: Path, tmp_path: Path):
    assert lyric_rows(Score.from_abc(abc)) == lyric_rows(
        oracle_score(upstream, abc, tmp_path, "-lyrics")
    )


@pytest.mark.parametrize(
    "body",
    [
        (
            "|: CDEF |\nw: one two three four\nGABc :|\nw: five six seven eight\n"
            "cBAG|\nw: nine ten eleven twelve"
        ),
        (
            "CDEF |:\nw: one two three four\nGABc |\nw: five six seven eight\n"
            "cBAG :|\nw: nine ten eleven twelve"
        ),
        "CDEF |\nw: one two three four\nGABc :|\nw: five six seven eight",
        "[V:solo] CDEF |\nw: one two three four\nGABc :|\nw: five six seven eight",
        (
            "|: CDEF |\nw: one two three four\n[1 GABc :|\nw: five six seven eight\n"
            "[2 cBAG|\nw: nine ten eleven twelve"
        ),
    ],
    ids=[
        "close",
        "opening-at-eol",
        "implicit",
        "implicit-inline-voice",
        "numbered-endings",
    ],
)
def test_abc_lyrics_at_repeat_boundaries(body: str, upstream: Path, tmp_path: Path):
    abc = HEADER + body
    assert lyric_rows(Score.from_abc(abc)) == lyric_rows(
        oracle_score(upstream, abc, tmp_path, "-lyrics")
    )


@pytest.mark.parametrize(
    ("body", "expected"),
    [
        ("C |:\nw: one\nD1 :|\nw: two", [(1, "one"), (481, "two"), (961, "two")]),
        (
            "C |: [I:MIDI program 0]\nw: one\nD |\nw: two\nE1 :|\nw: three",
            [(1, "one"), (481, "two"), (961, "three"), (1441, "two"), (1921, "three")],
        ),
    ],
)
def test_repeat_reenters_lyric_line(
    body: str, expected: list[tuple[int, str]], upstream: Path, tmp_path: Path
):
    abc = HEADER.replace("M:4/4", "M:1/4") + body
    assert lyric_rows(Score.from_abc(abc)) == expected
    assert lyric_rows(oracle_score(upstream, abc, tmp_path, "-lyrics")) == expected


def test_shorthand_decoration_spelling(upstream: Path, tmp_path: Path):
    abc = HEADER + "'C !'! D !H! E !T! F|"
    with pytest.warns(UserWarning, match=r"\[ignored-decoration\]") as warnings:
        native = Score.from_abc(abc)
    assert len(warnings) == 3
    assert performed(native) == performed(oracle_score(upstream, abc, tmp_path))


def test_abc_2_1_navigation_decorations_play_linearly(upstream: Path, tmp_path: Path):
    abc = (
        "%abc-2.1\n"
        + HEADER
        + "!segno!C !dacoda!D E !fine!F | !coda!G A B c !D.S.! !D.C.! !dacapo! |]"
    )
    plain = "%abc-2.1\n" + HEADER + "CDEF | GABc |]"
    assert performed(Score.from_abc(abc)) == performed(Score.from_abc(plain))
    assert performed(oracle_score(upstream, abc, tmp_path)) == performed(
        oracle_score(upstream, plain, tmp_path)
    )


def test_control_after_program(upstream: Path, tmp_path: Path):
    abc = HEADER + "%%MIDI program 24\n%%MIDI control 7 50\nCDEF|"
    native = performed_track(Score.from_abc(abc).tracks[0])
    actual = performed_track(oracle_score(upstream, abc, tmp_path).tracks[0])
    # MIDI writes the program on the pickup tick; the controller inherits
    # that delta in abc2midi. Native controllers use the written clock.
    assert native.pop("controls") == [[0, 7, 50]]
    assert actual.pop("controls") == [[1, 7, 50]]
    assert native == actual


def test_random_chord_attack_timing(upstream: Path, tmp_path: Path):
    abc = (
        HEADER + "%%MIDI chordattack 4\n%%MIDI randomchordattack 10\n"
        "[CEG] [DFA] !arpeggio![EGB] [FAc] | "
        "[I:MIDI randomchordattack 0] [CEG] !arpeggio![CEG] D |]"
    )
    # abc2midi uses the platform's rand(). Compare the timing contract, not
    # its sample sequence.
    for score in (Score.from_abc(abc), oracle_score(upstream, abc, tmp_path)):
        notes = sorted(score.tracks[0].notes, key=lambda note: (note.time, note.pitch))
        assert len(notes) == 19
        delays = []
        for chord, limit in enumerate((10, 10, 30, 10, 0, 0)):
            c, e, g = notes[3 * chord : 3 * chord + 3]
            start = 480 * chord
            assert c.time == start + 1
            assert c.end == e.end == g.end == start + 480
            gaps = (e.time - c.time, g.time - e.time)
            if limit:
                assert all(0 <= gap < limit for gap in gaps)
                if limit == 10:
                    delays.extend(gaps)
            else:
                fixed_gap = 4 if chord == 4 else 12
                assert gaps == (fixed_gap, fixed_gap)
        assert len(set(delays)) > 1
        assert notes[-1].time == 2881


@pytest.mark.parametrize(
    "body",
    [
        "%%MIDI beat 98 80 61 0\nCDEFGABc |]",
        "%%MIDI beat 98 80 61 1\nCDEF | [M:4/4] GABc |]",
        "%%MIDI beat 98 80 61 1\nCDEF | [M:3/4] [I:MIDI beat 99 81 62 0] GAB | cde |]",
        "!f!CDEF | [M:3/4] GAB |]",
        "%%MIDI beat 98 80 61 1\nC{a}[M:3/4]D E |]",
        "%%MIDI beat 98 80 61 1\n|: CDEF | [M:3/4] [I:MIDI beat 99 81 62 1] GAB :|",
    ],
)
def test_midi_beat_spacing(body: str, upstream: Path, tmp_path: Path):
    abc = HEADER + body
    native, reference = (Score.from_abc(abc), oracle_score(upstream, abc, tmp_path))
    # The reference may omit a restored meter event when it jumps back.
    assert (
        performed_track(native.tracks[0])["notes"]
        == performed_track(reference.tracks[0])["notes"]
    )


@pytest.mark.parametrize("model", [1, 2])
def test_phil_taylor_stress(model: int, upstream: Path, tmp_path: Path):
    abc = (
        HEADER + f"%%MIDI stressmodel {model}\n"
        "%%MIDI ptstress 4 112 1.4 75 0.6 102 1.2 65 0.8\n"
        "|: CDEF :| GABc |]"
    )
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


@pytest.mark.parametrize(
    "phrase",
    ["C F- F G", "C F- F- F", "C F2- | F2 G A", "C [I:MIDI volinc -78]F2- | F2 G A"],
)
def test_model_one_stress_uses_the_whole_tied_gate(
    phrase: str, upstream: Path, tmp_path: Path
):
    abc = (
        HEADER + "%%MIDI stressmodel 1\n"
        "%%MIDI ptstress 4 112 1.4 75 0.6 102 1.2 65 0.8\n" + phrase + " | DEFG |]"
    )
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


@pytest.mark.parametrize(
    ("options", "phrase"),
    [
        ("", "C TD2 E"),
        ("", "C RD EF"),
        ("", "(C {a}TD) EF"),
        ("%%MIDI expand 1/16\n", "C TD EF"),
        ("%%MIDI trim 0/1\n", "C TD EF"),
    ],
)
def test_stress_articulates_expanded_ornament_attacks(
    options: str, phrase: str, upstream: Path, tmp_path: Path
):
    abc = (
        HEADER + "%%MIDI stressmodel 1\n"
        "%%MIDI ptstress 4 112 1.4 75 0.6 102 1.2 65 0.8\n"
        + options
        + phrase
        + " | FGAz |]"
    )
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


@pytest.mark.parametrize("ornament", ["TD", "RD", "(TD)"])
def test_trim_applies_to_expanded_ornament_attacks(
    ornament: str, upstream: Path, tmp_path: Path
):
    abc = HEADER + f"%%MIDI trim 1/16\nC {ornament} EF |]"
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


@pytest.mark.parametrize("phrase", ["C .D2 E", "C (.D2 E)", "C {a}.D2 E", ".C4"])
def test_stress_articulates_the_staccato_gate(
    phrase: str, upstream: Path, tmp_path: Path
):
    abc = (
        HEADER + "%%MIDI stressmodel 1\n"
        "%%MIDI ptstress 4 112 1.4 75 0.6 102 1.2 65 0.8\n" + phrase + " | FGAB |]"
    )
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


def test_stress_timing_maps_staccato_release(upstream: Path, tmp_path: Path):
    abc = HEADER + "%%MIDI ptstress 4 112 1.4 75 0.6 102 1.2 65 0.8\nC .D2 E | FGAB |]"
    native = performed_track(Score.from_abc(abc).tracks[0])["notes"]
    reference = performed_track(oracle_score(upstream, abc, tmp_path).tracks[0])[
        "notes"
    ]
    # abc2midi maps the sounded half but leaves the following silence unwarped.
    assert native[:2] == reference[:2]
    assert native[2][0] == 1537
    assert native[3][0] == 1921


@pytest.mark.parametrize("model", [1, 2])
@pytest.mark.parametrize(
    "phrase",
    [
        "C H[DF] E | GABc |]",
        "%%MIDI fermatafixed\nC Hz2 D | EFGF |]",
        "%%MIDI grace 1/2\nC {a}HD E | FGAB |]",
    ],
)
def test_stress_maps_fermata_before_timing(
    model: int, phrase: str, upstream: Path, tmp_path: Path
):
    abc = (
        HEADER + f"%%MIDI stressmodel {model}\n"
        "%%MIDI ptstress 4 112 1.4 75 0.6 102 1.2 65 0.8\n" + phrase
    )
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


def test_stress_timing_compresses_grace_notes(upstream: Path, tmp_path: Path):
    abc = HEADER + "%%MIDI ptstress 4 112 1.4 75 0.6 102 1.2 65 0.8\nC {abc}D EF |]"
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


def test_stress_timing_of_harp_cut_after_grace(upstream: Path, tmp_path: Path):
    abc = (
        HEADER + "%%MIDI harpmode 1\n%%MIDI grace 3/4\n"
        "%%MIDI ptstress 4 112 2 75 .5 102 1 65 .5\nC3/4 {c}~D EF |]"
    )
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


@pytest.mark.parametrize("chords", ["[C-EG] [CFA]", "[CE-G] [FEA]", "[CEG-] [FAG]"])
def test_chord_spread_with_member_tie(chords: str, upstream: Path, tmp_path: Path):
    abc = HEADER + "%%MIDI chordattack 4\n" + chords + " D E |]"
    native = Score.from_abc(abc)
    reference = oracle_score(upstream, abc, tmp_path)
    # abc2midi can trim a sustained member's release with a later stagger.
    # Compare the initial attacks, including that member's inherited offset.
    assert sorted((n.time, n.pitch) for n in native.tracks[0].notes) == sorted(
        (n.time, n.pitch) for n in reference.tracks[0].notes
    )


@pytest.mark.parametrize(
    "body",
    [
        "A B [I:MIDI channel 10] [I:MIDI makechordchannels 1] [CEG] F |]",
        (
            "A A A [I:MIDI channel 10] [I:MIDI makechordchannels 1] [CE] |: "
            "[I:MIDI control 7 42] D [I:MIDI channel 2] E F G :|"
        ),
        "A B [I:MIDI channel 10] [I:MIDI makechordchannels 1] [CE] !trill!F |]",
        "A B [I:MIDI channel 10] [I:MIDI makechordchannels 1] [CE^F-] [FA_G] |]",
        "%%MIDI makechordchannels 1\n[CE] [I:MIDI makechordchannels 1] [FA] B c |]",
        "A B c d |: [I:MIDI makechordchannels 1] [CE] D E F :|",
        "%%MIDI makechordchannels 1\n[CE] [I:MIDI program 2 17] [FA] B c |]",
        "%%MIDI makechordchannels 1\n[CE] [I:MIDI makechordchannels 0] [FA] B c |]",
        "%%MIDI channel 2\n%%MIDI makechordchannels 1\n[CE] D E F |]",
        "%%MIDI makechordchannels 10\n[CDEFGABcdef] G A B |]",
    ],
)
def test_chord_channel_pitch_and_repeat_routing(
    body: str, upstream: Path, tmp_path: Path
):
    abc = HEADER + "%%MIDI transpose 5\n" + body
    assert_chord_channel_routing(abc, upstream, tmp_path)


@pytest.mark.parametrize(
    "body",
    [
        (
            "[V:a] [I:MIDI makechordchannels 1] [CE] D E F |]\n"
            "[V:b] !mf![I:MIDI makechordchannels 1] [CG] [I:MIDI program 2 17] "
            "[CF] [I:MIDI program 4 19] [DA] B |]"
        ),
        (
            '[V:a] "C"[I:MIDI makechordchannels 1] [CE] D E F |]\n'
            "[V:b] !mf![I:MIDI makechordchannels 1] [CG] [I:MIDI program 2 17] "
            "[CF] [I:MIDI program 7 19] [DA] B |]"
        ),
        (
            "C4 & !mf![I:MIDI makechordchannels 1] [CE] [I:MIDI program 4 19] [DF] G A | "
            "[I:MIDI makechordchannels 1] [CG] [I:MIDI program 2 17] [DA] B c |]"
        ),
    ],
)
def test_chord_channel_track_order(body: str, upstream: Path, tmp_path: Path):
    assert_chord_channel_routing(HEADER + body, upstream, tmp_path)


def assert_chord_channel_routing(abc: str, upstream: Path, tmp_path: Path) -> None:
    native = performed(Score.from_abc(abc))
    reference = performed(oracle_score(upstream, abc, tmp_path))
    # The score does not preserve physical channel numbers or their track order.
    for score in (native, reference):
        score["tracks"].sort(key=lambda track: (track["drums"], track["notes"]))
    assert native == reference


def test_idle_overlay_backing_in_an_explicit_group(upstream: Path, tmp_path: Path):
    abc = HEADER + 'C4 & !mf!D4 & !mf!"C"E4 | (& F4 | G4 & A4 | B4 &) |]'
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


@pytest.mark.parametrize(("quality", "ref_exit"), [("7", 0), ("unknown", 1)])
def test_bass_only_chord_quality(
    quality: str, ref_exit: int, upstream: Path, tmp_path: Path
):
    abc = HEADER + f'"c{quality}" C [Q:1/4=96] D E F |]'
    warning = (
        pytest.warns(UserWarning, match=r"\[unknown-chord\]")
        if ref_exit
        else nullcontext()
    )
    with warning:
        score = Score.from_abc(abc)
    reference = oracle_score(upstream, abc, tmp_path, expected_exit=ref_exit)
    assert performed(score) == performed(reference)


def test_backing_control_destinations(upstream: Path, tmp_path: Path):
    abc = (
        HEADER + "%%MIDI programbase 0\n%%MIDI bassprog 12\n%%MIDI chordprog 25\n"
        "%%MIDI control bass 7 41\n%%MIDI control chord 10 23\n"
        "%%MIDI portamento chord 13\n"
        '"C"CDEF | [I:MIDI control bass 7 67] [I:MIDI noportamento chord] "G"GABc |'
    )
    native = {track.program: track for track in Score.from_abc(abc).tracks}
    upstream_tracks = {
        track.program: track for track in oracle_score(upstream, abc, tmp_path).tracks
    }
    # Upstream also writes these commands on the melody channel and moves some
    # backing writes earlier. Compare the intended destinations and values.
    assert not native[0].controls
    for program, settings in [
        (12, [(7, 41), (7, 67)]),
        (25, [(10, 23), (65, 127), (5, 13), (65, 0)]),
    ]:
        assert [(c.number, c.value) for c in native[program].controls] == settings
        assert [
            (c.number, c.value) for c in upstream_tracks[program].controls
        ] == settings


def test_bad_key_accidental(upstream: Path, tmp_path: Path):
    abc = HEADER + "C [K:G ^xF] D E F|"
    with pytest.warns(
        UserWarning, match=r"<string>:8:4: ABC \[invalid-accidental\].*\^x"
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


# These tunes exercise differences in notation or MIDI serialization. The native
# sample tests below check the corresponding Score events on the original input.
NATIVE_SAMPLES = {
    "baym_rebin/63",
    "chordname_long/1",
    "coleraine/8",
    "decoration_space/1",
    "decoration_space/2",
    "demo/7",
    "demo/8",
}
SAMPLE_RECOVERY = {
    "chordname_too_long/1": "invalid-chord-name",
    "decoration_line_end/1": "orphan-decoration",
    "microtone/1": "invalid-accidental",
    "microtone_accidentals/1": "invalid-accidental",
    "microtone_accidentals/2": "invalid-accidental",
}


def upstream_sample_tunes() -> dict[str, str]:
    if not (source := os.environ.get("SYMUSIC_TEST_ABCMIDI_SOURCE")):
        return {}
    samples = Path(source) / "samples"
    cases = {}
    for path in sorted(samples.glob("*.abc")):
        abc = path.read_text(encoding="utf-8-sig")
        header, *tunes = re.split(r"(?=^[ \t]*X\s*:)", abc, flags=re.MULTILINE)
        for tune in tunes:
            reference = re.match(r"\s*X\s*:\s*(\d+)", tune).group(1)
            cases[f"{path.stem}/{reference}"] = header + tune
    assert cases, f"no upstream tunes found in {samples}"
    return cases


SAMPLES = upstream_sample_tunes()


@pytest.fixture(scope="module")
def samples():
    if not SAMPLES:
        pytest.skip("set SYMUSIC_TEST_ABCMIDI_SOURCE to read the original sample tunes")
    return SAMPLES


def sample_for_comparison(name: str, abc: str) -> str:
    """Give each melody the same opening beat and declare the sample's program numbering."""
    if name == "drums/1":  # Only the backing drums sound.
        return abc
    settings = ""
    if "%%MIDI beat " not in abc:
        settings += COMPARISON_BEAT + "\n"
    if "%%MIDI chordprog " in abc and "%%MIDI programbase " not in abc:
        settings += "%%MIDI programbase 0\n"
    abc = re.sub(r"(?m)^(X\s*:.*\n)", r"\g<0>" + settings, abc, count=1)

    # The tune header sets the first voice's beat. Each additional voice has
    # its own dynamics, which persist when the music returns to that voice.
    header, body = abc.split("\nK:", 1)
    voices = set()

    def first_entry(match):
        voice = match[1] or match[2]
        initialize = bool(voices) and voice not in voices
        voices.add(voice)
        if not initialize:
            return match[0]
        return match[0] + ("\n" + COMPARISON_BEAT if match[1] else "!mf!")

    body = re.sub(r"(?m)^V:\s*(\w+)[^\n]*|\[V:\s*(\w+)[^]]*\]", first_entry, body)
    if name == "temperament/2":  # Its second melody enters as an overlay.
        body = body.replace("& [I:MIDI program 44]", "& !mf![I:MIDI program 44]", 1)
    return header + "\nK:" + body


@pytest.mark.parametrize("name", sorted(SAMPLES.keys() - NATIVE_SAMPLES))
def test_upstream_sample(name: str, upstream: Path, tmp_path: Path):
    abc = sample_for_comparison(name, SAMPLES[name])
    warning = SAMPLE_RECOVERY.get(name) or {"boys/3": "unexpected-character"}.get(name)
    with (
        pytest.warns(UserWarning, match=rf"\[{warning}\]") if warning else nullcontext()
    ):
        native = Score.from_abc(abc)
    reference = oracle_score(
        upstream, abc, tmp_path, expected_exit=int(name in SAMPLE_RECOVERY)
    )
    assert performed(native) == performed(reference)


def test_sample_opening_tempo(samples):
    score = Score.from_abc(samples["baym_rebin/63"])
    assert score.tempos == [Tempo(0, mspq=750_000)]


def test_sample_opening_controls(samples):
    track = Score.from_abc(samples["coleraine/8"]).tracks[0]
    assert track.controls == [ControlChange(0, 7, 115), ControlChange(0, 10, 67)]


def test_sample_chord_intervals(samples):
    backing = Score.from_abc(samples["chordname_long/1"]).tracks[1]
    assert sorted(n.pitch for n in backing.notes if n.time == 1) == [
        48,
        52,
        55,
        58,
        62,
        65,
        69,
    ]


def test_sample_decoration_spacing(samples):
    spaced = performed(Score.from_abc(samples["decoration_space/1"]))
    adjacent = performed(Score.from_abc(samples["decoration_space/2"]))
    assert spaced == adjacent
    # The final L is the accent shorthand in ABC 2.1 §4.16.
    assert spaced["tracks"][0]["notes"][-1] == [7201, 959, 84, 105]


def test_sample_pipe_key(samples):
    score = Score.from_abc(samples["demo/7"])
    assert score.key_signatures == [KeySignature(0, 0, 0)]
    assert score.tempos == [Tempo(0, mspq=1_500_000)]


def test_sample_chord_program_and_tie(samples):
    abc = samples["demo/8"]
    with pytest.raises(ValueError, match=r"\[invalid-midi-value\].*between 1 and 128"):
        Score.from_abc(abc)
    score = Score.from_abc(
        abc.replace("%%MIDI chordprog 0", "%%MIDI programbase 0\n%%MIDI chordprog 0")
    )
    # The written G-|B crosses pitches, so both notes sound (ABC 2.1 §4.11).
    assert [
        (n.time, n.pitch, n.duration)
        for n in score.tracks[0].notes
        if 12481 <= n.time <= 14401
    ] == [
        (12481, 67, 1919),
        (14401, 71, 959),
    ]


@pytest.mark.parametrize("settings", ["", "%%MIDI drone 1 45 57 80 80\n"])
def test_program_base_drone(settings: str, upstream: Path, tmp_path: Path):
    abc = HEADER + "%%MIDI programbase 1\n" + settings + "%%MIDI droneon\nCDEF|"
    native = Score.from_abc(abc)
    actual = oracle_score(upstream, abc, tmp_path)
    assert (
        native.tracks[-1].program
        == actual.tracks[-1].program
        == (0 if settings else 69)
    )


def test_default_bend_preserves_note_clock(upstream: Path, tmp_path: Path):
    abc = HEADER + "!bend!CDEF|"
    native = Score.from_abc(abc).tracks[0]
    actual = oracle_score(upstream, abc, tmp_path).tracks[0]
    assert [(e.time, e.value) for e in native.pitch_bends[:-1]] == [
        (e.time, e.value) for e in actual.pitch_bends[:-1]
    ]
    # abc2midi advances the MIDI delta when it resets the default bend curve.
    assert (native.pitch_bends[-1].time, native.pitch_bends[-1].value) == (480, 0)
    assert (actual.pitch_bends[-1].time, actual.pitch_bends[-1].value) == (487, 0)
    assert (native.notes[1].time, actual.notes[1].time) == (481, 488)


def test_upstream_ordered_verses(samples, upstream: Path, tmp_path: Path):
    abc = samples["demo/5"]
    assert lyric_rows(Score.from_abc(abc)) == lyric_rows(
        oracle_score(upstream, abc, tmp_path, "-lyrics")
    )


def test_upstream_single_voice_export(upstream: Path, tmp_path: Path):
    score = roundtrip_score()
    abc = score.dumps_abc()
    played = oracle_score(upstream, abc, tmp_path)
    assert note_rows(Score.from_abc(abc)) == note_rows(played) == note_rows(score)
    assert played.tracks[0].program == score.tracks[0].program
    assert played.time_signatures == score.time_signatures
    assert played.tempos == score.tempos


def test_upstream_issue78_export(upstream: Path, tmp_path: Path):
    score = Score(TESTCASES_PATH / "issue78_dump_abc.mid")
    abc = score.dumps_abc()
    played = oracle_score(upstream, abc, tmp_path)
    assert note_rows(Score.from_abc(abc)) == note_rows(played) == note_rows(score)


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


def midi_note_events(path: Path) -> dict[int, list[tuple[int, int]]]:
    events = {}
    tick = 0
    for message in merge_tracks(MidiFile(path).tracks):
        tick += message.time
        if message.type in {"note_on", "note_off"}:
            velocity = message.velocity if message.type == "note_on" else 0
            events.setdefault(message.note, []).append((tick, velocity))
    return events


def test_export_dense_chord_reattacks(upstream: Path, tmp_path: Path):
    score = Score(480)
    track = Track("Dense chords")
    track.notes.extend(
        Note(time, 30, pitch, 80) for time in (15, 45) for pitch in range(51)
    )
    score.tracks.append(track)
    abc = score.dumps_abc()
    midi = oracle_midi(upstream, abc, tmp_path)
    original = note_rows(score)
    assert note_rows(Score.from_abc(abc)) == original
    assert note_rows(Score(midi)) == [
        (t, d - (t == 15), p, v) for t, d, p, v in original
    ]
    assert midi_note_events(midi) == {
        pitch: [(15, 80), (44, 0), (45, 80), (75, 0)] for pitch in range(51)
    }


@pytest.mark.parametrize("duration", [2, 240])
@pytest.mark.parametrize("chord", [False, True])
def test_export_reattack_releases_before_attack(
    duration: int, chord: bool, upstream: Path, tmp_path: Path
):
    score = Score(480)
    track = Track()
    track.notes.extend([Note(1, duration, 60, 80), Note(1 + duration, 30, 60, 90)])
    if chord:
        track.notes.append(Note(1, duration, 64, 80))
    score.tracks.append(track)
    if chord:
        with pytest.warns(
            UserWarning,
            match=rf"track 1.*\[midi-chord-release\].*tick {1 + duration} \(480 TPQ\).*pitch 64",
        ):
            abc = score.dumps_abc(warn=True)
    else:
        abc = score.dumps_abc(warn=True)
    assert note_rows(Score.from_abc(abc)) == note_rows(score)
    # Score objects hide the order of encoded Note On and Note Off messages.
    expected = {60: [(1, 80), (duration, 0), (1 + duration, 90), (31 + duration, 0)]}
    if chord:
        expected[64] = [(1, 80), (duration, 0)]
    assert midi_note_events(oracle_midi(upstream, abc, tmp_path)) == expected


def test_export_reattack_bend_diagnostic_uses_planned_clock(
    upstream: Path, tmp_path: Path
):
    score = Score(480)
    track = Track()
    track.notes.extend(
        [Note(15, 30, 60, 80), Note(15, 30, 64, 80), Note(45, 30, 60, 90)]
    )
    track.pitch_bends.append(PitchBend(7, 50))
    score.tracks.append(track)
    with pytest.warns(
        UserWarning, match=r"\[midi-(bend-timing|chord-release)\]"
    ) as warnings:
        abc = score.dumps_abc(warn=True)
    messages = [str(notice.message) for notice in warnings]
    assert any("[midi-bend-timing]" in message for message in messages)
    assert any(
        "[midi-chord-release]" in message
        and "planned at playback tick 45 (480 TPQ)" in message
        for message in messages
    )
    wire = midi_note_events(oracle_midi(upstream, abc, tmp_path))
    assert wire[64][-1] == (37, 0)
    assert wire[60][2] == (38, 90)
    assert note_rows(Score.from_abc(abc)) == note_rows(score)


@pytest.mark.parametrize(("start", "duration"), [(1, 480), (481, 240)])
def test_export_tied_overlap_warns(
    start: int, duration: int, upstream: Path, tmp_path: Path
):
    score = single_note(1, 960)
    pitch = score.tracks[0].notes[0].pitch
    velocity = score.tracks[0].notes[0].velocity
    score.tracks[0].notes.append(Note(start, duration, pitch + 4, velocity))
    with pytest.warns(UserWarning, match=r"\[midi-tied-release\].*tick 961"):
        abc = score.dumps_abc(warn=True)
    assert note_rows(Score.from_abc(abc)) == note_rows(score)
    actual = oracle_score(upstream, abc, tmp_path)
    assert note_rows(actual) == sorted(
        [(1, 959, pitch, velocity), (start, duration, pitch + 4, velocity)]
    )


@pytest.mark.parametrize("voices", [1, 2])
def test_export_conductor_tempo(voices: int, upstream: Path, tmp_path: Path):
    score = Score(480)
    score.tempos.extend(
        [Tempo(0, mspq=500_000), Tempo(1, mspq=750_000), Tempo(720, mspq=625_000)]
    )
    for index in range(voices):
        track = Track(f"  Voice {index}  ", 24 + index)
        track.notes.extend(
            [Note(15, 240, 60 + index, 90), Note(961, 240, 62 + index, 90)]
        )
        score.tracks.append(track)
    abc = score.dumps_abc()
    native = Score.from_abc(abc)
    actual = oracle_score(upstream, abc, tmp_path)
    assert native.tempos == actual.tempos == score.tempos
    assert [track.name for track in native.tracks] == [
        track.name for track in score.tracks
    ]
    assert performed(native) == performed(actual)


@pytest.mark.parametrize("drum_position", [0, 7, 15])
def test_export_full_channel_set(drum_position: int, upstream: Path, tmp_path: Path):
    score = Score(480)
    for index in range(16):
        track = Track(str(index), index, index == drum_position)
        track.notes.append(Note(15, 60, 30 + index, 80))
        score.tracks.append(track)
    abc = score.dumps_abc()
    native = Score.from_abc(abc)
    actual = oracle_score(upstream, abc, tmp_path)
    assert note_rows(native) == note_rows(score)
    assert note_rows(actual) == sorted(
        (15, 60 - (index == drum_position), 30 + index, 80) for index in range(16)
    )
    assert {(t.program, t.is_drum) for t in actual.tracks} == {
        (t.program, t.is_drum) for t in score.tracks
    }


def notes_by_program(score: Score, *, shorten_held_lead: bool = False):
    result = {}
    for track in score.tracks:
        notes = result.setdefault(track.program, [])
        for note in track.notes:
            shorten = shorten_held_lead and track.program == 30 and note.time == 15
            notes.append(
                (note.time, note.pitch, note.velocity, note.duration - shorten)
            )
    return {program: sorted(notes) for program, notes in result.items()}


def test_export_reassigned_onset_plays_upstream(upstream: Path, tmp_path: Path):
    score = score_with_constrained_onset()
    played = oracle_score(upstream, score.dumps_abc(), tmp_path)
    assert len(played.tracks) == 15
    # All five held lead notes need ties across the second onset. abc2midi
    # releases those ties one tick early; compare every other field exactly.
    assert notes_by_program(played) == notes_by_program(score, shorten_held_lead=True)


def test_export_full_voice_set(upstream: Path, tmp_path: Path):
    score = Score(480)
    for index in range(30):
        # Alternate channel-10 tracks with all 15 melodic channels. Use
        # disjoint drum pitches so shared-channel releases cannot overlap.
        track = Track(str(index), index // 2 if index % 2 else 0, index % 2 == 0)
        track.notes.append(Note(15, 60, 30 + index, 80))
        score.tracks.append(track)
    abc = score.dumps_abc()
    native = Score.from_abc(abc)
    actual = oracle_score(upstream, abc, tmp_path)
    assert note_rows(native) == note_rows(score)
    assert note_rows(actual) == sorted(
        (15, 60 - (index % 2 == 0), 30 + index, 80) for index in range(30)
    )
    assert sorted((t.is_drum, t.program) for t in actual.tracks) == sorted(
        (t.is_drum, t.program) for t in score.tracks
    )


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


def test_export_control_order_warns(upstream: Path, tmp_path: Path):
    score = single_note(481, 240)
    score.tracks[0].controls.append(ControlChange(481, 120, 0))
    with pytest.warns(
        UserWarning,
        match=r"\[midi-control-timing\].*CC 120 at tick 481|\[midi-tied-release\]",
    ) as notices:
        abc = score.dumps_abc(warn=True)
    assert any("CC 120 at tick 481" in str(notice.message) for notice in notices)
    assert Score.from_abc(abc).tracks[0].controls == score.tracks[0].controls
    path = oracle_midi(upstream, abc, tmp_path)
    events = []
    tick = 0
    for event in merge_tracks(MidiFile(path).tracks):
        tick += event.time
        if event.type in {"note_on", "control_change"}:
            events.append((tick, event.type))
    assert events == [(481, "note_on"), (481, "control_change")]


def test_export_timed_notation_fields_warn(upstream: Path, tmp_path: Path):
    score = single_note(1, 240)
    score.key_signatures.append(KeySignature(35, 1, 0))
    score.time_signatures.append(TimeSignature(35, 3, 4))
    score.tempos.append(Tempo(35, 90))
    with pytest.warns(
        UserWarning, match=r"\[midi-field-(timing|release)\].*tick 35"
    ) as notices:
        abc = score.dumps_abc(warn=True)
    assert sum("[midi-field-timing]" in str(notice.message) for notice in notices) == 2
    assert sum("[midi-field-release]" in str(notice.message) for notice in notices) == 1
    native = Score.from_abc(abc)
    actual = oracle_score(upstream, abc, tmp_path)
    assert native.key_signatures[-1].time == native.time_signatures[-1].time == 35
    assert actual.key_signatures[-1].time == actual.time_signatures[-1].time == 1
    assert native.tempos[-1].time == actual.tempos[-1].time == 35


def test_export_notation_fields_across_held_voices(upstream: Path, tmp_path: Path):
    score = single_note(1, 240)
    held = Track("Held", 24)
    held.notes.append(Note(10, 360, 60, 80))
    score.tracks.append(held)
    score.key_signatures.append(KeySignature(35, 1, 0))
    score.time_signatures.append(TimeSignature(35, 3, 4))
    score.tempos.append(Tempo(35, 96))

    abc = score.dumps_abc()
    native = Score.from_abc(abc)
    actual = oracle_score(upstream, abc, tmp_path)
    assert note_rows(native) == note_rows(score)
    assert note_rows(actual) == [(1, 239, 36, 80), (10, 359, 60, 80)]
    assert actual.tempos == native.tempos
    assert native.key_signatures[-1].time == native.time_signatures[-1].time == 35


def test_export_one_tick_drum_warns(upstream: Path, tmp_path: Path):
    score = single_note(1, 1)
    score.tracks[0].is_drum = True
    with pytest.warns(UserWarning, match=r"\[midi-short-drum\].*tick 1"):
        abc = score.dumps_abc(warn=True)
    assert note_rows(Score.from_abc(abc)) == note_rows(score)
    assert midi_note_events(oracle_midi(upstream, abc, tmp_path))[36] == [
        (1, 80),
        (1, 0),
    ]


def test_export_bend_inside_sustain(upstream: Path, tmp_path: Path):
    score = Score(480)
    track = Track("Lead", 24)
    track.notes.extend([Note(15, 240, 60, 80), Note(400, 40, 64, 73)])
    score.tracks.append(track)
    assert note_rows(oracle_score(upstream, score.dumps_abc(), tmp_path)) == note_rows(
        score
    )
    track.pitch_bends.append(PitchBend(50, 1024))
    with pytest.warns(
        UserWarning, match=r"track 1.*\[(midi-bend-timing|midi-tied-release)\]"
    ) as notices:
        abc = score.dumps_abc(warn=True)
    assert any("[midi-bend-timing]" in str(notice.message) for notice in notices)
    native = Score.from_abc(abc)
    assert note_rows(native) == note_rows(score)
    assert list(native.tracks[0].pitch_bends) == [PitchBend(50, 1024)]
    # abc2midi writes the wheel at delta zero and drops the elapsed delta.
    actual = oracle_score(upstream, abc, tmp_path)
    assert note_rows(actual) == [(15, 203, 60, 80), (364, 40, 64, 73)]
    assert list(actual.tracks[0].pitch_bends) == [PitchBend(15, 1024)]


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
    with pytest.warns(UserWarning, match=r"\[midi-tied-release\].*tick 1000002"):
        abc = score.dumps_abc(warn=True)
    assert note_rows(Score.from_abc(abc)) == note_rows(score)
    # abc2midi does not apply %%MIDI expand to a tied note.
    assert note_rows(oracle_score(upstream, abc, tmp_path)) == [(1, 1_000_000, 36, 80)]


def test_export_sustain_range():
    score = single_note(1, 67_108_866)
    with pytest.warns(
        UserWarning, match=r"\[midi-(sustain-range|tied-release)\]"
    ) as notices:
        abc = score.dumps_abc(warn=True)
    assert any("[midi-sustain-range]" in str(notice.message) for notice in notices)
    assert note_rows(Score.from_abc(abc)) == note_rows(score)


@pytest.mark.parametrize("wanted", [1, 777_777, 7_689_274, 16_777_215])
def test_export_tempos(wanted: int, upstream: Path, tmp_path: Path):
    score = single_note(1, 480)
    score.tracks[0].notes.extend([Note(481, 480, 38, 80), Note(961, 480, 40, 80)])
    score.tempos.extend(
        [Tempo(0, mspq=wanted), Tempo(480, 120), Tempo(960, mspq=wanted)]
    )
    abc = score.dumps_abc(warn=True)
    assert Score.from_abc(abc).tempos == score.tempos
    assert oracle_score(upstream, abc, tmp_path).tempos == score.tempos


def refresh(executable: Path, revision: str) -> None:
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        cases = {
            name: performed(
                oracle_score(
                    executable, sample["abc"], root, expected_exit=sample.get("exit", 0)
                )
            )
            for name, sample in CASES.items()
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


@pytest.mark.parametrize("body", ["C!bend!-C D E|", "C!bend!>D E F|", "C>!bend!D E F|"])
def test_live_bend_mark_does_not_interrupt_rhythm(upstream, tmp_path, body):
    abc = HEADER + "[I:MIDI bendstring 300 -200] " + body
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


def test_live_bend_restore_precedes_a_same_tick_wheel_command(upstream, tmp_path):
    abc = (
        HEADER + "[I:MIDI bendstring 300 -200] !bend!C [I:MIDI pitchbend 127 127] D E|"
    )
    native = performed_track(Score.from_abc(abc).tracks[0])
    actual = performed_track(oracle_score(upstream, abc, tmp_path).tracks[0])
    assert (
        native["bends"]
        == actual["bends"]
        == [[1, 300], [240, 100], [480, 0], [480, 8191]]
    )
    assert native["notes"][1][0] == 481
    assert actual["notes"][1][0] == 480


@pytest.mark.parametrize(
    "directives",
    [
        "[I:MIDI control 11 60][I:MIDI controlstring 11 10]",
        "[I:MIDI controlstring 64 127]",
    ],
)
def test_live_single_value_shape_matches_controls_and_notes(
    upstream, tmp_path, directives
):
    abc = HEADER + directives + "!shape!CDEF|"
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


@pytest.mark.parametrize("use_aliases", [False, True])
def test_live_shape_combines_controller_and_bend_samples_without_moving_notes(
    upstream, tmp_path, use_aliases
):
    if use_aliases:
        abc = (
            "%%MIDIdef fade controlstring 11 10 30\n%%MIDIdef pan controlstring 10 50 60 70\n"
            + HEADER
            + "%%MIDIx fade pan\n"
        )
    else:
        abc = (
            HEADER + "[I:MIDI controlstring 11 10 30][I:MIDI controlcombo]"
            "[I:MIDI controlstring 10 50 60 70]"
        )
    abc += "[I:MIDI bendstring 100 -50 10]!shape!CDEF|"
    native = performed_track(Score.from_abc(abc).tracks[0])
    actual = performed_track(oracle_score(upstream, abc, tmp_path).tracks[0])
    assert (
        native["controls"][:-2]
        == actual["controls"][:-2]
        == [[1, 11, 10], [1, 10, 50], [160, 10, 60], [240, 11, 30], [319, 10, 70]]
    )
    assert (
        native["bends"][:-1] == actual["bends"][:-1] == [[1, 100], [160, 50], [319, 60]]
    )
    # abc2midi's envelope emitter truncates the gate and subtracts that time from
    # the rest of the tune. Both players restore the wheel and controllers at release.
    for result, release in [(native, 480), (actual, 416)]:
        assert result["controls"][-2:] == [[release, 11, 127], [release, 10, 64]]
        assert result["bends"][-1] == [release, 0]
        assert result["notes"][0][:2] == [1, release - 1]
        assert result["notes"][1][0] == release + 1


@pytest.mark.parametrize(
    "body",
    [
        "!shape!{A}CDEF|",
        "{A}!shape!CDEF|",
        "{AB}!shape!CDEF|",
        "!shape!{A}!bend!CDEF|",
        "[I:MIDI vol 0]!shape!CDEF|",
        "[I:MIDI vol 0]!shape!{A}CDEF|",
        "[I:MIDI channel 10]!shape!CDEF|",
        "[I:MIDI channel 10]!bend!CDEF|",
        "[I:MIDI expand 3/1]C- [I:MIDI expand 0/1] C !shape!D E F z|",
    ],
)
def test_live_shape_attacks_grace_mute_and_drum(upstream, tmp_path, body):
    abc = HEADER + "[I:MIDI controlstring 11 10][I:MIDI bendstring 300]" + body
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


@pytest.mark.parametrize(
    ("head", "body"),
    [
        ("%%MIDI harpmode 1\n", "~F ~F2 ~F3 ~F6/2 ~F/2 ~F3/ A|"),
        ("", "%%MIDI harpmode 1\n(3~FGA ~F- F4|"),
        ("", "~F [I:MIDI harpmode 1] ~F [I:MIDI harpmode 0] ~F A|"),
        ("", "|: ~F [I:MIDI harpmode 1] ~F F6 :| ~F A7|"),
        ("", "V:1\n~F [I:MIDI harpmode 1] ~F F6|\nV:2\n!mf!~A, A,7|"),
        ("%%MIDI harpmode 1\n", "RF TF ~F0 A|"),
        ("%%MIDI harpmode 1\n", "(3~A3 B3 c3 d2|"),
        ("%%MIDI harpmode 1\n", "(2:3:2~d2 e2 F2|"),
        ("%%MIDI harpmode 1\n", "H~G3 ~.B|"),
        ("%%MIDI harpmode 1\n%%MIDI fermatafixed\n", "H~G4/2 ~.B F4|"),
        ("%%MIDI harpmode 1\n", "~C>~D E|"),
        ("%%MIDI harpmode 1\n", "~C>D/2 E|"),
        ("%%MIDI harpmode 1\n", "C2>~D E|"),
        ("%%MIDI harpmode 1\n", "R~C T~D E|"),
        ("%%MIDI harpmode 1\n", "{G}~C2 D E F|"),
        ("%%MIDI harpmode 1\n", "{G}H~C D E F|"),
        ("R:hornpipe\n%%MIDI harpmode 1\n", "C>~D E F|"),
        ("R:hornpipe\n%%MIDI harpmode 1\n", "|: ~C>~D EF| G2 AB :|"),
    ],
)
def test_live_harp_cut(upstream, tmp_path, head, body):
    header = HEADER.replace("L:1/4", "L:1/8").replace("K:C", f"{head}K:D")
    abc = header + body
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


def test_live_harp_cut_rejects_grace_that_consumes_its_first_attack(upstream, tmp_path):
    abc = "%%MIDI harpmode 1\n" + HEADER.replace("L:1/4", "L:1/8") + "{G}~C D E F|"
    with pytest.warns(UserWarning, match=r"\[grace-too-long\].*first harp attack"):
        native = Score.from_abc(abc)
    assert performed(native) == performed(oracle_score(upstream, abc, tmp_path))


@pytest.mark.parametrize("mark", ["", "!bend!"])
def test_live_muted_microtone(upstream, tmp_path, mark):
    abc = HEADER + f"[I:MIDI bendstring 300][I:MIDI vol 0]{mark}^/CDEF|"
    native = Score.from_abc(abc)
    oracle = oracle_score(upstream, abc, tmp_path)
    assert performed(native) == performed(oracle)
    assert [(e.time, e.value) for e in native.tracks[0].pitch_bends] == [
        (e.time, e.value) for e in oracle.tracks[0].pitch_bends
    ]


@pytest.mark.parametrize(
    "body",
    [
        "!shape!{A}[I:MIDI channel 10]CDEF|",
        "!shape!{A}[I:MIDI vol 0]CDEF|",
        "D !shape!{A}[I:MIDI control 11 80] CDE|",
        "D !shape!{A}[I:MIDI bendstring -100]!bend!CDE|",
        "D {A}!pp!CDE|",
        "D>{A}[I:MIDI vol 0]CDE|",
        "D {F}[L:1/8]FGA|",
        "D {F}[Q:1/4=90]FGA|",
        "D {A}[I:MIDI control 11 80]z CDE|",
        '"C"D {A}"Dm"CDE|',
    ],
)
def test_live_grace_precedes_trailing_effects(upstream, tmp_path, body):
    abc = HEADER + "[I:MIDI controlstring 11 10][I:MIDI bendstring 300]" + body
    native = Score.from_abc(abc)
    oracle = oracle_score(upstream, abc, tmp_path)
    assert performed(native) == performed(oracle)


@pytest.mark.parametrize("host", ["CDE|", "Z CDE|", "(5:3CDEFGAB|"])
def test_live_grace_precedes_meter_change(upstream, tmp_path, host):
    abc = HEADER.replace("L:1/4", "L:1/8") + "D {A}[M:6/8]" + host
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


@pytest.mark.parametrize(
    "music",
    [
        "(3C{d}E F G |]",
        "{d}(3CEF G |]",
        "(3CEF {d}G |]",
        "%%MIDI gracedivider 6\n(3C{d}E F G |]",
        "%%MIDI grace 1/4\n(3C{d}E F G |]",
        "(2{d}CD E |]",
    ],
)
def test_live_grace_timing_in_tuplets(upstream: Path, tmp_path: Path, music: str):
    abc = HEADER + music
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


def test_live_grace_keeps_its_written_key(upstream, tmp_path):
    abc = HEADER + "D {F}[K:G]FGA|"
    native = Score.from_abc(abc)
    oracle = oracle_score(upstream, abc, tmp_path)
    assert note_rows(native) == note_rows(oracle)


def test_live_harp_cut_rejects_unequal_broken_attacks(upstream, tmp_path):
    abc = "%%MIDI harpmode 1\n" + HEADER + "~C>D EF|"
    with pytest.warns(
        UserWarning, match=r"<string>:9:3: ABC \[unequal-broken-rhythm\]"
    ):
        native = Score.from_abc(abc)
    oracle = Score(oracle_midi(upstream, abc, tmp_path, expected_exit=1))
    assert performed(native) == performed(oracle)


@pytest.mark.parametrize("body", ["C-CDEF|", "C- C- C D|", "C4-|C2 D2|"])
def test_live_muted_tie(upstream, tmp_path, body):
    abc = HEADER + "[I:MIDI controlstring 11 10][I:MIDI vol 0]!shape!" + body
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


@pytest.mark.parametrize(
    "body",
    [
        "CDEF CDEF | [R:hornpipe] EFGA Bcde |]",
        "CD E\nR:Hornpipe\nF GABC |]",
        "[R:hornpipe] CDEF CDEF | [R:reel] EFGA Bcde |]",
        "|: CDEF CDEF | [R:hornpipe] EFGA Bcde :|",
        "[I:MIDI ratio 3 1] C>[R:hornpipe] D EF GABC |]",
        "[R:hornpipe] C>[I:MIDI ratio 3 1] D E[R:Hornpipe]F GABC |]",
        (
            "[V:1] CDEF CDEF | [V:2] !mf![R:hornpipe] cdef cdef | "
            "[V:1] EFGA Bcde | [V:2] efga bc'd'e' |]"
        ),
    ],
)
def test_live_written_hornpipe(upstream, tmp_path, body):
    abc = HEADER.replace("L:1/4", "L:1/8") + body
    # ABC 2.1 allows R: in the body. abc2midi reports an error but renders it.
    native = Score.from_abc(abc)
    oracle = oracle_score(upstream, abc, tmp_path, expected_exit=1)
    assert [track.notes for track in native.tracks] == [
        track.notes for track in oracle.tracks
    ]


@pytest.mark.parametrize(
    ("head", "body"),
    [
        ("", "|: C>D z6| E>[I:MIDI ratio 2 1]F z6 :|"),
        ("P:BAB\n", "P:A\nC>D z6|\n%%MIDI ratio 2 1\nP:B\nE>F z6|"),
    ],
)
def test_live_written_ratio_navigation(upstream, tmp_path, head, body):
    abc = HEADER.replace("L:1/4", "L:1/8").replace("K:C", f"{head}K:C") + body
    assert note_rows(Score.from_abc(abc)) == note_rows(
        oracle_score(upstream, abc, tmp_path)
    )


@pytest.mark.parametrize(
    ("header", "body", "expected_exit"),
    [
        ("M:4/4\nL:1/8\nR:hornpipe\n", "C[M:6/8]DEFGA |]", 0),
        (
            "M:2/4\nL:1/16\nR:hornpipe\n",
            "CDEF GABC | [M:4/4] CDEF GABC CDEF GABC |]",
            0,
        ),
        ("M:none\nL:1/8\nR:hornpipe\n", "CDEF GABC |]", 0),
        ("L:1/8\nR:hornpipe\n", "CDEF GABC |]", 0),
        ("M:C|\nL:1/16\n", "[R:hornpipe] CDEF GABC CDEF GABC |]", 1),
        ("M:6/8\nL:1/8\n", "[R:hornpipe] C>DEFGA | [M:4/4] CDEF GABC |]", 1),
    ],
)
def test_live_hornpipe_uses_opening_meter(
    upstream, tmp_path, header, body, expected_exit
):
    # Isolate timing from the two players' different default accents.
    abc = f"%abc-2.1\nX:1\nT:Hornpipe\n{header}%%MIDI beat 90 90 90 2\nK:C\n{body}"
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path, expected_exit=expected_exit)
    )


@pytest.mark.parametrize(
    ("head", "body"),
    [
        ("", "C>[L:1/16]D2 E2F2 G2A2B2c2 |]"),
        ("R:hornpipe\n", "C[L:1/8][Q:1/4=90]DEF GABC |]"),
        ("", "C>{G}[L:1/16]D2 E2F2 G2A2B2c2 |]"),
        ("", "C>[L:1/16]{G}D2 E2F2 G2A2B2c2 |]"),
        ("%%MIDI harpmode 1\n", "~C>[L:1/16]D EF |]"),
        ("%%MIDI harpmode 1\n", "C>[L:1/16]~D2 E2F2 G2A2B2c2 |]"),
        ("%%MIDI fermatafixed\n", "{G3}[L:1/16]HD E2F2 |]"),
    ],
)
def test_live_broken_rhythm_across_unit_field(upstream, tmp_path, head, body):
    abc = HEADER.replace("L:1/4", "L:1/8").replace("K:C", f"{head}K:C") + body
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


@pytest.mark.parametrize(
    ("opening", "body"),
    [
        (
            "K:C\n%%MIDI transpose 5\nI:MIDI rtranspose -2\n",
            "[V:one] CDEF |]\n[V:two] !mf!GABc |]",
        ),
        ("K:C\n%%MIDI rtranspose 3\n", "[V:one] CDEF |]\n[V:two] !mf!GABc |]"),
        (
            "V:one\nV:two\nK:C\n%%MIDI transpose 5\n",
            "[V:one] CDEF |]\n[V:two] !mf!GABc |]",
        ),
        ("K:C\n", "[V:one] C [I:MIDI transpose 5] DEF |]\n[V:two] !mf!GABc |]"),
        ("K:C\n%%MIDI rtranspose 3\n", '[V:one] "C" CDEF |]\n[V:two] !mf!"G" GABc |]'),
        ("K:C\n%%MIDI rtranspose 3\n", "C4 & !mf!D4 |]"),
        ("K:C\n", "|: C2 [I:MIDI rtranspose 1] D2 :| E2 F2 |]"),
        ("K:C\n", '"C" C [I:MIDI rtranspose 2] D E F |]'),
    ],
)
def test_live_midi_transpose_track_replays(upstream, tmp_path, opening, body):
    abc = HEADER.replace("K:C\n", opening) + body
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


@pytest.mark.parametrize(
    "body",
    [
        '|: "C"C D :| E F G A |]',
        "CDEF |: G A :| B c |]",
        "P:A\nCDEF |\nP:B\n |: G A :| B c |]",
    ],
)
def test_live_soft_repeat_pickup(upstream, tmp_path, body):
    abc = HEADER + body
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


@pytest.mark.parametrize("bars", [-1, 0, 11])
@pytest.mark.parametrize(
    ("command", "setup"),
    [
        ("gchordbars", '%%MIDI gchord fghc\n"C"'),
        ("drumbars", "%%MIDI drum dddd 36 37 38 39\n%%MIDI drumon\n"),
    ],
)
def test_live_out_of_range_backing_span(upstream, tmp_path, bars, command, setup):
    abc = f"{HEADER}%%MIDI {command} {bars}\n{setup}CDEF | GABc |]"
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


@pytest.mark.parametrize(
    "body",
    [
        '%%MIDI gchordbars 10\n%%MIDI gchord fghc\n"C"CDEF | GABc | cBAG | FEDC |]',
        '%%MIDI gchord fghc\n"C"CD\n%%MIDI gchordbars 10\n%%MIDI gchord fghc\nEF | GABc | cBAG | FEDC |]',
    ],
)
def test_live_long_harmony_spans(upstream, tmp_path, body):
    abc = HEADER + body
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )


@pytest.mark.parametrize(
    "body",
    [
        "%%MIDI chordattack 4\n[V:one] [CEG] DEF |]\n[V:two] !mf![GBe] ABc |]",
        "[V:one] [CEG] [I:MIDI chordattack 4] DEF |]\n[V:two] !mf![GBe] ABc |]",
        "[V:one] [CEG] DEF |]\n[V:two] !mf![GBe] [I:MIDI chordattack 4] ABc |]",
        "%%MIDI chordattack 4\nC4 & !mf![CEG] DEF |]",
        '%%MIDI chordattack 4\n%%MIDI gchordbars 10\n%%MIDI gchord fghc\n"C"CDEF | GABc | cBAG | FEDC |]',
        (
            "%%MIDI chordattack 4\n%%MIDI drumbars 10\n%%MIDI drum dddd 36 37 38 39\n"
            "%%MIDI drumon\nCDEF | GABc | cBAG | FEDC |]"
        ),
    ],
)
def test_live_chord_gap_track_state(upstream, tmp_path, body):
    abc = HEADER + body
    assert performed(Score.from_abc(abc)) == performed(
        oracle_score(upstream, abc, tmp_path)
    )
