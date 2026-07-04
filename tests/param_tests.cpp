// YES DAW — H14 CP1 ParamSpec mapping gates.

#include "engine/ParamSpec.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>

using yesdaw::engine::ParamMapping;
using yesdaw::engine::ParamSmoothing;
using yesdaw::engine::ParamSpec;
using yesdaw::engine::clampParamReal;
using yesdaw::engine::mapNormalized;
using yesdaw::engine::normalizedDefault;
using yesdaw::engine::unmapToNormalized;

namespace {

constexpr ParamSpec kLinear {
    1u,
    "Width",
    "%",
    -100.0,
    100.0,
    0.0,
    ParamMapping::Linear,
    ParamSmoothing::Linear5Ms,
};

constexpr ParamSpec kLog {
    2u,
    "Frequency",
    "Hz",
    20.0,
    20000.0,
    1000.0,
    ParamMapping::Log,
    ParamSmoothing::Linear5Ms,
};

constexpr ParamSpec kDb {
    3u,
    "Gain",
    "dB",
    -24.0,
    24.0,
    0.0,
    ParamMapping::Db,
    ParamSmoothing::Linear5Ms,
};

[[nodiscard]] double reversedLogFixtureMap (const ParamSpec& spec, double v01) noexcept
{
    const double v = std::clamp (v01, 0.0, 1.0);
    if (v <= 0.0)
        return spec.max;
    if (v >= 1.0)
        return spec.min;

    return spec.max * std::pow (spec.min / spec.max, v);
}

void requireRoundTrip (const ParamSpec& spec)
{
    for (const double v : { 0.0, 1.0 })
    {
        const double real = mapNormalized (spec, v);
        REQUIRE (real == (v == 0.0 ? spec.min : spec.max));
        REQUIRE (unmapToNormalized (spec, real) == v);
    }

    for (const double v : { 0.001, 0.01, 0.125, 0.25, 0.5, 0.75, 0.9, 0.999 })
    {
        const double real = mapNormalized (spec, v);
        REQUIRE (std::isfinite (real));
        REQUIRE (std::abs (unmapToNormalized (spec, real) - v) <= 1.0e-12);
    }
}

} // namespace

TEST_CASE ("ParamSpec maps normalized values with exact endpoints and tight round trips",
           "[param][mapping]")
{
    requireRoundTrip (kLinear);
    requireRoundTrip (kLog);
    requireRoundTrip (kDb);

    REQUIRE (mapNormalized (kLinear, 0.25) == -50.0);
    REQUIRE (mapNormalized (kLinear, 0.75) == 50.0);
    REQUIRE (mapNormalized (kDb, 0.5) == 0.0);
    REQUIRE (mapNormalized (kLog, 0.5) == Catch::Approx (std::sqrt (kLog.min * kLog.max)));
}

TEST_CASE ("ParamSpec clamps hostile values to finite in-range outputs",
           "[param][mapping][robust]")
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    for (const ParamSpec& spec : std::array { kLinear, kLog, kDb })
    {
        REQUIRE (mapNormalized (spec, -1.0) == spec.min);
        REQUIRE (mapNormalized (spec, 2.0) == spec.max);
        REQUIRE (mapNormalized (spec, nan) == spec.def);
        REQUIRE (mapNormalized (spec, inf) == spec.def);
        REQUIRE (mapNormalized (spec, -inf) == spec.def);

        REQUIRE (unmapToNormalized (spec, spec.min - 1.0e9) == 0.0);
        REQUIRE (unmapToNormalized (spec, spec.max + 1.0e9) == 1.0);
        REQUIRE (unmapToNormalized (spec, nan) == normalizedDefault (spec));
        REQUIRE (unmapToNormalized (spec, inf) == normalizedDefault (spec));
        REQUIRE (unmapToNormalized (spec, -inf) == normalizedDefault (spec));

        const double clampedDefault = clampParamReal (spec, nan);
        REQUIRE (std::isfinite (clampedDefault));
        REQUIRE (clampedDefault >= spec.min);
        REQUIRE (clampedDefault <= spec.max);
    }

    const ParamSpec invalidLog {
        4u, "Bad", "Hz", 0.0, 20000.0, 1000.0, ParamMapping::Log, ParamSmoothing::None
    };
    REQUIRE (mapNormalized (invalidLog, 0.5) == 0.0);
    REQUIRE (unmapToNormalized (invalidLog, 1000.0) == 0.0);
}

TEST_CASE ("ParamSpec mappings are monotonic over deterministic random pairs",
           "[param][mapping][property]")
{
    std::mt19937_64 rng { 0x5945534441574831ull };
    std::uniform_real_distribution<double> dist (0.0, 1.0);

    for (const ParamSpec& spec : std::array { kLinear, kLog, kDb })
    {
        for (int i = 0; i < 1000; ++i)
        {
            double a = dist (rng);
            double b = dist (rng);
            if (b < a)
                std::swap (a, b);

            const double mappedA = mapNormalized (spec, a);
            const double mappedB = mapNormalized (spec, b);
            REQUIRE (std::isfinite (mappedA));
            REQUIRE (std::isfinite (mappedB));
            REQUIRE (mappedA <= mappedB);
        }
    }
}

TEST_CASE ("ParamSpec negative control: reversed Log mapping fixture fails round-trip",
           "[param][mapping][negctl]")
{
    const double v = 0.25;
    const double reversedReal = reversedLogFixtureMap (kLog, v);
    const double unmapped = unmapToNormalized (kLog, reversedReal);

    REQUIRE (std::abs (unmapped - v) > 0.10);
}
