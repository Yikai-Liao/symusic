from __future__ import annotations

import argparse
import statistics
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from symusic import Score

REPO_ROOT = Path(__file__).resolve().parents[1]
MX_RESOURCES = REPO_ROOT / "3rdparty" / "mx" / "Resources"


def discover_cases(pattern: str | None, max_cases: int | None) -> list[Path]:
    cases = sorted(
        path
        for path in MX_RESOURCES.rglob("*")
        if path.suffix in {".xml", ".musicxml"}
    )
    if pattern:
        cases = [path for path in cases if pattern in path.as_posix()]
    if max_cases is not None:
        cases = cases[:max_cases]
    return cases


def benchmark_parse(paths: list[Path], iterations: int, workers: int) -> tuple[float, int]:
    def run_once(path: Path) -> int:
        return Score(path).note_num()

    jobs = paths * iterations
    started = time.perf_counter()
    with ThreadPoolExecutor(max_workers=workers) as executor:
        counts = list(executor.map(run_once, jobs))
    elapsed = time.perf_counter() - started
    return elapsed, sum(counts)


def benchmark_dump(paths: list[Path], iterations: int, workers: int) -> tuple[float, int]:
    scores = [Score(path) for path in paths]

    def run_once(index: int) -> int:
        return len(scores[index].dumps_musicxml())

    jobs = [index for _ in range(iterations) for index in range(len(scores))]
    started = time.perf_counter()
    with ThreadPoolExecutor(max_workers=workers) as executor:
        sizes = list(executor.map(run_once, jobs))
    elapsed = time.perf_counter() - started
    return elapsed, sum(sizes)


def benchmark_roundtrip(paths: list[Path], iterations: int, workers: int) -> tuple[float, int]:
    payloads = [Score(path).dumps_musicxml() for path in paths]

    def run_once(index: int) -> int:
        return Score.from_musicxml(payloads[index]).note_num()

    jobs = [index for _ in range(iterations) for index in range(len(payloads))]
    started = time.perf_counter()
    with ThreadPoolExecutor(max_workers=workers) as executor:
        counts = list(executor.map(run_once, jobs))
    elapsed = time.perf_counter() - started
    return elapsed, sum(counts)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Benchmark MusicXML parse/dump/roundtrip throughput on the mx sample corpus."
    )
    parser.add_argument(
        "--mode",
        choices=("parse", "dump", "roundtrip"),
        default="roundtrip",
        help="Operation to benchmark.",
    )
    parser.add_argument(
        "--workers",
        type=int,
        nargs="+",
        default=[1, 8],
        help="Worker counts to benchmark.",
    )
    parser.add_argument(
        "--iterations",
        type=int,
        default=3,
        help="How many full passes over the selected corpus to run.",
    )
    parser.add_argument(
        "--pattern",
        type=str,
        default=None,
        help="Optional substring filter applied to the relative corpus path.",
    )
    parser.add_argument(
        "--max-cases",
        type=int,
        default=64,
        help="Maximum number of corpus files to include.",
    )
    args = parser.parse_args()

    cases = discover_cases(args.pattern, args.max_cases)
    if not cases:
        raise SystemExit("No MusicXML benchmark cases matched the requested filter")

    runners = {
        "parse": benchmark_parse,
        "dump": benchmark_dump,
        "roundtrip": benchmark_roundtrip,
    }
    runner = runners[args.mode]

    print(f"mode={args.mode} cases={len(cases)} iterations={args.iterations}")
    print(f"corpus_root={MX_RESOURCES}")
    print("workers,elapsed_seconds,ops_per_second,checksum")

    measurements: list[float] = []
    for workers in args.workers:
        elapsed, checksum = runner(cases, args.iterations, workers)
        ops = len(cases) * args.iterations
        ops_per_second = ops / elapsed if elapsed > 0 else float("inf")
        measurements.append(ops_per_second)
        print(f"{workers},{elapsed:.6f},{ops_per_second:.3f},{checksum}")

    if len(measurements) > 1:
        print(
            "ops_per_second_mean="
            f"{statistics.mean(measurements):.3f} "
            f"ops_per_second_stdev={statistics.pstdev(measurements):.3f}"
        )


if __name__ == "__main__":
    main()
