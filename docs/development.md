# Development

```bash
uv sync --all-groups
```

- `docs`: `mkdocs`
- `bench`: `pysimdjson`
- `fuzz`: `hypothesis`


## Core Validation Commands

```bash
uv sync
uv run python -m unittest discover -s tests -v
uv run python benchmarks/compare_parsers.py --markdown
uv run python fuzz/run_api_stress.py --rounds 5000 --workers 4
mkdocs build --strict
```

## Design

When adding to the binding, prefer:

- stable ownership models over borrowed-state hacks
- one obvious Python spelling over many aliases
- explicit failure for stale stream items
- fewer public methods when a helper only mirrors a private or unsafe C++ implementation detail
