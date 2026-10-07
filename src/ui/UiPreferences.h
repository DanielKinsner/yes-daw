// YES DAW — the user's preferences (ADR-0061): one prefs.json in the per-user session-state folder.
//
// The document is kept as it was read, so keys this version does not know (a newer version's) are written back
// untouched; the values this version knows are parsed out of it, each falling back to its default on its own when
// it has the wrong type or range. JUCE-free (choc's JSON), so the model and its pure tests share it.

#pragma once

#include "choc/text/choc_JSON.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace yesdaw::ui {

// What reading prefs.json found (the probe's `prefs.state`).
enum class UiPreferencesState : std::uint8_t
{
    NotRead,      // no session-state folder: preferences live in memory only
    Missing,      // the first launch
    Loaded,
    Unreadable    // not a JSON object: the defaults, the file kept aside
};

class UiPreferences
{
public:
    static constexpr const char* kFileName = "prefs.json";
    static constexpr const char* kUnreadableFileName = "prefs.json.unreadable";
    static constexpr std::int64_t kVersion = 1;

    // An action's stable id and its chord ("" = unbound): the non-default bindings. nullopt = the file had none.
    std::optional<std::vector<std::pair<std::string, std::string>>> keymap;

    // Parse a file's text. False when it is not a JSON object (the caller keeps the defaults); true otherwise, every
    // known key that is malformed counted in rejectedKeys() and left at its default. A UTF-8 byte-order mark (an
    // editor's) is skipped.
    [[nodiscard]] bool parse (const std::string& text)
    {
        std::string_view body = text;
        const auto byte = [&body] (std::size_t i) { return static_cast<unsigned char> (body[i]); };
        if (body.size() >= 3 && byte (0) == 0xEFu && byte (1) == 0xBBu && byte (2) == 0xBFu)   // the mark, as bytes
            body.remove_prefix (3);
        choc::value::Value parsed;
        try
        {
            parsed = choc::json::parse (body);
        }
        catch (...)
        {
            return false;
        }
        if (! parsed.isObject())
            return false;

        document_ = std::move (parsed);
        rejectedKeys_ = 0;
        version_ = kVersion;
        keymap.reset();
        if (document_.hasObjectMember ("version"))
        {
            const choc::value::ValueView version = document_["version"];   // a whole number >= 1 (7.0 is 7)
            const double number = version.isInt() || version.isFloat() ? version.get<double>() : 0.0;
            if (number >= 1.0 && number <= 1.0e9 && std::floor (number) == number)
                version_ = static_cast<std::int64_t> (number);
            else
                ++rejectedKeys_;
        }
        if (document_.hasObjectMember ("keymap"))
        {
            const choc::value::ValueView entries = document_["keymap"];
            if (! entries.isObject())
            {
                ++rejectedKeys_;
            }
            else
            {
                std::vector<std::pair<std::string, std::string>> bindings;
                for (std::uint32_t i = 0; i < entries.size(); ++i)
                {
                    const choc::value::MemberNameAndValue member = entries.getObjectMemberAt (i);
                    if (member.value.isString())
                        bindings.emplace_back (member.name, std::string (member.value.getString()));
                    else
                        ++rejectedKeys_;
                }
                keymap = std::move (bindings);
            }
        }
        return true;
    }

    // The whole document: what was read (unknown keys kept), this version's keys set over it. A higher version
    // number read from a newer YES DAW is kept.
    [[nodiscard]] std::string serialise() const
    {
        choc::value::Value out = document_;
        out.setMember ("version", choc::value::createInt64 (std::max (version_, kVersion)));
        choc::value::Value bindings = choc::value::createObject ("");
        for (const auto& [stableId, chord] : keymap.value_or (std::vector<std::pair<std::string, std::string>> {}))
            bindings.setMember (stableId, choc::value::createString (chord));
        out.setMember ("keymap", bindings);
        return choc::json::toString (out, true) + "\n";
    }

    [[nodiscard]] int rejectedKeys() const noexcept { return rejectedKeys_; }
    void rejectKey() noexcept { ++rejectedKeys_; }   // a key the model could not apply (an invalid chord)

private:
    choc::value::Value document_ = choc::value::createObject ("");
    std::int64_t version_ = kVersion;
    int rejectedKeys_ = 0;
};

} // namespace yesdaw::ui
