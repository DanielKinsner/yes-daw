// Pure audio editing regressions: real immutable WAV bytes, model verbs and rendered samples.
#include "ui/UiAppModel.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace {
using namespace yesdaw;

engine::EntityId entity (std::uint8_t value)
{
    engine::EntityId id;
    id.bytes.back() = value;
    return id;
}

std::vector<char> bytesAt (const std::filesystem::path& path)
{
    std::ifstream file (path, std::ios::binary);
    REQUIRE (file.good());
    return { std::istreambuf_iterator<char> (file), std::istreambuf_iterator<char>() };
}

struct Fixture
{
    static constexpr engine::Tick start = 73;
    static constexpr std::uint64_t offset = 137;
    static constexpr std::uint64_t length = 9001;
    std::filesystem::path root = std::filesystem::temp_directory_path()
        / ("yesdaw-reversed-edit-" + std::to_string (std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup
    {
        std::filesystem::path path;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove_all (path, ignored); }
    } cleanup { root };
    std::filesystem::path bundle = root / "song.yesdaw";
    std::filesystem::path originalPath = root / "original.wav";
    std::filesystem::path assetPath;
    std::vector<float> samples = std::vector<float> (12000);
    std::vector<char> originalBytes;
    engine::Project original;
    ui::UiDecodedAsset decoded;
    ui::UiAppModel model;

    explicit Fixture (bool silence = false, double assetRate = 48000.0,
                      std::initializer_list<std::pair<unsigned, unsigned>> silentRuns = {
                          { 0u, 2500u }, { 3200u, 5700u }, { 6501u, 9001u } })
    {
        std::filesystem::create_directories (root);
        for (std::size_t i = 0; i < samples.size(); ++i)
            samples[i] = static_cast<float> (i + 1) / 32768.0f;
        if (silence)
            for (const auto& [begin, end] : silentRuns)
                std::fill (samples.begin() + static_cast<std::ptrdiff_t> (offset + begin),
                           samples.begin() + static_cast<std::ptrdiff_t> (offset + end), 0.0f);
        REQUIRE (io::writeFloat32WavFile (originalPath, engine::SampleRate { assetRate }, 1, samples.size(), samples).ok());
        originalBytes = bytesAt (originalPath);
        original.id = entity (1);
        original.sampleRate = engine::SampleRate { 48000.0 };
        engine::Asset asset;
        asset.id = entity (2);
        const auto byteView = std::span<const std::uint8_t> (
            reinterpret_cast<const std::uint8_t*> (originalBytes.data()), originalBytes.size());
        asset.contentHash = persistence::detail::sha256Bytes (byteView);
        asset.frames = samples.size();
        asset.sampleRate = engine::SampleRate { assetRate };
        asset.channels = 1;
        original.assets.push_back (asset);
        engine::Track track;
        track.id = entity (3);
        original.tracks.push_back (track);
        engine::Clip clip;
        clip.id = entity (4);
        clip.assetId = asset.id;
        clip.trackId = track.id;
        clip.timelineStart = start;
        // This fixture uses only equal rates and the exact rational 48000 / 44100 = 160 / 147.
        clip.timelineLength = static_cast<engine::Tick> (assetRate == 44100.0 ? (length * 160u + 73u) / 147u : length);
        clip.srcOffset = offset;
        clip.srcLen = length;
        clip.reversed = true;
        original.clips.push_back (clip);
        REQUIRE (original.hasValidAssetClipIndirection());
        {
            persistence::ProjectBundleDb db;
            REQUIRE (persistence::ProjectBundleDb::openOrCreateBundle (bundle, db).ok());
            REQUIRE (db.writeProjectSnapshot (original).ok());
        }
        assetPath = bundle / persistence::detail::assetRelativePathForHash (asset.contentHash);
        std::filesystem::create_directories (assetPath.parent_path());
        REQUIRE (std::filesystem::copy_file (originalPath, assetPath));
        decoded = ui::UiDecodedAsset::fromInterleaved (asset.id, asset.sampleRate, 1, std::vector<float> (samples));
        model.setSessionStateDirectory (root / "state");
        REQUIRE (model.loadProjectBundle (bundle, { &decoded, 1 }).ok());
        REQUIRE (model.selectTimelineClip (clip.id));
    }

    std::vector<float> render (const engine::Project& project,
                              engine::ResampleQuality quality = engine::ResampleQuality::OfflineRender) const
    {
        const auto view = engine::buildRateMatchedSamples (decoded.interleaved(), decoded.channels,
            decoded.sampleRate.hz, project.sampleRate.hz, quality);
        const engine::DecodedAssetAudio audio { decoded.assetId, project.sampleRate, view->frames,
                                               decoded.channels, view->interleaved };
        const auto result = engine::renderOfflineProject (engine::projectInViewFrames (project), { &audio, 1 });
        REQUIRE (result.ok());
        auto samplesOut = result.interleavedSamples;
        samplesOut.resize (static_cast<std::size_t> (start + original.clips.front().timelineLength) * 2u);
        return samplesOut;
    }

    void load (const engine::Project& project)
    {
        {
            persistence::ProjectBundleDb db;
            REQUIRE (persistence::ProjectBundleDb::openExistingBundle (bundle, db).ok());
            REQUIRE (db.writeProjectSnapshot (project).ok());
        }
        REQUIRE (model.loadProjectBundle (bundle, { &decoded, 1 }).ok());
        REQUIRE (model.selectTimelineClip (project.clips.front().id));
    }

    std::vector<float> play()
    {
        REQUIRE (model.locatePlaybackFrame (0));
        REQUIRE (model.dispatch (ui::UiActionId::TransportPlay).dispatched);
        auto rendered = model.renderPlaybackFrames (
            static_cast<std::uint64_t> (start + original.clips.front().timelineLength), 128);
        REQUIRE (model.dispatch (ui::UiActionId::TransportStop).dispatched);
        return rendered;
    }

    void unchangedBytes() const
    {
        CHECK (bytesAt (originalPath) == originalBytes);
        CHECK (bytesAt (assetPath) == originalBytes);
        CHECK (model.project().assets == original.assets);
    }

    void durable (const std::vector<engine::Clip>& clips)
    {
        persistence::ProjectBundleDb db;
        REQUIRE (persistence::ProjectBundleDb::openExistingBundle (bundle, db).ok());
        engine::Project saved;
        REQUIRE (db.readProjectSnapshot (saved).ok());
        CHECK (saved.clips == clips);
        ui::UiAppModel reopened;
        reopened.setSessionStateDirectory (root / "reopened-state");
        REQUIRE (reopened.loadProjectBundle (bundle, { &decoded, 1 }).ok());
        CHECK (reopened.project().clips == clips);
        CHECK (bool (render (reopened.project()) == render (model.project())));
        unchangedBytes();
    }

    void copyForNoOverlap (engine::Project project, engine::Tick placedLength)
    {
        engine::Clip copy = project.clips.front();
        copy.id = entity (8);
        copy.timelineStart = start + original.clips.front().timelineLength + 200;
        copy.timelineLength = placedLength;
        copy.srcOffset = 0;
        copy.srcLen = 1;
        copy.muted = true;
        project.clips.push_back (copy);
        load (project);
        REQUIRE (model.selectTimelineClip (copy.id));
        REQUIRE (model.dispatch (ui::UiActionId::TimelineClipCopy).dispatched);
        REQUIRE (model.dispatch (ui::UiActionId::EditModeNoOverlap).dispatched);
    }
};

std::vector<engine::Clip> noOverlapNeighbours (const ui::UiAppModel& model)
{
    std::vector<engine::Clip> neighbours;
    for (const auto& clip : model.project().clips)
        if (clip.id != entity (8) && clip.id != model.selectedTimelineClipId())
            neighbours.push_back (clip);
    std::sort (neighbours.begin(), neighbours.end(), [] (const auto& a, const auto& b) {
        return a.timelineStart < b.timelineStart;
    });
    return neighbours;
}

void checkNoOverlapHistory (Fixture& f, const engine::Project& before)
{
    const auto after = f.model.project().clips;
    REQUIRE (f.model.dispatch (ui::UiActionId::EditUndo).dispatched);
    CHECK (f.model.project().clips == before.clips);
    CHECK_FALSE (f.model.context().canUndo);
    REQUIRE (f.model.dispatch (ui::UiActionId::EditRedo).dispatched);
    CHECK (f.model.project().clips == after);
    CHECK_FALSE (f.model.context().canRedo);
    f.durable (after);
}

void checkNoOverlapRefusal (Fixture& f, engine::Tick placedStart)
{
    const auto beforeAudio = f.play();
    REQUIRE (f.model.locatePlaybackFrame (placedStart));
    const auto before = f.model.project();
    const auto selected = f.model.selectedTimelineClipId();
    const auto selectedCount = f.model.selectedTimelineClipCount();
    const auto dispatchCount = f.model.context().commandDispatchCount;
    const auto editCount = f.model.context().timelineEditCount;
    const auto replacementCount = f.model.playbackReplaceCount();
    const auto writeStamp = f.model.bundleWriteStamp();
    const auto canUndo = f.model.context().canUndo;
    const auto canRedo = f.model.context().canRedo;
    const auto dirty = f.model.hasUnsavedChanges();
    const auto result = f.model.dispatch (ui::UiActionId::TimelineClipPaste);
    CHECK_FALSE (result.dispatched);
    CHECK (std::string_view (result.state.disabledReason) == "edit mode refused the paste");
    CHECK (f.model.project().clips == before.clips);
    CHECK (f.model.project().assets == before.assets);
    CHECK (f.model.project().recordingTakes == before.recordingTakes);
    CHECK (f.model.project().recordingCompSegments == before.recordingCompSegments);
    CHECK (f.model.project().hasValidAssetClipIndirection());
    CHECK (f.model.selectedTimelineClipId() == selected);
    CHECK (f.model.selectedTimelineClipCount() == selectedCount);
    CHECK (f.model.isTimelineClipSelected (selected));
    CHECK (f.model.context().commandDispatchCount == dispatchCount);
    CHECK (f.model.context().timelineEditCount == editCount);
    CHECK (f.model.playbackReplaceCount() == replacementCount);
    CHECK (f.model.context().canUndo == canUndo);
    CHECK (f.model.context().canRedo == canRedo);
    CHECK (f.model.hasUnsavedChanges() == dirty);
    CHECK (f.model.bundleWriteStamp() == writeStamp);
    CHECK (bool (f.play() == beforeAudio));
    f.durable (before.clips);
    persistence::ProjectBundleDb db;
    REQUIRE (persistence::ProjectBundleDb::openExistingBundle (f.bundle, db).ok());
    engine::Project saved;
    REQUIRE (db.readProjectSnapshot (saved).ok());
    CHECK (saved.recordingTakes == before.recordingTakes);
    CHECK (saved.recordingCompSegments == before.recordingCompSegments);
}
} // namespace

TEST_CASE ("No Overlap placement preserves temporal audio neighbours and refuses invalid fragments atomically",
           "[reversed-clip][no-overlap]")
{
    SECTION ("same-rate head, tail and spanning neighbours keep exact source audio and metadata")
    {
        Fixture f;
        const bool reversed = GENERATE (false, true);
        const int branch = GENERATE (0, 1, 2);   // head, tail, both
        const bool fades = GENERATE (false, true);
        auto& original = f.original.clips.front();
        original.reversed = reversed;
        original.gain = 0.75f;
        original.colour = 0xff336699u;
        original.name = engine::ClipName { "neighbour" };
        if (fades)
        {
            original.fadeIn = 123;
            original.fadeOut = 456;
            original.fadeInShape = engine::FadeShape::Linear;
            original.fadeOutShape = engine::FadeShape::Log;
            original.fadeInCurve = -0.25f;
            original.fadeOutCurve = 0.5f;
        }
        const engine::Tick a = branch == 1 ? 0 : 1701;
        const engine::Tick b = branch == 0 ? 9001 : 6203;
        f.copyForNoOverlap (f.original, b - a);
        const auto before = f.model.project();
        const auto beforeAudio = f.render (before);
        REQUIRE (f.model.locatePlaybackFrame (Fixture::start + a));
        REQUIRE (f.model.dispatch (ui::UiActionId::TimelineClipPaste).dispatched);
        const auto neighbours = noOverlapNeighbours (f.model);
        REQUIRE (neighbours.size() == (branch == 2 ? 2u : 1u));
        std::vector<engine::Clip> expected;
        if (branch != 1)
        {
            auto head = original;
            head.timelineLength = 1701;
            head.srcOffset = reversed ? 7437u : 137u;
            head.srcLen = 1701;
            expected.push_back (head);
        }
        if (branch != 0)
        {
            auto tail = original;
            tail.timelineStart += 6203;
            tail.timelineLength = 2798;
            tail.srcOffset = reversed ? 137u : 6340u;
            tail.srcLen = 2798;
            if (branch == 2)
            {
                tail.id = neighbours.back().id;
                CHECK (tail.id != original.id);
                CHECK (tail.id.bytes[9] == 0xC6u);
                tail.fadeIn = 0;
            }
            expected.push_back (tail);
        }
        CHECK (neighbours == expected);
        if (! fades)
        {
            auto expectedAudio = beforeAudio;
            std::fill (expectedAudio.begin() + static_cast<std::ptrdiff_t> ((Fixture::start + a) * 2),
                       expectedAudio.begin() + static_cast<std::ptrdiff_t> ((Fixture::start + b) * 2), 0.0f);
            CHECK (bool (f.render (f.model.project()) == expectedAudio));
            CHECK (bool (f.play() == expectedAudio));
        }
        checkNoOverlapHistory (f, before);
    }

    SECTION ("natural cross-rate cuts move outward to the nearest absolute Asset boundary")
    {
        Fixture f { false, 44100.0 };
        const bool reversed = GENERATE (false, true);
        const int branch = GENERATE (0, 1, 2);
        const bool missing = GENERATE (false, true);
        f.original.clips.front().reversed = reversed;
        const engine::Tick request = missing ? (reversed ? 2728 : 2725) : 2727;
        const engine::Tick headEnd = missing ? (reversed ? 2727 : 2724) : 2727;
        const engine::Tick tailStart = missing ? (reversed ? 2729 : 2726) : 2727;
        const std::uint64_t headBoundary = reversed ? 6632u : (missing ? 2640u : 2642u);
        const std::uint64_t tailBoundary = reversed ? (missing ? 6631u : 6632u) : (missing ? 2641u : 2642u);
        const auto mapped = [] (std::uint64_t frame) { return (frame * 160u + 73u) / 147u; };
        CHECK ((reversed ? mapped (9138u) - mapped (headBoundary) : mapped (headBoundary) - mapped (137u))
               == static_cast<std::uint64_t> (headEnd));
        CHECK ((reversed ? mapped (9138u) - mapped (tailBoundary) : mapped (tailBoundary) - mapped (137u))
               == static_cast<std::uint64_t> (tailStart));
        CHECK (request - headEnd <= 1);
        CHECK (tailStart - request <= 1);
        const engine::Tick a = branch == 1 ? 0 : request;
        const engine::Tick b = branch == 0 ? 9797 : (branch == 1 ? request : 6203);
        f.copyForNoOverlap (f.original, b - a);
        const auto before = f.model.project();
        auto expectedOffline = f.render (before);
        auto expectedLive = f.play();
        REQUIRE (f.model.locatePlaybackFrame (Fixture::start + a));
        REQUIRE (f.model.dispatch (ui::UiActionId::TimelineClipPaste).dispatched);
        const auto neighbours = noOverlapNeighbours (f.model);
        REQUIRE (neighbours.size() == (branch == 2 ? 2u : 1u));
        if (branch != 1)
        {
            const auto& head = neighbours.front();
            CHECK (head.id == entity (4));
            CHECK (head.timelineStart == Fixture::start);
            CHECK (head.timelineLength == headEnd);
            CHECK (head.srcOffset == (reversed ? headBoundary : 137u));
            CHECK (head.srcLen == (reversed ? 9138u - headBoundary : headBoundary - 137u));
        }
        const auto retainedTail = branch == 2 ? 6203 : tailStart;
        const auto retainedTailBoundary = branch == 2 ? (reversed ? 3439u : 5836u) : tailBoundary;
        if (branch != 0)
        {
            const auto& tail = neighbours.back();
            CHECK (tail.timelineStart == Fixture::start + retainedTail);
            CHECK (tail.timelineLength == 9797 - retainedTail);
            CHECK (tail.srcOffset == (reversed ? 137u : retainedTailBoundary));
            CHECK (tail.srcLen == (reversed ? retainedTailBoundary - 137u : 9138u - retainedTailBoundary));
            CHECK (tail.timelineStart >= Fixture::start + b);
        }
        const auto silentBegin = Fixture::start + (branch == 1 ? 0 : headEnd);
        const auto silentEnd = Fixture::start + (branch == 0 ? 9797 : retainedTail);
        for (auto* expected : { &expectedOffline, &expectedLive })
            std::fill (expected->begin() + static_cast<std::ptrdiff_t> (silentBegin * 2),
                       expected->begin() + static_cast<std::ptrdiff_t> (silentEnd * 2), 0.0f);
        CHECK (bool (f.render (f.model.project()) == expectedOffline));
        CHECK (bool (f.play() == expectedLive));
        checkNoOverlapHistory (f, before);
    }

    SECTION ("legacy rounded outer endpoints retain their original padding or truncation")
    {
        Fixture f { false, 44100.0 };
        const bool reversed = GENERATE (false, true);
        const bool padded = GENERATE (false, true);
        auto& original = f.original.clips.front();
        original.reversed = reversed;
        original.srcOffset = padded ? 6 : 1;
        original.srcLen = padded ? 1001 : 1000;
        original.timelineLength = padded ? 1090 : 1088;
        const auto mapped = [] (std::uint64_t frame) { return (frame * 160u + 73u) / 147u; };
        const auto sourceEnd = original.srcOffset + original.srcLen;
        const auto headBoundary = reversed ? sourceEnd - 300u : original.srcOffset + 300u;
        const auto tailBoundary = reversed ? sourceEnd - 700u : original.srcOffset + 700u;
        const auto a = static_cast<engine::Tick> (reversed
            ? mapped (sourceEnd) - mapped (headBoundary) : mapped (headBoundary) - mapped (original.srcOffset));
        const auto b = static_cast<engine::Tick> (reversed
            ? mapped (sourceEnd) - mapped (tailBoundary) : mapped (tailBoundary) - mapped (original.srcOffset));
        f.copyForNoOverlap (f.original, b - a);
        const auto before = f.model.project();
        auto expectedOffline = f.render (before);
        auto expectedLive = f.play();
        REQUIRE (f.model.locatePlaybackFrame (Fixture::start + a));
        REQUIRE (f.model.dispatch (ui::UiActionId::TimelineClipPaste).dispatched);
        const auto neighbours = noOverlapNeighbours (f.model);
        REQUIRE (neighbours.size() == 2);
        CHECK (neighbours.front().timelineStart == original.timelineStart);
        CHECK (neighbours.front().timelineLength == a);
        CHECK (neighbours.front().srcOffset == (reversed ? headBoundary : original.srcOffset));
        CHECK (neighbours.front().srcLen == 300u);
        CHECK (neighbours.back().timelineStart == Fixture::start + b);
        CHECK (neighbours.back().timelineStart + neighbours.back().timelineLength
               == original.timelineStart + original.timelineLength);
        CHECK (neighbours.back().srcOffset == (reversed ? original.srcOffset : tailBoundary));
        CHECK (neighbours.back().srcLen == original.srcLen - 700u);
        for (auto* expected : { &expectedOffline, &expectedLive })
            std::fill (expected->begin() + static_cast<std::ptrdiff_t> ((Fixture::start + a) * 2),
                       expected->begin() + static_cast<std::ptrdiff_t> ((Fixture::start + b) * 2), 0.0f);
        CHECK (bool (f.render (f.model.project()) == expectedOffline));
        CHECK (bool (f.play() == expectedLive));
        checkNoOverlapHistory (f, before);
    }

    SECTION ("a wider natural rate ratio leaves only the minimal representable gap")
    {
        Fixture f { false, 16000.0 };
        const bool reversed = GENERATE (false, true);
        f.original.clips.front().reversed = reversed;
        f.original.clips.front().timelineLength = 27003;
        f.copyForNoOverlap (f.original, 6202 - 1703);
        const auto before = f.model.project();
        auto expectedOffline = f.render (before);
        auto expectedLive = f.play();
        REQUIRE (f.model.locatePlaybackFrame (Fixture::start + 1703));
        REQUIRE (f.model.dispatch (ui::UiActionId::TimelineClipPaste).dispatched);
        const auto neighbours = noOverlapNeighbours (f.model);
        REQUIRE (neighbours.size() == 2);
        CHECK (neighbours.front().timelineLength == 1701);
        CHECK (neighbours.front().srcOffset == (reversed ? 8571u : 137u));
        CHECK (neighbours.front().srcLen == 567u);
        CHECK (neighbours.back().timelineStart == Fixture::start + 6204);
        CHECK (neighbours.back().timelineLength == 27003 - 6204);
        CHECK (neighbours.back().srcOffset == (reversed ? 137u : 2205u));
        CHECK (neighbours.back().srcLen == 6933u);
        // Each adjacent Asset boundary is exactly three project frames apart: neither
        // retained fragment can move inward one Asset frame without entering placement.
        CHECK (567u * 3u <= 1703u);
        CHECK (568u * 3u > 1703u);
        CHECK (2068u * 3u >= 6202u);
        CHECK (2067u * 3u < 6202u);
        CHECK (1703 - neighbours.front().timelineLength == 2);
        CHECK (neighbours.back().timelineStart - (Fixture::start + 6202) == 2);
        for (auto* expected : { &expectedOffline, &expectedLive })
            std::fill (expected->begin() + static_cast<std::ptrdiff_t> ((Fixture::start + 1701) * 2),
                       expected->begin() + static_cast<std::ptrdiff_t> ((Fixture::start + 6204) * 2), 0.0f);
        CHECK (bool (f.render (f.model.project()) == expectedOffline));
        CHECK (bool (f.play() == expectedLive));
        checkNoOverlapHistory (f, before);
    }

    SECTION ("non-natural stretched windows use complementary original source boundaries")
    {
        Fixture f;
        const bool reversed = GENERATE (false, true);
        auto& original = f.original.clips.front();
        original.reversed = reversed;
        original.stretchFactor = 2.0f;
        original.timelineLength = 18002;
        f.copyForNoOverlap (f.original, 12407 - 3403);
        const auto before = f.model.project();
        REQUIRE (f.model.locatePlaybackFrame (Fixture::start + 3403));
        REQUIRE (f.model.dispatch (ui::UiActionId::TimelineClipPaste).dispatched);
        const auto neighbours = noOverlapNeighbours (f.model);
        REQUIRE (neighbours.size() == 2);
        CHECK (neighbours.front().srcOffset == (reversed ? 7436u : 137u));
        CHECK (neighbours.front().srcLen == 1702u);
        CHECK (neighbours.back().srcOffset == (reversed ? 137u : 6341u));
        CHECK (neighbours.back().srcLen == 2797u);
        CHECK (neighbours.front().timelineLength == 3403);
        CHECK (neighbours.back().timelineStart == Fixture::start + 12407);
        CHECK (neighbours.back().timelineLength == 5595);
        checkNoOverlapHistory (f, before);
    }

    SECTION ("affected linked RecordingTake and Comp refuse partial or complete coverage")
    {
        Fixture f;
        const bool reversed = GENERATE (false, true);
        const int branch = GENERATE (0, 1, 2, 3);
        f.original.clips.front().reversed = reversed;
        engine::RecordingTake take;
        take.id = entity (6);
        take.assetId = entity (2);
        take.trackId = entity (3);
        take.clipId = entity (4);
        take.timelineStart = Fixture::start;
        take.frameCount = Fixture::length;
        f.original.recordingTakes.push_back (take);
        f.original.recordingCompSegments.push_back ({ entity (7), take.id, Fixture::start, 1000, 30 });
        const engine::Tick a = branch == 1 || branch == 3 ? 0 : 1701;
        const engine::Tick b = branch == 0 || branch == 3 ? 9001 : 6203;
        f.copyForNoOverlap (f.original, b - a);
        checkNoOverlapRefusal (f, Fixture::start + a);
    }

    SECTION ("a required fragment with no representable source or timeline length refuses")
    {
        Fixture f { false, 24000.0 };
        const bool reversed = GENERATE (false, true);
        const bool head = GENERATE (false, true);
        f.original.clips.front().reversed = reversed;
        f.original.clips.front().srcLen = 1;
        f.original.clips.front().timelineLength = 2;
        f.copyForNoOverlap (f.original, 1);
        checkNoOverlapRefusal (f, Fixture::start + (head ? 1 : 0));
    }
}

TEST_CASE ("Reversed Clip split preserves rendered order and exact source adjacency", "[reversed-clip][split]")
{
    Fixture f;
    const auto before = f.render (f.original);
    CHECK (before[static_cast<std::size_t> (Fixture::start) * 2u] > before.back());
    CHECK (before.back() > 0.0f);
    CHECK (bool (f.play() == before));
    REQUIRE (f.model.splitSelectedTimelineClipAt (Fixture::start + 3203).dispatched);
    CHECK (bool (f.render (f.model.project()) == before));
    CHECK (bool (f.play() == before));
    const auto& clips = f.model.project().clips;
    REQUIRE (clips.size() == 2);
    CHECK (clips[0].id == entity (4));
    CHECK (clips[1].srcOffset == clips[0].srcOffset + clips[0].srcLen);
    CHECK (clips[1].timelineStart == Fixture::start);
    CHECK (clips[1].timelineLength == 3203);
    CHECK (clips[0].timelineStart == Fixture::start + 3203);
    CHECK (clips[0].timelineLength == 5798);
    const auto split = clips;
    REQUIRE (f.model.dispatch (ui::UiActionId::EditUndo).dispatched);
    CHECK (f.model.project().clips == f.original.clips);
    REQUIRE (f.model.dispatch (ui::UiActionId::EditRedo).dispatched);
    CHECK (f.model.project().clips == split);
    CHECK (bool (f.render (f.model.project()) == before));
    f.durable (split);
    REQUIRE (f.model.selectTimelineClips (std::array { split[0].id, split[1].id }));
    REQUIRE (f.model.dispatch (ui::UiActionId::TimelineClipHeal).dispatched);
    CHECK (f.model.project().clips == f.original.clips);
    CHECK (bool (f.play() == before));
    f.durable (f.original.clips);
}

TEST_CASE ("Reversed Clip repeated splits preserve audio and fade metadata", "[reversed-clip][split]")
{
    Fixture f;
    auto edited = f.original;
    engine::ProjectUndoStack undo;
    REQUIRE (undo.beginTransactionGroup());
    REQUIRE (undo.apply (edited, engine::ProjectEditCommand::splitClip (entity (4), entity (5), 3203, 3203)).applied());
    REQUIRE (undo.apply (edited, engine::ProjectEditCommand::splitClip (entity (4), entity (6), 1701, 1701)).applied());
    REQUIRE (undo.endTransactionGroup());
    CHECK (bool (f.render (edited) == f.render (f.original)));
    CHECK (edited.clips[0].srcOffset + edited.clips[0].srcLen == edited.clips[1].srcOffset);
    CHECK (edited.clips[1].srcOffset + edited.clips[1].srcLen == edited.clips[2].srcOffset);
    REQUIRE (undo.undo (edited) == engine::ProjectUndoStatus::Applied);
    CHECK (edited.clips == f.original.clips);
    REQUIRE (undo.redo (edited) == engine::ProjectUndoStatus::Applied);
    CHECK (bool (f.render (edited) == f.render (f.original)));

    auto faded = f.original;
    faded.clips[0].fadeIn = 200;
    faded.clips[0].fadeOut = 400;
    faded.clips[0].fadeInShape = engine::FadeShape::Linear;
    faded.clips[0].fadeOutShape = engine::FadeShape::Linear;
    REQUIRE (engine::splitClip (faded, entity (4), entity (5), 3203, 3203) == engine::ProjectEditStatus::Applied);
    for (const auto& clip : faded.clips)
    {
        CHECK (clip.fadeIn == 200);
        CHECK (clip.fadeOut == 400);
        CHECK (clip.fadeInShape == engine::FadeShape::Linear);
        CHECK (clip.fadeOutShape == engine::FadeShape::Linear);
        CHECK (engine::evaluateClipGainEnvelope (clip, 0).gain == 0.0f);
        CHECK (engine::evaluateClipGainEnvelope (clip, 100).gain == 0.5f);
        CHECK (engine::evaluateClipGainEnvelope (clip, clip.timelineLength - 200).gain == 0.5f);
    }
    f.unchangedBytes();
}

TEST_CASE ("Splitting a linked RecordingTake refuses atomically without changing Take or Comp", "[reversed-clip][linked-take]")
{
    Fixture f;
    auto project = f.original;
    project.clips[0].reversed = GENERATE (false, true);
    engine::RecordingTake take;
    take.id = entity (6);
    take.assetId = entity (2);
    take.trackId = entity (3);
    take.clipId = entity (4);
    take.timelineStart = Fixture::start;
    take.frameCount = Fixture::length;
    project.recordingTakes.push_back (take);
    project.recordingCompSegments.push_back ({ entity (7), take.id, Fixture::start, 1000, 30 });
    REQUIRE (project.hasValidAssetClipIndirection());
    const auto before = project;
    CHECK (engine::splitClip (project, entity (4), entity (5), 3203, 3203)
           == engine::ProjectEditStatus::InvalidSourceWindow);
    CHECK (project.clips == before.clips);
    CHECK (project.assets == before.assets);
    CHECK (project.recordingTakes == before.recordingTakes);
    CHECK (project.recordingCompSegments == before.recordingCompSegments);
    CHECK (project.hasValidAssetClipIndirection());
}

TEST_CASE ("Join restores reversed source order and refuses mixed directions", "[reversed-clip][join]")
{
    Fixture f;
    auto split = f.original;
    auto low = split.clips.front();
    auto high = low;
    low.timelineStart += 3203;
    low.timelineLength -= 3203;
    low.srcLen -= 3203;
    high.id = entity (5);
    high.timelineLength = 3203;
    high.srcOffset += low.srcLen;
    high.srcLen = 3203;
    split.clips = { low, high };
    SECTION ("same direction")
    {
        f.load (split);
        REQUIRE (f.model.selectTimelineClips (std::array { low.id, high.id }));
        REQUIRE (f.model.dispatch (ui::UiActionId::TimelineClipHeal).dispatched);
        CHECK (f.model.project().clips == f.original.clips);
        CHECK (bool (f.render (f.model.project()) == f.render (f.original)));
        CHECK (f.model.selectedTimelineClipId() == entity (4));
        REQUIRE (f.model.dispatch (ui::UiActionId::EditUndo).dispatched);
        CHECK (f.model.project().clips == split.clips);
        REQUIRE (f.model.dispatch (ui::UiActionId::EditRedo).dispatched);
        f.durable (f.original.clips);
    }
    SECTION ("mixed directions with forward source adjacency")
    {
        split.clips[0] = f.original.clips.front();
        split.clips[0].timelineLength = 3203;
        split.clips[0].srcLen = 3203;
        split.clips[0].reversed = false;
        split.clips[1] = split.clips[0];
        split.clips[1].id = entity (5);
        split.clips[1].timelineStart += 3203;
        split.clips[1].timelineLength = 5798;
        split.clips[1].srcOffset += 3203;
        split.clips[1].srcLen = 5798;
        split.clips[1].reversed = true;
        f.load (split);
        REQUIRE (f.model.selectTimelineClips (std::array { entity (4), entity (5) }));
        const auto result = f.model.dispatch (ui::UiActionId::TimelineClipHeal);
        CHECK_FALSE (result.dispatched);
        CHECK (std::string_view (result.state.disabledReason) == "clip reverse settings differ");
        CHECK (f.model.project().clips == split.clips);
        CHECK_FALSE (f.model.context().canUndo);
        f.durable (split.clips);
    }
}

TEST_CASE ("Strip Silence preserves reversed nonsilent intervals through undo and reopen", "[reversed-clip][strip-silence]")
{
    Fixture f { true };
    const auto before = f.render (f.original);
    CHECK (bool (f.play() == before));
    REQUIRE (f.model.dispatch (ui::UiActionId::TimelineClipStripSilence).dispatched);
    CHECK (bool (f.render (f.model.project()) == before));
    CHECK (bool (f.play() == before));
    auto clips = f.model.project().clips;
    REQUIRE (clips.size() == 2);
    std::sort (clips.begin(), clips.end(), [] (const auto& a, const auto& b) { return a.timelineStart < b.timelineStart; });
    CHECK (clips[0].timelineStart == Fixture::start + 2500);
    CHECK (clips[0].timelineLength == 801);
    CHECK (clips[0].srcOffset == Fixture::offset + 5700);
    CHECK (clips[1].timelineStart == Fixture::start + 5801);
    CHECK (clips[1].timelineLength == 700);
    CHECK (clips[1].srcOffset == Fixture::offset + 2500);
    const auto stripped = f.model.project().clips;
    REQUIRE (f.model.dispatch (ui::UiActionId::EditUndo).dispatched);
    CHECK (f.model.project().clips == f.original.clips);
    REQUIRE (f.model.dispatch (ui::UiActionId::EditRedo).dispatched);
    CHECK (f.model.project().clips == stripped);
    f.durable (stripped);
}

TEST_CASE ("Cross-rate Strip Silence preserves absolute source endpoints and both render tiers",
           "[reversed-clip][strip-silence][cross-rate]")
{
    Fixture f { true, 44100.0, { { 0u, 2501u }, { 3197u, 5703u }, { 6496u, 9001u } } };
    const bool reversed = GENERATE (false, true);
    if (! reversed)
    {
        f.original.clips.front().reversed = false;
        f.load (f.original);
    }
    // Independent integer half-away rounding for the exact 160 / 147 rate ratio.
    const auto mapped = [] (std::uint64_t sourceFrame) { return (sourceFrame * 160u + 73u) / 147u; };
    const auto high = mapped (Fixture::offset + Fixture::length);
    const auto low = mapped (Fixture::offset);
    CHECK (high == 9946u);
    CHECK (high - mapped (Fixture::offset + 6496u) == 2726u);
    CHECK (mapped (2505u) == 2727u);   // rounding a relative distance lands one frame late
    const auto beforeOffline = f.render (f.original);
    const auto beforeLive = f.play();
    CHECK (bool (beforeLive == f.render (f.original, engine::ResampleQuality::LivePlayback)));
    auto expectedOffline = beforeOffline;
    auto expectedLive = beforeLive;
    for (const auto& [begin, end] : { std::pair { 0u, 2501u }, { 3197u, 5703u }, { 6496u, 9001u } })
    {
        const auto first = static_cast<std::ptrdiff_t> (
            (static_cast<std::uint64_t> (Fixture::start) + (reversed
                ? high - mapped (Fixture::offset + end) : mapped (Fixture::offset + begin) - low)) * 2u);
        const auto last = static_cast<std::ptrdiff_t> (
            (static_cast<std::uint64_t> (Fixture::start) + (reversed
                ? high - mapped (Fixture::offset + begin) : mapped (Fixture::offset + end) - low)) * 2u);
        std::fill (expectedOffline.begin() + first, expectedOffline.begin() + last, 0.0f);
        std::fill (expectedLive.begin() + first, expectedLive.begin() + last, 0.0f);
    }
    REQUIRE (f.model.dispatch (ui::UiActionId::TimelineClipStripSilence).dispatched);
    auto clips = f.model.project().clips;
    REQUIRE (clips.size() == 2);
    std::sort (clips.begin(), clips.end(), [] (const auto& a, const auto& b) { return a.timelineStart < b.timelineStart; });
    std::array kept { std::pair { 2501u, 3197u }, std::pair { 5703u, 6496u } };
    if (reversed)
        std::reverse (kept.begin(), kept.end());
    std::size_t index = 0;
    for (const auto& [begin, end] : kept)
    {
        const auto& clip = clips[index++];
        CHECK (clip.srcOffset == Fixture::offset + begin);
        CHECK (clip.srcLen == end - begin);
        CHECK (clip.timelineStart == Fixture::start + static_cast<engine::Tick> (reversed
            ? high - mapped (Fixture::offset + end) : mapped (Fixture::offset + begin) - low));
        CHECK (clip.timelineLength == static_cast<engine::Tick> (
            mapped (Fixture::offset + end) - mapped (Fixture::offset + begin)));
    }
    // Each tier preserves its own original retained PCM; removing a run also removes resampler ringing there.
    CHECK (bool (f.render (f.model.project()) == expectedOffline));
    CHECK (bool (f.play() == expectedLive));
    const auto stripped = f.model.project().clips;
    REQUIRE (f.model.dispatch (ui::UiActionId::EditUndo).dispatched);
    CHECK (f.model.project().clips == f.original.clips);
    CHECK (bool (f.play() == beforeLive));
    REQUIRE (f.model.dispatch (ui::UiActionId::EditRedo).dispatched);
    CHECK (f.model.project().clips == stripped);
    CHECK (bool (f.play() == expectedLive));
    f.durable (stripped);
}

TEST_CASE ("Natural cross-rate model split quantizes to an absolute Asset boundary without changing audio",
           "[reversed-clip][split][cross-rate]")
{
    Fixture f { false, 44100.0 };
    const bool reversed = GENERATE (false, true);
    if (! reversed)
    {
        f.original.clips.front().reversed = false;
        f.load (f.original);
    }
    const auto mapped = [] (std::uint64_t frame) { return (frame * 160u + 73u) / 147u; };
    const engine::Tick requested = GENERATE (2727, 2728);
    // Reversed requests both choose boundary 6632 / local Tick 2727. The second request
    // therefore quantizes one Tick. Forward requests in this fixture are both representable.
    const auto absoluteRequest = reversed
        ? mapped (Fixture::offset + Fixture::length) - static_cast<std::uint64_t> (requested)
        : mapped (Fixture::offset) + static_cast<std::uint64_t> (requested);
    const std::uint64_t boundary = (absoluteRequest * 147u + 80u) / 160u;
    const auto expectedLeft = static_cast<engine::Tick> (reversed
        ? mapped (Fixture::offset + Fixture::length) - mapped (boundary)
        : mapped (boundary) - mapped (Fixture::offset));
    REQUIRE (boundary == (reversed ? 6632u : (requested == 2727 ? 2642u : 2643u)));
    REQUIRE (expectedLeft == (reversed ? 2727 : requested));
    const auto beforeOffline = f.render (f.original);
    const auto beforeLive = f.play();
    REQUIRE (f.model.splitSelectedTimelineClipAt (Fixture::start + requested).dispatched);
    const auto& clips = f.model.project().clips;
    REQUIRE (clips.size() == 2);
    CHECK (clips[0].id == entity (4));
    CHECK (clips[0].srcOffset == Fixture::offset);
    CHECK (clips[0].srcLen == boundary - Fixture::offset);
    CHECK (clips[1].srcOffset == boundary);
    CHECK (clips[1].srcLen == Fixture::offset + Fixture::length - boundary);
    const auto& temporalLeft = clips[reversed ? 1u : 0u];
    const auto& temporalRight = clips[reversed ? 0u : 1u];
    CHECK (temporalLeft.timelineLength == expectedLeft);
    CHECK (temporalRight.timelineStart == Fixture::start + expectedLeft);
    CHECK (temporalRight.timelineLength == f.original.clips.front().timelineLength - expectedLeft);
    CHECK (std::abs (temporalLeft.timelineLength - requested) <= 1);
    CHECK (bool (f.render (f.model.project()) == beforeOffline));
    CHECK (bool (f.play() == beforeLive));
    const auto split = clips;
    REQUIRE (f.model.dispatch (ui::UiActionId::EditUndo).dispatched);
    CHECK (f.model.project().clips == f.original.clips);
    REQUIRE (f.model.dispatch (ui::UiActionId::EditRedo).dispatched);
    CHECK (f.model.project().clips == split);
    CHECK (bool (f.play() == beforeLive));
    f.durable (split);
}

TEST_CASE ("Reversed Clip range boundaries preserve the copied and remaining audio", "[reversed-clip][range]")
{
    Fixture f;
    constexpr engine::Tick begin = Fixture::start + 1701;
    constexpr engine::Tick end = Fixture::start + 6203;
    const auto before = f.render (f.original);
    REQUIRE (f.model.setTimelineRangeSelection (begin, end));
    SECTION ("split edges")
    {
        REQUIRE (f.model.dispatch (ui::UiActionId::TimelineRangeSplitEdges).dispatched);
        REQUIRE (f.model.project().clips.size() == 3);
        CHECK (bool (f.render (f.model.project()) == before));
        const auto split = f.model.project().clips;
        REQUIRE (f.model.dispatch (ui::UiActionId::EditUndo).dispatched);
        CHECK (f.model.project().clips == f.original.clips);
        REQUIRE (f.model.dispatch (ui::UiActionId::EditRedo).dispatched);
        f.durable (split);
    }
    SECTION ("copy and paste at the original position")
    {
        REQUIRE (f.model.dispatch (ui::UiActionId::TimelineRangeCopy).dispatched);
        CHECK (f.model.project().clips == f.original.clips);
        REQUIRE (f.model.locatePlaybackFrame (begin));
        REQUIRE (f.model.dispatch (ui::UiActionId::TimelineClipPaste).dispatched);
        REQUIRE (f.model.project().clips.size() == 2);
        const auto& copy = f.model.project().clips.back();
        CHECK (copy.timelineStart == begin);
        CHECK (copy.timelineLength == end - begin);
        CHECK (copy.srcOffset == Fixture::offset + Fixture::length - 6203);
        CHECK (copy.srcLen == 4502);
        CHECK (copy.reversed);
        auto copiedOnly = f.model.project();
        copiedOnly.clips.erase (copiedOnly.clips.begin());
        auto expected = before;
        std::fill (expected.begin(), expected.begin() + static_cast<std::ptrdiff_t> (begin * 2), 0.0f);
        std::fill (expected.begin() + static_cast<std::ptrdiff_t> (end * 2), expected.end(), 0.0f);
        CHECK (bool (f.render (copiedOnly) == expected));
        f.durable (f.model.project().clips);
    }
    SECTION ("delete the interval")
    {
        REQUIRE (f.model.dispatch (ui::UiActionId::TimelineRangeDelete).dispatched);
        auto expected = before;
        std::fill (expected.begin() + static_cast<std::ptrdiff_t> (begin * 2),
                   expected.begin() + static_cast<std::ptrdiff_t> (end * 2), 0.0f);
        CHECK (bool (f.render (f.model.project()) == expected));
        const auto remaining = f.model.project().clips;
        REQUIRE (f.model.dispatch (ui::UiActionId::EditUndo).dispatched);
        CHECK (f.model.project().clips == f.original.clips);
        REQUIRE (f.model.dispatch (ui::UiActionId::EditRedo).dispatched);
        f.durable (remaining);
    }
}
