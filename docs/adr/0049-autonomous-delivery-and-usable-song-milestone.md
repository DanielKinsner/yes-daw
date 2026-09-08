# 0049. Autonomous delivery toward a usable-song milestone

- **Status:** Accepted
- **Date:** 2026-09-08
- **Deciders:** Dan (accepted the targeted plan revision and requested keeping a human out of the
  loop as much as possible), planning agent
- **Amends:** ADR-0046's phase ordering, checkpoint advancement, anti-wander rules and Enter handling
  during logical control navigation; ADR-0037's
  owner-only verification operation and H18 sequencing. All other clauses remain in force.
- **Preserves:** ADR-0002 engine invariants, ADR-0010 original-asset/read-boundary conversion,
  ADR-0015 plugin isolation contract, mechanical verification and append-only measurement evidence.
- **Plan:** `docs/plans/2026-09-01-real-daw-ground-up-plan.md`

## Context

The September shell plan correctly prioritizes editing and MIDI, but its execution contracts have
drifted: the EQ checkpoint passed while SS-1 through SS-3 still failed New/Import setup. Those
failures were recorded honestly, but the blanket parking-lot rule prevents restoration of the very
journeys every checkpoint promises to prove. G4 also makes real plugin hosting a dependency of G5's
import/export improvements. The late polish and recording phases lack small execution units.

Dan chose a targeted revision, preserving the full DAW and stack, with as little human involvement
as possible. A human is not the default code reviewer, UI tester, checkpoint approver, or operator
of a command an agent can run. Physical equipment and an interactive desktop still impose real
constraints; automation cannot manufacture evidence or silently take over Dan's input.

## Options considered

1. **Keep the current checkpoint stops and scope order.** Least documentation change, but leaves
   repeated owner interruptions and permits feature progress without restored basic workflows.
2. **Autonomous, evidence-gated milestones (chosen).** Restore workflow proof, finish a usable
   built-in song path before plugin deepening, and let agents advance within authorized scope.
3. **Unbounded loop with advisory gates.** Fewer interruptions, but makes a passing result optional
   and allows uncertain failures to accumulate. Rejected.

## Decision

### A complete product path before plugin deepening

Preserve item identifiers and delivered history. Execute the remaining arc in this order:

1. G4.0a restores SS-1 through SS-3 evidence; G4.0b proves shared keyboard control navigation.
2. Complete G4.2 through G4.7: built-in FX, sends, sidechain, automation and master controls.
3. Complete G5 project lifecycle and G6 polish/accessibility; close the **Usable-song milestone**.
4. Complete the separate G4.8/H18 plugin milestone, with its existing recorded real-VST3 worker
   smoke PASS as an entry condition, before plugin-feature implementation.
5. G7 recording, then G8 alpha distribution. Editing and MIDI still precede recording.

The Usable-song milestone is a packaged application in which an agent can create/import a mixed
audio/MIDI song, edit and mix with built-ins, save/reopen, export, and recover from interrupted work.
It requires the complete applicable session journeys, feel budgets, agent visual rubric and actual
hardware-playback evidence. It is not alpha and does not claim recording or third-party support.
Missing hardware evidence blocks its certification, not unrelated safe preparation of later work.

### Autonomous execution within a stated boundary

A build/continue request defaults to the current named milestone in STATUS unless Dan states a
smaller scope. Agents choose small committable steps, perform a separate critic pass, repair
failures, update evidence, and advance through covered checkpoints and phases without asking for
approval at each boundary. Stop at the requested scope's finish line, an actual external blocker,
or the host's execution/budget limit. A completed milestone is reported; starting the next one
requires an existing mandate covering it. A documentation request alone starts no implementation,
goal, scheduler, automation, or later background work.

Implementation ADRs explicitly scheduled by this plan may be authored and accepted by an agent
with a separate critic pass when their choices remain inside accepted product/engine contracts.
Replacing an owner-settled decision still requires the concrete human decision described below.

Local checks precede the code commit and push. Since CI runs on pushed commits, exact-code-SHA CI
completion precedes checkpoint completion and dependent advancement. Red receives corrective
commits; a later docs-only green run never substitutes for code verification.

An earlier-phase product regression or broken verification required by the active milestone is
in scope to repair immediately. New unrelated features and audits remain parked. Unknown failure
cause is blocking uncertainty, not evidence of a tooling flake. Confirmed harness/environment
failures require repair and a restored journey. Accepted runner noise is a named, evidenced,
bounded exception with a re-evaluation condition; it never changes a raw FAIL to PASS. The existing
macOS `YesDawTimelineGpuCheck`-only frame-budget exception is retained without reruns, pending the
runner-floor investigation specified in the plan.

After three unsuccessful corrective attempts on the same failure, a separate agent critic must
reassess the evidence and approach. Continue only on a new supported hypothesis, not repeated
attempts to obtain a lucky pass. If useful progress is exhausted, record the concrete blocker and
stop dependent work. A red counter alone is not a reason to ask Dan to debug.

### Verification without routine owner work

Agents run headless checks, app drives, screenshot review and available one-command hardware
checks. Before unattended app input, verify a separate machine/VM/logon-input session that cannot
steal Dan's foreground focus or mouse. A hidden app or another Windows virtual desktop alone is
not proof of isolation. On a shared desktop use an already-authorized hands-off window; one window
covers the agreed batch, not a new question per script. Otherwise keep UI drives pending while
performing safe work; request one concise access/window decision only when it blocks progress.

Only the measurement script generates hardware PASS/FAIL evidence; agents may invoke it and commit
its genuine output. No manual listening/eyeballing, synthetic substitute, edited result row or
silent threshold change earns hardware credit. Missing devices, loopback routing or an approved
plugin fixture are access dependencies. Check available resources first; ask only for the missing
physical/access action. Real-plugin testing requires explicit bounded fixture selection; this
decision does not authorize arbitrary plugin installation or purchases.

Human input is reserved for a consequential change outside accepted product/architecture decisions,
unavailable access/equipment that only the owner can supply, or an action outside granted authority
(such as spending, public release, destructive user-data operations). Prepare the reviewable result
and a recommendation before asking. Reversible repairs, review, visual-rubric judgment and ordinary
implementation choices stay with agents.

### Keyboard access through the existing command router

Keep native widgets from consuming DAW shortcuts. Add a **Control target** owned by the command
router, distinct from Arrange/Piano roll/Mixer **Focus context**: a visible logical target with
traversal order, activation/adjustment and accessibility semantics. Entering navigation through Tab
or an accessibility targeting action activates the Control target; returning to an editor canvas
ends that navigation. Key priority is active text entry, Space transport, Control-target Enter /
Esc / Tab navigation, other global transport, active control adjustment, then editor-context
commands. Enter activates/confirms the Control target while navigation is active; otherwise it
retains Return to zero. A key dispatches once. `Space` remains transport outside text entry; `Esc` cancels control interaction without
changing selection or unexpectedly dispatching an editor action. Control removal/closure restores
a valid target; changes use the existing command/undo path. This extends reachability without
reversing ADR-0046's no-widget-shortcut rule. Prove the contract before extending more FX editors;
G6 verifies whole-shell coverage.

## Consequences

- Dan gets completion and blocker reports instead of routine permission requests or manual tests.
- A broken core journey can interrupt feature work; this is required to make product progress real.
- The full DAW scope is retained, with an earlier separately named usable product milestone.
- Automation depends on genuine input isolation and hardware access. Planned infrastructure is
  not treated as already present; missing evidence remains visible and prevents certification.
- Old ADRs and measured results stay as history. This record and the revised active plan govern
  only the amended operational/ordering clauses; new irreversible decisions still need an ADR.
