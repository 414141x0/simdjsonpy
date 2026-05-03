# Benchmarks

The benchmark harness lives in `benchmarks/compare_parsers.py`.

It  benchmarks a **quickstart-style query workload** instead of pure raw parse time:

- `twitter.json`: read `search_metadata.count`
- `citm_catalog.json`: read `len(events)`

## Compared Parsers

- `json.loads(...)`
- `simdjson.Parser().parse(...)`
- `simdjsonpy.OnDemandParser().iterate(...)`

## Reproducing benchmark

```bash
uv sync --all-groups
uv run python benchmarks/compare_parsers.py --markdown
```

## Current Results

Numbers were measured on 2026-05-03 with release build. They are machine-specific and should be treated as a regression baseline for the repository.

| Parser | `twitter.json` ops/s | `twitter.json` MiB/s | `citm_catalog.json` ops/s | `citm_catalog.json` MiB/s |
| --- | ---: | ---: | ---: | ---: |
| `json` | 323 | 194.7 | 154 | 254.4 |
| `simdjson` | 4,274 | 2,574.3 | 1,812 | 2,984.1 |
| `simdjsonpy` | 6,038 | 3,636.4 | 3,189 | 5,252.5 |
