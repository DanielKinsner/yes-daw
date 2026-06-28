// YES DAW - pure canonical float32 WAV reader/writer (ADR-0021).
//
// Control-side file I/O only. The audio thread never calls this code.

#pragma once

#include "engine/Time.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace yesdaw::io {

static_assert (sizeof (float) == 4, "Canonical WAV float32 requires 32-bit float");

enum class WavStatus : std::uint8_t
{
    Ok = 0,
    FilesystemError,
    FormatInvalid,
    UnsupportedFormat,
    InvalidArgument
};

struct WavResult
{
    WavStatus  status = WavStatus::Ok;
    std::string message;

    [[nodiscard]] bool ok() const noexcept { return status == WavStatus::Ok; }
};

struct Float32Wav
{
    engine::SampleRate sampleRate;
    std::uint16_t      channels = 0;
    std::uint64_t      frames = 0;
    std::vector<float> interleavedSamples;
};

namespace detail {

inline WavResult invalidArgument (std::string message)
{
    return { WavStatus::InvalidArgument, std::move (message) };
}

inline WavResult formatInvalid (std::string message)
{
    return { WavStatus::FormatInvalid, std::move (message) };
}

inline WavResult unsupported (std::string message)
{
    return { WavStatus::UnsupportedFormat, std::move (message) };
}

inline WavResult filesystemError (std::string message)
{
    return { WavStatus::FilesystemError, std::move (message) };
}

inline void writeLe16 (std::ostream& out, std::uint16_t value)
{
    const std::array<char, 2> bytes {
        static_cast<char> (value & 0xFFu),
        static_cast<char> ((value >> 8u) & 0xFFu),
    };
    out.write (bytes.data(), static_cast<std::streamsize> (bytes.size()));
}

inline void writeLe32 (std::ostream& out, std::uint32_t value)
{
    const std::array<char, 4> bytes {
        static_cast<char> (value & 0xFFu),
        static_cast<char> ((value >> 8u) & 0xFFu),
        static_cast<char> ((value >> 16u) & 0xFFu),
        static_cast<char> ((value >> 24u) & 0xFFu),
    };
    out.write (bytes.data(), static_cast<std::streamsize> (bytes.size()));
}

inline void writeFloat32Le (std::ostream& out, float value)
{
    std::uint32_t bits = 0;
    std::memcpy (&bits, &value, sizeof (bits));
    writeLe32 (out, bits);
}

inline bool readExact (std::istream& in, char* data, std::size_t size)
{
    in.read (data, static_cast<std::streamsize> (size));
    return in.good();
}

inline bool readFourcc (std::istream& in, std::array<char, 4>& out)
{
    return readExact (in, out.data(), out.size());
}

inline bool readLe16 (std::istream& in, std::uint16_t& out)
{
    std::array<unsigned char, 2> bytes {};
    if (! readExact (in, reinterpret_cast<char*> (bytes.data()), bytes.size()))
        return false;

    out = static_cast<std::uint16_t> (bytes[0])
        | static_cast<std::uint16_t> (static_cast<std::uint16_t> (bytes[1]) << 8u);
    return true;
}

inline bool readLe32 (std::istream& in, std::uint32_t& out)
{
    std::array<unsigned char, 4> bytes {};
    if (! readExact (in, reinterpret_cast<char*> (bytes.data()), bytes.size()))
        return false;

    out = static_cast<std::uint32_t> (bytes[0])
        | (static_cast<std::uint32_t> (bytes[1]) << 8u)
        | (static_cast<std::uint32_t> (bytes[2]) << 16u)
        | (static_cast<std::uint32_t> (bytes[3]) << 24u);
    return true;
}

inline float float32FromLeBytes (const std::uint8_t* bytes)
{
    const std::uint32_t bits = static_cast<std::uint32_t> (bytes[0])
                             | (static_cast<std::uint32_t> (bytes[1]) << 8u)
                             | (static_cast<std::uint32_t> (bytes[2]) << 16u)
                             | (static_cast<std::uint32_t> (bytes[3]) << 24u);
    float value = 0.0f;
    std::memcpy (&value, &bits, sizeof (value));
    return value;
}

inline bool fourccEquals (const std::array<char, 4>& value, const char (&text)[5]) noexcept
{
    return value[0] == text[0] && value[1] == text[1] && value[2] == text[2] && value[3] == text[3];
}

inline bool sampleRateToUint32 (engine::SampleRate sampleRate, std::uint32_t& out) noexcept
{
    if (! sampleRate.isValid())
        return false;

    const double rounded = std::round (sampleRate.hz);
    if (std::fabs (sampleRate.hz - rounded) > 0.0
        || rounded <= 0.0
        || rounded > static_cast<double> (std::numeric_limits<std::uint32_t>::max()))
        return false;

    out = static_cast<std::uint32_t> (rounded);
    return true;
}

} // namespace detail

[[nodiscard]] inline WavResult writeFloat32WavFile (const std::filesystem::path& path,
                                                    engine::SampleRate sampleRate,
                                                    std::uint16_t channels,
                                                    std::uint64_t frames,
                                                    std::span<const float> interleavedSamples)
{
    if (channels == 0)
        return detail::invalidArgument ("WAV channel count must be positive");

    std::uint32_t sampleRateHz = 0;
    if (! detail::sampleRateToUint32 (sampleRate, sampleRateHz))
        return detail::invalidArgument ("WAV sample rate must be a finite positive integer");

    if (frames > std::numeric_limits<std::uint64_t>::max() / static_cast<std::uint64_t> (channels))
        return detail::invalidArgument ("WAV frame/channel count overflows");

    const std::uint64_t expectedSamples = frames * static_cast<std::uint64_t> (channels);
    if (expectedSamples > static_cast<std::uint64_t> (std::numeric_limits<std::size_t>::max())
        || interleavedSamples.size() != static_cast<std::size_t> (expectedSamples))
        return detail::invalidArgument ("WAV sample span does not match frames * channels");

    for (const float sample : interleavedSamples)
        if (! std::isfinite (sample))
            return detail::invalidArgument ("WAV sample payload must be finite");

    constexpr std::uint16_t kAudioFormatIeeeFloat = 3;
    constexpr std::uint16_t kBitsPerSample = 32;
    constexpr std::uint32_t kFmtChunkSize = 16;
    const std::uint32_t blockAlign = static_cast<std::uint32_t> (channels) * sizeof (float);
    const std::uint64_t dataBytes64 = expectedSamples * sizeof (float);
    const std::uint64_t riffSize64 = 4u + (8u + kFmtChunkSize) + (8u + dataBytes64);
    if (dataBytes64 > std::numeric_limits<std::uint32_t>::max()
        || riffSize64 > std::numeric_limits<std::uint32_t>::max())
        return detail::invalidArgument ("WAV file exceeds RIFF32 size limit");

    std::error_code ec;
    if (path.has_parent_path())
    {
        std::filesystem::create_directories (path.parent_path(), ec);
        if (ec)
            return detail::filesystemError (ec.message());
    }

    std::ofstream out (path, std::ios::binary | std::ios::trunc);
    if (! out)
        return detail::filesystemError ("failed to open WAV for writing");

    out.write ("RIFF", 4);
    detail::writeLe32 (out, static_cast<std::uint32_t> (riffSize64));
    out.write ("WAVE", 4);

    out.write ("fmt ", 4);
    detail::writeLe32 (out, kFmtChunkSize);
    detail::writeLe16 (out, kAudioFormatIeeeFloat);
    detail::writeLe16 (out, channels);
    detail::writeLe32 (out, sampleRateHz);
    detail::writeLe32 (out, sampleRateHz * blockAlign);
    detail::writeLe16 (out, static_cast<std::uint16_t> (blockAlign));
    detail::writeLe16 (out, kBitsPerSample);

    out.write ("data", 4);
    detail::writeLe32 (out, static_cast<std::uint32_t> (dataBytes64));
    for (const float sample : interleavedSamples)
        detail::writeFloat32Le (out, sample);

    out.close();
    if (! out)
        return detail::filesystemError ("failed to flush WAV file");

    return {};
}

[[nodiscard]] inline WavResult readFloat32WavFile (const std::filesystem::path& path, Float32Wav& out)
{
    out = {};

    std::ifstream in (path, std::ios::binary);
    if (! in)
        return detail::filesystemError ("failed to open WAV for reading");

    std::array<char, 4> riff {};
    std::array<char, 4> wave {};
    std::uint32_t riffSize = 0;
    if (! detail::readFourcc (in, riff) || ! detail::readLe32 (in, riffSize) || ! detail::readFourcc (in, wave))
        return detail::formatInvalid ("truncated RIFF/WAVE header");
    if (! detail::fourccEquals (riff, "RIFF") || ! detail::fourccEquals (wave, "WAVE"))
        return detail::formatInvalid ("file is not RIFF/WAVE");

    (void) riffSize;

    bool haveFmt = false;
    bool haveData = false;
    std::uint16_t audioFormat = 0;
    std::uint16_t channels = 0;
    std::uint32_t sampleRate = 0;
    std::uint32_t byteRate = 0;
    std::uint16_t blockAlign = 0;
    std::uint16_t bitsPerSample = 0;
    std::vector<std::uint8_t> dataBytes;

    while (in)
    {
        std::array<char, 4> chunkId {};
        std::uint32_t chunkSize = 0;
        if (! detail::readFourcc (in, chunkId))
            break;
        if (! detail::readLe32 (in, chunkSize))
            return detail::formatInvalid ("truncated chunk header");

        if (detail::fourccEquals (chunkId, "fmt "))
        {
            if (chunkSize < 16u)
                return detail::formatInvalid ("fmt chunk is too small");

            if (! detail::readLe16 (in, audioFormat)
                || ! detail::readLe16 (in, channels)
                || ! detail::readLe32 (in, sampleRate)
                || ! detail::readLe32 (in, byteRate)
                || ! detail::readLe16 (in, blockAlign)
                || ! detail::readLe16 (in, bitsPerSample))
                return detail::formatInvalid ("truncated fmt chunk");

            if (chunkSize > 16u)
            {
                in.ignore (static_cast<std::streamsize> (chunkSize - 16u));
                if (! in.good())
                    return detail::formatInvalid ("truncated fmt extension");
            }

            haveFmt = true;
        }
        else if (detail::fourccEquals (chunkId, "data"))
        {
            dataBytes.resize (chunkSize);
            if (chunkSize > 0 && ! detail::readExact (in, reinterpret_cast<char*> (dataBytes.data()), dataBytes.size()))
                return detail::formatInvalid ("truncated data chunk");
            haveData = true;
        }
        else
        {
            in.ignore (static_cast<std::streamsize> (chunkSize));
            if (! in.good())
                return detail::formatInvalid ("truncated ancillary chunk");
        }

        if ((chunkSize & 1u) != 0u)
        {
            in.ignore (1);
            if (! in.good())
                return detail::formatInvalid ("truncated chunk padding");
        }
    }

    if (! haveFmt || ! haveData)
        return detail::formatInvalid ("WAV is missing fmt or data chunk");

    constexpr std::uint16_t kAudioFormatIeeeFloat = 3;
    if (audioFormat != kAudioFormatIeeeFloat || bitsPerSample != 32)
        return detail::unsupported ("WAV is not canonical float32");
    if (channels == 0 || sampleRate == 0)
        return detail::formatInvalid ("WAV metadata has zero channels or sample rate");

    const std::uint32_t expectedBlockAlign = static_cast<std::uint32_t> (channels) * sizeof (float);
    if (blockAlign != expectedBlockAlign || byteRate != sampleRate * expectedBlockAlign)
        return detail::formatInvalid ("WAV byte-rate or block-align metadata is inconsistent");
    if (dataBytes.size() % blockAlign != 0u)
        return detail::formatInvalid ("WAV data chunk is not frame-aligned");

    const std::uint64_t frames = static_cast<std::uint64_t> (dataBytes.size() / blockAlign);
    const std::uint64_t sampleCount = frames * static_cast<std::uint64_t> (channels);
    if (sampleCount > static_cast<std::uint64_t> (std::numeric_limits<std::size_t>::max()))
        return detail::formatInvalid ("WAV data is too large");

    Float32Wav wav;
    wav.sampleRate = engine::SampleRate { static_cast<double> (sampleRate) };
    wav.channels = channels;
    wav.frames = frames;
    wav.interleavedSamples.resize (static_cast<std::size_t> (sampleCount));
    for (std::size_t i = 0; i < wav.interleavedSamples.size(); ++i)
    {
        wav.interleavedSamples[i] = detail::float32FromLeBytes (dataBytes.data() + i * sizeof (float));
        if (! std::isfinite (wav.interleavedSamples[i]))
            return detail::formatInvalid ("WAV sample payload contains a non-finite value");
    }

    out = std::move (wav);
    return {};
}

} // namespace yesdaw::io
