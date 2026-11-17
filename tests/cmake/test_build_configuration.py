"""Validate the generated compiler commands for the core-only LTO policy."""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path

import pytest


@pytest.mark.skipif(
    sys.platform != "linux" or shutil.which("cmake") is None,
    reason="Compile-command checks run on Linux with CMake installed",
)
def test_core_lto(tmp_path: Path) -> None:
    root = Path(__file__).resolve().parents[2]

    def configure(enabled: bool) -> str:
        result = subprocess.run(
            [
                "cmake",
                "-S",
                str(root),
                "-B",
                str(tmp_path),
                "-DCMAKE_BUILD_TYPE=Release",
                "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                "-DSYMUSIC_BUILD_PYTHON=ON",
                f"-DSYMUSIC_ENABLE_LTO={'ON' if enabled else 'OFF'}",
                f"-DPython_EXECUTABLE={sys.executable}",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        return result.stdout + result.stderr

    output = configure(True)
    if "Core LTO is unavailable" in output:
        pytest.skip("The host compiler cannot link LTO objects")

    def commands_for(directory: Path) -> list[str]:
        entries = json.loads((tmp_path / "compile_commands.json").read_text())
        return [
            entry["command"]
            for entry in entries
            if Path(entry["file"]).is_relative_to(directory)
        ]

    core = commands_for(root / "src")
    binding = commands_for(root / "py_src")
    nanobind = commands_for(root / "3rdparty" / "nanobind" / "src")
    assert core
    assert binding
    assert nanobind
    assert all("-flto" in command for command in core)
    assert all("-flto" not in command for command in binding + nanobind)

    configure(False)
    assert all("-flto" not in command for command in commands_for(root / "src"))
