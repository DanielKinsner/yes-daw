// YES DAW — the real-time hot-path attribute (one shared home).
//
// YESDAW_RT_HOT marks an audio hot-path function so the RTSan CI leg (-fsanitize=realtime, Clang 20)
// ABORTS if it ever allocates, locks, or does I/O — ADR-0002 #1 / ADR-0006, enforced mechanically, not
// by inspection. It was inline in dsp/SineSource.h during H0; the engine needs it too, so it lives here
// and both sides include it (this header has NO dependencies, so dsp never has to depend on engine).
//
// Guarded on actual attribute support (Clang 20+): MSVC / gcc / AppleClang expand it to nothing, so
// -Werror never trips on an unknown attribute. A function carrying it MUST be `noexcept` (else
// -Wperf-constraint-implies-noexcept fires on the RTSan leg).
//
// NOTE (ADR-0006): FTZ/DAZ denormal-flushing is deliberately NOT here yet. The first compiled graphs
// render a constant DC and produce no denormals; the ScopedNoDenormals / MXCSR+FPCR helper lands with
// the first denormal-producing DSP node, keeping this header platform-asm-free until then.

#pragma once

#if defined(__has_cpp_attribute) && __has_cpp_attribute(clang::nonblocking)
  #define YESDAW_RT_HOT [[clang::nonblocking]]
#else
  #define YESDAW_RT_HOT
#endif

// YESDAW_RT_ASSERT — a real-time-SAFE debug assertion. On failure it executes a trap instruction
// (__debugbreak / __builtin_trap), NOT printf+abort, so it never calls into libc and stays RTSan-clean
// inside a YESDAW_RT_HOT scope. Compiled out under NDEBUG. Use it for hot-path invariants — e.g. a
// use-after-free canary — that must never allocate, lock, or syscall.
#if defined(NDEBUG)
  #define YESDAW_RT_ASSERT(cond) ((void) 0)
#elif defined(_MSC_VER)
  #define YESDAW_RT_ASSERT(cond) do { if (! (cond)) __debugbreak(); } while (false)
#else
  #define YESDAW_RT_ASSERT(cond) do { if (! (cond)) __builtin_trap(); } while (false)
#endif
