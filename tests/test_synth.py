from __future__ import annotations

import inspect
import wave
from pathlib import Path

import numpy as np
import pytest
from pydantic import ValidationError

from symusic import (
    BuiltInSF3,
    ChorusWaveform,
    ControlChange,
    Note,
    Pedal,
    Score,
    SynthInterpolation,
    Synthesizer,
    Track,
    dump_wav,
)
from symusic.factory import _SynthSettingsModel


def _make_score() -> Score:
    score = Score.from_tpq(480, ttype="tick")
    track = Track("Piano", 0, False, ttype="tick")
    track.notes.append(Note(0, 480, 60, 100, ttype="tick"))
    track.notes.append(Note(480, 480, 64, 100, ttype="tick"))
    track.notes.append(Note(960, 960, 67, 100, ttype="tick"))
    score.tracks.append(track)
    return score


def _read_pcm16_wav(path: Path) -> np.ndarray:
    with wave.open(str(path), "rb") as wav_file:
        channels = wav_file.getnchannels()
        frames = wav_file.getnframes()
        assert wav_file.getsampwidth() == 2
        data = np.frombuffer(wav_file.readframes(frames), dtype="<i2")
    return data.reshape(frames, channels).T


def _make_melodic_track(name: str, pitch: int) -> Track:
    track = Track(name, 0, False, ttype="tick")
    track.notes.append(Note(0, 240, pitch, 100, ttype="tick"))
    return track


def _make_drum_track(name: str) -> Track:
    track = Track(name, 0, True, ttype="tick")
    track.notes.append(Note(0, 240, 36, 120, ttype="tick"))
    return track


@pytest.fixture(scope="module")
def soundfont_path() -> Path:
    return BuiltInSF3.MuseScoreGeneral().path(download=True)


def test_synthesizer_signature_has_no_legacy_args():
    signature = inspect.signature(Synthesizer)
    assert "backend" not in signature.parameters
    assert "quality" not in signature.parameters
    assert "config" not in signature.parameters
    assert "interpolation" in signature.parameters
    assert "chorus_waveform" in signature.parameters


def test_private_settings_model_normalizes_enums():
    settings = _SynthSettingsModel(
        interpolation="7th-order",
        chorus_waveform="triangle",
    )
    assert settings.interpolation is SynthInterpolation.seventh_order
    assert settings.chorus_waveform is ChorusWaveform.triangle

    settings = _SynthSettingsModel(
        interpolation=SynthInterpolation.linear,
        chorus_waveform=ChorusWaveform.sine,
    )
    assert settings.interpolation is SynthInterpolation.linear
    assert settings.chorus_waveform is ChorusWaveform.sine


def test_private_settings_model_reports_clear_errors():
    with pytest.raises(ValidationError, match="sample_rate"):
        _SynthSettingsModel(sample_rate=1000)

    with pytest.raises(ValidationError, match="interpolation"):
        _SynthSettingsModel(interpolation="bad-mode")

    with pytest.raises(ValidationError, match="chorus_waveform"):
        _SynthSettingsModel(chorus_waveform="square")


@pytest.mark.parametrize("stereo, expected_channels", [(False, 1), (True, 2)])
def test_render_smoke_and_repeated_render_is_numerically_stable(
    soundfont_path: Path,
    stereo: bool,
    expected_channels: int,
):
    score = _make_score()
    synth = Synthesizer(
        soundfont_path,
        44100,
        gain=0.3,
        polyphony=128,
        interpolation="default",
    )

    renders = [synth.render(score, stereo=stereo) for _ in range(3)]
    reference = renders[0]

    assert reference.dtype == np.float32
    assert reference.ndim == 2
    assert reference.shape[0] == expected_channels

    for rendered in renders[1:]:
        assert rendered.shape == reference.shape
        assert np.allclose(rendered, reference)


def test_render_custom_effect_settings(soundfont_path: Path, tmp_path: Path):
    score = _make_score()
    drum_track = Track("Drums", 0, True, ttype="tick")
    drum_track.notes.append(Note(0, 240, 36, 120, ttype="tick"))
    drum_track.notes.append(Note(480, 240, 38, 120, ttype="tick"))
    score.tracks.append(drum_track)

    synth = Synthesizer(
        soundfont_path,
        48000,
        gain=0.25,
        polyphony=192,
        interpolation=SynthInterpolation.linear,
        reverb=False,
        chorus=False,
        chorus_waveform=ChorusWaveform.triangle,
    )
    audio = synth.render(score, stereo=True)

    assert audio.dtype == np.float32
    assert audio.shape[0] == 2
    assert audio.shape[1] > 0
    assert np.max(np.abs(audio)) > 0.0

    wav_path = tmp_path / "render.wav"
    dump_wav(wav_path, audio, 48000)
    assert wav_path.exists()
    assert wav_path.stat().st_size > 44


def test_drum_render_is_independent_of_track_order(soundfont_path: Path):
    score_a = Score.from_tpq(480, ttype="tick")
    score_a.tracks.append(_make_drum_track("Drums"))
    score_a.tracks.append(_make_melodic_track("Piano", 60))

    score_b = Score.from_tpq(480, ttype="tick")
    score_b.tracks.append(_make_melodic_track("Piano", 60))
    score_b.tracks.append(_make_drum_track("Drums"))

    synth = Synthesizer(soundfont_path, 44100, interpolation="default")
    audio_a = synth.render(score_a, stereo=True)
    audio_b = synth.render(score_b, stereo=True)

    assert audio_a.shape == audio_b.shape
    assert np.allclose(audio_a, audio_b)


def test_render_rejects_the_256th_melodic_track(soundfont_path: Path):
    score = Score.from_tpq(480, ttype="tick")
    for index in range(256):
        score.tracks.append(Track(f"Melodic {index}", 0, False, ttype="tick"))

    synth = Synthesizer(soundfont_path, 44100, interpolation="default")
    with pytest.raises(
        RuntimeError,
        match="reserves channel 9 for drum tracks.*255 internal channels.*does not reuse channels",
    ):
        synth.render(score, stereo=False)


def test_direct_render_ignores_track_pedals_but_honors_raw_cc64(soundfont_path: Path):
    base_score = Score.from_tpq(480, ttype="tick")
    base_track = Track("Piano", 0, False, ttype="tick")
    base_track.notes.append(Note(0, 120, 60, 100, ttype="tick"))
    base_score.tracks.append(base_track)

    pedal_score = Score.from_tpq(480, ttype="tick")
    pedal_track = Track("Piano", 0, False, ttype="tick")
    pedal_track.notes.append(Note(0, 120, 60, 100, ttype="tick"))
    pedal_track.pedals.append(Pedal(0, 480, ttype="tick"))
    pedal_score.tracks.append(pedal_track)

    cc_score = Score.from_tpq(480, ttype="tick")
    cc_track = Track("Piano", 0, False, ttype="tick")
    cc_track.notes.append(Note(0, 120, 60, 100, ttype="tick"))
    cc_track.controls.append(ControlChange(0, 64, 127, ttype="tick"))
    cc_track.controls.append(ControlChange(480, 64, 0, ttype="tick"))
    cc_score.tracks.append(cc_track)

    synth = Synthesizer(soundfont_path, 44100, interpolation="default")
    base_audio = synth.render(base_score, stereo=False)
    pedal_audio = synth.render(pedal_score, stereo=False)
    cc_audio = synth.render(cc_score, stereo=False)

    assert base_audio.shape == pedal_audio.shape
    assert np.allclose(base_audio, pedal_audio)
    assert cc_audio.shape[1] >= base_audio.shape[1]
    assert cc_audio.shape[1] > base_audio.shape[1] or not np.allclose(
        base_audio,
        cc_audio[:, : base_audio.shape[1]],
    )


@pytest.mark.parametrize(
    ("audio", "expected"),
    [
        (
            np.array([0.0, 0.25, -0.5, 0.75], dtype=np.float32)[::-1],
            np.array([0.75, -0.5, 0.25, 0.0], dtype=np.float32),
        ),
        (
            np.array([0.0, 0.2, -0.4, 0.6, -0.8, 1.0], dtype=np.float32)[::2],
            np.array([0.0, -0.4, -0.8], dtype=np.float32),
        ),
    ],
)
def test_dump_wav_handles_strided_mono_views(tmp_path: Path, audio: np.ndarray, expected: np.ndarray):
    wav_path = tmp_path / "mono.wav"
    dump_wav(wav_path, audio, 16000, use_int16=True)

    written = _read_pcm16_wav(wav_path)
    expected_pcm = np.rint(np.clip(expected, -1.0, 1.0) * 32767.0).astype(np.int16)

    assert written.shape == (1, expected.size)
    assert np.array_equal(written[0], expected_pcm)


@pytest.mark.parametrize(
    ("audio", "expected"),
    [
        (
            np.array([[0.0, 0.25, -0.5], [0.5, -0.25, 0.0]], dtype=np.float32)[:, ::-1],
            np.array([[-0.5, 0.25, 0.0], [0.0, -0.25, 0.5]], dtype=np.float32),
        ),
        (
            np.array([[0.0, 0.1, 0.2, 0.3], [-0.1, -0.2, -0.3, -0.4]], dtype=np.float32)[:, ::2],
            np.array([[0.0, 0.2], [-0.1, -0.3]], dtype=np.float32),
        ),
    ],
)
def test_dump_wav_handles_strided_stereo_views(tmp_path: Path, audio: np.ndarray, expected: np.ndarray):
    wav_path = tmp_path / "stereo.wav"
    dump_wav(wav_path, audio, 22050, use_int16=True)

    written = _read_pcm16_wav(wav_path)
    expected_pcm = np.rint(np.clip(expected, -1.0, 1.0) * 32767.0).astype(np.int16)

    assert written.shape == expected_pcm.shape
    assert np.array_equal(written, expected_pcm)
