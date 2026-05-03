from __future__ import annotations

"""Python DOM wrappers over the simdjson DOM bindings."""

from typing import TypeAlias

from ._core import (
    DEFAULT_MAX_DEPTH,
    DOMArray as Array,
    DOMDocument as Document,
    DOMDocumentStream as DocumentStream,
    DOMElement as Element,
    DOMElementType as ElementType,
    DOMField as Field,
    DOMObject as Object,
    DOMParser as Parser,
    PaddedString,
    SIMDJSON_MAXSIZE_BYTES,
)

BufferInput: TypeAlias = bytes | bytearray | memoryview
JSONInput: TypeAlias = str | BufferInput | PaddedString


def parse(
    data: JSONInput,
    *,
    max_capacity: int = SIMDJSON_MAXSIZE_BYTES,
) -> Document:
    """Parse a single JSON document into an owned DOM document."""
    return Parser(max_capacity=max_capacity).parse(data)


def load(
    path: str,
    *,
    max_capacity: int = SIMDJSON_MAXSIZE_BYTES,
) -> Document:
    """Load and parse a single JSON document from disk."""
    return Parser(max_capacity=max_capacity).load(path)


def parse_many(
    data: JSONInput,
    *,
    max_capacity: int = SIMDJSON_MAXSIZE_BYTES,
    batch_size: int = 1_000_000,
) -> DocumentStream:
    """Stream multiple JSON documents from one buffer."""
    return Parser(max_capacity=max_capacity).parse_many(data, batch_size=batch_size)


def load_many(
    path: str,
    *,
    max_capacity: int = SIMDJSON_MAXSIZE_BYTES,
    batch_size: int = 1_000_000,
) -> DocumentStream:
    """Stream multiple JSON documents from a file."""
    return Parser(max_capacity=max_capacity).load_many(path, batch_size=batch_size)


__all__ = [
    "Array",
    "BufferInput",
    "DEFAULT_MAX_DEPTH",
    "Document",
    "DocumentStream",
    "Element",
    "ElementType",
    "Field",
    "JSONInput",
    "Object",
    "Parser",
    "PaddedString",
    "SIMDJSON_MAXSIZE_BYTES",
    "load",
    "load_many",
    "parse",
    "parse_many",
]
