# What YES DAW takes from SoundCraft (2026-10-11)

**Source:** [storytold/soundcraft](https://github.com/storytold/soundcraft), reviewed at its 2026-10-11 head (Rust + egui,
~72k lines, MIT OR Apache-2.0, positioned as a "clean-room reimplementation of Avid Pro Tools"). Two independent
comparisons (five dimensions each, every key claim re-checked by a separate skeptic; the second on Opus 5.5) plus a
reuse audit (licence, DSP, crates, strategy) and local proof (its build, its test suite: 533/540 on Windows; its CLI
bounced its demo song).

**Bottom line.** Its *code* does not come over: it is Rust (we are C++/JUCE, ADR-0004), its engine breaks our hard rules
(ADR-0002: the audio callback waits on a rayon lock and allocates every block; plugins run in-process), and its UI is
egui. Its *design* and a set of *features and checks* are worth taking - rebuilt natively, under our ADRs, to our gates.
The licence allows using it as a reference (MIT; credit SoundCraft in a THIRD-PARTY-NOTICES file if any algorithm is
ported line-for-line).

## 1. The UI: a density pass (rebuild in our shell)

SoundCraft looks clean because it shows a lot, in order, at a glance. Take the structure, not Avid's trade dress (our
reference rule stays Logic, then Pro Tools; generic DAW conventions are everyone's).

1. **Ruler stack** - bars|beats, min:secs, tempo, meter and markers as separate thin rulers, each one shown or hidden; the
   tempo ruler editable in place (click adds a change). (`ui-egui/src/edit_window.rs:278-342`)
2. **Fuller track headers** - name, record / input / solo / mute, a view chooser (waveform / notes / automation), the
   automation mode, volume and pan readouts and a one-line insert summary ("EQ · Comp"). (`docs/images/edit.png`)
3. **Side lists** - a Tracks list (show / hide, select) and a Clips list beside the lanes.
4. **Transport cluster** - selection start / end / length, grid and nudge readouts, count-in, meter and tempo readouts.
5. **Mix window strips** - inserts and sends visible on every strip (not behind a click), I/O, automation mode, group, and
   a tiny EQ curve drawn on the strip when an EQ is inserted. (`mix_window.rs:182-209`)
6. **Overview strip** - the whole arrangement as a thin minimap with a draggable view box and the playhead.
   (`edit_window.rs:76-112`)
7. **Big counter and a markers window** - dockable / floating.
8. **Command palette** - type to find any of our 239 actions, with its chord; keyboard-first, cheap (the registry
   already carries labels, chords and roles). (`palette.rs`)
9. **Clip names from the file name** instead of a wall of "Audio Clip"; a **clip-gain line** on the clip with an inline dB
   handle. (`engine/src/io.rs:22`, `edit_window.rs:1213-1240`)
10. **Interface Scale (75-150 %) and Reduce Motion** in preferences (ADR-0064's matrix already proves 100-200 %); a light
    theme later on the token system.
11. **A demo song on first launch** - synthesised in code, so a new user hears something at once. (`engine/src/demo.rs`)

Not to copy: its "ms/frame" readout (measures the gap between frames, not render cost), menu toggles nothing reads, view
state (scroll) that marks the project as modified.

## 2. Features

- **More effects** (natively, with golden tests; an "FX suite 2" ADR - ADR-0038 deferred these): gate / expander,
  saturator (with oversampling - its own aliases and one mode reaches ~+8 dBFS), chorus / flanger, phaser; de-esser only
  if ADR-0038's "YES Voice" boundary is reopened. Small native adds: polarity / trim, compressor sidechain HPF and mix.
- **Export:** AIFF / FLAC (JUCE writers we already link) and a peak / true-peak / LUFS report when an export finishes.
- **Project Audio Health** - missing media, sample-rate mismatches, clip windows outside their source, as a menu item
  and JSON.
- **A command surface for agents** - our UiActionId registry over a headless CLI, a JSON control port and MCP, with an
  "inspect" read-back: drives that cannot steal Dan's focus or mouse.
- **For G4.8 (plugins):** CLAP hosted in our out-of-process worker (JUCE 8.0.4 cannot load CLAP; its FFI shape is a
  reference), VST3 state saved as one blob (component + controller), one undo step per plugin-editor gesture, fixture
  plugins built in CI and hosted end to end. Not its in-process hosting, its 120 BPM hard-coded VST3 tempo or its null
  CLAP transport.
- **For G7 (recording):** playlist lanes under the track with click-to-promote as the minimal comping UI. Not its
  recording into one growing RAM buffer on the audio thread.

## 3. Checks (cheap, high value - no phase needed)

- **Every action, every state:** dispatch each UiActionId on an empty project and on the song fixture with default and
  hostile payloads; no crash, invariants hold, undo restores the bytes.
- **Random edit sequences** with full undo / redo round-trips; **mutated saved projects** (every numeric column pushed to
  extremes: open cleanly or refuse whole, render stays finite); **an FX fuzz** over every built-in node.
- **A ratchet** - CI fails if the test-case or drive-assertion count drops below its last floor.
- **An RTSan canary** (one deliberate allocation in an RT-hot scope, expected to fail) so CI proves the sanitizer fires;
  a **layering gate** (engine and persistence never include UI headers).

## 4. Do not take

- Its engine patterns (thread-pool fork-join from the audio callback, per-block allocation, the session document on the
  audio thread, in-process plugins, RAM recording).
- Its Pro Tools surface: the 512-item menu tree, Avid product and plugin names (AudioSuite, Elastic Audio, Dyn3, EQ3,
  BF-76...), "Pro Tools" strings; ArtCraft branding; its loudness meter (`meter.rs` reproduces libebur128's filter code
  uncredited - we already use libebur128 itself).
- Its parity percentage as a measure (it counts menu labels, some of whose commands set flags nothing reads).

## Where each part lands

| Part | Lands | Needs |
|---|---|---|
| §3 checks | the next checkpoints, as small units | nothing new (they serve the active milestone's proof) |
| §1 UI density pass | after the Usable-song milestone, before G4.8 (recommended) | an implementation ADR; Dan picks the order |
| §2 effects | with or after the density pass | an "FX suite 2" ADR |
| §2 G4.8 / G7 items | inside those phases | their own ADRs (ADR-0015 for hosting) |
| §2 export, health, command surface | parked in `docs/goals/parking-lot.md` until promoted | a phase close promotes them |
