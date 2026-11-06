# Candidate observation/restoration: narrow conditional lemma

The file-only checker verified all1,427 displayed code bytes across nine exact
candidate spans against the pinned UCRT file:411 instruction rows. It also checked
the complete64-state six-bit mapping and all4,096 before/after status pairs.
These are actual file/data checks. No native function, DAW or target stage ran.
The checker and its complete emitted lemma freeze at this first passing check.

The candidate file is SHA256
`DEDB7611E1ABC1FF3852DCAC3D4C93A048D729E25B061973B9969F22698F14CA`.
The complete file/data output is `lemma.json`, SHA256
`8F88017B8FFAE57AA73DE744BCCB5B98F515D7C1ED1CB6CF3288DC4947272D9A`.
Its pins identify both prior raw inspection receipts and all nine decoded spans.
Those inspections preserve complete requested ranges; the extracted function
extents exclude padding/other functions and cover every byte without gaps.

## Observation leaves

The actual fegetround leaf [56830,5686A) only stores MXCSR to caller scratch,
reads its6000h round mask and returns0/100h/200h/300h for the four cases. It makes
no call and changes neither MXCSR nor errno/global state. Its ordinary stack
scratch is caller-owned, rather than a claim that it uses no memory.

The shared status helper [55930,5597A) reads the low six MXCSR status bits s.
Its integer mapping T sends hardware bits0/1/2/3/4/5 to abstract bits4/5/3/2/1/0
and returns `T(s) | (T(s)<<24)`. It has no call, floating arithmetic, state write
or global access. The checker covers every six-bit input and the inverse.

fetestexcept[556E0,556FE) with FE_ALL_EXCEPT31 returns `T(s)&31`.
fegetexceptflag[55710,55747) writes `(T(s)&31) | ((T(s)&31)<<24)` to the supplied
valid fexcept_t pointer and returns0. Their only selected callee is that helper.
Neither mutates FP controls/status or errno. Null/invalid destination pointers
are outside this source caller, which supplies an initialized live field.

## Restoration leaves

The set helper [558B0,55923) maps the upper abstract six bits back through D,
then loads `(current_MXCSR & ~63) | D(status)` into MXCSR. All higher control
bits are preserved. It makes no call and performs no floating arithmetic.

feclearexcept[54D40,54D93) with31 clears the five named status flags, preserving
the denormal-operand flag and all controls. fesetexceptflag[55800,5589E) replaces
those same five flags from the prior snapshot while preserving the current
denormal flag and controls. Their only selected callees are the two helpers.
Subsequent integer status comparisons verify the requested named flags and return
zero on this path. The source observer then explicitly merges all six saved raw
bits, including denormal, with current controls. The complete4,096 pair check
shows this status result equals the saved original for every possible pair.
No general x87-state theorem is claimed: these are the selected x64 MXCSR bodies.

## The selected zero-mask control observation

_controlfp_s[5FBA0,5FC09) receives a nonnull current-word pointer, new0 and mask0.
Its invalid-new-control test is zero, so the error/errno/invalid-parameter branch
cannot execute. It calls the helper at5FC10 with new0/mask0, stores the returned
abstract word through that valid pointer and returns0.

The complete[5FC10,5FEFD) helper decodes current MXCSR using integer operations.
Its rounding and denormal-mode decoding branches all rejoin5FCB7. There,
`(~mask & old) | (new & mask)` equals old for mask0, independently of the decoded
word. The equality at5FCC4 bypasses5FCD8 and returns at5FCD0. Every ldmxcsr,
mutable global read/write and exceptional cold path in this helper lies beyond
that excluded unequal-control branch. Thus this exact observation preserves all
MXCSR bits and reaches no errno/handler callback, for every incoming MXCSR value.
This is a proof of the selected arguments, not purity of the general setter.

## Remaining conjunction and scope

Actual final object/thunk/import/provider/loaded-module association, readonly
interval stability and stock ABI/SDK premises remain required. The current
unlinked observer object is not that final image. These candidate spans were not
silently added to the frozen nine-region log certificate or selected Data.

The errno accessor has TLS/lazy-initialization helpers and a split cold path;
this packet does not label it pure or prove those helper effects. The exact
stock-runtime/ABI association and noninterference premise remains explicit,
including its final use after status restoration. get_new_handler and the true
handler/global-dispatch interval retain their separate qualifications. floor's
candidate export jumps through a different ISA-dispatch global and is outside
this fenv lemma; its selected value/provider/ABI contract is still required.

No full NativeFact, complete source funding, physical resource fit, immutable
binding, whole API proof or execution GO follows from this conditional lemma.
Independent review remains pending.
