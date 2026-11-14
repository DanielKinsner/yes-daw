#include "independent_clock.h"
#include "engine/Time.h"
#include "source-costs.h"
#include <array>
#include <bit>
#include <cfenv>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <intrin.h>
#include <io.h>
#include <share.h>
#include <string>
#include <sys/stat.h>
#include <type_traits>
#include <xmmintrin.h>
#include <cstddef>

#pragma fenv_access(on)
#pragma fp_contract(off)

#if !defined(_M_X64) || !defined(_DLL) || !defined(_MT) || defined(NDEBUG)
#error Unsupported Clock compile profile
#endif

static_assert(_MSC_FULL_VER == 194435228 && _ITERATOR_DEBUG_LEVEL == 0 && _CRT_PACKING == 8);
static_assert(std::endian::native == std::endian::little);
static_assert(sizeof(void*) == 8 && sizeof(std::size_t) == 8 && sizeof(std::uint64_t) == 8);
static_assert(sizeof(double) == 8 && sizeof(int) == 4 && sizeof(bool) == 1);

#pragma section(".yexact", read)
extern "C"
{
__declspec(allocate(".yexact")) extern constinit const std::uint64_t yesdaw_exact_context_cells[] = {
    0x5944455841435431ull, 1, 31,
    sizeof(std::nullptr_t), alignof(std::nullptr_t),
    sizeof(std::less<void>), alignof(std::less<void>),
    sizeof(std::equal_to<void>), alignof(std::equal_to<void>),
    sizeof(std::_Fake_allocator), alignof(std::_Fake_allocator),
    sizeof(std::_Fake_proxy_ptr_impl), alignof(std::_Fake_proxy_ptr_impl),
    sizeof(std::_Tidy_guard<std::vector<qc::Change>>),
    alignof(std::_Tidy_guard<std::vector<qc::Change>>),
    sizeof(std::_Zero_then_variadic_args_t), alignof(std::_Zero_then_variadic_args_t),
    sizeof(std::_One_then_variadic_args_t), alignof(std::_One_then_variadic_args_t),
    sizeof(std::_String_constructor_concat_tag), alignof(std::_String_constructor_concat_tag),
    sizeof(std::allocator<qc::Change>), alignof(std::allocator<qc::Change>),
    sizeof(std::allocator<char>), alignof(std::allocator<char>),
    sizeof(::__std_exception_data), alignof(::__std_exception_data),
    sizeof(yesdaw::engine::TempoMapView), alignof(yesdaw::engine::TempoMapView),
    _MSC_FULL_VER, _MSC_VER, _CRT_PACKING, _ITERATOR_DEBUG_LEVEL,
    clock_stage::source_costs::familyCount,
};
}
