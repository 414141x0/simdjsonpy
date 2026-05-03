# Threading

simdjsonpy is built for free-threaded Python 3.14 (`--disable-gil`). 
Every parser, document, and stream guards its mutable state with a per instance lock so that misuse degrades to serialization rather than data corruption.

## What is safe to share

| Object | Shared across threads | Notes |
| --- | --- | --- |
| `simdjsonpy.loads` | yes — fully concurrent | Uses a `thread_local` parser internally; no contention. |
| `DOMParser` | yes | Concurrent calls serialize on the parser's mutex. |
| `OnDemandParser` | yes | Same — internal lock. |
| `DOMDocument` | yes for reads | Reading is locked; do not call `parse_into` concurrently with reads. |
| `OnDemandDocument` | no | Cursor state is not safe to share. Give each thread its own. |
| `DOMDocumentStream`, `OnDemandDocumentStream` | no | Forward-only iterators. One consumer per stream. |
| `PaddedString` | yes for reads | The buffer is immutable once built. `append` mutates and is not synchronized. |

If you need true parallel parsing on a hot path, use `simdjsonpy.loads` (which never contends) or give each thread its own parser instance.

## Concurrent `loads`

```python
from concurrent.futures import ThreadPoolExecutor
import simdjsonpy

payloads = [b'{"id": %d}' % i for i in range(10_000)]

with ThreadPoolExecutor(max_workers=8) as pool:
    results = list(pool.map(simdjsonpy.loads, payloads))
```

Each worker thread keeps its own DOM parser. There is zero coordination overhead.

## Sharing a parser

```python
parser = simdjsonpy.DOMParser()

def worker(payload):
    return parser.parse(payload)["id"].get_int64()

with ThreadPoolExecutor(max_workers=8) as pool:
    results = list(pool.map(worker, payloads))
```

This works correctly but parses are serialized as only one thread is inside the parser at a time. 
Use this pattern only if the parsers are too expensive to instantiate per-thread (rarely).

## DOM parser per thread

For real concurrency in a worker pool, keep a parser per thread:

```python
import threading
import simdjsonpy

_local = threading.local()

def parse(payload):
    parser = getattr(_local, "parser", None)
    if parser is None:
        parser = simdjsonpy.DOMParser()
        _local.parser = parser
    return parser.parse(payload)
```

This is essentially what `simdjsonpy.loads` does internally with C++ `thread_local` storage.

## Stream consumption

```python
stream = simdjsonpy.iterate_many(payload)

# Hand a single iterator to a single thread.
for doc in stream:
    process(doc)
```

Splitting the same iterator across threads is unsafe. If you want parallel processing of NDJSON, partition the input bytes first (e.g. on newline boundaries) and parse each partition independently.

## Avoiding lock contention

The locks are fine-grained (per object) and only held for the duration of the C++ call. They become a bottleneck only when a single object is hammered from many threads. 

In order of preference:
1. Use `simdjsonpy.loads` — it is contention-free.
2. Create one parser per thread.
3. As a last resort, share a parser and accept the serialization.

## Free-threaded vs GIL builds

The bindings are compiled with nanobind's `FREE_THREADED` option. They work on both free-threaded (`python3.14t`) and standard GIL-bearing interpreters; on a GIL build, the per-instance locks are noops because the GIL provides the same guarantee.

## What about `OnDemandParser.threaded`?

That flag toggles simdjson's *internal* multi-threading for stage-1 parsing. It is unrelated to Python-side thread safety. It can be set independently per parser:

```python
parser = simdjsonpy.OnDemandParser()
parser.threaded = True
```

The default matches simdjson's compile-time configuration. Enabling it speeds up very large documents at the cost of some thread-pool overhead.
