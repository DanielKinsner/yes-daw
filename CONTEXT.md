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
A saved mixer target for a Return or sub-mix. It gathers one or more routed signals and has its own
strip state. The Master bus is the final output and is not the same as a user-editable Bus.
_Avoid_: channel, channel strip

**Master bus**:
The final bus everything sums into before audio leaves the app.
_Avoid_: main, output bus, master channel

**Send / Return**:
A tap that routes a copy of a signal to a separate Return node for parallel processing.

**Insert**:
A Node placed in a Track or Bus FX chain, processing that strip's signal in series (pre-Fader).
_Avoid_: effect slot (UI word), plugin (unless it is specifically a hosted Plugin)

**FX chain**:
The ordered list of Insert nodes on a Track or Bus strip. Saved as strip state; order is audible
and preserved.
_Avoid_: rack (a deferred UI concept), chain (when the routing branches)

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

**Monitoring policy**:
The user-visible choice for whether live input is heard during recording and which latency-compensation
rule applies to that monitoring path.
_Avoid_: listening mode (too subjective), monitor hack

**Latency calibration**:
A mechanical measurement or declared device-delay value used to place recorded input on the Project frame
it represents.
_Avoid_: manual nudge, by-ear sync

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

**Strip state**:
The saved mixer controls attached to a Track or Bus: fader, pan, mute, solo, and solo-safe. Meter and
loudness values are readbacks, not strip state.
_Avoid_: channel-strip object, transient UI control state

**Level**:
The loudness a meter shows.
_Avoid_: volume

**Track width**:
Whether a Track's strip runs mono or stereo. Derived from the Track's Clips: any stereo Asset makes
the strip stereo (ADR-0042). Not stored state; explicit user-chosen width arrives with recording-width
UX.
_Avoid_: channel count (for the strip), track format

**Balance**:
The stereo-strip counterpart of Pan (ADR-0042): centre passes both channels at unity; off-centre
attenuates only the far channel along the equal-power taper. Channels are never blended. Same
parameter target as Pan, so automation and the mixer control carry across widths.
_Avoid_: stereo pan, dual pan

**Loudness meter**:
The BS.1770/libebur128-aligned meter used for integrated, short-term, and momentary loudness checks.
It is a measurement surface, not an automatic gain change.
_Avoid_: auto-master, volume normalizer

### Timeline & arrangement

**Track**:
A saved lane in the arrangement. It owns audio Clips and MIDI Clips, carries strip state, and compiles to
the graph path that makes the Track audible. There is no separate channel-strip object.
_Avoid_: channel

**Armed Track**:
A Track selected to receive the next audio or MIDI recording pass.
_Avoid_: record-enabled channel, armed clip

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

**Take metadata**:
Saved Project bundle rows that identify a Take and link it to its Asset, Track, Clip, timing window,
device/input, and monitoring policy. It is Project truth, not something inferred from a filename.
_Avoid_: treating a recorded Clip as the Take identity

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

**Recorded audio asset**:
A recorded Take's immutable audio bytes inside the Project bundle Asset store. Canonical recorded audio is
RIFF/WAVE, 32-bit IEEE float, interleaved, at the Project/device sample rate.
_Avoid_: `.ysdtake` (internal test artifact), raw float blob

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

**ParamSpec**:
The stable registry row describing one automatable parameter: ParamID, name, unit, range, mapping
(how normalized 0–1 maps to real units), and default. ParamIDs are append-only forever — they are
saved in Project bundles.
_Avoid_: reusing or renumbering a ParamID, raw index as parameter identity

**Automation lane**:
The saved series of Breakpoints for one parameter target (a ParamID on a specific Track, Bus, or
Node). It is data first; its editor UI is an H16 surface.
_Avoid_: curve (when you mean the stored lane), envelope

**Breakpoint**:
One saved automation point: tick position, value, and the curve shape to the next Breakpoint.
_Avoid_: node (overloaded), keyframe (a video word)

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

**Instrument slot** (G3.1 / ADR-0047):
A Track's persisted instrument: a kind (`TrackInstrumentKind` — None, SimpleSynth; sampler and
plugin later) and an opaque kind-versioned state blob. One instrument node per Track
(`Instrument`, keyed by the Track id) plays every MIDI Clip on it, so voices are shared and a note
sustains across Clip edges. None resolves to SimpleSynth for a Track holding MIDI.

**MIDI merge** (G3.1 / ADR-0047):
The Track's one merged MIDI stream: a `MidiMergeNode` (role `MidiMerge`, keyed by the Track) whose
inputs are the Track's MIDI sources (each Clip's `DecodedMidiClipNode`; the live input in G3.10).
The compiled graph merges N event-producing inputs into one time-ordered stream in the executor
(stable on ties, bounded by the per-block event budget, no allocation) — the N-event-input law.

**Live note lane** (G3.2 cp2 / ADR-0047):
The transport's bounded SPSC of live `Event`s (`PlaybackEngine::postLiveEvent`): a NoteOn / NoteOff
addressed to one Instrument by `NotePayload::targetNode`, drained block-top and handed to EVERY node
as the `ProcessArgs::liveEvents` side-band; the addressed Instrument takes its own. Stopped, the
transport keeps the graph running while a live note is held or its release rings, with
`Transport::clipsSilenced` (clip sources emit nothing and hold their cursors). Audition today; live
MIDI input (G3.10) reuses the lane.
_Avoid_: preview (the H-era term for rendering a Clip alone), MIDI thru

**Audition** (G3.2 cp2):
Sounding a key through the Track's Instrument on a press — the roll's keyboard column, a note press, a
pencil-drawn note — held while the mouse is, released on mouse-up; not an edit (nothing to undo).
The Track is the roll's Clip's Track, else the selected strip.

**Control event** (G3.3):
An editable non-note MIDI message stored in a MIDI Clip in clip-relative ticks with a stable Entity ID:
a kind (control change, pitch bend, channel pressure, poly pressure, program change), a number
(controller / key / program, where the kind has one), a normalized value (0..1, or −1..1 for a bend)
and the voice address the Notes carry. One value per tick per lane. It becomes a `Midi1` Event (the
three wire bytes; 7-bit, 14-bit for a bend) only at render, ordered BEFORE any note event at the same
frame.
_Avoid_: automation (that targets a Node parameter, not the instrument's MIDI input), CC message (when
you mean the edit object)

**Musical typing** (G3.6):
The computer keyboard as a two-octave keyboard (Logic's layout: A W S E D F T G Y H U J K O L P ; from
the base key, Z / X an octave down / up, C / V velocity down / up), toggled by Ctrl+K or the roll
header's Typing button. A typed note plays through the live note lane (the audition law) and holds until
its key lifts. Typing takes only the keys it owns; every other chord still reaches the keymap.
_Avoid_: virtual keyboard, QWERTY piano

**Step input** (G3.6):
A mode (the roll header's Step button) in which each typed or clicked note is ENTERED into the MIDI
clip at the playhead with the step length — the snap chooser's grid, a beat when snap is off — and the
playhead advances one step; Right is a rest, Left steps back; every entry is one undo step. The playhead
must sit inside the clip.
_Avoid_: step sequencer (a grid of pads; not this), record

**MIDI Clip settings** (G3.5):
A MIDI Clip's own playback settings, applied at render and persisted with the Clip: mute (a valid,
silent source), transpose in semitones (±48; a note pushed off the keyboard drops), a velocity offset
(−1..1, added and clamped) and a loop length in ticks (0 = the content plays once; else the content
window repeats to fill the Clip, the last repeat cut at the end). A split yields two plain (unlooped)
Clips; a join bakes the right Clip's transpose and offset into its notes and keeps the left's settings.
_Avoid_: region parameters (Logic's name), clip gain (the audio Clip's)

**Quantize settings** (G3.4):
The session's quantize recipe — the grid (the snap grid, or 1/8, 1/16, 1/32), strength %, swing % (odd
grid slots land this much of the interval late; 0 straight, 66 the cap), note ends (the end quantizes to
the straight grid too) and humanize % (a deterministic offset within that share of the interval, from a
seed that advances per humanized apply). Set in the inspector's quantize panel (the MIDI clip's CLIP tab);
applied by Q or Apply to the note selection as one undo step. Session state, not Project data.
_Avoid_: groove (a template, parked), swing 50 % = straight (that is Logic's scale, not ours)

**Control lane** (G3.3):
The piano roll's expression lane for ONE kind + number of control event (CC1 Mod, CC64 Sustain, Pitch
Bend, Aftertouch, Program), chosen by the lane's chooser; the pointer places, drags and erases points,
the pencil paints freehand, Shift+pencil draws a line. Velocity stays its own lane (a Note property).
_Avoid_: automation lane, MIDI Draw (Logic's name)

**Key window** (G3.2 checkpoint):
The keys the piano roll shows: as many as fit its grid at the legible row target (11 px), never fewer
than ten nor more than the widest window (25), scrolled by the wheel and clamped at the keyboard's
ends. One law (`pianoRollVisibleKeys (gridHeight)`) for the paint, the hit-test, the wheel clamp, the
surface snapshot and the state probe; a gate that addresses a key first grows the dock to the full
window.
_Avoid_: key range (the persisted clip range), zoom (the roll's time zoom)

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

**Autosave recovery prompt**:
The user-facing choice to restore or discard a valid Autosave snapshot when opening a Project after an
interrupted edit.
_Avoid_: crash dialog (too narrow), backup chooser

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

**Design token**:
A named visual constant (color, spacing, type size) the app's LookAndFeel consumes. Components use
tokens, never hard-coded values, so taste changes are central and mechanical.
_Avoid_: magic hex values in components

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

**Recording and device UX**:
The H13 app state where device selection, Track arming, monitoring policy, recording, take lanes, basic
Comp assembly, latency calibration, and Autosave recovery prompts are driven through real input paths and
mechanically asserted.
_Avoid_: recording feature (too broad), device setup (too narrow)

**UI input harness**:
The self-asserting H12 test driver that constructs the shipped `MainComponent`, performs deterministic
mouse/key gestures against hit-tested Components, and then verifies Project, selection, transport, mixer,
piano-roll, undo/redo, accessibility, frame-time, and save/reopen state without human judgment.
_Avoid_: manual QA script, visual spot-check, model-only harness, back-channel command path

**Arrange window**:
The whole editing view of the YES DAW app: track headers on the left, ruler and clip lanes in the
centre, the inspector on the right, and the editor dock below. The Timeline canvas is the drawing
surface inside it, not the window itself (ADR-0046).
_Avoid_: timeline (when you mean the whole view), main view

**Editor dock**:
The resizable bottom panel of the Arrange window that hosts one tabbed editor at a time: Mixer,
Piano roll, or an automation editor. Toggled per editor with the reference-DAW keys (ADR-0046).
_Avoid_: modal view switch, bottom bar

**Focus context**:
Which editor currently receives non-global keys: Arrange, Piano roll, or Mixer. A chord may map to
different actions in different contexts; transport chords work in every context (ADR-0046).
_Avoid_: keyboard focus (the JUCE widget notion), active panel

**Command router**:
The single control-thread entry that turns a key chord or a menu/context/toolbar activation into a
UI action for the current Focus context. Widgets never own keys; only an active text field does.
_Avoid_: per-widget shortcut, key listener on a button

**Object selection**:
The set of selected Clips, Notes, or Tracks that editing verbs act on. Exactly one object kind is
current at a time, decided by the Focus context.
_Avoid_: highlighted items, selection array

**Time selection**:
A half-open tick range on one or more Tracks, made on the ruler or across lanes, that verbs such as
split, cut, delete, loop-from-selection, and paste-to act on. It coexists with the Object selection
(ADR-0046).
_Avoid_: range (unqualified), edit selection (a Pro Tools word), marquee (the gesture, not the state)

**Edit mode**:
The rule for what happens to neighbouring Clips when one is moved, trimmed, or deleted: *Overlap*
(default — neighbours untouched), *No overlap* (the moved Clip trims what it covers), *Shuffle*
(neighbours close up or move aside; Pro Tools "shuffle", Logic "shuffle L/R").
_Avoid_: ripple (Reaper/video word — use Shuffle), drag mode (Logic's menu label; our term is Edit mode)

**Smart tool**:
The pointer tool whose action depends on where in a Clip it hovers: body moves, edges trim, top
corners fade, the lower band makes a Time selection, and the cursor shape announces each zone before
the press (Pro Tools smart tool; Logic pointer zones).
_Avoid_: modal tool switching for daily edits

**Nudge value**:
The user-chosen distance a nudge key moves the selection: a grid unit, a bar, a beat, milliseconds,
samples, or frames. Shown in the toolbar; changeable without a dialog.
_Avoid_: nudge amount hard-wired to the snap grid

**Snap mode**:
How a dragged position lands: *Grid* (absolute grid lines), *Relative* (keeps the object's offset
from the grid), *Events* (Clip edges, Markers, the playhead), or *Off*. A modifier held during a drag
temporarily inverts snapping.
_Avoid_: snap on/off as the only choice

**Session script**:
A numbered list of user steps with observable outcomes ("press Space; the playhead moves") that a
phase of the shell plan must pass. Written in plain words so Dan can run it too; executed by the
Session drive (ADR-0046).
_Avoid_: test plan (too broad), dogfood notes (those are findings, not the script)

**Session drive**:
The one-command Windows script that launches the real built `YesDaw.exe`, injects real mouse and
key input, screenshots the window, reads the State probe, and asserts a Session script exit 0/1.
A mechanical gate class alongside ctest (ADR-0046).
_Avoid_: computer-use permission flow, manual clicking, headless harness (that is the UI input harness)

**State probe**:
A debug-only JSON snapshot of shell state (transport, selections, Focus context, view state, last
action, frame times, callback-registration count) the app writes each UI tick when
`YESDAW_STATE_PROBE` names a file. Read by the Session drive; never present in a normal launch.
_Avoid_: log scraping, pixel probing for state

**Feel budget**:
A numeric limit on latency or work the shell may spend per action or per frame (action-to-paint,
paint-per-frame, engine rebuilds per edit script, audio-callback removals). Gated; only ever
tightened (ADR-0046).
_Avoid_: "feels fast", subjective smoothness

**Reference-DAW rule**:
Any UI question is settled by what Logic Pro does, then Pro Tools, then Cubase/Reaper consensus,
with the precedent written into the item. No invented chords or gestures where a precedent exists
(ADR-0046).
_Avoid_: house style, agent taste

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

**Alpha**:
The ADR-0037 dogfood milestone: one real song recorded, edited, mixed, and exported by the owner on
a packaged portable Release build, with mechanical sub-asserts. Not public, not signed, no
third-party plugins.
_Avoid_: beta (that adds signing/installer/hosting), release

**Reality lane**:
The standing set of one-command, self-asserting owner-machine hardware smokes whose dated PASS/FAIL
results are committed to `docs/reality-lane.md`. CI cannot run them; they are still mechanical.
_Avoid_: manual testing (they self-assert), CI gate (they are owner-machine, outside CI)

**Packaged hardware verifier**:
The H17 portable package's one-command Reality-lane entry point. It automatically runs packaged
playback, recording, and frame checks against default hardware, emits structured evidence, and never
requires a checkout, build tools, UI operation, listening, or visual judgment.
_Avoid_: dev smoke (runs from the build tree), manual test (asks the owner to interpret behavior)

## Open questions (resolve as we decide the wedge)

- **Product name & wedge — RESOLVED.** YES DAW is a full general-purpose multi-track DAW (not a
  stem/finishing tool); "DAW" is the right word. "YES DAW" remains a working title. → ADR-0003.
- **User-facing chain word** — what the user calls an item in a node chain (e.g. "device" vs. "effect").
  Depends on the final H11 wording; ADR-0032 decides the UI stack, not this label.
- **Project file specifics** — bundle extension, and whether storage-format words ("JSONB") ever
  surface to users. → ADR fork #5.
