#pragma once

// SOURCE ONLY / UNCOMPILED. Include in the independent proof, never the app.
#include "formation.h"
#include <utility>

namespace yesdaw::app::musical_origin_source_r4 {
class FormationProof; // Independent test class; no implementation/authority here.
class ProofAccess final
{
    friend class FormationProof;
    ProofAccess() = delete;

    // Every method and type is private. Friendship is not transitive: the test
    // receives copied observations, never a State/owner reference, fund or setter.
    struct StateObservation final
    {
        const void* allocationIdentity; // Opaque, borrowed identity; no ownership.
        Canonical canonical;
        Raw raw;
        Slice fixed[5]; // root,captureCheck,stateCleanup,mapCleanup,parentCleanup
        Slice roles[roleCount];
        std::uint32_t references;
        Context context;
        bool workerIssued, retainedIssued, readerIssued, parentLive, metadataLive;
        CaptureStatus status;
        const void* mapIdentity;
        U64 mapCapacity, mapConstructed, legacyEnteredContribution;
    };
    struct OwnerObservation final
    {
        bool present, hasState, armed, terminalAcquired;
        FormationReceipt historicalReceipt;
        RootCensus historicalOrFailedRoot;
        StateObservation liveState;
    };
    struct RetainedObservation final
    {
        bool armed, terminalAcquired;
        StateObservation liveState;
    };
    struct SessionObservation final
    {
        bool live[2], hasRetiring;
        std::size_t current, retiring;
    };

    static_assert (std::is_trivial_v<StateObservation> && std::is_standard_layout_v<StateObservation>);
    static_assert (std::is_trivial_v<OwnerObservation> && std::is_standard_layout_v<OwnerObservation>);
    static_assert (std::is_trivial_v<RetainedObservation> && std::is_standard_layout_v<RetainedObservation>);
    static_assert (std::is_trivial_v<SessionObservation> && std::is_standard_layout_v<SessionObservation>);
    static_assert (std::is_trivially_copyable_v<FormationReceipt>);
    static_assert (std::is_trivially_copyable_v<RootCensus>);

    // Live-state precondition: the caller independently proves a real live owner,
    // no in-flight receiver and exclusive/quiescent access. A historical status,
    // this probe, or an observed signal value cannot establish that precondition.
    // This test diagnostic makes no canonical/Raw write and performs no acquire.
    static StateObservation observeState (const MusicalPreparationRequestState& s) noexcept
    {
        StateObservation out {};
        out.allocationIdentity = static_cast<const void*> (&s);
        out.canonical = s.ledger_;
        out.raw = s.raw_;
        out.fixed[0] = s.root_; out.fixed[1] = s.captureCheck_;
        out.fixed[2] = s.stateCleanup_; out.fixed[3] = s.mapCleanup_; out.fixed[4] = s.parentCleanup_;
        for (std::size_t i = 0; i != roleCount; ++i) out.roles[i] = s.roleLife_[i];
        out.references = s.references_; out.context = s.context_;
        out.workerIssued = s.workerIssued_; out.retainedIssued = s.retainedIssued_; out.readerIssued = s.readerIssued_;
        out.parentLive = s.parentLive_; out.metadataLive = s.metadataLive_; out.status = s.captureStatus_;
        out.mapIdentity = static_cast<const void*> (s.map_);
        out.mapCapacity = s.mapCapacity_; out.mapConstructed = s.mapConstructed_;
        out.legacyEnteredContribution = s.legacyEnteredContribution_;
        return out;
    }
    static OwnerObservation observeOwner (const ControlRequestOwner& owner) noexcept
    {
        OwnerObservation out {};
        out.present = true; out.hasState = owner.state_ != nullptr;
        out.armed = owner.armed_; out.terminalAcquired = owner.terminalAcquired_;
        out.historicalReceipt = owner.receipt_;
        out.historicalOrFailedRoot = owner.failureTail_;
        if (out.hasState) out.liveState = observeState (*owner.state_);
        return out;
    }
    static OwnerObservation observeSlot (const ApplicationRequests& app, std::size_t index) noexcept
    {
        // Caller proves app is alive/fully initialized and index is0 or1.
        // Never form a typed Control owner in a dead raw slot.
        if (! app.live_[index]) return {};
        const auto* owner = std::launder (reinterpret_cast<const ControlRequestOwner*> (app.slots_[index].bytes));
        return observeOwner (*owner);
    }
    static SessionObservation session (const ApplicationRequests& app) noexcept
    { return { { app.live_[0], app.live_[1] }, app.hasRetiring_, app.current_, app.retiring_ }; }
    static OwnerObservation current (const ApplicationRequests& app) noexcept
    { return observeSlot (app, app.current_); }
    static OwnerObservation retiring (const ApplicationRequests& app) noexcept
    {
        if (! app.hasRetiring_) return {};
        return observeSlot (app, app.retiring_);
    }
    static RetainedObservation retained (const RetainedOriginOwner& owner) noexcept
    { return { owner.armed_, owner.terminalAcquired_, observeState (*owner.state_) }; }
    static FormationReceipt historicalReceipt (const Readout& readout) noexcept
    { return readout.receipt_; }

    // Compile-time description ONLY, for the independent test allocator's final
    // byte capture. No destructor/deallocation callback is installed in formation.
    // The allocator must identify the exact request-State allocation, own the
    // original allocation address/size, and copy bytes BEFORE real deallocation.
    // After State destruction begins, it must NOT launder/cast/dereference State,
    // access a dead member, call observeState, or invoke an owner/history getter.
    // Read the still-owned storage solely through its byte representation; copy
    // exactly these subranges into byte arrays, then into separate LIVE initialized
    // Canonical/Raw objects. Trivial-copy/standard-layout/range gates apply below.
    // Padding is copied as bytes only, never compared as initialized scalar data.
    static_assert (std::is_standard_layout_v<MusicalPreparationRequestState>);
    static_assert (std::is_standard_layout_v<Canonical> && std::is_trivially_copyable_v<Canonical>);
    static_assert (std::is_standard_layout_v<Raw> && std::is_trivially_copyable_v<Raw>);
    static constexpr std::size_t stateBytes = sizeof (MusicalPreparationRequestState);
    static constexpr std::size_t stateAlignment = alignof (MusicalPreparationRequestState);
    static constexpr std::size_t canonicalOffset = offsetof (MusicalPreparationRequestState, ledger_);
    static constexpr std::size_t canonicalBytes = sizeof (Canonical);
    static constexpr std::size_t rawOffset = offsetof (MusicalPreparationRequestState, raw_);
    static constexpr std::size_t rawBytes = sizeof (Raw);
    static_assert (stateBytes >= canonicalBytes && canonicalOffset <= stateBytes - canonicalBytes);
    static_assert (stateBytes >= rawBytes && rawOffset <= stateBytes - rawBytes);
    static_assert (canonicalOffset + canonicalBytes <= rawOffset || rawOffset + rawBytes <= canonicalOffset);
    static_assert (noexcept (Catalogue::reviewed())); // Unevaluated; no price/read API.
    static_assert (noexcept (MusicalPreparationRequestState (MusicalPreparationRequestState::Uninitialized {})));
    static_assert (noexcept (std::declval<MusicalPreparationRequestState&>().~MusicalPreparationRequestState()));
    static_assert (noexcept (ControlRequestOwner (std::declval<Formed&>())));
    static_assert (noexcept (RetainedOriginOwner (std::declval<ControlRequestOwner&>())));

    // R2 test-only value observations. Caller independently proves index0/1,
    // live slot/owner/State, quiescent receiver and exclusive access. For captured,
    // metadataLive/parentLive must both be true; no get() ownership is inferred.
    // Neither method returns a State/owner reference or copies a SampleOwner.
    struct CapturedObservation final
    {
        CapturedMetadata metadata;
        const void* originalIdentity;
        const void* readyIdentity;
    };
    static_assert (std::is_standard_layout_v<CapturedMetadata> && std::is_trivially_copyable_v<CapturedMetadata>);
    static_assert (std::is_standard_layout_v<CapturedObservation> && std::is_trivially_copyable_v<CapturedObservation>);
    static_assert (std::is_standard_layout_v<engine::TempoChange> && std::is_trivially_copyable_v<engine::TempoChange>);
    static CapturedObservation captured (const ApplicationRequests& app, std::size_t index) noexcept
    {
        const auto* owner = std::launder (reinterpret_cast<const ControlRequestOwner*> (app.slots_[index].bytes));
        const auto& s = *owner->state_;
        const auto* parents = std::launder (reinterpret_cast<const Parents*> (s.parentBytes_));
        return { s.metadata(), static_cast<const void*> (parents->original.get()),
                 static_cast<const void*> (parents->readyView.get()) };
    }
    static engine::TempoChange mapRow (const ApplicationRequests& app, std::size_t index,
                                       std::size_t rowIndex) noexcept
    {
        // Additional preconditions: actual map exists,rowIndex<mapConstructed.
        const auto* owner = std::launder (reinterpret_cast<const ControlRequestOwner*> (app.slots_[index].bytes));
        const auto& s = *owner->state_;
        return s.map_[rowIndex];
    }

    // R2 compile-time description ONLY: the final initial no-State failed root
    // has no native State allocation. Save genuine live current index,failed-root
    // ownership and allocation base/extent BEFORE explicit App destruction. Keep
    // that raw allocation owned until the failed-root BYTES have been copied.
    // NEVER form a dead typed App/Control/member reference or call an observer
    // after the destructor. No hook/callback/runtime accessor is added here.
    static_assert (std::is_standard_layout_v<ApplicationRequests>);
    static_assert (std::is_standard_layout_v<OwnerSlot>);
    static_assert (std::is_standard_layout_v<ControlRequestOwner>);
    static_assert (std::is_standard_layout_v<RootCensus> && std::is_trivially_copyable_v<RootCensus>);
    static_assert (std::extent_v<decltype (ApplicationRequests::slots_)> == 2);
    static constexpr std::size_t applicationBytes = sizeof (ApplicationRequests);
    static constexpr std::size_t applicationAlignment = alignof (ApplicationRequests);
    static constexpr std::size_t applicationSlotsOffset = offsetof (ApplicationRequests, slots_);
    static constexpr std::size_t slotStride = sizeof (OwnerSlot);
    static constexpr std::size_t slotStorageOffset = offsetof (OwnerSlot, bytes);
    static constexpr std::size_t slotStorageBytes = sizeof (decltype (OwnerSlot::bytes));
    static constexpr std::size_t controlBytes = sizeof (ControlRequestOwner);
    static constexpr std::size_t failedRootInControlOffset = offsetof (ControlRequestOwner, failureTail_);
    static constexpr std::size_t failedRootBytes = sizeof (RootCensus);

    // Check multiply/subtract/add ranges before defining each composed offset.
    // All these are uncompiled qualifications,not guessed native ABI values.
    static_assert (slotStride > 0 && slotStride <= std::numeric_limits<std::size_t>::max() / 2);
    static constexpr std::size_t applicationSlotsBytes = slotStride * 2;
    static_assert (applicationSlotsBytes == sizeof (decltype (ApplicationRequests::slots_)));
    static_assert (applicationSlotsBytes <= applicationBytes
                   && applicationSlotsOffset <= applicationBytes - applicationSlotsBytes);
    static_assert (controlBytes == slotStorageBytes);
    static_assert (slotStorageBytes <= slotStride && slotStorageOffset <= slotStride - slotStorageBytes);
    static_assert (failedRootBytes <= controlBytes
                   && failedRootInControlOffset <= controlBytes - failedRootBytes);
    static_assert (slotStride <= std::numeric_limits<std::size_t>::max() - applicationSlotsOffset);
    static constexpr std::size_t slot0Offset = applicationSlotsOffset;
    static constexpr std::size_t slot1Offset = applicationSlotsOffset + slotStride;
    static_assert (slot0Offset <= applicationBytes - slotStride
                   && slot1Offset <= applicationBytes - slotStride);
    static_assert (slot0Offset <= slot1Offset && slotStride <= slot1Offset - slot0Offset);
    static_assert (slotStorageOffset <= std::numeric_limits<std::size_t>::max() - slot0Offset
                   && slotStorageOffset <= std::numeric_limits<std::size_t>::max() - slot1Offset);
    static constexpr std::size_t control0Offset = slot0Offset + slotStorageOffset;
    static constexpr std::size_t control1Offset = slot1Offset + slotStorageOffset;
    static_assert (controlBytes <= applicationBytes && control0Offset <= applicationBytes - controlBytes
                   && control1Offset <= applicationBytes - controlBytes);
    static_assert (control0Offset <= control1Offset && controlBytes <= control1Offset - control0Offset);
    static_assert (failedRootInControlOffset <= std::numeric_limits<std::size_t>::max() - control0Offset
                   && failedRootInControlOffset <= std::numeric_limits<std::size_t>::max() - control1Offset);
    static constexpr std::size_t failedRootOffsetBySlot[2] {
        control0Offset + failedRootInControlOffset,
        control1Offset + failedRootInControlOffset
    };
    static_assert (failedRootBytes <= applicationBytes
                   && failedRootOffsetBySlot[0] <= applicationBytes - failedRootBytes
                   && failedRootOffsetBySlot[1] <= applicationBytes - failedRootBytes);
    static_assert (failedRootOffsetBySlot[0] <= failedRootOffsetBySlot[1]
                   && failedRootBytes <= failedRootOffsetBySlot[1] - failedRootOffsetBySlot[0]);
};
} // namespace yesdaw::app::musical_origin_source_r4
