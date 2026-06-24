# Set up YES DAW on a new computer

Plain steps to clone, build, and run the check on a fresh machine. CI already builds + tests in the
cloud on every push (the green **CI** badge in the README is the real gate), so these local steps are
only for when you want to build/run on your **own hardware** — e.g. to hear it or run the real-hardware
check.

## What you need (Windows)
**You install nothing by hand** — the `bootstrap` script (Step 2) installs every one of these for you.
The list looks short *because* the bootstrap does the work. Here's what it installs:
- **Git** — to clone the repo. *(One catch: you need git already, to clone in Step 1. If `git` isn't
  found, run `winget install Git.Git` once, then continue.)*
- **CMake** (≥ 3.25) — the build tool.
- **Visual Studio 2022 Build Tools** — the C++ compiler (MSVC) **+ the Windows SDK** (the bootstrap now
  pulls both explicitly). Multi-GB download — the **first run takes a while**.
- **JUCE** — *not* a manual install; CMake downloads it automatically on the first configure (needs internet).

## Steps (Windows)
1. **Get the code** — in a terminal, wherever you want it to live:
   ```
   git clone https://github.com/DanielKinsner/yes-daw.git
   cd yes-daw
   ```
2. **Bootstrap** — installs the toolchain + builds, one command:
   ```
   powershell -ExecutionPolicy Bypass -File bootstrap\windows.ps1
   ```
   First time on a machine this installs Visual Studio Build Tools (big — let it finish). Safe to
   re-run: it skips anything already installed.
3. **Run the check** — the mechanical gate, no listening needed:
   ```
   ctest --test-dir build -C Debug --output-on-failure
   ```
   ✅ Success = **`100% tests passed`**.
4. **Run the app** (optional — to actually hear it):
   ```
   build\YesDaw_artefacts\Debug\YesDaw.exe
   ```
   A window opens and a 440 Hz tone plays.

## How you know it worked (no code-reading)
- Step 3 prints **`100% tests passed`** → the build + DSP check pass on this machine. **or**
- The README's **CI badge is green** → it also builds + passes on Windows/macOS/Linux in the cloud.

Either is proof on its own — nothing to judge by ear or eye.

## macOS / Linux (brief)
- **macOS:** `xcode-select --install` + `brew install cmake`, then
  `cmake -B build && cmake --build build && ctest --test-dir build --output-on-failure`.
- **Linux:** install clang/gcc, cmake, ninja + JUCE's apt deps (the exact list is in
  `.github/workflows/ci.yml`), then the same cmake/ctest commands.
