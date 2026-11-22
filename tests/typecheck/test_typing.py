from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

import pytest

CASES = Path(__file__).with_name("cases")


@pytest.mark.parametrize(
    "checker",
    [
        pytest.param(
            "mypy",
            marks=pytest.mark.skipif(
                sys.implementation.name == "pypy",
                reason="mypy does not run on PyPy",
            ),
        ),
        "pyright",
    ],
)
@pytest.mark.parametrize("valid", [True, False])
def test_generated_types(checker, valid):
    case = CASES / ("valid.py" if valid else "invalid.py")
    args = [sys.executable, "-m", checker]
    if checker == "mypy":
        args.extend(("--strict", "--no-color-output"))
    else:
        args.extend(("--pythonpath", sys.executable, "--outputjson"))
    result = subprocess.run(
        [*args, str(case)], capture_output=True, text=True, check=False
    )
    output = result.stdout + result.stderr
    rejected = {
        line
        for line, source in enumerate(case.read_text(encoding="utf-8").splitlines(), 1)
        if "# rejected" in source
    }
    if checker == "pyright":
        try:
            diagnostics = json.loads(result.stdout)["generalDiagnostics"]
        except json.JSONDecodeError as exc:
            pytest.fail(
                f"Invalid pyright JSON ({exc})\nstdout:\n{result.stdout}"
                f"\nstderr:\n{result.stderr}"
            )
        error_lines = {
            diagnostic["range"]["start"]["line"] + 1
            for diagnostic in diagnostics
            if diagnostic["severity"] == "error"
            and Path(diagnostic["file"]).resolve() == case.resolve()
        }
        assert error_lines == (set() if valid else rejected), output
    elif valid:
        assert "Revealed type" in result.stdout, output
    else:
        error_lines = {
            int(line)
            for file, line in re.findall(
                r"^(.+?):(\d+)(?::\d+)?: error:", result.stdout, re.MULTILINE
            )
            if Path(file).resolve() == case.resolve()
        }
        assert rejected <= error_lines, output
    assert result.returncode == (0 if valid else 1), output
