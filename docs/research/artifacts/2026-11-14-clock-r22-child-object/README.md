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

## Admission-enabled entry frames

A separate [compile-only purpose](entry-frame-purpose.py), independently
reviewed at SHA-256
`EDFB2D7F4A036CD5EE4833BE1D62969914AAB4B77FA10BF0012385FE9B385082`,
changed exactly the six target admission literals in a private copy.
Its variant source SHA-256 was
`00EA4D2C7F02E15AD42DE480305C77C5BC89D5CEBBC13AD75C534443F35E0AD9`.
MSVC exited 0 under the same `/WX /O2 /c` flags. The post-compile
dependency join matched all 110 prior includes, with two byte-identical
copies preserving the relative includes. The catalogue stayed undefined.
The dependency report SHA-256 was
`2E90E0D62C2055B5386D9973A32E257C55E5A642E0BC07A770E22A078D257D9C`.
The first wrapper refused the result because `.pdata` used a local code
label for `main`. The object and failed attempt were kept.

Run `python -E -B check-entry-frame.py` to check the preserved 482,581-byte
object as data. The check associates the exact unwind symbol for `main`
and verifies the source-level call path by decoded COFF relocations and
the AMD64 relative-call opcode. There are 349 unwind ranges.

| Emitted body | RSP decrement |
| --- | ---: |
| `main` | 2,328 bytes |
| `run` | 341,864 bytes |
| `readText` | 196,744 bytes |
| `main` catch 10 | 152 bytes |
| `main` catch 11 | 104 bytes |

The three directly connected frames sum to 540,936 bytes of RSP
decrement. Return addresses, other callees, unwind processing and
CRT startup have separate storage. A later bound object or final PE
must match its own emitted code and unwind records. This purpose
produced COFF and did not link or start Clock.
