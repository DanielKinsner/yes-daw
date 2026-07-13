#!/usr/bin/env bash
# tools/package.sh — build + stage + zip a portable YES DAW alpha build (macOS / Linux).
#
# H17 CP3 (see docs/plans/2026-07-03-h17-distribution-alpha-plan.md). The Windows portable zip
# is produced by tools/package.ps1; this is the POSIX sibling (macOS zip is produced but its
# notarization story is explicitly beta — ADR-0037).
#
# Honest scope: this builds via the `ci` preset, stages the artefact + alpha readme +
# git-describe version.txt (+ LICENSE if present), and zips it. It does NOT run the packaged
# self-check — that is CP1 `--selfcheck`, still open — and it never writes a reality-lane PASS
# row (owner-only, docs/reality-lane.md). Exit 0 on a produced zip, non-zero on any failure.
#
# Usage:
#   tools/package.sh            build (ci/Release) then package
#   tools/package.sh --no-build package an already-built build-ci/ tree
#   tools/package.sh --help     print this header
set -euo pipefail

no_build=0
for arg in "$@"; do
  case "$arg" in
    --no-build) no_build=1 ;;
    -h|--help) awk 'NR==1{next} /^#/{sub(/^# ?/,""); print; next} {exit}' "$0"; exit 0 ;;
    *) echo "package.sh: unknown arg: $arg" >&2; exit 2 ;;
  esac
done

# Repo root = parent of this script's own dir (never a typed absolute path).
script_dir="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$script_dir/.." && pwd)"
cd "$repo"

# Version: git describe with graceful fallbacks (no tags yet -> short sha; dirty tree -> -dirty).
if version="$(git -C "$repo" describe --tags --always --dirty 2>/dev/null)" && [ -n "$version" ]; then
  :
else
  version="0.0.0-nogit"
fi

# Platform label + expected Release artefact from the `ci` preset (binaryDir = build-ci/).
uname_s="$(uname -s)"
case "$uname_s" in
  Darwin) os_label="macos"; artefact="build-ci/YesDaw_artefacts/Release/YesDaw.app" ;;
  Linux)  os_label="linux"; artefact="build-ci/YesDaw_artefacts/Release/YesDaw" ;;
  *)      os_label="unknown"; artefact="build-ci/YesDaw_artefacts/Release/YesDaw" ;;
esac

if [ "$no_build" -eq 0 ]; then
  echo "[package] building via ci preset (Release)..."
  cmake --build --preset ci --target YesDaw
fi

if [ ! -e "$artefact" ]; then
  echo "[package] FAIL: artefact not found: $artefact (build first, or drop --no-build)" >&2
  exit 1
fi

stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT
pkg_name="YesDaw-${version}-${os_label}-portable"
pkg_root="$stage/$pkg_name"
mkdir -p "$pkg_root"

# Stage the artefact (dir-copy handles the macOS .app bundle; file-copy the bare Linux binary).
cp -R "$artefact" "$pkg_root/"

# version.txt — the single value the packaged `--version` must match (CP1 version-stamp gate).
printf '%s\n' "$version" > "$pkg_root/version.txt"

# README-alpha.md — how to run the portable build (staged if present).
if [ -f "$repo/README-alpha.md" ]; then
  cp "$repo/README-alpha.md" "$pkg_root/"
else
  echo "[package] WARN: README-alpha.md missing — packaging without it." >&2
fi

# LICENSE — staged if present. Its absence is flagged, never invented: licensing is an owner
# decision, not a packaging default (CLAUDE.md: no taste decisions).
if [ -f "$repo/LICENSE" ]; then
  cp "$repo/LICENSE" "$pkg_root/"
else
  echo "[package] WARN: no LICENSE at repo root — alpha zip ships without one (owner TODO)." >&2
fi

mkdir -p "$repo/dist"
out="$repo/dist/${pkg_name}.zip"
rm -f "$out"
( cd "$stage" && zip -r -q "$out" "$pkg_name" )

if [ ! -f "$out" ]; then
  echo "[package] FAIL: zip was not produced at $out" >&2
  exit 1
fi

echo "[package] PASS: $out ($version)"
