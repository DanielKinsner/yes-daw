#!/usr/bin/env bash
# YES DAW — H0 real-machine soak gate. Runs on ONE real machine (the owner's box, or a scheduled
# self-hosted runner). Exit 0 = healthy, 1 = dropout/silence, 2 = setup error. No human listens.
#
#   tools/soak.sh [seconds] [--loopback]
#
# Default 600 s (10 min) at the device's block size — the H0 audio exit criterion. Add --loopback once
# the output is wired back to an input (physical jumper or a virtual cable) to also assert that sound
# physically left the device at 440 Hz. The GPU-timeline 60 fps half of the H0 exit joins this script
# when spike #2 lands.
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
if s["loopback"] and not (s["loopback_peak_rms"] > 0.01):
    problems.append(f'silent loopback (rms={s["loopback_peak_rms"]:.4f})')

print(f'device="{s["device"]}" sr={s["sample_rate"]} block={s["block_size"]} '
      f'xruns={s["xruns"]} deadline_misses={s["deadline_misses"]} '
      f'max_block_ms={s["max_block_ms"]:.3f}/{s["block_budget_ms"]:.3f} '
      f'loop_rms={s["loopback_peak_rms"]:.4f}')

if problems:
    print("FAIL:", "; ".join(problems)); sys.exit(1)
print("PASS"); sys.exit(0)
PY
