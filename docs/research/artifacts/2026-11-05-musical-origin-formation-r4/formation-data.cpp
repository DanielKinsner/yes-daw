// SOURCE ONLY / UNCOMPILED. Reviewed conditional census R2, not execution credit.
#include "formation.h"

namespace yesdaw::app::musical_origin_source_r4 {
namespace {
constexpr U64 rawBytes = static_cast<U64> (sizeof (Raw));
constexpr U64 conversionUnit = std::is_same_v<U64, std::size_t> ? U64 { 0 } : U64 { 1 };
constexpr U64 fixedRoot = U64 { 9 } * rawBytes + U64 { 15471 } + U64 { 3 } * conversionUnit;

// Compile-time admission conditions only. None has been compiled or measured.
static_assert (siteCount == 129 && phaseCount == 6 && roleCount == 4);
static_assert (rawBytes > 0 && rawBytes <= U64 { 29824442 });
static_assert (conversionUnit <= 1);
static_assert (fixedRoot <= U64 { 268435452 } && fixedRoot < workCap);
static_assert (sizeof (MusicalPreparationRequestState) <= byteCap);
static_assert (std::numeric_limits<std::size_t>::max() >= byteCap);
static_assert (alignof (MusicalPreparationRequestState) <= alignof (std::max_align_t));
static_assert (alignof (engine::TempoChange) <= alignof (std::max_align_t));
static_assert (std::is_trivially_copyable_v<engine::TempoChange>);
static_assert (std::is_trivially_destructible_v<CapturedMetadata>);
static_assert (std::is_nothrow_copy_constructible_v<engine::EntityId>);
static_assert (std::is_nothrow_copy_constructible_v<engine::Clip>);
static_assert (std::is_nothrow_copy_constructible_v<engine::Asset>);
static_assert (std::is_nothrow_copy_constructible_v<engine::SampleRate>);
static_assert (std::is_nothrow_copy_constructible_v<engine::TempoChange>);
static_assert (std::is_nothrow_constructible_v<Parents, const SampleOwner&, const SampleOwner&>);
static_assert (std::is_nothrow_destructible_v<Parents>);
static_assert (std::is_trivially_destructible_v<OwnerSlot>);
}

// Exactly129 literals/constant expressions in Site ordinal order. No runtime
// initializer, factory, guard, setter, second table or caller-selected tariff.
constinit const Bounds Catalogue::reviewedData_ {
    {
        U64 { 177 } + U64 { 9 } * rawBytes, // 0 RootBegin
        101, // 1 RootSessionInit
        69, // 2 RootNextEntry
        67, // 3 RootCounterAlias
        87, // 4 RootCounterIssue
        79, // 5 RootCounterExhausted
        1087, // 6 RootIdentity
        66, // 7 RootIdentityRefused
        85, // 8 RootAllocate
        66, // 9 RootAllocateFailed
        88, // 10 RootObject
        718, // 11 RootInstall
        384, // 12 RootNoState
        743, // 13 RootFailedAdopt
        78, // 14 RootFailedConstructed
        414, // 15 RootFailedReadout
        68, // 16 RootFailedDiscard
        653, // 17 RootFailedSnapshot
        80, // 18 RootFailedDestroy
        101, // 19 RootFailedSlotErase
        93, // 20 RootFailedSessionCheck
        94, // 21 RootFailedSessionRetiring; admitted visits0, not zero price
        78, // 22 RootFailedSessionFinish
        691, // 23 Distribute
        414, // 24 CaptureDispatch
        U64 { 121 } + U64 { 3 } * conversionUnit, // 25 CaptureCheck
        91, // 26 CaptureSizes
        97, // 27 CaptureProducts
        198, // 28 CaptureNeed
        88, // 29 CaptureTooLarge
        88, // 30 CaptureOverflow
        88, // 31 CaptureBytesRefused
        88, // 32 CaptureReserveRefused
        262, // 33 CaptureWorkBegin
        94, // 34 ClipLoopEntry
        94, // 35 ClipLoopTest
        U64 { 687 } + conversionUnit, // 36 ClipRow
        88, // 37 ClipDecision
        U64 { 100 } + conversionUnit, // 38 ClipFound
        94, // 39 ClipContinue
        96, // 40 ClipEnd
        88, // 41 ClipMissingTest
        128, // 42 ClipMissing
        94, // 43 AssetLoopEntry
        94, // 44 AssetLoopTest
        U64 { 688 } + conversionUnit, // 45 AssetRow
        88, // 46 AssetDecision
        U64 { 100 } + conversionUnit, // 47 AssetFound
        94, // 48 AssetContinue
        96, // 49 AssetEnd
        88, // 50 AssetMissingTest
        128, // 51 AssetMissing
        638, // 52 CaptureMetadata; corrected four class copies+placement
        105, // 53 CaptureParents
        100, // 54 SourceCheck
        128, // 55 SourceMissing
        133, // 56 SourceLayout
        128, // 57 SourceLayoutRefused
        108, // 58 SourceExtent
        128, // 59 SourceExtentRefused
        102, // 60 SourceProduct
        128, // 61 SourceProductRefused
        88, // 62 MapNeededTest
        U64 { 124 } + conversionUnit, // 63 MapAllocate
        128, // 64 MapAllocateFailed
        110, // 65 MapInstall
        93, // 66 MapLoopEntry
        94, // 67 MapLoopTest
        U64 { 118 } + conversionUnit, // 68 MapRow
        90, // 69 MapFirst
        97, // 70 MapLater
        94, // 71 MapDecision
        133, // 72 MapInvalid
        111, // 73 MapStore
        94, // 74 MapContinue
        96, // 75 MapEnd
        128, // 76 CaptureFinish
        94, // 77 CaptureResultTest
        90, // 78 CaptureRefused
        463, // 79 ReserveScan
        87, // 80 ReserveFail
        131, // 81 ReserveInstall
        708, // 82 SnapshotFixed
        155, // 83 SnapshotRow
        774, // 84 OwnerAdopt
        107, // 85 OwnerConstructed
        435, // 86 ReadoutCommit
        93, // 87 NewFailureTest
        96, // 88 AbandonNewState
        110, // 89 SessionSelect
        106, // 90 SessionKeepRunning
        92, // 91 SessionReusePending
        116, // 92 SessionInstall
        133, // 93 SlotErase
        120, // 94 DrainRetiring
        125, // 95 SessionDestroyCheck
        126, // 96 SessionDestroyRetiring
        110, // 97 SessionDestroyFinish
        154, // 98 ArmWorker
        133, // 99 AbortWorker
        142, // 100 ReceiveWorker; corrected state parameter W
        142, // 101 ReceiveReader; corrected state parameter W
        112, // 102 ContextCancel
        148, // 103 ContextInspect
        102, // 104 ContextDecision
        128, // 105 ContextReturn
        118, // 106 AcquireWorker
        113, // 107 AcquireCurrentCaller
        118, // 108 AcquireReader
        155, // 109 RetainExternal
        130, // 110 ArmReader
        116, // 111 AbortReader
        111, // 112 ControlDestroy
        96, // 113 ControlDetachTest
        95, // 114 ControlDetach
        100, // 115 ControlDropCaller
        112, // 116 RetainedDestroy
        95, // 117 RetainedDropCaller
        104, // 118 DropProbe
        138, // 119 DropNonLast
        93, // 120 DropLast
        89, // 121 DropMapTest
        128, // 122 DropMapRetire
        89, // 123 DropParentTest
        128, // 124 DropParentRetire
        127, // 125 DropRoleRetire
        116, // 126 MapDestroy
        107, // 127 ParentDestroy
        515 // 128 StateDestroy
    },
    fixedRoot,
    U64 { 1145 } + U64 { 3 } * conversionUnit,
    515, 116, 107, // state/map/parent cleanup reservations
    { 3082, 874, 1213, 874 }, // Control,Worker,Retained,Reader
    U64 { 2925 } + conversionUnit,
    U64 { 970 } + U64 { 2 } * conversionUnit,
    U64 { 608 } + conversionUnit
};
} // namespace yesdaw::app::musical_origin_source_r4
