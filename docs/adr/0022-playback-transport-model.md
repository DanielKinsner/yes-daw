# 0022. Playback transport model

- **Status:** Accepted
- **Date:** 2026-06-28
- **Deciders:** Dan (owner), build agent
- **Related:** ADR-0002 (real-time foundations), ADR-0006 (immutable snapshot concurrency), ADR-0007
  (CompiledGraph + PDC), ADR-0010 (time model), ADR-0018 (recording), ADR-0019 (autosave recovery),
  ADR-0020 (H7-H11 roadmap), H8 playback runtime plan.

## Context

H8 wires a Project into the real lock-free `Runtime` / `RuntimeAudioDriver` path. Checkpoint 1 proved
play-from-0, but the remaining H8 gate needs transport behavior: `play`, `stop`, `locate(N)`, loop
repeat, recording driven from the transport frame, and autosave on a transport/edit tick.

The current Project playback graph uses `DecodedClipNode`. Before this ADR that node advanced an internal
monotonic `playFrame_` from `prepare()`, so it could play from 0 but could not sample-accurately locate or
loop without either rebuilding the graph at every transport move or adding a caller-owned playhead. The
audio thread still must not allocate, lock, log, or perform I/O.

## Options considered

1. **Rebuild and republish a graph for every locate/loop boundary.** Simple to reason about, but wrong for
   loop points inside a device block unless the audio callback publishes graphs, which violates the
   control-thread-only Runtime contract.
2. **Give source nodes mutable seek commands.** Keeps one graph alive, but adds node-specific command
   routing and state mutation before the transport model is stable.
3. **Pass an absolute timeline frame through `Transport` for each callback segment.** The control side owns
   play/stop/locate/loop state. The audio callback passes a trivially-copyable `timelineFrame` through the
   existing `ProcessArgs::transport` slot. Nodes that understand the frame read by absolute Project frame;
   older nodes ignore it. Accepted.

## Decision

Use **absolute-frame transport** for H8.

- `PlaybackEngine` owns `isPlaying`, `playheadFrame`, and optional `[loopStartFrame, loopEndFrame)` state.
- Each audio callback segment passes a `Transport` with `hasTimelineFrame=true`, `timelineFrame` set to the
  segment's absolute Project frame, `projectSampleRate`, and `isPlaying=true`.
- `stop()` suppresses output and does not advance `playheadFrame`.
- `locate(frame)` sets the next callback's absolute Project frame. Negative locates are rejected.
- Looping is half-open: when the playhead reaches `loopEndFrame`, the next segment wraps to
  `loopStartFrame`. If a device block crosses the loop boundary, `PlaybackEngine` splits the callback into
  two Runtime calls with pointer offsets; this is still allocation-free and lock-free.
- `DecodedClipNode` uses `Transport::timelineFrame` when present. Without it, it keeps the legacy internal
  monotonic frame so older direct graph callers stay compatible.
- Recording uses the transport playhead as the device-input start frame. Autosave remains persistence-owned
  and is triggered by a small playback/edit tick helper, not by the audio callback.

## Consequences

- **Positive:** locate and loop are sample-accurate without publishing graphs from the audio thread; the
  hot path stays bounded and allocation-free; the H7 offline/reference proof still applies because the same
  graph renders the same absolute Project frames.
- **Positive:** `ProcessArgs::transport` becomes a real node-facing contract instead of an empty placeholder,
  while staying trivially copyable.
- **Negative / accepted:** stateful effect tails are not rewound by this H8 source-frame model. A future
  stateful transport command can reset delay/plugin state on explicit locate if needed; H8's Project audio
  source path is now correct and mechanically covered.
- **Negative / accepted:** a loop-boundary split increments Runtime's processed generation once per segment,
  not once per physical device callback. Reclamation only needs monotonic progress, so this is safe.

## Verification

`YesDawPlaybackCheck` proves:

- play-from-0 through `RuntimeAudioDriver` matches the independent reference;
- output is block-size independent and bit-identical to H7 offline render;
- `locate`, `stop`, and loop repeat are sample-accurate;
- recording capture maps through the transport playhead and aligns a take to the click reference;
- autosave writes from a playback/edit tick and recovers through the normal bundle validators.
