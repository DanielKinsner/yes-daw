# 0082. Full Tempo-map editing, click and MIDI interchange

- **Status:** Accepted (in-scope implementation decision under Dan's editing mandate; independent engine/saved-data critic approved this concrete document). Implementation and executable proof remain pending.
- **Date:** 2026-11-03
- **Deciders:** build agent and independent engine/saved-data critic, under Dan's authorized editing scope.
- **Related:** [ADR-0010](0010-time-model-and-sample-rate.md),
  [ADR-0017](0017-midi-clip-edit-model-and-render-bridge.md),
  [ADR-0049](0049-autonomous-delivery-and-usable-song-milestone.md),
  [ADR-0052](0052-automation-v2.md), [ADR-0078](0078-ship-first-cadence.md),
  [editing brief](../plans/2026-11-03-editing-improvements-headless-plan.md).

## Context

The engine already stores canonical musical Ticks at PPQ 15360 and supports Tempo jumps and linear
ramps. The shell still creates/imports SampleLocked MIDI using head-tempo arithmetic, configures a
constant head-tempo click, and exports through the legacy 960-PPQ bridge with incomplete arrangement
occurrences. Piano-roll grids and several arrange commands assume one native unit everywhere.

Dan authorized coherent musical editing after background media preparation. This decision completes
the existing timing model without reinterpreting old Projects or replacing Accepted ADRs. Architecture
approval is not implementation, performance, native-input or hardware certification.

## Options

A. **Use the full existing musical model throughout, preserve old data and add a faithful Project-to-SMF
boundary (recommended).** Shared timing and occurrence helpers keep the surfaces consistent.

B. Retain head-tempo conversions and patch individual symptoms. Supported ramps would still disagree
between editing, click and interchange.

C. Migrate old Clip coordinates or replace canonical storage. That would replace owner-settled units
and silently change saved meaning.

## Decision

Select A. Keep PPQ, TimeBase values, source units, graph/PDC and Audio-thread contracts unchanged.
The following are implementation requirements, not claims about code already delivered.

### Fractional musical time and Meter boundaries

Add checked render/control-side fractional forward/inverse helpers around the existing compiled
Tempo-map equations and cumulative prefixes. Preserve the whole-Tick APIs and their bit-identity
test. Independent analytic tests must also establish accuracy: the existing near-zero-slope constant
approximation is not proof for arbitrarily long, shallow ramps. Fix demonstrated numeric defects
or return a precise numeric/domain failure; do not weaken stored validity or silently clamp timing.

Compute Meter boundaries from each segment's rational origin, never by accumulating a rounded beat
length. A Meter change starts an accented bar and truncates the preceding bar. Cover valid fractional
periods such as 7/39, off-bar changes and checked long-range arithmetic. Round an endpoint once at its
destination boundary; no saved fractional coordinates are introduced.

### Metronome and captured count-in

Build immutable indexed Tempo/Meter data on Control. Audio enumerates onsets and remaining
five-millisecond click tails for each transport segment; it performs no allocation, locking,
logging, reclamation, trigonometry or inverse calculation per output sample. Floor onsets to
frames. Tempo changes retain beat phase; Meter changes begin a downbeat. Seek includes preceding
tails, loops repeat the same absolute timeline grid, and offline export contains no monitoring click.

Stored BPM remains any finite positive value. Coalesce onsets landing in one frame with downbeat
priority; skip collapsed ordinal ranges and coincident Meter segments with bounded indexed/search
work instead of iterating each beat. Check domains/overflow and expose named preparation/lookup
failure. Establish the worst-case deadline mechanically before claiming a processing bound.

A count-in is one full nominal starting-Meter bar **from the accepted cursor**, integrated through
the captured full Tempo map. Add its rational quarter-note span to the fractional start, and ceil
the fixed capture-opening frame. Capture configurations remain immutable. Intervening Project edits
may continue; Meter changes inside the span alter accents but not its accepted length.

Use captured-map onsets before that fixed boundary and current-map onsets at/after it, switching
inside Audio rather than waiting for a Control tick. Retain accepted pre-boundary tails. Completion
and forced count-in enable are one-shot Audio-owned state transferred across engine replacement.
After completion obey ordinary metronome enable; later seek/loop below the boundary must not restart
the count-in. Song playback can intentionally follow a newly edited map while the remaining count-in
uses its accepted snapshot. Existing short-loop/seek reachability is not claimed repaired here.

### MIDI defaults and piano-roll units

New empty/imported MIDI defaults to TempoLocked, with an explicit fixed-time creation/import option.
Legacy SampleLocked integers remain unchanged. Never convert an existing Clip by flipping its enum:
any explicit conversion maps all absolute Clip/Note/controller endpoints before deriving relatives.

Convert insertion frames once through the full map. An empty Clip ends at the next actual bar
boundary strictly after its start. Imported source-PPQ endpoints convert once to canonical Ticks;
file tempo does not replace the Project map. Round content end upward to an actual bar boundary,
preserving an exact boundary.

Piano-roll grid, snap, quantize and step movement use mapped positions over the active Clip's range
and its native time base. One constant beat/bar length cannot describe map changes. Origins come
from Project Meter segments; TempoLocked uses canonical Ticks and SampleLocked uses mapped relative
frames. Convert step destinations back to transport frames before locating.

### Arrange boundaries and complete copies

Arrange gestures, inspector positions and supported range boundaries use absolute Project frames.
Project each entity's absolute start/end through its own time law, never a duration independently
from zero. Compute group bounds, requested displacement and clipboard placement offsets in frames,
then convert each destination once into its entity's native coordinates.

A TempoLocked move preserves Tick length and relative Note/controller/loop positions; a SampleLocked
move preserves exact frame duration. Reproject after canonical Tick rounding: equal requested frame
displacement is not a sample-exact mixed-group alignment promise, and musical displayed duration can
change across a ramp. Track-only/zero-time-delta operations preserve native coordinates exactly.
Commands, persistence and undo retain native units. ADR-0052 Automation Follows Clips uses each
resulting native delta, not the UI frame delta applied to every lane.

Cover clipboard copy/paste/repeat, explicitly mapped repeat bounds and inspector ordering/reveal.
Existing audio-only range operations need correct conversions; this does not invent MIDI range
support. Preserve complete MIDI controller, mute, transpose, velocity and repeat settings when
duplicating; the current append-copy helper's omitted fields require a regression and repair.

### Project MIDI export

Add a pure Project-to-SMF format-1 boundary at PPQ 15360 with conductor metadata in Track zero.
Keep the legacy 960-PPQ bridge and its goldens unchanged. Share checked occurrence expansion before
frame flooring: repeats, controllers, partial final cycles, mute, transpose, velocity and absolute
placement. Selection-else-all chooses the exported Clips. TempoLocked events keep exact Ticks;
SampleLocked absolute endpoints use the full inverse map.

The horizon is the latest chosen unmuted Clip ending. Include the map from zero through that
horizon; a later ramp endpoint still defines its truncated slope, while later changes are omitted.
Preflight repetition/event counts with checked arithmetic before materializing occurrences.

SMF uses discrete integer-microsecond tempos and power-of-two Meter denominators. Refuse
unrepresentable Meter/numeric/resource/file-size cases before opening or modifying the destination.
Validate absolute-Tick capacity, four-byte VLQ deltas up to 0x0FFFFFFF, Track counts, chunk/text sizes
and narrow metadata fields before serialization. Supply deterministic controller/Note-edge order,
including zero-length pairs and quantized collisions; stable Tick sorting alone is insufficient.

Approximate the accepted clock deterministically with at most 0.1 ms reconstructed error throughout
the horizon, including integer-tempo rounding and accumulated prefix error. Long fractional constant
tempos need correction events or a named precision/resource refusal. Independently reconstruct the
serialized integer-tempo clock, checking interval endpoints and interior extrema where original
BPM equals 60000000 divided by the serialized tempo. Keep event-Tick quantization separate from clock
approximation; make no sample-exact external-player claim.

## Delivery and verification

After background imports, deliver checked time/grid helpers, then click/count-in, then musical MIDI
and arrange/piano-roll boundaries before activating their defaults, then shared occurrences/export.
Independent pure helpers can be parallel units only when actual source and verification ownership
are disjoint. Update CONTEXT when the new user-facing default/interop terms become implemented.

Use independent rational/analytic oracles, block sizes 1/7/128/257, seek/loop tails, collapsed high-BPM
onsets, captured-map count-in with delayed Control service, mixed native units across ramps,
zero-delta identity, complete copies, native automation, undo/save/reopen and pre-write refusals.
Serialized export clock tests check interior extrema and total prefix error, not endpoints alone.
Retain existing goldens, RTSan/TSan and full suites. Compile GUI consumers without launching; run
only audited offscreen harnesses locally. Native/hardware proof remains separate.

## Sources and limits

Current source: `engine/Time.h`, `engine/PlaybackEngine.h`, `engine/Midi.h`, `ui/UiAppModel.h`,
`ui/PianoRollInputComponent.h` and `interchange/Smf.h`. Prior design reviews confirmed the native-unit
mismatch and missing MIDI-copy fields, and required one-shot count-in and export preflight.

Primary format context: [MIDI Association: Standard MIDI Files](https://midi.org/standard-midi-files),
with [the SMF specification text mirror](https://midimusic.github.io/tech/midispec.html) for field/VLQ
constraints. The 0.1 ms clock budget and refusal policy above are this project's decision.
