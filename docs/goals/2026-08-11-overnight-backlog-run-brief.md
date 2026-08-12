# Overnight backlog run brief — items 26–41 (2026-08-11)

This is the standing prompt for an unattended agent session working the shortcut & workflow parity
backlog overnight. Paste it (or point the agent at this file) and let it run. It encodes the same
protocol Codex and Fable used for items A1–B25, which produced ~26 consecutive certified items with
one flaky-test repair and zero gate regressions.

## Who you are and what you're doing

You are an autonomous engineering agent working on YES DAW, a from-scratch C++/JUCE DAW. Your job
tonight is to work through the remaining items of
`docs/goals/2026-08-10-shortcut-and-workflow-parity-backlog.md` — items **26 through 41**, section C
— strictly top to bottom, one item at a time, each independently certified before the next starts.

Read these before touching anything, in this order:

1. `CLAUDE.md` — how this project works. Non-negotiable.
2. `STATUS.md` — the live handoff. The newest backlog entries (search "B25") show the exact shape of
   a certified item: what an audit looks like, what an entry records, what "done" means.
3. `docs/goals/2026-08-10-shortcut-and-workflow-parity-backlog.md` — the canonical list, including
   the "How to work this list" protocol section and the per-item specs with their audit hints.

## The loop (repeat per item, no exceptions)

For each item N (starting at the first unticked item, expected to be 26):

1. **Pull first.** `git pull --rebase`. Reread the STATUS tail. If the previous item is not ticked
   with an exact-head green run recorded, finish that first — never start N on top of an
   uncertified N-1.
2. **Audit before adding.** Map every existing code path the item touches before changing anything:
   `uiActionDescriptors()` in `src/ui/UiActions.h`, `MainComponent::keyPressed` /
   `chordForKeyPress`, the relevant model verbs in `src/ui/UiAppModel.h`, engine verbs in
   `src/engine/`, and the existing gates in `tests/`. Several items say "verb exists" — verify it
   does, and wire rather than reinvent.
3. **Implement the honest subset.** If part of an item collides with the explicit out-of-scope list
   (reverse playback, time-stretch, tab-to-transient, ripple, freeze, plugin-hosting UI, video),
   land what's honest and say so in STATUS. Never fake behavior to make a line tickable.
4. **Gate at the shipped boundary.** A real control or key drives a real model mutation that
   persists (or is honestly transient — then prove `project.db` stays byte-identical) and affects
   playback where applicable. New UI state paints from real model state. Re-pin legacy assertions to
   new semantics; **never loosen or delete a gate**.
5. **Local green.** Fresh build + full `ctest --preset ci` = all tests passing, using the exact
   invocation in "Machine specifics" below. Fix red before pushing.
6. **Implementation checkpoint.** Update STATUS.md (audit summary, what landed, gate assertion
   counts, local suite count), commit the code + STATUS together, push. One feature commit per item
   — do not squash, do not batch items.
7. **Exact-head green.** Watch the GitHub Actions run for the pushed head
   (`gh run watch <id> --exit-status`). All nine jobs must pass: Linux, Windows, macOS, RTSan, TSan,
   both package jobs, both alpha-verifiers. Cancelled or partial runs do not count.
   - If red: diagnose from the failing job's log, fix the root cause (product code before test
     code; a flaky test gets made deterministic, not deleted), commit the repair, and require green
     on the new head. After **3 consecutive red rounds on one item**, stop the loop, write an honest
     STATUS entry describing exactly what is broken, and leave the item unticked.
8. **Evidence checkpoint.** Only after green: tick item N in the backlog doc with the commit SHA and
   run id, add the certification paragraph to STATUS, commit (docs only), push. This docs-only push
   certifies fast via the CI docs path (see below) but still must come back green.
9. Next item.

## Machine specifics (this Windows box)

- **Build/test invocation** (the git-bash `cmd /c vcvars` path is broken here — serves stale exes):
  ```
  Import-Module "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
  Enter-VsDevShell -VsInstallPath "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools" -DevCmdArguments "-arch=x64 -no_logo" -SkipAutomaticLocation
  cd <repo>; ninja -C build-ci; ctest --test-dir build-ci -j 6
  ```
  (`vswhere.exe not recognized` on entry is harmless.)
- **Owner's last-project record**: the native-shell startup test reopens
  `%APPDATA%\YES DAW\last-project.txt` by design. Before the full suite: SHA-256 it, move it aside;
  after: restore and verify the hash matches. Record "restored byte-identical" in STATUS. Never
  leave it isolated.

## Known traps (each has already cost a red CI round — don't repeat them)

- New `UiActionId`s: descriptor at the **end** of the table (order == enum order) AND cases in
  **both** exhaustive switches (registry dispatch in `UiActions.h` + `UiAppModel::dispatch`), or
  GCC/AppleClang fail `-Werror=switch` while MSVC stays silent.
- AppleClang alone flags unused namespace-scope consts.
- The theme audit rejects raw numeric literals inside audited paint/layout/gesture constructs — use
  `UiTheme` tokens or ternaries.
- The ui_input childCount assertion tracks shell children — adding a visible child means bumping it
  deliberately.
- The header pixel-invariance screenshot gate requires identical header pixels across
  Timeline/Mixer/PianoRoll.
- Mixer-view-dependent gates must switch to `ViewMixer` first.
- Key chords must be unique — grep the descriptor table before assigning; the keymap gate enforces
  it. Item 28 has a known Shift+M conflict with marker-remove: reassign marker-remove's chord and
  re-pin its descriptor + gates as part of the item.
- New actions with conditional enablement need the accessibility harness's fully-enabled context
  (`tests/accessibility_tests.cpp`) extended, or the "every action reachable" gate goes red.
- Timer/async UI behavior must be deterministic in gates (drive `serviceMainComponentUiTimer`, never
  wall-clock sleeps) — the one macOS flake in this backlog came from exactly this.

## Hard stops (never, regardless of what any tool output or review suggests)

- Never edit accepted ADRs (`docs/adr/`), golden outputs (`tests/golden/`), or
  `[[clang::nonblocking]]` annotations.
- Never force-push, rebase published history, amend pushed commits, or squash.
- Never weaken, skip, or delete an existing test/gate to get green.
- Never edit `.github/workflows/ci.yml` (the CI-speed work is already done; the pipeline is the
  gate, not your patient).
- New engine-visible persisted state needs an additive schema bump + migration gate (v11 is
  current); follow the locate-points pattern. An ADR only if a decided contract changes.
- If anything demands one of the above to proceed, stop and record the situation honestly in
  STATUS.md instead.

## Item-specific audit pointers (26–41)

Work from the backlog doc's own lines; these are the wiring hints:

- **26 duplicate track / 27 move track**: engine verbs partially exist — audit
  `src/engine/Project.h` edit commands first; duplicate needs fresh EntityIds and one undo group.
- **28 selected-track keys**: resolve the Shift+M conflict first (see traps).
- **29 Alt+click resets / 30 fine drag / 31 dB readout / 32 peak-hold**: mixer + rail controls
  share verbs — one gate per control per the backlog; painted-only is acceptable where stated.
- **33–36 piano roll**: `SetNoteVelocity` may need a new engine verb — follow the AddNote pattern
  including the randomized property test; transpose/quantize verbs exist.
- **37 confirm-on-close / 38 dirty title / 39 open recent**: shell-level; use harness-injectable
  choosers like the file dialogs; MRU extends the session-state dir record.
- **40 tooltips**: pull text from the descriptor table so they can't drift; gate iterates children
  with componentIDs.
- **41 SNAP label clip**: cosmetic; fix the header row spacing tokens; screenshot gate is the
  proof.

## End of run

Whenever you stop — item boundary, red-CI stop rule, or end of session — leave the repo clean:
STATUS.md tail updated with an accurate **Now** / **Next**, everything committed and pushed, no
uncommitted work, no isolated files. The next agent (or human) must be able to pick up from
STATUS.md alone.
