// Background import adoption uses real WAV bytes and the pure model, never a device or native shell.
#include "ui/UiAppModel.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace {
using namespace yesdaw;

io::AudioDecodeResult decodeWav (const std::filesystem::path& path)
{
    io::Float32Wav wav;
    const auto read = io::readFloat32WavFile (path, wav);
    if (! read.ok())
        return { std::nullopt, read.message };
    return { io::DecodedAudioFile { wav.sampleRate.hz, wav.frames, wav.channels,
                                   std::move (wav.interleavedSamples), io::ImportAudioFormatKind::Wav }, {} };
}

template <typename Predicate>
bool await (Predicate predicate)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds (20);
    while (! predicate() && std::chrono::steady_clock::now() < end)
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    return predicate();
}

struct Fixture
{
    std::filesystem::path root = std::filesystem::temp_directory_path()
        / ("yesdaw-import-model-" + std::to_string (std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::path bundle = root / "song.yesdaw";
    struct Cleanup
    {
        std::filesystem::path path;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove_all (path, ignored); }
    } cleanup { root }; // destroyed after the model closes its handles and joins owned workers
    ui::UiAppModel model;

    Fixture()
    {
        model.setSessionStateDirectory (root / "state");
        REQUIRE (model.createProjectBundle (bundle).ok());
    }
    std::filesystem::path wav (const char* name, double rate = 48'000.0, float level = 0.25f)
    {
        std::vector<float> samples (1'001, level);
        const auto path = root / name;
        REQUIRE (io::writeFloat32WavFile (path, engine::SampleRate { rate }, 1, samples.size(), samples).ok());
        return path;
    }

    bool finish()
    {
        bool adopted = false;
        REQUIRE (await ([&] { adopted = model.serviceAudioImport() || adopted; return ! model.audioImportBusy(); }));
        return adopted;
    }

    void sql (std::string_view statement)
    {
        persistence::ProjectBundleDb db;
        REQUIRE (persistence::ProjectBundleDb::openExistingBundle (bundle, db).ok());
        REQUIRE (db.executeSql (statement).ok());
    }

    engine::Project persisted()
    {
        persistence::ProjectBundleDb db;
        REQUIRE (persistence::ProjectBundleDb::openExistingBundle (bundle, db).ok());
        engine::Project project;
        REQUIRE (db.readProjectSnapshot (project).ok());
        return project;
    }
};

struct DecoderGate
{
    std::atomic<bool> entered { false };
    std::atomic<bool> release { false };
};
std::atomic<DecoderGate*> decoderGate { nullptr };

struct GateBinding
{
    explicit GateBinding (DecoderGate& gate) { decoderGate.store (&gate); }
    ~GateBinding() { decoderGate.store (nullptr); }
};
struct ReleaseGate
{
    DecoderGate& gate;
    ~ReleaseGate() { gate.release.store (true); }
};
io::AudioDecodeResult heldDecode (const std::filesystem::path& path)
{
    auto& gate = *decoderGate.load();
    gate.entered.store (true);
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds (5);
    while (! gate.release.load() && std::chrono::steady_clock::now() < end)
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    return decodeWav (path);
}
} // namespace

TEST_CASE ("Background import lands captured audio as one durable undo transaction", "[import-model]")
{
    Fixture f;
    const auto firstTrack = f.model.project().tracks.front().id;
    const auto path = f.wav ("first.wav", 44'100.0);
    const auto started = f.model.startAudioImport ({ path, path }, { firstTrack }, 123, true, decodeWav);
    REQUIRE (started.accepted);
    REQUIRE (f.finish());
    const auto completion = f.model.takeAudioImportCompletion();
    REQUIRE (completion.has_value());
    CHECK (completion->allowsMidiTail());
    CHECK (completion->placement.landed == 2);
    CHECK (completion->placement.tracksCreated == 1);
    REQUIRE (f.model.project().clips.size() == 2);
    CHECK (f.model.project().clips[0].trackId == firstTrack);
    CHECK (f.model.project().clips[0].timelineStart == 123);
    CHECK (f.model.project().assets.size() == 1);
    const auto landed = f.model.project().clips;
    REQUIRE (f.model.dispatch (ui::UiActionId::EditUndo).dispatched);
    CHECK (f.model.project().clips.empty());
    CHECK (f.model.project().tracks.size() == 1);
    CHECK (f.model.project().assets.size() == 1);
    REQUIRE (f.model.dispatch (ui::UiActionId::EditRedo).dispatched);
    CHECK (f.model.project().clips == landed);
    CHECK (f.persisted().clips == landed);
    CHECK (completion->ownerBatchMilliseconds >= 0.0);
}

TEST_CASE ("Held background imports preserve current edits and captured Track identities", "[import-model]")
{
    DecoderGate gate;
    GateBinding binding { gate };
    Fixture f;
    ReleaseGate release { gate };
    REQUIRE (f.model.addAudioTrack().dispatched);
    const auto first = f.model.project().tracks[0].id;
    const auto second = f.model.project().tracks[1].id;
    const auto path = f.wav ("held.wav");
    REQUIRE (f.model.startAudioImport ({ path, path, path }, { first, second }, 4'000, true, heldDecode).accepted);
    REQUIRE (await ([&] { return gate.entered.load(); }));
    const auto began = std::chrono::steady_clock::now();
    CHECK_FALSE (f.model.serviceAudioImport());
    REQUIRE (f.model.reorderProjectTrack (first, 1).dispatched);
    REQUIRE (f.model.renameProjectTrack (second, "Kept rename").dispatched);
    REQUIRE (f.model.addAudioTrack().dispatched);
    const auto unrelated = f.model.project().tracks.back().id;
    REQUIRE (f.model.selectMixerTrack (2));
    REQUIRE (f.model.locatePlaybackFrame (9'000));
    CHECK (std::chrono::steady_clock::now() - began < std::chrono::seconds (2));
    CHECK (f.model.project().clips.empty());
    CHECK_FALSE (f.model.startAudioImport ({ path }, { second }, 0, true, decodeWav).accepted);
    gate.release.store (true);
    REQUIRE (f.finish());
    REQUIRE (f.model.project().clips.size() == 3);
    CHECK (f.model.project().clips[0].trackId == first);
    CHECK (f.model.project().clips[1].trackId == second);
    CHECK (f.model.project().clips[2].trackId != unrelated);
    CHECK (f.model.project().tracks.size() == 4);
    CHECK (f.model.project().findTrack (second)->strip.name == "Kept rename");
    for (const auto& clip : f.model.project().clips)
        CHECK (clip.timelineStart == 4'000);
    const auto receipt = f.model.takeAudioImportCompletion();
    REQUIRE (receipt.has_value());
    CHECK (receipt->key.targetTrackIds == std::vector<engine::EntityId> { first, second });
    CHECK (receipt->placement.landed == 3);
    REQUIRE (f.model.dispatch (ui::UiActionId::EditUndo).dispatched);
    CHECK (f.model.project().tracks.size() == 3);
    CHECK (f.model.project().clips.empty());
    CHECK (f.model.project().tracks[0].id == second);
    CHECK (f.model.project().findTrack (second)->strip.name == "Kept rename");
    CHECK (f.model.project().findTrack (unrelated) != nullptr);
}

TEST_CASE ("Deleting and undoing a captured Track permanently retires its import", "[import-model]")
{
    DecoderGate gate;
    GateBinding binding { gate };
    Fixture f;
    ReleaseGate release { gate };
    REQUIRE (f.model.addAudioTrack().dispatched);
    const auto target = f.model.project().tracks.front().id;
    const auto path = f.wav ("retired.wav");
    REQUIRE (f.model.startAudioImport ({ path }, { target }, 0, true, heldDecode).accepted);
    REQUIRE (await ([&] { return gate.entered.load(); }));
    REQUIRE (f.model.removeProjectTrack (target).dispatched);
    CHECK_FALSE (f.model.audioImportPending());
    CHECK (f.model.audioImportRetiring());
    REQUIRE (f.model.dispatch (ui::UiActionId::EditUndo).dispatched);
    REQUIRE (f.model.project().findTrack (target) != nullptr);
    const auto receipt = f.model.takeAudioImportCompletion();
    REQUIRE (receipt.has_value());
    CHECK (receipt->disposition == ui::UiAudioImportDisposition::Stale);
    CHECK_FALSE (receipt->allowsMidiTail());
    const auto status = f.model.statusLineText();
    CHECK_FALSE (f.model.startAudioImport ({ path }, { target }, 0, true, decodeWav).accepted);
    gate.release.store (true);
    CHECK_FALSE (f.finish());
    CHECK (f.model.project().clips.empty());
    CHECK (f.model.statusLineText() == status);
    CHECK_FALSE (f.model.takeAudioImportCompletion().has_value());
}

TEST_CASE ("Failed publication and Save keep captured import intent eligible", "[import-model]")
{
    DecoderGate gate;
    GateBinding binding { gate };
    Fixture f;
    ReleaseGate release { gate };
    REQUIRE (f.model.addAudioTrack().dispatched);
    const auto target = f.model.project().tracks.front().id;
    const auto path = f.wav ("kept.wav");
    REQUIRE (f.model.startAudioImport ({ path }, { target }, 321, true, heldDecode).accepted);
    REQUIRE (await ([&] { return gate.entered.load(); }));
    f.sql ("CREATE TRIGGER refuse_track_delete BEFORE DELETE ON tracks BEGIN SELECT RAISE(ABORT, 'injected write refusal'); END;");
    REQUIRE_FALSE (f.model.removeProjectTrack (target).dispatched);
    CHECK (f.model.audioImportPending());
    f.sql ("DROP TRIGGER refuse_track_delete;");
    REQUIRE (f.model.saveProjectBundle().ok());
    CHECK (f.model.audioImportPending());
    gate.release.store (true);
    REQUIRE (f.finish());
    CHECK (f.model.project().clips.front().trackId == target);
}

TEST_CASE ("Cancellation and Project replacement return without waiting for held import decode", "[import-model]")
{
    DecoderGate gate;
    GateBinding binding { gate };
    Fixture f;
    ReleaseGate release { gate };
    const auto target = f.model.project().tracks.front().id;
    const auto path = f.wav ("cancel.wav");
    REQUIRE (f.model.startAudioImport ({ path }, { target }, 0, true, heldDecode).accepted);
    REQUIRE (await ([&] { return gate.entered.load(); }));
    auto disposition = ui::UiAudioImportDisposition::Cancelled;
    const auto began = std::chrono::steady_clock::now();
    SECTION ("cancel") { REQUIRE (f.model.cancelAudioImport()); }
    SECTION ("replace Project")
    {
        REQUIRE (f.model.createProjectBundle (f.root / "replacement.yesdaw").ok());
        disposition = ui::UiAudioImportDisposition::Stale;
    }
    CHECK (std::chrono::steady_clock::now() - began < std::chrono::seconds (2));
    CHECK (f.model.audioImportRetiring());
    const auto currentTrack = f.model.project().tracks.front().id;
    CHECK_FALSE (f.model.startAudioImport ({ path }, { currentTrack }, 0, true, decodeWav).accepted);
    const auto receipt = f.model.takeAudioImportCompletion();
    REQUIRE (receipt.has_value());
    CHECK (receipt->disposition == disposition);
    CHECK_FALSE (receipt->allowsMidiTail());
    const auto status = f.model.statusLineText();
    gate.release.store (true);
    CHECK_FALSE (f.finish());
    CHECK (f.model.project().clips.empty());
    CHECK (f.model.statusLineText() == status);
    REQUIRE (f.model.startAudioImport ({ path }, { currentTrack }, 0, true, decodeWav).accepted);
    REQUIRE (f.finish());
}

TEST_CASE ("Ordered per-file refusals consume no captured audio slot", "[import-model]")
{
    Fixture f;
    REQUIRE (f.model.addAudioTrack().dispatched);
    const auto first = f.model.project().tracks[0].id;
    const auto second = f.model.project().tracks[1].id;
    const auto path = f.wav ("valid.wav");
    const auto bad = f.root / "bad.wav";
    std::ofstream (bad) << "broken WAV";
    REQUIRE (f.model.startAudioImport ({ bad, path, path }, { first, second }, 456, false, decodeWav).accepted);
    REQUIRE (f.finish());
    const auto receipt = f.model.takeAudioImportCompletion();
    REQUIRE (receipt.has_value());
    CHECK (receipt->allowsMidiTail());
    REQUIRE (receipt->placement.refusals.size() == 1);
    CHECK (receipt->placement.refusals.front().find ("bad.wav:") == 0);
    CHECK (f.model.project().clips[0].trackId == first);
    CHECK (f.model.project().clips[1].trackId == second);
    CHECK (receipt->placement.landed == 2);
    REQUIRE (f.model.startAudioImport ({ bad }, { first }, 456, false, decodeWav).accepted);
    CHECK_FALSE (f.finish());
    const auto refused = f.model.takeAudioImportCompletion();
    REQUIRE (refused.has_value());
    CHECK (refused->allowsMidiTail());
    CHECK (refused->placement.landed == 0);
}

TEST_CASE ("An unconsumed completion cannot continue a MIDI tail after Project replacement", "[import-model]")
{
    Fixture f;
    const auto path = f.wav ("completed.wav");
    const auto target = f.model.project().tracks.front().id;
    REQUIRE (f.model.startAudioImport ({ path }, { target }, 0, true, decodeWav).accepted);
    REQUIRE (f.finish());
    REQUIRE (f.model.createProjectBundle (f.root / "new.yesdaw").ok());
    const auto receipt = f.model.takeAudioImportCompletion();
    REQUIRE (receipt.has_value());
    CHECK (receipt->disposition == ui::UiAudioImportDisposition::Stale);
    CHECK_FALSE (receipt->allowsMidiTail());
    CHECK (f.model.project().clips.empty());
}

TEST_CASE ("Bundle copies exclude a held import's private staging without changing source files", "[import-model]")
{
    DecoderGate gate;
    GateBinding binding { gate };
    Fixture f;
    ReleaseGate release { gate };
    const auto path = f.wav ("copy-held.wav");
    const auto target = f.model.project().tracks.front().id;
    const auto foreign = f.bundle / "audio" / ".import-customer-notes";
    std::filesystem::create_directory (foreign);
    std::ofstream (foreign / "retained.txt") << "ordinary foreign data";
    REQUIRE (f.model.startAudioImport ({ path }, { target }, 0, true, heldDecode).accepted);
    REQUIRE (await ([&] { return gate.entered.load(); }));
    std::filesystem::path stage;
    for (const auto& entry : std::filesystem::directory_iterator (f.bundle / "audio"))
        if (entry.path().filename().string().starts_with (".import-") && entry.path() != foreign)
            stage = entry.path();
    REQUIRE_FALSE (stage.empty());
    REQUIRE (std::filesystem::exists (stage / "item-0.wav"));
    std::ofstream (stage / "foreign.txt") << "retain at source";
    const auto destination = f.root / "copied.yesdaw";
    bool privateCopied = false;
    const auto inspectCopy = [&] (const auto& partial) {
        privateCopied = std::filesystem::exists (partial / "audio" / stage.filename());
    };
    bool saveAs = false;
    SECTION ("Save a Copy keeps the source import eligible")
    {
        REQUIRE (f.model.saveProjectBundleCopy (destination, inspectCopy).dispatched);
        CHECK (f.model.bundlePath() == f.bundle);
        CHECK (f.model.audioImportPending());
    }
    SECTION ("Save As retires the old destination import")
    {
        saveAs = true;
        REQUIRE (f.model.saveProjectBundleAs (destination, inspectCopy).dispatched);
        CHECK_FALSE (f.model.audioImportPending());
        CHECK (f.model.audioImportRetiring());
    }
    SECTION ("A failed Save As preserves the original import")
    {
        const auto failedDestination = f.root / "failed-copy.yesdaw";
        REQUIRE_FALSE (f.model.saveProjectBundleAs (failedDestination, [] (const auto& partial) {
            std::filesystem::rename (partial / "project.db", partial / "project.db.retained");
            std::filesystem::create_directory (partial / "project.db");
        }).dispatched);
        CHECK (f.model.bundlePath() == f.bundle);
        CHECK (f.model.audioImportPending());
        CHECK_FALSE (std::filesystem::exists (failedDestination));
        REQUIRE (f.model.saveProjectBundleCopy (destination, inspectCopy).dispatched);
    }
    CHECK_FALSE (privateCopied);
    CHECK_FALSE (std::filesystem::exists (destination / "audio" / stage.filename()));
    CHECK (std::filesystem::exists (destination / "audio" / foreign.filename() / "retained.txt"));
    CHECK (std::filesystem::exists (stage / "item-0.wav"));
    CHECK (std::filesystem::exists (stage / "foreign.txt"));
    gate.release.store (true);
    CHECK (f.finish() == ! saveAs);
    CHECK (std::filesystem::exists (stage / "foreign.txt"));
    const auto receipt = f.model.takeAudioImportCompletion();
    REQUIRE (receipt.has_value());
    CHECK (receipt->allowsMidiTail() == ! saveAs);
    CHECK (f.model.project().clips.size() == (saveAs ? 0u : 1u));
    persistence::ProjectBundleDb copied;
    REQUIRE (persistence::ProjectBundleDb::openExistingBundle (destination, copied).ok());
    engine::Project copiedProject;
    REQUIRE (copied.readProjectSnapshot (copiedProject).ok());
    CHECK (copiedProject.clips.empty());
}

TEST_CASE ("Persistence failure propagates to duplicate aliases without changing model or playback", "[import-model]")
{
    Fixture f;
    const auto path = f.wav ("persist.wav");
    const auto target = f.model.project().tracks.front().id;
    auto expected = f.model.project();
    std::vector<const void*> expectedBuffers;
    SECTION ("Asset adoption refusal reaches every alias")
    {
        f.sql ("CREATE TRIGGER refuse_asset BEFORE INSERT ON assets BEGIN SELECT RAISE(ABORT, 'injected Asset refusal'); END;");
        REQUIRE (f.model.startAudioImport ({ path, path }, { target }, 0, true, decodeWav).accepted);
        CHECK_FALSE (f.finish());
        const auto receipt = f.model.takeAudioImportCompletion();
        REQUIRE (receipt.has_value());
        CHECK (receipt->placement.refusals.size() == 2);
        CHECK (receipt->placement.refusals[0] == receipt->placement.refusals[1]);
    }
    SECTION ("Final snapshot refusal keeps the old published Project and engine")
    {
        const auto prior = f.wav ("existing.wav", 48'000.0, 0.125f);
        REQUIRE (f.model.startAudioImport ({ prior }, { target }, 0, true, decodeWav).accepted);
        REQUIRE (f.finish());
        expected = f.model.project();
        expectedBuffers = f.model.distinctDecodedBuffersForTest();
        REQUIRE (f.model.locatePlaybackFrame (0));
        REQUIRE (f.model.dispatch (ui::UiActionId::TransportPlay).dispatched);
        const auto beforeAudio = f.model.renderPlaybackFrames (256, 128);
        REQUIRE (f.model.dispatch (ui::UiActionId::TransportStop).dispatched);
        REQUIRE (f.model.saveProjectBundle().ok());
        f.sql ("CREATE TRIGGER refuse_clip BEFORE INSERT ON clips BEGIN SELECT RAISE(ABORT, 'injected Clip refusal'); END;");
        const auto replaceCount = f.model.playbackReplaceCount();
        const auto beforeDispatch = f.model.context().commandDispatchCount;
        REQUIRE (f.model.startAudioImport ({ path }, { target }, 0, true, decodeWav).accepted);
        CHECK_FALSE (f.finish());
        CHECK (f.model.playbackReplaceCount() == replaceCount);
        CHECK (f.model.context().commandDispatchCount == beforeDispatch);
        CHECK (f.model.context().canUndo);
        CHECK_FALSE (f.model.hasUnsavedChanges());
        const auto receipt = f.model.takeAudioImportCompletion();
        REQUIRE (receipt.has_value());
        CHECK (receipt->disposition == ui::UiAudioImportDisposition::Failed);
        CHECK_FALSE (receipt->allowsMidiTail());
        REQUIRE (f.model.locatePlaybackFrame (0));
        REQUIRE (f.model.dispatch (ui::UiActionId::TransportPlay).dispatched);
        CHECK (f.model.renderPlaybackFrames (256, 128) == beforeAudio);
    }
    CHECK (f.model.project().assets == expected.assets);
    CHECK (f.model.project().clips == expected.clips);
    CHECK (f.model.project().tracks == expected.tracks);
    CHECK (f.model.distinctDecodedBuffersForTest() == expectedBuffers);
    CHECK (f.persisted().clips == expected.clips);
}

TEST_CASE ("Import completion performs no full source hash or copy on its owner thread", "[import-model]")
{
    for (const std::size_t batchSize : { 1u, 8u, 32u })
    {
        Fixture f;
        std::vector<std::filesystem::path> paths;
        for (std::size_t index = 0; index < batchSize; ++index)
        {
            const auto name = "measured-" + std::to_string (index) + ".wav";
            paths.push_back (f.wav (name.c_str(), 44'100.0, static_cast<float> (index + 1) * 0.001f));
        }
        const auto target = f.model.project().tracks.front().id;
        REQUIRE (f.model.startAudioImport (std::move (paths), { target }, 0, true, decodeWav).accepted);
        persistence::detail::assetHashCallsForTest = 0;
        persistence::detail::copiedFileBytesForTest = 0;
        REQUIRE (f.finish());
        CHECK (persistence::detail::assetHashCallsForTest == 0);
        CHECK (persistence::detail::copiedFileBytesForTest == 0);
        CHECK (f.model.project().clips.size() == batchSize);
        CHECK (f.model.project().assets.size() == batchSize);
        const auto receipt = f.model.takeAudioImportCompletion();
        REQUIRE (receipt.has_value());
        CHECK (receipt->placement.landed == batchSize);
        CHECK (receipt->ownerBatchMilliseconds > 0);
        WARN ("Measured owner adoption batch: " << batchSize << " unique Assets, " << receipt->ownerBatchMilliseconds << " ms");
    }
}

TEST_CASE ("A loaded stretched cross-rate duplicate keeps its decoded owner and prepared playback", "[import-model]")
{
    Fixture f;
    const auto path = f.wav ("stretched.wav", 44'100.0);
    const auto target = f.model.project().tracks.front().id;
    REQUIRE (f.model.startAudioImport ({ path }, { target }, 0, true, decodeWav).accepted);
    REQUIRE (f.finish());
    REQUIRE (f.model.selectTimelineClip (f.model.project().clips.front().id));
    REQUIRE (f.model.setSelectedTimelineClipStretchFactor (1.5f).dispatched);
    REQUIRE (await ([&] { f.model.serviceStretchPreparation(); return ! f.model.stretchPreparationPending(); }));
    REQUIRE (f.model.project().clips.front().stretchFactor == 1.5f);
    const auto assetId = f.model.project().assets.front().id;
    auto decoded = decodeWav (path);
    REQUIRE (decoded.audio.has_value());
    auto loadedDecode = ui::UiDecodedAsset::fromInterleaved (assetId, engine::SampleRate { 44'100.0 }, 1,
                                                           std::move (decoded.audio->interleaved));
    ui::UiAppModel loaded;
    loaded.setSessionStateDirectory (f.root / "loaded-state");
    REQUIRE (loaded.loadProjectBundle (f.bundle, { &loadedDecode, 1 }).ok());
    const auto owner = loaded.findDecodedAsset (assetId)->samples;
    const auto buffers = loaded.distinctDecodedBuffersForTest();
    REQUIRE (buffers.size() == 2);
    REQUIRE (loaded.startAudioImport ({ path, path }, { target }, 2'000, true, decodeWav).accepted);
    bool adopted = false;
    REQUIRE (await ([&] { adopted = loaded.serviceAudioImport() || adopted; return ! loaded.audioImportBusy(); }));
    REQUIRE (adopted);
    CHECK (loaded.findDecodedAsset (assetId)->samples == owner);
    CHECK (loaded.distinctDecodedBuffersForTest() == buffers);
    CHECK (loaded.project().assets.size() == 1);
    CHECK (loaded.project().clips.size() == 3);
    CHECK (loaded.project().clips.front().stretchFactor == 1.5f);
    REQUIRE (loaded.locatePlaybackFrame (0));
    REQUIRE (loaded.dispatch (ui::UiActionId::TransportPlay).dispatched);
    const auto audio = loaded.renderPlaybackFrames (512, 128);
    CHECK (std::any_of (audio.begin(), audio.end(), [] (float sample) { return sample != 0.0f; }));
    REQUIRE (loaded.dispatch (ui::UiActionId::EditUndo).dispatched);
    CHECK (loaded.project().clips.size() == 1);
    REQUIRE (loaded.dispatch (ui::UiActionId::EditRedo).dispatched);
    CHECK (loaded.project().clips.size() == 3);
}

TEST_CASE ("Sampler import captures a Track and pad while unrelated edits continue", "[import-model]")
{
    DecoderGate gate;
    GateBinding binding { gate };
    Fixture f;
    ReleaseGate release { gate };
    const auto target = f.model.project().tracks.front().id;
    REQUIRE (f.model.selectMixerTrack (0));
    REQUIRE (f.model.setInstrumentOnSelectedTrack (engine::TrackInstrumentKind::Sampler).dispatched);
    REQUIRE (f.model.addAudioTrack().dispatched);
    const auto other = f.model.project().tracks.back().id;
    const auto path = f.wav ("kick.wav", 44'100.0);
    REQUIRE (f.model.startSamplerPadImport (path, target, 36, heldDecode).accepted);
    REQUIRE (await ([&] { return gate.entered.load(); }));
    REQUIRE (f.model.selectMixerTrack (0));
    const auto otherPadPath = f.wav ("snare.wav", 48'000.0, 0.3f);
    auto otherPad = decodeWav (otherPadPath);
    REQUIRE (otherPad.audio.has_value());
    REQUIRE (f.model.importSamplerPadFromSource (otherPadPath,
        ui::UiDecodedAsset::fromInterleaved ({}, engine::SampleRate { 48'000.0 }, 1,
                                            std::move (otherPad.audio->interleaved)), 38).ok());
    const auto keptPad = *f.model.project().findTrack (target)->findSamplerPad (38);
    REQUIRE (f.model.selectMixerTrack (1));
    REQUIRE (f.model.renameProjectTrack (target, "Sampler kept").dispatched);
    gate.release.store (true);
    REQUIRE (f.finish());
    const auto* pad = f.model.project().findTrack (target)->findSamplerPad (36);
    REQUIRE (pad != nullptr);
    CHECK (pad->nameView() == "kick");
    CHECK (f.model.project().findTrack (other)->samplerPads.empty());
    CHECK (f.model.project().findTrack (target)->strip.name == "Sampler kept");
    CHECK (*f.model.project().findTrack (target)->findSamplerPad (38) == keptPad);
    const auto receipt = f.model.takeAudioImportCompletion();
    REQUIRE (receipt.has_value());
    CHECK (receipt->samplerPad);
    CHECK_FALSE (receipt->allowsMidiTail());
    CHECK (receipt->placement.landed == 1);
    REQUIRE (f.model.dispatch (ui::UiActionId::EditUndo).dispatched);
    CHECK (f.model.project().findTrack (target)->findSamplerPad (36) == nullptr);
    CHECK (f.model.project().findTrack (target)->strip.name == "Sampler kept");
    CHECK (f.model.project().assets.size() == 2);
    REQUIRE (f.model.dispatch (ui::UiActionId::EditRedo).dispatched);
    CHECK (f.model.project().findTrack (target)->findSamplerPad (36) != nullptr);
    CHECK (f.persisted().findTrack (target)->findSamplerPad (36) != nullptr);
}

TEST_CASE ("Relevant Sampler pad edits retire a pending load even after undo", "[import-model]")
{
    DecoderGate gate;
    GateBinding binding { gate };
    Fixture f;
    ReleaseGate release { gate };
    const auto target = f.model.project().tracks.front().id;
    REQUIRE (f.model.selectMixerTrack (0));
    REQUIRE (f.model.setInstrumentOnSelectedTrack (engine::TrackInstrumentKind::Sampler).dispatched);
    const auto first = f.wav ("original.wav");
    REQUIRE (f.model.startSamplerPadImport (first, target, 36, decodeWav).accepted);
    REQUIRE (f.finish());
    const auto original = *f.model.project().findTrack (target)->findSamplerPad (36);
    const auto replacement = f.wav ("replacement.wav", 48'000.0, 0.5f);
    REQUIRE (f.model.startSamplerPadImport (replacement, target, 36, heldDecode).accepted);
    REQUIRE (await ([&] { return gate.entered.load(); }));
    SECTION ("clear") { REQUIRE (f.model.clearSamplerPadOnSelectedTrack (36).dispatched); }
    SECTION ("mode edit") { REQUIRE (f.model.toggleSamplerPadModeOnSelectedTrack (36).dispatched); }
    REQUIRE_FALSE (f.model.audioImportPending());
    REQUIRE (f.model.dispatch (ui::UiActionId::EditUndo).dispatched);
    gate.release.store (true);
    CHECK_FALSE (f.finish());
    CHECK (*f.model.project().findTrack (target)->findSamplerPad (36) == original);
    const auto receipt = f.model.takeAudioImportCompletion();
    REQUIRE (receipt.has_value());
    CHECK (receipt->disposition == ui::UiAudioImportDisposition::Stale);
    CHECK_FALSE (receipt->allowsMidiTail());
}
