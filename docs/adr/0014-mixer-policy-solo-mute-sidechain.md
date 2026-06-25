# 0014. Mixer policy: mute, SIP solo, solo-safe, and Sidechain pins

- **Status:** Accepted
- **Date:** 2026-06-25
- **Deciders:** Dan (owner), build agent (H3 mixer policy ADR worker)
- **Related:** ADR-0002 (#1 audio thread never blocks, #2 DAG + PDC), ADR-0006 (Snapshot publish),
  ADR-0007 (CompiledGraph + PDC + post-compile mute mask), ADR-0008 (Node contract), ADR-0009
  (Event stream + automation offsets), ADR-0010 (time model), ADR-0011 (Project identity), ADR-0013
  (PluginNode hosting boundary),
  [build plan](../plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md) H3,
  [deepening notes](../plans/2026-06-23-yes-daw-deepening-notes.md) -> *The graph* and
  *Time & event model*, `CONTEXT.md` (Mute, Solo, SIP solo, Solo-safe, Sidechain,
  Sidechain input pin).

## Context

H3 has a headless mixer projection over the frozen graph contracts: tracks compile through
`FaderNode`, `PanNode`, `MeterNode`, Send edges feed Bus `SumNode`s, and each Bus Return feeds the
master bus. The remaining mixer work needs two policy decisions before more code lands:

- how user mute, solo, Solo-In-Place (SIP), and solo-safe state becomes the post-compile mute mask
  already required by ADR-0007; and
- how Sidechain routing appears to the graph without changing ADR-0008's frozen `Node` base contract.

Current code has the low-level seam (`CompiledGraph::setMuted` / `isMuted`) but no policy for deriving
that mask from mixer state. ADR-0007 already says Sends are graph edges, PDC is computed at convergence
points including Returns and Sidechains, and the graph is not rebuilt just to mute. ADR-0009 also says
Event and automation offsets move with the same PDC as audio. These choices affect user-visible mixer
behavior and future saved Projects, so they need an ADR before implementation.

## Options considered

1. **Recompile or rewrite graph edges for mute and solo.**
   - Pros: the compiled graph could contain only the currently audible paths.
   - Cons: solo/mute would become topology churn; rapid UI toggles could force recompiles; the audio
     thread would depend on policy that ADR-0007 already placed in a post-compile mask. Rejected.
2. **SIP solo + solo-safe as a derived post-compile mute mask; Sidechain as a real graph input pin.**
   - Pros: matches the plan and research; keeps solo/mute as atomic graph state; keeps Sidechain visible
     to topo, PDC, buffer liveness, and event-offset handling; does not change `Node::process`.
   - Cons: the control side must compute effective mute points carefully, especially around Returns and
     Send contributions. Accepted.
3. **Implement PFL/AFL monitor solo first, or model Sidechain as hidden plugin parameters/events.**
   - Pros: PFL/AFL is useful for gain staging; hidden plugin-side routing is superficially small.
   - Cons: PFL/AFL needs a separate monitor bus that H3 has not designed yet; hidden Sidechains would
     bypass PDC and buffer liveness. Rejected for H3; PFL/AFL is a later monitor-section feature.

## Decision

**H3 ships Solo-In-Place (SIP) solo, not PFL/AFL.**

- SIP is the normal Solo button behavior: the main mix is heard with non-soloed, non-solo-safe targets
  soft-muted by the post-compile mute mask.
- PFL/AFL will require a dedicated monitor bus later. H3 must not smuggle monitor-bus behavior into the
  main mix path.
- Track outputs and Bus Returns are solo/mute targets. The Master bus is not a solo target. Sidechain
  input pins are not solo targets.

**Mute, solo, and solo-safe resolve to an effective mute mask on the control side.**

- Explicit **Mute** wins over Solo and Solo-safe. A muted target is silent until unmuted.
- If no unmuted target is soloed, every unmuted target is audible.
- If one or more unmuted targets are soloed, only soloed targets and solo-safe targets remain audible.
- **Solo-safe** only exempts a target from solo-induced muting. It does not override explicit Mute.
- The audio thread does not evaluate this policy. It only reads the already-published mute mask.
- The mask is over compiled mix contribution points, not Project rows. Mapping a Track or Return to the
  exact compiled Node IDs is mixer-projection work, but changing solo/mute state must not rewrite
  routing edges or allocate on the audio thread.
- If a logical target cannot be represented by mute-capable compiled contribution points, the projection
  must fail a self-asserting check rather than silently leave audio unmuted.

**Send/Return behavior under SIP is explicit.**

- A Send remains an edge from a tap point to a Bus `SumNode`; PreFader/PostFader still only means the
  tap is before or after the `FaderNode`.
- A Return is the Bus output that feeds the master bus. Returns can be solo-safe; this is how FX Returns
  stay audible while a source Track is soloed.
- A solo-safe Return does not open unrelated source Sends. Under SIP, a source's audible Send
  contribution is removed when that source is explicitly muted or solo-muted. This prevents a solo-safe
  reverb Return from leaking non-soloed Tracks into the soloed mix.
- Explicitly muting a Return silences that Return regardless of source solo state.

**A Bus Return is stereo and centered, mirroring the Track chain.**

This decision was deferred when the Send/Return graph edge first landed; the original projection summed
Send taps into a **mono** `SumNode` and wired that straight into the stereo master bus. A mono producer
fills only channel 0 of a stereo consumer, so a `Send -> Bus -> Return` was audible in the master's **left
channel only** — the right was silent. A mono signal entering a stereo master must be **centered**, the
same way the project centers a mono Track (its `PanNode` places centre at the equal-power gain ×0.707 on
both sides), not hard-left.

- A Bus `SumNode` keeps summing its (mono) Send taps in mono. The **Return then widens to stereo through
  its own `PanNode` -> `MeterNode`**, exactly mirroring the Track chain (`FaderNode -> PanNode ->
  MeterNode`), before feeding the master bus. No new Node type is introduced; the Return reuses the same
  building blocks as a Track.
- The Return's pan **defaults to centre** and uses the **same equal-power pan law as a Track**, so a
  centred Return and a centred Track share one gain convention (×0.707 L/R) and a mono Send is never
  hard-left. Per-Return pan is then a free later extension (the `PanNode` already exists).
- The Return gets a `MeterNode` like a Track, so a Return has level metering. A **Return fader is
  deferred** — Returns currently have no level control of their own; a dedicated return fader (and richer
  return routing) is a later mixer feature, not an H3 decision.
- Width beyond stereo (true multichannel Returns / Buses) is out of H3 scope; H3 Buses and Returns are
  stereo, consistent with the stereo master.

**A Sidechain input pin is a real graph input, not an audible Send/Return.**

- A Sidechain input pin is an ordered auxiliary input on a sidechain-capable `Node` / `PluginNode`. The
  main input remains the main input; sidechain pins are separate named/numbered inputs.
- A Sidechain edge is present before graph compilation so topo sort, cycle detection, PDC, buffer
  liveness, and last-reader analysis all see it. It must never be hidden inside plugin code or modeled
  as a parameter event.
- Pin roles are graph/compiler metadata or adapter-specific binding. ADR-0008's `Node` base contract
  and `ProcessArgs` shape stay frozen.
- One Sidechain pin receives one resolved stream. If multiple sources feed the same pin, they must first
  converge through an explicit `SumNode` / Bus so the convergence is visible to PDC.
- Sidechain pins are non-audible control inputs. Destination fader, pan, meter, Send, and Return policy
  do not make them audible.
- Sidechain feeds follow the effective state of their source tap: explicit Mute and SIP solo-mute
  silence the feed unless a later ADR introduces a separate key-listen or sidechain-safe monitor mode.
  This keeps soloed material from being changed by hidden non-soloed sources.

**PDC and Events treat Sidechain like any other graph dependency.**

- A sidechain-capable consumer is a PDC convergence point between its main input and every Sidechain
  input pin. GraphBuilder must delay shorter paths so the main and Sidechain signals reach the consumer
  at the same compensated sample.
- Any Event or automation stream carried with a Sidechain path is shifted by the same per-path PDC as
  that path's audio. Automation targeting the sidechain-consuming Node is evaluated at that Node's
  compensated process block.

## Consequences

- **Positive:** solo/mute stays RT-safe and cheap; solo-safe FX Returns have defined behavior; Sidechain
  routing participates in the same DAG/PDC/buffer/event machinery as normal audio; plugin sidechains
  remain additive over the ADR-0008 `Node` contract. Bus Returns are stereo-correct and centred, sharing
  one equal-power centering convention with Tracks and reusing `PanNode`/`MeterNode` rather than a new
  Node type.
- **Negative / accepted costs:** the mixer projection must compute effective mute points, not just pass
  through user flags; Send contribution gating needs explicit tests; sidechain-capable nodes need
  compiler-visible pin metadata/binding instead of a hidden plugin shortcut. Each Bus Return now carries a
  `PanNode` + `MeterNode` (cheap, control-thread build only); a dedicated return fader is deferred.
- **Follow-ups:** implement a self-asserting mixer policy gate for the effective mute table, mute-mask
  capacity/coverage, SIP Return leakage prevention, and Sidechain PDC/event-offset alignment. PFL/AFL,
  key-listen, sidechain monitor modes, Project/persistence schema shape, and UI affordances remain later
  decisions.
