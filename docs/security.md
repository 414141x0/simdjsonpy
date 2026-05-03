# Security and fuzzing

simdjsonpy parses untrusted input. We tried.

## Property-based tests

`tests/test_fuzz_properties.py` uses Hypothesis to generate JSON values, drive them through the parser, and assert that the result round-trips against the standard library:

```bash
uv sync --group fuzz
uv run python -m unittest tests.test_fuzz_properties -v
```

Three properties are checked:

- DOM round-trip matches `json.loads`
- On-Demand round-trip matches `json.loads`
- Streaming APIs (`parse_many` / `iterate_many`) match the per-document output of `json.loads`

There is also a "raw bytes never leak C++ exceptions" property: arbitrary bytes are fed to every public entry point, and the test asserts that nothing escapes as a non-Python exception.

## Mutation stress

`fuzz/run_api_stress.py` seeds from the simdjson example corpus, mutates each seed, and calls every public API with the mutated payload across multiple worker threads.

```bash
uv run python fuzz/run_api_stress.py --rounds 5000 --workers 4
```

Use this together with a sanitizer build to catch memory and threading bugs.

## Sanitizer builds

The CMake project exposes three toggles, one at a time:

```bash
# AddressSanitizer
uv run python -m pip install -e . -Ccmake.define.SIMDJSONPY_ENABLE_ASAN=ON

# UndefinedBehaviorSanitizer
uv run python -m pip install -e . -Ccmake.define.SIMDJSONPY_ENABLE_UBSAN=ON

# ThreadSanitizer
uv run python -m pip install -e . -Ccmake.define.SIMDJSONPY_ENABLE_TSAN=ON
```

Sanitizer builds disables LTO. Run the full test suite plus the stress runner under each:

```bash
uv run python -m unittest discover -s tests
uv run python fuzz/run_api_stress.py --rounds 5000 --workers 4
```

ThreadSanitizer exercises the per-instance locks under concurrent load.

## Valgrind

For environments where AddressSanitizer is unavailable:

```bash
valgrind --tool=memcheck --error-exitcode=1 \
  python fuzz/run_api_stress.py --rounds 200 --workers 1
```

## Safe (?)

- **Use-after-free across stream advances.** Mitigated by the generation/revision counter on each stream item; advancing the iterator marks earlier items invalid and a subsequent read raises `ReferenceError`.
- **Buffer races on `bytearray` / `memoryview` inputs.** Mitigated by going through `PyObject_GetBuffer(... PyBUF_CONTIG_RO)` which acquires a buffer lock that prevents concurrent resize.
- **Document size DoS.** Bounded by the parser's `max_capacity` (default `SIMDJSON_MAXSIZE_BYTES`, 4 GiB). Override per-parser if your environment needs a tighter cap.
- **Nesting-depth DoS.** Bounded by `max_depth` (default `DEFAULT_MAX_DEPTH`). Configurable per-parser.
