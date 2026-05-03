# Fuzzing

There are two layers here:

- `tests/test_fuzz_properties.py`: property-based fuzzing for valid JSON and arbitrary byte inputs
- `fuzz/run_api_stress.py`: corpus-and-mutation stress runner for the public API


```bash
uv sync --all-groups
uv run python -m unittest tests.test_fuzz_properties -v
uv run python fuzz/run_api_stress.py --rounds 5000 --workers 4
```

## Sanitizer Builds

CMake exposes three toggles:

- `SIMDJSONPY_ENABLE_ASAN`
- `SIMDJSONPY_ENABLE_UBSAN`
- `SIMDJSONPY_ENABLE_TSAN`

```bash
uv run python -m pip install -e . -Ccmake.define.SIMDJSONPY_ENABLE_ASAN=ON
```

Use one sanitizer mode at a time. When any sanitizer is enabled, LTO is disabled.

## Valgrind

```bash
valgrind --tool=memcheck --error-exitcode=1 \
  python fuzz/run_api_stress.py --rounds 200 --workers 1
```


## Thread Safety

Explicit thread-stress tests for shared parser use are in `tests/test_simdjsonpy.py`.
