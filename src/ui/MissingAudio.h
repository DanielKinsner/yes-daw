// YES DAW — missing audio, as an open asks about it (ADR-0062).
//
// An open that refuses over its Asset files asks about each missing or damaged one in turn; this is what the question
// shows. JUCE-free, so the shell, the native box and the gates share one wording.

#pragma once

#include "engine/Project.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace yesdaw::ui {

struct UiMissingAsset
{
    std::string name;          // ADR-0056: its first clip's name, else "Asset <first eight hex digits>"
    std::string description;   // "<name> - <m:ss.s> <rate> kHz <mono|stereo>, used by <n> clip(s)"
    std::string refusal;       // why the last chosen file was refused (empty on the first question)
    bool damaged = false;      // present with the wrong bytes, rather than missing
};

// The question about the Asset with `hash` in the stored `project` (takes and Sampler pads count with the clips).
[[nodiscard]] inline UiMissingAsset describeMissingAsset (const engine::Project& project, const engine::AssetContentHash& hash,
                                                          bool damaged)
{
    UiMissingAsset out;
    out.damaged = damaged;
    const engine::Asset* asset = nullptr;
    for (const engine::Asset& candidate : project.assets)
        if (candidate.contentHash == hash)
            asset = &candidate;

    std::size_t uses = 0;
    if (asset != nullptr)
    {
        for (const engine::Clip& clip : project.clips)
            if (clip.assetId == asset->id)
            {
                if (out.name.empty())
                    out.name = clip.name.c_str();
                ++uses;
            }
        for (const engine::RecordingTake& take : project.recordingTakes)
            uses += take.assetId == asset->id ? 1u : 0u;
        for (const engine::Track& track : project.tracks)
            for (const engine::SamplerPad& pad : track.samplerPads)
                uses += pad.assetId == asset->id ? 1u : 0u;
    }
    if (out.name.empty())
    {
        static constexpr char digits[] = "0123456789abcdef";
        out.name = "Asset ";
        for (std::size_t i = 0; i < 4u; ++i)
        {
            out.name += digits[(hash.bytes[i] >> 4u) & 0x0Fu];
            out.name += digits[hash.bytes[i] & 0x0Fu];
        }
    }

    std::string facts;
    if (asset != nullptr && asset->sampleRate.hz > 0.0)
    {
        const double seconds = static_cast<double> (asset->frames) / asset->sampleRate.hz;
        const long long tenths = std::llround (seconds * 10.0);
        char length[32] = {};
        std::snprintf (length, sizeof (length), "%lld:%02lld.%lld", tenths / 600, (tenths / 10) % 60, tenths % 10);
        const double kHz = asset->sampleRate.hz / 1'000.0;
        char rate[32] = {};
        if (std::floor (kHz) == kHz)
            std::snprintf (rate, sizeof (rate), "%.0f kHz", kHz);
        else
            std::snprintf (rate, sizeof (rate), "%.1f kHz", kHz);
        const std::string channels = asset->channels == 1u ? "mono"
                                   : asset->channels == 2u ? "stereo"
                                                           : std::to_string (asset->channels) + " channels";
        facts = std::string (" - ") + length + " " + rate + " " + channels;
    }
    const std::string used = uses == 0u ? "not used by any clip"
                           : uses == 1u ? "used by 1 clip"
                                        : "used by " + std::to_string (uses) + " clips";
    out.description = out.name + facts + ", " + used;
    return out;
}

} // namespace yesdaw::ui
