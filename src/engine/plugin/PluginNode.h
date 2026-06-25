// YES DAW — PluginNode: the graph-visible IPC proxy over the RT-lane ring (ADR-0015 / ADR-0013 / ADR-0008).
//
// PluginNode is a hosted plugin's adapter into the graph: it implements the FROZEN Node contract (ADR-0008)
// like any built-in, but its DSP lives behind the RT-lane shared-memory ring (RtLaneRing) — the audio
// thread's ONLY contact with the plugin. Adding hosting was always meant to be an adapter, not an engine
// rewrite (ADR-0002 #3); this is that adapter. The compiler sees a generic single-input node (it falls into
// CompiledNodeKind::Plugin via GraphBuilder::detectKind's fallback), so NO change to the Node contract, the
// graph compiler, or the buffer pool is needed.
//
// ── The audio thread (process(), YESDAW_RT_HOT) ──────────────────────────────────────────────────────
// The graph hands a single-input node its producer's audio copied INTO its own output slot, then calls
// process() with that slot as both input and output (CompiledGraph::process). So process() is exactly one
// RtLaneRing::exchangeBlock for this Block: write Block N's input audio + Events into the ring, read Block
// N-1's already-published output back into the same buffer, FAIL OPEN (last-good -> silence -> bypass) if
// the child is late. exchangeBlock captures the input before it overwrites the output, so in-place is safe.
// It never allocates / locks / logs / does I/O / signals / waits (RTSan-enforced via exchangeBlock).
//
// ── Latency & PDC (ADR-0007 / ADR-0015) ──────────────────────────────────────────────────────────────
// properties().latencySamples = one pipeline Block (the ring's deterministic single-Block delay) + the
// plugin's validated, post-prepareToPlay latency. Plugin-reported latency is VALIDATED before it reaches
// the compiler: negatives are quarantined to zero and absurd values clamped to a sane max, so a plugin
// claiming INT64_MAX cannot overflow the PDC longest-path walk or blow up a delay-line allocation. The
// pipeline Block size is fixed at construction (the host passes the same maxBlockSize it compiles with),
// because the compiler reads properties() BEFORE prepare() runs.
//
// ── Headless for now (this chunk) ────────────────────────────────────────────────────────────────────
// The "plugin" is the ring's CHILD role, driven by an in-process stub processor (identity by default;
// settable to a gain / latency-reporting stand-in). There is NO real child process, no YesDawPluginHost
// worker exe, no JUCE hosting, no scanner/watchdog/coordinator — those are the chunks after this one. In
// production the child is a separate process polling the shared-memory ring; here serviceStubChild()
// stands in for that off-audio-thread poll so the whole proxy is testable headlessly (RTSan/TSan-covered).
// PluginNode therefore contains NO juce::AudioProcessor — that adapter lives only in the future host child,
// preserving ADR-0008's engine⇏hosting layering boundary.

#pragma once

#include "engine/Node.h"
#include "engine/plugin/RtLaneRing.h"
#include "rt/RtHot.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <span>
#include <utility>

namespace yesdaw::engine {

class PluginNode final : public Node
{
public:
    static constexpr int kMaxChannels = 8;   // mirrors GraphBuilder::kMaxChannelsPerNode

    // Generous-but-bounded ceilings so validation can never let a plugin overflow the PDC walk. A pipeline
    // Block is capped at 1 s @ 192 kHz and the validated plugin latency at ~57 s @ 192 kHz; their sum stays
    // comfortably under GraphBuilder::kMaxLatencyCap (60 s @ 192 kHz), so a clamped report is ACCEPTED by
    // the compiler (latency compensated) rather than rejected — a hostile claim degrades, never crashes.
    static constexpr int          kMaxPipelineBlockSamples    = 192000;
    static constexpr std::int64_t kMaxValidatedLatencySamples = 11'000'000;

    // The in-process stub child's DSP: (events, in[ch], out[ch], channels, numFrames). Off-audio-thread.
    using StubProcessor = std::function<void (std::span<const Event>, const float* const*, float* const*, int, int)>;

    explicit PluginNode (NodeId id = 0, int channels = 1, int pipelineBlockSamples = 512)
        : id_ (id),
          channels_ (clampChannels (channels)),
          pipelineBlock_ (clampBlock (pipelineBlockSamples)),
          stub_ (makeIdentityStub())
    {
    }

    // ── Frozen Node contract (ADR-0008) ──────────────────────────────────────────────────────────────

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ true, /*producesEvents*/ false,
                                channels_, reportedLatencySamples(), id_ };
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (&input_, input_ != nullptr ? 1u : 0u);
    }

    // Control thread: size the RT-lane region for this channel count / maximum Block. The ONLY allocation.
    void prepare (double /*sampleRate*/, int maxBlockSize) override
    {
        RtLaneConfig cfg;
        cfg.channels           = channels_;
        cfg.maxBlockSize       = maxBlockSize > 0 ? maxBlockSize : 1;
        cfg.maxEventsPerBlock  = kDefaultMaxEventsPerBlock;
        cfg.lastGoodHoldBlocks = lastGoodHold_;
        cfg.bypassAfterMisses  = bypassAfter_;
        ring_.prepare (cfg);
        ring_.setValidatedLatencySamples (validatedLatency_);
    }

    // Audio thread: drive one Block through the ring. Single exchangeBlock — input and output are the same
    // in-place buffer the graph handed us (exchangeBlock reads input fully before writing output).
    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        float* const* const channels = args.audio.channels;
        const int           numCh    = args.audio.numChannels;
        ring_.exchangeBlock (channels, numCh, args.numFrames, args.events.events(), channels, numCh);
    }

    // Audio thread: drop the pipeline's fail-open state to "primed" without reallocating (RT-safe).
    void reset() noexcept override { ring_.reset(); }

    // Control thread: the ring's storage is freed with this node; nothing extra to release here.
    void release() override {}

    // ── Control-thread wiring / configuration (host side; set before compile) ─────────────────────────

    void setInput (Node* in) noexcept { input_ = in; }

    // The plugin's reported latency (e.g. post-prepareToPlay). Validated so it can never reach PDC negative
    // or absurd: negatives quarantine to zero, absurd values clamp to kMaxValidatedLatencySamples.
    void setReportedLatencySamples (std::int64_t reportedLatency) noexcept
    {
        validatedLatency_ = validateLatency (reportedLatency);
        ring_.setValidatedLatencySamples (validatedLatency_);   // mirror into the ring's control word
    }

    // Fail-open ladder thresholds (consecutive missed Blocks), applied at the next prepare().
    void setFailOpenThresholds (std::uint32_t lastGoodHoldBlocks, std::uint32_t bypassAfterMisses) noexcept
    {
        lastGoodHold_ = lastGoodHoldBlocks;
        bypassAfter_  = bypassAfterMisses;
    }

    // The in-process stub child's DSP (default identity). Ignored if empty.
    void setStubProcessor (StubProcessor stub)
    {
        if (stub)
            stub_ = std::move (stub);
    }

    // ── Headless stub child (stands in for the real out-of-process plugin host child) ─────────────────
    // Off the audio thread: poll the input ring once and, if a new Block is pending, run the stub and
    // publish its output. Returns true iff it produced an output this call. In production a separate child
    // process does this against the shared-memory ring; here the test pumps it to model that cadence.
    bool serviceStubChild() { return ring_.pollOnce (stub_); }

    // ── Diagnostics (any thread) ──────────────────────────────────────────────────────────────────────
    RtLaneOutput lastOutputSource() const noexcept { return ring_.lastStatus(); }
    bool         bypassActive()     const noexcept { return ring_.bypassActive(); }
    int          channels()         const noexcept { return channels_; }

private:
    static constexpr std::uint32_t kDefaultMaxEventsPerBlock = 64;
    // Default fail-open thresholds mirror RtLaneConfig's (last-good held 2 Blocks; bypass after 8 misses).
    static constexpr std::uint32_t kDefaultLastGoodHoldBlocks = 2;
    static constexpr std::uint32_t kDefaultBypassAfterMisses  = 8;

    static int clampChannels (int channels) noexcept
    {
        return channels < 1 ? 1 : (channels > kMaxChannels ? kMaxChannels : channels);
    }

    static int clampBlock (int blockSamples) noexcept
    {
        return blockSamples < 1 ? 1 : (blockSamples > kMaxPipelineBlockSamples ? kMaxPipelineBlockSamples : blockSamples);
    }

    // Reject the impossible (negative -> quarantine to zero) and clamp the absurd, so a hostile plugin's
    // claim is bounded BEFORE it reaches the compiler (ADR-0015).
    static std::int64_t validateLatency (std::int64_t reportedLatency) noexcept
    {
        if (reportedLatency < 0)
            return 0;
        if (reportedLatency > kMaxValidatedLatencySamples)
            return kMaxValidatedLatencySamples;
        return reportedLatency;
    }

    // One pipeline Block (the ring's deterministic single-Block delay) + the validated plugin latency. Both
    // operands are bounded above, so the sum cannot overflow and stays under the compiler's latency cap.
    std::int64_t reportedLatencySamples() const noexcept
    {
        return static_cast<std::int64_t> (pipelineBlock_) + validatedLatency_;
    }

    static StubProcessor makeIdentityStub()
    {
        return [] (std::span<const Event>, const float* const* in, float* const* out,
                   int channels, int numFrames) noexcept
        {
            for (int c = 0; c < channels; ++c)
                for (int f = 0; f < numFrames; ++f)
                    out[c][f] = in[c][f];
        };
    }

    NodeId        id_;
    int           channels_;
    int           pipelineBlock_;
    std::int64_t  validatedLatency_ = 0;
    std::uint32_t lastGoodHold_     = kDefaultLastGoodHoldBlocks;
    std::uint32_t bypassAfter_      = kDefaultBypassAfterMisses;
    Node*         input_            = nullptr;
    StubProcessor stub_;
    RtLaneRing    ring_;
};

} // namespace yesdaw::engine
