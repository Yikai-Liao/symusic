from __future__ import annotations

import subprocess
import sys

import pytest
from symusic import Score, Tempo, TimeSignature

from tests.utils import MIDI_PATHS_ALL, TESTCASES_PATH

ISSUE_104_PATH = (
    TESTCASES_PATH / "MIDIs_corrupted" / "Issue104_Bizarre_Love_Triangle2.mid"
)
ABC_FIXTURE_PATH = TESTCASES_PATH / "abc_files" / "a_morning_in_summer.abc"


def build_single_track_midi(track_bytes: bytes, tpq: int = 480) -> bytes:
    header = b"MThd" + (6).to_bytes(4, "big")
    header += (0).to_bytes(2, "big")
    header += (1).to_bytes(2, "big")
    header += tpq.to_bytes(2, "big")
    track = b"MTrk" + len(track_bytes).to_bytes(4, "big") + track_bytes
    return header + track


def test_malformed_midi_default_load_is_strict():
    with pytest.raises(RuntimeError):
        Score(ISSUE_104_PATH)


@pytest.mark.parametrize("sanitize_data", [False, True], ids=["strict", "sanitize"])
def test_malformed_midi_raises_runtime_error(sanitize_data: bool):
    with pytest.raises(RuntimeError):
        Score(ISSUE_104_PATH, sanitize_data=sanitize_data)

    with pytest.raises(RuntimeError):
        Score.from_file(ISSUE_104_PATH, format="midi", sanitize_data=sanitize_data)


def test_valid_midi_path_loading_accepts_sanitize_data():
    midi_path = MIDI_PATHS_ALL[0]

    default_score = Score(midi_path)
    sanitized_score = Score(midi_path, sanitize_data=True)
    sanitized_from_file = Score.from_file(midi_path, format="midi", sanitize_data=True)

    assert sanitized_score.note_num() == default_score.note_num()
    assert sanitized_from_file.note_num() == default_score.note_num()
    assert sanitized_score.ticks_per_quarter == default_score.ticks_per_quarter
    assert sanitized_from_file.ticks_per_quarter == default_score.ticks_per_quarter


def test_abc_rejects_sanitize_data():
    with pytest.raises(ValueError, match="sanitize_data"):
        Score.from_file(ABC_FIXTURE_PATH, format="abc", sanitize_data=True)


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
    assert score.note_num() == 1


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
    assert [(pedal.time, pedal.duration) for pedal in channel_track.pedals] == [(0, 3)]
    assert [(bend.time, bend.value) for bend in channel_track.pitch_bends] == [(2, 1)]
    assert Score.from_midi(score.dumps_midi()) == score


def test_midi_round_trips_global_events_without_tracks():
    score = Score(480)
    score.time_signatures.append(TimeSignature(480, 3, 4))
    score.tempos.append(Tempo(0, 120))

    assert Score.from_midi(score.dumps_midi()) == score


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
    assert Score.from_midi(score.dumps_midi()) == score


def test_midi_rejects_unrepresentable_denominator():
    body = bytes.fromhex("00 ff 58 04 04 08 18 08 00 ff 2f 00")
    with pytest.raises(RuntimeError, match="time signature denominator"):
        Score.from_midi(build_single_track_midi(body))


@pytest.mark.skipif(sys.platform != "linux", reason="requires /dev/stdin")
def test_midi_rejects_non_seekable_file():
    script = """
from symusic.core import ScoreTick

try:
    ScoreTick.from_file("/dev/stdin", format="midi")
except RuntimeError as error:
    if "Cannot seek" in str(error):
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
    for parsed in (score, Score.from_midi(score.dumps_midi())):
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
