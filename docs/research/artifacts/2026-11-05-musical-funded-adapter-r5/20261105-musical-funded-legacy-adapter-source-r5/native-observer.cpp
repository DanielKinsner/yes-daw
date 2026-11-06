#include "native-profile.h"
#include <cerrno>
#include <cfenv>
#include <cmath>

#if defined(_MSC_VER) && defined(_M_X64)
 #if defined(PSAPI_VERSION) && PSAPI_VERSION != 2
  #error This exact native profile requires PSAPI_VERSION 2 and K32GetModuleInformation.
 #endif
 #ifndef PSAPI_VERSION
  #define PSAPI_VERSION 2
 #endif
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <psapi.h>
 #include <float.h>
 #include <xmmintrin.h>
 #pragma comment (lib, "Kernel32.lib")
 #pragma fenv_access (on)
 #define YESDAW_QUERY_OBSERVER_NOINLINE __declspec(noinline)
#else
 #define YESDAW_QUERY_OBSERVER_NOINLINE
#endif

namespace yesdaw::app::musical_origin_source_r8 {
// No variable loop or numeric/runtime-status getter precedes qualification.
// These leaves' complete source/POD/SDK footprints are sponsored by caller Sites.
YESDAW_QUERY_OBSERVER_NOINLINE NativeControl NativeObserver::control() noexcept
{
    NativeControl value {};
#if defined(_MSC_VER) && defined(_M_X64)
    value.kind = NativeKind::WindowsMsvcX64; value.compilerFull = _MSC_FULL_VER;
    value.mxcsrControl = static_cast<std::uint64_t> (_mm_getcsr()) & ~std::uint64_t { 0x3f };
#endif
    return value;
}

YESDAW_QUERY_OBSERVER_NOINLINE NativeModule NativeObserver::logAnchorModule() noexcept
{
    NativeModule value {};
#if defined(_MSC_VER) && defined(_M_X64)
    // Select the actual C DOUBLE overload explicitly. Taking its address does
    // not invoke math. A wrapper/thunk in another module simply fails matching.
    using Entry = double (__cdecl*) (double);
    const auto entry = static_cast<Entry> (&::log1p);
    HMODULE module {};
    const auto ok = GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                      | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                      reinterpret_cast<LPCWSTR> (entry), &module);
    value.succeeded = ok != 0;
    value.module = reinterpret_cast<std::uintptr_t> (module);
    value.anchor = reinterpret_cast<std::uintptr_t> (entry);
#endif
    return value;
}

YESDAW_QUERY_OBSERVER_NOINLINE NativeModule NativeObserver::getterAnchorModule() noexcept
{
    NativeModule value {};
#if defined(_MSC_VER) && defined(_M_X64)
    const auto entry = &_get_FMA3_enable; // address ONLY, never call before match.
    HMODULE module {};
    const auto ok = GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                      | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                      reinterpret_cast<LPCWSTR> (entry), &module);
    value.succeeded = ok != 0;
    value.module = reinterpret_cast<std::uintptr_t> (module);
    value.anchor = reinterpret_cast<std::uintptr_t> (entry);
#endif
    return value;
}

YESDAW_QUERY_OBSERVER_NOINLINE NativeImage NativeObserver::moduleInfo (std::uintptr_t module) noexcept
{
    NativeImage value {};
#if defined(_MSC_VER) && defined(_M_X64)
    MODULEINFO info {};
    const auto process = GetCurrentProcess(); // pseudo-handle, no acquired owner.
    const auto ok = K32GetModuleInformation (process, reinterpret_cast<HMODULE> (module),
                                           &info, static_cast<DWORD> (sizeof (info)));
    value.succeeded = ok != 0;
    value.base = reinterpret_cast<std::uintptr_t> (info.lpBaseOfDll);
    value.bytes = static_cast<std::uint64_t> (info.SizeOfImage);
#else
    (void) module;
#endif
    return value;
}

YESDAW_QUERY_OBSERVER_NOINLINE NativePage NativeObserver::page (std::uintptr_t address) noexcept
{
    NativePage value {};
#if defined(_MSC_VER) && defined(_M_X64)
    static_assert (committed == MEM_COMMIT && image == MEM_IMAGE);
    static_assert (readOnly == PAGE_READONLY && executeRead == PAGE_EXECUTE_READ);
    static_assert (readWrite == PAGE_READWRITE && writeCopy == PAGE_WRITECOPY);
    MEMORY_BASIC_INFORMATION info {};
    const auto returned = VirtualQuery (reinterpret_cast<const void*> (address), &info, sizeof (info));
    value.returnedBytes = static_cast<std::uint64_t> (returned);
    value.expectedBytes = static_cast<std::uint64_t> (sizeof (info));
    value.base = reinterpret_cast<std::uintptr_t> (info.BaseAddress);
    value.allocationBase = reinterpret_cast<std::uintptr_t> (info.AllocationBase);
    value.regionBytes = static_cast<std::uint64_t> (info.RegionSize);
    value.state = info.State; value.type = info.Type; value.protect = info.Protect;
#else
    (void) address;
#endif
    return value;
}

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
} // namespace yesdaw::app::musical_origin_source_r8

#undef YESDAW_QUERY_OBSERVER_NOINLINE
