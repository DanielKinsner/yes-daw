#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#ifndef YESDAW_SOURCE_DIR
#error "YESDAW_SOURCE_DIR must point at the repository root"
#endif

namespace {

struct ThemeAuditFinding
{
    std::filesystem::path path;
    int line = 0;
    std::string text;
};

bool isUiSourceFile (const std::filesystem::path& path)
{
    const std::string ext = path.extension().string();
    return ext == ".h" || ext == ".hpp" || ext == ".cpp" || ext == ".mm";
}

bool isThemeDefinitionFile (const std::filesystem::path& path)
{
    return path.filename() == "UiTheme.h";
}

std::vector<ThemeAuditFinding> auditThemeTokens (const std::filesystem::path& root)
{
    const std::regex rawHexColour { R"(\b0xff[0-9A-Fa-f]{6}\b)" };
    const std::regex juceNamedColour { R"(\bjuce::Colours::[A-Za-z_][A-Za-z0-9_]*)" };
    const std::regex rawFontSize { R"(\bFontOptions\s*\(\s*[0-9]+(?:\.[0-9]+)?f?\b)" };
    std::vector<ThemeAuditFinding> findings;

    for (const auto& entry : std::filesystem::recursive_directory_iterator (root))
    {
        if (! entry.is_regular_file() || ! isUiSourceFile (entry.path()) || isThemeDefinitionFile (entry.path()))
            continue;

        std::ifstream in (entry.path());
        REQUIRE (in.is_open());

        std::string line;
        int lineNumber = 0;
        while (std::getline (in, line))
        {
            ++lineNumber;
            if (std::regex_search (line, rawHexColour)
                || std::regex_search (line, juceNamedColour)
                || std::regex_search (line, rawFontSize))
            {
                findings.push_back ({ entry.path(), lineNumber, line });
            }
        }
    }

    return findings;
}

} // namespace

TEST_CASE ("H16 theme audit rejects raw UI tokens outside UiTheme", "[ui][theme]")
{
    const auto findings = auditThemeTokens (std::filesystem::path { YESDAW_SOURCE_DIR } / "src" / "ui");
    if (! findings.empty())
        INFO (findings.front().path.string() + ":" + std::to_string (findings.front().line)
              + ": " + findings.front().text);
    REQUIRE (findings.empty());
}

TEST_CASE ("H16 theme audit negative control catches inline raw tokens", "[ui][theme]")
{
    const auto scratch = std::filesystem::temp_directory_path()
                       / "yesdaw-theme-audit-negative-control";
    std::filesystem::remove_all (scratch);
    std::filesystem::create_directories (scratch);

    {
        std::ofstream out (scratch / "ScratchUi.cpp");
        REQUIRE (out.is_open());
        out << "void paint() { const auto raw = juce::Colour (0xff112233); }\n";
        out << "void label() { const auto raw = juce::FontOptions (13.0f); }\n";
    }

    const auto findings = auditThemeTokens (scratch);
    std::filesystem::remove_all (scratch);

    REQUIRE (findings.size() == 2u);
    REQUIRE (findings.front().line == 1);
    REQUIRE (findings.back().line == 2);
}
