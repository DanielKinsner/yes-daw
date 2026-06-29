# 0031. Device hot-swap survival

- **Status:** Accepted
- **Date:** 2026-06-29
- **Deciders:** Dan (owner), build agent
- **Related:** ADR-0002 (real-time foundations), ADR-0005 (mechanical verification), ADR-0006
  (RT-safe graph publish/swap), ADR-0022 (playback transport), ADR-0027 (scheduler safety guard),
  H10 mixing/mastering and interchange plan, `CONTEXT.md` (Device hot-swap).

## Context

H10 needs a deterministic device hot-swap survival path before H11 puts real device controls in the UI.
The current playback stack is intentionally split:

- `PlaybackEngine` owns the Project graph and the transport queue.
- `RuntimeAudioDriver` is the device-callback-shaped wrapper over `Runtime::processBlock`.
- The audio callback may only call `processBlock`; graph construction, publish, reclaim, and device
  lifetime all stay off the audio thread.

That split is the right foundation, but it means a hot-swap cannot mutate device state inside the audio
callback. A new device may have a different maximum Block size, and `PlaybackEngine::processBlock`
mechanically rejects Blocks larger than the max Block size it was created with. Preserving transport
position across a device change therefore needs a control-side state machine around `PlaybackEngine`, not
an audio-thread patch.

The gate must remain mechanical: no "listen for a glitch" and no real hardware dependency.

## Options considered

1. **Control-side stop/snapshot/rebuild/resume state machine around `PlaybackEngine`.**
   - Pros: keeps allocation and graph rebuild off the audio thread; reuses existing transport commands;
     lets the fake device prove exact frame continuity; handles changed device Block sizes by rebuilding
     with a new max Block size.
   - Cons: the first H10 slice models a clean device replacement, not a driver crash or sample-rate
     conversion.
2. **Mutate the live `RuntimeAudioDriver` in place while the callback keeps running.**
   - Pros: superficially closer to a seamless driver handoff.
   - Cons: would require cross-thread device lifetime mutation and synchronization in the hottest path.
     This conflicts with ADR-0002 and is rejected.
3. **Keep the same `PlaybackEngine` and only change the external device object.**
   - Pros: smaller API surface if the new device format is identical.
   - Cons: does not cover changed max Block size, which is the format change the current code already
     enforces. It would be a weak gate.
4. **Defer hot-swap to the future JUCE device layer.**
   - Pros: avoids modeling fake devices now.
   - Cons: leaves H10 without a mechanical survival proof and pushes a critical transport/state boundary
     into H11 UI work. Rejected.

## Decision

H10 implements **a control-side device hot-swap coordinator** around `PlaybackEngine`.

State machine:

- `Running`: one fake/real device callback is allowed to call the current engine.
- `StoppingOldCallback`: the device layer stops or quiesces the old callback before any engine rebuild.
- `SnapshotTransport`: after the old callback is stopped, the control side snapshots the playback state:
  playhead frame, playing/stopped, loop enabled, loop start/end, Project sample rate, channel count, and
  current output frame contract.
- `RebuildPlayback`: the control side rebuilds a fresh `PlaybackEngine` from the current Project and
  decoded assets with the new device max Block size.
- `PrimeNewCallback`: the rebuilt engine receives the restored transport commands (`locate`, loop state,
  play/stop) before the new fake device starts pumping audio.
- `Running`: the new callback resumes from the same absolute frame; old graphs are reclaimed on the
  control/janitor side.

Format scope for H10:

- Supported: same Project sample rate, same engine output channel count, changed device identity, and
  changed max Block size.
- Unsupported, with explicit status and no engine replacement: sample-rate changes, output channel-count
  changes, invalid max Block sizes, and attempts to rebuild while the old callback is still active.
- Driver crash recovery is not part of this ADR. A crash/timeout policy belongs with the future device
  layer and watchdog work.

Audio-thread contract:

- The device callback only calls the active engine's `processBlock`.
- No allocation, locking, logging, graph build, `PlaybackEngine::create`, or `reclaim` happens in the
  callback.
- If the coordinator is between devices, the fake device must not call the old or new callback. A callback
  while stopped is counted as a deterministic fake-device error, not tolerated as "probably fine."

`YesDawDeviceHotSwapCheck` is the mechanical gate:

- Build a Project fixture with decoded audio and an independent/offline reference.
- Pump the old fake device for several Blocks, trigger a device change at a non-zero transport frame, then
  resume with a new fake device using a different max Block size.
- Verify the concatenated output is bit-identical to the uninterrupted reference: no duplicated frame, no
  dropped frame, no zero gap, and no stale output after the swap.
- Verify loop state and stopped state survive a swap.
- Verify the old graph is reclaimed after the new engine is active.
- Negative controls must bite: unsupported sample-rate change, unsupported channel-count change, invalid
  max Block size, and attempted rebuild while the old callback is still active must fail without advancing
  or replacing playback.

## Consequences

- **Positive:** H10 proves the user-visible "device changed while playing" state boundary mechanically,
  without depending on hardware or subjective glitch checks.
- **Positive:** changed max Block size is covered by rebuilding the playback runtime with a new contract
  instead of weakening `processBlock` validation.
- **Positive:** H11 can wire real device UI to a proven control-side state machine.
- **Negative / accepted costs:** H10 does not resample for sample-rate changes or remap channel layouts.
- **Negative / accepted costs:** crash recovery is explicitly out of scope.
- **Follow-ups:** implement the coordinator and fake-device harness in `YesDawDeviceHotSwapCheck`; later,
  map real JUCE device notifications onto the same state machine.
