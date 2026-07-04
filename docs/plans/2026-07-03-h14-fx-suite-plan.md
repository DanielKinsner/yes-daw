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
offline Render == RT with a full FX mix; FX state save/reopen through schema v7; the frozen
FX-era fixture bundle forever-gate; RTSan clean on every new `process()`.

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
clip. Change: group `project.clips` by owning Track; per Track build `SumNode` over that track's
clip sources (clip gain stays applied at each source; **remove** the `combinedGain = clip.gain ×
track.strip.linearGain` fold — track gain now lives only on the track's single `FaderNode`), then
(optional sidechain VCA) → Fader → Pan → Meter, ids from a new `projectMixerNodeIdForTrack(EntityId,
Role)` (FNV-32 over the Track entity, same recipe as the clip allocator). Gates: all existing
render/playback/offline gates stay green (parity); new cases in `YesDawMixerProjectionCheck`: a
two-clip track renders identically to the same content pre-consolidation reference; track fader
change affects both clips; per-clip gain still independent. *Negative control:* re-introducing the
double-fold (clip×track at source AND fader) fails the two-clip parity case.

**CP3 — FX chain model + schema v7 + undo.** `Project` gains per-Track/Bus
`std::vector<FxInsert> fxChain` in strip state (`FxInsert { EntityId id; FxKind kind; bool
enabled; std::vector<std::pair<std::uint32_t, double>> normalizedParams; }`). Schema v7 in
`ProjectBundle.h` per the append-only recipe (new `kSchemaV7Sql`, bump `kCodeSchemaVersion`,
extend `kMigrations`): `fx_inserts(id BLOB PK, owner_entity BLOB, position INTEGER, kind INTEGER,
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
every new `process()`; save/reopen the full mix. Commit the **frozen v7 fixture bundle** +
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
samples; recompute filter coefficients every 16 samples during an active ramp, anchored to the
event offset (block-size independent by construction). Bypass/enable crossfades dry/wet over 5 ms.

### A1 `EqNode` — 6-band TPT SVF

Per band per channel, the Zavalishin/cytomic TPT SVF: `g = tan(π·f/fs)`, `k` per type below,
`a1 = 1/(1 + g(g+k))`, `a2 = g·a1`, `a3 = g·a2`. Per sample: `v3 = in − ic2; v1 = a1·ic1 + a2·v3;
v2 = ic2 + a2·ic1 + a3·v3; ic1 = 2v1 − ic1; ic2 = 2v2 − ic2`. With `A = 10^(dB/40)`:
Bell `k = 1/(Q·A)`, `out = in + k·(A²−1)·v1`; LowShelf `g = tan(π·f/fs)/√A`, `k = 1/Q`,
`out = in + (A−1)·k·v1 + (A²−1)·v2`… use the cytomic *SVF cookbook* forms exactly (HPF:
`out = in − k·v1 − v2`; LPF: `out = v2`; Notch: `out = in − k·v1`). Params per band (ParamIDs
sequential from `band·16`): type (stepped), freq 20–20k log, gain ±24 dB, Q 0.1–18 log. Defaults:
all bands Bell, 0 dB (= null). **Gates:** all-flat null bit-exact when every gain is 0 dB and
types are Bell (the `in +` terms vanish); response: 8192-sample impulse → FFT → \|H\| within
±0.25 dB of the closed-form `H(z)` at 24 log-spaced frequencies in [40, 18k], per band type;
hostile params finite; block-size sweep. *Negative controls:* per master list + a band at
Nyquist-adjacent freq (20 kHz @ 44.1k) stays stable (`g` clamp: `f ≤ 0.49·fs`).

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

### A5 `LimiterNode` — lookahead sliding-minimum

Lookahead D = 5 ms default (param 1–10 ms; **`latencySamples = D`**, `tailSamples = D`). Delay
line D per channel. Gain target per sample: `t[n] = min(1, ceiling / peak[n])` where
`peak[n] = max over window [n, n+D) of max(|L|,|R|)` — maintained by a monotonic-deque
sliding-window maximum over a fixed ring (allocated in `prepare`, O(1) amortized, alloc-free).
Smoothed gain: attack — since the window looks ahead D samples, ramp `g` linearly toward `t[n]`
so it arrives no later than the peak does (`g -= max((g − t)/samplesUntilPeak, minStep)`;
simplest correct form: `g = min(g, t[n])` then release); release — one-pole rise
`g += αr·(1 − g)` (release 50–1000 ms log). Output `y = delayed(x)·g`. Params: ceiling
−12..0 dBFS, release, lookahead. **Gates:** ceiling property — 10,000-block randomized program
(spikes ×10, DC steps, silence) → **no output sample exceeds ceiling** (exact sample-peak
assertion) and no NaN; transparency — −6 dBFS sine under −1 dBFS ceiling → |out−delayed(in)| ≤
1e-6; latency honesty — reported latency equals measured impulse delay exactly; PDC alignment
(CP8); GR readback exposed like A2.

### A6 Equal-power crossfade (CP10)

Fade-in gain `sin((π/2)·t/T)`, fade-out `cos((π/2)·t/T)` (t in frames over fade length T),
replacing the linear law in the shared clip-fade path. Constant-power property gate: crossfading
two equal constant signals holds summed RMS within ±0.1 dB across the overlap; the old linear law
fails this (that is the negative control). Golden updates per brief §12.
