# 0025. Blacklist-on-failure action

- **Status:** Accepted
- **Date:** 2026-06-28
- **Deciders:** Dan (owner), build agent
- **Related:** ADR-0013, ADR-0015, H9 engine scaling and robustness plan.

## Context

ADR-0015 says a plugin crash or watchdog timeout escalates to bypass/recompile and persistent blacklist.
H3 proved the process boundary, fail-open audio path, placeholder swap, and blacklist table, but the
review debt was the exact action that ties a failure report to a persisted blacklist row.

## Options considered

1. **Only persist blacklist rows during scanning.**
   - Pros: keeps runtime simpler.
   - Cons: a plugin that fails during playback can be retried on restart and fail again. Rejected.
2. **Persist on every audio-thread miss.**
   - Pros: aggressive quarantine.
   - Cons: audio thread must never write to disk, and transient late Blocks are not necessarily plugin
     failures. Rejected.
3. **Persist on coordinator-classified crash or watchdog failure.**
   - Pros: keeps persistence on the control side and uses the existing failure classification. Accepted.

## Decision

The plugin host coordinator may turn a classified crash or watchdog timeout into one control-thread
blacklist action:

- write `{format, plugin_uid, plugin_version, reason}` to `plugin_blacklist`;
- enqueue bypass/recompile through the existing graph-change action;
- keep the audio thread fail-open and disk-free.

## Consequences

- **Positive:** a real host failure can now be mechanically proven to survive restart as a blacklist row.
- **Negative / accepted costs:** a failure without a stable plugin identity cannot be persisted; it still
  gets bypass/recompile for the live graph.
- **Follow-ups:** scanner UI can surface the blacklist reason and allow explicit user removal later.
