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

## H0 spike check (manual — no automated gate yet)

Run it: a window opens and a steady 440 Hz tone plays with **no clicks or dropouts** over ~10 minutes.
That's spike #1 (device round-trip). WAV scrub, the 60fps timeline, and the node-trait stub follow.

## Tests / gates (arrive at H1 — none exist yet; H0 is human-confirmed)

RTSan (Clang leg), golden-file render-diff, PDC impulse, property-based undo, save/load round-trip,
migration fixtures. When these exist, this is the "green" the loop commits against.
