# Clock R22 target compile-only result

Pinned MSVC compiled the exact guarded `clock_stage.cpp` to a 475,557-byte
x64 COFF object. The dependency report listed 110 distinct pinned paths.
The compile returned zero with `/W4 /WX`, and standard error was empty.
The [receipt](result.json) records the compiler, source, object and
dependency hashes and the reached include identities.

The independently reviewed [purpose](compile-only.py) keeps the exact
compile flags and uses a fresh output directory, null stdin, no window
and `/c`. Its SHA-256 at invocation was
`39F754887DA56ABE7A2EB6FFC1578B5E21CD91198A6C7E1C9B8D4C9C57F9BE1F`.
With exact R22 staged, run `python -E -B compile-only.py <new-output-directory>`.
No linker, catalogue definition, guard change or executable launch
was part of the purpose. This evidence is for the guarded source under
the pinned include profile. It does not qualify enabled stack frames
or the eventual target image.
