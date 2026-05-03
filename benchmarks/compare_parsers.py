from __future__ import annotations

import argparse
import json
import statistics
import time
from dataclasses import dataclass
from pathlib import Path

import simdjson
import simdjsonpy


ROOT = Path(__file__).resolve().parents[1]
MIB = 1024 * 1024
TARGET_BYTES = 128 * MIB


@dataclass(frozen=True)
class Scenario:
    name: str
    path: Path


SCENARIOS = [
    Scenario(name="twitter.json", path=ROOT / "ext/simdjson/jsonexamples/twitter.json"),
    Scenario(
        name="citm_catalog.json",
        path=ROOT / "ext/simdjson/jsonexamples/citm_catalog.json",
    ),
]


def iterations_for(payload_size: int) -> int:
    return max(32, TARGET_BYTES // max(payload_size, 1))


def benchmark(callable_, iterations: int, repeats: int) -> float:
    for _ in range(5):
        callable_()

    samples: list[float] = []
    for _ in range(repeats):
        start = time.perf_counter_ns()
        for _ in range(iterations):
            callable_()
        elapsed_ns = time.perf_counter_ns() - start
        samples.append(elapsed_ns / 1_000_000_000)
    return statistics.median(samples)


def format_table(results: dict[str, dict[str, tuple[float, float]]]) -> str:
    parsers = list(next(iter(results.values())).keys())
    scenarios = list(results.keys())

    header_parts = ["| Parser"]
    sep_parts = ["| ---"]
    for s in scenarios:
        header_parts.extend([f"| `{s}` ops/s", f"| `{s}` MiB/s"])
        sep_parts.extend(["| ---:", "| ---:"])
    header = " ".join(header_parts) + " |"
    sep = " ".join(sep_parts) + " |"

    rows = [header, sep]
    for p in parsers:
        parts = [f"| **{p}**"]
        for s in scenarios:
            ops, mib = results[s][p]
            parts.extend([f"| {ops:,.0f}", f"| {mib:,.1f}"])
        rows.append(" ".join(parts) + " |")
    return "\n".join(rows)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repeats", type=int, default=7)
    parser.add_argument("--markdown", action="store_true")
    args = parser.parse_args()

    pysimdjson_parser = simdjson.Parser()
    simdjsonpy_od_parser = simdjsonpy.OnDemandParser()
    simdjsonpy_dom_parser = simdjsonpy.DOMParser()

    results: dict[str, dict[str, tuple[float, float]]] = {}

    for scenario in SCENARIOS:
        payload = scenario.path.read_bytes()
        iterations = iterations_for(len(payload))
        scenario_results: dict[str, tuple[float, float]] = {}

        runners = {
            "json.loads": lambda p=payload: json.loads(p),
            "pysimdjson": lambda p=payload: pysimdjson_parser.parse(p),
            "simdjsonpy.loads": lambda p=payload: simdjsonpy.loads(p),
            "simdjsonpy DOM": lambda p=payload: simdjsonpy_dom_parser.parse(p),
            "simdjsonpy On-Demand": lambda p=payload: simdjsonpy_od_parser.iterate(
                p
            ),
        }

        for parser_name, runner in runners.items():
            seconds = benchmark(runner, iterations=iterations, repeats=args.repeats)
            ops_per_second = iterations / seconds
            mib_per_second = (len(payload) * iterations) / seconds / MIB
            scenario_results[parser_name] = (ops_per_second, mib_per_second)

        results[scenario.name] = scenario_results

    if args.markdown:
        print(format_table(results))
        return

    for scenario_name, scenario_results in results.items():
        print(scenario_name)
        for parser_name, (ops_per_second, mib_per_second) in scenario_results.items():
            print(
                f"  {parser_name:25s}  ops/s={ops_per_second:,.0f}  MiB/s={mib_per_second:,.1f}"
            )


if __name__ == "__main__":
    main()
