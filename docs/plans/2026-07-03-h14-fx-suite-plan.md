# H14 — Built-in FX suite: focused plan

> Decisions: [ADR-0038](../adr/0038-built-in-fx-suite.md) (FX suite, ParamSpec, tails, inserts),
> [ADR-0037](../adr/0037-alpha-target-and-h14-h19-recarve.md) (why FX before hosting).
> Guardrails for the implementing model: [`docs/fable5/implementer-brief.md`](../fable5/implementer-brief.md).
> Do not open H14 until H13 is closed remote-green and ADR-0037/0038 are Accepted.

**Goal.** Five built-in FX Nodes (EQ, compressor, delay, reverb, limiter) on persisted Track/Bus
insert chains, driven by the ParamSpec model, fully headless, every gate biting. Plus the
equal-power crossfade upgrade. No FX UI (that is H16).

**Mechanical exit criterion.** All new `YesDaw*Check` gates below green in CI on all five jobs,
including: per-node null/response/ballistics/tap/RT60/ceiling gates with their named negative
controls; block-size independence sweeps bit-identical; limiter PDC parallel-path alignment;
offline Render == RT with a full FX mix; FX state save/reopen through the new FX schema version;
the frozen FX-era fixture bundle forever-gate; RTSan clean on every new `process()`.

> **Review amendments (2026-07-03).** Amended per
> [`docs/reviews/2026-07-03-adversarial-review-h14-h17-packet.md`](../reviews/2026-07-03-adversarial-review-h14-h17-packet.md):
> clip-gain ownership named in CP2 (finding 3); complete normative EQ equations + identity
> anchors in A1 (finding 4); one normative limiter algorithm in A5 (finding 5); the
> absolute-frame anchoring rule in the appendix preamble, shared with H15 (finding 6); schema
> numbers are "next free version" (H13 still open).

---

## Gates that must BITE (master list — negative controls land in the same commit as the gate)

| Gate | Proves | Named negative control |
|---|---|---|
| Null/unity per node | Neutral settings pass audio through (≤ 1e-6 abs error; bit-exact where stated) | Perturb one internal coefficient/term by 1% → gate fails |
| EQ frequency response | Measured \|H(f)\| matches the closed-form SVF transfer | Detune one band's `g` by 1% → fails |
| Compressor static curve + ballistics | Gain computer and envelope match closed form | Swap attack/release coefficients → ballistics gate fails |
| Delay tap alignment | Impulse lands at exactly `round(t·fs)` samples | Off-by-one read index → fails |
| Reverb RT60 + stability | Decay time within tolerance; silence decays to zero; finite forever | One line gain set > 1 → stability gate fails |
| Limiter ceiling property | No output sample ever exceeds ceiling over adversarial programs | Shrink the lookahead window by 1 sample → an over is detected |
| PDC parallel alignment | Limiter path stays sample-aligned with dry sibling | Zero out the reported `latencySamples` → misalignment detected |
| Block-size independence | Bit-identical output across block schedules | Introduce a block-boundary-dependent state reset → fails |
| Param smoothing contract | Param steps produce bounded sample-to-sample deltas; ramps anchored to event offsets, not block edges | Apply the event at block start instead of its offset → cross-schedule compare fails |
| Persistence round-trip | v7 FX chains + params reopen identical; undo/redo verbs diff-correct | Reject unknown kind / out-of-range normalized / orphaned param rows (each its own failing fixture) |
| Fixture forever-gate | First v7 bundle opens and re-renders identically on HEAD, forever | (The fixture is a golden — never regenerated; see brief §6) |
| Offline == RT with FX | Same graph, same samples | Renderer-input mutation control (H7 pattern) |

## Checkpoints (each independently green + committed; baton-sized)

**CP1 — ParamSpec infrastructure.** `src/engine/ParamSpec.h`: `struct ParamSpec { std::uint32_t id;
const char* name; const char* unit; double min, max, def; ParamMapping mapping; ParamSmoothing
smoothing; }` + `mapNormalized(spec, v01) / unmapToNormalized(spec, real)` with mappings
`Linear`, `Log` (freq-style: `real = min·(max/min)^v`), `Db` (`real_dB = min + v·(max−min)`,
applied as `10^(dB/20)` where the consumer needs linear). Clamp NaN/Inf/out-of-range to finite
(follow `FaderNode::clampGain`'s fold-to-default pattern). New gate `YesDawParamCheck`: mapping
round-trips (`unmap(map(v)) ≈ v`, 1e-12), endpoints exact, hostile-value clamping, monotonicity
property (1000 random pairs). *Negative control:* a deliberately reversed Log mapping fixture
fails round-trip.

**CP2 — Per-Track strip consolidation.** `ProjectMixerProjection` currently emits one strip per
clip, with `combinedGain = clip.gain × track.strip.linearGain` folded onto the per-clip
projection fader — which this checkpoint deletes, so clip gain needs a named new owner (review
finding 3). **Mechanism (normative): `DecodedClipNode` gains a clip-gain factor applied at
render**, exactly like the clip fade envelope it already applies — the same class of
non-destructive, render-time Clip metadata; the Asset is never touched. (Today
`OfflineRenderer`'s source factory hands raw samples to `DecodedClipNode` and clip gain rides
the projection fader — both call sites change together.) Then: group `project.clips` by owning
Track; per Track build `SumNode` over that track's clip sources → (optional sidechain VCA) →
Fader (gain = `track.strip.linearGain` ONLY) → Pan → Meter, ids from a new
`projectMixerNodeIdForTrack(EntityId, Role)` (FNV-32 over the Track entity, same recipe as the
clip allocator). This also pays down ADR-0034's requirement that strips derive from Tracks/Buses,
not Clips. Gates: all existing render/playback/offline gates stay green (parity); new cases in
`YesDawMixerProjectionCheck`: a two-clip track renders sample-equal to the pre-consolidation
reference of the same content; track fader change affects both clips; per-clip gain still
independent; strip/meter identities match the Track/Bus expectations of ADR-0034. *Negative
controls:* re-introducing the double application (clip gain at source AND on a per-clip fader)
fails the two-clip parity case; applying track gain at both the source and the Track fader fails
it too.

**CP3 — FX chain model + schema v7 + undo.** `Project` gains per-Track/Bus
`std::vector<FxInsert> fxChain` in strip state (`FxInsert { EntityId id; FxKind kind; bool
enabled; std::vector<std::pair<std::uint32_t, double>> normalizedParams; }`). The **next free schema version** (v7 at time of
writing — H13 is still open; verify `kCodeSchemaVersion` at kickoff) in `ProjectBundle.h` per the
append-only recipe (new `kSchemaVNSql`, bump `kCodeSchemaVersion`, extend `kMigrations`): `fx_inserts(id BLOB PK, owner_entity BLOB, position INTEGER, kind INTEGER,
enabled INTEGER, UNIQUE(owner_entity, position))` + `fx_insert_params(insert_id BLOB, param_id
INTEGER, value REAL CHECK(value>=0 AND value<=1), PK(insert_id, param_id))`. Undo verbs
`addFxInsert / removeFxInsert / reorderFxInsert / setFxInsertEnabled / setFxInsertParam` through
the `ProjectEditCommand` row-diff pattern. Gates in `YesDawProjectCheck`/`YesDawPersistenceCheck`:
round-trip, migration from a v6 fixture, undo/redo property coverage extended with FX verbs.
*Negative controls:* open-time validators reject unknown `kind`, duplicate `(owner, position)`,
orphaned param rows, out-of-range values — one failing fixture each.

**CP4 — `EqNode`** (`src/engine/nodes/EqNode.h`, gate `YesDawEqCheck`). Spec in Appendix A1.

**CP5 — `CompressorNode`** (gate `YesDawCompressorCheck`). Spec A2.

**CP6 — `FxDelayNode`** (gate `YesDawFxDelayCheck`). Spec A3.

**CP7 — `tailSamples` + `ReverbNode`.** Add `std::int64_t tailSamples = 0;` to `NodeProperties`
(additive; no existing node changes behavior). `OfflineRenderer::buildProjectGraph` extends
`timelineEndFrames` by `totalLatency() + Σ tailSamples` over compiled nodes (checked add, existing
`OutputTooLarge` path). Then `ReverbNode` (gate `YesDawReverbCheck`), spec A4. Tail gate: a
project ending in a reverb renders non-trivial energy after the last clip end and decays below
−60 dBFS before the extended end. *Negative control:* `tailSamples = 0` on the reverb → the
post-clip energy assertion fails.

**CP8 — `LimiterNode` + the PDC clause.** Gate `YesDawLimiterCheck`, spec A5. Plus the resurrected
H3 exit clause in `YesDawGraphCheck` or a new case: two parallel paths from one source — path A
through `LimiterNode` (latency = lookahead), path B dry — summed; an impulse renders as ONE
aligned impulse (PDC splices path B), amplitude exactly the sum. *Negative control:* report
latency 0 from the limiter → double-impulse detected.

**CP9 — Insert-chain wiring + suite integration.** Mixer projection consumes `fxChain` (kind →
node factory, params applied via ParamSpec at build; inserts between chainHead and Fader on
Tracks, before Pan on Buses; disabled inserts still compiled, bypassed via 5 ms crossfade ramp).
Suite gates: offline == RT over a fixture project using all five FX; block-size independence
sweep (schedules forcing 1..9-frame blocks + {64,128,333,512}) bit-identical; RTSan lane covers
every new `process()`; save/reopen the full mix. Commit the **frozen FX-schema fixture bundle** +
forever-gate (risk R3).

**CP10 — Equal-power crossfade + close-out.** Fade law: gains `cos(θ)/sin(θ)`, `θ = (π/2)·t/T`
(replaces linear) in the engine fade path used by `DecodedClipNode`; offline == RT by
construction (shared node). **Golden-update procedure (brief §12):** its own dedicated commit
regenerating only the fade-affected goldens via `bless-goldens`, each listed in the message with
before/after rationale; the crossfade property gate (constant-power: summed energy of a
crossfaded constant signal stays within ±0.1 dB across the fade) lands in the same commit and
must fail against the old linear law. Close-out: roadmap/STATUS notes, `docs/reviews/` follow-up
adversarial pass scheduled.

## Not yet (guardrails — do NOT build in H14)

FX UI of any kind (H16); per-clip FX; compressor sidechain input; oversampling / true-peak
limiting; presets; FX on the Master bus UI surface (the graph supports a Master-bus chain — wiring
its UI is H16); tempo-synced delay UI (store ms; sync resolution is an edit-time concern);
character/analog modeling; automation of FX params (H15 — but the event seam must already work,
proven by the param-smoothing gate).

---

## Appendix A — DSP specifications (implement as written; deviations need a plan amendment)

Shared rules: all processing float32, per-sample state machines, stereo unless stated;
coefficients double where cheap. Allocation only in `prepare(sampleRate, maxBlockSize)`.
`process()` consumes `ParameterChange` events at their `timeInBlock` offsets (FaderNode pattern);
each param change starts a linear ramp of the *real-valued* param over `min(5 ms, remaining)`
samples; recompute filter coefficients every 16 samples during an active ramp.
**Anchoring rule (normative, shared with H15 — review finding 6):** the event's absolute timeline
frame (block start + `timeInBlock`) is the phase origin; recompute instants are
`eventFrame + 16·k` and all ramp positions are absolute-frame-derived, persisting across block
boundaries and PDC-shifted streams until the ramp ends. An implementation that resets any cadence
phase at block boundaries is wrong — the cross-schedule negative control must catch exactly that.
Bypass/enable crossfades dry/wet over 5 ms.

### A1 `EqNode` — 6-band TPT SVF (complete normative equations — review finding 4)

All six band types share one TPT SVF core (Zavalishin/Simper). Per band per channel, state
`ic1eq, ic2eq`; per sample with input `v0`:

```
v3 = v0 − ic2eq
v1 = a1·ic1eq + a2·v3
v2 = ic2eq + a2·ic1eq + a3·v3
ic1eq = 2·v1 − ic1eq
ic2eq = 2·v2 − ic2eq
out = m0·v0 + m1·v1 + m2·v2
```

with `a1 = 1/(1 + g·(g + k))`, `a2 = g·a1`, `a3 = g·a2`. Band coefficients (normative;
`A = 10^(dB/40)`; `f`, `Q` clamped first):

| Type | g | k | m0 | m1 | m2 |
|---|---|---|---|---|---|
| Bell | tan(π·f/fs) | 1/(Q·A) | 1 | k·(A²−1) | 0 |
| LowShelf | tan(π·f/fs)/√A | 1/Q | 1 | k·(A−1) | A²−1 |
| HighShelf | tan(π·f/fs)·√A | 1/Q | A² | k·(1−A)·A | 1−A² |
| HPF | tan(π·f/fs) | 1/Q | 1 | −k | −1 |
| LPF | tan(π·f/fs) | 1/Q | 0 | 0 | 1 |
| Notch | tan(π·f/fs) | 1/Q | 1 | −k | 0 |

Clamp rule (normative, applied BEFORE computing `g` so `tan()` stays finite at every sample
rate): `f ∈ [20, min(20000, 0.49·fs)]`, `Q ∈ [0.1, 18]`, gain ∈ [−24, +24] dB.

**Independent gate reference** (never derive it from the difference equations — that was the H7
copy-the-polynomial sin): the TPT SVF is the exact bilinear transform of the analog SVF
prototype, so the expected complex response at probe frequency `f_p` is

```
s      = j·tan(π·f_p/fs)
D(s)   = s² + k·g·s + g²
H_ref  = m0 + m1·(g·s)/D + m2·(g²)/D
```

per band, cascade = product over bands. Gate: 24 log-spaced probes in [40, 18k];
measured \|H\| (8192-sample impulse → FFT) within ±0.25 dB of \|H_ref\| per band type.
**Identity anchors** (exact; assert on `H_ref` itself to 1e-9 so a transcription error in the
formulas is caught before any audio runs): Bell at `s = j·g` → \|H\| = A²; LowShelf at `s = 0` →
A², at `s → ∞` → 1; HighShelf at `s = 0` → 1, at `s → ∞` → A²; LPF at `s = 0` → 1; HPF at
`s → ∞` → 1; Notch at `s = j·g` → 0.

Params per band (ParamIDs sequential from `band·16`): type (stepped), freq log, gain dB,
Q log. Defaults: all bands Bell at 0 dB — then `A = 1`, `m1 = 0`, so `out = v0` **bit-exactly**
regardless of filter state (assert the null and assert no state-dependent pollution). Hostile
params finite; block-size sweep per the master list. *Negative controls:* per master list, plus
a band at 20 kHz @ 44.1 kHz stays stable (proving the clamp), plus one deliberately mistyped
`m1` sign in a test-local coefficient table must fail the identity anchors.

### A2 `CompressorNode` — feed-forward, log domain

Detector per sample (stereo max of |L|,|R|): `x_dB = 20·log10(max(|x|, 1e-10))`. Branching
envelope: `e += α·(x_dB − e)` with `α_a = 1 − e^(−1/(τ_a·fs))` when rising else `α_r`. Gain
computer (knee W dB, threshold T, ratio R): below `T−W/2` → 0; above `T+W/2` →
`(e−T)·(1−1/R)`; inside → `(1−1/R)·(e−T+W/2)²/(2W)`. Applied gain
`g = 10^((makeup − GR)/20)`, one value per sample, both channels (stereo-linked). Params:
T −60..0 dB, R 1..20 (map top of range to ∞ later — not in alpha), attack 0.1–300 ms log,
release 10–3000 ms log, knee 0–24 dB, makeup 0–24 dB. GR readout exposed like `MeterNode`'s
readback (atomic, control-side poll) for the H16 meter. **Gates:** static curve — steady 1 kHz at
input levels −40..0 step 5 dB, measure settled output, match closed form ±0.5 dB across
3 (T,R,W) combos; ballistics — level step −20→−5 dB, envelope crosses 63% of its delta within
τ_a ± 20% (release symmetric); unity — R=1 → |out−in| ≤ 1e-6; hostile inputs finite.

### A3 `FxDelayNode` — stereo, feedback, damping, ping-pong

Ring buffer per channel sized `⌈2.0 s·fs⌉ + maxBlock`. Params: timeMs L/R 1–2000 log, feedback
0–0.95 (hard internal clamp 0.98), damping cutoff 500 Hz–20 kHz log (one-pole LP in the feedback
path: `s += αd·(x − s)`, `αd = 1 − e^(−2π·fc/fs)`), pingPong bool (stepped), mix 0–1. Write:
`buf[w] = in + fb·damped(read)`; ping-pong crosses feedback L↔R. Integer-sample read for a static
time (`D = round(t·fs)`); on a time-change ramp completion of the *param*, switch via **dual-tap
equal-power crossfade over 20 ms** (old D → new D; no varispeed pitch bend). **Gates:** tap
alignment — impulse, mix 1, fb 0 → single tap at exactly D samples, amplitude 1; feedback decay —
taps at k·D with amplitude fb^k within 1e-4 (damping off); damping — spectral ratio of tap₂/tap₁
matches the one-pole magnitude at two probe frequencies ±0.5 dB; ping-pong — impulse L → first
tap R; time-change — no sample exceeds the crossfade-bounded envelope (no clicks: max
sample-delta bounded); `tailSamples = min(D·⌈ln(10^(−60/20))/ln(fb)⌉, 30·fs)` for fb>0 else D —
asserted against the rendered decay.

### A4 `ReverbNode` — 8-line FDN, Householder

Input: pre-delay line (0–200 ms) → 2 series Schroeder allpasses per channel (g = 0.5, base
lengths {142, 379} samples at 48k, scaled by fs/48000) → mono sum feeds all 8 lines. Line base
lengths at 48k (mutually prime): {1123, 1327, 1523, 1723, 1931, 2129, 2311, 2539}, scaled by
`size` (0.5–2.0) and fs/48000 (rounded, min 32). Feedback matrix Householder
`H = I − (2/8)·J`: `y_i = x_i − (2/8)·Σx` (O(N) via the shared sum). Per-line gain
`g_i = 10^(−3·L_i/(RT60·fs))`; per-line damping one-pole (same form as A3) before the gain.
Output: `L = Σ(+1,−1,+1,−1,…)·line_i`, `R = Σ(+1,+1,−1,−1,…)·line_i`, each ×(1/√8), then
mix dry/wet. Params: preDelay 0–200 ms, rt60 0.1–10 s log, size 0.5–2.0, damping cutoff log, mix.
`tailSamples = min(⌈RT60·fs⌉ + preDelay + maxLine, 30·fs)`. **Gates:** RT60 — impulse response,
Schroeder backward integration, T60 estimate (from the −5..−35 dB fit, extrapolated) within ±20%
at settings 1 s and 3 s; stability — 30 s noise then silence → output monotonically below
−60 dBFS within 1.5×RT60, all samples finite; decorrelation — normalized L/R cross-correlation
peak < 0.9 on the tail; mono sum non-null. *Negative controls:* per master list + damping-off vs
on changes the late-tail spectral tilt in the asserted direction.

### A5 `LimiterNode` — lookahead limiter (one normative algorithm — review finding 5)

Lookahead `D` samples (param 1–10 ms log, default 5 ms; **`latencySamples = D`**,
`tailSamples = D`). Stereo-linked: one gain for both channels. Per-sample pipeline (normative —
the earlier draft's "sliding-window maximum of peaks" formulation and its alternative ramp law
are superseded):

1. **Raw target:** `peak[n] = max(|L[n]|, |R[n]|)`;
   `t_raw[n] = min(1, ceiling / max(peak[n], 1e-10))`.
2. **Release smoothing** (one-pole rise toward 1, instant fall):
   `t_rel[n] = min(t_raw[n], t_rel[n−1] + α_r·(1 − t_rel[n−1]))`,
   `α_r = 1 − exp(−1/(τ_rel·fs))`, release τ 50–1000 ms log.
3. **Sliding-window MINIMUM of target gains** over the next `D` samples:
   `g_min[n] = min{ t_rel[n+w] : w ∈ [0, D) }` — monotonic deque over a ring allocated in
   `prepare()`, O(1) amortized, alloc-free. (This is the ADR's "sliding-window minimum"; there
   is no sliding maximum anywhere in this design.)
4. **Attack smoothing:** boxcar moving average of length `D`:
   `g[n] = (1/D)·Σ_{k=0}^{D−1} g_min[n−k]` — running sum in double precision with an **exact
   rebuild every 4096 samples anchored to absolute frames** (protects the bit-identical
   block-size sweep from accumulator drift).
5. **Output:** `y[n] = x[n−D]·g[n]` per channel (delay line allocated in `prepare()`).

**Ceiling guarantee (provable; the property gate's basis):** for a delayed peak at sample `p`,
every window behind `g_min[p−k]`, `k ∈ [0, D)`, contains `p`, so every averaged term is
`≤ t_rel[p] ≤ ceiling/peak[p]` — hence `|y[p]| ≤ ceiling` exactly. Attack duration = `D` by
construction; the gain trajectory is continuous, so no attack clicks: `|g[n] − g[n−1]| ≤ 1/D`.

Params: ceiling −12..0 dBFS, release, lookahead. GR readback exposed like A2. **Gates:** ceiling
property — 10,000-block randomized program (spikes ×10, DC steps, silence) → **no output sample
exceeds ceiling** (exact sample-peak assertion) and no NaN; transparency — −6 dBFS sine under a
−1 dBFS ceiling → `|out − delayed(in)| ≤ 1e-6`; gain-slope bound — `|g[n] − g[n−1]| ≤ 1/D` over
the adversarial program; latency honesty — reported latency equals measured impulse delay
exactly; PDC alignment (CP8); block-size sweep bit-identical. *Negative controls:* shrink the
stage-3 window by 1 sample → an over is detected; bypass stage 4 (use `g_min` directly) → the
gain-slope bound fails.

### A6 Equal-power crossfade (CP10)

Fade-in gain `sin((π/2)·t/T)`, fade-out `cos((π/2)·t/T)` (t in frames over fade length T),
replacing the linear law in the shared clip-fade path. Constant-power property gate: crossfading
two equal constant signals holds summed RMS within ±0.1 dB across the overlap; the old linear law
fails this (that is the negative control). Golden updates per brief §12.
