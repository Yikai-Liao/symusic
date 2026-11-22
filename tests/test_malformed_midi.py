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


@pytest.mark.parametrize("policy", ["raise", "clamp"])
def test_malformed_midi_raises_runtime_error(policy):
    with pytest.raises(RuntimeError):
        Score.from_midi(ISSUE_104_PATH.read_bytes(), invalid_data_bytes=policy)


def test_valid_midi_baseline_matches_clamped():
    midi_path = MIDI_PATHS_ALL[0]
    assert Score.from_midi(midi_path.read_bytes(), invalid_data_bytes="clamp") == Score(
        midi_path
    )


def test_format_options_are_memory_only():
    with pytest.raises(TypeError):
        Score(ABC_FIXTURE_PATH, invalid_data_bytes="clamp")
    with pytest.raises(ValueError, match="invalid_data_bytes"):
        Score.from_midi(b"", invalid_data_bytes="sanitize")


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


def test_midi_round_trips_global_events_without_tracks():
    score = Score(ticks_per_quarter=480)
    score.time_signatures.append(TimeSignature(480, 3, 4))
    score.tempos.append(Tempo(0, 120))

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


def test_midi_clamps_pitch_bend_bytes_before_combining():
    body = bytes.fromhex("00 90 3c 40 00 e0 ff 01 01 80 3c 40 00 ff 2f 00")
    data = build_single_track_midi(body)
    with pytest.raises(RuntimeError, match="pitch_bend"):
        Score.from_midi(data)
    score = Score.from_midi(data, invalid_data_bytes="clamp")
    assert score.tracks[0].pitch_bends[0].value == 255 - 8192


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
