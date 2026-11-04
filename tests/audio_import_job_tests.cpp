// YES DAW - pure owned import worker checks. Successful audio always comes from real float32 WAV files.
#include "app/AudioImportJob.h"
#include "io/WavFile.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace {
using namespace yesdaw;

engine::EntityId id (std::uint64_t low)
{
    return engine::EntityId::fromBigEndianParts (1, low);
}

io::AudioDecodeResult decodeWav (const std::filesystem::path& path)
{
    const auto* format = io::importAudioFormatForPath (path);
    if (format == nullptr)
        return { std::nullopt, "unsupported format (" + io::lowerCaseExtension (path) + ")" };
    if (format->kind != io::ImportAudioFormatKind::Wav)
        return { std::nullopt, "not a readable " + std::string (format->name) + " file" };
    io::Float32Wav wav;
    const auto read = io::readFloat32WavFile (path, wav);
    if (! read.ok())
        return { std::nullopt, read.message };
    return { io::DecodedAudioFile { wav.sampleRate.hz, wav.frames, wav.channels,
                                   std::move (wav.interleavedSamples), format->kind }, {} };
}

struct Fixture
{
    std::filesystem::path root = std::filesystem::temp_directory_path()
        / ("yesdaw-audio-import-job-" + std::to_string (std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::path bundle = root / "song.yesdaw";

    Fixture()
    {
        persistence::ProjectBundleDb db;
        REQUIRE (persistence::ProjectBundleDb::openOrCreateBundle (bundle, db).ok());
    }
    ~Fixture()
    {
        std::error_code ignored;
        std::filesystem::remove_all (root, ignored);
    }

    std::filesystem::path wav (const char* name, double rate = 48'000.0, std::uint16_t channels = 2,
                               std::uint64_t frames = 1'001)
    {
        std::vector<float> samples (static_cast<std::size_t> (frames) * channels);
        for (std::size_t index = 0; index < samples.size(); ++index)
            samples[index] = static_cast<float> (0.3 * std::sin (static_cast<double> (index) * 0.071));
        const auto path = root / name;
        REQUIRE (io::writeFloat32WavFile (path, engine::SampleRate { rate }, channels, frames, samples).ok());
        return path;
    }

    app::AudioImportRequest request (std::vector<std::filesystem::path> paths)
    {
        app::AudioImportRequest request;
        request.key = { 41, 7, 3, id (1), 12345, { id (2), id (3) }, true };
        request.destinationBundle = bundle;
        request.stageId = id (40);
        request.viewRateHz = 48'000.0;
        request.decoder = decodeWav;
        for (const auto& path : paths)
            request.files.push_back ({ path, id (100 + request.files.size()) });
        return request;
    }

    std::filesystem::path stage() const
    {
        return bundle / "audio" / (".import-" + persistence::detail::hexBytes (id (40).bytes));
    }

    void checkNoAdoption() const
    {
        persistence::ProjectBundleDb db;
        REQUIRE (persistence::ProjectBundleDb::openExistingBundle (bundle, db).ok());
        sqlite3_int64 rows = -1;
        REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM assets;", rows).ok());
        CHECK (rows == 0);
        REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM pending_fs_ops;", rows).ok());
        CHECK (rows == 0);
        for (const auto& entry : std::filesystem::directory_iterator (bundle / "audio"))
            CHECK (entry.is_directory()); // no canonical file appeared
    }
};

template <typename Predicate>
bool await (Predicate predicate)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds (20);
    while (! predicate() && std::chrono::steady_clock::now() < end)
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    return predicate();
}

void terminal (app::AudioImportJob& job)
{
    REQUIRE (await ([&] { return job.terminal(); }));
}

struct DecoderGate
{
    std::atomic<unsigned> calls { 0 };
    unsigned holdAtCall = 1;
    std::atomic<bool> entered { false };
    std::atomic<bool> release { false };
    std::atomic<bool> unwound { false };
    std::filesystem::path snapshot;
};
std::atomic<DecoderGate*> decoderGate { nullptr };

struct GateBinding
{
    explicit GateBinding (DecoderGate& gate) { decoderGate.store (&gate); }
    ~GateBinding() { decoderGate.store (nullptr); }
};

// Declared after a job, so a failed REQUIRE releases the decoder before the job destructor joins it.
struct ReleaseGate
{
    DecoderGate& gate;
    ~ReleaseGate() { gate.release.store (true); }
};

struct StagingCopyGate
{
    std::atomic<bool> entered { false };
    std::atomic<bool> release { false };
    std::atomic<bool> timedOut { false };

    static void afterFirstChunk (void* context) noexcept
    {
        auto& gate = *static_cast<StagingCopyGate*> (context);
        gate.entered.store (true, std::memory_order_release);
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds (20);
        while (! gate.release.load (std::memory_order_acquire))
        {
            if (std::chrono::steady_clock::now() >= end)
            {
                gate.timedOut.store (true, std::memory_order_release);
                return;
            }
            std::this_thread::sleep_for (std::chrono::milliseconds (1));
        }
    }
};

// All referenced storage outlives this guard. A failed REQUIRE cancels, unblocks and joins before
// the hook is cleared, so neither a held copy nor an unexpectedly reached decoder can strand a worker.
struct StagingCopyBinding
{
    app::AudioImportJob& job;
    StagingCopyGate& copy;
    DecoderGate& decoder;
    const persistence::detail::StagingCopyHookForTest hook { StagingCopyGate::afterFirstChunk, &copy };

    StagingCopyBinding (app::AudioImportJob& ownedJob, StagingCopyGate& copyGate, DecoderGate& decoderBarrier)
        : job (ownedJob), copy (copyGate), decoder (decoderBarrier)
    {
        persistence::detail::stagingCopyHookForTest.store (&hook, std::memory_order_release);
    }

    ~StagingCopyBinding()
    {
        job.requestCancel();
        copy.release.store (true, std::memory_order_release);
        decoder.release.store (true, std::memory_order_release);
        job.join();
        persistence::detail::stagingCopyHookForTest.store (nullptr, std::memory_order_release);
    }
};

io::AudioDecodeResult heldDecode (const std::filesystem::path& path)
{
    DecoderGate& gate = *decoderGate.load();
    if (gate.calls.fetch_add (1) + 1 != gate.holdAtCall)
        return decodeWav (path);
    struct Unwind
    {
        DecoderGate& gate;
        ~Unwind() { gate.unwound.store (true, std::memory_order_release); }
    } unwind { gate };
    gate.snapshot = path;
    gate.entered.store (true, std::memory_order_release);
    while (! gate.release.load (std::memory_order_acquire))
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    return decodeWav (path);
}

io::AudioDecodeResult nonfiniteDecode (const std::filesystem::path& path)
{
    auto decoded = decodeWav (path);
    if (decoded.audio && ! decoded.audio->interleaved.empty())
        decoded.audio->interleaved.front() = std::numeric_limits<float>::quiet_NaN();
    return decoded;
}

io::AudioDecodeResult nonfiniteRateDecode (const std::filesystem::path& path)
{
    auto decoded = decodeWav (path);
    if (decoded.audio)
        decoded.audio->sampleRateHz = path.extension() == ".wave"
            ? std::numeric_limits<double>::infinity() : std::numeric_limits<double>::quiet_NaN();
    return decoded;
}

io::AudioDecodeResult conflictingDecode (const std::filesystem::path& path)
{
    auto decoded = decodeWav (path);
    if (decoded.audio && path.extension() == ".wave")
        decoded.audio->sampleRateHz = 44'100.0;
    return decoded;
}

io::AudioDecodeResult throwingDecode (const std::filesystem::path& path)
{
    const auto decoded = decodeWav (path);
    if (decoded.audio)
        throw std::runtime_error ("codec exception after real decode");
    return decoded;
}

} // namespace

TEST_CASE ("Audio import hands off immutable real WAV audio and preserves request identity", "[audio-import-job]")
{
    Fixture fixture;
    const auto first = fixture.wav ("first.wav");
    const auto second = fixture.wav ("second.wav", 44'100.0, 1);
    const auto request = fixture.request ({ first, second });
    const auto decoded = decodeWav (second);
    REQUIRE (decoded.audio);
    const auto reference = engine::buildRateMatchedSamples (decoded.audio->interleaved, 1, 44'100.0,
                                                           48'000.0, engine::ResampleQuality::LivePlayback);
    app::AudioImportJob job (request);
    CHECK (job.state() == app::AudioImportState::Preparing);
    CHECK (job.phase() == app::AudioImportPhase::Waiting);
    CHECK (job.totalFiles() == 2);
    CHECK (job.completedFiles() == 0);
    CHECK_FALSE (job.eligibleForAdoption());
    CHECK_FALSE (job.takeResult());
    REQUIRE (job.start());
    terminal (job);
    REQUIRE (job.state() == app::AudioImportState::Succeeded);
    REQUIRE (job.eligibleForAdoption());
    CHECK (job.phase() == app::AudioImportPhase::Finished);
    CHECK (job.completedFiles() == 2);
    auto result = job.takeResult(); // all handoff assertions precede join
    REQUIRE (result);
    CHECK_FALSE (job.takeResult());
    CHECK_FALSE (job.eligibleForAdoption());
    CHECK (result->key == request.key);
    CHECK (job.key() == request.key);
    CHECK (result->viewRateHz == 48'000.0);
    REQUIRE (result->prepared.size() == 2);
    REQUIRE (result->items.size() == 2);
    CHECK (result->items[0].sourcePath == first);
    CHECK (result->items[1].sourcePath == second);
    CHECK (result->items[0].preparedIndex == 0);
    CHECK (result->items[1].preparedIndex == 1);
    CHECK (result->prepared[0].source == result->prepared[0].view);
    const auto& cross = result->prepared[1];
    CHECK (cross.preparedAsset.asset().id == request.files[1].assetId);
    CHECK (cross.preparedAsset.originalFilename() == second.filename());
    CHECK (cross.preparedAsset.asset().frames == 1'001);
    CHECK (cross.preparedAsset.asset().sampleRate.hz == 44'100.0);
    CHECK (cross.source->frames == 1'001);
    CHECK (cross.source->interleaved == decoded.audio->interleaved);
    CHECK (cross.view->frames == 1'090);
    CHECK (cross.view->interleaved == reference->interleaved);
    static_assert (std::is_const_v<typename decltype (cross.source)::element_type>);
    static_assert (! std::is_copy_constructible_v<app::AudioImportResult>);
    fixture.checkNoAdoption();
    job.join();
    CHECK_FALSE (job.start());
    result.reset();
    CHECK_FALSE (std::filesystem::exists (fixture.stage()));
}

TEST_CASE ("Audio import folds verified duplicates after each original format policy", "[audio-import-job]")
{
    Fixture fixture;
    const auto first = fixture.wav ("same.wav");
    std::vector<std::filesystem::path> paths { first };
    for (const char* name : { "same.wave", "renamed.aiff", "renamed.flac", "wrong.txt" })
    {
        paths.push_back (fixture.root / name);
        REQUIRE (std::filesystem::copy_file (first, paths.back()));
    }
    paths.push_back (fixture.root / "missing.wav");
    app::AudioImportJob job (fixture.request (paths));
    REQUIRE (job.start());
    terminal (job);
    auto result = job.takeResult();
    REQUIRE (result);
    REQUIRE (result->items.size() == 6);
    REQUIRE (result->prepared.size() == 1);
    CHECK (result->items[0].preparedIndex == 0);
    CHECK (result->items[1].preparedIndex == result->items[0].preparedIndex);
    CHECK (result->items[0].reason.empty());
    CHECK (result->items[1].reason.empty());
    CHECK (result->items[2].reason == "not a readable AIFF file");
    CHECK (result->items[3].reason == "not a readable FLAC file");
    CHECK (result->items[4].reason == "unsupported format (.txt)");
    CHECK (result->items[5].reason.find ("cannot read import source") != std::string::npos);
    for (std::size_t index = 0; index < paths.size(); ++index)
    {
        CHECK (result->items[index].sourcePath == paths[index]);
        if (index > 1)
            CHECK_FALSE (result->items[index].preparedIndex);
    }
    const auto& one = result->prepared[*result->items[0].preparedIndex];
    const auto& repeated = result->prepared[*result->items[1].preparedIndex];
    CHECK (one.source == repeated.source);
    CHECK (one.view == repeated.view);
    CHECK (one.preparedAsset.asset().id == id (100));
    CHECK (std::filesystem::exists (fixture.stage() / "item-0.wav"));
    for (std::size_t index = 1; index < paths.size(); ++index)
        CHECK_FALSE (std::filesystem::exists (fixture.stage() / ("item-" + std::to_string (index) + paths[index].extension().string())));
    fixture.checkNoAdoption();
    job.join();
    result.reset();
    CHECK_FALSE (std::filesystem::exists (fixture.stage()));
}

TEST_CASE ("Audio import hashes and decodes its owned snapshot after the original changes", "[audio-import-job]")
{
    Fixture fixture;
    const auto original = fixture.wav ("original.wav", 96'000.0, 1);
    const auto reference = decodeWav (original);
    REQUIRE (reference.audio);
    engine::AssetContentHash originalHash;
    REQUIRE (persistence::detail::hashFile (original, originalHash).ok());
    DecoderGate gate;
    GateBinding binding (gate);
    std::unique_ptr<app::AudioImportResult> result;
    std::weak_ptr<const engine::AssetSamples> source;
    {
        auto request = fixture.request ({ original });
        request.decoder = heldDecode;
        app::AudioImportJob job (std::move (request));
        ReleaseGate release { gate };
        REQUIRE (job.start());
        REQUIRE (await ([&] { return gate.entered.load (std::memory_order_acquire); }));
        REQUIRE (std::filesystem::equivalent (gate.snapshot, fixture.stage() / "item-0.wav"));
        CHECK (job.phase() == app::AudioImportPhase::Decoding);
        SECTION ("original removed") { REQUIRE (std::filesystem::remove (original)); }
        SECTION ("original replaced") { (void) fixture.wav ("original.wav", 8'000.0, 2, 27); }
        gate.release.store (true);
        terminal (job);
        REQUIRE (gate.unwound.load (std::memory_order_acquire));
        result = job.takeResult();
        REQUIRE (result);
        REQUIRE (result->prepared.size() == 1);
        CHECK (result->prepared[0].preparedAsset.asset().contentHash == originalHash);
        CHECK (result->prepared[0].source->interleaved == reference.audio->interleaved);
        CHECK (result->prepared[0].view->frames == 501);
        source = result->prepared[0].source;
        fixture.checkNoAdoption();
    }
    REQUIRE_FALSE (source.expired());
    REQUIRE (std::filesystem::exists (gate.snapshot)); // move-only handoff retains the private lease
    result.reset();
    CHECK (source.expired());
    CHECK_FALSE (std::filesystem::exists (fixture.stage()));
}

TEST_CASE ("Audio import preserves source frames and existing live conversion at supported rates", "[audio-import-job]")
{
    Fixture fixture;
    struct RateCase { double rate; std::uint64_t expected; };
    const std::array<RateCase, 4> cases {{ { 8'000.0, 6'006 }, { 44'100.0, 1'090 },
                                        { 96'000.0, 501 }, { 384'000.0, 125 } }};
    std::vector<std::filesystem::path> paths;
    for (std::size_t index = 0; index < cases.size(); ++index)
        paths.push_back (fixture.wav (("rate-" + std::to_string (index) + ".wav").c_str(),
                                     cases[index].rate, index % 2 == 0 ? 1 : 2));
    app::AudioImportJob job (fixture.request (paths));
    REQUIRE (job.start());
    terminal (job);
    auto result = job.takeResult();
    REQUIRE (result);
    REQUIRE (result->prepared.size() == cases.size());
    for (std::size_t index = 0; index < cases.size(); ++index)
    {
        const auto& prepared = result->prepared[index];
        CHECK (prepared.preparedAsset.asset().frames == 1'001);
        CHECK (prepared.preparedAsset.asset().sampleRate.hz == cases[index].rate);
        CHECK (prepared.source->frames == 1'001);
        CHECK (prepared.view->frames == cases[index].expected);
        const auto reference = engine::buildRateMatchedSamples (
            prepared.source->interleaved, prepared.preparedAsset.asset().channels,
            cases[index].rate, 48'000.0, engine::ResampleQuality::LivePlayback);
        CHECK (prepared.view->interleaved == reference->interleaved);
        const auto left = engine::mapWindowToView (0, 333, 48'000.0 / cases[index].rate, prepared.view->frames);
        const auto right = engine::mapWindowToView (333, 668, 48'000.0 / cases[index].rate, prepared.view->frames);
        CHECK (left.srcOffset + left.srcLen == right.srcOffset);
        CHECK (left.srcLen + right.srcLen == prepared.view->frames);
    }
    job.join();
}

TEST_CASE ("Audio import cancellation before start creates nothing and remains one shot", "[audio-import-job]")
{
    Fixture fixture;
    app::AudioImportJob job (fixture.request ({ fixture.wav ("cancel.wav") }));
    job.requestCancel();
    REQUIRE (job.start());
    terminal (job);
    CHECK (job.state() == app::AudioImportState::Cancelled);
    CHECK (job.cancelRequested());
    CHECK_FALSE (job.eligibleForAdoption());
    CHECK_FALSE (job.takeResult());
    CHECK_FALSE (job.start());
    CHECK_FALSE (std::filesystem::exists (fixture.stage()));
    fixture.checkNoAdoption();
    job.join();
}

TEST_CASE ("Audio import retires a held decoder and unwinds every snapshot before terminal", "[audio-import-job]")
{
    Fixture fixture;
    const auto first = fixture.wav ("first.wav");
    const auto second = fixture.wav ("second.wav", 96'000.0, 1);
    DecoderGate gate;
    SECTION ("first codec is held") { gate.holdAtCall = 1; }
    SECTION ("an earlier prepared item must also unwind") { gate.holdAtCall = 2; }
    GateBinding binding (gate);
    auto request = fixture.request ({ first, second });
    request.decoder = heldDecode;
    std::vector<std::unique_ptr<app::AudioImportJob>> retiring;
    retiring.push_back (std::make_unique<app::AudioImportJob> (std::move (request)));
    ReleaseGate release { gate };
    auto& job = *retiring.front();
    REQUIRE (job.start());
    REQUIRE (await ([&] { return gate.entered.load (std::memory_order_acquire); }));
    CHECK (job.completedFiles() == gate.holdAtCall - 1);
    REQUIRE (std::filesystem::exists (gate.snapshot));
    job.requestCancel();
    CHECK (job.cancelRequested());
    CHECK_FALSE (job.terminal());
    CHECK_FALSE (job.takeResult());
    CHECK_FALSE (job.eligibleForAdoption());
    CHECK (job.phase() == app::AudioImportPhase::Decoding);
    gate.release.store (true);
    terminal (job);
    CHECK (gate.unwound.load (std::memory_order_acquire));
    CHECK (job.state() == app::AudioImportState::Cancelled);
    CHECK_FALSE (std::filesystem::exists (gate.snapshot));
    CHECK_FALSE (std::filesystem::exists (fixture.stage())); // checked before join can hide premature terminal
    fixture.checkNoAdoption();
    job.join();
}

TEST_CASE ("Audio import final teardown cancels and joins a held codec without detaching", "[audio-import-job]")
{
    Fixture fixture;
    DecoderGate gate;
    GateBinding binding (gate);
    auto request = fixture.request ({ fixture.wav ("held.wav") });
    request.decoder = heldDecode;
    auto job = std::make_unique<app::AudioImportJob> (std::move (request));
    ReleaseGate release { gate };
    REQUIRE (job->start());
    REQUIRE (await ([&] { return gate.entered.load (std::memory_order_acquire); }));
    std::atomic<bool> destructionStarted { false };
    std::atomic<bool> destructionFinished { false };
    std::thread teardown ([owned = std::move (job), &destructionStarted, &destructionFinished]() mutable {
        destructionStarted.store (true);
        owned.reset();
        destructionFinished.store (true);
    });
    struct JoinOnExit
    {
        std::thread& thread;
        ~JoinOnExit() { if (thread.joinable()) thread.join(); }
    } joinOnExit { teardown };
    // Release the decoder before joining even when an assertion fails.
    ReleaseGate releaseBeforeJoin { gate };
    REQUIRE (await ([&] { return destructionStarted.load(); }));
    CHECK_FALSE (destructionFinished.load());
    gate.release.store (true);
    teardown.join();
    CHECK (destructionFinished.load());
    CHECK (gate.unwound.load());
    CHECK_FALSE (std::filesystem::exists (fixture.stage()));
    fixture.checkNoAdoption();
}

TEST_CASE ("Audio import names all per-file refusals and leaves no private or canonical audio", "[audio-import-job]")
{
    Fixture fixture;
    const auto corrupt = fixture.root / "corrupt.wav";
    { std::ofstream out (corrupt); out << "broken RIFF"; }
    std::vector<std::filesystem::path> paths { corrupt, fixture.root / "absent.wav",
        fixture.wav ("surround.wav", 48'000.0, 3), fixture.wav ("low.wav", 4'000.0),
        fixture.wav ("high.wav", 385'000.0), fixture.wav ("empty.wav", 48'000.0, 1, 0) };
    app::AudioImportJob job (fixture.request (paths));
    REQUIRE (job.start());
    terminal (job);
    auto result = job.takeResult();
    REQUIRE (result);
    CHECK (result->prepared.empty());
    REQUIRE (result->items.size() == paths.size());
    CHECK (result->items[2].reason == "3 channels (mono or stereo only)");
    CHECK (result->items[3].reason == "unsupported sample rate (4000 Hz); supported range is 8000 to 384000 Hz");
    CHECK (result->items[4].reason == "unsupported sample rate (385000 Hz); supported range is 8000 to 384000 Hz");
    CHECK (result->items[5].reason == "no audio in the file");
    for (std::size_t index = 0; index < paths.size(); ++index)
    {
        CHECK (result->items[index].sourcePath == paths[index]);
        CHECK_FALSE (result->items[index].reason.empty());
        CHECK_FALSE (result->items[index].preparedIndex);
    }
    CHECK (job.completedFiles() == paths.size());
    CHECK_FALSE (std::filesystem::exists (fixture.stage()));
    fixture.checkNoAdoption();
    job.join();
}

TEST_CASE ("Audio import names non-finite source rates without adopting audio", "[audio-import-job]")
{
    Fixture fixture;
    auto request = fixture.request ({ fixture.wav ("nan.wav"), fixture.wav ("infinite.wave") });
    request.decoder = nonfiniteRateDecode;
    app::AudioImportJob job (std::move (request));
    REQUIRE (job.start());
    terminal (job);
    auto result = job.takeResult();
    REQUIRE (result);
    REQUIRE (result->items.size() == 2);
    CHECK (result->prepared.empty());
    CHECK (result->items[0].reason.starts_with ("unsupported sample rate (nan"));
    CHECK (result->items[1].reason.starts_with ("unsupported sample rate (inf"));
    for (const auto& item : result->items)
        CHECK (item.reason.ends_with ("Hz); supported range is 8000 to 384000 Hz"));
    fixture.checkNoAdoption();
}

TEST_CASE ("Audio import validates decoded samples and duplicate metadata before publishing", "[audio-import-job]")
{
    Fixture fixture;
    const auto first = fixture.wav ("same.wav");
    const auto repeated = fixture.root / "same.wave";
    REQUIRE (std::filesystem::copy_file (first, repeated));
    auto request = fixture.request ({ first, repeated });
    SECTION ("non-finite codec output") { request.decoder = nonfiniteDecode; }
    SECTION ("same hash with conflicting decoded metadata") { request.decoder = conflictingDecode; }
    app::AudioImportJob job (std::move (request));
    REQUIRE (job.start());
    terminal (job);
    auto result = job.takeResult();
    REQUIRE (result);
    REQUIRE (result->items.size() == 2);
    if (result->prepared.empty())
    {
        CHECK (result->items[0].reason == "decoded audio contains non-finite samples");
        CHECK (result->items[1].reason == "decoded audio contains non-finite samples");
        CHECK_FALSE (std::filesystem::exists (fixture.stage()));
    }
    else
    {
        REQUIRE (result->prepared.size() == 1);
        CHECK (result->items[0].preparedIndex == 0);
        CHECK_FALSE (result->items[1].preparedIndex);
        CHECK (result->items[1].reason == "same content hash has conflicting Asset metadata");
        CHECK_FALSE (std::filesystem::exists (fixture.stage() / "item-1.wave"));
    }
    fixture.checkNoAdoption();
    job.join();
}

TEST_CASE ("Audio import contains start and codec failures and cancellation invalidates completed handoffs", "[audio-import-job]")
{
    Fixture fixture;
    auto request = fixture.request ({ fixture.wav ("one.wav") });
    bool succeeds = false;
    SECTION ("no decoder") { request.decoder = nullptr; }
    SECTION ("invalid live rate") { request.viewRateHz = std::numeric_limits<double>::infinity(); }
    SECTION ("missing destination") { request.destinationBundle = fixture.root / "missing.yesdaw"; }
    SECTION ("invalid stage ID") { request.stageId = {}; }
    SECTION ("codec throws") { request.decoder = throwingDecode; }
    SECTION ("cancellation after success preserves the result but forbids adoption") { succeeds = true; }
    app::AudioImportJob job (std::move (request));
    REQUIRE (job.start());
    terminal (job);
    CHECK_FALSE (job.start());
    if (succeeds)
    {
        REQUIRE (job.eligibleForAdoption());
        job.requestCancel();
        CHECK_FALSE (job.eligibleForAdoption());
        CHECK (job.state() == app::AudioImportState::Succeeded);
        auto result = job.takeResult();
        REQUIRE (result);
        CHECK (result->prepared.size() == 1);
    }
    else
    {
        CHECK (job.state() == app::AudioImportState::Failed);
        CHECK_FALSE (job.failureReason().empty());
        CHECK_FALSE (job.takeResult());
    }
    CHECK_FALSE (std::filesystem::exists (fixture.stage()));
    fixture.checkNoAdoption();
    job.join();
}

TEST_CASE ("Staged copy and hash honor owned cancellation without finalizing a proof", "[audio-import-job]")
{
    Fixture fixture;
    const auto source = fixture.wav ("source.wav");
    std::atomic<bool> cancellation { true };
    engine::AssetContentHash unchanged;
    unchanged.bytes.fill (17);
    auto hash = unchanged;
    CHECK_FALSE (persistence::detail::hashFile (source, hash, &cancellation).ok());
    CHECK (hash == unchanged);
    {
        persistence::AssetImportStaging staging;
        REQUIRE (persistence::AssetImportStaging::create (fixture.bundle, id (40), staging).ok());
        persistence::StagedAssetImport snapshot;
        CHECK_FALSE (staging.stageSource (source, 0, snapshot, &cancellation).ok());
        CHECK_FALSE (std::filesystem::exists (fixture.stage() / "item-0.wav"));
    }
    CHECK_FALSE (std::filesystem::exists (fixture.stage()));
}

TEST_CASE ("Audio import cancellation during bounded staging cleans a partially copied file", "[audio-import-job]")
{
    Fixture fixture;
    const auto source = fixture.root / "large.wav";
    constexpr std::uintmax_t totalBytes = 128u * 1024u * 1024u;
    {
        std::ofstream out (source, std::ios::binary);
        std::vector<char> block (1024u * 1024u, 0);
        for (unsigned index = 0; index < 128; ++index)
            out.write (block.data(), static_cast<std::streamsize> (block.size()));
        REQUIRE (out.good());
    }
    DecoderGate gate;
    GateBinding binding (gate);
    auto request = fixture.request ({ source });
    request.decoder = heldDecode;
    app::AudioImportJob job (std::move (request));
    StagingCopyGate copyGate;
    StagingCopyBinding copyBinding (job, copyGate, gate);
    REQUIRE (job.start());
    const auto partial = fixture.stage() / "item-0.wav";
    REQUIRE (await ([&] { return copyGate.entered.load (std::memory_order_acquire); }));
    std::error_code error;
    const auto witnessedBytes = std::filesystem::file_size (partial, error);
    REQUIRE_FALSE (error);
    CHECK (witnessedBytes >= 65'536);
    CHECK (witnessedBytes == 65'536); // the observer can arrive only after the worker is held
    REQUIRE (witnessedBytes < totalBytes);
    job.requestCancel();
    copyGate.release.store (true, std::memory_order_release);
    gate.release.store (true);
    terminal (job);
    CHECK_FALSE (copyGate.timedOut.load (std::memory_order_acquire));
    CHECK (job.state() == app::AudioImportState::Cancelled);
    CHECK (gate.calls.load() == 0); // cancellation interrupted bulk copy/hash before the codec
    CHECK_FALSE (std::filesystem::exists (partial));
    CHECK_FALSE (std::filesystem::exists (fixture.stage()));
    CHECK_FALSE (job.takeResult());
    fixture.checkNoAdoption();
    job.join();
}
