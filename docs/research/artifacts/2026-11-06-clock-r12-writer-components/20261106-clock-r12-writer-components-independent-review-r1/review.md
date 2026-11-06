# Independent writer/serializer component review R1

**REVISE the R1/R2 component bounds.** Their stated arithmetic recomposes, but
the complete failure/request upper needs the fixes below. Subjects, prior failed
attempts, caps and R12 source remain unchanged. This is a component review; no
complete cold codec term, whole32MiB fit, immutable binding or GO is accepted.

The independently authored checker ran only file/hash/JSON/integer operations and
completed 297 recorded checks, exit0. It did not import or execute any subject
helper, PowerShell parser/AST/cmdlet/codec, compiler, probe, target program, native
API, stage, GUI, Git or CI. Its complete generated evidence is checks.json
`8FA2674F057017AD17F9238CCFD812A0760E087B8BC22243B89CED551D41DB4E`.
All input identities, the R2 captured graph snapshot, document/phase arithmetic,
direct-owner equations and finite candidate repair arithmetic are recorded there.
These successful data checks do not turn a source-premise omission into a bound.

## Findings

1. **R1's failure-only-reduces statement is false.** JsonConvert675..687 uses a
   JsonTextWriter in one using scope. JsonWriter.Close411..415 runs
   AutoCompleteAll733..739; AutoCompleteClose756..781 can write null for one pending
   property. A nominal one-unit value can become four-unit null: the abstract
   `{"x":0}`/`{"x":null}` difference is3. There is at most one pending scalar;
   closing an entered child updates its parent state. R2 correctly raises the
   writer-only nominal text upper L to L+3. A failed codec never returns ToString,
   LF-concatenated text or UTF8 output to the caller, so those successful-output
   requests continue to use nominalL. Runtime-source R1 explicitly preserves this
   correction rather than certifying the original overclaim.

2. **L+3 does not cover StringBuilder's orphan/retry requests.** In the pinned
   StringBuilder.ExpandByABlock2546..2586, the new char array is allocated before
   `new StringBuilder(this)`, and the array is attached only after that constructor
   returns. If that second allocation throws, the array request already happened
   while the builder state remains unchanged. The same using disposal can then
   try an expansion while emitting null or a closing delimiter. A bound on final
   text/chunk geometry cannot discard the earlier orphan array or assume every
   requested array belongs to the completed chunk chain. This is a controlled
   char-array request, even though the exception's general CLR graph is excluded.

   A supported conservative addendum is two extra `(Achar(8000)+SBobject)` pairs
   per failed encode. The first unhandled body failure aborts InternalWriter;
   with the closed no-handler/no-callback/trace/converter profile, its catches
   rethrow and do not call HandleError. The using calls Dispose/Close once, with
   no further Close retry after failure. Compressed cleanup writes at most one
   null plus the outstanding delimiters; selected depth<=5 gives at most9 units.
   Any completed growth has at least256 cells, so cleanup cannot complete one
   growth and then require another. One failed body expansion and at most one
   cleanup expansion therefore suffice. The bound conservatively charges both
   object attempts even if an unsuccessful managed allocation produces no object.
   No deliberate OOM, allocation trap or runtime failure was invoked.

3. **SB48 is not implied by the stated generic field-slot premise.** The source
   has two references and three int fields. `header16 + five slots<=8` gives56,
   not48. A packed selected CoreCLR layout might support48, but that requires an
   additional layout premise not supplied by the component's generic source-slot
   argument. Use56 for every main writer chunk object, every diagnostic BuildPath
   builder and every orphan chunk-object attempt. This avoids a new size probe
   or an assertion of actual offsets/sizeof. Other stated direct-object uppers
   are conditional source-slot bounds, not measurements.

## Reviewed ownership and equations

The35-char initial escape array and all larger requests are included. With null
pool, growth is strict and bounded by36..518; existing buffers are assigned
before the next writer append and have no intervening allocation that could
produce the StringBuilder orphan case. For g replacements, capacity sum is at
most nominal emitted unchanged runs plus6g; minimum sum is g(g+71)/2, giving
g(g+59)<=2L and g<=483. Each requested char array costs at most2n+30 including
header/padding. The selected minimum of the complete distinct-capacity sum and
`Achar(35)+2L+42g` is a valid conditional array upper. Using L+3 is conservative
for failure-close output. Null/closing delimiters do not create another escape
buffer. No pool return, clearing, Dispose, scope exit or hypothetical GC is a
refund.

For the selected append-only writes, earlier completed chunks are full.
ExpandByABlock chooses max(required,min(currentLength,8000)); every admitted
write span is<=3072, so later arrays have<=8000 cells. The completed-chain
capacity upper `max(256,L+min(8000,L)-1)` and minimum-growth chunk count correctly
bound that chain. They require the separate failure-request addendum above.
The scalar source strings/keys are bounded and integers have<=20 output units.

The normal successful output has one ToString request, one LF concatenation and
one exact ASCII byte array. Strict UTF8 construction first creates two replacement
fallback objects while its derived throw flag is false, then overwrites their
references with shared exception fallbacks. Both replaced objects remain charged.
The encoding object upper72 plus two replacement objects24 each is retained.
The other14 non-encode constructors remain a separate whole-phase obligation.

Settings31 fields (including Nullable<StreamingContext> upper24), serializer31
fields (including StreamingContext upper16), InternalWriter8 fields, two List
owners and serializeStack backing requests give the stated fixed1288 conditional
upper. Dictionary traversal uses IDictionaryEnumerator.Entry rather than boxing
one DictionaryEntry per element. Per-map64 and per-list48 enumerator uppers are
additional to O originals/K preprocessing clones. R2 correctly substitutes the
actual selected container-chain backing schedules rather than global depth24.
The writer's JsonPosition stack needs at most d-1 positions; serializeStack needs
at most d. Their separate list owners/backings are retained.

The captured graph inputs are marker1/0/d1/key35, receipt58/23/d5/key60,
failure79/1/d3/key38 and bundle1294/2/d3/key41. Keys are fixed ASCII identifiers;
none contains a JsonPosition special character. The independently sealed
452921-byte bundle upper applies only to the provisional selected root/profile,
not a future canonical union. All20 numeric tokens retain the independent bound's
scope. No new output or root assumption was added by this review.

JsonPosition.BuildPath precomputes capacity. Object components reserve key+5;
array components reserve digit-count+2. Thus a builder/array/string per reached
path calculation is funded without an escaping writer/buffer or builder growth.
Ancestor array positions are established by UpdateScopeWithFinishedValue before
Push, and start-state current positions are omitted by Path; no arbitrary
negative-index formatting expansion is needed. Error propagation reaches at
most d container paths plus one outer Path, one reused ErrorContext64 and up to
d+1 ErrorEventArgs32; List catches additionally box at most d index values24.
The excluded general exception tree is not relabeled zero.

A first failed main encode leaves the main try. The script can then attempt
failure-inventory and receipt-footer encoding, each once in its own catch.
Therefore at most3 encodes have serialization-failure addenda. With SB56 and the
actual key/chain inputs, the diagnostic bound is20408. Two extra pairs per failed
encode give `3*2*(Achar(8000)+56)=96480`. These are source-sized conservative
request uppers, not demonstrated failures or attainable lower bounds.

Applying only these supported fixes to R2 gives candidate B prefixes
`[0,2755816,2755816,2999612,3243408,6800264]`. This review does not mutate R2 or
authorize those values automatically; a separate whole successor component must
preserve the source/profile inputs and undergo its own review.

Cold13256 remains a known once-only subtotal, not a complete cold bound. R2 adds
JsonPosition's special-character array64 and List<JsonConverter>'s empty array24,
giving13344. This belongs once in F from the earliest applicable prefix, without
warmup/BeforeFieldInit credit. The three Union traversals, initial set capacity7,
34/34/38 unique chars, all7/17/37[/89] bucket/entry arrays, old List buffers,
iterator/enumerator owners, JsonWriter state arrays/EnumInfo/boxed tokens and
encoding/fallback/direct static owners remain explicit. EnumUtils' concurrent
store/reflection/attribute iterators, contract resolver/binder/cache and selected
general CLR/SDK type/metadata/loader boundaries remain named and unresolved.

Object-header16/array-header24/alignment8/source-slot8, embedded struct widths,
selected feature branches and actual source/binary association remain explicit
representation/profile premises. These figures establish neither actual object
sizes, peak/live memory, physical frees, total CLR heap/RSS, complete codec F,
whole32MiB fit/NOFIT, immutable binding nor execution authority.
