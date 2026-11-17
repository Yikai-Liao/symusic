"""Build experimental cp312-abi3 wheels on Linux and check their target stubs.

The native build supplies core.pyi from the same source revision. Cross builds
use target Python headers/libraries and explicit wheel tags; target runners
then regenerate the stub and test the installed wheel without rebuilding it.
"""

from __future__ import annotations

import argparse
import difflib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PYTHON_VERSION = "3.12.13"
TARGETS = {
    "aarch64-apple-darwin": {
        "python": "macos-aarch64-none",
        "platform": "macosx_13_0_arm64",
        "runner": "macos-15",
    },
    "x86_64-apple-darwin": {
        "python": "macos-x86_64-none",
        "platform": "macosx_13_0_x86_64",
        "runner": "macos-15-intel",
    },
    "x86_64-pc-windows-msvc": {
        "python": "windows-x86_64-none",
        "platform": "win_amd64",
        "runner": "windows-2022",
    },
    "aarch64-pc-windows-msvc": {
        "python": "windows-aarch64-none",
        "platform": "win_arm64",
        "runner": "windows-11-arm",
    },
    "aarch64-unknown-linux-gnu": {
        "python": "linux-aarch64-gnu",
        "platform": "linux_aarch64",
        "runner": "ubuntu-24.04-arm",
    },
}


def run(*args: str | Path) -> None:
    subprocess.run([str(arg) for arg in args], cwd=ROOT, check=True)


def build(args: argparse.Namespace) -> None:
    if sys.platform != "linux" or sys.version_info[:2] != (3, 12):
        sys.exit("Cross wheels require a Linux CPython 3.12 host")
    target = TARGETS[args.target]
    work = args.work_dir.resolve()
    work.mkdir(parents=True, exist_ok=True)
    python_id = f"cpython-{PYTHON_VERSION}-{target['python']}"
    run(
        "uv",
        "python",
        "install",
        "--no-bin",
        "--install-dir",
        work / "python",
        python_id,
    )
    python_root = work / "python" / python_id
    definitions = {
        "XCLANG_TARGET": args.target,
        "Python_EXECUTABLE": sys.executable,
        "Python_INCLUDE_DIR": python_root / "include" / "python3.12",
        "SYMUSIC_PYTHON_STUB": args.stub.resolve(),
    }
    if target["python"].startswith("windows-"):
        definitions.update(
            Python_INCLUDE_DIR=python_root / "include",
            Python_LIBRARY=python_root / "libs" / "python312.lib",
            Python_SABI_LIBRARY=python_root / "libs" / "python3.lib",
            # Windows imports core.pyd; an abi3.pyd suffix is not recognized.
            SKBUILD_SOSABI="",
        )
    constraints = work / "build-constraints.txt"
    constraints.write_text("scikit-build-core==1.1.2\n", encoding="utf-8")
    settings = {
        "build-dir": work / "build",
        "build.tool-args": f"-j{args.jobs}",
        "cmake.toolchain-file": args.toolchain.resolve(),
        "cmake.python-hints": "false",
        "wheel.tags": f"cp312-abi3-{target['platform']}",
        **{f"cmake.define.{key}": value for key, value in definitions.items()},
    }
    config = [
        part for key, value in settings.items() for part in ("-C", f"{key}={value}")
    ]
    run(
        "uv",
        "build",
        "--wheel",
        "--python",
        sys.executable,
        "--build-constraint",
        constraints,
        "--out-dir",
        args.out_dir.resolve(),
        *config,
    )


def check_stubs() -> None:
    import symusic

    installed = Path(symusic.__file__).with_name("core.pyi")
    with tempfile.TemporaryDirectory() as temp:
        raw = Path(temp) / "core.raw.pyi"
        lifted = Path(temp) / "core.pyi"
        run(
            sys.executable,
            ROOT / "3rdparty/nanobind/src/stubgen.py",
            "-m",
            "symusic.core",
            "-o",
            raw,
        )
        run(sys.executable, ROOT / "scripts/lift_generic_stubs.py", raw, lifted)
        expected = installed.read_text(encoding="utf-8")
        actual = lifted.read_text(encoding="utf-8")
        if expected != actual:
            diff = difflib.unified_diff(
                expected.splitlines(),
                actual.splitlines(),
                fromfile="wheel/core.pyi",
                tofile="target/core.pyi",
                lineterm="",
            )
            sys.exit("\n".join(diff))
    print(f"Target-generated stubs match {installed}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("plan")
    commands.add_parser("check-stubs")
    builder = commands.add_parser("build")
    builder.add_argument("--target", choices=TARGETS, required=True)
    builder.add_argument("--toolchain", type=Path, required=True)
    builder.add_argument("--stub", type=Path, required=True)
    builder.add_argument("--work-dir", type=Path, required=True)
    builder.add_argument("--out-dir", type=Path, required=True)
    builder.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args()
    if args.command == "plan":
        matrix = [
            {"target": name, "runner": spec["runner"]} for name, spec in TARGETS.items()
        ]
        print("matrix=" + json.dumps({"include": matrix}, separators=(",", ":")))
    elif args.command == "check-stubs":
        check_stubs()
    else:
        build(args)


if __name__ == "__main__":
    main()
