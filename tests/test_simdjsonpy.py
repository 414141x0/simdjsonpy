from __future__ import annotations

import unittest
from concurrent.futures import ThreadPoolExecutor

import simdjsonpy as simdjson


class CommonTests(unittest.TestCase):
    def test_padded_string_and_common_helpers(self) -> None:
        padded = simdjson.PaddedString('{"msg":"hi"}')
        self.assertEqual(len(padded), 12)
        self.assertEqual(bytes(padded), b'{"msg":"hi"}')
        self.assertTrue(padded.append(" "))
        self.assertEqual(bytes(padded), b'{"msg":"hi"} ')

        self.assertTrue(simdjson.validate_utf8("hello"))
        self.assertTrue(simdjson.is_valid('{"ok":true}'))
        self.assertEqual(simdjson.minify('{ "ok": true }'), '{"ok":true}')

    def test_available_implementations_are_exposed(self) -> None:
        implementations = simdjson.available_implementations()
        self.assertGreaterEqual(len(implementations), 1)
        self.assertTrue(all(implementation.name for implementation in implementations))


class DomTests(unittest.TestCase):
    def test_parser_reuse_keeps_documents_alive(self) -> None:
        parser = simdjson.DOMParser()
        first = parser.parse('{"value":1}')
        second = parser.parse('{"value":2}')

        self.assertEqual(first["value"].get_int64(), 1)
        self.assertEqual(second["value"].get_int64(), 2)

    def test_parse_into_reuses_document_storage(self) -> None:
        parser = simdjson.DOMParser()
        document = simdjson.DOMDocument()

        parser.parse_into(document, '{"status":"first"}')
        self.assertEqual(document["status"].get_string(), "first")

        parser.parse_into(document, '{"status":"second"}')
        self.assertEqual(document["status"].get_string(), "second")

    def test_stream_items_become_stale_after_advance(self) -> None:
        parser = simdjson.DOMParser()
        stream = parser.parse_many('{"value":1} {"value":2}', batch_size=64)
        iterator = iter(stream)

        first = next(iterator)
        self.assertEqual(first["value"].get_int64(), 1)

        second = next(iterator)
        self.assertEqual(second["value"].get_int64(), 2)

        with self.assertRaises(ReferenceError):
            first["value"].get_int64()

    def test_shared_dom_parser_is_safe_to_reuse_from_threads(self) -> None:
        parser = simdjson.DOMParser()

        def parse_value(value: int) -> int:
            document = parser.parse(f'{{"value":{value}}}')
            return document["value"].get_int64()

        with ThreadPoolExecutor(max_workers=4) as executor:
            results = list(executor.map(parse_value, range(12)))

        self.assertEqual(results, list(range(12)))


class OnDemandTests(unittest.TestCase):
    def test_iterate_supports_pythonic_repeated_access(self) -> None:
        document = simdjson.iterate('{"items":[1,2,3],"flag":true}')

        self.assertEqual(document.type, simdjson.OnDemandJSONType.OBJECT)
        self.assertTrue(document["flag"].get_bool())
        self.assertEqual(len(document["items"].get_array()), 3)
        self.assertEqual(document["items"].get_array()[1].get_int64(), 2)

    def test_iterate_returns_stable_document_state(self) -> None:
        parser = simdjson.OnDemandParser()
        document = parser.iterate('{"flag":true,"count":2}')

        self.assertTrue(document.get_object().find_field_unordered("flag").get_bool())
        self.assertEqual(document["count"].get_int64(), 2)

    def test_document_stream_references_become_stale_after_advance(self) -> None:
        parser = simdjson.OnDemandParser()
        stream = parser.iterate_many('{"value":1} {"value":2}', batch_size=64)
        iterator = iter(stream)

        first = next(iterator)
        self.assertEqual(first["value"].get_int64(), 1)

        second = next(iterator)
        self.assertEqual(second["value"].get_int64(), 2)

        with self.assertRaises(ReferenceError):
            first["value"].get_int64()

    def test_raw_json_string_and_location_are_pythonic(self) -> None:
        document = simdjson.iterate('{"msg":"h\\u00E9"}')
        raw = document["msg"].get_raw_json_string()

        self.assertEqual(raw.raw(), r"h\u00E9")
        self.assertEqual(raw.unescape(), "hé")
        self.assertIsInstance(document.current_location(), int)
        self.assertGreaterEqual(document.current_location(), 0)

    def test_shared_ondemand_parser_config_is_safe_from_threads(self) -> None:
        parser = simdjson.OnDemandParser()

        def parse_value(value: int) -> int:
            document = parser.iterate(f'{{"value":{value}}}')
            return document["value"].get_int64()

        with ThreadPoolExecutor(max_workers=4) as executor:
            results = list(executor.map(parse_value, range(12)))

        self.assertEqual(results, list(range(12)))


if __name__ == "__main__":
    unittest.main()
