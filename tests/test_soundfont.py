"""SoundFont cache and download tests with no network access."""

import io
import ssl
from hashlib import sha384
from urllib.error import URLError

import pytest
from symusic import soundfont

PAYLOAD = b"test SoundFont"


@pytest.fixture
def font(monkeypatch, tmp_path):
    monkeypatch.setattr(soundfont, "user_data_path", lambda *_: tmp_path)
    return soundfont.Soundfont(
        "test.sf3", "https://example.com/test.sf3", sha384(PAYLOAD).hexdigest()
    )


def test_path_has_no_side_effect(font, monkeypatch):
    def unexpected_download(*args, **kwargs):
        pytest.fail("locating a SoundFont opened the network")

    monkeypatch.setattr(soundfont.urllib.request, "urlopen", unexpected_download)
    assert font.path().name == "test.sf3"
    assert not font.path().exists()
    assert not font.verify()


def test_cache_verification_is_reused_and_invalidated(font, monkeypatch):
    font.path().write_bytes(PAYLOAD)
    original = soundfont.sha384
    calls = []

    def hash_file():
        calls.append(None)
        return original()

    monkeypatch.setattr(soundfont, "sha384", hash_file)
    assert font.verify()
    assert font.verify()
    assert len(calls) == 1
    replacement = font.path().with_suffix(".part")
    replacement.write_bytes(b"!" * len(PAYLOAD))
    replacement.replace(font.path())
    assert not font.verify()
    assert len(calls) == 2


def test_download_publishes_verified_file(font, monkeypatch):
    def open_url(url, *, timeout):
        assert url == font.url
        assert timeout > 0
        assert font.path().read_bytes() == b"previous copy"
        return io.BytesIO(PAYLOAD)

    font.path().write_bytes(b"previous copy")
    monkeypatch.setattr(soundfont.urllib.request, "urlopen", open_url)
    assert font.download() == font.path()
    assert font.verify()
    assert list(font.path().parent.iterdir()) == [font.path()]
    assert font.download() == font.path()


@pytest.mark.parametrize("failure", ["url", "ssl", "digest"])
def test_failed_download_preserves_destination(font, monkeypatch, failure):
    class FailingStream:
        def __enter__(self):
            return self

        def __exit__(self, *args):
            return False

        def read(self, size):
            msg = "connection reset"
            raise ssl.SSLError(msg)

    def open_url(*args, **kwargs):
        if failure == "url":
            reason = "offline"
            raise URLError(reason)
        if failure == "ssl":
            return FailingStream()
        return io.BytesIO(b"invalid content")

    font.path().write_bytes(b"previous copy")
    monkeypatch.setattr(soundfont.urllib.request, "urlopen", open_url)
    with pytest.raises(ConnectionError) as error:
        font.download()
    assert str(font.path()) in str(error.value)
    assert font.path().read_bytes() == b"previous copy"
    assert list(font.path().parent.iterdir()) == [font.path()]


def test_builtin_names():
    assert soundfont.BuiltInSF2.musescore_general().name.endswith(".sf2")
    assert soundfont.BuiltInSF3.musescore_general().name.endswith(".sf3")
