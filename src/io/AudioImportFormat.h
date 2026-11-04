// YES DAW - shared audio import values and format policy, independent of the codec library.
#pragma once

#include "io/PathText.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace yesdaw::io {

enum class ImportAudioFormatKind : std::uint8_t
{
    Wav,
    Aiff,
    Flac,
    OggVorbis,
    Mp3
};

struct ImportAudioFormat
{
    ImportAudioFormatKind kind;
    std::string_view name;
    std::array<std::string_view, 3> extensions;   // lower case, with the dot; "" pads the array
};

// The supported list and its order (ADR-0054). MP3, the one format without a fixed signature, is last.
inline constexpr std::array<ImportAudioFormat, 5> kImportAudioFormats {{
    { ImportAudioFormatKind::Wav, "WAV", { ".wav", ".wave", "" } },
    { ImportAudioFormatKind::Aiff, "AIFF", { ".aif", ".aiff", ".aifc" } },
    { ImportAudioFormatKind::Flac, "FLAC", { ".flac", "", "" } },
    { ImportAudioFormatKind::OggVorbis, "Ogg Vorbis", { ".ogg", ".oga", "" } },
    { ImportAudioFormatKind::Mp3, "MP3", { ".mp3", "", "" } },
}};

struct DecodedAudioFile
{
    double sampleRateHz = 0.0;
    std::uint64_t frames = 0;
    std::uint16_t channels = 0;
    std::vector<float> interleaved;
    ImportAudioFormatKind format = ImportAudioFormatKind::Wav;
};

struct AudioDecodeResult
{
    std::optional<DecodedAudioFile> audio;
    std::string reason;   // set exactly when `audio` is empty
};

// The recorded shape a stored asset must decode to (the Asset row is the authority).
struct ExpectedAudioShape
{
    std::uint64_t frames = 0;
    double sampleRateHz = 0.0;
    std::uint16_t channels = 0;
};

[[nodiscard]] inline std::string lowerCaseExtension (const std::filesystem::path& path)
{
    std::string extension = yesdaw::io::utf8Text (path.extension());
    std::transform (extension.begin(), extension.end(), extension.begin(),
                    [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });
    return extension;
}

[[nodiscard]] inline const ImportAudioFormat* importAudioFormatForPath (const std::filesystem::path& path)
{
    const std::string extension = lowerCaseExtension (path);
    if (extension.empty())
        return nullptr;
    for (const ImportAudioFormat& format : kImportAudioFormats)
        for (const std::string_view candidate : format.extensions)
            if (! candidate.empty() && candidate == extension)
                return &format;
    return nullptr;
}

[[nodiscard]] inline bool isImportableAudioPath (const std::filesystem::path& path)
{
    return importAudioFormatForPath (path) != nullptr;
}

// "*.wav;*.wave;*.aif;..." — the import choosers' filter, from the one list.
[[nodiscard]] inline std::string importAudioFilePatterns()
{
    std::string patterns;
    for (const ImportAudioFormat& format : kImportAudioFormats)
        for (const std::string_view extension : format.extensions)
            if (! extension.empty())
                patterns += (patterns.empty() ? "*" : ";*") + std::string (extension);
    return patterns;
}

// ADR-0055: the rates an import accepts.
[[nodiscard]] inline bool importSampleRateSupported (double rateHz) noexcept
{
    return rateHz >= 8'000.0 && rateHz <= 384'000.0;
}

} // namespace yesdaw::io
