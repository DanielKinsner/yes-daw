# 0080. Background media preparation with owned jobs and owner-thread adoption

- **Status:** Accepted (in-scope implementation decision under Dan's 2026-11-03 mandate; independent critic approved ownership and the first stretch slice, with the request/durability clarifications below applied)
- **Date:** 2026-11-03
- **Deciders:** build agent; independent engine/saved-data critic. Implementation and headless integration proof remain required.
- **Related:** [ADR-0002](0002-realtime-engine-foundations.md),
  [ADR-0030](0030-time-stretch-node.md), [ADR-0049](0049-autonomous-delivery-and-usable-song-milestone.md),
  [ADR-0054](0054-audio-import-formats.md), [ADR-0055](0055-cross-rate-audio.md),
  [ADR-0058](0058-export-v2.md), [ADR-0059](0059-shared-decoded-assets.md),
  [ADR-0078](0078-ship-first-cadence.md),
  [improvement review](../ideation/2026-11-03-yes-daw-improvements-ideation.html),
  [headless implementation plan](../plans/2026-11-03-editing-improvements-headless-plan.md).

## Context

Audio import and time-stretch preparation currently run synchronously on the Control thread.
`MainComponentBrowser.cpp::importAudioFromPath` decodes before importing; the timeline drop and
Sampler-load paths also decode directly. `ProjectBundleDb::importAssetBytes` then hashes the source,
copies and verifies its bytes, flushes and renames the stored file, and writes its Asset row.
Cross-rate sources may also need a complete live rate-matched view. Moving only decode to a worker
would leave other large operations blocking the same thread.

`UiAppModel::refreshStretchOwners` prepares a cache miss synchronously. Signalsmith's current
`prepareTimeStretch` wrapper processes the complete source window in one call. Its preparation
already belongs off the Audio thread under ADR-0030; it need not occupy the thread handling edits.

ExportJob supplies a local precedent: a worker owns its snapshot and buffers, publishes progress and
completion, and never reaches into the model or SQLite. The owner polls outcomes and retires work
when its originating Project is replaced. The new work needs the same ownership discipline, plus
an explicit rule for applying a prepared edit after other editing has continued.

The architectural choice is the ownership and adoption boundary between worker and model, including
staged source files and cancellation. This ADR preserves the Project format, immutable Asset bytes,
existing DSP output, source/view frame laws and the Audio thread's hard rules. It adds no saved job,
pending-clip or cache record to a Project bundle.

## Options considered

1. **Owned, operation-specific preparation jobs (recommended).** A one-shot stretch job owns its
   input buffers; a one-shot import job owns staged source bytes and preparation results. The model
   alone validates and adopts a finished result.
   - Pros: explicit lifetimes and stale-result checks; ordinary edits continue; the existing export
     pattern is familiar; pure stretch preparation can be tested without JUCE or devices.
   - Cons: completion becomes a separate step; import needs a prepared-Asset adoption seam rather
     than a call back into the existing copy-and-hash import function.
2. **Move decode alone to a worker and reuse the current synchronous adoption path.**
   - Pros: smaller first import change; existing persistence logic is reused directly.
   - Cons: hashing, copying, resampling and stretch cache misses can still block; this is a partial
     improvement and cannot satisfy the full background-preparation goal.
3. **A shared task platform or worker access to the live model/database.**
   - Pros: a shared scheduler could support later operations; direct model access avoids a separate
     handoff API.
   - Cons: a platform adds scope before two concrete operations need it; sharing mutable Project,
     undo or SQLite state creates much broader synchronization and lifetime obligations.

## Decision

Select option 1: typed, one-shot jobs for the two operations, with adoption on the model's owning
Control thread. Reuse ExportJob's handoff pattern, not its export-specific result type, and do not
introduce a generic task platform. The contracts below apply to both operations.

### 1. Owned inputs; no worker access to live state

- A request owns every sample buffer it reads through `shared_ptr<const AssetSamples>` or an
  equivalent owning immutable reference. A borrowed span is used only while that owner remains
  alive. Do not copy complete audio merely to extend its lifetime.
- A worker holds no model, shell, live Project, undo-stack or `ProjectBundleDb` pointer or reference.
  It does not issue UI callbacks, change the status line, or write SQLite. It writes only its owned
  result, progress/cancellation state and, for import, its own staged files.
- The worker publishes its completed result before publishing terminal state with release/acquire
  synchronization. The owner does not inspect mutable result data before that handoff.
- The Audio thread never starts, polls, cancels, destroys or joins these jobs. Existing processing
  continues to read immutable published buffers through the engine's established ownership law.

### 2. Intent, Project generation and stale results

- Capture request ID, `attachGeneration`, Project ID, stable target Track/Clip IDs, and the intended
  position or edit parameters when accepting a request. Playback movement or a later selection does
  not change where a completed import lands or which Clip receives a stretch.
- Each pending target also has a request version. Replacing or cancelling the request invalidates
  that version immediately. A successful worker result is not permission to adopt it.
- Before adoption, the owner checks the generation, Project identity, current target existence,
  request version and operation-specific source/dependency key. Project replacement, deletion or
  relevant undo/redo invalidates the pending edit. A missing target is a named refusal; do not
  silently import into a fallback Track.
- Apply the result to the **current** Project and undo stack, preserving unrelated edits made while
  preparation ran. Never publish a whole Project snapshot captured when the job began.
- For a batch import, capture the target Track identities and intended append policy. Reordering a
  Track must not redirect an import by stale lane index. A vanished required target invalidates the
  batch's pending placement; it does not retarget the files implicitly.

### 3. Polling, cancellation and retirement

- The owner services active jobs from its regular tick, like `serviceExport`. State reads and
  cancellation requests do not wait for computation. Normal service ticks and Project replacement
  never join a running worker. Terminal jobs are joined/reclaimed only after completion is observed.
- User cancellation, a superseding request and Project replacement make the result ineligible for
  adoption immediately. Request worker cancellation and retain ownership in a retiring-job list
  until it finishes. A retired outcome cannot change the current Project, undo, selection or status.
- Bound execution to one worker per operation, counting a retiring worker until it is terminal.
  Stretch keeps at most one latest pending replacement request and coalesces gesture intermediates;
  it does not launch a thread per pointer event. Import accepts one batch at a time and names a busy
  refusal for another batch. Project replacement does not bypass these bounds.
- Workers check cancellation between bounded copy/decode/conversion stages wherever those APIs
  permit it. The current monolithic Signalsmith call may finish after cancellation; its result is
  discarded. Do not claim a cancellation-latency bound that the processing API cannot provide.
- Final teardown cancels and joins outstanding jobs off the Audio thread before their state is
  destroyed. Teardown may wait for an in-flight monolithic stage; this is a known limitation, not
  an instantaneous-shutdown guarantee. Do not detach a worker that can outlive its owned state.
- Report actual phases and available work counts. Use indeterminate Preparing for a monolithic
  stretch stage; do not manufacture a percentage or show success before adoption succeeds.

### 4. Stretch preparation and atomic adoption

- First implement one selected-Clip stretch request. Keep committed Clip geometry and audible
  playback unchanged while preparing; any pending outline/readout is transient, not Project truth.
  One target's latest request wins. Avoid an unbounded queue of obsolete gesture intermediates.
- The preparation/cache key includes Project generation, Clip/Asset identity, Asset content hash,
  immutable source-buffer identity, source/view sample rates, channel count, mapped source offset
  and length, and stretch factor. Retain the source owner so identity cannot be reused while the
  job exists. Geometry/time-base dependencies of an absolute-end gesture are checked as well.
- Source windows remain stored in Asset frames. Prepare against the same project-rate view and
  mapped window that the current engine path uses under ADR-0055. No second conversion or saved
  change to those units is introduced.
- Request construction and revalidation must also avoid expensive cache-miss work. Obtain an
  already-ready immutable rate-matched view through a lookup, or let the worker prepare a missing
  live-tier view from its owned decoded source. Do not call the synchronous cache-miss resampler
  while accepting the request, then describe only the subsequent stretch as background work.
- On a current, successful result, make the prepared buffer available to the engine build/live
  schedule path, apply the final stretch to the current Project as one durable undo step, and
  publish through the existing adoption law. Failure leaves the prior Project, undo and playback
  intact; any abandoned prepared buffer is only derived cache state.
- Interactive adoption must not hide a synchronous preparation fallback on a cache miss. A missing
  required prepared result leaves the edit pending or refuses it with a reason. Offline render and
  existing synchronous non-interactive preparation remain governed by their current contracts.
- Preserve existing Signalsmith output and checked-in fingerprints. Moving computation to a worker
  is not authority to change the DSP algorithm or bless new goldens.

### 5. Import staging and adoption

- After the stretch slice, migrate source staging, decoding and required live resampling into the
  import job. Prepare all required results before making the import a Project edit. Chooser,
  Browser and timeline-drop import surfaces use the same preparation/adoption contract; migrate
  other direct decoder surfaces explicitly rather than claiming they improved by association.
- Copy source bytes into a uniquely owned staging location on the destination filesystem, retaining
  the original format/filename metadata. Hash and decode the staged bytes so the immutable stored
  source, decoded audio and metadata describe one snapshot even if the original later changes.
  Do not alter the external source file.
- The staging root and unique job directory are captured and validated when the request starts.
  A worker may create/remove only files belonging to that directory. A stale or cancelled job may
  not delete canonical Assets, another job's files, or arbitrary paths in an old/new bundle.
- Introduce a prepared-Asset adoption seam: the owner validates the handoff, retains content-hash
  deduplication and metadata checks, adopts verified staged bytes into the existing canonical Asset
  path with the existing durability guarantees, and writes SQLite on the owner. Do not repeat the
  full source copy/hash/decode through `importAssetBytes` on the completion tick.
- The concrete durability order is verified immutable staging, owner generation/metadata
  revalidation, owner StageAsset journal entry before filesystem mutation, same-filesystem
  transfer, required file/directory flushes, then the Asset row and committed journal marker.
  Recovery recognizes transferred ownership; staging cleanup never removes a canonical Asset.
- A canonical duplicate's bytes are verified off-thread, not inferred from matching metadata.
  If that validation becomes stale or a canonical file appears after preparation, defer for
  revalidation or refuse with a reason. Never overwrite it, trust stale validation, or hide a
  new large hash pass in the completion tick.
- Adopt each successful prepared import against the current Project. Preserve the existing batch
  policy: named per-file refusals, successful audio files on their intended consecutive targets,
  and one undo transaction for landed Tracks/Clips. Cancellation before adoption lands nothing.
  Asset bytes/rows remain outside clip undo as today; orphan-cleanup behavior is not redefined.
- Prepared audio and any live resampled view use ADR-0059's immutable owners. Replacing a Project
  does not free buffers still owned by a worker; completing that worker does not resurrect the old
  Project. A failed adoption cleans only job-owned staging and reports the precise failure.

## Consequences

- **Positive:** expensive media preparation leaves the edit-handling thread; completion preserves
  intervening edits; cancellation and Project replacement have explicit, testable ownership rules.
- **Negative / accepted costs:** preparation still needs memory for its immutable input and output;
  final persistence/adoption has a small synchronous phase whose actual cost must be measured.
  Monolithic codec/DSP calls can delay worker retirement and final shutdown. This ADR does not
  promise instant cancellation, zero adoption cost, disk streaming or background Project opening.
- **First executable slice:** an owned selected-Clip stretch job and pure headless checks, then model
  integration for that operation. Next, staged source/decode/resample import and prepared-Asset
  adoption. A standalone worker passing tests is not proof that a shell path uses it.
- **Checks:** compare worker output to existing preparation without changing goldens; deterministic
  held-job tests for cancellation, supersession, Project replacement, input lifetime and stale
  completion; current-Project adoption preserving unrelated edits; exactly one undo step; failed
  persistence preserving prior state; cross-rate window/owner identity; staged-file ownership,
  duplicate content and per-format import behavior. Use existing TSan/pure-test coverage for the
  handoff and keep the Audio thread's existing safety checks unchanged.
- **Evidence boundary:** the current authorized work is headless. Do not launch the app, open an
  audio/MIDI device, inject desktop input or claim new app/hardware proof. Record integration or
  hardware evidence that cannot be obtained under that restriction as pending.
- **Vocabulary:** existing Project, Asset, Clip, Control thread, Audio thread and undo terms retain
  their meanings. Worker job, request version and staged result are implementation details, not
  new saved entities or a change to `CONTEXT.md`'s product model.
