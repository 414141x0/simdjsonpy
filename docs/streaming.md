# Streaming

simdjson can read a buffer that contains many concatenated JSON documents typically NDJSON (newline-delimited JSON) and yield them one at a time. 
Both the DOM and On-Demand surfaces expose this.

## NDJSON walkthrough

```python
import simdjsonpy

ndjson = b'\n'.join([
    b'{"id": 1, "tag": "a"}',
    b'{"id": 2, "tag": "b"}',
    b'{"id": 3, "tag": "c"}',
])

# DOM stream — each item is a full DOMElement
for doc in simdjsonpy.dom.parse_many(ndjson):
    print(doc["id"].get_uint64(), doc["tag"].get_string())

# On-Demand stream — each item is an OnDemandDocumentReference
for doc in simdjsonpy.iterate_many(ndjson):
    print(doc["id"].get_uint64(), doc["tag"].get_string())
```

Documents may be separated by **any whitespace**, not just newlines.

`{"a":1}{"a":2}` and `{"a":1}\n{"a":2}` both parse correctly.

## Batch size

```python
# Process 1 MiB at a time (default)
simdjsonpy.iterate_many(payload, batch_size=1_000_000)

# Smaller batches reduce peak memory but increase overhead
simdjsonpy.iterate_many(payload, batch_size=64 * 1024)
```

`batch_size` should be at least as large as the largest single document.
simdjson processes one batch at a time and yields completed documents from it; oversized documents that span batches raise `SimdjsonError` with code `INCOMPLETE_ARRAY_OR_OBJECT`.

## Comma-separated documents (On-Demand only)

```python
simdjsonpy.iterate_many(b'{"a":1},{"a":2}', allow_comma_separated=True)
```

This is required for some non-standard producers. 

By default, only whitespace-separated documents are accepted.

## Forward-only access

The crucial rule for streams: **once you advance, earlier items are invalid**.

```python
stream = simdjsonpy.iterate_many(b'{"a":1}\n{"a":2}')
it = iter(stream)

first = next(it)
first["a"].get_uint64()          # 1   — fine

second = next(it)                # advance the cursor
second["a"].get_uint64()         # 2   — fine

first["a"].get_uint64()          # ReferenceError
```

The same rule applies to DOM streams. This is enforced at runtime where every cached reference checks a generation counter before use. 

The error message is `"<API> stream item is no longer valid because the underlying parser or stream has advanced"`.

If you need to keep a document around, materialize it before advancing:

```python
results = []
for doc in simdjsonpy.iterate_many(payload):
    results.append({
        "id": doc["id"].get_uint64(),
        "tag": doc["tag"].get_string(),
    })
```

Or use `simdjsonpy.loads` per line:

```python
results = [simdjsonpy.loads(line) for line in payload.split(b'\n') if line]
```

(The streaming API is faster as it parses the whole buffer in one pass but the per line variant is sometimes easier to reason about.)

## Stream metadata

```python
stream = simdjsonpy.dom.parse_many(payload)
for _ in stream:
    pass

stream.size_in_bytes        # total bytes processed
stream.truncated_bytes      # bytes at the end that could not be parsed
                            # (a partial document at the buffer tail)
```

Both properties acquire the stream's mutex internally and are safe to read after iteration completes.

## Loading from disk

```python
parser = simdjsonpy.DOMParser()
stream = parser.load_many("data/big.ndjson", batch_size=4 * 1024 * 1024)
for doc in stream:
    ...
```

`load_many` mmaps the file when possible, which is faster than `read_bytes()` + `parse_many` for large files.

## Errors

| Cause | Exception |
| --- | --- |
| Single document larger than `batch_size` | `SimdjsonError` (INCOMPLETE_ARRAY_OR_OBJECT) |
| Malformed document inside the batch | `SimdjsonError` (raised on `next()` for that item) |
| Use of an item after the iterator advanced | `ReferenceError` |
| Unexpected non-whitespace separator | `SimdjsonError` (TAPE_ERROR) |

Truncated tails (a partial document at the end of the buffer) do not raise for they are reported via `stream.truncated_bytes`. Inspect it after iteration finishes if you care about whether the input ended cleanly.
