#include "native-profile.h"
#include <cerrno>
#include <cfenv>
#include <cmath>

#if defined(_MSC_VER) && defined(_M_X64)
 #include <float.h>
 #include <xmmintrin.h>
 #pragma fenv_access (on)
 #define YESDAW_QUERY_OBSERVER_NOINLINE __declspec(noinline)
#else
 #define YESDAW_QUERY_OBSERVER_NOINLINE
#endif

namespace yesdaw::app::musical_origin_source_r7 {
// Readonly qualification observations. MSVC's CRT word is NOT a raw x87 CW.
// Neither this source nor equal snapshots prove the hidden Clock interval.
YESDAW_QUERY_OBSERVER_NOINLINE NativeSnapshot NativeObserver::read() noexcept
{
    // Value-init flags as well: SDK failure may leave fexcept_t untouched, and
    // the later returned aggregate must never copy an indeterminate member.
    NativeSnapshot value {};
    value.errorNumber = errno;
    value.rounding = std::fegetround();
    value.exceptions = std::fetestexcept (FE_ALL_EXCEPT);
    value.flagError = std::fegetexceptflag (&value.flags, FE_ALL_EXCEPT);
    value.newHandler = std::get_new_handler();
#if defined(_MSC_VER) && defined(_M_X64)
    const auto mxcsr = static_cast<std::uint64_t> (_mm_getcsr());
    unsigned int word = 0;
    value.controlError = _controlfp_s (&word, 0, 0);
    value.kind = NativeKind::WindowsMsvcX64; value.compilerFull = _MSC_FULL_VER;
    value.fma3Selection = _get_FMA3_enable();
    value.mxcsrControl = mxcsr & ~std::uint64_t { 0x3f };
    value.mxcsrStatus = mxcsr & std::uint64_t { 0x3f };
    value.crtControl = static_cast<std::uint64_t> (word);
#else
    value.kind = NativeKind::Unsupported; value.compilerFull = 0;
    value.controlError = -1; value.mxcsrControl = 0; value.mxcsrStatus = 0; value.crtControl = 0;
    value.fma3Selection = -1;
#endif
    return value;
}

// Restore ONLY the extra classifier/replay's status effects, never a global FP
// policy/mode. Caller proves/reads unchanged controls before this actual call.
// Preserve the real Clock's prior status/errno; stop restoring BEFORE real
// resolve so its legitimate later status effects are retained.
YESDAW_QUERY_OBSERVER_NOINLINE bool NativeObserver::restoreStatus (const NativeSnapshot& target) noexcept
{
    const int clearError = std::feclearexcept (FE_ALL_EXCEPT);
    const int setError = std::fesetexceptflag (&target.flags, FE_ALL_EXCEPT);
#if defined(_MSC_VER) && defined(_M_X64)
    // FE_ALL_EXCEPT omits the denormal-operand sticky flag. Restore all SIX
    // raw MXCSR status bits explicitly, keeping every current control bit.
    const auto current = _mm_getcsr();
    _mm_setcsr ((current & ~0x3fu) | (static_cast<unsigned int> (target.mxcsrStatus) & 0x3fu));
    const bool rawStatusRestored = (_mm_getcsr() & 0x3fu) == target.mxcsrStatus;
#else
    const bool rawStatusRestored = false;
#endif
    errno = target.errorNumber;
    return (clearError == 0) & (setError == 0) & rawStatusRestored;
}
} // namespace yesdaw::app::musical_origin_source_r7

#undef YESDAW_QUERY_OBSERVER_NOINLINE
