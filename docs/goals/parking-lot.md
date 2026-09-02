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
- [ ] 2026-09-01 · shell · SS-1 step 1 assumes launch lands in an empty project; the shipped shell launches with NO project ("Create or open a Project") because every edit writes a full snapshot to a bundle path that only New/Open provide · `src/ui/MainComponent.cpp` constructor (last-project reopen) / `UiAppModel::createProjectBundle` · promote-to: G5.5 (new-project dialog / templates decide where an unsaved project lives); until then SS-1 step 1's empty-project line stays red by design and the G0 exit is evaluated with that one line excluded (logged in STATUS G0.2)
- [ ] 2026-09-01 · audio · the driver's xrun count reads 1 since launch in every G0.3 SS-1 run, with 0 during the edit burst and 0 deadline misses of our own — one xrun somewhere between device open and the first edits (device start? first Play?) · `src/ui/MainComponent.cpp` `audioDeviceAboutToStart` / probe `audio.underruns` · promote-to: G0.4 or G0.5 (localize with a per-step probe read; never hide)
- [ ] 2026-09-01 · engine · a running engine re-located to 0 is not bit-identical to a fresh engine for the first 5 ms: the fader's Linear5Ms smoother starts from its last value, a fresh one snaps to the strip gain then ramps to the automation lane's value (seen when G0.5 kept the engine alive across a delete/undo) · `src/engine/nodes/FaderNode.h` `reset()` / `CompiledGraph.h` lane priming · promote-to: G2.15/G2.16 (loop and locate determinism gates decide which start is the law)
- [ ] 2026-09-01 · engine · the runtime applies at most 64 commands per block, so a burst of N live edits lands within ceil(N/64) device blocks; the stopped transport does not drain commands at all (edits while stopped apply on the first block after Play) · `src/engine/Runtime.h` `Config::maxCommandsPerBlock` · promote-to: later (only matters above ~60 edits per 2.7 ms)

- 2026-09-01 (G0.7 cp1): the song fixture stacks its four MIDI clips on top of audio clips on the
  last four tracks — real overlap, but it reads as a bug in the arrangement; a "MIDI track" needs
  its own identity (icon, colour, no audio clips) → G2 (track kinds) / fixture realism.
- 2026-09-01 (G0.7 cp1): at 1280×720 the 260 px mixer dock leaves three whole 72 px rows; Logic
  keeps ~5 at that size with a shorter dock → the dock's default height is a G2 call.
- 2026-09-01 (G0.7 cp1): the header master card's meter is a 6 px sliver; the card wants a real
  stereo meter + LUFS stack at 260 wide → G0.7 cp2 or G2.
- 2026-09-02 (G0.7 cp2): the editor dock's plan height (300) waits for the inspector's fixed
  section stack to scroll or collapse (at 1536×960 the takes/FX sections drop; at the floor the
  timeline hides) → G2 inspector rebuild; `mixerHeight` pinned at 260 until then (STATUS D27).
- 2026-09-02 (G0.7 cp2): the rail's PAN/VOL cluster labels are 8 px captions in a 68 px box —
  legible, not a reference look → G2 rail pass.
- 2026-09-02 (G0.7 cp3): the playhead's ruler badge covers bar 1's number in the bars row; the
  badge belongs in the time row or the marker lane (Logic: the playhead's triangle rides the
  lower ruler edge) → G2 ruler/playhead pass.

- **Keymap editor: chord capture (2026-09-02, G1.7).** A chord is typed as text in the editor ("Ctrl+Shift+K"); a capture field that records the next key press is polish for a later G-item. Also parked: a strip-menu **Add Send ▸** submenu (§3.3) once the bus list can be a submenu (G4).

- **Inspector stack must scroll (2026-09-02, G2.1 cp2, STATUS D50).** With the plan's 300 px dock the CLIP tab's fixed section tops (takes at 426) drop the takes section whole at 1536×960 once the settings row is open; at 1080p the arrangement shows seven whole lanes, not the plan's nine, because the timeline's toolbar and status rows are not folded into the header. Both belong to the G2 inspector rebuild / header pass.

- **Timeline GPU frame-budget benchmark is within noise of its threshold on the hosted runners (2026-09-02).** `timeline_gpu_tests.cpp:84` (`sustained < kFrameBudgetMs` 16.6 ms) went red on runs `33611885978` (macOS 17.06), `33626513605` (macOS 18.54) and `33627456994` (Windows 16.88) on commits that do not touch the canvas paint; the outlier allowance (8) does not cover a sustained value 0.3 ms over. Reruns pass. Owner: the render-budget item (G3 layered rendering) — measure the hosted runners' floor before pinning a budget they cannot hold; never weaken the gate without that number.
