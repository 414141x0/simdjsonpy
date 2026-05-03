# Development

## Setup

```bash
git clone --recurse-submodules https://github.com/414141x0/simdjsonpy.git
cd simdjsonpy
uv sync --all-groups
```

The repository vendors `simdjson` and `nanobind` as git submodules. 
`uv sync --all-groups` installs the runtime, `bench`, `fuzz`, and `docs` groups together.

## Build

```bash
uv pip install -e . --reinstall
```

Build flags:

```bash
# Debug build with assertions
uv pip install -e . -Ccmake.define.CMAKE_BUILD_TYPE=Debug

# AddressSanitizer (mutually exclusive with UBSan/TSan)
uv pip install -e . -Ccmake.define.SIMDJSONPY_ENABLE_ASAN=ON

# Pin a specific simdjson implementation at compile time
# (set via SIMDJSON_IMPLEMENTATION upstream; see ext/simdjson/CMakeLists.txt)
```

LTO is on by default for `Release` and disabled automatically for sanitizer builds.

## Test

```bash
uv run python -m unittest discover -s tests -v
```

Three suites live in `tests/`:

- `CommonTests`     — helpers, padded strings, implementation introspection
- `DomTests`        — DOM API surface, parser reuse, streaming, threading
- `OnDemandTests`   — On-Demand API surface, raw strings, threading

Plus the property-based suite:

```bash
uv run python -m unittest tests.test_fuzz_properties -v
```

## Benchmark

```bash
uv run python benchmarks/compare_parsers.py
uv run python benchmarks/compare_parsers.py --markdown
```

See [Benchmarks](benchmarks.md) for methodology.

## Stress

```bash
uv run python fuzz/run_api_stress.py --rounds 5000 --workers 4
```

Combine with a sanitizer build for the strongest signal. 

See [Security and fuzzing](security.md).

## Documentation

```bash
uv sync --group docs
mkdocs serve
mkdocs build --strict
```

The site sources live in `docs/`.
