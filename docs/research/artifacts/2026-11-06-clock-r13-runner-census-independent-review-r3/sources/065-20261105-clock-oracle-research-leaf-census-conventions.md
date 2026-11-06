# Research census: declared primitive call boundary

Source-only clarification of separate contract344408DAEC2BD355D44B0B6DA266BF00119B09DF5028B70524699E8E2CD36010.
This is the research scalar convention, not production ADR0085's logical primitive
model or a reinterpretation of any historical certificate.

- Count evaluated caller arguments/reference formation and dispatch, plus the
  declared primitive boundary's entry and return. An actually reached visible
  wrapper has its own entry/body/return; do not merge it with the primitive.
- Existing declared CRT/OS/libm/intrinsic and CLR runtime/JSON implementation CPU
  remains opaque. Public implementation source does not expand that boundary.
  Selected C++ SDK and all newly authored helper bodies remain expanded.
- External scalar/result/out-buffer destination writes occur once. A guaranteed
  result initialized directly into the caller destination is not another value
  transfer or second destination write. Caller evaluation supplies its value read;
  actual parameter initialization supplies its write, without a second input read.
- Reference arguments copy no referent cells. Actual reference formation/binding
  and implicit receiver actions are accounted once under the research convention;
  generic forwarding does not manufacture a second referent copy.
- All controlled payloads, buffers, clones, capacities, preprocessing and retained
  owners still need their complete lifetime worksheet. Primitive CPU/general CLR
  heap/RSS exclusions do not turn an explicit array into physically freed storage.

No price, work limit, constant data, payload fit or execution authority is supplied.
Root relayed this same boundary to both independent runner and C++ census reviewers.
