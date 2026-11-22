"""Offline tests for the FluidSynth backend and its Python boundary."""

from __future__ import annotations

import ctypes
import math
import os
import struct
import subprocess
import sys
from pathlib import Path

import numpy as np
import pytest
from symusic import (
    ControlChange,
    Note,
    PitchBend,
    Quarter,
    Score,
    Second,
    Synthesizer,
    Tempo,
    Tick,
    Track,
)
from symusic._vendor.fluidsynth import _library

SAMPLE_RATE = 22050
SAMPLE_COUNT = 22050


def _chunk(tag: bytes, payload: bytes) -> bytes:
    assert len(payload) % 2 == 0
    return tag + struct.pack("<I", len(payload)) + payload


def _list(kind: bytes, *chunks: bytes) -> bytes:
    return _chunk(b"LIST", kind + b"".join(chunks))


def _name(text: str) -> bytes:
    return text.encode("ascii").ljust(20, b"\0")


def write_test_soundfont(path: Path) -> None:
    """Write SF2 or SF3 presets backed by one 440 Hz sine sample."""
    pcm = np.round(
        12000 * np.sin(2 * math.pi * 440 * np.arange(SAMPLE_COUNT) / SAMPLE_RATE)
    ).astype("<i2")
    # The SF2 spec requires 46 zero samples after each sample.
    smpl = pcm.tobytes() + bytes(46 * 2)

    sample_end = SAMPLE_COUNT
    sample_type = 1
    version = (2, 1)
    if path.suffix == ".sf3":
        vorbis = (Path(__file__).parent / "testcases/soundfonts/sine.ogg").read_bytes()
        smpl = vorbis + bytes(len(vorbis) % 2)
        sample_end = len(vorbis)
        sample_type |= 0x10
        version = (3, 0)

    info = _list(b"INFO", _chunk(b"ifil", struct.pack("<HH", *version)))
    sdta = _list(b"sdta", _chunk(b"smpl", smpl))

    def phdr(name, preset, bank, bag):
        return _name(name) + struct.pack("<HHHIII", preset, bank, bag, 0, 0, 0)

    def gen(oper, amount):
        return struct.pack("<HH", oper, amount)

    key_pitch = 69
    shdr = (
        _name("sine")
        + struct.pack(
            "<IIIIIBbHH",
            0,
            sample_end,
            0,
            SAMPLE_COUNT,
            SAMPLE_RATE,
            key_pitch,
            0,
            0,
            sample_type,
        )
        + _name("EOS")
        + struct.pack("<IIIIIBbHH", 0, 0, 0, 0, 0, 0, 0, 0, 0)
    )

    presets = [(0, 0, 0), (0, 1, 12), (1, 0, -12), (128, 0, 3), (130, 0, 7)]
    headers = b"".join(
        phdr("sine", program, bank, index)
        for index, (bank, program, _) in enumerate(presets)
    )
    bags = b"".join(
        struct.pack("<HH", index * 2, 0) for index in range(len(presets) + 1)
    )
    generators = b"".join(gen(51, tune & 0xFFFF) + gen(41, 0) for _, _, tune in presets)
    pdta = _list(
        b"pdta",
        _chunk(b"phdr", headers + phdr("EOP", 0, 0, len(presets))),
        _chunk(b"pbag", bags),
        _chunk(b"pmod", bytes(10)),
        _chunk(b"pgen", generators + gen(0, 0)),
        _chunk(
            b"inst",
            _name("sine") + struct.pack("<H", 0) + _name("EOI") + struct.pack("<H", 1),
        ),
        _chunk(b"ibag", struct.pack("<HHHH", 0, 0, 1, 0)),
        _chunk(b"imod", bytes(10)),
        _chunk(b"igen", gen(53, 0) + gen(0, 0)),  # SampleID 0, terminal
        _chunk(b"shdr", shdr),
    )
    body = b"sfbk" + info + sdta + pdta
    path.write_bytes(b"RIFF" + struct.pack("<I", len(body)) + body)


@pytest.fixture(scope="module")
def soundfont(tmp_path_factory: pytest.TempPathFactory) -> Path:
    path = tmp_path_factory.mktemp("sf2") / "sine.sf2"
    write_test_soundfont(path)
    return path


@pytest.fixture(scope="module")
def synth(soundfont: Path) -> Synthesizer:
    return Synthesizer(soundfont, sample_rate=SAMPLE_RATE)


@pytest.fixture
def score() -> Score:
    score = Score(ticks_per_quarter=480)
    track = Track(program=0, is_drum=False, name="sine")
    track.notes.append(Note(0, 480, 69, 100))
    score.tracks.append(track)
    return score


@pytest.mark.parametrize("stereo", [True, False])
def test_render_shape_and_dtype(synth: Synthesizer, score: Score, stereo: bool) -> None:
    audio = synth.render(score, stereo=stereo)
    assert audio.dtype == np.float32
    assert audio.flags.c_contiguous
    # The 0.5 s note ends at frame 11025, dispatched at the next 64-frame block.
    assert audio.shape == (11072 + SAMPLE_RATE, 2 if stereo else 1)
    assert np.abs(audio).max() > 0.01


def test_render_mono_downmix(synth: Synthesizer, score: Score) -> None:
    score.tracks[0].controls.append(ControlChange(0, 10, 100))
    stereo = synth.render(score)
    mono = synth.render(score, stereo=False)
    assert np.max(np.abs(stereo[:, 0] - stereo[:, 1])) > 0.01
    np.testing.assert_allclose(mono[:, 0], stereo.mean(axis=1), atol=1e-6)


@pytest.mark.parametrize("unit", [Tick, Quarter, Second])
def test_render_time_units(
    synth: Synthesizer, score: Score, unit: type[Tick] | type[Quarter] | type[Second]
) -> None:
    np.testing.assert_array_equal(synth.render(score.to(unit)), synth.render(score))


def test_render_tempo_changes(synth, score):
    score.tempos.extend([Tempo(0, 120), Tempo(480, 60)])
    score.tracks[0].notes[0].duration = 960
    expected = math.ceil(1.5 * SAMPLE_RATE / 64) * 64 + SAMPLE_RATE
    for unit in (Tick, Quarter, Second):
        assert synth.render(score.to(unit)).shape == (expected, 2)


def test_sf3_render(score: Score, tmp_path: Path) -> None:
    sf3 = tmp_path / "sine.sf3"
    write_test_soundfont(sf3)
    audio = Synthesizer(sf3, sample_rate=SAMPLE_RATE, tail_seconds=0).render(score)
    assert audio.shape == (11072, 2)
    assert audio.dtype == np.float32
    assert np.abs(audio).max() > 0.01


def _frequency(audio) -> float:
    channel = audio[2048:8192, 0]
    bins = np.abs(np.fft.rfft(channel * np.hanning(len(channel))))
    return float(np.argmax(bins) * SAMPLE_RATE / len(channel))


def test_pitch_bend(synth: Synthesizer, score: Score) -> None:
    plain = synth.render(score)
    score.tracks[0].pitch_bends.append(PitchBend(0, 8191))
    bent = synth.render(score)
    assert _frequency(bent) / _frequency(plain) == pytest.approx(
        2 ** (2 / 12), rel=0.02
    )


def test_control_and_render_isolation(synth: Synthesizer, score: Score) -> None:
    plain = synth.render(score)
    score.tracks[0].controls.append(ControlChange(0, 7, 0))
    assert np.abs(synth.render(score)).max() < np.abs(plain).max() * 0.01
    score.tracks[0].controls.clear()
    np.testing.assert_array_equal(synth.render(score), plain)


def test_track_controls_are_independent(synth: Synthesizer, score: Score) -> None:
    plain = synth.render(score)
    muted = Track(program=0, is_drum=False)
    muted.notes.append(Note(0, 480, 76, 100))
    muted.controls.append(ControlChange(0, 7, 0))
    score.tracks.append(muted)
    np.testing.assert_allclose(synth.render(score), plain, atol=1e-6)


@pytest.mark.parametrize("control", [124, 125, 126, 127])
@pytest.mark.parametrize("track_index", [0, 1])
def test_channel_mode_is_track_local(synth, score, control, track_index):
    other = Track()
    other.notes.append(Note(0, 480, 76, 100))
    score.tracks.append(other)
    track = score.tracks[track_index]
    track.notes[0].duration = 120
    expected = synth.render(score)

    track.notes[0].duration = 480
    track.controls.append(ControlChange(120, control, 0))
    np.testing.assert_allclose(synth.render(score), expected, atol=1e-6)


@pytest.mark.parametrize(
    ("program", "is_drum", "controls", "semitones"),
    [
        (1, False, [], 12),
        (0, True, [], 3),
        (0, False, [(0, 1)], -12),
        (0, False, [(0, 1), (32, 2)], 7),
    ],
)
def test_select_presets(synth, score, program, is_drum, controls, semitones) -> None:
    track = score.tracks[0]
    track.program = program
    track.is_drum = is_drum
    for number, value in controls:
        track.controls.append(ControlChange(0, number, value))
    assert _frequency(synth.render(score)) == pytest.approx(
        440 * 2 ** (semitones / 12), rel=0.02
    )


def test_sustain(synth: Synthesizer, score: Score) -> None:
    track = score.tracks[0]
    track.notes[0].duration = 240
    track.controls.append(ControlChange(720, 64, 0))
    plain = synth.render(score)
    # Put events out of time order to exercise the scheduling sort.
    track.controls.append(ControlChange(120, 64, 127))
    held = synth.render(score)
    assert np.abs(held[9500:13000]).max() > 0.01
    assert np.abs(plain[9500:13000]).max() < 0.001
    assert np.abs(held[19000:20000]).max() < 0.001


def test_same_key_retrigger(synth: Synthesizer, score: Score) -> None:
    score.tracks[0].notes.append(Note(240, 480, 69, 100))
    audio = synth.render(score)
    assert np.abs(audio[13000:15000]).max() > 0.01
    assert np.abs(audio[19000:20000]).max() < 0.001


def test_short_note_gets_one_block(soundfont: Path, score: Score) -> None:
    seconds = score.to(Second)
    seconds.tracks[0].notes[0].duration = 0.00001
    audio = Synthesizer(soundfont, sample_rate=SAMPLE_RATE, tail_seconds=0).render(
        seconds
    )
    assert audio.shape == (64, 2)
    assert np.abs(audio).max() > 0


@pytest.mark.parametrize(("duration", "velocity"), [(0, 100), (0.5, 0)])
def test_invalid_silent_note_time(synth, score, duration, velocity):
    seconds = score.to(Second)
    note = seconds.tracks[0].notes[0]
    note.time = -0.25
    note.duration = duration
    note.velocity = velocity
    with pytest.raises(ValueError, match="finite and nonnegative"):
        synth.render(seconds)


@pytest.mark.parametrize("stereo", [False, True])
@pytest.mark.parametrize("tail_seconds", [None, 0, 0.25])
def test_empty_score_has_no_tail(soundfont, score, stereo, tail_seconds):
    kwargs = {} if tail_seconds is None else {"tail_seconds": tail_seconds}
    synth = Synthesizer(soundfont, sample_rate=SAMPLE_RATE, **kwargs)
    channels = 2 if stereo else 1
    audio = synth.render(Score(), stereo=stereo)
    assert audio.shape == (0, channels)
    assert audio.dtype == np.float32
    tail = 1 if tail_seconds is None else tail_seconds
    assert synth.render(score, stereo=stereo).shape == (
        11072 + math.floor(SAMPLE_RATE * tail + 0.5),
        channels,
    )


@pytest.mark.parametrize("invalid_file", [False, True])
def test_runtime_load_error(monkeypatch, soundfont, tmp_path, invalid_file):
    library = tmp_path / _library.LIBRARY_NAME
    if invalid_file:
        library.write_text("invalid library", encoding="utf-8")
    monkeypatch.setattr(_library, "library_path", lambda: library)
    with pytest.raises(RuntimeError, match="Cannot load FluidSynth") as error:
        Synthesizer(soundfont)
    assert str(library) in str(error.value)


def test_runtime_reports_missing_symbol(monkeypatch, soundfont):
    library = _library.library_path().with_name(_library.SNDFILE_NAME)
    monkeypatch.setattr(_library, "library_path", lambda: library)
    with pytest.raises(RuntimeError, match="is missing fluid_version") as error:
        Synthesizer(soundfont)
    assert str(library) in str(error.value)


def test_runtime_layout():
    runtime = _library.library_path().parent
    files = {path.name for path in runtime.iterdir() if path.suffix != ".txt"}
    assert files - {"__pycache__", "_library.py"} == {
        _library.LIBRARY_NAME,
        _library.SNDFILE_NAME,
    }
    for name in (_library.LIBRARY_NAME, _library.SNDFILE_NAME):
        assert (runtime / name).is_file()
        assert not (runtime / name).is_symlink()
    for name in ("FluidSynth", "libsndfile", "libogg", "libvorbis", "GCEM"):
        assert (runtime / f"{name}.txt").is_file()


def test_soundfont_required() -> None:
    with pytest.raises(TypeError, match="soundfont"):
        Synthesizer()


def test_missing_soundfont(tmp_path: Path) -> None:
    with pytest.raises(FileNotFoundError, match=r"missing.sf2"):
        Synthesizer(tmp_path / "missing.sf2")


def test_render_missing_soundfont(soundfont, score, tmp_path):
    path = tmp_path / "removed.sf2"
    path.write_bytes(soundfont.read_bytes())
    synth = Synthesizer(path)
    path.unlink()
    with pytest.raises(FileNotFoundError) as error:
        synth.render(score)
    assert error.value.filename == str(path)


@pytest.mark.skipif(not hasattr(os, "mkfifo"), reason="requires POSIX named pipes")
def test_render_fifo_soundfont(tmp_path):
    path = tmp_path / "pipe.sf2"
    os.mkfifo(path)
    # Bound the wait even if native code blocks while opening the pipe.
    program = """
import errno
import sys
from symusic import Score, Synthesizer

path = sys.argv[1]
try:
    Synthesizer(path).render(Score())
except OSError as error:
    assert error.errno == errno.EINVAL, error
    assert error.filename == path, error
else:
    raise AssertionError("Expected rendering with a FIFO to raise OSError")
"""
    subprocess.run([sys.executable, "-c", program, str(path)], check=True, timeout=5)


@pytest.mark.skipif(
    os.name == "nt", reason="chmod does not revoke read access on Windows"
)
def test_render_unreadable_soundfont(soundfont, score, tmp_path):
    path = tmp_path / "unreadable.sf2"
    path.write_bytes(soundfont.read_bytes())
    synth = Synthesizer(path)
    path.chmod(0)
    try:
        if os.access(path, os.R_OK):
            pytest.skip("the test process can read files without read permission")
        with pytest.raises(PermissionError) as error:
            synth.render(score)
        assert error.value.filename == str(path)
    finally:
        path.chmod(0o600)


def test_render_malformed_soundfont(score, tmp_path, capfd):
    path = tmp_path / "malformed.sf2"
    path.write_bytes(b"invalid SoundFont")
    synth = Synthesizer(path)
    with pytest.raises(RuntimeError, match="Cannot load SoundFont") as error:
        synth.render(score)
    assert str(path) in str(error.value)
    assert capfd.readouterr() == ("", "")
    assert "RIFF" in str(error.value)

    runtime = ctypes.CDLL(str(_library.library_path()))
    runtime.fluid_log.argtypes = (ctypes.c_int, ctypes.c_char_p)
    runtime.fluid_log(1, b"unrelated FluidSynth error")  # FLUID_ERR
    assert capfd.readouterr() == ("", "")


@pytest.mark.parametrize("unit", [Tick, Quarter, Second])
def test_note_duration_setter_rejects_negative_values(score, unit):
    note = score.to(unit).tracks[0].notes[0]
    original = note.duration
    with pytest.raises(ValueError, match="duration"):
        note.duration = -1
    assert note.duration == original
