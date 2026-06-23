<!-- Mechanical-first verification + CI plan (4-agent research synthesis, 2026-06-23).
     The agent's reference for the 'stand up CI' task. Build artifacts to create are listed at the end. -->

# YES DAW — Mechanical-First Operating Model

**The one-line shift:** Today the gate is *Dan listens and watches on real hardware* (STATUS.md line 29, CLAUDE.md lines 31–33, 57). That doesn't scale and it blocks the H1 unattended loop. This plan replaces every "human confirms" with a **machine that renders to numbers and exits 0/1**, so the only thing Dan ever does is read a green or red check.

---

## The model — how verification works now

**The gate is GitHub Actions, not Dan.** A commit is "good" iff CI is green. Dan's entire job becomes: *push, glance at the check mark, merge if green.* He never reads a diff, never listens, never watches — the machine does all three for him.

**Who/what runs each gate:**

| Layer | What it proves | Where it runs | What Dan does |
|---|---|---|---|
| **Build matrix** (Win/mac/Linux) | It compiles everywhere | Cloud CI, every push | Nothing — red = broken |
| **Catch2 unit tests** via `ctest` | DSP/data/file logic is correct; the sine *is* 440 Hz (FFT-asserted) | Cloud CI | Nothing |
| **Golden render-diff** | Audio output didn't change unintentionally | Cloud CI (offline render, no sound card) | Bless once per intended change (one command) |
| **RTSan** (`-fsanitize=realtime`) | Audio thread never allocates/locks (CLAUDE.md hard rule, line 43) | Cloud CI, Clang/Linux job | Nothing |
| **clang-format / `/WX`** | Code is formatted, warning-clean | Cloud CI | Nothing |
| **Real-machine soak** | Sound *actually leaves the speaker*; zero dropouts over 10 min | One self-hosted runner on a real machine, scheduled | Nothing — script exits 0/1 |

**The honest boundary:** everything that proves "it works" is mechanical and cloud-CI-able. The *two* things that need a real machine — "did audio physically come out of the device" and "no dropouts over a long real-time run" — are made mechanical by a **self-asserting soak script with loopback capture** (output wired back to an input; the script asserts captured RMS/FFT, no ears). The single irreducibly-human moment is **blessing a new golden once** when behavior *intentionally* changes — and even that is "approve this PR," not "listen to this."

**What Dan ever has to do, total:** `git push` (the agent does this), look at the check, and occasionally type `cmake --build build --target bless-goldens` when he deliberately changed how something sounds. That's it.

---

## Mechanical gates for H0 specifically

Restating each H0 exit criterion from STATUS.md (lines 20–29). Every "human confirms" is replaced with a concrete automated check. The build target stays the throwaway `YesDaw` GUI app; we add a **second console target `YesDawCheck`** (built from extracted spike logic) that CI can drive headlessly. The trick for H0: **pull the sine/WAV/timeline logic out of `getNextAudioBlock` and `paint` into plain functions** so a test can call them without an audio device or a window.

| H0 item (STATUS.md) | Old gate | **New mechanical gate** | CI-able? |
|---|---|---|---|
| Toolchain installs; `cmake -B build` fetches JUCE | "no error" (already mechanical) | Build job configures + builds → exit 0 | ✅ cloud |
| App builds, window opens | human sees window | `YesDaw` target links; under Xvfb, construct `MainComponent`, assert no crash/assert and `getBounds()==600×300` | ✅ cloud (Xvfb) |
| **440 Hz tone plays** | **human listens** | Extract the sine generator to `fillSine(buffer, freq, sr)`. Test: render 1 s offline → FFT → **assert peak bin is 440 Hz ±1 Hz and amplitude ≈ 0.10**. Proves pitch+level with zero listening. | ✅ cloud |
| Gentle fade-in / lower level / start-stop | human "isn't jumpscared" | Render the fade ramp offline → assert sample[0]≈0, monotonic non-decreasing envelope over the fade window, steady-state == target amplitude | ✅ cloud |
| **Load + scrub one WAV** | human hears it | Commit a tiny fixture WAV. Test: load via `WavAudioFormat` → assert sample-rate/channel/length metadata, then **golden-diff the decoded buffer** (tolerance 1e-4). Scrub = render a sub-range, assert it equals the matching slice of the source. | ✅ cloud |
| **Timeline draws 100+ elements at 60fps** | human watches the feel | Two split checks: **(a) correctness** — `createComponentSnapshot` of the timeline with 100 elements → PNG golden-diff (catches layout regressions). **(b) cost** — benchmark the paint N× and `REQUIRE(msPerFrame < 16.6)` (loose factor on CI). True GPU smoothness → soak (below). | ⚠️ correctness ✅ cloud; smoothness → real machine |
| **One Node behind the trait stub** | implicit | Test: drive the stub node's `process(block)` through the offline loop with varied block sizes (1, 31, 512, 4096 — JUCE passes odd/zero blocks) → assert output matches a golden + no NaN/denormal. | ✅ cloud |
| **Exit: zero dropouts over 10 min + 60fps** | **human-confirmed** (STATUS.md line 29) | **Soak script on a real machine** (see below). Exits 0 iff `xruns==0 && dropped_frames==0 && loopback_peak_rms>0.01 && fft_peak≈440Hz`. | ❌ needs real device → automated as a script |

**The few that still need a real machine, and how they're scripted:**

Only the **10-minute soak** (the H0 exit gate) and **real-GPU smoothness** need hardware. Both collapse into one unattended script:

```bash
#!/usr/bin/env bash
# tools/soak.sh — runs on ONE real machine (self-hosted runner / Task Scheduler).
# Exit 0 = healthy, 1 = problem. No human listens or watches.
set -euo pipefail
DUR=${1:-600}                                   # H0 gate = 600s (10 min)
./build/YesDaw --headless-soak --seconds "$DUR" --stats-out stats.json
# The app, in soak mode: opens the REAL audio device, plays the 440 Hz signal,
# (optionally) drives the timeline, and logs counters to stats.json:
#   xruns (AudioIODevice::getXRunCount), max_block_ms, block_budget_ms,
#   dropped_frames, max_frame_ms, loopback_peak_rms, loopback_fft_peak_hz
python3 - <<'PY'
import json,sys
s=json.load(open("stats.json"))
ok = (s["xruns"]==0
      and s["dropped_frames"]==0
      and s["max_block_ms"] < s["block_budget_ms"]
      and s["loopback_peak_rms"] > 0.01            # sound ACTUALLY came out
      and abs(s["loopback_fft_peak_hz"] - 440) < 2 # and it was the right tone
      and s["max_frame_ms"] < 16.6)                # real-GPU 60fps held
sys.exit(0 if ok else 1)
PY
```

**Loopback capture** (OS loopback / a virtual cable / a physical jumper from out→in) is what converts "Dan listens" into an exit code: the app records its own output and asserts the captured RMS is non-zero and the FFT peak is 440 Hz. Wire `soak.sh` to a **self-hosted runner on Dan's real machine** triggered nightly + on release tags; it pings him only on red. This is the *only* piece that touches hardware, and Dan still never listens.

**One required code change to enable all of this:** add a `--headless-soak`/`--stats-out` path and factor the spike's DSP (`fillSine`) and the WAV/timeline logic into free functions. The throwaway nature of H0 spike code (Main.cpp lines 1–8) is preserved — we're just making it *callable* by a test.

---

## Starter GitHub Actions workflow

Commit this as `.github/workflows/ci.yml` now. It does what works in CI *today* (build + the gates that don't need engine code yet). RTSan and golden tiers get added as the test target lands; the structure already has slots for them.

```yaml
name: ci
on: [push, pull_request]
concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true

jobs:
  build-and-test:
    strategy:
      fail-fast: false          # one OS failing must not hide the others (agent triages, not a human)
      matrix:
        include:
          - { name: Windows, os: windows-latest }
          - { name: Linux,   os: ubuntu-22.04 }
          - { name: macOS,   os: macos-14 }
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4

      - name: Linux deps (full JUCE list + xvfb)
        if: runner.os == 'Linux'
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            libasound2-dev libjack-jackd2-dev ladspa-sdk libcurl4-openssl-dev \
            libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev \
            libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev \
            libwebkit2gtk-4.1-dev libglu1-mesa-dev mesa-common-dev xvfb ninja-build

      - uses: mozilla-actions/sccache-action@v0.0.10
      - uses: lukka/get-cmake@latest        # consistent CMake + Ninja on all 3 runners

      - name: Cache JUCE (FetchContent)
        uses: actions/cache@v4
        with:
          path: build/_deps                 # avoids re-cloning JUCE every run
          key: juce-${{ runner.os }}-8.0.4   # keyed to the pinned tag in CMakeLists.txt

      - name: Configure (Release — Debug hangs on JUCE asserts in CI)
        run: cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
             -DCMAKE_C_COMPILER_LAUNCHER=sccache
             -DCMAKE_CXX_COMPILER_LAUNCHER=sccache
        env: { SCCACHE_GHA_ENABLED: "true" }

      - name: Build
        run: cmake --build build --config Release

      - name: Test (headless; xvfb only on Linux)
        run: |
          if [ "$RUNNER_OS" = "Linux" ]; then xvfb-run -a ctest --test-dir build --output-on-failure
          else ctest --test-dir build --output-on-failure; fi
        shell: bash

  lint:
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v4
      - uses: jidicula/clang-format-action@v4.13.0
        with: { clang-format-version: '18', check-path: 'src' }
```

**Add as soon as `YesDawCheck` (Catch2 target) exists** — a Linux-only RTSan job (the highest-value gate for CLAUDE.md's "audio thread never allocates" rule):

```yaml
  rtsan:
    runs-on: ubuntu-24.04                   # needs Clang 20+
    env:
      RTSAN_OPTIONS: "halt_on_error=1:suppressions=${{ github.workspace }}/rtsan_suppressions.txt"
    steps:
      - uses: actions/checkout@v4
      - run: sudo apt-get update && sudo apt-get install -y clang-20 ninja-build
             libasound2-dev libfreetype-dev libx11-dev libwebkit2gtk-4.1-dev libglu1-mesa-dev
      - run: cmake -B build-rt -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
             -DCMAKE_C_COMPILER=clang-20 -DCMAKE_CXX_COMPILER=clang++-20
             -DYESDAW_SANITIZER=realtime
      - run: cmake --build build-rt --target YesDawCheck
      - run: ./build-rt/YesDawCheck          # the test that drives processBlock; aborts on RT violation
```

With a `rtsan_suppressions.txt` that silences known JUCE-internal frames but keeps Dan's code strict:
```
call-stack-contains:juce::VST3*
call-stack-contains:juce::AudioProcessorPlayer*
```

**CMake additions** to support the above (append to the existing CMakeLists.txt):
```cmake
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)        # clang-tidy/format consume this

option(YESDAW_SANITIZER "asan|undefined|thread|realtime" "")
function(yesdaw_harden tgt)
  if (MSVC)
    target_compile_options(${tgt} PRIVATE /W4 /WX)   # warnings-as-errors on OUR code only, never JUCE
  else()
    target_compile_options(${tgt} PRIVATE -Wall -Wextra -Werror)
  endif()
  if (YESDAW_SANITIZER)
    target_compile_options(${tgt} PRIVATE -fsanitize=${YESDAW_SANITIZER} -fno-omit-frame-pointer)
    target_link_options   (${tgt} PRIVATE -fsanitize=${YESDAW_SANITIZER})
  endif()
endfunction()
# yesdaw_harden(YesDaw)            # enable once spike warnings are clean
# add Catch2 via FetchContent + a YesDawCheck console target here, then: enable_testing()
```

---

## One-command bootstrap

**Goal:** any of Dan's 3 machines goes from bare to building with one command and **zero hardcoded paths**. CI is the real gate, so local toolchain is for *iteration speed only* — optional, not mandatory.

`bootstrap/windows.ps1` (idempotent — the winget exit-code trap is the load-bearing part):
```powershell
# Run from anywhere in the repo:  powershell -ExecutionPolicy Bypass -File bootstrap/windows.ps1
$ErrorActionPreference = 'Stop'
function Ensure-Pkg($id, $override) {
  $a = @('install','--id',$id,'-e','--accept-package-agreements','--accept-source-agreements')
  if ($override) { $a += @('--override', $override) }
  winget @a
  # winget returns -1978335189 ("No applicable update found") when a package is ALREADY present.
  # Without this trap the script fails on every machine after the first.
  if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne -1978335189) { throw "winget failed for $id ($LASTEXITCODE)" }
}
Ensure-Pkg 'Microsoft.VisualStudio.2022.BuildTools' `
  '--passive --wait --includeRecommended --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.26100'
Ensure-Pkg 'Kitware.CMake'        # top-level, not the stale CMake buried in VS — matches CI
Ensure-Pkg 'Ninja-build.Ninja'
Ensure-Pkg 'Git.Git'

# Derive the repo root from the script's own location — never an absolute user path.
$repo = Resolve-Path (Join-Path $PSScriptRoot '..')
cmake -S "$repo" -B "$repo/build" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$repo/build" --config Release
ctest --test-dir "$repo/build" --output-on-failure
```

**No-hardcoded-paths rules (the 3-machine contract):**
1. **Repo root is always derived, never typed.** Scripts: `$PSScriptRoot/..`. Agent in bash: `ROOT="$(git rev-parse --show-toplevel)"; cd "$ROOT"`. Everything downstream is repo-relative (`$ROOT/tests/golden/...`).
2. **`--wait --passive` on the VS install** or winget returns before VS finishes and the next `cmake` fails confusingly.
3. **CMake + Ninja are top-level winget packages**, never the ones bundled inside VS Build Tools (those are old and on a deep path → "works in CI, not locally" drift).
4. **JUCE pinned by tag** (already `8.0.4` in CMakeLists.txt line 12) — never float `develop`; a floating dep silently breaks reproducibility across machines, the worst failure mode for a non-coder.
5. **`CMakePresets.json` is the single entry point** so all 3 machines *and* CI run byte-identical, path-free invocations:
```jsonc
{ "version": 6,
  "configurePresets": [{ "name":"ci","generator":"Ninja","binaryDir":"${sourceDir}/build",
    "cacheVariables": { "CMAKE_BUILD_TYPE":"Release" } }],
  "buildPresets":     [{ "name":"ci","configurePreset":"ci" }],
  "testPresets":      [{ "name":"ci","configurePreset":"ci",
    "output": { "outputOnFailure": true } }] }
```
Then everywhere — bootstrap, CI, every machine — it's `cmake --preset ci && cmake --build --preset ci && ctest --preset ci`.

---

## Commit / review model

The constraint inverts the usual rules: **human-readable diffs stop mattering** (Dan can't read them), and **bisectability + always-green** become everything, because CI — not a person — is the reviewer. This formalizes CLAUDE.md's existing "commit in small chunks" (lines 23, 31) into a *mechanical* definition.

- **Trunk-based, short-lived branches; merge only on green.** Protect `main` with the CI matrix as a **required status check**. The gate *is* the reviewer; nobody approves by reading code.
- **Every commit on `main` must be independently green.** Not "human-small" (CLAUDE.md already says commits needn't be small) but **"green-small"**: the smallest change that still builds and passes `ctest`. This is the precondition for `git bisect run ctest --preset ci` to mechanically find any regression — the one triage tool that works without a human reading code.
- **Don't bundle a breaking refactor with the feature that needs it** in one trunk commit — that defeats bisect and rollback. Separate them, each green.
- **Squash-merge feature branches** so trunk history = one green commit per logical change (ideal bisect granularity); messy intermediate commits stay on the branch where bisect won't see them.
- **Goldens, ADRs, and `[[clang::nonblocking]]` annotations are protected** — the agent never edits them silently to make a build pass (already a CLAUDE.md hard-stop, line 56). A changed golden requires an explicit `bless-goldens` commit Dan can see in the PR title.
- **Standing instruction for the agent:** *never push a knowingly-red commit to trunk; a commit is "done" only when `cmake --build --preset ci && ctest --preset ci` passes locally or is expected to pass CI.*

This is exactly what unblocks the H1 unattended loop: CLAUDE.md line 40 already gates the loop on "CI gates exist." This plan builds those gates, so the loop can commit autonomously the moment H0's test target lands.

---

## Doc/process changes

Concrete edits to install this model:

**`STATUS.md`**
- Line 29 — replace the exit criterion. From `zero dropouts over a 10 min run + timeline holds 60fps → H0 done (human-confirmed)` to: `tools/soak.sh exits 0 (xruns==0, dropped_frames==0, loopback FFT==440Hz, max_frame_ms<16.6) on a real machine → H0 done.`
- Add a **"Verification = CI"** line to the cross-machine rule block (top): "A change is done when CI is green, not when Dan listens. The only human step is blessing a golden on intended audio changes."
- In the H0 checklist, append to each audio/visual item its mechanical check (the FFT-440, golden-diff, snapshot-diff, soak gates from the table above) so the worklog reflects the new gates.

**`CLAUDE.md`**
- Lines 31–33 ("Report plainly… verify (in H0: build + listen/watch on real hardware)") — change to: "what the CI gate checks, and on red, the failing job." Remove "listen/watch" as the verification mechanism.
- Lines 39–40 / 56–57 — soften the "H0 is hands-on, no gates yet / GUI human-eyeballed" framing: the gates now exist *from H0* (build + Catch2 + golden + snapshot + soak). The unattended loop's precondition ("once CI gates exist") is met at H0, not H1.
- Add to **Hard rules** (after line 48): "Verification is mechanical. Every exit criterion is an automated check (CI exit 0/1 or `soak.sh`). No criterion may read 'human confirms/listens/watches.' The audio-thread no-alloc rule (line 43) is enforced by the RTSan CI job, not by inspection."
- Add a **CI / bootstrap** section: point to `.github/workflows/ci.yml`, `CMakePresets.json`, `bootstrap/windows.ps1`, and the `bless-goldens` command.

**The plan** (`docs/plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md`)
- Add a top-level **"Mechanical verification (the gate)"** section capturing: CI matrix, the four CI gate tiers (build, Catch2 signal-asserts, golden render-diff, RTSan), the real-machine soak with loopback, and the green-small/bisect commit rule. Reference it from every horizon's exit criterion so H1–H6 inherit "exit = a CI check goes green," never "human confirms."
- Note the new repo artifacts to create: `.github/workflows/ci.yml`, `CMakePresets.json`, `bootstrap/windows.ps1` (+ a `bootstrap/linux.sh` later), `rtsan_suppressions.txt`, `tools/soak.sh`, `tests/` (Catch2 `YesDawCheck` target), `tests/golden/` (committed `.wav`/`.png` references — `.gitattributes` already treats `.wav` as binary, line for `.png` should be added).

**`.gitattributes`** — add `*.png binary` so golden snapshot images don't get line-ending-mangled (the `.wav` rule already exists).

**New ADR** — `docs/adr/0005-mechanical-verification-and-ci-gates.md`: records the decision that CI is the source of truth, the four-tier gate model, the real-machine-soak boundary, and the green-small commit rule. This is an architecture decision (it shapes how every future horizon is gated) and CLAUDE.md line 14 requires decisions to land as ADRs before the code that depends on them.

---

**Relevant absolute paths** (all exist, all referenced above):
- `C:\Users\SM - Dan\Documents\GitHub\yes-daw\STATUS.md` (exit criterion line 29 to rewrite)
- `C:\Users\SM - Dan\Documents\GitHub\yes-daw\CLAUDE.md` (lines 31–33, 39–40, 56–57 to update)
- `C:\Users\SM - Dan\Documents\GitHub\yes-daw\CMakeLists.txt` (JUCE pinned 8.0.4 line 12; add sanitizer option + test target + `yesdaw_harden`)
- `C:\Users\SM - Dan\Documents\GitHub\yes-daw\src\Main.cpp` (extract `fillSine`/WAV/timeline logic into testable free functions; add `--headless-soak`)
- `C:\Users\SM - Dan\Documents\GitHub\yes-daw\.gitattributes` (`.wav` binary already set; add `*.png binary`)
- To create: `.github/workflows/ci.yml`, `CMakePresets.json`, `bootstrap/windows.ps1`, `rtsan_suppressions.txt`, `tools/soak.sh`, `tests/` + `tests/golden/`, `docs/adr/0005-mechanical-verification-and-ci-gates.md`