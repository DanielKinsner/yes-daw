# R19 directory-scan scalar review

Verdict is **revise** for the
[R19 author proposal](2026-11-14-clock-r19-stage-scalar-total.md).
The review verified call-count and schedule arithmetic conditional on the
stated component bounds. The following source findings prevent accepting
those bounds.

- `Wide current = path` uses the implicit memberwise constructor. Its
  array copy reads every `wchar_t`, including untouched cells beyond the
  NUL. A new source revision must read only the initialized range.
  [C++20 memberwise copy](https://timsong-cpp.github.io/cppwp/n4861/class.copy.ctor#14)
  [Indeterminate values](https://timsong-cpp.github.io/cppwp/n4861/basic.indet#2)
- Aggregate zeroing needs the selected type conversions. The 285 directory
  fields and nine attribute fields per reached ordinary record are now
  explicit in the proposal. The other findings still hold.
- Both `wchar_t` comparisons in the name loop need operand promotions.
  The final comparison with the integer zero needs one more promotion.
- The `failure` construction row must cover the caller and callee,
  aggregate writes, casts, condition and return. Each implicit assignment
  of the four-field result also needs its call boundary and actual reads
  and writes.
- The lock acquire and release rows need the native prototype parameter
  and entry accounting. A diagnostic must separately count any native
  error read, the path wrapper, the flag operation and both return paths.

The successor's work count must reprice the explicit prefix copy and close
these findings before any accepted scalar total or execution binding.
