# 0023. Transport command queue

- **Status:** Accepted
- **Date:** 2026-06-28
- **Deciders:** Dan (owner), build agent
- **Related:** ADR-0002, ADR-0006, ADR-0022, H9 engine scaling and robustness plan.

## Context

ADR-0022 gave playback an absolute-frame transport, but H8's implementation still let control calls
mutate `playheadFrame`, loop state, and `playing` directly. That is safe only when the caller and audio
callback are single-threaded. H9 needs concurrent control-to-audio transport changes to be race-free
without adding locks to the audio path.

## Options considered

1. **Make every transport field atomic.**
   - Pros: small patch.
   - Cons: multi-field loop updates are not atomic as a group, and every callback would observe partial
     control states unless another protocol is added.
2. **Protect transport with a mutex.**
   - Pros: straightforward ownership.
   - Cons: violates the audio-thread rule.
3. **Route transport changes through a bounded SPSC command queue.**
   - Pros: matches ADR-0006, keeps one ordered control-to-audio lane, and makes the audio thread the sole
     owner of live transport state.
   - Cons: commands apply at the next block boundary and callers must handle a full queue. Accepted.

## Decision

`PlaybackEngine` uses a bounded control-to-audio SPSC command queue for `play`, `stop`, `locate`,
`setLoop`, and `clearLoop`.

- Control calls validate synchronously, enqueue a small POD command, and return `false` if the queue is
  full.
- The audio callback drains a bounded number of transport commands at the top of each block, before
  deciding whether to play, stop, locate, or split at a loop boundary.
- Live transport fields are audio-thread-owned after construction. Debug getters are for stopped or
  externally synchronized inspection.
- FIFO ordering is the coalescing rule for now: later commands win by being applied later. A future UI
  can coalesce repeated locates before enqueueing if needed.

## Consequences

- **Positive:** concurrent control and audio transport operation is TSan-checkable and lock-free on the
  callback.
- **Positive:** H8's sample-accurate locate/loop behavior remains observable at block boundaries.
- **Negative / accepted costs:** a saturated command queue rejects control changes instead of blocking;
  callers must retry or drop stale UI gestures.
- **Follow-ups:** if scrub UI sends very high-rate locates, add control-side last-locate-wins coalescing
  before the SPSC queue.
