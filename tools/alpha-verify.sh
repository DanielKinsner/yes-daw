#!/usr/bin/env bash
# tools/alpha-verify.sh — H17 CP5 mechanical close for the alpha gate (macOS / Linux).
#
# Runs the objective sub-asserts from docs/alpha-gate.md against a produced project bundle + its
# exported WAV. Exit 0 iff every assert passes; any red = not alpha. The subjective "feel session"
# is separate and never gates (see docs/alpha-gate.md). An agent never writes a reality-lane PASS
# row — this only runs mechanical checks.
#
# The 5 asserts (each with a named negative control exercised by --self-test):
#   1. export exists + non-empty        (control: an empty file)
#   2. export re-imports bit-exact      (control: a non-WAV / garbage file)  -> YesDawSelfCheck --verify-wav
#   3. integrated loudness in -30..-6   (control: out-of-range values + an unmeasurable file) -> --loudness
#   4. bundle reopens, zero validators  (control: a path that is not a bundle) -> --selfcheck
#   5. autosave snapshot present         (control: a bundle dir with no autosave)
#
# Honest scope: the full POSITIVE pass needs a real produced song (H17 CP2's committed demo bundle,
# or your own session mix). --self-test does NOT need one: it proves each assert can fail on a
# synthetic bad input, plus the loudness range logic directly. CI runs --self-test.
#
# Usage:
#   tools/alpha-verify.sh --bundle <b.yesdaw> --wav <mix.wav> [--selfcheck <path>]
#   tools/alpha-verify.sh --self-test [--selfcheck <path>]
#   tools/alpha-verify.sh --help
set -euo pipefail

LUFS_MIN=-30
LUFS_MAX=-6

bundle=""
wav=""
selfcheck=""
self_test=0

print_help() { awk 'NR==1{next} /^#/{sub(/^# ?/,""); print; next} {exit}' "$0"; }

while [ "$#" -gt 0 ]; do
  case "$1" in
    --bundle)    bundle="${2:-}"; shift 2 ;;
    --wav)       wav="${2:-}"; shift 2 ;;
    --selfcheck) selfcheck="${2:-}"; shift 2 ;;
    --self-test) self_test=1; shift ;;
    -h|--help)   print_help; exit 0 ;;
    *) echo "alpha-verify: unknown arg: $1" >&2; exit 2 ;;
  esac
done

# Locate the YesDawSelfCheck binary (explicit --selfcheck wins; else the ci-preset build path; else PATH).
locate_selfcheck() {
  if [ -n "$selfcheck" ]; then printf '%s' "$selfcheck"; return 0; fi
  local repo; repo="$(cd "$(dirname "$0")/.." && pwd)"
  local candidate="$repo/build-ci/YesDawSelfCheck_artefacts/Release/YesDawSelfCheck"
  if [ -x "$candidate" ]; then printf '%s' "$candidate"; return 0; fi
  if command -v YesDawSelfCheck >/dev/null 2>&1; then printf '%s' "YesDawSelfCheck"; return 0; fi
  return 1
}

# --- Assert helpers: return 0 = pass, non-zero = fail. Never call bare under `set -e`; guard with `if`.
assert_export_exists()     { [ -s "$1" ]; }
assert_reimport_bitexact() { "$SELFCHECK" --verify-wav "$1" >/dev/null 2>&1; }
assert_reopen_selfcheck()  { "$SELFCHECK" --selfcheck  "$1" >/dev/null 2>&1; }
assert_autosave_present()  { [ -e "$1/autosave/last.yesdaw" ]; }

loudness_in_range() { # $1 = LUFS value
  awk -v x="$1" -v lo="$LUFS_MIN" -v hi="$LUFS_MAX" 'BEGIN { exit !(x >= lo && x <= hi) }'
}

assert_loudness_range() { # $1 = wav
  local out lufs
  out="$("$SELFCHECK" --loudness "$1" 2>/dev/null)" || return 1
  lufs="$(printf '%s' "$out" | sed -n 's/.*integrated \(-\{0,1\}[0-9.]*\) LUFS.*/\1/p')"
  [ -n "$lufs" ] || return 1
  loudness_in_range "$lufs"
}

report() { # $1 = name, $2 = 0/1 pass/fail
  if [ "$2" -eq 0 ]; then echo "  PASS  $1"; else echo "  FAIL  $1"; fi
}

run_verify() {
  [ -n "$bundle" ] && [ -n "$wav" ] || { echo "alpha-verify: --bundle and --wav are required" >&2; exit 2; }
  SELFCHECK="$(locate_selfcheck)" || { echo "alpha-verify: YesDawSelfCheck not found (pass --selfcheck)" >&2; exit 2; }

  echo "alpha-verify: bundle=$bundle wav=$wav"
  local fails=0

  if assert_export_exists     "$wav";    then report "export exists + non-empty" 0; else report "export exists + non-empty" 1; fails=1; fi
  if assert_reimport_bitexact "$wav";    then report "export re-imports bit-exact" 0; else report "export re-imports bit-exact" 1; fails=1; fi
  if assert_loudness_range    "$wav";    then report "integrated loudness in ${LUFS_MIN}..${LUFS_MAX} LUFS" 0; else report "integrated loudness in ${LUFS_MIN}..${LUFS_MAX} LUFS" 1; fails=1; fi
  if assert_reopen_selfcheck  "$bundle"; then report "bundle reopens, zero validator errors" 0; else report "bundle reopens, zero validator errors" 1; fails=1; fi
  if assert_autosave_present  "$bundle"; then report "autosave snapshot present" 0; else report "autosave snapshot present" 1; fails=1; fi

  if [ "$fails" -eq 0 ]; then echo "ALPHA-VERIFY PASS"; return 0; fi
  echo "ALPHA-VERIFY FAIL"; return 1
}

# Negative controls: each synthetic bad input MUST make its assert fail; the loudness range logic
# must reject out-of-range values and accept an in-range one. Exit non-zero if any control misbehaves.
run_self_test() {
  SELFCHECK="$(locate_selfcheck)" || { echo "alpha-verify: YesDawSelfCheck not found (pass --selfcheck)" >&2; exit 2; }

  local tmp; tmp="$(mktemp -d)"
  # shellcheck disable=SC2064
  trap "rm -rf '$tmp'" EXIT

  local bad=0
  check_fails() { # $1 = name, then the assert command — the assert MUST fail (non-zero)
    local name="$1"; shift
    if "$@"; then echo "  BAD   control did NOT fail: $name"; bad=1; else echo "  ok    control fails: $name"; fi
  }

  : > "$tmp/empty.wav"
  check_fails "empty export -> export-exists" assert_export_exists "$tmp/empty.wav"

  printf 'this is not a wav file' > "$tmp/garbage.wav"
  check_fails "garbage file -> reimport-bit-exact" assert_reimport_bitexact "$tmp/garbage.wav"
  check_fails "garbage file -> loudness (unmeasurable)" assert_loudness_range "$tmp/garbage.wav"

  # loudness range logic, directly (no WAV needed): out-of-range must reject, in-range must accept.
  if loudness_in_range -70;  then echo "  BAD   -70 LUFS accepted";  bad=1; else echo "  ok    -70 LUFS rejected"; fi
  if loudness_in_range 0;    then echo "  BAD   0 LUFS accepted";    bad=1; else echo "  ok    0 LUFS rejected"; fi
  if loudness_in_range -12;  then echo "  ok    -12 LUFS accepted";  else echo "  BAD   -12 LUFS rejected"; bad=1; fi

  check_fails "non-bundle dir -> reopen-selfcheck" assert_reopen_selfcheck "$tmp"

  mkdir -p "$tmp/no-autosave-bundle"
  check_fails "bundle without autosave -> autosave-present" assert_autosave_present "$tmp/no-autosave-bundle"

  if [ "$bad" -eq 0 ]; then echo "ALPHA-VERIFY SELF-TEST PASS"; return 0; fi
  echo "ALPHA-VERIFY SELF-TEST FAIL"; return 1
}

if [ "$self_test" -eq 1 ]; then run_self_test; else run_verify; fi
