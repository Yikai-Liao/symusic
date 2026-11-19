"""Offline tests for Synthesizer rendering and WAV export."""

from __future__ import annotations

import math
import struct
import wave
from typing import TYPE_CHECKING

import numpy as np
import pytest
from symusic import Note, Score, Synthesizer, Track, dump_wav

if TYPE_CHECKING:
    from pathlib import Path

SAMPLE_RATE = 22050
SAMPLE_COUNT = 22050


def _chunk(tag: bytes, payload: bytes) -> bytes:
    assert len(payload) % 2 == 0
    return tag + struct.pack("<I", len(payload)) + payload


def _list(kind: bytes, *chunks: bytes) -> bytes:
    return _chunk(b"LIST", kind + b"".join(chunks))


def _name(text: str) -> bytes:
    return text.encode("ascii").ljust(20, b"\0")


def write_minimal_sf2(path: Path) -> None:
    """Write a one-preset SF2 whose only sample is a unlooped 440 Hz sine."""
    pcm = np.round(
        12000 * np.sin(2 * math.pi * 440 * np.arange(SAMPLE_COUNT) / SAMPLE_RATE)
    ).astype("<i2")
    # The SF2 spec requires 46 zero samples after each sample.
    smpl = pcm.tobytes() + bytes(46 * 2)

    info = _list(b"INFO", _chunk(b"ifil", struct.pack("<HH", 2, 1)))
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
            SAMPLE_COUNT,
            0,
            SAMPLE_COUNT,
            SAMPLE_RATE,
            key_pitch,
            0,
            0,
            1,
        )
        + _name("EOS")
        + struct.pack("<IIIIIBbHH", 0, 0, 0, 0, 0, 0, 0, 0, 0)
    )

    pdta = _list(
        b"pdta",
        _chunk(b"phdr", phdr("piano", 0, 0, 0) + phdr("EOP", 0, 0, 1)),
        _chunk(b"pbag", struct.pack("<HHHH", 0, 0, 1, 0)),
        _chunk(b"pmod", bytes(10)),
        _chunk(b"pgen", gen(41, 0) + gen(0, 0)),  # Instrument 0, terminal
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
def synth(tmp_path_factory: pytest.TempPathFactory) -> Synthesizer:
    sf2 = tmp_path_factory.mktemp("sf2") / "sine.sf2"
    write_minimal_sf2(sf2)
    return Synthesizer(sf2, sample_rate=SAMPLE_RATE)


@pytest.fixture
def score() -> Score:
    score = Score(480)
    track = Track(program=0, is_drum=False, name="t")
    track.notes.append(Note(0, 480, 69, 100))
    score.tracks.append(track)
    return score


@pytest.mark.parametrize("stereo", [True, False])
def test_render_shape_and_dtype(synth: Synthesizer, score: Score, stereo: bool) -> None:
    audio = synth.render(score, stereo=stereo)
    assert isinstance(audio, np.ndarray)
    assert audio.dtype == np.float32
    assert audio.ndim == 2
    assert audio.shape[0] == (2 if stereo else 1)
    assert audio.shape[1] > 0
    assert np.abs(audio).max() > 0


def test_render_other_time_units(synth: Synthesizer, score: Score) -> None:
    for unit in ("quarter", "second"):
        audio = synth.render(score.to(unit), stereo=True)
        assert audio.shape[0] == 2
        assert np.abs(audio).max() > 0


def test_dump_wav_round_trip(synth: Synthesizer, score: Score, tmp_path: Path) -> None:
    audio = synth.render(score, stereo=True)
    out = tmp_path / "out.wav"
    dump_wav(str(out), audio, SAMPLE_RATE, use_int16=True)
    with wave.open(str(out), "rb") as wav:
        assert wav.getnchannels() == 2
        assert wav.getframerate() == SAMPLE_RATE
        assert wav.getsampwidth() == 2
        assert wav.getnframes() == audio.shape[1]
        assert any(wav.readframes(wav.getnframes()))


@pytest.mark.parametrize("quality", [5, 255])
def test_quality_out_of_range_raises(tmp_path: Path, quality: int) -> None:
    sf2 = tmp_path / "sine.sf2"
    write_minimal_sf2(sf2)
    with pytest.raises(ValueError, match="quality"):
        Synthesizer(sf2, sample_rate=SAMPLE_RATE, quality=quality)
