# Delivery steps — Musical audio prepared lane

Status: implementation plan under [Accepted ADR-0083](../adr/0083-musical-audio-prepared-lane.md). Implementation and executable admission remain pending. Root controls tracked edits, heavy-lane execution, commit/push and CI.

## Boundary and order

Finish the currently authorized clock/native/MIDI interchange checkpoints first. Accept the concrete new Musical-audio ADR with a **fresh** engine/saved-data critic before code depends on it. Each unit below is a logical committable behavior, not a requirement to split coupled code arbitrarily. Refine allowlists against current source before GO; file names for new components are proposals.

Per unit: select one focused feature smoke or actual regression before production changes; build and relevant existing checks, full pure suite where the implementation brief requires it, GUI consumers compiled without launch, applicable engine/saved-data critic, concise STATUS, explicit-path main commit/push and exact-code CI. Save/load/autosave/recovery changes always have tests. Run one heavy lane. Do not add per-UI critic/rubric/native-drive gates beyond ADR-0078.

Every planned check below is **unrun**. No manual PASS row, synthetic hardware credit or human ear/eye gate.

## 0. Promote the decision, without claiming the renderer works

- Recheck next ADR number and source seams; independently review numeric A versus B, dormant-TL/ADR-0055 compatibility, original-speed conversion, nonzero quiet refusal and the distinction between scheduling/state/waveform evidence.
- Land the actual ADR and exact steps before dependent implementation; do not edit Accepted ADRs. Preserve all ignored research runs and their admission FAIL.
- Prepare vocabulary changes for Fixed/Musical in CONTEXT at implementation, preserving existing Project/Clip/Asset/time_base meanings.
- Finish line: fresh critic accepts the concrete document; unresolved owner-contract conflicts are written as concrete alternatives, not guessed away.

## 1. Resolve Musical windows and exact keys without activating playback

**Behavior:** one checked resolver constructs S/L, exact rational Q queries and A/B/D; one immutable dependency key represents the exact input/map/tier.

**Likely scope:** a new JUCE-free engine policy/window/key header, focused new pure tests and CMake. Reuse Time/RateMatchedView/ClipEnvelope surfaces without modifying legacy arithmetic or goldens.

**Proof:** independent constant/jump/ramp/prefix boundaries; the S=1,L=1 floor witness; ambiguous brackets/collapse/signed capacity; uint64 denominator above UINT32_MAX without huge allocations; representable large positions; complete-prefix key inequality despite equal endpoint duration; tier/source-owner ABA distinctions. Zero-time and Track-only identity remains unchanged.

**Finish line:** pure API precise success/refusal and key equality exist, tests pass, existing Fixed outputs unchanged. No graph/UI activation or current-candidate renderer acceptance.

## 2. Add explicit conversion intents and round-trip preservation

**Behavior:** pure current-Clip conversion for Fixed->Musical, known BPM, musical-range fit and Musical->Fixed Original speed. Normalize fades exactly as the ADR; no enum flip or persisted BPM.

**Likely scope:** policy helper; existing Project edit/undo row-diff seam only as needed; tests for pure conversions and stored-row round trips. No schema migration unless a later reviewed finding demonstrates a genuinely new field.

**Proof:** ordinary/reversed/cross-rate/faded windows; zero/overlong fades; Tick quantization after inverse; exact integer-Frame inputs beyond double's contiguous integer range are certified or precisely refused before losing their identity; nonintegral natural duration; legacy TL factor1.5 survives untouched while ignored by proposed Musical rendering. Save/load/undo/redo/autosave/recovery preserve complete before/after Clip rows. A recording-Take linked Clip is refused, including stored muted TL at full-load eligibility, while another Clip sharing its Asset remains eligible. Failed conversion leaves bytes/history unchanged.

**Finish line:** supported intents return complete proposed rows/refusals; no user control invokes unproved rendering.

## 3. Certify the planner and source/state ledger as a separate candidate

**Behavior:** one persistent pinned-instance planner with exact cumulative cuts, bounded ordinary I/O, fractional Tempo boundaries, declared pre-roll/tail/extraction and complete owned state/context records.

**Likely scope:** a new varying-preparation component/harness, not prepareTimeStretch or TimeStretchNode goldens. Dependency source remains pinned/unchanged.

**Proof:** integer/Decimal oracles; offset arithmetic for all denominators through2048; ordinary default/odd and budget-fitting large configurations; zero-output coalescing; source-history/physical extraction counts; center <=2 Frames; exact context and correct state initialization/advance across silence/resume. Account every retained contribution. Keep the old complete-causal experiment failed and separate.

**Quiet prerequisite:** independently replay recorded source4480->retained0 and require its mapping checker to reject versusQ6720. Prove the new named nonzero-fast-return refusal/detector; current Project, undo, source bytes and PCM publication stay unchanged. Explain/certify the odd-rate context record or give its named refusal, without a rate whitelist.

**Finish line:** actual planner/state/context evidence and precise refusal; no quality, allocator or production credit by association.

## 4. Certify waveform and complete resources before renderer admission

**Behavior:** varying preparation produces exactlyD finite samples/channel, reverses the selected source before map, applies one fractional native min-envelope afterward and leaves gain live.

**Proof:** calibrated independent 220/997 Hz estimator, four-second mono/stereo/ratios/ramp+jump matrix with unchanged pitch/RMS/peak/transient/envelope thresholds; no ramp/jump interior exclusions. Exact zero/stereo identity, quiet refusal and silence/resume/boundaries. Actual pitch/level/latency/reverse/fade mutants fail their own gates. Compare predicted allocation requests/reservations against actual DSP/FFT/ring/view/PCM/peak payload and work counters under all stated caps.

**Finish line:** admitted-domain renderer with concrete input/config/resource failures; scalar wrapper/fingerprints/goldens unchanged. No unsupported universal-quality or wall-time claim.

## 5. Make both builders consume ready Musical owners

**Behavior:** buildProjectGraph and buildTrackClipSchedule share the compiled-clock resolver and Musical branch before scalar. They use immutable prepared owner0/D atA, reversefalse/fades0/currentgain.

**Likely scope:** OfflineRenderer/ProjectMixerProjection/ClipSchedule option/ownership plumbing and focused rendering/playback tests. A ReadyInputsOnly policy is explicit; no synchronous miss fallback.

**Proof:** graph and live schedule use exact owner/window/lifetime, sample-equal same-tier prepared reference under blocks1/7/128/257, interior ramp seek/loop, silence outsideA/B, gain edit without preparation, and no double reverse/fade. Tail/PDC remains separate. Missing owners return PreparationRequired and preserve prior state. Existing unsupported-time-base fixtures are intentionally replaced with supported plus genuine named-refusal fixtures, never by editing goldens.

**Finish line:** both engine paths work when supplied certified owners; UI default paths still cannot silently enable unsupported stored cases.

## 6. Own bounded Musical preparation jobs

**Behavior:** extend the existing operation-specific stretch lane with owned typed batch requests/results, one active/retiring worker and one latest replacement. Worker owns all buffers/maps, never model/undo/SQLite/shell.

**Proof:** held workers for cancel/supersede/replace/delete/source-owner ABA; release/acquire terminal publication; no service-tick join; retained source lifetime; resource refusal; ready-view lookup versus worker-side view miss; cancellation between bounded phases and honest indeterminate DSP progress.

**Finish line:** pure worker lifetime/refusal contract. Worker-only checks do not claim model adoption.

## 7. Protect whole-step history eligibility

**Behavior:** nonpersisted history identity/version covers the complete grouped/coalesced step, cursor and generation.

**Likely scope:** ProjectUndo and owning model navigation requests; no saved-history schema.

**Proof:** coalescing without depth change, group membership/sealing/net-no-op removal, push/pop/clear/branch/cursor changes, stale Undo/Redo/history jump, checked token exhaustion and owner ABA. Pending navigation cannot consume history or silently undo an unrelated edit.

**Finish line:** stable current-state eligibility usable by asynchronous map/conversion adoption; ordinary existing one-gesture behavior unchanged.

## 8. Adopt current-Project map/conversion batches atomically

**Behavior:** capture intent, prepare, rebase onto current Project/history, re-enumerate affected Musical inputs, compile ready-only complete clock/MIDI/click/audio, persist once and publish one coherent replacement.

**Proof:** held-worker map-prefix/source/window/fade/reverse edits; newly added/unmuted Clips; unrelated gain/name/selection edits preserved; current-history rerun; missing dependencies extend/restart or refuse; persistence/install failure consumes nothing. Instrument/poison synchronous preparation to prove no fallback. Preserve captured count-in map/boundary/pre-boundary tails/one-shot completion. No new-map/old-PCM interval.

**Finish line:** actual model integration under current ownership/history, one durable undo step. No user-facing controls until remaining activation dependencies pass.

## 9. Preserve export provenance and tier identities

**Behavior:** ExportSnapshot retains original canonical Project/window provenance; mix and stems prepare OfflineRender views/owners in their worker, with no live-tier substitution.

**Proof:** cross-rate original-window key, once-only view mapping, actual interior clock landmarks, exactD and same-tier sample reference for mix and every stem, later Project edits leave export snapshot unchanged; cancel/failure preserves earlier destination. Do not require live/offline bit identity where ADR-0055 declares different kernels.

**Finish line:** both real export paths have complete tier-correct preparation. DAWproject Musical audio stays explicitly unsupported.

## 10. Add explicit full-load preparation

**Behavior:** full load checks linked TL Take eligibility, prepares Musical audio after validated decodes/views and before engine creation, builds ready-only and seeds only adopted cache. Existing synchronous loading remains explicit.

**Proof:** ordinary TL reopen/audio parity; legacy factor bytes; muted linked-Take refusal; independent recorded-Asset reference; source/precision/resource failure keeps old Project/engine attached; metadata-only/missing-Asset state cannot Play unavailable audio; relink/full load supplies readiness; save/autosave/recovery tests remain genuine.

**Finish line:** full reopen works for admitted Musical inputs without pretending all Project opening became asynchronous.

## 11. Repair native editing and faithful displays before activation

**Behavior:** finish mixed Frame/native range/edit boundaries, source progress and prepared-PCM waveform/fade/readout projection.

**Likely scope:** pure Clip helpers/UiAppModel editing and Arrange/Inspector projections in logically small batches; no broad cosmetic redesign.

**Proof:** mixed Fixed/Musical No overlap and Shuffle compare Frame intervals and retain correct native fragments; split/trim/slip/ranges/Strip Silence/stretch-to-range/copy/repeat obey source/native rules and reversal identity. Track-only/zero-delta exact identity and Automation Follows Clips native deltas. Independently mapped ramp waveform landmarks use prepared peaks/samples, seconds readouts expose actual boundaries, overlong fade values remain untouched, painting never prepares. Save/undo/reopen for each stored behavior change.

**Finish line:** existing and new audible/native/display boundaries agree mechanically. No raw-Asset uniform waveform squeezed over a varying span.

## 12. Activate the explicit controls and certify the feature boundary

**Behavior:** Fixed/Musical, known-BPM and range-fit controls call the already proven request/adoption path; pending state shows committed audio plus transient indication; Original speed is labelled clearly.

**Proof:** one real audited offscreen feature smoke through controls and saved state, including failure/no-change and one-step Undo/Redo. Existing pure/GUI compile/warnings/offscreen suites and unchanged RTSan/TSan/deadline CI pass. Use exact-code evidence; native interaction/device/hardware remains separately pending under this headless authorization.

**Finish line:** update CONTEXT/STATUS user-facing capability only for delivered admitted behavior. Do not certify the whole OptionA request while linked editing/folders/ghost views or other requested units remain.

## Routine choices and escalation boundary

Accepted ADR-0083 selects A for policy, numeric semantics, renderer hypothesis, loading and quiet refusal; the concrete architecture passed fresh independent critique. These in-scope decisions need no new Dan interview. Remaining implementation choices are concrete names/file splits, checked arithmetic representation, bounded refinement strategy, exact context/state instrumentation and detector proof. Select them from source/evidence and review normally.

The current renderer is not admitted. A failed waveform/state/resource proof calls for a supported correction or another critic-reviewed hypothesis after three unsuccessful attempts; retain failures and thresholds. A dependency/backend replacement, changed owner-settled supported contract, spending/publication/destructive user-data operation or unavailable essential equipment requires preparing the concrete choice under AGENTS before any human question. No such approval is requested by these drafts.
