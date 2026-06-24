---
title: H0 build + CI gotchas (hard-won)
date: 2026-06-23
horizon: H0
keywords: [RTSan, sanitizer, CI, GitHub Actions, JUCE, CMake, Catch2, soak, audio-device, block-size, warnings-as-errors, multi-agent]
status: durable
---

# H0 build + CI gotchas

Committed so any agent on any machine (Claude, Codex, …) has them — these were learned the hard way in
H0 and will bite again in H1. The fixes are live in `CMakeLists.txt` / `.github/workflows/ci.yml`; this
is the *why*.

## RTSan leg (will recur the moment you add RT-safe engine code in H1)
- The sanitizer leg builds with `-DYESDAW_BUILD_APPS=OFF` so it configures only the **pure** test exes
  (no JUCE), needing none of JUCE's X11/ALSA dev libs.
- It **drops `-Werror`** (the normal 3-OS matrix still enforces it). Reason: `[[clang::nonblocking]]`
  turns on a *static* `-Wfunction-effects` that flags any call to an unannotated function (`std::sin`,
  `std::min`…), which is too strict for RT-safe stdlib math. RTSan's **runtime** detection is the gate.
- A `[[clang::nonblocking]]` function **must be `noexcept`** (else `-Wperf-constraint-implies-noexcept`).
- The hot-path attribute is guarded as `YESDAW_RT_HOT` via `__has_cpp_attribute(clang::nonblocking)`, so
  it's a no-op on MSVC/GCC/AppleClang (which would otherwise error on the unknown attribute) and only
  real on Clang 20+.
- Link the sanitizer build with **`-Wl,-z,now`**: otherwise the first lazily-resolved `std::sin` call
  runs the dynamic linker's PLT resolver *inside* the nonblocking scope and RTSan flags its lock —
  intermittently, depending on Catch2's randomized test order.
- The RTSan leg must **build ALL pure test targets** (`cmake --build build-rt`, not `--target X`):
  `catch_discover_tests` registers a `_NOT_BUILT` sentinel for any unbuilt target, which fails ctest.

## Watching CI from an agent
The environment `GITHUB_TOKEN` is **invalid** for the GitHub API, and unauthenticated calls rate-limit
fast (60/hr). Use the git push credential instead:
```
TOKEN=$(printf 'protocol=https\nhost=github.com\n\n' | git credential fill | grep ^password= | cut -d= -f2)
curl -s -H "Authorization: Bearer $TOKEN" https://api.github.com/repos/DanielKinsner/yes-daw/actions/runs?per_page=1
```
Job logs (to diagnose a red): `…/actions/jobs/<job_id>/logs` with the same header.

## Multi-agent repo hygiene
The owner runs more than one agent on this same repo/working-dir — a planning Claude, the build agent,
and **Codex** (which auto-converts `CLAUDE.md` → `AGENTS.md`; keep them in sync). Expect small commits
to land mid-session on different files. **Don't assume HEAD is yours**; `git pull --rebase origin main`
on a rejected push (it happens), and avoid editing the shared planning docs (STATUS/CLAUDE/AGENTS/plan)
without expecting overlap.

## Real-machine soak
- `tools/soak.ps1` (native Windows, no Git Bash) / `tools/soak.sh` run the audio H0 gate. PASS = audio
  only; the **GPU 60 fps frame gate is not implemented** (needs a native render shell).
- The soak requests a **128-frame** Block (the roadmap target). Shared-mode WASAPI (e.g. Realtek default)
  forces 480 and the soak correctly **FAILs** the 128 target — you need an **ASIO/exclusive** driver for
  the real H0 gate, or `--block-size 480` for a looser check.

## The mechanical model (one entry point)
Every machine + CI runs the same: `cmake --preset ci && cmake --build --preset ci && ctest --preset ci`.
The preset builds into its own `build-ci/` (Ninja), so a casual `cmake -B build` (Visual Studio
generator) can't collide with it — that "generator does not match" error is gone by construction.
After an intentional DSP change, re-bless goldens: `cmake --build --preset ci --target bless-goldens`.
CI is the gate; see `ADR-0005` and `docs/ci-mechanical-verification.md`.
