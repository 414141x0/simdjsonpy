# simdjsonpy

Typed Python 3.14+ bindings for [simdjson](https://simdjson.org) using [nanobind](https://github.com/wjakob/nanobind).

## Installation

Install directly from GitHub with [`uv`](https://docs.astral.sh/uv/):

```bash
uv pip install git+https://github.com/414141x0/simdjsonpy.git
```

Requires Python >= 3.14 and a C++20 compiler.

## Quick start

Drop-in replacement for `json.loads`:

```python
import simdjsonpy

data = simdjsonpy.loads('{"name": "Alice", "scores": [98, 95, 100]}')
# {'name': 'Alice', 'scores': [98, 95, 100]}
```

`loads` accepts `str`, `bytes`, `bytearray`, `memoryview`, or `PaddedString`. It returns native Python objects (`dict`, `list`, `str`, `int`, `float`, `bool`, `None`).

`dumps` delegates to `json.dumps` for serialization:

```python
simdjsonpy.dumps(data)
# '{"name": "Alice", "scores": [98, 95, 100]}'
```

## Advanced APIs

For maximum throughput, use the DOM or On-Demand APIs directly, they return lightweight wrapper objects instead of materializing Python dicts/lists.

### DOM (full parse, random access)

```python
parser = simdjsonpy.DOMParser()
doc = parser.parse('{"users": [{"name": "Alice"}, {"name": "Bob"}]}')
doc["users"].get_array().at(0)["name"].get_string()  # 'Alice'
```

The DOM parser builds a complete tape representation. Parsed documents support random access, iteration, and JSON Pointer queries.

### On-Demand (lazy, streaming)

```python
parser = simdjsonpy.OnDemandParser()
doc = parser.iterate('{"users": [{"name": "Alice"}, {"name": "Bob"}]}')
doc["users"].get_array()[0].get_object().find_field("name").get_string()  # 'Alice'
```

The On-Demand parser processes tokens lazily — only fields accessed are parsed. Ideal for extracting a few values from large documents.

### Streaming (NDJSON / concatenated)

```python
parser = simdjsonpy.DOMParser()
for doc in parser.parse_many(b'{"a":1}\n{"a":2}\n{"a":3}'):
    print(doc["a"].get_int64())
```

## Benchmarks

Python 3.14.3 (free-threaded) : simdjson v4.6.3 : M4. Median of 7 runs where each parser executes a full parse of the document.

| Parser | `twitter.json` ops/s | `twitter.json` MiB/s | `citm_catalog.json` ops/s | `citm_catalog.json` MiB/s |
| --- | ---: | ---: | ---: | ---: |
| **json.loads** | 315 | 189.4 | 152 | 250.6 |
| **pysimdjson** | 4,269 | 2,571.3 | 1,742 | 2,868.6 |
| **simdjsonpy.loads** | 669 | 403.1 | 314 | 518.0 |
| **simdjsonpy DOM** | 4,528 | 2,727.0 | 1,801 | 2,967.0 |
| **simdjsonpy On-Demand** | 8,242 | 4,963.9 | 3,063 | 5,046.0 |

- **`simdjsonpy.loads`** — 2x faster than `json.loads`. Returns native Python objects.
- **`simdjsonpy DOM`** — 14x faster than `json.loads`. Returns wrapper objects backed by simdjson's tape. No Python dicts/lists are created.
- **`simdjsonpy On-Demand`** — 26x faster than `json.loads`. Lazy token-by-token parsing.
- **`pysimdjson`** — 13x faster than `json.loads`. Returns lazy proxy objects over simdjson's DOM.

Reproduce with:
```bash
uv sync --group bench
uv run python benchmarks/compare_parsers.py
```

## Thread safety

All parsers are safe to share across threads in free-threaded Python 3.14+. Each parse call acquires a per-object mutex, so multiple threads can parse concurrently using different parsers without contention:

```python
from concurrent.futures import ThreadPoolExecutor
import simdjsonpy

parser = simdjsonpy.DOMParser()

def parse(i):
    return simdjsonpy.loads(f'{{"id": {i}}}')

with ThreadPoolExecutor(max_workers=8) as pool:
    results = list(pool.map(parse, range(1000)))
```

`loads` uses a thread-local parser internally, so concurrent calls never contend.

## Development

```bash
uv sync
uv run python -m unittest discover -s tests -v
```

## Documentation

- [Overview](docs/index.md)
- [Quickstart](docs/quickstart.md)
- [Benchmarks](docs/benchmarks.md)
- [Security & Fuzzing](docs/security.md)
- [Development](docs/development.md)
