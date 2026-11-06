#pragma once

// SOURCE ONLY. Private data below has NO definition. A linked native profile
// may be authored only after independent complete forward proof/qualification.
#include <array>
#include <cstddef>
#include <cfenv>
#include <cstdint>
#include <new>
#include <type_traits>

namespace yesdaw::app::musical_origin_source_r7 {
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

// Defined concretely in native-observer.cpp. That TU must be separately built
// /fp:strict and pinned; no global compute/DSP/CMake flag is changed here.
class NativeObserver final
{
    friend class FundedNativeSession;
    static NativeSnapshot read() noexcept;
    static bool restoreStatus (const NativeSnapshot&) noexcept;
};

enum class NativeFact : std::uint8_t
{
    FrozenSourceAndCatalogue, SelectedSdkTopologyAndConfiguration,
    ActualPreciseComputeFlagsNoContract, ObserverStrictObject,
    PrivatePrefixInitialAndStoreInduction, BuildReplayQueryCodegenEquivalence,
    FullDomainLibmAndClassification, LoadedCrtImageAndDispatchAssociation,
    Fma3GetterAndTrueIntervalStability, OriginalClockAllocationCallbackInterval,
    NewHandlerIdentityAndInterval, RelevantControlInterval,
    ExceptionFlagAndErrnoRestoration, ImmutableInputLifetimeNoAlias,
    CompleteNewSourceFunding, ActualCombinedCallerIdentity,
    IndependentAllInputForwardProof, Count
};
class NativeBinding final
{
    friend class FundedNativeSession;
    struct Fact final { NativeFact kind; std::array<std::uint8_t, 32> evidenceDigest; };
    struct Data final
    {
        NativeBuildIdentity compute;
        std::uint64_t observerCompilerFull, expectedMxcsrControl, expectedCrtControl;
        int expectedRounding, expectedFma3Selection;
        std::new_handler expectedNewHandler;
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
} // namespace yesdaw::app::musical_origin_source_r7
