# R22 Clock checker admission read

The supervisor stores the conjunction of the five existing compile-time
admission constants in a local const volatile bool and reads it once.
This preserves the original short-circuit truth table and exit 10 while
requiring the compiler to emit the conditional path. The added access
has its own source census row. Every admission constant remains false.
