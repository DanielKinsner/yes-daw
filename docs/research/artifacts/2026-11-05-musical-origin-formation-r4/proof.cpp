// SOURCE ONLY / UNRUN R2: guarded independent proof awaiting fresh whole review.
#include "proof-access.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>
#include <optional>
#include <thread>

namespace formation_proof_support {
using namespace yesdaw::app::musical_origin_source_r4;
inline unsigned checks = 0, failures = 0;
inline std::array<bool, siteCount> observedSites {};
inline const char* currentCase = "startup";
inline void check (bool ok, const char* expression, int line) noexcept
{
    ++checks;
    if (! ok)
    {
        ++failures;
        std::fprintf (stderr, "FAIL %s line %d: %s\n", currentCase, line, expression);
        std::fputs ("FAIL formation whole proof: stopped before dependent work; no complete lifetime/inventory credit\n", stderr);
        // Do not unwind an armed/unknown owner or run a receiver as cleanup.
        // All harness threads/owners are automatic. Our static objects are fixed
        // trivially destructible bookkeeping; no atexit owner/thread callback.
        std::exit (1);
    }
}
#define EXPECT(expression) ::formation_proof_support::check ((expression), #expression, __LINE__)

// This finite isolated allocator records only direct nothrow request calls.
// Fixture/container allocations use the ordinary forwarding replacement below.
// Scope/ordinal provenance identifies State; native size is a separate check.
struct Allocation final
{
    void* identity {};
    std::size_t bytes {}, request {}, ordinal {};
    bool state {}, freed {}, captured {}, freeForwarded {};
    unsigned releaseOrder {};
    Canonical finalCanonical {};
    Raw finalRaw {};
};
inline std::array<Allocation, 512> allocations {};
inline std::size_t allocationCount = 0, requestNumber = 0, requestOrdinal = 0;
inline std::size_t failOrdinal = 0, attempts = 0;
inline bool requestActive = false;
inline unsigned releaseOrder = 0;
struct RequestTrace final
{
    std::size_t count {}, successes {}, injectedFailures {}, sizes[2] {};
};
inline std::array<RequestTrace, 512> requests {};
struct ParentTrace final
{
    const void* identity {};
    std::size_t request {};
    unsigned releasedAt {};
    bool forwarded {};
};
inline std::array<ParentTrace, 4> parentTraces {};
inline std::size_t parentTraceCount = 0;
using Capture = void (*) (Allocation&, const void*) noexcept;
inline Capture capture = nullptr;

inline void beginRequest (std::size_t fail = 0) noexcept
{
    EXPECT (! requestActive);
    EXPECT (requestNumber < requests.size());
    requestActive = true; ++requestNumber; requestOrdinal = 0; failOrdinal = fail;
}
inline void endRequest() noexcept
{
    EXPECT (requestActive);
    requestActive = false; failOrdinal = 0;
}
inline void* nativeAllocate (std::size_t bytes) noexcept
{
    if (! requestActive) return std::malloc (bytes == 0 ? 1 : bytes);
    ++requestOrdinal; ++attempts;
    auto& trace = requests[requestNumber - 1];
    EXPECT (trace.count < 2);
    if (trace.count < 2) trace.sizes[trace.count] = bytes;
    ++trace.count;
    if (requestOrdinal == failOrdinal) { ++trace.injectedFailures; return nullptr; }
    void* p = std::malloc (bytes == 0 ? 1 : bytes);
    if (p == nullptr) return nullptr;
    if (allocationCount == allocations.size())
    {
        EXPECT (false); std::free (p); return nullptr;
    }
    auto& record = allocations[allocationCount++];
    record.identity = p; record.bytes = bytes; record.request = requestNumber;
    record.ordinal = requestOrdinal; record.state = requestOrdinal == 1;
    ++trace.successes;
    return p;
}
inline void nativeRelease (void* p) noexcept
{
    if (p == nullptr) return;
    ParentTrace* parent = nullptr;
    for (std::size_t i = 0; i != parentTraceCount; ++i)
        if (parentTraces[i].identity == p && parentTraces[i].releasedAt == 0)
        { parent = &parentTraces[i]; parent->releasedAt = ++releaseOrder; }
    Allocation* released = nullptr;
    // Most recent live identity handles genuine malloc address reuse.
    for (std::size_t i = allocationCount; i != 0; --i)
    {
        auto& record = allocations[i - 1];
        if (record.identity == p && ! record.freed)
        {
            released = &record;
            if (! record.state)
                for (std::size_t j = 0; j != parentTraceCount; ++j)
                    if (parentTraces[j].request == record.request)
                        EXPECT (parentTraces[j].releasedAt == 0); // Parents still live at actual map free.
            if (record.state)
            {
                EXPECT (capture != nullptr);
                if (capture != nullptr) capture (record, p);
            }
            record.freed = true; record.releaseOrder = ++releaseOrder;
            break;
        }
    }
    std::free (p); // Every intercepted nonnull release forwards exactly once.
    if (released != nullptr) released->freeForwarded = true;
    if (parent != nullptr) parent->forwarded = true;
}

// Independent reviewed price literals, not a query into Catalogue/production.
// B and d are native compile-time qualifications, not guessed ABI constants.
inline constexpr U64 B = sizeof (Raw);
inline constexpr U64 d = std::is_same_v<U64, std::size_t> ? 0 : 1;
inline constexpr std::array<U64, siteCount> prices {{
    177+9*B,101,69,67,87,79,1087,66,85,66,88,718,384,743,78,414,
    68,653,80,101,93,94,78,691,414,121+3*d,91,97,198,88,88,88,
    88,262,94,94,687+d,88,100+d,94,96,88,128,94,94,688+d,88,
    100+d,94,96,88,128,638,105,100,128,133,128,108,128,102,128,
    88,124+d,128,110,93,94,118+d,90,97,94,133,111,94,96,128,94,
    90,463,87,131,708,155,774,107,435,93,96,110,106,92,116,133,
    120,125,126,110,154,133,142,142,112,148,102,128,118,113,118,
    155,130,116,111,96,95,100,112,95,104,138,93,89,128,89,128,
    127,116,107,515
}};
inline constexpr U64 rootReservation = 9*B + 15471 + 3*d;
inline constexpr U64 roleReservations[4] { 3082,874,1213,874 };
inline constexpr std::size_t index (Site site) noexcept { return static_cast<std::size_t> (site); }

// The oracle describes public schedules. It never invokes Formation/Ledger,
// manufactures a Fund, edits observed fields or gives production a test tariff.
struct Expected final
{
    std::array<U64, siteCount> sites {};
    std::array<U64, roleCount> roleSpent {};
    std::array<bool, roleCount> roleRetired {};
    U64 mapRows = 0, admittedVariable = 0;
    void event (Site site, U64 count = 1) noexcept { sites[index (site)] += count; }
    void roleEvent (Role role, Site site) noexcept
    { event (site); roleSpent[static_cast<std::size_t> (role)] += prices[index (site)]; }
    U64 n (Site site) const noexcept { return sites[index (site)]; }
    static std::size_t phase (std::size_t site) noexcept
    {
        if ((site >= 25 && site <= 76) || (site >= 79 && site <= 81)) return 1;
        if (site >= 126) return 4;
        if ((site >= 84 && site <= 85) || site == 88 || (site >= 93 && site <= 125)) return 2;
        return 0;
    }
    std::array<U64, phaseCount> byPhase() const noexcept
    {
        std::array<U64, phaseCount> out {};
        for (std::size_t i = 0; i != sites.size(); ++i) out[phase (i)] += sites[i] * prices[i];
        return out;
    }
    std::array<U64, phaseCount> phaseAllocation (bool physical) const noexcept
    {
        std::array<U64, phaseCount> out {};
        out[0] = rootReservation;
        if (physical)
        {
            constexpr U64 check = 1145+3*d;
            constexpr U64 roles = 3082+874+1213+874;
            out[0] -= check + roles + 515;
            out[1] = check + admittedVariable;
            out[2] = roles;
            out[4] = 515;
            if (n (Site::ReserveInstall) != 0) { out[1] -= 223; out[4] += 223; }
        }
        return out;
    }
    void canonical (const Canonical& canonical, bool final = false) const noexcept
    {
        const auto used = byPhase(), allocated = phaseAllocation (true);
        U64 total = 0;
        for (std::size_t p = 0; p != phaseCount; ++p)
        {
            EXPECT (canonical.actual[p] == used[p]);
            EXPECT (used[p] <= allocated[p]);
            EXPECT (canonical.reserved[p] == allocated[p] - used[p]);
            EXPECT (canonical.terminal[p] <= canonical.reserved[p]);
            if (final) EXPECT (canonical.terminal[p] == canonical.reserved[p]);
            total += canonical.actual[p] + canonical.reserved[p];
        }
        EXPECT (total == rootReservation + admittedVariable);
        EXPECT (total <= workCap);
    }
    void receipt (const FormationReceipt& receipt, CaptureStatus status, bool physical) const noexcept
    {
        EXPECT (receipt.status == status);
        EXPECT (receipt.physicalState == physical);
        const auto used = byPhase(), allocated = phaseAllocation (physical);
        U64 actualTotal = 0, reservedTotal = 0;
        for (std::size_t p = 0; p != phaseCount; ++p)
        {
            EXPECT (receipt.origin.actualByPhase[p] == used[p]);
            EXPECT (receipt.origin.reservedByPhase[p] == allocated[p] - used[p]);
            actualTotal += used[p]; reservedTotal += allocated[p] - used[p];
        }
        EXPECT (receipt.origin.cap == workCap);
        EXPECT (receipt.origin.actual == actualTotal);
        EXPECT (receipt.origin.reserved == reservedTotal);
        EXPECT (receipt.origin.total == actualTotal + reservedTotal);
        EXPECT (receipt.origin.total == rootReservation + admittedVariable);
        exactSites (receipt.raw); rawDiagnostics (receipt.raw);
    }
    void snapshot() noexcept { event (Site::SnapshotFixed); event (Site::SnapshotRow, 6); }
    void failedSnapshot() noexcept { event (Site::RootFailedSnapshot); }
    void drop (Role role, bool last, bool map, bool parents) noexcept
    {
        roleEvent (role, Site::DropProbe);
        roleRetired[static_cast<std::size_t> (role)] = true;
        if (! last) { roleEvent (role, Site::DropNonLast); return; }
        roleEvent (role, Site::DropLast); roleEvent (role, Site::DropMapTest);
        if (map) event (Site::MapDestroy);
        roleEvent (role, Site::DropMapRetire); roleEvent (role, Site::DropParentTest);
        if (parents) event (Site::ParentDestroy);
        roleEvent (role, Site::DropParentRetire); roleEvent (role, Site::DropRoleRetire);
        event (Site::StateDestroy);
    }
    void controlDestroyed (bool last, bool map, bool parents, bool retained = false) noexcept
    {
        roleEvent (Role::Control, Site::SlotErase); roleEvent (Role::Control, Site::ControlDestroy);
        roleEvent (Role::Control, Site::ControlDetachTest);
        if (retained) roleEvent (Role::Control, Site::ControlDetach);
        roleEvent (Role::Control, Site::ControlDropCaller); drop (Role::Control, last, map, parents);
    }
    void workerReturned (bool cancelled) noexcept
    {
        roleEvent (Role::Worker, Site::ReceiveWorker); roleEvent (Role::Worker, Site::ContextCancel);
        roleEvent (Role::Worker, Site::ContextDecision);
        if (! cancelled) roleEvent (Role::Worker, Site::ContextInspect);
        roleEvent (Role::Worker, Site::ContextReturn); drop (Role::Worker, false, false, false);
    }
    void readerReturned (bool cancelled) noexcept
    {
        roleEvent (Role::Reader, Site::ReceiveReader); roleEvent (Role::Reader, Site::ContextCancel);
        roleEvent (Role::Reader, Site::ContextDecision);
        if (! cancelled) roleEvent (Role::Reader, Site::ContextInspect);
        roleEvent (Role::Reader, Site::ContextReturn); drop (Role::Reader, false, false, false);
    }
    U64 actual() const noexcept
    {
        U64 out = 0;
        for (std::size_t i = 0; i != sites.size(); ++i) out += sites[i] * prices[i];
        return out;
    }
    void exactSites (const Raw& raw) const noexcept
    {
        for (std::size_t i = 0; i != sites.size(); ++i)
        {
            if (raw.sites[i] != 0) observedSites[i] = true;
            if (raw.sites[i] != sites[i] && failures < 30)
                std::fprintf (stderr, "Site %zu expected %llu got %llu\n", i,
                              static_cast<unsigned long long> (sites[i]),
                              static_cast<unsigned long long> (raw.sites[i]));
            EXPECT (raw.sites[i] == sites[i]);
        }
    }
    void rawDiagnostics (const Raw& raw) const noexcept
    {
        const U64 roots = n (Site::RootBegin);
        const U64 identity = n (Site::RootIdentity);
        const U64 installed = n (Site::Distribute);
        const U64 reserved = n (Site::ReserveScan);
        const U64 searches = n (Site::ClipRow) + n (Site::AssetRow);
        const U64 snapshots = n (Site::SnapshotFixed);
        const U64 failedSnapshots = n (Site::RootFailedSnapshot);
        const U64 final = n (Site::StateDestroy);
        // Closed formulas over actual public outcomes, including fixed loops,
        // full 16-byte ID comparisons, found breaks and false exits separately.
        EXPECT (raw.loopEntries == roots + 2*identity + 3*installed + reserved + searches
                + n (Site::ClipLoopEntry) + n (Site::AssetLoopEntry) + n (Site::MapLoopEntry)
                + 2*snapshots + failedSnapshots + final);
        EXPECT (raw.loopBodies == B*roots + 32*identity + 14*installed + 6*reserved + 16*searches
                + searches + n (Site::MapRow) + 12*snapshots + 6*failedSnapshots + 4*final);
        EXPECT (raw.loopConditions == (B+1)*roots + 34*identity + 17*installed + 7*reserved
                + 17*searches + n (Site::ClipLoopTest) + n (Site::AssetLoopTest) + n (Site::MapLoopTest)
                + 14*snapshots + 7*failedSnapshots + 5*final);
        EXPECT (raw.loopIncrements == B*roots + 32*identity + 14*installed + 6*reserved
                + 16*searches + n (Site::ClipContinue) + n (Site::AssetContinue) + n (Site::MapContinue)
                + 12*snapshots + 6*failedSnapshots + 4*final);
        EXPECT (raw.loopFinalFalse == roots + 2*identity + 3*installed + reserved + searches
                + n (Site::ClipEnd) + n (Site::AssetEnd) + n (Site::MapEnd)
                + 2*snapshots + failedSnapshots + final);
        EXPECT (raw.loopBreaks == n (Site::ClipFound) + n (Site::ClipEnd) + n (Site::AssetFound)
                + n (Site::AssetEnd) + n (Site::MapEnd));
        EXPECT (raw.loopReturns == n (Site::MapInvalid));
        EXPECT (raw.searchRows == searches);
        EXPECT (raw.copiedMapRows == n (Site::MapStore));
        EXPECT (raw.allocationAttempts == n (Site::RootAllocate) + n (Site::MapAllocate));
        EXPECT (raw.allocationSuccesses == n (Site::RootObject) + n (Site::MapInstall));
        EXPECT (raw.allocationReleases == n (Site::MapDestroy) + final);
        const U64 stateSize = sizeof (MusicalPreparationRequestState);
        const U64 mapBytes = mapRows * sizeof (yesdaw::engine::TempoChange);
        EXPECT (raw.requestedBytes == n (Site::RootAllocate)*stateSize + n (Site::MapAllocate)*mapBytes);
        EXPECT (raw.currentBytes == (n (Site::RootObject)-final)*stateSize
                + (n (Site::MapInstall)-n (Site::MapDestroy))*mapBytes);
        EXPECT (raw.peakBytes == n (Site::RootObject)*stateSize + n (Site::MapInstall)*mapBytes);
        EXPECT (raw.ownerConstructions == n (Site::OwnerConstructed) + n (Site::RootFailedConstructed));
        EXPECT (raw.ownerDestructions == n (Site::ControlDropCaller) + n (Site::RootFailedDestroy));
        EXPECT (raw.referenceRetains == n (Site::ArmWorker) + n (Site::RetainExternal) + n (Site::ArmReader));
        EXPECT (raw.referenceDrops == n (Site::DropNonLast) + n (Site::DropLast));
        EXPECT (raw.terminalStores == n (Site::ContextReturn));
        EXPECT (raw.terminalLoads == n (Site::AcquireWorker) + n (Site::AcquireReader));
    }
};
} // namespace formation_proof_support

namespace formation_proof_support {
namespace e = yesdaw::engine;
struct Fixture final
{
    e::Project project;
    SampleOwner original, ready;
    e::EntityId clipId = e::EntityId::fromBigEndianParts (0x8801, 3);
    U64 nextId = 1, version = 0, generation = 1;
    Fixture()
    {
        project.id = e::EntityId::fromBigEndianParts (0x8801, 1);
        project.sampleRate = { 48000.0 };
        e::Asset asset;
        asset.id = e::EntityId::fromBigEndianParts (0x8801, 2);
        asset.frames = 8; asset.channels = 2; asset.sampleRate = { 48000.0 };
        project.assets.push_back (asset);
        e::Clip clip;
        clip.id = clipId; clip.assetId = asset.id;
        clip.trackId = e::EntityId::fromBigEndianParts (0x8801, 4);
        clip.timeBase = e::TimeBase::TempoLocked;
        clip.timelineStart = 7; clip.timelineLength = 3840;
        clip.srcOffset = 1; clip.srcLen = 4;
        project.clips.push_back (clip);
        project.tempoMap = { { 0, 120.0, e::TempoCurve::LinearRamp },
                             { 960, 93.0, e::TempoCurve::Jump },
                             { 3840, 137.0, e::TempoCurve::Jump } };
        auto samples = std::make_shared<e::AssetSamples>();
        samples->channels = 2; samples->frames = 8;
        for (int i = 0; i != 16; ++i) samples->interleaved.push_back (static_cast<float> (i) * 0.03125f);
        original = samples; ready = original;
    }
};

struct CaptureCase final
{
    CaptureStatus status = CaptureStatus::RequestCaptured;
    Site counter = Site::RootCounterIssue;
    Site refusal = Site::Count;
    U64 clipVisits = 1, assetVisits = 1, mapVisits = 3, mapCopies = 3;
    bool identityRefused = false, stateAllocationFailed = false;
    bool clipFound = true, assetFound = true;
};

// One independent path description per fixture supplies selected traversal
// lengths and outcomes. This never reads a production diagnostic to choose a path.
inline Expected captured (const Fixture& fixture, const CaptureCase& c, bool initial) noexcept
{
    Expected m;
    m.mapRows = fixture.project.tempoMap.size();
    m.event (Site::RootBegin); m.event (initial ? Site::RootSessionInit : Site::RootNextEntry);
    m.event (c.counter); m.event (Site::RootIdentity);
    if (c.identityRefused)
    {
        m.event (Site::RootIdentityRefused); m.event (Site::RootNoState); return m;
    }
    m.event (Site::RootAllocate);
    if (c.stateAllocationFailed)
    {
        m.event (Site::RootAllocateFailed); m.event (Site::RootNoState); return m;
    }
    m.event (Site::RootObject); m.event (Site::RootInstall); m.event (Site::Distribute);
    m.event (Site::CaptureDispatch); m.event (Site::CaptureCheck);
    if (c.refusal != Site::CaptureTooLarge)
    {
        m.event (Site::CaptureSizes);
        if (c.refusal != Site::CaptureOverflow)
        {
            m.event (Site::CaptureProducts);
            if (c.refusal != Site::CaptureBytesRefused)
            {
                m.event (Site::CaptureNeed); m.event (Site::ReserveScan);
                if (c.refusal == Site::CaptureReserveRefused) m.event (Site::ReserveFail);
                else
                {
                    m.event (Site::ReserveInstall);
                    m.admittedVariable = 3148+d
                        + (fixture.project.clips.size()+fixture.project.assets.size())*(970+2*d)
                        + fixture.project.tempoMap.size()*(608+d);
                    m.event (Site::CaptureWorkBegin); m.event (Site::ClipLoopEntry);
                    m.event (Site::ClipLoopTest, c.clipVisits + (c.clipFound ? 0 : 1));
                    m.event (Site::ClipRow, c.clipVisits); m.event (Site::ClipDecision, c.clipVisits);
                    m.event (Site::ClipContinue, c.clipVisits - (c.clipFound ? 1 : 0));
                    if (c.clipFound) m.event (Site::ClipFound); else m.event (Site::ClipEnd);
                    m.event (Site::ClipMissingTest);
                    if (! c.clipFound) m.event (Site::ClipMissing);
                    else
                    {
                        m.event (Site::AssetLoopEntry);
                        m.event (Site::AssetLoopTest, c.assetVisits + (c.assetFound ? 0 : 1));
                        m.event (Site::AssetRow, c.assetVisits); m.event (Site::AssetDecision, c.assetVisits);
                        m.event (Site::AssetContinue, c.assetVisits - (c.assetFound ? 1 : 0));
                        if (c.assetFound) m.event (Site::AssetFound); else m.event (Site::AssetEnd);
                        m.event (Site::AssetMissingTest);
                        if (! c.assetFound) m.event (Site::AssetMissing);
                        else
                        {
                            m.event (Site::CaptureMetadata); m.event (Site::CaptureParents); m.event (Site::SourceCheck);
                            if (c.refusal != Site::SourceMissing)
                            {
                                m.event (Site::SourceLayout);
                                if (c.refusal != Site::SourceLayoutRefused)
                                {
                                    m.event (Site::SourceExtent);
                                    if (c.refusal != Site::SourceExtentRefused)
                                    {
                                        m.event (Site::SourceProduct);
                                        if (c.refusal != Site::SourceProductRefused)
                                        {
                                            m.event (Site::MapNeededTest);
                                            if (m.mapRows != 0)
                                            {
                                                m.event (Site::MapAllocate);
                                                if (c.refusal != Site::MapAllocateFailed) m.event (Site::MapInstall);
                                            }
                                            if (c.refusal != Site::MapAllocateFailed)
                                            {
                                                m.event (Site::MapLoopEntry);
                                                const bool valid = c.status == CaptureStatus::RequestCaptured;
                                                m.event (Site::MapLoopTest, c.mapVisits + (valid ? 1 : 0));
                                                m.event (Site::MapRow, c.mapVisits);
                                                if (c.mapVisits != 0)
                                                {
                                                    m.event (Site::MapFirst); m.event (Site::MapLater, c.mapVisits-1);
                                                }
                                                m.event (Site::MapDecision, c.mapVisits);
                                                m.event (Site::MapStore, c.mapCopies); m.event (Site::MapContinue, c.mapCopies);
                                                if (valid) { m.event (Site::MapEnd); m.event (Site::CaptureFinish); }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if (c.refusal != Site::Count) m.event (c.refusal);
    m.event (Site::CaptureResultTest);
    if (c.status != CaptureStatus::RequestCaptured) m.event (Site::CaptureRefused);
    return m;
}
} // namespace formation_proof_support

void* operator new (std::size_t n)
{
    if (void* p = std::malloc (n == 0 ? 1 : n)) return p;
    throw std::bad_alloc {};
}
void* operator new[] (std::size_t n) { return ::operator new (n); }
void* operator new (std::size_t n, const std::nothrow_t&) noexcept
{ return formation_proof_support::nativeAllocate (n); }
void* operator new[] (std::size_t n, const std::nothrow_t&) noexcept
{ return formation_proof_support::nativeAllocate (n); }
void operator delete (void* p) noexcept { formation_proof_support::nativeRelease (p); }
void operator delete[] (void* p) noexcept { formation_proof_support::nativeRelease (p); }
void operator delete (void* p, std::size_t) noexcept { formation_proof_support::nativeRelease (p); }
void operator delete[] (void* p, std::size_t) noexcept { formation_proof_support::nativeRelease (p); }
void operator delete (void* p, const std::nothrow_t&) noexcept { formation_proof_support::nativeRelease (p); }
void operator delete[] (void* p, const std::nothrow_t&) noexcept { formation_proof_support::nativeRelease (p); }

namespace yesdaw::app::musical_origin_source_r4 {
class FormationProof final
{
    using Access = ProofAccess;
    using Expected = formation_proof_support::Expected;
    using Fixture = formation_proof_support::Fixture;
    using CaptureCase = formation_proof_support::CaptureCase;
    using Allocation = formation_proof_support::Allocation;
    using StateView = Access::StateObservation;
    struct AppStorage final { alignas (ApplicationRequests) std::byte bytes[sizeof (ApplicationRequests)]; };
    struct Origin final
    {
        Expected now, history;
        CaptureStatus status;
        bool physical;
        std::size_t request;
    };

    static Access::SessionObservation checkedSession (const ApplicationRequests& app) noexcept
    {
        const auto s = Access::session (app); // Fully live App; no State access.
        EXPECT (s.current < 2 && s.retiring < 2);
        EXPECT (s.current != s.retiring);
        EXPECT (s.live[s.current]); // Reached only after the real index guard.
        EXPECT (s.live[s.retiring] == s.hasRetiring);
        return s;
    }
    static Access::OwnerObservation checkedCurrent (const ApplicationRequests& app) noexcept
    {
        const auto s = checkedSession (app);
        // Caller independently owns a quiescent current origin: never use this
        // after arm until the actual abort or production terminal acquire.
        const auto owner = Access::observeSlot (app, s.current);
        EXPECT (owner.present);
        return owner;
    }
    static StateView captureState (const ApplicationRequests& app, std::size_t slot,
                                   U64 requiredRows) noexcept
    {
        const auto s = checkedSession (app);
        EXPECT (slot < 2 && slot == s.current && s.live[slot]);
        const auto owner = checkedCurrent (app);
        EXPECT (owner.hasState && (! owner.armed || owner.terminalAcquired));
        const auto& state = owner.liveState;
        EXPECT (state.allocationIdentity != nullptr && state.context == Context::Control);
        EXPECT (state.metadataLive && state.parentLive);
        EXPECT (state.mapConstructed <= state.mapCapacity);
        EXPECT (requiredRows <= state.mapConstructed);
        EXPECT (requiredRows == 0 || state.mapIdentity != nullptr);
        return state;
    }
    static void readyForWorker (const ApplicationRequests& app, const Origin& origin) noexcept
    {
        const auto s = checkedSession (app);
        EXPECT (! s.hasRetiring);
        current (app, origin);
        const auto owner = checkedCurrent (app);
        EXPECT (owner.hasState && ! owner.armed && ! owner.terminalAcquired);
        EXPECT (owner.liveState.status == CaptureStatus::RequestCaptured);
        EXPECT (owner.liveState.context == Context::Control && ! owner.liveState.workerIssued);
        EXPECT (owner.liveState.metadataLive && owner.liveState.parentLive);
        captureState (app, s.current, origin.now.n (Site::MapStore));
    }

    static void captureAtRelease (Allocation& record, const void* allocation) noexcept
    {
        EXPECT (record.bytes == Access::stateBytes);
        EXPECT (! record.captured && ! record.freed);
        if (record.bytes != Access::stateBytes) return;
        std::array<std::byte, Access::canonicalBytes> canonicalBytes;
        std::array<std::byte, Access::rawBytes> rawBytes;
        const auto* bytes = static_cast<const std::byte*> (allocation);
        // No typed State/member access after destruction. The real malloc storage
        // remains owned here; nativeRelease forwards free after this returns.
        std::memcpy (canonicalBytes.data(), bytes + Access::canonicalOffset, canonicalBytes.size());
        std::memcpy (rawBytes.data(), bytes + Access::rawOffset, rawBytes.size());
        std::memcpy (&record.finalCanonical, canonicalBytes.data(), canonicalBytes.size());
        std::memcpy (&record.finalRaw, rawBytes.data(), rawBytes.size());
        record.captured = true;
    }
    static Allocation* allocationFor (std::size_t request, bool state) noexcept
    {
        using namespace formation_proof_support;
        Allocation* found = nullptr;
        for (std::size_t i = 0; i != allocationCount; ++i)
            if (allocations[i].request == request && allocations[i].state == state)
            { EXPECT (found == nullptr); found = &allocations[i]; }
        EXPECT (found != nullptr);
        return found;
    }
    static void live (const StateView& s, const Origin& origin, Context context) noexcept
    {
        using namespace formation_proof_support;
        const auto& m = origin.now;
        m.exactSites (s.raw); m.rawDiagnostics (s.raw); m.canonical (s.canonical);
        EXPECT (s.status == origin.status);
        EXPECT (s.context == context);
        EXPECT (s.references == 1 + m.n (Site::ArmWorker)+m.n (Site::RetainExternal)+m.n (Site::ArmReader)
                -m.n (Site::DropNonLast)-m.n (Site::DropLast));
        EXPECT (s.references <= 2);
        EXPECT (s.workerIssued == (m.n (Site::ArmWorker) != 0));
        EXPECT (s.retainedIssued == (m.n (Site::RetainExternal) != 0));
        EXPECT (s.readerIssued == (m.n (Site::ArmReader) != 0));
        EXPECT (s.parentLive == (m.n (Site::CaptureParents) != 0));
        EXPECT (s.metadataLive == (m.n (Site::CaptureMetadata) != 0));
        EXPECT (s.legacyEnteredContribution == 0);
        EXPECT (s.mapCapacity == (m.n (Site::MapInstall) != 0 ? m.mapRows : 0));
        EXPECT (s.mapConstructed == m.n (Site::MapStore));
        EXPECT ((s.mapIdentity != nullptr) == (m.n (Site::MapInstall) != 0));
        std::array<U64, phaseCount> remaining {};
        for (const auto& slice : s.fixed)
        {
            EXPECT (static_cast<std::size_t> (slice.phase) < phaseCount);
            remaining[static_cast<std::size_t> (slice.phase)] += slice.remaining;
        }
        for (std::size_t i = 0; i != roleCount; ++i)
        {
            const auto& role = s.roles[i];
            EXPECT (role.phase == Phase::Ownership);
            EXPECT (m.roleSpent[i] <= roleReservations[i]);
            EXPECT (role.remaining == (m.roleRetired[i] ? 0 : roleReservations[i] - m.roleSpent[i]));
            remaining[static_cast<std::size_t> (role.phase)] += role.remaining;
        }
        for (std::size_t p = 0; p != phaseCount; ++p)
            EXPECT (remaining[p] + s.canonical.terminal[p] == s.canonical.reserved[p]);
        EXPECT (s.fixed[0].phase == Phase::Formation);
        EXPECT (s.fixed[0].remaining == s.canonical.reserved[0]);
        EXPECT (s.fixed[1].phase == Phase::Capture);
        EXPECT (s.fixed[2].phase == Phase::Cleanup && s.fixed[2].remaining == 515);
        EXPECT (s.fixed[3].phase == Phase::Cleanup);
        EXPECT (s.fixed[4].phase == Phase::Cleanup);
        EXPECT (s.fixed[3].remaining == (m.n (Site::ReserveInstall) != 0 ? 116 : 0));
        EXPECT (s.fixed[4].remaining == (m.n (Site::ReserveInstall) != 0 ? 107 : 0));
        U64 checkSpent = 0, variableSpent = 0, ownershipTerminal = 0;
        for (std::size_t i = 25; i <= 32; ++i) checkSpent += m.sites[i] * prices[i];
        for (std::size_t i = 79; i <= 81; ++i) checkSpent += m.sites[i] * prices[i];
        for (std::size_t i = 33; i <= 76; ++i) variableSpent += m.sites[i] * prices[i];
        EXPECT (checkSpent <= 1145+3*d);
        EXPECT (s.fixed[1].remaining == 1145+3*d-checkSpent);
        for (std::size_t i = 0; i != roleCount; ++i)
            if (m.roleRetired[i]) ownershipTerminal += roleReservations[i]-m.roleSpent[i];
        const U64 variableTerminal = m.n (Site::ReserveInstall) != 0
            ? m.admittedVariable-223-variableSpent : 0;
        EXPECT (s.canonical.terminal[0] == 0);
        EXPECT (s.canonical.terminal[1] == variableTerminal);
        EXPECT (s.canonical.terminal[2] == ownershipTerminal);
        EXPECT (s.canonical.terminal[3] == 0 && s.canonical.terminal[4] == 0 && s.canonical.terminal[5] == 0);
        auto* record = allocationFor (origin.request, true);
        if (record != nullptr)
        {
            EXPECT (record->identity == s.allocationIdentity);
            EXPECT (record->bytes == Access::stateBytes);
            EXPECT (! record->freed && ! record->captured);
        }
    }
    static void failedRoot (const RootCensus& root, const Expected& m, bool handedOff) noexcept
    {
        m.exactSites (root.raw); m.rawDiagnostics (root.raw);
        EXPECT (root.actual == m.actual());
        EXPECT (root.unused == formation_proof_support::rootReservation - m.actual());
        EXPECT (root.handedOff == handedOff);
    }
    static void current (const ApplicationRequests& app, const Origin& origin,
                         Context context = Context::Control) noexcept
    {
        const auto owner = checkedCurrent (app);
        EXPECT (owner.present && owner.hasState == origin.physical);
        EXPECT (owner.armed == (origin.now.n (Site::ArmWorker) != 0 && origin.now.n (Site::AbortWorker) == 0));
        EXPECT (owner.terminalAcquired == (origin.now.n (Site::AcquireWorker) != 0));
        origin.history.receipt (owner.historicalReceipt, origin.status, origin.physical);
        if (origin.physical) live (owner.liveState, origin, context);
        else failedRoot (owner.historicalOrFailedRoot, origin.now, false);
    }
    static ApplicationRequests* initial (AppStorage& storage, Fixture& fixture, CaptureCase c,
                                         Origin& origin, std::size_t failOrdinal = 0,
                                         bool alias = false) noexcept
    {
        using namespace formation_proof_support;
        const U64 beforeId = fixture.nextId, beforeVersion = fixture.version;
        beginRequest (failOrdinal);
        auto* app = ::new (static_cast<void*> (storage.bytes)) ApplicationRequests (
            fixture.project, fixture.clipId, fixture.original, fixture.ready,
            fixture.nextId, alias ? fixture.nextId : fixture.version, fixture.generation);
        endRequest();
        origin = { captured (fixture, c, true), {}, c.status,
                   ! c.identityRefused && ! c.stateAllocationFailed, requestNumber };
        if (origin.physical)
        {
            origin.now.roleEvent (Role::Control, Site::OwnerAdopt); origin.now.snapshot();
            origin.history = origin.now;
            origin.now.roleEvent (Role::Control, Site::OwnerConstructed);
        }
        else
        {
            origin.now.event (Site::RootFailedAdopt); origin.now.failedSnapshot();
            origin.history = origin.now; origin.now.event (Site::RootFailedConstructed);
        }
        const bool issued = c.counter == Site::RootCounterIssue;
        EXPECT (fixture.nextId == beforeId + (issued ? 1 : 0));
        EXPECT (fixture.version == beforeVersion + (issued ? 1 : 0));
        const auto session = checkedSession (*app);
        EXPECT (session.current == 0 && session.retiring == 1 && ! session.hasRetiring);
        EXPECT (session.live[0] && ! session.live[1]);
        current (*app, origin);
        requestTrace (origin, failOrdinal);
        return app;
    }
    static void final (const Origin& origin) noexcept
    {
        auto* state = allocationFor (origin.request, true);
        if (state == nullptr) return;
        EXPECT (state->freed && state->captured && state->freeForwarded);
        origin.now.exactSites (state->finalRaw); origin.now.rawDiagnostics (state->finalRaw);
        origin.now.canonical (state->finalCanonical, true);
        EXPECT (state->finalRaw.currentBytes == 0);
        if (origin.now.n (Site::MapInstall) != 0)
        {
            auto* map = allocationFor (origin.request, false);
            if (map != nullptr)
            {
                EXPECT (map->freed && map->freeForwarded && ! map->captured);
                EXPECT (map->bytes == origin.now.mapRows * sizeof (engine::TempoChange));
                EXPECT (map->releaseOrder < state->releaseOrder);
            }
        }
    }
    static void requestTrace (const Origin& origin, std::size_t injectedOrdinal = 0) noexcept
    {
        using namespace formation_proof_support;
        EXPECT (origin.request != 0 && origin.request <= requestNumber);
        if (origin.request == 0 || origin.request > requestNumber) return;
        const auto& trace = requests[origin.request-1];
        const U64 expected = origin.now.n (Site::RootAllocate)+origin.now.n (Site::MapAllocate);
        EXPECT (trace.count == expected);
        EXPECT (trace.successes == origin.now.n (Site::RootObject)+origin.now.n (Site::MapInstall));
        EXPECT (trace.injectedFailures == (injectedOrdinal != 0 ? 1u : 0u));
        if (expected != 0) EXPECT (trace.sizes[0] == Access::stateBytes);
        if (expected == 2) EXPECT (trace.sizes[1] == origin.now.mapRows*sizeof (engine::TempoChange));
        std::size_t records = 0;
        for (std::size_t i = 0; i != allocationCount; ++i)
            if (allocations[i].request == origin.request)
            { ++records; EXPECT (allocations[i].ordinal == (allocations[i].state ? 1u : 2u)); }
        EXPECT (records == trace.successes);
    }
    static void destroy (AppStorage& storage, ApplicationRequests* app, Origin& origin, bool last = true) noexcept
    {
        // Save genuine live slot identity before destroying the App in still-owned
        // byte-array storage. No typed App/owner access occurs afterwards.
        const auto session = checkedSession (*app);
        EXPECT (! session.hasRetiring);
        current (*app, origin); // Actual shape/flags/context before any destructor.
        const auto owner = checkedCurrent (*app);
        EXPECT (! owner.armed || owner.terminalAcquired);
        const auto slot = session.current;
        if (origin.physical)
        {
            origin.now.roleEvent (Role::Control, Site::SessionDestroyCheck);
            origin.now.roleEvent (Role::Control, Site::SessionDestroyFinish);
            const bool map = origin.now.n (Site::MapInstall) != 0;
            const bool parents = origin.now.n (Site::CaptureParents) != 0;
            origin.now.controlDestroyed (last, map, parents, origin.now.n (Site::RetainExternal) != 0);
        }
        else
        {
            origin.now.event (Site::RootFailedSessionCheck); origin.now.event (Site::RootFailedSessionFinish);
            origin.now.event (Site::RootFailedSlotErase); origin.now.event (Site::RootFailedDestroy);
        }
        app->~ApplicationRequests();
        if (origin.physical) { if (last) final (origin); }
        else
        {
            EXPECT (slot < 2);
            if (slot >= 2) return;
            std::array<std::byte, Access::failedRootBytes> bytes;
            std::memcpy (bytes.data(), storage.bytes + Access::failedRootOffsetBySlot[slot], bytes.size());
            RootCensus actual {};
            std::memcpy (&actual, bytes.data(), bytes.size());
            failedRoot (actual, origin.now, true);
        }
    }
    static void capturedValues (ApplicationRequests& app, Fixture& fixture, const Origin& origin,
                                const CaptureCase& c)
    {
        if (origin.now.n (Site::CaptureMetadata) == 0) return;
        const auto slot = checkedSession (app).current;
        const U64 expectedRows = origin.now.n (Site::MapStore);
        EXPECT (expectedRows <= c.mapCopies);
        captureState (app, slot, expectedRows);
        EXPECT (c.clipVisits != 0 && c.clipVisits <= fixture.project.clips.size());
        EXPECT (c.assetVisits != 0 && c.assetVisits <= fixture.project.assets.size());
        EXPECT (expectedRows <= fixture.project.tempoMap.size());
        const auto expectedProject = fixture.project.id;
        const auto expectedClip = fixture.project.clips[static_cast<std::size_t> (c.clipVisits-1)];
        const auto expectedAsset = fixture.project.assets[static_cast<std::size_t> (c.assetVisits-1)];
        const auto expectedRate = fixture.project.sampleRate;
        const auto rows = fixture.project.tempoMap;
        const auto verify = [&] {
            captureState (app, slot, expectedRows);
            const auto actual = Access::captured (app, slot);
            EXPECT (actual.metadata.projectId == expectedProject);
            EXPECT (actual.metadata.clip == expectedClip);
            EXPECT (actual.metadata.asset == expectedAsset);
            EXPECT (actual.metadata.projectRate == expectedRate);
            EXPECT (actual.metadata.requestId == fixture.nextId-1);
            EXPECT (actual.metadata.requestVersion == fixture.version);
            EXPECT (actual.metadata.attachGeneration == fixture.generation);
            EXPECT (actual.originalIdentity == fixture.original.get());
            EXPECT (actual.readyIdentity == fixture.ready.get());
            for (std::size_t i = 0; i != expectedRows; ++i)
            {
                captureState (app, slot, i+1);
                EXPECT (Access::mapRow (app, slot, i) == rows[i]);
            }
        };
        verify();
        // CapturedIntent has returned. Real captured fixed/map values must survive
        // subsequent owner-side Project changes without borrowing its vectors.
        fixture.project.id = {};
        fixture.project.clips.clear(); fixture.project.assets.clear(); fixture.project.tempoMap.clear();
        fixture.project.sampleRate = { 44100.0 };
        verify();
    }
    static void initialCase (const char* name, Fixture& fixture, CaptureCase c = {},
                             std::size_t fail = 0, bool alias = false)
    {
        using namespace formation_proof_support;
        currentCase = name;
        const auto originalUses = fixture.original.use_count(), readyUses = fixture.ready.use_count();
        AppStorage storage;
        Origin origin {};
        auto* app = initial (storage, fixture, c, origin, fail, alias);
        const bool parents = origin.now.n (Site::CaptureParents) != 0;
        if (fixture.original)
            EXPECT (fixture.original.use_count() == originalUses
                    + (parents ? 1 + (fixture.original == fixture.ready ? 1 : 0) : 0));
        if (fixture.ready)
            EXPECT (fixture.ready.use_count() == readyUses
                    + (parents ? 1 + (fixture.original == fixture.ready ? 1 : 0) : 0));
        capturedValues (*app, fixture, origin, c);
        current (*app, origin);
        destroy (storage, app, origin);
        EXPECT (fixture.original.use_count() == originalUses);
        EXPECT (fixture.ready.use_count() == readyUses);
    }
    static void initialCases()
    {
        using namespace formation_proof_support;
        { Fixture f; initialCase ("valid full map", f); }
        { Fixture f; f.project.tempoMap.clear(); CaptureCase c; c.mapVisits = c.mapCopies = 0;
          initialCase ("valid empty map", f, c); }
        { Fixture f; f.project.tempoMap.resize (1); CaptureCase c; c.mapVisits = c.mapCopies = 1;
          initialCase ("valid one-row map", f, c); }
        { Fixture f; f.ready.reset(); initialCase ("valid source with empty ready handle", f); }
        { Fixture f; CaptureCase c; c.status = CaptureStatus::AllocationFailed; c.stateAllocationFailed = true;
          initialCase ("actual State allocation failure", f, c, 1); }
        { Fixture f; CaptureCase c; c.status = CaptureStatus::AllocationFailed; c.refusal = Site::MapAllocateFailed;
          c.mapVisits = c.mapCopies = 0; initialCase ("actual map allocation failure", f, c, 2); }
        for (unsigned kind = 0; kind != 8; ++kind)
        {
            Fixture f; CaptureCase c; c.status = CaptureStatus::InvalidIdentity; c.identityRefused = true;
            if (kind == 0) f.project.id = {};
            if (kind == 1) f.clipId = {};
            if (kind == 2) f.generation = 0;
            if (kind == 3) c.counter = Site::RootCounterAlias;
            if (kind == 4) { f.nextId = 0; c.counter = Site::RootCounterExhausted; }
            if (kind == 5) { f.nextId = std::numeric_limits<U64>::max(); c.counter = Site::RootCounterExhausted; }
            if (kind == 6) { f.version = std::numeric_limits<U64>::max(); c.counter = Site::RootCounterExhausted; }
            if (kind == 7) { f.nextId = f.version = std::numeric_limits<U64>::max()-1;
                            c.status = CaptureStatus::RequestCaptured; c.identityRefused = false; }
            initialCase ("identity and monotonic counter boundary", f, c, 0, kind == 3);
        }
        for (const U64 value : { U64 { 0 }, U64 { 7 }, std::numeric_limits<U64>::max()-1,
                                std::numeric_limits<U64>::max() })
        {
            Fixture f; f.nextId = value;
            CaptureCase c; c.counter = Site::RootCounterAlias;
            c.status = CaptureStatus::InvalidIdentity; c.identityRefused = true;
            initialCase ("aliased counter unchanged at every boundary", f, c, 0, true);
        }
        for (unsigned kind = 0; kind != 4; ++kind)
        {
            Fixture f; CaptureCase c;
            if (kind < 2)
            {
                c.status = CaptureStatus::MissingClip; c.clipFound = false;
                if (kind == 0) { f.project.clips.clear(); c.clipVisits = 0; }
                else f.project.clips[0].id = engine::EntityId::fromBigEndianParts (99, 99);
            }
            else
            {
                c.status = CaptureStatus::MissingAsset; c.assetFound = false;
                if (kind == 2) { f.project.assets.clear(); c.assetVisits = 0; }
                else f.project.assets[0].id = engine::EntityId::fromBigEndianParts (99, 98);
            }
            initialCase ("empty and exhausted lookup", f, c);
        }
        { Fixture f; CaptureCase c;
          auto otherClip = f.project.clips[0]; otherClip.id = engine::EntityId::fromBigEndianParts (99, 97);
          f.project.clips.insert (f.project.clips.begin(), 2, otherClip); c.clipVisits = 3;
          auto otherAsset = f.project.assets[0]; otherAsset.id = engine::EntityId::fromBigEndianParts (99, 96);
          f.project.assets.insert (f.project.assets.begin(), 3, otherAsset); c.assetVisits = 4;
          initialCase ("full fixed-ID comparison and last matching rows", f, c); }
        { Fixture f; CaptureCase c; c.status = CaptureStatus::MissingClip; c.clipFound = false;
          auto wrong = f.project.clips[0]; wrong.id = engine::EntityId::fromBigEndianParts (91, 1);
          f.project.clips.assign (3, wrong); c.clipVisits = 3;
          initialCase ("many exhausted Clip rows", f, c); }
        { Fixture f; CaptureCase c; c.status = CaptureStatus::MissingAsset; c.assetFound = false;
          auto wrong = f.project.assets[0]; wrong.id = engine::EntityId::fromBigEndianParts (91, 2);
          f.project.assets.assign (4, wrong); c.assetVisits = 4;
          initialCase ("many exhausted Asset rows", f, c); }
        for (unsigned kind = 0; kind != 11; ++kind)
        {
            Fixture f; CaptureCase c; c.status = CaptureStatus::InvalidSource;
            c.refusal = Site::SourceLayoutRefused;
            auto& clip = f.project.clips[0]; auto& asset = f.project.assets[0];
            if (kind == 0) { f.original.reset(); c.refusal = Site::SourceMissing; }
            if (kind == 1) clip.timeBase = engine::TimeBase::SampleLocked;
            if (kind == 2) clip.srcLen = 0;
            if (kind == 3) asset.channels = 0;
            if (kind == 4) asset.channels = 3;
            if (kind == 5) asset.channels = 1;
            if (kind == 6) asset.frames = 7;
            if (kind == 7) clip.srcOffset = 9;
            if (kind == 8) { clip.srcLen = 8; c.refusal = Site::SourceExtentRefused; }
            if (kind == 9)
            {
                auto huge = std::make_shared<engine::AssetSamples>(); huge->channels = 2;
                huge->frames = std::numeric_limits<U64>::max();
                f.original = huge; asset.frames = huge->frames; c.refusal = Site::SourceExtentRefused;
            }
            if (kind == 10)
            {
                auto shortSource = std::make_shared<engine::AssetSamples>(); shortSource->channels = 2;
                shortSource->frames = 8; shortSource->interleaved.resize (15);
                f.original = shortSource; c.refusal = Site::SourceProductRefused;
            }
            initialCase ("source layout extent and product refusal", f, c);
        }
        for (unsigned row = 0; row != 3; ++row)
            for (unsigned reason = 0; reason != 6; ++reason)
            {
                Fixture f; CaptureCase c; c.status = CaptureStatus::InvalidMap;
                c.refusal = Site::MapInvalid; c.mapVisits = row+1; c.mapCopies = row;
                auto& invalid = f.project.tempoMap[row];
                if (reason == 0) invalid.tick = row == 0 ? 1 : f.project.tempoMap[row-1].tick;
                if (reason == 1) invalid.bpm = 0;
                if (reason == 2) invalid.bpm = -1;
                if (reason == 3) invalid.bpm = std::numeric_limits<double>::quiet_NaN();
                if (reason == 4) invalid.bpm = std::numeric_limits<double>::infinity();
                if (reason == 5) invalid.curveToNext = static_cast<engine::TempoCurve> (255);
                initialCase ("bad map every prefix and invalid property", f, c);
            }
        { Fixture f; f.project.tempoMap[0].bpm = std::numeric_limits<double>::max();
          initialCase ("finite positive map upper endpoint", f); }
        { Fixture f; CaptureCase c; c.status = CaptureStatus::ResourceLimit;
          c.refusal = Site::CaptureReserveRefused;
          const std::size_t rows = static_cast<std::size_t> (workCap/(970+2*d)+1);
          f.project.clips.resize (rows);
          initialCase ("real finite oversized work reservation refuses before scans", f, c); }
        // Real nearest-admissible count and the next count at this map size.
        // These do not fake vector size/capacity, lower initial authority or
        // assert an impossible exact arithmetic equality at the public cap.
        const U64 overhead = rootReservation+3148+d+3*(608+d);
        EXPECT (overhead < workCap);
        if constexpr (overhead < workCap)
        {
            const U64 searchRows = (workCap-overhead)/(970+2*d);
            EXPECT (searchRows > 1 && searchRows < workCap/970+1);
            if constexpr (searchRows > 1 && searchRows < workCap/970+1)
                for (unsigned extra = 0; extra != 2; ++extra)
                {
                    Fixture f; CaptureCase c;
                    f.project.clips.resize (static_cast<std::size_t> (searchRows-1+extra));
                    if (extra != 0) { c.status = CaptureStatus::ResourceLimit; c.refusal = Site::CaptureReserveRefused; }
                    initialCase ("largest admitted count then next real count refuses", f, c);
                }
        }
    }

    using CellBytes = std::array<std::byte, Access::controlBytes>;
    static CellBytes unstartedCell (const AppStorage& storage, const ApplicationRequests& app,
                                    std::size_t slot) noexcept
    {
        // Caller proves this is a LIVE owner cell and no receiver has started.
        // This copies only outer Control storage, never a handed-off State.
        EXPECT (slot < 2);
        const auto session = checkedSession (app);
        EXPECT (session.live[slot]);
        CellBytes bytes {};
        if (slot >= 2) return bytes;
        const auto offset = slot == 0 ? Access::control0Offset : Access::control1Offset;
        std::memcpy (bytes.data(), storage.bytes+offset, bytes.size());
        return bytes;
    }
    static void sameCell (const CellBytes& a, const CellBytes& b) noexcept
    {
        // Compare saved object representations as bytes, without interpreting
        // padding as initialized fields or constructing a Control object.
        EXPECT (std::memcmp (a.data(), b.data(), a.size()) == 0);
    }
    static void sameRaw (const Raw& a, const Raw& b) noexcept
    {
        for (std::size_t i = 0; i != siteCount; ++i) EXPECT (a.sites[i] == b.sites[i]);
        EXPECT (a.loopEntries == b.loopEntries && a.loopConditions == b.loopConditions);
        EXPECT (a.loopBodies == b.loopBodies && a.loopIncrements == b.loopIncrements);
        EXPECT (a.loopFinalFalse == b.loopFinalFalse && a.loopBreaks == b.loopBreaks && a.loopReturns == b.loopReturns);
        EXPECT (a.allocationAttempts == b.allocationAttempts && a.allocationSuccesses == b.allocationSuccesses);
        EXPECT (a.allocationReleases == b.allocationReleases && a.requestedBytes == b.requestedBytes);
        EXPECT (a.currentBytes == b.currentBytes && a.peakBytes == b.peakBytes);
        EXPECT (a.copiedMapRows == b.copiedMapRows && a.searchRows == b.searchRows);
        EXPECT (a.ownerConstructions == b.ownerConstructions && a.ownerDestructions == b.ownerDestructions);
        EXPECT (a.referenceRetains == b.referenceRetains && a.referenceDrops == b.referenceDrops);
        EXPECT (a.terminalStores == b.terminalStores && a.terminalLoads == b.terminalLoads);
    }
    static void sameState (const StateView& a, const StateView& b) noexcept
    {
        EXPECT (a.allocationIdentity == b.allocationIdentity);
        sameRaw (a.raw, b.raw);
        for (std::size_t p = 0; p != phaseCount; ++p)
        {
            EXPECT (a.canonical.actual[p] == b.canonical.actual[p]);
            EXPECT (a.canonical.reserved[p] == b.canonical.reserved[p]);
            EXPECT (a.canonical.terminal[p] == b.canonical.terminal[p]);
        }
        for (std::size_t i = 0; i != 5; ++i)
            EXPECT (a.fixed[i].phase == b.fixed[i].phase && a.fixed[i].remaining == b.fixed[i].remaining);
        for (std::size_t i = 0; i != roleCount; ++i)
            EXPECT (a.roles[i].phase == b.roles[i].phase && a.roles[i].remaining == b.roles[i].remaining);
        EXPECT (a.references == b.references && a.context == b.context);
        EXPECT (a.workerIssued == b.workerIssued && a.retainedIssued == b.retainedIssued && a.readerIssued == b.readerIssued);
        EXPECT (a.parentLive == b.parentLive && a.metadataLive == b.metadataLive && a.status == b.status);
        EXPECT (a.mapIdentity == b.mapIdentity && a.mapCapacity == b.mapCapacity && a.mapConstructed == b.mapConstructed);
        EXPECT (a.legacyEnteredContribution == b.legacyEnteredContribution);
    }
    static void sameCaptured (const Access::CapturedObservation& a, const Access::CapturedObservation& b) noexcept
    {
        EXPECT (a.metadata.projectId == b.metadata.projectId && a.metadata.clip == b.metadata.clip);
        EXPECT (a.metadata.asset == b.metadata.asset && a.metadata.projectRate == b.metadata.projectRate);
        EXPECT (a.metadata.requestId == b.metadata.requestId && a.metadata.requestVersion == b.metadata.requestVersion);
        EXPECT (a.metadata.attachGeneration == b.metadata.attachGeneration);
        EXPECT (a.originalIdentity == b.originalIdentity && a.readyIdentity == b.readyIdentity);
    }
    static Origin next (ApplicationRequests& app, Fixture& fixture, CaptureCase c, Origin& old,
                        bool keepRunning = false, std::size_t fail = 0, bool alias = false) noexcept
    {
        using namespace formation_proof_support;
        const U64 beforeId = fixture.nextId, beforeVersion = fixture.version;
        const auto before = checkedSession (app); // Never old handed-off State.
        if (keepRunning) EXPECT (! before.hasRetiring);
        else current (app, old); // Current pending origin is quiescent; retiring is untouched.
        beginRequest (fail);
        auto readout = app.captureNext (fixture.project, fixture.clipId, fixture.original, fixture.ready,
                                       fixture.nextId, alias ? fixture.nextId : fixture.version, fixture.generation);
        endRequest();
        Origin out { captured (fixture, c, false), {}, c.status,
                     ! c.identityRefused && ! c.stateAllocationFailed, requestNumber };
        const bool issued = c.counter == Site::RootCounterIssue;
        EXPECT (fixture.nextId == beforeId+(issued ? 1 : 0));
        EXPECT (fixture.version == beforeVersion+(issued ? 1 : 0));
        EXPECT (readout.status() == c.status);
        EXPECT (Access::historicalReceipt (readout).physicalState == out.physical);
        if (! out.physical)
        {
            out.now.event (Site::RootFailedReadout); out.now.event (Site::RootFailedDiscard);
            out.now.failedSnapshot();
            out.now.receipt (Access::historicalReceipt (readout), c.status, false);
            EXPECT (readout.disposition() == Disposition::PreservedOnFailure);
        }
        else
        {
            out.now.event (Site::NewFailureTest);
            if (c.status != CaptureStatus::RequestCaptured)
            {
                out.now.roleEvent (Role::Control, Site::AbandonNewState);
                out.now.event (Site::ReadoutCommit); out.now.snapshot();
                out.now.receipt (Access::historicalReceipt (readout), c.status, true);
                EXPECT (readout.disposition() == Disposition::PreservedOnFailure);
                out.now.drop (Role::Control, true, out.now.n (Site::MapInstall) != 0,
                              out.now.n (Site::CaptureParents) != 0);
                final (out); // The readout preceded cleanup; the actual free proves the later delta.
            }
            else
            {
                out.now.event (Site::SessionSelect);
                if (keepRunning)
                {
                    out.now.event (Site::SessionKeepRunning);
                    EXPECT (readout.disposition() == Disposition::KeptRetiring);
                }
                else
                {
                    out.now.event (Site::SessionReusePending);
                    EXPECT (readout.disposition() == Disposition::ReplacedPending);
                    if (old.physical)
                    {
                        old.now.controlDestroyed (true, old.now.n (Site::MapInstall) != 0,
                                                  old.now.n (Site::CaptureParents) != 0);
                        final (old);
                    }
                    else
                    {
                        // This old failed tail is overwritten by placement. Its
                        // final bytes are not observable here; the exact common
                        // failed destruction bodies are covered by destroy().
                        old.now.event (Site::RootFailedSlotErase); old.now.event (Site::RootFailedDestroy);
                    }
                }
                out.now.event (Site::SessionInstall); out.now.event (Site::ReadoutCommit);
                out.now.roleEvent (Role::Control, Site::OwnerAdopt); out.now.snapshot();
                out.history = out.now;
                out.now.roleEvent (Role::Control, Site::OwnerConstructed); out.now.snapshot();
                out.now.receipt (Access::historicalReceipt (readout), c.status, true);
                current (app, out);
                const auto after = checkedSession (app);
                EXPECT (after.current == (keepRunning ? before.current ^ std::size_t { 1 } : before.current));
                EXPECT (after.live[after.current]);
                EXPECT (after.hasRetiring == (keepRunning || before.hasRetiring));
                if (keepRunning) EXPECT (after.retiring == before.current && after.live[before.current]);
            }
        }
        if (c.status != CaptureStatus::RequestCaptured)
        {
            const auto after = checkedSession (app);
            EXPECT (after.current == before.current && after.retiring == before.retiring);
            EXPECT (after.hasRetiring == before.hasRetiring);
            EXPECT (after.live[0] == before.live[0] && after.live[1] == before.live[1]);
        }
        requestTrace (out, fail);
        return out;
    }
    class PausedWorker final
    {
        ControlRequestOwner& owner_;
        std::atomic<bool> cancel_, start_ { false };
        std::thread worker_;
    public:
        PausedWorker (ControlRequestOwner& owner, bool cancelled)
            : owner_ (owner), cancel_ (cancelled), worker_ ([this] {
                while (! start_.load (std::memory_order_acquire)) std::this_thread::yield();
                ApplicationRequests::workerRequestCheck (owner_, cancel_);
            }) {}
        PausedWorker (const PausedWorker&) = delete;
        void finish()
        {
            if (worker_.joinable())
            {
                start_.store (true, std::memory_order_release);
                try { worker_.join(); }
                catch (...)
                {
                    std::fputs ("FAIL formation whole proof: Worker join exception; no proof credit\n", stderr);
                    std::exit (2); // Never unwind a still-joinable std::thread.
                }
            }
        }
        ~PausedWorker() noexcept
        {
            if (worker_.joinable())
            {
                // An unexpected exception must not START a receiver on unwind,
                // destroy its stable owner, or invoke std::thread::terminate.
                std::fputs ("FAIL formation whole proof: unfinished Worker during harness unwind; no proof credit\n", stderr);
                std::exit (2);
            }
        }
    };
    static bool runWorker (ControlRequestOwner& owner, bool cancelled)
    {
        std::optional<PausedWorker> worker;
        try { worker.emplace (owner, cancelled); }
        catch (...) { return false; } // Caller restores the genuine unstarted arm before FAIL.
        worker->finish(); return true;
        // A join alone grants no production authority; callers still acquire.
    }
    static void abortFailedWorkerLaunch (ApplicationRequests& app, Origin& origin) noexcept
    {
        // Only used when construction proved no receiver started and this
        // original owner is STILL current. Never abort a retiring reference.
        app.abortUnstartedCurrentWorker();
        origin.now.roleEvent (Role::Control, Site::AbortWorker);
        origin.now.drop (Role::Worker, false, false, false);
    }
    static void directWorkerCases()
    {
        using namespace formation_proof_support;
        for (bool cancelled : { false, true })
        {
            currentCase = cancelled ? "cancelled Worker then real direct acquire" : "normal Worker then real direct acquire";
            Fixture fixture; AppStorage storage; Origin origin {};
            auto* app = initial (storage, fixture, {}, origin);
            readyForWorker (*app, origin);
            auto& workerOwner = app->armCurrentWorker();
            origin.now.roleEvent (Role::Control, Site::ArmWorker);
            if (! runWorker (workerOwner, cancelled))
            { abortFailedWorkerLaunch (*app, origin); destroy (storage, app, origin); EXPECT (false); return; }
            origin.now.workerReturned (cancelled);
            app->acquireCurrentWorkerAfterTerminal();
            origin.now.roleEvent (Role::Control, Site::AcquireWorker);
            origin.now.roleEvent (Role::Control, Site::AcquireCurrentCaller);
            current (*app, origin);
            const auto observed = checkedCurrent (*app);
            EXPECT (observed.armed && observed.terminalAcquired);
            EXPECT (origin.now.n (Site::AcquireCurrentCaller) == 1 && origin.now.n (Site::DrainRetiring) == 0);
            destroy (storage, app, origin); requestTrace (origin);
        }
        {
            currentCase = "unstarted Worker abort retains one-use issuance and retires Worker schedule";
            Fixture fixture; AppStorage storage; Origin origin {};
            auto* app = initial (storage, fixture, {}, origin);
            readyForWorker (*app, origin);
            app->armCurrentWorker(); origin.now.roleEvent (Role::Control, Site::ArmWorker);
            app->abortUnstartedCurrentWorker(); origin.now.roleEvent (Role::Control, Site::AbortWorker);
            origin.now.drop (Role::Worker, false, false, false);
            current (*app, origin);
            const auto observed = checkedCurrent (*app);
            EXPECT (! observed.armed && ! observed.terminalAcquired);
            destroy (storage, app, origin);
        }
    }
    static void pendingCases (bool sessionDestructor)
    {
        using namespace formation_proof_support;
        currentCase = sessionDestructor ? "retiring Worker acquired by real session destructor"
                                        : "repeated pending replacement and failure preservation then real drain";
        Fixture fixture; AppStorage storage; Origin old {};
        auto* app = initial (storage, fixture, {}, old);
        readyForWorker (*app, old);
        const auto oldIndex = checkedSession (*app).current;
        const void* const oldStateIdentity = checkedCurrent (*app).liveState.allocationIdentity;
        auto& oldOwner = app->armCurrentWorker(); old.now.roleEvent (Role::Control, Site::ArmWorker);
        std::optional<PausedWorker> workerTask;
        try { workerTask.emplace (oldOwner, false); }
        catch (...)
        {
            abortFailedWorkerLaunch (*app, old);
            destroy (storage, app, old); EXPECT (false); return;
        }
        // The OS thread exists, but its harness gate prevents ANY production
        // receiver entry or old Control/State access during capture/replacement.
        const auto oldCell = unstartedCell (storage, *app, oldIndex);
        Origin pending = next (*app, fixture, {}, old, true);
        sameCell (unstartedCell (storage, *app, oldIndex), oldCell);
        if (! sessionDestructor)
        {
            for (unsigned replacement = 0; replacement != 3; ++replacement)
            {
                Origin newer = next (*app, fixture, {}, pending);
                pending = newer;
                sameCell (unstartedCell (storage, *app, oldIndex), oldCell);
            }
            const auto session = checkedSession (*app);
            const auto currentBefore = checkedCurrent (*app);
            const auto pendingCell = unstartedCell (storage, *app, session.current);
            captureState (*app, session.current, 3);
            const auto capturedBefore = Access::captured (*app, session.current);
            // Fixed schema here is exactly3rows. Do not allocate/copy a vector
            // while the old Worker is armed and its OS thread is paused.
            EXPECT (fixture.project.tempoMap.size() == 3);
            const std::array<engine::TempoChange, 3> mapBefore {
                fixture.project.tempoMap[0], fixture.project.tempoMap[1], fixture.project.tempoMap[2] };
            const auto usesBefore = fixture.original.use_count();
            for (unsigned kind = 0; kind != 7; ++kind)
            {
                CaptureCase c;
                const auto savedProjectId = fixture.project.id;
                const double savedBpm = fixture.project.tempoMap[1].bpm;
                if (kind == 0) { fixture.project.id = {}; c.identityRefused = true; c.status = CaptureStatus::InvalidIdentity; }
                if (kind == 1) { c.stateAllocationFailed = true; c.status = CaptureStatus::AllocationFailed; }
                if (kind == 2) { c.refusal = Site::MapAllocateFailed; c.status = CaptureStatus::AllocationFailed; c.mapVisits = c.mapCopies = 0; }
                if (kind == 3) { fixture.project.tempoMap[1].bpm = 0; c.refusal = Site::MapInvalid;
                                c.status = CaptureStatus::InvalidMap; c.mapVisits = 2; c.mapCopies = 1; }
                if (kind == 4) { fixture.project.clips[0].srcLen = 8; c.refusal = Site::SourceExtentRefused;
                                c.status = CaptureStatus::InvalidSource; }
                if (kind == 5) { c.counter = Site::RootCounterAlias; c.identityRefused = true; c.status = CaptureStatus::InvalidIdentity; }
                if (kind == 6) { fixture.version = std::numeric_limits<U64>::max(); c.counter = Site::RootCounterExhausted;
                                c.identityRefused = true; c.status = CaptureStatus::InvalidIdentity; }
                const auto failed = next (*app, fixture, c, pending, false, kind == 1 ? 1 : kind == 2 ? 2 : 0, kind == 5);
                EXPECT (failed.status == c.status);
                fixture.project.id = savedProjectId; fixture.project.tempoMap[1].bpm = savedBpm;
                if (kind == 4) fixture.project.clips[0].srcLen = 4;
                // Exhaustion is last and the exclusive caller counter stays at
                // max; no test resets an exhausted application sequence for reuse.
                sameCell (unstartedCell (storage, *app, oldIndex), oldCell);
                sameCell (unstartedCell (storage, *app, session.current), pendingCell);
                const auto after = checkedCurrent (*app);
                sameState (currentBefore.liveState, after.liveState);
                captureState (*app, session.current, mapBefore.size());
                sameCaptured (capturedBefore, Access::captured (*app, session.current));
                for (std::size_t i = 0; i != mapBefore.size(); ++i)
                {
                    captureState (*app, session.current, i+1);
                    EXPECT (Access::mapRow (*app, session.current, i) == mapBefore[i]);
                }
                EXPECT (fixture.original.use_count() == usesBefore);
                current (*app, pending);
            }
        }
        // No old State observation occurred after ArmWorker. The real receiver
        // still consumes its original stable Control reference after replacements.
        const auto beforeFinish = checkedSession (*app);
        EXPECT (beforeFinish.hasRetiring && beforeFinish.retiring == oldIndex);
        current (*app, pending); // Only the still-quiescent new current origin.
        workerTask->finish(); old.now.workerReturned (false);
        if (sessionDestructor)
        {
            pending.now.roleEvent (Role::Control, Site::SessionDestroyCheck);
            pending.now.roleEvent (Role::Control, Site::SessionDestroyRetiring);
            old.now.roleEvent (Role::Control, Site::AcquireWorker);
            old.now.controlDestroyed (true, true, true);
            pending.now.roleEvent (Role::Control, Site::SessionDestroyFinish);
            pending.now.controlDestroyed (true, true, true);
            app->~ApplicationRequests(); // Actual production acquire precedes old destruction/capture.
            final (old); final (pending);
            EXPECT (old.now.n (Site::AcquireCurrentCaller) == 0 && old.now.n (Site::DrainRetiring) == 0);
            EXPECT (pending.now.n (Site::SessionDestroyRetiring) == 1);
        }
        else
        {
            app->drainRetiringAfterTerminal();
            old.now.roleEvent (Role::Control, Site::AcquireWorker);
            old.now.roleEvent (Role::Control, Site::DrainRetiring);
            old.now.controlDestroyed (true, true, true); final (old);
            const auto session = checkedSession (*app);
            EXPECT (! session.hasRetiring && ! session.live[oldIndex]);
            current (*app, pending);
            EXPECT (old.now.n (Site::AcquireCurrentCaller) == 0 && old.now.n (Site::DrainRetiring) == 1);
            // Draining permits the pending request's one Worker, with its own origin.
            readyForWorker (*app, pending);
            auto& worker = app->armCurrentWorker(); pending.now.roleEvent (Role::Control, Site::ArmWorker);
            if (! runWorker (worker, true))
            { abortFailedWorkerLaunch (*app, pending); destroy (storage, app, pending); EXPECT (false); return; }
            pending.now.workerReturned (true);
            app->acquireCurrentWorkerAfterTerminal();
            pending.now.roleEvent (Role::Control, Site::AcquireWorker);
            pending.now.roleEvent (Role::Control, Site::AcquireCurrentCaller);
            current (*app, pending); destroy (storage, app, pending);
        }
        auto* oldAllocation = allocationFor (old.request, true);
        if (oldAllocation != nullptr) EXPECT (oldAllocation->identity == oldStateIdentity);
    }
    static void retainedCases()
    {
        using namespace formation_proof_support;
        for (unsigned kind = 0; kind != 5; ++kind)
        {
            currentCase = "retained origin control detach/drop and normal/cancelled/aborted/absent Reader";
            Fixture fixture; AppStorage storage; Origin origin {};
            auto* app = initial (storage, fixture, {}, origin);
            readyForWorker (*app, origin);
            auto& worker = app->armCurrentWorker(); origin.now.roleEvent (Role::Control, Site::ArmWorker);
            if (! runWorker (worker, false))
            { abortFailedWorkerLaunch (*app, origin); destroy (storage, app, origin); EXPECT (false); return; }
            origin.now.workerReturned (false);
            app->acquireCurrentWorkerAfterTerminal();
            origin.now.roleEvent (Role::Control, Site::AcquireWorker);
            origin.now.roleEvent (Role::Control, Site::AcquireCurrentCaller);
            current (*app, origin); // Actual acquisition/context before retain.
            const auto beforeRetain = checkedCurrent (*app);
            EXPECT (beforeRetain.armed && beforeRetain.terminalAcquired);
            EXPECT (beforeRetain.hasState && ! beforeRetain.liveState.retainedIssued);
            {
                auto retained = app->retainCurrentAfterTerminal();
                origin.now.roleEvent (Role::Control, Site::RetainExternal);
                current (*app, origin);
                const auto initialRetained = Access::retained (retained);
                EXPECT (! initialRetained.armed && ! initialRetained.terminalAcquired);
                live (initialRetained.liveState, origin, Context::Control);
                if (kind != 4)
                {
                    destroy (storage, app, origin, false);
                    live (Access::retained (retained).liveState, origin, Context::DetachedControl);
                    if (kind < 3)
                    {
                        bool returned = false;
                        const auto beforeArm = Access::retained (retained);
                        EXPECT (! beforeArm.armed && ! beforeArm.terminalAcquired);
                        EXPECT (beforeArm.liveState.allocationIdentity != nullptr);
                        EXPECT (beforeArm.liveState.context == Context::DetachedControl);
                        EXPECT (! beforeArm.liveState.readerIssued && beforeArm.liveState.references == 1);
                        EXPECT (beforeArm.liveState.metadataLive && beforeArm.liveState.parentLive);
                        EXPECT (beforeArm.liveState.status == CaptureStatus::RequestCaptured);
                        retained.armReader(); origin.now.roleEvent (Role::Retained, Site::ArmReader);
                        if (kind == 2)
                        {
                            // No receiver was started, as the actual harness shows.
                            retained.abortUnstartedReader();
                            origin.now.roleEvent (Role::Retained, Site::AbortReader);
                            origin.now.drop (Role::Reader, false, false, false);
                        }
                        else
                        {
                            std::atomic<bool> cancel { kind == 1 };
                            std::optional<std::thread> reader;
                            try
                            {
                                reader.emplace ([&] { ApplicationRequests::readerRequestCheck (retained, cancel); });
                            }
                            catch (...)
                            {
                                retained.abortUnstartedReader();
                                origin.now.roleEvent (Role::Retained, Site::AbortReader);
                                origin.now.drop (Role::Reader, false, false, false);
                                EXPECT (false); // Restore actual detached context, then stop; no fake final credit.
                            }
                            if (reader.has_value())
                            {
                                try { reader->join(); }
                                catch (...)
                                {
                                    std::fputs ("FAIL formation whole proof: Reader join exception; no proof credit\n", stderr);
                                    std::exit (2);
                                }
                                returned = true;
                            }
                            if (returned)
                            {
                                origin.now.readerReturned (kind == 1);
                                retained.acquireReturnedReader(); // Required even after join.
                                origin.now.roleEvent (Role::Retained, Site::AcquireReader);
                            }
                        }
                        const auto observed = Access::retained (retained);
                        EXPECT (observed.armed == returned);
                        EXPECT (observed.terminalAcquired == returned);
                        live (observed.liveState, origin, Context::DetachedControl);
                    }
                    origin.now.roleEvent (Role::Retained, Site::RetainedDestroy);
                    origin.now.roleEvent (Role::Retained, Site::RetainedDropCaller);
                    origin.now.drop (Role::Retained, true, true, true);
                }
                else
                {
                    origin.now.roleEvent (Role::Retained, Site::RetainedDestroy);
                    origin.now.roleEvent (Role::Retained, Site::RetainedDropCaller);
                    origin.now.drop (Role::Retained, false, false, false);
                }
            } // Actual exactly-once retained destruction; no later typed owner access.
            if (kind == 4) { current (*app, origin); destroy (storage, app, origin); }
            else final (origin);
        }
    }
    static void parentLastOwnerCase()
    {
        using namespace formation_proof_support;
        currentCase = "last real parent owners survive map release then ready/original/State free";
        Fixture fixture;
        auto* original = new engine::AssetSamples;
        original->channels = 2; original->frames = 8; original->interleaved.resize (16);
        auto* ready = new engine::AssetSamples;
        ready->channels = 2; ready->frames = 8; ready->interleaved.resize (16);
        fixture.original = SampleOwner (original); fixture.ready = SampleOwner (ready);
        std::weak_ptr<const engine::AssetSamples> weakOriginal = fixture.original, weakReady = fixture.ready;
        AppStorage storage; Origin origin {};
        auto* app = initial (storage, fixture, {}, origin);
        EXPECT (fixture.original.use_count() == 2 && fixture.ready.use_count() == 2);
        parentTraceCount = 2;
        parentTraces[0] = { original, origin.request, 0, false };
        parentTraces[1] = { ready, origin.request, 0, false };
        fixture.original.reset(); fixture.ready.reset();
        EXPECT (! weakOriginal.expired() && ! weakReady.expired());
        current (*app, origin);
        destroy (storage, app, origin);
        EXPECT (weakOriginal.expired() && weakReady.expired());
        EXPECT (parentTraces[0].releasedAt != 0 && parentTraces[1].releasedAt != 0);
        EXPECT (parentTraces[0].forwarded && parentTraces[1].forwarded);
        EXPECT (parentTraces[1].releasedAt < parentTraces[0].releasedAt);
        auto* map = allocationFor (origin.request, false);
        auto* state = allocationFor (origin.request, true);
        if (map != nullptr && state != nullptr)
        {
            EXPECT (map->releaseOrder < parentTraces[1].releasedAt);
            EXPECT (parentTraces[0].releasedAt < state->releaseOrder);
        }
        parentTraceCount = 0; // Weak control-block destruction is a separate later event.
    }
    static void ordinaryReplacementCases()
    {
        using namespace formation_proof_support;
        currentCase = "never-armed pending replacement retains aliased inputs before old cleanup";
        Fixture fixture; AppStorage storage; Origin old {};
        auto* app = initial (storage, fixture, {}, old);
        const auto uses = fixture.original.use_count();
        for (unsigned i = 0; i != 3; ++i)
        {
            Origin replacement = next (*app, fixture, {}, old);
            EXPECT (fixture.original.use_count() == uses);
            old = replacement;
        }
        capturedValues (*app, fixture, old, {});
        current (*app, old); destroy (storage, app, old);

        currentCase = "initial no-State owner preserves failed next attempt then accepts new request";
        Fixture other; other.generation = 0;
        CaptureCase invalid; invalid.status = CaptureStatus::InvalidIdentity; invalid.identityRefused = true;
        AppStorage otherStorage; Origin failed {};
        auto* otherApp = initial (otherStorage, other, invalid, failed);
        const auto failedCell = unstartedCell (otherStorage, *otherApp, checkedSession (*otherApp).current);
        const auto attempt = next (*otherApp, other, invalid, failed);
        EXPECT (! attempt.physical);
        sameCell (unstartedCell (otherStorage, *otherApp, checkedSession (*otherApp).current), failedCell);
        current (*otherApp, failed);
        other.generation = 1;
        Origin success = next (*otherApp, other, {}, failed);
        current (*otherApp, success); destroy (otherStorage, otherApp, success);
    }
public:
    static int run()
    {
        using namespace formation_proof_support;
        capture = &captureAtRelease;
        initialCases();
        // An already-biting copied-value/diagnostic failure must exit as failure
        // before an invariant-sensitive receiver could consume mutated data.
        if (failures == 0) ordinaryReplacementCases();
        if (failures == 0) directWorkerCases();
        if (failures == 0) pendingCases (false);
        if (failures == 0) pendingCases (true);
        if (failures == 0) retainedCases();
        if (failures == 0) parentLastOwnerCase();
        currentCase = "final harness release inventory";
        EXPECT (! requestActive && parentTraceCount == 0);
        std::size_t summedAttempts = 0, successes = 0;
        for (std::size_t i = 0; i != requestNumber; ++i)
        { summedAttempts += requests[i].count; successes += requests[i].successes; }
        EXPECT (summedAttempts == attempts && successes == allocationCount);
        if (failures == 0)
        {
            // Independent finite controller inventory:58initial+7replacement+
            // 3direct+12pending/drain+2session-retiring+5retained+1parent.
            EXPECT (requestNumber == 88);
            EXPECT (attempts == 121 && allocationCount == 117);
            for (std::size_t i = 0; i != siteCount; ++i)
            {
                const bool sourceQualifiedOnly = i == 21 || (i >= 29 && i <= 31);
                EXPECT (observedSites[i] == ! sourceQualifiedOnly);
            }
        }
        for (std::size_t i = 0; i != allocationCount; ++i)
        {
            EXPECT (allocations[i].freed && allocations[i].freeForwarded);
            EXPECT (allocations[i].captured == allocations[i].state);
        }
        std::printf ("%s formation whole proof: %u checks, %u failures, %zu requests, %zu native allocations\n",
                     failures == 0 ? "PASS" : "FAIL", checks, failures, requestNumber, allocationCount);
        return failures == 0 ? 0 : 1;
    }
};
} // namespace yesdaw::app::musical_origin_source_r4

int main()
{
    try { return yesdaw::app::musical_origin_source_r4::FormationProof::run(); }
    catch (...)
    {
        std::fputs ("FAIL formation whole proof: harness allocation/thread exception; no proof credit\n", stderr);
        return 2;
    }
}
