# Clock R22 native source work map

The source-only check matches all five native file pins and maps
**87 function definitions** across the four C++ translation units
to their worksheets. It distinguishes the meter overloads. The
unused `admit_write(Meter&)` overload is listed in the capture
occurrence worksheet with zero reachable calls.

Run `python -E -B check.py` with the exact R22 candidate staged.
The map checks coverage of definitions in the frozen formatting.
Local leaves shared by several callers still need all their reached
occurrences in the [combined phase arithmetic](../../2026-11-14-clock-r22-native-phase-work.md).
The check reproduces the prefix and tail totals from the stated
worksheet operands. Native premises, retention, binding and the
controller/target source work remain separate.
