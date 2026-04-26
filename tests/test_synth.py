from __future__ import annotations

import inspect
from pathlib import Path

import numpy as np
import pytest
from pydantic import ValidationError

from symusic import (
    BuiltInSF3,
    ChorusWaveform,
    Note,
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
