from __future__ import annotations

from pathlib import Path

import symusic

ABC_DIR = Path(__file__).resolve().parent / "testcases" / "abc_files"


def iter_abc_files() -> list[Path]:
    return sorted(ABC_DIR.glob("*.abc"))


def test_read_abc_roundtrip():
    for abc_file in iter_abc_files():
        score = symusic.ScoreTick.from_file(abc_file)
        assert score.note_num() > 0, f"{abc_file.name} should produce notes"
        dumped = score.dumps_abc()
        reparsed = symusic.ScoreTick.from_abc(dumped)
        assert score == reparsed, f"Round-trip mismatch for {abc_file.name}"


def test_dump_abc(tmp_path: Path):
    for abc_file in iter_abc_files():
        score = symusic.ScoreTick.from_file(abc_file)
        out = tmp_path / abc_file.name
        score.dump_abc(out)
        regenerated = symusic.ScoreTick.from_file(out)
        assert regenerated == score, f"Dump mismatch for {abc_file.name}"
