// Compile-only public-type layout table. No main, constructor, or target call.
#include "../20261105-musical-funded-legacy-adapter-source-r5/legacy-adapter.h"
#include <cstdint>
using namespace yesdaw::app::musical_origin_source_r8;
static_assert(sizeof(std::uint64_t)==8);
extern "C" {
extern constinit const std::uint64_t yesdaw_r4_public_layout[] = {
    0x5945534441575234ULL, 37ULL,
    sizeof(Bounds), alignof(Bounds),
    sizeof(LegacyRaw), alignof(LegacyRaw),
    sizeof(LegacyObservation), alignof(LegacyObservation),
    sizeof(LegacyReadout), alignof(LegacyReadout),
    sizeof(Raw), alignof(Raw),
    sizeof(Slice), alignof(Slice),
    sizeof(Canonical), alignof(Canonical),
    sizeof(Snapshot), alignof(Snapshot),
    sizeof(FormationReceipt), alignof(FormationReceipt),
    sizeof(RootCensus), alignof(RootCensus),
    sizeof(Readout), alignof(Readout),
    sizeof(Parents), alignof(Parents),
    sizeof(CapturedMetadata), alignof(CapturedMetadata),
    sizeof(LegacyPreparationOptions), alignof(LegacyPreparationOptions),
    sizeof(CapturedIntent), alignof(CapturedIntent),
    sizeof(MusicalPreparationRequestState), alignof(MusicalPreparationRequestState),
    sizeof(Ledger), alignof(Ledger),
    sizeof(Formed), alignof(Formed),
    sizeof(Formation), alignof(Formation),
    sizeof(Exchange), alignof(Exchange),
    sizeof(WorkerLegacyResult), alignof(WorkerLegacyResult),
    sizeof(ControlRequestOwner), alignof(ControlRequestOwner),
    sizeof(RetainedOriginOwner), alignof(RetainedOriginOwner),
    sizeof(OwnerSlot), alignof(OwnerSlot),
    sizeof(ApplicationRequests), alignof(ApplicationRequests),
    sizeof(WorkerArtifacts), alignof(WorkerArtifacts),
    sizeof(ActionVector), alignof(ActionVector),
    sizeof(QueryAccounting), alignof(QueryAccounting),
    sizeof(FundedNativeSession), alignof(FundedNativeSession),
    sizeof(QueryCallResult), alignof(QueryCallResult),
    sizeof(FundedWindowQuery), alignof(FundedWindowQuery),
    sizeof(NativeBuildIdentity), alignof(NativeBuildIdentity),
    sizeof(NativeSnapshot), alignof(NativeSnapshot),
    sizeof(NativeControl), alignof(NativeControl),
    sizeof(NativeModule), alignof(NativeModule),
    sizeof(NativeImage), alignof(NativeImage),
    sizeof(NativePage), alignof(NativePage),
};
}
