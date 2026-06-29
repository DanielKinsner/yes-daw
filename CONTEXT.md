# CONTEXT — Ubiquitous Language for YES DAW

The shared vocabulary for this project. Code, docs, and conversation use these exact words with these
exact meanings. This file is a **glossary only** — no implementation detail (that lives in
`docs/adr/`). It is still partly open; the questions at the bottom resolve once we choose the product
wedge.

## Language

### Engine & timing

**Audio thread**:
The one part of the program whose only job is delivering sound on time. It never does slow work.
_Avoid_: real-time thread (loosely), DSP thread

**Control thread**:
The non-real-time side that handles the screen, files, and user actions.
_Avoid_: main thread, UI thread (when you mean all non-audio work)

**Writer thread**:
The non-audio thread that drains recorded chunks to disk. It may do file I/O; the Audio thread never
does.
_Avoid_: recorder thread (when you mean the disk writer specifically)

**Recording FIFO**:
The bounded single-producer/single-consumer queue that carries fixed-size recording chunks from the
Audio thread to the Writer thread.
_Avoid_: recording buffer (ambiguous), unbounded queue

**Frame**:
One audio sample across all channels at a single instant.
_Avoid_: sample (when you mean every channel at once)

**Block**:
A small batch of frames the engine processes in one cycle. Its size can vary.
_Avoid_: buffer (when you mean the batch of frames, not the memory)

**Underrun**:
An audible glitch (click, pop, dropout) caused by missing the audio deadline.
_Avoid_: glitch, stutter, xrun

**Transport**:
The global clock: playhead, play/stop/record, tempo map, markers. In H8 playback code, the transport
also carries the current absolute Project `timelineFrame` for the audio callback segment.

**Transport command queue**:
The bounded SPSC control-to-audio queue that carries play/stop/locate/loop changes into the audio
callback. The audio thread owns live transport state after it drains this queue.
_Avoid_: calling transport fields directly from the control thread

**Loop region**:
A half-open timeline range `[start, end)` that repeats while the Transport is playing.
_Avoid_: inclusive loop end

**Tempo map**:
The timeline of tempo and time-signature changes.

**Tick**:
The canonical unit of musical position — an `int64` count, never a float. Samples are derived from
ticks through the tempo map, never stored as the source of truth.
_Avoid_: beat (when you mean the stored unit), sample position (as authoritative)

**PPQ**:
Ticks per quarter note — fixed at **15360**. A large grid so resolution is never the limit.

**Snapshot**:
The immutable, compiled form of the graph that the audio thread reads. Published by atomic swap;
the old one is freed off-thread. The audio thread only ever reads it.
_Avoid_: live graph (the editable side), current graph (ambiguous)

### Graph & mixing

**Node**:
The engine's internal processing unit. Built-in tools and plugins are both nodes.
_Avoid_: processor, effect, module

**Plugin**:
A node that wraps third-party code (CLAP / VST3 / AU). A kind of node.
_Avoid_: calling built-in nodes "plugins"

**PluginNode**:
The node adapter that represents a hosted plugin inside the graph. It behaves like any other Node to
the engine, while hosting details stay behind the adapter boundary.

**Plugin host child**:
The separate process that runs one hosted plugin. The audio thread never waits on it.
_Avoid_: plugin process (when you mean the graph-visible PluginNode)

**Plugin host coordinator**:
The control-thread supervisor in the main process that spawns, watchdogs, and tears down plugin host
children and owns the control-lane message channel. It is also what kills a hung child and escalates it
to the Plugin blacklist. The audio thread never talks to it.

**Plugin scanner**:
The non-audio discovery pass that inspects installed plugins and records what can be loaded.

**Plugin blacklist**:
The persistent quarantine for plugins that crash, hang, fail validation, or report unsafe properties.

**Graph**:
The one-way flow of nodes from inputs to outputs; never loops back on itself.
_Avoid_: chain (when the routing branches or merges), pipeline

**Bus**:
A node that sums several inputs into one (e.g. a group, or the master).
_Avoid_: channel, channel strip

**Master bus**:
The final bus everything sums into before audio leaves the app.
_Avoid_: main, output bus, master channel

**Send / Return**:
A tap that routes a copy of a signal to a separate Return node for parallel processing.

**Mute**:
A mixer state that silences a track or bus path without deleting routing. Explicit mute wins over Solo
and Solo-safe.

**Solo**:
A mixer state that listens to selected paths by muting other audible paths.

**SIP solo**:
Solo-in-place. Solo that keeps the normal graph and master route, rather than switching to a separate
solo bus.
_Avoid_: solo bus (when you mean SIP solo)

**Solo-safe**:
A path that Solo does not automatically mute. Explicit mute still silences it.

**Sidechain**:
A non-audible control route into a Sidechain input pin. It controls how a node treats its main input;
it is not a Send/Return or Bus.

**Sidechain input pin**:
An auxiliary input on a sidechain-capable Node. It is a graph input for timing and routing, but it is
not an audible mixer target.

**Plugin delay compensation (PDC)**:
Automatically aligning paths so nodes that add delay stay in time with nodes that don't.

**Monitoring latency compensation**:
The frame-placement rule that subtracts known input latency, and for loopback click-reference recording
also output latency, so a recorded take lands on the Project frame it represents.
_Avoid_: nudging, manual offset

**CompiledGraph**:
The flat, contiguous, read-only result of compiling the editable routing — what a Snapshot *is*. The
audio thread iterates it in order with no scheduling or allocation.

**Determinism gate**:
The scheduler check that requires the same Project graph to produce bit-identical output across worker
counts and the serial render reference. It fails on arrival-order-dependent floating-point behavior.

**Work-stealing scheduler**:
The engine scheduler that hands ready render jobs to a fixed worker set while preserving deterministic
sample order. H9 starts with scheduled render jobs over immutable graph snapshots; per-node DAG stealing
comes after the parallel-aware buffer pool.

**Event**:
One sample-accurate, block-sliced thing that happens (a parameter change, a note, an automation point).
Carries an exact offset inside the Block. MIDI is one kind of Event.
_Avoid_: message (when you mean our internal event), MIDI event (for non-MIDI events)

**Note**:
An editable musical note stored in a MIDI Clip, in clip-relative ticks, with stable Entity ID, pitch,
velocity, length, and voice identity. It becomes `NoteOn` / `NoteOff` Events only at render.
_Avoid_: raw MIDI message (when you mean the edit object)

**MPE voice allocation**:
The input/import boundary step that assigns stable voice addresses for per-note expression. The graph
preserves the address; Nodes do not guess one.
_Avoid_: voice stealing (that is an instrument-internal policy)

### Levels

**Gain**:
An adjustment applied to a signal (e.g. a clip's gain, an input's gain), usually before processing.
_Avoid_: volume

**Fader**:
A track or bus's main output control.
_Avoid_: volume

**Level**:
The loudness a meter shows.
_Avoid_: volume

**Loudness meter**:
The BS.1770/libebur128-aligned meter used for integrated, short-term, and momentary loudness checks.
It is a measurement surface, not an automatic gain change.
_Avoid_: auto-master, volume normalizer

### Timeline & arrangement

**Track**:
A lane in the arrangement holding clips and a chain of nodes. Its mixer controls (fader, pan, meter)
belong to the track and *compile to* graph nodes — there is no separate channel-strip object.
_Avoid_: channel

**Clip**:
A non-destructive placement of (part of) an asset on a track, with its own start/end, gain, fades.
_Avoid_: region, segment

**MIDI Clip**:
A non-destructive placement of editable Notes on a track. Unlike an audio Clip, it owns Notes instead
of referencing an Asset.
_Avoid_: MIDI region, MIDI file (unless discussing import/export)

**Take**:
One recorded pass placed on the timeline. Loop recording creates one Take per loop iteration.
_Avoid_: clip (when discussing the recorded pass before comping)

**Comp**:
A timeline selection assembled from one or more Takes.
_Avoid_: best take (too subjective), flatten (that means destructive replacement)

**Punch record**:
Recording only inside an armed timeline window.
_Avoid_: manual trim (the capture boundary, not an edit after recording)

**Loop record**:
Recording repeated passes over the same timeline loop, producing separate Takes.
_Avoid_: overdub (unless the mode actually merges into an existing Take)

**Asset**:
The underlying audio a clip points into. Copied into the project by default. Immutable and
content-hashed; never edited in place.
_Avoid_: file, sample (when you mean the imported audio)

**time_base**:
A clip's choice of how its position follows time: **tempo-locked** (moves with the tempo map) or
**sample-locked** (a fixed sample duration that ignores tempo). Set per clip, from the schema.

### Automation

**Automation**:
Control changes pinned to song position that the user draws or records (e.g. a fade over bars 4–8).
_Avoid_: modulation

**Modulation**:
A live repeating shape or envelope moving a control in real time, not pinned to the song. A separate,
deferred concept — not built early.
_Avoid_: automation

**Piano roll**:
The editor for MIDI Clip Notes: move, length, split/cut, quantize, transpose, and expression lanes.
_Avoid_: MIDI editor (too broad when you mean Note editing)

**Instrument Node**:
A Node that consumes Note Events and produces audio.
_Avoid_: synth plugin (unless it is specifically a hosted Plugin)

**Time-stretch Node**:
A Node that changes an audio signal's duration without changing Project placement. H10 decides its
Signalsmith-backed contract, latency, tail, and scheduler-safety rules.
_Avoid_: resample (that changes speed and pitch together), warp (too UI-specific)

**Instrument track auto-wire**:
The H9 headless bridge that turns a Project MIDI Clip into `DecodedMidiClipNode -> ImpulseInstrumentNode`
inside the mixer graph. It proves timing and transport; it is not the final user-facing instrument model.

**MIDI-effect Node**:
A Node that consumes Events and produces transformed Events, such as transpose, scale/chord, or arp.
_Avoid_: audio effect

### Project & assets

**Project**:
The full saved body of work: tracks, clips, routing, automation, node state.
_Avoid_: session, set, document

**Project bundle**:
The folder/package on disk holding the project's database, copied assets, and caches.
Working extension `.yesdaw` (not final).

**Autosave snapshot**:
A bundle-shaped last-good copy under a Project bundle's `autosave/` area. It is restored only after the
normal Project bundle validators accept it.
_Avoid_: temp file (too vague), backup (not necessarily user-managed)

**Plugin state chunk**:
Opaque saved bytes returned by a plugin, wrapped by YES DAW metadata before storage and associated with
the saved plugin node's Entity ID. It is restored as plugin-owned state, not rebuilt from parameter
values.

**Waveform cache**:
Regenerable visual peak data for drawing waveforms, built in the background.

**Entity ID**:
The stable identity of any saved thing (asset, clip, track, node). A **128-bit ULID** — never reused,
unique across projects, so templates, cross-project paste, and interchange stay unambiguous.
_Avoid_: rowid, index (when you mean stable identity)

### Render & export

**Export**:
The user action of saving finished audio or stems to a file outside the project.
_Avoid_: bounce

**Render**:
The internal offline process that generates audio faster than real time (what an export runs).
_Avoid_: bounce

**Canonical export WAV**:
The H7 bit-exact export file: RIFF/WAVE, 32-bit IEEE float, Project sample rate, Master bus channels,
interleaved samples.
_Avoid_: treating integer WAV, compressed files, or resampled output as the canonical gate format

**DAWproject export**:
The interchange package YES DAW writes so another DAW or reference reader can reconstruct the supported
Project surface. It is export-only in H10 unless a later ADR says otherwise. Unsupported or lossy cases
fail with explicit statuses rather than degrade silently (ADR-0029) — e.g. a MIDI Note with an
unassigned channel (`-1`) is rejected because DAWproject `channel` is `0..15`.
_Avoid_: backup, native project file

**Device hot-swap**:
Changing the active audio device while a Project is open without corrupting engine state or losing the
Transport position. H10's gate is a deterministic fake-device survival check, not a subjective hardware
test.
_Avoid_: device setup (too broad), driver crash recovery (a different failure mode)

**Freeze**:
Temporarily rendering a track's processing to a cache to save CPU; reversible (like Premiere's
render preview). The permanent version — replace the track with its audio — is "flatten" (deferred).
_Avoid_: commit

### UI & accessibility

**YES DAW app**:
The native JUCE single-window application shell that presents the Project, timeline, transport, mixer,
piano roll, and H7-H10 feature surfaces.
_Avoid_: browser shell, prototype window

**UI action registry**:
The stable list of user-facing actions, each with an action ID, label, default key binding, enabled state,
and command-layer implementation or read-only query. Menus, buttons, shortcuts, accessibility, tests, and
future agents all use the same action IDs.
_Avoid_: button callback as the source of truth

**Keymap**:
The remappable mapping from keyboard shortcuts to UI action IDs.
_Avoid_: hard-coded shortcut

**Timeline canvas**:
The dense arrangement drawing surface inside the YES DAW app. It uses the Project/timeline projections and
may use a GPU-backed renderer, but it is driven by measured frame-time gates rather than visual judgment.
_Avoid_: WebView timeline, CPU-only proof of smoothness

**Accessibility tree**:
The semantic roles, names, keyboard reachability, and actions exposed by the YES DAW app for assistive
technology and headless verification.
_Avoid_: visual labels only

**Operable Session UX**:
The H12 app state where a Project can be created/opened/saved, audio can be imported into the Project
bundle, timeline Clips and MIDI Clip Notes can be selected and edited through real input paths, mixer and
inspector values can be changed, transport feedback is visible, undo/redo works, and save/reopen parity is
mechanically asserted.
_Avoid_: calling a painted mockup or projection-only surface "operable"

**UI input harness**:
The self-asserting H12 test driver that performs deterministic app gestures and command paths against the
YES DAW app model/Components, then verifies Project, selection, transport, mixer, piano-roll, undo/redo,
accessibility, frame-time, and save/reopen state without human judgment.
_Avoid_: manual QA script, visual spot-check

### Product & AI

**Stem**:
An isolated part of a mix (drums, bass, vocals, other).

**Stem separation**:
Splitting a mixed track into stems with a local model.

**Finishing / mastering assistant**:
A feature that analyses audio, suggests a chain, and lets the user override every move.
_Avoid_: auto-master (implies the user has no control)

**Local-first**:
Models and data run on the user's machine — private, offline, no per-use cloud cost. (Applies to the
separate stem/mastering apps; YES DAW itself also runs fully local.)

## Open questions (resolve as we decide the wedge)

- **Product name & wedge — RESOLVED.** YES DAW is a full general-purpose multi-track DAW (not a
  stem/finishing tool); "DAW" is the right word. "YES DAW" remains a working title. → ADR-0003.
- **User-facing chain word** — what the user calls an item in a node chain (e.g. "device" vs. "effect").
  Depends on the final H11 wording; ADR-0032 decides the UI stack, not this label.
- **Project file specifics** — bundle extension, and whether storage-format words ("JSONB") ever
  surface to users. → ADR fork #5.
