# Analytic instrument r5 — reviewed source readiness, calibration UNRUN

Seven new ignored files were authored. All execution is UNRUN: no authored import, Freeze, source generator, compiler, test, analytic calibration, vendor DSP, environment change, native/device, network or Git action occurred. Only read-only standard-library AST/hash/arithmetic source inspection accompanied authoring. All earlier revisions/runs, Accepted ADRs, vendor bytes, goldens, RT annotations and tracked files remain untouched.

R5 preserves R4's actual fixed-coefficient numerical implementation. Root relayed independent numeric acceptance and complete source-cost acceptance, with the latter conditional on Geometry<=64 and Window<=512. This successor adds exactly those compile-time storage assertions, pins MSVC's numbers header, promotes both actual C++ readiness constants and replaces the runner's literal block with an explicit source-literal/manifest acceptance match. R4 remains byte-exact and immutable. No objective, arithmetic algorithm, coefficient, threshold, cap, clock, fixture or control changes.

The exact R5 successor still awaits root review and fresh source acceptance before any execution GO. Root must separately authorize Freeze, review its actual generated outputs/hashes, then authorize one CompileCalibrate invocation with that reviewed bundle hash. Source readiness supplies no calibration, waveform, source-error or production credit; all controls/calibration remain UNRUN.

## What changes mathematically

Every trial obtains one deterministic binary64 coefficient vector beta from midpoint certified chirp-Z RHS and centered analytic Gram intervals, using a finite guarded pivoted3x3 solve. That exact point beta is fixed **before** pruning. Every later direct sample residual uses those same three point values; no new direct RHS scan or second solve changes the objective.

Let A be the exact centered cosine/sine/DC design, and B the deterministic point basis that direct evaluation will regenerate from its cached initial/step coefficients. R5 retains R4's objective of its actual point beta:

    R(beta,B) = E - 2 beta^T b_B + beta^T G_B beta.

It never substitutes E-beta^T*b, which is valid only at an exact normal-equation solution. This is a new approximate-least-squares implementation requiring fresh calibration; no R2 result or bitwise fit-output identity is inherited.

## Centered Gram and cached basis

For omega=2*pi*f/R, q_n=n-(M-1)/2, the exact centered identities are

    C1=sin(M*omega/2)/sin(omega/2), C2=sin(M*omega)/sin(omega),
    Gcc=(M+C2)/2, Gss=(M-C2)/2,
    Gc,DC=C1, Gs,DC=Gcs=0, GDC,DC=M.

Outward intervals enclose every sine, ratio and Gram value. A denominator containing zero refuses the whole case. The actual grid remains the original rounded expression lo+(hi-lo)*i/(T-1), with all trials/endpoints; no expected-pitch relocation or near-DC clamp is added. Unsupported band-edge/precision geometry refuses explicitly.

The cached initial and step values are certified interval-Taylor midpoints. Point regeneration always uses the same explicit four-multiply/two-add complex product. For a unit true step, coefficient errors e0/eStep and gamma8 arithmetic, a nonnegative bound follows by induction:

    lambda=(1+eStep)*(1+gamma8),
    eps_n <= (1+e0)*lambda^n-1 + 8*delta*n*lambda^n.

Exponentiation by squaring evaluates an outward upper before point traversal. The maximum through M is used for every row. There is no costly per-sample interval-error traversal in the direct pass. eps<=2^-20 is a conservative numerical-basis guard, distinct from the old unmeasured analytic source-error premise.

If each sine/cosine component differs from A by at most eps, |A_ni|<=1 gives

    |Delta G_pair| <= M*(2*eps+eps^2),
    |Delta G_DC| <= M*eps,
    |Delta b_pair| <= eps*sum(abs(y)).

DC terms are exact ones. Raw energy/DC sums get separate product/Kahan enclosures. These bounds enclose the exact statistics of the *future computed B values*, not statistics of an assumed ideal direct solver. No measured candidate sample or expected frequency supplies them.

## Least-squares accuracy is checked separately

The computed beta need not solve B's normal equations exactly. Gershgorin interval bounds must establish G_B>=ell*I with ell>M/4. For r_B=b_B-G_B*beta, completing the quadratic gives the exact excess above B's least-squares minimum:

    R(beta,B)-R_min(B) = r_B^T G_B^-1 r_B
                      <= norm(r_B)^2/ell.

R4 bounds every component of r_B and reports its excess upper. Before pruning, every trial must satisfy excess<=2^-32*rawEnergyLower. This strict dyadic numerical-precision criterion was chosen before candidate data. It limits relative objective error far below the analytic calibration's grid-resolution scale; it is not a new pitch/RMS tolerance or a general frequency-error theorem. The unchanged0.25cent/0.01dB analytic calibration remains mandatory, and product5cent/1dB remains unchanged. Failure in any trial aborts the case instead of silently removing that trial.

## Generic compensated-sum enclosure

For nearest binary64 arithmetic, u=2^-53 and delta=denorm_min. R4 uses the supplied generic recurrence, including underflow, rather than assuming textbook Kahan asymptotics. q is rounded outward above1+u; **plain1+u would round to1 and is not used**. Let S bound the computed sum magnitude, C its correction, E its error, and T bound an input term:

    A=q*(T+C)+delta
    ea=u*(T+C)+delta
    en=u*(S+A)+delta
    et=u*(A+en)+delta
    ec=u*(en+et)+delta
    E'=E+C+ea+en
    C'=en+et+ec
    S'=q*(S+A)+delta.

Each nonnegative state is affine in T and delta. Its two coefficients are propagated once for M, with outward scalar operations charged before use. Induction follows the actual code's adjusted=x-correction, next=sum+adjusted, correction=(next-sum)-adjusted. It covers signs, cancellation, zero terms and subnormals without an unexplained relative-error assumption. Product roundoff in y*y and beta*basis, residual subtractions and the final square is bounded separately.

`evaluationError()` propagates uniform product and subtraction errors for the exact source order

    p0=beta0*c; p1=beta1*s;
    d0=y-p0; d1=d0-p1; d2=d1-beta2; term=d2*d2.

It bounds term evaluation error plus the affine Kahan sum error for all M terms. Expanding E-2*beta^T*b_B+beta^T*G_B*beta by that amount encloses the *future computed direct RSS*, including rounding in the last pass. No old solver-to-solver perturbation constant is needed.

## Safe finalist pruning and exact first minimum

Every trial's full computed-RSS enclosure is constructed before selection. With m=min_j U_j, retain exactly every i for which L_i<=m. If i is excluded, some j has r_j<=U_j=m<L_i<=r_i, so i cannot be the minimum. This proves the complete finalist set contains every actual minimum, independently of cancellation or near ties.

All finalists are directly evaluated from every actual sample in increasing index order. Winner updates use strict `<`, preserving the first exact minimum. The winning RSS is always the direct sum, not its algebraic estimate. If all required finalists cannot fit, the entire case returns CHECKER_WORK_REFUSED before any finalist pass. There is no nominal-frequency result, dropped window or unchecked uncomputed-trial guard.

## FFT and chirp-Z error derivation for review

All roots are precomputed by outward interval Taylor arithmetic. The source stores each plan's actual certified maximum root error, plus separate maxima for input chirps, kernel coefficients and packed-seed recovery roots. The2^-37 table admission ceiling does not replace those tighter maxima in error propagation.

Pi is enclosed by adjacent binary64 constants. The new freezer additionally contains an exact rational Machin qualification: pi=16*atan(1/5)-4*atan(1/239), using24/8 alternating terms and their next-term bounds. Its actual execution is UNRUN. The Taylor argument is reduced to an independently checked[-0.8,0.8] interval; quadrant selection is only a hint. Degree25 sine/degree24 cosine remainders are below2^-80 because26!>2^80 and |r|<1. Every basic bound operation expands outward. This is independent of libm sin/cos accuracy.

For one explicit complex product, four rounded real products and two additions give component error gamma2 times the sum of absolute products. Cauchy bounds the two-component error by sqrt(2)*gamma2*|a|*|b| plus underflow;2*gamma3 and8*delta conservatively dominate those terms. A complex addition contributes u*(|a|+|b|)+2*delta. With certified root error e, set

    H=(1+e)*(1+2*gamma3), Q=qUpper*H, qUpper>=1+u.

For two disjoint input subtrees with total magnitude W, assume each subtree's error is<=eta*itsInputL1+alpha. Combining the two branches and rounding their complex addition gives

    eta'=Q*(1+eta)-1,
    alpha'=qUpper*(1+H)*alpha+16*delta,
    |FFT_computed_j-FFT_exact_j| <= eta*sum(abs(input))+alpha.

The local absolute term16*delta exceeds qUpper*8*delta+2*delta, including the product and output-add primitive underflows. These recurrences are evaluated outward using each plan's actual certified root maximum. The real1+u is explicitly rounded upward; a plain binary64 expression would become1.

This includes absolute underflow and zero-magnitude inputs. Permutations are exact. Inverse power-of-two normalization gets its own rounding allowance and2*delta complex underflow term. Packed real recovery separately bounds the two half-transform errors, recovery-root error and its explicit arithmetic. `seedAmplitudeErrorUpper` is diagnostic; global seed and4x dominance still use the actual complete non-DC transform and unchanged mask. Calibration independently checks the known analytic seed, never moves the band to it.

For circular chirp-Z, P>=M+T-1 makes k-n in[-(M-1),T-1] unique moduloP. Exact convolution of actual time arrays differs from the desired chirp arrays by their certified table/recurrence/input-product errors. If a1/a2 and b1/b2 bound time-array l1/l2 norms, forward transform point-error bounds are ea=eta*a1+alpha and eb=eta*b1+alpha. Parseval/Cauchy gives mean product-error contributions

    ea*b2 + eb*a2 + ea*eb.

Mean computed pointwise multiplication gets gamma8*(a2+ea)*(b2+eb)+8*delta. The inverse gets eta times its bounded mean input magnitude plus alpha/P, then division roundoff. Input-chirp deviations, output rotation/table errors and rounded-grid drift are added separately. Output-rotation scale multiplies the existing convolution error; those cross terms are not discarded.

The actual rounded trial f_i may differ from lo+spacing*i. An outward affine interval bounds that difference;2*pi*M*abs(deltaF)/R bounds its centered phase drift. All source/root/phase/multiply/FFT/normalization/underflow terms are retained. Root relayed fresh independent numeric acceptance of these actual local constants/norm transitions. They are source derivations, not empirical constants fitted to calibration or vendor results; the unchanged controls and full calibration remain UNRUN.

## Whole-case phases and reviewed source-cost ledger

One persistent parent budget covers all396 windows of BOTH channels. The driver does not assume two seed grids:

1. Prepay the setup reservation. Construct tables/affine sum map inside that bounded phase.
2. Prepay the complete global-seed phase for every window. Record all actual seeds/dominance outcomes, and check analytic seed containment *after* measurement.
3. Enumerate every actual distinct seed grid. Prepay its complete cache construction plus ranking for all windows. Construct every beta/RSS interval and every finalist set.
4. Count and prepay the total finalist sample work for the entire case before the first direct pass. Evaluate all windows/finalists, then report full results.

Phase subcounters validate their reserved upper bounds; the parent counter never resets across windows/channels. Receipts distinguish charged reserved uppers from actual loop counters. Overflow, excessive cache/payload, unsupported geometry or insufficient work refuses the whole case. No partial rows earn a case PASS.

The independently reviewed upper ledgers in code are:

| Phase | Reserved source units |
|---|---:|
| Tables, kernel transform and affine sum map |400,000,000/case|
| Global packed seed, scans, statistics and error bounds |10,500,000/window/channel|
| Chirp-Z, all trial coefficients/intervals/finalists |9,000,000/window/channel|
| Each actual distinct geometry's basis/Gram cache |2,000,000/grid|
| Direct finalists |40*M+128 each|
| Direct-stage window bookkeeping/results |4096/window|

The unchanged compositional source-count worksheet gives interval point2, up/down1, add6, multiply14, divide28, expansion10, gamma19, angle80 and exactly1422 nested trig counter units. FFT bit reversal is<5N, butterflies12*(N/2)*log2N and inverse normalization2N. Root relayed the complete independent cost review: seed<=10,328,357/window<10.5M; correlation/ranking<8.5M/window<9M; geometry<1,481,000/grid<2M; setup<250M<400M. These are reviewed source-count upper bounds, not measured timings, executed work totals or hardware evidence. The reviewed MSVC vectors allocate the requested capacities; the pre-allocation Grid bound is2,911,504B.

For792 windows with one finalist each and up to8 actual analytic seed grids, those reservations plus input generation and metadata are below2^34. The code still learns the actual grid count and finalist count and checks the cap; it never grants that analytical count as runtime credit. Wider interval overlap or more grids can honestly refuse. No rate whitelist or production budget is changed.

Storage preflight reserves input, worst-case per-window geometry/trial/finalist rows, actual table capacities and separate FFT/convolution/basis/metadata workspace before phase2. BasisSpec<=128, Trial<=64, Geometry<=64 and the local whole-case Window<=512 are compile-time profile checks; the latter two enforce the independent cost review's conditions before allocation. The unchanged example792 windows,8 actual grids and792 finalists has charged source upper16,724,553,984<17,179,869,184 and payload upper53,089,040<67,108,864B. Those dimensions remain an analytical example; actual complete cases must discover grids/finalists and satisfy the original cap. No full case has run. Production2^28/512MiB/1GiB limits are unchanged and receive no credit from checker limits.

## Qualification code and unchanged references

The original independent clock bytes,1125 analytic definitions,45 expected mixture refusals,690 clock references and all existing gates are preserved. The six20-second mono/distinct-stereo cases retain3564 window references. Original data-file hashes must still be compared after a separately authorized Freeze; authoring claims no generated result. The old2^-20 source-error premise remains conditional and UNMEASURED.

Authored controls include small complex/packed DFT and circular chirp defining sums; all-trial RHS/Gram/computed-RSS comparisons on seven declared DC/mixture/cancellation/near-tie/band-edge vectors; complete minimum versus pruned-finalist selection; and a separate equal-objective direct-sample tie prerequisite. Component controls explicitly supplying a test geometry do not qualify global seed behavior; the1125 rows/full cases use the real global FFT. Every control is UNRUN. A missing/unreached control or failed full-case budget remains failure, not a synthetic PASS.

Main requires FE_TONEAREST and `(MXCSR & ~0x3f)==0x1f80` before any floating work, ignoring only sticky exception flags. It does not change controls. The hidden runner retains the pinned compiler, exact argv/hashes, PE-console prerequisite, exclusive one-use bundle and no-retry behavior. Both C++ proof-readiness constants are now true, and the runner requires its explicit source readiness literal to match the reviewed manifest field before consuming a bundle or reaching the compiler. Separate root stage GO and the actual-output reviewed Freeze-bundle hash remain required. This is a source promotion, not a metadata-only bypass. There is no vendor/materializer/renderer/RT/UI/hardware admission by association.

Historical plan/CONTEXT pins remain explicitly bound to root's exact archives. New current ADR0084/plan/CONTEXT are pinned separately; old manifests/proposals are immutable DATA, not silently revalidated against changed live documentation. No architecture document supplies numerical expected values.

## Root-relayed independent review record and exact successor scope

- `/root/critic_quality_r4_numeric`: final ACCEPT of the actual fixed-beta numerical implementation and reviewed successor design; all seven R4 files, six source hashes and55 preserved pins checked; required the MSVC `numbers` provenance pin.
- `/root/critic_quality_r4_cost`: final ACCEPT of the complete source work ledger conditional on `sizeof(qf::Geometry)<=64` and `sizeof(Window)<=512`; both assertions are added in R5, with the local Window assertion immediately before its vector allocation. Source-count upper figures are recorded above and in the manifest.

These findings were relayed by root; this author did not execute new numeric or performance measurements. R4's manifest102E2226E561C738E7DD24E6D0044A0003FA883DF10913B48F11357E383FE6D9 and all six R4 source hashes are preserved as historical pins. The added pinned MSVC14.44.35207 `include/numbers` SHA256 is53AF2276596D45F8A9BFA8C6BB5FEE451AC3F57BB32F3C40F74C527B8BEEA5C1.

The generated metadata filename `whole-case-source-hold.json` remains unchanged for exact bundle-path continuity; its authored R5 contents now state accepted source readiness and calibration UNRUN. No generated file exists yet. The original1125 analytic definitions/45 expected refusals/690 clocks and all six full-case definitions/3564 window references remain unchanged. Freeze and every calibration stage are UNRUN.
