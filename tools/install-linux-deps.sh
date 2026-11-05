#!/usr/bin/env bash
set -euo pipefail

if (( $# != 1 )); then
  printf 'usage: %s base|package\n' "$0" >&2
  exit 2
fi

packages=(
  libasound2-dev libjack-jackd2-dev ladspa-sdk
  libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev
  libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev
  libglu1-mesa-dev mesa-common-dev ninja-build
)
case "$1" in
  base) ;;
  package) packages+=(zip unzip) ;;
  *) printf 'unknown Linux dependency profile: %s\n' "$1" >&2; exit 2 ;;
esac

apt_options=(
  -o Acquire::http::Timeout=30
  -o Acquire::https::Timeout=30
  -o Acquire::Retries=2
)

printf 'Linux dependencies: update\n'
sudo -n /usr/bin/timeout --verbose --signal=TERM --kill-after=15s 4m \
  /usr/bin/env DEBIAN_FRONTEND=noninteractive \
  apt-get "${apt_options[@]}" --error-on=any update

printf 'Linux dependencies: install (%s)\n' "$1"
sudo -n /usr/bin/timeout --verbose --signal=TERM --kill-after=15s 8m \
  /usr/bin/env DEBIAN_FRONTEND=noninteractive \
  apt-get "${apt_options[@]}" install -y "${packages[@]}"
