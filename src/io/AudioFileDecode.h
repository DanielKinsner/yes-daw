// YES DAW - ADR-0054: the one audio decoder every import surface and reopen use.
//
// JUCE's own readers, in a fixed order — WAV, AIFF, FLAC, Ogg Vorbis, MP3 — so a file decodes the same on every
// platform (no OS codecs). Import picks the reader by the file's extension; reopen identifies a stored asset by
// its content: the first reader whose decode has the Asset's recorded frames, rate and channels. Every refusal
// carries the reason a person reads on the status line. Mono and stereo only (ADR-0042).

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
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
    std::string extension = path.extension().string();
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

namespace detail {

[[nodiscard]] inline std::unique_ptr<juce::AudioFormat> makeAudioFormat (ImportAudioFormatKind kind)
{
    switch (kind)
    {
        case ImportAudioFormatKind::Wav: return std::make_unique<juce::WavAudioFormat>();
        case ImportAudioFormatKind::Aiff: return std::make_unique<juce::AiffAudioFormat>();
        case ImportAudioFormatKind::Flac: return std::make_unique<juce::FlacAudioFormat>();
        case ImportAudioFormatKind::OggVorbis: return std::make_unique<juce::OggVorbisAudioFormat>();
        case ImportAudioFormatKind::Mp3: return std::make_unique<juce::MP3AudioFormat>();
    }
    return {};
}

[[nodiscard]] inline juce::File juceFileForPath (const std::filesystem::path& path)
{
    const std::u8string utf8 = path.u8string();
    return juce::File { juce::String::fromUTF8 (reinterpret_cast<const char*> (utf8.data()),
                                                static_cast<int> (utf8.size())) };
}

[[nodiscard]] inline std::uint32_t bigEndian32 (const char* bytes) noexcept
{
    return (static_cast<std::uint32_t> (static_cast<unsigned char> (bytes[0])) << 24u)
         | (static_cast<std::uint32_t> (static_cast<unsigned char> (bytes[1])) << 16u)
         | (static_cast<std::uint32_t> (static_cast<unsigned char> (bytes[2])) << 8u)
         | static_cast<std::uint32_t> (static_cast<unsigned char> (bytes[3]));
}

// An AIFF-C file's compression type from its COMM chunk ("" when the file is not AIFF-C or has no COMM).
[[nodiscard]] inline std::string aiffcCompressionType (const std::filesystem::path& path)
{
    std::ifstream in (path, std::ios::binary);
    std::array<char, 12> header {};
    if (! in.read (header.data(), static_cast<std::streamsize> (header.size())))
        return {};
    if (std::string_view (header.data(), 4) != "FORM" || std::string_view (header.data() + 8, 4) != "AIFC")
        return {};
    std::array<char, 8> chunk {};
    while (in.read (chunk.data(), static_cast<std::streamsize> (chunk.size())))
    {
        const std::uint32_t size = bigEndian32 (chunk.data() + 4);
        if (std::string_view (chunk.data(), 4) == "COMM")
        {
            // channels (2), frames (4), bits (2), rate (10, extended), compression type (4)
            std::array<char, 22> comm {};
            if (size < comm.size() || ! in.read (comm.data(), static_cast<std::streamsize> (comm.size())))
                return {};
            return std::string (comm.data() + 18, 4);
        }
        in.seekg (static_cast<std::streamoff> (size + (size & 1u)), std::ios::cur);
    }
    return {};
}

// One reader over one file: the shape checks, then the whole file into interleaved floats.
[[nodiscard]] inline AudioDecodeResult decodeWith (const ImportAudioFormat& format,
                                                   const std::filesystem::path& path,
                                                   const std::optional<ExpectedAudioShape>& expected)
{
    AudioDecodeResult result;
    const std::string unreadable = "not a readable " + std::string (format.name) + " file";
    const std::unique_ptr<juce::AudioFormat> codec = makeAudioFormat (format.kind);
    const juce::File file = juceFileForPath (path);
    std::unique_ptr<juce::AudioFormatReader> reader;
    if (codec != nullptr && file.existsAsFile())
        reader.reset (codec->createReaderFor (new juce::FileInputStream (file), true));
    if (reader == nullptr || ! (reader->sampleRate > 0.0) || reader->numChannels < 1u)
    {
        result.reason = unreadable;
        return result;
    }
    if (reader->numChannels > 2u)
    {
        result.reason = std::to_string (reader->numChannels) + " channels (mono or stereo only)";
        return result;
    }
    if (reader->lengthInSamples <= 0)
    {
        result.reason = "no audio in the file";
        return result;
    }
    if (reader->lengthInSamples > static_cast<juce::int64> (std::numeric_limits<int>::max()))
    {
        result.reason = "too long to import";
        return result;
    }
    if (expected.has_value()
        && (static_cast<std::uint64_t> (reader->lengthInSamples) != expected->frames
            || reader->sampleRate != expected->sampleRateHz
            || reader->numChannels != static_cast<unsigned int> (expected->channels)))
    {
        result.reason = "does not decode to its recorded length, rate and channels";
        return result;
    }

    const int frames = static_cast<int> (reader->lengthInSamples);
    const int channels = static_cast<int> (reader->numChannels);
    juce::AudioBuffer<float> buffer (channels, frames);
    if (! reader->read (&buffer, 0, frames, 0, true, channels > 1))
    {
        result.reason = "could not be read to the end";
        return result;
    }

    DecodedAudioFile decoded;
    decoded.sampleRateHz = reader->sampleRate;
    decoded.frames = static_cast<std::uint64_t> (frames);
    decoded.channels = static_cast<std::uint16_t> (channels);
    decoded.format = format.kind;
    decoded.interleaved.resize (static_cast<std::size_t> (frames) * static_cast<std::size_t> (channels));
    for (int channel = 0; channel < channels; ++channel)
    {
        const float* const source = buffer.getReadPointer (channel);
        for (int frame = 0; frame < frames; ++frame)
            decoded.interleaved[static_cast<std::size_t> (frame) * static_cast<std::size_t> (channels)
                                + static_cast<std::size_t> (channel)] = source[frame];
    }
    result.audio = std::move (decoded);
    return result;
}

} // namespace detail

// Import: the reader the extension names; the refusal reason when it cannot be imported.
[[nodiscard]] inline AudioDecodeResult decodeAudioFile (const std::filesystem::path& path)
{
    const ImportAudioFormat* format = importAudioFormatForPath (path);
    if (format == nullptr)
    {
        const std::string extension = lowerCaseExtension (path);
        return { std::nullopt, "unsupported format (" + (extension.empty() ? std::string ("no extension") : extension) + ")" };
    }

    AudioDecodeResult result = detail::decodeWith (*format, path, std::nullopt);
    if (! result.audio.has_value() && format->kind == ImportAudioFormatKind::Aiff)
    {
        // The reader takes uncompressed AIFF-C and the 'sowt' / 'fl32' variants; name any other compression.
        const std::string compression = detail::aiffcCompressionType (path);
        if (! compression.empty() && compression != "NONE" && compression != "none" && compression != "twos"
            && compression != "sowt" && compression != "fl32" && compression != "FL32")
            result.reason = "AIFF compression '" + compression + "' is not supported";
    }
    return result;
}

// Reopen: the first reader, in the list's order, whose decode has the recorded shape.
[[nodiscard]] inline AudioDecodeResult decodeStoredAudio (const std::filesystem::path& path,
                                                          const ExpectedAudioShape& expected)
{
    for (const ImportAudioFormat& format : kImportAudioFormats)
    {
        AudioDecodeResult result = detail::decodeWith (format, path, expected);
        if (result.audio.has_value())
            return result;
    }
    return { std::nullopt, "no supported reader decodes it with its recorded length, rate and channels" };
}

} // namespace yesdaw::io
