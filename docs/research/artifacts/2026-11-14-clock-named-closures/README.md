# Exact named Clock closure layouts

The independently reviewed instrumentation uses `decltype` immediately
after the original `get` and `nextBit` lambda definitions. Removing the
observer statements and the template preamble restores the exact original
source bytes. The compiler included the two staged headers in the same
order and verified all 110 reported include pins.

Pinned MSVC 19.44.35228 emitted the [measured cells](result.json).
`get` is 32 bytes, aligned to 8. `nextBit` is 16 bytes, aligned to 8.
No link or execution was performed.

The first compile exited zero and produced both matching records.
The decoder initially refused because the full checker object had 1,708
sections, above the generic probe's 512-section check. A second decode
then found the export-name check expected the namespace in the wrong
position of the decorated symbol. The corrected decoder validates the
actual table extent and the two tagged records and namespaced exports.
It recovered both measurements from the first object, SHA-256
`D31072CDBA1AE22F3111347A5B0AA24A115A90C3F539F3EA89575BC573DC1D02`.
The second isolated compile reproduced both values.

Run `python -E -B instrument.py <new-source-directory>`, then
`python -E -B run-purpose.py <source-directory> <compiler-directory> <new-output-directory>`.
The remaining 15 closure types require their exact caller or template
association.
