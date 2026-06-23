# docs/ — the YES DAW knowledge base

This directory is the project's memory. Every workflow we run reads from and writes to a specific
place here, so decisions, plans, and learnings never get lost in chat history.

| Folder | Purpose | Written by | Created |
|---|---|---|---|
| `research/` | The source research reports the project is grounded in. Read-only reference. | (imported) | ✅ exists |
| `adr/` | Architecture Decision Records — one file per significant, hard-to-reverse decision, plus the backlog of decisions still pending. | every architecture choice | ✅ exists |
| `goals/` | Long-horizon roadmap and goal definitions that `/goals` and `/loop` iterate against. | when horizons change | ✅ exists |
| `brainstorms/` | Output of `/workflows:brainstorm` — explored intent, approaches, trade-offs before a plan exists. | `/workflows:brainstorm` | on first use |
| `plans/` | Output of `/workflows:plan` — concrete, step-by-step implementation plans derived from an accepted brainstorm. | `/workflows:plan` | on first use |
| `solutions/` | Reusable learnings, gotchas, and post-mortems with frontmatter metadata, so future work can search past lessons. | as we learn | on first use |

## The flow between them

```
research/ ──► brainstorms/ ──► (grill: sharpens CONTEXT.md) ──► adr/ ──► plans/ ──► code ──► solutions/
                                                                  │
                                                          goals/ tracks the long arc across all of it
```

A decision is "real" once it has an ADR. A plan is "ready" once the ADRs it depends on are accepted.
Code is "ready to write" once its plan exists. This ordering is the whole point — see the root
[`README.md`](../README.md) for why.
