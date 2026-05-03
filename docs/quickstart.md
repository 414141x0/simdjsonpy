# Quickstart

## Install

```bash
uv pip install git+https://github.com/414141x0/simdjsonpy.git
```

The build needs Python 3.14+ and a C++20 compiler.

## Drop-in for `json.loads`

```python
import simdjsonpy

data = simdjsonpy.loads('{"items": [1, 2, 3], "ok": true}')
data["items"]   # [1, 2, 3]
data["ok"]      # True
```

`loads` accepts `str`, `bytes`, `bytearray`, `memoryview`, or any object exporting the buffer protocol. Output is the same Python types `json.loads` produces.

To swap an existing module wholesale:

```python
import simdjsonpy as json

config = json.loads(open("config.json", "rb").read())
```

`dumps` delegates to the stdlib (`simdjson` does not serialize), so the import shim still works for write-back.

## Parse a file from disk

For files, `simdjsonpy.load` (DOM) takes a path directly and avoids a round-trip through Python:

```python
doc = simdjsonpy.load("data/twitter.json")
doc["search_metadata"]["count"].get_uint64()
```

If you need native Python objects, read the file yourself:

```python
with open("data/twitter.json", "rb") as f:
    payload = f.read()

native = simdjsonpy.loads(payload)
```

## Pick the right API

```python
# Materialize everything (drop-in for json.loads)
data = simdjsonpy.loads(payload)

# Build a tape; query repeatedly without re-parsing
doc = simdjsonpy.parse(payload)
ids = [user["id"].get_uint64() for user in doc["users"].get_array()]

# Lazy cursor; only the fields you touch are parsed
doc = simdjsonpy.iterate(payload)
count = doc["search_metadata"]["count"].get_uint64()
```

Read [DOM](dom.md) and [On-Demand](ondemand.md) for the full surfaces.

## Streaming NDJSON

```python
ndjson = b'{"id":1}\n{"id":2}\n{"id":3}'
for doc in simdjsonpy.iterate_many(ndjson):
    print(doc["id"].get_uint64())
```

See [Streaming](streaming.md) for batch sizing and the rules around stale stream items.

## Errors you should expect

```python
import simdjsonpy

try:
    simdjsonpy.loads(b'{"oops"')           # malformed
except simdjsonpy.SimdjsonError as exc:
    print(exc)

doc = simdjsonpy.parse('{"a": 1}')
try:
    doc["missing"].get_int64()             # not present
except KeyError:
    pass
```

See [Error handling](error-handling.md).
