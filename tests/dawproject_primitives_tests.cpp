#include "interchange/DawprojectPrimitives.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace {

using yesdaw::engine::AssetContentHash;
using yesdaw::engine::EntityId;
using yesdaw::engine::SampleRate;
using yesdaw::interchange::dawproject::PrimitiveStatus;

constexpr EntityId idFromBytes (std::array<std::uint8_t, 16> bytes) noexcept
{
    return EntityId::fromBytes (bytes);
}

AssetContentHash hashFromSeed (std::uint8_t seed) noexcept
{
    AssetContentHash hash;
    for (std::size_t i = 0; i < hash.bytes.size(); ++i)
        hash.bytes[i] = static_cast<std::uint8_t> (seed + static_cast<std::uint8_t> (i));
    return hash;
}

} // namespace

TEST_CASE ("DAWproject XML ids are deterministic XML-safe YES DAW id projections", "[dawproject][primitive]")
{
    const EntityId clipId = idFromBytes ({
        0x01, 0x23, 0x45, 0x67,
        0x89, 0xab, 0xcd, 0xef,
        0x10, 0x32, 0x54, 0x76,
        0x98, 0xba, 0xdc, 0xfe
    });

    const auto clip = yesdaw::interchange::dawproject::xmlIdFor ("clip", clipId);
    REQUIRE (clip.status == PrimitiveStatus::Ok);
    REQUIRE (clip.value == "yd_clip_0123456789abcdef1032547698badcfe");

    const auto gain = yesdaw::interchange::dawproject::parameterXmlIdFor ("track", clipId, "volume");
    REQUIRE (gain.status == PrimitiveStatus::Ok);
    REQUIRE (gain.value == "yd_track_0123456789abcdef1032547698badcfe_volume");
}

TEST_CASE ("DAWproject XML id helpers reject ids and tokens that would not round-trip", "[dawproject][primitive]")
{
    const EntityId good = idFromBytes ({
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 1
    });

    REQUIRE (yesdaw::interchange::dawproject::xmlIdFor ("clip", EntityId {}).status
             == PrimitiveStatus::InvalidEntityId);
    REQUIRE (yesdaw::interchange::dawproject::xmlIdFor ("", good).status
             == PrimitiveStatus::InvalidToken);
    REQUIRE (yesdaw::interchange::dawproject::xmlIdFor ("AudioClip", good).status
             == PrimitiveStatus::InvalidToken);
    REQUIRE (yesdaw::interchange::dawproject::parameterXmlIdFor ("track", good, "pan-left").status
             == PrimitiveStatus::InvalidToken);
}

TEST_CASE ("DAWproject media paths are content-hash keyed canonical WAV paths", "[dawproject][primitive]")
{
    const AssetContentHash hash = hashFromSeed (0x10);

    REQUIRE (yesdaw::interchange::dawproject::assetAudioPathFor (hash)
             == "audio/101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f.wav");
}

TEST_CASE ("DAWproject time conversion preserves YES DAW tick and frame units", "[dawproject][primitive]")
{
    const auto beats = yesdaw::interchange::dawproject::ticksToBeats (yesdaw::engine::kTicksPerQuarter * 3
                                                                      + yesdaw::engine::kTicksPerQuarter / 2);
    REQUIRE (beats.status == PrimitiveStatus::Ok);
    REQUIRE (beats.value == Catch::Approx (3.5));

    const auto seconds = yesdaw::interchange::dawproject::framesToSeconds (96000, SampleRate { 48000.0 });
    REQUIRE (seconds.status == PrimitiveStatus::Ok);
    REQUIRE (seconds.value == Catch::Approx (2.0));

    REQUIRE (yesdaw::interchange::dawproject::ticksToBeats (-1).status == PrimitiveStatus::InvalidTime);
    REQUIRE (yesdaw::interchange::dawproject::framesToSeconds (1, SampleRate { 0.0 }).status
             == PrimitiveStatus::InvalidSampleRate);
}

TEST_CASE ("DAWproject XML escaping is deterministic and rejects XML-invalid control bytes", "[dawproject][primitive]")
{
    const auto escaped = yesdaw::interchange::dawproject::xmlEscape ("A&B <clip> \"name\" 'take'");
    REQUIRE (escaped.status == PrimitiveStatus::Ok);
    REQUIRE (escaped.value == "A&amp;B &lt;clip&gt; &quot;name&quot; &apos;take&apos;");

    const std::string withControl { 'o', 'k', '\x01' };
    REQUIRE (yesdaw::interchange::dawproject::xmlEscape (withControl).status
             == PrimitiveStatus::NonXmlCharacter);
}
