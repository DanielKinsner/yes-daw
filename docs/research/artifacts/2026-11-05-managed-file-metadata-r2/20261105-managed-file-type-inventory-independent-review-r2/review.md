# Managed metadata reader R2: independent whole source review

**ACCEPT, source only.** The complete 377-line / 22,475-byte reader, README and
source-pins were read. All 48 ordinary-file/hash/text checks pass. This permits
root to consider the separate metadata tool's reviewed scope; it is no execution,
runtime layout, physical size, allocation, lifetime, price, R11 or stage approval.

Subject manifest SHA256:
`72F84E45F43CB4ED86E91B6C1CCB464A904E043E5338FAF941C47284E69DC7DC`.
Reader SHA256:
`9215EED3EDAC196C983353ADC68D1BA95FCD0C4833ED1130FB7C8F39CCC8D348`.
All four target file pins, twelve dependency pins, six authority references and
four author-source/evidence pins were independently checked against actual bytes.
Frozen R1 source and its execution inventory remain unchanged.

The independently generated diff matches the author's diff. The original four
fixed paths/lengths/hashes and 21 required names are byte-exact. Main, bounded
failure categories, the same-handle SHA/rewind/PEReader gate, complete WriteType,
GenericContext and signature/name/cycle decoder remain byte-exact to R1. The
actual change is the V2 result schema and narrow ReadAll metadata walk below.

The JObject child walk uses GetNestedTypes on the already required JObject
TypeDef. Thus only directly declared child names are listed. Selection requires
the exact ordinal complete parent/name prefix, a nonempty suffix and ASCII digits
at every suffix position. A second match and a missing match each produce their
own fixed refusal. The numeric suffix is discovered, never guessed. The unique
selected handle uses the unchanged field/base/signature writer. No other child's
fields, methods, IL or attributes are inspected by this discovery.

Only CoreLib adds the three exact optional names to its ordinal wanted dictionary.
The same TypeDef name walk detects duplicates, records each presence, and invokes
WriteType only for present definitions. Absence emits false rather than a fake
field row. All 21 required definitions still fail on absence. The final gate
requires 21 required rows, one generated row, one discovery, three optional
presence records and 22..25 inventoried rows. Nested/name walks are finite within
the fixed hashed file metadata; existing nesting/TypeSpec cycle refusals remain.

Every inventoried field is declared, nonstatic instance metadata, retaining name,
attributes, decoded signature and raw blob. Type attributes, ClassLayout metadata,
base TypeDef/Ref/Spec and generic parameter handling remain exact. This reads no
instance field value, live Type, object offset, constructor, method body, IL,
custom-attribute invocation, native API, environment or directory inventory. No
target assembly is explicitly loaded. Ordinary toolhost runtime/metadata/file and
DTO code is distinct from invoking an inventoried target type.

Output remains a private buffer until the complete metadata walk and count gates
finish. Main then makes one stdout buffer write. Metadata failure before that
point cannot publish a partial inventory. As with R1, a stdout transport failure
can interrupt that write; root must retain exit/raw-output and complete-JSON
validation rather than treat buffering as a transactional transport guarantee.
All exposed failure reasons are fixed source literals; actual exception text,
stack and paths are not copied. Zero CLI arguments and fixed file paths prevent
scope redirection.

No source defect was found. The first unsealed text checker mistakenly matched
the substring GetValue inside Dictionary.TryGetValue; its failed evidence is
preserved. The corrected member-call boundary check passes without subject edits.

No C# compiler, parser, Add-Type, reader/helper, assembly import/load, target
constructor, executable/protocol stage, native probe, GUI/device, Git or CI was
invoked by this review. Only its own ordinary Python file/hash/text checker ran.
Root owns any later compiler/reference/host association, hidden bounded launcher,
raw capture and execution. Actual compiled field-branch evidence, if obtained,
will still establish no runtime size/offset/padding, control block, reclamation,
32MiB fit, R11 source price/binding or GO.
