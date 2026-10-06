// YES DAW - ADR-0054 gates: one decoder for WAV, AIFF, FLAC, Ogg Vorbis and MP3.
//
// Lossless formats decode to the source PCM; Ogg (encoded here with JUCE's writer) and the committed MP3 fixtures
// (tests/fixtures/import, made with ffmpeg — the commands are in its README) decode to the source's rate and
// channels and match it after alignment within a stated floor; MP3 lengths are pinned (the same on every CI
// platform); every refusal carries its reason; reopen identifies a stored asset by its content and its row.

#include "io/AudioFileDecode.h"
#include "ui/MainComponentInternal.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::filesystem::path importFixture (const char* name)
{
    return std::filesystem::path (YESDAW_IMPORT_FIXTURE_DIR) / name;
}

std::filesystem::path scratchDirectory (const char* name)
{
    const auto path = std::filesystem::temp_directory_path()
        / ("yesdaw-import-" + std::string (name) + "-"
           + std::to_string (std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories (path);
    return path;
}

// A deterministic source: two sines at half scale (440 Hz left, 660 Hz right), 0.25 s at 48 kHz.
constexpr int kFrames = 12'000;
constexpr double kRate = 48'000.0;

juce::AudioBuffer<float> sourceSignal (int channels)
{
    juce::AudioBuffer<float> buffer (channels, kFrames);
    for (int channel = 0; channel < channels; ++channel)
        for (int frame = 0; frame < kFrames; ++frame)
            buffer.setSample (channel, frame,
                              0.5f * static_cast<float> (std::sin (2.0 * 3.141592653589793 * (channel == 0 ? 440.0 : 660.0)
                                                                   * static_cast<double> (frame) / kRate)));
    return buffer;
}

void writeWith (juce::AudioFormat& format, const std::filesystem::path& path, const juce::AudioBuffer<float>& buffer,
                int bitsPerSample, int quality = 0)
{
    const juce::File file = yesdaw::io::detail::juceFileForPath (path);
    (void) file.deleteFile();
    auto stream = std::make_unique<juce::FileOutputStream> (file);
    REQUIRE (stream->openedOk());
    std::unique_ptr<juce::AudioFormatWriter> writer (format.createWriterFor (
        stream.get(), kRate, static_cast<unsigned int> (buffer.getNumChannels()), bitsPerSample, {}, quality));
    REQUIRE (writer != nullptr);
    (void) stream.release();   // the writer owns it now
    REQUIRE (writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples()));
}

std::vector<float> interleave (const juce::AudioBuffer<float>& buffer)
{
    std::vector<float> out (static_cast<std::size_t> (buffer.getNumSamples() * buffer.getNumChannels()));
    for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            out[static_cast<std::size_t> (frame * buffer.getNumChannels() + channel)] = buffer.getSample (channel, frame);
    return out;
}

// The best alignment of a decoded channel against the source (lossy codecs add delay), and the SNR there.
double alignedSnrDb (const std::vector<float>& decoded, int decodedChannels,
                     const std::vector<float>& source, int sourceChannels, int channel, int maxLag)
{
    const auto sample = [] (const std::vector<float>& data, int channels, int frame, int ch) {
        return static_cast<double> (data[static_cast<std::size_t> (frame * channels + ch)]);
    };
    const int sourceFrames = static_cast<int> (source.size()) / sourceChannels;
    const int decodedFrames = static_cast<int> (decoded.size()) / decodedChannels;
    const int window = sourceFrames - 2'048;   // skip the edges (codec ramp-in / ramp-out)
    int bestLag = 0;
    double bestCorrelation = -1.0e300;
    for (int lag = 0; lag <= maxLag; ++lag)
    {
        double correlation = 0.0;
        for (int frame = 1'024; frame < window && frame + lag < decodedFrames; frame += 4)
            correlation += sample (source, sourceChannels, frame, channel) * sample (decoded, decodedChannels, frame + lag, channel);
        if (correlation > bestCorrelation)
        {
            bestCorrelation = correlation;
            bestLag = lag;
        }
    }
    double signal = 0.0;
    double noise = 0.0;
    for (int frame = 1'024; frame < window && frame + bestLag < decodedFrames; ++frame)
    {
        const double s = sample (source, sourceChannels, frame, channel);
        const double d = sample (decoded, decodedChannels, frame + bestLag, channel);
        signal += s * s;
        noise += (s - d) * (s - d);
    }
    return 10.0 * std::log10 (signal / std::max (noise, 1.0e-30));
}

std::vector<float> readWavFixture (const char* name, int& channels)
{
    const yesdaw::io::AudioDecodeResult decoded = yesdaw::io::decodeAudioFile (importFixture (name));
    REQUIRE (decoded.audio.has_value());
    channels = decoded.audio->channels;
    return decoded.audio->interleaved;
}

} // namespace

TEST_CASE ("ADR-0054 the supported list: one set of extensions for every surface, case-insensitive",
           "[import-formats]")
{
    using yesdaw::io::isImportableAudioPath;
    for (const char* name : { "a.wav", "a.WAVE", "a.aif", "a.AIFF", "a.aifc", "a.flac", "a.ogg", "a.oga", "a.Mp3" })
        REQUIRE (isImportableAudioPath (name));
    for (const char* name : { "a.m4a", "a.aac", "a.wma", "a.mid", "a", "a.txt", ".wav" })
        REQUIRE_FALSE (isImportableAudioPath (name));
    REQUIRE (yesdaw::io::importAudioFilePatterns() == "*.wav;*.wave;*.aif;*.aiff;*.aifc;*.flac;*.ogg;*.oga;*.mp3");
}

TEST_CASE ("ADR-0054 lossless formats decode to the source PCM", "[import-formats]")
{
    const auto directory = scratchDirectory ("lossless");
    const juce::AudioBuffer<float> stereo = sourceSignal (2);
    const std::vector<float> source = interleave (stereo);

    struct Case
    {
        const char* file;
        int bits;
        int kind;   // 0 WAV, 1 AIFF, 2 FLAC
    };
    for (const Case c : { Case { "s8.wav", 8, 0 }, Case { "s16.wav", 16, 0 }, Case { "s24.wav", 24, 0 }, Case { "s32.wav", 32, 0 },
                          Case { "s16.aiff", 16, 1 }, Case { "s24.aif", 24, 1 }, Case { "s16.flac", 16, 2 }, Case { "s24.flac", 24, 2 } })
    {
        INFO (c.file);
        const auto path = directory / c.file;
        std::unique_ptr<juce::AudioFormat> format;
        if (c.kind == 0)
            format = std::make_unique<juce::WavAudioFormat>();
        else if (c.kind == 1)
            format = std::make_unique<juce::AiffAudioFormat>();
        else
            format = std::make_unique<juce::FlacAudioFormat>();
        writeWith (*format, path, stereo, c.bits);

        const yesdaw::io::AudioDecodeResult decoded = yesdaw::io::decodeAudioFile (path);
        REQUIRE (decoded.audio.has_value());
        REQUIRE (decoded.audio->frames == static_cast<std::uint64_t> (kFrames));
        REQUIRE (decoded.audio->channels == 2u);
        REQUIRE (decoded.audio->sampleRateHz == kRate);
        // The source PCM: within one quantisation step of the float source, and the same integers in every
        // container (a 16-bit WAV, AIFF and FLAC decode to identical floats — compared below).
        const double step = 1.0 / std::ldexp (1.0, c.bits - 1);
        for (std::size_t i = 0; i < source.size(); ++i)
            REQUIRE (std::abs (static_cast<double> (decoded.audio->interleaved[i]) - static_cast<double> (source[i])) <= step);
    }
    const auto same = [&directory] (const char* a, const char* b) {
        return yesdaw::io::decodeAudioFile (directory / a).audio->interleaved == yesdaw::io::decodeAudioFile (directory / b).audio->interleaved;
    };
    REQUIRE (same ("s16.wav", "s16.aiff"));
    REQUIRE (same ("s16.wav", "s16.flac"));
    REQUIRE (same ("s24.wav", "s24.aif"));
    REQUIRE (same ("s24.wav", "s24.flac"));

    // 32-bit float WAV: the floats themselves.
    std::vector<float> floats = source;
    REQUIRE (yesdaw::io::writeFloat32WavFile (directory / "f32.wav", yesdaw::engine::SampleRate { kRate }, 2,
                                               static_cast<std::uint64_t> (kFrames), floats).ok());
    const yesdaw::io::AudioDecodeResult float32 = yesdaw::io::decodeAudioFile (directory / "f32.wav");
    REQUIRE (float32.audio.has_value());
    REQUIRE (float32.audio->interleaved == source);
}

TEST_CASE ("ADR-0054 Ogg Vorbis decodes to the source's shape and matches it after alignment", "[import-formats]")
{
    const auto directory = scratchDirectory ("ogg");
    const juce::AudioBuffer<float> stereo = sourceSignal (2);
    juce::OggVorbisAudioFormat ogg;
    const int highQuality = ogg.getQualityOptions().size() - 1;
    writeWith (ogg, directory / "tone.ogg", stereo, 16, highQuality);

    const yesdaw::io::AudioDecodeResult first = yesdaw::io::decodeAudioFile (directory / "tone.ogg");
    REQUIRE (first.audio.has_value());
    REQUIRE (first.audio->sampleRateHz == kRate);
    REQUIRE (first.audio->channels == 2u);
    REQUIRE (first.audio->frames >= static_cast<std::uint64_t> (kFrames));
    REQUIRE (first.audio->frames <= static_cast<std::uint64_t> (kFrames) + 2'048u);   // within one codec block
    const std::vector<float> source = interleave (stereo);
    for (int channel = 0; channel < 2; ++channel)
        REQUIRE (alignedSnrDb (first.audio->interleaved, 2, source, 2, channel, 2'048) > 20.0);
    // The same bytes decode the same samples.
    REQUIRE (yesdaw::io::decodeAudioFile (directory / "tone.ogg").audio->interleaved == first.audio->interleaved);
}

TEST_CASE ("ADR-0054 MP3 fixtures report pinned lengths on every platform and match the source", "[import-formats]")
{
    int sourceChannels = 0;
    const std::vector<float> source = readWavFixture ("source_48k_stereo.wav", sourceChannels);
    REQUIRE (sourceChannels == 2);

    struct Case
    {
        const char* file;
        std::uint64_t frames;   // the pinned decoder's length (JUCE 8.0.4)
    };
    // 22 MPEG frames of 1152 for the 0.5 s source (its 24 000 frames plus the encoder's delay and padding), read
    // from the header where there is one and estimated from the stream size where there is not.
    for (const Case c : { Case { "cbr128_info.mp3", 25'344 }, Case { "cbr128_noinfo.mp3", 25'344 },
                          Case { "vbr_xing.mp3", 25'344 }, Case { "cbr128_id3v1.mp3", 25'344 } })
    {
        INFO (c.file);
        const yesdaw::io::AudioDecodeResult first = yesdaw::io::decodeAudioFile (importFixture (c.file));
        REQUIRE (first.audio.has_value());
        INFO ("decoded frames " << first.audio->frames);
        REQUIRE (first.audio->format == yesdaw::io::ImportAudioFormatKind::Mp3);
        REQUIRE (first.audio->sampleRateHz == kRate);
        REQUIRE (first.audio->channels == 2u);
        REQUIRE (first.audio->frames == c.frames);
        for (int channel = 0; channel < 2; ++channel)
            REQUIRE (alignedSnrDb (first.audio->interleaved, 2, source, 2, channel, 3'000) > 15.0);
        REQUIRE (yesdaw::io::decodeAudioFile (importFixture (c.file)).audio->interleaved == first.audio->interleaved);
    }
}

TEST_CASE ("ADR-0054 every refusal carries its reason", "[import-formats]")
{
    const auto directory = scratchDirectory ("refusals");
    const auto refusal = [] (const std::filesystem::path& path) {
        const yesdaw::io::AudioDecodeResult result = yesdaw::io::decodeAudioFile (path);
        REQUIRE_FALSE (result.audio.has_value());
        return result.reason;
    };
    const auto writeBytes = [] (const std::filesystem::path& path, const std::string& bytes) {
        std::ofstream out (path, std::ios::binary);
        out << bytes;
    };

    writeBytes (directory / "song.m4a", "not audio");
    REQUIRE (refusal (directory / "song.m4a") == "unsupported format (.m4a)");
    REQUIRE (refusal (directory / "README") == "unsupported format (no extension)");
    writeBytes (directory / "junk.flac", "this is not a flac stream at all");
    REQUIRE (refusal (directory / "junk.flac") == "not a readable FLAC file");
    writeBytes (directory / "junk.mp3", std::string (4'096, '\0'));
    REQUIRE (refusal (directory / "junk.mp3") == "not a readable MP3 file");
    REQUIRE (refusal (directory / "missing.wav") == "not a readable WAV file");

    // Six channels: named, never downmixed.
    {
        juce::AudioBuffer<float> six (6, 256);
        six.clear();
        juce::WavAudioFormat wav;
        writeWith (wav, directory / "six.wav", six, 16);
        REQUIRE (refusal (directory / "six.wav") == "6 channels (mono or stereo only)");
    }
    // No frames.
    {
        juce::AudioBuffer<float> empty (1, 0);
        juce::WavAudioFormat wav;
        writeWith (wav, directory / "empty.wav", empty, 16);
        REQUIRE (refusal (directory / "empty.wav") == "no audio in the file");
    }
    // An AIFF-C whose compression the reader does not take is named by its type.
    {
        std::string bytes = "FORM";
        const auto be32 = [] (std::uint32_t v) {
            return std::string { static_cast<char> (v >> 24u), static_cast<char> (v >> 16u), static_cast<char> (v >> 8u), static_cast<char> (v) };
        };
        std::string comm;
        comm += std::string ("\x00\x01", 2);                // channels
        comm += be32 (100);                                 // frames
        comm += std::string ("\x00\x10", 2);                // bits
        comm += std::string ("\x40\x0E\xBB\x80\x00\x00\x00\x00\x00\x00", 10);   // 48000 as an 80-bit extended
        comm += "ulaw";
        comm += std::string ("\x04" "uLaw" "\x00", 6);      // the compression name (pascal string, padded)
        const std::string body = std::string ("AIFC") + "COMM" + be32 (static_cast<std::uint32_t> (comm.size())) + comm;
        bytes += be32 (static_cast<std::uint32_t> (body.size())) + body;
        writeBytes (directory / "ulaw.aifc", bytes);
        REQUIRE (refusal (directory / "ulaw.aifc") == "AIFF compression 'ulaw' is not supported");
    }
}

TEST_CASE ("ADR-0054 reopen identifies a stored asset by content and refuses one that no longer matches its row",
           "[import-formats]")
{
    // A stored asset has no extension (audio/<hash>.asset): each format is found by its content.
    const auto directory = scratchDirectory ("stored");
    const juce::AudioBuffer<float> stereo = sourceSignal (2);
    juce::FlacAudioFormat flac;
    writeWith (flac, directory / "tone.flac", stereo, 16);
    std::filesystem::copy_file (directory / "tone.flac", directory / "flac.asset");
    std::filesystem::copy_file (importFixture ("cbr128_info.mp3"), directory / "mp3.asset");

    const yesdaw::io::AudioDecodeResult flacImport = yesdaw::io::decodeAudioFile (directory / "tone.flac");
    const yesdaw::io::AudioDecodeResult flacStored = yesdaw::io::decodeStoredAudio (
        directory / "flac.asset", { flacImport.audio->frames, kRate, 2 });
    REQUIRE (flacStored.audio.has_value());
    REQUIRE (flacStored.audio->format == yesdaw::io::ImportAudioFormatKind::Flac);
    REQUIRE (flacStored.audio->interleaved == flacImport.audio->interleaved);

    const yesdaw::io::AudioDecodeResult mp3Import = yesdaw::io::decodeAudioFile (importFixture ("cbr128_info.mp3"));
    const yesdaw::io::AudioDecodeResult mp3Stored = yesdaw::io::decodeStoredAudio (
        directory / "mp3.asset", { mp3Import.audio->frames, kRate, 2 });
    REQUIRE (mp3Stored.audio.has_value());
    REQUIRE (mp3Stored.audio->format == yesdaw::io::ImportAudioFormatKind::Mp3);
    REQUIRE (mp3Stored.audio->interleaved == mp3Import.audio->interleaved);

    // The row is the authority: a different length, rate or width is refused, whatever reader opens the bytes.
    REQUIRE_FALSE (yesdaw::io::decodeStoredAudio (directory / "flac.asset", { flacImport.audio->frames + 1u, kRate, 2 }).audio.has_value());
    REQUIRE_FALSE (yesdaw::io::decodeStoredAudio (directory / "flac.asset", { flacImport.audio->frames, 44'100.0, 2 }).audio.has_value());
    REQUIRE_FALSE (yesdaw::io::decodeStoredAudio (directory / "mp3.asset", { mp3Import.audio->frames, kRate, 1 }).audio.has_value());
}

TEST_CASE ("ADR-0054 a bundle with MP3 and FLAC assets reopens: each decoded by content, matching its row",
           "[import-formats]")
{
    const auto directory = scratchDirectory ("bundle");
    const juce::AudioBuffer<float> stereo = sourceSignal (2);
    juce::FlacAudioFormat flac;
    writeWith (flac, directory / "tone.flac", stereo, 24);

    std::vector<yesdaw::ui::UiAudioImportItem> items;
    for (const std::filesystem::path& path : { importFixture ("vbr_xing.mp3"), directory / "tone.flac" })
    {
        yesdaw::ui::shell::UiAudioDecodeResult decoded = yesdaw::ui::shell::decodeProjectAudio (path);
        REQUIRE (decoded.decoded.has_value());
        items.push_back ({ path, std::move (*decoded.decoded) });
    }
    const std::filesystem::path bundle = directory / "formats.yesdaw";
    {
        yesdaw::ui::UiAppModel model;
        REQUIRE (model.createProjectBundle (bundle).ok());
        const yesdaw::ui::UiAudioDropResult dropped = model.importAudioFilesAt (std::move (items), 0, 0);
        REQUIRE (dropped.landed == 2u);
        REQUIRE (dropped.tracksCreated == 1u);   // a new project has one track; the second file made one
        REQUIRE (dropped.refusals.empty());
        REQUIRE (model.saveProjectBundle().ok());
    }

    const auto stored = yesdaw::ui::shell::decodeStoredProjectAssets (bundle);
    INFO (stored.failureReason);
    REQUIRE (stored.assets.has_value());
    REQUIRE (stored.assets->size() == 2u);
    const auto& project = stored.prepared.project();
    for (const yesdaw::ui::UiDecodedAsset& asset : *stored.assets)
    {
        const yesdaw::engine::Asset* row = project.findAsset (asset.assetId);
        REQUIRE (row != nullptr);
        REQUIRE (asset.frames == row->frames);
        REQUIRE (asset.channels == row->channels);
    }
}
