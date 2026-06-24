// YES DAW - ScopedNoDenormals: RT-safe FTZ/DAZ guard for audio hot paths.
//
// CompiledGraph enables this once it runs real nodes. The guard saves the current floating-point control
// word, enables flush-to-zero behavior for the duration of a block, then restores the previous state.
// It does not allocate, lock, log, or call into the OS.

#pragma once

#include "rt/RtHot.h"

#include <cstdint>

#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
  #include <xmmintrin.h>
#endif

namespace yesdaw::dsp {

class ScopedNoDenormals
{
public:
    ScopedNoDenormals() noexcept YESDAW_RT_HOT
        : saved_ (readControlWord())
    {
        writeControlWord (saved_ | flushBits());
    }

    ~ScopedNoDenormals() noexcept YESDAW_RT_HOT { writeControlWord (saved_); }

    ScopedNoDenormals (const ScopedNoDenormals&)            = delete;
    ScopedNoDenormals& operator= (const ScopedNoDenormals&) = delete;

private:
#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
    using ControlWord = std::uint32_t;

    static ControlWord readControlWord() noexcept YESDAW_RT_HOT
    {
        return static_cast<ControlWord> (_mm_getcsr());
    }

    static void writeControlWord (ControlWord value) noexcept YESDAW_RT_HOT
    {
        _mm_setcsr (value);
    }

    static constexpr ControlWord flushBits() noexcept
    {
        constexpr ControlWord kDenormalsAreZero = 1u << 6;
        constexpr ControlWord kFlushToZero      = 1u << 15;
        return kDenormalsAreZero | kFlushToZero;
    }

#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
    using ControlWord = std::uint64_t;

    static ControlWord readControlWord() noexcept YESDAW_RT_HOT
    {
        ControlWord value = 0;
        __asm__ __volatile__ ("mrs %0, fpcr" : "=r" (value));
        return value;
    }

    static void writeControlWord (ControlWord value) noexcept YESDAW_RT_HOT
    {
        __asm__ __volatile__ ("msr fpcr, %0" : : "r" (value));
    }

    static constexpr ControlWord flushBits() noexcept
    {
        return ControlWord { 1 } << 24; // FPCR.FZ
    }

#else
    using ControlWord = std::uint32_t;

    static constexpr ControlWord readControlWord() noexcept YESDAW_RT_HOT { return 0; }
    static constexpr void        writeControlWord (ControlWord) noexcept YESDAW_RT_HOT {}
    static constexpr ControlWord flushBits() noexcept { return 0; }
#endif

    ControlWord saved_;
};

} // namespace yesdaw::dsp
