// YES DAW - ADR-0062 gates: missing audio relink. cp1: the bundle inspection (every missing or damaged Asset file,
// nothing written) and the adoption of an Asset's original bytes only.

#include "io/WavFile.h"
#include "persistence/ProjectBundle.h"
#include "ui/MainComponentInternal.h"
#include "ui/UiAppModel.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

namespace {

using MissingAssetFile = yesdaw::persistence::ProjectBundleDb::MissingAssetFile;

std::filesystem::path relinkScratch (const std::string& label)
{
    const auto path = std::filesystem::temp_directory_path()
        / ("yesdaw-relink-" + label + "-" + std::to_string (std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories (path);
    return path;
}

void writeTone (const std::filesystem::path& path, std::size_t frames, float level, std::size_t period)
{
    std::vector<float> samples (frames);
    for (std::size_t i = 0; i < frames; ++i)
        samples[i] = level * static_cast<float> ((i % period) < period / 2u ? 1.0 : -1.0);
    REQUIRE (yesdaw::io::writeFloat32WavFile (path, yesdaw::engine::SampleRate { 48'000.0 }, 1, samples.size(), samples).ok());
}

std::string bytesOf (const std::filesystem::path& path)
{
    std::ifstream in (path, std::ios::binary);
    return std::string ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char>());
}

void writeBytes (const std::filesystem::path& path, const std::string& bytes)
{
    std::ofstream out (path, std::ios::binary | std::ios::trunc);
    out << bytes;
}

// Every file in a bundle and its bytes (SQLite's transient -wal / -shm left out).
std::map<std::string, std::string> snapshotOf (const std::filesystem::path& bundle)
{
    std::map<std::string, std::string> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator (bundle))
    {
        const std::string name = entry.path().filename().string();
        if (! entry.is_regular_file() || name == "project.db-wal" || name == "project.db-shm")
            continue;
        files[std::filesystem::relative (entry.path(), bundle).generic_string()] = bytesOf (entry.path());
    }
    return files;
}

std::filesystem::path assetFileOf (const std::filesystem::path& bundle, const yesdaw::engine::AssetContentHash& hash)
{
    return bundle / std::filesystem::path (yesdaw::persistence::detail::assetRelativePathForHash (hash));
}

// A closed bundle with three imported tones (each an Asset whose original is the .wav beside the bundle).
struct ThreeTones
{
    std::filesystem::path directory;
    std::filesystem::path bundle;
    std::vector<std::filesystem::path> originals;
    std::vector<yesdaw::engine::AssetContentHash> hashes;

    explicit ThreeTones (const std::string& label) : directory (relinkScratch (label)), bundle (directory / "song.yesdaw")
    {
        yesdaw::ui::UiAppModel model;
        REQUIRE (model.createProjectBundle (bundle).ok());
        for (std::size_t i = 0; i < 3u; ++i)
        {
            const auto wav = directory / ("tone-" + std::to_string (i) + ".wav");
            writeTone (wav, 12'000 + 6'000 * i, 0.2f, 60u + 20u * i);
            yesdaw::ui::shell::UiAudioDecodeResult decoded = yesdaw::ui::shell::decodeProjectAudio (wav);
            REQUIRE (decoded.decoded.has_value());
            REQUIRE (model.importAudioFile (wav, std::move (*decoded.decoded)).ok());
            originals.push_back (wav);
        }
        REQUIRE (model.project().assets.size() == 3u);
        for (const yesdaw::engine::Asset& asset : model.project().assets)
            hashes.push_back (asset.contentHash);
    }
};

bool listed (const std::vector<MissingAssetFile>& missing, const yesdaw::engine::AssetContentHash& hash, bool damaged)
{
    for (const MissingAssetFile& file : missing)
        if (file.hash == hash && file.damaged == damaged)
            return true;
    return false;
}

} // namespace

TEST_CASE ("ADR-0062 the inspection lists every missing and damaged Asset file and writes nothing", "[relink]")
{
    ThreeTones f ("inspect");
    std::filesystem::remove (assetFileOf (f.bundle, f.hashes[0]));
    writeBytes (assetFileOf (f.bundle, f.hashes[1]), "damaged bytes");
    yesdaw::persistence::ProjectBundleDb refused;
    REQUIRE_FALSE (yesdaw::persistence::ProjectBundleDb::openExistingBundle (f.bundle, refused).ok());   // the open stops at one

    const auto before = snapshotOf (f.bundle);
    yesdaw::engine::Project project;
    std::vector<MissingAssetFile> missing;
    REQUIRE (yesdaw::persistence::ProjectBundleDb::inspectAssetFiles (f.bundle, project, missing).ok());
    REQUIRE (missing.size() == 2u);   // both, past the first
    REQUIRE (listed (missing, f.hashes[0], false));
    REQUIRE (listed (missing, f.hashes[1], true));
    REQUIRE (project.assets.size() == 3u);
    REQUIRE (project.clips.size() == 3u);
    REQUIRE (snapshotOf (f.bundle) == before);   // nothing written, nothing swept
}

TEST_CASE ("ADR-0062 a bundle on an older schema is not inspected", "[relink]")
{
    ThreeTones f ("old-schema");
    std::filesystem::remove (assetFileOf (f.bundle, f.hashes[0]));
    {
        sqlite3* db = nullptr;
        REQUIRE (sqlite3_open_v2 (yesdaw::persistence::detail::utf8Path (f.bundle / "project.db").c_str(), &db,
                                  SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK);
        const std::string sql = "PRAGMA user_version = " + std::to_string (yesdaw::persistence::kCodeSchemaVersion - 1) + ";";
        REQUIRE (sqlite3_exec (db, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK);
        sqlite3_close (db);
    }
    const auto before = snapshotOf (f.bundle);
    yesdaw::engine::Project project;
    std::vector<MissingAssetFile> missing;
    REQUIRE_FALSE (yesdaw::persistence::ProjectBundleDb::inspectAssetFiles (f.bundle, project, missing).ok());
    REQUIRE (missing.empty());
    REQUIRE (snapshotOf (f.bundle) == before);   // no migration ran
}

TEST_CASE ("ADR-0062 adoption takes only an Asset's original bytes; a damaged file goes to the trash, never over earlier "
           "evidence",
           "[relink]")
{
    ThreeTones f ("adopt");
    std::filesystem::remove (assetFileOf (f.bundle, f.hashes[0]));
    writeBytes (assetFileOf (f.bundle, f.hashes[1]), "damaged bytes");

    // Another tone is refused with its reason and writes nothing.
    const auto before = snapshotOf (f.bundle);
    const yesdaw::persistence::BundleResult wrong =
        yesdaw::persistence::ProjectBundleDb::adoptAssetFile (f.bundle, f.hashes[0], f.originals[2]);
    REQUIRE_FALSE (wrong.ok());
    REQUIRE (wrong.message == "its content differs");
    REQUIRE (snapshotOf (f.bundle) == before);

    // The originals are taken: the missing one appears, the damaged one moves to the trash.
    REQUIRE (yesdaw::persistence::ProjectBundleDb::adoptAssetFile (f.bundle, f.hashes[0], f.originals[0]).ok());
    REQUIRE (bytesOf (assetFileOf (f.bundle, f.hashes[0])) == bytesOf (f.originals[0]));
    REQUIRE (yesdaw::persistence::ProjectBundleDb::adoptAssetFile (f.bundle, f.hashes[1], f.originals[1]).ok());
    REQUIRE (bytesOf (assetFileOf (f.bundle, f.hashes[1])) == bytesOf (f.originals[1]));
    const std::string hex = yesdaw::persistence::detail::hexBytes (f.hashes[1].bytes);
    REQUIRE (bytesOf (f.bundle / ".trash" / (hex + ".asset.1")) == "damaged bytes");

    // Damaged again and relinked again: the first evidence stays.
    writeBytes (assetFileOf (f.bundle, f.hashes[1]), "damaged again");
    REQUIRE (yesdaw::persistence::ProjectBundleDb::adoptAssetFile (f.bundle, f.hashes[1], f.originals[1]).ok());
    REQUIRE (bytesOf (f.bundle / ".trash" / (hex + ".asset.1")) == "damaged bytes");
    REQUIRE (bytesOf (f.bundle / ".trash" / (hex + ".asset.2")) == "damaged again");

    // A file already holding the bytes is left alone (no new trash entry).
    REQUIRE (yesdaw::persistence::ProjectBundleDb::adoptAssetFile (f.bundle, f.hashes[1], f.originals[1]).ok());
    REQUIRE_FALSE (std::filesystem::exists (f.bundle / ".trash" / (hex + ".asset.3")));

    for (const auto& entry : std::filesystem::directory_iterator (f.bundle / "audio"))
        REQUIRE (entry.path().extension() != ".tmp");   // no temporary left
    yesdaw::persistence::ProjectBundleDb reopened;
    REQUIRE (yesdaw::persistence::ProjectBundleDb::openExistingBundle (f.bundle, reopened).ok());   // the validator agrees
}
