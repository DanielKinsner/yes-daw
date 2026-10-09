// ADR-0069: a real, hydrated Windows cloud placeholder with hard links disabled takes the copy fallback.
// Explicit local gate only: YesDawPersistenceCheck.exe "[cloud-placeholder]" --success
// This owns a fresh sync root under USERPROFILE; it never uses or changes a user's cloud-provider folders.
// The host's AppData/Local/Temp rejects CfRegisterSyncRoot, so the gate requires an explicit profile directory.

#define YESDAW_PERSISTENCE_TEST_HOOKS 1
#include "persistence/AutosaveRecovery.h"

#include <catch2/catch_test_macros.hpp>

#if defined(_WIN32)
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

// Keep Windows' legacy near/far macros out of the engine and standard headers.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cfapi.h>

namespace {

class CloudApi
{
public:
    CloudApi() = default;
    CloudApi (const CloudApi&) = delete;
    CloudApi& operator= (const CloudApi&) = delete;

    ~CloudApi()
    {
        if (module != nullptr && ! FreeLibrary (module))
            std::fprintf (stderr, "Cloud fixture FreeLibrary failed: %lu\n", GetLastError());
    }

    void load()
    {
        module = LoadLibraryW (L"cldapi.dll");
        const DWORD error = GetLastError();
        CAPTURE (error);
        REQUIRE (module != nullptr);
        getPlatformInfo = resolve<decltype (&CfGetPlatformInfo)> ("CfGetPlatformInfo");
        registerRoot = resolve<decltype (&CfRegisterSyncRoot)> ("CfRegisterSyncRoot");
        unregisterRoot = resolve<decltype (&CfUnregisterSyncRoot)> ("CfUnregisterSyncRoot");
        convert = resolve<decltype (&CfConvertToPlaceholder)> ("CfConvertToPlaceholder");
        placeholderState = resolve<decltype (&CfGetPlaceholderStateFromAttributeTag)> (
            "CfGetPlaceholderStateFromAttributeTag");
        placeholderInfo = resolve<decltype (&CfGetPlaceholderInfo)> ("CfGetPlaceholderInfo");
    }

    void close()
    {
        const BOOL closed = FreeLibrary (module);
        if (closed)
            module = nullptr;
        const DWORD error = GetLastError();
        CAPTURE (error);
        REQUIRE (closed != FALSE);
    }

    decltype (&CfGetPlatformInfo) getPlatformInfo = nullptr;
    decltype (&CfRegisterSyncRoot) registerRoot = nullptr;
    decltype (&CfUnregisterSyncRoot) unregisterRoot = nullptr;
    decltype (&CfConvertToPlaceholder) convert = nullptr;
    decltype (&CfGetPlaceholderStateFromAttributeTag) placeholderState = nullptr;
    decltype (&CfGetPlaceholderInfo) placeholderInfo = nullptr;

private:
    template <typename Function>
    Function resolve (const char* name)
    {
        const auto address = GetProcAddress (module, name);
        const DWORD error = GetLastError();
        CAPTURE (name, error);
        REQUIRE (address != nullptr);
        // Preserve the exact SDK signature without MSVC's unsafe function-pointer cast warning.
        return std::bit_cast<Function> (address);
    }

    HMODULE module = nullptr;
};

class FileHandle
{
public:
    FileHandle (const std::filesystem::path& path, DWORD access, DWORD disposition,
                DWORD flagsAndAttributes = FILE_ATTRIBUTE_NORMAL)
        : handle (CreateFileW (path.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, disposition, flagsAndAttributes, nullptr))
    {
        const DWORD error = GetLastError();
        CAPTURE (path, error);
        REQUIRE (handle != INVALID_HANDLE_VALUE);
    }
    FileHandle (const FileHandle&) = delete;
    FileHandle& operator= (const FileHandle&) = delete;
    ~FileHandle()
    {
        if (handle != INVALID_HANDLE_VALUE && ! CloseHandle (handle))
            std::fprintf (stderr, "Cloud fixture CloseHandle failed: %lu\n", GetLastError());
    }

    void close()
    {
        const BOOL closed = CloseHandle (handle);
        if (closed)
            handle = INVALID_HANDLE_VALUE;
        const DWORD error = GetLastError();
        CAPTURE (error);
        REQUIRE (closed != FALSE);
    }

    HANDLE get() const noexcept { return handle; }

private:
    HANDLE handle = INVALID_HANDLE_VALUE;
};

class PlaceholderInspectionMode
{
public:
    PlaceholderInspectionMode()
    {
        const auto module = GetModuleHandleW (L"ntdll.dll");
        REQUIRE (module != nullptr);
        const auto setAddress = GetProcAddress (module, "RtlSetThreadPlaceholderCompatibilityMode");
        const auto queryAddress = GetProcAddress (module, "RtlQueryThreadPlaceholderCompatibilityMode");
        REQUIRE (setAddress != nullptr);
        REQUIRE (queryAddress != nullptr);
        setMode = std::bit_cast<SetMode> (setAddress);
        queryMode = std::bit_cast<QueryMode> (queryAddress);
    }
    PlaceholderInspectionMode (const PlaceholderInspectionMode&) = delete;
    PlaceholderInspectionMode& operator= (const PlaceholderInspectionMode&) = delete;
    ~PlaceholderInspectionMode()
    {
        if (needsRestore && setMode (previousMode) < 0)
            std::fprintf (stderr, "Cloud fixture could not restore thread placeholder mode %d\n",
                          static_cast<int> (previousMode));
    }

    void expose()
    {
        const CHAR queried = queryMode();
        INFO ("Thread placeholder mode before inspection: " << static_cast<int> (queried));
        REQUIRE (queried >= 0);
        REQUIRE (queried <= exposeMode);
        previousMode = setMode (exposeMode);
        needsRestore = previousMode >= 0;
        REQUIRE (previousMode == queried);
        REQUIRE (queryMode() == exposeMode);
    }

    void restore()
    {
        const CHAR replaced = setMode (previousMode);
        if (replaced >= 0)
            needsRestore = false;
        INFO ("Thread placeholder mode restored to: " << static_cast<int> (previousMode));
        REQUIRE (replaced == exposeMode);
        REQUIRE (queryMode() == previousMode);
    }

private:
    using SetMode = CHAR (NTAPI*) (CHAR);
    using QueryMode = CHAR (NTAPI*) ();
    static constexpr CHAR exposeMode = 2; // PHCM_EXPOSE_PLACEHOLDERS, documented by RtlSetThreadPlaceholderCompatibilityMode.
    SetMode setMode = nullptr;
    QueryMode queryMode = nullptr;
    CHAR previousMode = 0;
    bool needsRestore = false;
};

class OwnedSyncRoot
{
public:
    explicit OwnedSyncRoot (CloudApi& apiIn) : api (apiIn) {}
    OwnedSyncRoot (const OwnedSyncRoot&) = delete;
    OwnedSyncRoot& operator= (const OwnedSyncRoot&) = delete;

    ~OwnedSyncRoot()
    {
        // This also runs on a failing REQUIRE, after all later file/database handles unwind.
        const auto result = cleanup();
        if (FAILED (result.unregistered) || result.removalError)
            std::fprintf (stderr, "Cloud fixture cleanup failed: HRESULT=0x%08lx, filesystem=%d\n",
                          static_cast<unsigned long> (result.unregistered), result.removalError.value());
    }

    void create()
    {
        const DWORD required = GetEnvironmentVariableW (L"USERPROFILE", nullptr, 0);
        const DWORD sizeError = GetLastError();
        CAPTURE (required, sizeError);
        REQUIRE (required > 1);
        std::wstring profile (required, L'\0');
        const DWORD written = GetEnvironmentVariableW (L"USERPROFILE", profile.data(), required);
        const DWORD readError = GetLastError();
        CAPTURE (written, readError);
        REQUIRE (written > 0);
        REQUIRE (written < required);
        profile.resize (written);
        const std::filesystem::path profilePath (profile);
        REQUIRE (profilePath.is_absolute());
        rootParent = std::filesystem::canonical (profilePath);
        REQUIRE (std::filesystem::is_directory (rootParent));
        const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto prefix = "yesdaw-cloud-placeholder-" + std::to_string (GetCurrentProcessId())
                          + "-" + std::to_string (ticks) + "-";
        for (unsigned attempt = 0; attempt < 100; ++attempt)
        {
            root = rootParent / (prefix + std::to_string (attempt));
            std::error_code error;
            ownsDirectory = std::filesystem::create_directory (root, error);
            CAPTURE (root, error);
            REQUIRE_FALSE (error);
            if (ownsDirectory)
                break;
        }
        REQUIRE (ownsDirectory); // Never reuse an existing directory, even after a name collision.

        CF_SYNC_REGISTRATION registration {};
        registration.StructSize = sizeof (registration);
        registration.ProviderName = L"YES DAW placeholder verification";
        registration.ProviderVersion = L"1";
        CF_SYNC_POLICIES policies {};
        policies.StructSize = sizeof (policies);
        policies.Hydration.Primary = CF_HYDRATION_POLICY_ALWAYS_FULL;
        policies.Population.Primary = CF_POPULATION_POLICY_ALWAYS_FULL;
        policies.HardLink = CF_HARDLINK_POLICY_NONE;
        const HRESULT result = api.registerRoot (root.c_str(), &registration, &policies,
            CF_REGISTER_FLAG_DISABLE_ON_DEMAND_POPULATION_ON_ROOT);
        registered = SUCCEEDED (result);
        CAPTURE (root, result);
        REQUIRE (result == S_OK);
    }

    const std::filesystem::path& path() const noexcept { return root; }

    struct CleanupResult
    {
        HRESULT unregistered = S_OK;
        std::error_code removalError;
        std::uintmax_t removed = 0;
    };

    CleanupResult cleanup() noexcept
    {
        CleanupResult result;
        if (registered)
        {
            result.unregistered = api.unregisterRoot (root.c_str());
            if (FAILED (result.unregistered))
                return result; // Keep the owned path available for an unregister retry.
            registered = false;
        }
        try
        {
            if (ownsDirectory)
            {
                // Resolve again before recursive deletion; never follow a replaced root elsewhere.
                const auto resolved = std::filesystem::weakly_canonical (root, result.removalError);
                if (result.removalError)
                    return result;
                if (! root.is_absolute() || resolved != root || resolved.parent_path() != rootParent
                    || ! resolved.filename().wstring().starts_with (L"yesdaw-cloud-placeholder-"))
                {
                    result.removalError = std::make_error_code (std::errc::invalid_argument);
                    return result;
                }
                result.removed = std::filesystem::remove_all (root, result.removalError);
                if (! result.removalError)
                    ownsDirectory = false;
            }
        }
        catch (...)
        {
            // A destructor must still report cleanup failure while Catch unwinds a failed REQUIRE.
            result.removalError = std::make_error_code (std::errc::io_error);
        }
        return result;
    }

private:
    CloudApi& api;
    std::filesystem::path rootParent;
    std::filesystem::path root;
    bool ownsDirectory = false;
    bool registered = false;
};

void requireHydratedPlaceholder (CloudApi& api, const std::filesystem::path& path,
                                 const yesdaw::engine::AssetContentHash& identity)
{
    // Windows may disguise placeholder attributes for compatibility. Expose them only for this inspection;
    // the guard restores the prior thread mode on success and on a failing REQUIRE, before production I/O.
    PlaceholderInspectionMode inspectionMode;
    inspectionMode.expose();
    // Inspect the reparse point itself, without the normal reparse processing of a data open.
    FileHandle file (path, FILE_READ_ATTRIBUTES, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT);
    CAPTURE (path);
    // Query CfAPI first: its result distinguishes failed conversion from attribute/tag reporting.
    alignas (CF_PLACEHOLDER_STANDARD_INFO)
        std::array<std::byte, sizeof (CF_PLACEHOLDER_STANDARD_INFO) + yesdaw::engine::AssetContentHash::kNumBytes> buffer {};
    DWORD returned = 0;
    const HRESULT result = api.placeholderInfo (file.get(), CF_PLACEHOLDER_INFO_STANDARD, buffer.data(),
                                                static_cast<DWORD> (buffer.size()), &returned);
    CAPTURE (result, returned);
    REQUIRE (result == S_OK);
    REQUIRE (returned >= offsetof (CF_PLACEHOLDER_STANDARD_INFO, FileIdentity) + identity.bytes.size());
    REQUIRE (returned <= buffer.size());
    const auto* info = reinterpret_cast<const CF_PLACEHOLDER_STANDARD_INFO*> (buffer.data());
    REQUIRE (info->FileIdentityLength == identity.bytes.size());
    REQUIRE (std::memcmp (buffer.data() + offsetof (CF_PLACEHOLDER_STANDARD_INFO, FileIdentity),
                         identity.bytes.data(), identity.bytes.size()) == 0);
    FILE_ATTRIBUTE_TAG_INFO tag {};
    const BOOL readTag = GetFileInformationByHandleEx (file.get(), FileAttributeTagInfo, &tag, sizeof (tag));
    const DWORD error = GetLastError();
    CAPTURE (error);
    REQUIRE (readTag != FALSE);
    const auto state = api.placeholderState (tag.FileAttributes, tag.ReparseTag);
    CAPTURE (tag.FileAttributes, tag.ReparseTag, state);
    REQUIRE (state != CF_PLACEHOLDER_STATE_INVALID);
    REQUIRE ((state & CF_PLACEHOLDER_STATE_PLACEHOLDER) != 0);
    REQUIRE ((state & CF_PLACEHOLDER_STATE_PARTIAL) == 0);
    REQUIRE ((state & CF_PLACEHOLDER_STATE_PARTIALLY_ON_DISK) == 0);
    file.close();
    inspectionMode.restore();
}

std::vector<std::uint8_t> readBytes (const std::filesystem::path& path)
{
    std::vector<std::uint8_t> bytes (static_cast<std::size_t> (std::filesystem::file_size (path)));
    std::ifstream stream (path, std::ios::binary);
    REQUIRE (stream.good());
    stream.read (reinterpret_cast<char*> (bytes.data()), static_cast<std::streamsize> (bytes.size()));
    REQUIRE (stream.gcount() == static_cast<std::streamsize> (bytes.size()));
    REQUIRE_FALSE (stream.bad());
    return bytes;
}

} // namespace
#endif

TEST_CASE ("ADR-0069 a real hydrated cloud placeholder with hard links disabled copies and recovers",
           "[.][persistence][autosave][cloud-placeholder]")
{
#if defined(_WIN32)
    using namespace yesdaw;
    using namespace persistence;
    CloudApi api;
    api.load();
    CF_PLATFORM_INFO platform {};
    const HRESULT platformResult = api.getPlatformInfo (&platform);
    CAPTURE (platformResult, platform.BuildNumber, platform.RevisionNumber, platform.IntegrationNumber);
    REQUIRE (platformResult == S_OK);

    OwnedSyncRoot syncRoot (api);
    syncRoot.create();
    const auto bundlePath = syncRoot.path() / "song.yesdaw";
    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());
        std::vector<std::uint8_t> bytes (8192);
        for (std::size_t i = 0; i < bytes.size(); ++i)
            bytes[i] = static_cast<std::uint8_t> (i % 251);
        engine::Project project;
        project.id = engine::EntityId::fromBigEndianParts (0, 1);
        project.sampleRate = engine::SampleRate { 48000.0 };
        engine::Asset asset;
        asset.id = engine::EntityId::fromBigEndianParts (0, 2);
        asset.contentHash = detail::sha256Bytes (bytes);
        asset.frames = 1024;
        asset.sampleRate = project.sampleRate;
        asset.channels = 2;
        project.assets.push_back (asset);
        engine::Track track;
        track.id = engine::EntityId::fromBigEndianParts (0, 3);
        track.strip.name = "Cloud placeholder audio";
        project.tracks.push_back (track);
        engine::Clip clip;
        clip.id = engine::EntityId::fromBigEndianParts (0, 4);
        clip.assetId = asset.id;
        clip.trackId = track.id;
        clip.timelineLength = 15360;
        clip.srcLen = asset.frames;
        project.clips.push_back (clip);
        REQUIRE (project.hasValidAssetClipIndirection());
        REQUIRE (db.writeProjectSnapshot (project).ok());

        const auto source = storedAssetPathForHash (bundlePath, asset.contentHash);
        {
            FileHandle file (source, GENERIC_READ | GENERIC_WRITE, CREATE_NEW);
            DWORD written = 0;
            const BOOL wrote = WriteFile (file.get(), bytes.data(), static_cast<DWORD> (bytes.size()), &written, nullptr);
            const DWORD writeError = GetLastError();
            CAPTURE (writeError, written);
            REQUIRE (wrote != FALSE);
            REQUIRE (written == bytes.size());
            const BOOL flushed = FlushFileBuffers (file.get());
            const DWORD flushError = GetLastError();
            CAPTURE (flushError);
            REQUIRE (flushed != FALSE);
            const HRESULT converted = api.convert (file.get(), asset.contentHash.bytes.data(),
                static_cast<DWORD> (asset.contentHash.bytes.size()), CF_CONVERT_FLAG_MARK_IN_SYNC, nullptr, nullptr);
            CAPTURE (converted);
            REQUIRE (converted == S_OK);
            file.close();
        }
        requireHydratedPlaceholder (api, source, asset.contentHash);

        // A real filesystem rejection, in the same root as autosave, with no injected link error.
        const auto probeLink = bundlePath / "hard-link-probe.asset";
        const BOOL linked = CreateHardLinkW (probeLink.c_str(), source.c_str(), nullptr);
        const DWORD linkError = GetLastError();
        CAPTURE (linkError);
        REQUIRE (linked == FALSE);
        REQUIRE (linkError == ERROR_CLOUD_FILE_INCOMPATIBLE_HARDLINKS);
        REQUIRE_FALSE (std::filesystem::exists (probeLink));
        REQUIRE_FALSE (autosave_detail::failLinkForTest.has_value());
        REQUIRE (autosave_detail::afterCarryForTest == nullptr);
        detail::assetHashCallsForTest = 0;
        autosave_detail::assetBytesCopiedForTest = 0;
        const auto result = writeAutosaveSnapshot (db, project);
        CAPTURE (result.message, result.bundle.message, result.linked, result.copied);
        REQUIRE (result.ok());
        REQUIRE (result.copied == 1);
        REQUIRE (result.linked == 0);
        REQUIRE (detail::assetHashCallsForTest == 1);
        REQUIRE (autosave_detail::assetBytesCopiedForTest == bytes.size());

        const auto copied = storedAssetPathForHash (autosaveSnapshotPath (bundlePath), asset.contentHash);
        REQUIRE_FALSE (std::filesystem::equivalent (source, copied));
        REQUIRE (readBytes (source) == bytes);
        REQUIRE (readBytes (copied) == bytes);
        requireHydratedPlaceholder (api, source, asset.contentHash);

        engine::Project recovered;
        std::int64_t recoveredStamp = 0;
        detail::assetHashCallsForTest = 0;
        const auto recovery = readAutosaveSnapshot (bundlePath, recovered, &recoveredStamp);
        CAPTURE (recovery.message, recovery.bundle.message);
        REQUIRE (recovery.ok());
        REQUIRE (detail::assetHashCallsForTest >= 1);
        REQUIRE (recoveredStamp == 1);
        REQUIRE (recovered.id == project.id);
        REQUIRE (recovered.sampleRate == project.sampleRate);
        REQUIRE (recovered.assets == project.assets);
        REQUIRE (recovered.tracks == project.tracks);
        REQUIRE (recovered.clips == project.clips);
        REQUIRE (recovered.hasValidAssetClipIndirection());
        requireHydratedPlaceholder (api, source, asset.contentHash);
    }
    // All file/database handles are closed before unregistering. Cleanup is a measured gate too.
    const auto cleanup = syncRoot.cleanup();
    CAPTURE (cleanup.unregistered, cleanup.removalError, cleanup.removed);
    REQUIRE (cleanup.unregistered == S_OK);
    REQUIRE_FALSE (cleanup.removalError);
    REQUIRE (cleanup.removed > 0);
    REQUIRE_FALSE (std::filesystem::exists (syncRoot.path()));
    api.close();
#else
    FAIL ("The explicit cloud-placeholder gate requires Windows CfAPI and a usable local USERPROFILE directory.");
#endif
}
