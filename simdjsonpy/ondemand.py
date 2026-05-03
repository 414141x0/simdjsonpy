from __future__ import annotations

"""Python wrappers over simdjson's On-Demand API."""

from typing import TypeAlias

from ._core import (
    DEFAULT_MAX_DEPTH,
    OnDemandArray as Array,
    OnDemandDocument as Document,
    OnDemandDocumentReference as DocumentReference,
    OnDemandDocumentStream as DocumentStream,
    OnDemandField as Field,
    OnDemandJSONType as JSONType,
    OnDemandNumber as Number,
    OnDemandNumberType as NumberType,
    OnDemandObject as Object,
    OnDemandParser as Parser,
    OnDemandRawJSONString as RawJSONString,
    OnDemandValue as Value,
    PaddedString,
    SIMDJSON_MAXSIZE_BYTES,
)

DEFAULT_BATCH_SIZE = 1_000_000

BufferInput: TypeAlias = bytes | bytearray | memoryview
JSONInput: TypeAlias = str | BufferInput | PaddedString


def iterate(
    data: JSONInput,
    *,
    max_capacity: int = SIMDJSON_MAXSIZE_BYTES,
) -> Document:
    """Parse a single JSON document with the On-Demand API."""
    return Parser(max_capacity=max_capacity).iterate(data)


def iterate_many(
    data: JSONInput,
    *,
    max_capacity: int = SIMDJSON_MAXSIZE_BYTES,
    batch_size: int = DEFAULT_BATCH_SIZE,
    allow_comma_separated: bool = False,
) -> DocumentStream:
    """Stream multiple JSON documents with the On-Demand API."""
    return Parser(max_capacity=max_capacity).iterate_many(
        data,
        batch_size=batch_size,
        allow_comma_separated=allow_comma_separated,
    )


__all__ = [
    "Array",
    "BufferInput",
    "DEFAULT_BATCH_SIZE",
    "DEFAULT_MAX_DEPTH",
    "Document",
    "DocumentReference",
    "DocumentStream",
    "Field",
    "JSONInput",
    "JSONType",
    "Number",
    "NumberType",
    "Object",
    "Parser",
    "PaddedString",
    "RawJSONString",
    "SIMDJSON_MAXSIZE_BYTES",
    "Value",
    "iterate",
    "iterate_many",
]
