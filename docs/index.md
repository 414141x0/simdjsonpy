# simdjsonpy

Python 3.14+ bindings for [simdjson](https://simdjson.org), built with [nanobind](https://github.com/wjakob/nanobind) for free-threaded interpreters.

There are three ways to read JSON, ordered from most ergonomic to fastest:

| API | Returns | When to use |
| --- | --- | --- |
| [`loads`](loads.md) | Native Python `dict`/`list`/etc. | Drop-in replacement for `json.loads`. |
| [`DOM`](dom.md) | Wrapper objects over a parsed tape. | Random access, JSON Pointer queries, repeated traversal. |
| [`On-Demand`](ondemand.md) | Lazy cursor over the bytes. | Extracting a few fields from large documents. |

Streaming variants for NDJSON and concatenated documents live in [Streaming](streaming.md). Shared utilities — `PaddedString`, `validate_utf8`, `minify`, implementation switching — are in [Common helpers](common.md).

## Install

```bash
uv pip install git+https://github.com/414141x0/simdjsonpy.git
```

A C++20 compiler is required to build the native module.

## Hello world

```python
import simdjsonpy

data = simdjsonpy.loads('{"name": "Ada", "ok": true, "scores": [98, 100]}')
# {'name': 'Ada', 'ok': True, 'scores': [98, 100]}
```

For maximum throughput, skip the dict materialization:

```python
parser = simdjsonpy.OnDemandParser()
doc = parser.iterate('{"name": "Ada", "ok": true, "scores": [98, 100]}')
doc["name"].get_string()              # 'Ada'
doc["scores"].get_array().at(0).get_uint64()  # 98
```

## Free-threaded Python

All parser objects guard their internal state with per-instance locks. 
`loads` uses a thread-local DOM parser to avoid contention entirely. 

See [Threading](threading.md) for the model and worked examples.

## Performance

```text
twitter.json         json.loads          315 ops/s    189 MiB/s
                     simdjsonpy.loads    669 ops/s    403 MiB/s   (2x)
                     simdjsonpy DOM    4,528 ops/s  2,727 MiB/s  (14x)
                     simdjsonpy OD     8,242 ops/s  4,964 MiB/s  (26x)
```

Full numbers and methodology: [Benchmarks](benchmarks.md).
