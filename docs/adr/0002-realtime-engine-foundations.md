# 0002. Real-time engine foundations

**Status:** accepted · 2026-06-23 · Deciders: Dan

Both research reports (`docs/research/`) agree on a handful of audio-engine rules that are cheap to
follow from the start and a near-rewrite to retrofit. We adopt all of them now, independent of the
still-open choices about engine language, UI stack, and product wedge.

## The rules

1. **The audio thread never blocks.** The code that fills the output buffer must not allocate or free
   memory, take locks, log, or do file/network I/O. Anything slow happens on the control thread and is
   handed over without making the audio side wait. (Missing the deadline = an audible click or dropout.)

2. **Routing is a one-way graph (DAG) with delay compensation built in.** Signal flows forward only,
   never looping back on itself. Every node reports how much delay it adds, and the engine lines up
   parallel paths so they stay in time — even while every built-in node still reports zero delay. The
   machinery exists from day one because adding it later means rebuilding the graph.

3. **One format-neutral node contract.** Built-in tools and third-party plugins implement the same
   interface, so adding plugin hosting later is an adapter, not an engine rewrite.

4. **Events are sample-accurate and block-sliced.** Parameter changes and notes carry an exact
   position inside the processing block, so timing is precise from the start rather than "roughly right."

5. **UI ↔ audio communication is lock-free.** The screen and the audio engine exchange data through
   wait-free hand-offs (commands one way; meters and playhead the other) so neither side ever blocks
   the other.

## Why record this

These shape the entire engine and can't be cheaply reversed. A future reader who sees delay
compensation wired in while every node reports zero delay should know it's deliberate, not accidental.
This ADR decides *only* these invariants — not the engine language, UI stack, plugin strategy, or
product wedge.
