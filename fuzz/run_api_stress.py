from __future__ import annotations

import argparse
import random
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import simdjsonpy as simdjson


ROOT = Path(__file__).resolve().parents[1]
SEED_FILES = [
    ROOT / "ext/simdjson/jsonexamples/twitter.json",
    ROOT / "ext/simdjson/jsonexamples/citm_catalog.json",
    ROOT / "ext/simdjson/jsonexamples/amazon_cellphones.ndjson",
]


def mutate(payload: bytes, rng: random.Random) -> bytes:
    if not payload:
        return bytes([rng.randrange(256) for _ in range(rng.randint(1, 32))])

    choice = rng.randrange(5)
    if choice == 0:
        index = rng.randrange(len(payload))
        mutated = bytearray(payload)
        mutated[index] ^= 1 << rng.randrange(8)
        return bytes(mutated)
    if choice == 1:
        return payload[: rng.randrange(len(payload) + 1)]
    if choice == 2:
        start = rng.randrange(len(payload))
        end = rng.randrange(start, len(payload))
        return payload[:start] + payload[start:end] + payload[start:]
    if choice == 3:
        insert = bytes([rng.randrange(256) for _ in range(rng.randint(1, 16))])
        index = rng.randrange(len(payload) + 1)
        return payload[:index] + insert + payload[index:]
    return payload + b" " + payload[: rng.randrange(min(len(payload), 64)) + 1]


def exercise_payload(
    payload: bytes,
    dom_parser: simdjson.DOMParser,
    ondemand_parser: simdjson.OnDemandParser,
) -> None:
    try:
        simdjson.validate_utf8(payload)
    except Exception:
        pass

    try:
        simdjson.is_valid(payload)
    except Exception:
        pass

    try:
        simdjson.minify(payload)
    except Exception:
        pass

    try:
        dom_parser.parse(payload).to_json()
    except Exception:
        pass

    try:
        ondemand_parser.iterate(payload).raw_json()
    except Exception:
        pass

    try:
        dom_parser.parse_many(payload, batch_size=max(64, len(payload)))
    except Exception:
        pass

    try:
        ondemand_parser.iterate_many(payload, batch_size=max(64, len(payload)))
    except Exception:
        pass


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rounds", type=int, default=5000)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--seed", type=int, default=0)
    args = parser.parse_args()

    rng = random.Random(args.seed)
    seeds = [path.read_bytes() for path in SEED_FILES if path.exists()]
    dom_parser = simdjson.DOMParser()
    ondemand_parser = simdjson.OnDemandParser()

    payloads = [mutate(rng.choice(seeds), rng) for _ in range(args.rounds)]

    with ThreadPoolExecutor(max_workers=args.workers) as executor:
        futures = [
            executor.submit(exercise_payload, payload, dom_parser, ondemand_parser)
            for payload in payloads
        ]
        for future in futures:
            future.result()

    print(
        f"completed {args.rounds} mutated payloads with {args.workers} worker(s)"
    )


if __name__ == "__main__":
    main()
