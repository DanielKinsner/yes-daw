# 0004. Stack: C++ / JUCE 8 framework + our own engine

**Status:** accepted · 2026-06-23 · Deciders: Dan

We build YES DAW in **C++ on JUCE 8**, using JUCE as the **framework** — cross-platform audio device
I/O (`AudioDeviceManager`), plugin hosting (`AudioPluginFormatManager`, VST3/AU), the UI toolkit, and
DSP utilities — while building **our own engine and data model** on top: the graph, transport,
timeline, mixer, editing, event model, and SQLite project format are ours.

We do **not** adopt Tracktion Engine (the newest paper's "adopt the engine" option), and we do **not**
use Rust (the two older papers' default). We also do **not** use `juce::AudioProcessorGraph` as the
real engine graph — it forces rebuilds onto the message thread and lacks first-class plugin delay
compensation; we build our own immutable compiled-snapshot graph (see the build plan) and use
`juce::AudioProcessor` only as an *adapter target* for hosted plugins, never as our internal Node contract.

Rationale: framework maturity (plugin hosting and OS audio I/O are battle-tested across thousands of
products) with full control of the engine — the middle path between building everything from scratch
and adopting a whole DAW engine. Builds on [ADR-0002](0002-realtime-engine-foundations.md); detailed
in the [build plan](../plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md).
