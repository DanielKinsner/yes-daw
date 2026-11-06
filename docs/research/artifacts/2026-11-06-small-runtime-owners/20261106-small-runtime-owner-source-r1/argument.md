# Selected bounded BigInteger and FileInfo source owners

Conditional source/representation component; independent acceptance pending.
The only executed work was own Python/file/hash/JSON/integer arithmetic and
official source downloads. No target parser/import/compiler/native method,
allocation probe, program, stage, GUI, desktop input or audio/MIDI device ran.

The unchanged raw grammar admits only signed-Int64/unsigned-UInt64 decimal
integers. A JSON reader integer overflow that reaches BigInteger is therefore
positive in[2^63,2^64-1], with19 or20 digits. Negative overflow, fractions,
exponents and arbitrary precision beyond that interval are refused before SDK.
Fixed bootstrap/predecessor documents do not enter this branch.

JsonTextReader2190..2198 creates one StringReference.ToString token copy, bounded
by Str20=Align8(22+40)=64. BigIntegerParse2257 returns object, boxing the result.
JsonWriter's Integer WriteToken branch unboxes it into a BigInteger local and
passes it to the virtual WriteValue(object) overload, boxing a second copy.
There is deliberately no BigInteger overload on JsonWriter. JTokenWriter's
object override recognizes BigInteger and AddRawValue passes that object to
JValue's object constructor without another copy/box or limbs-array clone.

Pinned VMR BigInteger.Parse(string,provider) forwards through a span. For at most
20 digits, TryParseBigIntegerNumber uses stackalloc byte[length+2], at most22.
NumberToBigInteger uses at most3 base1e9 words and resultLength5. Both buffers
take the selected Release StackAllocThreshold64 arm, reserving64 uints each;
neither takes ArrayPool. Scale<=20<1233 selects the naive multiply/add route,
whose span-only local loops create no further controlled arrays or recursion.
The constructor trims leading zeros; every value in the admitted overflow
interval has exactly2 base2^32 limbs and allocates one uint[2],32 bytes. The
BigInteger value's int sign/reference fields fit16 source bytes and a boxed
selected upper32. The two boxes share the immutable limbs array.

Thus b20 beyond the separately counted JValue64 is64(token)+32(limbs)+64(two
boxes)=160. Complete selected scalar224 is below both existing child344 and
report1112 string-scalar envelopes, so use the scalar maximum rather than add
this on top of that maximum. Shared BigInteger's static minimum-int uint[1]
adds32 once if that cold owner is included, without presumed prior-init credit.
The explicit synchronous parse stackalloc peak is22+2*64*4=534 bytes; it is
distinct from cumulative heap and the separately qualified CLR/native frames.
No general heap/RSS, metadata/culture initialization or actual measured layout
follows. Ordinary selected runtime/source/Release association remains required.

Convert-TaskControlOperand later constructs a fresh BigInteger(UInt64Max) for
its comparison. That constructor independently allocates another uint[2],32
bytes on every reached branch. It is outside b20. Its temporary-value boxing
and the eventual normalized UInt64 box retain their own exact model boundaries;
the normalized output box24 cannot sponsor this limbs array.

For FileInfo, the pinned MarshalByRefObject base has no instance fields.
Selected FileSystemInfo contributes four references, bool and int, plus Windows
WIN32_FILE_ATTRIBUTE_DATA. The latter has three FILE_TIME values (each two
DWORDs) and three further DWORDs, nine in total. A conservative scalar-slot
upper gives72 for that embedded value and16+6*8+72=136 for the whole FileInfo.
This avoids an unmeasured claim of its tighter actual CLR layout. FileInfo's
constructor stores paths but Name stays lazy; Length does not create a Name
string. Every path normalization/referent string, constructor occasion, stream,
error and other V/P owner remains additional. The field-shape/source association
and all actual counts still need the independent whole phase proof.

The initial source fetch expected the historical BigNumber.cs name, which the
actual pinned directory does not contain. Its source/index and exact tool error
remain historical; the separate continuation selects Number.BigInteger.cs from
the actual source tree. No failed fetch is represented as an execution PASS.
