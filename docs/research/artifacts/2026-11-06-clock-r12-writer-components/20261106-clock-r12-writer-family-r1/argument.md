# R12 writer buffers and selected cold owners

Source/integer component only, pending independent review. No complete codec or
whole32MiB fit is claimed. R12 remains unrun and all its guards remain false.
The frozen first component and separate cold supplement retain their exact
inputs, arithmetic and remaining obligations. No old upper is inherited as proof.

## Once encoding and retained requests

Encode-TaskJson704..716 admits an immutable already-measured private graph,
invokes one exact bound ConvertTo-Json with Compress/EscapeNonAscii, then checks
the text length equals measured bytes minus1 and every output unit is ordinary
ASCII without LF/CR. It appends one LF and uses one fresh strict UTF8Encoding to
allocate the final byte array. Measure/fixed-point passes do not encode. Original
graphs and preprocessing clones belong to O/K and are additional to this B term.

Under the selected unchanged codec source, JsonConvert675..687 creates one
StringBuilder(256), one StringWriter around it and one JsonTextWriter. It returns
the builder's ToString once through StringWriter. No disposal, replacement,
scope exit, pool return or hypothetical GC refunds an earlier request here.
Failure may shorten an admitted encode; its already-reached objects remain.

The supplied document maxima are marker319, receipt23635, failure262760 and
bundle1048576 bytes. These are source-profile upper inputs, not observed lengths.
The final selected root may support a separately proved tighter bundle maximum;
the historical R3 estimate is not transferred. Prefix vectors are0/4/4/5/6/7
encodes: marker1, failure1 and receipts2/2/3/4/4 after P0, with bundle1 only at P5.
A later error can follow bundle encoding, so bundle and failure requests are
both retained in that prefix upper.

## Every escape buffer, including old arrays

JsonTextWriter454..457 calls EnsureWriteBuffer before every string, including
property names. EnsureWriteBuffer819..826 allocates exactly35 chars if null.
With the selected null pool, BufferUtils always allocates exactly its request;
it does not round to a pool bucket. JavaScriptUtils only replaces a nonnull
buffer when its requested size exceeds the old length. Every admitted source
string/key has at most512 units. A copied unchanged run plus its optional
six-unit Unicode prefix therefore requests at most518. Integer writes reuse
this buffer; dates, binary/base64, custom objects and indented formatting are
outside the admitted value kinds. No indent array or Base64Encoder is constructed.

Thus all requests are one35 and a strictly increasing subsequence of36..518.
The first simple upper sums Align8(24+2*n) for every n35..518, including headers
and alignment of all old arrays. It replaces the unsupported historical payload-
only2*sum(1..518) allowance.

A second upper couples requests to actual emitted text length L. Each growth
copies a disjoint emitted unchanged run and at most six extra prefix cells.
The first35 is already separate; its presence means the small six-cell temporary
branch never allocates another smaller buffer. If g replacements occur, their
capacity sum is at most L+6g. Strict growth also gives minimum sum
g*(g+71)/2. Consequently g*(g+59)<=2L and g<=483. A char-array request is at most
2*n+30 including its header/padding, giving total at most96+2L+42g. The component
selects the minimum of both independently valid uppers. It does not assert the
maximizing sequence is attainable or reclaim any discarded buffer.

## StringBuilder chunks, output strings and writer fields

Append-only writer calls have bounded spans: escaped source strings at most3072,
unescaped runs at most512 and integer text at most20. StringBuilder starts256.
ExpandByABlock2546..2590 chooses max(request,min(currentLength,8000)); each later
chunk therefore has at most8000 cells. Earlier chunks are full. With final
length at most L, total capacity is at most max(256,L+min(8000,L)-1).
The minimum-growth schedule maximizes chunk count: cumulative capacity starts256,
doubles until8192, then grows8000 per expansion. Larger append spans can reduce
this count. All chunk arrays include their own header/padding, bounded by
2*totalCapacity+30*chunkCount. ExpandByABlock creates one new StringBuilder chunk
object per expansion; the initial object also counts. Its five source fields
give a selected x64 field-slot upper48 per chunk, additional to the arrays.

Charge Str(L)=Align8(22+2L) for ToString, Str(L+1) for LF concatenation and
Align8(24+M) for the exact ASCII UTF8 array, where M=L+1. ASCII means no UTF8
fallback buffer is invoked. Encoding.GetBytes(string) allocates the exact
GetByteCount result and writes into it; no second output copy is inferred.

Installed public file metadata corroborates JsonWriter12 fields,
JsonTextWriter11 more and JsonPosition4 fields. A conservative selected source
field-slot model uses an object header16 and at most8 bytes per scalar/reference
field. The embedded JsonPosition is bounded by32, not treated as one reference.
This gives224 for the combined writer,64 for StringWriter including TextWriter
and its compiler-generated Encoding backing field, and40 for its List position
owner. Depth24 permits List capacities4/8/16/32; all60 JsonPosition cells are
charged with stride upper32 plus four array headers. These are conditional source
uppers, not measured actual object sizes or offsets.

Strict UTF8Encoding construction first runs Encoding's virtual fallback setter
while its derived throw flag is false. It creates two replacement fallback
objects before the strict constructor replaces their references with shared
exception fallbacks. Charge both24-byte source-slot objects and72 for the
encoding. The previous objects are not refunded. The14 non-encode constructors
are additional at their own actual sites, owned by the whole phase calculation.

## Shared cold owners and exact remaining boundary

Three bool[128] escape arrays are shared once. JavaScriptUtils'38-element List
retains all old char arrays4/8/16/32/64. The three actual Union foreach instances
contain34/34/38 unique chars. Pinned ToCollection declares internal initial set
capacity7: backing capacities are7/17/37 for34 and7/17/37/89 for38. Charge every
bucket/entry backing, Union iterator, HashSet and boxed List/array enumerator.
There is no Union materialized result array in these foreach statements. The
selected upper Entry stride24 covers its three source fields; char values cannot
take string randomized-rehash branches. These source/representation associations
and the character default-comparer field shape remain explicit profile premises.

JsonWriter contributes template outer8/eight int-state[10], ToList's old8/16/32
reference backings, final outer18, EnumInfo/three arrays18 and18 boxed underlying
JsonToken values. Additional known source owners include shared generic empty
arrays, two Encoding/UTF8 default instances and their overwritten fallbacks, four
shared replacement/exception fallbacks, TextWriter.Null, EnumUtils' naming object,
store wrapper and creator delegate. The selected known once subtotal is13256.
It must appear once in F rather than once per encode. Earlier codec activity or
BeforeFieldInit timing supplies no free credit; charge it from the earliest
applicable source prefix unless a tighter actual initialization order is proved.

Cold closure is incomplete. cold-supplement.json names four precise remaining
terms: EnumUtils' actual default ConcurrentDictionary/lock/bucket/node profile;
reflection outputs and up to18 Cast plus18 Select iterator objects for empty
EnumMemberAttribute queries; JsonSerializerSettings/serializer/InternalWriter,
serialization traversal/serialize-stack and contract-cache owners; and the
expressly scoped general CLR/SDK metadata/literal/cache boundary. Any excluded
general heap is unqualified, not0. Selected fixed arrays above do not certify
their transitive cache/metadata or another compiled feature branch.

The selected B prefix uppers are0/2628032/2628032/2867944/3107856/10757826 bytes,
plus the once cold term and every named remainder. These remain conditional
request uppers. They establish neither fit nor NOFIT, actual live memory, total
CLR heap/RSS, immutable binding, runtime codec success or execution authority.
Only ordinary file/source/hash/JSON/integer operations were used; no target
parser, import, compiler, codec, native function, program, stage, GUI, desktop
input or audio/MIDI device was invoked.
