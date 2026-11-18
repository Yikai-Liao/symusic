from __future__ import annotations

import warnings
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import pytest
from symusic import (
    ControlChange,
    KeySignature,
    Note,
    Pedal,
    Score,
    Tempo,
    TimeSignature,
    Track,
)

from tests.utils import TESTCASES_PATH

HEADER = "X:1\nT:Roundtrip\nM:4/4\nL:1/4\nQ:1/4=120\nK:C\n"
ABC_PATHS = sorted((TESTCASES_PATH / "abc_files").glob("*.abc"))


def note_rows(score: Score) -> list[tuple[int, int, int, int]]:
    return sorted(
        (n.time, n.duration, n.pitch, n.velocity)
        for track in score.tracks
        for n in track.notes
    )


def roundtrip_score() -> Score:
    score = Score(480)
    track = Track("Roundtrip", 24, False)
    track.notes.extend(
        [Note(1, 480, 60, 96), Note(481, 480, 62, 96), Note(961, 960, 64, 96)]
    )
    score.tracks.append(track)
    score.tempos.append(Tempo(0, 120))
    score.time_signatures.append(TimeSignature(0, 4, 4))
    return score


@pytest.mark.parametrize("abc_path", ABC_PATHS, ids=lambda path: path.name)
def test_read_abc(abc_path: Path):
    assert Score(abc_path, fmt="abc").note_num() > 0


def test_read_abc_recovers_stray_coda():
    path = TESTCASES_PATH / "abc_invalid" / "rising_sun_stray_coda.abc"
    with pytest.warns(
        UserWarning,
        match=r"rising_sun_stray_coda\.abc:(7:11|10:33): ABC \[ignored-decoration\]",
    ) as caught:
        score = Score(path)
    assert len(caught) == 2
    assert score.note_num() == 74


@pytest.mark.parametrize(
    ("body", "code", "line", "column"),
    [
        ("C4 [DF", "malformed-group", 7, 4),
        ("CD [K:G", "unclosed-inline-field", 7, 4),
        ("C/0", "zero-length-divisor", 7, 2),
        ("[I:MIDI program 128] C", "invalid-midi-value", 7, 2),
        ("C,,,,,,,,", "pitch-range", 7, 1),
    ],
)
def test_abc_fatal_diagnostics(body: str, code: str, line: int, column: int):
    with pytest.raises(ValueError, match=rf"<string>:{line}:{column}: ABC \[{code}\]"):
        Score.from_abc(HEADER + body)


def test_missing_key_not_recovered():
    with pytest.raises(ValueError, match=r"<string>:3:1: ABC \[missing-key\]"):
        Score.from_abc("X:1\nM:4/4\nCDEF\n")


@pytest.mark.parametrize("bad_byte", [b"\xff", b"\x00"])
def test_abc_rejects_invalid_text_at_its_byte_column(tmp_path: Path, bad_byte: bytes):
    path = tmp_path / "bad text.abc"
    path.write_bytes(HEADER.encode() + '"雪"!'.encode() + bad_byte + b"! CDEF")
    with pytest.raises(ValueError, match=r"bad text\.abc:7:7: ABC \[invalid-text\]"):
        Score(path)


def test_abc_warns_on_second_tune_and_recovers_nonbinary_duration():
    text = HEADER + "C/3 D/3\n\n" + HEADER.replace("X:1", "X:2") + "GAB"
    with pytest.warns(
        UserWarning, match=r"\[(non-binary-length|extra-tunes)\]"
    ) as warnings:
        score = Score.from_abc(text)
    assert [str(w.message).split(": ABC ")[0] for w in warnings] == [
        "<string>:7:2",
        "<string>:7:6",
        "<string>:9:1",
    ]
    assert [row[:3] for row in note_rows(score)] == [(1, 119, 60), (121, 119, 62)]


def test_abc_rejects_negative_pedal_even_with_explicit_cc64():
    score = roundtrip_score()
    score.tracks[0].pedals.append(Pedal(10, -1))
    score.tracks[0].controls.append(ControlChange(10, 64, 127))
    with pytest.raises(ValueError, match="pedal duration cannot be negative"):
        score.dumps_abc()


def test_abc_warn_as_error_leaves_output_untouched(tmp_path: Path):
    score = roundtrip_score()
    score.tracks[0].notes.append(Note(0, 10, 62, 80))
    path = tmp_path / "score.abc"
    path.write_text("untouched", encoding="utf-8")
    with warnings.catch_warnings():
        warnings.simplefilter("error")
        with pytest.raises(UserWarning, match=r"\[note-start-offset\]"):
            score.dump_abc(path, warn=True)
    assert path.read_text(encoding="utf-8") == "untouched"


def test_abc_malformed_slash_bass_uses_the_chord_root():
    with pytest.warns(UserWarning, match=r"<string>:7:1: ABC \[invalid-chord-bass\]"):
        score = Score.from_abc(HEADER + '"C/Eextra"CDEF|')
    assert [n.pitch for n in score.tracks[1].notes] == [36, 36]


def test_abc_continued_chord_reports_physical_location():
    with pytest.warns(UserWarning, match=r"<string>:8:5: ABC \[stray-chord-close\]"):
        score = Score.from_abc(HEADER + "[CE\\\r\nG]  ]F")
    assert [note.pitch for note in score.tracks[0].notes] == [60, 64, 67, 65]


def test_abc_repeat_restores_written_key_and_length():
    score = Score.from_abc(HEADER + "|: F [K:G][L:1/8] F2 :| F2")
    assert [n[:3] for n in note_rows(score)] == [
        (1, 479, 65),
        (481, 479, 66),
        (961, 479, 65),
        (1441, 479, 66),
        (1921, 479, 66),
    ]


@pytest.mark.parametrize(
    ("body", "pitches"),
    [
        ("C4 || D4 :| E4 |", [60, 62, 60, 62, 64]),
        ("|: C4 || D4 :| E4 |", [60, 62, 60, 62, 64]),
        ("|: C [1 D :| [2 E :| [3 F |", [60, 62, 60, 64, 60, 65]),
        (
            '|: CD ["1"] EF :| ["2"] GA | Bc',
            [60, 62, 64, 65, 60, 62, 64, 65, 67, 69, 71, 72],
        ),
    ],
)
def test_abc_repeat_does_not_drop_sections(body: str, pitches: list[int]):
    assert [row[2] for row in note_rows(Score.from_abc(HEADER + body))] == pitches


def test_abc_broken_pair_places_intervening_field_at_correct_time():
    score = Score.from_abc(HEADER + "C>!tenuto![K:G]F|")
    assert [(e.time, e.key) for e in score.key_signatures] == [(0, 0), (720, 1)]
    assert [row[:3] for row in note_rows(score)] == [(1, 719, 60), (721, 239, 66)]


def test_abc_overlay_keeps_ties_and_restores_primary_key():
    score = Score.from_abc(HEADER + "C2 & [K:G]F2- | F2 & F2 |")
    main, overlay = score.tracks
    assert [(n.time, n.pitch) for n in main.notes] == [(1, 60), (961, 65)]
    assert [(n.time, n.duration, n.pitch) for n in overlay.notes] == [(1, 1919, 66)]


def test_abc_overlay_effects_persist_only_on_their_own_lane():
    score = Score.from_abc(HEADER + "C2 & !p!F2 & F2 | F2 & F2 & F2 |")
    assert [[n.velocity for n in lane.notes] for lane in score.tracks] == [
        [105, 105],
        [60, 60],
        [105, 105],
    ]


def test_abc_orphan_broken_mark_reports_its_location():
    with pytest.warns(
        UserWarning, match=r"<string>:7:5: ABC \[unfinished-broken-rhythm\]"
    ):
        Score.from_abc(HEADER + "C D >")


def test_abc_ignored_key_does_not_reset_accidentals():
    with pytest.warns(UserWarning, match=r"<string>:7:5: ABC \[invalid-inline-key\]"):
        score = Score.from_abc(HEADER + "^F [K:bad] FGA")
    assert [row[2] for row in note_rows(score)] == [66, 66, 67, 69]
    assert len(score.key_signatures) == 1


def test_abc_first_voice_keeps_preamble_and_music():
    with pytest.warns(UserWarning, match=r"<string>:9:1: ABC \[late-first-voice\]"):
        score = Score.from_abc(
            HEADER + "%%MIDI program 43\nC2\nV:melody name=Lead\nD2|"
        )
    (track,) = score.tracks
    assert (track.name, track.program) == ("Lead", 43)
    assert [note.time for note in track.notes] == [1, 961]


def test_abc_aligns_lyrics_to_attacks_on_their_music_line():
    score = Score.from_abc(
        HEADER + "[CEG] z D E |\nw: One * three\nF G |\nw: Four five"
    )
    assert [(e.time, e.text) for e in score.tracks[0].lyrics] == [
        (1, "One"),
        (1441, "three"),
        (1921, "Four"),
        (2401, "five"),
    ]


def test_abc_pedals_are_not_paired_across_overlay_lanes():
    score = Score.from_abc(
        HEADER + "(& C D [I:MIDI control 64 127] & [I:MIDI control 64 0] E F &) G A"
    )
    assert len(score.tracks) == 2
    assert not [pedal for track in score.tracks for pedal in track.pedals]


def test_abc_backing_cannot_overflow_note_end():
    text = HEADER.replace("L:1/4", "L:1/1920")
    text += '%%MIDI gchordoff\nz2147483547\n%%MIDI gchord f\n%%MIDI gchordon\n"C"C'
    with pytest.raises(ValueError, match=r"<string>:11:4: ABC \[time-range\]"):
        Score.from_abc(text)


def test_abc_playback_budget_counts_chord_members():
    text = HEADER.replace("K:C", "P:((A32)32)32\nK:C") + "P:A\n[" + "C" * 64 + "]"
    with pytest.raises(ValueError, match=r"<string>:9:1: ABC \[expansion-limit\]"):
        Score.from_abc(text)


def test_abc_playback_budget_counts_replayed_text():
    text = HEADER.replace("K:C", "P:A20000\nK:C")
    text += "P:A\n%%MIDI beatstring " + "f" * 6400 + "\nC |"
    with pytest.raises(ValueError, match=r"<string>:9:1: ABC \[expansion-limit\]"):
        Score.from_abc(text)


@pytest.mark.parametrize("ttype", ["tick", "quarter", "second"])
def test_abc_time_units(ttype: str):
    expected = Score.from_abc(HEADER + "CDEF|").to(ttype)
    assert Score.from_abc(HEADER + "CDEF|", ttype) == expected
    assert (
        Score.from_abc(expected.dumps_abc(), ttype).tracks[0].notes
        == expected.tracks[0].notes
    )


@pytest.mark.parametrize(
    "file_name", ["round trip.abc", "旋律 I O.AbC", "& ; melody.abc"]
)
def test_abc_file_roundtrip(tmp_path: Path, file_name: str):
    score = roundtrip_score()
    path = tmp_path / "多语言 路径" / file_name
    path.parent.mkdir()
    score.dump_abc(path)
    assert path.read_text(encoding="utf-8") == score.dumps_abc()
    assert note_rows(Score(path)) == note_rows(score)
    assert note_rows(Score.from_file(path, format="ABC")) == note_rows(score)


def test_abc_needs_no_converter_or_temp_file(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
):
    for variable in ("SYMUSIC_ABC2MIDI", "SYMUSIC_MIDI2ABC", "TMPDIR", "TEMP", "TMP"):
        monkeypatch.setenv(variable, str(tmp_path / "does-not-exist"))
    abc = roundtrip_score().dumps_abc()
    assert Score.from_abc(abc).note_num() == 3
    assert not list(tmp_path.iterdir())


def test_abc_warnings_can_be_disabled_only_for_export():
    score = roundtrip_score()
    score.tracks[0].notes.append(Note(0, 5, 61, 100))
    with pytest.warns(UserWarning, match=r"ABC export score \[note-start-offset\]"):
        text = score.dumps_abc(warn=True)
    with warnings.catch_warnings():
        warnings.simplefilter("error")
        assert score.dumps_abc(warn=False) == text
    assert note_rows(Score.from_abc(text)) == [
        (t + 1, d, p, v) for t, d, p, v in note_rows(score)
    ]


def test_abc_export_retains_short_audible_note_but_does_not_shift_for_silent_note():
    score = Score(1920)
    track = Track("Short notes")
    track.notes.extend([Note(0, 20, 60, 0), Note(480, 1, 61, 99)])
    score.tracks.append(track)
    assert note_rows(Score.from_abc(score.dumps_abc())) == [(120, 1, 61, 99)]


def test_abc_export_preserves_tempo_keys_meter_polyphony_and_controls():
    score = roundtrip_score()
    track = score.tracks[0]
    track.notes.extend([Note(241, 600, 67, 92), Note(481, 240, 69, 80)])
    track.controls.append(ControlChange(240, 7, 50))
    track.pedals.append(Pedal(240, 240))
    score.tempos.append(Tempo(960, 97))
    score.time_signatures.append(TimeSignature(960, 3, 4))
    score.key_signatures.extend([KeySignature(0, 0, 0), KeySignature(960, 1, 0)])
    with pytest.warns(UserWarning, match=r"\[split-voice\]"):
        text = score.dumps_abc(warn=True)
    loaded = Score.from_abc(text)
    assert note_rows(loaded) == note_rows(score)
    assert loaded.tempos == score.tempos
    assert loaded.time_signatures == score.time_signatures
    assert loaded.key_signatures == score.key_signatures
    assert len(loaded.tracks) == 2
    for channel in loaded.tracks:
        assert channel.program == 24
        assert track.controls[0] in channel.controls
        assert channel.pedals == track.pedals


def test_abc_export_merges_overlapping_pedals():
    score = roundtrip_score()
    score.tracks[0].pedals.extend([Pedal(120, 240), Pedal(240, 240)])
    with pytest.warns(UserWarning, match=r"\[overlapping-pedals\]"):
        text = score.dumps_abc(warn=True)
    assert list(Score.from_abc(text).tracks[0].pedals) == [Pedal(120, 360)]


def test_abc_export_warns_if_pedals_disagree_with_controls():
    score = roundtrip_score()
    track = score.tracks[0]
    track.pedals.append(Pedal(0, 480))
    track.controls.extend([ControlChange(120, 64, 80), ControlChange(360, 64, 0)])
    with pytest.warns(UserWarning, match=r"\[pedal-conflict\]"):
        text = score.dumps_abc(warn=True)
    loaded = Score.from_abc(text).tracks[0]
    assert loaded.controls == track.controls
    assert list(loaded.pedals) == [Pedal(120, 240)]


def test_abc_export_partitions_large_polyphony():
    score = Score(480)
    track = Track("Polyphony")
    track.notes.extend(
        Note(time, 30, pitch, 80) for time in (15, 45) for pitch in range(51)
    )
    score.tracks.append(track)
    text = score.dumps_abc()
    assert len(Score.from_abc(text).tracks) == 3
    assert note_rows(Score.from_abc(text)) == note_rows(score)


def test_abc_export_keeps_the_meter_after_a_backslash_in_the_title():
    score = roundtrip_score()
    score.tracks[0].name = "ending in a backslash\\"
    score.time_signatures[0] = TimeSignature(0, 3, 8)
    loaded = Score.from_abc(score.dumps_abc())
    assert loaded.tracks[0].name == score.tracks[0].name
    assert loaded.time_signatures == score.time_signatures


def test_abc_export_rejects_output_beyond_the_work_limit():
    score = Score(480)
    track = Track("Many onsets")
    track.notes.extend(Note(1 + i * 480, 480, 60, 80) for i in range(252_000))
    score.tracks.append(track)
    with pytest.raises(ValueError, match=r"ABC export track 1.*\[expansion-limit\]"):
        score.dumps_abc()


def test_abc_export_rejects_too_many_voices():
    score = Score(480)
    for program in range(16):
        track = Track(str(program), program)
        track.notes.append(Note(15, 30, 60, 80))
        score.tracks.append(track)
    with pytest.raises(ValueError, match="more than 15 ABC voices"):
        score.dumps_abc()


@pytest.mark.skipif(not Path("/dev/full").exists(), reason="requires /dev/full")
def test_abc_reports_buffered_write_failure():
    with pytest.raises(RuntimeError, match="Could not write output file: /dev/full"):
        roundtrip_score().dump_abc(Path("/dev/full"))


def test_abc_conversion_is_safe_under_parallel_calls(tmp_path: Path):
    score = roundtrip_score()
    abc_text = score.dumps_abc()

    def convert(index: int):
        path = tmp_path / f"parallel {index}.abc"
        Score.from_abc(abc_text).dump_abc(path)
        return note_rows(Score(path))

    with ThreadPoolExecutor(max_workers=4) as executor:
        assert list(executor.map(convert, range(8))) == [note_rows(score)] * 8


def test_dump_abc_issue78_regression(tmp_path: Path):
    score = Score(TESTCASES_PATH / "issue78_dump_abc.mid")
    path = tmp_path / "issue78.abc"
    score.dump_abc(path)
    assert path.read_text(encoding="utf-8") == score.dumps_abc()
    assert note_rows(Score(path)) == note_rows(score)


def test_abc_discards_overlong_chord_definition():
    source = HEADER + '%%MIDI chordname over 0 1 2 3 4 5 6 7 8 9 10\n"C"CDEF|'
    with pytest.warns(UserWarning, match=r":7:1: ABC \[invalid-chord-name\]"):
        result = Score.from_abc(source)
    assert len(result.tracks[2].notes) == 6
