# 0083. Musical audio as an owned, full-map prepared lane

- **Status:** Accepted (in-scope implementation decision under Dan's Option A mandate; fresh independent engine/saved-data critic accepted the concrete contract). Implementation and executable admission remain pending.
- **Date:** 2026-11-04 (local project date).
- **Deciders:** build agent and independent engine/saved-data critic, under Dan's Option A mandate.
- **Delivery plan:** [Musical audio prepared lane](../plans/2026-11-04-musical-audio-prepared-lane-plan.md).
- **Preserves:** ADR-0002, ADR-0010/0011, ADR-0030, ADR-0036, ADR-0049, ADR-0055/0058/0059, ADR-0078/0079/0080/0082 and existing goldens/RT annotations.
- **Scope:** explicit Fixed/Musical audio, known-source-tempo fit and complete Tempo-map follow. No warp markers, transient detection, automatic BPM detection, recording-rate change, new backend, generic job platform or DAWproject audio extension.

## Context and evidence boundary

Clip already stores TimeBase, native timeline/fade integers, an Asset-frame source window, reverse, gain and a legal scalar stretchFactor. New audio and voiceover are SampleLocked. OfflineRenderer currently refuses TempoLocked audio; its resolver is SampleLocked-only and its source factory tests scalar stretch without a Musical branch. There is no previously supported audible Musical rendering to preserve.

Dan authorized this feature and agent-owned headless verification. This is an independently accepted in-scope architecture extension. It is not authority to edit an Accepted ADR, dependency, golden or threshold. If review establishes a conflict with an owner-settled supported behavior, stop promotion and prepare the concrete conflict and alternatives before any owner question.

Actual ignored research is retained, not converted into a production PASS:

| Evidence | Recorded outcome | What it does not establish |
|---|---|---|
| Earlier ten-fixture/H1 study | 376 center comparisons, zero observed failures, 54 unproved centers | Default-hop completeness, general quality or a renderer |
| Default-hop run 20261105-030511-8801134 | Preparation/compile/inventory exit 0; probe exit 3; 12 fixtures; 741 compared centers; zero implemented center/ledger failures | Admission: 730 phase-ancestry cases, one context case and 131,136 quiet copies remain unproved |
| Same run's quiet trace | Source Frame 4480 copied to retained destination 0 under Q(x)=floor(3*x/2), whose expected destination is 6720 | Correct quiet-input mapping; audibility or pitch error was not measured |
| Waveform/resource/integration gates | Unreached | Pitch, level, transient, complete allocation, deadline, RT, production or hardware credit |

The first reset-anchor preparation failure remains separately retained. The corrected research runner recorded child exit 3; the outer PowerShell invocation reported 1. Neither is a product success.

## Options and recommendation

A. **Use existing TimeBase and a distinct, owned Musical preparation lane with one pinned persistent variable-I/O instance (recommended).** Preserve immutable prepared reads and the existing dependency; prove timing, state and waveform obligations separately.

B. Use uniform sub-renders and overlap/crossfade. This adds seam, phase, gain and duplicate-context obligations and inherits neither the scalar wrapper's whole-Clip goldens nor freedom from its quiet behavior. Keep as a separate fallback hypothesis.

C. Add a follow flag/source-BPM field, run varying DSP on Audio, or replace the backend. Redundant stored policy and Audio-thread DSP are rejected. A backend replacement would reopen ADR-0030 and is not authorized by this proposal.

Select A for the architecture and next renderer hypothesis, not admission of the existing research candidate.

## Decision 1 — policy, stored meaning and conversions

| User policy | Existing stored representation | Meaning |
|---|---|---|
| Fixed | TimeBase::SampleLocked | Existing exact Project-frame placement/duration and scalar-stretch law; Tempo edits do not alter it |
| Musical | TimeBase::TempoLocked | Canonical Tick start S and length L define one traversal of the selected source window through the complete Tempo map |

Audio chooser/drop/Browser import and voiceover remain Fixed by default. Opening a Project never converts a Clip. Asset bytes, srcOffset/srcLen in Asset frames, existing TimeBase values and schema fields remain unchanged. Add no persisted BPM, follow flag, pending job or cache row.

Musical timelineLength is authoritative. Select its branch before scalar stretch. A stored legal TempoLocked stretchFactor, including 1.5, round-trips unchanged through save/load/undo/autosave/recovery but is ignored by Musical rendering. Do not multiply both timing laws. This extends the previously refused audio branch; it does not change Fixed rendering or promise preservation of a previously audible TL result.

Conversions are complete, undoable Clip-row edits, never enum flips:

Retain exact integer Frame endpoints until inverse/rounding certification. Casting a large integer to double and comparing an inverse/forward round trip only against that already-rounded value is not a certificate. Certify the exact input and final Tick rounding, or return MusicalInversePrecision; do not impose a blanket magnitude cutoff in place of that check.

- **Fixed to Musical:** map the current absolute integer-Frame start and end through the checked full inverse, round each once to canonical Ticks and subtract to obtain L. Resolve the canonical endpoints again before display/adoption. The result preserves the requested arrangement span only to Tick quantization, not exact old integer Frames. Preserve source window, reverse, gain, identities and metadata; set factor to 1. The old Fixed scalar duration is represented by the resulting span rather than reapplied.
- **Fit source tempo / known BPM:** source BPM is a finite positive operation input. Compute L by checked nearest rounding of srcLen / assetRate * sourceBpm / 60 * PPQ, retaining original Asset units and avoiding premature integer division. Set Musical/factor 1 and retain the complete selected source window. Placement converts once through the full map. Do not store or infer BPM later; refuse L<=0 or unsupported preparation.
- **Fit to musical range:** use the accepted musical start/end boundaries, converting Frame boundaries once under ADR-0082 if needed. Derive L from those ends and use the same Musical preparation law/factor 1.
- **Musical to Fixed — Original speed:** retain the original selected source window, place at current resolved A and set natural duration to checked nearest(srcLen * projectRate / assetRate), SampleLocked/factor 1. Label this as original-speed return. It does not freeze the currently heard varying result; freeze/bounce is separate scope.

For every conversion, rescale each fade independently by checked nearest(oldFade * newNativeLength / oldNativeLength). Preserve exact zero, shape, curve, temporal ends and legal overlong fades; do not clamp to Clip length. This preserves normalized progress, not milliseconds. Musical fades remain Ticks; Fixed fades remain Project frames.

A Clip whose ID is referenced by RecordingTake.clipId is conversion-ineligible. Full audio preparation/loading also refuses an already-stored linked TempoLocked Clip with MusicalLinkedTakeUnsupported, even if muted, before building/publishing audio. The current validator does not exclude that dormant combination. Do not rewrite Take/Comp rows or change saved validity. An independent Clip sharing the recorded Asset remains eligible. Preserve reversed split/Join identity, including the lower-source original ID on the temporal right of a reversed split.

## Decision 2 — one resolved clock and exact integer cuts

S>=0 and L>0 are canonical Ticks. Check S+L before use. F is the shared checked compiled complete Tempo-map clock at the Project rate, including its cumulative prefix:

    A = floor(F(S))
    B = floor(F(S+L))
    D = B-A
    Q(i) = floor(F(S + L*i/N)) - A, 0<=i<=N
    Q(0)=0; Q(N)=D

N is the selected project-rate input-view window length after ADR-0055's two-end Asset-to-view mapping. Keep N/i and source counts in uint64 domains; use checked quotient/remainder or equivalent wide arithmetic for L*i/N. Do not narrow the denominator to uint32, overflow the product/remainder, round to a whole Tick, or sum rounded per-block durations.

**Numeric A — computed-clock cuts with ambiguity refusal (selected).** F means the existing ADR-0082 compiled binary64 lookup and its operation order, not the exact real logarithm. Integral queries use its whole-Tick/zero-fraction path. Construct adjacent binary64 fraction bounds enclosing a nonintegral exact rational remainder/denominator by integer bit construction/long division; normalize an upper endpoint of one by checked carry. Do not first round numerator and denominator independently to double. Evaluate F at both bounds; require finite ordered results, width <=1/1024 Frame and identical integer floors. Otherwise return MusicalBoundaryAmbiguous. The common floor is the normative cut with zero integer tolerance. No epsilon snapping is allowed.

**Numeric B — certified real-analytic floors (not selected).** This would require a bounded exact-rational/interval evaluator for all preceding prefixes and local transcendental math, escalating until one integer floor is proved or refusing. Two agreeing precisions, long double, adjacent-double queries or an epsilon are insufficient. Selecting B later is an explicit new decision, not a silent replacement of A.

Under A, use the checked clock's existing [0,2^63) Frame domain and strict cast guards; impose no additional blanket magnitude cutoff. Every actual queried rational position must pass its bracket/common-floor checks. An exactly represented large query may pass while a smaller near-cut query refuses. Independent rational constant/jump and high-precision ramp/prefix fixtures must achieve <=1/1024 Frame absolute clock error and inverse-forward residual in their admitted fixture domain, including near-integer cuts and representable values beyond 2^40. These are mechanical fixture requirements, not a universal real-log error theorem.

One resolved A/B/D object supplies requests, keys, result validation, both actual schedule paths, horizon and export. Pass the compiled clock into resolution; do not retain independent graph arithmetic. A Tempo change before S can alter prefix phase/D, so it is a dependency. For example, 120 BPM/48 kHz at S=1,L=1 gives D=2 by absolute floors, whereas rounded endpoints give 1.

Validate order, finite/domain results, signed schedule capacity, size_t products, and reservation limits before narrowing/allocation. D<=0 is MusicalWindowCollapsed. Successful PCM has exactly D Frames per channel: no force-one, unexplained pad/truncate, or min(sourceFrames,D) fixup. The audio horizon includes B; existing graph/PDC tail is separate. Fixed integer geometry/scalar and MIDI's distinct event-floor/horizon-ceil contracts are unchanged.

## Decision 3 — source views, renderer state and precise refusals

Resolve and retain original immutable Asset owner/hash/window. Obtain or prepare the correct project-rate view, mapping source ends under ADR-0055's existing half-away rounding. View mapping is separate from Musical timeline flooring. A mapped empty window refuses.

Reverse only the selected source window **before** the varying map. Keep one deterministic pinned Signalsmith 1.1.0 instance across the Clip; do not reset at each Tempo segment or present one N-to-D call as interior follow. Use consecutive variable integer I/O calls, exact cumulative Q cuts, explicit fractional source-cell Tempo boundaries and zero-output coalescing that retains all logical input/history.

First-lane layout is mono/stereo. The continuous local ratio (L/N)*F'(S+L*u) must remain within [0.5,2.0], checking both sides of jumps and ramp extrema. Otherwise return UnsupportedMusicalStretchRatio. This limits the new preparation lane, not stored BPM/map validity. Quantized integer call ratios can differ briefly from the continuous ratio; do not clamp them or reject them solely for that difference. Preserve the existing checked total-input/output int ceilings and storage bounds until a separately reviewed implementation proves an expansion; uint64 planning alone does not authorize processing every uint64 length.

Four separate obligations apply:

1. **Scheduling/ledger:** exact owned current and previous analysis reads; actual current-analysis-center and physical synthesis/extraction geometry; unchanged <=2-Frame center scheduling and exact count/index checks.
2. **Persistent state:** phase/band/channel/history state is initialized, finite, advanced and owned by one preparation generation. No stale/unowned read or cross-Clip state. Prior phase is intentional recurrent state, not a single current-source timestamp.
3. **Boundary context:** every startup/seek/end-zero operand, synthesis interval and retained read has a declared owner and bound. Account all retained contributions; neither difficult windows nor centers outside the source interval disappear from the ledger. Separately certified context is not an interior-center exclusion.
4. **Waveform:** independent pitch/level/peak/transient/stereo/silence/boundary/envelope checks below. Scheduling geometry cannot substitute for it.

The existing complete-transitive-phase-to-single-timestamp experiment remains FAIL with its original source, counters and gates. It is a stronger causal-attribution experiment whose single timestamp is not a defined timestamp for normal recurrent phase. This ADR does not zero its counters, certify that graph, excuse quiet timing, or reuse its limited results as product admission.

**Quiet input:** the pinned fast-return uses energy <1e-15, can cyclically copy genuinely nonzero input and skips ordinary STFT advancement. The recorded source4480->retained0 result contradicts Q(4480)=6720 in the 3/2 case. Before publishing PCM, the implementation must certify mapping/state/extraction for any retained nonzero fast-return contribution or return MusicalQuietPathUnsupported. The initial hypothesis selects the named refusal, using actual observation or a proved-equivalent pinned branch/state model; an unproved detector itself refuses. Exact zero has a separately counted path and still needs silence/resume proof. Do not silently drop tiny samples, dither/normalize them, enlarge a tolerance, fall back to Fixed playback, or patch/bump the dependency under this ADR. A broader quiet-path repair is a separately reviewed renderer hypothesis.

The odd-rate context case is likewise unresolved: identify its actual input window, declared context, synthesis interval and retained reads. Certify it under the context rule or return MusicalBoundaryContextUnsupported. Do not use a sample-rate whitelist instead.

These proposed failure names are requirements, not claims of existing C++ enums. Keep domain/overflow/ambiguity/collapse/layout/ratio/configuration/quiet/context/resource/ready-input/stale/history/linked-Take failures distinct and report them through existing operation-specific results. Derived preparation refusal never invalidates or rewrites stored Project/Asset bytes.

## Decision 4 — numeric, quality and resource requirements

These retain the prior draft's targets. They remain unproved until execution; failure does not permit tolerance adjustment.

| Layer | Required gate |
|---|---|
| Boundary progress | Zero disagreement for A/B/D/Q, cumulative logical input N and designated output D. Separate physical pre-roll/lookahead/end-zero context |
| Rational/inverse precision | Rational clock bracket <=1/1024 Frame with common floor; forward(inverse(A+j)) residual <=1/1024 Frame for fade queries |
| Calls and pinned float offsets | Ordinary process I/O each 1..2048; actual callback o in [0,O); pinned round(o*float(I)/O) equals independent exact nonnegative rational rounding, ties away: zero integer disagreement |
| Center schedule | Every actual contributing interior analysis center x maps to y within 2 Project Frames of Q(x); certify all other retained context separately |
| Planner proposal | Same-segment unrounded chord deviation target <=1/4 Frame, with analytic extrema and exact fractional Tempo changes retained; this never substitutes for actual-center checks |
| Latency | Separate inputLatency/outputLatency; identify retained interval before DSP; discard the single documented startup outputLatency; exact physical/read ledger and exactly D audible Frames |
| Configuration | Finite binary32 rate; derive/check preset W/H, FFT K, channels/bands, W/H/history/count/index and rounded-ring products before vendor narrowing/allocation; queried configuration must agree |
| Work | <=2^20 process calls and <=2^28 preparation work units per Clip, counting actual map/prefix/planner/refinement/source/callback checks |
| Storage | <=512 MiB newly reserved preparation payload per Clip; <=1 GiB across a pending batch and <=1 GiB reusable Musical cache, including maps/index, DSP/FFT, new views, working planes, completed PCM and peak work |
| Signal | All samples finite; exact-zero input stays zero; identical stereo remains identical |
| Tone quality | 220/997 Hz, at least four seconds, peak<=0.25; 250 ms frequency/RMS windows every 50 ms; <=5 cents, <=1 dB, peak<=1.25*inputPeak+1e-6 |
| Estimator calibration | Analytic input error <=0.25 cent and <=0.01 dB before using the estimator |
| Transients | Symmetric 1 ms zero-DC broadband bursts >=4 queried windows apart; energy-centroid error <=5 ms using a local +/-2-window analysis region |
| Fade correctness | Independent native fractional envelope; sample error <=2e-6*max(1,abs(unfadedSample)); envelope [0,1]; exact zero outside [A,B) |
| Same-owner schedule reads | Sample equality under absolute offsets, blocks 1/7/128/257, seek and loops; quality tolerances cannot excuse a wrong PCM read |

Tone windows exclude only one queried STFT window at source/Clip edges; do not exclude ramp/jump interiors. The initial 44.1/48/96 kHz matrix and factors 0.5/0.75/1/1.5/2 are fixtures, not rate/segment/window whitelists. Extend representative low/high/boundary cases needed for claims; no universal quality claim follows from three rates.

Audit the actual pinned FFT/ring allocation graph before configuration. Check wider products and rounded capacities before entering signed-int helpers; the next-power-of-two ring bound follows from its int loop, not an arbitrary 65536 window cap. Count reserved payload, shared storage once and actual overlapping owners; live/retiring owners cannot be evicted to invent headroom. Do not claim total RSS or a preparation wall deadline from these caps. A budget-fitting 5000-segment same-BPM map and safe configurations above 65536 remain eligible when actual checks pass.

Establish IEEE binary32 round-to-nearest and no fast-math/reciprocal reassociation for the pinned offset proof; otherwise UnsupportedArithmetic. Check all denominators through 2048 with an independent integer oracle. The exact small multiply/divide argument proves neither STFT alignment, phase quality nor full-map accuracy.

Negative controls must fail their relevant unchanged gates: endpoint-average timing, ignored fractional jump, one-Frame source/history or latency index error, outputLatency omission/shift, three-Frame center offset, mishandled silent advancement, reverse-after-stretch signal, 6-cent pitch error, 1.1-dB level error and double fades. The historical observer/record mutations are not evidence that all waveform mutants ran. Never claim an unreached negative control.

## Decision 5 — one-time fades and immutable schedule projection

After varying stretch, evaluate fades at each absolute output Frame A+j through the checked inverse map. Use the existing fade-shape/curve formulas and min(inGain,outGain), with a new fractional-native evaluator alongside the integer law. Do not round each sample's Tick. Only the leading floor-quantized sample may lie less than one Frame before S; bound that case and use local zero. Do not clamp other invalid inverses or overlong fades. Clip gain remains live.

Every successful Musical schedule entry reads:

    immutable prepared owner; prepared source offset 0
    startFrame=A; sourceFrames=windowFrames=D
    reversed=false; fadeInFrames=fadeOutFrames=0
    gain=current Clip.gain

The two real builders are buildProjectGraph and buildTrackClipSchedule, through makeScheduledClipSource/ProjectMixerProjection/ClipSchedule. Both consume the same resolved window/ready owner. Runtime remains immutable absolute-frame reads with zero added source latency to PDC; no vendor DSP, allocation, locking, logging, I/O, job work or reclamation runs on Audio.

engineProjectFor/projectInViewFrames may map Asset/source windows into view coordinates; they never rewrite Musical native Tick geometry or save a prepared window.

## Decision 6 — complete keys and quality tiers

Add a distinct Musical prepared owner/key rather than extending scalar StretchedOwnership's identity by inference. The PCM key includes exact equality over:

- Clip/Asset identity, original content hash, original immutable owner identity/rate/channels/frame count and Asset-frame window;
- mapped immutable view owner/window/rate/channels and ResampleQuality;
- native S/L, Project rate and the immutable complete Tempo map, including all preceding prefix phase and later ramp-defining endpoints;
- reverse, native fade lengths/shapes/curves, resolved A/B/D and planner/quality/library identity.

Hashing is only an accelerator. Retain original/view owners during identity comparison to prevent address-reuse ABA. Gain, selection, name/colour, playhead and Meter are not baked-PCM dependencies. Track existence, mute/routing, attachment generation and request version remain separate adoption/publication dependencies.

Live uses LivePlayback views; export/stems/offline render use their own OfflineRender views and derived owners. Never satisfy the offline tier with live PCM merely because geometry matches. Keep original canonical Project/window provenance in ExportSnapshot or equivalent immutable per-Clip requests before view mapping; derive the mapped projection once in the Export job. Do not reverse rounded view coordinates back into original source identity. Apply this to mix and all stems.

## Decision 7 — pending batches, history and publication

Reuse ADR-0080's operation-specific lane: one worker including any retiring worker, with at most one latest pending replacement. Capture owned immutable inputs, map, request/version, attachGeneration, Project ID, stable targets and intent. No model, shell, SQLite or undo handle crosses to the worker. Progress names real phases/work; cancellation never invents completion.

Keep committed geometry/playback while preparing. The accepting tick performs only ready lookup or captures a worker-side view miss. On completion:

1. Reapply the captured intent to copies of the current Project and current undo stack.
2. Re-enumerate required Musical Clips and all exact keys, including newly added/unmuted Clips, source/fade/reverse edits and preceding-map changes. Muted Clips remain structurally checked. Reuse exact matching owners; extend/restart within bounds or refuse missing dependencies.
3. Compile the complete clock/MIDI/click/audio candidate with ReadyInputsOnly. Missing required Musical or scalar/view owners yields PreparationRequired; no interactive call to refreshStretchOwners/prepareTimeStretch/resampling may hide a cache miss.
4. Through the existing checked durable-adoption/publication law, persist once, adopt one Project/undo result and publish one coherent replacement. New clock/old Musical PCM and old clock/new PCM combinations are forbidden. Validate installation capacity/eligibility before consuming state; refusal/persistence failure leaves committed Project, playback and history unchanged.
5. Preserve transport/loop state and ADR-0082's captured count-in map, fixed opening boundary, pre-boundary tails and one-shot completion. A map edit is not an ADR-0079 notes-only update.
6. Retire cancelled/superseded/replaced work off Audio after terminal release/acquire handoff; service ticks never join running workers. An in-flight DSP stage may delay retirement; no instant-cancellation claim.

Undo/Redo/history navigation needs nonpersisted eligibility for the **whole step**: history generation, cursor, whole-step identity and version covering every grouped entry/diff. Invalidate for push/pop/clear/branch change, group membership, in-place coalescing/sealing/net-no-op removal even if depth/verb stays the same. nextUndo/nextRedo alone are insufficient. Use nonwrapping checked versions or ownership-backed identity; exhaustion/ambiguity refuses rather than accepting ABA.

Capture the intended step, validate current history at adoption, then rerun it on copies of current state. Never transplant an earlier nextUndo stack or silently target an intervening unrelated edit. Stale/failed navigation consumes no history. Ordinary conversion/map completion appends to current history, preserving unrelated gain/name/selection edits and established one-gesture coalescing.

## Decision 8 — loading, display and activation

Opening remains the existing synchronous full-load surface, not a promise of background Project opening. Enumerate/refuse linked TL Takes first, even muted. After validated decodes/views, add explicit Preparing musical audio before PlaybackEngine::create; prepare LivePlayback owners under the same law, then build ReadyInputsOnly and seed cache only on successful attachment. Keep the previous Project/engine until the candidate is ready; failures retain it. Metadata-only/missing-Asset states mark audio unavailable and cannot Play/render missing Musical input. No native popup, listening or owner approval gate is added.

Before controls activate, migrate every affected native-unit boundary: move/trim/slip/split/ranges, copy/paste/repeat, stretch-to-range, fades, Strip Silence and Shuffle/No overlap. Mixed overlap/order/coverage compares resolved absolute Frame ends, then converts retained fragments into each Clip's native law. Musical source progress is proportional to Tick progress. Preserve exact source adjacency/identity rules; Track-only and zero-time-delta operations preserve native values. Automation Follows Clips uses each resulting native delta.

Timeline canvas/inspector/heard boundaries use the same A/B/D while retaining S/L. Musical waveforms read ready prepared PCM peaks/samples keyed by the complete key, offset0/countD, without second reverse/fade. Fade controls edit native lengths and map their geometry/envelope; any seconds readout uses actual fade-segment endpoints and does not rewrite legal overlong parameters. Gain stays separate/live. Peak misses schedule derived work, never stretching or synchronous peak building in paint. Pending UI keeps committed waveform/geometry with a transient pending indication.

DAWproject's existing Musical-audio refusal remains. Unsupported-TimeBase tests may be deliberately replaced by genuine supported and named-refusal fixtures; goldens and RT annotations are not rewritten.

## Compatibility and source seams

ADR-0055's scalar length/half-away formulas remain normative for supported Fixed audio and Asset-to-view mapping. The new absolute-floor/Tick-span law is an explicit extension for previously refused TL audio, not a retrofit to Fixed or a source-unit migration. If the fresh critic finds an actual owner-settled supported case altered, promotion stops. ADR-0030's pinned dependency, scalar prepareTimeStretch duration/fingerprints and scheduler/PDC shape remain intact.

Read snapshot anchors (line numbers may move; symbols govern):

| Source | Existing fact / required seam |
|---|---|
| Project.h:659 Clip; ProjectBundle schema/read/write | All stored fields already exist; no new schema is proposed |
| Project.h:1829 recordingTakesReferenceProjectRows | Linked identity is Clip ID; it does not currently reject TL |
| Time.h frameForMusicalTime/musicalTimeForFrame | Checked computed clock and precise lookup failures, not certified real-log enclosure |
| OfflineRenderer.h:59 StretchedOwnership; :159 ResolvedClipWindow; :298 makeScheduledClipSource | Scalar ownership/window lack complete Musical key/map |
| OfflineRenderer.h:389 scalar branch; :468 resolveClipWindow; :578 buildProjectGraph refusal | Musical must branch before scalar and share one compiled-clock resolver |
| ProjectMixerProjection/ClipSchedule | Actual positioned immutable source/schedule path; no invented AudioClipNode API |
| ClipEnvelope.h; RateMatchedView.h | Integer fade law and two-end view rounding stay; add fractional fade separately |
| app/StretchPreparationJob.h:47 key | Existing bounded owned scalar job pattern; do not pass live model/database |
| UiAppModel.h readyLiveRateMatchedView; refreshStretchOwners:14868 | Ready lookup exists; refresh can still prepare misses synchronously |
| ProjectUndo.h:3068 record; :3201 history; :3236/3241 nextUndo/nextRedo | Coalescing mutates existing top/group entries; whole-step tokens are missing |
| UiAppModel.h loadPreparedProjectBundle/attachProjectBundle | Explicit full loading must prepare before engine creation/attachment |
| ExportJob.h:344/:613 OfflineRender views; :363/:647 render paths | Both mix and stems need canonical original provenance and tier-specific Musical preparation |
| MainComponentArrange/Inspector; UiAppModel applyEditModeAfterPlacement | Waveform/fade/native mixed-unit migration is an activation dependency |
| DawprojectPackage.h | Existing TL-audio refusal is a separate format boundary |

## Consequences and promotion

Benefits: coherent full-map audio, immutable Audio reads, explicit failure rather than silent retiming, no stored-policy duplication and preserved Fixed behavior. Costs: derived PCM memory/work, pending edits, named refusal for uncertified inputs, and synchronous full-load preparation until a later design changes it.

A fresh independent critic accepted this concrete contract and its compatibility/numeric A/B distinction before promotion. Update CONTEXT for implemented user-facing vocabulary at the corresponding delivery boundary; do not mark the feature delivered merely by accepting architecture. Follow ADR-0078: small logical commits, required relevant proof/existing suites, engine/saved-data critic, direct-main commit/push and CI; no needless per-UI rubric or per-unit native drive.

Local verification is strictly headless: no app/native peer/window/dialog, desktop input, audio/MIDI device or hardware launch. Use pure checks and audited offscreen harnesses with exit 0/1; preserve raw failure/receipts and exact source pins. Native/hardware proof stays separate. After three unsuccessful corrections require a fresh critic and a supported new hypothesis, never a threshold increase or a request that Dan listen/debug.

This architecture acceptance supplies no new execution result, production admission, quality/performance certification or permission to broaden the accepted scope. The companion plan identifies the remaining implementation and mechanical proof.
