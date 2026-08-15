# Mix-truth & strip-parity backlog — MIDI through the strip, a real mixer, honest paint (2026-08-14)

Carved after a fresh adversarial re-audit of current `main` (head `66cb10b`, the E1–E35 editing-first
run complete) against Pro Tools / Logic-class usability. The audit read the REAL projection and paint
code paths and judged rendered screenshots of the shipped shell at 1152×720 / 1536×960 / 1920×1080;
the load-bearing findings are quoted with `file:line` in the item hints below.

The headline finding: **the mix lies about MIDI.** Every Track gets a mixer strip in the UI
(`src/ui/UiMixerSurface.h:339`), but the engine only projects a Track that owns an audio Clip
(`src/engine/ProjectMixerProjection.h:388` — `if (! ownsAudioClip) continue;`). Each MIDI Clip is
instead given its OWN hidden strip at unity gain straight to master
(`src/engine/OfflineRenderer.h:426`–`457`), inheriting only the owning Track's mute/solo
(`:521`–`524`). So on a MIDI track the fader, pan, FX chain, sends and automation are **dead
controls** — they move, they persist, and they change nothing you hear. That is the exact class of
defect this project exists to kill, and it sits under every "MIDI is a first-class citizen" line
E8–E13 landed.

## How to work this list (non-negotiable)

The full protocol lives in `docs/goals/2026-08-11-overnight-backlog-run-brief.md` and the
2026-08-12 brief, and applies verbatim: one item at a time, strict top-to-bottom order,
audit-before-build, shipped-boundary gates that fail before and pass after, full local ctest with
the owner-file isolation ritual, one feature commit + exact-head nine-job CI green + a separate
docs-only evidence commit per item, never edit ADRs / goldens / `[[clang::nonblocking]]`
annotations / `.github/workflows/ci.yml`, never weaken or delete an existing gate (re-pinning
legacy assertions to NEW semantics is allowed and expected), stop after 3 consecutive red CI rounds
on one item. Multi-track behavior is gated on 3+ track fixtures. For UI work: render the real
shipped shell at real window sizes, judge it yourself against PT/Logic, iterate until it looks
legit, THEN lock the fix with a mechanical token/layout gate.

Items are numbered **M1–M14**; tick each here with the commit SHA + run id in the docs-only
evidence commit.

## Phase 1 — the mix tells the truth about MIDI

1. [x] **M1 — MIDI plays through its owning Track's strip.** DONE — feature `a02f912`, exact-head
   nine-job CI run `31842604705` green (first try), local 350/350 with NO assertions re-pinned (the
   unity/centre/dry path is bit-identical). Decision recorded in **ADR-0045**. The `[midi-strip]`
   gate (116 assertions) measures MIDI and audio in separate windows of one render: the MIDI Track's
   fader at 0.5 halves the MIDI peak exactly with the audio window bit-identical, hard-left pan
   silences only the MIDI window's right channel, bypassing the Track's EQ changes only MIDI,
   removing the Track's send drops the MIDI level, every edit undoes bit-identically. Red before at
   assertion 52 (0.22697 where 0.11349 was required — the fader did nothing).
   Original spec: a MIDI Clip was projected as its
   own `DecodedMidiClipNode -> SimpleSynthNode -> Fader(1.0) -> Pan(0) -> Meter -> Master` chain
   (`src/engine/OfflineRenderer.h:441`–`457`) — the ADR-0026 H9 stopgap, whose own text calls
   persisted instrument/track wiring "later product work". Route each MIDI Clip's instrument into
   its OWNING Track's source `SumNode` instead, so the Track's FX chain, fader, pan, meter, sends
   and automation apply to MIDI exactly as they do to audio, and project a Track that owns MIDI
   Clips even when it owns no audio Clip. Mono instrument output widens onto a stereo strip the
   same way `DecodedClipNode` does (ADR-0042). Playback and offline share `buildProjectGraph`, so
   export == playback by construction. **Needs a new ADR** (it supersedes ADR-0026's per-Clip
   projection shape — write ADR-0045; never edit 0026). No new persisted state, so no schema bump.
   *Gate:* a 3-track fixture (audio track, MIDI-only track, mixed track) rendered offline: the MIDI
   track's fader at 0.5 halves the MIDI peak EXACTLY and leaves the audio track's peak untouched;
   pan hard-left silences the right channel of the MIDI track only; an EQ/Limiter insert on the
   MIDI track audibly changes the MIDI render; a send from the MIDI track feeds its bus; a Fader
   automation lane on the MIDI track renders the closed-form curve; RT playback matches offline
   within 1e-6. Expected to fail before at the first assertion (fader 0.5 changes nothing).
2. [x] **M2 — Every Track projects; automation never orphans.** DONE — feature `71e652e`,
   exact-head nine-job CI run `31843851909` green (first try), local 350/350. Three dead controls,
   one cause; each proven red on its own (assertions 63, 101, 128 of the new gate): deleting the
   last Clip of an automated Track, removing an automated FX insert, and removing a send that sits
   before an automated one. Every Track projects now; an insert's lanes go with it in one undo step;
   a removed send's lane is dropped and later lanes are re-seated one ordinal down. The
   `[lane-orphan]` gate (137 assertions) drives the shipped controls on a three-track project. It
   also flushed a latent TEST defect — `timelineClipCenterPoint` always returned lane 0, so
   multi-lane gates could click the wrong clip; a lane-aware twin was added beside it. The H15/CP3
   projection-rejection case was re-pinned strictly stronger (clip-less track must PROJECT;
   unprojected bus and out-of-range send ordinal still rejected).
   Original spec: after M1 a Track with no Clips at
   all is still skipped, and any automation lane whose owner is unprojected fails the WHOLE
   projection (`src/engine/ProjectMixerProjection.h:599`–`609` → `InvalidAutomationTarget`), which
   surfaces as `adoptEditedProject` returning false (`src/ui/UiAppModel.h:6284`) — a SILENT
   refusal. Bite to confirm in the audit: automate a Track's fader, then delete that Track's last
   Clip → the delete should be silently refused and the user sees a dead Delete key. Project every
   Track (an empty strip projects a zero-input Sum, or an equivalent honest silence source — audit
   `GraphBuilder`'s fan-in rules first), so strips and lanes survive an empty Track. *Gate:* on a
   3-track fixture, delete the last clip of an automated track — the delete APPLIES, one undo
   restores it bit-identically, the project reopens, and the remaining tracks render bit-identically
   to before the deleted clip existed.
3. [x] **M3 — Track output routing (submix groups).** DONE — feature `e192fc7`, exact-head nine-job
   CI run `31844767502` green (first try), local 350/350. A Track carries `outputBusId` (invalid =
   master, the historical default): new `SetTrackOutput` verb (track diff family, property arm 24),
   `trackOutputsReferenceBuses()` validation, bus removal refused while a Track's output lands on
   it, projection routing the strip's post-meter output into the destination bus's sum (a bus
   carrying a track output projects even with no FX and no sends), additive schema **v13**
   (`track_outputs`) written only for non-default routing so default projects keep their v12 bytes,
   both migration gates re-pinned to v13, and an `mixer.track.output` chooser on the control lane
   (childCount 128→129). The `[track-output]` gate (90 assertions) proves routing three tracks to
   one bus leaves the mix identical, the BUS fader then halves all three EXACTLY, removal is
   refused while it carries outputs, and undo returns a bit-identical straight-to-master render.
   Compile-fail before (no `Track::outputBusId`, no chooser).
   Original spec: buses were fed ONLY by sends
   (`src/engine/MixerGraphProjection.h:434`) — a Track's main output always lands on master, so
   there is no group/submix workflow (drums under one fader). Add a persisted per-Track output
   target (master or a Bus) with an additive schema bump + migration gate (v12 is current; follow
   the locate-points / `master_strip` pattern — write the row only when it is non-default so legacy
   bundles round-trip byte-identically), a `SetTrackOutput` undoable verb with its property arm,
   projection routing the Track's post-pan output into the chosen Bus's sum, honest refusal of
   routing cycles, and a strip output chooser in the UI. *Gate:* three tracks routed to one bus —
   the bus fader at 0.5 halves all three EXACTLY, the master render equals the sum, a routing cycle
   is refused, save/reopen preserves the routing, one undo restores it, and a default project's
   `project.db` is byte-identical to the pre-M3 write. Expected to fail before at the compiler (no
   output field).

## Phase 2 — the mixer looks and works like a mixer

4. [x] **M4 — FX insert slots on the strip.** DONE — feature `cf948e1` + adaptive-height repair
   `4828185` + CI fix `71a78b8`; exact-head nine-job CI run `31847432954` green on the final SHA
   `639dad8`, local 350/350. Every strip paints its chain between the S/M row and the fader (name,
   bypass dot, empty wells, selected slot outlined); ONE law drives paint, click and gates
   (`paintedInsertRowBoundsForLane`, exported as `mainComponentPaintedInsertSlotBounds`); clicking a
   row selects that strip and opens exactly that insert's params, an empty row closes the panel.
   Judged in pixels at all three sizes and iterated twice (empty wells gained an outline;
   "Compressor" → "Comp" after it overflowed a 112px strip), plus a self-caught regression repaired
   before certification: the fixed-height block starved the timeline view's mini-mixer fader, so the
   block is adaptive now and a strip with no room falls back to the exact historical fader top.
   `[strip-inserts]` gate: 212 assertions. TWO RED CI ROUNDS, both real: `-Werror=range-loop-construct`
   on Linux/macOS (the E26 trap), then a fixture that described a send nothing routed.
   Original spec: judged from the rendered mixer at 1536×960: the only
   way to touch inserts is a stack of debug text buttons in the left control lane ("Audio 1 FX:
   none", "FX", "+ FX", "Audio 1 GR: none") while the strips themselves carry name + pan knob +
   S/M + fader and nothing else — no DAW mixer looks like that. Put a real insert slot column on
   EVERY strip: one row per slot showing the FX name, a bypass dot, empty slots reading as empty,
   click to select the slot (driving the existing param panel), and the existing add/remove/reorder
   verbs reachable from the strip. *Gate:* `[strip-inserts]` — painted slot rows exist for a
   3-track fixture with different chain lengths, clicking a painted slot selects exactly that
   insert on that strip (non-zero strip AND non-zero slot), bypass toggles the real verb and the
   render changes audibly, and slot geometry stays inside the strip at 1152×720 / 1536×960 /
   1920×1080.
5. [x] **M5 — Sends on the strip.** DONE — the feature landed inside commit `71a78b8` (a staging
   slip: it is labelled as M4's CI fix but carries M5's whole change; pushed history is never
   rewritten, and STATUS records it) plus fixture re-pins in `639dad8`; exact-head nine-job CI run
   `31847432954` green on `639dad8`, local 350/350. Each strip paints one row per PERSISTED send row
   (destination bus name, PRE/PST tap, a level bar), and a drag on the bar sets the level — press
   previews transiently, release commits ONE undoable `SetSendLevel`. The send readout was rewritten
   to read the real send rows (ADR-0044) and merge automation into them: it was built from
   AUTOMATION LANES alone before, so a send you added but never automated did not exist as far as
   the mixer surface was concerned. Two legacy fixtures that carried a SendLevel lane for an ordinal
   nothing routed were re-pinned with the real send row (every existing assertion held unchanged).
   Rows drop on a short strip exactly like the insert slots. `[strip-sends]` gate: 63 assertions.
   Original spec: same finding, send half: sends live only in the debug lane
   ("Audio 1 sends: none", "Send", "+ Send"). Put a send row per send on each strip: destination
   bus name, level, pre/post tap marker, and a control that edits the level through the existing
   `SetSendLevel` verb; the destination and tap choosers from E18 stay reachable from the row.
   *Gate:* `[strip-sends]` — painted send rows match the project's sends for a non-zero strip,
   dragging a painted send level changes the rendered bus contribution EXACTLY, tap/destination
   edits ride their existing verbs, one undo restores, and rows stay inside the strip at all three
   sizes.
6. [x] **M6 — Fader scale, unity and readouts.** DONE — feature `7b3c73c` + macOS repair inside
   `c1a3bd8`; exact-head nine-job CI run `31850019863` green on `7252d63`, local 350/350. The audit
   found a worse defect than the carve guessed: the live faders already travel 0..2 in LINEAR gain
   (unity at half travel) while the painted thumb multiplied gain by the rail height (unity at the
   TOP) — paint and control disagreed by half a fader on every unselected strip, and the rail's
   ticks marked nothing. ONE law now drives the painted thumb, dB ticks (0/-6/-12/-24/-60) and a
   distinct unity mark, and the readout carries its unit ("0.0 dB" / "-inf dB"). Persisted gain
   semantics unchanged, so no existing gate moved. `[fader-scale]` gate: 43 assertions incl. a 1.5
   boost raising the rendered peak by exactly 1.5. ONE RED ROUND (macOS `-Werror=unused-variable`
   on the factored-out rail local).
   Original spec: judged: track faders paint a bare rail with
   unlabeled dashes, unity sits at the very TOP (no boost range), and the numeric readout is a bare
   `0.0` with no unit; the master pane's fader sits mid-scale beside a labeled 0/-12/-24/-60 scale,
   so the two disagree. Give track/bus faders the master's dB law: labeled ticks, a unity mark
   below the top with the shipped boost headroom above it, and a `dB`-suffixed readout everywhere.
   Persisted gain semantics do not change — this is the mapping and the paint. *Gate:*
   `[fader-scale]` — the painted unity mark's y is the exact token-derived position, a click at the
   unity mark sets linear gain to exactly 1.0, the top of the rail sets exactly the token's max
   boost, readouts carry the unit, and track/bus/master share ONE scale law (pinned by comparing
   the three).

## Phase 3 — honest paint

7. [x] **M7 — No fabricated waveforms; MIDI clips show their notes.** DONE — feature `c1a3bd8` +
   renderer-tolerance repair `7252d63`; exact-head nine-job CI run `31850019863` green on `7252d63`,
   local 350/350. The canvas carries the real notes of each MIDI clip (mapped through the tempo
   map) and paints them as a mini piano roll inside the clip body — auto-ranged, strided to a token
   cap for the frame budget — while a clip with neither notes nor peaks paints an honest pending
   body (one centre line, no invented peaks). A clip whose notes share one pitch draws mid-band
   instead of pretending its pitch means a position. `[clip-paint-honest]` gate: 10 pixel-exact
   assertions on the shipped canvas paint, including that two pending clips on adjacent lanes paint
   identical bodies — the old hash-seeded placeholder never could. ONE RED ROUND (macOS: the
   lane-identity probe demanded pixel-exact equality across renderers; it now allows 1% of its
   area, which the pinned defect exceeds by orders of magnitude).
   Original spec: `drawClipWaveform`
   (`src/ui/TimelineCanvas.h:183`–`226`) synthesizes a waveform from a hashed seed when no peak
   cache is available, and MIDI clips always take that path (`src/ui/MainComponent.cpp:7902` pushes
   an EMPTY asset hash) — so every MIDI clip on the timeline paints a fake audio waveform of
   nothing. Replace the fake: MIDI clips paint their real Notes as a mini piano-roll preview
   (pitch-scaled bars inside the clip body), and an audio clip whose peaks are not ready yet paints
   an honest pending treatment (flat body, no invented peaks). Watch the frame budget — the E5/E9
   red rounds came from clip paint cost; the preview must stride like the waveform path. *Gate:*
   `[clip-paint-honest]` — a pixel probe proving a MIDI clip's painted content maps to its actual
   notes (moving a note moves the painted bar; an empty MIDI clip paints NO bars), the pending
   audio clip paints no invented peaks, and `YesDawTimelineGpuCheck`'s dense fixture stays inside
   the 16.6 ms budget.
8. [x] **M8 — Piano roll: a real keyboard and real velocity bars.** DONE — feature `f0dc2e8`,
   exact-head nine-job CI run `31850912382` green (first try), local 350/350. The audit sharpened
   the carve: the key column DID paint per-key rows, but white keys used the panel's raised grey, so
   it read as striped rows; white keys are now genuinely light with a dark label and black keys are
   dark, narrower, and sit ON them from the left edge. Velocity paints one bar per note anchored at
   its start (the joined path read as a curve through values that do not exist between notes); the
   Pitch lane keeps its line because it is a per-note MPE value. `[roll-sizes]` extended (106
   assertions) with pixel probes: the key column must hold BOTH genuinely light and genuinely dark
   pixels, and the velocity lane's painted columns must be isolated bars (< 1/4 of the lane width,
   longest run ≤ 8px) rather than a stroke spanning the lane.
   Original spec: judged from the rendered roll:
   the key column is just alternating dark rows with C3/C4/C5 text — there is no piano keyboard —
   and the velocity lane draws a LINE GRAPH between note points
   (`src/ui/UiPianoRollSurface.h:235`–`242` feeds points, the paint connects them) where every DAW
   draws a bar per note. Draw real white/black keys with octave labels, and draw velocity as one
   bar per note anchored at its start tick. *Gate:* `[roll-keyboard]` — pixel probes proving black
   keys paint at the exact semitone rows across the viewport (including after an E10 scroll/zoom),
   and `[roll-velocity-bars]` — one bar per note at the note's x with height proportional to
   velocity, no line segments between notes; both red against the current paint.
9. [x] **M9 — Floor-size layout defects.** DONE — feature `31ea2d0`, exact-head nine-job CI run
   `31851814735` green (first try), local 350/350. The header's master card was drawn at a FIXED x
   with a fixed width, so at the floor it ran past the window edge — label kept, meter and LUFS
   clipped. It is right-anchored against the gear now and drops WHOLE below a token minimum, with
   the LUFS readout riding its right edge. Every mixer utility row goes through one
   place-if-it-fits law, so nothing hangs past the panel's bottom edge. `[shell-sizes]` extended:
   the exported card law is empty exactly when the LUFS readout is empty, otherwise fully inside
   the window with the readout on its right edge, and no utility row's bounds may exceed the
   window bottom.
   Original spec: at the supported floor (1152×720) the header's MASTER
   card keeps its label but loses its meter and LUFS readout (a half-drop — E27's whole-section
   drop law applies to the inspector only), and the mixer control lane's bottom row is clipped by
   the panel edge. Apply the whole-section drop law to the header master card and make the control
   lane drop rows that do not fit instead of clipping them. *Gate:* extend `[shell-sizes]` — at the
   token floor the master card is present WHOLE or absent whole (never label-only), and no
   control-lane row intersects the panel boundary; red against the current layout.

## Phase 4 — workflow gaps a PT user hits in the first hour

10. [x] **M10 — Drag and drop audio files from the OS.** DONE — feature `fa45939`, exact-head
    nine-job CI run `31852819727` green (first try), local 350/350. The timeline input component is
    a `FileDragAndDropTarget`: a drop is interesting only with a project open and at least one WAV,
    and the drop POINT maps through the shipped canvas geometry to a lane and a snapped tick. The
    shared import verb gained an optional explicit start (all other callers keep the playhead law)
    behind a new `importAudioFileAt`; several files land on consecutive lanes at the same tick; a
    refused file changes nothing. `[file-drop]` gate: 79 assertions. HONEST LAW pinned rather than
    faked: an import is not an undo step (the verb clears the undo stack — the asset copy is a
    filesystem act), so the gate pins the import count and proves that DELETING a dropped clip
    undoes back to exactly where it landed. Making imports undoable is a separate product decision.
    Original spec: there is no `FileDragAndDropTarget`
    anywhere in `src/ui` — the only import path is Ctrl+I / the Import button, which always lands
    on the selected track. Accept file drops on the timeline: the drop point picks the TRACK (the
    lane under the mouse) and the START (the snapped tick under the mouse), multiple files import
    in one undo group onto consecutive lanes, non-audio files are refused honestly, and an
    unsupported WAV variant reports the same error the menu path reports. *Gate:* `[file-drop]` —
    `filesDropped` with real fixture paths at real coordinates on a 3-track project lands clips on
    the exact lane at the exact snapped tick, playback changes audibly, one undo removes them all,
    and a junk path is refused with the project byte-identical.
11. [x] **M11 — Multi-track record arm.** DONE — feature `b5c8961`, exact-head nine-job CI run
    `31857667381` green (first try), local 350/350. Arming ADDS a Track to an arm SET; each armed
    Track owns a fixed capture slot (own SPSC FIFO, own picked channel window, own session
    buffers) fed from the SAME device block under one shared recording window, and commits its own
    take to its own Track with per-Track ordinals and provenance. Single-arm laws hold by
    construction (the set IS the primary at size 1). Every armed row's badge lights and meters its
    own input. The `[multi-arm]` gate (148 assertions) proves distinct picks land distinct samples
    read back from the persisted WAVs; red before at assertion 18 (arming the third Track
    retargeted the arm off the first). The Shift+R shell gate was re-pinned to the additive law.
    **Honest substitution:** the gate below asked for "undo removes all three as one step" — a
    recorded commit is not an undo step in this app (bundle-owned persistence, same law as an
    import, see M10). The gate pins that real law instead and proves each take is removable with
    the undoable `deleteRecordingTake` verb without touching the other armed Tracks' takes.
    Original spec: the arm model is single-track by construction (E28–E35
    honest scope) — you cannot record a drum kit. Move to an arm SET: per-armed-track input channel
    picks, one capture buffer per armed track fed from the same device block, and one take per
    armed track at stop, each committed to its own track with the shared latency compensation. Keep
    every single-arm law intact (a one-track arm set behaves exactly as today). *Gate:*
    `[multi-arm]` — three armed tracks with distinct input picks record one deterministic block
    each; each committed take carries EXACTLY its own picked channel's samples; ordinals and
    provenance are per track; disarming one mid-session drops only that track; undo removes all
    three as one step. Expected to fail before at the compiler (no arm-set API).
12. [x] **M12 — Loop-cycle MIDI beyond cycle 0.** DONE — feature `b986434`, exact-head nine-job CI
    run `31858341164` green (first try), local 350/350. Each pending capture buffer now carries the
    H5 take ordinal it came from (the pending list had LOST it — sparse cycles are skipped when
    building it), and the MIDI hook runs for every cycle's take placing only that cycle's notes.
    One MidiClip per cycle that carried notes, beside that cycle's own audio take, sharing its
    exact window. The mapping is untouched, so pre-roll stays rejected in every cycle. Red before
    at assertion 42 of the extended `[midi-record]` gate (one MidiClip where three were required).
    Original spec: E34 honestly dropped MIDI captured after the
    first loop cycle. Map each cycle's MIDI events through the same per-cycle window audio already
    uses and commit one MidiClip per cycle alongside that cycle's audio take. *Gate:* extend
    `[midi-record]` — an 8-frame loop with events in cycles 0, 2 and 5 commits three MidiClips at
    the right per-cycle positions with the compensation applied; pre-roll events stay rejected.

## Phase 5 — monitoring truth and the H18 precondition

13. [x] **M13 — Latency-compensated monitoring (honest subset).** DONE — feature `8523651`,
    exact-head nine-job CI run `31859274754` green (first try), local 350/350. The compensated
    policy routes the armed pick through the armed Track's OWN strip DSP, in the strip's own order
    (with inserts Pan → FX… → Fader, without them Fader → Pan), built from the same FX node factory
    and the same ADR-0042 widen/balance law the graph uses — so the monitored signal carries the
    strip's gain, pan, FX colour AND exactly the strip's own reported latency (the delay the
    recorded take will have on playback). The chain is built and prepared on the control thread and
    published under the audio-suspend seam; the audio thread only copies, processes and sums, so
    RTSan stays green. `[monitor-compensated]` (183 assertions) pins it policy by policy; red before
    at assertion 22 (silence where the strip value was required). The E31 `[monitoring]` no-op
    assertion is re-pinned.
    Original spec: `LatencyCompensated` is a shipped
    chooser value that does NOTHING (E31 left it an honest no-op) — a dishonest control. Honest
    subset: monitor the armed pick through the armed Track's strip so the performer hears the mix
    path (FX chain, fader, pan), with the strip's reported latency aligned against the transport so
    the monitored signal lands where the recorded take will land. RT-safety is non-negotiable (no
    allocation, no locks on the audio thread; the RTSan job must stay green). If the aligned path
    cannot be made RT-safe within the item, land the strip-path monitoring alone and say so — but
    never leave the chooser value inert. *Gate:* `[monitor-compensated]` — policy-by-policy output
    assertions proving the compensated policy differs from DirectInput by exactly the compensation
    and that the strip's gain/pan/FX apply; RUNTIME red against today's no-op.
14. [ ] **M14 — Reality-lane Smoke 2: one real VST3 across the worker boundary.** ADR-0037 makes
    this smoke H18's precondition and `docs/reality-lane.md` records that it has NEVER run; the
    worker has only ever run passthrough and `src/plugin_host/PluginHostCoordinator.h` has no
    scanner. Build the smoke exactly as the reality lane specifies — point the existing worker at
    ONE named real VST3, load, process N blocks of known input through the RT lane, assert no
    crash, no watchdog kill, no NaN/Inf, output ≠ input, opaque state chunk round-trips — as a
    one-command self-asserting script following the E35 pattern (`PASS`/`FAIL` exit codes, exit 2
    for setup problems such as "no plugin installed"). **Guardrail from the ADR: this must NOT grow
    into hosting features** — no editor UI, no parameter surface, no scanner. *Gate:* CI compiles
    the tool, pins `--version`, and exercises the whole load→process→assert path against the
    existing synthetic worker plugin (the real-VST3 run is the owner-machine action).

## Explicitly out of scope — do not fake

- **MIDI CC / sustain / pitch-bend / aftertouch** — no CC model exists anywhere in the engine;
  adding one is a schema + editor + recording track of its own. The piano roll's "Pitch" lane is
  per-note MPE pitch (real data, flat for ordinary notes), not pitch-bend — do not relabel it as
  bend to look complete.
- **Same-track clip overlap policy** — overlapping clips sum today. Changing that is a product
  decision only Dan can make (PT crossfades/mutes the underlap; Logic trims). Do not silently
  change it and do not gate-bless it as "designed". **Recommendation when Dan picks it up:** adopt
  the Logic-style "later clip wins, earlier clip is trimmed non-destructively" law behind an
  explicit toggle.
- **H18 plugin hosting proper** (scanner, plugin identity/validation, blacklist UX, editor
  hosting). ADR-0037 makes it a horizon with its own kickoff ADR and preconditions it on the M14
  smoke — and the owner machine currently has **no VST3 installed** (`C:\Program Files\Common
  Files\VST3` is empty), so the precondition cannot be satisfied unattended. M14 builds the smoke;
  installing a plugin, running it, and writing the H18 kickoff ADR are Dan's calls.
- MIDI clip trim/split (note source-window semantics need a product decision first), punch-in/out
  UI, comping UI beyond the existing Comp button, per-track instrument choice / patch editing
  (ADR-0043 defers it), stems export, track height resize, track colour, reverse playback,
  time-stretch/elastic audio, tab-to-transient, ripple editing, freeze/bounce-in-place, video.

If an item above collides with one of these, land the honest subset and say so in `STATUS.md`.

## Definition of done (whole list)

Every landed item: gate-covered at the shipped boundary (failing before, passing after; 3+ track
fixtures for multi-track behavior), suite green locally with the owner-file isolation ritual, one
feature commit, EXACT-HEAD GitHub Actions run green across all nine jobs, then a separate docs-only
evidence commit ticking the item here with the SHA + run id and adding the STATUS certification
paragraph. Visual items additionally require rendered shipped-shell screenshots judged before the
gate is written.
