from __future__ import annotations

import json
import unittest

from hypothesis import HealthCheck, given, settings
from hypothesis import strategies as st

import simdjsonpy as simdjson


def compact_json(value: object) -> str:
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"))


json_text = st.text(
    alphabet=st.characters(blacklist_categories=("Cs",)),
    max_size=40,
)
json_scalar = (
    st.none()
    | st.booleans()
    | st.integers(min_value=-(2**63), max_value=2**63 - 1)
    | st.floats(allow_nan=False, allow_infinity=False, width=64)
    | json_text
)
json_value = st.recursive(
    json_scalar,
    lambda children: st.lists(children, max_size=6)
    | st.dictionaries(json_text, children, max_size=6),
    max_leaves=25,
)
stream_value = st.lists(json_value, max_size=6) | st.dictionaries(
    json_text, json_value, max_size=6
)


class FuzzPropertyTests(unittest.TestCase):
    @settings(
        max_examples=250,
        deadline=None,
        suppress_health_check=[HealthCheck.too_slow],
    )
    @given(json_value)
    def test_dom_roundtrip_matches_stdlib(self, value: object) -> None:
        payload = compact_json(value)
        document = simdjson.parse(payload)
        self.assertEqual(json.loads(document.to_json()), json.loads(payload))

    @settings(
        max_examples=250,
        deadline=None,
        suppress_health_check=[HealthCheck.too_slow],
    )
    @given(json_value)
    def test_ondemand_roundtrip_matches_stdlib(self, value: object) -> None:
        payload = compact_json(value)
        document = simdjson.iterate(payload)
        self.assertEqual(json.loads(document.raw_json()), json.loads(payload))

    @settings(
        max_examples=200,
        deadline=None,
        suppress_health_check=[HealthCheck.too_slow],
    )
    @given(st.lists(stream_value, min_size=1, max_size=5))
    def test_streaming_apis_match_stdlib(self, values: list[object]) -> None:
        payload = " ".join(compact_json(value) for value in values)
        expected = [json.loads(compact_json(value)) for value in values]

        dom_values = [json.loads(item.to_json()) for item in simdjson.dom.parse_many(payload)]
        ondemand_values = [
            json.loads(item.raw_json()) for item in simdjson.ondemand.iterate_many(payload)
        ]

        self.assertEqual(dom_values, expected)
        self.assertEqual(ondemand_values, expected)

    @settings(max_examples=400, deadline=None)
    @given(st.binary(max_size=4096))
    def test_arbitrary_bytes_only_raise_python_exceptions(self, payload: bytes) -> None:
        operations = (
            simdjson.validate_utf8,
            simdjson.is_valid,
            simdjson.minify,
            lambda data: simdjson.parse(data).to_json(),
            lambda data: simdjson.iterate(data).raw_json(),
        )

        for operation in operations:
            try:
                operation(payload)
            except Exception:
                pass


if __name__ == "__main__":
    unittest.main()
