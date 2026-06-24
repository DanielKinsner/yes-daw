#!/usr/bin/env bash
# YES DAW — H0 real-machine soak gate. Runs on ONE real machine (the owner's box, or a scheduled
# self-hosted runner). Exit 0 = healthy, 1 = dropout/silence, 2 = setup error. No human listens.
#
#   tools/soak.sh [seconds] [--loopback] [--block-size N]
#
# This is the AUDIO half of the H0 exit: 600 s (10 min) at a 128-frame Block (the roadmap target;
# pass --block-size to override) with zero dropouts. Add --loopback once the output is wired back to
# an input (physical jumper or virtual cable) to also assert that sound physically left the device AND
# was 440 Hz. NOTE: the GPU-timeline 60 fps half (max_frame_ms < 16.6) is NOT YET implemented here — it
# needs the native render shell that doesn't exist yet, so a PASS below means the AUDIO gate only.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DUR="${1:-600}"
shift || true
EXTRA="$*"

# Find the built soak binary across the Ninja (preset) and Visual Studio layouts.
BIN=""
for c in "$ROOT/build/YesDawSoak" "$ROOT/build/YesDawSoak.exe" \
         "$ROOT/build/Release/YesDawSoak.exe" "$ROOT/build/Debug/YesDawSoak.exe" \
         "$ROOT/build/YesDawSoak_artefacts/Release/YesDawSoak.exe" \
         "$ROOT/build/YesDawSoak_artefacts/Debug/YesDawSoak.exe"; do
  if [ -x "$c" ]; then BIN="$c"; break; fi
done
if [ -z "$BIN" ]; then
  echo "soak: YesDawSoak not built. Run:  cmake --preset ci && cmake --build --preset ci"
  exit 2
fi

STATS="$ROOT/soak-stats.json"
# shellcheck disable=SC2086
"$BIN" --seconds "$DUR" --stats-out "$STATS" $EXTRA

python3 - "$STATS" <<'PY'
import json, sys
s = json.load(open(sys.argv[1]))

if s.get("setup_error"):
    print("FAIL: setup error:", s["setup_error"]); sys.exit(2)

problems = []
# Device-reported xruns are authoritative when supported (>=0); -1 means fall back to deadline misses.
if s["xruns"] >= 0 and s["xruns"] != 0:
    problems.append(f'xruns={s["xruns"]}')
if s["deadline_misses"] != 0:
    problems.append(f'deadline_misses={s["deadline_misses"]}')
if s["device_error"]:
    problems.append("device_error")
if s["block_budget_ms"] > 0 and s["max_block_ms"] >= s["block_budget_ms"]:
    problems.append(f'max_block_ms {s["max_block_ms"]:.3f} >= budget {s["block_budget_ms"]:.3f}')
# The roadmap names a 128-frame Block; if the device couldn't go that tight, the H0 target isn't met.
if s["block_size"] > s["requested_block_size"]:
    problems.append(f'block {s["block_size"]} > target {s["requested_block_size"]} '
                    f'(device could not meet it — needs a low-latency/exclusive driver)')
if s["loopback"]:
    if not (s["loopback_peak_rms"] > 0.01):
        problems.append(f'silent loopback (rms={s["loopback_peak_rms"]:.4f})')
    # Non-silent isn't enough — the captured energy must actually be at 440 Hz (a pure tone has
    # 440_mag ~ sqrt(2)*rms; hum/noise has a small 440 bin).
    elif not (s["loopback_440_mag"] > 0.5 * s["loopback_peak_rms"]):
        problems.append(f'loopback not 440 Hz (440_mag={s["loopback_440_mag"]:.4f} '
                        f'vs rms={s["loopback_peak_rms"]:.4f})')

print(f'device="{s["device"]}" sr={s["sample_rate"]} block={s["block_size"]}/{s["requested_block_size"]} '
      f'xruns={s["xruns"]} deadline_misses={s["deadline_misses"]} '
      f'max_block_ms={s["max_block_ms"]:.3f}/{s["block_budget_ms"]:.3f} '
      f'loop_rms={s["loopback_peak_rms"]:.4f} loop_440={s["loopback_440_mag"]:.4f}')

if problems:
    print("FAIL:", "; ".join(problems)); sys.exit(1)
print("PASS (audio gate; GPU 60 fps frame gate not yet implemented)"); sys.exit(0)
PY
