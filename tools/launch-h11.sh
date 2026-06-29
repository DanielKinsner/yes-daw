#!/usr/bin/env bash
# YES DAW - H11 visual-feel launch.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD=1

if [[ "${1:-}" == "--no-build" ]]; then
  BUILD=0
  shift
fi

if [[ "$BUILD" -eq 1 ]]; then
  cmake --build --preset ci --target YesDaw
fi

candidates=(
  "$ROOT/build-ci/YesDaw_artefacts/Release/YesDaw"
  "$ROOT/build-ci/YesDaw_artefacts/Debug/YesDaw"
  "$ROOT/build-ci/YesDaw_artefacts/Release/YesDaw.app/Contents/MacOS/YesDaw"
  "$ROOT/build-ci/YesDaw_artefacts/Debug/YesDaw.app/Contents/MacOS/YesDaw"
)

for candidate in "${candidates[@]}"; do
  if [[ -x "$candidate" ]]; then
    "$candidate" "$@" &
    echo "Launched $candidate"
    exit 0
  fi
done

echo "YesDaw executable was not found under build-ci/YesDaw_artefacts." >&2
exit 1
