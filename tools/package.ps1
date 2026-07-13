# tools/package.ps1 — build + stage + zip a portable YES DAW alpha build (Windows).
#
# H17 CP3 (see docs/plans/2026-07-03-h17-distribution-alpha-plan.md). Produces
# YesDaw-<version>-win64-portable.zip from the `ci` (Release) build. This is the primary alpha
# artefact path (ADR-0037: portable unsigned zip for alpha; signing/installer are beta).
#
# Honest scope: build -> stage exe + alpha readme + git-describe version.txt (+ LICENSE if
# present) -> zip. It does NOT run the packaged self-check (CP1 `--selfcheck`, still open) and
# never writes a reality-lane PASS row. Exit 0 on a produced zip, 1 on failure.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools\package.ps1            build then package
#   powershell -ExecutionPolicy Bypass -File tools\package.ps1 -NoBuild   package build-ci/ as-is
param(
  [switch] $NoBuild
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

# Version: git describe with graceful fallback (no tags -> short sha; dirty tree -> -dirty).
try {
  $version = (git -C $root describe --tags --always --dirty 2>$null)
  if ($version) { $version = $version.Trim() }
} catch {
  $version = ''
}
if (-not $version) { $version = '0.0.0-nogit' }

if (-not $NoBuild) {
  Write-Host "[package] building via ci preset (Release)..."
  Push-Location $root
  try { cmake --build --preset ci --target YesDaw } finally { Pop-Location }
}

$exe = Join-Path $root 'build-ci\YesDaw_artefacts\Release\YesDaw.exe'
if (-not (Test-Path -LiteralPath $exe)) {
  Write-Error "[package] artefact not found: $exe (build first, or drop -NoBuild)"
  exit 1
}

$pkgName = "YesDaw-$version-win64-portable"
$stage   = Join-Path ([System.IO.Path]::GetTempPath()) ("yesdaw-pkg-" + [System.Guid]::NewGuid().ToString('N'))
$pkgRoot = Join-Path $stage $pkgName
New-Item -ItemType Directory -Force -Path $pkgRoot | Out-Null

try {
  Copy-Item -LiteralPath $exe -Destination $pkgRoot

  # version.txt — the single value the packaged `--version` must match (CP1 version-stamp gate).
  Set-Content -LiteralPath (Join-Path $pkgRoot 'version.txt') -Value $version

  # README-alpha.md — how to run the portable build (staged if present).
  $readme = Join-Path $root 'README-alpha.md'
  if (Test-Path -LiteralPath $readme) {
    Copy-Item -LiteralPath $readme -Destination $pkgRoot
  } else {
    Write-Warning "[package] README-alpha.md missing - packaging without it."
  }

  # LICENSE — staged if present. Absence is flagged, never invented (licensing is an owner
  # decision, not a packaging default; CLAUDE.md: no taste decisions).
  $license = Join-Path $root 'LICENSE'
  if (Test-Path -LiteralPath $license) {
    Copy-Item -LiteralPath $license -Destination $pkgRoot
  } else {
    Write-Warning "[package] no LICENSE at repo root - alpha zip ships without one (owner TODO)."
  }

  $dist = Join-Path $root 'dist'
  New-Item -ItemType Directory -Force -Path $dist | Out-Null
  $out = Join-Path $dist "$pkgName.zip"
  if (Test-Path -LiteralPath $out) { Remove-Item -LiteralPath $out -Force }
  Compress-Archive -Path $pkgRoot -DestinationPath $out -Force

  if (-not (Test-Path -LiteralPath $out)) {
    Write-Error "[package] zip was not produced at $out"
    exit 1
  }
  Write-Host "[package] PASS: $out ($version)"
} finally {
  Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
}
