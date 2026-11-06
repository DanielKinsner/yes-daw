# Initial unsealed integer assertion failure

The first own-Python derivation exited1 at derive.py80 before runtime-component.json
was written. The arithmetic expression computed151512; the separately typed
expected constant151416 omitted8-byte char-array alignment in each d5 path.
Actual output ended with `assert error_tail == 151416` and `AssertionError`.
The exact source is preserved as derive-attempt1-assert-failure.py.

The supported correction changes only that expected constant to151512. No bound,
formula, source, cap, admitted domain, target code or earlier evidence changes.
No target PowerShell/compiler/codec/program/native/stage ran in this attempt.
