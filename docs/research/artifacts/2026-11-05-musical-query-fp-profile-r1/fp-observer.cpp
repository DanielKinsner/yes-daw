#include "probe-types.h"
#include <cerrno>
#include <cfenv>

#if defined(_M_X64) || defined(__x86_64__)
 #include <xmmintrin.h>
 #if defined(_MSC_VER)
  #include <float.h>
 #endif
#endif

#if defined(_MSC_VER)
 #pragma fenv_access (on)
#endif

namespace yesdaw::query_fp_probe {
// Compile this TU with /fp:strict for both numeric profiles. All reads are observational.
YESDAW_PROBE_NOINLINE NativeSnapshot snapshot() noexcept
{
    NativeSnapshot s;
    s.errorNumber = errno;
    s.rounding = std::fegetround();
    s.exceptions = std::fetestexcept (FE_ALL_EXCEPT);
#if defined(_M_X64) || defined(__x86_64__)
    const auto mxcsr = static_cast<U64> (_mm_getcsr());
    s.control0 = mxcsr & ~U64 { 0x3f };
    s.status0 = mxcsr & U64 { 0x3f };
    const bool sseKnown = (mxcsr & U64 { 0x1f80 }) == U64 { 0x1f80 }
                      && (mxcsr & U64 { 0x6000 }) == 0
                      && (mxcsr & U64 { 0x8040 }) == 0;
 #if defined(_MSC_VER)
    unsigned int word = 0;
    const auto error = _controlfp_s (&word, 0, 0);
    s.control1 = static_cast<U64> (word);
    const bool crtKnown = error == 0 && (word & _MCW_EM) == _MCW_EM
                       && (word & _MCW_RC) == _RC_NEAR
                       && (word & _EM_AMBIGUOUS) == 0;
    s.supported = sseKnown && crtKnown && s.rounding == FE_TONEAREST;
 #elif defined(__GNUC__) || defined(__clang__)
    unsigned short word = 0, status = 0;
    __asm__ __volatile__ ("fnstcw %0" : "=m" (word));
    __asm__ __volatile__ ("fnstsw %0" : "=am" (status));
    s.control1 = static_cast<U64> (word);
    s.status1 = static_cast<U64> (status);
    s.supported = sseKnown && (word & 0x3fu) == 0x3fu
               && (word & 0xc00u) == 0 && s.rounding == FE_TONEAREST;
 #endif
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
    __asm__ __volatile__ ("mrs %0, fpcr" : "=r" (s.control0));
    __asm__ __volatile__ ("mrs %0, fpsr" : "=r" (s.status0));
    // This measurement accepts only the ordinary all-zero ARM64 control profile.
    s.supported = s.control0 == 0 && s.rounding == FE_TONEAREST;
#endif
    return s;
}
}
