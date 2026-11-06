#pragma once

// SOURCE ONLY. Private data below has NO definition. A linked native profile
// may be authored only after independent complete forward proof/qualification.
#include <array>
#include <cstddef>
#include <cfenv>
#include <cstdint>
#include <new>
#include <type_traits>

namespace yesdaw::app::musical_origin_source_r8 {
enum class NativeKind : std::uint8_t { Unsupported, WindowsMsvcX64 };
struct NativeBuildIdentity final
{
    NativeKind kind;
    std::uint64_t compilerFull, language, stlUpdate, iteratorDebug, hardening;
};
struct NativeSnapshot final
{
    NativeKind kind;
    std::uint64_t compilerFull, mxcsrControl, mxcsrStatus, crtControl;
    int controlError, flagError, rounding, exceptions, errorNumber, fma3Selection;
    std::fexcept_t flags;
    std::new_handler newHandler;
};
static_assert (std::is_trivially_copyable_v<NativeSnapshot>);
static_assert (std::is_trivially_default_constructible_v<NativeSnapshot>);

// Trivial fixed-source kernel results. Every leaf value-initializes its whole
// result and native stack struct even when the API refuses. No owner is acquired.
struct NativeControl final
{
    NativeKind kind;
    std::uint64_t compilerFull, mxcsrControl;
};
struct NativeModule final
{
    bool succeeded;
    std::uintptr_t module, anchor;
};
struct NativeImage final
{
    bool succeeded;
    std::uintptr_t base;
    std::uint64_t bytes;
};
struct NativePage final
{
    std::uint64_t returnedBytes, expectedBytes, regionBytes;
    std::uintptr_t base, allocationBase;
    std::uint32_t state, type, protect;
};
static_assert (std::is_trivial_v<NativeControl> && std::is_trivial_v<NativeModule>
               && std::is_trivial_v<NativeImage> && std::is_trivial_v<NativePage>);

// Defined concretely in native-observer.cpp. That TU must be separately built
// /fp:strict and pinned; no global compute/DSP/CMake flag is changed here.
class NativeObserver final
{
    friend class FundedNativeSession;
    // Before native qualification, ONLY these private fixed kernel leaves may
    // run. No getter/fenv/libm/allocator/stdio/restoration or variable walk here.
    static NativeControl control() noexcept;
    static NativeModule logAnchorModule() noexcept;
    static NativeModule getterAnchorModule() noexcept;
    static NativeImage moduleInfo (std::uintptr_t) noexcept;
    static NativePage page (std::uintptr_t) noexcept;
    // Selected Windows constants; strict TU checks the SDK representations.
    static constexpr std::uint32_t committed = 0x1000, image = 0x1000000;
    static constexpr std::uint32_t readOnly = 0x02, executeRead = 0x20;
    static constexpr std::uint32_t readWrite = 0x04, writeCopy = 0x08;
    static NativeSnapshot read() noexcept;
    static bool restoreStatus (const NativeSnapshot&) noexcept;
};

enum class NativeFact : std::uint8_t
{
    FrozenSourceAndCatalogue, SelectedSdkTopologyAndConfiguration,
    ActualPreciseComputeFlagsNoContract, ObserverStrictObject,
    PrivatePrefixInitialAndStoreInduction, BuildReplayQueryCodegenEquivalence,
    FullDomainLibmAndClassification, RelevantLoadedMathRegionsAndDispatchAssociation,
    Fma3GetterAndTrueIntervalStability, OriginalClockAllocationCallbackInterval,
    NewHandlerIdentityAndInterval, RelevantControlInterval,
    ExceptionFlagAndErrnoRestoration, ImmutableInputLifetimeNoAlias,
    CompleteNewSourceFunding, ActualCombinedCallerIdentity,
    IndependentAllInputForwardProof, Count
};
class NativeBinding final
{
    friend class FundedNativeSession;
    friend class FundedWindowQuery;
    struct Fact final { NativeFact kind; std::array<std::uint8_t, 32> evidenceDigest; };
    enum class RegionRole : std::uint8_t { Code, ReadOnlyTable };
    struct Region final
    {
        RegionRole role;
        std::uintptr_t rva;
        std::uint64_t bytes, expectedOffset;
    };
    enum class AccessRole : std::uint8_t { Fma3DispatchWord };
    struct AccessRegion final
    {
        AccessRole role;
        std::uintptr_t rva;
        std::uint64_t bytes;
    };
    struct Data final
    {
        NativeBuildIdentity compute;
        std::uint64_t observerCompilerFull, expectedMxcsrControl, expectedCrtControl;
        int expectedRounding;
        // Private policy bits0/1 permit initial getter0/1 respectively. Both
        // finite code arms should be matched; no policy forces every CPU to1.
        std::uint32_t allowedFma3Mask;
        std::new_handler expectedNewHandler;
        std::uintptr_t logEntryRva, getterEntryRva;
        // M/L are immutable extents, NOT prices. The arrays are private static
        // constant storage, sequential expected spans, no runtime/lazy loader.
        // All dependency/index/PIC/relocation and lifetime proofs remain HOLD.
        std::uint64_t regionCount, expectedByteCount;
        const Region* regions;
        const std::uint8_t* expectedBytes;
        // D/A address-only dependency extents. The mutable dispatch word is
        // NEVER compared/read here. Its later actual getter read has a separate
        // fixed aligned DWORD footprint and true writer/lifetime interval fact.
        std::uint64_t accessRegionCount, accessByteCount;
        const AccessRegion* accessRegions;
        Fact facts[static_cast<std::size_t> (NativeFact::Count)];
    };
    // Immutable reviewed profile/evidence identities only. No bool saying
    // "verified", setter, public constructor/token, callback or caller override.
    // Nonzero digests are completeness checks, NOT runtime validation of proofs.
    // Data binding itself is held until every named fact is actually proved.
    // A future definition must be constinit const, with no runtime initializer,
    // loader callback, dynamic evidence generation or hidden initialization guard.
    static const Data data_;
    static NativeBuildIdentity actualComputeIdentity() noexcept
    {
#if defined(_MSC_VER) && defined(_M_X64)
        return { NativeKind::WindowsMsvcX64, _MSC_FULL_VER, _MSVC_LANG,
                 _MSVC_STL_UPDATE, _ITERATOR_DEBUG_LEVEL, _MSVC_STL_HARDENING_VECTOR };
#else
        return { NativeKind::Unsupported, 0, 0, 0, 0, 0 };
#endif
    }
};
} // namespace yesdaw::app::musical_origin_source_r8
