from __future__ import annotations

import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import pytest
from symusic import Note, Score, Tempo, TimeSignature, Track

from tests.utils import TESTCASES_PATH

ABC_PATHS = sorted((TESTCASES_PATH / "abc_files").glob("*.abc"))


def make_roundtrip_score() -> Score:
    score = Score(480)
    track = Track("Roundtrip", 0, False)
    track.notes.append(Note(0, 480, 60, 96))
    track.notes.append(Note(480, 480, 62, 96))
    track.notes.append(Note(960, 960, 64, 96))
    score.tracks.append(track)
    score.tempos.append(Tempo(0, 120))
    score.time_signatures.append(TimeSignature(0, 4, 4))
    return score


def make_roundtrip_abc_text() -> str:
    return "\n".join(
        [
            "X:1",
            "T:Roundtrip",
            "M:4/4",
            "L:1/4",
            "Q:1/4=120",
            "K:C",
            "C D E F|",
            "",
        ],
    )


def list_symusic_temp_dirs() -> set[Path]:
    return {
        path
        for path in Path(tempfile.gettempdir()).glob("symusic-abc-*")
        if path.is_dir()
    }


@pytest.fixture()
def isolated_symusic_temp_root(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
):
    original_tempdir = tempfile.tempdir
    temp_root = tmp_path / "symusic-temp-root"
    temp_root.mkdir()
    monkeypatch.setenv("TMPDIR", str(temp_root))
    monkeypatch.setenv("TMP", str(temp_root))
    monkeypatch.setenv("TEMP", str(temp_root))
    tempfile.tempdir = None
    try:
        yield temp_root
    finally:
        tempfile.tempdir = original_tempdir


@pytest.mark.parametrize("abc_path", ABC_PATHS, ids=lambda path: path.name)
def test_read_abc(abc_path: Path):
    score = Score(abc_path, fmt="abc")

    assert len(score.tracks) > 0
    assert score.note_num() > 0


def test_dump_abc_roundtrip(tmp_path: Path):
    score = make_roundtrip_score()
    output_dir = tmp_path / "abc output"
    output_dir.mkdir()
    output_path = output_dir / "round trip score.abc"

    abc_text = score.dumps_abc()
    assert abc_text.strip()

    score.dump_abc(output_path)

    assert output_path.exists()
    assert output_path.stat().st_size > 0

    loaded_with_ctor = Score(output_path, fmt="abc")
    loaded_with_factory = Score.from_file(output_path, format="abc")

    assert loaded_with_ctor.note_num() == score.note_num()
    assert loaded_with_factory.note_num() == score.note_num()
    assert len(loaded_with_ctor.tracks) == 1
    assert len(loaded_with_factory.tracks) == 1


@pytest.mark.parametrize(
    ("directory_name", "file_name"),
    [
        ("abc output", "round trip score.abc"),
        pytest.param(
            "windows shell like",
            "shell & semicolon ;.abc",
            marks=pytest.mark.skipif(
                sys.platform != "win32",
                reason="Windows-specific path",
            ),
        ),
        pytest.param(
            "shell like",
            'bad"; touch symusic-injection-probe; echo ".abc',
            marks=pytest.mark.skipif(
                sys.platform == "win32",
                reason="invalid path characters",
            ),
        ),
        ("多语言 路径", "旋律 导出.abc"),
    ],
)
def test_dump_abc_roundtrip_special_paths(
    tmp_path: Path,
    directory_name: str,
    file_name: str,
):
    score = make_roundtrip_score()
    output_dir = tmp_path / directory_name
    output_dir.mkdir()
    output_path = output_dir / file_name
    injection_probe = Path.cwd() / "symusic-injection-probe"
    if injection_probe.exists():
        injection_probe.unlink()

    try:
        score.dump_abc(output_path)
    finally:
        assert not injection_probe.exists()

    assert output_path.exists()
    loaded_score = Score.from_file(output_path, format="abc")
    assert loaded_score.note_num() == score.note_num()


@pytest.mark.skipif(sys.platform != "linux", reason="shell script test is Linux-only")
def test_dumps_abc_warn_false_suppresses_stderr(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture[str],
):
    converter = tmp_path / "midi2abc.sh"
    converter.write_text(
        "\n".join(
            [
                "#!/bin/sh",
                'printf "suppressed stderr\\n" >&2',
                "cat > \"$3\" <<'EOF'",
                "X:1",
                "T:Suppressed",
                "M:4/4",
                "L:1/4",
                "Q:1/4=120",
                "K:C",
                "C D E F|",
                "EOF",
                "",
            ],
        ),
    )
    converter.chmod(0o755)
    monkeypatch.setenv("SYMUSIC_MIDI2ABC", str(converter))

    abc_text = make_roundtrip_score().dumps_abc(warn=False)
    captured = capsys.readouterr()

    assert "Suppressed" in abc_text
    assert captured.err == ""


def test_abc_conversion_is_safe_under_parallel_calls(tmp_path: Path):
    score = make_roundtrip_score()
    abc_text = make_roundtrip_abc_text()
    expected_from_abc = Score.from_abc(abc_text).note_num()

    def dump_once(index: int) -> int:
        output_path = tmp_path / f"parallel {index}.abc"
        score.dump_abc(output_path)
        return Score.from_file(output_path, format="abc").note_num()

    def load_once(_: int) -> int:
        return Score.from_abc(abc_text).note_num()

    with ThreadPoolExecutor(max_workers=8) as executor:
        dumped = list(executor.map(dump_once, range(4)))
        loaded = list(executor.map(load_once, range(4)))

    assert dumped == [score.note_num()] * 4
    assert loaded == [expected_from_abc] * 4


@pytest.mark.usefixtures("isolated_symusic_temp_root")
def test_from_abc_missing_converter_cleans_temp_dirs(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
):
    missing_converter = tmp_path / "missing-abc2midi"
    if sys.platform == "win32":
        missing_converter = missing_converter.with_suffix(".exe")

    monkeypatch.setenv("SYMUSIC_ABC2MIDI", str(missing_converter))
    before = list_symusic_temp_dirs()

    with pytest.raises(RuntimeError, match="Failed to start process"):
        Score.from_abc(make_roundtrip_abc_text())

    assert list_symusic_temp_dirs() == before


@pytest.mark.skipif(sys.platform != "linux", reason="shell script test is Linux-only")
@pytest.mark.usefixtures("isolated_symusic_temp_root")
def test_from_abc_failure_cleans_temp_dirs(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
):
    failing_converter = tmp_path / "abc2midi.sh"
    failing_converter.write_text(
        "\n".join(
            [
                "#!/bin/sh",
                'printf "abc2midi failed on purpose\\n" >&2',
                "exit 23",
                "",
            ],
        ),
    )
    failing_converter.chmod(0o755)
    monkeypatch.setenv("SYMUSIC_ABC2MIDI", str(failing_converter))

    before = list_symusic_temp_dirs()

    with pytest.raises(RuntimeError, match="Process exited with code 23"):
        Score.from_abc(make_roundtrip_abc_text())

    assert list_symusic_temp_dirs() == before


# ---------------------------------------------------------------------------
# Regression test for GitHub issue #78
# https://github.com/lzqlzzq/symusic/issues/78
# ---------------------------------------------------------------------------

ISSUE78_MIDI = TESTCASES_PATH / "issue78_dump_abc.mid"


@pytest.mark.skipif(not ISSUE78_MIDI.exists(), reason="issue78 test MIDI not found")
def test_dump_abc_issue78_regression(tmp_path: Path):
    """Regression test for GitHub issue #78.

    ``score.dump_abc()`` used to fail with ``RuntimeError: midi2abc execution
    failed (return code: 512)`` when processing certain MIDI files.  Now that
    symusic bundles its own midi2abc, this must succeed.
    """
    score = Score(ISSUE78_MIDI)
    assert score.note_num() > 0, "sanity: the MIDI file should contain notes"

    # --- dump_abc to file must not raise ---
    output_path = tmp_path / "issue78.abc"
    score.dump_abc(output_path)

    assert output_path.exists(), "ABC file was not created"
    assert output_path.stat().st_size > 0, "ABC file is empty"

    # --- dumps_abc (to string) must also work ---
    abc_text = score.dumps_abc()
    assert abc_text.strip(), "dumps_abc returned empty text"

    # --- roundtrip: reload the ABC and verify note count ---
    reloaded = Score(output_path, fmt="abc")
    assert reloaded.note_num() > 0, "reloaded Score has no notes"
