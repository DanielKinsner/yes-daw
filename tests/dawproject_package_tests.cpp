#include "interchange/DawprojectPackage.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <clocale>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <locale>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using yesdaw::engine::Asset;
using yesdaw::engine::AssetContentHash;
using yesdaw::engine::Clip;
using yesdaw::engine::DecodedAssetAudio;
using yesdaw::engine::EntityId;
using yesdaw::engine::MidiClip;
using yesdaw::engine::Note;
using yesdaw::engine::Project;
using yesdaw::engine::SampleRate;
using yesdaw::engine::TimeBase;
using yesdaw::engine::Track;
using yesdaw::interchange::dawproject::PackageStatus;

EntityId idFromLowByte (std::uint8_t low) noexcept
{
    EntityId id;
    id.bytes[15] = low;
    return id;
}

AssetContentHash hashFromSeed (std::uint8_t seed) noexcept
{
    AssetContentHash hash;
    for (std::size_t i = 0; i < hash.bytes.size(); ++i)
        hash.bytes[i] = static_cast<std::uint8_t> (seed + static_cast<std::uint8_t> (i));
    return hash;
}

std::filesystem::path makeTempPath (std::string_view label, std::string_view extension)
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path()
        / ("yesdaw-dawproject-" + std::string (label) + "-" + std::to_string (stamp) + std::string (extension));
}

Project makeInterchangeProject()
{
    Project project;
    project.id = idFromLowByte (1);
    project.sampleRate = SampleRate { 48000.0 };
    project.tempoMap.push_back ({ 0, 128.0, yesdaw::engine::TempoCurve::Jump });
    project.meterMap.push_back ({ 0, 3, 4 });

    Asset asset;
    asset.id = idFromLowByte (2);
    asset.contentHash = hashFromSeed (0x40);
    asset.frames = 8;
    asset.sampleRate = project.sampleRate;
    asset.channels = 1;
    project.assets.push_back (asset);

    Clip first;
    first.id = idFromLowByte (10);
    first.assetId = asset.id;
    first.trackId = idFromLowByte (30);
    first.timelineStart = 4800;
    first.timelineLength = 9600;
    first.srcOffset = 1;
    first.srcLen = 4;
    first.gain = 0.75f;
    first.fadeIn = 480;
    first.fadeOut = 960;
    first.timeBase = TimeBase::SampleLocked;

    Clip second;
    second.id = idFromLowByte (11);
    second.assetId = asset.id;
    second.trackId = first.trackId;
    second.timelineStart = 20000;
    second.timelineLength = 2400;
    second.srcOffset = 2;
    second.srcLen = 3;
    second.gain = 1.25f;
    second.fadeIn = 0;
    second.fadeOut = 240;
    second.timeBase = TimeBase::SampleLocked;
    project.clips = { first, second };

    MidiClip midi;
    midi.id = idFromLowByte (20);
    midi.trackId = idFromLowByte (21);
    midi.timelineStart = yesdaw::engine::kTicksPerQuarter * 2;
    midi.timelineLength = yesdaw::engine::kTicksPerQuarter;
    midi.timeBase = TimeBase::TempoLocked;

    Note firstNote;
    firstNote.id = idFromLowByte (22);
    firstNote.startTick = yesdaw::engine::kTicksPerQuarter / 4;
    firstNote.lengthTicks = yesdaw::engine::kTicksPerQuarter / 2;
    firstNote.key = 64;
    firstNote.pitchNote = 64.0;
    firstNote.normalizedVelocity = 0.625;
    firstNote.channel = 2;

    Note secondNote;
    secondNote.id = idFromLowByte (23);
    secondNote.startTick = 0;
    secondNote.lengthTicks = yesdaw::engine::kTicksPerQuarter / 4;
    secondNote.key = 60;
    secondNote.pitchNote = 60.0;
    secondNote.normalizedVelocity = 0.5;
    secondNote.channel = 1;

    midi.notes = { firstNote, secondNote };
    Track audioTrack;
    audioTrack.id = first.trackId;
    audioTrack.strip.name = "Audio 1";
    Track midiTrack;
    midiTrack.id = midi.trackId;
    midiTrack.strip.name = "MIDI Track";
    project.tracks = { audioTrack, midiTrack };
    project.midiClips = { midi };

    REQUIRE (project.hasValidAssetClipIndirection());
    REQUIRE (project.midiClips.front().isValid());
    return project;
}

std::vector<DecodedAssetAudio> makeDecodedAssets (const Project& project)
{
    static const std::vector<float> samples {
        -0.25f, -0.125f, 0.0f, 0.125f, 0.25f, 0.5f, 0.75f, 1.0f
    };

    return {
        DecodedAssetAudio {
            project.assets.front().id,
            project.assets.front().sampleRate,
            project.assets.front().frames,
            project.assets.front().channels,
            std::span<const float> (samples.data(), samples.size())
        }
    };
}

std::vector<yesdaw::interchange::storedzip::Entry> readEntries (const std::filesystem::path& packagePath)
{
    auto read = yesdaw::interchange::storedzip::readStoredZip (packagePath);
    REQUIRE (read.ok());
    return std::move (read.entries);
}

std::string projectXmlFromEntries (const std::vector<yesdaw::interchange::storedzip::Entry>& entries)
{
    const auto* entry = yesdaw::interchange::storedzip::findEntry (
        std::span<const yesdaw::interchange::storedzip::Entry> (entries.data(), entries.size()),
        "project.xml");
    REQUIRE (entry != nullptr);
    return { entry->bytes.begin(), entry->bytes.end() };
}

void replaceProjectXml (std::vector<yesdaw::interchange::storedzip::Entry>& entries, const std::string& xml)
{
    for (yesdaw::interchange::storedzip::Entry& entry : entries)
    {
        if (entry.path == "project.xml")
        {
            entry.bytes.assign (xml.begin(), xml.end());
            return;
        }
    }

    FAIL ("project.xml entry missing");
}

void replaceFirst (std::string& text, std::string_view from, std::string_view to)
{
    const std::size_t pos = text.find (from);
    REQUIRE (pos != std::string::npos);
    text.replace (pos, from.size(), to);
}

void setFirstAttributeAfter (std::string& xml,
                             std::string_view elementNeedle,
                             std::string_view attribute,
                             std::string_view value)
{
    const std::size_t element = xml.find (elementNeedle);
    REQUIRE (element != std::string::npos);
    const std::string attrNeedle = std::string (attribute) + "=\"";
    const std::size_t attr = xml.find (attrNeedle, element);
    REQUIRE (attr != std::string::npos);
    const std::size_t valueStart = attr + attrNeedle.size();
    const std::size_t valueEnd = xml.find ('"', valueStart);
    REQUIRE (valueEnd != std::string::npos);
    xml.replace (valueStart, valueEnd - valueStart, value);
}

void writeEntries (const std::filesystem::path& packagePath,
                   const std::vector<yesdaw::interchange::storedzip::Entry>& entries)
{
    const auto result = yesdaw::interchange::storedzip::writeStoredZip (
        packagePath,
        std::span<const yesdaw::interchange::storedzip::Entry> (entries.data(), entries.size()));
    REQUIRE (result.ok());
}

std::uint32_t crc32OfText (std::string_view text) noexcept
{
    return yesdaw::interchange::storedzip::detail::crc32 (
        std::span<const std::uint8_t> (reinterpret_cast<const std::uint8_t*> (text.data()), text.size()));
}

std::vector<std::uint8_t> readRawBytes (const std::filesystem::path& path)
{
    std::ifstream file (path, std::ios::binary);
    REQUIRE (file.good());
    return std::vector<std::uint8_t> (std::istreambuf_iterator<char> (file), std::istreambuf_iterator<char> ());
}

// Independent little-endian decoders so the byte-layout assertions never reuse the reader's own helpers.
std::uint16_t le16At (const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    REQUIRE (offset + 2u <= bytes.size());
    return static_cast<std::uint16_t> (bytes[offset]) | static_cast<std::uint16_t> (bytes[offset + 1u] << 8u);
}

std::uint32_t le32At (const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    REQUIRE (offset + 4u <= bytes.size());
    return static_cast<std::uint32_t> (bytes[offset])
         | (static_cast<std::uint32_t> (bytes[offset + 1u]) << 8u)
         | (static_cast<std::uint32_t> (bytes[offset + 2u]) << 16u)
         | (static_cast<std::uint32_t> (bytes[offset + 3u]) << 24u);
}

} // namespace

TEST_CASE ("StoredZip CRC-32 matches published known-answer vectors", "[h10][dawproject][zip]")
{
    // The whole package gate is otherwise a symmetric writer/reader round-trip, so a wrong CRC polynomial,
    // init, or final-XOR would self-mask. These vectors come from the CRC-32/ISO-HDLC reference, not our code.
    REQUIRE (crc32OfText ("") == 0x00000000u);
    REQUIRE (crc32OfText ("123456789") == 0xCBF43926u);
    REQUIRE (crc32OfText ("a") == 0xE8B7BE43u);
    REQUIRE (crc32OfText ("The quick brown fox jumps over the lazy dog") == 0x414FA339u);
}

TEST_CASE ("StoredZip writer emits a spec-correct STORED archive byte layout", "[h10][dawproject][zip]")
{
    // Hand-verify the exact PKZIP byte layout of a one-entry stored archive, decoding every multi-byte
    // field independently of readStoredZip. This bites endianness, field-offset, size, CRC-placement, and
    // central-directory-offset regressions that a symmetric round-trip cannot see.
    std::vector<yesdaw::interchange::storedzip::Entry> entries {
        { "a", std::vector<std::uint8_t> { static_cast<std::uint8_t> ('b') } }
    };
    const std::filesystem::path packagePath = makeTempPath ("zip-layout", ".zip");
    writeEntries (packagePath, entries);
    const std::vector<std::uint8_t> bytes = readRawBytes (packagePath);

    constexpr std::size_t kLocalSize = 30u + 1u + 1u; // fixed header + name "a" + data "b"
    constexpr std::size_t kCentralSize = 46u + 1u;    // fixed header + name "a"
    REQUIRE (bytes.size() == kLocalSize + kCentralSize + 22u);

    // --- Local file header at offset 0 ---
    REQUIRE (le32At (bytes, 0) == 0x04034b50u);                 // signature
    REQUIRE (le16At (bytes, 8) == 0u);                          // compression method = stored
    REQUIRE (le16At (bytes, 10) == 0x0000u);                    // DOS time (valid 00:00:00)
    REQUIRE (le16At (bytes, 12) == 0x0021u);                    // DOS date (valid 1980-01-01, not 0)
    REQUIRE ((le16At (bytes, 12) & 0x1Fu) >= 1u);               // day in 1..31
    REQUIRE ((le16At (bytes, 12) & 0x1Fu) <= 31u);
    REQUIRE (((le16At (bytes, 12) >> 5u) & 0x0Fu) >= 1u);       // month in 1..12
    REQUIRE (((le16At (bytes, 12) >> 5u) & 0x0Fu) <= 12u);
    REQUIRE (le32At (bytes, 14) == crc32OfText ("b"));          // CRC stored little-endian
    REQUIRE (le32At (bytes, 18) == 1u);                         // compressed size
    REQUIRE (le32At (bytes, 22) == 1u);                         // uncompressed size
    REQUIRE (le16At (bytes, 26) == 1u);                         // file name length
    REQUIRE (le16At (bytes, 28) == 0u);                         // extra length
    REQUIRE (bytes[30] == static_cast<std::uint8_t> ('a'));     // name
    REQUIRE (bytes[31] == static_cast<std::uint8_t> ('b'));     // payload

    // --- Central directory header at offset kLocalSize ---
    REQUIRE (le32At (bytes, kLocalSize) == 0x02014b50u);                 // signature
    REQUIRE (le32At (bytes, kLocalSize + 16u) == crc32OfText ("b"));     // CRC
    REQUIRE (le32At (bytes, kLocalSize + 42u) == 0u);                    // local header offset

    // --- End of central directory at offset kLocalSize + kCentralSize ---
    const std::size_t eocd = kLocalSize + kCentralSize;
    REQUIRE (le32At (bytes, eocd) == 0x06054b50u);              // signature
    REQUIRE (le16At (bytes, eocd + 8u) == 1u);                  // entries on this disk
    REQUIRE (le16At (bytes, eocd + 10u) == 1u);                 // total entries
    REQUIRE (le32At (bytes, eocd + 12u) == kCentralSize);       // central directory size
    REQUIRE (le32At (bytes, eocd + 16u) == kLocalSize);         // central directory offset

    std::error_code ec;
    std::filesystem::remove (packagePath, ec);
}

TEST_CASE ("DAWproject export writes a stored package and verifies through an independent summary reader",
           "[h10][dawproject]")
{
    const Project project = makeInterchangeProject();
    const std::vector<DecodedAssetAudio> decoded = makeDecodedAssets (project);
    const std::filesystem::path packagePath = makeTempPath ("roundtrip", ".dawproject");

    REQUIRE (yesdaw::interchange::dawproject::exportProjectToDawproject (
                 packagePath,
                 project,
                 std::span<const DecodedAssetAudio> (decoded.data(), decoded.size())).ok());

    const auto verified = yesdaw::interchange::dawproject::verifyDawprojectPackageMatches (
        packagePath,
        project,
        std::span<const DecodedAssetAudio> (decoded.data(), decoded.size()));
    REQUIRE (verified.ok());

    const auto read = yesdaw::interchange::dawproject::readDawprojectSummary (packagePath);
    REQUIRE (read.ok());
    REQUIRE (read.summary.tempoBpm == Catch::Approx (128.0));
    REQUIRE (read.summary.meterNumerator == 3);
    REQUIRE (read.summary.meterDenominator == 4);
    REQUIRE (read.summary.tracks.size() == 4u);      // master + two synthetic audio tracks + one MIDI track
    REQUIRE (read.summary.audioClips.size() == 2u);
    REQUIRE (read.summary.midiClips.size() == 1u);
    REQUIRE (read.summary.media.size() == 1u);       // two audio Clips share the same Asset media path
    REQUIRE (read.summary.media.front().samples.size() == decoded.front().interleavedSamples.size());
    REQUIRE (std::equal (read.summary.media.front().samples.begin(),
                         read.summary.media.front().samples.end(),
                         decoded.front().interleavedSamples.begin()));
}

TEST_CASE ("DAWproject number I/O stays radix-'.' under a comma-decimal locale", "[h10][dawproject][locale]")
{
    // Find any comma-decimal locale the host actually has; skip cleanly if none is installed.
    std::locale commaLocale;
    const char* chosen = nullptr;
    for (const char* name : { "de_DE.UTF-8", "de-DE", "German_Germany.1252", "fr_FR.UTF-8", "fr-FR" })
    {
        try { commaLocale = std::locale (name); chosen = name; break; }
        catch (...) {}
    }
    if (chosen == nullptr)
    {
        SUCCEED ("no comma-decimal locale installed; locale independence cannot be exercised on this host");
        return;
    }

    // Mutate BOTH locale channels: the C++ global locale drives ostringstream (formatDouble) and C
    // LC_NUMERIC drives strtod/std::stod (the old parse path). Restore both however the test exits.
    const char* const previousCNumericRaw = std::setlocale (LC_NUMERIC, nullptr);
    const std::string previousCNumeric = previousCNumericRaw != nullptr ? previousCNumericRaw : "C";
    const std::locale previousGlobal = std::locale::global (commaLocale);
    std::setlocale (LC_NUMERIC, chosen);
    struct Restore
    {
        std::locale global;
        std::string cNumeric;
        ~Restore()
        {
            std::locale::global (global);
            std::setlocale (LC_NUMERIC, cNumeric.c_str());
        }
    } restore { previousGlobal, previousCNumeric };

    const Project project = makeInterchangeProject();
    const std::vector<DecodedAssetAudio> decoded = makeDecodedAssets (project);
    const std::filesystem::path packagePath = makeTempPath ("locale", ".dawproject");

    REQUIRE (yesdaw::interchange::dawproject::exportProjectToDawproject (
                 packagePath,
                 project,
                 std::span<const DecodedAssetAudio> (decoded.data(), decoded.size())).ok());

    // Bites formatDouble: the clip gain must serialize as "0.75", never the locale's "0,75".
    const std::string xml = projectXmlFromEntries (readEntries (packagePath));
    REQUIRE (xml.find ("value=\"0.75\"") != std::string::npos);
    REQUIRE (xml.find ("value=\"0,75\"") == std::string::npos);
    REQUIRE (xml.find (',') == std::string::npos);

    // Bites parseDouble/parseInt and the integer insertions in the export stream: the reader must
    // round-trip the '.'-radix package under the comma/grouping locale.
    REQUIRE (yesdaw::interchange::dawproject::verifyDawprojectPackageMatches (
                 packagePath,
                 project,
                 std::span<const DecodedAssetAudio> (decoded.data(), decoded.size())).ok());

    std::error_code ec;
    std::filesystem::remove (packagePath, ec);
}

TEST_CASE ("DAWproject reader rejects missing media and malformed package summaries", "[h10][dawproject][negative]")
{
    const Project project = makeInterchangeProject();
    const std::vector<DecodedAssetAudio> decoded = makeDecodedAssets (project);
    const std::filesystem::path packagePath = makeTempPath ("negative-source", ".dawproject");
    REQUIRE (yesdaw::interchange::dawproject::exportProjectToDawproject (
                 packagePath,
                 project,
                 std::span<const DecodedAssetAudio> (decoded.data(), decoded.size())).ok());

    {
        std::vector<yesdaw::interchange::storedzip::Entry> entries = readEntries (packagePath);
        const std::string mediaPath =
            yesdaw::interchange::dawproject::assetAudioPathFor (project.assets.front().contentHash);
        entries.erase (std::remove_if (entries.begin(), entries.end(),
                                       [&mediaPath] (const auto& entry) { return entry.path == mediaPath; }),
                       entries.end());
        const std::filesystem::path missingMedia = makeTempPath ("missing-media", ".dawproject");
        writeEntries (missingMedia, entries);
        REQUIRE (yesdaw::interchange::dawproject::readDawprojectSummary (missingMedia).status
                 == PackageStatus::MissingMedia);
    }

    {
        std::vector<yesdaw::interchange::storedzip::Entry> entries = readEntries (packagePath);
        std::string xml = projectXmlFromEntries (entries);
        setFirstAttributeAfter (xml, "<Clip ", "time", "-1");
        replaceProjectXml (entries, xml);
        const std::filesystem::path malformedTiming = makeTempPath ("malformed-timing", ".dawproject");
        writeEntries (malformedTiming, entries);
        REQUIRE (yesdaw::interchange::dawproject::readDawprojectSummary (malformedTiming).status
                 == PackageStatus::MalformedXml);
    }

    {
        std::vector<yesdaw::interchange::storedzip::Entry> entries = readEntries (packagePath);
        std::string xml = projectXmlFromEntries (entries);
        setFirstAttributeAfter (xml, "<Audio ", "sampleRate", "44100");
        replaceProjectXml (entries, xml);
        const std::filesystem::path wrongMedia = makeTempPath ("wrong-media", ".dawproject");
        writeEntries (wrongMedia, entries);
        REQUIRE (yesdaw::interchange::dawproject::readDawprojectSummary (wrongMedia).status
                 == PackageStatus::WrongMediaMetadata);
    }
}

TEST_CASE ("DAWproject verifier rejects changed gain and MIDI note data", "[h10][dawproject][negative]")
{
    const Project project = makeInterchangeProject();
    const std::vector<DecodedAssetAudio> decoded = makeDecodedAssets (project);
    const std::filesystem::path packagePath = makeTempPath ("verify-source", ".dawproject");
    REQUIRE (yesdaw::interchange::dawproject::exportProjectToDawproject (
                 packagePath,
                 project,
                 std::span<const DecodedAssetAudio> (decoded.data(), decoded.size())).ok());

    {
        std::vector<yesdaw::interchange::storedzip::Entry> entries = readEntries (packagePath);
        std::string xml = projectXmlFromEntries (entries);
        replaceFirst (xml, "value=\"0.75\"", "value=\"0.5\"");
        replaceProjectXml (entries, xml);
        const std::filesystem::path changedGain = makeTempPath ("changed-gain", ".dawproject");
        writeEntries (changedGain, entries);
        REQUIRE (yesdaw::interchange::dawproject::verifyDawprojectPackageMatches (
                     changedGain,
                     project,
                     std::span<const DecodedAssetAudio> (decoded.data(), decoded.size())).status
                 == PackageStatus::SummaryMismatch);
    }

    {
        std::vector<yesdaw::interchange::storedzip::Entry> entries = readEntries (packagePath);
        std::string xml = projectXmlFromEntries (entries);
        replaceFirst (xml, "key=\"64\"", "key=\"65\"");
        replaceProjectXml (entries, xml);
        const std::filesystem::path changedNote = makeTempPath ("changed-note", ".dawproject");
        writeEntries (changedNote, entries);
        REQUIRE (yesdaw::interchange::dawproject::verifyDawprojectPackageMatches (
                     changedNote,
                     project,
                     std::span<const DecodedAssetAudio> (decoded.data(), decoded.size())).status
                 == PackageStatus::SummaryMismatch);
    }
}

TEST_CASE ("DAWproject exporter rejects unsupported or lossy Project surfaces explicitly",
           "[h10][dawproject][negative]")
{
    Project project = makeInterchangeProject();
    std::vector<DecodedAssetAudio> decoded = makeDecodedAssets (project);

    REQUIRE (yesdaw::interchange::dawproject::exportProjectToDawproject (
                 makeTempPath ("missing-decoded", ".dawproject"),
                 project,
                 std::span<const DecodedAssetAudio> {}).status == PackageStatus::MissingAssetAudio);

    {
        std::vector<DecodedAssetAudio> mismatched = decoded;
        mismatched.front().frames += 1u;
        REQUIRE (yesdaw::interchange::dawproject::exportProjectToDawproject (
                     makeTempPath ("mismatch", ".dawproject"),
                     project,
                     std::span<const DecodedAssetAudio> (mismatched.data(), mismatched.size())).status
                 == PackageStatus::AssetMetadataMismatch);
    }

    {
        Project unsupportedTimeBase = project;
        unsupportedTimeBase.clips.front().timeBase = TimeBase::TempoLocked;
        REQUIRE (yesdaw::interchange::dawproject::exportProjectToDawproject (
                     makeTempPath ("tempo-audio", ".dawproject"),
                     unsupportedTimeBase,
                     std::span<const DecodedAssetAudio> (decoded.data(), decoded.size())).status
                 == PackageStatus::UnsupportedTimeBase);
    }

    {
        Project wide = project;
        wide.assets.front().channels = 3;
        wide.assets.front().frames = 2;
        wide.clips.front().srcOffset = 0;
        wide.clips.front().srcLen = 2;
        wide.clips.back().srcOffset = 0;
        wide.clips.back().srcLen = 2;
        std::vector<float> wideSamples { 0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f };
        std::vector<DecodedAssetAudio> wideDecoded {
            DecodedAssetAudio {
                wide.assets.front().id,
                wide.assets.front().sampleRate,
                wide.assets.front().frames,
                wide.assets.front().channels,
                std::span<const float> (wideSamples.data(), wideSamples.size())
            }
        };
        REQUIRE (wide.hasValidAssetClipIndirection());
        REQUIRE (yesdaw::interchange::dawproject::exportProjectToDawproject (
                     makeTempPath ("wide", ".dawproject"),
                     wide,
                     std::span<const DecodedAssetAudio> (wideDecoded.data(), wideDecoded.size())).status
                 == PackageStatus::UnsupportedAssetChannels);
    }

    {
        // -1 is a legal internal channel sentinel but a lossy DAWproject export; it must fail explicitly
        // (ADR-0029) rather than silently coercing to channel 0.
        Project unassignedChannel = project;
        unassignedChannel.midiClips.front().notes.front().channel = -1;
        REQUIRE (unassignedChannel.midiClips.front().isValid());
        REQUIRE (yesdaw::interchange::dawproject::exportProjectToDawproject (
                     makeTempPath ("unassigned-channel", ".dawproject"),
                     unassignedChannel,
                     std::span<const DecodedAssetAudio> (decoded.data(), decoded.size())).status
                 == PackageStatus::InvalidProject);
    }

    {
        Project invalidTrackOwner = project;
        invalidTrackOwner.midiClips.front().trackId = invalidTrackOwner.clips.front().id;
        REQUIRE_FALSE (invalidTrackOwner.hasValidAssetClipIndirection());
        REQUIRE (yesdaw::interchange::dawproject::exportProjectToDawproject (
                     makeTempPath ("invalid-track-owner", ".dawproject"),
                     invalidTrackOwner,
                     std::span<const DecodedAssetAudio> (decoded.data(), decoded.size())).status
                 == PackageStatus::InvalidProject);
    }
}
