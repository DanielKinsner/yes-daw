# Parking lot (the only place findings go during the G-arc)

Per [ADR-0046](../adr/0046-feel-first-shell-arc.md) §13 and the
[Real-DAW plan](../plans/2026-09-01-real-daw-ground-up-plan.md) §8.2: during the G0–G8 arc, no new
adversarial audit carves happen and no finding becomes a backlog item on its own. Every finding —
yours, a reviewer's, a tool's — is appended here with a `file:line` and a one-line "why it
matters". Items are **promoted only at a phase close**, and only if they serve the *next* phase's
exit. Promotion means: move the line into the plan's phase list in a docs commit and delete it
here.

Format: `- [ ] <date> · <area> · <one line> · <file:line> · promote-to: <phase or "later">`

## Carried in from the 2026-08-25 backlog's parked list

- [ ] 2026-08-25 · mixer · VCA / track grouping (fader groups) · `Project.h:364` (strip state has no group id) · promote-to: later (after G4)
- [ ] 2026-08-25 · mixer · stereo width control per strip · `PanNode.h:40` (balance law only) · promote-to: later
- [ ] 2026-08-25 · mixer · polarity invert + input trim per strip · — · promote-to: later
- [ ] 2026-08-25 · mixer · pan-law choice (−3/−4.5/−6 dB) · `PanNode.h:1` · promote-to: later
- [ ] 2026-08-25 · assets · streaming audio from disk instead of whole-asset decode in memory · `UiAppModel.h` `decodedAssets_` · promote-to: later (after G5.4)
- [ ] 2026-08-25 · recording · loop-record cycles beyond 8 are silently dropped · `UiAppModel.h:718` · promote-to: G7
- [ ] 2026-08-25 · hardware · owner loopback-cable PASS for the shipped record path · `docs/reality-lane.md:95` · promote-to: G7 (owner lane)

## Found while writing the plan (2026-09-01)

- [ ] 2026-09-01 · MIDI · `DecodedMidiClipNode` silently drops events past 1024 per Block · `src/engine/nodes/DecodedMidiClipNode.h:27` · promote-to: G3.3 (surface via status line or raise the cap)
- [ ] 2026-09-01 · RT · `Node::reset()` is not marked RT-hot, so RTSan does not enforce it · `src/rt/RtHot.h` · promote-to: G0.5 if the placement lane touches reset(), else later
- [ ] 2026-09-01 · automation · evaluator emits one parameter Event per frame (correct, event-dense) · `src/engine/Automation.h:211` · promote-to: G4.6 (block-ramp events)
- [ ] 2026-09-01 · time · audio clips with `TimeBase::TempoLocked` are refused by the renderer · `OfflineRenderer.h:252` · promote-to: later (needs the G2.9 stretch lane first)
- [ ] 2026-09-01 · export · no MIDI clip/notes in DAWproject export beyond the current subset · `src/interchange/DawprojectPackage.h` · promote-to: later
- [ ] 2026-09-01 · shell · `UiActionContext` carries ~30 test-only counters inside live UI state · `UiActions.h:248-329` · promote-to: G1.1 (move to a `TestCounters` struct)

## New findings (append below)


- [ ] 2026-09-01 · arrange · with the counter at 010|04 and playhead-follow on, no playhead line is visible in the lanes at 2560×1440 (SS-1 step 13 shot, zoom "2x") · `src/ui/MainComponent.cpp` `followPlaybackPlayhead` · promote-to: G2.16 (follow modes)
