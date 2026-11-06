// YES DAW — InstrumentState: the persisted per-Track instrument parameter blob (G3.1 / ADR-0047).
//
// A Track's instrument slot stores an OPAQUE, kind-versioned blob. For SimpleSynth it is the list
// of normalized ParamSpec values the user set (unset = the spec's default), encoded flat and
// little-endian so the same bytes mean the same thing on every host:
//
//   byte 0        : format version (1)
//   bytes 1..4    : count (u32 LE)
//   then count ×  : param id (u32 LE), normalized value (f64 bits, LE)
//
// Empty bytes = no overrides. Control-side only (never on the audio thread).

#pragma once

#include "engine/ParamSpec.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>
#include <vector>

namespace yesdaw::engine {

using InstrumentParamValues = std::vector<std::pair<std::uint32_t, double>>;

inline constexpr std::uint8_t kInstrumentStateFormatVersion = 1;

[[nodiscard]] inline std::vector<std::uint8_t> encodeInstrumentParams (const InstrumentParamValues& values)
{
    std::vector<std::uint8_t> out;
    if (values.empty())
        return out;   // no overrides = no bytes (the pre-slot shape)

    const auto putU32 = [&out] (std::uint32_t v)
    {
        for (int shift = 0; shift < 32; shift += 8)
            out.push_back (static_cast<std::uint8_t> ((v >> shift) & 0xFFu));
    };
    const auto putF64 = [&out] (double v)
    {
        const std::uint64_t bits = std::bit_cast<std::uint64_t> (v);
        for (int shift = 0; shift < 64; shift += 8)
            out.push_back (static_cast<std::uint8_t> ((bits >> shift) & 0xFFu));
    };

    out.reserve (5u + values.size() * 12u);
    out.push_back (kInstrumentStateFormatVersion);
    putU32 (static_cast<std::uint32_t> (values.size()));
    for (const auto& [id, value] : values)
    {
        putU32 (id);
        putF64 (value);
    }
    return out;
}

// False for a malformed blob (wrong version, short, non-finite value). Empty bytes decode to no values.
[[nodiscard]] inline bool decodeInstrumentParams (std::span<const std::uint8_t> bytes, InstrumentParamValues& out)
{
    out.clear();
    if (bytes.empty())
        return true;
    if (bytes.size() < 5u || bytes[0] != kInstrumentStateFormatVersion)
        return false;

    const auto getU32 = [&bytes] (std::size_t at)
    {
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i)
            v |= static_cast<std::uint32_t> (bytes[at + static_cast<std::size_t> (i)]) << (8 * i);
        return v;
    };
    const auto getF64 = [&bytes] (std::size_t at)
    {
        std::uint64_t bits = 0;
        for (int i = 0; i < 8; ++i)
            bits |= static_cast<std::uint64_t> (bytes[at + static_cast<std::size_t> (i)]) << (8 * i);
        return std::bit_cast<double> (bits);
    };

    const std::uint32_t count = getU32 (1);
    if (bytes.size() != 5u + static_cast<std::size_t> (count) * 12u)
        return false;

    out.reserve (count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        const std::size_t at = 5u + static_cast<std::size_t> (i) * 12u;
        const std::uint32_t id = getU32 (at);
        const double value = getF64 (at + 4u);
        if (! std::isfinite (value) || value < 0.0 || value > 1.0)
            return false;
        for (const auto& [seenId, seenValue] : out)
            if (seenId == id)
                return false;   // a duplicate id is malformed
        out.emplace_back (id, value);
    }
    return true;
}

// Sets (or replaces) one value in a decoded list, keeping ids in ascending order so the
// re-encoded blob is canonical (equal states encode to equal bytes).
inline void setInstrumentParamValue (InstrumentParamValues& values, std::uint32_t id, double normalized)
{
    for (auto& [seenId, seenValue] : values)
        if (seenId == id)
        {
            seenValue = normalized;
            return;
        }
    auto at = values.begin();
    while (at != values.end() && at->first < id)
        ++at;
    values.insert (at, { id, normalized });
}

} // namespace yesdaw::engine
