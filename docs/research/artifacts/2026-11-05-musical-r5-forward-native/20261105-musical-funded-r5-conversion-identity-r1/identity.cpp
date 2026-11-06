// Compile-only fundamental-type identity constant; no main or target call.
#include <cstddef>
#include <cstdint>
#include <type_traits>
extern "C" {
extern constinit const std::uint64_t yesdaw_conversion_identity[] = {
    0x5945534441574431ULL, 1ULL,
    std::is_same_v<std::uint64_t, std::size_t> ? 0ULL : 1ULL
};
}
