// YES DAW - owned stretch preparation and its pure worker/Control-thread handoff (ADR-0080).

#include "app/StretchPreparationJob.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <thread>
#include <type_traits>
#include <utility>

namespace {

using namespace yesdaw;

app::StretchPreparationRequest requestFor (double sourceRate = 48'000.0, std::uint16_t channels = 2)
{
    auto source = std::make_shared<engine::AssetSamples>();
    source->channels = static_cast<int> (channels);
    source->frames = 8'192;
    source->interleaved.resize (static_cast<std::size_t> (source->frames) * channels);
    for (std::size_t frame = 0; frame < source->frames; ++frame)
        for (std::size_t channel = 0; channel < channels; ++channel)
            source->interleaved[frame * channels + channel] = static_cast<float> (
                0.3 * std::sin (static_cast<double> (frame) * (0.07 + static_cast<double> (channel) * 0.03)));

    app::StretchPreparationRequest request;
    request.key.requestId = 41;
    request.key.requestVersion = 7;
    request.key.attachGeneration = 3;
    request.key.projectId = engine::EntityId::fromBigEndianParts (1, 1);
    request.key.trackId = engine::EntityId::fromBigEndianParts (1, 2);
    request.key.clipId = engine::EntityId::fromBigEndianParts (1, 3);
    request.key.assetId = engine::EntityId::fromBigEndianParts (1, 4);
    request.key.contentHash.bytes.fill (19);
    request.key.source = std::move (source);
    request.key.sourceRateHz = sourceRate;
    request.key.viewRateHz = 48'000.0;
    request.key.channels = channels;
    request.key.srcOffset = 143;
    request.key.srcLen = 4'001;
    request.key.stretchFactor = 1.5;
    return request;
}

void awaitTerminal (app::StretchPreparationJob& job)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds (20);
    while (! job.terminal() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    REQUIRE (job.terminal());
}

engine::PreparedTimeStretch referenceFor (const app::StretchPreparationKey& key,
                                         const engine::AssetSamples& view,
                                         engine::ViewWindow window)
{
    return engine::prepareTimeStretch (
        std::span<const float> (view.interleaved).subspan (
            static_cast<std::size_t> (window.srcOffset) * key.channels,
            static_cast<std::size_t> (window.srcLen) * key.channels),
        key.channels, key.viewRateHz, key.stretchFactor);
}

} // namespace

TEST_CASE ("Stretch preparation publishes the existing DSP output and exact request identity", "[stretch-job]")
{
    const auto request = requestFor();
    const engine::ViewWindow window { request.key.srcOffset, request.key.srcLen };
    const auto reference = referenceFor (request.key, *request.key.source, window);
    REQUIRE (reference.ok());

    app::StretchPreparationJob job (request);
    CHECK (job.state() == app::StretchPreparationState::Preparing);
    CHECK_FALSE (job.result());
    CHECK_FALSE (job.eligibleForAdoption());
    REQUIRE (job.start());
    awaitTerminal (job);
    REQUIRE (job.state() == app::StretchPreparationState::Succeeded);
    REQUIRE (job.eligibleForAdoption());
    CHECK (job.failure() == app::StretchPreparationFailure::None);
    CHECK (job.preparationStatus() == engine::TimeStretchStatus::Ok);
    const auto result = job.result();
    REQUIRE (result);
    CHECK (result->key == request.key);
    CHECK (result->key.source == request.key.source);
    CHECK (result->viewSamples == request.key.source);
    CHECK (result->window.srcOffset == window.srcOffset);
    CHECK (result->window.srcLen == window.srcLen);
    REQUIRE (result->samples);
    CHECK (result->samples->channels == 2);
    CHECK (result->samples->frames == reference.outputFrames);
    CHECK (result->samples->interleaved == reference.interleavedSamples);

    static_assert (std::is_const_v<typename decltype (result)::element_type>);
    static_assert (std::is_const_v<typename decltype (result->samples)::element_type>);
    job.join(); // Outcome assertions above exercise terminal publication before join can synchronize it.
    CHECK_FALSE (job.start()); // one shot, including after join
    CHECK (job.result() == result);
    CHECK (job.state() == app::StretchPreparationState::Succeeded);
}

TEST_CASE ("Stretch preparation owns its source and immutable result across caller retirement", "[stretch-job]")
{
    std::weak_ptr<const engine::AssetSamples> source;
    std::shared_ptr<const app::StretchPreparationResult> result;
    {
        auto request = requestFor (48'000.0, 1);
        source = request.key.source;
        app::StretchPreparationJob job (std::move (request));
        REQUIRE_FALSE (source.expired());
        REQUIRE (job.start());
        awaitTerminal (job);
        result = job.result();
        REQUIRE (result);
    }
    REQUIRE_FALSE (source.expired());
    REQUIRE (result->samples);
    CHECK (result->samples->frames == 6'002);
    CHECK (result->samples->interleaved.size() == 6'002);
    result.reset();
    CHECK (source.expired());
}

TEST_CASE ("Stretch preparation cancellation invalidates adoption without changing a published result", "[stretch-job]")
{
    app::StretchPreparationJob job (requestFor());
    SECTION ("before starting")
    {
        job.requestCancel();
        CHECK_FALSE (job.eligibleForAdoption());
        REQUIRE (job.start());
        awaitTerminal (job);
        CHECK (job.state() == app::StretchPreparationState::Cancelled);
        CHECK_FALSE (job.result());
    }
    SECTION ("after starting")
    {
        REQUIRE (job.start());
        job.requestCancel();
        CHECK (job.cancelRequested());
        CHECK_FALSE (job.eligibleForAdoption());
        awaitTerminal (job);
        CHECK ((job.state() == app::StretchPreparationState::Cancelled
                || job.state() == app::StretchPreparationState::Succeeded));
        CHECK_FALSE (job.eligibleForAdoption());
    }
    SECTION ("after completion")
    {
        REQUIRE (job.start());
        awaitTerminal (job);
        const auto result = job.result();
        REQUIRE (result);
        const auto samples = result->samples;
        const auto before = samples->interleaved;
        REQUIRE (job.eligibleForAdoption());
        job.requestCancel();
        CHECK_FALSE (job.eligibleForAdoption());
        CHECK (job.state() == app::StretchPreparationState::Succeeded);
        CHECK (job.result() == result);
        CHECK (result->samples == samples);
        CHECK (samples->interleaved == before);
    }
    CHECK (job.cancelRequested());
    CHECK (job.failure() == app::StretchPreparationFailure::None);
}

TEST_CASE ("Stretch preparation refuses invalid source and view metadata", "[stretch-job]")
{
    auto request = requestFor();
    SECTION ("no source owner") { request.key.source.reset(); }
    SECTION ("source channel mismatch") { request.key.channels = 1; }
    SECTION ("source frame count mismatch")
    {
        auto source = std::make_shared<engine::AssetSamples> (*request.key.source);
        ++source->frames;
        request.key.source = std::move (source);
    }
    SECTION ("source window beyond end") { request.key.srcOffset = request.key.source->frames; }
    SECTION ("overflowing source window") { request.key.srcLen = std::numeric_limits<std::uint64_t>::max(); }
    SECTION ("empty source window") { request.key.srcLen = 0; }
    SECTION ("invalid source rate") { request.key.sourceRateHz = 0.0; }
    SECTION ("invalid view rate") { request.key.viewRateHz = std::numeric_limits<double>::quiet_NaN(); }
    SECTION ("wrong ready view frame count")
    {
        request = requestFor (44'100.0);
        request.readyView = request.key.source;
    }
    SECTION ("wrong ready view channel count")
    {
        request = requestFor (44'100.0);
        auto view = std::make_shared<engine::AssetSamples>();
        view->channels = 1;
        view->frames = engine::resampledFrameCount (request.key.source->frames, 44'100.0, 48'000.0);
        view->interleaved.resize (static_cast<std::size_t> (view->frames));
        request.readyView = std::move (view);
    }

    app::StretchPreparationJob job (std::move (request));
    REQUIRE (job.start());
    awaitTerminal (job);
    CHECK (job.state() == app::StretchPreparationState::Failed);
    CHECK (job.failure() == app::StretchPreparationFailure::InvalidRequest);
    CHECK_FALSE (job.result());
    CHECK_FALSE (job.eligibleForAdoption());
}

TEST_CASE ("Stretch preparation reports existing preparation refusal statuses", "[stretch-job]")
{
    auto request = requestFor();
    engine::TimeStretchStatus expected = engine::TimeStretchStatus::InvalidStretchFactor;
    SECTION ("factor outside supported range") { request.key.stretchFactor = 2.5; }
    SECTION ("nonfinite source sample")
    {
        auto source = std::make_shared<engine::AssetSamples> (*request.key.source);
        source->interleaved[static_cast<std::size_t> (request.key.srcOffset) * request.key.channels] =
            std::numeric_limits<float>::quiet_NaN();
        request.key.source = std::move (source);
        expected = engine::TimeStretchStatus::NonFiniteInput;
    }
    app::StretchPreparationJob job (std::move (request));
    REQUIRE (job.start());
    awaitTerminal (job);
    CHECK (job.state() == app::StretchPreparationState::Failed);
    CHECK (job.failure() == app::StretchPreparationFailure::TimeStretch);
    CHECK (job.preparationStatus() == expected);
    CHECK_FALSE (job.result());
}

TEST_CASE ("Stretch preparation uses the live rate-matched view and endpoint-mapped Asset window", "[stretch-job][cross-rate]")
{
    auto request = requestFor (44'100.0);
    const auto referenceView = engine::buildRateMatchedSamples (
        request.key.source->interleaved, request.key.channels, request.key.sourceRateHz,
        request.key.viewRateHz, engine::ResampleQuality::LivePlayback);
    REQUIRE (referenceView);
    const auto window = engine::mapWindowToView (
        request.key.srcOffset, request.key.srcLen,
        engine::rateRatio (request.key.sourceRateHz, request.key.viewRateHz), referenceView->frames);
    // These ends intentionally differ from independently rounding the source length by one frame.
    REQUIRE (window.srcLen != engine::viewFrameFor (
        request.key.srcLen, engine::rateRatio (request.key.sourceRateHz, request.key.viewRateHz)));
    const auto reference = referenceFor (request.key, *referenceView, window);
    REQUIRE (reference.ok());
    bool suppliedView = false;
    SECTION ("worker prepares a missing live view") { CHECK_FALSE (request.readyView); }
    SECTION ("caller supplies an already-ready live view")
    {
        request.readyView = referenceView;
        suppliedView = true;
    }
    const auto key = request.key;
    app::StretchPreparationJob job (std::move (request));
    REQUIRE (job.start());
    awaitTerminal (job);
    REQUIRE (job.state() == app::StretchPreparationState::Succeeded);
    const auto result = job.result();
    REQUIRE (result);
    CHECK (result->key == key);
    REQUIRE (result->viewSamples);
    CHECK (result->viewSamples->frames == referenceView->frames);
    CHECK (result->viewSamples->interleaved == referenceView->interleaved);
    if (suppliedView)
        CHECK (result->viewSamples == referenceView);
    CHECK (result->window.srcOffset == window.srcOffset);
    CHECK (result->window.srcLen == window.srcLen);
    REQUIRE (result->samples);
    CHECK (result->samples->frames == reference.outputFrames);
    CHECK (result->samples->interleaved == reference.interleavedSamples);
}

TEST_CASE ("Stretch preparation keys distinguish stale requests and a replaced source owner", "[stretch-job]")
{
    const auto original = requestFor().key;
    auto changed = original;
    REQUIRE (changed == original);
    SECTION ("request id") { ++changed.requestId; }
    SECTION ("request version") { ++changed.requestVersion; }
    SECTION ("Project generation") { ++changed.attachGeneration; }
    SECTION ("Project identity") { ++changed.projectId.bytes.back(); }
    SECTION ("Track identity") { ++changed.trackId.bytes.back(); }
    SECTION ("Clip identity") { ++changed.clipId.bytes.back(); }
    SECTION ("Asset identity") { ++changed.assetId.bytes.back(); }
    SECTION ("Asset content") { ++changed.contentHash.bytes.back(); }
    SECTION ("source owner") { changed.source = std::make_shared<engine::AssetSamples> (*original.source); }
    SECTION ("source rate") { changed.sourceRateHz = 44'100.0; }
    SECTION ("view rate") { changed.viewRateHz = 96'000.0; }
    SECTION ("channels") { changed.channels = 1; }
    SECTION ("Asset offset") { ++changed.srcOffset; }
    SECTION ("Asset length") { --changed.srcLen; }
    SECTION ("stretch factor") { changed.stretchFactor = 0.75; }
    CHECK_FALSE (changed == original);
}
