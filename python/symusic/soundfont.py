"""Locate, verify and download curated SoundFonts."""

import os
import urllib.request
from dataclasses import dataclass
from functools import lru_cache
from hashlib import sha384
from http.client import HTTPException
from pathlib import Path
from tempfile import NamedTemporaryFile
from urllib.error import URLError

from platformdirs import user_data_path

_CHUNK_SIZE = 1024 * 1024


def _identity(stat: os.stat_result) -> tuple[int, int, int, int, int]:
    return stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns, stat.st_ctime_ns


@lru_cache(maxsize=32)
def _verify(
    path: Path, identity: tuple[int, int, int, int, int], expected: str
) -> bool:
    digest = sha384()
    with path.open("rb") as file:
        if _identity(os.fstat(file.fileno())) != identity:
            return False
        while chunk := file.read(_CHUNK_SIZE):
            digest.update(chunk)
        return (
            _identity(os.fstat(file.fileno())) == identity
            and digest.hexdigest() == expected
        )


@dataclass(frozen=True)
class Soundfont:
    """Soundfont(name: str, url: str, sha384_digest: str)

    Location and expected SHA-384 digest for a SoundFont artifact."""

    name: str
    url: str
    sha384_digest: str

    def path(self) -> Path:
        """path(self) -> Path

        Return the cache path for this SoundFont."""
        return user_data_path("soundfont", "symusic") / self.name

    def verify(self) -> bool:
        """verify(self) -> bool

        Return whether the cached SoundFont exists and matches its SHA-384 digest."""
        path = self.path()
        try:
            return _verify(path, _identity(path.stat()), self.sha384_digest)
        except FileNotFoundError:
            return False

    def download(self) -> Path:
        """download(self) -> Path

        Return a verified local copy, downloading it if needed."""
        path = self.path()
        if self.verify():
            return path
        path.parent.mkdir(parents=True, exist_ok=True)
        temporary = None
        digest = sha384()
        try:
            with NamedTemporaryFile(dir=path.parent, delete=False) as file:
                temporary = Path(file.name)
                with urllib.request.urlopen(self.url, timeout=30) as response:
                    while chunk := response.read(_CHUNK_SIZE):
                        file.write(chunk)
                        digest.update(chunk)
            if digest.hexdigest() != self.sha384_digest:
                msg = f"SHA-384 mismatch downloading {self.url} to {path}"
                raise ConnectionError(msg)
            os.replace(temporary, path)  # noqa: PTH105
        except (URLError, HTTPException, TimeoutError) as error:
            msg = f"Failed to download {self.url} to {path}"
            raise ConnectionError(msg) from error
        finally:
            if temporary is not None:
                temporary.unlink(missing_ok=True)
        return path


class BuiltInSF3:
    """BuiltInSF3()

    Curated SoundFont 3 (SF3) presets.

    Methods return a :class:`Soundfont` descriptor that can be used to download
    and verify the corresponding asset.
    """

    @staticmethod
    def musescore_general() -> Soundfont:
        """musescore_general() -> Soundfont

        MuseScore General (SF3) preset.

        Returns
        -------
        Soundfont
            Descriptor for the MuseScore General SF3 asset.
        """
        return Soundfont(
            name="MuseScore_General.sf3",
            url="https://ftp.osuosl.org/pub/musescore/soundfont/MuseScore_General/MuseScore_General.sf3",
            sha384_digest="2c7cefb39fb95f8e9258da215736ea1dc325adad6860a2d7a090a33b47235797086c35e8c962ca4c0654bfa6fde6f28c",
        )


class BuiltInSF2:
    """BuiltInSF2()

    Curated SoundFont 2 (SF2) presets.

    Methods return a :class:`Soundfont` descriptor that can be used to download
    and verify the corresponding asset.
    """

    @staticmethod
    def musescore_general() -> Soundfont:
        """musescore_general() -> Soundfont

        MuseScore General (SF2) preset.

        Returns
        -------
        Soundfont
            Descriptor for the MuseScore General SF2 asset.
        """
        return Soundfont(
            name="MuseScore_General.sf2",
            url="https://ftp.osuosl.org/pub/musescore/soundfont/MuseScore_General/MuseScore_General.sf2",
            sha384_digest="bcffbd48c829a094b14ade8102ea412cc48018a21081920ea0f3a0b8003c0c1a61cbee988094e094ef7a64236af67504",
        )
