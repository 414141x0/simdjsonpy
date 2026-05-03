# Quickstart

```python
import simdjsonpy as simdjson

json_bytes = simdjson.PaddedString.load("ext/simdjson/jsonexamples/twitter.json")
tweets = simdjson.iterate(json_bytes)
count = tweets["search_metadata"]["count"].get_uint64()

print(f"{count} results.")
```

## DOM Variant

If you want a stable document object that you can keep and revisit freely, use the DOM parser:

```python
import simdjsonpy as simdjson

json_bytes = simdjson.PaddedString.load("ext/simdjson/jsonexamples/twitter.json")
tweets = simdjson.parse(json_bytes)
count = tweets["search_metadata"]["count"].get_uint64()

print(f"{count} results.")
```
## Note:
### Why `PaddedString.load()`

`simdjson` expects padded input for its fastest paths. `PaddedString.load()` gives the binding an owned, already padded buffer so the native layer does not need to create a throwaway copy first.

### When To Prefer Each Surface

- Prefer `iterate()` for query-style access where you only need a small portion of a large payload.
- Prefer `parse()` when you want stable references across time, repeated object lookups, or multiple independent traversals.
