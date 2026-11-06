# R4 public-file metadata reader: whole source accepted

The independent file/text checker passed 247 assertions on its first run. The
entire 468-line reader was reviewed, including fixed inputs, same-handle length
and SHA gates, read-only PE metadata access, exact TypeDef lookup, inherited
name/signature context, nonstatic declared field output, cycle/refusal paths,
optional types, exactly-one JObject iterator selection and complete buffered
publication. Every original target/dependency pin remains exact.

The complete reader prefix is unchanged. After replacing the explicit input
region and exactly four schema/count literals, the entire remaining reader is
byte-identical to accepted R3. Its PowerShell wrapper differs only at the
manifest pin and reader length/SHA pins. Eight fixed DLLs request 84 named
definitions, preserving all original 22 and adding 62; the existing one generated
selection and zero to three optional definitions give 85 to 88 output records.

All source, target-file, dependency and authority-reference hashes were checked
by reading their bytes. No compiler, target parser, PowerShell import, file
metadata reader, target assembly load, inspected constructor/method, live value,
offset/layout/allocation probe, native window, device or Clock stage ran.
Raw candidate-name hits do not establish that the requested TypeDefs exist.
Missing or wrong assembly/name requests must cause the later reader to fail
before publishing any partial metadata.

The separate outer launcher must validate complete identities using the pair
(file, qualified_name). SafeThreadHandle and DecoderNLS are requested in more
than one assembly; using the predecessor's flat name set would incorrectly
reject otherwise complete output. Root has authored that separate unrun
launcher, which needs its own independent review and bounded purpose authority.

This accepts source only. Normal SDK services implement the purpose program's
file hashing, metadata decoding, compilation and output; the inspected files
are accessed as bytes, without reflection-loading or invoking their inspected
types. No live object size, field offset, source/binary association, generic
instantiation layout, complete wrapper/resource bound, fit, native binding or
Clock execution GO follows. The six remaining qualifications in the source
manifest remain pending.
