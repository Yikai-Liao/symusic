from __future__ import annotations

from hashlib import sha384
from pathlib import Path

import pytest

from symusic.soundfont import Soundfont


def test_download_uses_pypdl_and_reuses_hash_check(monkeypatch: pytest.MonkeyPatch, tmp_path: Path):
    destination = tmp_path / "downloaded.sf3"
    payload = b"expected soundfont payload"
    recorded_call: dict[str, object] = {}

    monkeypatch.setattr("symusic.soundfont.user_data_path", lambda *_args: tmp_path)

    class FakePypdl:
        def start(self, **kwargs):
            recorded_call.update(kwargs)
            destination.write_bytes(payload)
            return []

    monkeypatch.setattr("symusic.soundfont.Pypdl", FakePypdl)

    soundfont = Soundfont(
        name=destination.name,
        url="https://example.com/soundfont.sf3",
        hash_=sha384(payload).hexdigest(),
    )

    soundfont.download()

    assert recorded_call == {
        "url": soundfont.url,
        "file_path": str(destination),
        "multisegment": False,
        "retries": 3,
        "display": False,
    }
    assert destination.read_bytes() == payload


def test_download_removes_invalid_file_and_raises(monkeypatch: pytest.MonkeyPatch, tmp_path: Path):
    destination = tmp_path / "broken.sf3"
    monkeypatch.setattr("symusic.soundfont.user_data_path", lambda *_args: tmp_path)

    class FakePypdl:
        def start(self, **_kwargs):
            destination.write_bytes(b"corrupted")
            return []

    monkeypatch.setattr("symusic.soundfont.Pypdl", FakePypdl)

    soundfont = Soundfont(
        name=destination.name,
        url="https://example.com/broken.sf3",
        hash_=sha384(b"expected").hexdigest(),
    )

    with pytest.raises(ConnectionError, match="Failed to download broken.sf3"):
        soundfont.download()

    assert not destination.exists()
