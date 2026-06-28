# H8 plan — Playback runtime (device I/O + transport)

**Why this exists.** Everything is headless libraries today: the engine renders, but nothing *plays* a
Project through the production real-time path. H8 wires a Project into the lock-free `Runtime` /
`RuntimeAudioDriver` behind a **transport** (play / stop / locate / loop), gives **recording (H5) and
autosave (H6) their first production callers**, and proves it all mechanically — block by block — against
the same independent reference H7 uses. This is the **audible "it plays" milestone**. The only part that
needs real hardware (literal device output) is a tracked one-command smoke, **not** a CI blocker.

## Exit gate (headless)

`YesDawPlaybackCheck` is the H8 blocking CI gate:

- Plays a known Project through the real `Runtime` / `RuntimeAudioDriver` production path (publish a graph,
  drain the command queue, pump `processDeviceBlock`), collecting the output block by block.
- Asserts the played output equals the **independent reference** (Clips summed at their timeline positions
  with gain, the canonical linear fade, and center pan) within tolerance — the same reference H7 proved the
  offline render against, so this proves the *runtime path* plays correctly (not the engine vs itself).
- Proves block-size independence (bit-identical across block sizes) of the played output.
- Proves the transport is sample-accurate: `locate(N)` plays the reference from frame N; a loop region
  repeats the reference; `stop` zeroes output and freezes the playhead.
- Drives **recording (H5)** from the transport and asserts a recorded take aligns to the click reference;
  drives **autosave (H6)** on a transport/edit tick and asserts recovery through the normal validators.

The real-device output (zero Underruns out an actual device at a 128-frame Block) is a **one-command
self-asserting hardware smoke** (ADR-0005 pattern), tracked separately — it absorbs the still-open H0 soak
and is **not** a CI gate.

## Build order

Each checkpoint is one small, independently-green commit; CI is the gate. No boundary stop between
horizons — continue headless.

1. **Kickoff docs.** This plan; switch `loop/horizon.md` to H8; add transport terms to `CONTEXT.md`. **[done]**
2. **Shared project-graph builder.** Extract the projection + decoded-source factory from
   `OfflineRenderer` into a `buildProjectGraph(project, decodedAssets, options)` so offline render AND
   playback share the EXACT graph (H7 already proved that graph == the independent reference). No new ADR
   — a refactor; `OfflineRenderer` keeps its gate green. **[done]**
3. **`PlaybackEngine` (play-from-0).** A production caller that builds the graph, publishes it to a
   `RuntimeAudioDriver`, and pumps `processDeviceBlock`. Gate: played output == independent reference,
   block-size independent. No new ADR (integration of existing Runtime + graph). **[done]**
4. **ADR-0022 — transport model.** Decide how the transport playhead reaches the nodes and what `locate`
   does (reset + reposition the graph at a new start frame vs nodes reading a shared playhead). Needed
   before locate/loop code lands. **[done]**
5. **Transport controls.** Play / stop / locate / loop on top of `PlaybackEngine`, each gated against the
   reference (shifted for locate, repeated for loop, silent+frozen for stop). **[done]**
6. **Production callers for recording + autosave.** Drive H5 recording and H6 autosave from the transport;
   gate take alignment and autosave recovery through the real callers. **[done]**
7. **Real-device smoke (tracked, not CI).** One-command script: play a known Project out the actual device
   with zero Underruns at a 128-frame Block. Absorbs the open H0 soak. **[done: `tools/playback-smoke.ps1`
   / `tools/playback-smoke.sh`, build-checked; hardware run remains owner-machine smoke, not CI]**
8. **Close.** Headless `YesDawPlaybackCheck` + full `ci` green. **[done]**

## Non-goals (H8)

- No UI / transport bar / meters surface — that is H11.
- No input monitoring or input-FX chain (recording spine only; monitoring stays deferred).
- No sample-rate conversion or device hot-swap (H10).
- No multi-device / aggregate device handling.
- The real-device output is a tracked smoke, not part of the CI gate.

## Decisions

- ADR-0022 accepted the absolute-frame transport model: `PlaybackEngine` owns play/stop/locate/loop state
  and passes a caller-owned Project `timelineFrame` through `Transport` for each audio callback segment.

## Status

Closed locally on 2026-06-28. `YesDawPlaybackCheck` passes 6 cases / 125 assertions: play-from-0 through
`RuntimeAudioDriver`, block-size independence, offline parity, sample-accurate locate/stop/loop, H5
recording capture from the transport playhead, and H6 autosave recovery from the playback/edit tick.
`YesDawSoak` builds with `--playback-project`; the tracked hardware command is
`tools/playback-smoke.ps1 -Loopback` (or `tools/playback-smoke.sh --loopback`) on a real audio machine.
Local full gate: `ctest --preset ci --output-on-failure` passed 239/239.
