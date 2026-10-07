// YES DAW — the user's preferences (ADR-0061): one prefs.json in the per-user session-state folder.
//
// The document is kept as it was read, so keys this version does not know (a newer version's) are written back
// untouched; the values this version knows are parsed out of it, each falling back to its default on its own when
// it has the wrong type or range. JUCE-free (choc's JSON), so the model and its pure tests share it.

#pragma once

#include "choc/text/choc_JSON.h"
#include "ui/UiActions.h"   // the dock tab and snap mode the view and editing preferences name

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <initializer_list>
#include <optional>
#include <string_view>
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

// ADR-0061 cp2: the view and dock defaults (the last arrangement), the editing and the export defaults. Factory
// values unless the file says otherwise; a size of 0 means "the theme's default" (the shell clamps sizes).
struct UiViewPreferences
{
    int railWidth = 0;
    int inspectorWidth = 0;
    int dockHeight = 0;
    bool narrowStrips = false;
    bool inspectorVisible = true;
    bool inspectorTrackTab = false;
    bool dockVisible = true;
    UiEditorDockTab dockTab = UiEditorDockTab::Mixer;

    friend bool operator== (const UiViewPreferences&, const UiViewPreferences&) = default;
};

struct UiEditingPreferences
{
    bool snapEnabled = true;
    std::int64_t snapGridTicks = 512;   // the chooser's grids: 2048 (bar), 512 (beat), 128 (sixteenth)
    UiSnapMode snapMode = UiSnapMode::Grid;
    bool metronome = false;

    friend bool operator== (const UiEditingPreferences&, const UiEditingPreferences&) = default;
};

struct UiExportPreferences
{
    std::string bitDepth = "float32";   // "float32" | "int24" | "int16"
    bool dither = true;
    bool normalize = false;

    friend bool operator== (const UiExportPreferences&, const UiExportPreferences&) = default;
};

class UiPreferences
{
public:
    static constexpr const char* kFileName = "prefs.json";
    static constexpr const char* kUnreadableFileName = "prefs.json.unreadable";
    static constexpr std::int64_t kVersion = 1;

    // An action's stable id and its chord ("" = unbound): the non-default bindings. nullopt = the file had none.
    std::optional<std::vector<std::pair<std::string, std::string>>> keymap;
    UiViewPreferences view;
    UiEditingPreferences editing;
    UiExportPreferences exportChoices;

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
        readSection ("view", [this] (const choc::value::ValueView& section) {
            readInt (section, "railWidth", view.railWidth);
            readInt (section, "inspectorWidth", view.inspectorWidth);
            readInt (section, "dockHeight", view.dockHeight);
            readBool (section, "narrowStrips", view.narrowStrips);
            readBool (section, "inspectorVisible", view.inspectorVisible);
            readName (section, "inspectorTab", view.inspectorTrackTab, { { "track", true }, { "clip", false } });
            readBool (section, "dockVisible", view.dockVisible);
            readName (section, "dockTab", view.dockTab,
                        { { "mixer", UiEditorDockTab::Mixer }, { "pianoRoll", UiEditorDockTab::PianoRoll },
                          { "instrument", UiEditorDockTab::Instrument }, { "browser", UiEditorDockTab::Browser } });
        });
        readSection ("editing", [this] (const choc::value::ValueView& section) {
            readBool (section, "snapEnabled", editing.snapEnabled);
            readGrid (section, "snapGridTicks", editing.snapGridTicks, { 2048, 512, 128 });
            readName (section, "snapMode", editing.snapMode,
                        { { "grid", UiSnapMode::Grid }, { "relative", UiSnapMode::Relative },
                          { "events", UiSnapMode::Events }, { "off", UiSnapMode::Off } });
            readBool (section, "metronome", editing.metronome);
        });
        readSection ("export", [this] (const choc::value::ValueView& section) {
            readName (section, "bitDepth", exportChoices.bitDepth,
                        { { "float32", std::string ("float32") }, { "int24", std::string ("int24") }, { "int16", std::string ("int16") } });
            readBool (section, "dither", exportChoices.dither);
            readBool (section, "normalize", exportChoices.normalize);
        });
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

        choc::value::Value viewSection = sectionOf (out, "view");
        viewSection.setMember ("railWidth", choc::value::createInt64 (view.railWidth));
        viewSection.setMember ("inspectorWidth", choc::value::createInt64 (view.inspectorWidth));
        viewSection.setMember ("dockHeight", choc::value::createInt64 (view.dockHeight));
        viewSection.setMember ("narrowStrips", choc::value::createBool (view.narrowStrips));
        viewSection.setMember ("inspectorVisible", choc::value::createBool (view.inspectorVisible));
        viewSection.setMember ("inspectorTab", choc::value::createString (view.inspectorTrackTab ? "track" : "clip"));
        viewSection.setMember ("dockVisible", choc::value::createBool (view.dockVisible));
        viewSection.setMember ("dockTab", choc::value::createString (view.dockTab == UiEditorDockTab::PianoRoll    ? "pianoRoll"
                                                                     : view.dockTab == UiEditorDockTab::Instrument ? "instrument"
                                                                     : view.dockTab == UiEditorDockTab::Browser    ? "browser"
                                                                                                                   : "mixer"));
        out.setMember ("view", viewSection);

        choc::value::Value editingSection = sectionOf (out, "editing");
        editingSection.setMember ("snapEnabled", choc::value::createBool (editing.snapEnabled));
        editingSection.setMember ("snapGridTicks", choc::value::createInt64 (editing.snapGridTicks));
        editingSection.setMember ("snapMode", choc::value::createString (editing.snapMode == UiSnapMode::Relative ? "relative"
                                                                         : editing.snapMode == UiSnapMode::Events ? "events"
                                                                         : editing.snapMode == UiSnapMode::Off    ? "off"
                                                                                                                  : "grid"));
        editingSection.setMember ("metronome", choc::value::createBool (editing.metronome));
        out.setMember ("editing", editingSection);

        choc::value::Value exportSection = sectionOf (out, "export");
        exportSection.setMember ("bitDepth", choc::value::createString (exportChoices.bitDepth));
        exportSection.setMember ("dither", choc::value::createBool (exportChoices.dither));
        exportSection.setMember ("normalize", choc::value::createBool (exportChoices.normalize));
        out.setMember ("export", exportSection);
        return choc::json::toString (out, true) + "\n";
    }

    [[nodiscard]] int rejectedKeys() const noexcept { return rejectedKeys_; }
    void rejectKey() noexcept { ++rejectedKeys_; }   // a key the model could not apply (an invalid chord)

private:
    // A section as it was read (its unknown keys kept) or a fresh one.
    [[nodiscard]] static choc::value::Value sectionOf (const choc::value::Value& document, const char* name)
    {
        if (document.hasObjectMember (name) && document[name].isObject())
            return choc::value::Value (document[name]);
        return choc::value::createObject ("");
    }

    template <typename Read>
    void readSection (const char* name, Read&& read)
    {
        if (! document_.hasObjectMember (name))
            return;
        const choc::value::ValueView section = document_[name];
        if (! section.isObject())
        {
            ++rejectedKeys_;
            return;
        }
        read (section);
    }

    void readBool (const choc::value::ValueView& section, const char* key, bool& out)
    {
        if (! section.hasObjectMember (key))
            return;
        if (section[key].isBool())
            out = section[key].getBool();
        else
            ++rejectedKeys_;
    }

    // A whole number in [0, 10000] (sizes in pixels; 0 = the default).
    void readInt (const choc::value::ValueView& section, const char* key, int& out)
    {
        if (! section.hasObjectMember (key))
            return;
        const choc::value::ValueView value = section[key];
        const double number = value.isInt() || value.isFloat() ? value.get<double>() : -1.0;
        if (number >= 0.0 && number <= 10'000.0 && std::floor (number) == number)
            out = static_cast<int> (number);
        else
            ++rejectedKeys_;
    }

    // One of a fixed set of names.
    template <typename Value>
    void readName (const choc::value::ValueView& section, const char* key, Value& out,
                   std::initializer_list<std::pair<const char*, Value>> choices)
    {
        if (! section.hasObjectMember (key))
            return;
        const choc::value::ValueView value = section[key];
        if (value.isString())
            for (const auto& [name, choice] : choices)
                if (value.getString() == std::string_view (name))
                {
                    out = choice;
                    return;
                }
        ++rejectedKeys_;
    }

    // One of the snap chooser's grids.
    void readGrid (const choc::value::ValueView& section, const char* key, std::int64_t& out,
                   std::initializer_list<std::int64_t> grids)
    {
        if (! section.hasObjectMember (key))
            return;
        const choc::value::ValueView value = section[key];
        if (value.isInt() || value.isFloat())
            for (const std::int64_t grid : grids)
                if (value.get<double>() == static_cast<double> (grid))
                {
                    out = grid;
                    return;
                }
        ++rejectedKeys_;
    }

    choc::value::Value document_ = choc::value::createObject ("");
    std::int64_t version_ = kVersion;
    int rejectedKeys_ = 0;
};

} // namespace yesdaw::ui
