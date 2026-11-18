"""Differential ABC fixtures. The upstream executable is optional and test-only."""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import tempfile
from contextlib import nullcontext
from itertools import groupby
from pathlib import Path

import pytest
from symusic import ControlChange, Note, Pedal, Score, Tempo, Track

from tests.test_abc import HEADER, note_rows, roundtrip_score
from tests.utils import TESTCASES_PATH

SNAPSHOT = TESTCASES_PATH / "abc_oracle.json"
DETUNE = "%%temperament +00 -06 -04.5 -02 -08 +02 -08 -02 -04 -06 +00 -10\n"
RECOVERED = {
    "decoration_line_end": "orphan-decoration",
    "rising_sun_stray_coda": "ignored-decoration",
    "hornpipe_unsupported_meter": "hornpipe-meter",
    "invalid_key_accidental_letter": "invalid-accidental",
}
WARNINGS = RECOVERED | {"chord_channels": "shared-pitch-wheel"}
CASES = {
    "straight": HEADER + "C D E F | G A B c |",
    "rests": HEADER + "C z2 D | x E2 z |",
    "engraving_block_after_continuation": HEADER
    + "CD\\\n%%begintext\nGABc|\n%%endtext% comment\nEF|",
    "tuning_after_continuation": HEADER + "C\\\n" + DETUNE + "DEF|",
    "engraving_spacers_in_groups": HEADER + "[Cy2E] D {Fy/G} A B|",
    "engraving_blocks": "%%begintext justify\nThis is not music.\nK:None\n%%endtext\n"
    + HEADER
    + "C y2 D y E y/ F|\n%%beginps\nK:D\nCzC \\\n%%endps\nGABc|",
    "tuplets": HEADER + "(3CDE (5:4:5 FGABc |",
    "broken_rhythm": HEADER + "C>D E<F G>>A B<<c |",
    "trill_on_tie_continuation": HEADER + "C- TC D E| ^C-| TC D E F|",
    "roll_on_tie_continuation": HEADER + "C- RC D E|",
    "grace_breaks_tie": HEADER + "C- {D}C D E|",
    "cut_breaks_tie": HEADER + "C- ~C- C D|",
    "arpeggio_next_chord": HEADER + "!arpeggio!C C [DFA] z|[CEG] z3|",
    "arpeggio_chord_spread": HEADER + "%%MIDI chordattack 4\n!arpeggio![CEG] [DFA] z2|",
    "one_member_chord_consumes_arpeggio": HEADER + "!arpeggio![C] [DFA] z2|",
    "pedal_end": HEADER + "CDEF|\n!ped!C D !ped-end!E F|",
    "chords_ties": HEADER + "[CEG] [D2F3A]2 | [FA]-[FA] C-C |",
    "grace": HEADER + "{DE}F {G}A B c |",
    "repeat_endings": HEADER + "C |: D E |1 F :|2 G | A |: B :|",
    "final_numbered_ending": HEADER + "|: C2 [1-2 D2 :| [3 E2 |]",
    "broken_dynamic": HEADER + "C>[K:G]!p!F G A |",
    "overlay_tie": HEADER + "C2 & E2- | D2 & E2 |",
    "part_label_inside_measure": HEADER.replace("K:C", "P:(AB)2\nK:C")
    + "P:A\nCDEF| G A\nP:B\nB c d e|",
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
    "bagpipe_key": HEADER.replace("K:C", "K:HP") + "CDEF| [K:Hp] CDEF|",
    "invalid_key_accidental_letter": HEADER.replace("K:C", "K:C ^1/4H") + "CDEF|",
    "half_note_meter_accents": HEADER.replace("M:4/4", "M:6/2") + "C4 C2 C2 C4|",
    "odd_meter_accents": HEADER.replace("M:4/4", "M:7/8").replace("L:1/4", "L:1/8")
    + "CDEFGAB|",
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
    "directives": HEADER + "%%MIDI program 24\n"
    "%%MIDI beat 90 80 60 2\nC D E F |\n%%MIDI program 7\n%%MIDI channel 10\nC D E F |",
    "single_note_volume": HEADER
    + "C [I:MIDI vol 30] D [I:MIDI volinc 30] E F |\n%%MIDI vol 110\nGABc|",
    "single_note_volume_order": HEADER
    + "[I:MIDI vol 60][I:MIDI volinc -35] C D [I:MIDI vol 127] [EGB] F|",
    "single_note_volume_tie": HEADER + "C- [I:MIDI vol 30] C D E|",
    "single_note_volume_grace": HEADER + "C [I:MIDI vol 30] {DE} F G A |",
    "controllers": HEADER + "%%MIDI control 7 50\nCDEF|\n%%MIDI control 7 40\nGABc|"
    "[I:MIDI control 7 30]cBAG|",
    "beatstring_positions": HEADER.replace("L:1/4", "L:1/8")
    + "%%MIDI beatstring pFmPffmP\nC D E F G A B c |",
    "hornpipe_eighths": HEADER.replace("L:1/4", "L:1/8").replace(
        "K:C", "R:hornpipe\nK:C"
    )
    + "C| DEFG ABcd | C>D E<F G>A B<c |",
    "hornpipe_sixteenths": HEADER.replace("M:4/4", "M:2/4").replace(
        "K:C", "R:Hornpipe\nK:C"
    )
    + "C//D//E//F// G//A//B//c//| C<D E>F|",
    "hornpipe_chords_and_tuplets": HEADER.replace("L:1/4", "L:1/8").replace(
        "K:C", "R:hornpipe\nK:C"
    )
    + 'CzE[FG] {A}Bcd|C (3DEF G/A/ Bc| C D "C"E F G "C"A B c|',
    "hornpipe_unsupported_meter": HEADER.replace("M:4/4", "M:6/8")
    .replace("L:1/4", "L:1/8")
    .replace("K:C", "R:hornpipe\nK:C")
    + "C<D E>F G<A|",
    "rhythm_ratio": HEADER + "%%MIDI ratio 2 1\nC>D E<F| G>>A B<<c|",
    "fermata_modes": HEADER + "%%MIDI fermatafixed\nHC2 D Hz |\n"
    "%%MIDI fermataproportional\nH[EG]2 F G|",
    "roll_octaves_and_key": HEADER.replace("K:C", "K:G") + "RB RC RE RF |",
    "ornament_continuation": HEADER + "CDE !fermata!\\\n F |",
    "trill_slow_meter": HEADER.replace("L:1/4", "L:1/16").replace("=120", "=110")
    + "TC T D2 T E3 T F4 TA6|",
    "trill_rhythm": HEADER + "(3TCDE TC>F T A/ B/|",
    "trill_header_speed": HEADER.replace("=120", "=240") + "TCDE|TC2DE|",
    "trill_tempo_change": HEADER + "TCDE|[Q:90]TC2DE|",
    "trill_unit_change": HEADER + "CDEF|[L:1/16]TC TD2 TE3 TF10|",
    "staccatissimo_mark": HEADER + "' C ' [DE] F G|",
    "trill_beat": HEADER + "TCDE|TC2DE|",
    "trim_and_slurs": HEADER + "%%MIDI trim 1/5\n(C/D/) (.EF) (G .A) z2|",
    "trim_staccato_and_short_notes": HEADER
    + "%%MIDI trim 1/5\n.C .D/ .[EG] .F// G/8 A B|",
    "trim_tied_notes": HEADER + "%%MIDI trim 1/5\nC-C D E| F-F-F G|",
    "expand_slurred_notes": HEADER + "%%MIDI expand 1/5\n(C D) .E z|",
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
    "detune_reset": HEADER + DETUNE + "^b _c ^/D E|\n%%MIDI temperamentnormal\nCDEF|",
    "detune_repeat": HEADER + "|: CDEF|\n" + DETUNE + "GABc:|",
    "detune_voices": HEADER
    + "V:one\nCDEF|\n"
    + DETUNE
    + "V:two\nCDEF|z4|\nV:one\nGABc|",
    "detune_header": HEADER.replace("K:C", DETUNE + "K:C") + "CDEF|",
    "detune_before_tune": DETUNE + HEADER + "CDEF|",
    "detune_transpose": HEADER + DETUNE + "%%MIDI transpose 2\n%%MIDI C 63\nCDEF|",
    "microtone_carry": HEADER + "^/F F _3/4E E | F E ^/4F =F |",
    "microtone_wide": HEADER.replace("M:4/4", "M:1/4")
    + "^9/4F|_9/4F|^3/2F|^2F|^3F|_3F|",
    "microtone_tie_wheel": HEADER + "^/C- _/C D E|",
    "microtone_tie": HEADER + "^/C4- | C4 | D4 |",
    "microtone_key": HEADER.replace("K:C", "K:G _/B _3/4e _3/4A")
    + "B e A G|_/B _3/4e _3/4A G|",
    "free_meter_soft_bars": HEADER.replace("M:4/4", "M:none")
    + "CDEF||GABc|dcBA||GFED|",
    "backing_partial_final_bar": HEADER + "%%MIDI gchord fc\n"
    "%%MIDI drum dd 35 42 80 81\n%%MIDI drumon\n"
    '"C"CDEF|"G"GAB|',
    "chord_channels": HEADER
    + "%%MIDI makechordchannels 2\n[^/C E _/G] ^/C [C^/EFG] D|",
    "chord_channels_programs": HEADER + "%%MIDI program 3\n%%MIDI makechordchannels 2\n"
    "[CEG]\n%%MIDI program 4\n[CEG] CD|",
    "chord_channels_controls": HEADER + "%%MIDI makechordchannels 2\n"
    "[^/C E _/G] [I:MIDI control 7 40] [CEG] F G|",
    "chord_channels_voices": HEADER + "V:1\n%%MIDI makechordchannels 2\n[CEG] C D E|"
    "\nV:2\n[CEG] C D E|",
    "chord_channels_temperament": HEADER + "%%MIDI temperamentlinear 1200 702\n"
    "%%MIDI makechordchannels 3\n[C_22/100EG]2 z [CEG] |",
    "temperament_linear": HEADER + "%%MIDI temperamentlinear 1200 702\n"
    "CD _22/100E G|\n%%MIDI temperamentnormal\nCDEF|",
    "temperament_equal": HEADER
    + "%%MIDI temperamentequal 31\nC ^/C ^C ^3C D _/D _D _3D|",
    "temperament_equal_steps": HEADER
    + "%%MIDI temperamentequal 11 1200 7 1\nC __D _D D ^D ^^D E G|",
    "temperament_stretched_octave": HEADER
    + "%%MIDI temperamentequal 19 1205\nCDEFGABc|",
    "middle_c_header": HEADER.replace("K:C", "%%MIDI C 61\nK:C") + "CDEF|",
    "middle_c_file": "%%MIDI C 61\n" + HEADER + "CDEF|",
    "middle_c_repeat": HEADER + "|: CDEF|\n%%MIDI C 63\nGABc:|",
    "middle_c_voices": HEADER
    + "V:one\nCDEF|\n%%MIDI C 63\nV:two\nCDEF|z4|\nV:one\nGABc|",
    "middle_c_temperament_reset": HEADER + "%%MIDI C 61\nCDEF|\n"
    "%%MIDI temperamentlinear 1200 702\nCDEF|\n%%MIDI C 61\nCD [I:MIDI temperamentnormal] EF|",
    "midi_middle_c": HEADER + "%%MIDI C 62\nCDEF |",
    "harmony": HEADER + '"C/E"C D E F | "G7"G A B c |',
    "drums_enable_without_restarting": HEADER
    + "%%MIDI drum dddd 35 42 37 38\nCD [I:MIDI drumon] EF|",
    "drums_mute_without_restarting": HEADER
    + "%%MIDI drum dddd 35 42 37 38\n%%MIDI drumon\nC[I:MIDI drumoff]DE[I:MIDI drumon]F|",
    "drums_replace_between_slots": HEADER
    + "%%MIDI drum dddd 35 42 37 38\n%%MIDI drumon\nC/[I:MIDI drum dd 39 43] D3/ EF|",
    "harmony_replace_between_slots": HEADER
    + '%%MIDI gchord fghc\n"C"C/ [I:MIDI gchord h2g2] D3/ EF|',
    "harmony_bar_span_before_definition": HEADER
    + '%%MIDI gchordbars 2\n%%MIDI gchord fghcffgh\n"C"CDEF|GABc|cBAG|FEDC|',
    "harmony_bar_span_after_definition": HEADER
    + '%%MIDI gchord fghc\n%%MIDI gchordbars 2\n"C"CDEF|GABc|cBAG|FEDC|',
    "drum_spacing_survives_meter_change": HEADER
    + "%%MIDI drum dddd 35 42 37 38\n%%MIDI drumon\nCDEF|[M:3/4]GAB|",
    "harmony_meter_keeps_pending_slot": HEADER
    + '%%MIDI gchord fghc\n"C"CD[M:3/4]EF|"C"GAB|',
    "harmony_stops_after_one_pattern_per_long_bar": HEADER
    + '%%MIDI gchord fghc\n"C"C12|C4|',
    "drums_multibar_pickup": HEADER
    + "%%MIDI drumbars 2\n%%MIDI drum dddddddd 35 42 37 38 39 40 41 43\n%%MIDI drumon\nCD|GABc|cBAG|FEDC|",
    "drums_fractional_slot_ticks": HEADER
    + "%%MIDI drum ddddddd 35 42 37 38 39 40 41\n%%MIDI drumon\nCDEF|GABc|",
    "quintuple_meter_backing": HEADER.replace("M:4/4", "M:5/4") + '"C"C5|',
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
    if name in WARNINGS:
        with pytest.warns(UserWarning, match=rf"\[{WARNINGS[name]}\]"):
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


def test_shorthand_decoration_spelling(upstream: Path, tmp_path: Path):
    abc = HEADER + "'C !'! D !H! E !T! F|"
    with pytest.warns(UserWarning, match=r"\[ignored-decoration\]") as warnings:
        native = Score.from_abc(abc)
    assert len(warnings) == 3
    assert performed(native) == performed(oracle_score(upstream, abc, tmp_path))


def test_control_after_program(upstream: Path, tmp_path: Path):
    abc = HEADER + "%%MIDI program 24\n%%MIDI control 7 50\nCDEF|"
    native = performed_track(Score.from_abc(abc).tracks[0])
    actual = performed_track(oracle_score(upstream, abc, tmp_path).tracks[0])
    # MIDI writes the program on the pickup tick; the controller inherits
    # that delta in abc2midi. Native controllers use the written clock.
    assert native.pop("controls") == [[0, 7, 50]]
    assert actual.pop("controls") == [[1, 7, 50]]
    assert native == actual


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


# Pin exceptions at the affected rows so the rest of each tune is still compared.
SAMPLE_DIFFERENCES = {
    # The body tempo at time zero supersedes the implicit default.
    "baym_rebin/63": (("tempos",), [], [[0, 500000]]),
    # The MIDI queue schedules the first full backing chord twice at the same tick.
    "chordname_long/1": (
        ("tracks", 1, "notes"),
        [],
        [[1, 1919, pitch, 75] for pitch in (48, 52, 55, 58, 62, 65, 69)],
    ),
    # Native controllers keep their written time; the MIDI program lands at tick 1.
    "coleraine/8": (
        ("tracks", 0, "controls"),
        [[0, 7, 115], [0, 10, 67]],
        [[1, 7, 115], [1, 10, 67]],
    ),
    # Two pitches start and release together in the score. The converter applies
    # the program-change delta again for each drone note-on.
    "demo/7": (
        ("tracks", 1, "notes"),
        [[1, 30719, 33, 90], [1, 30719, 45, 90]],
        [[2, 30721, 45, 90], [3, 30720, 33, 90]],
    ),
    # G tied to B across a barline. Preserve both pitches rather than dropping B.
    "demo/8": (
        ("tracks", 0, "notes"),
        [[12481, 1919, 67, 105], [14401, 959, 71, 105]],
        [[12481, 2879, 67, 105]],
    ),
}
SAMPLE_RECOVERY = {
    "chordname_too_long/1": "invalid-chord-name",
    "decoration_line_end/1": "orphan-decoration",
    "microtone/1": "invalid-accidental",
    "microtone_accidentals/1": "invalid-accidental",
    "microtone_accidentals/2": "invalid-accidental",
}


def upstream_sample_tunes():
    if not (source := os.environ.get("SYMUSIC_TEST_ABCMIDI_SOURCE")):
        return [
            pytest.param(
                None,
                "",
                "",
                marks=pytest.mark.skip(reason="set SYMUSIC_TEST_ABCMIDI_SOURCE"),
            )
        ]
    samples = Path(source) / "samples"
    cases = []
    for path in sorted(samples.glob("*.abc")):
        abc = path.read_text(encoding="utf-8-sig")
        header, *tunes = re.split(r"(?=^[ \t]*X\s*:)", abc, flags=re.MULTILINE)
        for tune in tunes:
            reference = re.match(r"\s*X\s*:\s*(\d+)", tune).group(1)
            cases.append(
                pytest.param(
                    path, reference, header + tune, id=f"{path.stem}/{reference}"
                )
            )
    assert cases, f"no upstream tunes found in {samples}"
    return cases


def drop_sample_rows(events: dict, path: tuple, rows: list[list[int]]) -> None:
    for part in path:
        events = events[part]
    for row in rows:
        events.remove(row)


@pytest.mark.parametrize(("path", "reference", "abc"), upstream_sample_tunes())
def test_upstream_sample(
    path: Path, reference: str, abc: str, upstream: Path, tmp_path: Path
):
    label = f"{path.stem}/{reference}"
    output = tmp_path / "sample.mid"
    result = subprocess.run(
        [upstream.resolve(), path, reference, "-o", output, "-silent"],
        capture_output=True,
        text=True,
        check=False,
        timeout=15,
    )
    assert result.returncode == int(label in SAMPLE_RECOVERY), (
        result.stdout + result.stderr
    )
    expected = performed(Score(output))
    warning = SAMPLE_RECOVERY.get(label) or {"boys/3": "unexpected-character"}.get(
        label
    )
    with (
        pytest.warns(UserWarning, match=rf"\[{warning}\]") if warning else nullcontext()
    ):
        actual = performed(Score.from_abc(abc))
    if difference := SAMPLE_DIFFERENCES.get(label):
        location, native_rows, upstream_rows = difference
        drop_sample_rows(expected, location, upstream_rows)
        drop_sample_rows(actual, location, native_rows)
    assert actual == expected


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
