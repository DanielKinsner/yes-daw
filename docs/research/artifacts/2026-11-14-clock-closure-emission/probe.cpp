#include <cstdint>

#pragma section(".yclos", read)

template<unsigned Site, class Closure>
struct ClosureLayout
{
    __declspec(dllexport) __declspec(allocate(".yclos"))
    static inline constinit const std::uint64_t cells[] = {
        0x5944434C4F533031ull, Site, sizeof(Closure), alignof(Closure),
    };
};

void observe_closure_emission(int input)
{
    const auto captured = [&input] { return input; };
    const auto uncaptured = [](int value) { return value; };
    (void)&ClosureLayout<1, decltype(captured)>::cells;
    (void)&ClosureLayout<2, decltype(uncaptured)>::cells;
}
