# Research census: bulk primitive clarification

This clarifies DE33D91712372C2FE887A02FB137AA9A96C8C3BB93CB0D88BF57AA32DBEAE564
under unchanged separate contract344408DA. Both records remain source-only.

The accepted preparation's library row and the selected C++ transitive-closure
map explicitly preserve the bulk CRT/OS/CLR/libm primitive CPU boundary. In
particular, an explicit SDK memcpy remains a bulk primitive; its surrounding
visible wrappers and source calculations remain expanded.

- For a declared opaque memcpy, memmove, memset, fread or corresponding existing
  bulk CLR primitive, count its evaluated caller arguments, dispatch, boundary
  entry/return and visible wrappers. Do not count n internal byte writes, invent
  a per-byte source loop, or add a new bulk-scalar fee on top of that call.
- Explicit scalar results and scalar out-parameter destinations are initialized
  once. Guaranteed direct caller-result initialization is not another transfer.
  The earlier record's out-buffer wording does not reopen bulk primitive internals.
- Fully retain byte extents, capacity, lifetime, old/new overlap and controlled
  payload obligations. An opaque CPU body grants no storage reclamation or fit.
- Visible C++ aggregate/default/per-field initialization, copy and retirement
  retain their actual source actions. Do not replace those semantics with a
  hypothetical optimized memcpy. Selected C++ SDK wrappers stay expanded; existing
  CLR/codec runtime internals retain their accepted distinct primitive boundary.
- Count an actually invoked selected special member's entry, receiver binding,
  return and real field actions. Empty bodies are not optimizer-discounted.
  Aggregate initialization that invokes no constructor gets no invented one;
  uninitialized storage gets no fabricated zero/write, and an uninvoked special
  member gets no call. Do not invent a field loop for trivial retirement.

This supplies neither prices nor a new work limit. The old R4 NOFIT, all source
seals, production policy and resource limits remain unchanged. Future independent
review must inspect actual primitive-versus-visible call incidence.
