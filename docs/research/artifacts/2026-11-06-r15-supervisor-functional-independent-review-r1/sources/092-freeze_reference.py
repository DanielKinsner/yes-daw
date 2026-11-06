"""Independent analytic freezer; authoring this file does not run it.

No C++/engine/vendor import or invocation. Numeric A floors deliberately are NOT
computed with Python's different libm. Source-conditional bounds are not measured
source-error certificates, and Decimal agreement is not a real-floor theorem.
"""
import argparse
import csv
import hashlib
import json
import math
from decimal import Decimal, localcontext
from fractions import Fraction
from pathlib import Path

P = 15360
S = 3 * P + 1
L = 10 * P
J = S + 4 * P + 1
RATES = (44100, 48000, 96000)
PI_TEXT = "3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117067982148086513282306647"
WORK_CAP = 1 << 34
SCRATCH_CAP = 64 << 20
SOURCE_ERROR_BOUND = 2.0 ** -20
FIELDS = ("id", "group", "rate", "start", "frames", "amplitude1", "frequency1",
          "phase1", "amplitude2", "frequency2", "phase2", "dc", "expected_status",
          "expected_frequency", "expected_rms", "geometry_id")
COUNTERS = {"bound_calls": 0, "decimal_series_terms": 0,
            "decimal_library_calls": 0, "work_units": 0}


def charge(units):
    if units > WORK_CAP - COUNTERS["work_units"]:
        raise RuntimeError("FREEZER_WORK_REFUSED")
    COUNTERS["work_units"] += units


def exact(value):
    """Exact rational value of a frozen binary64 parameter, represented in Decimal."""
    n, d = float(value).as_integer_ratio()
    return Decimal(n) / Decimal(d)


def dsin(x):
    pi = Decimal(PI_TEXT)
    x = x % (2 * pi)
    if x > pi:
        x -= 2 * pi
    if x < -pi:
        x += 2 * pi
    term = x
    total = x
    for k in range(1, 201):
        charge(100)
        COUNTERS["decimal_series_terms"] += 1
        term = -term * x * x / Decimal((2 * k) * (2 * k + 1))
        total += term
        if abs(term) < Decimal("1e-112"):
            return total
    raise RuntimeError("DECIMAL_SINE_SERIES_REFUSED")


def dcos(x):
    return dsin(x + Decimal(PI_TEXT) / 2)


def mean_cos(theta, phase, count):
    if theta == 0:
        return dcos(phase)
    return (dsin(Decimal(count) * theta / 2) / dsin(theta / 2)
            * dcos(phase + Decimal(count - 1) * theta / 2) / Decimal(count))


def mean_sin(theta, phase, count):
    return (dsin(Decimal(count) * theta / 2) / dsin(theta / 2)
            * dsin(phase + Decimal(count - 1) * theta / 2) / Decimal(count))


def analytic_rms(row):
    """Finite-window RMS including DC, tone means and every mixture cross term."""
    with localcontext() as ctx:
        ctx.prec = 120
        rate, start, count = row["rate"], row["start"], row["frames"]
        dc = exact(row["dc"])
        terms = []
        for suffix in ("1", "2"):
            amplitude = exact(row["amplitude" + suffix])
            if amplitude == 0:
                continue
            omega = 2 * Decimal(PI_TEXT) * exact(row["frequency" + suffix]) / Decimal(rate)
            phase = exact(row["phase" + suffix]) + omega * Decimal(start)
            terms.append((amplitude, omega, phase))
        energy = dc * dc
        for amplitude, omega, phase in terms:
            energy += 2 * dc * amplitude * mean_sin(omega, phase, count)
            energy += amplitude * amplitude * (1 - mean_cos(2 * omega, 2 * phase, count)) / 2
        for i, (ai, wi, pi) in enumerate(terms):
            for aj, wj, pj in terms[i + 1:]:
                energy += ai * aj * (mean_cos(wi - wj, pi - pj, count)
                                     - mean_cos(wi + wj, pi + pj, count))
        if energy <= 0:
            raise RuntimeError("ANALYTIC_RMS_DOMAIN")
        COUNTERS["decimal_library_calls"] += 1
        charge(1)
        return str(energy.sqrt())


def up(value):
    return math.nextafter(value, math.inf)


def down(value):
    return math.nextafter(value, -math.inf)


def dirichlet_upper(frequency, nu, rate, count):
    """Conditional outward-rounded bound on |sum exp(i*2*pi*(f-nu)*n/R)|.

    The finite sum is <=min(M,1/|sin(pi*(f-nu)/R)|). Fold to [0,pi/2]
    and use sin(z)>=z-z^3/6. No FFT or candidate observations are used.
    Basic binary64 arithmetic is bracketed with nextafter; finite fixture
    parameters keep every operation normal and away from overflow.
    """
    COUNTERS["bound_calls"] += 1
    charge(128)
    lo, hi = down(frequency - nu), up(frequency - nu)
    alo = 0.0 if lo <= 0.0 <= hi else min(abs(lo), abs(hi))
    ahi = max(abs(lo), abs(hi))
    # |f-nu| < R in these fixtures. Distance to the nearest multiple of R.
    folded_lo = max(0.0, min(alo, down(rate - ahi)))
    folded_hi = rate / 2 if alo <= rate / 2 <= ahi else min(ahi, up(rate - alo))
    zlo = down(down(math.pi) * down(folded_lo / rate))
    zhi = up(up(math.pi) * up(folded_hi / rate))
    cubic_hi = up(up(zhi * zhi) * zhi)
    denominator_lo = down(zlo - up(cubic_hi / 6.0))
    if denominator_lo <= 0:
        return float(count)
    return min(float(count), up(1.0 / denominator_lo))


def component_upper(amplitude, frequency, nu, rate, count):
    a = abs(amplitude) * 0.5
    return up(up(a * dirichlet_upper(frequency, nu, rate, count))
              + up(a * dirichlet_upper(-frequency, nu, rate, count)))


def geometric_qualification(rate, components, dc, expected_status):
    """Stream all non-DC FFT bins; cache only this compact geometry receipt.

    Conditional on |actual float sample - analytic sample|<=2^-20, an M*e
    triangle bound covers source perturbation. Calibration will independently
    check actual global seed-band containment and actual 4x power dominance.
    This conditional premise is explicitly not claimed as measured here.
    """
    count = rate // 4
    fft_size = 1 << (4 * count - 1).bit_length()
    scratch_bound = 16 * fft_size + 4 * count + (1 << 20)
    if scratch_bound > SCRATCH_CAP:
        raise RuntimeError("FREEZER_SCRATCH_REFUSED")
    seed_radius = 2.0 * rate / fft_size
    mask_radius = 2.0 * rate / count
    perturbation_hi = up(count * SOURCE_ERROR_BOUND)
    nearest_lowers = []
    for amplitude, frequency in components:
        nearest = max(1, min(fft_size // 2, int(math.floor(frequency * fft_size / rate + 0.5))))
        nu = nearest * rate / fft_size  # exact: integer product, power-of-two denominator
        delta_hi = up(abs(frequency - nu))
        zhi = up(up(count * up(math.pi)) * up(delta_hi / rate))
        positive_lower = down(count * down(1.0 - up(up(zhi * zhi) / 6.0)))
        lower = down(abs(amplitude) * 0.5 * positive_lower)
        lower = down(lower - up(abs(amplitude) * 0.5
                    * dirichlet_upper(-frequency, nu, rate, count)))
        for other_amplitude, other_frequency in components:
            if other_frequency != frequency:
                lower = down(lower - component_upper(other_amplitude, other_frequency, nu, rate, count))
        lower = down(lower - up(abs(dc) * dirichlet_upper(0.0, nu, rate, count)))
        nearest_lowers.append(max(0.0, down(lower - perturbation_hi)))
    maximum_outside_seed = 0.0
    maximum_outside_inner_mask = 0.0
    global_upper = 0.0
    for k in range(1, fft_size // 2 + 1):
        charge(16)
        nu = k * rate / fft_size
        upper = perturbation_hi
        for amplitude, frequency in components:
            upper = up(upper + component_upper(amplitude, frequency, nu, rate, count))
        upper = up(upper + up(abs(dc) * dirichlet_upper(0.0, nu, rate, count)))
        global_upper = max(global_upper, upper)
        # For equal mixtures qualify the union; for dominant cases only f1.
        target = components if expected_status == "ESTIMATOR_UNPROVEN" else components[:1]
        if all(up(abs(nu - f)) >= down(seed_radius) for _, f in target):
            maximum_outside_seed = max(maximum_outside_seed, upper)
        if up(abs(nu - components[0][1])) >= down(mask_radius - seed_radius):
            maximum_outside_inner_mask = max(maximum_outside_inner_mask, upper)
    if expected_status == "OK":
        localized = nearest_lowers[0] > maximum_outside_seed
        predicted_dominance = nearest_lowers[0] >= up(2.0 * maximum_outside_inner_mask)
        qualified = localized and predicted_dominance
    else:
        localized = max(nearest_lowers) > maximum_outside_seed
        # Other tone's nearest bin lies far outside every eligible seed's 8 Hz mask.
        separation = abs(components[0][1] - components[1][1])
        predicted_dominance = (separation > mask_radius + 2 * seed_radius
                              and global_upper < down(2.0 * min(nearest_lowers)))
        qualified = localized and predicted_dominance
    if not qualified:
        raise RuntimeError("SOURCE_CONDITIONAL_GEOMETRY_REFUSED: "
                           + json.dumps({"rate": rate, "components": components, "dc": dc,
                                         "status": expected_status, "lower": nearest_lowers,
                                         "outside": maximum_outside_seed,
                                         "mask_upper": maximum_outside_inner_mask,
                                         "global_upper": global_upper}))
    return {"status": "SOURCE_CONDITIONAL_BOUND_ONLY_NOT_MEASURED", "rate": rate,
            "M": count, "K": fft_size, "sample_error_premise": SOURCE_ERROR_BOUND,
            "source_error_premise_measured": False,
            "seed_band_half_width_hz": seed_radius, "mask_half_width_hz": mask_radius,
            "nearest_amplitude_lowers": nearest_lowers,
            "max_outside_seed_union_amplitude_upper": maximum_outside_seed,
            "max_outside_inner_mask_amplitude_upper": maximum_outside_inner_mask,
            "global_amplitude_upper": global_upper, "expected_estimator_status": expected_status,
            "localized_under_premise": localized, "dominance_or_refusal_under_premise": predicted_dominance,
            "scratch_payload_bound_bytes": scratch_bound}


def calibration_rows():
    phases = (0.0, math.pi / 7, math.pi / 2, math.pi, 11 * math.pi / 6)
    for rate in RATES:
        starts = (0, rate // 20, rate // 20 + 1)
        for group in ("frequency", "dc", "amplitude", "dominant", "equal"):
            if group == "frequency":
                variants = [(f, c, 0.2, 0.0, 0.0, 0.0) for f in (220, 997) for c in (-6, -5, 0, 5, 6)]
            elif group == "dc":
                variants = [(f, 0, 0.2, 0.0, 0.0, dc) for f in (220, 997) for dc in (-0.01, 0.0, 0.01)]
            elif group == "amplitude":
                variants = []
                for f in (220, 997):
                    for db in (-1.1, 0.0, 1.1):
                        with localcontext() as ctx:
                            ctx.prec = 100
                            charge(2)
                            COUNTERS["decimal_library_calls"] += 2
                            a = float(Decimal("0.2") * (Decimal(10).ln() * Decimal(str(db)) / 20).exp())
                        variants.append((f, 0, a, 0.0, 0.0, 0.0))
            elif group == "dominant":
                variants = [(220, 0, 0.20, 0.025, 997.0, 0.0),
                            (997, 0, 0.20, 0.025, 220.0, 0.0)]
            else:
                variants = [(220, 0, 0.10, 0.10, 997.0, 0.0)]
            for vi, (nominal, cents, a1, a2, f2, dc) in enumerate(variants):
                with localcontext() as ctx:
                    ctx.prec = 100
                    charge(2)
                    COUNTERS["decimal_library_calls"] += 2
                    f1 = float(Decimal(nominal) * (Decimal(2).ln() * Decimal(cents) / 1200).exp())
                for pi, phase in enumerate(phases):
                    for si, start in enumerate(starts):
                        yield {"id": f"{group}_{rate}_{vi}_{pi}_{si}", "group": group,
                               "rate": rate, "start": start, "frames": rate // 4,
                               "amplitude1": a1, "frequency1": f1, "phase1": phase,
                               "amplitude2": a2, "frequency2": f2,
                               "phase2": phase + math.pi / 7 if a2 else 0.0,
                               "dc": dc, "expected_status": "ESTIMATOR_UNPROVEN" if group == "equal" else "OK",
                               "expected_frequency": 0.0 if group == "equal" else f1}


def fixture_map(kind):
    if kind < 5:
        bpm = (120, 80, 60, 40, 30)[kind]
        return [(0, 90, False), (S, bpm, False), (S + L, bpm, False)]
    first, last = (120, 30) if kind in (5, 7) else (30, 120)
    if kind < 7:
        return [(0, 90, False), (S, first, True), (S + L, last, False)]
    return [(0, 90, False), (S, first, False), (J, last, False), (S + L, last, False)]


def real_frame(rate, changes, tick):
    total = Decimal(0)
    scale = Decimal(60 * rate) / P
    for i, (begin, bpm, ramp) in enumerate(changes):
        if tick <= begin:
            break
        end = changes[i + 1][0] if i + 1 < len(changes) else tick
        delta = min(tick, Decimal(end)) - begin
        if ramp and i + 1 < len(changes) and changes[i + 1][1] != bpm:
            slope = Decimal(changes[i + 1][1] - bpm) / Decimal(end - begin)
            charge(1)
            COUNTERS["decimal_library_calls"] += 1
            total += scale / slope * (1 + slope * delta / bpm).ln()
        else:
            total += scale * delta / bpm
        if tick <= end:
            break
    return total


def real_inverse(rate, changes, target):
    scale = Decimal(60 * rate) / P
    for i, (begin, bpm, ramp) in enumerate(changes):
        prefix = real_frame(rate, changes, Decimal(begin))
        end = changes[i + 1][0] if i + 1 < len(changes) else None
        if end is not None and target >= real_frame(rate, changes, Decimal(end)):
            continue
        local = target - prefix
        if ramp and end is not None and changes[i + 1][1] != bpm:
            slope = Decimal(changes[i + 1][1] - bpm) / Decimal(end - begin)
            charge(1)
            COUNTERS["decimal_library_calls"] += 1
            delta = Decimal(bpm) / slope * ((local * slope / scale).exp() - 1)
        else:
            delta = local * bpm / scale
        return Decimal(begin) + delta
    raise RuntimeError("REAL_INVERSE_DOMAIN")


def write_clock(path):
    count = 0
    with path.open("x", newline="", encoding="utf-8") as stream, localcontext() as ctx:
        ctx.prec = 110
        writer = csv.writer(stream, delimiter="\t", lineterminator="\n")
        writer.writerow(("id", "mode", "rate", "map", "whole", "num", "den", "frame", "reference"))
        for rate in RATES:
            n = 10 * rate
            for kind in range(9):
                changes = fixture_map(kind)
                ticks = [Fraction(S) + Fraction(L * i, n) for i in
                         (0, 1, n // 4 - 1, n // 4, n // 4 + 1, 2 * rate, 4 * rate,
                          6 * rate, 8 * rate, n - 1, n)]
                ticks += [Fraction(J) + Fraction(delta, 7) for delta in (-1, 0, 1)]
                for qi, tick in enumerate(ticks):
                    whole, rem = divmod(tick.numerator, tick.denominator)
                    native = Decimal(tick.numerator) / Decimal(tick.denominator)
                    reference = real_frame(rate, changes, native)
                    writer.writerow((f"F_{rate}_{kind}_{qi}", "F", rate, kind, whole,
                                     rem, tick.denominator, "0", str(reference)))
                    count += 1
                start = int(real_frame(rate, changes, Decimal(S))) + 1
                end = int(real_frame(rate, changes, Decimal(S + L)))
                for qi, frame in enumerate(range(start, end, rate)):
                    inverse = real_inverse(rate, changes, Decimal(frame))
                    writer.writerow((f"I_{rate}_{kind}_{qi}", "I", rate, kind,
                                     0, 0, 1, frame, str(inverse)))
                    count += 1
    return count


def write_whole_cases(output):
    """Independent20-second analytic definitions; no candidate or NumericA cuts."""
    fields=("case","channel","start","frequency","phase","rms")
    count=0
    with (output/"whole-cases.tsv").open("x",encoding="utf-8",newline="") as cases, \
            (output/"whole-windows.tsv").open("x",encoding="utf-8",newline="") as windows:
        cw=csv.writer(cases,delimiter="\t",lineterminator="\n")
        ww=csv.writer(windows,delimiter="\t",lineterminator="\n")
        cw.writerow(("case","rate","channels","frames","windows_per_channel"));ww.writerow(fields)
        for rate in RATES:
            for channels in (1,2):
                charge(64)
                name=f"whole_{rate}_{channels}";frames=20*rate;m=rate//4;hop=rate//20
                expected=(frames-m)//hop+1
                if expected!=396:raise RuntimeError("WHOLE_CASE_WINDOW_INVENTORY")
                cw.writerow((name,rate,channels,frames,expected))
                for channel in range(channels):
                    frequency=997.0 if channel else 220.0
                    phase=math.pi/7 if channel else 0.0
                    for start in range(0,frames-m+1,hop):
                        charge(64)
                        row=dict(rate=rate,start=start,frames=m,amplitude1=0.2,
                                 frequency1=frequency,phase1=phase,amplitude2=0.0,
                                 frequency2=0.0,phase2=0.0,dc=0.0)
                        rms=analytic_rms(row)
                        ww.writerow((name,channel,start,format(frequency,".17g"),
                                     format(phase,".17g"),rms));count+=1
    if count!=3564:raise RuntimeError("WHOLE_CASE_TOTAL_WINDOWS")
    with (output/"whole-case-source-hold.json").open("x",encoding="utf-8",newline="\n") as stream:
        json.dump(dict(status="ANALYTIC_REFERENCES_ONLY_CALIBRATION_UNRUN",cases=6,
                       windows=3564,windows_per_channel=396,case_budget_reset_per_window=False,
                       work_cap=WORK_CAP,scratch_cap=SCRATCH_CAP,
                       source_error_premise_measured=False,
                       fixed_coefficient_objective_source_authored=True,
                       transform_proof_independently_accepted=True,
                       whole_case_preflight_closed=True,
                       source_readiness_acceptance="R4_NUMERIC_COST_ACCEPTED_R5_STORAGE_PROFILE_ASSERTED_ROOT_STAGES_REQUIRED",
                       execution_gate="ROOT_ACTUAL_FREEZE_OUTPUT_REVIEW_THEN_SEPARATE_COMPILE_GO_REQUIRED",
                       new_transform_and_ranking_controls_executed=False,
                       vendor_invoked=False,production_admission=False),stream,indent=2)
        stream.write("\n")
    return count


def prove_pi_bracket():
    """Exact rational Machin identity; no floating libm or candidate values."""
    def atan_bounds(denominator,count):
        total=Fraction(0)
        for k in range(count):
            charge(256)
            term=Fraction(1,(2*k+1)*denominator**(2*k+1))
            total=total+term if k%2==0 else total-term
        next_term=Fraction(1,(2*count+1)*denominator**(2*count+1))
        return (total-next_term,total) if count%2 else (total,total+next_term)
    a,b=atan_bounds(5,24),atan_bounds(239,8)
    lower,upper=16*a[0]-4*b[1],16*a[1]-4*b[0]
    pi_low=Fraction.from_float(float.fromhex("0x1.921fb54442d18p+1"))
    pi_high=Fraction.from_float(float.fromhex("0x1.921fb54442d19p+1"))
    if not (pi_low<lower<=upper<pi_high):raise RuntimeError("EXACT_PI_BRACKET_REFUSED")
    # tan(4*atan(1/5)-atan(1/239))=1, with the angle in(0,pi/2),
    # proves Machin's identity. Alternating rational next terms enclose atan.
    return dict(kind="EXACT_RATIONAL_MACHIN_ATAN_ALTERNATING",terms=[24,8],
                lower_numerator=str(lower.numerator),lower_denominator=str(lower.denominator),
                upper_numerator=str(upper.numerator),upper_denominator=str(upper.denominator),
                binary64_bracket=["0x1.921fb54442d18p+1","0x1.921fb54442d19p+1"])


def freeze(output):
    if not output.is_dir():
        raise RuntimeError("OUTPUT_DIRECTORY_MUST_ALREADY_EXIST")
    # Source-level preflight: finite row count, streamed rows/queries, bounded geometry cache.
    pi_proof = prove_pi_bracket()
    expected_rows = 1125
    predicted_geometry_work = 100 * (131072 // 2) * (6 * 128 + 16)
    predicted_analytic_work = (expected_rows + 3564) * 18 * 200 * 100 + 4096 * 4096 + 10_000_000
    if predicted_geometry_work + predicted_analytic_work > WORK_CAP:
        raise RuntimeError("FREEZER_PREFLIGHT_WORK_REFUSED")
    if 16 * 131072 + 4 * 24000 + (1 << 20) > SCRATCH_CAP:
        raise RuntimeError("FREEZER_PREFLIGHT_SCRATCH_REFUSED")
    geometries = {}
    group_counts = {}
    row_count = 0
    with (output / "calibration.tsv").open("x", newline="", encoding="utf-8") as table, \
            (output / "geometry.jsonl").open("x", encoding="utf-8", newline="\n") as geometry_stream:
        writer = csv.writer(table, delimiter="\t", lineterminator="\n")
        writer.writerow(FIELDS)
        for row in calibration_rows():
            components = [(row["amplitude1"], row["frequency1"])]
            if row["amplitude2"]:
                components.append((row["amplitude2"], row["frequency2"]))
            key = (row["rate"], tuple(components), abs(row["dc"]), row["expected_status"])
            if key not in geometries:
                if len(geometries) >= 100:
                    raise RuntimeError("GEOMETRY_CACHE_REFUSED")
                geometry_id = f"geometry_{len(geometries)}"
                bound = geometric_qualification(row["rate"], components, row["dc"], row["expected_status"])
                geometry_stream.write(json.dumps({"id": geometry_id, **bound}, sort_keys=True) + "\n")
                geometries[key] = geometry_id
            row["geometry_id"] = geometries[key]
            row["expected_rms"] = analytic_rms(row)
            writer.writerow([format(row[field], ".17g") if isinstance(row[field], float)
                             else row[field] for field in FIELDS])
            row_count += 1
            group_counts[row["group"]] = group_counts.get(row["group"], 0) + 1
    if row_count != expected_rows:
        raise RuntimeError(f"FREEZER_ROW_COUNT_MISMATCH:{row_count}")
    clock_count = write_clock(output / "clock.tsv")
    whole_windows = write_whole_cases(output)
    files = {}
    for name in ("calibration.tsv", "geometry.jsonl", "clock.tsv", "whole-cases.tsv", "whole-windows.tsv", "whole-case-source-hold.json"):
        digest = hashlib.sha256()
        with (output / name).open("rb") as source:
            for block in iter(lambda: source.read(1 << 20), b""):
                digest.update(block)
        files[name] = digest.hexdigest()
    receipt = {"status": "ANALYTIC_FREEZE_ONLY_NOT_CALIBRATION", "rows": row_count,
               "groups": group_counts, "clock_queries": clock_count, "geometries": len(geometries),
               "whole_cases": 6, "whole_windows": whole_windows,
               "source_readiness_acceptance": "R4_NUMERIC_COST_ACCEPTED_R5_STORAGE_PROFILE_ASSERTED_ROOT_STAGES_REQUIRED",
               "calibration_status": "UNRUN_ROOT_ACTUAL_OUTPUT_REVIEW_AND_SEPARATE_COMPILE_GO_REQUIRED",
               "exact_pi_proof": pi_proof,
               "source_error_premise_measured": False, "vendor_invoked": False,
               "numeric_a_floors": "UNRUN_CXX_SAME_RUNTIME_BACKEND_REQUIRED",
               "real_reference": "DECIMAL110_120_NUMERICAL_REFERENCE_NOT_CERTIFIED_REAL_FLOORS",
               "files_sha256": files, "counters": COUNTERS,
               "work_cap": WORK_CAP, "scratch_payload_cap": SCRATCH_CAP,
               "preflight_geometry_work_upper": predicted_geometry_work,
               "preflight_analytic_work_upper": predicted_analytic_work}
    with (output / "freeze_receipt.json").open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(receipt, stream, indent=2, sort_keys=True)
        stream.write("\n")
    print(json.dumps(receipt, sort_keys=True))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    freeze(args.output.resolve())
