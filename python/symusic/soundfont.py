from dataclasses import dataclass
from hashlib import sha384
from pathlib import Path

from platformdirs import user_data_path
from pySmartDL import SmartDL


@dataclass(frozen=True)
class Soundfont:
    name: str
    url: str
    hash_: str

    def path(self, download: bool = True) -> Path:
        if download and not self.exists():
            self.download()
        return user_data_path("soundfont", "symusic") / self.name

    def exists(self) -> bool:
        path = self.path(download=False)
        if not path.exists():
            return False
        with path.open("rb") as f:
            return sha384(f.read()).hexdigest() == self.hash_

    def download(self) -> None:
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
    @staticmethod
    def MuseScoreGeneral() -> Soundfont:
        return Soundfont(
            name="MuseScore_General.sf3",
            url="https://ftp.osuosl.org/pub/musescore/soundfont/MuseScore_General/MuseScore_General.sf3",
            hash_="2c7cefb39fb95f8e9258da215736ea1dc325adad6860a2d7a090a33b47235797086c35e8c962ca4c0654bfa6fde6f28c",
        )


class BuiltInSF2:
    @staticmethod
    def MuseScoreGeneral() -> Soundfont:
        return Soundfont(
            name="MuseScore_General.sf2",
            url="https://ftp.osuosl.org/pub/musescore/soundfont/MuseScore_General/MuseScore_General.sf2",
            hash_="bcffbd48c829a094b14ade8102ea412cc48018a21081920ea0f3a0b8003c0c1a61cbee988094e094ef7a64236af67504",
        )
