# 0059. Decoded Assets are shared, immutable buffers: one per Asset, held by reference everywhere

- **Status:** Accepted (agent, 2026-10-07, under ADR-0049's implementation-ADR clause, after a separate agent
  critic pass whose findings are folded in: one pointer type for same-rate and cross-rate sources; the creation
  sites and their checks named; the field, accessor and test factory named; the gate's accessor named; the waveform
  service and device hot-swap API changes stated; the reclaim boundary and the thread rules made exact. Committed
  alone, before any G5.4 code.)
- **Date:** 2026-10-07
- **Deciders:** build agent (proposer), separate agent critic
- **Related:** the plan's G5.4 ("Decoded-asset sharing. One decoded buffer per asset shared by reference (R30), while
  preserving the immutable ownership/lifetime contract established for export. Gate: fixture memory assertion and
  edit/export/project-close lifetime tests; no extra decode per clip referencing an asset.") in
  [`docs/plans/2026-09-01-real-daw-ground-up-plan.md`](../plans/2026-09-01-real-daw-ground-up-plan.md); backlog R30 in
  `docs/goals/2026-08-25-reality-run-backlog.md`; ADR-0058 (the export job owns `shared_ptr<const AssetSamples>` per
  Asset and expects G5.4 to make the model's storage those buffers); ADR-0055 (a rate-matched view is derived data
  per Asset and rate, freed with its Asset); ADR-0010 (Assets are immutable source audio; clips reference them);
  ADR-0046 §6 and G0.5 (what the audio thread may hold is never freed under it; clip schedules keep their owners alive).

## Context

An Asset's decoded samples are immutable once decoded, yet the model treats them as values. `UiDecodedAsset` holds a
`std::vector<float>` by value, so:

- a recording commit and a test take copy every Asset's samples (`nextDecoded = decodedAssets_`) and move the copy
  back — every buffer changes address; an import, a Sampler pad and a drop keep a full deep copy for rollback;
- the engine cannot use the model's vectors directly, so `makeDecodedOwners` copies each same-rate Asset into an
  `AssetSamples` owner, cached by the vector's address — every address change above recopies everything, and the
  cache is never released when a project closes;
- a running export copies each Asset into its snapshot and the worker's render copies it again;
- the waveform service converts every Asset to a channel-major copy on every request, before checking its disk
  cache.

An idle same-rate Asset therefore sits in memory at least twice; a five-minute stereo stem is ~115 MB, so ten stems
are gigabytes and every recording pass copies them all on the message thread. What is hard to reverse: the
ownership type every holder (model, engine, export job, peak builder) shares, and the rule that a decoded buffer is
never written after it is published.

## Options considered

1. **`shared_ptr<const engine::AssetSamples>` as the one storage (chosen).** The decoded samples move once into an
   immutable `AssetSamples` (the type the engine and the export job already take); the model, the engine's clip
   schedules and Sampler pads, export jobs and peak builds all hold that pointer.
   - Pros: no copies anywhere a buffer is only read; lifetime is reference counting, already the engine's law
     (`keepAlive`); the export job's contract is met as written.
   - Cons: every reader of `UiDecodedAsset::interleavedSamples` changes to the shared buffer.
2. **`shared_ptr<const UiDecodedAsset>` elements (R30's cheap fix).** Copying the list copies pointers.
   - Pros: smallest diff for the model's own copies.
   - Cons: the engine and the export job still need `AssetSamples`, so `makeDecodedOwners` keeps copying; two
     ownership types for one buffer.
3. **Stream from disk instead of holding decodes.** Out of scope (parked): a different memory model with its own I/O
   rules.

## Decision

- **One immutable buffer per decoded Asset.** `UiDecodedAsset` keeps its metadata (`assetId`, `sampleRate`, `frames`,
  `channels`) and its samples become `std::shared_ptr<const engine::AssetSamples> samples`, with a
  `std::span<const float> interleaved() const` accessor for readers; the `interleavedSamples` vector field goes. It is
  made by one factory, `UiDecodedAsset::fromInterleaved (assetId, sampleRate, channels, std::vector<float>&&)`, which
  moves the vector into a new `AssetSamples` (no copy) and derives `frames`; tests that build a `UiDecodedAsset` from a
  vector migrate to it. A published buffer is never written again. Copying a `UiDecodedAsset` (a rollback snapshot, a
  list copy) copies the pointer.
- **Validity is checked once, at creation.** Each creation site runs the shape check (`decodedAudioIsValid`) and a
  finiteness pass before publishing: the decode of a file (`uiDecodedAssetFrom`, import and reopen), the real
  recording commit, and the test take (`makeDeterministicRecordedAudio` / `commitRecordedAudioTake`). A non-finite
  decode is refused there with its reason. `makeDecodedOwners`' finiteness scan goes; readers trust a published
  buffer.
- **The recording capture.** A sealed take's samples (`shared_ptr<vector<float>>` from the capture) are moved into the
  Asset's buffer when the model holds the only reference to them, else copied once — that copy is the take's one
  buffer, never repeated.
- **The engine reads the same buffer.** The model's engine owners are those buffers (the address-keyed owner cache
  goes away); a same-rate clip's schedule and a Sampler pad hold the Asset's buffer, so any number of clips on one
  Asset share one buffer and a build copies nothing. A cross-rate Asset's rate-matched view stays derived data per
  (Asset, rate) (ADR-0055), the only second buffer an Asset can have.
- **An export job holds the same buffers.** `ExportAssetAudio` carries one `shared_ptr<const AssetSamples> buffer` (the
  Asset's) and its `sourceRateHz`; its separate `source` vector goes. The snapshot references each Asset's buffer (no
  copy); for a same-rate Asset the render takes it as an owner (`OfflineRenderOptions::assetOwners`, so the worker
  copies nothing); for a cross-rate Asset the worker builds the offline view from it. ADR-0058's contract is
  unchanged — the job owns what it reads — but owning is now a reference, not a copy.
- **A peak build holds the buffer.** `WaveformPeakService::requestBuild` takes the Asset's `shared_ptr<const
  AssetSamples>` instead of a channel-major vector; `interleavedToChannelMajor` moves into the worker and runs only
  when the disk cache misses.
- **A device hot-swap holds the buffers.** `DeviceHotSwapCoordinator` receives the Assets' owners
  (`std::vector<AssetOwnership>`) beside its spans, so its rebuilt graph references the same buffers.
- **Lifetime is reference counting, released on the control thread.** A buffer lives while any holder — the model's
  decode list, an engine's clip schedules and Sampler pads (`ClipSchedule::keepAlive`), an export job, a peak build,
  a hot-swap build — holds it. The audio thread never holds a `shared_ptr` to a buffer: it reads through raw pointers
  into published schedules, so it can never drop the last reference. A retired engine's schedules keep their
  references until the control thread's janitor (`reclaimRetiredAudioObjects` → `PlaybackEngine::reclaim`) frees
  them once the device thread is provably past them (ADR-0046 §6) — that janitor must stay on the control thread.
  Closing or replacing a project drops the model's own references at once; the audio is freed when the last retired
  engine is reclaimed and no export job, peak build or hot-swap of it remains.
- **Thread rule.** Each thread holds its own `shared_ptr` copies (taken on the control thread before a worker starts);
  no `shared_ptr` object is read and written by two threads at once.
- **The audition voice is not an Asset** (ADR-0056): it keeps its own copy of a browser file.

### Gates (`[asset-sharing]`)

- An Asset's buffer is the same object (pointer identity) before and after: an import of another file, a recording
  commit, a test take, an undo, a refused drop's rollback, an engine rebuild and a save.
- Ten clips on one Asset: the engine's owners and a Sampler pad on that Asset hold the model's buffer (identity), and
  a build makes no copy. The fixture memory assertion uses `UiAppModel::distinctDecodedBuffersForTest()` — the
  distinct buffer addresses held by the decode list, the rate-matched views and the owners handed to the engine — which
  equals the number of decoded Assets plus their cross-rate views.
- An export job's snapshot holds the model's buffers (identity); closing the project mid-job leaves the job's output
  equal to the export of the project as it was (the job's references keep the audio alive).
- Closing a project releases its audio: a weak reference to an Asset's buffer expires once the model has opened
  another project and reclaimed its retired engines, and no export job or peak build of it remains.
- Behaviour is unchanged: every existing playback, render, export and reopen gate passes as it is.

## Consequences

- **Positive:** memory per Asset is its decode (plus a cross-rate view when needed); recording, importing and
  undoing stop copying audio on the message thread; exports start without copying; a closed project's audio is
  freed once its retired engines are reclaimed (no longer held forever by an owner cache).
- **Negative / accepted costs:** every reader of `interleavedSamples` moves to the shared buffer (a mechanical sweep);
  a decoded buffer can never be edited in place (none is today — edits are clips over immutable Assets, ADR-0010).
- **Follow-ups:** `CONTEXT.md` gains **Decoded buffer** (an Asset's one immutable, shared decode). R30 is closed by
  this work. Streaming from disk stays parked.
