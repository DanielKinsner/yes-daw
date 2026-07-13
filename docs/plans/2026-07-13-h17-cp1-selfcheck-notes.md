# H17 CP1 — `--selfcheck` implementation notes + one decision

*Vera, 2026-07-13 (Fable session). Companion to
[`2026-07-03-h17-distribution-alpha-plan.md`](2026-07-03-h17-distribution-alpha-plan.md) CP1.
Written from a Linux session that cannot build C++/JUCE — so the code lands on branch `vera/h17`
labeled "needs build-verify," and this note records the reasoning so nothing is lost.*

## Decision needed from Dan: exe-mode vs. dedicated console app

The CP1 plan says `YesDaw --selfcheck <bundle>` — i.e. a **mode of the GUI exe**. I implemented it
instead as a **dedicated console app `YesDawSelfCheck`**, mirroring the existing `YesDawSoak` /
`YesDawPluginHost` console targets. This is a design fork, so I'm flagging it (per implementer-brief
hard-stop #9) rather than burying it.

**Why the console app (my recommendation):**
- A Windows **GUI-subsystem** exe cannot reliably write to a parent console — `YesDaw.exe --selfcheck`
  would run, but its `PASS`/`FAIL` line may go nowhere on Windows (the exact platform the alpha
  targets). A console app prints clean stdout on all three OSes.
- It matches how this repo **already** does headless tools (`YesDawSoak` is a console app "links only
  juce_audio_devices, no GUI"). Consistent, low-surprise.
- It keeps the GUI exe free of a headless code path.

**Cost / how to reverse:** if you want the literal `YesDaw --selfcheck` CLI too, it's a thin
passthrough later (GUI `initialise()` detects the flag and `exec`s / calls into the same
`YesDawSelfCheck` logic, or we add `AttachConsole` on Windows). Nothing here locks that out. If you'd
rather I fold it into the GUI exe instead, say so and I'll move it — it's on a branch, reversible.

## Verified surfaces (read from the tree, not guessed)

The self-check reuses the H7/H8 offline path exactly as `tests/bundle_render_tests.cpp` does:

| Step | API (verified) |
|---|---|
| open bundle | `yesdaw::persistence::ProjectBundleDb::openExistingBundle(path, db)` → `BundleResult::ok()` |
| load project | `db.readProjectSnapshot(project)` → `BundleResult::ok()` |
| structural validate | `project.hasValidEntityIds()`, `project.hasValidAssetClipIndirection()` |
| (slice 2) render | `yesdaw::engine::renderOfflineProject(project, decodedAssets, opts)` → `OfflineRenderResult` |
| (slice 2) decode assets | per-asset: `bundlePath / detail::assetRelativePathForHash(asset.contentHash)` → WAV read → `DecodedAssetAudio{ assetId, sampleRate, frames, channels, interleavedSamples }` |
| (slice 3) export | `yesdaw::io::writeFloat32WavFile(path, ...)` → `WavResult::ok()` |

## Slice plan (small green slices, per house rules)

- **Slice 1 — load + validate (DRAFTED on `vera/h17`).** `--selfcheck <bundle>` opens the bundle,
  reads the snapshot, runs the two structural validators, prints `SELFCHECK PASS/FAIL: <reason>`,
  exits 0/1. Plus `--version` (git-describe stamp, kills the `0.0.0` for the CLI). Depends only on
  persistence + engine headers — no decode/format/DSP — so it's the lowest-risk first slice and its
  **negative control** (the plan's "corrupted-fixture copy is rejected by validators") is satisfied:
  a corrupt `project.db` fails `readProjectSnapshot`, a malformed project fails the validators.
- **Slice 2 — render N blocks.** Decode each asset into `DecodedAssetAudio`, call
  `renderOfflineProject`, assert `status == Ok`, finite, `frames > 0`. (Needs the asset-decode surface
  wired; keep decoded-sample vectors alive for the `std::span` in `DecodedAssetAudio`.)
- **Slice 3 — export to temp + validate.** Write the rendered buffer via `writeFloat32WavFile` to a
  temp path, re-read it, assert bit-exact round-trip (H7 gate pattern), then delete. This is what the
  CI packaging job (CP3) runs against the *packaged* exe from a clean temp dir.

## Version stamping (kills `project(... VERSION 0.0.0)` for the CLI)

CMake `execute_process(git describe --tags --always --dirty)` → `YESDAW_GIT_VERSION`, passed to the
self-check target as `YESDAW_VERSION_STRING`. Falls back to `PROJECT_VERSION` when git is absent
(e.g. an exported tarball). The GUI app's `getApplicationVersion()` still returns `0.0.0`; stamping
*that* (and the JUCE bundle version) is a separate follow-up — flagged, not silently changed, because
it touches the shipped app identity.

## Build-verify checklist for Dan (when you have keyboard time)

1. `cmake --build --preset ci --target YesDawSelfCheck`
2. `./build-ci/YesDawSelfCheck_artefacts/YesDawSelfCheck --version` → prints a git-describe string.
3. `... --selfcheck tests/fixtures/h15_cp1_automation_schema_v8.yesdaw` → `SELFCHECK PASS: ...`.
4. `... --selfcheck <a deliberately-truncated copy>` → `SELFCHECK FAIL: ...`, exit 1 (negative control).

If it compiles + those four behave, slice 1 is green and I'll do slices 2–3.
