# simdjsonpy

`simdjsonpy` is a Python 3.14 binding for `simdjson` with two explicit surfaces:

- DOM for stable, reusable document views
- On-Demand for lower-overhead query-style access

The repository offers:

- clear lifetime semantics
- free-threaded-friendly locking around shared parser state
- fuzz and sanitizer guidance for the binding layer

## Package Shape

Top-level helpers:

```python
import simdjsonpy as simdjson

doc = simdjson.parse(b'{"name":"Ada"}')
fast = simdjson.iterate(b'{"ok":true}')
```

DOM module:

```python
from simdjsonpy import dom

document = dom.parse('{"users":[{"id":1},{"id":2}]}')
users = document["users"].get_array()
assert users[0]["id"].get_int64() == 1
```

On-Demand module:

```python
from simdjsonpy import ondemand

document = ondemand.iterate('{"flag":true,"items":[1,2,3]}')
assert document["flag"].get_bool() is True
assert len(document["items"].get_array()) == 3
```

## Lifetime Rules

### DOM

- `DOMParser.parse()` and `DOMParser.parse_into()` produce owned `DOMDocument` instances.
- Reusing a parser does not invalidate previously returned `DOMDocument` objects.
- `DOMDocumentStream` is forward-only.
- A streamed item stays valid until the next item is requested from the same iterator.
- Accessing an older streamed item after advancing raises `ReferenceError`.

### On-Demand

- `OnDemandParser.iterate()` creates an owned document state per parse.
- The underlying parser is constructed in its final stable location before parsing.
- Top-level conveniences such as `document["key"]` rewind the root document before lookup.
- Lower-level object and value methods still follow simdjson's forward-only semantics.
- `OnDemandDocumentStream` is forward-only.
- A streamed document reference stays valid until the next item is requested from the same iterator.
- Accessing an older streamed document after advancing raises `ReferenceError`.

## Fast Paths

- Use `PaddedString` when you already know the payload will be parsed more than once or you want an owned padded buffer.
- Use DOM when you need long-lived random access.
- Use On-Demand when your workload is query-oriented and you do not want to materialize more than you consume.
