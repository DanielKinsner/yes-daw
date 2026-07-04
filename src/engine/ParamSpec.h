// YES DAW — ParamSpec: stable, normalized parameter mapping for built-in FX (ADR-0038).

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace yesdaw::engine {

enum class ParamMapping : std::uint8_t
{
    Linear = 0,
    Log    = 1,
    Db     = 2,
};

enum class ParamSmoothing : std::uint8_t
{
    None       = 0,
    Linear5Ms  = 1,
};

struct ParamSpec
{
    std::uint32_t id        = 0;
    const char*   name      = "";
    const char*   unit      = "";
    double        min       = 0.0;
    double        max       = 1.0;
    double        def       = 0.0;
    ParamMapping  mapping   = ParamMapping::Linear;
    ParamSmoothing smoothing = ParamSmoothing::None;
};

[[nodiscard]] inline bool paramSpecHasUsableRange (const ParamSpec& spec) noexcept
{
    if (! std::isfinite (spec.min) || ! std::isfinite (spec.max) || ! (spec.max > spec.min))
        return false;

    if (spec.mapping == ParamMapping::Log)
        return spec.min > 0.0 && spec.max > 0.0;

    return true;
}

[[nodiscard]] inline double clampParamReal (const ParamSpec& spec, double real) noexcept
{
    if (! paramSpecHasUsableRange (spec))
        return 0.0;

    const double def = std::isfinite (spec.def) ? std::clamp (spec.def, spec.min, spec.max) : spec.min;
    if (! std::isfinite (real))
        return def;

    return std::clamp (real, spec.min, spec.max);
}

[[nodiscard]] inline double normalizedDefault (const ParamSpec& spec) noexcept
{
    if (! paramSpecHasUsableRange (spec))
        return 0.0;

    const double def = clampParamReal (spec, spec.def);
    if (def <= spec.min)
        return 0.0;
    if (def >= spec.max)
        return 1.0;

    switch (spec.mapping)
    {
        case ParamMapping::Log:
            return std::log (def / spec.min) / std::log (spec.max / spec.min);
        case ParamMapping::Linear:
        case ParamMapping::Db:
            return (def - spec.min) / (spec.max - spec.min);
    }

    return 0.0;
}

[[nodiscard]] inline double clampNormalizedParamValue (const ParamSpec& spec, double v01) noexcept
{
    if (! paramSpecHasUsableRange (spec))
        return 0.0;

    if (! std::isfinite (v01))
        return normalizedDefault (spec);

    return std::clamp (v01, 0.0, 1.0);
}

[[nodiscard]] inline double mapNormalized (const ParamSpec& spec, double v01) noexcept
{
    if (! paramSpecHasUsableRange (spec))
        return 0.0;

    if (! std::isfinite (v01))
        return clampParamReal (spec, spec.def);

    const double v = clampNormalizedParamValue (spec, v01);
    if (v <= 0.0)
        return spec.min;
    if (v >= 1.0)
        return spec.max;

    switch (spec.mapping)
    {
        case ParamMapping::Log:
            return spec.min * std::pow (spec.max / spec.min, v);
        case ParamMapping::Linear:
        case ParamMapping::Db:
            return spec.min + v * (spec.max - spec.min);
    }

    return clampParamReal (spec, spec.def);
}

[[nodiscard]] inline double unmapToNormalized (const ParamSpec& spec, double real) noexcept
{
    if (! paramSpecHasUsableRange (spec))
        return 0.0;

    const double x = clampParamReal (spec, real);
    if (x <= spec.min)
        return 0.0;
    if (x >= spec.max)
        return 1.0;

    switch (spec.mapping)
    {
        case ParamMapping::Log:
            return std::log (x / spec.min) / std::log (spec.max / spec.min);
        case ParamMapping::Linear:
        case ParamMapping::Db:
            return (x - spec.min) / (spec.max - spec.min);
    }

    return normalizedDefault (spec);
}

} // namespace yesdaw::engine
