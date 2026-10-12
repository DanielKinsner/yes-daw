// ADR-0069: a real, hydrated Windows cloud placeholder with hard links disabled takes the copy fallback.
// Explicit local gate only: YesDawPersistenceCheck.exe "[cloud-placeholder]" --success
// This owns a fresh sync root under USERPROFILE; it never uses or changes a user's cloud-provider folders.
// The host's AppData/Local/Temp rejects CfRegisterSyncRoot, so the gate requires an explicit profile directory.
// Close-out unit 5's real-app drive (ss8 Step 8) reuses this fixture across processes through four more hidden cases:
// [cloud-drive-setup] registers an owned root and names it in the file YESDAW_CLOUD_DRIVE_STATE; the app then creates a
// project there; [cloud-drive-convert] turns that bundle's (YESDAW_CLOUD_DRIVE_BUNDLE) stored audio into hydrated
// placeholders and proves the OS rejects a hard link to each; [cloud-drive-verify] checks the user's audio is still those
// placeholders, byte-valid, and (YESDAW_CLOUD_DRIVE_EXPECT_SNAPSHOT=1) that the autosave holds real copies, never links;
// [cloud-drive-cleanup] unregisters and removes the root the state file names.

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
#include <iterator>
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

std::filesystem::path canonicalProfile()
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
    const auto canonical = std::filesystem::canonical (profilePath);
    REQUIRE (std::filesystem::is_directory (canonical));
    return canonical;
}

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
        resolveProfile();
        const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto prefix = std::string (kPrefix) + std::to_string (GetCurrentProcessId())
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

    // The app drive's root outlives this process: release() hands it on, still registered and on disk; adopt() takes a
    // released one back so its cleanup runs - only an existing direct child of USERPROFILE named like this fixture's.
    [[nodiscard]] std::filesystem::path release() noexcept
    {
        registered = false;
        ownsDirectory = false;
        return root;
    }

    void adopt (const std::filesystem::path& released)
    {
        resolveProfile();
        CAPTURE (released);
        REQUIRE (released.is_absolute());
        REQUIRE (std::filesystem::is_directory (released));
        REQUIRE (std::filesystem::canonical (released) == released);
        REQUIRE (released.parent_path() == rootParent);
        REQUIRE (released.filename().wstring().starts_with (L"yesdaw-cloud-placeholder-"));
        root = released;
        ownsDirectory = true;
        registered = true;
    }

private:
    static constexpr const char* kPrefix = "yesdaw-cloud-placeholder-";

    void resolveProfile() { rootParent = canonicalProfile(); }

public:
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

// ---- Close-out unit 5: the same fixture, held across the real app's drive (ss8 Step 8) ----

#if defined(_WIN32)
namespace {

std::filesystem::path requiredEnvironmentPath (const wchar_t* name)
{
    INFO ("environment variable " << std::filesystem::path (name).string());
    const DWORD required = GetEnvironmentVariableW (name, nullptr, 0);
    REQUIRE (required > 1);
    std::wstring value (required, L'\0');
    const DWORD written = GetEnvironmentVariableW (name, value.data(), required);
    REQUIRE (written > 0);
    REQUIRE (written < required);
    value.resize (written);
    const std::filesystem::path path (value);
    REQUIRE (path.is_absolute());
    return path;
}

bool environmentFlag (const wchar_t* name)
{
    std::array<wchar_t, 4> value {};
    const DWORD written = GetEnvironmentVariableW (name, value.data(), static_cast<DWORD> (value.size()));
    return written == 1 && value[0] == L'1';
}

// The root [cloud-drive-setup] handed on, as its state file names it.
std::filesystem::path stateRoot()
{
    const auto stateFile = requiredEnvironmentPath (L"YESDAW_CLOUD_DRIVE_STATE");
    std::ifstream state (stateFile, std::ios::binary);
    REQUIRE (state.good());
    const std::string text ((std::istreambuf_iterator<char> (state)), std::istreambuf_iterator<char>());
    std::u8string utf8;
    for (const char c : text)
        utf8.push_back (static_cast<char8_t> (c));
    return std::filesystem::path (utf8);
}

// The drive's bundle sits directly in the root this fixture registered and handed on - never a user's audio anywhere
// else (no other folder of that name, no junction out of it).
void requireDriveBundle (const std::filesystem::path& bundle)
{
    const auto root = stateRoot();
    CAPTURE (bundle, root);
    REQUIRE (std::filesystem::is_directory (bundle));
    REQUIRE (bundle.extension() == ".yesdaw");
    REQUIRE (std::filesystem::canonical (bundle) == bundle);
    REQUIRE (bundle.parent_path() == root);
    REQUIRE (root.parent_path() == canonicalProfile());
    REQUIRE (root.filename().wstring().starts_with (L"yesdaw-cloud-placeholder-"));
}

// Read-only (ADR-0062's inspection: no journal change, reconcile or sweep), and every Asset's file present and hashing
// true - the app's own open is the only thing that opens these bundles for writing.
yesdaw::engine::Project readBundleProject (const std::filesystem::path& bundle)
{
    yesdaw::engine::Project project;
    std::vector<yesdaw::persistence::ProjectBundleDb::MissingAssetFile> missing;
    const auto inspected = yesdaw::persistence::ProjectBundleDb::inspectAssetFiles (bundle, project, missing);
    CAPTURE (bundle, inspected.message, missing.size());
    REQUIRE (inspected.ok());
    REQUIRE (missing.empty());
    REQUIRE (project.hasValidAssetClipIndirection());
    return project;
}

void requireBytesHash (const std::filesystem::path& file, const yesdaw::engine::AssetContentHash& hash)
{
    CAPTURE (file);
    yesdaw::engine::AssetContentHash actual;
    const auto hashed = yesdaw::persistence::detail::hashFile (file, actual);
    CAPTURE (hashed.message);
    REQUIRE (hashed.ok());
    REQUIRE (actual == hash);
}

} // namespace
#endif

TEST_CASE ("ADR-0069 drive fixture: register an owned placeholder root for the app", "[.][cloud-drive-setup]")
{
#if defined(_WIN32)
    const auto stateFile = requiredEnvironmentPath (L"YESDAW_CLOUD_DRIVE_STATE");
    CloudApi api;
    api.load();
    OwnedSyncRoot syncRoot (api);
    syncRoot.create();
    {
        std::ofstream state (stateFile, std::ios::binary | std::ios::trunc);
        const auto text = syncRoot.path().u8string();
        state.write (reinterpret_cast<const char*> (text.data()), static_cast<std::streamsize> (text.size()));
        REQUIRE (state.good());
    }
    const auto handedOn = syncRoot.release();   // registered and on disk until [cloud-drive-cleanup]
    REQUIRE (std::filesystem::is_directory (handedOn));
#else
    FAIL ("The cloud-placeholder drive requires Windows CfAPI.");
#endif
}

TEST_CASE ("ADR-0069 drive fixture: the app's stored audio becomes hydrated placeholders that refuse hard links",
           "[.][cloud-drive-convert]")
{
#if defined(_WIN32)
    using namespace yesdaw::persistence;
    const auto bundle = requiredEnvironmentPath (L"YESDAW_CLOUD_DRIVE_BUNDLE");
    requireDriveBundle (bundle);
    CloudApi api;
    api.load();
    const auto project = readBundleProject (bundle);
    REQUIRE_FALSE (project.assets.empty());
    for (const auto& asset : project.assets)
    {
        const auto source = storedAssetPathForHash (bundle, asset.contentHash);
        requireBytesHash (source, asset.contentHash);
        {
            FileHandle file (source, GENERIC_READ | GENERIC_WRITE, OPEN_EXISTING);
            const HRESULT converted = api.convert (file.get(), asset.contentHash.bytes.data(),
                static_cast<DWORD> (asset.contentHash.bytes.size()), CF_CONVERT_FLAG_MARK_IN_SYNC, nullptr, nullptr);
            CAPTURE (source, converted);
            REQUIRE (converted == S_OK);
            file.close();
        }
        requireHydratedPlaceholder (api, source, asset.contentHash);
        const auto probeLink = bundle / "hard-link-probe.asset";
        const BOOL linked = CreateHardLinkW (probeLink.c_str(), source.c_str(), nullptr);
        const DWORD linkError = GetLastError();
        CAPTURE (linkError);
        REQUIRE (linked == FALSE);
        REQUIRE (linkError == ERROR_CLOUD_FILE_INCOMPATIBLE_HARDLINKS);
        REQUIRE_FALSE (std::filesystem::exists (probeLink));
    }
    api.close();
#else
    FAIL ("The cloud-placeholder drive requires Windows CfAPI.");
#endif
}

TEST_CASE ("ADR-0069 drive fixture: the user's audio stays hydrated placeholders and the autosave holds copies",
           "[.][cloud-drive-verify]")
{
#if defined(_WIN32)
    using namespace yesdaw::persistence;
    const auto bundle = requiredEnvironmentPath (L"YESDAW_CLOUD_DRIVE_BUNDLE");
    requireDriveBundle (bundle);
    CloudApi api;
    api.load();
    const auto project = readBundleProject (bundle);
    REQUIRE_FALSE (project.assets.empty());
    for (const auto& asset : project.assets)
    {
        const auto source = storedAssetPathForHash (bundle, asset.contentHash);
        requireBytesHash (source, asset.contentHash);
        requireHydratedPlaceholder (api, source, asset.contentHash);   // nothing the app did replaced the user's file
    }
    if (environmentFlag (L"YESDAW_CLOUD_DRIVE_EXPECT_SNAPSHOT"))
    {
        const auto snapshot = autosaveSnapshotPath (bundle);
        const auto held = readBundleProject (snapshot);   // read-only: every copy present and hashing true
        REQUIRE_FALSE (held.assets.empty());
        for (const auto& asset : held.assets)
        {
            const auto copy = storedAssetPathForHash (snapshot, asset.contentHash);
            requireBytesHash (copy, asset.contentHash);
            const auto source = storedAssetPathForHash (bundle, asset.contentHash);
            if (std::filesystem::exists (source))
                REQUIRE_FALSE (std::filesystem::equivalent (source, copy));   // a real copy: the OS refused the link
        }
    }
    api.close();
#else
    FAIL ("The cloud-placeholder drive requires Windows CfAPI.");
#endif
}

TEST_CASE ("ADR-0069 drive fixture: unregister and remove the drive's placeholder root", "[.][cloud-drive-cleanup]")
{
#if defined(_WIN32)
    const auto released = stateRoot();
    CloudApi api;
    api.load();
    {
        OwnedSyncRoot syncRoot (api);
        syncRoot.adopt (released);
        // A scanner or indexer can hold a handle a moment after the app exits: retry the step that failed (cleanup
        // keeps its progress - an unregistered root is not unregistered twice).
        OwnedSyncRoot::CleanupResult cleanup;
        std::uintmax_t removed = 0;
        for (int attempt = 0; attempt < 10; ++attempt)
        {
            cleanup = syncRoot.cleanup();
            removed += cleanup.removed;
            if (SUCCEEDED (cleanup.unregistered) && ! cleanup.removalError)
                break;
            Sleep (500);
        }
        CAPTURE (cleanup.unregistered, cleanup.removalError, removed);
        REQUIRE (cleanup.unregistered == S_OK);
        REQUIRE_FALSE (cleanup.removalError);
        REQUIRE (removed > 0);
    }
    REQUIRE_FALSE (std::filesystem::exists (released));
    api.close();
#else
    FAIL ("The cloud-placeholder drive requires Windows CfAPI.");
#endif
}
