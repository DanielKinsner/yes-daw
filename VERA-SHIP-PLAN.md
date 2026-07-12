# YesDAW — Ship Plan

*Vera, 2026-07-12 (Fable session). Verified against `origin/main` @ 5e7c464 (CI green).*

## Honest headline
The engine is genuinely *impressive* — and it's the **furthest from ship** of the four. C++20 + JUCE 8, a hand-rolled real-time audio engine with RTSan/TSan CI legs and a 3-OS build matrix. Most of a real DAW **works** (transport, multi-track timeline w/ clip edit + crossfades, audio+MIDI recording w/ punch/comp, full mixer w/ buses/sends/sidechain, built-in EQ/comp/delay/reverb/limiter, piano roll, offline WAV export, undo/redo, autosave/crash-recovery). 39 ADRs, disciplined green-only commits. This is not a toy.

**But** — and this is the whole story — almost everything is proven **only in CI/headless.** The scary part below.

## The three things standing between here and "a musician makes a track"
1. **🔴 Reality-lane smokes: ZERO have ever passed.** No confirmed real-hardware playback, no real recording round-trip, no real-GPU 60fps window test, no real VST3 test. Risk R1 is stated in the repo itself: *"green in CI, dead on real hardware."* This is the single biggest unverified assumption in the entire project. **Until you run one real track on real hardware, we don't actually know it works.** These smokes are owner-only (an agent isn't allowed to fake a PASS row).
2. **🟡 UI not accepted.** You've rejected two visual closeout passes as off-mockup; a third (native LookAndFeel + font fix) is CI-green and awaiting your eyes. Blocks the downstream milestone.
3. **🟡 H17 Packaging/Alpha = 0% built.** No installer, no `--selfcheck`, no demo-song fixture, version still stamped `0.0.0`. Today "using it" means building from source via `bootstrap/windows.ps1`.

Deferred-to-beta (correctly parked): automation write/touch recording, real 3rd-party plugin hosting (architecture exists, never run against a real plugin), signing/notarization, auto-update, and the YES-family-as-plugins integration (waits on the sibling apps shipping solo).

## The plan (sequenced around the fact that the blockers are YOURS, not code)
**Step 1 (you, highest leverage, do before more engineering):** run **one real track** on real hardware from the dev build — record something through a real interface, edit it, mix it, export the WAV. That single session either validates the whole engine or exposes R1. Nothing else matters until this happens. Pair it with accepting/rejecting the current UI pass while you're in there.

**Step 2 (me, can run in parallel):** scaffold **H17** — the packaging script (`tools/package.ps1`), `--selfcheck` mode, version stamping (git-describe, kill the `0.0.0`), and a demo-song fixture. This is agent-doable and gets you a portable Windows build to hand testers, without waiting on the smokes.

**Step 3 (decision-gated):** decide how much **real VST3 hosting** you actually need for v1. If the built-in FX suite is enough for an alpha, you can defer all of H18 and ship far sooner. My rec: **defer 3rd-party hosting past first alpha** — it's the biggest remaining unknown and the built-in FX are already solid.

## Decisions I need from you
- **Block on the reality-lane smokes, or let me scaffold H17 in parallel while they wait for your hands?** → My rec: parallel. I build packaging; you run the one-real-track smoke when you have keyboard time.
- **Full H17 scope, or a narrower "prove one exported real track from the dev build" first?** → My rec: narrow first. Validate reality before polishing distribution.
- **Real 3rd-party plugin hosting in v1?** → My rec: no, defer.
- **Windows-only for the first packaged build?** → Recommend yes (only the Windows zip path is fully specced).

## Suite context
Ships **LAST** of the four — it's the deepest and the least-validated on real hardware. Don't let its ambition pull focus off yes-master, which is days-from-paid. YesDAW is a months-not-weeks story, and that's fine; it's also the crown jewel. Best next move for it is cheap and yours: **one real track, one real afternoon.**
