#include <catch2/catch_test_macros.hpp>

#include <cctype>
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

std::string trimCopy (std::string_view text)
{
    while (! text.empty() && std::isspace (static_cast<unsigned char> (text.front())) != 0)
        text.remove_prefix (1);

    while (! text.empty() && std::isspace (static_cast<unsigned char> (text.back())) != 0)
        text.remove_suffix (1);

    return std::string { text };
}

bool isRawNumericToken (const std::string& text)
{
    static const std::regex rawNumeric { R"(^[0-9]+(?:\.[0-9]+)?f?$)" };
    return std::regex_match (trimCopy (text), rawNumeric);
}

std::vector<std::string> splitTopLevelArgs (std::string_view args)
{
    std::vector<std::string> result;
    std::size_t start = 0;
    int depth = 0;

    for (std::size_t i = 0; i < args.size(); ++i)
    {
        const char c = args[i];
        if (c == '(' || c == '{' || c == '[')
            ++depth;
        else if (c == ')' || c == '}' || c == ']')
            --depth;
        else if (c == ',' && depth == 0)
        {
            result.push_back (trimCopy (args.substr (start, i - start)));
            start = i + 1;
        }
    }

    result.push_back (trimCopy (args.substr (start)));
    return result;
}

bool roundedRectangleUsesRawRadius (std::string_view statement)
{
    const auto fillPos = statement.find ("fillRoundedRectangle");
    const auto drawPos = statement.find ("drawRoundedRectangle");
    const bool isFill = fillPos != std::string_view::npos
                     && (drawPos == std::string_view::npos || fillPos < drawPos);
    const std::size_t functionPos = isFill ? fillPos : drawPos;
    if (functionPos == std::string_view::npos)
        return false;

    const auto open = statement.find ('(', functionPos);
    const auto close = statement.rfind (')');
    if (open == std::string_view::npos || close == std::string_view::npos || close <= open)
        return false;

    const auto args = splitTopLevelArgs (statement.substr (open + 1, close - open - 1));
    if (isFill)
    {
        if (args.size() != 2u && args.size() != 5u)
            return false;

        return isRawNumericToken (args.back());
    }

    if (args.size() != 3u && args.size() != 6u)
        return false;

    return isRawNumericToken (args[args.size() - 2u]);
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
        std::string roundedStatement;
        int roundedStatementLine = 0;
        while (std::getline (in, line))
        {
            ++lineNumber;
            if (std::regex_search (line, rawHexColour)
                || std::regex_search (line, juceNamedColour)
                || std::regex_search (line, rawFontSize))
            {
                findings.push_back ({ entry.path(), lineNumber, line });
            }

            if (roundedStatement.empty())
            {
                if (line.find ("fillRoundedRectangle") == std::string::npos
                    && line.find ("drawRoundedRectangle") == std::string::npos)
                {
                    continue;
                }

                roundedStatementLine = lineNumber;
            }

            if (! roundedStatement.empty())
                roundedStatement += ' ';
            roundedStatement += line;

            if (line.find (";") != std::string::npos)
            {
                if (roundedRectangleUsesRawRadius (roundedStatement))
                    findings.push_back ({ entry.path(), roundedStatementLine, roundedStatement });

                roundedStatement.clear();
                roundedStatementLine = 0;
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
        out << "void panel(juce::Graphics& g, juce::Rectangle<int> r) { g.fillRoundedRectangle (r.toFloat(), 4.0f); }\n";
    }

    const auto findings = auditThemeTokens (scratch);
    std::filesystem::remove_all (scratch);

    REQUIRE (findings.size() == 3u);
    REQUIRE (findings.front().line == 1);
    REQUIRE (findings[1].line == 2);
    REQUIRE (findings.back().line == 3);
}
