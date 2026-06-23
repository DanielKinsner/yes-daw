# YES DAW — STATUS (live handoff)

**Read this first on any machine.** This is the single source of truth for *where we are right now*.
The [plan](docs/plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md) and
[roadmap](docs/goals/roadmap.md) are the stable reference; **this** file is the live, constantly-updated
worklog.

> **Cross-machine rule:** `git pull` at the start of a session. At the end, update this file, commit in
> small chunks, and `git push`. Then the next machine — or the next session — is never lost.

**Last updated:** 2026-06-23
**Current horizon:** Planning → next is **H0 (spikes)**

---

## Now — the one small task in flight
- **Next agent's first job: make verification mechanical** — stand up CI + a self-asserting check
  harness so no step needs Dan to read code, listen, or watch. Then continue the spikes (each with its
  own automated check). Dan is non-coder + busy: CI green is the gate, not him.

## Current-horizon checklist — H0 (plain English, small steps)
- [x] Install the C++ toolchain (CMake + MSVC via VS 2022 Build Tools). ✓
- [x] `cmake -B build` configures and fetches JUCE with no error. ✓
- [x] App builds and a window opens (`YesDaw.exe`). ✓ — *`Main.cpp` compiled clean first try.*
- [x] A 440 Hz tone plays out real hardware (spike #1: device round-trip core). ✓
- [ ] **Stand up CI + a self-asserting check harness** — follow `docs/ci-mechanical-verification.md`
  (it has the starter `ci.yml`, `CMakePresets.json`, the RTSan job, `tools/soak.sh`, ADR-0005, and the
  green-small / direct-to-main commit rule). The gate is a green check, not Dan. *(first task)*
- [ ] Tame the spike (fade-in / lower level / start-stop) so a local run isn't a jumpscare.
- [ ] Load + scrub one WAV — verified by a golden-output check (finish spike #1).
- [ ] GPU timeline draws 100+ elements at 60fps (spike #2) — asserted by a frame-time budget, not eyeballed; decide native vs WebView.
- [ ] One Node behind a stub of the format-neutral trait (spike #3).
- [ ] **Exit:** mechanical gates green (CI) + a one-machine self-check script passes the real-hardware soak → H0 done. *(no human judgment)*

## Done recently
- 2026-06-23 — **Foundation** committed: research corpus, CONTEXT glossary, ADR-0001/0002, roadmap, CLAUDE.md.
- 2026-06-23 — **Brainstorm**: direction locked — full general-purpose DAW; C++/JUCE + our own engine;
  audio + MIDI co-equal; linear timeline; editing-first; long-horizon.
- 2026-06-23 — **Plan** written; ADR-0003 (product) + ADR-0004 (stack); roadmap rebuilt; docs reconciled.
- 2026-06-23 — **Deepen-plan** applied: deepening-notes companion; loops section; decision #14
  (sample-rate); 10 simplifications adopted (8 scope-cuts rejected — full scope kept); housekeeping.
- 2026-06-23 — **Loop workflow adopted in full**; **3 H1 conflicts resolved** (15360-tick grid /
  128-bit ULID / out-of-process hosting).
- 2026-06-23 — **Codex plan review applied** (all 7 findings, no scope cut): made the snapshot /
  state-ownership / graph-publication model exact; promoted bundle crash-recovery into H1's gate;
  fleshed the out-of-process host runtime + isolation gate; PDC test now covers automation + events;
  sample-rate → H1 + automation-curve added as decision #15; fixed stale docs (adr/README, CLAUDE.md).
- 2026-06-23 — **Codex review round 2 applied:** plugin-IPC nonblocking contract (audio thread never
  waits on a child — one-block pipeline + fail-open); per-run state arenas (RT vs offline never share
  state); fixed persistence contradiction; H1 recovery gate = save/migration (import-kill → H2);
  marked resolved conflicts historical.
- 2026-06-23 — **H0 kickoff:** committed CMake + JUCE scaffold + sine spike (`src/Main.cpp`), `AGENT.md`,
  `.gitignore`. Unverified until the toolchain is installed and it's built.
- 2026-06-23 — **H0 spike #1 core WORKING:** toolchain in (MSVC 19.44 / CMake), JUCE fetched + built,
  `Main.cpp` compiled clean **first try**, `YesDaw.exe` plays a 440 Hz sine out real hardware. Full
  stack proven end-to-end.
- 2026-06-23 — **Mechanical-first model + CI cheat-sheet** committed (`docs/ci-mechanical-verification.md`)
  + `bootstrap/windows.ps1` (idempotent one-command toolchain install; fixes the winget-quoting pain).
  Standing up CI is the agent's first H0 task. Commit rule: frequent, straight to main, no squash.

## Next
- ✅ **Agentic-loop workflow: adopted in full** (activates at H1).
- ✅ **3 architecture conflicts resolved** (2026-06-23): time = **15360**-tick grid · stable IDs =
  **128-bit ULID** · plugin hosting = **out-of-process / sandboxed** from the start.
- **Begin H0 spikes** (next build step): device round-trip, 60fps GPU timeline, one Node behind the trait stub.

## Blocked / open threads
- Engine concurrency model (plan's *Threading & the real-time boundary* + *The graph* sections) is out
  for a **Codex re-verify** pass. H0 does not depend on it, so H0 proceeds in parallel.
