# Musical waveform quality: source-only preparation

Status: UNRUN. No oracle, candidate, generator, compiler, test, DSP, app, device or hardware execution belongs to this packet. It grants no implementation or renderer-admission GO. Only this new ignored directory was written. The source pins are in `source-pins.json`; outstanding work and receipts are in `proof-plan.json`.

## Contract and evidence boundaries

Accepted ADR-0083 Decisions 2--5 and plan step 4 govern. The existing scalar `TimeStretch.h` and `time_stretch_tests.cpp` retain their fingerprints, rounding and goldens unchanged. Their 8192-Frame mixed-tone/impulse fixtures, absolute reads and scalar graph/schedule equality are useful regression coverage, not the required four-second varying-map signal proof. The older ignored contract draft corroborates the origin of these targets but is not normative.

The quality checker must consume actual retained unfaded PCM, original input samples, immutable clock/map/source-window identities, queried W/H/latencies, and independently observed physical/read receipts. It must not take expected pitch, RMS, landmark location, envelope, latency or center error from a future renderer. Captured unfaded PCM is allowed only as the measured operand of the one-time-fade and same-owner-read properties; it is not an expected waveform for quality.

Do not modify the dependency, scalar wrapper, goldens, Accepted ADRs, RT annotations or thresholds. No output normalization, dither, cropping, padding, backend substitution, rate whitelist, quiet masking or wider tolerance is proposed. A refused or unreached signal fixture is not a waveform PASS. Current state/context/resource admission remains separate and pending; this packet does not duplicate the private context ledger or require recurrent phase ancestry to have one source timestamp.

## Fixtures frozen before future candidate execution

Use project/view rates R = 44100, 48000, 96000, channels 1 and 2; P = 15360 Ticks/quarter from `Time.h`. Select N = 10*R Frames, S = 3*P+1, L = 10*P. A selected window starts at a nonzero Asset/view offset; unrelated prefix/suffix sentinels prove that reversal selects this window, not the whole Asset. Generate only this analytically defined source window for signal metrics.

The five constant local ratios r are 0.5, 0.75, 1, 1.5, 2, represented by constant BPM 60/r. Four further maps span the supported envelope: linear BPM 120->30, linear BPM 30->120, and constant jumps 120->30 and 30->120 at native J = S+4*P+1. Ramps start at S and finish at S+L. Prefix BPM is 90 from Tick 0 to S, with an explicit change at S. A final change at S+L terminates the ramp; preserve the complete map. Source location of J is 4*R+R/P, fractional at every listed rate (R/P = 2.87109375, 3.125, 6.25). The continuous local ratio is 60/BPM: endpoints are in [0.5,2]; these maps do not extend the supported envelope.

Tone source: float samples obtained once from x[n] = 0.2*sin(2*pi*f*n/R + phi), f = 220 or 997 Hz, with a separately generated analytic reference. Ten seconds gives at least five output seconds even at ratio 0.5 and leaves over four seconds of eligible analysis after the declared edges. Assign phase 0 to mono, pi/7 to identical stereo, and phases 0/pi/7 to the two channels of distinct-tone stereo; these are assignments, not another matrix dimension. Calibration uses the larger phase set below. Input peak is measured from the actual frozen float source and must be <=0.25. Freeze source hashes before calling any candidate.

Core matrix: 3 rates * 9 maps * 2 frequencies * 2 layouts = 108 tone preparations (mono and bit-identical stereo). Add 27 stereo 220/997 channel-distinct preparations, checked channel by channel, to expose channel confusion. No stereo averaging can conceal a bad channel. Zero-input matrix: 3*9*2 = 54 preparations. Burst matrix: 3*9*2 = 54 preparations. Each row is sequential; retain only current-case buffers and compact receipts, not an in-memory batch of all PCM.

Reverse/fade variants reuse these frozen laws but are additional, separately named preparations. The 135+54+54 rows do not pretend to count those future variants or existing scheduling/context controls. Three rates and nine maps are representative fixtures, never an API whitelist or universal quality theorem. Representative low/high/configuration-boundary cases remain required for broader claims.

## Two independent clock roles

1. **Normative Numeric A cuts.** Before DSP, independently construct exact rational S+L*i/N using checked integers, enclosing adjacent binary64 fractions without independently converting numerator and denominator to double. Evaluate the pinned computed-clock operation order at both endpoints; require ordered finite results, <=1/1024-Frame width and one common floor. Integral queries take the exact whole-Tick path. Freeze A=floor(F(S)), B=floor(F(S+L)), D=B-A and Q(i)=floor(F(S+L*i/N))-A. No epsilon snapping or floor of a real-log reference replaces these cuts. A standalone port of the pinned clock/bracket operation order must be reviewed against the already pinned `Time.h`/window contract; calling the future renderer/planner for expected Q is forbidden.
2. **Analytic clock calibration.** Independently integrate each constant segment as (60*R/P)*deltaTick/BPM. For a ramp b(t)=b0+k*(t-a), use (60*R/P)/k*log1p(k*deltaTick/b0), with its complete cumulative prefix. Invert with deltaTick=(b0/k)*expm1(deltaFrame*P*k/(60*R)); constants use deltaTick=deltaFrame*P*BPM/(60*R). Evaluate with independently implemented high precision and freeze reference values. Require <=1/1024 Frame absolute error/inverse-forward residual in these fixtures. Agreement of two decimal precisions is a numerical cross-check, not a certified real-analytic floor proof. Numeric B is not selected.

Freeze expected burst centers Q(p), reverse centers Q(N-1-p), endpoint A/B/D, jump's exact fractional source position, and native fade landmarks before candidate invocation. Record both computed-clock cuts and real-valued analytic references under distinct field names. They must never be interchanged to make a near-cut case pass.

## Independent estimator proposal: finite-window sinusoid fit

Use every unmodified sample in each eligible rectangular 250 ms window: M=R/4, starts every Hq=R/20 (50 ms). These are integers at all three listed rates. Fit

    y[n] = a*cos(2*pi*f*q[n]/R) + b*sin(2*pi*f*q[n]/R) + c,
    q[n] = n - (M-1)/2.

For each trial frequency, solve the 3-column least-squares system with pivot/conditioning checks and compensated sums. Recompute the residual directly rather than subtracting two nearly equal total energies. The independent implementation uses elementary sine/cosine and its own small matrix solve; no stretcher, vendor FFT/phase estimate, zero-cross counter or scalar wrapper participates. Fitting both quadratures removes unknown window phase; the DC column prevents a constant offset from becoming a frequency estimate. Do not subtract c before the level/peak gates: actual DC energy still counts.

Concrete bounded search proposal: implement a test-only radix-2 DFT/FFT from the defining sum, independently check it against a direct DFT on small analytic vectors, and zero-pad to K=nextPowerOfTwo(4*M). Find the largest non-DC spectral bin without constraining it to the expected pitch. Fit a uniform <=0.02 Hz grid covering +/-2*R/K about that bin, including both interval endpoints. K>=R makes the band <=4 Hz wide, at most 203 trials. Choose the least-residual trial; report 1200*log2(fhat/fInput). The grid's half-step is <=0.01 Hz, about 0.079 cent at 220 Hz. It does not itself prove the spectral seed is correct: phase/frequency/off-bin/mixture calibration and an independent seed-localization review are still mandatory before this estimator can gate a vendor candidate. A boundary winner, singular/nonfinite fit, absent tone or unqualified/ambiguous seed yields ESTIMATOR_UNPROVEN/FAIL, never a nominal-frequency result. Do not silently move the search band to the expected pitch.

The proposal avoids a nonlinear minimizer's unstated unimodality assumption, phase-biased zero crossings and parabolic FFT interpolation as the final pitch estimate. It is a concrete design, not yet an implemented or certified estimator. Calibration may identify a supported correction; thresholds stay fixed.

For raw RMS use sqrt(sum(y[n]^2)/M), with compensated energy accumulation. The level reference is the whole frozen ten-second input-tone RMS; ten seconds contains an integer number of cycles at both nominal frequencies. This reference is independent of the map and output phase. Compare raw output-window RMS directly: abs(20*log10(RMSout/RMSinput)) <=1 dB. Do not replace raw RMS by fitted amplitude, remove DC, or widen 1 dB for leakage.

Calibrate raw RMS against the exact finite-window identity for an unrounded analytic sine:

    mean(x^2) = amplitude^2/2 *
      [1 - cos(2*phi+(M-1)*omega)*sin(M*omega)/(M*sin(omega))],
    omega = 2*pi*f/R.

Use the appropriate window-start phase and separately bound/measure the frozen float-sample rounding contribution. This identity accounts for phase leakage instead of asserting every 250 ms window has RMS amplitude/sqrt(2). At nominal 220 Hz the 250 ms window contains 55 cycles; at 997 Hz it contains 249.25 cycles. The latter's finite-window phase variation is bounded by abs(sin(M*omega)/(M*sin(omega))) in energy (about 0.00064), small but nonzero. These are analysis facts, not tolerance changes.

## Calibration must finish first

No vendor candidate may be evaluated until the same independent checker passes analytic inputs at all three rates, both frequencies, phases {0,pi/7,pi/2,pi,11*pi/6}, starts on and between 50 ms grid positions, and known offsets {-6,-5,0,+5,+6} cents. Use known amplitudes at 0 and +/-1.1 dB relative to 0.2, still peak<=0.25. Expected f/amplitude/finite-window RMS are fixed analytically, never estimated by the candidate under test. Require <=0.25 cent frequency error and <=0.01 dB RMS error on every calibration row, not averages. The +/-5-cent rows establish resolution at the actual boundary; no calibration tolerance is added to the eventual 5-cent gate.

Calibration also includes fractional-bin phases, deterministic DC offsets, and known competing-tone vectors to qualify seed localization or force ESTIMATOR_UNPROVEN rather than a false nominal result. Competing-tone/DC vectors qualify the measuring instrument; they do not change product signal thresholds or assert universal pitch estimation. Freeze their analytic definitions, expected dominant tone and finite-window RMS before execution. Their exact amplitude table and seed-localization argument are an explicit unresolved implementation detail; no result exists in this packet.

Required immutable calibration receipt: checker source hash, analytic fixture definition/hash, rate/window/start/phase/known f/known RMS, measured f/RMS, error, conditioning/search extent/seed information, every status and process exit code. A failure blocks candidate waveform certification. No stale or unrelated calibration receipt can qualify a changed checker.

## Edge policy and whole-output gates

Query actual configured W, H, inputLatency and outputLatency after safe configuration, and compare them to the checked predicted configuration. The pinned default preset requests sampleRate*0.12 and sampleRate*0.03; use actual queried integers, not an assumed duration in the checker.

Freeze one source-edge interval [W,N-W] and one Clip-edge interval [W,D-W]. An eligible tone window must lie wholly in their intersection after independent Q mapping: [max(W,Q(W)), min(D-W,Q(N-W))]. This takes the union of the two edge exclusions; it does not add them or exclude interior Tempo changes. List every 50 ms window and its exact eligibility reason before running candidate PCM. Never omit a ramp/jump interior, bad estimate, low-energy window or failed row. Empty/insufficient coverage is fixture/proof failure, not PASS.

Each eligible window/channel must satisfy <=5 cents and <=1 dB. All output Frames, including edge regions, must be finite and obey peak <=1.25*actualInputPeak+1e-6. Fades are disabled during pitch/RMS quality measurement; fade correctness is measured separately. Exact-zero input must produce numeric exact zeros throughout retained PCM and outside [A,B); identical input stereo must produce sample-for-sample identical channels, without an epsilon. Record the IEEE bit patterns as diagnostics, including signed-zero handling, while the gate is numeric exact zero/equality. Exact-zero/silence/resume state proof is still separately required.

## Transient source, mapping and measurement

At each rate let Jb=floor(R/2000). The support k=-Jb..Jb spans <=1 ms. Define deterministic q[k] for positive k by this uint32 arithmetic (all products modulo 2^32): v=k*0x9e3779b9; v^=v>>16; v*=0x85ebca6b; v^=v>>13; v*=0xc2b2ae35; v^=v>>16; q[k]=+1 if v&1 else -1. Set q[-k]=q[k] and q[0]=-2*sum(q[k],k=1..Jb). Choose the smallest nonnegative integer m with max(abs(q))*2^-m<=0.25, then define source burst b[k]=q[k]*2^-m. This is a fixture amplitude definition, not normalization of candidate PCM.

These small integer/dyadic samples are exactly representable in float. The complete burst has exact zero DC, symmetric energy and exact source energy centroid p. Its nonzero finite-support trigonometric polynomial is broadband rather than a sustained tone; no flat-spectrum claim is made. Freeze and check the actual sample sum, symmetry, peak and analytic centroid before DSP. Centers are p={2R,4R,6R,8R}; verify their source and independently mapped output separation is >=4*queriedW and that every +/-2W output analysis region lies wholly inside [0,D). A violated fixture constraint fails construction, rather than clipping a region.

For each independently expected center e=Q(p), accumulate E=sum(y[j]^2), mu=sum(j*y[j]^2)/E over j in [e-2W,e+2W] (document endpoint convention once). Require E>0, finite sums and abs(mu-e)/R<=0.005. Work per channel, including stereo identity. No peak picking, fitted time shift, local crop, removal of late energy or candidate-reported center may replace this measurement. Reversal uses e=Q(N-1-p), computed before DSP. If the quiet/silence/context laws refuse the burst fixture, record NOT_REACHED/REFUSED; this is not transient or reverse credit.

## Reverse, native fractional fades and live gain

Apply reversal to only the selected input window: xReversed[i]=xSelected[N-1-i], then run the varying map. Reverse-after-stretch generally predicts D-1-Q(p), a different landmark from Q(N-1-p). The asymmetric jump map and p=2R provide a concrete discriminating witness: before rounding their locations differ by approximately 3 seconds, far beyond 5 ms. Freeze exact Numeric A locations and confirm the >10 ms separation before using this future control; do not infer it from DSP output. A constant affine map is not an adequate reverse-order witness.

For each output j, independently obtain native u=inverseF(A+j)-S using a separately implemented, source-pinned inverse and require the <=1/1024-Frame forward residual. Only the single leading floor-quantized sample less than one Frame before S may use u=0. Other invalid inverses refuse. Never round u to an integer Tick. The independent high-precision analytic inverse is a calibration reference; it is not silently substituted for the computed-clock law.

Reimplement the existing formulas directly, without calling `ClipEnvelope.h`: c=clamp(curve,-1,1), z=clamp(progress,0,1), h=z^(2^(-2*c)); shapes are h, sin(pi*h/2), h*h*(3-2*h), and (1-exp(-4*h))/(1-exp(-4)) for Linear, EqualPower, SCurve, Log. Fade-in is 1 for fi<=0 or u>=fi, otherwise shape(u/fi). Fade-out is 1 for fo<=0 or u<max(0,L-fo), otherwise shape(1-(u-max(0,L-fo))/fo). The envelope is min(in,out), including overlaps and legal overlong fades; do not replace it with a product or (L-u)/fo for overlong fades. Match the declared float metadata and documented final float conversion; compute the reference formula independently at higher precision.

Require e in [0,1], baked[j] vs capturedUnfaded[j]*e error <=2e-6*max(1,abs(capturedUnfaded[j])), and exact zero outside [A,B). Cover all four shapes, curves {-1,0,1}, no fade, overlapping fi=fo=3L/4, and overlong fi=2L+1/fo=L/2. First validate the fractional evaluator directly at externally specified native fractions; then validate the inverse plus bake path. Track any analytic-vs-computed inverse difference separately; no tolerance extension follows from it.

The prepared schedule must reference the same owner, offset0/countD/startA, reversed=false, fades0; Clip.gain is live. Mechanical reads through blocks 1/7/128/257, absolute offsets, interior ramp seek and loops must be sample equal to that same PCM. Changing gain must leave the prepared owner/sample hash unchanged and alter only live multiplication, verified in both real schedule builders when those seams exist. These same-owner properties do not prove pitch quality.

## Actual future negative controls: each owns a gate

All controls below are UNRUN. Run only after an unmutated reached baseline passes; retain the real mutated wrapper/sample execution and unchanged checker. Mutating a receipt or expected number is not a control.

| Control | Concrete future intervention | Unchanged gate that must reject it |
|---|---|---|
| pitch+6cent | Actual private wrapper pitch parameter/sample path produces a steady f*2^(6/1200), holding level/map fixed; an analytic +6-cent checker self-control is separate | <=5-cent per-window frequency gate; calibrated <=0.25-cent instrument must reach the mutant |
| level+1.1dB | Actual PCM written by the private wrapper is multiplied by 10^(1.1/20), without changing expected source level | <=1-dB raw RMS gate; peak failure alone is not level-control credit |
| doubleFade | Actual wrapper multiplies the independently reached nontrivial native envelope twice | One-time fade sample-error gate; witness requires unfaded magnitude and 0<e<1 giving error above its fixed bound |
| reverseAfter | Actual private wrapper stretches selected forward samples then reverses its retained PCM | Independently mapped jump/burst landmark <=5-ms gate; no signal/path reached means no control credit |
| latency+1Frame | Actual extraction reads physical outputLatency+j+1 instead of outputLatency+j for D retained Frames | Exact physical extraction/read-index gate (zero disagreement), not the 5-ms centroid gate |
| center3Frame | Actual wrapper physical extraction-to-Project origin is translated by signed 3 Frames, affecting actual contributing samples/center locations | <=2-Frame actual-center gate; sign is chosen by a frozen rule to increase the measured baseline signed residual, not by editing expected Q/records |
| omitOutputLatency | Actual extraction begins one complete queried outputLatency too early | Exact latency/read gate and, once reached, the burst centroid <=5-ms gate; report each result separately |

For the three-Frame case, select a reached baseline contributing interior center, independently compute its signed residual d=y-Q(x), and inject +3 if d>=0 else -3 in the wrapper's physical origin. The actual mutant must retain evidence that this real extraction translation changed audible sample locations. Its residual then has magnitude abs(d)+3>2; changing only observer coordinates earns nothing. If the available seam cannot apply this physical mutation without changing the vendor, leave the control pending and prepare another wrapper-level witness. Vendor bytes stay immutable even for controls.

Additional ADR controls (endpoint-average timing, ignored fractional jump, actual source/history index error, silent advancement) belong to the scheduling/state preparation packet and remain separately required. This packet does not claim them or suppress their outcomes. A baseline refusal, unrelated earlier failure or dead mutant is NOT_REACHED, never the claimed own-gate FAIL.

## Bounded future work and remaining proof

Production preparation retains the ADR limits: <=2^20 process calls, <=2^28 actual preparation work units per Clip, <=512 MiB new reserved payload per Clip, <=1 GiB pending batch and <=1 GiB reusable Musical cache. Complete DSP/FFT/ring/view/working-plane/PCM/peaks/overlapping-owner accounting is independent outstanding proof, not supplied by this checker design.

The test-only checker has its own explicit budget, separate from and unable to enlarge production caps: one case at a time; at most two channels, 10 seconds input, 20 seconds output in the base matrix; <=64 MiB new checker scratch excluding already accounted candidate/input owners; <=2^34 counted scalar sample-term/FFT-butterfly/clock-reference operations per case, with elementary-library calls counted and reported separately. Preflight products, FFT capacity, <=203 fit trials/window, window count, calibration rows and burst/fade work before allocation; stop/refuse before exhausting a budget. No cap is a wall-time or total-RSS promise. A broader matrix or rate needs a new checked estimate, not bigger silent limits.

Future order: (1) finish the independent clock/estimator/seed-localization design and fixture table, (2) implement and calibrate the checker without vendor calls, (3) independently review the measuring instrument and freezes, (4) admit the real state/context/configuration/resource path or record its precise refusal, (5) run reached baseline PCM and actual controls, (6) integrate same-owner/fade/live-gain checks, (7) root-owned relevant build/suites/CI and exact-source evidence. Every executable stage remains UNRUN. No timing, waveform, allocation, RT, CI, product or hardware certificate is issued here.
