from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import warnings
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import pytest
from symusic import (
    ABCConversionWarning,
    Note,
    Score,
    Tempo,
    TimeSignature,
    Track,
    _abc,
    core,
)

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


@pytest.fixture
def isolated_symusic_temp_root(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
):
    original_tempdir = tempfile.tempdir
    temp_root = tmp_path / "临时 ABC"
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


def test_read_abc_recovers_with_warning(capfd):
    path = TESTCASES_PATH / "abc_invalid" / "rising_sun_stray_coda.abc"
    with pytest.warns(
        ABCConversionWarning, match=r"Error in line-char .*Malformed note"
    ):
        score = Score(path)
    assert score.note_num() > 0
    assert capfd.readouterr() == ("", "")


@pytest.mark.parametrize(
    ("abc", "diagnostic"),
    [
        ("X:1\nT:Missing key\nC|\n", "No valid K: field"),
        ("X:1\n%%MIDI drumon\nK:C\nC|\n", "drumon must occur after"),
    ],
)
def test_read_abc_reports_failure(abc, diagnostic, capfd):
    with pytest.raises(RuntimeError, match=diagnostic):
        Score.from_abc(abc)
    assert capfd.readouterr() == ("", "")


@pytest.mark.parametrize("ttype", ["tick", "quarter", "second"])
def test_abc_tune_selection(tmp_path: Path, ttype):
    first = make_roundtrip_abc_text().replace("X:1", "X:42")
    second = first.replace("X:42", "X:7").replace("C D E F|", "G G G G|")
    third = first.replace("X:42", "X:0").replace("C D E F|", "A A A A|")
    text = "\n".join((first, second, third))
    path = tmp_path / "book.abc"
    path.write_text(text, encoding="utf-8")

    def pitches(score):
        assert isinstance(score, type(Score(ttype=ttype)))
        return [note.pitch for note in score.tracks[0].notes]

    assert pitches(Score(path, ttype)) == [60, 62, 64, 65]
    assert pitches(Score.from_abc(text, ttype, tune=7)) == [67] * 4
    assert pitches(Score.from_abc(text, ttype, tune=0)) == [69] * 4
    assert pitches(Score.from_abc_file(path, ttype, tune=7)) == [67] * 4
    assert pitches(Score.from_file(path, ttype, tune=7)) == [67] * 4
    assert pitches(Score(path, ttype, tune=7)) == [67] * 4
    with pytest.raises(ValueError, match="X:99"):
        Score.from_abc(text, ttype, tune=99)
    with pytest.raises(ValueError, match="X:0"):
        Score.from_abc(first, ttype, tune=0)


def test_abc_tune_zero_skips_broken_tune():
    broken = "X:1\n%%MIDI drumon\nK:C\nC|\n\n"
    selected = "X:0\nM:1/4\nL:1/4\nK:C\nA|\n"
    score = Score.from_abc(broken + selected, tune=0)
    assert [note.pitch for note in score.tracks[0].notes] == [69]


def test_abc_tune_zero_ignores_text_blocks():
    text = (
        "%%begintext\nX:0\n%%endtext\n"
        "X:1\nM:1/4\nL:1/4\nK:C\nC|\n\n"
        "X:0\nM:1/4\nL:1/4\nK:C\nA|\n"
    )
    score = Score.from_abc(text, tune=0)
    assert [note.pitch for note in score.tracks[0].notes] == [69]


@pytest.mark.parametrize("markers", ["%%begintext %%endtext", "%%endtext %%begintext"])
def test_abc_endtext_wins_on_same_line(markers):
    abc = f"%%begintext\n% {markers}\nX:0\nM:1/4\nL:1/4\nK:C\nA|\n"
    score = Score.from_abc(abc, tune=0)
    assert [note.pitch for note in score.tracks[0].notes] == [69]


def test_abc_tune_zero_in_include(tmp_path, monkeypatch):
    monkeypatch.chdir(tmp_path)
    Path("tune.abc").write_bytes(b"X:000\rM:1/4\rL:1/4\rK:C\rA|")
    abc = "X:1\nK:C\nC|\n\n%%abc-include tune.abc\n\nX:2\nK:C\nB|\n"
    score = Score.from_abc(abc, tune=0)
    assert [note.pitch for note in score.tracks[0].notes] == [69]


def test_abc_tune_zero_missing_with_broken_tune():
    broken = "X:1\nK:C\n@C|\n"
    with pytest.raises(ValueError, match="X:0"):
        Score.from_abc(broken, tune=0)


def test_abc_tune_reference_must_match_exactly():
    with pytest.raises(ValueError, match="X:214748363"):
        Score.from_abc("X:2147483639\nK:C\nC|\n", tune=214748363)


def test_abc_tune_reference_prefers_exact_match():
    text = "X:214748363\nK:C\nA|\n\nX:2147483639\nK:C\nC|\n"
    with pytest.warns(ABCConversionWarning, match="Number too big"):
        score = Score.from_abc(text, tune=214748363)
    assert [note.pitch for note in score.tracks[0].notes] == [69]


@pytest.mark.usefixtures("isolated_symusic_temp_root")
def test_abc_include_resolves_from_caller(tmp_path, monkeypatch):
    monkeypatch.chdir(tmp_path)
    Path("notes.abc").write_text("C D\n", encoding="utf-8")
    abc = make_roundtrip_abc_text().replace("C D E F|", "%%abc-include notes.abc\nE F|")
    score = Score.from_abc(abc)
    assert [note.pitch for note in score.tracks[0].notes] == [60, 62, 64, 65]


@pytest.mark.parametrize("include", [False, True])
@pytest.mark.parametrize("prefix", ["%%MIDI", "I:MIDI", "I:\tMIDI"])
def test_abc_stress_resolves_from_caller(tmp_path, monkeypatch, include, prefix):
    monkeypatch.chdir(tmp_path)
    Path("stress.txt").write_text("4\n95 1\n80 1\n85 1\n80 1\n95 1\n")
    directive = f"{prefix} ptstress stress.txt"
    if include:
        Path("directives.abc").write_text(directive + "\n")
        directive = "%%abc-include directives.abc"
    abc = make_roundtrip_abc_text().replace("C D E F|", f"{directive}\nC D E F|")
    score = Score.from_abc(abc)
    assert [note.pitch for note in score.tracks[0].notes] == [60, 62, 64, 65]


@pytest.mark.parametrize("absolute", [False, True])
def test_abc_stress_instruction_comment(tmp_path, monkeypatch, absolute):
    monkeypatch.chdir(tmp_path)
    path = tmp_path / "stress pattern.txt"
    path.write_text("4\n95 1\n80 1\n85 1\n80 1\n95 1\n")
    filename = path if absolute else path.name
    abc = make_roundtrip_abc_text()
    expected = Score.from_abc(
        abc.replace("C D E F|", f"I:MIDI ptstress {filename}\nC D E F|")
    )
    commented = abc.replace(
        "C D E F|", f"I:MIDI ptstress {filename}% stress comment\nC D E F|"
    )
    assert Score.from_abc(commented) == expected
    midi, _ = _abc._read_abc(commented.encode(), tune=None)
    assert b" stress comment" in midi


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


def test_dump_abc_is_quiet_and_has_no_title(tmp_path, capfd):
    score = make_roundtrip_score()
    destination = tmp_path / "score.abc"
    abc_text = score.dumps_abc(warn=True)
    score.dump_abc(destination, True)

    assert abc_text.startswith("X: 1\n")
    assert not any(line.startswith("T:") for line in abc_text.splitlines())
    assert "calling midi2abc" not in abc_text
    assert destination.read_bytes() == abc_text.encode("utf-8")
    assert capfd.readouterr() == ("", "")


def test_dumps_abc_warning(monkeypatch, capfd):
    stdout = b"calling midi2abc\nconverter stdout\nX: 1\nT: from input.mid\nK:C\nC|\n"

    def run(args, **kwargs):
        return subprocess.CompletedProcess(args, 0, stdout, b"converter stderr\n")

    monkeypatch.setattr(_abc.subprocess, "run", run)
    score = make_roundtrip_score()
    with warnings.catch_warnings(record=True) as recorded:
        assert score.dumps_abc(warn=False) == "X: 1\nK:C\nC|\n"
    assert not recorded
    with pytest.warns(
        ABCConversionWarning, match=r"converter stdout\s+converter stderr"
    ):
        score.dumps_abc(warn=True)
    assert capfd.readouterr() == ("", "")


def test_dump_abc_failure_includes_diagnostics(tmp_path, capfd):
    output = tmp_path / "score.abc"
    output.write_text("existing content", encoding="utf-8")
    with pytest.raises(
        RuntimeError, match=r"(?s)calling midi2abc.*MIDI file has no notes"
    ):
        Score().dump_abc(output)
    assert output.read_text(encoding="utf-8") == "existing content"
    assert capfd.readouterr() == ("", "")


@pytest.mark.usefixtures("isolated_symusic_temp_root")
def test_abc_conversion_uses_one_directory(monkeypatch, tmp_path):
    directories = []
    temporary_directory = tempfile.TemporaryDirectory

    def record_directory(*args, **kwargs):
        directory = temporary_directory(*args, **kwargs)
        directories.append(Path(directory.name))
        return directory

    monkeypatch.setattr(_abc.tempfile, "TemporaryDirectory", record_directory)
    source = tmp_path / "source.abc"
    source.write_text(make_roundtrip_abc_text(), encoding="utf-8")
    score = Score.from_abc(make_roundtrip_abc_text())
    Score.from_abc_file(source)
    score.dumps_abc()
    score.dump_abc(tmp_path / "out.abc")

    assert len(directories) == len(set(directories)) == 4
    assert all(path.parent.name == "临时 ABC" for path in directories)
    assert not any(path.exists() for path in directories)


def test_abc_does_not_use_environment(tmp_path):
    keys = ("SYMUSIC_ABC2MIDI", "SYMUSIC_MIDI2ABC")
    env = os.environ.copy()
    for key in keys:
        env.pop(key, None)
    check = """
import os
from symusic import Score

keys = ("SYMUSIC_ABC2MIDI", "SYMUSIC_MIDI2ABC")
assert not any(key in os.environ for key in keys)
for key in keys:
    os.environ[key] = "missing-converter"
score = Score.from_abc("X:1\\nK:C\\nC D E F|\\n")
assert score.dumps_abc().startswith("X: 1\\n")
"""
    subprocess.run([sys.executable, "-c", check], cwd=tmp_path, env=env, check=True)


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


@pytest.mark.parametrize("tool", ["_ABC2MIDI", "_MIDI2ABC"])
@pytest.mark.usefixtures("isolated_symusic_temp_root")
def test_missing_converter_cleans_temp_dirs(tmp_path, monkeypatch, tool):
    missing_converter = tmp_path / "missing-converter.exe"
    monkeypatch.setattr(_abc, tool, missing_converter)
    before = list_symusic_temp_dirs()

    if tool == "_ABC2MIDI":
        with pytest.raises(FileNotFoundError):
            Score.from_abc(make_roundtrip_abc_text())
    else:
        with pytest.raises(FileNotFoundError):
            make_roundtrip_score().dumps_abc()

    assert list_symusic_temp_dirs() == before


@pytest.mark.usefixtures("isolated_symusic_temp_root")
def test_partial_midi_is_rejected(monkeypatch):
    # The writer leaves the track length zero if it fails before patching it.
    midi = bytearray(make_roundtrip_score().dumps_midi())
    midi[18:22] = b"\x00" * 4

    def run(args, *, cwd, **kwargs):
        (cwd / "output.mid").write_bytes(midi[:-8])
        return subprocess.CompletedProcess(
            args,
            1,
            b"MIDI read/write error : error writing",
            b"disk limit",
        )

    monkeypatch.setattr(_abc.subprocess, "run", run)
    before = list_symusic_temp_dirs()
    with pytest.raises(
        RuntimeError, match=r"(?s)incomplete MIDI.*error writing.*disk limit"
    ):
        Score.from_abc(make_roundtrip_abc_text())
    assert list_symusic_temp_dirs() == before


def test_core_score_keeps_abc_methods(tmp_path):
    path = tmp_path / "score.abc"
    score = core.ScoreQuarter.from_abc(make_roundtrip_abc_text())
    assert type(score) is core.ScoreQuarter
    score.dump_abc(path)
    assert core.ScoreQuarter.from_file(path).note_num() == score.note_num()


@pytest.mark.parametrize("path_type", [str, bytes])
def test_core_score_file_pathlike(tmp_path, path_type):
    class FilePath:
        def __init__(self, path):
            self.path = path

        def __fspath__(self):
            return os.fspath(self.path) if path_type is str else os.fsencode(self.path)

    score = core.ScoreTick.from_abc(make_roundtrip_abc_text())
    path = FilePath(tmp_path / "score.abc")
    score.dump_abc(path)
    assert core.ScoreTick.from_file(path).note_num() == score.note_num()
    assert core.ScoreTick.from_abc_file(path).note_num() == score.note_num()
    midi = FilePath(tmp_path / "score.mid")
    score.dump_midi(midi)
    assert core.ScoreTick.from_file(midi) == score


@pytest.mark.parametrize("warn", ["false", 0, None])
def test_dump_abc_requires_bool(tmp_path, warn):
    score = make_roundtrip_score()
    path = tmp_path / "score.abc"
    with pytest.raises(TypeError):
        score.dumps_abc(warn=warn)
    with pytest.raises(TypeError):
        score.dump_abc(path, warn=warn)
    assert not path.exists()


@pytest.mark.parametrize("extension", ["abc", "mid"])
@pytest.mark.parametrize("sanitize_data", ["false", 0, None])
def test_score_from_file_requires_bool(tmp_path, extension, sanitize_data):
    path = tmp_path / f"score.{extension}"
    if extension == "abc":
        path.write_text(make_roundtrip_abc_text(), encoding="utf-8")
    else:
        make_roundtrip_score().dump_midi(path)
    with pytest.raises(TypeError):
        core.ScoreTick.from_file(path, sanitize_data=sanitize_data)


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
