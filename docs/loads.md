# `loads` and `dumps`

The smallest API surface — and the one most users will pick.
`loads` parses a JSON payload and returns native Python objects.
`dumps` is a thin pass-through to `json.dumps`.

## `simdjsonpy.loads(data)`

| Parameter | Type | Description |
| --- | --- | --- |
| `data` | `str` \| `bytes` \| `bytearray` \| `memoryview` \| any buffer-protocol object \| `PaddedString` | JSON to decode. |

Returns a Python object — `dict`, `list`, `str`, `int`, `float`, `bool`, or `None` — matching what `json.loads` would produce on the same input.

```python
import simdjsonpy

simdjsonpy.loads('null')           # None
simdjsonpy.loads('true')           # True
simdjsonpy.loads('42')             # 42
simdjsonpy.loads('3.14')           # 3.14
simdjsonpy.loads('"héllo"')        # 'héllo'
simdjsonpy.loads('[1, 2, 3]')      # [1, 2, 3]
simdjsonpy.loads('{"a": 1}')       # {'a': 1}
```

### Inputs

```python
simdjsonpy.loads('{"a":1}')                       # str
simdjsonpy.loads(b'{"a":1}')                      # bytes
simdjsonpy.loads(bytearray(b'{"a":1}'))           # bytearray
simdjsonpy.loads(memoryview(b'{"a":1}'))          # memoryview

# Pre-padded buffer parses faster on hot paths
ps = simdjsonpy.PaddedString(b'{"a":1}')
simdjsonpy.loads(ps)                              # {'a': 1}
```

For mutable buffer types (`bytearray`, `memoryview` over a writable object), `loads` exports the buffer protocol with a read-only lock for the duration of the call so other threads cannot resize the buffer underneath the parser.

### Numeric ranges

```python
simdjsonpy.loads('9223372036854775807')           # int64 max
simdjsonpy.loads('18446744073709551615')          # uint64 max
simdjsonpy.loads('99999999999999999999999999')    # arbitrary precision int
simdjsonpy.loads('1e308')                         # 1e308
```

Integers that exceed `uint64` are returned as Python ints via simdjson's `BIGINT` path. There is no precision loss.

### Unicode

```python
simdjsonpy.loads(r'"é"')                          # 'é'
simdjsonpy.loads(b'"\xc3\xa9"')                   # 'é'  (UTF-8)
```

Strings are validated as UTF-8 during parsing — invalid byte sequences raise `UnicodeError`.

### Performance notes

- `loads` keeps a `thread_local` DOM parser, so repeated calls on the same thread reuse the tape buffer with no allocation churn.
- Concurrent calls from different threads never contend (each thread has its own parser).
- The dominant cost is constructing Python objects (`PyDict_New`, `PyUnicode_FromStringAndSize`,), not parsing. For workloads where you only need a few fields out of a large document, [On-Demand](ondemand.md) is much faster.

### Errors

| Cause | Exception |
| --- | --- |
| Malformed JSON | `SimdjsonError` |
| Invalid UTF-8 | `UnicodeError` |
| Document larger than `SIMDJSON_MAXSIZE_BYTES` | `SimdjsonError` (CAPACITY) |
| Wrong input type (e.g. `int`) | `TypeError` |

```python
try:
    simdjsonpy.loads(b'{"oops"')
except simdjsonpy.SimdjsonError as exc:
    print(exc.args[0])    # 'TAPE_ERROR: ...'
```

## `simdjsonpy.dumps(obj, /, **kwargs)`

```python
simdjsonpy.dumps({"a": 1, "b": [2, 3]})
# '{"a": 1, "b": [2, 3]}'
```

`dumps` forwards to `json.dumps`. 
simdjson is just a parser, thus the function exists so `import simdjsonpy as json` is a complete drop-in:

```python
import simdjsonpy as json

payload = json.dumps({"hello": "world"}, indent=2)
data = json.loads(payload)
```

All `json.dumps` keyword arguments (`indent`, `sort_keys`, `ensure_ascii`, `default`,) work as usual.

## Comparison with `json`

`loads` is roughly 2× the throughput of `json.loads` for full materialization. It does **not** support the parsing hooks (`object_hook`, `parse_float`, `parse_int`, `object_pairs_hook`, `parse_constant`, `cls`) — those would require calling Python during the C++ parse and erase the performance win. If you need them, use `json.loads`.

For larger gains, use the [DOM](dom.md) or [On-Demand](ondemand.md) APIs, which never materialize Python `dict`/`list` objects.
