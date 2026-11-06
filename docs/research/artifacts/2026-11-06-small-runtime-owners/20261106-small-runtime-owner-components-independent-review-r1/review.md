# Independent small-runtime owner review

Verdict: **ACCEPT_CONDITIONAL_SOURCE_COMPONENT_ONLY**. No functional or arithmetic
revision is required within the stated boundary. This accepts neither measured
object sizes nor a complete phase bound, 32 MiB fit, source/binary association,
binding data, prices or execution authority.

The whole subject argument, component, derivation and seal sources were read.
The selected branches of the pinned official sources and unchanged R12 raw
grammar/control consumer were independently followed. All subject and component
input pins were recomputed. The new independent file/text/integer checker exited
0 with 105 generated checks; no root checker or target was run.

The original-token grammar caps magnitude at UInt64.MaxValue (negative at
Int64.MinValue), rejects leading zeros, and excludes fraction/exponent syntax
through its next-token state. Consequently an admitted Int64 overflow reaching
the ordinary JsonTextReader integer branch is positive, with 19 or 20 digits.
It copies at most String20=64 bytes before BigIntegerParse. The latter returns
object and therefore boxes the value. JsonWriter's Integer branch unboxes the
pattern variable and selects WriteValue(object); it has no BigInteger overload.
That converts the local value back into an object box. JTokenWriter recognizes
that object and passes it through AddRawValue to JValue's object/type constructor.
That constructor stores the same object, with no third box or limbs clone.

The pinned Release StackAllocThreshold is the constant 64. Decimal input length
at most 20 takes byte[length+2], at most 22. At most three base1e9 words and
resultLength at most five take two stackalloc uint[64] arms. Scale<=20 selects
Naive, whose selected MultiplyAdd/span loops have no pool, managed buffer or
recursive call. The final BigInteger constructor trims zeros and takes ToArray
of exactly two uint limbs, because every value in [2^63,2^64-1] has exactly two
base2^32 limbs. Both boxes retain that same immutable array. Under the declared
header/alignment and scalar-slot premises the extra requests are therefore
64+32+32+32=160 beyond JValue64, and 224 is subsumed by both the 344 and 1112
alternative scalar envelopes. This is a maximum, not an additive charge on top
of the chosen scalar maximum.

The 534-byte statement is the sum of explicit stackalloc payloads, not an ABI
stack-frame bound or general stack bound. The static minimum-int uint[1] is a
separate once owner of 32 bytes; its existence is not erased by presumed warmup.
The later Convert-TaskControlOperand UInt64Max constructor allocates its own
uint[2], also 32 bytes, outside b20. Its temporary boxing and normalized output
remain separately sponsored; this component does not certify their occasions.

For FileInfo, the selected FileSystemInfo source declares FullPath, OriginalPath,
_name, _linkTarget, bool _linkTargetIsValid; its Windows part adds int
_dataInitialized and embedded WIN32_FILE_ATTRIBUTE_DATA. FileInfo adds no
instance field, and the pinned MarshalByRefObject base has no instance field.
Three FILE_TIME values each contain two DWORDs, plus three further DWORDs.
The generic source-slot upper gives Align8(16+6*8+9*8)=136. It does not rely on
packing those native fields into their tighter actual layout. The constructor
stores/normalizes paths but leaves Name lazy; Length reaches attributes and
LengthCore without requesting Name. Path referents/normalization, constructor
counts, streams, exceptional control and all V/P joins remain additional.

The source's general NumberFormatInfo/culture/type initialization, runtime and
Release-source correspondence and generic layout premises remain conditional.
No current object offsets, allocator sizes, private field values, runtime parse,
reclamation, general heap/RSS or global fit were observed. No cap, source, ADR,
binding, probe or target was changed. Historical failed fetches remain intact.
