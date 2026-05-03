# On-Demand API

The On-Demand parser walks the document as a forward-only cursor. It does not build a tape; it only validates and extracts the bytes you actually touch. 

This is the fastest path when you need a few fields out of a large payload.

## Parsing

```python
import simdjsonpy

doc = simdjsonpy.iterate('{"name": "Ada", "scores": [98, 100]}')

# Or via the parser class for reuse
parser = simdjsonpy.OnDemandParser()
doc = parser.iterate(b'{"name": "Ada", "scores": [98, 100]}')
```

`iterate` keeps a reference to the input buffer for the lifetime of the document; strings and offsets it returns point into that buffer.

## Convenience access

The wrappers expose Python-friendly helpers that handle reset/rewind for you. 

```python
doc = simdjsonpy.iterate('{"a": 1, "b": [10, 20, 30]}')

doc["a"].get_uint64()              # 1
doc["b"]                           # OnDemandValue
doc["b"].get_array()[0].get_uint64()  # 10
len(doc["b"].get_array())          # 3

list(iter(doc["b"].get_array()))   # full materialization of "b"
```

Because the convenience helpers rewind under the hood, you can requery without thinking about cursor state. 
Use the lower-level methods (`find_field`, `at`) when you want to commit to forward-only access for maximum speed.

## `OnDemandDocument`

```python
doc = simdjsonpy.iterate('{"flag": true, "items": [1, 2]}')

doc.type                    # OnDemandJSONType.OBJECT
doc.is_scalar()             # False
doc.get_object()            # OnDemandObject
doc.get_value()             # OnDemandValue at the root

doc.find_field("flag").get_bool()              # True   (ordered)
doc.find_field_unordered("flag").get_bool()    # True   (any order)
doc.at("items")             # raises — root is an object, not an array

doc.rewind()                # reset the cursor to the start
doc.is_alive()              # True until the cursor walks past the end
doc.at_end()                # whether the cursor has consumed the document
doc.current_depth()         # nesting depth at the cursor
doc.current_location()      # byte offset into the input
```

`find_field` requires the requested key to come at or after the cursor's current position; `find_field_unordered` resets the cursor to the start of the object and searches everywhere. 
Use `find_field` when you know your access pattern matches the document order — it is strictly faster.

## `OnDemandValue`

A polymorphic node. Methods that depend on type raise `TypeError` if the wrong one is called.

```python
v = simdjsonpy.iterate('{"v": 42}')["v"]

v.type                         # OnDemandJSONType.NUMBER
v.is_string()                  # False
v.is_integer()                 # True
v.is_negative()                # False
v.get_number_type()            # OnDemandNumberType.UNSIGNED_INTEGER
v.get_uint64()                 # 42

v.raw_json_token()             # '42 '   (raw bytes including trailing whitespace)
v.raw_json()                   # '42'    (just the value)
```

### Numeric getters

```python
v.get_int64()
v.get_uint64()
v.get_int32()
v.get_uint32()
v.get_double()

# Same getters but the number is encoded inside a JSON string
v.get_int64_in_string()
v.get_uint64_in_string()
v.get_double_in_string()

# Generic number — call once, branch on the type
n = v.get_number()
if n.is_double():
    n.get_double()
elif n.is_int64():
    n.get_int64()
else:
    n.get_uint64()
n.as_double()                  # always returns a double
```

### String getters

```python
v.get_string()                          # strict UTF-8
v.get_string(allow_replacement=True)    # replace invalid UTF-8 with U+FFFD
v.get_wobbly_string()                   # accept lone surrogates (WTF-8)

raw = v.get_raw_json_string()           # OnDemandRawJSONString
raw.raw()                               # bytes between the JSON quotes
raw.unescape()                          # apply JSON escape rules
raw.unescape_wobbly()                   # WTF-8 unescape
raw.is_equal('expected')                # fast string comparison
```

`OnDemandRawJSONString` is useful for fast equality checks: comparing the raw JSON bytes is faster than fully unescaping when you only need to know whether two strings match.

## `OnDemandArray`

```python
arr = simdjsonpy.iterate('[10, 20, 30]').get_array()

len(arr)                            # 3       (counts elements; rewinds the cursor)
arr.count_elements()                # 3
arr.is_empty()                      # False
arr[1].get_uint64()                 # 20
arr.at(2).get_uint64()              # 30
list(v.get_uint64() for v in arr)   # [10, 20, 30]
arr.raw_json()                      # '[10, 20, 30]'
```

Subscripting and `__len__` reset the cursor before each call, so you can iterate the same array multiple times. Plain `for v in arr` iterates from the current cursor position — it is forward-only and will not rewind by itself.

## `OnDemandObject`

```python
obj = simdjsonpy.iterate('{"a": 1, "b": 2}').get_object()

len(obj)                          # 2
obj["a"].get_uint64()             # 1
obj.find_field("a").get_uint64()  # 1   (ordered, faster)
obj.find_field_unordered("b").get_uint64()
"a" in obj                        # True
list(obj)                         # ['a', 'b']
list(f.unescaped_key() for f in obj.fields())
obj.is_empty()                    # False
obj.count_fields()                # 2
obj.raw_json()                    # '{"a": 1, "b": 2}'
```

## `OnDemandField`

Yielded by `OnDemandObject.fields()`.

```python
for f in obj.fields():
    f.unescaped_key()            # 'a'
    f.escaped_key()              # 'a'
    f.key_raw_json_token()       # '"a"'
    f.key().raw()                # 'a'         (OnDemandRawJSONString)
    f.value.get_uint64()         # 1
```

## Streams

```python
parser = simdjsonpy.OnDemandParser()
for doc in parser.iterate_many(b'{"a":1}\n{"a":2}\n{"a":3}'):
    print(doc["a"].get_uint64())
```

Stream documents become invalid the moment you advance the iterator.
Holding onto an earlier reference and reading it raises `ReferenceError`. 

See [Streaming](streaming.md).

## Configuration

```python
parser = simdjsonpy.OnDemandParser(max_capacity=64 * 1024 * 1024)
parser.allocate(capacity=1_000_000, max_depth=512)
parser.set_max_capacity(128 * 1024 * 1024)
parser.threaded = True             # enable simdjson's internal pipelining
parser.capacity                    # currently allocated
parser.max_capacity                # configured cap
parser.max_depth                   # configured nesting limit
```

`threaded` controls whether simdjson uses background threads for stage-1 parsing. The default matches simdjson's compile-time setting.

## Lifetime rules

1. The document holds the input buffer alive. Strings and `raw_json` slices borrow into it.
2. Conversion to a Python `str` (e.g. `get_string`) copies — those values outlive the document.
3. After `parser.iterate_many(...)`, advancing the iterator invalidates earlier items. Use `next(...)` and consume each item before advancing.
4. `OnDemandValue` returned from a previous lookup may be invalid after rewinding the document or its parent. Convenience helpers like `doc["a"]` do this rewinding automatically — be careful when mixing them with hand-written cursor code.

## When to use this over DOM

| Question | Use On-Demand | Use DOM |
| --- | --- | --- |
| Few fields out of a large doc? | yes | no |
| Many random-access reads of the same doc? | no | yes |
| Need JSON Pointer / wildcard JSONPath? | partial (point lookups only) | full |
| Need stable child references across re-rooting calls? | no | yes |
| Need to materialize as Python objects? | use `loads` instead | use `loads` instead |
