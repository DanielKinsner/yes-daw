# 0029. DAWproject export subset

- **Status:** Accepted
- **Date:** 2026-06-28
- **Deciders:** Dan (owner), build agent
- **Related:** ADR-0005 (mechanical verification), ADR-0011 (Asset->Clip->Project indirection),
  ADR-0017 (MIDI Clip edit model), ADR-0020 (H7-H11 roadmap), ADR-0021 (canonical export WAV),
  H10 mixing/mastering and interchange plan, `CONTEXT.md` (DAWproject export), upstream
  [DAWproject format](https://github.com/bitwig/dawproject).

## Context

H10 needs interchange insurance before the H11 UI exposes project export. DAWproject is the right target
for this slice because its upstream format is stable at 1.0 and is a `.dawproject` ZIP package with UTF-8
`project.xml` and `metadata.xml`; the exporter can choose its media directory layout. The Project XML
schema provides the subset YES DAW needs now: `Track`, `Channel`, `Volume`, `Pan`, `Lanes`, `Clips`,
`Clip`, `Notes`, `Note`, `Audio`, and `File path`.

The hard constraints are:

- The gate must be mechanical and must not compare our writer output to our own writer helpers.
- H10 must preserve the current Project surface: tracks, audio clips, MIDI clips, timing, gain/pan, and
  asset references.
- YES DAW has stable 128-bit `EntityId`s, but DAWproject XML `id` values are XML IDs, so the mapping must
  be deterministic and valid XML.
- YES DAW does not yet have a full saved Track model or user-facing pan per Clip. The export must be
  honest about that instead of inventing hidden state.
- H10 DAWproject is export-only. Import is a later decision.

## Options considered

1. **Export the current Project surface as a narrow DAWproject subset and verify it with an independent
   reader.**
   - Pros: matches the H10 exit criterion; preserves real arrangement structure instead of flattening to
     stems; keeps code scope small enough for CI; leaves unsupported DAWproject features explicit.
   - Cons: the first export is intentionally narrower than the full DAWproject schema; synthetic tracks are
     needed until YES DAW stores real audio track rows.
2. **Adopt the upstream Java object model or generated schema bindings and target broad DAWproject
   coverage now.**
   - Pros: closer to the complete standard; less hand-written XML mapping.
   - Cons: adds Java/generated-code complexity to a C++ headless gate; forces decisions for plugins,
     automation, warps, devices, and track hierarchy that YES DAW has not built yet. Rejected for H10.
3. **Flatten the Project into stems inside a DAWproject package.**
   - Pros: easy for other DAWs to import; can hide missing track structure.
   - Cons: loses non-destructive Clip/Asset identity, MIDI notes, source windows, and edit intent. That is
     not the H10 interchange gate.
4. **Emit only `project.xml` as a loose XML file.**
   - Pros: quickest shape check.
   - Cons: not a `.dawproject` package, no media-file proof, and too easy for the gate to pass without real
     interchange behavior.

## Decision

H10 implements **export-only DAWproject 1.0 for the current YES DAW Project surface**.

Package layout:

- `.dawproject` is a ZIP package with stored, uncompressed entries. Compression is not required for the
  format, and stored entries keep the H10 package reader/writer small and deterministic.
- The root entries are `project.xml` and `metadata.xml`, encoded as UTF-8.
- Referenced audio media is written under `audio/<asset-content-hash>.wav`.
- Each referenced audio Asset is written once as the ADR-0021 canonical float32 WAV: Project sample rate,
  32-bit IEEE float, interleaved, no dither/resampling. Unused Assets are not exported.

Project mapping:

- XML IDs are deterministic `yd_<kind>_<32 lowercase hex EntityId>` values. Parameter IDs append a stable
  role suffix, for example `_volume` or `_pan`.
- A DAWproject master `Track` is always emitted.
- Until YES DAW has saved audio Track rows, every audio Clip exports as one synthetic audio `Track`. The
  track ID is derived from the Clip ID; the Channel `Volume` stores the Clip gain, and the Channel `Pan`
  is fixed at DAWproject center (`0.5`, normalized). This preserves today's audible mixer state without
  pretending a user-authored track/pan model exists.
- MIDI Clips export as notes tracks grouped by their existing `MidiClip::trackId`. Notes keep key,
  channel, velocity, start, and duration.
- Sample-locked timing exports in seconds. Tempo-locked MIDI timing exports in beats using the YES DAW
  PPQ contract. Tempo-locked audio Clips are rejected for H10 because YES DAW has no audio warp/time-stretch
  decision yet.
- Audio `Clip` elements carry time, duration, source `playStart`, fade in/out, and an `Audio` child with
  channels, sample rate, duration, and `File path`.
- Tempo and meter export from the Project transport surface enough for the reader to validate timing
  units. Full tempo automation, plugin devices/states, sends, sidechains, clip launcher scenes, time warp,
  note expressions, and built-in device mappings are unsupported in H10.

Unsupported or lossy cases fail with explicit export statuses rather than silently degrading:

- invalid Project IDs or duplicate mapped XML IDs;
- missing or mismatched decoded Asset audio;
- audio Assets wider than stereo;
- tempo-locked audio Clips;
- non-finite gain/timing values;
- MIDI Notes with an unassigned channel (`-1`): legal as an internal sentinel, but DAWproject `channel`
  is `0..15`, so the export fails rather than silently coercing the note to channel 0;
- media write/package failures.

`YesDawDawprojectCheck` is the mechanical gate:

- Write a deterministic `.dawproject` package containing audio Clips, MIDI Clips, tempo/meter data, gain,
  center pan, fades, source windows, and shared Asset references.
- Read it back with a small independent package/XML reader that does not call the writer or compare raw
  strings.
- The reader reconstructs a reduced summary and verifies track count, XML ID uniqueness, Clip timing,
  MIDI notes, gain/pan, fade/source-window data, audio metadata, media file paths, and decoded WAV content.
- Negative controls must bite: missing media, duplicate XML IDs, malformed timing, wrong media metadata,
  unsupported time base, unsupported channel count, and changed gain/note data must fail the gate.

## Consequences

- **Positive:** H10 gets real interchange packaging without waiting for the full UI or plugin/device model.
- **Positive:** stable YES DAW IDs survive into the export in a deterministic XML-safe form.
- **Positive:** media references are tested through actual package bytes and WAV decoding, not through an
  XML string snapshot.
- **Negative / accepted costs:** audio tracks are synthetic until the native Project stores real track rows.
- **Negative / accepted costs:** pan is fixed center and clip gain is represented as channel volume because
  that is the only mixer state the current Project surface owns.
- **Negative / accepted costs:** tempo-locked audio, warps, plugins, automation, sends, scenes, and import
  are intentionally outside this checkpoint.
- **Follow-ups:** add the DAWproject package/XML writer, independent reader, and `YesDawDawprojectCheck`;
  leave import and broader schema coverage to later ADRs.
