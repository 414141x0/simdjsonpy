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
    query_name: str


SCENARIOS = [
    Scenario(
        name="twitter.json",
        path=ROOT / "ext/simdjson/jsonexamples/twitter.json",
        query_name="search_metadata.count",
    ),
    Scenario(
        name="citm_catalog.json",
        path=ROOT / "ext/simdjson/jsonexamples/citm_catalog.json",
        query_name="len(events)",
    ),
]


def compact_query_stdlib(name: str, payload: bytes) -> int:
    document = json.loads(payload)
    if name == "search_metadata.count":
        return document["search_metadata"]["count"]
    if name == "len(events)":
        return len(document["events"])
    raise ValueError(f"unknown query: {name}")


def compact_query_pysimdjson(
    name: str, parser: simdjson.Parser, payload: bytes
) -> int:
    document = parser.parse(payload)
    if name == "search_metadata.count":
        return document["search_metadata"]["count"]
    if name == "len(events)":
        return len(document["events"])
    raise ValueError(f"unknown query: {name}")


def compact_query_simdjsonpy(
    name: str, parser: simdjsonpy.OnDemandParser, payload: bytes
) -> int:
    document = parser.iterate(payload)
    if name == "search_metadata.count":
        return document["search_metadata"]["count"].get_uint64()
    if name == "len(events)":
        return len(document["events"].get_object())
    raise ValueError(f"unknown query: {name}")


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
    header = (
        "| Parser | `twitter.json` ops/s | `twitter.json` MiB/s | "
        "`citm_catalog.json` ops/s | `citm_catalog.json` MiB/s |"
    )
    separator = "| --- | ---: | ---: | ---: | ---: |"
    rows = [header, separator]
    for parser_name in ("json", "simdjson", "simdjsonpy"):
        twitter_ops, twitter_mib = results["twitter.json"][parser_name]
        citm_ops, citm_mib = results["citm_catalog.json"][parser_name]
        rows.append(
            f"| `{parser_name}` | {twitter_ops:,.0f} | {twitter_mib:,.1f} | "
            f"{citm_ops:,.0f} | {citm_mib:,.1f} |"
        )
    return "\n".join(rows)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repeats", type=int, default=7)
    parser.add_argument("--markdown", action="store_true")
    args = parser.parse_args()

    pysimdjson_parser = simdjson.Parser()
    simdjsonpy_parser = simdjsonpy.OnDemandParser()

    results: dict[str, dict[str, tuple[float, float]]] = {}

    for scenario in SCENARIOS:
        payload = scenario.path.read_bytes()
        expected = compact_query_stdlib(scenario.query_name, payload)
        assert compact_query_pysimdjson(
            scenario.query_name, pysimdjson_parser, payload
        ) == expected
        assert compact_query_simdjsonpy(
            scenario.query_name, simdjsonpy_parser, payload
        ) == expected

        iterations = iterations_for(len(payload))
        scenario_results: dict[str, tuple[float, float]] = {}
        runners = {
            "json": lambda payload=payload, name=scenario.query_name: compact_query_stdlib(
                name, payload
            ),
            "simdjson": lambda payload=payload, name=scenario.query_name: compact_query_pysimdjson(
                name, pysimdjson_parser, payload
            ),
            "simdjsonpy": lambda payload=payload, name=scenario.query_name: compact_query_simdjsonpy(
                name, simdjsonpy_parser, payload
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
                f"  {parser_name:10s}  ops/s={ops_per_second:,.0f}  MiB/s={mib_per_second:,.1f}"
            )


if __name__ == "__main__":
    main()
