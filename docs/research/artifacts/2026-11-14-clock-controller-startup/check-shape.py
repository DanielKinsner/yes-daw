"""Join the saved endpoint shape to selected CoreLib allocation requests."""

import hashlib
import json
from pathlib import Path
import re


HERE = Path(__file__).resolve().parent
BASELINE = HERE.parent / "2026-11-06-clock-r12-resource-hold-baseline/sources"


def checked_bytes(path, expected):
    raw = path.read_bytes()
    if hashlib.sha256(raw).hexdigest().upper() != expected:
        raise ValueError(f"unexpected resource input: {path.name}")
    return raw


def align8(size):
    return (size + 7) // 8 * 8


def array(count, cell_bytes):
    return align8(24 + count * cell_bytes)


def main():
    observation = checked_bytes(HERE / "result.json", "C4EBC5BC121CF1D4086B29F1F908B60AF20FBC714C613406244237BBF9A7FE51")
    result = json.loads(observation)
    table = checked_bytes(BASELINE / "017-HashHelpers.cs", "498F12BD53366F09D1BABAB8F95AF01088643EF941D23CDEA31E6178A59773F4")
    hashtable = checked_bytes(BASELINE / "018-Hashtable.cs", "BC06B72D387FAE36844C63ADA61D0AB5BC447E9710AA3891906EE4E923DE674C")
    assert b"_loadFactor = 0.72f * loadFactor;" in hashtable
    primes = [int(n) for n in re.findall(rb"\d+", table.split(b"ReadOnlySpan<int> Primes =>")[1].split(b"];", 1)[0])]

    fields = result["known_value_utf16_units"]
    count = result["managed_count"]
    assert len(fields) == count == 15 and result["other_managed_count"] == 0
    assert all(name.isascii() and units > 0 for name, units in fields.items())
    assert sum(map(len, fields)) == result["managed_key_utf16_units"] == 155
    assert sum(fields.values()) == result["managed_value_utf16_units"] == 751
    sizes = [3]
    for filled in range(count):
        if filled >= sizes[-1] * 72 // 100:
            sizes.append(next(prime for prime in primes if prime >= 2 * sizes[-1]))
    generic_capacity = next(prime for prime in primes if prime >= count)
    assert sizes == [3, 7, 17, 37] and generic_capacity == 17

    strings = sum(align8(22 + 2 * extent) for name, units in fields.items() for extent in (len(name), units))
    buckets = sum(array(size, 24) for size in sizes)
    dictionary = array(generic_capacity, 4) + array(generic_capacity, 24)
    subtotal = strings + buckets + dictionary
    assert (strings, buckets, dictionary, subtotal) == (2584, 1632, 528, 4744)
    print(f"PASS: {strings:,} string + {buckets:,} Hashtable + {dictionary:,} Dictionary array bytes")
    print(f"Conditional selected requests: {subtotal:,} per capture, {3 * subtotal:,} across three captures")


if __name__ == "__main__":
    main()
