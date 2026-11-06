# Direct serializer owners and error paths

Proposed finite source component, not whole codec/resource acceptance. Only own
Python/file/hash/integer work was executed. No target codec/parser/import,
compiler, native function, program, stage, app window or device ran.

Source-field inventory finds24 explicit JsonSerializerSettings fields plus seven
auto-property backing fields; JsonSerializer has30 explicit fields plus Error's
event field. StreamingContext has one reference and one int enum, bounded by16
source bytes under the selected x64 field-slot model; Nullable<StreamingContext>
is bounded by24. Thus settings280 and serializer272 are conservative selected
source object uppers. InternalWriter has its base's five fields plus three more,
giving80. Settings constructs one empty List<JsonConverter>40. InternalWriter
constructs one List<object>40 for serializeStack. At most25 nested containers
under the24-depth preflight need old4/8/16/32 reference backings:576 more bytes.
Total direct fixed owner upper is1288 per encode. These are not actual object
sizes/offsets; compiled source/representation association remains conditional.

The closed preprocessed graphs contain Dictionary<string,object> and List<object>
containers and primitive leaves. SerializeDictionary uses an IDictionaryEnumerator
and e.Entry, avoiding one DictionaryEntry box per element. The selected Dictionary
enumerator has dictionary/index/version/current-KVP/return-kind fields:64 is a
safe boxed source-slot upper. SerializeList's nongeneric enumeration boxes a
List<object> enumerator, bounded by48. Empty generic enumerators may be shared;
the upper charges each traversal rather than treating empty cases as a refund.
Hence the direct runtime term is1288+64*maps+48*arrays for each document. It is
additional to the original O graphs, K preprocessing clones, writer buffers,
JsonPosition stack and selected cold owners already separately reported.

The whole phase owner's source-profile graph inputs are marker1map/0array,
receipt58/23, failure79/1 and bundle1294/2. Their container-chain lengths including
root are1/5/3/3. The whole join must pin its independently derived graph/key list;
the initial component records these as profile inputs, not inherited prices.

## Failure-path correction before any complete acceptance

The previous writer component's claim that every failure only reduces writer
output was too broad. JsonWriter.Close/AutoCompleteAll/AutoCompleteClose may emit
null for a pending property. A one-character integer value can therefore be
replaced by four-character null during cleanup: abstract nominal {"x":0} versus
{"x":null} differs by3 units. There is at most one pending scalar property;
closing an entered nested container updates its parent's state rather than
creating another pending scalar. Compressed closures themselves correspond to
nominal closing delimiters. A corrected successor must use nominalL+3 for writer
builder/escape-buffer bounds. ToString/LF/UTF8 output arrays still use nominalL:
a codec exception never returns a completed text to those later caller steps.
The first writer packet remains frozen history; no complete fit was accepted.

InternalWriter's nested catches compute ContainerPath and its outer Serialize
catch computes Path. With d containers, at most d+1 path calls are reached for
one unhandled failure; no handler/default delegate/trace/custom converter/callback
exists in the closed graph. IsErrorHandled constructs one ErrorContext64 and up
to d+1 ErrorEventArgs32 even when no Error event handler is installed. A List
catch can additionally box its index, at most d*24. ErrorContext is reused while
the same failure propagates; an error in its handling can skip work but does not
reenter the source loop or create a retry. General CLR exception/stack payload
remains in its explicit separately qualified family, not certified here.

All output keys are fixed ASCII identifiers with underscores, unlike scalar
path values. JsonPosition.SpecialCharacters therefore matches none of them.
BuildPath first sums each component's CalculateLength, then creates exactly one
StringBuilder(capacity). Object components reserve keyLength+5; array components
reserve unsigned digit count+2 (even initial Position=-1 is over-reserved by its
ulong conversion). Actual identifier/delimiter/index text fits that capacity.
No escaping StringWriter, buffer, builder growth or integer ToString fallback is
needed. Using K<=512, C=d*max(K+5,22) bounds the path capacity. Charge one builder48,
one Align8(24+2C) array and one Align8(22+2C) string per reached path computation.
The error-route upper is(d+1)*(these requests+32)+64+24*d.

A first failed main encode exits the main try. The catch can separately try only
failure-inventory encoding and receipt-footer encoding, each caught once. Thus
the global serialization-failure addendum is at most one main route of d<=5,
one failure route d<=3 and one footer route d<=5:151512 selected bytes. It is
charged once after encoding becomes reachable, not once per every successful
document. This is a conservative request upper, not an attainable trace/lower.

JsonPosition's shared special-character array18 and List<JsonConverter>'s shared
empty array add64+24 once. List<object>'s empty array24 and its shared empty
enumerator upper48 need identity deduplication with K/J. They are named shared
owners rather than additional per-encode requests.

DefaultContractResolver/DefaultSerializationBinder and their contract/thread-
store/dynamic delegate caches remain an explicit cold F source/storage obligation.
No hidden prototype, reflection/codegen/type cache, CLR metadata, native allocator,
total heap/RSS or arbitrary exception tree is silently assigned0. Independent
whole review must verify these components, failure correction, graph profile and
every phase join before selecting a bound, immutable binding or execution GO.

The first authoring attempt stopped before output at a mistyped expected integer
assertion; its exact source and actual failure description are preserved. Only
the expected constant was corrected; formulas did not change. The successful
component and complete source are now frozen for independent review.
