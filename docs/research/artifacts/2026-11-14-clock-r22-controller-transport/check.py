"""Check selected R22 transport payload and builder bounds without running the runner."""

import hashlib
import json
from pathlib import Path


ARTIFACTS = Path(__file__).resolve().parents[1]
ROOT = ARTIFACTS.parents[2]
REVISION = ROOT / (
    "build-ci/musical-audio-persistent-planner-certification/source-revisions/"
    "clock-oracle-r22-guard-read-source-r1"
)
METADATA = ARTIFACTS / (
    "2026-11-06-managed-file-metadata-r4/20261106-managed-file-type-inventory-source-r4/"
    "execution-r1/inventory.json"
)


def read_pin(path, expected):
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest().upper() != expected:
        raise ValueError(f"source pin differs: {path}")
    return data


def align(value):
    return (value + 7) & ~7


def array(length, width):
    return align(24 + width * length)


def string(length):
    return align(22 + 2 * length)


def wide_units(value):
    return len(value.encode("utf-16-le")) // 2


def command_rental_sizes(filename_units, argument_units, quote):
    used = 0
    capacity = 256
    rented = []
    # Appends: optional opening quote, filename, optional closing quote,
    # space, unquoted --request16 argument, final NUL from the pinned reference.
    for count in (quote, filename_units, quote, 1, argument_units, 1):
        if used + count > capacity:
            needed = max(used + count, 2 * capacity)
            capacity = 1 << (needed - 1).bit_length()
            rented.append(capacity)
        used += count
    return rented


def environment_chunk_sizes(key_lengths, value_lengths):
    used = 0
    capacity = 64
    blocks = [capacity]
    for key, value in zip(key_lengths, value_lengths, strict=True):
        for count in (key, 1, value, 1):
            if used + count > capacity:
                block = max(used + count - capacity, min(capacity, 8000))
                blocks.append(block)
                capacity += block
            used += count
    return blocks


def main():
    if not __debug__:
        raise ValueError("run this source check without Python optimization")
    manifest = json.loads(read_pin(REVISION / "source-manifest.json",
                                  "28619B3443768E03BA45FD698030BC5DD743C6B819125D475B4AA1A2687D0C06"))
    runner = read_pin(REVISION / "run-clock.ps1",
                      "294C4D91D0FF0EB0E80048B71DFC1E24C45103E88AD97F2F7715F9BD252F4798")
    metadata = json.loads(read_pin(METADATA, "7C23E7BF82F9D293962C06BBD450BC4D9F7BBD74FF9E5344C8E4C88686B04D6D"))
    allocation = ARTIFACTS / "2026-11-05-managed-payload-abi-probe-r2"
    runtime = json.loads(read_pin(
        allocation / "20261105-managed-payload-abi-runtime-metadata-r1/runtime.json",
        "45817E116A4E6F6F9C9AA5089D1773F3F8978442A369D1DAFFDCF6A03CFBB8D0",
    ))
    input_hashes = {row["sha256"] for row in manifest["inputs"]}
    assert all(row["sha256"] in input_hashes for row in runtime["files"])
    assert runtime["current_tool_host"]["pointer_bytes"] == 8
    assert not any(manifest["guards"].values())
    for anchor in (
        b"$taskHashBuffer=[byte[]]::new(65536)", b"$taskValues=[string[]]::new(8)",
        b"$taskStrings=[string[]]::new(12+$Arguments.Length)", b"$taskBytes=[byte[]]::new($taskLength)",
        b"$taskChars=[char[]]::new(12+4*$Path.Length)", b"return [string]::new($taskChars)",
        b"$taskCleanup=[Collections.Generic.List[string]]::new(15)",
        b"$taskBytes=[byte[]]::new(308)", b"$taskU=[uint32[]]::new(16)",
        b"$taskV=[uint64[]]::new(3)", b"$taskP=[uint64[]]::new(14)",
        b"$taskInfo.Environment.Clear()", b"$taskInfo.ArgumentList.Add($taskRequestArgument)",
        b"$taskProcess=[Diagnostics.Process]::Start($taskInfo)",
        b"if($null -ne $script:taskQuarantine){throw 'Unsettled prior child owns storage; no next child'}",
    ):
        assert anchor in runner, anchor
    for artifact, relative, expected, anchor in (
        ("2026-11-06-controller-v-source-hold", "001-Process.Windows.cs",
         "49620FB20FEFFB84E36D587A381C0CF161E3EC8C114F6FF1E7FA2D7672B518CF",
         b"new ValueStringBuilder(stackalloc char[256])"),
        ("2026-11-06-controller-v-source-hold", "042-Process.cs",
         "458239074E7A2167A2D01B44459552E2F13DB5DA9BCF7C581933028BC8A093AE",
         b"process.StartInfo = startInfo"),
        ("2026-11-06-clock-r12-fresh-path-hold", "006-SharedArrayPool.cs",
         "9AB00C5DE4F3B220149DBDA3B9E4924F04AC132FC451B592EBD53872FC198DBA",
         b"Utilities.GetMaxSizeForBucket(bucketIndex)"),
        ("2026-11-06-clock-r12-fresh-path-hold", "007-ValueStringBuilder.cs",
         "737198AD816E8023DFD3CB4309D092DD1CE431A4CE2268CC37D42310F0528056",
         b"ArrayPool<char>.Shared.Rent(newCapacity)"),
        ("2026-11-06-clock-r13-runner-census-r3", "055-StringBuilder.cs",
         "4852F4B57D749BBFB9D5B865E0784974EE5F056DE4427C6E584FB4ECD193C751",
         b"Math.Max(minBlockCharCount, Math.Min(Length, MaxChunkSize))"),
    ):
        assert anchor in read_pin(ARTIFACTS / artifact / "sources" / relative, expected), relative

    tools = {row["qualified_name"]: row for row in metadata["types"]}
    expected_fields = {
        "System.Diagnostics.Process": 51, "System.ComponentModel.Component": 2,
        "System.MarshalByRefObject": 0, "System.Diagnostics.ProcessStartInfo": 24,
        "System.Collections.Specialized.DictionaryWrapper": 1, "System.Diagnostics.Stopwatch": 3,
    }
    for name, count in expected_fields.items():
        row = tools[name]
        assert row["declared_nonstatic_instance_field_count"] == count
        assert row["file_sha256"] in input_hashes

    path_units = 512
    request_caps = [140 + (12 + count) * (4 + 2 * path_units) for count in (28, 6, 6)]
    terminal = sum(array(length, width) for length, width in ((308, 1), (16, 4), (3, 8), (14, 8)))
    transport_files = sum(request_caps) + 3 * 308
    assert request_caps == manifest["supervisor"]["request_caps"] == [41260, 18644, 18644]
    assert transport_files == manifest["supervisor"]["transport_cap"] == 79472
    encapsulated_request = [array(size, 1) + array(count, 8)
                            for size, count in zip(request_caps, (40, 18, 18), strict=True)]
    assert encapsulated_request == [41632, 18840, 18840] and terminal == 608

    argument_at_max = 12 + 4 * path_units
    command_units = path_units + 2 + 1 + argument_at_max + 1
    command_builder = max(
        2 * 256 + sum(array(capacity, 2) for capacity in command_rental_sizes(name, 12 + 4 * path, quote))
        for name in range(1, path_units + 1) for path in range(1, path_units + 1) for quote in (0, 1)
    )
    assert command_units == 2576 and command_builder == 13896

    env = manifest["environment"]
    names = sorted(env, key=str.upper)
    temp = "<OWNED_OPAQUE_TEMP>"
    assert {name for name in names if env[name] == temp} == {"TEMP", "TMP"}
    assert len(names) == 8 and sum(wide_units(key) for key in names) == 43
    fixed_values = sum(wide_units(v) for v in env.values() if v != temp)
    assert fixed_values == 497
    environment_block_units = 43 + fixed_values + 2 * path_units + 2 * len(names)
    environment_builder = max(
        sum(array(capacity, 2) for capacity in environment_chunk_sizes(
            list(map(wide_units, names)), [path if env[name] == temp else wide_units(env[name]) for name in names]
        )) for path in range(path_units + 1)
    )
    assert environment_block_units == 1580 and environment_builder == 5568
    print(json.dumps({
        "compiler_request_array_and_references_bytes": encapsulated_request[0],
        "target_request_array_and_references_bytes": encapsulated_request[1],
        "controller_terminal_arrays_bytes": terminal,
        "one_shared_hash_array_bytes": array(65536, 1),
        "three_current_digest_arrays_bytes": 3 * array(32, 1),
        "hex_request_array_and_string_bytes": array(argument_at_max, 2) + string(argument_at_max),
        "helper_command_utf16_units_including_nul": command_units,
        "conditional_command_builder_array_and_stack_bytes": command_builder,
        "helper_environment_utf16_units_including_final_nul": environment_block_units + 1,
        "conditional_environment_keys_builder_block_bytes": array(8, 8) + environment_builder
                                                            + string(environment_block_units),
        "conditional_field_counts": expected_fields,
        "retained_transport_file_bytes": transport_files,
    }, indent=2))


if __name__ == "__main__":
    main()
