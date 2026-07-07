# H16 CP2 — Async waveform cache: implementation plan

> Parent: [`2026-07-03-h16-real-ui-plan.md`](2026-07-03-h16-real-ui-plan.md) (CP2 row).
> Decisions: ADR-0037 (mockup = structural spec), ADR-0002 (audio thread never allocates/locks/does
> I/O — **the peak build is control-side, never on the audio or UI-paint thread**). Guardrails:
> [`docs/fable5/implementer-brief.md`](../fable5/implementer-brief.md).
> Precondition: **CP1 (design tokens) is declared complete** — see the parent plan's "CP1 exit"
> note. Do not reopen CP1 token slices; broaden tokens only when a CP2 change introduces a *new*
> raw literal in real UI chrome.

## Why this checkpoint

`WaveformPeakCache.h` already builds a correct min/max/rms peak pyramid and persists it to
`peaks/<hash>.ypeaks` (build/write/read helpers all exist and are gated). **Nothing in `src/ui/`
consumes it** — clips still paint a hash fake (`drawClipWaveform`,
[`TimelineCanvas.h:123`](../../src/ui/TimelineCanvas.h)). CP2 builds the plumbing that gets real
peaks from Asset samples to the paint path **without ever building on the UI or audio thread**.
CP3 (next plan) swaps the fake draw for real columns; CP2 deliberately makes **no visual change** —
its proof is entirely mechanical (off-thread build + persistence + paint-never-builds).

## Goal

A control-side `WaveformPeakService` owns a single background worker thread. On import/open,
`UiAppModel` enqueues one build job per Asset (from already-decoded samples). The worker builds the
pyramid, writes `peaks/<hash>.ypeaks`, and publishes the ready cache as an immutable
`shared_ptr<const WaveformPeakCache>`. The paint path only ever *reads* the published pointer (or a
"not ready yet" miss) — it never decodes, never builds, never blocks on I/O. On open, an existing
`.ypeaks` file is reloaded instead of rebuilt.

## Mechanical exit criterion

`YesDawWaveformCacheCheck` (new) is green in CI and proves all of:

1. **Off-thread build.** A build requested through the service is executed on the worker thread, not
   the requesting thread (thread-id captured at build time ≠ caller's id).
2. **Paint never builds.** A thread-identity guard flags any peak build that runs on the registered
   UI/paint thread. The **named negative control** — a `forceSynchronousBuildOnCallerThread` shim —
   runs the same build inline on the caller and the guard flags it, failing the control case.
3. **Publish + read handoff.** After a build completes, `tryGetReady(hash)` returns a non-null
   `shared_ptr<const WaveformPeakCache>` equal (`operator==`) to the pyramid `buildWaveformPeakCache`
   produces for the same samples; before completion it returns null (placeholder path).
4. **Persist + reload.** The worker writes `peaks/<hash>.ypeaks`; a fresh service opening the same
   bundle reloads from disk (byte-identical cache, no rebuild — proven by a build-count of 0 on the
   reload path).

No new golden files. No engine/Project-model change. No tolerance decisions (this is exact-integer /
byte-identity, not DSP).

## Design (what Codex builds)

### New: `src/ui/WaveformPeakService.h` (control-side, header-only, matches repo style)

State:
- One `std::thread` worker; a `std::mutex` + `std::condition_variable` + `std::deque<Job>` queue; a
  `bool stop_` for clean shutdown in the destructor (join).
- Published results: `std::mutex resultsMutex_` guarding
  `std::unordered_map<AssetContentHash, std::shared_ptr<const WaveformPeakCache>>`. (Hash needs a
  `std::hash` specialization or a hex-string key — use the existing `peakHexBytes` helper as the map
  key to avoid touching `AssetContentHash`.)
- `std::thread::id workerThreadId_` captured as the worker's first action.
- `std::atomic<std::uint64_t> buildCount_` (test-observable; proves reload does not rebuild).

API:
- `void start(std::filesystem::path bundlePath)` — spawns the worker, records `bundlePath_`.
- `void requestBuild(const engine::Asset& asset, std::vector<float> channelMajorSamples)` — enqueues
  a job. **First tries an on-disk reload** (`readWaveformPeakCache`); on hit, publishes without a
  build (`buildCount_` unchanged). On miss, the worker calls `buildWaveformPeakCache` +
  `writeWaveformPeakCache`, then publishes and increments `buildCount_`.
- `std::shared_ptr<const WaveformPeakCache> tryGetReady(const engine::AssetContentHash&) const` —
  lock, copy the `shared_ptr`, return (null if absent). Cheap; safe from the paint thread.
- `void registerPaintThread(std::thread::id)` / a `[[nodiscard]] bool builtOnForbiddenThread()`
  observability hook the gate reads.
- Destructor joins the worker.

Thread-safety contract (write it as a comment block at the top, mirroring `WaveformPeakCache.h`'s
header note): peaks are derived, deletable, and Project truth never depends on them; the audio thread
never touches this class; the UI thread only calls `tryGetReady` (a mutex-guarded `shared_ptr` copy,
never a build); builds happen only on the worker.

### Integrate into `UiAppModel`

- Hold a `WaveformPeakService` member (or a `std::unique_ptr` to keep the header cheap).
- After a successful import (`addAudioAssetClipFromSource`) and after `openProjectBundle` /
  `loadProjectBundle`, enqueue a build for each Asset that has decoded samples available in
  `decodedAssets_` — deinterleave to channel-major (the cache wants channel-major; `UiDecodedAsset`
  holds interleaved — add a small pure `interleavedToChannelMajor` helper, unit-checked).
- Expose `const WaveformPeakService& waveformService() const`.
- Start the service against `bundlePath_` when a bundle is attached.

### Paint path (CP2 scope: wire the read, keep the fake as placeholder)

- `TimelineCanvas` gains access to a `tryGetReady`-style lookup (pass a
  `std::function<std::shared_ptr<const WaveformPeakCache>(clip→assetHash)>` or a lightweight
  view struct from `MainComponent` — do **not** give the canvas the whole `UiAppModel`).
- In CP2, when a cache **is** ready, still draw the existing fake (real columns are CP3) but flip a
  cheap observable flag / take the "ready" branch; when **not** ready, draw the placeholder. The
  point of CP2 is the *data path and the gate*, not pixels. CP3 replaces the ready branch body with
  `computeWaveformColumns`.
- **Assert-in-paint:** the paint path must never call `requestBuild` or `buildWaveformPeakCache`.
  The gate's negative control enforces this.

## Checkpoints (each its own independently-green commit, straight to `main`)

- **CP2a — Service skeleton + off-thread build gate.** `WaveformPeakService.h` with worker, queue,
  `requestBuild`/`tryGetReady`, thread-id capture. `YesDawWaveformCacheCheck` proving criteria 1 & 3
  plus the negative control (criterion 2). Register the gate in `CMakeLists.txt` mirroring
  `YesDawThemeAuditCheck` (lines 247–252, 687). Negative control lands in the **same commit**.
- **CP2b — Disk reload path.** `requestBuild` reloads an existing `.ypeaks` without rebuilding;
  extend the gate with criterion 4 (build-count 0 on reload; byte-identity vs a pre-written fixture
  cache). Its negative control: delete the file → build-count becomes 1.
- **CP2c — `interleavedToChannelMajor` helper + `UiAppModel` wiring.** Pure helper, unit-checked
  (interleaved↔channel-major round trip). `UiAppModel` enqueues builds on import/open and exposes
  `waveformService()`. Extend `YesDawUiActionCheck` (or the cache gate) to assert that after an
  import the service reaches ready for the imported Asset's hash.
- **CP2d — Paint reads published cache (no visual change).** `TimelineCanvas` takes the ready/not-
  ready branch via the lookup; paint-never-builds assertion wired into the gate. Full
  `ctest --preset ci` green.

## Gates that must BITE

| Gate | Proves | Named negative control (same commit) |
|---|---|---|
| `YesDawWaveformCacheCheck` — off-thread | Build runs on worker, not caller | `forceSynchronousBuildOnCallerThread` runs inline → thread-id guard flags it → case fails |
| `YesDawWaveformCacheCheck` — paint-never-builds | No build on the registered paint thread | Register caller as paint thread + force sync build → `builtOnForbiddenThread()` true → fails |
| `YesDawWaveformCacheCheck` — reload | Existing `.ypeaks` reloads without rebuild | Remove the file before request → `buildCount_` goes 0→1 → the "no rebuild" assert fails |
| `interleavedToChannelMajor` unit | Deinterleave is exact | Swap a channel index → round-trip inequality fails |

## Not yet (guardrails — do not do these in CP2)

- Real column rendering / replacing the fake `drawClipWaveform` body — that is **CP3**.
- Any change to `WaveformPeakCache.h`'s format, tiering, or the `.ypeaks` on-disk layout (it is
  gated and feeds saved bundles — additive only, and not needed here).
- Any engine/Project-model or audio-thread change. The service is control-side UI infrastructure.
- Reopening CP1 token slices.
- Writing to `docs/reality-lane.md` (owner-only).

## Sequencing / stop points

CP2 before CP3 (CP3 depends on the ready cache existing). Stop and flag in `STATUS.md` rather than
guessing if: (a) `AssetContentHash` cannot be used as a map key without editing `engine/` (prefer the
hex-string key to avoid it); (b) decoded samples are not actually available at import time for a given
path (record which path); (c) the worker-join-on-destruct interacts badly with the existing headless
test lifecycle. One baton successor only after your own CI is green.
