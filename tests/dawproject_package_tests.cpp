#include "interchange/DawprojectPackage.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
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
    secondNote.channel = -1;

    midi.notes = { firstNote, secondNote };
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

} // namespace

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
        Project duplicateXmlId = project;
        duplicateXmlId.midiClips.front().trackId = duplicateXmlId.clips.front().id;
        REQUIRE (duplicateXmlId.hasValidAssetClipIndirection());
        REQUIRE (duplicateXmlId.midiClips.front().isValid());
        REQUIRE (yesdaw::interchange::dawproject::exportProjectToDawproject (
                     makeTempPath ("duplicate-id", ".dawproject"),
                     duplicateXmlId,
                     std::span<const DecodedAssetAudio> (decoded.data(), decoded.size())).status
                 == PackageStatus::DuplicateXmlId);
    }
}
