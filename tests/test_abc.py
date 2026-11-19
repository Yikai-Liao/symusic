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
    PitchBend,
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
        ("[I:MIDI drummap J 36] C", "invalid-drummap", 7, 2),
        ("[I:MIDI drummap ^/F 36] C", "invalid-drummap", 7, 2),
        ("[I:MIDI drummap C 128] C", "invalid-midi-value", 7, 2),
        ("C,,,,,,,,", "pitch-range", 7, 1),
    ],
)
def test_abc_fatal_diagnostics(body: str, code: str, line: int, column: int):
    with pytest.raises(ValueError, match=rf"<string>:{line}:{column}: ABC \[{code}\]"):
        Score.from_abc(HEADER + body)


def test_midi_alias_stops_at_undefined_name():
    text = (
        HEADER
        + "%%MIDIdef lead program 20\n%%MIDIdef later program 53\n"
        + "[I:MIDIx lead missing later] CDEF|"
    )
    with pytest.warns(
        UserWarning, match=r"<string>:9:2: ABC \[undefined-midi-alias\].*'missing'"
    ):
        score = Score.from_abc(text)
    assert [track.program for track in score.tracks] == [20]


def test_midi_alias_command_error_locates_use():
    text = HEADER + "%%MIDIdef wrong program 128\n\n%%MIDIx wrong\nCDEF|"
    with pytest.raises(ValueError, match=r"<string>:9:1: ABC \[invalid-midi-value\]"):
        Score.from_abc(text)


@pytest.mark.parametrize(
    ("alias", "codes"),
    [
        ("MIDIdef p program 53", ["orphan-lyrics"]),
        ("MIDIx p", ["undefined-midi-alias", "orphan-lyrics"]),
    ],
)
def test_midi_alias_only_line_does_not_attach_lyrics(alias: str, codes: list[str]):
    with pytest.warns(
        UserWarning, match=r"\[(orphan-lyrics|undefined-midi-alias)\]"
    ) as notices:
        score = Score.from_abc(HEADER + f"CDEF|\n[I:{alias}]\nw: one\nGABc|")
    assert [
        str(notice.message).split("[")[1].split("]")[0] for notice in notices
    ] == codes
    assert not any(track.lyrics for track in score.tracks)


def test_midi_alias_file_preamble_requires_percent_directive():
    text = "I:MIDIdef lead program 53\n" + HEADER + "%%MIDIx lead\nCDEF|"
    with pytest.warns(
        UserWarning, match=r"\[(midi-alias-placement|undefined-midi-alias)\]"
    ) as notices:
        score = Score.from_abc(text)
    assert len(notices) == 2
    assert "<string>:1:1: ABC [midi-alias-placement]" in str(notices[0].message)
    assert "<string>:8:1: ABC [undefined-midi-alias]" in str(notices[1].message)
    assert [track.program for track in score.tracks] == [0]


def test_midi_program_base_in_file_header_is_ignored():
    text = "%%MIDI programbase 1\n" + HEADER + "%%MIDI program 48\nCDEF|"
    with pytest.warns(
        UserWarning, match=r"<string>:1:1: ABC \[program-base-placement\]"
    ):
        score = Score.from_abc(text)
    assert [track.program for track in score.tracks] == [48]


def test_missing_key_not_recovered():
    with pytest.raises(ValueError, match=r"<string>:3:1: ABC \[missing-key\]"):
        Score.from_abc("X:1\nM:4/4\nCDEF\n")


def test_explicit_key_keeps_midi_signature():
    score = Score.from_abc(HEADER.replace("K:C", "K:A _F exp ^G") + "CDEFGABc|")
    assert [n.pitch for n in score.tracks[0].notes] == [60, 62, 64, 64, 68, 69, 71, 72]
    assert [(key.time, key.key, key.tonality) for key in score.key_signatures] == [
        (0, 3, 0)
    ]


def test_unknown_attached_key_mode_locates_the_recovery():
    with pytest.warns(
        UserWarning, match=r"<string>:6:1: ABC \[unknown-key-mode\].*'exp'"
    ):
        score = Score.from_abc(HEADER.replace("K:C", "K:Gexp") + "CDEF|")
    assert [n.pitch for n in score.tracks[0].notes] == [60, 62, 64, 66]
    assert score.key_signatures[0].key == 1


@pytest.mark.parametrize("bad_byte", [b"\xff", b"\x00"])
def test_abc_rejects_invalid_text_at_its_byte_column(tmp_path: Path, bad_byte: bytes):
    path = tmp_path / "bad text.abc"
    path.write_bytes(HEADER.encode() + '"雪"!'.encode() + bad_byte + b"! CDEF")
    with pytest.raises(ValueError, match=r"bad text\.abc:7:7: ABC \[invalid-text\]"):
        Score(path)


@pytest.mark.parametrize("reference", ["X:2", "X :2"])
def test_abc_warns_on_second_tune_and_recovers_nonbinary_duration(reference: str):
    text = HEADER + "C/3 D/3\n\n" + HEADER.replace("X:1", reference) + "GAB"
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


@pytest.mark.parametrize("ttype", ["tick", "quarter", "second"])
@pytest.mark.parametrize("event", ["note", "pedal"])
def test_abc_rejects_negative_duration(ttype: str, event: str):
    score = roundtrip_score().to(ttype)
    track = score.tracks[0]
    if event == "note":
        track.notes[0].duration = -1
    else:
        track.pedals.append(Pedal(10, -1, ttype=ttype))
        track.controls.append(ControlChange(10, 64, 127, ttype=ttype))
    with pytest.raises(
        ValueError, match=rf"track 1 .*: {event} duration cannot be negative"
    ):
        score.dumps_abc()


@pytest.mark.parametrize("ttype", ["quarter", "second"])
@pytest.mark.parametrize("duration", [float("nan"), float("inf")])
def test_abc_rejects_nonfinite_duration(ttype: str, duration: float):
    score = roundtrip_score().to(ttype)
    score.tracks[0].notes[0].duration = duration
    with pytest.raises(ValueError, match="note duration must be finite"):
        score.dumps_abc()


@pytest.mark.parametrize("ttype", ["quarter", "second"])
def test_abc_retains_short_floating_note(ttype: str):
    score = Score(1, ttype=ttype)
    track = Track(ttype=ttype)
    track.notes.append(Note(1, 0.0001, 60, 90, ttype=ttype))
    track.pedals.append(Pedal(1, 0.0001, ttype=ttype))
    score.tracks.append(track)
    with pytest.warns(
        UserWarning, match=r"\[(resampled-time|short-note|short-pedal)\]"
    ) as caught:
        text = score.dumps_abc(warn=True)
    assert {str(w.message).split("[")[1].split("]")[0] for w in caught} == {
        "resampled-time",
        "short-note",
        "short-pedal",
    }
    assert note_rows(Score.from_abc(text)) == [
        (480 if ttype == "quarter" else 960, 1, 60, 90)
    ]


@pytest.mark.parametrize("first", [0, 15])
def test_abc_export_locates_invalid_onsets(first: int):
    score = Score(480)
    track = Track("Lead")
    track.notes.extend([Note(first, 30, 60, 90), Note(-1, 30, 62, 90)])
    score.tracks.append(track)
    with pytest.raises(ValueError, match=r"track 1 \('Lead'\): event time must be"):
        score.dumps_abc()


def test_abc_export_reports_one_rounding_example_per_track():
    score = Score(960)
    track = Track("Lead")
    track.notes.extend([Note(3, 30, 60, 90), Note(9, 30, 64, 90)])
    score.tracks.append(track)
    with pytest.warns(UserWarning, match=r"\[resampled-time\]") as caught:
        score.dumps_abc(warn=True)
    assert [str(w.message) for w in caught] == [
        (
            "ABC export track 1 ('Lead') [resampled-time]: "
            "rounded 1.5 ABC ticks to 2 on the 480-TPQ grid"
        )
    ]


def test_abc_second_export_uses_tempo_map_for_endpoints():
    score = Score(1, ttype="second")
    score.tempos.extend([Tempo(0, 120, ttype="second"), Tempo(0.5, 60, ttype="second")])
    track = Track(ttype="second")
    track.notes.append(Note(0.25, 0.75, 60, 90, ttype="second"))
    score.tracks.append(track)
    actual = Score.from_abc(score.dumps_abc(warn=True))
    assert note_rows(actual) == [(240, 480, 60, 90)]
    assert list(actual.tempos) == [Tempo(0, 120), Tempo(480, 60)]


def test_abc_second_export_rejects_invalid_tempo_before_conversion():
    score = Score(480, ttype="second")
    tempo = Tempo(0, 120, ttype="second")
    tempo.time = float("nan")
    score.tempos.append(tempo)
    with pytest.raises(ValueError, match="tempo: time must be finite and nonnegative"):
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


def test_abc_part_order_rejects_overflowing_nested_counts():
    source = HEADER.replace("K:C", "P:(A65536)65536\nK:C") + "P:A\nC4 |"
    with pytest.raises(ValueError, match=r"<string>:6:1: ABC \[expansion-limit\]"):
        Score.from_abc(source)


def test_abc_part_itinerary_with_sparse_voices():
    source = HEADER.replace("K:C", "P:ABA\nK:C") + (
        "V:one\nV:two\nP:A\n[V:one] C | [V:two] E |\nP:B\n[V:one] D |"
    )
    with pytest.warns(UserWarning, match=r"\[missing-part\].*part B") as warnings:
        score = Score.from_abc(source)
    assert len(warnings) == 1
    assert [[(n.time, n.pitch) for n in t.notes] for t in score.tracks] == [
        [(1, 60), (481, 62), (961, 60)],
        [(1, 64), (481, 64)],
    ]


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


def test_abc_overlay_reentry_keeps_route_clock_and_backing():
    score = Score.from_abc(
        HEADER
        + "%%MIDI drum dd 36 42\nC2 & [I:MIDI drumon] !p!E- & G2 | F2 & E2 & B | C2 |"
    )
    assert [track.is_drum for track in score.tracks] == [False, False, False, True]
    assert [
        [(n.time, n.duration, n.pitch, n.velocity) for n in track.notes]
        for track in score.tracks
    ] == [
        [(1, 959, 60, 105), (961, 959, 65, 105), (1921, 959, 60, 105)],
        [(1, 1439, 64, 60)],
        [(1, 959, 67, 105), (961, 479, 71, 105)],
        [(961, 959, 36, 80), (1921, 959, 36, 80)],
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


@pytest.mark.parametrize(("field", "column"), [("Q:0", 3), ("[Q:0]", 4)])
def test_abc_continued_field_location(field: str, column: int):
    with pytest.raises(
        ValueError, match=rf"<string>:8:{column}: ABC \[invalid-tempo\]"
    ):
        Score.from_abc(HEADER + "\\\r\n  " + field)


def test_abc_ambiguous_drummap_leaves_map_unchanged():
    abc = (
        HEADER + "%%MIDI channel 10\n%%MIDI drummap C 38\n%%MIDI drummap ^^2C 36\nCCCC|"
    )
    with pytest.warns(UserWarning, match=r"\[invalid-accidental\]"):
        score = Score.from_abc(abc)
    assert [n.pitch for n in score.tracks[0].notes] == [38] * 4


def test_abc_lyrics_follow_logical_music_line():
    score = Score.from_abc(
        HEADER + "C D " + chr(92) + "\n E F |\nw: one two three four"
    )
    assert [(e.time, e.text) for e in score.tracks[0].lyrics] == [
        (1, "one"),
        (481, "two"),
        (961, "three"),
        (1441, "four"),
    ]


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


def test_abc_lyrics_follow_repeat_jump_without_restarting_verse():
    first = "First line goes here"
    repeated = "The next four words cross the repeat bar"
    score = Score.from_abc(
        HEADER + f"|: CDEF|\nw: {first}\nGABc :| cBAG|\nw: {repeated}"
    )
    assert [(e.time, e.text) for e in score.tracks[0].lyrics] == list(
        zip(range(1, 9601, 480), first.split() + repeated.split() * 2, strict=True)
    )


@pytest.mark.parametrize(
    ("lyrics", "times"),
    [
        ("one | two\\-three", [1, 1921]),
        ("| one two\\-three", [1921, 2401]),
        ("|| one two\\-three", [3841, 4321]),
        ("* * * one | two\\-three", [1441, 1921]),
    ],
)
def test_abc_lyrics_skip_to_bar(lyrics: str, times: list[int]):
    score = Score.from_abc(HEADER + "| CDEF \\\n| GABc | cBAG |\nw: " + lyrics)
    assert [(e.time, e.text) for e in score.tracks[0].lyrics] == list(
        zip(times, ["one", "two-three"], strict=True)
    )


@pytest.mark.parametrize("middle", ["|\n", "z4|\n", "V:solo\n", "!f!\n"])
def test_abc_lyrics_without_anchors(middle: str):
    with pytest.warns(
        UserWarning, match=r"<string>:10:1: ABC \[(orphan|extra)-lyrics\]"
    ):
        score = Score.from_abc(
            HEADER.replace("K:C", "V:solo\nK:C") + "CDEF|\n" + middle + "w: one two"
        )
    assert not score.tracks[0].lyrics


@pytest.mark.parametrize(
    "words",
    [
        "w: one two\n+: three four",
        "w: one two\nw: +three four",
        "w: one two \\\nw: three four",
    ],
)
def test_abc_lyrics_extension(words: str):
    score = Score.from_abc(HEADER + "CDEF|\n" + words)
    assert [e.text for e in score.tracks[0].lyrics] == ["one", "two", "three", "four"]


def test_abc_pedals_are_not_paired_across_overlay_lanes():
    score = Score.from_abc(
        HEADER + "(& C D [I:MIDI control 64 127] & [I:MIDI control 64 0] E F &) G A"
    )
    assert len(score.tracks) == 2
    assert not [pedal for track in score.tracks for pedal in track.pedals]


@pytest.mark.parametrize("pattern", ["gchord f雪", "drum d雪"])
def test_abc_locates_invalid_backing_bytes(pattern: str):
    with pytest.raises(
        ValueError,
        match=r"<string>:7:1: ABC \[invalid-backing-pattern\]: unexpected byte 0xe9",
    ):
        Score.from_abc(HEADER + f"%%MIDI {pattern}\nCDEF|")


def test_abc_unsupported_default_backing_keeps_melody():
    with pytest.warns(UserWarning, match=r"<string>:7:1: ABC \[no-default-backing\]"):
        score = Score.from_abc(HEADER.replace("M:4/4", "M:7/16") + '"C"CDEF|')
    assert len(score.tracks) == 1
    assert score.note_num() == 4


def test_abc_backing_cannot_overflow_note_end():
    text = HEADER.replace("L:1/4", "L:1/1920")
    text += '%%MIDI gchordoff\nz2147483547|\n%%MIDI gchord f\n%%MIDI gchordon\n"C"C'
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


def test_abc_export_skipped_track_does_not_supply_title():
    score = Score(480)
    ghost = Track("Ghost", 24)
    sounding = Track("", 40)
    sounding.notes.append(Note(15, 60, 60, 80))
    score.tracks.extend([ghost, sounding])
    assert [track.name for track in Score.from_abc(score.dumps_abc()).tracks] == [""]


def test_abc_export_preserves_tempo_keys_meter_polyphony_and_controls():
    score = roundtrip_score()
    track = score.tracks[0]
    track.notes.extend([Note(241, 600, 67, 92), Note(481, 240, 69, 80)])
    track.controls.append(ControlChange(240, 7, 50))
    track.pedals.append(Pedal(240, 240))
    score.tempos.append(Tempo(960, 97))
    score.time_signatures.append(TimeSignature(960, 3, 4))
    score.key_signatures.extend([KeySignature(0, 0, 0), KeySignature(960, 1, 0)])
    with pytest.warns(
        UserWarning, match=r"\[(split-voice|midi-field-timing|midi-control-timing)\]"
    ) as notices:
        text = score.dumps_abc(warn=True)
    assert sum("[split-voice]" in str(notice.message) for notice in notices) == 1
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


def test_abc_warns_on_incompatible_chord_bends():
    with pytest.warns(UserWarning, match=r"<string>:7:1: ABC \[shared-pitch-wheel\]"):
        score = Score.from_abc(HEADER + "[^/CE] F G A|")
    assert len(score.tracks[0].notes) == 5
    assert list(score.tracks[0].pitch_bends) == [PitchBend(1, 2048), PitchBend(11, 0)]


@pytest.mark.parametrize("chord", ["[DC]", "[CD]"])
def test_abc_warns_on_bending_a_continuation(chord: str):
    with pytest.warns(UserWarning, match=r"<string>:7:6: ABC \[shared-pitch-wheel\]"):
        score = Score.from_abc(HEADER + f"^/C- {chord} F G|")
    track = score.tracks[0]
    assert [(note.time, note.duration, note.pitch) for note in track.notes[:2]] == [
        (1, 959, 60),
        (481, 479, 62),
    ]
    assert list(track.pitch_bends) == [PitchBend(1, 2048), PitchBend(481, 0)]


def test_abc_trill_expansion_limit():
    with pytest.raises(
        ValueError, match=r"<string>:7:3: ABC \[expansion-limit\].*trill"
    ):
        Score.from_abc(HEADER + "T C1000001|")


def test_abc_unclosed_layout():
    abc = HEADER + "CDEF|\n  %%begintext\nX:99\nK:G\nGABc|"
    with pytest.warns(
        UserWarning, match=r"<string>:8:3: ABC \[unclosed-layout\].*%%endtext"
    ):
        score = Score.from_abc(abc)
    assert [note.pitch for note in score.tracks[0].notes] == [60, 62, 64, 65]


@pytest.mark.parametrize("invalid", ["0 0 0", "0 0 nan", "201 0 0 0 0 0 0 0 0 0 0 0"])
def test_abc_malformed_detune(invalid: str):
    tuning = "%%temperament 0 0 10 0 0 0 0 0 0 0 0 0\n"
    abc = HEADER + tuning + f"%%temperament {invalid}\nD E F G|"
    with pytest.warns(
        UserWarning, match=r"<string>:8:1: ABC \[invalid-temperament\].*12 detunings"
    ):
        score = Score.from_abc(abc)
    assert list(score.tracks[0].pitch_bends) == [PitchBend(1, 410), PitchBend(481, 0)]


@pytest.mark.parametrize(
    "command",
    ["temperamentequal 0", "temperamentequal 31 0", "temperamentlinear inf 702"],
)
def test_abc_malformed_scale(command: str):
    abc = HEADER + "%%MIDI C 61\n" + f"%%MIDI {command}\nCDEF|"
    with pytest.warns(UserWarning, match=r"<string>:8:1: ABC \[invalid-temperament\]"):
        score = Score.from_abc(abc)
    assert [note.pitch for note in score.tracks[0].notes] == [61, 63, 65, 66]


def test_abc_rejects_zero_microtone_divisor():
    with pytest.raises(ValueError, match=r"<string>:7:3: ABC \[invalid-microtone\]"):
        Score.from_abc(HEADER + "C ^1/0D EF |")


def test_abc_export_preserves_pitch_bends():
    score = roundtrip_score()
    track = score.tracks[0]
    track.pitch_bends.extend(
        [PitchBend(0, -8192), PitchBend(143, 8191), PitchBend(480, 0)]
    )
    with pytest.warns(UserWarning, match=r"\[midi-bend-timing\].*shift later notes"):
        abc = score.dumps_abc(warn=True)
    loaded = Score.from_abc(abc)
    assert loaded.tracks[0].pitch_bends == track.pitch_bends
    assert note_rows(loaded) == note_rows(score)


@pytest.mark.parametrize("value", [-8193, 8192, 2**31 - 1])
def test_abc_export_rejects_out_of_range_pitch_bend(value: int):
    score = roundtrip_score()
    score.tracks[0].pitch_bends.append(PitchBend(0, value))
    with pytest.raises(ValueError, match="pitch bend must be in MIDI range"):
        score.dumps_abc()


def test_abc_export_merges_overlapping_pedals():
    score = roundtrip_score()
    score.tracks[0].pedals.extend([Pedal(120, 240), Pedal(240, 240)])
    with pytest.warns(
        UserWarning, match=r"\[(overlapping-pedals|midi-control-timing)\]"
    ) as notices:
        text = score.dumps_abc(warn=True)
    assert len(notices) == 2
    assert any("[overlapping-pedals]" in str(notice.message) for notice in notices)
    assert list(Score.from_abc(text).tracks[0].pedals) == [Pedal(120, 360)]


def test_abc_export_warns_if_pedals_disagree_with_controls():
    score = roundtrip_score()
    track = score.tracks[0]
    track.pedals.append(Pedal(0, 480))
    track.controls.extend([ControlChange(120, 64, 80), ControlChange(360, 64, 0)])
    with pytest.warns(
        UserWarning, match=r"\[(pedal-conflict|midi-control-timing)\]"
    ) as notices:
        text = score.dumps_abc(warn=True)
    assert len(notices) == 2
    assert any("[pedal-conflict]" in str(notice.message) for notice in notices)
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


def test_abc_export_budgets_all_channel_events_at_one_tick():
    score = Score(480)
    track = Track("Many same-tick controls")
    track.notes.extend(Note(15, 480, 60, velocity) for velocity in range(1, 16))
    track.controls.extend(ControlChange(15, 7, i % 128) for i in range(70_000))
    score.tracks.append(track)
    with pytest.raises(ValueError, match=r"ABC export track 1.*\[expansion-limit\]"):
        score.dumps_abc()


def test_abc_export_reuses_channel_events_across_split_voices():
    score = Score(480)
    track = Track("Split")
    track.notes.extend([Note(15, 30, 60, 90), Note(15, 45, 60, 80)])
    track.controls.extend([ControlChange(14, 7, 40), ControlChange(14, 7, 60)])
    score.tempos.append(Tempo(14, mspq=625_000))
    score.tracks.append(track)
    abc = score.dumps_abc()
    native = Score.from_abc(abc)
    assert note_rows(native) == note_rows(score)
    assert len(native.tracks) == 2
    assert all(list(t.controls) == list(track.controls) for t in native.tracks)
    assert abc.count("[Q:") == 1
    assert native.tempos[-1] == score.tempos[0]


@pytest.mark.parametrize("tpq", [480, 960])
def test_abc_export_rejects_too_many_voices(tpq: int):
    score = Score(tpq)
    for program in range(16):
        track = Track(str(program), program)
        track.notes.append(Note(tpq // 32, tpq // 16, 60, 80))
        score.tracks.append(track)
    with pytest.raises(
        ValueError,
        match=r"\[voice-placement\].*at playback tick 15 \(480 TPQ\).*15 melodic ABC voices",
    ):
        score.dumps_abc()


@pytest.mark.parametrize("reverse", [False, True])
def test_abc_export_prioritizes_constrained_attacks(reverse: bool):
    score = Score(480)
    for index in range(13):
        other = Track(str(index), index)
        other.notes.append(Note(15, 240, 61, 83))
        score.tracks.append(other)
    track = Track("Overlaps", 30)
    short = [Note(45, 30, 62, 80), Note(45, 30, 61, 81)]
    track.notes.extend([Note(15, 240, 60, 80), Note(15, 240, 61, 81)])
    track.notes.extend(reversed(short) if reverse else short)
    score.tracks.append(track)
    exported = Score.from_abc(score.dumps_abc())
    assert len(exported.tracks) == 15
    assert note_rows(exported) == note_rows(score)


@pytest.mark.parametrize("reverse", [False, True])
def test_abc_export_reuses_voices_at_the_same_velocity(reverse: bool):
    score = Score(480)
    for index in range(13):
        other = Track(str(index), index)
        other.notes.append(Note(15, 240, 61, 83))
        score.tracks.append(other)
    track = Track("Overlaps", 30)
    track.notes.append(Note(15, 60, 63, 81))
    chord = [Note(45, 30, 63, 81), Note(45, 60, 62, 80), Note(45, 30, 60, 81)]
    track.notes.extend(reversed(chord) if reverse else chord)
    score.tracks.append(track)
    exported = Score.from_abc(score.dumps_abc())
    assert len(exported.tracks) == 15
    assert note_rows(exported) == note_rows(score)


def score_with_constrained_onset(reverse: bool = False) -> Score:
    score = Score(480)
    for program in range(12):
        track = Track(str(program), program)
        track.notes.append(Note(15, 240, 61, 83))
        score.tracks.append(track)
    lead = Track("Lead", 30)
    lead.notes.extend(
        Note(*row)
        for row in [
            (15, 240, 50, 70),
            (15, 240, 51, 71),
            (15, 240, 60, 72),
            (15, 240, 61, 71),
            (15, 240, 62, 71),
        ]
    )
    attacks = [Note(45, 30, 60, 80), Note(45, 30, 61, 81), Note(45, 30, 62, 82)]
    lead.notes.extend(reversed(attacks) if reverse else attacks)
    score.tracks.append(lead)
    return score


@pytest.mark.parametrize("reverse", [False, True])
def test_abc_export_reassigns_a_constrained_onset(reverse: bool):
    score = score_with_constrained_onset(reverse)
    exported = Score.from_abc(score.dumps_abc())
    assert len(exported.tracks) == 15
    assert note_rows(exported) == note_rows(score)


def test_abc_export_voice_search_limit():
    # Each attack at tick 45 has a distinct velocity. Independent groups
    # compete for disjoint lane sets; taking lane 0 for the first attack
    # leaves the final group short even though lane 1 would also work.
    allowed_lanes = (
        [{0, 1}]
        + [{6, 7, 8}] * 3
        + [{9, 10, 11}] * 3
        + [{12, 13, 14}] * 3
        + [{0, 2, 3, 4, 5}] * 5
    )
    score = Score(480)
    track = Track("Search limit")
    # One common held pitch establishes the lane numbering.
    track.notes.extend(Note(15, 240, 20, 60 + lane) for lane in range(15))
    for pitch, allowed in enumerate(allowed_lanes, start=40):
        track.notes.extend(
            Note(15, 240, pitch, 60 + lane) for lane in range(15) if lane not in allowed
        )
        track.notes.append(Note(45, 30, pitch, 90 + pitch - 40))
    score.tracks.append(track)
    with pytest.raises(
        ValueError,
        match=r"track 1.*\[voice-search-limit\].*tick 45 \(480 TPQ\).*15 available voices.*100000 retry candidates",
    ):
        score.dumps_abc()


def test_abc_export_rejects_more_than_30_total_voices():
    score = Score(480)
    for index in range(31):
        track = Track(str(index), is_drum=index >= 15)
        track.notes.append(Note(15, 30, 30 + index, 80))
        score.tracks.append(track)
    with pytest.raises(
        ValueError,
        match=r"\[voice-placement\].*at playback tick 15.*0 available voices.*30 ABC voices",
    ):
        score.dumps_abc()


def test_abc_export_warns_on_shared_drum_channel():
    score = Score(480)
    for pitch in [35, 42]:
        track = Track(is_drum=True)
        track.notes.append(Note(15, 30, pitch, 80))
        score.tracks.append(track)
    with pytest.warns(UserWarning, match=r"\[shared-drum-channel\].*channel 10"):
        abc = score.dumps_abc(warn=True)
    assert note_rows(Score.from_abc(abc)) == note_rows(score)


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


def test_one_slot_drum_pattern_does_not_duplicate_strikes():
    score = Score.from_abc(HEADER + "%%MIDI drum d 35 80\n%%MIDI drumon\nCDEF|GAB|")
    drums = next(track for track in score.tracks if track.is_drum)
    assert [(note.time, note.duration, note.pitch) for note in drums.notes] == [
        (1, 1919, 35),
        (1921, 1440, 35),
    ]
