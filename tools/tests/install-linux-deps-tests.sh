#!/usr/bin/env bash
# Execute the actual helper with fake sudo/APT and a real GNU timeout, never real APT.
set -euo pipefail
export LC_ALL=C

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
script="$script_dir/install-linux-deps-tests.sh"
helper="$script_dir/../install-linux-deps.sh"

if [[ ${1:-} != --worker ]]; then
  if (( $# != 0 )); then
    printf 'usage: bash %s\n' "$script" >&2
    exit 2
  fi
  real_timeout=$(command -v timeout)
  real_sleep=$(command -v sleep)
  version=$("$real_timeout" --version)
  if [[ $version != *'GNU coreutils'* ]]; then
    printf 'FAIL: GNU timeout is required, got %s\n' "$real_timeout" >&2
    exit 1
  fi
  printf '%s\n' "${version%%$'\n'*}"
  # The worker's fixtures also have finite waits if a platform's signaling fails.
  exec "$real_timeout" --verbose --signal=TERM --kill-after=5s 60s \
    "$BASH" "$script" --worker "$real_timeout" "$real_sleep"
fi

if (( $# != 3 )); then
  printf 'FAIL: invalid internal worker arguments\n' >&2
  exit 2
fi
real_timeout=$2
real_sleep=$3
original_path=$PATH
temp_parent=$(cd -- "${TMPDIR:-/tmp}" && pwd -P)
work=$(mktemp -d "$temp_parent/yesdaw-linux-deps-tests.XXXXXXXX")
if [[ ${work%/*} != "$temp_parent" || ${work##*/} != yesdaw-linux-deps-tests.* || -L $work ]]; then
  printf 'FAIL: unexpected temporary directory: %s\n' "$work" >&2
  exit 1
fi
readonly work temp_parent
case_dir=

cleanup() {
  local status=$?
  trap - EXIT
  if (( status == 0 )); then
    # One Bash process owns this exact mktemp child; never remove a computed parent.
    if [[ ${work%/*} != "$temp_parent" || ${work##*/} != yesdaw-linux-deps-tests.* || -L $work ]]; then
      printf 'FAIL: refusing unexpected cleanup target: %s\n' "$work" >&2
      exit 1
    fi
    rm -rf -- "$work"
  else
    printf 'FAIL: diagnostics retained at %s\n' "$work" >&2
    if [[ -n $case_dir ]]; then
      for log in "$case_dir/stdout.log" "$case_dir/stderr.log"; do
        if [[ -f $log ]]; then
          printf '%s\n' "--- $log ---" >&2
          cat -- "$log" >&2
        fi
      done
    fi
  fi
}
trap cleanup EXIT
trap 'exit 143' TERM
trap 'exit 130' INT
printf 'Linux dependency regression artifacts: %s\n' "$work"

fail() { printf 'FAIL: %s\n' "$*" >&2; exit 1; }
fakebin="$work/bin"
mkdir -- "$fakebin"

cat > "$fakebin/sudo" <<'FAKE_SUDO'
#!/usr/bin/env bash
set -euo pipefail
incoming=("$@")
if (( $# < 17 )) || [[ $1 != -n || $2 != /usr/bin/timeout || $3 != --verbose ||
    $4 != --signal=TERM || $5 != --kill-after=15s || $7 != /usr/bin/env ||
    $8 != DEBIAN_FRONTEND=noninteractive || $9 != apt-get ]]; then
  printf 'fake sudo rejected supervisor/options/privilege order\n' >&2
  exit 97
fi
duration=$6
shift 9
if [[ $1 != -o || $2 != Acquire::http::Timeout=30 || $3 != -o ||
    $4 != Acquire::https::Timeout=30 || $5 != -o || $6 != Acquire::Retries=2 ]]; then
  printf 'fake sudo rejected APT transport options\n' >&2
  exit 97
fi
if [[ $7 == --error-on=any && $8 == update ]] && (( $# == 8 )); then
  phase=update
  [[ $duration == 4m ]] || exit 97
elif [[ $7 == install && $8 == -y ]] && (( $# > 8 )); then
  phase=install
  [[ $duration == 8m ]] || exit 97
else
  printf 'fake sudo rejected APT operation\n' >&2
  exit 97
fi
printf '%s\n' "$phase" >> "$CASE_DIR/sudo.calls"
printf '%s\0' "${incoming[@]}" > "$CASE_DIR/$phase.sudo.argv"
if [[ $TEST_SCENARIO == sudo-refusal ]]; then
  printf 'fake sudo refusal\n' >&2
  exit 73
fi
if [[ $TEST_SCENARIO == command-missing ]]; then
  # Never fall through PATH to a real apt-get in this negative control.
  exec "$REAL_TIMEOUT" --verbose --signal=TERM --kill-after=1s 1s \
    "$CASE_DIR/command-that-does-not-exist"
fi
# Assert the real production timers above, then shorten only this fake boundary.
exec "$REAL_TIMEOUT" --verbose --signal=TERM --kill-after=1s 1s \
  /usr/bin/env DEBIAN_FRONTEND=noninteractive apt-get "$@"
FAKE_SUDO

cat > "$fakebin/apt-get" <<'FAKE_APT'
#!/usr/bin/env bash
set -euo pipefail
[[ ${DEBIAN_FRONTEND:-} == noninteractive ]] || exit 96
if (( $# < 8 )); then exit 96; fi
case "$7:$8" in
  --error-on=any:update) phase=update ;;
  install:-y) phase=install ;;
  *) exit 96 ;;
esac
printf '%s\n' "$phase" >> "$CASE_DIR/apt.calls"
printf '%s\0' "$@" > "$CASE_DIR/$phase.apt.argv"
printf '%s\n' "$DEBIAN_FRONTEND" > "$CASE_DIR/$phase.frontend"
printf 'fake apt stdout: %s\n' "$phase"
printf 'fake apt stderr: %s\n' "$phase" >&2
case "$TEST_SCENARIO:$phase" in
  update-failure:update) exit 100 ;;
  install-failure:install) exit 37 ;;
  update-hang:update|install-hang:install)
    printf '%s\n' "$$" > "$CASE_DIR/owned.pid"
    exec "$REAL_SLEEP" 5
    ;;
  term-exit-zero:update)
    printf '%s\n' "$$" > "$CASE_DIR/owned.pid"
    trap 'printf "fake apt handled TERM and exits zero\n" >&2; exit 0' TERM
    for (( attempt=0; attempt<50; ++attempt )); do "$REAL_SLEEP" 0.1; done
    exit 91
    ;;
  term-ignored:update)
    printf '%s\n' "$$" > "$CASE_DIR/owned.pid"
    trap '' TERM
    exec "$REAL_SLEEP" 5
    ;;
esac
FAKE_APT
chmod +x -- "$fakebin/sudo" "$fakebin/apt-get"

# Contract data is independent of the helper's arrays or parsed source.
apt_options=(-o Acquire::http::Timeout=30 -o Acquire::https::Timeout=30 -o Acquire::Retries=2)
base_packages=(
  libasound2-dev libjack-jackd2-dev ladspa-sdk
  libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev
  libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev
  libglu1-mesa-dev mesa-common-dev ninja-build
)

assert_argv() {
  local file=$1
  shift
  local observed=() expected=("$@") index
  [[ -f $file ]] || fail "missing argv receipt: $file"
  mapfile -d '' -t observed < "$file"
  (( ${#observed[@]} == ${#expected[@]} )) || fail "argument count: $file"
  for (( index=0; index<${#expected[@]}; ++index )); do
    [[ ${observed[index]} == "${expected[index]}" ]] || fail "argument $index: $file"
  done
}

assert_calls() {
  local file=$1
  shift
  local observed=() expected=("$@") index
  if [[ -f $file ]]; then mapfile -t observed < "$file"; fi
  (( ${#observed[@]} == ${#expected[@]} )) || fail "call count: $file"
  for (( index=0; index<${#expected[@]}; ++index )); do
    [[ ${observed[index]} == "${expected[index]}" ]] || fail "call order: $file"
  done
}

assert_output() {
  local phase=$1
  [[ $(< "$case_dir/stdout.log") == *"fake apt stdout: $phase"* ]] || fail "lost $phase stdout"
  [[ $(< "$case_dir/stderr.log") == *"fake apt stderr: $phase"* ]] || fail "lost $phase stderr"
  [[ $(< "$case_dir/$phase.frontend") == noninteractive ]] || fail "interactive $phase"
}

assert_timeout_signal() {
  [[ $(< "$case_dir/stderr.log") == *"sending signal $1 to command"* ]] || fail "lost GNU timeout $1 diagnostic"
}

assert_update() {
  assert_argv "$case_dir/update.apt.argv" "${apt_options[@]}" --error-on=any update
  assert_argv "$case_dir/update.sudo.argv" -n /usr/bin/timeout --verbose \
    --signal=TERM --kill-after=15s 4m /usr/bin/env DEBIAN_FRONTEND=noninteractive \
    apt-get "${apt_options[@]}" --error-on=any update
  assert_output update
}

assert_install() {
  local extra=("$@")
  assert_argv "$case_dir/install.apt.argv" "${apt_options[@]}" install -y \
    "${base_packages[@]}" "${extra[@]}"
  assert_argv "$case_dir/install.sudo.argv" -n /usr/bin/timeout --verbose \
    --signal=TERM --kill-after=15s 8m /usr/bin/env DEBIAN_FRONTEND=noninteractive \
    apt-get "${apt_options[@]}" install -y "${base_packages[@]}" "${extra[@]}"
  assert_output install
}

assert_no_owned_process() {
  local pid attempt
  [[ -f $case_dir/owned.pid ]] || fail 'waiting fixture did not record its PID'
  read -r pid < "$case_dir/owned.pid"
  [[ $pid =~ ^[0-9]+$ && $pid != "$$" && $pid != "$PPID" ]] || fail 'invalid fixture PID'
  for (( attempt=0; attempt<20; ++attempt )); do
    if ! kill -0 "$pid" 2>/dev/null; then return; fi
    "$real_sleep" 0.1
  done
  # Never signal an arbitrary/stale PID; the private fixture itself has a five-second bound.
  fail "owned fixture $pid still present after timeout; inspect retained diagnostics"
}

run_case() {
  local name=$1 scenario=$2 expected=$3 status
  shift 3
  case_dir="$work/$name"
  mkdir -- "$case_dir"
  set +e
  PATH="$fakebin:$original_path" CASE_DIR="$case_dir" TEST_SCENARIO="$scenario" \
    REAL_TIMEOUT="$real_timeout" REAL_SLEEP="$real_sleep" \
    "$BASH" "$helper" "$@" > "$case_dir/stdout.log" 2> "$case_dir/stderr.log"
  status=$?
  set -e
  printf '%s\n' "$status" > "$case_dir/exit.status"
  printf 'case %s: raw exit %s\n' "$name" "$status"
  (( status == expected )) || fail "$name returned $status, expected $expected"
}

run_case base-success success 0 base
assert_calls "$case_dir/sudo.calls" update install
assert_calls "$case_dir/apt.calls" update install
assert_update
assert_install

run_case package-success success 0 package
assert_calls "$case_dir/apt.calls" update install
assert_update
assert_install zip unzip

run_case update-failure update-failure 100 base
assert_calls "$case_dir/apt.calls" update
assert_calls "$case_dir/sudo.calls" update
assert_update
[[ $(< "$case_dir/stdout.log") != *'Linux dependencies: install'* ]] || fail 'install reached after update failure'

run_case install-failure install-failure 37 base
assert_calls "$case_dir/apt.calls" update install
assert_update
assert_install

run_case update-hang update-hang 124 base
assert_calls "$case_dir/apt.calls" update
assert_update
assert_timeout_signal TERM
assert_no_owned_process

run_case install-hang install-hang 124 base
assert_calls "$case_dir/apt.calls" update install
assert_update
assert_install
assert_timeout_signal TERM
assert_no_owned_process

run_case term-exit-zero term-exit-zero 124 base
assert_calls "$case_dir/apt.calls" update
assert_update
assert_timeout_signal TERM
[[ $(< "$case_dir/stderr.log") == *'fake apt handled TERM and exits zero'* ]] || fail 'TERM handler was not reached'
assert_no_owned_process

run_case term-ignored term-ignored 137 base
assert_calls "$case_dir/apt.calls" update
assert_update
assert_timeout_signal TERM
assert_timeout_signal KILL
assert_no_owned_process

run_case sudo-refusal sudo-refusal 73 base
assert_calls "$case_dir/sudo.calls" update
assert_calls "$case_dir/apt.calls"

run_case command-missing command-missing 127 base
assert_calls "$case_dir/sudo.calls" update
assert_calls "$case_dir/apt.calls"

run_case unknown-profile success 2 other
assert_calls "$case_dir/sudo.calls"
assert_calls "$case_dir/apt.calls"
run_case missing-profile success 2
assert_calls "$case_dir/sudo.calls"
assert_calls "$case_dir/apt.calls"
run_case extra-argument success 2 base unexpected
assert_calls "$case_dir/sudo.calls"
assert_calls "$case_dir/apt.calls"

printf 'PASS: Linux dependency helper (13 cases; fake sudo/APT, real GNU timeout)\n'
