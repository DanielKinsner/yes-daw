#!/usr/bin/env bash
# YES DAW - H8 real-device playback smoke.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DUR="${1:-600}"
shift || true

"$ROOT/tools/soak.sh" "$DUR" "$@" --block-size 128 --playback-project
