#!/usr/bin/env python3
"""Benchmark a verbose pip install and export a machine-readable summary."""

from __future__ import annotations

import argparse
import json
import os
import platform
import re
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

STEP_LINE = re.compile(r"(?:DEBUG\s+)?\[(\d+)/(\d+)\]\s+(.*)")
ELAPSED_LINE = re.compile(r"(?:DEBUG\s+)?Elapsed time \(seconds\): ([0-9.]+)")
PREPARED_LINE = re.compile(r"(?:DEBUG\s+)?Prepared .* in ([0-9.]+)s")


def format_command(cmd: list[str]) -> str:
    return subprocess.list2cmdline(cmd) if os.name == "nt" else " ".join(cmd)


def bucket_step(command: str) -> str:
    if "CMakeFiles/core.dir/" in command or "core.cpython" in command:
        return "python-core"
    if "CMakeFiles/symusic.dir/" in command or "libsymusic.a" in command:
        return "symusic-lib"
    if "prestosynth" in command:
        return "prestosynth"
    if "nanobind-static" in command:
        return "nanobind-static"
    if "abcmidi" in command or "abc2midi" in command or "midi2abc" in command:
        return "abcmidi-tools"
    if "test_multilang_path" in command:
        return "test_multilang_path"
    return "other"


def stream_command(cmd: list[str], log_path: Path) -> int:
    with log_path.open("w", encoding="utf-8", newline="") as log_file:
        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        assert process.stdout is not None
        for line in process.stdout:
            print(line, end="")
            log_file.write(line)
        return process.wait()


def parse_log(log_path: Path) -> dict[str, Any]:
    lines = log_path.read_text(encoding="utf-8", errors="replace").splitlines()
    current_step: str | None = None
    steps: list[dict[str, Any]] = []
    prepared_seconds: float | None = None

    for line in lines:
        if match := PREPARED_LINE.search(line):
            prepared_seconds = float(match.group(1))
        if match := STEP_LINE.search(line):
            current_step = match.group(3).strip()
            continue
        if match := ELAPSED_LINE.search(line):
            if current_step is None:
                continue
            steps.append(
                {
                    "seconds": float(match.group(1)),
                    "command": current_step,
                    "bucket": bucket_step(current_step),
                }
            )
            current_step = None

    bucket_totals: dict[str, float] = {}
    for step in steps:
        bucket = step["bucket"]
        bucket_totals[bucket] = bucket_totals.get(bucket, 0.0) + float(step["seconds"])

    steps.sort(key=lambda item: item["seconds"], reverse=True)
    top_steps = steps[:10]
    sorted_buckets = [
        {"bucket": bucket, "seconds": seconds}
        for bucket, seconds in sorted(bucket_totals.items(), key=lambda item: item[1], reverse=True)
    ]

    return {
        "prepared_seconds": prepared_seconds,
        "top_steps": top_steps,
        "bucket_totals": sorted_buckets,
    }


def write_summary(summary_path: Path, summary: dict[str, Any]) -> None:
    lines = [
        "# Install Benchmark",
        "",
        f"- Command: `{summary['command']}`",
        f"- Wall time: `{summary['wall_seconds']:.2f}s`",
        f"- Exit code: `{summary['exit_code']}`",
        f"- Python: `{summary['python_implementation']} {summary['python_version']}`",
        f"- Platform: `{summary['platform']}`",
    ]
    if summary["prepared_seconds"] is not None:
        lines.append(f"- Build/prepare time: `{summary['prepared_seconds']:.2f}s`")

    lines.extend(["", "## Slowest Steps", "", "| Seconds | Step |", "| ---: | --- |"])
    if summary["top_steps"]:
        for step in summary["top_steps"]:
            lines.append(f"| {step['seconds']:.3f} | `{step['command']}` |")
    else:
        lines.append("| n/a | No CMake timing lines were detected. |")

    lines.extend(["", "## Bucket Totals", "", "| Bucket | Seconds |", "| --- | ---: |"])
    if summary["bucket_totals"]:
        for bucket in summary["bucket_totals"]:
            lines.append(f"| `{bucket['bucket']}` | {bucket['seconds']:.3f} |")
    else:
        lines.append("| `n/a` | 0.000 |")

    summary_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def append_github_summary(summary_path: Path) -> None:
    github_summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if not github_summary:
        return
    with Path(github_summary).open("a", encoding="utf-8") as handle:
        handle.write(summary_path.read_text(encoding="utf-8"))
        handle.write("\n")


def write_github_output(summary: dict[str, Any]) -> None:
    github_output = os.environ.get("GITHUB_OUTPUT")
    if not github_output:
        return
    with Path(github_output).open("a", encoding="utf-8") as handle:
        handle.write(f"install_wall_seconds={summary['wall_seconds']:.6f}\n")
        if summary["prepared_seconds"] is not None:
            handle.write(f"install_prepared_seconds={summary['prepared_seconds']:.6f}\n")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", default="build-bench")
    parser.add_argument("--package", default=".[test]")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    log_path = output_dir / "pip-install.log"
    json_path = output_dir / "summary.json"
    md_path = output_dir / "summary.md"

    command = [sys.executable, "-m", "pip", "install", "-v", args.package]
    started_at = time.perf_counter()
    exit_code = stream_command(command, log_path)
    wall_seconds = time.perf_counter() - started_at

    parsed = parse_log(log_path)
    summary = {
        "command": format_command(command),
        "wall_seconds": wall_seconds,
        "exit_code": exit_code,
        "python_version": platform.python_version(),
        "python_implementation": platform.python_implementation(),
        "platform": platform.platform(),
        "runner_os": os.environ.get("RUNNER_OS"),
        "github_ref": os.environ.get("GITHUB_REF"),
        **parsed,
    }

    json_path.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    write_summary(md_path, summary)
    append_github_summary(md_path)
    write_github_output(summary)
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
