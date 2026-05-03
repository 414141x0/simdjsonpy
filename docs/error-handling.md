# Error handling

simdjsonpy translates simdjson's error codes into the Python exception type that best matches the failure. 

Catch a built-in exception when you can; fall back to `SimdjsonError` for everything else.

## Exception hierarchy

```
RuntimeError
└── SimdjsonError       # everything not covered by a more specific built-in
```

Plus the standard library exceptions raised for specific simdjson errors:

| simdjson error | Python exception | When |
| --- | --- | --- |
| `INCORRECT_TYPE` | `TypeError` | Calling `get_int64()` on a string, etc. |
| `NO_SUCH_FIELD` | `KeyError` | Object does not contain the requested key. |
| `INDEX_OUT_OF_BOUNDS` | `IndexError` | Array index past the end. |
| `NUMBER_OUT_OF_RANGE` | `OverflowError` | Number does not fit the requested integer width. |
| `UTF8_ERROR` | `UnicodeError` | Invalid UTF-8 in a string or in the input. |
| `INVALID_JSON_POINTER` | `ValueError` | Malformed JSON Pointer. |
| `INVALID_URI_FRAGMENT` | `ValueError` | Malformed URI fragment in a pointer. |
| Any other code | `SimdjsonError` | TAPE_ERROR, CAPACITY, MEMALLOC, etc. |

A stale stream item (use after iterator advance) raises `ReferenceError`. That is the only case where we use `ReferenceError`.

## `SimdjsonError`

`SimdjsonError` is a subclass of `RuntimeError`. The instance carries the simdjson error code so you can switch on it.

```python
import simdjsonpy

try:
    simdjsonpy.loads(b'{"oops"')
except simdjsonpy.SimdjsonError as exc:
    print(exc.code)             # ErrorCode.TAPE_ERROR
    print(exc.args[0])          # 'TAPE_ERROR: Something went wrong... (loads)'
```

The message format is `<NAME>: <description> (<context>)`.

## `ErrorCode`

Every code returned by the parser is exposed as an enum member.

```python
simdjsonpy.ErrorCode.SUCCESS
simdjsonpy.ErrorCode.CAPACITY
simdjsonpy.ErrorCode.TAPE_ERROR
simdjsonpy.ErrorCode.NO_SUCH_FIELD
# ...

simdjsonpy.error_message(simdjsonpy.ErrorCode.MEMALLOC)
# 'Error allocating memory, we're most likely out of memory'

simdjsonpy.error_name(simdjsonpy.ErrorCode.MEMALLOC)
# 'MEMALLOC'

simdjsonpy.is_fatal(simdjsonpy.ErrorCode.MEMALLOC)
# True — the parser should not be reused after a fatal error
```

`is_fatal` is true for codes that indicate the parser's internal state is unrecoverable. 
Replace the parser instance when you see one.

## Patterns

### Validate before processing

```python
def parse_user_input(payload: bytes):
    try:
        return simdjsonpy.loads(payload)
    except simdjsonpy.SimdjsonError as exc:
        raise ValueError(f"invalid JSON: {exc}") from exc
    except UnicodeError as exc:
        raise ValueError("invalid UTF-8") from exc
```

### Safe key access

```python
doc = simdjsonpy.parse(payload).get_object()

# Built-in get() with default
name = doc.get("name", default=None)
if name is not None:
    print(name.get_string())

# Or catch
try:
    print(doc["name"].get_string())
except KeyError:
    pass
```

`DOMObject.get` returns the default for missing keys without raising. 
The On-Demand object surface is forward-only and does not have `get` — use `find_field_unordered` and catch `KeyError`:

```python
od_obj = simdjsonpy.iterate(payload).get_object()

try:
    od_obj.find_field_unordered("name").get_string()
except KeyError:
    pass
```

### Inspecting the original code

```python
try:
    doc = simdjsonpy.DOMParser(max_capacity=1024).parse(huge_payload)
except simdjsonpy.SimdjsonError as exc:
    if exc.code is simdjsonpy.ErrorCode.CAPACITY:
        # The document exceeded our capacity cap. Either reject it or
        # raise the cap and retry.
        ...
    else:
        raise
```

### Range-checked numeric reads

```python
v = simdjsonpy.iterate(b'{"x": 1234567890123456789}')["x"]

try:
    v.get_int32()
except OverflowError:
    v.get_int64()
```

### Distinguishing missing key from wrong type

```python
try:
    age = doc["age"].get_int64()
except KeyError:
    age = None             # field absent
except TypeError:
    age = None             # field present but not an integer
```

## Fatal vs recoverable errors

Most errors leave the parser usable for the next call. A few do not:

- `MEMALLOC` — out of memory
- `UNSUPPORTED_ARCHITECTURE` — runtime CPU lacks required SIMD features
- Anything for which `simdjsonpy.is_fatal(code)` returns `True`

After a fatal error, replace the parser instance. 
simdjsonpy does not silently recreate it for you because that would mask the cause.

## Stream lifetime errors

```python
stream = simdjsonpy.iterate_many(payload)
it = iter(stream)
first = next(it)
second = next(it)

first["x"].get_uint64()
# ReferenceError: On-Demand stream item is no longer valid because the
# underlying parser or stream has advanced
```

This is intentional and unavoidable — simdjson's streaming API reuses the parser's internal state across documents. 
If you need to retain documents, materialize them into Python objects (`simdjsonpy.loads` or fully traversing into a `dict`) before advancing.
