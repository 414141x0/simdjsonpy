# Benchmarks

The benchmark harness lives in `benchmarks/compare_parsers.py`. It runs a full-parse workload where every parser materializes the entire document across two real-world payloads from the simdjson corpus.

## Methodology

- Two payloads: `twitter.json` (600 KiB), `citm_catalog.json` (1.7 MiB)
- 5 warm-up calls, then median of 7 timing runs
- Iterations sized so each measurement processes ≥ 128 MiB total
- Reported : operations per second and effective MiB/s of input parsed

## Results

Apple M4 Pro · Python 3.14.3 (free-threaded) · simdjson v4.6.3 · `Release` build with LTO.

| Parser | `twitter.json` ops/s | `twitter.json` MiB/s | `citm_catalog.json` ops/s | `citm_catalog.json` MiB/s |
| --- | ---: | ---: | ---: | ---: |
| **json.loads** | 315 | 189.4 | 152 | 250.6 |
| **pysimdjson** | 4,269 | 2,571.3 | 1,742 | 2,868.6 |
| **simdjsonpy.loads** | 669 | 403.1 | 314 | 518.0 |
| **simdjsonpy DOM** | 4,528 | 2,727.0 | 1,801 | 2,967.0 |
| **simdjsonpy On-Demand** | 8,242 | 4,963.9 | 3,063 | 5,046.0 |


- **`json.loads`** is the stdlib baseline. Slow because Python parses every byte and constructs Python objects in a single pass.
- **`simdjsonpy.loads`** is roughly 2× the baseline. simdjson handles parsing; Returns the same Python objects as `json.loads`.
- **`pysimdjson`** returns lazy proxy objects over a simdjson DOM. Comparable to our DOM surface — both skip Python materialization.
- **`simdjsonpy DOM`** also returns wrapper objects. Slight edge over `pysimdjson` on this workload, mostly from nanobind's lower-overhead binding code.
- **`simdjsonpy On-Demand`** is the fastest. It only validates and walks the bytes lazily; in this workload nothing is consumed beyond the parse, so On-Demand pays only for the SIMD scan.

Calling `dict.get` over the result of `loads`, you pay the materialization cost regardless. 
If you only need a few fields, the wrapper APIs win by a wide margin.

## Reproducing locally

```bash
uv sync --group bench
uv run python benchmarks/compare_parsers.py
uv run python benchmarks/compare_parsers.py --markdown
```

Numbers vary substantially with CPU, build flags, and Python version.
