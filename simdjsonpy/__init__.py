from __future__ import annotations

"""Python bindings for simdjson using nanobind."""

from pkgutil import extend_path

__path__ = extend_path(__path__, __name__)

from ._core import (
    DEFAULT_MAX_DEPTH,
    DOMArray,
    DOMDocument,
    DOMDocumentStream,
    DOMElement,
    DOMElementType,
    DOMField,
    DOMObject,
    DOMParser,
    ErrorCode,
    Implementation,
    OnDemandArray,
    OnDemandDocument,
    OnDemandDocumentReference,
    OnDemandDocumentStream,
    OnDemandField,
    OnDemandJSONType,
    OnDemandNumber,
    OnDemandNumberType,
    OnDemandObject,
    OnDemandParser,
    OnDemandRawJSONString,
    OnDemandValue,
    PaddedString,
    PaddedStringBuilder,
    SIMDJSON_MAXSIZE_BYTES,
    SIMDJSON_PADDING,
    SimdjsonError,
    active_implementation,
    available_implementations,
    builtin_implementation,
    error_message,
    error_name,
    is_fatal,
    is_valid,
    minify,
    set_active_implementation,
    validate_utf8,
)
from . import dom, ondemand

JSONInput = dom.JSONInput


def parse(
    data: JSONInput,
    *,
    max_capacity: int = SIMDJSON_MAXSIZE_BYTES,
) -> DOMDocument:
    """Parse a single JSON document into an owned DOM document."""
    return dom.parse(data, max_capacity=max_capacity)


def load(
    path: str,
    *,
    max_capacity: int = SIMDJSON_MAXSIZE_BYTES,
) -> DOMDocument:
    """Load and parse a JSON document from a file path."""
    return dom.load(path, max_capacity=max_capacity)


def iterate(
    data: JSONInput,
    *,
    max_capacity: int = SIMDJSON_MAXSIZE_BYTES,
) -> OnDemandDocument:
    """Parse a single JSON document using the On-Demand API."""
    return ondemand.iterate(data, max_capacity=max_capacity)


def iterate_many(
    data: JSONInput,
    *,
    max_capacity: int = SIMDJSON_MAXSIZE_BYTES,
    batch_size: int = ondemand.DEFAULT_BATCH_SIZE,
    allow_comma_separated: bool = False,
) -> OnDemandDocumentStream:
    """Iterate a stream of JSON documents with the On-Demand API."""
    return ondemand.iterate_many(
        data,
        max_capacity=max_capacity,
        batch_size=batch_size,
        allow_comma_separated=allow_comma_separated,
    )


__all__ = [
    "DEFAULT_MAX_DEPTH",
    "DOMArray",
    "DOMDocument",
    "DOMDocumentStream",
    "DOMElement",
    "DOMElementType",
    "DOMField",
    "DOMObject",
    "DOMParser",
    "ErrorCode",
    "Implementation",
    "JSONInput",
    "OnDemandArray",
    "OnDemandDocument",
    "OnDemandDocumentReference",
    "OnDemandDocumentStream",
    "OnDemandField",
    "OnDemandJSONType",
    "OnDemandNumber",
    "OnDemandNumberType",
    "OnDemandObject",
    "OnDemandParser",
    "OnDemandRawJSONString",
    "OnDemandValue",
    "PaddedString",
    "PaddedStringBuilder",
    "SIMDJSON_MAXSIZE_BYTES",
    "SIMDJSON_PADDING",
    "SimdjsonError",
    "active_implementation",
    "available_implementations",
    "builtin_implementation",
    "dom",
    "error_message",
    "error_name",
    "is_fatal",
    "is_valid",
    "iterate",
    "iterate_many",
    "load",
    "minify",
    "ondemand",
    "parse",
    "set_active_implementation",
    "validate_utf8",
]
