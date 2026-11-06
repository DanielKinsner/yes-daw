# Reader capacities for the provisional R11 payload equations

This is source/data arithmetic, not an executed PowerShell/JSON helper, measured
private capacity, allocation probe, fit certificate or protocol GO. The exact
provisional R11 manifest and predecessor bytes are pinned in fixed-inputs.json.
Changing the canonical input union requires fresh data arithmetic.

The pinned JsonTextReader source calls ShiftBufferIfNeeded before ParseNumber
and after consuming the opening quote in ParseString/ParseProperty. It does not
call it for true/false/null or punctuation. Consequently the512-unit string
predicate alone does not imply a universal32768-character reader buffer. A long
literal run can position a MatchValue EnsureChars(...,true) near the buffer end.
Outside-string compaction removes whitespace pacing; it does not remove this
literal/delimiter run.

For an identity-pinned compact input, let G be the largest distance between
successive shift sites, also including initial/terminal sentinels. U=G+6 covers
the intervening token/run, reached lookahead and sentinel slack. At a site that
does not shift, remaining capacity is greater than C*0.1. If C>=10U, this is
greater than U (0.1's binary64 value is above1/10, and multiplying these power-of-
two capacities is exact). At a shifting site, positions/remaining contents are
reset. A nonappend read occurs only at an exhausted strict-grammar boundary and
retains at most the bounded lookahead; it cannot replace a buffer C>=1024.
For append, required<=5 and used<=C-1, so max(2C,used+required+1)=2C.
Thus a starting1024 buffer never grows past nextPow2(max(1024,10U)). Retain every
old array; no ReturnBuffer, Close, scope or null assignment is a release proof.

The independent lexical scanner deliberately uses the slightly looser factor16
for the two fixed inputs. Their G values438 and289 yield U444 and295; both give
C8192 with either factor10 or16. All arrays1024,2048,4096,8192 total30,816bytes
including the measured24-byte array headers. This scan reads only valid immutable
JSON DATA, enumerates quote/number spans and hashes files; it does not reimplement
or invoke the target reader's state machine. Source induction supplies the bound.

For the arbitrary captured compiler report, preserve the whole guarded domain:
N2698 tokens including keys (2*include_input_count1221+256), decoded string S<=512UTF16 units, decimal integers
<=20characters, depth24 and original bytes<=4MiB. A quoted token has at most
6S+2 wire characters. Across a gap between shift sites, the intervening literal,
container and delimiter run is bounded by7N (the prior reviewed node accounting).
Use U=6S+2+7N+6=21,966. The actual last10% trigger gives
nextPow2(max(1024,10U))=262144. This replaces the earlier factor16 bound524288;
it does not replace N by one token. All nine arrays1024 through262144 total
1,046,744bytes including headers (1,046,528payload bytes plus216headers).

The two ordinary native child captures have at most4 and12 ASCII nonempty lines
with every line below1024characters, so16 initial reader arrays total33,152bytes.
Their fixed literal/property strings have no escapes, so this specific route
does not allocate StringBuffer. The fixed manifest/predecessor and generic report
can each allocate one1024-character StringBuffer (2072bytes including header);
each decoded string remains<=512 so no growth. All decoded token strings and
JToken/conversion owners are separate terms, not hidden in these array amounts.

This yields a conditional reader-array sum1,141,528 and StringBuffer sum6,216 for
the fully reached provisional profile. It does not imply a32MiB global upper.
The report can still have4MiB compact bytes, an8MiB decoded source string and
many separate parsed strings; that independent family is not lowered here.

Source: [pinned JsonTextReader](https://github.com/JamesNK/Newtonsoft.Json/blob/4e13299d4b0ec96bd4df9954ef646bd2d1b5bf2a/Src/Newtonsoft.Json/JsonTextReader.cs)
lines157-162,253-362,365-405,1136-1323,1578-1590,1968-1976,2366-2412.
