"""Compare the performance of two git refs, or time the current environment.

Usage:
    python benchmarks/ab.py BASE HEAD [--workdir DIR] [--rounds N] [--samples N]
    python benchmarks/ab.py --local [--samples N]

A/B mode checks each ref out into its own git worktree under WORKDIR, installs
it in a Release build into its own virtual environment, then alternates the
scenarios of ``scenarios.py`` between the two installs (base first on even
rounds, head first on odd rounds) so slow drift of the machine hits both sides
equally. Both sides run the scenario file and fixtures of the current checkout,
so the refs only supply the library. When both refs resolve to the same commit
a single install is timed twice, which measures the noise floor.

WORKDIR must be outside the source tree. Slowdowns never change the exit
status; only errors do.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import statistics
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCENARIOS = Path(__file__).resolve().parent / "scenarios.py"


def run(cmd: list[str], **kwargs) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, check=True, text=True, **kwargs)


def resolve(ref: str) -> str:
    """Resolve a ref, falling back to the remote branch of the same name (CI clones)."""
    for candidate in (ref, f"origin/{ref}"):
        found = subprocess.run(
            ["git", "rev-parse", "--verify", "--quiet", f"{candidate}^{{commit}}"],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        if found.returncode == 0:
            return found.stdout.strip()
    msg = f"cannot resolve git ref {ref!r}"
    raise SystemExit(msg)


def install(sha: str, workdir: Path, python: str) -> Path:
    """Build ``sha`` in Release mode into a fresh venv and return its interpreter."""
    src, venv = workdir / f"src-{sha[:12]}", workdir / f"venv-{sha[:12]}"
    if not src.exists():
        run(["git", "worktree", "add", "--detach", str(src), sha], cwd=ROOT)
        run(
            ["git", "submodule", "update", "--init", "--recursive", "--depth", "1"],
            cwd=src,
        )
    run(["uv", "venv", "-q", "-p", python, str(venv)])
    exe = venv / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
    run(
        [
            "uv",
            "pip",
            "install",
            "-q",
            "-p",
            str(exe),
            str(src),
            "numpy",
            "-C",
            "cmake.build-type=Release",
        ],
    )
    return exe


def sample(exe: Path, samples: int, only: str) -> dict[str, list[float]]:
    cmd = [
        str(exe),
        str(SCENARIOS),
        "--json",
        "--samples",
        str(samples),
        "--only",
        only,
    ]
    # The native library may print diagnostics; the JSON document is the last line.
    return json.loads(run(cmd, capture_output=True).stdout.strip().splitlines()[-1])["samples_us"]


def table(base: list[dict], head: list[dict], base_name: str, head_name: str) -> str:
    """Markdown table; ``base`` and ``head`` hold one samples dict per round."""
    lines = [
        f"| scenario | {base_name} (us) | {head_name} (us) | head/base | round range |",
        "|---|---:|---:|---:|---:|",
    ]
    for name in base[0]:
        b = statistics.median(x for r in base for x in r[name])
        h = statistics.median(x for r in head for x in r[name])
        per_round = [
            statistics.median(hr[name]) / statistics.median(br[name])
            for br, hr in zip(base, head, strict=True)
        ]
        lines.append(
            f"| {name} | {b:.1f} | {h:.1f} | {h / b:.3f} | {min(per_round):.3f} - {max(per_round):.3f} |"
        )
    lines += [
        "",
        (
            f"Medians pool every sample over {len(base)} interleaved rounds; "
            "round range is the lowest and highest head/base ratio of per-round medians. "
            "Ratios inside the range seen between identical builds are noise."
        ),
    ]
    return "\n".join(lines)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("base", nargs="?", help="git ref used as the baseline")
    parser.add_argument("head", nargs="?", help="git ref under test")
    parser.add_argument(
        "--local", action="store_true", help="time the current environment only"
    )
    parser.add_argument(
        "--workdir", type=Path, default=Path.home() / ".cache" / "symusic-bench"
    )
    parser.add_argument("--rounds", type=int, default=6)
    parser.add_argument(
        "--samples", type=int, default=5, help="samples per scenario per round"
    )
    parser.add_argument(
        "--only", default="", help="run scenarios whose name contains this"
    )
    parser.add_argument(
        "--python", default="3.12", help="CPython version for the venvs"
    )
    parser.add_argument("--keep", action="store_true", help="keep worktrees and venvs")
    parser.add_argument(
        "--summary", type=Path, help="also append the table to this file"
    )
    args = parser.parse_args(argv)

    if args.local:
        return run(
            [
                sys.executable,
                str(SCENARIOS),
                "--samples",
                str(args.samples),
                "--only",
                args.only,
            ]
        ).returncode
    if not (args.base and args.head):
        parser.error("give BASE and HEAD, or --local")

    workdir = args.workdir.resolve()
    if workdir == ROOT or ROOT in workdir.parents:
        parser.error("--workdir must be outside the source tree")
    workdir.mkdir(parents=True, exist_ok=True)

    shas = [resolve(args.base), resolve(args.head)]
    installs: dict[str, Path] = {}
    try:
        for sha in dict.fromkeys(shas):
            print(f"building {sha[:12]}", file=sys.stderr, flush=True)
            installs[sha] = install(sha, workdir, args.python)
        exes = [installs[sha] for sha in shas]
        results: list[list[dict]] = [[], []]
        for rnd in range(args.rounds):
            print(f"round {rnd + 1}/{args.rounds}", file=sys.stderr, flush=True)
            for side in (0, 1) if rnd % 2 == 0 else (1, 0):
                results[side].append(sample(exes[side], args.samples, args.only))
    finally:
        if not args.keep:
            for sha in installs:
                shutil.rmtree(workdir / f"venv-{sha[:12]}", ignore_errors=True)
                subprocess.run(
                    [
                        "git",
                        "worktree",
                        "remove",
                        "--force",
                        str(workdir / f"src-{sha[:12]}"),
                    ],
                    cwd=ROOT,
                )

    out = table(results[0], results[1], f"base {args.base}", f"head {args.head}")
    print(out)
    if args.summary:
        with args.summary.open("a", encoding="utf-8") as fh:
            fh.write(out + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
