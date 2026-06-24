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
The global clock: playhead, play/stop/record, tempo map, markers.

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

**Sidechain**:
A second input that controls how a node treats its main input.

**Plugin delay compensation (PDC)**:
Automatically aligning paths so nodes that add delay stay in time with nodes that don't.

**CompiledGraph**:
The flat, contiguous, read-only result of compiling the editable routing — what a Snapshot *is*. The
audio thread iterates it in order with no scheduling or allocation.

**Event**:
One sample-accurate, block-sliced thing that happens (a parameter change, a note, an automation point).
Carries an exact offset inside the Block. MIDI is one kind of Event.
_Avoid_: message (when you mean our internal event), MIDI event (for non-MIDI events)

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

### Timeline & arrangement

**Track**:
A lane in the arrangement holding clips and a chain of nodes. Its mixer controls (fader, pan, meter)
belong to the track and *compile to* graph nodes — there is no separate channel-strip object.
_Avoid_: channel

**Clip**:
A non-destructive placement of (part of) an asset on a track, with its own start/end, gain, fades.
_Avoid_: region, segment

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

### Project & assets

**Project**:
The full saved body of work: tracks, clips, routing, automation, node state.
_Avoid_: session, set, document

**Project bundle**:
The folder/package on disk holding the project's database, copied assets, and caches.
Working extension `.yesdaw` (not final).

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

**Freeze**:
Temporarily rendering a track's processing to a cache to save CPU; reversible (like Premiere's
render preview). The permanent version — replace the track with its audio — is "flatten" (deferred).
_Avoid_: commit

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
  Depends on the UI/wedge. → ADR fork #2.
- **Project file specifics** — bundle extension, and whether storage-format words ("JSONB") ever
  surface to users. → ADR fork #5.
