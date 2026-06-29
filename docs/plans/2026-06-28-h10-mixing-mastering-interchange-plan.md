# H10 plan - Mixing/mastering features & interchange

**Why this exists.** H7-H9 made render/export, playback, and deterministic scheduling real. H10 adds the
headless feature set the H11 UI will expose: loudness metering, DAWproject interchange, time-stretch, and
device hot-swap survival. Every slice is ADR-backed and mechanically gated.

## Exit gate (headless)

H10 closes only when all four focused gates are green in the full CI preset:

- **Loudness:** integrated/short-term/momentary loudness and true-peak-oriented fixture checks match the
  libebur128/BS.1770 reference within tolerance.
- **DAWproject:** export produces a DAWproject package that a reference reader can round-trip for the
  current Project surface: tracks, audio clips, MIDI clips, timing, gain/pan, and asset references.
- **Time-stretch:** the Node is deterministic across block splits, sample-accurate on duration, and matches
  a checked-in reference/golden for fixed ratios.
- **Device hot-swap:** a simulated device change during playback keeps the session alive without an
  Underrun, preserves the transport frame, and recovers deterministically.

## Green command

```
cmake --preset ci
cmake --build --preset ci
ctest --preset ci --output-on-failure
ctest --test-dir build-ci -R "YesDaw(Loudness|Dawproject|TimeStretch|DeviceHotSwap)Check" --output-on-failure
```

## Build order

Each checkpoint is one small, independently green commit. No code lands before the decision it depends on
is written as an ADR.

1. **Kickoff docs. [done]** Switch `loop/horizon.md` to H10, record H9's remote-green closeout,
   add this plan, and add the H10 glossary terms to `CONTEXT.md`.

2. **Loudness metering - ADR-0028 + `YesDawLoudnessCheck`. [done]** Decide the metering surface, channel
   weighting, block/update model, true-peak scope, and reference tolerance. Land a headless meter that can
   run offline over interleaved Project samples and later feed H11. Gate against libebur128/BS.1770
   fixtures with negative controls.

3. **DAWproject export - ADR-0029 + `YesDawDawprojectCheck`. [local gate green]**
   Decide the minimal supported DAWproject subset, package layout, ID mapping, asset reference policy, and
   unsupported-feature degradation. Export the current Project surface and verify it through an independent
   reader, not a string comparison of our own writer. The primitive preflight now locks deterministic XML
   IDs, media paths, timing conversion, and XML escaping; the package gate now writes stored ZIP/XML/WAV
   packages and verifies them through an independent reader with negative controls.

4. **Time-stretch Node - ADR-0030 + `YesDawTimeStretchCheck`.** Decide the Signalsmith-backed Node
   contract, ratio/range limits, latency/tail handling, scheduler safety, and golden policy. Gate fixed
   ratios, block-size independence, duration accuracy, silence, and invalid inputs.

5. **Device hot-swap survival - ADR-0031 + `YesDawDeviceHotSwapCheck`.** Decide the device-change state
   machine around `PlaybackEngine` / `RuntimeAudioDriver`: stop old callback, preserve transport frame,
   rebuild or reconnect the driver, and resume without an Underrun. Gate this with a deterministic fake
   device harness; real hardware remains a one-command smoke, not a subjective check.

6. **Close H10.** Run the focused H10 lane and full `ctest --preset ci --output-on-failure`, push, verify
   remote CI, then update `STATUS.md`, `loop/horizon.md`, and `docs/goals/roadmap.md`.

## Non-goals (H10)

- No single-window UI, mixer UI, piano-roll UI, or visual accessibility shell - that is H11.
- No extra export formats beyond the DAWproject interchange slice and the existing canonical float32 WAV.
- No production plugin format expansion.
- No subjective listening checks; any golden update must be explicit and mechanical.

## Decisions to write (ADRs, in order)

- **ADR-0028 - loudness metering model:** BS.1770/libebur128 reference, channel handling, true-peak scope,
  and meter update surface. **[accepted; code checkpoint remote CI run `28341446711` green]**
- **ADR-0029 - DAWproject export subset:** package shape, supported fields, ID/asset mapping, and
  reference-reader gate. **[accepted; local gate green]**
- **ADR-0030 - time-stretch Node:** Signalsmith integration, ratio limits, latency/tail policy, and
  scheduler safety.
- **ADR-0031 - device hot-swap survival:** device-change state machine, fake-device gate, and hardware
  smoke boundary.

## Status

Opened on 2026-06-28 after H9 remote CI went green on `a5a1db4` (run `28339991428`). Kickoff docs are
green on remote CI run `28340551455`. ADR-0028 is accepted and green on remote CI run `28340956377`.
`YesDawLoudnessCheck` is implemented and green on remote CI run `28341446711`; the next checkpoint is
ADR-0030 plus `YesDawTimeStretchCheck`. ADR-0029 is accepted; `YesDawDawprojectPrimitivesCheck` is locally
green in the full `ci` preset **242/242**; and `YesDawDawprojectCheck` is locally green in the full `ci`
preset **243/243** plus the focused H10 lane **2/2**.
