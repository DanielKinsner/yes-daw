# AGENT.md — build / run / test reference

Per-OS commands to build and check YES DAW. The agentic loop (from H1) reads this; humans use it too.
Keep it current as the toolchain evolves.

## Prerequisites (install once)

**Windows** — Visual Studio 2022 Build Tools with the "Desktop development with C++" workload (gives
MSVC), or VS 2022 Community; plus CMake ≥ 3.25 and Ninja. Via winget:

```
winget install Kitware.CMake
winget install Ninja-build.Ninja
winget install --id Microsoft.VisualStudio.2022.BuildTools -e
```

Then add the C++ compiler **via the GUI** (the `--override "..."` one-liner is fragile across
cmd/PowerShell quoting): open **"Visual Studio Installer"** from the Start menu → on **Build Tools**
click **Modify** → check **"Desktop development with C++"** → **Install**.

The first `cmake` configure downloads JUCE (pinned in `CMakeLists.txt`) — needs internet.

## Configure + build (Debug)

Simplest on Windows — the **Visual Studio generator** finds MSVC automatically (no special prompt):

```
cmake -B build
cmake --build build --config Debug
```

**One dir per purpose — never share a dir across generators.** This convenience build owns `build/`
(VS generator); the `ci` preset (below) owns `build-ci/` (Ninja); the RTSan leg owns `build-rt/`; the
Ninja-Debug iteration below owns `build-ninja/`. Mixing two generators in one dir is what triggers
CMake's "generator does not match the generator used previously" error — keeping them separate makes
that impossible, so any of these can be run freely without wiping a dir first.

Faster iteration with **Ninja**, but MSVC + Ninja needs the compiler env, so run these from the
**"x64 Native Tools Command Prompt for VS 2022"** (Start menu):

```
cmake -B build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-ninja
```

### Windows: loading the MSVC env in a NON-interactive shell (agents / scripts)

The Ninja and `--preset ci` builds need `cl.exe` on PATH; an interactive "x64 Native Tools prompt"
isn't available to an automated shell. Load the env programmatically, and **re-run it in the same shell
invocation as the build** (env does not persist between separate tool calls), e.g. in PowerShell:

```powershell
$vs = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"   # Community? swap "BuildTools"->"Community"
Import-Module "$vs\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vs -DevCmdArguments '-arch=x64 -host_arch=x64' -SkipAutomaticLocation | Out-Null
$env:Path = "C:\Program Files\CMake\bin;" + $env:Path   # winget CMake/Ninja may not be on the dev-shell PATH
Set-Location "<repo>"
cmake --preset ci; cmake --build --preset ci; ctest --preset ci
```

Notes: a harmless `vswhere.exe is not recognized` line may print during the DevShell import — ignore it.
If the VS path differs, find it with `& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`.
Locally on Windows you can only run the **MSVC** build + ctest; the **RTSan/TSan** legs need Clang 20
(Linux) — they are CI-only, so push and let CI be the gate for those.

## Run

```
build\YesDaw_artefacts\Debug\YesDaw.exe            # exact path varies by generator/config
```

## The mechanical gate (build + test on every machine, byte-identical to CI)

Everything runs through the `ci` CMake preset so the cloud and all three machines invoke it the same:

```
cmake --preset ci          # configure (Ninja, Release) into build-ci/ — needs the MSVC env on Windows (x64 Native Tools prompt)
cmake --build --preset ci  # build the app + the headless test
ctest --preset ci          # run YesDawCheck — exit 0/1, the gate
```

The preset builds into its own `build-ci/` (not `build/`), so the Debug VS-generator quick-build above
can coexist with it — neither command ever errors with a "generator does not match" mismatch.

`ctest` runs the headless `YesDawCheck` (golden + 440 Hz pitch + level + purity + fade + perf). A
green run IS the verification — no listening. After an **intentional** DSP change, re-bless the golden:
`cmake --build --preset ci --target bless-goldens` (path-free — targets the preset's build-ci/).

## Real-machine soak (the H0 exit gate — needs real hardware)

CI can't open an audio device, so the one physical check is a self-asserting script:

```
tools/soak.sh 600              # 10 min on the default device; PASS/FAIL + exit 0/1, no listening
tools/soak.sh 600 --loopback   # also assert sound physically came out at 440 Hz (wire out->in first)
```

## Gates (live now; more arrive with the engine at H1)

Live at H0: build matrix (Win/Linux/mac), Catch2 self-check via `ctest`, golden render-diff, RTSan
(`-fsanitize=realtime`, Clang 20 leg), warnings-as-errors, the real-machine soak. Arriving at H1 as the
engine lands: PDC impulse, property-based undo, save/load round-trip, migration fixtures. This green is
what the agentic loop commits against. See ADR-0005 and `docs/ci-mechanical-verification.md`.
