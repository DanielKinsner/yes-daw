#pragma once

#include "engine/OfflineRenderer.h"
#include "interchange/DawprojectPrimitives.h"
#include "interchange/StoredZip.h"
#include "io/WavFile.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Build-time version stamp (git-describe), injected by CMake (YESDAW_GIT_VERSION) — the same stamp
// the GUI app and the YesDawSelfCheck CLI carry. The exported .dawproject <Application> tag uses it
// so interchange files a user shares to another DAW no longer advertise a bare "0.0.0". Falls back
// to a dev marker for a target that does not define it (e.g. a pure test build).
#ifndef YESDAW_VERSION_STRING
  #define YESDAW_VERSION_STRING "0.0.0-dev"
#endif

namespace yesdaw::interchange::dawproject {

enum class PackageStatus : std::uint8_t
{
    Ok = 0,
    InvalidProject,
    InvalidTimeline,
    MissingAssetAudio,
    AssetMetadataMismatch,
    UnsupportedAssetChannels,
    UnsupportedTimeBase,
    NonFiniteValue,
    DuplicateXmlId,
    PackageWriteFailed,
    PackageReadFailed,
    MissingPackageEntry,
    MalformedXml,
    MissingMedia,
    WrongMediaMetadata,
    MediaDecodeFailed,
    SummaryMismatch
};

struct PackageResult
{
    PackageStatus status = PackageStatus::Ok;
    std::string   message;

    [[nodiscard]] bool ok() const noexcept { return status == PackageStatus::Ok; }
};

struct TrackSummary
{
    std::string id;
    std::string contentType;
    std::string role;
    std::string destination;
    int         audioChannels = 0;
    double      volume = 0.0;
    double      pan = 0.0;
};

struct AudioClipSummary
{
    std::string trackId;
    std::string timeUnit;
    double      time = 0.0;
    double      duration = 0.0;
    double      playStart = 0.0;
    double      playStop = 0.0;
    double      fadeIn = 0.0;
    double      fadeOut = 0.0;
    std::string mediaPath;
    int         audioChannels = 0;
    int         sampleRate = 0;
    double      mediaDuration = 0.0;
};

struct NoteSummary
{
    double time = 0.0;
    double duration = 0.0;
    int    channel = 0;
    int    key = 0;
    double velocity = 0.0;
};

struct MidiClipSummary
{
    std::string              trackId;
    std::string              timeUnit;
    double                   time = 0.0;
    double                   duration = 0.0;
    std::vector<NoteSummary> notes;
};

struct MediaSummary
{
    std::string              path;
    engine::SampleRate       sampleRate;
    std::uint16_t            channels = 0;
    std::uint64_t            frames = 0;
    std::vector<float>       samples;
};

struct ProjectSummary
{
    double                        tempoBpm = 120.0;
    int                           meterNumerator = 4;
    int                           meterDenominator = 4;
    std::vector<std::string>      xmlIds;
    std::vector<TrackSummary>     tracks;
    std::vector<AudioClipSummary> audioClips;
    std::vector<MidiClipSummary>  midiClips;
    std::vector<MediaSummary>     media;
};

struct ReadResult
{
    PackageStatus status = PackageStatus::Ok;
    std::string   message;
    ProjectSummary summary;

    [[nodiscard]] bool ok() const noexcept { return status == PackageStatus::Ok; }
};

namespace detail {

inline PackageResult fail (PackageStatus status, std::string message)
{
    return { status, std::move (message) };
}

inline ReadResult readFail (PackageStatus status, std::string message)
{
    return { status, std::move (message), {} };
}

// DAWproject XML numbers are radix-'.' always. std::ostringstream/std::stod follow the ambient locale
// (the C++ global locale and C LC_NUMERIC respectively), so under a comma-decimal locale the exporter
// would emit "1,5" (invalid XML) and the reader would misparse. Imbue the classic locale on every stream
// so format/parse are locale-independent in both directions. setprecision(17) keeps double round-trip.
inline std::string formatDouble (double value)
{
    std::ostringstream out;
    out.imbue (std::locale::classic());
    out << std::setprecision (17) << value;
    return out.str();
}

inline bool parseDouble (std::string_view text, double& out) noexcept
{
    try
    {
        std::istringstream in { std::string (text) };
        in.imbue (std::locale::classic());
        double value = 0.0;
        if (! (in >> value) || in.get() != std::char_traits<char>::eof() || ! std::isfinite (value))
            return false;

        out = value;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

inline bool parseInt (std::string_view text, int& out) noexcept
{
    try
    {
        std::istringstream in { std::string (text) };
        in.imbue (std::locale::classic());
        int value = 0;
        if (! (in >> value) || in.get() != std::char_traits<char>::eof())
            return false;

        out = value;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

inline std::vector<std::uint8_t> bytesForString (std::string_view text)
{
    return { text.begin(), text.end() };
}

inline bool readFileBytes (const std::filesystem::path& path, std::vector<std::uint8_t>& out)
{
    out.clear();

    std::ifstream file (path, std::ios::binary);
    if (! file)
        return false;

    file.seekg (0, std::ios::end);
    const std::streamoff size = file.tellg();
    file.seekg (0, std::ios::beg);
    if (size < 0)
        return false;

    out.resize (static_cast<std::size_t> (size));
    if (! out.empty())
        file.read (reinterpret_cast<char*> (out.data()), static_cast<std::streamsize> (out.size()));

    return static_cast<bool> (file) || out.empty();
}

inline bool writeFileBytes (const std::filesystem::path& path, std::span<const std::uint8_t> bytes)
{
    std::error_code ec;
    if (path.has_parent_path())
    {
        std::filesystem::create_directories (path.parent_path(), ec);
        if (ec)
            return false;
    }

    std::ofstream file (path, std::ios::binary | std::ios::trunc);
    if (! file)
        return false;
    if (! bytes.empty())
        file.write (reinterpret_cast<const char*> (bytes.data()), static_cast<std::streamsize> (bytes.size()));
    file.close();
    return static_cast<bool> (file);
}

inline std::filesystem::path sidecarTempPath (const std::filesystem::path& packagePath,
                                              std::string_view label,
                                              std::size_t index,
                                              std::string_view extension)
{
    const std::filesystem::path parent = packagePath.has_parent_path()
        ? packagePath.parent_path()
        : std::filesystem::current_path();
    return parent / (packagePath.filename().string() + "." + std::string (label) + "." + std::to_string (index)
                     + std::string (extension));
}

inline bool sampleRateToInt (engine::SampleRate sampleRate, int& out) noexcept
{
    if (! sampleRate.isValid())
        return false;

    const double rounded = std::round (sampleRate.hz);
    if (std::fabs (sampleRate.hz - rounded) > 0.0
        || rounded <= 0.0
        || rounded > static_cast<double> (std::numeric_limits<int>::max()))
        return false;

    out = static_cast<int> (rounded);
    return true;
}

inline const engine::DecodedAssetAudio* findDecodedAsset (std::span<const engine::DecodedAssetAudio> assets,
                                                          engine::EntityId assetId) noexcept
{
    for (const engine::DecodedAssetAudio& audio : assets)
        if (audio.assetId == assetId)
            return &audio;

    return nullptr;
}

inline bool decodedAssetMatches (const engine::DecodedAssetAudio& decoded, const engine::Asset& asset) noexcept
{
    if (decoded.assetId != asset.id
        || decoded.sampleRate != asset.sampleRate
        || decoded.frames != asset.frames
        || decoded.channels != asset.channels
        || decoded.channels == 0)
        return false;

    const std::uint64_t sampleCount = decoded.frames * static_cast<std::uint64_t> (decoded.channels);
    if (sampleCount > static_cast<std::uint64_t> (std::numeric_limits<std::size_t>::max())
        || decoded.interleavedSamples.size() != static_cast<std::size_t> (sampleCount))
        return false;

    for (const float sample : decoded.interleavedSamples)
        if (! std::isfinite (sample))
            return false;

    return true;
}

inline bool tickAsFrame (engine::Tick tick, std::uint64_t& out) noexcept
{
    if (tick < 0)
        return false;

    out = static_cast<std::uint64_t> (tick);
    return true;
}

inline bool sampleLockedSeconds (engine::Tick tick, engine::SampleRate sampleRate, double& out) noexcept
{
    std::uint64_t frames = 0;
    if (! tickAsFrame (tick, frames))
        return false;

    const DoubleResult seconds = framesToSeconds (frames, sampleRate);
    if (! seconds.ok())
        return false;

    out = seconds.value;
    return true;
}

inline bool timeValue (engine::TimeBase timeBase, engine::Tick tick, engine::SampleRate sampleRate, double& out) noexcept
{
    if (timeBase == engine::TimeBase::TempoLocked)
    {
        const DoubleResult beats = ticksToBeats (tick);
        if (! beats.ok())
            return false;

        out = beats.value;
        return true;
    }

    if (timeBase == engine::TimeBase::SampleLocked)
        return sampleLockedSeconds (tick, sampleRate, out);

    return false;
}

inline std::string timeUnitFor (engine::TimeBase timeBase)
{
    return timeBase == engine::TimeBase::TempoLocked ? "beats" : "seconds";
}

inline PackageResult makeId (std::string_view kind, engine::EntityId id, std::string& out)
{
    const StringResult result = xmlIdFor (kind, id);
    if (! result.ok())
        return fail (PackageStatus::InvalidProject, "failed to make XML id");

    out = result.value;
    return {};
}

inline PackageResult makeParameterId (std::string_view kind, engine::EntityId id, std::string_view role, std::string& out)
{
    const StringResult result = parameterXmlIdFor (kind, id, role);
    if (! result.ok())
        return fail (PackageStatus::InvalidProject, "failed to make XML parameter id");

    out = result.value;
    return {};
}

inline PackageResult addXmlId (std::vector<std::string>& ids, const std::string& id)
{
    if (id.empty())
        return fail (PackageStatus::InvalidProject, "empty XML id");

    if (std::find (ids.begin(), ids.end(), id) != ids.end())
        return fail (PackageStatus::DuplicateXmlId, "duplicate mapped XML id");

    ids.push_back (id);
    return {};
}

inline PackageResult makeTrackedId (std::vector<std::string>& ids,
                                    std::string_view kind,
                                    engine::EntityId entity,
                                    std::string& out)
{
    PackageResult result = makeId (kind, entity, out);
    if (! result.ok())
        return result;

    return addXmlId (ids, out);
}

inline PackageResult makeTrackedParameterId (std::vector<std::string>& ids,
                                             std::string_view kind,
                                             engine::EntityId entity,
                                             std::string_view role,
                                             std::string& out)
{
    PackageResult result = makeParameterId (kind, entity, role, out);
    if (! result.ok())
        return result;

    return addXmlId (ids, out);
}

inline bool mediaPathAlreadyAdded (std::span<const storedzip::Entry> entries, std::string_view path) noexcept
{
    return storedzip::findEntry (entries, path) != nullptr;
}

inline PackageResult addWavEntry (std::vector<storedzip::Entry>& entries,
                                  const std::filesystem::path& packagePath,
                                  const engine::Asset& asset,
                                  const engine::DecodedAssetAudio& decoded)
{
    const std::string mediaPath = assetAudioPathFor (asset.contentHash);
    if (mediaPathAlreadyAdded (std::span<const storedzip::Entry> (entries.data(), entries.size()), mediaPath))
        return {};

    const std::filesystem::path wavPath = sidecarTempPath (packagePath, "asset", entries.size(), ".wav");
    const io::WavResult writeResult = io::writeFloat32WavFile (wavPath,
                                                               decoded.sampleRate,
                                                               decoded.channels,
                                                               decoded.frames,
                                                               decoded.interleavedSamples);
    if (! writeResult.ok())
        return fail (PackageStatus::PackageWriteFailed, "failed to write temporary WAV: " + writeResult.message);

    std::vector<std::uint8_t> wavBytes;
    const bool readOk = readFileBytes (wavPath, wavBytes);
    std::error_code ec;
    std::filesystem::remove (wavPath, ec);
    if (! readOk)
        return fail (PackageStatus::PackageWriteFailed, "failed to read temporary WAV bytes");

    entries.push_back ({ mediaPath, std::move (wavBytes) });
    return {};
}

inline bool getAttr (std::string_view tag, std::string_view name, std::string& out)
{
    const std::string needle = std::string (name) + "=\"";
    const std::size_t pos = tag.find (needle);
    if (pos == std::string_view::npos)
        return false;

    const std::size_t valueStart = pos + needle.size();
    const std::size_t valueEnd = tag.find ('"', valueStart);
    if (valueEnd == std::string_view::npos)
        return false;

    out = std::string (tag.substr (valueStart, valueEnd - valueStart));
    return true;
}

inline bool getAttrDouble (std::string_view tag, std::string_view name, double& out)
{
    std::string text;
    return getAttr (tag, name, text) && parseDouble (text, out);
}

inline bool getAttrInt (std::string_view tag, std::string_view name, int& out)
{
    std::string text;
    return getAttr (tag, name, text) && parseInt (text, out);
}

inline std::size_t findStartTag (std::string_view xml, std::string_view name, std::size_t start) noexcept
{
    const std::string needle = "<" + std::string (name);
    std::size_t pos = start;
    while ((pos = xml.find (needle, pos)) != std::string_view::npos)
    {
        const std::size_t afterName = pos + needle.size();
        if (afterName < xml.size()
            && (xml[afterName] == ' ' || xml[afterName] == '>' || xml[afterName] == '/'))
            return pos;

        ++pos;
    }

    return std::string_view::npos;
}

inline bool extractElement (std::string_view xml,
                            std::string_view name,
                            std::size_t start,
                            std::string& startTag,
                            std::string& body,
                            std::size_t& next)
{
    const std::size_t tagStart = findStartTag (xml, name, start);
    if (tagStart == std::string_view::npos)
        return false;

    const std::size_t tagEnd = xml.find ('>', tagStart);
    if (tagEnd == std::string_view::npos)
        return false;

    startTag = std::string (xml.substr (tagStart, tagEnd - tagStart + 1u));
    if (tagEnd > tagStart && xml[tagEnd - 1u] == '/')
    {
        body.clear();
        next = tagEnd + 1u;
        return true;
    }

    const std::string close = "</" + std::string (name) + ">";
    const std::size_t closeStart = xml.find (close, tagEnd + 1u);
    if (closeStart == std::string_view::npos)
        return false;

    body = std::string (xml.substr (tagEnd + 1u, closeStart - tagEnd - 1u));
    next = closeStart + close.size();
    return true;
}

inline bool collectXmlIds (std::string_view xml, std::vector<std::string>& out)
{
    std::size_t pos = 0;
    while ((pos = xml.find (" id=\"", pos)) != std::string_view::npos)
    {
        pos += 5u;
        const std::size_t end = xml.find ('"', pos);
        if (end == std::string_view::npos)
            return false;

        const std::string id (xml.substr (pos, end - pos));
        if (std::find (out.begin(), out.end(), id) != out.end())
            return false;

        out.push_back (id);
        pos = end + 1u;
    }

    return true;
}

inline bool nearlyEqual (double a, double b, double tolerance = 1.0e-9) noexcept
{
    return std::fabs (a - b) <= tolerance;
}

inline const TrackSummary* findTrack (std::span<const TrackSummary> tracks, std::string_view id) noexcept
{
    for (const TrackSummary& track : tracks)
        if (track.id == id)
            return &track;

    return nullptr;
}

inline const AudioClipSummary* findAudioClip (std::span<const AudioClipSummary> clips, std::string_view trackId) noexcept
{
    for (const AudioClipSummary& clip : clips)
        if (clip.trackId == trackId)
            return &clip;

    return nullptr;
}

inline const MediaSummary* findMedia (std::span<const MediaSummary> media, std::string_view path) noexcept
{
    for (const MediaSummary& item : media)
        if (item.path == path)
            return &item;

    return nullptr;
}

inline bool midiTrackAlreadySeen (std::span<const engine::EntityId> tracks, engine::EntityId id) noexcept
{
    for (const engine::EntityId track : tracks)
        if (track == id)
            return true;

    return false;
}

} // namespace detail

[[nodiscard]] inline PackageResult exportProjectToDawproject (const std::filesystem::path& packagePath,
                                                              const engine::Project& project,
                                                              std::span<const engine::DecodedAssetAudio> decodedAssets)
{
    if (! project.hasValidAssetClipIndirection())
        return detail::fail (PackageStatus::InvalidProject, "Project Asset/Clip indirection is invalid");

    std::vector<std::string> xmlIds;
    std::vector<storedzip::Entry> entries;

    std::ostringstream projectXml;
    // Locale-independent: integers inserted directly (sampleRate, channels, key, meter, ...) must never
    // pick up the ambient locale's thousands separator (e.g. "48.000"), and formatDouble already imbues
    // classic for its own radix. Without this, a grouping locale yields XML the reader cannot parse.
    projectXml.imbue (std::locale::classic());
    projectXml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    projectXml << "<Project version=\"1.0\">\n";
    projectXml << "  <Application name=\"YES DAW\" version=\"" << YESDAW_VERSION_STRING << "\"/>\n";

    const engine::TempoChange tempo = project.tempoMap.empty() ? engine::TempoChange {} : project.tempoMap.front();
    if (tempo.tick != 0 || ! tempo.hasValidBpm())
        return detail::fail (PackageStatus::InvalidTimeline, "tempo map must start with a finite tempo at tick 0");
    const engine::MeterChange meter = project.meterMap.empty() ? engine::MeterChange {} : project.meterMap.front();
    if (meter.tick != 0 || ! meter.isValid())
        return detail::fail (PackageStatus::InvalidTimeline, "meter map must start with a valid meter at tick 0");

    std::string tempoId;
    std::string meterId;
    if (PackageResult result = detail::makeTrackedParameterId (xmlIds, "project", project.id, "tempo", tempoId); ! result.ok())
        return result;
    if (PackageResult result = detail::makeTrackedParameterId (xmlIds, "project", project.id, "meter", meterId); ! result.ok())
        return result;

    projectXml << "  <Transport>\n";
    projectXml << "    <Tempo id=\"" << tempoId << "\" unit=\"bpm\" value=\"" << detail::formatDouble (tempo.bpm) << "\"/>\n";
    projectXml << "    <TimeSignature id=\"" << meterId << "\" numerator=\"" << meter.numerator
               << "\" denominator=\"" << meter.denominator << "\"/>\n";
    projectXml << "  </Transport>\n";

    std::string masterTrackId;
    std::string masterChannelId;
    std::string masterVolumeId;
    std::string masterPanId;
    if (PackageResult result = detail::makeTrackedId (xmlIds, "master_track", project.id, masterTrackId); ! result.ok())
        return result;
    if (PackageResult result = detail::makeTrackedId (xmlIds, "master_channel", project.id, masterChannelId); ! result.ok())
        return result;
    if (PackageResult result = detail::makeTrackedParameterId (xmlIds, "master_channel", project.id, "volume", masterVolumeId); ! result.ok())
        return result;
    if (PackageResult result = detail::makeTrackedParameterId (xmlIds, "master_channel", project.id, "pan", masterPanId); ! result.ok())
        return result;

    projectXml << "  <Structure>\n";
    projectXml << "    <Track id=\"" << masterTrackId << "\" name=\"Master\" contentType=\"audio\" loaded=\"true\">\n";
    projectXml << "      <Channel id=\"" << masterChannelId << "\" audioChannels=\"2\" role=\"master\">\n";
    projectXml << "        <Pan id=\"" << masterPanId << "\" unit=\"normalized\" value=\"0.5\"/>\n";
    projectXml << "        <Volume id=\"" << masterVolumeId << "\" unit=\"linear\" value=\"1\"/>\n";
    projectXml << "      </Channel>\n";
    projectXml << "    </Track>\n";

    for (const engine::Clip& clip : project.clips)
    {
        if (! engine::detail::clipEditMetadataIsStorageSafe (clip))
            return detail::fail (PackageStatus::InvalidTimeline, "audio Clip metadata is invalid");
        if (clip.timeBase != engine::TimeBase::SampleLocked)
            return detail::fail (PackageStatus::UnsupportedTimeBase, "tempo-locked audio Clips are unsupported");
        if (! std::isfinite (clip.gain))
            return detail::fail (PackageStatus::NonFiniteValue, "audio Clip gain is non-finite");

        const engine::Asset* const asset = project.findAsset (clip.assetId);
        if (asset == nullptr)
            return detail::fail (PackageStatus::InvalidProject, "audio Clip references a missing Asset");
        if (asset->channels == 0u || asset->channels > 2u)
            return detail::fail (PackageStatus::UnsupportedAssetChannels, "DAWproject H10 exports mono/stereo audio only");

        const engine::DecodedAssetAudio* const decoded = detail::findDecodedAsset (decodedAssets, asset->id);
        if (decoded == nullptr)
            return detail::fail (PackageStatus::MissingAssetAudio, "decoded Asset audio is missing");
        if (! detail::decodedAssetMatches (*decoded, *asset))
            return detail::fail (PackageStatus::AssetMetadataMismatch, "decoded Asset metadata does not match Project Asset");

        int sampleRate = 0;
        if (! detail::sampleRateToInt (asset->sampleRate, sampleRate))
            return detail::fail (PackageStatus::AssetMetadataMismatch, "Asset sample rate must be a finite integer");

        std::string trackId;
        std::string channelId;
        std::string volumeId;
        std::string panId;
        if (PackageResult result = detail::makeTrackedId (xmlIds, "track", clip.id, trackId); ! result.ok())
            return result;
        if (PackageResult result = detail::makeTrackedId (xmlIds, "channel", clip.id, channelId); ! result.ok())
            return result;
        if (PackageResult result = detail::makeTrackedParameterId (xmlIds, "channel", clip.id, "volume", volumeId); ! result.ok())
            return result;
        if (PackageResult result = detail::makeTrackedParameterId (xmlIds, "channel", clip.id, "pan", panId); ! result.ok())
            return result;

        projectXml << "    <Track id=\"" << trackId << "\" name=\"Audio Clip\" contentType=\"audio\" loaded=\"true\">\n";
        projectXml << "      <Channel id=\"" << channelId << "\" audioChannels=\"" << asset->channels
                   << "\" destination=\"" << masterChannelId << "\" role=\"regular\">\n";
        projectXml << "        <Pan id=\"" << panId << "\" unit=\"normalized\" value=\"0.5\"/>\n";
        projectXml << "        <Volume id=\"" << volumeId << "\" unit=\"linear\" value=\""
                   << detail::formatDouble (static_cast<double> (clip.gain)) << "\"/>\n";
        projectXml << "      </Channel>\n";
        projectXml << "    </Track>\n";

        if (PackageResult result = detail::addWavEntry (entries, packagePath, *asset, *decoded); ! result.ok())
            return result;
        (void) sampleRate;
    }

    std::vector<engine::EntityId> midiTrackIds;
    for (const engine::MidiClip& midiClip : project.midiClips)
    {
        if (! midiClip.isValid())
            return detail::fail (PackageStatus::InvalidProject, "MIDI Clip is invalid");
        if (detail::midiTrackAlreadySeen (std::span<const engine::EntityId> (midiTrackIds.data(), midiTrackIds.size()),
                                          midiClip.trackId))
            continue;

        midiTrackIds.push_back (midiClip.trackId);

        std::string trackId;
        std::string channelId;
        std::string volumeId;
        std::string panId;
        if (PackageResult result = detail::makeTrackedId (xmlIds, "track", midiClip.trackId, trackId); ! result.ok())
            return result;
        if (PackageResult result = detail::makeTrackedId (xmlIds, "channel", midiClip.trackId, channelId); ! result.ok())
            return result;
        if (PackageResult result = detail::makeTrackedParameterId (xmlIds, "channel", midiClip.trackId, "volume", volumeId); ! result.ok())
            return result;
        if (PackageResult result = detail::makeTrackedParameterId (xmlIds, "channel", midiClip.trackId, "pan", panId); ! result.ok())
            return result;

        projectXml << "    <Track id=\"" << trackId << "\" name=\"MIDI Track\" contentType=\"notes\" loaded=\"true\">\n";
        projectXml << "      <Channel id=\"" << channelId << "\" audioChannels=\"2\" destination=\"" << masterChannelId
                   << "\" role=\"regular\">\n";
        projectXml << "        <Pan id=\"" << panId << "\" unit=\"normalized\" value=\"0.5\"/>\n";
        projectXml << "        <Volume id=\"" << volumeId << "\" unit=\"linear\" value=\"1\"/>\n";
        projectXml << "      </Channel>\n";
        projectXml << "    </Track>\n";
    }
    projectXml << "  </Structure>\n";

    std::string arrangementId;
    std::string lanesId;
    if (PackageResult result = detail::makeTrackedId (xmlIds, "arrangement", project.id, arrangementId); ! result.ok())
        return result;
    if (PackageResult result = detail::makeTrackedId (xmlIds, "lanes", project.id, lanesId); ! result.ok())
        return result;

    projectXml << "  <Arrangement id=\"" << arrangementId << "\" name=\"Arrangement\">\n";
    projectXml << "    <Lanes id=\"" << lanesId << "\">\n";

    for (const engine::Clip& clip : project.clips)
    {
        const engine::Asset* const asset = project.findAsset (clip.assetId);
        if (asset == nullptr)
            return detail::fail (PackageStatus::InvalidProject, "audio Clip references a missing Asset");

        double time = 0.0;
        double duration = 0.0;
        double playStart = 0.0;
        double playStop = 0.0;
        double fadeIn = 0.0;
        double fadeOut = 0.0;
        if (clip.srcLen > std::numeric_limits<std::uint64_t>::max() - clip.srcOffset)
            return detail::fail (PackageStatus::InvalidTimeline, "audio Clip source window overflows");
        const std::uint64_t sourceStopFrame = clip.srcOffset + clip.srcLen;
        const DoubleResult sourceStopSeconds = framesToSeconds (sourceStopFrame, asset->sampleRate);
        if (! detail::sampleLockedSeconds (clip.timelineStart, project.sampleRate, time)
            || ! detail::sampleLockedSeconds (clip.timelineLength, project.sampleRate, duration)
            || ! detail::sampleLockedSeconds (static_cast<engine::Tick> (clip.srcOffset), asset->sampleRate, playStart)
            || ! sourceStopSeconds.ok()
            || ! detail::sampleLockedSeconds (clip.fadeIn, project.sampleRate, fadeIn)
            || ! detail::sampleLockedSeconds (clip.fadeOut, project.sampleRate, fadeOut))
        {
            return detail::fail (PackageStatus::InvalidTimeline, "audio Clip timing is invalid");
        }
        playStop = sourceStopSeconds.value;

        double mediaDuration = 0.0;
        if (! detail::sampleLockedSeconds (static_cast<engine::Tick> (asset->frames), asset->sampleRate, mediaDuration))
            return detail::fail (PackageStatus::InvalidTimeline, "Asset duration is invalid");

        int sampleRate = 0;
        if (! detail::sampleRateToInt (asset->sampleRate, sampleRate))
            return detail::fail (PackageStatus::AssetMetadataMismatch, "Asset sample rate must be a finite integer");

        std::string trackId;
        std::string clipsId;
        std::string audioId;
        if (PackageResult result = detail::makeId ("track", clip.id, trackId); ! result.ok())
            return result;
        if (PackageResult result = detail::makeTrackedId (xmlIds, "clips", clip.id, clipsId); ! result.ok())
            return result;
        if (PackageResult result = detail::makeTrackedId (xmlIds, "audio", clip.id, audioId); ! result.ok())
            return result;

        projectXml << "      <Clips id=\"" << clipsId << "\" track=\"" << trackId << "\" timeUnit=\"seconds\">\n";
        projectXml << "        <Clip time=\"" << detail::formatDouble (time)
                   << "\" duration=\"" << detail::formatDouble (duration)
                   << "\" contentTimeUnit=\"seconds\" playStart=\"" << detail::formatDouble (playStart)
                   << "\" playStop=\"" << detail::formatDouble (playStop)
                   << "\" fadeTimeUnit=\"seconds\" fadeInTime=\"" << detail::formatDouble (fadeIn)
                   << "\" fadeOutTime=\"" << detail::formatDouble (fadeOut) << "\">\n";
        projectXml << "          <Audio id=\"" << audioId << "\" timeUnit=\"seconds\" duration=\""
                   << detail::formatDouble (mediaDuration) << "\" channels=\"" << asset->channels
                   << "\" sampleRate=\"" << sampleRate << "\">\n";
        projectXml << "            <File path=\"" << assetAudioPathFor (asset->contentHash) << "\"/>\n";
        projectXml << "          </Audio>\n";
        projectXml << "        </Clip>\n";
        projectXml << "      </Clips>\n";
    }

    for (const engine::EntityId trackEntityId : midiTrackIds)
    {
        std::string trackId;
        std::string clipsId;
        if (PackageResult result = detail::makeId ("track", trackEntityId, trackId); ! result.ok())
            return result;
        if (PackageResult result = detail::makeTrackedId (xmlIds, "midi_clips", trackEntityId, clipsId); ! result.ok())
            return result;

        projectXml << "      <Clips id=\"" << clipsId << "\" track=\"" << trackId << "\" timeUnit=\"beats\">\n";
        for (const engine::MidiClip& midiClip : project.midiClips)
        {
            if (midiClip.trackId != trackEntityId)
                continue;

            double clipTime = 0.0;
            double clipDuration = 0.0;
            if (! detail::timeValue (midiClip.timeBase, midiClip.timelineStart, project.sampleRate, clipTime)
                || ! detail::timeValue (midiClip.timeBase, midiClip.timelineLength, project.sampleRate, clipDuration))
            {
                return detail::fail (PackageStatus::InvalidTimeline, "MIDI Clip timing is invalid");
            }

            std::string notesId;
            if (PackageResult result = detail::makeTrackedId (xmlIds, "notes", midiClip.id, notesId); ! result.ok())
                return result;

            const std::string unit = detail::timeUnitFor (midiClip.timeBase);
            projectXml << "        <Clip time=\"" << detail::formatDouble (clipTime)
                       << "\" duration=\"" << detail::formatDouble (clipDuration)
                       << "\" contentTimeUnit=\"" << unit << "\">\n";
            projectXml << "          <Notes id=\"" << notesId << "\" timeUnit=\"" << unit << "\" track=\"" << trackId << "\">\n";
            for (const engine::Note& note : midiClip.notes)
            {
                double noteTime = 0.0;
                double noteDuration = 0.0;
                if (! detail::timeValue (midiClip.timeBase, note.startTick, project.sampleRate, noteTime)
                    || ! detail::timeValue (midiClip.timeBase, note.lengthTicks, project.sampleRate, noteDuration))
                {
                    return detail::fail (PackageStatus::InvalidTimeline, "MIDI Note timing is invalid");
                }

                // -1 is a legal internal "unassigned" sentinel, but DAWproject channel is 0..15. Per
                // ADR-0029, a lossy case fails explicitly instead of silently coercing it to channel 0.
                if (note.channel < 0)
                    return detail::fail (PackageStatus::InvalidProject,
                                         "MIDI Note channel is unassigned (-1); allocate a 0..15 channel before DAWproject export");
                projectXml << "            <Note time=\"" << detail::formatDouble (noteTime)
                           << "\" duration=\"" << detail::formatDouble (noteDuration)
                           << "\" channel=\"" << note.channel
                           << "\" key=\"" << note.key
                           << "\" vel=\"" << detail::formatDouble (note.normalizedVelocity) << "\"/>\n";
            }
            projectXml << "          </Notes>\n";
            projectXml << "        </Clip>\n";
        }
        projectXml << "      </Clips>\n";
    }

    projectXml << "    </Lanes>\n";
    projectXml << "  </Arrangement>\n";
    projectXml << "</Project>\n";

    std::vector<storedzip::Entry> packageEntries;
    packageEntries.push_back ({ "project.xml", detail::bytesForString (projectXml.str()) });
    packageEntries.push_back ({ "metadata.xml", detail::bytesForString ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<MetaData>\n  <Comment>YES DAW H10 export</Comment>\n</MetaData>\n") });
    packageEntries.insert (packageEntries.end(), entries.begin(), entries.end());

    if (const storedzip::ZipResult result = storedzip::writeStoredZip (packagePath,
                                                                       std::span<const storedzip::Entry> (packageEntries.data(),
                                                                                                          packageEntries.size()));
        ! result.ok())
    {
        return detail::fail (PackageStatus::PackageWriteFailed, result.message);
    }

    return {};
}

[[nodiscard]] inline ReadResult readDawprojectSummary (const std::filesystem::path& packagePath)
{
    storedzip::ReadResult package = storedzip::readStoredZip (packagePath);
    if (! package.ok())
        return detail::readFail (PackageStatus::PackageReadFailed, package.message);

    const storedzip::Entry* const projectXmlEntry =
        storedzip::findEntry (std::span<const storedzip::Entry> (package.entries.data(), package.entries.size()), "project.xml");
    const storedzip::Entry* const metadataEntry =
        storedzip::findEntry (std::span<const storedzip::Entry> (package.entries.data(), package.entries.size()), "metadata.xml");
    if (projectXmlEntry == nullptr || metadataEntry == nullptr)
        return detail::readFail (PackageStatus::MissingPackageEntry, "DAWproject is missing project.xml or metadata.xml");

    const std::string xml (projectXmlEntry->bytes.begin(), projectXmlEntry->bytes.end());
    ProjectSummary summary;
    if (! detail::collectXmlIds (xml, summary.xmlIds))
        return detail::readFail (PackageStatus::MalformedXml, "XML ids are malformed or duplicated");

    std::string transportTag;
    std::string transportBody;
    std::size_t next = 0;
    if (detail::extractElement (xml, "Transport", 0, transportTag, transportBody, next))
    {
        std::string tempoTag;
        std::string tempoBody;
        std::size_t tempoNext = 0;
        if (detail::extractElement (transportBody, "Tempo", 0, tempoTag, tempoBody, tempoNext)
            && ! detail::getAttrDouble (tempoTag, "value", summary.tempoBpm))
        {
            return detail::readFail (PackageStatus::MalformedXml, "Tempo value is malformed");
        }

        std::string meterTag;
        std::string meterBody;
        std::size_t meterNext = 0;
        if (detail::extractElement (transportBody, "TimeSignature", 0, meterTag, meterBody, meterNext)
            && (! detail::getAttrInt (meterTag, "numerator", summary.meterNumerator)
                || ! detail::getAttrInt (meterTag, "denominator", summary.meterDenominator)))
        {
            return detail::readFail (PackageStatus::MalformedXml, "TimeSignature is malformed");
        }
    }

    std::string structureTag;
    std::string structureBody;
    if (! detail::extractElement (xml, "Structure", 0, structureTag, structureBody, next))
        return detail::readFail (PackageStatus::MalformedXml, "Structure is missing");

    std::size_t trackPos = 0;
    while (true)
    {
        std::string trackTag;
        std::string trackBody;
        std::size_t trackNext = 0;
        if (! detail::extractElement (structureBody, "Track", trackPos, trackTag, trackBody, trackNext))
            break;

        TrackSummary track;
        if (! detail::getAttr (trackTag, "id", track.id))
            return detail::readFail (PackageStatus::MalformedXml, "Track id is missing");
        (void) detail::getAttr (trackTag, "contentType", track.contentType);

        std::string channelTag;
        std::string channelBody;
        std::size_t channelNext = 0;
        if (! detail::extractElement (trackBody, "Channel", 0, channelTag, channelBody, channelNext))
            return detail::readFail (PackageStatus::MalformedXml, "Track Channel is missing");
        if (! detail::getAttrInt (channelTag, "audioChannels", track.audioChannels)
            || ! detail::getAttr (channelTag, "role", track.role))
        {
            return detail::readFail (PackageStatus::MalformedXml, "Channel metadata is malformed");
        }
        (void) detail::getAttr (channelTag, "destination", track.destination);

        std::string volumeTag;
        std::string volumeBody;
        std::size_t volumeNext = 0;
        if (detail::extractElement (channelBody, "Volume", 0, volumeTag, volumeBody, volumeNext)
            && ! detail::getAttrDouble (volumeTag, "value", track.volume))
        {
            return detail::readFail (PackageStatus::MalformedXml, "Volume value is malformed");
        }

        std::string panTag;
        std::string panBody;
        std::size_t panNext = 0;
        if (detail::extractElement (channelBody, "Pan", 0, panTag, panBody, panNext)
            && ! detail::getAttrDouble (panTag, "value", track.pan))
        {
            return detail::readFail (PackageStatus::MalformedXml, "Pan value is malformed");
        }

        summary.tracks.push_back (std::move (track));
        trackPos = trackNext;
    }

    std::string arrangementTag;
    std::string arrangementBody;
    if (! detail::extractElement (xml, "Arrangement", 0, arrangementTag, arrangementBody, next))
        return detail::readFail (PackageStatus::MalformedXml, "Arrangement is missing");

    std::string lanesTag;
    std::string lanesBody;
    if (! detail::extractElement (arrangementBody, "Lanes", 0, lanesTag, lanesBody, next))
        return detail::readFail (PackageStatus::MalformedXml, "Arrangement Lanes are missing");

    std::size_t clipsPos = 0;
    while (true)
    {
        std::string clipsTag;
        std::string clipsBody;
        std::size_t clipsNext = 0;
        if (! detail::extractElement (lanesBody, "Clips", clipsPos, clipsTag, clipsBody, clipsNext))
            break;

        std::string trackId;
        std::string clipsTimeUnit;
        if (! detail::getAttr (clipsTag, "track", trackId) || ! detail::getAttr (clipsTag, "timeUnit", clipsTimeUnit))
            return detail::readFail (PackageStatus::MalformedXml, "Clips track/timeUnit is missing");

        std::size_t clipPos = 0;
        while (true)
        {
            std::string clipTag;
            std::string clipBody;
            std::size_t clipNext = 0;
            if (! detail::extractElement (clipsBody, "Clip", clipPos, clipTag, clipBody, clipNext))
                break;

            std::string audioTag;
            std::string audioBody;
            std::size_t audioNext = 0;
            if (detail::extractElement (clipBody, "Audio", 0, audioTag, audioBody, audioNext))
            {
                AudioClipSummary clip;
                clip.trackId = trackId;
                clip.timeUnit = clipsTimeUnit;
                if (! detail::getAttrDouble (clipTag, "time", clip.time)
                    || ! detail::getAttrDouble (clipTag, "duration", clip.duration)
                    || ! detail::getAttrDouble (clipTag, "playStart", clip.playStart)
                    || ! detail::getAttrDouble (clipTag, "playStop", clip.playStop)
                    || ! detail::getAttrDouble (clipTag, "fadeInTime", clip.fadeIn)
                    || ! detail::getAttrDouble (clipTag, "fadeOutTime", clip.fadeOut)
                    || ! detail::getAttrDouble (audioTag, "duration", clip.mediaDuration)
                    || ! detail::getAttrInt (audioTag, "channels", clip.audioChannels)
                    || ! detail::getAttrInt (audioTag, "sampleRate", clip.sampleRate))
                {
                    return detail::readFail (PackageStatus::MalformedXml, "Audio Clip timing or media metadata is malformed");
                }
                if (clip.time < 0.0 || clip.duration < 0.0 || clip.playStart < 0.0 || clip.playStop < clip.playStart
                    || clip.fadeIn < 0.0 || clip.fadeOut < 0.0 || clip.mediaDuration <= 0.0)
                {
                    return detail::readFail (PackageStatus::MalformedXml, "Audio Clip timing is negative");
                }

                std::string fileTag;
                std::string fileBody;
                std::size_t fileNext = 0;
                if (! detail::extractElement (audioBody, "File", 0, fileTag, fileBody, fileNext)
                    || ! detail::getAttr (fileTag, "path", clip.mediaPath))
                {
                    return detail::readFail (PackageStatus::MalformedXml, "Audio File path is missing");
                }

                summary.audioClips.push_back (std::move (clip));
            }
            else
            {
                std::string notesTag;
                std::string notesBody;
                std::size_t notesNext = 0;
                if (! detail::extractElement (clipBody, "Notes", 0, notesTag, notesBody, notesNext))
                    return detail::readFail (PackageStatus::MalformedXml, "Clip has neither Audio nor Notes");

                MidiClipSummary midiClip;
                midiClip.trackId = trackId;
                if (! detail::getAttr (notesTag, "timeUnit", midiClip.timeUnit)
                    || ! detail::getAttrDouble (clipTag, "time", midiClip.time)
                    || ! detail::getAttrDouble (clipTag, "duration", midiClip.duration))
                {
                    return detail::readFail (PackageStatus::MalformedXml, "MIDI Clip timing is malformed");
                }
                if (midiClip.time < 0.0 || midiClip.duration < 0.0)
                    return detail::readFail (PackageStatus::MalformedXml, "MIDI Clip timing is negative");

                std::size_t notePos = 0;
                while (true)
                {
                    const std::size_t noteStart = detail::findStartTag (notesBody, "Note", notePos);
                    if (noteStart == std::string_view::npos)
                        break;
                    const std::size_t noteEnd = notesBody.find ('>', noteStart);
                    if (noteEnd == std::string::npos)
                        return detail::readFail (PackageStatus::MalformedXml, "MIDI Note is truncated");

                    const std::string noteTag = notesBody.substr (noteStart, noteEnd - noteStart + 1u);
                    NoteSummary note;
                    if (! detail::getAttrDouble (noteTag, "time", note.time)
                        || ! detail::getAttrDouble (noteTag, "duration", note.duration)
                        || ! detail::getAttrInt (noteTag, "channel", note.channel)
                        || ! detail::getAttrInt (noteTag, "key", note.key)
                        || ! detail::getAttrDouble (noteTag, "vel", note.velocity))
                    {
                        return detail::readFail (PackageStatus::MalformedXml, "MIDI Note value is malformed");
                    }
                    if (note.time < 0.0 || note.duration < 0.0 || note.channel < 0 || note.channel > 15
                        || note.key < 0 || note.key > 127 || note.velocity < 0.0 || note.velocity > 1.0)
                    {
                        return detail::readFail (PackageStatus::MalformedXml, "MIDI Note value is out of range");
                    }

                    midiClip.notes.push_back (note);
                    notePos = noteEnd + 1u;
                }

                summary.midiClips.push_back (std::move (midiClip));
            }

            clipPos = clipNext;
        }

        clipsPos = clipsNext;
    }

    std::vector<std::string> decodedMediaPaths;
    for (const AudioClipSummary& clip : summary.audioClips)
    {
        if (std::find (decodedMediaPaths.begin(), decodedMediaPaths.end(), clip.mediaPath) != decodedMediaPaths.end())
            continue;

        const storedzip::Entry* const mediaEntry =
            storedzip::findEntry (std::span<const storedzip::Entry> (package.entries.data(), package.entries.size()), clip.mediaPath);
        if (mediaEntry == nullptr)
            return detail::readFail (PackageStatus::MissingMedia, "referenced media entry is missing");

        const std::filesystem::path wavPath = detail::sidecarTempPath (packagePath, "read", decodedMediaPaths.size(), ".wav");
        if (! detail::writeFileBytes (wavPath, std::span<const std::uint8_t> (mediaEntry->bytes.data(), mediaEntry->bytes.size())))
            return detail::readFail (PackageStatus::MediaDecodeFailed, "failed to write temporary media for decode");

        io::Float32Wav wav;
        const io::WavResult wavResult = io::readFloat32WavFile (wavPath, wav);
        std::error_code ec;
        std::filesystem::remove (wavPath, ec);
        if (! wavResult.ok())
            return detail::readFail (PackageStatus::MediaDecodeFailed, "failed to decode media WAV: " + wavResult.message);

        int wavSampleRate = 0;
        if (! detail::sampleRateToInt (wav.sampleRate, wavSampleRate)
            || wavSampleRate != clip.sampleRate
            || wav.channels != static_cast<std::uint16_t> (clip.audioChannels))
        {
            return detail::readFail (PackageStatus::WrongMediaMetadata, "media WAV metadata does not match XML");
        }

        const double wavDuration = static_cast<double> (wav.frames) / wav.sampleRate.hz;
        if (! detail::nearlyEqual (wavDuration, clip.mediaDuration))
            return detail::readFail (PackageStatus::WrongMediaMetadata, "media WAV duration does not match XML");

        summary.media.push_back ({ clip.mediaPath,
                                   wav.sampleRate,
                                   wav.channels,
                                   wav.frames,
                                   std::move (wav.interleavedSamples) });
        decodedMediaPaths.push_back (clip.mediaPath);
    }

    return { PackageStatus::Ok, {}, std::move (summary) };
}

[[nodiscard]] inline PackageResult verifyDawprojectPackageMatches (const std::filesystem::path& packagePath,
                                                                   const engine::Project& project,
                                                                   std::span<const engine::DecodedAssetAudio> decodedAssets)
{
    ReadResult read = readDawprojectSummary (packagePath);
    if (! read.ok())
        return { read.status, read.message };

    const engine::TempoChange tempo = project.tempoMap.empty() ? engine::TempoChange {} : project.tempoMap.front();
    const engine::MeterChange meter = project.meterMap.empty() ? engine::MeterChange {} : project.meterMap.front();
    if (! detail::nearlyEqual (read.summary.tempoBpm, tempo.bpm)
        || read.summary.meterNumerator != static_cast<int> (meter.numerator)
        || read.summary.meterDenominator != static_cast<int> (meter.denominator))
    {
        return detail::fail (PackageStatus::SummaryMismatch, "tempo or meter summary does not match Project");
    }

    std::vector<engine::EntityId> midiTrackIds;
    for (const engine::MidiClip& midiClip : project.midiClips)
        if (! detail::midiTrackAlreadySeen (std::span<const engine::EntityId> (midiTrackIds.data(), midiTrackIds.size()),
                                            midiClip.trackId))
            midiTrackIds.push_back (midiClip.trackId);

    const std::size_t expectedTrackCount = 1u + project.clips.size() + midiTrackIds.size();
    if (read.summary.tracks.size() != expectedTrackCount)
        return detail::fail (PackageStatus::SummaryMismatch, "track count does not match Project");

    for (const engine::Clip& clip : project.clips)
    {
        const engine::Asset* const asset = project.findAsset (clip.assetId);
        if (asset == nullptr)
            return detail::fail (PackageStatus::InvalidProject, "audio Clip references a missing Asset");

        std::string trackId;
        if (PackageResult result = detail::makeId ("track", clip.id, trackId); ! result.ok())
            return result;

        const TrackSummary* const track =
            detail::findTrack (std::span<const TrackSummary> (read.summary.tracks.data(), read.summary.tracks.size()), trackId);
        if (track == nullptr || track->contentType != "audio" || track->role != "regular"
            || track->audioChannels != static_cast<int> (asset->channels)
            || ! detail::nearlyEqual (track->volume, static_cast<double> (clip.gain))
            || ! detail::nearlyEqual (track->pan, 0.5))
        {
            return detail::fail (PackageStatus::SummaryMismatch, "audio track/channel summary does not match Project");
        }

        const AudioClipSummary* const audioClip =
            detail::findAudioClip (std::span<const AudioClipSummary> (read.summary.audioClips.data(),
                                                                      read.summary.audioClips.size()),
                                   trackId);
        if (audioClip == nullptr || audioClip->timeUnit != "seconds")
            return detail::fail (PackageStatus::SummaryMismatch, "audio Clip summary is missing");

        double expectedTime = 0.0;
        double expectedDuration = 0.0;
        double expectedPlayStart = 0.0;
        double expectedPlayStop = 0.0;
        double expectedFadeIn = 0.0;
        double expectedFadeOut = 0.0;
        double expectedMediaDuration = 0.0;
        if (clip.srcLen > std::numeric_limits<std::uint64_t>::max() - clip.srcOffset)
            return detail::fail (PackageStatus::InvalidTimeline, "audio Clip source window overflows");
        const DoubleResult sourceStopSeconds = framesToSeconds (clip.srcOffset + clip.srcLen, asset->sampleRate);
        if (! detail::sampleLockedSeconds (clip.timelineStart, project.sampleRate, expectedTime)
            || ! detail::sampleLockedSeconds (clip.timelineLength, project.sampleRate, expectedDuration)
            || ! detail::sampleLockedSeconds (static_cast<engine::Tick> (clip.srcOffset), asset->sampleRate, expectedPlayStart)
            || ! sourceStopSeconds.ok()
            || ! detail::sampleLockedSeconds (clip.fadeIn, project.sampleRate, expectedFadeIn)
            || ! detail::sampleLockedSeconds (clip.fadeOut, project.sampleRate, expectedFadeOut)
            || ! detail::sampleLockedSeconds (static_cast<engine::Tick> (asset->frames), asset->sampleRate, expectedMediaDuration))
        {
            return detail::fail (PackageStatus::InvalidTimeline, "failed to derive expected audio timing");
        }
        expectedPlayStop = sourceStopSeconds.value;

        int sampleRate = 0;
        if (! detail::sampleRateToInt (asset->sampleRate, sampleRate))
            return detail::fail (PackageStatus::AssetMetadataMismatch, "Asset sample rate must be a finite integer");

        const std::string mediaPath = assetAudioPathFor (asset->contentHash);
        if (! detail::nearlyEqual (audioClip->time, expectedTime)
            || ! detail::nearlyEqual (audioClip->duration, expectedDuration)
            || ! detail::nearlyEqual (audioClip->playStart, expectedPlayStart)
            || ! detail::nearlyEqual (audioClip->playStop, expectedPlayStop)
            || ! detail::nearlyEqual (audioClip->fadeIn, expectedFadeIn)
            || ! detail::nearlyEqual (audioClip->fadeOut, expectedFadeOut)
            || ! detail::nearlyEqual (audioClip->mediaDuration, expectedMediaDuration)
            || audioClip->audioChannels != static_cast<int> (asset->channels)
            || audioClip->sampleRate != sampleRate
            || audioClip->mediaPath != mediaPath)
        {
            return detail::fail (PackageStatus::SummaryMismatch, "audio Clip timing/media summary does not match Project");
        }

        const engine::DecodedAssetAudio* const decoded = detail::findDecodedAsset (decodedAssets, asset->id);
        const MediaSummary* const media =
            detail::findMedia (std::span<const MediaSummary> (read.summary.media.data(), read.summary.media.size()), mediaPath);
        if (decoded == nullptr || media == nullptr)
            return detail::fail (PackageStatus::SummaryMismatch, "decoded media summary is missing");
        if (media->sampleRate != decoded->sampleRate
            || media->channels != decoded->channels
            || media->frames != decoded->frames
            || media->samples.size() != decoded->interleavedSamples.size())
        {
            return detail::fail (PackageStatus::SummaryMismatch, "decoded media metadata does not match source");
        }
        for (std::size_t i = 0; i < media->samples.size(); ++i)
            if (media->samples[i] != decoded->interleavedSamples[i])
                return detail::fail (PackageStatus::SummaryMismatch, "decoded media samples do not match source");
    }

    if (read.summary.midiClips.size() != project.midiClips.size())
        return detail::fail (PackageStatus::SummaryMismatch, "MIDI Clip count does not match Project");

    for (const engine::MidiClip& midiClip : project.midiClips)
    {
        std::string trackId;
        if (PackageResult result = detail::makeId ("track", midiClip.trackId, trackId); ! result.ok())
            return result;

        const std::string expectedUnit = detail::timeUnitFor (midiClip.timeBase);
        double expectedTime = 0.0;
        double expectedDuration = 0.0;
        if (! detail::timeValue (midiClip.timeBase, midiClip.timelineStart, project.sampleRate, expectedTime)
            || ! detail::timeValue (midiClip.timeBase, midiClip.timelineLength, project.sampleRate, expectedDuration))
        {
            return detail::fail (PackageStatus::InvalidTimeline, "failed to derive expected MIDI timing");
        }

        const MidiClipSummary* matched = nullptr;
        for (const MidiClipSummary& candidate : read.summary.midiClips)
        {
            if (candidate.trackId == trackId
                && candidate.timeUnit == expectedUnit
                && detail::nearlyEqual (candidate.time, expectedTime)
                && detail::nearlyEqual (candidate.duration, expectedDuration)
                && candidate.notes.size() == midiClip.notes.size())
            {
                matched = &candidate;
                break;
            }
        }

        if (matched == nullptr)
            return detail::fail (PackageStatus::SummaryMismatch, "MIDI Clip summary is missing");

        for (std::size_t i = 0; i < midiClip.notes.size(); ++i)
        {
            const engine::Note& note = midiClip.notes[i];
            const NoteSummary& summary = matched->notes[i];

            double expectedNoteTime = 0.0;
            double expectedNoteDuration = 0.0;
            if (! detail::timeValue (midiClip.timeBase, note.startTick, project.sampleRate, expectedNoteTime)
                || ! detail::timeValue (midiClip.timeBase, note.lengthTicks, project.sampleRate, expectedNoteDuration))
            {
                return detail::fail (PackageStatus::InvalidTimeline, "failed to derive expected MIDI Note timing");
            }

            // Export rejects channel < 0, so a verified package always carries the real 0..15 channel;
            // compare it strictly rather than re-applying the old lossy coercion (which would self-confirm).
            if (! detail::nearlyEqual (summary.time, expectedNoteTime)
                || ! detail::nearlyEqual (summary.duration, expectedNoteDuration)
                || summary.channel != static_cast<int> (note.channel)
                || summary.key != note.key
                || ! detail::nearlyEqual (summary.velocity, note.normalizedVelocity))
            {
                return detail::fail (PackageStatus::SummaryMismatch, "MIDI Note summary does not match Project");
            }
        }
    }

    return {};
}

} // namespace yesdaw::interchange::dawproject
