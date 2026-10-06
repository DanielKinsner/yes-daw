// YES DAW — a path as UTF-8 text.
//
// std::filesystem::path::string() converts through the ANSI code page on Windows and throws on a character that
// page cannot hold (a CJK file name on a Western system), so a name the UI shows, a status line or a path built
// from another path's name goes through here instead. JUCE reads std::string as UTF-8.

#pragma once

#include <filesystem>
#include <string>

namespace yesdaw::io {

[[nodiscard]] inline std::string utf8Text (const std::filesystem::path& path)
{
    const std::u8string text = path.u8string();
    return std::string (reinterpret_cast<const char*> (text.data()), text.size());
}

} // namespace yesdaw::io
