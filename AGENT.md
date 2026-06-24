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

Faster iteration with **Ninja**, but MSVC + Ninja needs the compiler env, so run these from the
**"x64 Native Tools Command Prompt for VS 2022"** (Start menu):

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## Run

```
build\YesDaw_artefacts\Debug\YesDaw.exe            # exact path varies by generator/config
```

## The mechanical gate (build + test on every machine, byte-identical to CI)

Everything runs through the `ci` CMake preset so the cloud and all three machines invoke it the same:

```
cmake --preset ci          # configure (Ninja, Release) — needs the MSVC env on Windows (x64 Native Tools prompt)
cmake --build --preset ci  # build the app + the headless test
ctest --preset ci          # run YesDawCheck — exit 0/1, the gate
```

`ctest` runs the headless `YesDawCheck` (golden + 440 Hz pitch + level + purity + fade + perf). A
green run IS the verification — no listening. After an **intentional** DSP change, re-bless the golden:
`cmake --build build --target bless-goldens`.

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
