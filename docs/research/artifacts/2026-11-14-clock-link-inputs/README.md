# Clock R22 link input preflight

The existing R22 input manifest already pins the staged linker and
seven candidate libraries. All eight matched their archived bytes.
Each of the four measured x64 objects names MSVCRT and OLDNAMES in
its COFF directive section, and those two libraries are included.

With the exact local R22 candidate staged, run
`python -E -B check.py <R22 object directory>` from this directory.
The check compares all four objects to the compile-only receipt
before reading directives. A future bound image needs its own link
input trace, import and source correspondence. This check read
existing files without linking or running any of them.
