# Common helpers

Utilities that sit underneath both the DOM and On-Demand surfaces. 

None of them are strictly required for everyday parsing, but they unlock the fastest paths and let you reach into simdjson for things that have no obvious place in the higher-level APIs.

## `PaddedString`

simdjson requires its input buffer to have at least `SIMDJSON_PADDING` bytes of trailing space (zero-filled is fine). 
The library copies your input into a padded buffer when you pass plain bytes. 
Skip the copy by giving it a `PaddedString` directly.

```python
import simdjsonpy

ps = simdjsonpy.PaddedString(b'{"a": 1}')
len(ps)                                             # 8     (size of the JSON, not the padding)
bytes(ps)                                           # b'{"a": 1}'
ps.append(b'  ')                                    # extends the buffer; returns False if no room
simdjsonpy.PaddedString.load("data/twitter.json")   # read + pad in one call

doc = simdjsonpy.parse(ps)                          # no copy
doc = simdjsonpy.iterate(ps)                        # no copy
doc = simdjsonpy.loads(ps)                          # no copy
```

Pre-padding pays off when you parse the same buffer many times, or when the buffer is large.

For one-shot small inputs the copy is negligible.

## `PaddedStringBuilder`

Build a `PaddedString` from chunks without repeated reallocations. 
Useful for assembling streamed input.

```python
builder = simdjsonpy.PaddedStringBuilder(capacity=4096)
builder.append(b'{"chunk":')
builder.append(b' "1"}')
builder.length                       # current length

ps = builder.build()                 # snapshot — builder remains usable
ps = builder.convert()               # move the buffer out — builder is empty after
```

`build` copies the contents into a new `PaddedString`. 
`convert` transfers ownership of the underlying buffer (cheaper) and resets the builder.

## Byte-buffer helpers

```python
simdjsonpy.validate_utf8(b'\xff')        # False
simdjsonpy.validate_utf8('héllo')        # True
simdjsonpy.is_valid('{"a": 1}')          # True
simdjsonpy.is_valid('{"a"')              # False
simdjsonpy.minify('{ "a": 1 }')          # '{"a":1}'
```

`is_valid` allocates its own DOM parser internally, so use it for one-off validation. 

If you are about to parse the document anyway, just call `loads` / `parse` and catch `SimdjsonError` as there is no point validating twice.

`minify` strips insignificant whitespace using simdjson's SIMD path. It does not pretty-print; pretty-printing is on `DOMElement.prettify()`.

## Constants

```python
simdjsonpy.SIMDJSON_PADDING          # padding bytes required by the parser
simdjsonpy.SIMDJSON_MAXSIZE_BYTES    # default max document size
simdjsonpy.DEFAULT_MAX_DEPTH         # default nesting limit
simdjsonpy.__version__               # binding version
```

`SIMDJSON_MAXSIZE_BYTES` defaults to 4 GiB.
Override per-parser via the `max_capacity` argument.

## Implementation switching

simdjson ships several SIMD backends (haswell, westmere, arm64, fallback) and picks the best one for the running CPU. 
You can introspect and override the active choice — typically only useful for benchmarking and debugging.

```python
simdjsonpy.active_implementation()
# Implementation(name=arm64, description=ARM NEON)

simdjsonpy.builtin_implementation()
# The compile-time default

[i.name for i in simdjsonpy.available_implementations()]
# ['arm64', 'fallback']

# Force a specific implementation (by name or by object)
simdjsonpy.set_active_implementation('fallback')
simdjsonpy.set_active_implementation(simdjsonpy.builtin_implementation())
```

Each `Implementation` exposes:

```python
impl = simdjsonpy.active_implementation()
impl.name                         # 'arm64'
impl.description                  # 'ARM NEON'
impl.supported_by_runtime_system  # True
impl.validate_utf8(b'hello')      # True
impl.minify(b'{"a": 1}')          # b'{"a":1}'
```

Switching implementations is a global mutation. 
Do it once at startup; do not toggle it per-request.

## Error introspection

```python
simdjsonpy.error_message(simdjsonpy.ErrorCode.NO_SUCH_FIELD)
# 'No such field'

simdjsonpy.error_name(simdjsonpy.ErrorCode.NO_SUCH_FIELD)
# 'NO_SUCH_FIELD'

simdjsonpy.is_fatal(simdjsonpy.ErrorCode.MEMALLOC)
# True   — fatal codes indicate the parser is unusable
```

`ErrorCode` is the full enum of simdjson error codes. 

See [Error handling](error-handling.md) for the mapping to Python exception types.
