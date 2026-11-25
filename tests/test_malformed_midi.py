from __future__ import annotations

import subprocess
import sys
from io import BytesIO

import pytest
from symusic import Score, Tempo, TimeSignature, _score_io

from tests.utils import MIDI_PATHS_ALL, TESTCASES_PATH

ISSUE_104_PATH = (
    TESTCASES_PATH / "MIDIs_corrupted" / "Issue104_Bizarre_Love_Triangle2.mid"
)
ABC_FIXTURE_PATH = TESTCASES_PATH / "abc_files" / "a_morning_in_summer.abc"


def build_single_track_midi(track_bytes: bytes, ticks_per_quarter: int = 480) -> bytes:
    header = b"MThd" + (6).to_bytes(4, "big")
    header += (0).to_bytes(2, "big")
    header += (1).to_bytes(2, "big")
    header += ticks_per_quarter.to_bytes(2, "big")
    track = b"MTrk" + len(track_bytes).to_bytes(4, "big") + track_bytes
    return header + track


def test_malformed_midi_default_load_is_strict():
    with pytest.raises(RuntimeError):
        Score(ISSUE_104_PATH)


@pytest.mark.parametrize("strict", [True, False])
def test_malformed_midi_raises_runtime_error(strict):
    with pytest.raises(RuntimeError):
        Score.from_midi(ISSUE_104_PATH.read_bytes(), strict=strict)


def test_valid_midi_baseline_matches_clamped():
    midi_path = MIDI_PATHS_ALL[0]
    assert Score.from_midi(midi_path.read_bytes(), strict=False) == Score(midi_path)


def test_abc_strict_on_valid_input():
    text = ABC_FIXTURE_PATH.read_text(encoding="utf-8")
    assert Score(ABC_FIXTURE_PATH, strict=False) == Score(ABC_FIXTURE_PATH)
    assert Score.from_abc(text, strict=False) == Score.from_abc(text)


@pytest.mark.parametrize("entry_point", ["path", "stream", "text"])
def test_abc_strict_controls_converted_midi(monkeypatch, entry_point):
    data = build_single_track_midi(
        bytes.fromhex("00 90 3c 40 00 e0 ff 01 01 80 3c 40 00 ff 2f 00")
    )
    monkeypatch.setattr(
        _score_io, "read_abc", lambda input_data, tune, parse_midi: parse_midi(data)
    )

    def load(strict):
        if entry_point == "path":
            return Score(ABC_FIXTURE_PATH, strict=strict)
        if entry_point == "stream":
            return Score(BytesIO(b""), format="abc", strict=strict)
        return Score.from_abc("", strict=strict)

    with pytest.raises(RuntimeError, match="pitch_bend"):
        load(True)
    assert load(False).tracks[0].pitch_bends[0].value == 255 - 8192


@pytest.mark.parametrize(
    ("case_name", "midi_bytes"),
    [
        pytest.param(
            "zero-length-sysex",
            build_single_track_midi(
                bytes(
                    [
                        0x00,
                        0xF0,
                        0x00,
                        0x00,
                        0x90,
                        0x3C,
                        0x40,
                        0x60,
                        0x80,
                        0x3C,
                        0x40,
                        0x00,
                        0xFF,
                        0x2F,
                        0x00,
                    ],
                ),
            ),
            id="zero-length-sysex",
        ),
        pytest.param(
            "zero-length-meta",
            build_single_track_midi(
                bytes(
                    [
                        0x00,
                        0xFF,
                        0x01,
                        0x00,
                        0x00,
                        0x90,
                        0x3C,
                        0x40,
                        0x60,
                        0x80,
                        0x3C,
                        0x40,
                        0x00,
                        0xFF,
                        0x2F,
                        0x00,
                    ],
                ),
            ),
            id="zero-length-meta",
        ),
    ],
)
def test_minimidi_vlq_cursor_regressions_parse_minimal_sequences(
    case_name: str,
    midi_bytes: bytes,
):
    # These byte sequences exercise the sequencing-sensitive VLQ parsing paths in minimidi.
    score = Score.from_midi(midi_bytes)

    assert case_name
    assert score.ticks_per_quarter == 480
    assert len(score.tracks) == 1
    assert score.note_count() == 1


def test_minimidi_unknown_status_reports_status_byte():
    midi_bytes = build_single_track_midi(bytes([0x00, 0xF4, 0x00, 0xFF, 0x2F, 0x00]))

    with pytest.raises(RuntimeError, match=r"Unknown or unsupported status byte: 0xf4"):
        Score.from_midi(midi_bytes)


def test_malformed_midi_raises_in_subprocess():
    script = f"""
from pathlib import Path
from symusic import Score

path = Path({str(ISSUE_104_PATH)!r})
try:
    Score(path)
except RuntimeError:
    raise SystemExit(0)
raise SystemExit(1)
"""
    result = subprocess.run(
        [sys.executable, "-c", script],
        check=False,
        capture_output=True,
        text=True,
    )

    assert result.returncode == 0, result.stderr or result.stdout


def test_midi_lyrics_use_first_created_note_track():
    body = bytes.fromhex(
        "00 ff 05 01 61 "  # lyric before notes
        "00 97 3c 40 01 87 3c 00 "  # channel 7 creates the first track
        "00 90 3d 40 01 80 3d 00 "  # channel 0 sorts before it
        "00 c7 01 00 97 3e 40 01 87 3e 00 "  # a new program on channel 7
        "00 ff 2f 00"
    )
    score = Score.from_midi(build_single_track_midi(body))
    assert len(score.tracks) == 3
    assert [
        (track.notes[0].pitch, track.lyrics[0].text)
        for track in score.tracks
        if track.lyrics
    ] == [
        (60, "a"),
    ]


def test_midi_without_notes_keeps_lyric_programs():
    body = bytes.fromhex("00 ff 05 01 61 00 cf 01 00 ff 05 01 62 00 ff 2f 00")
    score = Score.from_midi(build_single_track_midi(body))
    assert [(track.program, track.lyrics[0].text) for track in score.tracks] == [
        (0, "a"),
        (1, "b"),
    ]


def test_midi_moving_lyrics_preserves_channel_events():
    body = bytes.fromhex(
        "00 ff 05 01 61 "  # lyric
        "00 bf 40 7f "  # sustain on, channel 16
        "00 90 3c 40 01 80 3c 00 "  # note, channel 1
        "01 ef 01 40 "  # pitch bend, channel 16
        "01 bf 40 00 "  # sustain off, channel 16
        "00 ff 2f 00"
    )
    score = Score.from_midi(build_single_track_midi(body))
    note_track, channel_track = score.tracks
    assert [lyric.text for lyric in note_track.lyrics] == ["a"]
    assert not channel_track.lyrics
    assert [(cc.time, cc.number, cc.value) for cc in channel_track.controls] == [
        (0, 64, 127),
        (3, 64, 0),
    ]
    assert [(pedal.time, pedal.duration) for pedal in channel_track.pedals()] == [
        (0, 3)
    ]
    assert [(bend.time, bend.value) for bend in channel_track.pitch_bends] == [(2, 1)]
    assert Score.from_midi(score.to_midi()) == score


def test_midi_round_trips_global_events_without_tracks():
    score = Score(ticks_per_quarter=480)
    score.time_signatures.append(TimeSignature(480, 3, 4))
    score.tempos.append(Tempo(0, 120))

    assert Score.from_midi(score.to_midi()) == score


def test_midi_keeps_noteless_channel_tracks():
    body = bytes.fromhex(
        "00 b0 40 7f "  # sustain on, channel 1
        "01 c1 01 "  # program 1, channel 2
        "01 e1 01 40 "  # pitch bend, channel 2
        "01 b0 40 00 "  # sustain off, channel 1
        "00 ff 2f 00"
    )
    score = Score.from_midi(build_single_track_midi(body))
    first, second = score.tracks
    assert (first.program, second.program) == (0, 1)
    assert [(cc.time, cc.value) for cc in first.controls] == [(0, 127), (3, 0)]
    assert not first.pitch_bends
    assert [(bend.time, bend.value) for bend in second.pitch_bends] == [(2, 1)]
    assert not second.controls
    assert Score.from_midi(score.to_midi()) == score


def test_midi_rejects_unrepresentable_denominator():
    body = bytes.fromhex("00 ff 58 04 04 08 18 08 00 ff 2f 00")
    with pytest.raises(RuntimeError, match="time signature denominator"):
        Score.from_midi(build_single_track_midi(body))


@pytest.mark.skipif(sys.platform != "linux", reason="requires /dev/stdin")
def test_midi_rejects_non_seekable_file():
    script = """
from symusic import Score

try:
    Score("/dev/stdin", format="midi")
except OSError as error:
    if error.errno == 29:
        raise SystemExit(0)
    raise
raise SystemExit(1)
"""
    result = subprocess.run(
        [sys.executable, "-c", script],
        input=b"MThd",
        capture_output=True,
        check=False,
    )
    assert result.returncode == 0, result.stderr


@pytest.mark.parametrize("with_note", [False, True])
def test_midi_pending_programs(with_note):
    body = bytes.fromhex(
        "00 b2 07 64 00 e2 01 40 00 c2 01 00 b2 0a 20 00 e2 02 40 00 c2 00 00 b2 0b 30"
    )
    if with_note:
        body += bytes.fromhex("00 c2 01 00 92 3c 40 01 82 3c 00")
    score = Score.from_midi(
        build_single_track_midi(body + bytes.fromhex("00 ff 2f 00"))
    )
    expected = (
        [(1, [7, 10, 11], [1, 2])]
        if with_note
        else [
            (0, [7, 11], [1]),
            (1, [10], [2]),
        ]
    )
    for parsed in (score, Score.from_midi(score.to_midi())):
        assert [
            (
                track.program,
                [cc.number for cc in track.controls],
                [bend.value for bend in track.pitch_bends],
            )
            for track in parsed.tracks
        ] == expected


def test_midi_lyric_does_not_merge_pending_programs_on_channel_15():
    body = bytes.fromhex(
        "00 bf 07 64 "  # CC7 on channel 16, program 0
        "00 cf 01 "  # program 1 on channel 16
        "00 ff 05 01 61 "  # lyric while channel 16 is program 1
        "00 90 3c 40 01 80 3c 00 "  # note on channel 1
        "00 ff 2f 00"
    )
    score = Score.from_midi(build_single_track_midi(body))
    assert [
        (
            track.program,
            len(track.notes),
            [cc.number for cc in track.controls],
            [lyric.text for lyric in track.lyrics],
        )
        for track in score.tracks
    ] == [
        (0, 1, [], ["a"]),
        (0, 0, [7], []),
    ]


def test_midi_clamps_pitch_bend_bytes_before_combining(tmp_path):
    body = bytes.fromhex("00 90 3c 40 00 e0 ff 01 01 80 3c 40 00 ff 2f 00")
    data = build_single_track_midi(body)
    with pytest.raises(RuntimeError, match="pitch_bend"):
        Score.from_midi(data)
    score = Score.from_midi(data, strict=False)
    assert score.tracks[0].pitch_bends[0].value == 255 - 8192
    path = tmp_path / "invalid-pitch-bend.mid"
    path.write_bytes(data)
    with pytest.raises(RuntimeError, match="pitch_bend"):
        Score(path)
    assert Score(path, strict=False) == score
    assert Score(BytesIO(data), format="midi", strict=False) == score


def test_channel_sustain_release_stays_with_the_press_after_program_change():
    body = bytes.fromhex(
        "00 90 3c 40 00 b0 40 40 01 80 3c 40 "
        "00 c0 01 00 90 3e 40 01 b0 40 3f 01 80 3e 40 00 ff 2f 00"
    )
    first, second = Score.from_midi(build_single_track_midi(body)).tracks
    assert (first.program, second.program) == (0, 1)
    assert [(cc.time, cc.value) for cc in first.controls] == [(0, 64), (2, 63)]
    assert not second.controls
    assert [(pedal.time, pedal.duration) for pedal in first.pedals()] == [(0, 2)]
