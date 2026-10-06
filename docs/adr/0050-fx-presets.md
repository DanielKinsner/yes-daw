# 0050. FX presets: per-user files of real values keyed by stable parameter names

- **Status:** Accepted (agent, 2026-10-05, under ADR-0049's implementation-ADR clause, after a separate
  agent critic; its required changes are folded in below; committed alone, before any preset code)
- **Date:** 2026-10-05
- **Deciders:** build agent (proposer), separate agent critic
- **Related:** the plan's G4.2 cp7 ("presets save/load including malformed preset refusal") in
  [`docs/plans/2026-09-01-real-daw-ground-up-plan.md`](../plans/2026-09-01-real-daw-ground-up-plan.md);
  ADR-0049 (autonomous delivery; implementation ADRs); the `ParamSpec` law (`src/engine/ParamSpec.h`);
  G5.6 (persistent preferences) for the per-user state directory it will share.

## Context

A built-in effect's settings live in its insert (`FxInsert::normalizedParams`, normalized 0..1 by
numeric parameter id) inside one project; the bundle persists them the same way
(`fx_insert_params (insert_id, param_id, …)`). A user wants to keep a setting ("Vocal comp", "Slap back")
and reach it from another song. Three things are hard to reverse once files exist on users' machines:
**where** presets live, **what** a preset file stores, and **how** a bad file is treated.

Inside one project the numeric id and the normalized mapping are fine — the project and the code move
together. A preset outlives both: an id is a position in a kind's table and a normalized value follows the
spec's range and curve, all of which may change as the built-ins mature. The spec's `name` and its real
unit are what a person means by a setting, so **for presets** they are the chosen durable identity. (The
project does not key on names today; this decision does not change the bundle.)

One wrinkle: names are not unique within every kind. The EQ's 24 parameters are six bands of the same
four specs (`eq.band.type`, `eq.band.freq`, `eq.band.gain`, `eq.band.q`).

## Options considered

1. **Per-user files of real values keyed by stable names (chosen).**
   - Pros: survives id reordering and range or curve changes; readable and diffable; one file per preset
     is easy to share, back up or delete; no project schema change.
   - Cons: a renamed parameter orphans its value (refused below, never silently guessed).
2. **Normalized values by id (the insert's own encoding).**
   - Pros: no mapping code.
   - Cons: any range, curve or table change silently turns old presets into different sounds.
3. **Presets inside the project bundle.**
   - Pros: travels with the song.
   - Cons: unreachable from another song — the point of a preset; the song already stores its settings.

## Decision

Option 1, for **every** built-in `FxKind`, the four MIDI FX included.

- **Location:** the per-user session-state directory (`%APPDATA%/YES DAW/…` natively; the harness's own
  directory in tests) under `presets/<kind name>/`, one UTF-8 JSON file per preset, `.yesfx`. Kind names
  are fixed: `EQ`, `Compressor`, `Delay`, `Reverb`, `Limiter`, `MIDI Transpose`, `MIDI Scale`,
  `Arpeggiator`, `Chord`. A preset name is the file stem: letters, digits, space, `-`, `_`, `(`, `)`, 1–64
  characters after trimming, never a Windows device name (`CON`, `NUL`, `COM1`…). Saving over an existing
  name — compared without case, as Windows does — is refused (the user picks another); delete and rename are
  file operations, outside this decision.
- **Content:** `{ "format": "yesdaw.fx-preset", "version": 1, "kind": "<kind name>", "params": { … } }`
  with **every** parameter of the kind as **key → real value** in the spec's unit (`mapNormalized`). A
  choice-shaped parameter (`choiceCount ≥ 2`) is written as its whole choice index — the nearest choice to
  the mapped value, which is what the node itself plays — and loads onto that choice exactly
  (`normalizedForChoice`). The key is the spec name; when a name repeats within
  the kind, every occurrence carries its 1-based ordinal in id order (`eq.band.freq.1` … `eq.band.freq.6`).
  Numbers are written and read in the C locale (`juce::JSON`), never the user's decimal comma.
- **Load is all or nothing.** The file must be at most 256 KiB on disk (checked before it is read, a BOM
  counted) and parse as UTF-8 JSON (a leading UTF-8 BOM is skipped); carry the format tag; a whole version
  from 1 up to the supported 1; the kind of the insert it loads onto; and every key a parameter of that kind
  with a finite value inside its `min..max` (`ParamSpec` guarantees `max > min`), a choice's value a whole
  index. Any failure refuses the whole preset with
  one specific reason and leaves the insert untouched. A parameter the file omits takes its spec default (a
  preset is a complete setting, so a load is deterministic).
- **Undo and engine:** a load is **one** undo step — one `beginTransactionGroup()` of the existing
  `SetFxInsertParam` verb — adopted through the ordinary edit path (bundle write and engine rebuild).
- **UI:** the FX editor's title row gains a **Presets** button; its menu lists this kind's presets
  (alphabetical, case-insensitive) and **Save Preset…**. Refusals and results reach the status line. The
  button is keyboard reachable through the Control target like every button.

## Consequences

- **Positive:** settings travel between songs; the format outlives internal id/range changes; a broken or
  foreign file can never half-apply.
- **Negative / accepted costs:** a renamed parameter, or a reordered EQ band table, refuses or remaps old
  presets until a migration is written; no factory presets yet; no in-app delete or rename.
- **Follow-ups:** `CONTEXT.md` gains **FX preset**. Gates: round-trip of every built-in kind (the EQ's six
  bands distinct); refusal of malformed JSON, wrong format, newer version, wrong kind, unknown key,
  out-of-range and non-finite values, an oversized file and bad names, each with the insert unchanged;
  one-step undo; the Presets menu through the real editor.
