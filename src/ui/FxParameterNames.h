// G4.2: the name an FX parameter row shows. A ParamSpec's name is a stable id ("compressor.threshold",
// "delay.time_l") that persistence and automation key on; the editor shows a reader's name instead.
#pragma once

#include <array>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>

namespace yesdaw::ui {

[[nodiscard]] inline std::string fxParameterDisplayName (std::string_view stableName)
{
    const std::size_t dot = stableName.rfind ('.');
    const std::string_view leaf = dot == std::string_view::npos ? stableName : stableName.substr (dot + 1);

    // Where the generic rule would read badly: channel suffixes, acronyms, hyphenated terms.
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 5> kOverrides {{
        { "time_l", "Time L" }, { "time_r", "Time R" }, { "ping_pong", "Ping-pong" },
        { "rt60", "Decay" },    { "pre_delay", "Pre-delay" },
    }};
    for (const auto& [id, name] : kOverrides)
        if (leaf == id)
            return std::string (name);

    std::string name (leaf);
    for (char& c : name)
        if (c == '_')
            c = ' ';
    if (! name.empty())
        name[0] = static_cast<char> (std::toupper (static_cast<unsigned char> (name[0])));
    return name;
}

} // namespace yesdaw::ui
