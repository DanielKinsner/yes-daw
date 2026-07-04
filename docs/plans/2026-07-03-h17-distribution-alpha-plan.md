# H17 — Distribution + Alpha: focused plan

> Decisions: [ADR-0037](../adr/0037-alpha-target-and-h14-h19-recarve.md) (portable unsigned zip
> for alpha; signing/installer/auto-update/telemetry are **beta**, adapting the yes-master
> playbook — that app is Tauri/Rust, this one C++/JUCE, so adapt, don't copy). Precondition:
> H16 closed. Guardrails: [`docs/fable5/implementer-brief.md`](../fable5/implementer-brief.md).

**Goal.** A packaged, portable, versioned YES DAW build that Dan can unzip and run — and the
alpha gate itself: one real song recorded, edited, mixed, and exported on that packaged build.

**Mechanical exit criterion.** The packaging job produces a versioned zip whose contained exe
passes a headless self-check on a clean machine profile (no repo, no build tree); the demo
fixture round-trips on the packaged build; the alpha checklist's mechanical sub-asserts all pass;
reality-lane PASSes recorded for playback and recording on the packaged build; the one human feel
session is signed off in the alpha log.

**Build facts (verified 2026-07-03):** the sole CMake preset `ci` already builds **Release**
(Ninja, `build-ci`, GUI app included); `tools/launch-h11.ps1` starts
`build-ci/YesDaw_artefacts/Release/YesDaw.exe`; there are **no packaging scripts**. So H17 is
about *packaging and proving the artifact*, not about creating a first optimized build.

## Gates that must BITE

| Gate | Proves | Named negative control |
|---|---|---|
| Packaged self-check | The zipped exe runs on a machine profile without repo/build-tree/dev deps: loads the bundled demo Project headlessly, renders N blocks, exports, exits 0 | Delete a staged runtime file from the zip in a test case → self-check fails |
| Version stamping | `--version` output == git describe == zip name | Mismatched stamp fixture fails |
| Demo fixture round-trip | The committed demo song bundle opens, renders, and re-saves cleanly on the packaged build | A corrupted-fixture copy is rejected by validators |
| Alpha export asserts | The alpha song's export exists, re-imports bit-exact (H7 gate pattern), integrated loudness within −30..−6 LUFS, Project reopens with zero validator errors | Each assert has a synthetic failing input in the gate's unit tests |
| Reality lane on the artifact | Playback + recording smokes PASS against the **packaged** exe, not the dev build | (Owner-machine; logged per reality-lane rules) |

## Checkpoints

**CP1 — Self-check mode.** `YesDaw --selfcheck <bundle>`: headless load → render N blocks →
export to temp → validate → exit 0/1 with a printed PASS/FAIL line (reuses the H7/H8 surfaces;
no audio device required). Gated in CI on all three desktop jobs.

**CP2 — Demo fixture.** A small committed demo song bundle (`tests/fixtures/demo-song.yesdaw/`)
exercising every alpha feature: imported + recorded audio, MIDI clip → instrument, FX on tracks
and a bus, automation lanes, markers, loop region. Used by `--selfcheck`, H16 screenshots, and
the alpha session as the warm-up project.

**CP3 — Packaging.** `tools/package.ps1` (+ `.sh`): build via the `ci` preset → stage exe +
LICENSE + README-alpha.md + `version.txt` (git describe) → zip
`YesDaw-<version>-win64-portable.zip`; a CI packaging step runs it and then runs the packaged
self-check from a clean temp dir (repo-independence proof). macOS zip is produced but its
notarization story is explicitly **beta**.

**CP4 — Crash-safety defaults.** Autosave scheduling ON by default in the shipped shell (H13
recovery prompt already exists); device-failure paths produce a dialog, never a crash — both
asserted through the existing UI input harness patterns.

**CP5 — The alpha gate.** `docs/alpha-gate.md`: the scripted checklist Dan runs on the packaged
build — record (audio + MIDI), edit, mix (FX + automation), export. A companion
`tools/alpha-verify.ps1` runs the mechanical sub-asserts against the produced bundle + WAV
(export exists/re-imports, loudness range, reopen validators, autosave present). The **one
sanctioned human feel session** happens here, against a written checklist; its outcome and the
sub-assert results are logged in `docs/alpha-gate.md` and the reality-lane table. Alpha closes
only on: sub-asserts green + smokes PASS + Dan's sign-off line committed.

## Beta parking lot (explicitly NOT H17)

Code signing + timestamping (adapt yes-master playbook), installer (MSIX/Inno), macOS
notarization, auto-update, crash telemetry, licensing/activation, file-association registration,
localization, marketing site. Each needs its own ADR when beta opens.
