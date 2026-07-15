# YES DAW — Alpha gate (CP5)

*The scripted session Dan runs on the **packaged** build to declare alpha, plus the mechanical
close that actually gates the horizon. Per [H17 plan](plans/2026-07-03-h17-distribution-alpha-plan.md)
CP5 and CLAUDE.md ("verification is mechanical; nothing subjective gates CI or the horizon").*

> Status (2026-07-15): this is the **runbook + gate definition**. Its blockers have now cleared —
> **CP1 (all slices: open→validate→render→export bit-exact) and CP3 (packaging + the CI job that runs
> the packaged self-check) are done and merged.** So the reopen assert (`YesDawSelfCheck --selfcheck`),
> the export-exists assert, and the autosave-present assert are buildable now; the remaining two
> (export re-imports bit-exact, integrated loudness) still wait on a small verify-CLI capability
> (a WAV re-read + a libebur128 loudness read) — both now shipped as `YesDawSelfCheck --verify-wav`
> and `--loudness`. The human steps are runnable as soon as there's a packaged build.
> **`tools/alpha-verify.sh` / `.ps1` are now written** — they run all 5 asserts on a produced
> bundle+WAV, and their `--self-test` (each assert's negative control) is CI-gated on Linux+Windows.
> The full POSITIVE pass still needs a real produced song (H17 CP2's committed demo bundle, or an
> owner session), so the mechanical close is tooling-complete but not yet exercised end-to-end green.

## What "alpha" means here

Two layers, and only the first one **gates**:

1. **Mechanical close (the gate).** `tools/alpha-verify` sub-asserts are green **and** the packaged
   build has reality-lane PASS rows for playback and recording. This is objective, exit-0/1, and is
   the complete close condition. Nothing subjective gates it.
2. **The feel session (non-gating).** The one sanctioned human GUI check — does it look/sound right —
   run against the checklist below. Every finding becomes a **tracked task** (a token/layout fix or an
   explicit deferral), never a silent block. Dan's "alpha declared" line is a product milestone
   layered on top of — never gating — the mechanical close.

## Precondition

- A packaged portable build produced by `tools/package.ps1` (CP3), unzipped on a clean profile.
- `YesDawSelfCheck --selfcheck <bundle>` returns PASS on the demo fixture (CP1) — proves the build
  can open a project before you invest a whole session in it.

## The scripted session (run on the packaged build)

Do these in order, in one sitting, saving as you go. This exercises every alpha feature.

1. **New project**, set sample rate + tempo.
2. **Record audio** through a real interface (one take, then a punch-in over a section).
3. **Record MIDI** into a second track (instrument), a few bars.
4. **Edit**: trim/split a clip, add a crossfade, nudge timing, drop a couple of markers, set a loop
   region.
5. **Mix**: insert built-in FX on a track and on a bus (EQ + comp + a time FX), set a send, draw one
   automation lane (e.g. a fader or a plugin param).
6. **Export** the mix to WAV.
7. **Reopen** the saved project from scratch (close app → relaunch → open bundle) to confirm it
   round-trips.

## Mechanical sub-asserts (`tools/alpha-verify`, run after the session)

Run against the produced project bundle + exported WAV. Each is exit-0/1; any red = not alpha.

| Assert | Proves | Waits on |
|---|---|---|
| Export exists + non-empty | the render actually wrote a file | CP1 slice 3 (export) |
| Export re-imports **bit-exact** | the WAV is well-formed and reversible (H7 gate pattern) | `writeFloat32WavFile` / WAV reader |
| Integrated loudness in **−30..−6 LUFS** | the mix isn't silent or clipped-to-hell | libebur128 (already a dep) |
| Project **reopens with zero validator errors** | the saved bundle is sound | `YesDawSelfCheck --selfcheck` (CP1 slice 1 ✅) |
| Autosave present | crash-recovery default is on (CP4) | `PlaybackAutosave` / recovery |

Each assert ships with a **named negative control** in the same change (implementer-brief #8): a
synthetic failing input (empty export, corrupted WAV, silent mix, truncated bundle, autosave absent)
that must make the corresponding assert fail — a gate without its control is not done.

## Reality lane (owner-only, per `docs/reality-lane.md`)

On Dan's machine, against the **packaged** exe (not the dev build): playback smoke PASS + recording
round-trip PASS, rows appended by the smoke script only. An agent never writes a PASS row.

## Declaring alpha

When the mechanical close is green and the reality-lane rows are in, Dan records the product-level
line here:

```
ALPHA DECLARED: <version> — <date> — <one line on what the feel session found / deferred>
```

(none yet — mechanical close not reached)
