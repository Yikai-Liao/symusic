from dataclasses import dataclass
from hashlib import md5
from pathlib import Path

from platformdirs import user_data_path
from pySmartDL import SmartDL


@dataclass(frozen=True)
class Soundfont:
    name: str
    url: str
    md5: str

    def path(self, download: bool = True) -> Path:
        if download and not self.exists():
            self.download()
        return user_data_path("soundfont", "symusic") / self.name

    def exists(self) -> bool:
        path = self.path(download=False)
        if not path.exists():
            return False
        with open(path, "rb") as f:
            return md5(f.read()).hexdigest() == self.md5

    def download(self) -> None:
        # Get path
        path = self.path(download=False)
        # Create directory if not exists
        path.parent.mkdir(parents=True, exist_ok=True)
        # Download
        dl = SmartDL(self.url, str(path), threads=8)
        dl.add_hash_verification("md5", self.md5)
        print(f"Downloading {self.name} to {path}")
        dl.start()
        # Check if download is successful
        if not dl.isSuccessful():
            raise ConnectionError(
                f"Failed to download {self.name} to {self.path}. "
                + f"You could try to download it manually from {self.url}, "
                + "and pass the path to the synthesizer."
            )


class BuiltInSF3:
    @staticmethod
    def MuseScoreGeneral() -> Soundfont:
        return Soundfont(
            name="MuseScore_General.sf3",
            url="https://ftp.osuosl.org/pub/musescore/soundfont/MuseScore_General/MuseScore_General.sf3",
            md5="7b354559c7ef4e859a2bfad8738d477d",
        )


class BuiltInSF2:
    @staticmethod
    def MuseScoreGeneral() -> Soundfont:
        return Soundfont(
            name="MuseScore_General.sf2",
            url="https://ftp.osuosl.org/pub/musescore/soundfont/MuseScore_General/MuseScore_General.sf2",
            md5="4395439e3e5f9149aae7ea5094442813",
        )
