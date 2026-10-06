// G4.2 cp7 / ADR-0050: FX presets — per-user files of REAL values keyed by stable parameter names.
//
// A preset is `<state dir>/presets/<kind>/<name>.yesfx`, UTF-8 JSON:
//   { "format": "yesdaw.fx-preset", "version": 1, "kind": "Compressor",
//     "params": { "compressor.threshold": -20.0, ... every parameter of the kind ... } }
// A key is the parameter's spec name; a name that repeats within the kind (the EQ's six bands) carries
// its 1-based ordinal in id order ("eq.band.freq.3"). Loading is all or nothing: one specific reason
// refuses the whole file and nothing is applied.
#pragma once

#include "engine/Project.h"
#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace yesdaw::ui {

inline constexpr const char* kFxPresetFormat = "yesdaw.fx-preset";
inline constexpr int kFxPresetVersion = 1;
inline constexpr const char* kFxPresetExtension = ".yesfx";
inline constexpr std::size_t kFxPresetNameMaxLength = 64;
inline constexpr std::size_t kFxPresetMaxBytes = 256u * 1024u;

[[nodiscard]] inline const char* fxPresetKindName (engine::FxKind kind) noexcept
{
    switch (kind)
    {
        case engine::FxKind::Eq:              return "EQ";
        case engine::FxKind::Compressor:      return "Compressor";
        case engine::FxKind::Delay:           return "Delay";
        case engine::FxKind::Reverb:          return "Reverb";
        case engine::FxKind::Limiter:         return "Limiter";
        case engine::FxKind::MidiTranspose:   return "MIDI Transpose";
        case engine::FxKind::MidiScaleMap:    return "MIDI Scale";
        case engine::FxKind::MidiArpeggiator: return "Arpeggiator";
        case engine::FxKind::MidiChord:       return "Chord";
    }
    return "";
}

struct FxPresetKey
{
    std::uint32_t id = 0;
    std::string key;
};

// Every parameter a kind accepts, in id order, with its unique preset key (ids may be sparse; the scan
// bound is generous).
[[nodiscard]] inline std::vector<FxPresetKey> fxPresetKeys (engine::FxKind kind)
{
    std::vector<FxPresetKey> keys;
    for (std::uint32_t id = 0; id < 256u; ++id)
        if (engine::fxKindAcceptsParameterId (kind, id))
            keys.push_back ({ id, engine::fxParamSpecForKind (kind, id).name });
    for (std::size_t i = 0; i < keys.size(); ++i)
    {
        const std::string base = engine::fxParamSpecForKind (kind, keys[i].id).name;
        int ordinal = 0, total = 0;
        for (std::size_t j = 0; j < keys.size(); ++j)
            if (base == engine::fxParamSpecForKind (kind, keys[j].id).name)
            {
                ++total;
                if (j <= i)
                    ++ordinal;
            }
        if (total > 1)
            keys[i].key = base + "." + std::to_string (ordinal);
    }
    return keys;
}

[[nodiscard]] inline std::string fxPresetTrimmedName (std::string_view raw)
{
    std::size_t first = 0, last = raw.size();
    while (first < last && raw[first] == ' ') ++first;
    while (last > first && raw[last - 1] == ' ') --last;
    return std::string (raw.substr (first, last - first));
}

// Empty when `raw` is a usable preset name; otherwise the reason it is not. Names are file stems:
// letters, digits, space, '-', '_', '(' and ')', 1-64 characters after trimming, never a Windows
// device name (CON, NUL, COM1 ...).
[[nodiscard]] inline std::string fxPresetNameRefusal (std::string_view raw)
{
    const std::string name = fxPresetTrimmedName (raw);
    if (name.empty())
        return "a preset needs a name";
    if (name.size() > kFxPresetNameMaxLength)
        return "a preset name has at most 64 characters";
    for (const char c : name)
    {
        const bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                          || c == ' ' || c == '-' || c == '_' || c == '(' || c == ')';
        if (! allowed)
            return "a preset name uses letters, digits, spaces and - _ ( ) only";
    }
    std::string upper = name;
    for (char& c : upper)
        if (c >= 'a' && c <= 'z')
            c = static_cast<char> (c - 'a' + 'A');
    static constexpr const char* kReserved[] = { "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6",
                                                 "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6",
                                                 "LPT7", "LPT8", "LPT9" };
    for (const char* reserved : kReserved)
        if (upper == reserved)
            return "\"" + name + "\" is reserved by Windows";
    return {};
}

// The JSON text of a complete setting: every parameter of the kind as its real value (an insert that
// omits a parameter contributes the spec default, which is what the node runs). A choice is written as
// its index.
[[nodiscard]] inline std::string encodeFxPreset (engine::FxKind kind,
                                                 const std::vector<std::pair<std::uint32_t, double>>& normalizedParams)
{
    auto* params = new juce::DynamicObject();
    juce::var paramsVar (params);
    for (const FxPresetKey& key : fxPresetKeys (kind))
    {
        const engine::ParamSpec spec = engine::fxParamSpecForKind (kind, key.id);
        double normalized = engine::normalizedDefault (spec);
        for (const auto& [paramId, value] : normalizedParams)
            if (paramId == key.id)
                normalized = value;
        const double real = engine::mapNormalized (spec, normalized);
        if (spec.choiceCount >= 2)
            params->setProperty (juce::Identifier (key.key), static_cast<int> (std::lround (real)));
        else
            params->setProperty (juce::Identifier (key.key), real);
    }
    auto* root = new juce::DynamicObject();
    juce::var rootVar (root);
    root->setProperty ("format", kFxPresetFormat);
    root->setProperty ("version", kFxPresetVersion);
    root->setProperty ("kind", fxPresetKindName (kind));
    root->setProperty ("params", paramsVar);
    return juce::JSON::toString (rootVar).toStdString();
}

struct FxPresetDecode
{
    bool ok = false;
    std::string reason;   // when refused: the one specific reason
    std::vector<std::pair<std::uint32_t, double>> normalizedParams;   // when ok: every parameter of the kind
};

[[nodiscard]] inline FxPresetDecode decodeFxPreset (std::string_view text, engine::FxKind kind)
{
    FxPresetDecode out;
    const auto refuse = [&out] (std::string reason) {
        out.ok = false;
        out.reason = std::move (reason);
        out.normalizedParams.clear();
        return out;
    };
    if (text.size() > kFxPresetMaxBytes)
        return refuse ("the file is too large to be a preset");
    if (text.size() >= 3 && static_cast<unsigned char> (text[0]) == 0xEF && static_cast<unsigned char> (text[1]) == 0xBB
        && static_cast<unsigned char> (text[2]) == 0xBF)
        text.remove_prefix (3);
    const std::string owned (text);
    if (owned.find ('\0') != std::string::npos)   // juce::String would end the text there and parse a prefix
        return refuse ("the file is not a preset (not text)");
    if (! juce::CharPointer_UTF8::isValidString (owned.c_str(), static_cast<int> (owned.size())))
        return refuse ("the file is not a preset (not UTF-8 text)");
    juce::var root;
    if (const juce::Result parsed = juce::JSON::parse (juce::String (juce::CharPointer_UTF8 (owned.c_str())), root);
        parsed.failed() || root.getDynamicObject() == nullptr)   // an array is a var object too
        return refuse ("the file is not a preset (not readable JSON)");
    if (root.getProperty ("format", {}).toString() != kFxPresetFormat)
        return refuse ("the file is not a YES DAW FX preset");
    const juce::var version = root.getProperty ("version", {});
    if (! (version.isInt() || version.isInt64() || version.isDouble()) || ! std::isfinite (static_cast<double> (version))
        || static_cast<double> (version) < 1.0 || std::floor (static_cast<double> (version)) != static_cast<double> (version))
        return refuse ("the preset has no valid version");
    if (static_cast<double> (version) > kFxPresetVersion)
        return refuse ("the preset was saved by a newer YES DAW (version " + version.toString().toStdString() + ")");
    const juce::String presetKind = root.getProperty ("kind", {}).toString();
    if (presetKind.isEmpty())
        return refuse ("the preset does not say which effect it is for");
    if (presetKind != fxPresetKindName (kind))
        return refuse ("the preset is for " + presetKind.toStdString() + ", not " + fxPresetKindName (kind));
    const juce::DynamicObject* object = root.getProperty ("params", {}).getDynamicObject();
    if (object == nullptr)
        return refuse ("the preset has no parameters");

    const std::vector<FxPresetKey> keys = fxPresetKeys (kind);
    std::vector<std::pair<std::uint32_t, double>> values;
    for (const FxPresetKey& key : keys)
        values.emplace_back (key.id, engine::normalizedDefault (engine::fxParamSpecForKind (kind, key.id)));
    for (const auto& property : object->getProperties())
    {
        const std::string name = property.name.toString().toStdString();
        const auto it = std::find_if (keys.begin(), keys.end(), [&] (const FxPresetKey& key) { return key.key == name; });
        if (it == keys.end())
            return refuse ("the preset names an unknown parameter \"" + name + "\"");
        const juce::var& value = property.value;
        if (! (value.isInt() || value.isInt64() || value.isDouble()))
            return refuse ("\"" + name + "\" is not a number");
        const double real = static_cast<double> (value);
        const engine::ParamSpec spec = engine::fxParamSpecForKind (kind, it->id);
        if (! std::isfinite (real) || real < spec.min || real > spec.max)
            return refuse ("\"" + name + "\" is outside its range");
        double normalized = engine::unmapToNormalized (spec, real);
        if (spec.choiceCount >= 2)
        {
            if (std::floor (real) != real)
                return refuse ("\"" + name + "\" is not one of its choices");
            normalized = engine::normalizedForChoice (spec, static_cast<std::uint8_t> (real));
        }
        values[static_cast<std::size_t> (it - keys.begin())].second = normalized;
    }
    out.ok = true;
    out.normalizedParams = std::move (values);
    return out;
}

[[nodiscard]] inline juce::File fxPresetJuceFile (const std::filesystem::path& path)
{
    const std::u8string utf8 = path.u8string();
    return juce::File (juce::String (juce::CharPointer_UTF8 (reinterpret_cast<const char*> (utf8.c_str()))));
}

[[nodiscard]] inline std::filesystem::path fxPresetDirectory (const std::filesystem::path& stateDirectory, engine::FxKind kind)
{
    return stateDirectory / "presets" / fxPresetKindName (kind);
}

// This kind's preset names, alphabetical (case-insensitive). Only usable names are listed: a file whose
// stem is not a preset name cannot be one.
[[nodiscard]] inline std::vector<std::string> listFxPresets (const std::filesystem::path& stateDirectory, engine::FxKind kind)
{
    std::vector<std::string> names;
    if (stateDirectory.empty())
        return names;
    std::error_code ec;
    const std::filesystem::path directory = fxPresetDirectory (stateDirectory, kind);
    if (! std::filesystem::is_directory (directory, ec))
        return names;
    for (const auto& entry : std::filesystem::directory_iterator (directory, ec))
    {
        if (! entry.is_regular_file (ec) || entry.path().extension() != kFxPresetExtension)
            continue;
        const std::u8string stem = entry.path().stem().u8string();
        const std::string name (reinterpret_cast<const char*> (stem.c_str()), stem.size());
        if (fxPresetNameRefusal (name).empty() && name == fxPresetTrimmedName (name))
            names.push_back (name);
    }
    std::sort (names.begin(), names.end(), [] (const std::string& a, const std::string& b) {
        return juce::String (a).compareIgnoreCase (juce::String (b)) < 0;
    });
    return names;
}

struct FxPresetWrite
{
    bool ok = false;
    std::string reason;
};

[[nodiscard]] inline FxPresetWrite saveFxPreset (const std::filesystem::path& stateDirectory,
                                                 engine::FxKind kind,
                                                 std::string_view rawName,
                                                 const std::vector<std::pair<std::uint32_t, double>>& normalizedParams)
{
    if (stateDirectory.empty())
        return { false, "there is no place to keep presets" };
    if (const std::string refusal = fxPresetNameRefusal (rawName); ! refusal.empty())
        return { false, refusal };
    const std::string name = fxPresetTrimmedName (rawName);
    std::error_code ec;
    const std::filesystem::path directory = fxPresetDirectory (stateDirectory, kind);
    std::filesystem::create_directories (directory, ec);
    if (ec)
        return { false, "the presets folder could not be created" };
    for (const std::string& existing : listFxPresets (stateDirectory, kind))
        if (juce::String (existing).equalsIgnoreCase (juce::String (name)))
            return { false, "a preset named \"" + existing + "\" already exists" };
    const std::filesystem::path file = directory / (name + kFxPresetExtension);
    if (std::filesystem::exists (file, ec))
        return { false, "a preset named \"" + name + "\" already exists" };
    const std::string text = encodeFxPreset (kind, normalizedParams) + "\n";
    if (! fxPresetJuceFile (file).replaceWithData (text.data(), text.size()))
        return { false, "the preset could not be written" };
    return { true, {} };
}

[[nodiscard]] inline FxPresetDecode loadFxPreset (const std::filesystem::path& stateDirectory, engine::FxKind kind, const std::string& name)
{
    FxPresetDecode refused;
    if (stateDirectory.empty() || ! fxPresetNameRefusal (name).empty())
    {
        refused.reason = "there is no preset named \"" + name + "\"";
        return refused;
    }
    const juce::File source = fxPresetJuceFile (fxPresetDirectory (stateDirectory, kind) / (name + kFxPresetExtension));
    if (! source.existsAsFile())
    {
        refused.reason = "the preset \"" + name + "\" is gone";
        return refused;
    }
    if (source.getSize() > static_cast<juce::int64> (kFxPresetMaxBytes))
    {
        refused.reason = "the file is too large to be a preset";
        return refused;
    }
    juce::MemoryBlock bytes;
    if (! source.loadFileAsData (bytes))
    {
        refused.reason = "the preset \"" + name + "\" could not be read";
        return refused;
    }
    return decodeFxPreset (std::string_view (static_cast<const char*> (bytes.getData()), bytes.getSize()), kind);
}

} // namespace yesdaw::ui
