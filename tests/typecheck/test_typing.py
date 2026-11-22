from __future__ import annotations

import subprocess
import sys
from pathlib import Path

import pytest

CASES = Path(__file__).with_name("cases")


@pytest.mark.parametrize("checker", ["mypy", "pyright"])
@pytest.mark.parametrize("valid", [True, False])
def test_generated_types(checker, valid):
    case = CASES / ("valid.py" if valid else "invalid.py")
    args = [sys.executable, "-m", checker]
    if checker == "mypy":
        args.append("--strict")
    else:
        args.extend(("--pythonpath", sys.executable))
    result = subprocess.run(
        [*args, str(case)], capture_output=True, text=True, check=False
    )
    output = result.stdout + result.stderr
    if valid:
        assert result.returncode == 0, output
        assert "Revealed type" in output or "Type of" in output
    else:
        assert result.returncode == 1, output
        for line, source in enumerate(case.read_text().splitlines(), 1):
            if "# rejected" in source:
                assert f"{case.name}:{line}:" in output, output
