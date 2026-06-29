#pragma once

#include "engine/Project.h"
#include "engine/Time.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace yesdaw::interchange::dawproject {

enum class PrimitiveStatus : std::uint8_t
{
    Ok = 0,
    InvalidEntityId,
    InvalidToken,
    InvalidTime,
    InvalidSampleRate,
    NonXmlCharacter
};

struct StringResult
{
    PrimitiveStatus status = PrimitiveStatus::Ok;
    std::string     value;

    [[nodiscard]] bool ok() const noexcept { return status == PrimitiveStatus::Ok; }
};

struct DoubleResult
{
    PrimitiveStatus status = PrimitiveStatus::Ok;
    double          value = 0.0;

    [[nodiscard]] bool ok() const noexcept { return status == PrimitiveStatus::Ok; }
};

namespace detail {

inline char hexDigit (std::uint8_t value) noexcept
{
    return static_cast<char> (value < 10u ? ('0' + value) : ('a' + (value - 10u)));
}

inline bool tokenCharIsValid (char c) noexcept
{
    return (c >= 'a' && c <= 'z')
        || (c >= '0' && c <= '9')
        || c == '_';
}

inline bool tokenIsValid (std::string_view token) noexcept
{
    if (token.empty())
        return false;

    for (const char c : token)
        if (! tokenCharIsValid (c))
            return false;

    return true;
}

inline bool xmlCharIsValid (char c) noexcept
{
    const auto u = static_cast<unsigned char> (c);
    return u == 0x09u || u == 0x0Au || u == 0x0Du || u >= 0x20u;
}

} // namespace detail

inline std::string hexLower (const engine::EntityId::StorageBytes& bytes)
{
    std::string out;
    out.resize (bytes.size() * 2u);

    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        out[i * 2u] = detail::hexDigit (static_cast<std::uint8_t> ((bytes[i] >> 4u) & 0x0Fu));
        out[i * 2u + 1u] = detail::hexDigit (static_cast<std::uint8_t> (bytes[i] & 0x0Fu));
    }

    return out;
}

inline std::string hexLower (const engine::AssetContentHash::StorageBytes& bytes)
{
    std::string out;
    out.resize (bytes.size() * 2u);

    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        out[i * 2u] = detail::hexDigit (static_cast<std::uint8_t> ((bytes[i] >> 4u) & 0x0Fu));
        out[i * 2u + 1u] = detail::hexDigit (static_cast<std::uint8_t> (bytes[i] & 0x0Fu));
    }

    return out;
}

inline StringResult xmlIdFor (std::string_view kind, engine::EntityId id)
{
    if (! id.isValid())
        return { PrimitiveStatus::InvalidEntityId, {} };

    if (! detail::tokenIsValid (kind))
        return { PrimitiveStatus::InvalidToken, {} };

    return { PrimitiveStatus::Ok, "yd_" + std::string (kind) + "_" + hexLower (id.bytes) };
}

inline StringResult parameterXmlIdFor (std::string_view kind, engine::EntityId owner, std::string_view role)
{
    StringResult base = xmlIdFor (kind, owner);
    if (! base.ok())
        return base;

    if (! detail::tokenIsValid (role))
        return { PrimitiveStatus::InvalidToken, {} };

    base.value += "_";
    base.value += role;
    return base;
}

inline std::string assetAudioPathFor (const engine::AssetContentHash& hash)
{
    return "audio/" + hexLower (hash.bytes) + ".wav";
}

inline DoubleResult ticksToBeats (engine::Tick tick) noexcept
{
    if (tick < 0)
        return { PrimitiveStatus::InvalidTime, 0.0 };

    const double beats = static_cast<double> (tick) / static_cast<double> (engine::kTicksPerQuarter);
    return std::isfinite (beats) ? DoubleResult { PrimitiveStatus::Ok, beats }
                                : DoubleResult { PrimitiveStatus::InvalidTime, 0.0 };
}

inline DoubleResult framesToSeconds (std::uint64_t frames, engine::SampleRate sampleRate) noexcept
{
    if (! sampleRate.isValid())
        return { PrimitiveStatus::InvalidSampleRate, 0.0 };

    const double seconds = static_cast<double> (frames) / sampleRate.hz;
    return std::isfinite (seconds) ? DoubleResult { PrimitiveStatus::Ok, seconds }
                                  : DoubleResult { PrimitiveStatus::InvalidTime, 0.0 };
}

inline StringResult xmlEscape (std::string_view text)
{
    std::string out;
    out.reserve (text.size());

    for (const char c : text)
    {
        if (! detail::xmlCharIsValid (c))
            return { PrimitiveStatus::NonXmlCharacter, {} };

        switch (c)
        {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            case '\'':
                out += "&apos;";
                break;
            default:
                out.push_back (c);
                break;
        }
    }

    return { PrimitiveStatus::Ok, std::move (out) };
}

} // namespace yesdaw::interchange::dawproject
