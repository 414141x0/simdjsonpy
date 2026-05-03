# DOM API

The DOM parser builds simdjson's tape representation of the entire document up front, then hands you wrapper objects you can traverse freely. 
Use it when you need random access, repeated traversal, or JSON Pointer / JSONPath queries.

## Parsing

```python
import simdjsonpy

# Module-level convenience — creates a parser, parses, returns the document.
doc = simdjsonpy.parse('{"name": "Ada", "scores": [98, 95, 100]}')

# Or via the parser class for reuse.
parser = simdjsonpy.DOMParser()
doc1 = parser.parse(b'{"id": 1}')
doc2 = parser.parse(b'{"id": 2}')   # parser is reusable
```

Reusing a parser does **not** invalidate older documents — each call returns an owned `DOMDocument` with its own tape memory.

### From a file path

```python
doc = simdjsonpy.load("data/twitter.json")
# Equivalent to:
# doc = simdjsonpy.DOMParser().load("data/twitter.json")
```

`load(path)` uses simdjson's native file loader, which is faster than reading the file in Python and passing the bytes through.

### Reusing the document object

```python
parser = simdjsonpy.DOMParser()
doc = simdjsonpy.DOMDocument()

parser.parse_into(doc, b'{"v": 1}')
doc["v"].get_int64()           # 1

parser.parse_into(doc, b'{"v": 2}')
doc["v"].get_int64()           # 2
```

`parse_into` reuses the document's tape buffer.
References handed out by the previous parse are no longer valid — the tape was overwritten.

If you need to keep older documents around, use `parse` instead.

## `DOMDocument`

```python
doc = simdjsonpy.parse('{"x": 1, "y": [2, 3]}')

doc["x"].get_int64()           # 1            (subscript on root)
doc.root                       # DOMElement   (explicit root)
doc.capacity                   # tape capacity in bytes
str(doc)                       # '{"x":1,"y":[2,3]}'   (minified)
doc.prettify()                 # pretty-printed form
```

The document has the same lookup helpers as a root element: `__getitem__`, `at_pointer`, `at_path`, `at_key`, `at_key_case_insensitive`, `get_array`, `get_object`, plus typed scalar getters.

## `DOMElement`

The fundamental wrapper. Every lookup eventually returns one.

### Type discrimination

```python
e = simdjsonpy.parse('{"v": 1}')["v"]

e.is_int64()           # True
e.is_uint64()          # False
e.is_double()          # False
e.is_number()          # True   (any numeric)
e.is_string()          # False
e.is_array()           # False
e.is_object()          # False
e.is_bool()            # False
e.is_null()            # False
e.is_bigint()          # False  (only for integers > uint64)
e.type                 # DOMElementType.INT64
```

`DOMElementType` enum values: `ARRAY`, `OBJECT`, `INT64`, `UINT64`, `DOUBLE`, `STRING`, `BOOL`, `NULL_VALUE`, `BIGINT`.

### Typed conversion

```python
e.get_int64()
e.get_uint64()
e.get_double()
e.get_string()
e.get_string_length()       # length without copying the string
e.get_bool()
e.get_array()               # returns DOMArray
e.get_object()              # returns DOMObject
e.get_bigint()              # returns the digit string
```

A typed getter raises `TypeError` if the element is the wrong type, and `OverflowError` if the value does not fit.

### Subscript

```python
arr = simdjsonpy.parse('[10, 20, 30]')
arr[0].get_int64()                          # 10
arr[-1]                                     # IndexError — negative indexes are not supported

obj = simdjsonpy.parse('{"a": 1, "b": 2}')
obj["a"].get_int64()                        # 1
obj["missing"]                              # KeyError
```

### JSON Pointer (RFC 6901)

```python
doc = simdjsonpy.parse('{"users": [{"id": 1, "name": "Ada"}, {"id": 2}]}')
doc.at_pointer("/users/0/name").get_string()                                # 'Ada'
doc.at_pointer("/users/1/id").get_uint64()                                  # 2
```

### JSONPath subset

```python
doc.at_path("$.users[0].name").get_string()                             # 'Ada'

# Wildcard returns a list of matching elements
[e.get_uint64() for e in doc.at_path_with_wildcard("$.users[*].id")]
# [1, 2]
```

### Key lookups

```python
obj = simdjsonpy.parse('{"Name": "Ada"}').get_object()

obj.at_key("Name").get_string()                          # 'Ada'
obj.at_key_case_insensitive("name").get_string()         # 'Ada'
"Name" in obj                                            # True
```

## `DOMArray`

```python
arr = simdjsonpy.parse('[10, 20, 30]').get_array()

len(arr)                                            # 3
arr[1].get_int64()                                  # 20
arr.at(2).get_int64()                               # 30   (same as subscript)
list(arr)                                           # [DOMElement, DOMElement, DOMElement]
arr.to_json()                                       # '[10,20,30]'
```

Arrays are iterable and follow `collections.abc.Sequence`.

## `DOMObject`

```python
obj = simdjsonpy.parse('{"a": 1, "b": 2}').get_object()

len(obj)                          # 2
obj["a"].get_int64()              # 1
obj.get("missing", default=None)  # None — does not raise
"a" in obj                        # True
list(obj)                         # ['a', 'b']               (keys)
list(obj.fields())                # [DOMField, DOMField]     (key + element)
dict((f.key, f.value.to_json()) for f in obj.fields())
```

Iteration order matches the document order.
Lookups are linear in the number of keys (as building a hash table would defeat simdjson's tape design).

## DOM streaming

```python
parser = simdjsonpy.DOMParser()
stream = parser.parse_many(b'{"a":1}\n{"a":2}\n{"a":3}')
for doc in stream:
    print(doc["a"].get_int64())
```

DOM stream items follow forward-only semantics i.e once you advance to the next document, references into earlier ones raise `ReferenceError`. 

See [Streaming](streaming.md) for the full rules.

## Padded inputs

```python
ps = simdjsonpy.PaddedString.load("data/twitter.json")
doc = simdjsonpy.parse(ps)            # zero-copy hot path
```

A `PaddedString` is an owned UTF-8 buffer with `simdjson`'s required tail padding. 
Passing one to `parse` skips the internal copy that other input types require. 

See [Common helpers](common.md).

## Capacity controls

```python
parser = simdjsonpy.DOMParser(max_capacity=64 * 1024 * 1024)
parser.allocate(capacity=1_000_000, max_depth=512)

parser.capacity              # currently allocated
parser.max_capacity          # configured upper bound
parser.max_depth             # configured nesting limit
parser.set_max_capacity(...) # change cap at runtime
```

Any document larger than `max_capacity` raises `SimdjsonError` with code `CAPACITY`.

## Thread safety

`DOMParser` and `DOMDocument` each hold a per-instance lock. 

Sharing them across threads is safe; concurrent calls serialize on the lock.

For real concurrency, give each thread its own parser, or use `simdjsonpy.loads` (which keeps a thread-local parser internally). 

See [Threading](threading.md).
