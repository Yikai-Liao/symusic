from dataclasses import dataclass
from hashlib import sha384
from pathlib import Path

from platformdirs import user_data_path
from pySmartDL import SmartDL


@dataclass(frozen=True)
class Soundfont:
    """Descriptor for a SoundFont artifact (.sf2/.sf3).

    This lightweight record tracks where to fetch a curated font, how to
    verify it, and where it is stored locally. It exposes convenience
    helpers so downstream code can obtain a valid path without duplicating
    download or integrity logic.
    """
    name: str
    """Local filename used when saving the SoundFont."""
    
    url: str
    """Public URL from which the SoundFont can be downloaded."""
    
    hash_: str
    """SHA-384 digest of the file used to verify integrity after download."""

    def path(self, download: bool = True) -> Path:
        """Return the local path to the SoundFont file.

        Parameters
        ----------
        download : bool, default True
            When True, automatically download the file if it is missing or
            fails integrity verification.

        Returns
        -------
        pathlib.Path
            Absolute path where the SoundFont is stored.
        """
        if download and not self.exists():
            self.download()
        return user_data_path("soundfont", "symusic") / self.name

    def exists(self) -> bool:
        """Check whether the local SoundFont file exists and matches the hash.

        Returns
        -------
        bool
            True if the file exists and its SHA-384 digest matches ``hash_``.
        """
        path = self.path(download=False)
        if not path.exists():
            return False
        with path.open("rb") as f:
            return sha384(f.read()).hexdigest() == self.hash_

    def download(self) -> None:
        """Download the SoundFont file and verify its integrity.

        The payload is stored under the platform-specific
        ``platformdirs.user_data_path('soundfont', 'symusic')`` directory.
        Verification uses the expected SHA-384 digest in ``hash_``.
        """
        # Get path
        path = self.path(download=False)
        # Create directory if not exists
        path.parent.mkdir(parents=True, exist_ok=True)
        # Download
        dl = SmartDL(self.url, str(path), threads=8)
        dl.add_hash_verification("sha384", self.hash_)
        dl.start()
        # Check if download is successful
        if not dl.isSuccessful():
            msg = (
                f"Failed to download {self.name} to {self.path}. "
                f"You could try to download it manually from {self.url}, "
                "and pass the path to the synthesizer."
            )
            raise ConnectionError(
                msg,
            )


class BuiltInSF3:
    """Curated SoundFont 3 (SF3) presets.

    Methods return a :class:`Soundfont` descriptor that can be used to download
    and verify the corresponding asset.
    """
    @staticmethod
    def MuseScoreGeneral() -> Soundfont:
        """MuseScore General (SF3) preset.

        Returns
        -------
        Soundfont
            Descriptor for the MuseScore General SF3 asset.
        """
        return Soundfont(
            name="MuseScore_General.sf3",
            url="https://ftp.osuosl.org/pub/musescore/soundfont/MuseScore_General/MuseScore_General.sf3",
            hash_="2c7cefb39fb95f8e9258da215736ea1dc325adad6860a2d7a090a33b47235797086c35e8c962ca4c0654bfa6fde6f28c",
        )


class BuiltInSF2:
    """Curated SoundFont 2 (SF2) presets.

    Methods return a :class:`Soundfont` descriptor that can be used to download
    and verify the corresponding asset.
    """
    @staticmethod
    def MuseScoreGeneral() -> Soundfont:
        """MuseScore General (SF2) preset.

        Returns
        -------
        Soundfont
            Descriptor for the MuseScore General SF2 asset.
        """
        return Soundfont(
            name="MuseScore_General.sf2",
            url="https://ftp.osuosl.org/pub/musescore/soundfont/MuseScore_General/MuseScore_General.sf2",
            hash_="bcffbd48c829a094b14ade8102ea412cc48018a21081920ea0f3a0b8003c0c1a61cbee988094e094ef7a64236af67504",
        )
