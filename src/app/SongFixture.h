// YES DAW - the G0.6 song fixture generator (plan §6 G0.6; feel budgets B2/B4/B5).
//
// A deterministic, hash-stable "16-track three-minute song": N stereo stems synthesized with
// INTEGER arithmetic only (phase accumulators, triangle/saw partials, beat envelopes — no libm,
// no float contraction, so every platform produces the same bytes), written as float32 WAVs and
// assembled into a .yesdaw bundle with several clips per track (windows, fades, gains) and MIDI
// clips on the last tracks. Never committed as audio: the generator and its hashes are the
// fixture (YesDawSongFixtureCheck pins them on a small variant; tools/fixtures builds the full
// one for the Session drive and the feel-budget measurements).
//
// Pure C++ over engine + persistence + io — no JUCE.
#pragma once

#include "engine/Project.h"
#include "io/WavFile.h"
#include "persistence/ProjectBundle.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace yesdaw::app::fixture {

struct SongFixtureSpec
{
    int           tracks       = 16;
    double        seconds      = 180.0;
    std::uint32_t sampleRateHz = 48000;
    std::uint16_t channels     = 2;
    int           midiTracks   = 4;      // the LAST `midiTracks` tracks also carry one MIDI clip
};

struct SongFixtureResult
{
    bool          ok = false;
    std::string   error;
    std::filesystem::path bundlePath;
    std::vector<std::filesystem::path> stemPaths;
    std::uint64_t stemHash    = 0;   // FNV-1a over every stem's float32 bytes, in stem order
    std::uint64_t projectHash = 0;   // FNV-1a over the placements (clips, MIDI notes, tempo/meter)
    std::size_t   clipCount   = 0;
    std::size_t   noteCount   = 0;
    std::uint64_t stemFrames  = 0;
};

namespace detail {

inline constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
inline constexpr std::uint64_t kFnvPrime  = 1099511628211ull;

inline std::uint64_t fnv1a (const void* data, std::size_t size, std::uint64_t hash) noexcept
{
    const auto* bytes = static_cast<const std::uint8_t*> (data);
    for (std::size_t i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= kFnvPrime;
    }
    return hash;
}

template <typename T>
inline std::uint64_t fnv1aValue (const T& value, std::uint64_t hash) noexcept
{
    return fnv1a (&value, sizeof (T), hash);
}

inline engine::EntityId fixtureId (std::uint64_t kind, std::uint64_t index) noexcept
{
    return engine::EntityId::fromBigEndianParts (0x0000000000F1A700ull | kind, 1u + index);
}

// Twelve-tone semitone ratios in 16.16 fixed point (2^(n/12) * 65536, rounded) — integers only.
inline constexpr std::uint32_t kSemitoneRatio16[12] = {
    65536u, 69433u, 73562u, 77936u, 82570u, 87480u, 92682u, 98193u, 104032u, 110218u, 116772u, 123715u
};

// Phase increment (32-bit phase) for `milliHz` at `sampleRateHz`: integer division, exact.
inline std::uint32_t phaseIncrement (std::uint64_t milliHz, std::uint32_t sampleRateHz) noexcept
{
    return static_cast<std::uint32_t> ((milliHz << 32) / (static_cast<std::uint64_t> (sampleRateHz) * 1000ull));
}

// Triangle from a 32-bit phase: -32768..32767.
inline std::int32_t triangle (std::uint32_t phase) noexcept
{
    const std::int32_t saw = static_cast<std::int32_t> (phase >> 16) - 32768;   // -32768..32767
    const std::int32_t folded = saw < 0 ? -saw : saw;                              // 0..32768
    return folded * 2 - 32768;
}

inline std::int32_t saw (std::uint32_t phase) noexcept
{
    return static_cast<std::int32_t> (phase >> 16) - 32768;
}

// One stem's sample as an integer in -32768..32767, from integer state only.
struct StemVoice
{
    std::uint32_t phase[3] = { 0u, 0u, 0u };
    std::uint32_t inc[3]   = { 0u, 0u, 0u };
    std::uint32_t lfoPhase = 0u;
    std::uint32_t lfoInc   = 0u;
    bool          percussive = false;
    int           amplitudeShift = 2;
};

inline StemVoice makeVoice (int track, int channel, std::uint32_t sampleRateHz) noexcept
{
    StemVoice voice;
    // A fundamental per track: 55 Hz up two octaves and a bit, one semitone per track.
    const std::uint64_t baseMilliHz = 55000ull;
    const int semitone = track % 12;
    const int octave = 1 + (track / 12);
    std::uint64_t fundamentalMilliHz = (baseMilliHz * kSemitoneRatio16[semitone]) >> 16;
    fundamentalMilliHz <<= octave;
    // The right channel is detuned by 0.2 % so the stereo image moves — integer only.
    if (channel == 1)
        fundamentalMilliHz += fundamentalMilliHz / 500ull;
    for (int partial = 0; partial < 3; ++partial)
    {
        voice.inc[partial] = phaseIncrement (fundamentalMilliHz * static_cast<std::uint64_t> (partial + 1), sampleRateHz);
        voice.phase[partial] = static_cast<std::uint32_t> ((track * 7919 + channel * 104729 + partial * 1299709) & 0x7FFFFFFF) << 1;
    }
    voice.lfoInc = phaseIncrement (250ull + static_cast<std::uint64_t> (track) * 37ull, sampleRateHz);   // 0.25..0.8 Hz
    voice.lfoPhase = static_cast<std::uint32_t> (track) * 0x1234567u;
    voice.percussive = (track % 2) == 0;
    voice.amplitudeShift = 3 + (track % 3);   // 0.125, 0.0625, 0.03125 full scale: sixteen summed stay under 0dBFS
    return voice;
}

inline std::int32_t nextSample (StemVoice& voice, std::uint64_t frame, std::uint32_t beatFrames) noexcept
{
    // Partials: triangle fundamental, half-amplitude saw second, quarter-amplitude triangle third.
    std::int32_t mix = triangle (voice.phase[0]) + (saw (voice.phase[1]) >> 1) + (triangle (voice.phase[2]) >> 2);
    mix = mix / 2;   // keep inside 16 bits after the partial sum (worst case 1.75 x)
    for (int partial = 0; partial < 3; ++partial)
        voice.phase[partial] += voice.inc[partial];

    std::int32_t envelope;   // 0..65535
    if (voice.percussive)
    {
        const std::uint64_t inBeat = beatFrames > 0 ? frame % beatFrames : 0;
        envelope = 65535 - static_cast<std::int32_t> ((inBeat * 65535ull) / (beatFrames > 0 ? beatFrames : 1));
    }
    else
    {
        // 0.6 + 0.4 * tremolo
        envelope = 39321 + ((triangle (voice.lfoPhase) + 32768) * 26214) / 65536;
    }
    voice.lfoPhase += voice.lfoInc;

    const std::int64_t shaped = (static_cast<std::int64_t> (mix) * envelope) >> 16;
    return static_cast<std::int32_t> (shaped >> voice.amplitudeShift);
}

} // namespace detail

// Generate the fixture into `outDir` (created; a previous bundle there is replaced). Stems go to
// `outDir/stems/stem-NN.wav`, the bundle to `outDir/song.yesdaw`.
[[nodiscard]] inline SongFixtureResult buildSongFixture (const std::filesystem::path& outDir,
                                                          const SongFixtureSpec& spec)
{
    SongFixtureResult result;
    if (spec.tracks <= 0 || spec.seconds <= 0.0 || spec.sampleRateHz == 0u || spec.channels == 0u
        || spec.midiTracks < 0 || spec.midiTracks > spec.tracks)
    {
        result.error = "invalid fixture spec";
        return result;
    }

    std::error_code ec;
    std::filesystem::create_directories (outDir / "stems", ec);
    if (ec)
    {
        result.error = "cannot create " + outDir.string() + ": " + ec.message();
        return result;
    }
    result.bundlePath = outDir / "song.yesdaw";
    std::filesystem::remove_all (result.bundlePath, ec);

    const std::uint64_t stemFrames = static_cast<std::uint64_t> (spec.seconds * static_cast<double> (spec.sampleRateHz));
    const std::uint32_t beatFrames = spec.sampleRateHz / 2u;   // 120 BPM
    result.stemFrames = stemFrames;

    persistence::ProjectBundleDb db;
    if (auto opened = persistence::ProjectBundleDb::openOrCreateBundle (result.bundlePath, db); ! opened.ok())
    {
        result.error = "bundle: " + opened.message;
        return result;
    }

    engine::Project project;
    project.id = detail::fixtureId (0x01, 0);
    project.sampleRate = engine::SampleRate { static_cast<double> (spec.sampleRateHz) };
    project.tempoMap.push_back ({ 0, 120.0, engine::TempoCurve::Jump });
    project.meterMap.push_back ({ 0, 4, 4 });

    std::uint64_t stemHash = detail::kFnvOffset;
    std::vector<float> interleaved;
    interleaved.resize (static_cast<std::size_t> (stemFrames) * spec.channels);

    for (int track = 0; track < spec.tracks; ++track)
    {
        std::vector<detail::StemVoice> voices;
        for (int channel = 0; channel < spec.channels; ++channel)
            voices.push_back (detail::makeVoice (track, channel, spec.sampleRateHz));

        for (std::uint64_t frame = 0; frame < stemFrames; ++frame)
            for (int channel = 0; channel < spec.channels; ++channel)
            {
                const std::int32_t value = detail::nextSample (voices[static_cast<std::size_t> (channel)], frame, beatFrames);
                // /32768 is exact in binary float: identical bytes on every platform.
                interleaved[static_cast<std::size_t> (frame) * spec.channels + static_cast<std::size_t> (channel)] =
                    static_cast<float> (value) / 32768.0f;
            }

        stemHash = detail::fnv1a (interleaved.data(), interleaved.size() * sizeof (float), stemHash);

        char name[32];
        std::snprintf (name, sizeof (name), "stem-%02d.wav", track + 1);
        const std::filesystem::path stemPath = outDir / "stems" / name;
        if (auto written = io::writeFloat32WavFile (stemPath, project.sampleRate, spec.channels, stemFrames,
                                                     std::span<const float> (interleaved.data(), interleaved.size()));
            ! written.ok())
        {
            result.error = "stem write: " + written.message;
            return result;
        }
        result.stemPaths.push_back (stemPath);

        persistence::AssetImportRequest request;
        request.sourcePath = stemPath;
        request.assetId = detail::fixtureId (0x02, static_cast<std::uint64_t> (track));
        request.frames = stemFrames;
        request.sampleRate = project.sampleRate;
        request.channels = spec.channels;
        engine::Asset asset;
        if (auto imported = db.importAssetBytes (request, asset); ! imported.ok())
        {
            result.error = "asset import: " + imported.message;
            return result;
        }
        project.assets.push_back (asset);

        engine::Track projectTrack;
        projectTrack.id = detail::fixtureId (0x03, static_cast<std::uint64_t> (track));
        projectTrack.strip.name = "Stem " + std::to_string (track + 1);
        project.tracks.push_back (projectTrack);

        // Clips: 3..6 windows of the stem spread across the song, with fades and gains.
        const int clipCount = 3 + (track % 4);
        const std::uint64_t slot = stemFrames / static_cast<std::uint64_t> (clipCount);
        const std::uint64_t length = (slot * 4ull) / 5ull;
        const std::uint64_t fade = spec.sampleRateHz / 20u;   // 50 ms
        for (int i = 0; i < clipCount; ++i)
        {
            engine::Clip clip;
            clip.id = detail::fixtureId (0x04, static_cast<std::uint64_t> (track) * 64ull + static_cast<std::uint64_t> (i));
            clip.assetId = asset.id;
            clip.trackId = projectTrack.id;
            const std::uint64_t start = slot * static_cast<std::uint64_t> (i)
                                      + (static_cast<std::uint64_t> (track) * 3701ull) % (slot - length + 1ull);
            clip.timelineStart = static_cast<engine::Tick> (start);
            clip.timelineLength = static_cast<engine::Tick> (length);
            clip.srcOffset = (static_cast<std::uint64_t> (i) * 12345ull + static_cast<std::uint64_t> (track) * 777ull)
                             % (stemFrames - length + 1ull);
            clip.srcLen = length;
            clip.gain = 1.0f - 0.05f * static_cast<float> (i % 3);
            clip.fadeIn = static_cast<engine::Tick> (fade);
            clip.fadeOut = static_cast<engine::Tick> (fade);
            clip.timeBase = engine::TimeBase::SampleLocked;
            project.clips.push_back (clip);
        }
    }

    // MIDI clips on the last `midiTracks` tracks: four bars of sixteenths through the track's instrument.
    for (int m = 0; m < spec.midiTracks; ++m)
    {
        const int track = spec.tracks - spec.midiTracks + m;
        engine::MidiClip midiClip;
        midiClip.id = detail::fixtureId (0x05, static_cast<std::uint64_t> (m));
        midiClip.trackId = project.tracks[static_cast<std::size_t> (track)].id;
        midiClip.timelineStart = 0;
        midiClip.timelineLength = static_cast<engine::Tick> (4 * 4 * engine::kTicksPerQuarter);
        midiClip.timeBase = engine::TimeBase::TempoLocked;
        for (int n = 0; n < 16; ++n)
        {
            engine::Note note;
            note.id = detail::fixtureId (0x06, static_cast<std::uint64_t> (m) * 64ull + static_cast<std::uint64_t> (n));
            note.startTick = static_cast<engine::Tick> (n) * static_cast<engine::Tick> (engine::kTicksPerQuarter / 4);
            note.lengthTicks = static_cast<engine::Tick> (engine::kTicksPerQuarter / 5);
            note.key = static_cast<std::int16_t> (48 + (n * 5 + m * 7) % 24);
            note.pitchNote = static_cast<double> (note.key);
            note.normalizedVelocity = 0.7;
            note.portIndex = 1;
            note.channel = 2;
            midiClip.notes.push_back (note);
        }
        project.midiClips.push_back (midiClip);
    }

    if (! project.hasValidAssetClipIndirection())
    {
        result.error = "generated project failed asset/clip indirection validation";
        return result;
    }
    if (auto written = db.writeProjectSnapshot (project); ! written.ok())
    {
        result.error = "snapshot: " + written.message;
        return result;
    }

    // Placement hash: everything the arrangement says, independent of ids and bundle bytes.
    std::uint64_t projectHash = detail::kFnvOffset;
    projectHash = detail::fnv1aValue (spec.tracks, projectHash);
    projectHash = detail::fnv1aValue (stemFrames, projectHash);
    for (const engine::Clip& clip : project.clips)
    {
        projectHash = detail::fnv1aValue (clip.timelineStart, projectHash);
        projectHash = detail::fnv1aValue (clip.timelineLength, projectHash);
        projectHash = detail::fnv1aValue (clip.srcOffset, projectHash);
        projectHash = detail::fnv1aValue (clip.srcLen, projectHash);
        projectHash = detail::fnv1aValue (clip.gain, projectHash);
        projectHash = detail::fnv1aValue (clip.fadeIn, projectHash);
        projectHash = detail::fnv1aValue (clip.fadeOut, projectHash);
    }
    for (const engine::MidiClip& midiClip : project.midiClips)
    {
        projectHash = detail::fnv1aValue (midiClip.timelineStart, projectHash);
        projectHash = detail::fnv1aValue (midiClip.timelineLength, projectHash);
        for (const engine::Note& note : midiClip.notes)
        {
            projectHash = detail::fnv1aValue (note.startTick, projectHash);
            projectHash = detail::fnv1aValue (note.lengthTicks, projectHash);
            projectHash = detail::fnv1aValue (note.key, projectHash);
            result.noteCount++;
        }
    }

    result.stemHash = stemHash;
    result.projectHash = projectHash;
    result.clipCount = project.clips.size();
    result.ok = true;
    return result;
}

} // namespace yesdaw::app::fixture
