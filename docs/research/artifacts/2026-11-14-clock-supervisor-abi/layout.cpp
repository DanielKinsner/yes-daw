#include "sup.h"
#include <cstddef>

using namespace yesdaw_clock_supervisor_r4;

#if !defined(_M_X64) || !defined(_DLL) || !defined(_MT) || defined(NDEBUG)
#error Unsupported supervisor compile profile
#endif

static_assert(_MSC_FULL_VER == 194435228 && _CRT_PACKING == 8 && _ITERATOR_DEBUG_LEVEL == 0);
static_assert(sizeof(void*) == 8 && sizeof(wchar_t) == 2);

#pragma section(".ysupabi", read)

#define LAYOUT(type) sizeof(type), alignof(type)
extern "C"
{
__declspec(allocate(".ysupabi")) extern constinit const U64 yesdaw_supervisor_cells[] = {
    0x5944535550414249ull, 1,
    LAYOUT(State), LAYOUT(Request), LAYOUT(Terminal), LAYOUT(Worker),
    LAYOUT(WorkerExit), LAYOUT(Meter), LAYOUT(Catalogue), LAYOUT(Wide),
    LAYOUT(Digest), LAYOUT(Text), LAYOUT(Sha256), LAYOUT(Failure), LAYOUT(PipeCount),
    LAYOUT(WIN32_FIND_DATAW), LAYOUT(WIN32_FILE_ATTRIBUTE_DATA),
    LAYOUT(STARTUPINFOEXW), LAYOUT(PROCESS_INFORMATION), LAYOUT(SECURITY_ATTRIBUTES),
    LAYOUT(JOBOBJECT_BASIC_ACCOUNTING_INFORMATION), LAYOUT(JOBOBJECT_EXTENDED_LIMIT_INFORMATION),
    LAYOUT(LARGE_INTEGER), LAYOUT(SRWLOCK), LAYOUT(HANDLE),
    offsetof(State, request), offsetof(State, workers), offsetof(State, hash_buffer),
    offsetof(State, target_command), offsetof(State, environment), offsetof(State, terminal_capture),
    offsetof(Worker, buffer), offsetof(Request, capture),
};
}
#undef LAYOUT
