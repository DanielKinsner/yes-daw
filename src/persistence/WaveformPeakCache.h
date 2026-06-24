// YES DAW - derived waveform peak cache helpers (H2).
//
// Peak files are keyed by Asset content hash under peaks/. They are derived from decoded Asset samples
// on the control/background side and are safe to delete/regenerate; Project truth never depends on them.

#pragma once

#include "engine/Project.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace yesdaw::persistence {

inline constexpr std::uint32_t kWaveformPeakTier0Frames = 256;
inline constexpr std::uint32_t kWaveformPeakFoldFactor = 16;
inline constexpr std::uint32_t kMaxWaveformPeakTiers = 16;

enum class WaveformPeakCacheStatus : std::uint8_t
{
    Ok = 0,
    InvalidInput,
    FilesystemError,
    FormatInvalid
};

struct WaveformPeak
{
    float min = 0.0f;
    float max = 0.0f;
    float rms = 0.0f;

    friend bool operator== (const WaveformPeak&, const WaveformPeak&) noexcept = default;
};

struct WaveformPeakTier
{
    std::uint32_t framesPerPeak = 0;
    std::vector<WaveformPeak> peaks; // bucket-major, then channel-major within each bucket

    friend bool operator== (const WaveformPeakTier&, const WaveformPeakTier&) noexcept = default;
};

struct WaveformPeakCache
{
    engine::AssetContentHash      contentHash;
    std::uint64_t                 sourceFrames = 0;
    std::uint16_t                 channels = 0;
    std::vector<WaveformPeakTier> tiers;

    friend bool operator== (const WaveformPeakCache&, const WaveformPeakCache&) noexcept = default;
};

struct WaveformPeakCacheResult
{
    WaveformPeakCacheStatus status = WaveformPeakCacheStatus::Ok;
    WaveformPeakCache       cache;
    std::string             message;

    [[nodiscard]] bool ok() const noexcept { return status == WaveformPeakCacheStatus::Ok; }
};

namespace detail {

inline WaveformPeakCacheResult peakOk (WaveformPeakCache cache = {})
{
    return WaveformPeakCacheResult { WaveformPeakCacheStatus::Ok, std::move (cache), {} };
}

inline WaveformPeakCacheResult peakError (WaveformPeakCacheStatus status, std::string message)
{
    return WaveformPeakCacheResult { status, {}, std::move (message) };
}

inline bool fitsSizeT (std::uint64_t value) noexcept
{
    return value <= static_cast<std::uint64_t> (std::numeric_limits<std::size_t>::max());
}

inline std::uint64_t ceilDiv (std::uint64_t value, std::uint64_t divisor) noexcept
{
    return value == 0 ? 0 : ((value - 1u) / divisor) + 1u;
}

inline std::uint64_t bucketFrameCount (std::uint64_t sourceFrames,
                                       std::uint64_t framesPerPeak,
                                       std::uint64_t peakIndex) noexcept
{
    const std::uint64_t start = peakIndex * framesPerPeak;
    if (start >= sourceFrames)
        return 0;

    const std::uint64_t remaining = sourceFrames - start;
    return std::min (remaining, framesPerPeak);
}

inline std::string peakHexBytes (std::span<const std::uint8_t> bytes)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.resize (bytes.size() * 2u);

    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        out[i * 2u] = digits[(bytes[i] >> 4u) & 0x0Fu];
        out[i * 2u + 1u] = digits[bytes[i] & 0x0Fu];
    }

    return out;
}

inline bool checkedPeakEntryCount (std::uint64_t peakCount,
                                   std::uint16_t channels,
                                   std::size_t& out) noexcept
{
    if (channels == 0)
        return false;
    if (peakCount > static_cast<std::uint64_t> (std::numeric_limits<std::size_t>::max()) / channels)
        return false;

    out = static_cast<std::size_t> (peakCount) * static_cast<std::size_t> (channels);
    return true;
}

inline void appendU16 (std::vector<std::uint8_t>& out, std::uint16_t value)
{
    for (std::uint32_t shift = 0; shift < 16u; shift += 8u)
        out.push_back (static_cast<std::uint8_t> ((value >> shift) & 0xFFu));
}

inline void appendU32 (std::vector<std::uint8_t>& out, std::uint32_t value)
{
    for (std::uint32_t shift = 0; shift < 32u; shift += 8u)
        out.push_back (static_cast<std::uint8_t> ((value >> shift) & 0xFFu));
}

inline void appendU64 (std::vector<std::uint8_t>& out, std::uint64_t value)
{
    for (std::uint32_t shift = 0; shift < 64u; shift += 8u)
        out.push_back (static_cast<std::uint8_t> ((value >> shift) & 0xFFu));
}

inline void appendFloat (std::vector<std::uint8_t>& out, float value)
{
    appendU32 (out, std::bit_cast<std::uint32_t> (value));
}

class ByteReader final
{
public:
    explicit ByteReader (std::span<const std::uint8_t> bytes) noexcept : bytes_ (bytes) {}

    [[nodiscard]] std::size_t remaining() const noexcept { return bytes_.size() - offset_; }

    [[nodiscard]] bool readBytes (std::span<std::uint8_t> out) noexcept
    {
        if (out.size() > remaining())
            return false;

        std::copy_n (bytes_.data() + offset_, out.size(), out.data());
        offset_ += out.size();
        return true;
    }

    [[nodiscard]] bool readU16 (std::uint16_t& out) noexcept
    {
        if (remaining() < 2u)
            return false;

        out = static_cast<std::uint16_t> (bytes_[offset_])
            | static_cast<std::uint16_t> (static_cast<std::uint16_t> (bytes_[offset_ + 1u]) << 8u);
        offset_ += 2u;
        return true;
    }

    [[nodiscard]] bool readU32 (std::uint32_t& out) noexcept
    {
        if (remaining() < 4u)
            return false;

        out = 0;
        for (std::uint32_t i = 0; i < 4u; ++i)
            out |= static_cast<std::uint32_t> (bytes_[offset_ + i]) << (i * 8u);

        offset_ += 4u;
        return true;
    }

    [[nodiscard]] bool readU64 (std::uint64_t& out) noexcept
    {
        if (remaining() < 8u)
            return false;

        out = 0;
        for (std::uint32_t i = 0; i < 8u; ++i)
            out |= static_cast<std::uint64_t> (bytes_[offset_ + i]) << (i * 8u);

        offset_ += 8u;
        return true;
    }

    [[nodiscard]] bool readFloat (float& out) noexcept
    {
        std::uint32_t bits = 0;
        if (! readU32 (bits))
            return false;

        out = std::bit_cast<float> (bits);
        return true;
    }

private:
    std::span<const std::uint8_t> bytes_;
    std::size_t                   offset_ = 0;
};

inline std::string peakUtf8Path (const std::filesystem::path& path)
{
    const auto utf8 = path.generic_u8string();
    return std::string (utf8.begin(), utf8.end());
}

inline bool isValidPeak (const WaveformPeak& peak) noexcept
{
    return std::isfinite (peak.min) && std::isfinite (peak.max) && std::isfinite (peak.rms)
        && peak.min <= peak.max && peak.rms >= 0.0f;
}

} // namespace detail

inline std::string waveformPeakCacheRelativePathForHash (const engine::AssetContentHash& hash)
{
    return "peaks/" + detail::peakHexBytes (hash.bytes) + ".ypeaks";
}

inline std::filesystem::path waveformPeakCachePathForHash (const std::filesystem::path& bundlePath,
                                                           const engine::AssetContentHash& hash)
{
    return bundlePath / waveformPeakCacheRelativePathForHash (hash);
}

[[nodiscard]] inline WaveformPeakCacheResult buildWaveformPeakCache (
    const engine::Asset& asset,
    std::span<const float> channelMajorSamples,
    std::uint32_t tier0Frames = kWaveformPeakTier0Frames)
{
    if (! asset.isValid() || tier0Frames == 0)
        return detail::peakError (WaveformPeakCacheStatus::InvalidInput, "Asset metadata cannot build a waveform peak cache");

    if (! detail::fitsSizeT (asset.frames))
        return detail::peakError (WaveformPeakCacheStatus::InvalidInput, "Asset frame count exceeds this build");

    if (asset.frames > static_cast<std::uint64_t> (std::numeric_limits<std::size_t>::max()) / asset.channels)
        return detail::peakError (WaveformPeakCacheStatus::InvalidInput, "decoded sample span would overflow");

    const std::size_t frames = static_cast<std::size_t> (asset.frames);
    const std::size_t expectedSamples = frames * static_cast<std::size_t> (asset.channels);
    if (channelMajorSamples.size() != expectedSamples)
        return detail::peakError (WaveformPeakCacheStatus::InvalidInput, "decoded sample span does not match Asset metadata");

    WaveformPeakCache cache;
    cache.contentHash = asset.contentHash;
    cache.sourceFrames = asset.frames;
    cache.channels = asset.channels;

    WaveformPeakTier tier0;
    tier0.framesPerPeak = tier0Frames;

    const std::uint64_t peakCount = detail::ceilDiv (asset.frames, tier0Frames);
    std::size_t peakEntries = 0;
    if (! detail::checkedPeakEntryCount (peakCount, asset.channels, peakEntries))
        return detail::peakError (WaveformPeakCacheStatus::InvalidInput, "waveform peak tier is too large");

    tier0.peaks.reserve (peakEntries);
    for (std::uint64_t peakIndex = 0; peakIndex < peakCount; ++peakIndex)
    {
        const std::uint64_t frameStart64 = peakIndex * tier0Frames;
        const std::uint64_t frameCount64 = detail::bucketFrameCount (asset.frames, tier0Frames, peakIndex);
        const std::size_t frameStart = static_cast<std::size_t> (frameStart64);
        const std::size_t frameCount = static_cast<std::size_t> (frameCount64);

        for (std::uint16_t channel = 0; channel < asset.channels; ++channel)
        {
            const std::size_t channelOffset = static_cast<std::size_t> (channel) * frames;
            const float first = channelMajorSamples[channelOffset + frameStart];
            if (! std::isfinite (first))
                return detail::peakError (WaveformPeakCacheStatus::InvalidInput, "decoded samples must be finite");

            WaveformPeak peak { first, first, 0.0f };
            double sumSq = 0.0;

            for (std::size_t i = 0; i < frameCount; ++i)
            {
                const float sample = channelMajorSamples[channelOffset + frameStart + i];
                if (! std::isfinite (sample))
                    return detail::peakError (WaveformPeakCacheStatus::InvalidInput, "decoded samples must be finite");

                peak.min = std::min (peak.min, sample);
                peak.max = std::max (peak.max, sample);
                sumSq += static_cast<double> (sample) * static_cast<double> (sample);
            }

            peak.rms = static_cast<float> (std::sqrt (sumSq / static_cast<double> (frameCount)));
            tier0.peaks.push_back (peak);
        }
    }

    cache.tiers.push_back (std::move (tier0));

    while (cache.tiers.size() < kMaxWaveformPeakTiers)
    {
        const WaveformPeakTier& previous = cache.tiers.back();
        const std::uint64_t previousPeakCount =
            static_cast<std::uint64_t> (previous.peaks.size()) / cache.channels;
        if (previousPeakCount <= 1)
            break;

        if (previous.framesPerPeak > std::numeric_limits<std::uint32_t>::max() / kWaveformPeakFoldFactor)
            return detail::peakError (WaveformPeakCacheStatus::InvalidInput, "waveform peak tier width overflowed");

        WaveformPeakTier folded;
        folded.framesPerPeak = previous.framesPerPeak * kWaveformPeakFoldFactor;

        const std::uint64_t foldedPeakCount = detail::ceilDiv (cache.sourceFrames, folded.framesPerPeak);
        if (! detail::checkedPeakEntryCount (foldedPeakCount, cache.channels, peakEntries))
            return detail::peakError (WaveformPeakCacheStatus::InvalidInput, "folded waveform peak tier is too large");
        folded.peaks.reserve (peakEntries);

        for (std::uint64_t foldedPeakIndex = 0; foldedPeakIndex < foldedPeakCount; ++foldedPeakIndex)
        {
            const std::uint64_t previousStart = foldedPeakIndex * kWaveformPeakFoldFactor;
            const std::uint64_t previousEnd =
                std::min (previousStart + kWaveformPeakFoldFactor, previousPeakCount);

            for (std::uint16_t channel = 0; channel < cache.channels; ++channel)
            {
                WaveformPeak peak { std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), 0.0f };
                double sumSq = 0.0;
                std::uint64_t totalFrames = 0;

                for (std::uint64_t previousPeakIndex = previousStart; previousPeakIndex < previousEnd; ++previousPeakIndex)
                {
                    const WaveformPeak& source =
                        previous.peaks[static_cast<std::size_t> (previousPeakIndex) * cache.channels + channel];
                    const std::uint64_t framesInPeak =
                        detail::bucketFrameCount (cache.sourceFrames, previous.framesPerPeak, previousPeakIndex);

                    peak.min = std::min (peak.min, source.min);
                    peak.max = std::max (peak.max, source.max);
                    sumSq += static_cast<double> (source.rms) * static_cast<double> (source.rms)
                           * static_cast<double> (framesInPeak);
                    totalFrames += framesInPeak;
                }

                peak.rms = static_cast<float> (std::sqrt (sumSq / static_cast<double> (totalFrames)));
                folded.peaks.push_back (peak);
            }
        }

        cache.tiers.push_back (std::move (folded));
    }

    return detail::peakOk (std::move (cache));
}

[[nodiscard]] inline WaveformPeakCacheResult writeWaveformPeakCache (const std::filesystem::path& bundlePath,
                                                                     const WaveformPeakCache& cache)
{
    if (cache.sourceFrames == 0 || cache.channels == 0 || cache.tiers.empty())
        return detail::peakError (WaveformPeakCacheStatus::InvalidInput, "waveform peak cache is empty");

    std::vector<std::uint8_t> bytes;
    bytes.reserve (64u);
    bytes.insert (bytes.end(), { 'Y', 'S', 'P', 'K' });
    detail::appendU16 (bytes, 1u);
    detail::appendU16 (bytes, cache.channels);
    detail::appendU64 (bytes, cache.sourceFrames);
    bytes.insert (bytes.end(), cache.contentHash.bytes.begin(), cache.contentHash.bytes.end());
    detail::appendU32 (bytes, static_cast<std::uint32_t> (cache.tiers.size()));

    for (const WaveformPeakTier& tier : cache.tiers)
    {
        const std::uint64_t peakCount = static_cast<std::uint64_t> (tier.peaks.size()) / cache.channels;
        if (tier.framesPerPeak == 0 || peakCount == 0 || tier.peaks.size() != static_cast<std::size_t> (peakCount) * cache.channels)
            return detail::peakError (WaveformPeakCacheStatus::InvalidInput, "waveform peak tier has inconsistent shape");

        detail::appendU32 (bytes, tier.framesPerPeak);
        detail::appendU64 (bytes, peakCount);
        for (const WaveformPeak& peak : tier.peaks)
        {
            if (! detail::isValidPeak (peak))
                return detail::peakError (WaveformPeakCacheStatus::InvalidInput, "waveform peak tier contains invalid values");

            detail::appendFloat (bytes, peak.min);
            detail::appendFloat (bytes, peak.max);
            detail::appendFloat (bytes, peak.rms);
        }
    }

    const std::filesystem::path finalPath = waveformPeakCachePathForHash (bundlePath, cache.contentHash);
    const std::filesystem::path tempPath = finalPath.parent_path() / ("." + finalPath.filename().string() + ".tmp");

    std::error_code ec;
    std::filesystem::create_directories (finalPath.parent_path(), ec);
    if (ec)
        return detail::peakError (WaveformPeakCacheStatus::FilesystemError, ec.message());

    std::filesystem::remove (tempPath, ec);
    if (ec)
        return detail::peakError (WaveformPeakCacheStatus::FilesystemError, ec.message());

    {
        std::ofstream output (tempPath, std::ios::binary | std::ios::trunc);
        if (! output)
            return detail::peakError (WaveformPeakCacheStatus::FilesystemError, "failed to open peak cache temp file: " + detail::peakUtf8Path (tempPath));

        output.write (reinterpret_cast<const char*> (bytes.data()), static_cast<std::streamsize> (bytes.size()));
        output.flush();
        output.close();
        if (! output)
            return detail::peakError (WaveformPeakCacheStatus::FilesystemError, "failed to write peak cache temp file: " + detail::peakUtf8Path (tempPath));
    }

    std::filesystem::remove (finalPath, ec);
    if (ec)
        return detail::peakError (WaveformPeakCacheStatus::FilesystemError, ec.message());

    std::filesystem::rename (tempPath, finalPath, ec);
    if (ec)
        return detail::peakError (WaveformPeakCacheStatus::FilesystemError, ec.message());

    return detail::peakOk();
}

[[nodiscard]] inline WaveformPeakCacheResult readWaveformPeakCache (const std::filesystem::path& bundlePath,
                                                                    const engine::AssetContentHash& hash)
{
    const std::filesystem::path path = waveformPeakCachePathForHash (bundlePath, hash);

    std::error_code ec;
    const std::uintmax_t fileSize = std::filesystem::file_size (path, ec);
    if (ec)
        return detail::peakError (WaveformPeakCacheStatus::FilesystemError, ec.message());
    if (fileSize > static_cast<std::uintmax_t> (std::numeric_limits<std::size_t>::max()))
        return detail::peakError (WaveformPeakCacheStatus::FormatInvalid, "peak cache file is too large");

    std::vector<std::uint8_t> bytes (static_cast<std::size_t> (fileSize));
    {
        std::ifstream input (path, std::ios::binary);
        if (! input)
            return detail::peakError (WaveformPeakCacheStatus::FilesystemError, "failed to open peak cache file: " + detail::peakUtf8Path (path));

        if (! bytes.empty())
            input.read (reinterpret_cast<char*> (bytes.data()), static_cast<std::streamsize> (bytes.size()));
        if (! input)
            return detail::peakError (WaveformPeakCacheStatus::FilesystemError, "failed to read peak cache file: " + detail::peakUtf8Path (path));
    }

    detail::ByteReader reader { bytes };
    std::array<std::uint8_t, 4> magic {};
    if (! reader.readBytes (magic) || magic != std::array<std::uint8_t, 4> { 'Y', 'S', 'P', 'K' })
        return detail::peakError (WaveformPeakCacheStatus::FormatInvalid, "peak cache header magic is invalid");

    std::uint16_t version = 0;
    std::uint16_t channels = 0;
    std::uint64_t sourceFrames = 0;
    engine::AssetContentHash storedHash;
    std::uint32_t tierCount = 0;

    if (! reader.readU16 (version) || ! reader.readU16 (channels) || ! reader.readU64 (sourceFrames)
        || ! reader.readBytes (storedHash.bytes) || ! reader.readU32 (tierCount))
        return detail::peakError (WaveformPeakCacheStatus::FormatInvalid, "peak cache header is truncated");

    if (version != 1u || channels == 0 || sourceFrames == 0 || tierCount == 0 || tierCount > kMaxWaveformPeakTiers)
        return detail::peakError (WaveformPeakCacheStatus::FormatInvalid, "peak cache header values are invalid");
    if (! (storedHash == hash))
        return detail::peakError (WaveformPeakCacheStatus::FormatInvalid, "peak cache content hash does not match its key");

    WaveformPeakCache cache;
    cache.contentHash = storedHash;
    cache.sourceFrames = sourceFrames;
    cache.channels = channels;
    cache.tiers.reserve (tierCount);

    std::uint32_t previousFramesPerPeak = 0;
    for (std::uint32_t tierIndex = 0; tierIndex < tierCount; ++tierIndex)
    {
        WaveformPeakTier tier;
        std::uint64_t peakCount = 0;
        if (! reader.readU32 (tier.framesPerPeak) || ! reader.readU64 (peakCount))
            return detail::peakError (WaveformPeakCacheStatus::FormatInvalid, "peak cache tier header is truncated");

        if (tier.framesPerPeak == 0 || peakCount != detail::ceilDiv (sourceFrames, tier.framesPerPeak))
            return detail::peakError (WaveformPeakCacheStatus::FormatInvalid, "peak cache tier shape is invalid");
        if (tierIndex > 0)
        {
            if (previousFramesPerPeak > std::numeric_limits<std::uint32_t>::max() / kWaveformPeakFoldFactor
                || tier.framesPerPeak != previousFramesPerPeak * kWaveformPeakFoldFactor)
                return detail::peakError (WaveformPeakCacheStatus::FormatInvalid, "peak cache folded tier width is invalid");
        }

        std::size_t entryCount = 0;
        if (! detail::checkedPeakEntryCount (peakCount, channels, entryCount))
            return detail::peakError (WaveformPeakCacheStatus::FormatInvalid, "peak cache tier is too large");
        if (entryCount > reader.remaining() / (sizeof (std::uint32_t) * 3u))
            return detail::peakError (WaveformPeakCacheStatus::FormatInvalid, "peak cache tier payload length is invalid");

        tier.peaks.reserve (entryCount);
        for (std::size_t i = 0; i < entryCount; ++i)
        {
            WaveformPeak peak;
            if (! reader.readFloat (peak.min) || ! reader.readFloat (peak.max) || ! reader.readFloat (peak.rms))
                return detail::peakError (WaveformPeakCacheStatus::FormatInvalid, "peak cache tier payload is truncated");
            if (! detail::isValidPeak (peak))
                return detail::peakError (WaveformPeakCacheStatus::FormatInvalid, "peak cache tier payload contains invalid values");

            tier.peaks.push_back (peak);
        }

        cache.tiers.push_back (std::move (tier));
        previousFramesPerPeak = cache.tiers.back().framesPerPeak;
    }

    if (reader.remaining() != 0)
        return detail::peakError (WaveformPeakCacheStatus::FormatInvalid, "peak cache file has trailing bytes");

    return detail::peakOk (std::move (cache));
}

} // namespace yesdaw::persistence
