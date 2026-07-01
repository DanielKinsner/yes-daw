# yes-daw — Fable session cook card

> Add-on to [`yes-daw.md`](yes-daw.md) + [`framework.md`](framework.md) + the shared **Constitution**
> (`…/yes-master/docs/fable-planning/00-FABLE-SUITE-CONSTITUTION.md`). Your 5-stage pipeline is the
> strongest of the four — keep it. This card is only the stuff **Fable can't derive from the code**:
> your intent, and where to let it think big. No bug lists — Fable's Step Zero reads HEAD itself.

## What only you know (feed this; it's not in the code)

- Solo builder. **Mechanical-verification-only culture:** you can't review diffs by eye, so every
  checkpoint must be a self-asserting test (exit 0/1); the only human check allowed is a
  one-command visual/audible feel smoke.
- The stack (own engine, C++/JUCE) is a **deliberate, sunk-cost decision** — you are not
  relitigating it. But you *do* want to know if any still-open decision quietly inherits that
  bet's risk.
- This is one app in a family; yes-daw is the technical odd-one-out (C++/JUCE, hosts third-party
  plugins), so it may not fit the family's shared Rust/Tauri infra.

## The one instruction that beats any hand-written status

You have the repo — trust the code over every doc, including your roadmap, your ADRs' "still
open" notes, and this brief. Reconstruct the true state (what's built vs what's only planned)
before you plan.

## Cook here — think past my framing

- Is a **general-purpose DAW** the right ship target for a solo builder, or a scope trap? If this
  were yours, what's the "minimum lovable DAW" cut that ships years sooner and still delights —
  and what would you cut to get there? (Your own research flags full scope as the solo-dev
  ship-killer; stress-test that against where you actually are.)
- Where does the **mechanical-only philosophy hit its ceiling** as the app gets subjective (feel,
  playability, mix taste), and what would you put in its place at the last mile?
- The 5 decisions between here and shippable you'd most regret getting wrong — and which are
  one-way doors accruing risk *right now* (e.g. anything that touches a real user's saved
  project).

## Cross-repo note (not a dependency)
When yes-daw reaches signing/packaging, yes-master already has a playbook — but daw is C++/JUCE,
not Tauri/Rust, so expect to adapt rather than copy. Not a blocker for planning the app itself.
