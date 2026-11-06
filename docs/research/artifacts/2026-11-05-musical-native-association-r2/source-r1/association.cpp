#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <cmath>
#include <cfenv>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace {
struct Entry final
{
    const char* name;
    const void* address;
};

int failure (const char* reason, const char* name) noexcept
{
    std::fprintf (stderr, "FAIL native module association: %s symbol=%s\n", reason, name);
    return 1;
}

int run() noexcept
{
    static_assert (sizeof (void*) == 8);
    // Taking imported function addresses performs no numerical operation and
    // never calls the FMA3 setter, changes FP controls or clears status.
    const Entry entries[] {
        { "log1p", reinterpret_cast<const void*> (&::log1p) },
        { "floor", reinterpret_cast<const void*> (&::floor) },
        { "get_FMA3_enable", reinterpret_cast<const void*> (&::_get_FMA3_enable) },
        { "fegetexceptflag", reinterpret_cast<const void*> (&::fegetexceptflag) },
        { "feclearexcept", reinterpret_cast<const void*> (&::feclearexcept) },
        { "fesetexceptflag", reinterpret_cast<const void*> (&::fesetexceptflag) }
    };
    constexpr std::size_t expected = 6;
    static_assert (sizeof (entries) / sizeof (entries[0]) == expected);
    const int dispatch = ::_get_FMA3_enable();
    if (std::printf ("compiler_numeric=%u fma3_selection=%d symbols=%zu\n",
                     static_cast<unsigned> (_MSC_FULL_VER), dispatch, expected) < 0)
        return failure ("stdout", "inventory");
    std::size_t observed = 0;
    for (const auto& entry : entries)
    {
        HMODULE module = nullptr;
        if (! GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                  | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                  reinterpret_cast<LPCWSTR> (entry.address), &module))
            return failure ("GetModuleHandleExW", entry.name);
        wchar_t path[512] {};
        const DWORD length = GetModuleFileNameW (module, path, 512);
        if (length == 0 || length >= 512 || path[length] != L'\0')
            return failure ("bounded module path", entry.name);
        // The API's FROM_ADDRESS contract identifies the containing module.
        // Use integer arithmetic, avoiding subtraction of unrelated pointers.
        const auto address = reinterpret_cast<std::uintptr_t> (entry.address);
        const auto base = reinterpret_cast<std::uintptr_t> (module);
        if (address < base || address - base > std::numeric_limits<std::uint32_t>::max())
            return failure ("bounded RVA", entry.name);
        const auto rva = static_cast<std::uint32_t> (address - base);
        if (std::printf ("symbol=%s rva=0x%08lx module=%ls\n", entry.name,
                         static_cast<unsigned long> (rva), path) < 0)
            return failure ("stdout", entry.name);
        ++observed;
    }
    if (observed != expected) return failure ("complete inventory", "inventory");
    if (std::printf ("PASS actual loaded module association for this console executable only\n") < 0)
        return failure ("stdout", "result");
    return 0;
}
}

int main()
{
    return run();
}
