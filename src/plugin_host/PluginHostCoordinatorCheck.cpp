#include "plugin_host/PluginHostCoordinator.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace {

const char* statusName (yesdaw::plugin_host::PluginHostCoordinator::HandshakeStatus status) noexcept
{
    using Status = yesdaw::plugin_host::PluginHostCoordinator::HandshakeStatus;

    switch (status)
    {
        case Status::notStarted:      return "notStarted";
        case Status::success:         return "success";
        case Status::launchFailed:    return "launchFailed";
        case Status::readyTimeout:    return "readyTimeout";
        case Status::probeSendFailed: return "probeSendFailed";
        case Status::echoTimeout:     return "echoTimeout";
        case Status::connectionLost:  return "connectionLost";
    }

    return "unknown";
}

const char* statusName (yesdaw::plugin_host::PluginHostCoordinator::StopStatus status) noexcept
{
    using Status = yesdaw::plugin_host::PluginHostCoordinator::StopStatus;

    switch (status)
    {
        case Status::notStarted:  return "notStarted";
        case Status::stopped:     return "stopped";
        case Status::stopTimeout: return "stopTimeout";
    }

    return "unknown";
}

const char* statusName (yesdaw::plugin_host::PluginHostCoordinator::WatchdogStatus status) noexcept
{
    using Status = yesdaw::plugin_host::PluginHostCoordinator::WatchdogStatus;

    switch (status)
    {
        case Status::notStarted:             return "notStarted";
        case Status::launchFailed:           return "launchFailed";
        case Status::readyTimeout:           return "readyTimeout";
        case Status::probeSendFailed:        return "probeSendFailed";
        case Status::timeoutKilled:          return "timeoutKilled";
        case Status::killObservationTimeout: return "killObservationTimeout";
        case Status::connectionLost:         return "connectionLost";
        case Status::unexpectedResponse:     return "unexpectedResponse";
    }

    return "unknown";
}

const char* statusName (yesdaw::plugin_host::PluginHostCoordinator::CrashStatus status) noexcept
{
    using Status = yesdaw::plugin_host::PluginHostCoordinator::CrashStatus;

    switch (status)
    {
        case Status::notStarted:         return "notStarted";
        case Status::launchFailed:       return "launchFailed";
        case Status::readyTimeout:       return "readyTimeout";
        case Status::connectionLost:     return "connectionLost";
        case Status::observationTimeout: return "observationTimeout";
    }

    return "unknown";
}

const char* statusName (yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind kind) noexcept
{
    using Kind = yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind;

    switch (kind)
    {
        case Kind::none:             return "none";
        case Kind::crash:            return "crash";
        case Kind::watchdogTimeout:  return "watchdogTimeout";
    }

    return "unknown";
}

const char* statusName (yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind action) noexcept
{
    using Action = yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind;

    switch (action)
    {
        case Action::none:                return "none";
        case Action::bypassAndRecompile:  return "bypassAndRecompile";
    }

    return "unknown";
}

const char* statusName (yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandKind command) noexcept
{
    using Command = yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandKind;

    switch (command)
    {
        case Command::none:                return "none";
        case Command::bypassAndRecompile:  return "bypassAndRecompile";
    }

    return "unknown";
}

const char* statusName (yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus status) noexcept
{
    using Status = yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus;

    switch (status)
    {
        case Status::noAction:      return "noAction";
        case Status::commandReady:  return "commandReady";
    }

    return "unknown";
}

const char* statusName (yesdaw::plugin_host::PluginHostCoordinator::GraphRecompileStatus status) noexcept
{
    using Status = yesdaw::plugin_host::PluginHostCoordinator::GraphRecompileStatus;

    switch (status)
    {
        case Status::noAction:            return "noAction";
        case Status::compileFailed:       return "compileFailed";
        case Status::missingPlaceholder:  return "missingPlaceholder";
        case Status::publishFailed:       return "publishFailed";
        case Status::graphPublished:      return "graphPublished";
    }

    return "unknown";
}

const char* statusName (yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalationStatus status) noexcept
{
    using Status = yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalationStatus;

    switch (status)
    {
        case Status::noAction:          return "noAction";
        case Status::escalationReady:   return "escalationReady";
    }

    return "unknown";
}

const char* statusName (
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionRequestStatus status) noexcept
{
    using Status = yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionRequestStatus;

    switch (status)
    {
        case Status::noAction:      return "noAction";
        case Status::requestReady:  return "requestReady";
    }

    return "unknown";
}

const char* statusName (
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionCommandKind command) noexcept
{
    using Command = yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionCommandKind;

    switch (command)
    {
        case Command::none:                   return "none";
        case Command::requestPolicyDecision:  return "requestPolicyDecision";
    }

    return "unknown";
}

const char* statusName (
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionCommandStatus status) noexcept
{
    using Status = yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionCommandStatus;

    switch (status)
    {
        case Status::noAction:      return "noAction";
        case Status::commandReady:  return "commandReady";
    }

    return "unknown";
}

const char* statusName (
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionOutcomeStatus status) noexcept
{
    using Status = yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionOutcomeStatus;

    switch (status)
    {
        case Status::noAction:      return "noAction";
        case Status::outcomeReady:  return "outcomeReady";
    }

    return "unknown";
}

const char* statusName (
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionOutcomeHandlingStatus status) noexcept
{
    using Status = yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionOutcomeHandlingStatus;

    switch (status)
    {
        case Status::noAction:       return "noAction";
        case Status::handlingReady:  return "handlingReady";
    }

    return "unknown";
}

const char* statusName (
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingRequestStatus status) noexcept
{
    using Status = yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingRequestStatus;

    switch (status)
    {
        case Status::noAction:      return "noAction";
        case Status::requestReady:  return "requestReady";
    }

    return "unknown";
}

const char* statusName (
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingCommandKind command) noexcept
{
    using Command = yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingCommandKind;

    switch (command)
    {
        case Command::none:                    return "none";
        case Command::handleBlacklistRequest:  return "handleBlacklistRequest";
    }

    return "unknown";
}

const char* statusName (
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingCommandStatus status) noexcept
{
    using Status = yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingCommandStatus;

    switch (status)
    {
        case Status::noAction:      return "noAction";
        case Status::commandReady:  return "commandReady";
    }

    return "unknown";
}

const char* statusName (
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingOutcomeStatus status) noexcept
{
    using Status = yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingOutcomeStatus;

    switch (status)
    {
        case Status::noAction:      return "noAction";
        case Status::outcomeReady:  return "outcomeReady";
    }

    return "unknown";
}

const char* statusName (
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingOutcomeHandlingStatus status) noexcept
{
    using Status = yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingOutcomeHandlingStatus;

    switch (status)
    {
        case Status::noAction:       return "noAction";
        case Status::handlingReady:  return "handlingReady";
    }

    return "unknown";
}

const char* statusName (yesdaw::plugin_host::PluginHostCoordinator::ChildState state) noexcept
{
    using State = yesdaw::plugin_host::PluginHostCoordinator::ChildState;

    switch (state)
    {
        case State::idle:        return "idle";
        case State::launching:   return "launching";
        case State::ready:       return "ready";
        case State::handshaking: return "handshaking";
        case State::running:     return "running";
        case State::stopping:    return "stopping";
        case State::stopped:     return "stopped";
        case State::lost:        return "lost";
    }

    return "unknown";
}

const char* statusName (yesdaw::plugin_host::PluginHostCoordinator::RtLaneLoadStatus status) noexcept
{
    using Status = yesdaw::plugin_host::PluginHostCoordinator::RtLaneLoadStatus;

    switch (status)
    {
        case Status::notStarted:       return "notStarted";
        case Status::launchFailed:     return "launchFailed";
        case Status::readyTimeout:     return "readyTimeout";
        case Status::allocationFailed: return "allocationFailed";
        case Status::messageSendFailed: return "messageSendFailed";
        case Status::replyTimeout:     return "replyTimeout";
        case Status::connectionLost:   return "connectionLost";
        case Status::workerRejected:   return "workerRejected";
        case Status::success:          return "success";
    }

    return "unknown";
}

const char* statusName (yesdaw::plugin_host::RtLaneLoadReplyStatus status) noexcept
{
    using Status = yesdaw::plugin_host::RtLaneLoadReplyStatus;

    switch (status)
    {
        case Status::none:                    return "none";
        case Status::accepted:                return "accepted";
        case Status::rejectedInvalidIdentity: return "rejectedInvalidIdentity";
        case Status::rejectedAttachFailed:    return "rejectedAttachFailed";
    }

    return "unknown";
}

const char* statusName (yesdaw::engine::RtLaneAttachFailure failure) noexcept
{
    using Failure = yesdaw::engine::RtLaneAttachFailure;

    switch (failure)
    {
        case Failure::None:           return "None";
        case Failure::OpenFailed:     return "OpenFailed";
        case Failure::RegionTooSmall: return "RegionTooSmall";
        case Failure::HeaderMismatch: return "HeaderMismatch";
        case Failure::SizeMismatch:   return "SizeMismatch";
    }

    return "unknown";
}

bool requestMatches (yesdaw::plugin_host::PluginHostCoordinator::FailureActionRequest actual,
                     yesdaw::plugin_host::PluginHostCoordinator::FailureActionRequest expected) noexcept
{
    return actual.action == expected.action
        && actual.failureKind == expected.failureKind
        && actual.bypassRequested == expected.bypassRequested
        && actual.recompileRequested == expected.recompileRequested;
}

bool commandMatches (yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommand actual,
                     yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommand expected) noexcept
{
    return actual.command == expected.command
        && actual.failureKind == expected.failureKind
        && actual.bypassRequested == expected.bypassRequested
        && actual.recompileRequested == expected.recompileRequested;
}

bool commandResultMatches (yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandResult actual,
                           yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandResult expected) noexcept
{
    return actual.status == expected.status
        && requestMatches (actual.drainedRequest, expected.drainedRequest)
        && commandMatches (actual.command, expected.command)
        && actual.pendingRequestConsumed == expected.pendingRequestConsumed
        && actual.graphRecompileExecuted == expected.graphRecompileExecuted;
}

class SimulatedHostedPluginNode final : public yesdaw::engine::Node
{
public:
    explicit SimulatedHostedPluginNode (yesdaw::engine::NodeId id, int channels = 1) noexcept
        : id_ (id), channels_ (channels > 0 ? channels : 1)
    {
    }

    yesdaw::engine::NodeProperties properties() const noexcept override
    {
        return { true, false, channels_, 0, id_ };
    }

    std::span<yesdaw::engine::Node* const> directInputs() const noexcept override
    {
        return std::span<yesdaw::engine::Node* const> (&input_, input_ != nullptr ? 1u : 0u);
    }

    void prepare (double, int) override {}
    void process (const yesdaw::engine::ProcessArgs&) noexcept YESDAW_RT_HOT override {}
    void reset() noexcept override {}
    void release() override {}

    void setInput (yesdaw::engine::Node* input) noexcept
    {
        input_ = input;
    }

private:
    yesdaw::engine::NodeId id_;
    int channels_;
    yesdaw::engine::Node* input_ = nullptr;
};

constexpr yesdaw::engine::NodeId kRecoverySourceId = 71001;
constexpr yesdaw::engine::NodeId kRecoveryOffenderId = 71002;
constexpr yesdaw::engine::NodeId kRecoveryMasterId = 71003;

yesdaw::engine::GraphBuilder::Inputs recoveryGraphInputs (bool placeholder, yesdaw::engine::GraphId graphId)
{
    yesdaw::engine::GraphBuilder::Inputs inputs;
    inputs.id = graphId;
    inputs.masterNodeId = kRecoveryMasterId;
    inputs.maxBlockSize = 8;

    auto source = std::make_unique<yesdaw::engine::IdentityDcNode> (kRecoverySourceId, 0.75f, 1);
    yesdaw::engine::Node* const sourcePtr = source.get();

    std::unique_ptr<yesdaw::engine::Node> offender;
    if (placeholder)
    {
        auto replacement = std::make_unique<yesdaw::engine::PlaceholderNode> (kRecoveryOffenderId, 1);
        replacement->setInput (sourcePtr);
        offender = std::move (replacement);
    }
    else
    {
        auto hosted = std::make_unique<SimulatedHostedPluginNode> (kRecoveryOffenderId, 1);
        hosted->setInput (sourcePtr);
        offender = std::move (hosted);
    }

    yesdaw::engine::Node* const offenderPtr = offender.get();
    auto master = std::make_unique<yesdaw::engine::MasterNode> (kRecoveryMasterId, 1);
    master->setInputNodes ({ offenderPtr });

    inputs.nodes.push_back (std::move (source));
    inputs.nodes.push_back (std::move (offender));
    inputs.nodes.push_back (std::move (master));
    return inputs;
}

bool allSamplesNear (const std::vector<float>& values, float expected) noexcept
{
    for (const float value : values)
        if (std::abs (value - expected) > 0.000001f)
            return false;

    return true;
}

bool blacklistCandidateMatches (yesdaw::plugin_host::PluginHostCoordinator::BlacklistCandidateStatus actual,
                                yesdaw::plugin_host::PluginHostCoordinator::BlacklistCandidateStatus expected) noexcept
{
    return actual.failureKind == expected.failureKind
        && actual.candidate == expected.candidate
        && actual.crashCandidate == expected.crashCandidate
        && actual.watchdogTimeoutCandidate == expected.watchdogTimeoutCandidate;
}

bool blacklistEscalationMatches (yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalation actual,
                                 yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalation expected) noexcept
{
    return actual.failureKind == expected.failureKind
        && actual.crashCandidate == expected.crashCandidate
        && actual.watchdogTimeoutCandidate == expected.watchdogTimeoutCandidate
        && actual.controlThreadEscalationRequested == expected.controlThreadEscalationRequested;
}

bool blacklistEscalationResultMatches (
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalationResult actual,
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalationResult expected) noexcept
{
    return actual.status == expected.status
        && blacklistCandidateMatches (actual.drainedCandidate, expected.drainedCandidate)
        && blacklistEscalationMatches (actual.escalation, expected.escalation)
        && actual.pendingCandidateConsumed == expected.pendingCandidateConsumed
        && actual.blacklistPolicyApplied == expected.blacklistPolicyApplied
        && actual.blacklistStatePersisted == expected.blacklistStatePersisted;
}

bool deferredBlacklistEscalationStatusMatches (
    yesdaw::plugin_host::PluginHostCoordinator::DeferredBlacklistEscalationStatus actual,
    yesdaw::plugin_host::PluginHostCoordinator::DeferredBlacklistEscalationStatus expected) noexcept
{
    return actual.status == expected.status
        && blacklistEscalationResultMatches (actual.lastResult, expected.lastResult)
        && actual.escalationRecorded == expected.escalationRecorded
        && actual.blacklistPolicyApplied == expected.blacklistPolicyApplied
        && actual.blacklistStatePersisted == expected.blacklistStatePersisted;
}

bool blacklistPolicyDecisionRequestMatches (
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionRequest actual,
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionRequest expected) noexcept
{
    return actual.status == expected.status
        && actual.failureKind == expected.failureKind
        && actual.crashCandidate == expected.crashCandidate
        && actual.watchdogTimeoutCandidate == expected.watchdogTimeoutCandidate
        && actual.controlThreadPolicyDecisionRequested == expected.controlThreadPolicyDecisionRequested
        && actual.blacklistPolicyApplied == expected.blacklistPolicyApplied
        && actual.blacklistStatePersisted == expected.blacklistStatePersisted;
}

bool blacklistPolicyDecisionCommandMatches (
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionCommand actual,
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionCommand expected) noexcept
{
    return actual.command == expected.command
        && actual.failureKind == expected.failureKind
        && actual.crashCandidate == expected.crashCandidate
        && actual.watchdogTimeoutCandidate == expected.watchdogTimeoutCandidate
        && actual.controlThreadPolicyDecisionRequested == expected.controlThreadPolicyDecisionRequested;
}

bool blacklistPolicyDecisionCommandResultMatches (
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionCommandResult actual,
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionCommandResult expected) noexcept
{
    return actual.status == expected.status
        && blacklistPolicyDecisionRequestMatches (actual.drainedRequest, expected.drainedRequest)
        && blacklistPolicyDecisionCommandMatches (actual.command, expected.command)
        && actual.pendingRequestConsumed == expected.pendingRequestConsumed
        && actual.blacklistPolicyApplied == expected.blacklistPolicyApplied
        && actual.blacklistStatePersisted == expected.blacklistStatePersisted;
}

bool deferredBlacklistPolicyDecisionCommandStatusMatches (
    yesdaw::plugin_host::PluginHostCoordinator::DeferredBlacklistPolicyDecisionCommandStatus actual,
    yesdaw::plugin_host::PluginHostCoordinator::DeferredBlacklistPolicyDecisionCommandStatus expected) noexcept
{
    return actual.status == expected.status
        && blacklistPolicyDecisionCommandResultMatches (actual.lastResult, expected.lastResult)
        && actual.commandRecorded == expected.commandRecorded
        && actual.blacklistPolicyApplied == expected.blacklistPolicyApplied
        && actual.blacklistStatePersisted == expected.blacklistStatePersisted;
}

bool blacklistPolicyDecisionOutcomeMatches (
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionOutcome actual,
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionOutcome expected) noexcept
{
    return actual.status == expected.status
        && actual.failureKind == expected.failureKind
        && actual.crashCandidate == expected.crashCandidate
        && actual.watchdogTimeoutCandidate == expected.watchdogTimeoutCandidate
        && actual.controlThreadPolicyDecisionInspected == expected.controlThreadPolicyDecisionInspected
        && actual.blacklistPolicyApplied == expected.blacklistPolicyApplied
        && actual.blacklistStatePersisted == expected.blacklistStatePersisted;
}

bool blacklistPolicyDecisionOutcomeHandlingMatches (
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionOutcomeHandling actual,
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionOutcomeHandling expected) noexcept
{
    return actual.failureKind == expected.failureKind
        && actual.crashCandidate == expected.crashCandidate
        && actual.watchdogTimeoutCandidate == expected.watchdogTimeoutCandidate
        && actual.controlThreadBlacklistHandlingRequested == expected.controlThreadBlacklistHandlingRequested;
}

bool blacklistPolicyDecisionOutcomeHandlingResultMatches (
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionOutcomeHandlingResult actual,
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionOutcomeHandlingResult expected) noexcept
{
    return actual.status == expected.status
        && blacklistPolicyDecisionOutcomeMatches (actual.drainedOutcome, expected.drainedOutcome)
        && blacklistPolicyDecisionOutcomeHandlingMatches (actual.handling, expected.handling)
        && actual.pendingOutcomeConsumed == expected.pendingOutcomeConsumed
        && actual.blacklistPolicyApplied == expected.blacklistPolicyApplied
        && actual.blacklistStatePersisted == expected.blacklistStatePersisted;
}

bool deferredBlacklistPolicyDecisionOutcomeHandlingStatusMatches (
    yesdaw::plugin_host::PluginHostCoordinator::DeferredBlacklistPolicyDecisionOutcomeHandlingStatus actual,
    yesdaw::plugin_host::PluginHostCoordinator::DeferredBlacklistPolicyDecisionOutcomeHandlingStatus expected) noexcept
{
    return actual.status == expected.status
        && blacklistPolicyDecisionOutcomeHandlingResultMatches (actual.lastResult, expected.lastResult)
        && actual.handlingRecorded == expected.handlingRecorded
        && actual.blacklistPolicyApplied == expected.blacklistPolicyApplied
        && actual.blacklistStatePersisted == expected.blacklistStatePersisted;
}

bool blacklistHandlingRequestMatches (
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingRequest actual,
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingRequest expected) noexcept
{
    return actual.status == expected.status
        && actual.failureKind == expected.failureKind
        && actual.crashCandidate == expected.crashCandidate
        && actual.watchdogTimeoutCandidate == expected.watchdogTimeoutCandidate
        && actual.controlThreadBlacklistHandlingRequested == expected.controlThreadBlacklistHandlingRequested
        && actual.blacklistPolicyApplied == expected.blacklistPolicyApplied
        && actual.blacklistStatePersisted == expected.blacklistStatePersisted;
}

bool blacklistHandlingCommandMatches (
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingCommand actual,
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingCommand expected) noexcept
{
    return actual.command == expected.command
        && actual.failureKind == expected.failureKind
        && actual.crashCandidate == expected.crashCandidate
        && actual.watchdogTimeoutCandidate == expected.watchdogTimeoutCandidate
        && actual.controlThreadBlacklistHandlingRequested == expected.controlThreadBlacklistHandlingRequested;
}

bool blacklistHandlingCommandResultMatches (
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingCommandResult actual,
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingCommandResult expected) noexcept
{
    return actual.status == expected.status
        && blacklistHandlingRequestMatches (actual.drainedRequest, expected.drainedRequest)
        && blacklistHandlingCommandMatches (actual.command, expected.command)
        && actual.pendingRequestConsumed == expected.pendingRequestConsumed
        && actual.blacklistPolicyApplied == expected.blacklistPolicyApplied
        && actual.blacklistStatePersisted == expected.blacklistStatePersisted;
}

bool deferredBlacklistHandlingCommandStatusMatches (
    yesdaw::plugin_host::PluginHostCoordinator::DeferredBlacklistHandlingCommandStatus actual,
    yesdaw::plugin_host::PluginHostCoordinator::DeferredBlacklistHandlingCommandStatus expected) noexcept
{
    return actual.status == expected.status
        && blacklistHandlingCommandResultMatches (actual.lastResult, expected.lastResult)
        && actual.commandRecorded == expected.commandRecorded
        && actual.blacklistPolicyApplied == expected.blacklistPolicyApplied
        && actual.blacklistStatePersisted == expected.blacklistStatePersisted;
}

bool blacklistHandlingOutcomeMatches (
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingOutcome actual,
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingOutcome expected) noexcept
{
    return actual.status == expected.status
        && actual.failureKind == expected.failureKind
        && actual.crashCandidate == expected.crashCandidate
        && actual.watchdogTimeoutCandidate == expected.watchdogTimeoutCandidate
        && actual.controlThreadBlacklistHandlingInspected == expected.controlThreadBlacklistHandlingInspected
        && actual.blacklistPolicyApplied == expected.blacklistPolicyApplied
        && actual.blacklistStatePersisted == expected.blacklistStatePersisted;
}

bool blacklistHandlingOutcomeHandlingMatches (
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingOutcomeHandling actual,
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingOutcomeHandling expected) noexcept
{
    return actual.failureKind == expected.failureKind
        && actual.crashCandidate == expected.crashCandidate
        && actual.watchdogTimeoutCandidate == expected.watchdogTimeoutCandidate
        && actual.controlThreadBlacklistHandlingRequested == expected.controlThreadBlacklistHandlingRequested;
}

bool blacklistHandlingOutcomeHandlingResultMatches (
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingOutcomeHandlingResult actual,
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingOutcomeHandlingResult expected) noexcept
{
    return actual.status == expected.status
        && blacklistHandlingOutcomeMatches (actual.drainedOutcome, expected.drainedOutcome)
        && blacklistHandlingOutcomeHandlingMatches (actual.handling, expected.handling)
        && actual.pendingOutcomeConsumed == expected.pendingOutcomeConsumed
        && actual.blacklistPolicyApplied == expected.blacklistPolicyApplied
        && actual.blacklistStatePersisted == expected.blacklistStatePersisted;
}

bool deferredBlacklistHandlingOutcomeHandlingStatusMatches (
    yesdaw::plugin_host::PluginHostCoordinator::DeferredBlacklistHandlingOutcomeHandlingStatus actual,
    yesdaw::plugin_host::PluginHostCoordinator::DeferredBlacklistHandlingOutcomeHandlingStatus expected) noexcept
{
    return actual.status == expected.status
        && blacklistHandlingOutcomeHandlingResultMatches (actual.lastResult, expected.lastResult)
        && actual.handlingRecorded == expected.handlingRecorded
        && actual.blacklistPolicyApplied == expected.blacklistPolicyApplied
        && actual.blacklistStatePersisted == expected.blacklistStatePersisted;
}

bool waitForOutputSeqAtLeast (yesdaw::engine::RtLaneRing& ring, std::uint64_t expected)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds (2);
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (ring.outputSeq() >= expected)
            return true;

        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }

    return ring.outputSeq() >= expected;
}

float workerPollSampleFor (int channel, std::uint64_t block, int frame) noexcept
{
    return static_cast<float> ((channel + 1) * 1000)
        + static_cast<float> (block * 100u)
        + static_cast<float> (frame);
}

} // namespace

int main (int argc, char** argv)
{
    if (argc != 2)
    {
        std::printf ("FAIL: expected path to YesDawPluginHost worker executable\n");
        return 2;
    }

    const juce::File workerExecutable { juce::String (argv[1]) };
    if (! workerExecutable.existsAsFile())
    {
        std::printf ("FAIL: worker executable does not exist: %s\n", argv[1]);
        return 2;
    }

    yesdaw::engine::RtLaneConfig rtLaneConfig;
    rtLaneConfig.channels = 1;
    rtLaneConfig.maxBlockSize = 8;
    rtLaneConfig.maxEventsPerBlock = 2;

    yesdaw::plugin_host::PluginHostCoordinator rtLaneLoadCoordinator;
    const auto rtLaneLoad = rtLaneLoadCoordinator.launchAndLoadRtLane (workerExecutable, rtLaneConfig);
    const auto activeRtLaneIdentity = rtLaneLoadCoordinator.activeRtLaneLoadIdentity();
    const bool activeRtLaneUsesOsSharedMemory = rtLaneLoadCoordinator.activeRtLaneUsesOsSharedMemory();
    const auto rtLaneLoadStop = rtLaneLoadCoordinator.requestStopAndWait();
    if (rtLaneLoad.status != yesdaw::plugin_host::PluginHostCoordinator::RtLaneLoadStatus::success
        || rtLaneLoad.workerReplyStatus != yesdaw::plugin_host::RtLaneLoadReplyStatus::accepted
        || rtLaneLoad.workerAttachFailure != yesdaw::engine::RtLaneAttachFailure::None
        || ! rtLaneLoad.readySeen
        || ! rtLaneLoad.loadMessageSent
        || ! rtLaneLoad.loadReplySeen
        || ! rtLaneLoad.coordinatorAllocated
        || ! rtLaneLoad.coordinatorUsesOsSharedMemory
        || ! rtLaneLoad.workerAccepted
        || rtLaneLoad.identity.sharedMemoryName.empty()
        || activeRtLaneIdentity.sharedMemoryName != rtLaneLoad.identity.sharedMemoryName
        || ! activeRtLaneUsesOsSharedMemory
        || rtLaneLoadStop.status != yesdaw::plugin_host::PluginHostCoordinator::StopStatus::stopped)
    {
        std::printf ("FAIL: plugin host coordinator RT-lane load/control transfer is wrong: status=%s reply=%s attach=%s/%d ready=%d sent=%d replySeen=%d allocated=%d os=%d accepted=%d name=%s active=%s activeOs=%d stop=%s\n",
                     statusName (rtLaneLoad.status),
                     statusName (rtLaneLoad.workerReplyStatus),
                     statusName (rtLaneLoad.workerAttachFailure),
                     rtLaneLoad.workerAttachSystemError,
                     rtLaneLoad.readySeen ? 1 : 0,
                     rtLaneLoad.loadMessageSent ? 1 : 0,
                     rtLaneLoad.loadReplySeen ? 1 : 0,
                     rtLaneLoad.coordinatorAllocated ? 1 : 0,
                     rtLaneLoad.coordinatorUsesOsSharedMemory ? 1 : 0,
                     rtLaneLoad.workerAccepted ? 1 : 0,
                     rtLaneLoad.identity.sharedMemoryName.c_str(),
                     activeRtLaneIdentity.sharedMemoryName.c_str(),
                     activeRtLaneUsesOsSharedMemory ? 1 : 0,
                     statusName (rtLaneLoadStop.status));
        return 2;
    }

    yesdaw::engine::RtLaneConfig rtLanePollConfig;
    rtLanePollConfig.channels = 2;
    rtLanePollConfig.maxBlockSize = 8;
    rtLanePollConfig.maxEventsPerBlock = 2;

    yesdaw::plugin_host::PluginHostCoordinator rtLanePollCoordinator;
    const auto rtLanePollLoad =
        rtLanePollCoordinator.launchAndLoadRtLane (workerExecutable, rtLanePollConfig);
    const auto rtLanePollActiveIdentity = rtLanePollCoordinator.activeRtLaneLoadIdentity();
    const bool rtLanePollActiveUsesOsSharedMemory = rtLanePollCoordinator.activeRtLaneUsesOsSharedMemory();

    yesdaw::engine::RtLaneRing rtLaneAudioEndpoint;
    bool rtLaneAudioEndpointAttached = false;
    yesdaw::engine::RtLaneAttachFailure rtLaneAudioAttachFailure =
        yesdaw::engine::RtLaneAttachFailure::None;
    int rtLaneAudioAttachSystemError = 0;
    std::uint64_t rtLanePollInitialOutputSeq = 0;
    std::uint64_t rtLanePollOutputSeqAfterPoll = 0;
    yesdaw::engine::RtLaneExchangeResult rtLanePollR0;
    yesdaw::engine::RtLaneExchangeResult rtLanePollR1;
    bool rtLanePollOutputReady = false;
    bool rtLanePollSamplesMatch = false;
    int rtLanePollFailedChannel = -1;
    int rtLanePollFailedFrame = -1;
    float rtLanePollObserved = 0.0f;
    float rtLanePollExpected = 0.0f;

    if (rtLanePollLoad.status
            == yesdaw::plugin_host::PluginHostCoordinator::RtLaneLoadStatus::success
        && rtLanePollLoad.workerReplyStatus == yesdaw::plugin_host::RtLaneLoadReplyStatus::accepted
        && rtLanePollLoad.workerAttachFailure == yesdaw::engine::RtLaneAttachFailure::None
        && rtLanePollLoad.readySeen
        && rtLanePollLoad.loadMessageSent
        && rtLanePollLoad.loadReplySeen
        && rtLanePollLoad.coordinatorAllocated
        && rtLanePollLoad.coordinatorUsesOsSharedMemory
        && rtLanePollLoad.workerAccepted
        && rtLanePollActiveIdentity.sharedMemoryName == rtLanePollLoad.identity.sharedMemoryName
        && rtLanePollActiveUsesOsSharedMemory)
    {
        rtLaneAudioEndpointAttached =
            rtLaneAudioEndpoint.attachSharedMemory (rtLanePollActiveIdentity.sharedMemoryName);
        rtLaneAudioAttachFailure = rtLaneAudioEndpoint.lastAttachFailure();
        rtLaneAudioAttachSystemError = rtLaneAudioEndpoint.lastAttachSystemError();

        if (rtLaneAudioEndpointAttached)
        {
            rtLanePollInitialOutputSeq = rtLaneAudioEndpoint.outputSeq();

            constexpr int pollChannels = 2;
            constexpr int pollFrames = 8;
            std::vector<float> pollInput (static_cast<std::size_t> (pollChannels * pollFrames), 0.0f);
            std::vector<float> pollOutput (static_cast<std::size_t> (pollChannels * pollFrames), -1.0f);
            std::vector<float*> pollInputChannels (pollChannels);
            std::vector<float*> pollOutputChannels (pollChannels);
            for (int channel = 0; channel < pollChannels; ++channel)
            {
                pollInputChannels[static_cast<std::size_t> (channel)] =
                    pollInput.data() + static_cast<std::size_t> (channel * pollFrames);
                pollOutputChannels[static_cast<std::size_t> (channel)] =
                    pollOutput.data() + static_cast<std::size_t> (channel * pollFrames);
            }

            auto fillPollInput = [&] (std::uint64_t block)
            {
                for (int channel = 0; channel < pollChannels; ++channel)
                    for (int frame = 0; frame < pollFrames; ++frame)
                        pollInput[static_cast<std::size_t> (channel * pollFrames + frame)] =
                            workerPollSampleFor (channel, block, frame);
            };

            fillPollInput (0);
            rtLanePollR0 = rtLaneAudioEndpoint.exchangeBlock (
                pollInputChannels.data(), pollChannels, pollFrames, {},
                pollOutputChannels.data(), pollChannels);

            rtLanePollOutputReady = waitForOutputSeqAtLeast (rtLaneAudioEndpoint, 1);
            rtLanePollOutputSeqAfterPoll = rtLaneAudioEndpoint.outputSeq();

            fillPollInput (1);
            rtLanePollR1 = rtLaneAudioEndpoint.exchangeBlock (
                pollInputChannels.data(), pollChannels, pollFrames, {},
                pollOutputChannels.data(), pollChannels);

            rtLanePollSamplesMatch = true;
            for (int channel = 0; channel < pollChannels && rtLanePollSamplesMatch; ++channel)
            {
                for (int frame = 0; frame < pollFrames; ++frame)
                {
                    const float expected = workerPollSampleFor (channel, 0, frame);
                    const float observed =
                        pollOutput[static_cast<std::size_t> (channel * pollFrames + frame)];
                    if (observed != expected)
                    {
                        rtLanePollSamplesMatch = false;
                        rtLanePollFailedChannel = channel;
                        rtLanePollFailedFrame = frame;
                        rtLanePollObserved = observed;
                        rtLanePollExpected = expected;
                        break;
                    }
                }
            }
        }
    }

    const auto rtLanePollStop = rtLanePollCoordinator.requestStopAndWait();
    if (rtLanePollLoad.status != yesdaw::plugin_host::PluginHostCoordinator::RtLaneLoadStatus::success
        || rtLanePollLoad.workerReplyStatus != yesdaw::plugin_host::RtLaneLoadReplyStatus::accepted
        || rtLanePollLoad.workerAttachFailure != yesdaw::engine::RtLaneAttachFailure::None
        || ! rtLanePollLoad.readySeen
        || ! rtLanePollLoad.loadMessageSent
        || ! rtLanePollLoad.loadReplySeen
        || ! rtLanePollLoad.coordinatorAllocated
        || ! rtLanePollLoad.coordinatorUsesOsSharedMemory
        || ! rtLanePollLoad.workerAccepted
        || rtLanePollActiveIdentity.sharedMemoryName != rtLanePollLoad.identity.sharedMemoryName
        || ! rtLanePollActiveUsesOsSharedMemory
        || ! rtLaneAudioEndpointAttached
        || rtLaneAudioAttachFailure != yesdaw::engine::RtLaneAttachFailure::None
        || rtLanePollInitialOutputSeq != 0
        || rtLanePollR0.source != yesdaw::engine::RtLaneOutput::Silence
        || ! rtLanePollOutputReady
        || rtLanePollOutputSeqAfterPoll < 1
        || rtLanePollR1.source != yesdaw::engine::RtLaneOutput::Fresh
        || rtLanePollR1.outputBlockIndex != 0
        || ! rtLanePollSamplesMatch
        || rtLanePollStop.status != yesdaw::plugin_host::PluginHostCoordinator::StopStatus::stopped)
    {
        std::printf ("FAIL: plugin host worker did not poll the mapped RT lane through the hosted processor path: status=%s reply=%s attach=%s/%d ready=%d sent=%d replySeen=%d allocated=%d os=%d accepted=%d active=%s activeOs=%d audioAttached=%d audioAttach=%s/%d initialOutputSeq=%llu r0=%d outputReady=%d outputSeq=%llu r1=%d block=%llu sampleMatch=%d failed=%d/%d observed=%f expected=%f stop=%s\n",
                     statusName (rtLanePollLoad.status),
                     statusName (rtLanePollLoad.workerReplyStatus),
                     statusName (rtLanePollLoad.workerAttachFailure),
                     rtLanePollLoad.workerAttachSystemError,
                     rtLanePollLoad.readySeen ? 1 : 0,
                     rtLanePollLoad.loadMessageSent ? 1 : 0,
                     rtLanePollLoad.loadReplySeen ? 1 : 0,
                     rtLanePollLoad.coordinatorAllocated ? 1 : 0,
                     rtLanePollLoad.coordinatorUsesOsSharedMemory ? 1 : 0,
                     rtLanePollLoad.workerAccepted ? 1 : 0,
                     rtLanePollActiveIdentity.sharedMemoryName.c_str(),
                     rtLanePollActiveUsesOsSharedMemory ? 1 : 0,
                     rtLaneAudioEndpointAttached ? 1 : 0,
                     statusName (rtLaneAudioAttachFailure),
                     rtLaneAudioAttachSystemError,
                     static_cast<unsigned long long> (rtLanePollInitialOutputSeq),
                     static_cast<int> (rtLanePollR0.source),
                     rtLanePollOutputReady ? 1 : 0,
                     static_cast<unsigned long long> (rtLanePollOutputSeqAfterPoll),
                     static_cast<int> (rtLanePollR1.source),
                     static_cast<unsigned long long> (rtLanePollR1.outputBlockIndex),
                     rtLanePollSamplesMatch ? 1 : 0,
                     rtLanePollFailedChannel,
                     rtLanePollFailedFrame,
                     static_cast<double> (rtLanePollObserved),
                     static_cast<double> (rtLanePollExpected),
                     statusName (rtLanePollStop.status));
        return 2;
    }

    yesdaw::engine::RtLaneConfig runningWatchdogConfig;
    runningWatchdogConfig.channels = 1;
    runningWatchdogConfig.maxBlockSize = 8;
    runningWatchdogConfig.maxEventsPerBlock = 2;

    yesdaw::plugin_host::PluginHostCoordinator runningWatchdogCoordinator {
        std::chrono::milliseconds (500),
        std::chrono::milliseconds (25)
    };
    const auto runningWatchdog =
        runningWatchdogCoordinator.launchAndExpectRunningWatchdogTimeout (
            workerExecutable, runningWatchdogConfig);
    const auto runningWatchdogStatus = runningWatchdogCoordinator.status();
    const auto runningWatchdogFailure = runningWatchdogCoordinator.hostFailureReport();
    const auto runningWatchdogAction = runningWatchdogCoordinator.failureActionRequest();
    const auto autoQueuedRunningWatchdogAction =
        runningWatchdogCoordinator.pendingFailureActionRequest();

    yesdaw::engine::Runtime placeholderRecoveryRuntime;
    std::vector<float> placeholderRecoveryOut (8u, 0.0f);
    yesdaw::engine::GraphBuildError liveRecoveryBuildError;
    auto liveRecoveryGraph =
        yesdaw::engine::GraphBuilder::build (recoveryGraphInputs (false, 1), &liveRecoveryBuildError);
    const bool liveRecoveryGraphBuilt = liveRecoveryGraph != nullptr;
    const bool liveRecoveryPublishAccepted =
        liveRecoveryGraphBuilt && placeholderRecoveryRuntime.publish (std::move (liveRecoveryGraph));
    if (liveRecoveryPublishAccepted)
        placeholderRecoveryRuntime.processBlock (placeholderRecoveryOut.data(),
                                                 static_cast<int> (placeholderRecoveryOut.size()));
    const bool liveRecoveryOutputAudible = allSamplesNear (placeholderRecoveryOut, 0.75f);

    const auto placeholderRecoveryResult =
        runningWatchdogCoordinator.executePendingFailureActionRequestToPlaceholderGraph (
            placeholderRecoveryRuntime,
            recoveryGraphInputs (true, 2),
            kRecoveryOffenderId);
    if (placeholderRecoveryResult.orderedPublishAccepted)
        placeholderRecoveryRuntime.processBlock (placeholderRecoveryOut.data(),
                                                 static_cast<int> (placeholderRecoveryOut.size()));
    const bool placeholderRecoveryOutputSilent = allSamplesNear (placeholderRecoveryOut, 0.0f);
    const std::size_t placeholderRecoveryReclaimed = placeholderRecoveryRuntime.reclaim();

    yesdaw::plugin_host::PluginHostCoordinator missingPlaceholderRecoveryCoordinator;
    const auto missingPlaceholderQueuedAction =
        missingPlaceholderRecoveryCoordinator.queueFailureActionRequest (runningWatchdogAction);
    yesdaw::engine::Runtime missingPlaceholderRuntime;
    const auto missingPlaceholderRecoveryResult =
        missingPlaceholderRecoveryCoordinator.executePendingFailureActionRequestToPlaceholderGraph (
            missingPlaceholderRuntime,
            recoveryGraphInputs (false, 3),
            kRecoveryOffenderId);

    const auto queuedRunningWatchdogAction =
        runningWatchdogCoordinator.queueFailureActionRequestForCurrentFailure();
    const auto runningWatchdogCommandResult =
        runningWatchdogCoordinator.drainPendingFailureActionRequestToControlCommand();
    const auto queuedRunningWatchdogBlacklistCandidate =
        runningWatchdogCoordinator.queueBlacklistCandidateForCurrentFailure();
    const auto runningWatchdogBlacklistEscalationResult =
        runningWatchdogCoordinator.drainPendingBlacklistCandidateToControlEscalation();
    if (runningWatchdog.status
            != yesdaw::plugin_host::PluginHostCoordinator::WatchdogStatus::timeoutKilled
        || ! runningWatchdog.readySeen
        || ! runningWatchdog.watchdogTimedOut
        || ! runningWatchdog.killRequested
        || ! runningWatchdog.connectionLostSeen
        || ! runningWatchdog.runningRtLaneBacklogSeen
        || ! runningWatchdog.runningRtLaneOutputProgressSeen
        || runningWatchdog.runningRtLaneInputSeq <= runningWatchdog.runningRtLaneOutputSeq
        || runningWatchdogStatus.state != yesdaw::plugin_host::PluginHostCoordinator::ChildState::stopped
        || runningWatchdogStatus.watchdogStatus
            != yesdaw::plugin_host::PluginHostCoordinator::WatchdogStatus::timeoutKilled
        || runningWatchdogStatus.failureKind
            != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout
        || ! runningWatchdogStatus.watchdogTimedOut
        || ! runningWatchdogStatus.watchdogKillRequested
        || ! runningWatchdogStatus.connectionLostSeen
        || runningWatchdogFailure.kind
            != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout
        || ! runningWatchdogFailure.connectionLostSeen
        || ! runningWatchdogFailure.watchdogTimedOut
        || ! runningWatchdogFailure.watchdogKillRequested
        || ! requestMatches (
            runningWatchdogAction,
            { yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::bypassAndRecompile,
              yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout,
              true,
              true })
        || ! requestMatches (autoQueuedRunningWatchdogAction, runningWatchdogAction)
        || ! liveRecoveryGraphBuilt
        || liveRecoveryBuildError.code() != yesdaw::engine::GraphBuildError::Code::None
        || ! liveRecoveryPublishAccepted
        || ! liveRecoveryOutputAudible
        || placeholderRecoveryResult.status
            != yesdaw::plugin_host::PluginHostCoordinator::GraphRecompileStatus::graphPublished
        || ! placeholderRecoveryResult.pendingRequestConsumed
        || ! placeholderRecoveryResult.placeholderCompiled
        || ! placeholderRecoveryResult.orderedPublishAccepted
        || ! placeholderRecoveryResult.graphRecompileExecuted
        || ! placeholderRecoveryResult.commandResult.graphRecompileExecuted
        || placeholderRecoveryResult.commandResult.command.failureKind
            != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout
        || ! placeholderRecoveryOutputSilent
        || placeholderRecoveryReclaimed == 0u
        || ! requestMatches (missingPlaceholderQueuedAction, runningWatchdogAction)
        || missingPlaceholderRecoveryResult.status
            != yesdaw::plugin_host::PluginHostCoordinator::GraphRecompileStatus::missingPlaceholder
        || missingPlaceholderRecoveryResult.placeholderCompiled
        || missingPlaceholderRecoveryResult.orderedPublishAccepted
        || missingPlaceholderRecoveryResult.graphRecompileExecuted
        || ! requestMatches (queuedRunningWatchdogAction, runningWatchdogAction)
        || runningWatchdogCommandResult.status
            != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::commandReady
        || ! runningWatchdogCommandResult.pendingRequestConsumed
        || runningWatchdogCommandResult.graphRecompileExecuted
        || runningWatchdogCommandResult.command.failureKind
            != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout
        || ! blacklistCandidateMatches (
            queuedRunningWatchdogBlacklistCandidate,
            { yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout,
              true,
              false,
              true })
        || runningWatchdogBlacklistEscalationResult.status
            != yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalationStatus::escalationReady
        || ! runningWatchdogBlacklistEscalationResult.pendingCandidateConsumed
        || runningWatchdogBlacklistEscalationResult.blacklistPolicyApplied
        || runningWatchdogBlacklistEscalationResult.blacklistStatePersisted)
    {
        std::printf ("FAIL: plugin host coordinator running RT-lane watchdog is wrong: status=%s ready=%d timedOut=%d kill=%d lost=%d backlog=%d progress=%d inputSeq=%llu outputSeq=%llu state=%s watchdogStatus=%s failure=%s action=%s/%s autoQueued=%s/%s recovery=%s consumed=%d placeholder=%d published=%d executed=%d outputSilent=%d reclaimed=%zu liveBuilt=%d liveBuildError=%d livePublished=%d liveOutput=%d missingQueued=%s/%s missingRecovery=%s missingPlaceholder=%d missingPublished=%d missingExecuted=%d queued=%s/%s command=%s/%s consumed=%d executed=%d blacklist=%s/%d/%d/%d escalation=%s consumed=%d policy=%d persisted=%d\n",
                     statusName (runningWatchdog.status),
                     runningWatchdog.readySeen ? 1 : 0,
                     runningWatchdog.watchdogTimedOut ? 1 : 0,
                     runningWatchdog.killRequested ? 1 : 0,
                     runningWatchdog.connectionLostSeen ? 1 : 0,
                     runningWatchdog.runningRtLaneBacklogSeen ? 1 : 0,
                     runningWatchdog.runningRtLaneOutputProgressSeen ? 1 : 0,
                     static_cast<unsigned long long> (runningWatchdog.runningRtLaneInputSeq),
                     static_cast<unsigned long long> (runningWatchdog.runningRtLaneOutputSeq),
                     statusName (runningWatchdogStatus.state),
                     statusName (runningWatchdogStatus.watchdogStatus),
                     statusName (runningWatchdogFailure.kind),
                     statusName (runningWatchdogAction.action),
                     statusName (runningWatchdogAction.failureKind),
                     statusName (autoQueuedRunningWatchdogAction.action),
                     statusName (autoQueuedRunningWatchdogAction.failureKind),
                     statusName (placeholderRecoveryResult.status),
                     placeholderRecoveryResult.pendingRequestConsumed ? 1 : 0,
                     placeholderRecoveryResult.placeholderCompiled ? 1 : 0,
                     placeholderRecoveryResult.orderedPublishAccepted ? 1 : 0,
                     placeholderRecoveryResult.graphRecompileExecuted ? 1 : 0,
                     placeholderRecoveryOutputSilent ? 1 : 0,
                     placeholderRecoveryReclaimed,
                     liveRecoveryGraphBuilt ? 1 : 0,
                     static_cast<int> (liveRecoveryBuildError.code()),
                     liveRecoveryPublishAccepted ? 1 : 0,
                     liveRecoveryOutputAudible ? 1 : 0,
                     statusName (missingPlaceholderQueuedAction.action),
                     statusName (missingPlaceholderQueuedAction.failureKind),
                     statusName (missingPlaceholderRecoveryResult.status),
                     missingPlaceholderRecoveryResult.placeholderCompiled ? 1 : 0,
                     missingPlaceholderRecoveryResult.orderedPublishAccepted ? 1 : 0,
                     missingPlaceholderRecoveryResult.graphRecompileExecuted ? 1 : 0,
                     statusName (queuedRunningWatchdogAction.action),
                     statusName (queuedRunningWatchdogAction.failureKind),
                     statusName (runningWatchdogCommandResult.command.command),
                     statusName (runningWatchdogCommandResult.command.failureKind),
                     runningWatchdogCommandResult.pendingRequestConsumed ? 1 : 0,
                     runningWatchdogCommandResult.graphRecompileExecuted ? 1 : 0,
                     statusName (queuedRunningWatchdogBlacklistCandidate.failureKind),
                     queuedRunningWatchdogBlacklistCandidate.candidate ? 1 : 0,
                     queuedRunningWatchdogBlacklistCandidate.crashCandidate ? 1 : 0,
                     queuedRunningWatchdogBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (runningWatchdogBlacklistEscalationResult.status),
                     runningWatchdogBlacklistEscalationResult.pendingCandidateConsumed ? 1 : 0,
                     runningWatchdogBlacklistEscalationResult.blacklistPolicyApplied ? 1 : 0,
                     runningWatchdogBlacklistEscalationResult.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    yesdaw::plugin_host::PluginHostCoordinator resetStateCoordinator;
    const auto staleQueuedAction =
        resetStateCoordinator.queueFailureActionRequest (runningWatchdogAction);
    const auto staleCommandResult =
        resetStateCoordinator.drainPendingFailureActionRequestToControlCommand();
    const auto staleDeferredCommandStatus =
        resetStateCoordinator.recordDeferredGraphChangeCommandResult (staleCommandResult);
    const auto staleQueuedActionAfterDeferred =
        resetStateCoordinator.queueFailureActionRequest (runningWatchdogAction);
    const auto staleQueuedBlacklistCandidate =
        resetStateCoordinator.queueBlacklistCandidate (
            { yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout,
              true,
              false,
              true });
    const auto resetHandshake = resetStateCoordinator.launchAndHandshake (workerExecutable);
    const auto resetPendingAction = resetStateCoordinator.pendingFailureActionRequest();
    const auto resetDeferredCommandStatus = resetStateCoordinator.deferredGraphChangeCommandStatus();
    const auto resetPendingBlacklistCandidate = resetStateCoordinator.pendingBlacklistCandidateStatus();
    const auto resetStop = resetStateCoordinator.requestStopAndWait();
    if (! requestMatches (staleQueuedAction, runningWatchdogAction)
        || staleCommandResult.status
            != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::commandReady
        || ! staleDeferredCommandStatus.commandRecorded
        || ! requestMatches (staleQueuedActionAfterDeferred, runningWatchdogAction)
        || ! staleQueuedBlacklistCandidate.candidate
        || resetHandshake.status != yesdaw::plugin_host::PluginHostCoordinator::HandshakeStatus::success
        || resetPendingAction.action != yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::none
        || resetPendingAction.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none
        || resetPendingAction.bypassRequested
        || resetPendingAction.recompileRequested
        || resetDeferredCommandStatus.status
            != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::noAction
        || resetDeferredCommandStatus.commandRecorded
        || resetDeferredCommandStatus.graphRecompileExecuted
        || resetPendingBlacklistCandidate.candidate
        || resetPendingBlacklistCandidate.failureKind
            != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none
        || resetStop.status != yesdaw::plugin_host::PluginHostCoordinator::StopStatus::stopped)
    {
        std::printf ("FAIL: plugin host coordinator resetState left stale recovery pipelines: staleAction=%s/%s staleCommand=%s/%s staleDeferred=%s/%d staleQueuedAgain=%s/%s staleBlacklist=%s/%d handshake=%s pending=%s/%s/%d/%d deferred=%s/%d/%d blacklist=%s/%d stop=%s\n",
                     statusName (staleQueuedAction.action),
                     statusName (staleQueuedAction.failureKind),
                     statusName (staleCommandResult.status),
                     statusName (staleCommandResult.command.failureKind),
                     statusName (staleDeferredCommandStatus.status),
                     staleDeferredCommandStatus.commandRecorded ? 1 : 0,
                     statusName (staleQueuedActionAfterDeferred.action),
                     statusName (staleQueuedActionAfterDeferred.failureKind),
                     statusName (staleQueuedBlacklistCandidate.failureKind),
                     staleQueuedBlacklistCandidate.candidate ? 1 : 0,
                     statusName (resetHandshake.status),
                     statusName (resetPendingAction.action),
                     statusName (resetPendingAction.failureKind),
                     resetPendingAction.bypassRequested ? 1 : 0,
                     resetPendingAction.recompileRequested ? 1 : 0,
                     statusName (resetDeferredCommandStatus.status),
                     resetDeferredCommandStatus.commandRecorded ? 1 : 0,
                     resetDeferredCommandStatus.graphRecompileExecuted ? 1 : 0,
                     statusName (resetPendingBlacklistCandidate.failureKind),
                     resetPendingBlacklistCandidate.candidate ? 1 : 0,
                     statusName (resetStop.status));
        return 2;
    }

    const yesdaw::plugin_host::RtLaneLoadConfig rtLaneLoadConfig {
        1u,
        8u,
        2u,
        rtLaneConfig.lastGoodHoldBlocks,
        rtLaneConfig.bypassAfterMisses
    };

    yesdaw::plugin_host::PluginHostCoordinator missingRtLaneIdentityCoordinator;
    const auto missingRtLaneIdentity =
        missingRtLaneIdentityCoordinator.launchAndSendRtLaneLoadIdentity (
            workerExecutable,
            { {}, rtLaneLoadConfig });
    const auto missingRtLaneStop = missingRtLaneIdentityCoordinator.requestStopAndWait();
    if (missingRtLaneIdentity.status
            != yesdaw::plugin_host::PluginHostCoordinator::RtLaneLoadStatus::workerRejected
        || missingRtLaneIdentity.workerReplyStatus
            != yesdaw::plugin_host::RtLaneLoadReplyStatus::rejectedInvalidIdentity
        || missingRtLaneIdentity.workerAttachFailure != yesdaw::engine::RtLaneAttachFailure::None
        || ! missingRtLaneIdentity.readySeen
        || ! missingRtLaneIdentity.loadMessageSent
        || ! missingRtLaneIdentity.loadReplySeen
        || missingRtLaneIdentity.coordinatorAllocated
        || missingRtLaneIdentity.coordinatorUsesOsSharedMemory
        || missingRtLaneIdentity.workerAccepted
        || missingRtLaneStop.status != yesdaw::plugin_host::PluginHostCoordinator::StopStatus::stopped)
    {
        std::printf ("FAIL: plugin host worker accepted or fabricated storage for a missing RT-lane identity: status=%s reply=%s attach=%s/%d ready=%d sent=%d replySeen=%d allocated=%d os=%d accepted=%d stop=%s\n",
                     statusName (missingRtLaneIdentity.status),
                     statusName (missingRtLaneIdentity.workerReplyStatus),
                     statusName (missingRtLaneIdentity.workerAttachFailure),
                     missingRtLaneIdentity.workerAttachSystemError,
                     missingRtLaneIdentity.readySeen ? 1 : 0,
                     missingRtLaneIdentity.loadMessageSent ? 1 : 0,
                     missingRtLaneIdentity.loadReplySeen ? 1 : 0,
                     missingRtLaneIdentity.coordinatorAllocated ? 1 : 0,
                     missingRtLaneIdentity.coordinatorUsesOsSharedMemory ? 1 : 0,
                     missingRtLaneIdentity.workerAccepted ? 1 : 0,
                     statusName (missingRtLaneStop.status));
        return 2;
    }

    yesdaw::plugin_host::PluginHostCoordinator absentRtLaneIdentityCoordinator;
    const std::string absentRtLaneName = yesdaw::engine::RtLaneRing::makeUniqueSharedMemoryName();
    const auto absentRtLaneIdentity =
        absentRtLaneIdentityCoordinator.launchAndSendRtLaneLoadIdentity (
            workerExecutable,
            { absentRtLaneName, rtLaneLoadConfig });
    const auto absentRtLaneStop = absentRtLaneIdentityCoordinator.requestStopAndWait();
    if (absentRtLaneIdentity.status
            != yesdaw::plugin_host::PluginHostCoordinator::RtLaneLoadStatus::workerRejected
        || absentRtLaneIdentity.workerReplyStatus
            != yesdaw::plugin_host::RtLaneLoadReplyStatus::rejectedAttachFailed
        || absentRtLaneIdentity.workerAttachFailure != yesdaw::engine::RtLaneAttachFailure::OpenFailed
        || ! absentRtLaneIdentity.readySeen
        || ! absentRtLaneIdentity.loadMessageSent
        || ! absentRtLaneIdentity.loadReplySeen
        || absentRtLaneIdentity.coordinatorAllocated
        || absentRtLaneIdentity.coordinatorUsesOsSharedMemory
        || absentRtLaneIdentity.workerAccepted
        || absentRtLaneIdentity.identity.sharedMemoryName != absentRtLaneName
        || absentRtLaneStop.status != yesdaw::plugin_host::PluginHostCoordinator::StopStatus::stopped)
    {
        std::printf ("FAIL: plugin host worker accepted or fabricated storage for an absent RT-lane region: status=%s reply=%s attach=%s/%d ready=%d sent=%d replySeen=%d allocated=%d os=%d accepted=%d name=%s stop=%s\n",
                     statusName (absentRtLaneIdentity.status),
                     statusName (absentRtLaneIdentity.workerReplyStatus),
                     statusName (absentRtLaneIdentity.workerAttachFailure),
                     absentRtLaneIdentity.workerAttachSystemError,
                     absentRtLaneIdentity.readySeen ? 1 : 0,
                     absentRtLaneIdentity.loadMessageSent ? 1 : 0,
                     absentRtLaneIdentity.loadReplySeen ? 1 : 0,
                     absentRtLaneIdentity.coordinatorAllocated ? 1 : 0,
                     absentRtLaneIdentity.coordinatorUsesOsSharedMemory ? 1 : 0,
                     absentRtLaneIdentity.workerAccepted ? 1 : 0,
                     absentRtLaneIdentity.identity.sharedMemoryName.c_str(),
                     statusName (absentRtLaneStop.status));
        return 2;
    }

    yesdaw::plugin_host::PluginHostCoordinator coordinator;
    const auto initialStatus = coordinator.status();
    const auto initialPendingAction = coordinator.pendingFailureActionRequest();
    const auto initialDeferredCommandStatus = coordinator.deferredGraphChangeCommandStatus();
    const auto initialBlacklistCandidate = coordinator.blacklistCandidateStatus();
    const auto initialPendingBlacklistCandidate = coordinator.pendingBlacklistCandidateStatus();
    const auto initialBlacklistEscalationResult = coordinator.drainPendingBlacklistCandidateToControlEscalation();
    const auto initialDeferredBlacklistEscalationStatus = coordinator.deferredBlacklistEscalationStatus();
    const auto initialBlacklistPolicyDecisionRequest = coordinator.blacklistPolicyDecisionRequest();
    const auto initialQueuedBlacklistPolicyDecisionRequest =
        coordinator.queueBlacklistPolicyDecisionRequestForDeferredEscalation();
    const auto initialPendingBlacklistPolicyDecisionRequest =
        coordinator.pendingBlacklistPolicyDecisionRequest();
    const auto initialDrainedBlacklistPolicyDecisionRequest =
        coordinator.drainPendingBlacklistPolicyDecisionRequest();
    const auto initialDeferredBlacklistEscalationAcknowledgement =
        coordinator.acknowledgeDeferredBlacklistEscalationStatus();
    const auto initialAfterAcknowledgedBlacklistPolicyDecisionRequest =
        coordinator.blacklistPolicyDecisionRequest();
    const auto initialAfterDrainedBlacklistPolicyDecisionRequest =
        coordinator.drainPendingBlacklistPolicyDecisionRequest();
    const auto initialBlacklistPolicyDecisionCommandResult =
        coordinator.drainPendingBlacklistPolicyDecisionRequestToControlCommand();
    const auto initialDeferredBlacklistPolicyDecisionCommandStatus =
        coordinator.deferredBlacklistPolicyDecisionCommandStatus();
    const auto initialDeferredBlacklistPolicyDecisionCommandRecord =
        coordinator.recordDeferredBlacklistPolicyDecisionCommandResult (
            initialBlacklistPolicyDecisionCommandResult);
    const auto initialDeferredBlacklistPolicyDecisionCommandInspection =
        coordinator.deferredBlacklistPolicyDecisionCommandStatus();
    const auto initialDeferredBlacklistPolicyDecisionCommandAcknowledgement =
        coordinator.acknowledgeDeferredBlacklistPolicyDecisionCommandStatus();
    const auto initialBlacklistPolicyDecisionOutcome = coordinator.blacklistPolicyDecisionOutcomeStatus();
    const auto initialQueuedBlacklistPolicyDecisionOutcome =
        coordinator.queueBlacklistPolicyDecisionOutcomeForDeferredCommand();
    const auto initialPendingBlacklistPolicyDecisionOutcome =
        coordinator.pendingBlacklistPolicyDecisionOutcomeStatus();
    const auto initialDrainedBlacklistPolicyDecisionOutcome =
        coordinator.drainPendingBlacklistPolicyDecisionOutcomeStatus();
    const auto initialBlacklistPolicyDecisionOutcomeHandlingResult =
        coordinator.drainPendingBlacklistPolicyDecisionOutcomeToControlHandling();
    const auto initialDeferredBlacklistPolicyDecisionOutcomeHandlingStatus =
        coordinator.deferredBlacklistPolicyDecisionOutcomeHandlingStatus();
    const auto initialDeferredBlacklistPolicyDecisionOutcomeHandlingRecord =
        coordinator.recordDeferredBlacklistPolicyDecisionOutcomeHandlingResult (
            initialBlacklistPolicyDecisionOutcomeHandlingResult);
    const auto initialDeferredBlacklistPolicyDecisionOutcomeHandlingInspection =
        coordinator.deferredBlacklistPolicyDecisionOutcomeHandlingStatus();
    const auto initialDeferredBlacklistPolicyDecisionOutcomeHandlingAcknowledgement =
        coordinator.acknowledgeDeferredBlacklistPolicyDecisionOutcomeHandlingStatus();
    const auto initialBlacklistHandlingRequest = coordinator.blacklistHandlingRequest();

    if (initialStatus.state != yesdaw::plugin_host::PluginHostCoordinator::ChildState::idle
        || initialStatus.handshakeStatus != yesdaw::plugin_host::PluginHostCoordinator::HandshakeStatus::notStarted
        || initialStatus.stopStatus != yesdaw::plugin_host::PluginHostCoordinator::StopStatus::notStarted
        || initialStatus.watchdogStatus != yesdaw::plugin_host::PluginHostCoordinator::WatchdogStatus::notStarted
        || initialStatus.crashStatus != yesdaw::plugin_host::PluginHostCoordinator::CrashStatus::notStarted
        || initialStatus.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none
        || initialStatus.launchAttempted
        || initialStatus.readySeen
        || initialStatus.probeEchoed
        || initialStatus.stopRequested
        || initialStatus.watchdogTimedOut
        || initialStatus.watchdogKillRequested
        || initialStatus.crashObservationRequested
        || initialStatus.connectionLostSeen)
    {
        std::printf ("FAIL: plugin host coordinator initial status is not idle: state=%s handshake=%s stop=%s watchdog=%s crash=%s failure=%s launch=%d ready=%d echo=%d stopRequested=%d watchdogTimeout=%d watchdogKill=%d crashObservation=%d connectionLost=%d\n",
                     statusName (initialStatus.state),
                     statusName (initialStatus.handshakeStatus),
                     statusName (initialStatus.stopStatus),
                     statusName (initialStatus.watchdogStatus),
                     statusName (initialStatus.crashStatus),
                     statusName (initialStatus.failureKind),
                     initialStatus.launchAttempted ? 1 : 0,
                     initialStatus.readySeen ? 1 : 0,
                     initialStatus.probeEchoed ? 1 : 0,
                     initialStatus.stopRequested ? 1 : 0,
                     initialStatus.watchdogTimedOut ? 1 : 0,
                     initialStatus.watchdogKillRequested ? 1 : 0,
                     initialStatus.crashObservationRequested ? 1 : 0,
                     initialStatus.connectionLostSeen ? 1 : 0);
        return 2;
    }

    if (initialPendingAction.action != yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::none
        || initialPendingAction.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none
        || initialPendingAction.bypassRequested
        || initialPendingAction.recompileRequested)
    {
        std::printf ("FAIL: plugin host coordinator initial pending action request is wrong: action=%s failure=%s bypass=%d recompile=%d\n",
                     statusName (initialPendingAction.action),
                     statusName (initialPendingAction.failureKind),
                     initialPendingAction.bypassRequested ? 1 : 0,
                     initialPendingAction.recompileRequested ? 1 : 0);
        return 2;
    }

    if (initialDeferredCommandStatus.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::noAction
        || initialDeferredCommandStatus.lastResult.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::noAction
        || initialDeferredCommandStatus.commandRecorded
        || initialDeferredCommandStatus.graphRecompileExecuted)
    {
        std::printf ("FAIL: plugin host coordinator initial deferred command status is wrong: status=%s last=%s recorded=%d executed=%d\n",
                     statusName (initialDeferredCommandStatus.status),
                     statusName (initialDeferredCommandStatus.lastResult.status),
                     initialDeferredCommandStatus.commandRecorded ? 1 : 0,
                     initialDeferredCommandStatus.graphRecompileExecuted ? 1 : 0);
        return 2;
    }

    if (! blacklistCandidateMatches (initialBlacklistCandidate, {}))
    {
        std::printf ("FAIL: plugin host coordinator initial blacklist-candidate status is wrong: failure=%s candidate=%d crash=%d watchdog=%d\n",
                     statusName (initialBlacklistCandidate.failureKind),
                     initialBlacklistCandidate.candidate ? 1 : 0,
                     initialBlacklistCandidate.crashCandidate ? 1 : 0,
                     initialBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0);
        return 2;
    }

    if (! blacklistCandidateMatches (initialPendingBlacklistCandidate, {}))
    {
        std::printf ("FAIL: plugin host coordinator initial pending blacklist-candidate status is wrong: failure=%s candidate=%d crash=%d watchdog=%d\n",
                     statusName (initialPendingBlacklistCandidate.failureKind),
                     initialPendingBlacklistCandidate.candidate ? 1 : 0,
                     initialPendingBlacklistCandidate.crashCandidate ? 1 : 0,
                     initialPendingBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0);
        return 2;
    }

    if (initialBlacklistEscalationResult.status != yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalationStatus::noAction
        || ! blacklistCandidateMatches (initialBlacklistEscalationResult.drainedCandidate, {})
        || ! blacklistEscalationMatches (initialBlacklistEscalationResult.escalation, {})
        || initialBlacklistEscalationResult.pendingCandidateConsumed
        || initialBlacklistEscalationResult.blacklistPolicyApplied
        || initialBlacklistEscalationResult.blacklistStatePersisted)
    {
        std::printf ("FAIL: plugin host coordinator initial blacklist escalation result is wrong: status=%s drained=%s/%d/%d/%d escalation=%s/%d/%d/%d consumed=%d policy=%d persisted=%d\n",
                     statusName (initialBlacklistEscalationResult.status),
                     statusName (initialBlacklistEscalationResult.drainedCandidate.failureKind),
                     initialBlacklistEscalationResult.drainedCandidate.candidate ? 1 : 0,
                     initialBlacklistEscalationResult.drainedCandidate.crashCandidate ? 1 : 0,
                     initialBlacklistEscalationResult.drainedCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (initialBlacklistEscalationResult.escalation.failureKind),
                     initialBlacklistEscalationResult.escalation.crashCandidate ? 1 : 0,
                     initialBlacklistEscalationResult.escalation.watchdogTimeoutCandidate ? 1 : 0,
                     initialBlacklistEscalationResult.escalation.controlThreadEscalationRequested ? 1 : 0,
                     initialBlacklistEscalationResult.pendingCandidateConsumed ? 1 : 0,
                     initialBlacklistEscalationResult.blacklistPolicyApplied ? 1 : 0,
                     initialBlacklistEscalationResult.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    if (! deferredBlacklistEscalationStatusMatches (initialDeferredBlacklistEscalationStatus, {}))
    {
        std::printf ("FAIL: plugin host coordinator initial deferred blacklist escalation status is wrong: status=%s last=%s recorded=%d policy=%d persisted=%d\n",
                     statusName (initialDeferredBlacklistEscalationStatus.status),
                     statusName (initialDeferredBlacklistEscalationStatus.lastResult.status),
                     initialDeferredBlacklistEscalationStatus.escalationRecorded ? 1 : 0,
                     initialDeferredBlacklistEscalationStatus.blacklistPolicyApplied ? 1 : 0,
                     initialDeferredBlacklistEscalationStatus.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    if (! deferredBlacklistEscalationStatusMatches (initialDeferredBlacklistEscalationAcknowledgement, {}))
    {
        std::printf ("FAIL: plugin host coordinator initial deferred blacklist escalation acknowledge/clear status is wrong: status=%s last=%s recorded=%d policy=%d persisted=%d\n",
                     statusName (initialDeferredBlacklistEscalationAcknowledgement.status),
                     statusName (initialDeferredBlacklistEscalationAcknowledgement.lastResult.status),
                     initialDeferredBlacklistEscalationAcknowledgement.escalationRecorded ? 1 : 0,
                     initialDeferredBlacklistEscalationAcknowledgement.blacklistPolicyApplied ? 1 : 0,
                     initialDeferredBlacklistEscalationAcknowledgement.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    if (! blacklistPolicyDecisionRequestMatches (initialBlacklistPolicyDecisionRequest, {})
        || ! blacklistPolicyDecisionRequestMatches (initialQueuedBlacklistPolicyDecisionRequest, {})
        || ! blacklistPolicyDecisionRequestMatches (initialPendingBlacklistPolicyDecisionRequest, {})
        || ! blacklistPolicyDecisionRequestMatches (initialDrainedBlacklistPolicyDecisionRequest, {})
        || ! blacklistPolicyDecisionRequestMatches (initialAfterAcknowledgedBlacklistPolicyDecisionRequest, {})
        || ! blacklistPolicyDecisionRequestMatches (initialAfterDrainedBlacklistPolicyDecisionRequest, {})
        || ! blacklistPolicyDecisionCommandResultMatches (initialBlacklistPolicyDecisionCommandResult, {})
        || ! deferredBlacklistPolicyDecisionCommandStatusMatches (
            initialDeferredBlacklistPolicyDecisionCommandStatus, {})
        || ! deferredBlacklistPolicyDecisionCommandStatusMatches (
            initialDeferredBlacklistPolicyDecisionCommandRecord, {})
        || ! deferredBlacklistPolicyDecisionCommandStatusMatches (
            initialDeferredBlacklistPolicyDecisionCommandInspection, {})
        || ! deferredBlacklistPolicyDecisionCommandStatusMatches (
            initialDeferredBlacklistPolicyDecisionCommandAcknowledgement, {}))
    {
        std::printf ("FAIL: plugin host coordinator initial blacklist policy-decision request/command should be empty: before=%s/%s/%d/%d/%d queued=%s/%s pending=%s/%s drained=%s/%s after=%s/%s/%d/%d/%d afterDrain=%s/%s command=%s/%s/%s consumed=%d receipt=%s recorded=%d inspected=%s inspectedRecorded=%d acknowledged=%s acknowledgedRecorded=%d policy=%d persisted=%d afterPolicy=%d afterPersisted=%d commandPolicy=%d commandPersisted=%d receiptPolicy=%d receiptPersisted=%d inspectedPolicy=%d inspectedPersisted=%d acknowledgedPolicy=%d acknowledgedPersisted=%d\n",
                     statusName (initialBlacklistPolicyDecisionRequest.status),
                     statusName (initialBlacklistPolicyDecisionRequest.failureKind),
                     initialBlacklistPolicyDecisionRequest.crashCandidate ? 1 : 0,
                     initialBlacklistPolicyDecisionRequest.watchdogTimeoutCandidate ? 1 : 0,
                     initialBlacklistPolicyDecisionRequest.controlThreadPolicyDecisionRequested ? 1 : 0,
                     statusName (initialQueuedBlacklistPolicyDecisionRequest.status),
                     statusName (initialQueuedBlacklistPolicyDecisionRequest.failureKind),
                     statusName (initialPendingBlacklistPolicyDecisionRequest.status),
                     statusName (initialPendingBlacklistPolicyDecisionRequest.failureKind),
                     statusName (initialDrainedBlacklistPolicyDecisionRequest.status),
                     statusName (initialDrainedBlacklistPolicyDecisionRequest.failureKind),
                     statusName (initialAfterAcknowledgedBlacklistPolicyDecisionRequest.status),
                     statusName (initialAfterAcknowledgedBlacklistPolicyDecisionRequest.failureKind),
                     initialAfterAcknowledgedBlacklistPolicyDecisionRequest.crashCandidate ? 1 : 0,
                     initialAfterAcknowledgedBlacklistPolicyDecisionRequest.watchdogTimeoutCandidate ? 1 : 0,
                     initialAfterAcknowledgedBlacklistPolicyDecisionRequest.controlThreadPolicyDecisionRequested ? 1 : 0,
                     statusName (initialAfterDrainedBlacklistPolicyDecisionRequest.status),
                     statusName (initialAfterDrainedBlacklistPolicyDecisionRequest.failureKind),
                     statusName (initialBlacklistPolicyDecisionCommandResult.status),
                     statusName (initialBlacklistPolicyDecisionCommandResult.command.command),
                     statusName (initialBlacklistPolicyDecisionCommandResult.command.failureKind),
                     initialBlacklistPolicyDecisionCommandResult.pendingRequestConsumed ? 1 : 0,
                     statusName (initialDeferredBlacklistPolicyDecisionCommandRecord.status),
                     initialDeferredBlacklistPolicyDecisionCommandRecord.commandRecorded ? 1 : 0,
                     statusName (initialDeferredBlacklistPolicyDecisionCommandInspection.status),
                     initialDeferredBlacklistPolicyDecisionCommandInspection.commandRecorded ? 1 : 0,
                     statusName (initialDeferredBlacklistPolicyDecisionCommandAcknowledgement.status),
                     initialDeferredBlacklistPolicyDecisionCommandAcknowledgement.commandRecorded ? 1 : 0,
                     initialBlacklistPolicyDecisionRequest.blacklistPolicyApplied ? 1 : 0,
                     initialBlacklistPolicyDecisionRequest.blacklistStatePersisted ? 1 : 0,
                     initialAfterAcknowledgedBlacklistPolicyDecisionRequest.blacklistPolicyApplied ? 1 : 0,
                     initialAfterAcknowledgedBlacklistPolicyDecisionRequest.blacklistStatePersisted ? 1 : 0,
                     initialBlacklistPolicyDecisionCommandResult.blacklistPolicyApplied ? 1 : 0,
                     initialBlacklistPolicyDecisionCommandResult.blacklistStatePersisted ? 1 : 0,
                     initialDeferredBlacklistPolicyDecisionCommandRecord.blacklistPolicyApplied ? 1 : 0,
                     initialDeferredBlacklistPolicyDecisionCommandRecord.blacklistStatePersisted ? 1 : 0,
                     initialDeferredBlacklistPolicyDecisionCommandInspection.blacklistPolicyApplied ? 1 : 0,
                     initialDeferredBlacklistPolicyDecisionCommandInspection.blacklistStatePersisted ? 1 : 0,
                     initialDeferredBlacklistPolicyDecisionCommandAcknowledgement.blacklistPolicyApplied ? 1 : 0,
                     initialDeferredBlacklistPolicyDecisionCommandAcknowledgement.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    if (! blacklistPolicyDecisionOutcomeMatches (initialBlacklistPolicyDecisionOutcome, {})
        || ! blacklistPolicyDecisionOutcomeMatches (initialQueuedBlacklistPolicyDecisionOutcome, {})
        || ! blacklistPolicyDecisionOutcomeMatches (initialPendingBlacklistPolicyDecisionOutcome, {})
        || ! blacklistPolicyDecisionOutcomeMatches (initialDrainedBlacklistPolicyDecisionOutcome, {})
        || ! blacklistPolicyDecisionOutcomeHandlingResultMatches (
            initialBlacklistPolicyDecisionOutcomeHandlingResult, {})
        || ! deferredBlacklistPolicyDecisionOutcomeHandlingStatusMatches (
            initialDeferredBlacklistPolicyDecisionOutcomeHandlingStatus, {})
        || ! deferredBlacklistPolicyDecisionOutcomeHandlingStatusMatches (
            initialDeferredBlacklistPolicyDecisionOutcomeHandlingRecord, {})
        || ! deferredBlacklistPolicyDecisionOutcomeHandlingStatusMatches (
            initialDeferredBlacklistPolicyDecisionOutcomeHandlingInspection, {})
        || ! deferredBlacklistPolicyDecisionOutcomeHandlingStatusMatches (
            initialDeferredBlacklistPolicyDecisionOutcomeHandlingAcknowledgement, {})
        || ! blacklistHandlingRequestMatches (initialBlacklistHandlingRequest, {}))
    {
        std::printf ("FAIL: plugin host coordinator initial blacklist policy-decision outcome should be empty: outcome=%s/%s/%d/%d/%d queued=%s/%s pending=%s/%s drained=%s/%s handling=%s consumed=%d receipt=%s recorded=%d receiptRecord=%s receiptRecordRecorded=%d receiptInspection=%s receiptInspectionRecorded=%d receiptAck=%s receiptAckRecorded=%d blacklistRequest=%s/%s/%d/%d/%d policy=%d persisted=%d handlingPolicy=%d handlingPersisted=%d receiptPolicy=%d receiptPersisted=%d requestPolicy=%d requestPersisted=%d\n",
                     statusName (initialBlacklistPolicyDecisionOutcome.status),
                     statusName (initialBlacklistPolicyDecisionOutcome.failureKind),
                     initialBlacklistPolicyDecisionOutcome.crashCandidate ? 1 : 0,
                     initialBlacklistPolicyDecisionOutcome.watchdogTimeoutCandidate ? 1 : 0,
                     initialBlacklistPolicyDecisionOutcome.controlThreadPolicyDecisionInspected ? 1 : 0,
                     statusName (initialQueuedBlacklistPolicyDecisionOutcome.status),
                     statusName (initialQueuedBlacklistPolicyDecisionOutcome.failureKind),
                     statusName (initialPendingBlacklistPolicyDecisionOutcome.status),
                     statusName (initialPendingBlacklistPolicyDecisionOutcome.failureKind),
                     statusName (initialDrainedBlacklistPolicyDecisionOutcome.status),
                     statusName (initialDrainedBlacklistPolicyDecisionOutcome.failureKind),
                     statusName (initialBlacklistPolicyDecisionOutcomeHandlingResult.status),
                     initialBlacklistPolicyDecisionOutcomeHandlingResult.pendingOutcomeConsumed ? 1 : 0,
                     statusName (initialDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.status),
                     initialDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.handlingRecorded ? 1 : 0,
                     statusName (initialDeferredBlacklistPolicyDecisionOutcomeHandlingRecord.status),
                     initialDeferredBlacklistPolicyDecisionOutcomeHandlingRecord.handlingRecorded ? 1 : 0,
                     statusName (initialDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.status),
                     initialDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.handlingRecorded ? 1 : 0,
                     statusName (initialDeferredBlacklistPolicyDecisionOutcomeHandlingAcknowledgement.status),
                     initialDeferredBlacklistPolicyDecisionOutcomeHandlingAcknowledgement.handlingRecorded ? 1 : 0,
                     statusName (initialBlacklistHandlingRequest.status),
                     statusName (initialBlacklistHandlingRequest.failureKind),
                     initialBlacklistHandlingRequest.crashCandidate ? 1 : 0,
                     initialBlacklistHandlingRequest.watchdogTimeoutCandidate ? 1 : 0,
                     initialBlacklistHandlingRequest.controlThreadBlacklistHandlingRequested ? 1 : 0,
                     initialBlacklistPolicyDecisionOutcome.blacklistPolicyApplied ? 1 : 0,
                     initialBlacklistPolicyDecisionOutcome.blacklistStatePersisted ? 1 : 0,
                     initialBlacklistPolicyDecisionOutcomeHandlingResult.blacklistPolicyApplied ? 1 : 0,
                     initialBlacklistPolicyDecisionOutcomeHandlingResult.blacklistStatePersisted ? 1 : 0,
                     initialDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.blacklistPolicyApplied ? 1 : 0,
                     initialDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.blacklistStatePersisted ? 1 : 0,
                     initialBlacklistHandlingRequest.blacklistPolicyApplied ? 1 : 0,
                     initialBlacklistHandlingRequest.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    const auto initialQueuedBlacklistHandlingRequest =
        coordinator.queueBlacklistHandlingRequestForDeferredOutcomeHandling();
    const auto initialPendingBlacklistHandlingRequest = coordinator.pendingBlacklistHandlingRequest();
    const auto initialDrainedBlacklistHandlingRequest = coordinator.drainPendingBlacklistHandlingRequest();
    const auto initialAfterDrainedBlacklistHandlingRequest = coordinator.pendingBlacklistHandlingRequest();
    const auto initialBlacklistHandlingCommandResult =
        coordinator.drainPendingBlacklistHandlingRequestToControlCommand();
    const auto initialDeferredBlacklistHandlingCommandStatus =
        coordinator.deferredBlacklistHandlingCommandStatus();
    const auto initialDeferredBlacklistHandlingCommandRecord =
        coordinator.recordDeferredBlacklistHandlingCommandResult (initialBlacklistHandlingCommandResult);
    const auto initialDeferredBlacklistHandlingCommandInspection =
        coordinator.deferredBlacklistHandlingCommandStatus();
    const auto initialBlacklistHandlingOutcome = coordinator.blacklistHandlingOutcomeStatus();
    const auto initialQueuedBlacklistHandlingOutcome =
        coordinator.queueBlacklistHandlingOutcomeForDeferredCommand();
    const auto initialPendingBlacklistHandlingOutcome =
        coordinator.pendingBlacklistHandlingOutcomeStatus();
    const auto initialDrainedBlacklistHandlingOutcome =
        coordinator.drainPendingBlacklistHandlingOutcomeStatus();
    const auto initialAfterDrainedBlacklistHandlingOutcome =
        coordinator.pendingBlacklistHandlingOutcomeStatus();
    const auto initialBlacklistHandlingOutcomeHandlingResult =
        coordinator.drainPendingBlacklistHandlingOutcomeToControlHandling();
    const auto initialDeferredBlacklistHandlingOutcomeHandlingStatus =
        coordinator.deferredBlacklistHandlingOutcomeHandlingStatus();
    const auto initialDeferredBlacklistHandlingOutcomeHandlingRecord =
        coordinator.recordDeferredBlacklistHandlingOutcomeHandlingResult (
            initialBlacklistHandlingOutcomeHandlingResult);
    const auto initialDeferredBlacklistHandlingOutcomeHandlingInspection =
        coordinator.deferredBlacklistHandlingOutcomeHandlingStatus();
    const auto initialAcknowledgedDeferredBlacklistHandlingCommandStatus =
        coordinator.acknowledgeDeferredBlacklistHandlingCommandStatus();
    const auto initialAfterAcknowledgedDeferredBlacklistHandlingCommandStatus =
        coordinator.deferredBlacklistHandlingCommandStatus();
    const auto initialAfterAcknowledgedBlacklistHandlingOutcome =
        coordinator.blacklistHandlingOutcomeStatus();
    if (! blacklistHandlingRequestMatches (initialQueuedBlacklistHandlingRequest, {})
        || ! blacklistHandlingRequestMatches (initialPendingBlacklistHandlingRequest, {})
        || ! blacklistHandlingRequestMatches (initialDrainedBlacklistHandlingRequest, {})
        || ! blacklistHandlingRequestMatches (initialAfterDrainedBlacklistHandlingRequest, {})
        || ! blacklistHandlingCommandResultMatches (initialBlacklistHandlingCommandResult, {})
        || ! deferredBlacklistHandlingCommandStatusMatches (
            initialDeferredBlacklistHandlingCommandStatus, {})
        || ! deferredBlacklistHandlingCommandStatusMatches (
            initialDeferredBlacklistHandlingCommandRecord, {})
        || ! deferredBlacklistHandlingCommandStatusMatches (
            initialDeferredBlacklistHandlingCommandInspection, {})
        || ! deferredBlacklistHandlingCommandStatusMatches (
            initialAcknowledgedDeferredBlacklistHandlingCommandStatus, {})
        || ! deferredBlacklistHandlingCommandStatusMatches (
            initialAfterAcknowledgedDeferredBlacklistHandlingCommandStatus, {})
        || ! blacklistHandlingOutcomeMatches (initialBlacklistHandlingOutcome, {})
        || ! blacklistHandlingOutcomeMatches (initialQueuedBlacklistHandlingOutcome, {})
        || ! blacklistHandlingOutcomeMatches (initialPendingBlacklistHandlingOutcome, {})
        || ! blacklistHandlingOutcomeMatches (initialDrainedBlacklistHandlingOutcome, {})
        || ! blacklistHandlingOutcomeMatches (initialAfterDrainedBlacklistHandlingOutcome, {})
        || ! blacklistHandlingOutcomeHandlingResultMatches (
            initialBlacklistHandlingOutcomeHandlingResult, {})
        || ! deferredBlacklistHandlingOutcomeHandlingStatusMatches (
            initialDeferredBlacklistHandlingOutcomeHandlingStatus, {})
        || ! deferredBlacklistHandlingOutcomeHandlingStatusMatches (
            initialDeferredBlacklistHandlingOutcomeHandlingRecord, {})
        || ! deferredBlacklistHandlingOutcomeHandlingStatusMatches (
            initialDeferredBlacklistHandlingOutcomeHandlingInspection, {})
        || ! blacklistHandlingOutcomeMatches (initialAfterAcknowledgedBlacklistHandlingOutcome, {}))
    {
        std::printf ("FAIL: plugin host coordinator initial pending blacklist-handling request/outcome should be empty: queued=%s/%s pending=%s/%s drained=%s/%s after=%s/%s command=%s/%s receipt=%s recorded=%d receiptRecord=%s receiptRecordRecorded=%d receiptInspection=%s receiptInspectionRecorded=%d outcome=%s/%s/%d/%d/%d queuedOutcome=%s/%s pendingOutcome=%s/%s drainedOutcome=%s/%s afterDrainedOutcome=%s/%s handling=%s/%d handlingReceipt=%s handlingRecorded=%d handlingReceiptRecord=%s handlingReceiptRecordRecorded=%d handlingReceiptInspection=%s handlingReceiptInspectionRecorded=%d receiptAck=%s receiptAckRecorded=%d afterReceiptAck=%s afterReceiptAckRecorded=%d afterOutcome=%s/%s/%d/%d/%d receiptPolicy=%d receiptPersisted=%d outcomePolicy=%d outcomePersisted=%d handlingPolicy=%d handlingPersisted=%d handlingReceiptPolicy=%d handlingReceiptPersisted=%d afterOutcomePolicy=%d afterOutcomePersisted=%d\n",
                     statusName (initialQueuedBlacklistHandlingRequest.status),
                     statusName (initialQueuedBlacklistHandlingRequest.failureKind),
                     statusName (initialPendingBlacklistHandlingRequest.status),
                     statusName (initialPendingBlacklistHandlingRequest.failureKind),
                     statusName (initialDrainedBlacklistHandlingRequest.status),
                     statusName (initialDrainedBlacklistHandlingRequest.failureKind),
                     statusName (initialAfterDrainedBlacklistHandlingRequest.status),
                     statusName (initialAfterDrainedBlacklistHandlingRequest.failureKind),
                     statusName (initialBlacklistHandlingCommandResult.status),
                     statusName (initialBlacklistHandlingCommandResult.command.failureKind),
                     statusName (initialDeferredBlacklistHandlingCommandStatus.status),
                     initialDeferredBlacklistHandlingCommandStatus.commandRecorded ? 1 : 0,
                     statusName (initialDeferredBlacklistHandlingCommandRecord.status),
                     initialDeferredBlacklistHandlingCommandRecord.commandRecorded ? 1 : 0,
                     statusName (initialDeferredBlacklistHandlingCommandInspection.status),
                     initialDeferredBlacklistHandlingCommandInspection.commandRecorded ? 1 : 0,
                     statusName (initialBlacklistHandlingOutcome.status),
                     statusName (initialBlacklistHandlingOutcome.failureKind),
                     initialBlacklistHandlingOutcome.crashCandidate ? 1 : 0,
                     initialBlacklistHandlingOutcome.watchdogTimeoutCandidate ? 1 : 0,
                     initialBlacklistHandlingOutcome.controlThreadBlacklistHandlingInspected ? 1 : 0,
                     statusName (initialQueuedBlacklistHandlingOutcome.status),
                     statusName (initialQueuedBlacklistHandlingOutcome.failureKind),
                     statusName (initialPendingBlacklistHandlingOutcome.status),
                     statusName (initialPendingBlacklistHandlingOutcome.failureKind),
                     statusName (initialDrainedBlacklistHandlingOutcome.status),
                     statusName (initialDrainedBlacklistHandlingOutcome.failureKind),
                     statusName (initialAfterDrainedBlacklistHandlingOutcome.status),
                     statusName (initialAfterDrainedBlacklistHandlingOutcome.failureKind),
                      statusName (initialBlacklistHandlingOutcomeHandlingResult.status),
                      initialBlacklistHandlingOutcomeHandlingResult.pendingOutcomeConsumed ? 1 : 0,
                      statusName (initialDeferredBlacklistHandlingOutcomeHandlingStatus.status),
                      initialDeferredBlacklistHandlingOutcomeHandlingStatus.handlingRecorded ? 1 : 0,
                      statusName (initialDeferredBlacklistHandlingOutcomeHandlingRecord.status),
                      initialDeferredBlacklistHandlingOutcomeHandlingRecord.handlingRecorded ? 1 : 0,
                      statusName (initialDeferredBlacklistHandlingOutcomeHandlingInspection.status),
                      initialDeferredBlacklistHandlingOutcomeHandlingInspection.handlingRecorded ? 1 : 0,
                      statusName (initialAcknowledgedDeferredBlacklistHandlingCommandStatus.status),
                     initialAcknowledgedDeferredBlacklistHandlingCommandStatus.commandRecorded ? 1 : 0,
                     statusName (initialAfterAcknowledgedDeferredBlacklistHandlingCommandStatus.status),
                     initialAfterAcknowledgedDeferredBlacklistHandlingCommandStatus.commandRecorded ? 1 : 0,
                     statusName (initialAfterAcknowledgedBlacklistHandlingOutcome.status),
                     statusName (initialAfterAcknowledgedBlacklistHandlingOutcome.failureKind),
                     initialAfterAcknowledgedBlacklistHandlingOutcome.crashCandidate ? 1 : 0,
                     initialAfterAcknowledgedBlacklistHandlingOutcome.watchdogTimeoutCandidate ? 1 : 0,
                     initialAfterAcknowledgedBlacklistHandlingOutcome.controlThreadBlacklistHandlingInspected ? 1 : 0,
                     initialDeferredBlacklistHandlingCommandRecord.blacklistPolicyApplied ? 1 : 0,
                     initialDeferredBlacklistHandlingCommandRecord.blacklistStatePersisted ? 1 : 0,
                     initialBlacklistHandlingOutcome.blacklistPolicyApplied ? 1 : 0,
                      initialBlacklistHandlingOutcome.blacklistStatePersisted ? 1 : 0,
                      initialBlacklistHandlingOutcomeHandlingResult.blacklistPolicyApplied ? 1 : 0,
                      initialBlacklistHandlingOutcomeHandlingResult.blacklistStatePersisted ? 1 : 0,
                      initialDeferredBlacklistHandlingOutcomeHandlingRecord.blacklistPolicyApplied ? 1 : 0,
                      initialDeferredBlacklistHandlingOutcomeHandlingRecord.blacklistStatePersisted ? 1 : 0,
                      initialAfterAcknowledgedBlacklistHandlingOutcome.blacklistPolicyApplied ? 1 : 0,
                      initialAfterAcknowledgedBlacklistHandlingOutcome.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    const auto initialAcknowledgedDeferredBlacklistHandlingOutcomeHandlingStatus =
        coordinator.acknowledgeDeferredBlacklistHandlingOutcomeHandlingStatus();
    const auto initialAfterAcknowledgedDeferredBlacklistHandlingOutcomeHandlingStatus =
        coordinator.deferredBlacklistHandlingOutcomeHandlingStatus();
    if (! deferredBlacklistHandlingOutcomeHandlingStatusMatches (
            initialAcknowledgedDeferredBlacklistHandlingOutcomeHandlingStatus, {})
        || ! deferredBlacklistHandlingOutcomeHandlingStatusMatches (
            initialAfterAcknowledgedDeferredBlacklistHandlingOutcomeHandlingStatus, {}))
    {
        std::printf ("FAIL: plugin host coordinator initial deferred blacklist-handling outcome handling acknowledge/clear status is wrong: ack=%s recorded=%d after=%s afterRecorded=%d\n",
                     statusName (initialAcknowledgedDeferredBlacklistHandlingOutcomeHandlingStatus.status),
                     initialAcknowledgedDeferredBlacklistHandlingOutcomeHandlingStatus.handlingRecorded ? 1 : 0,
                     statusName (initialAfterAcknowledgedDeferredBlacklistHandlingOutcomeHandlingStatus.status),
                     initialAfterAcknowledgedDeferredBlacklistHandlingOutcomeHandlingStatus.handlingRecorded ? 1 : 0);
        return 2;
    }

    yesdaw::plugin_host::PluginHostCoordinator noneFailureCoordinator;
    const auto queuedNoneFailureAction = noneFailureCoordinator.queueFailureActionRequest (
        { yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::bypassAndRecompile,
          yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none,
          true,
          true });
    const auto noneFailureCommandResult = noneFailureCoordinator.drainPendingFailureActionRequestToControlCommand();
    if (queuedNoneFailureAction.action != yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::none
        || queuedNoneFailureAction.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none
        || queuedNoneFailureAction.bypassRequested
        || queuedNoneFailureAction.recompileRequested
        || noneFailureCommandResult.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::noAction
        || noneFailureCommandResult.drainedRequest.action != yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::none
        || noneFailureCommandResult.command.command != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandKind::none
        || noneFailureCommandResult.pendingRequestConsumed
        || noneFailureCommandResult.graphRecompileExecuted)
    {
        std::printf ("FAIL: plugin host coordinator HostFailureKind::none produced a control command: queued=%s/%s status=%s drained=%s/%s command=%s/%s consumed=%d executed=%d\n",
                     statusName (queuedNoneFailureAction.action),
                     statusName (queuedNoneFailureAction.failureKind),
                     statusName (noneFailureCommandResult.status),
                     statusName (noneFailureCommandResult.drainedRequest.action),
                     statusName (noneFailureCommandResult.drainedRequest.failureKind),
                     statusName (noneFailureCommandResult.command.command),
                     statusName (noneFailureCommandResult.command.failureKind),
                     noneFailureCommandResult.pendingRequestConsumed ? 1 : 0,
                     noneFailureCommandResult.graphRecompileExecuted ? 1 : 0);
        return 2;
    }

    const auto queuedNoneBlacklistCandidate = noneFailureCoordinator.queueBlacklistCandidate (
        { yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none, true, true, false });
    const auto drainedNoneBlacklistCandidate = noneFailureCoordinator.drainPendingBlacklistCandidateStatus();
    const auto queuedInconsistentBlacklistCandidate = noneFailureCoordinator.queueBlacklistCandidate (
        { yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash, true, true, true });
    const auto inconsistentBlacklistEscalationResult =
        noneFailureCoordinator.drainPendingBlacklistCandidateToControlEscalation();
    const auto inconsistentDeferredBlacklistEscalationStatus =
        noneFailureCoordinator.recordDeferredBlacklistEscalationResult (inconsistentBlacklistEscalationResult);
    if (! blacklistCandidateMatches (queuedNoneBlacklistCandidate, {})
        || ! blacklistCandidateMatches (drainedNoneBlacklistCandidate, {})
        || ! blacklistCandidateMatches (queuedInconsistentBlacklistCandidate, {})
        || inconsistentBlacklistEscalationResult.status != yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalationStatus::noAction
        || inconsistentBlacklistEscalationResult.pendingCandidateConsumed
        || inconsistentBlacklistEscalationResult.blacklistPolicyApplied
        || inconsistentBlacklistEscalationResult.blacklistStatePersisted
        || ! deferredBlacklistEscalationStatusMatches (inconsistentDeferredBlacklistEscalationStatus, {}))
    {
        std::printf ("FAIL: plugin host coordinator accepted an invalid pending blacklist candidate/escalation: none=%s/%d/%d/%d drained=%s/%d/%d/%d inconsistent=%s/%d/%d/%d escalation=%s consumed=%d policy=%d persisted=%d receipt=%s recorded=%d\n",
                     statusName (queuedNoneBlacklistCandidate.failureKind),
                     queuedNoneBlacklistCandidate.candidate ? 1 : 0,
                     queuedNoneBlacklistCandidate.crashCandidate ? 1 : 0,
                     queuedNoneBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (drainedNoneBlacklistCandidate.failureKind),
                     drainedNoneBlacklistCandidate.candidate ? 1 : 0,
                     drainedNoneBlacklistCandidate.crashCandidate ? 1 : 0,
                     drainedNoneBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (queuedInconsistentBlacklistCandidate.failureKind),
                     queuedInconsistentBlacklistCandidate.candidate ? 1 : 0,
                     queuedInconsistentBlacklistCandidate.crashCandidate ? 1 : 0,
                     queuedInconsistentBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (inconsistentBlacklistEscalationResult.status),
                     inconsistentBlacklistEscalationResult.pendingCandidateConsumed ? 1 : 0,
                     inconsistentBlacklistEscalationResult.blacklistPolicyApplied ? 1 : 0,
                     inconsistentBlacklistEscalationResult.blacklistStatePersisted ? 1 : 0,
                     statusName (inconsistentDeferredBlacklistEscalationStatus.status),
                     inconsistentDeferredBlacklistEscalationStatus.escalationRecorded ? 1 : 0);
        return 2;
    }

    yesdaw::plugin_host::PluginHostCoordinator deferredReceiptCoordinator;
    yesdaw::plugin_host::PluginHostCoordinator blacklistReceiptCoordinator;
    yesdaw::plugin_host::PluginHostCoordinator blacklistPolicyCommandReceiptCoordinator;

    const auto handshake = coordinator.launchAndHandshake (workerExecutable);

    if (handshake.status != yesdaw::plugin_host::PluginHostCoordinator::HandshakeStatus::success)
    {
        std::printf ("FAIL: plugin host coordinator handshake failed: status=%s ready=%d echo=%d\n",
                     statusName (handshake.status),
                     handshake.readySeen ? 1 : 0,
                     handshake.probeEchoed ? 1 : 0);
        return 2;
    }

    const auto runningStatus = coordinator.status();

    if (runningStatus.state != yesdaw::plugin_host::PluginHostCoordinator::ChildState::running
        || runningStatus.handshakeStatus != yesdaw::plugin_host::PluginHostCoordinator::HandshakeStatus::success
        || runningStatus.stopStatus != yesdaw::plugin_host::PluginHostCoordinator::StopStatus::notStarted
        || runningStatus.watchdogStatus != yesdaw::plugin_host::PluginHostCoordinator::WatchdogStatus::notStarted
        || runningStatus.crashStatus != yesdaw::plugin_host::PluginHostCoordinator::CrashStatus::notStarted
        || runningStatus.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none
        || ! runningStatus.launchAttempted
        || ! runningStatus.readySeen
        || ! runningStatus.probeEchoed
        || runningStatus.stopRequested
        || runningStatus.watchdogTimedOut
        || runningStatus.watchdogKillRequested
        || runningStatus.crashObservationRequested
        || runningStatus.connectionLostSeen)
    {
        std::printf ("FAIL: plugin host coordinator running status is wrong: state=%s handshake=%s stop=%s watchdog=%s crash=%s failure=%s launch=%d ready=%d echo=%d stopRequested=%d watchdogTimeout=%d watchdogKill=%d crashObservation=%d connectionLost=%d\n",
                     statusName (runningStatus.state),
                     statusName (runningStatus.handshakeStatus),
                     statusName (runningStatus.stopStatus),
                     statusName (runningStatus.watchdogStatus),
                     statusName (runningStatus.crashStatus),
                     statusName (runningStatus.failureKind),
                     runningStatus.launchAttempted ? 1 : 0,
                     runningStatus.readySeen ? 1 : 0,
                     runningStatus.probeEchoed ? 1 : 0,
                     runningStatus.stopRequested ? 1 : 0,
                     runningStatus.watchdogTimedOut ? 1 : 0,
                     runningStatus.watchdogKillRequested ? 1 : 0,
                     runningStatus.crashObservationRequested ? 1 : 0,
                     runningStatus.connectionLostSeen ? 1 : 0);
        return 2;
    }

    const auto stop = coordinator.requestStopAndWait();

    if (stop.status != yesdaw::plugin_host::PluginHostCoordinator::StopStatus::stopped
        || ! stop.connectionLostSeen)
    {
        std::printf ("FAIL: plugin host coordinator stop/lost-child check failed: status=%s connectionLost=%d\n",
                     statusName (stop.status),
                     stop.connectionLostSeen ? 1 : 0);
        return 2;
    }

    const auto stoppedStatus = coordinator.status();

    if (stoppedStatus.state != yesdaw::plugin_host::PluginHostCoordinator::ChildState::stopped
        || stoppedStatus.handshakeStatus != yesdaw::plugin_host::PluginHostCoordinator::HandshakeStatus::success
        || stoppedStatus.stopStatus != yesdaw::plugin_host::PluginHostCoordinator::StopStatus::stopped
        || stoppedStatus.watchdogStatus != yesdaw::plugin_host::PluginHostCoordinator::WatchdogStatus::notStarted
        || stoppedStatus.crashStatus != yesdaw::plugin_host::PluginHostCoordinator::CrashStatus::notStarted
        || stoppedStatus.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none
        || ! stoppedStatus.launchAttempted
        || ! stoppedStatus.readySeen
        || ! stoppedStatus.probeEchoed
        || ! stoppedStatus.stopRequested
        || stoppedStatus.watchdogTimedOut
        || stoppedStatus.watchdogKillRequested
        || stoppedStatus.crashObservationRequested
        || ! stoppedStatus.connectionLostSeen)
    {
        std::printf ("FAIL: plugin host coordinator stopped status is wrong: state=%s handshake=%s stop=%s watchdog=%s crash=%s failure=%s launch=%d ready=%d echo=%d stopRequested=%d watchdogTimeout=%d watchdogKill=%d crashObservation=%d connectionLost=%d\n",
                     statusName (stoppedStatus.state),
                     statusName (stoppedStatus.handshakeStatus),
                     statusName (stoppedStatus.stopStatus),
                     statusName (stoppedStatus.watchdogStatus),
                     statusName (stoppedStatus.crashStatus),
                     statusName (stoppedStatus.failureKind),
                     stoppedStatus.launchAttempted ? 1 : 0,
                     stoppedStatus.readySeen ? 1 : 0,
                     stoppedStatus.probeEchoed ? 1 : 0,
                     stoppedStatus.stopRequested ? 1 : 0,
                     stoppedStatus.watchdogTimedOut ? 1 : 0,
                     stoppedStatus.watchdogKillRequested ? 1 : 0,
                     stoppedStatus.crashObservationRequested ? 1 : 0,
                     stoppedStatus.connectionLostSeen ? 1 : 0);
        return 2;
    }

    const auto normalStopFailure = coordinator.hostFailureReport();
    if (normalStopFailure.kind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none
        || ! normalStopFailure.connectionLostSeen
        || normalStopFailure.watchdogTimedOut
        || normalStopFailure.watchdogKillRequested)
    {
        std::printf ("FAIL: plugin host coordinator normal stop failure report is wrong: failure=%s connectionLost=%d watchdogTimeout=%d watchdogKill=%d\n",
                     statusName (normalStopFailure.kind),
                     normalStopFailure.connectionLostSeen ? 1 : 0,
                     normalStopFailure.watchdogTimedOut ? 1 : 0,
                     normalStopFailure.watchdogKillRequested ? 1 : 0);
        return 2;
    }

    const auto normalStopBlacklistCandidate = coordinator.blacklistCandidateStatus();
    if (! blacklistCandidateMatches (normalStopBlacklistCandidate, {}))
    {
        std::printf ("FAIL: plugin host coordinator normal stop should not be a blacklist candidate: failure=%s candidate=%d crash=%d watchdog=%d\n",
                     statusName (normalStopBlacklistCandidate.failureKind),
                     normalStopBlacklistCandidate.candidate ? 1 : 0,
                     normalStopBlacklistCandidate.crashCandidate ? 1 : 0,
                     normalStopBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0);
        return 2;
    }

    const auto normalStopQueuedBlacklistCandidate = coordinator.queueBlacklistCandidateForCurrentFailure();
    const auto normalStopPendingBlacklistCandidate = coordinator.pendingBlacklistCandidateStatus();
    const auto normalStopDrainedBlacklistCandidate = coordinator.drainPendingBlacklistCandidateStatus();
    const auto normalStopBlacklistEscalationResult =
        coordinator.drainPendingBlacklistCandidateToControlEscalation();
    const auto normalStopDeferredBlacklistEscalationStatus =
        coordinator.recordDeferredBlacklistEscalationResult (normalStopBlacklistEscalationResult);
    const auto normalStopBlacklistPolicyDecisionRequest = coordinator.blacklistPolicyDecisionRequest();
    const auto normalStopBlacklistPolicyDecisionCommandResult =
        coordinator.drainPendingBlacklistPolicyDecisionRequestToControlCommand();
    const auto normalStopBlacklistPolicyDecisionOutcome =
        coordinator.blacklistPolicyDecisionOutcomeStatus();
    const auto normalStopQueuedBlacklistPolicyDecisionOutcome =
        coordinator.queueBlacklistPolicyDecisionOutcomeForDeferredCommand();
    const auto normalStopPendingBlacklistPolicyDecisionOutcome =
        coordinator.pendingBlacklistPolicyDecisionOutcomeStatus();
    const auto normalStopDrainedBlacklistPolicyDecisionOutcome =
        coordinator.drainPendingBlacklistPolicyDecisionOutcomeStatus();
    const auto normalStopBlacklistPolicyDecisionOutcomeHandlingResult =
        coordinator.drainPendingBlacklistPolicyDecisionOutcomeToControlHandling();
    if (! blacklistCandidateMatches (normalStopQueuedBlacklistCandidate, {})
        || ! blacklistCandidateMatches (normalStopPendingBlacklistCandidate, {})
        || ! blacklistCandidateMatches (normalStopDrainedBlacklistCandidate, {})
        || normalStopBlacklistEscalationResult.status != yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalationStatus::noAction
        || normalStopBlacklistEscalationResult.pendingCandidateConsumed
        || normalStopBlacklistEscalationResult.blacklistPolicyApplied
        || normalStopBlacklistEscalationResult.blacklistStatePersisted
        || ! deferredBlacklistEscalationStatusMatches (normalStopDeferredBlacklistEscalationStatus, {})
        || ! blacklistPolicyDecisionRequestMatches (normalStopBlacklistPolicyDecisionRequest, {})
        || ! blacklistPolicyDecisionCommandResultMatches (normalStopBlacklistPolicyDecisionCommandResult, {})
        || ! blacklistPolicyDecisionOutcomeMatches (normalStopBlacklistPolicyDecisionOutcome, {})
        || ! blacklistPolicyDecisionOutcomeMatches (normalStopQueuedBlacklistPolicyDecisionOutcome, {})
        || ! blacklistPolicyDecisionOutcomeMatches (normalStopPendingBlacklistPolicyDecisionOutcome, {})
        || ! blacklistPolicyDecisionOutcomeMatches (normalStopDrainedBlacklistPolicyDecisionOutcome, {})
        || ! blacklistPolicyDecisionOutcomeHandlingResultMatches (
            normalStopBlacklistPolicyDecisionOutcomeHandlingResult, {}))
    {
        std::printf ("FAIL: plugin host coordinator normal stop pending blacklist candidate/escalation/policy request command/outcome should remain empty: queued=%s/%d/%d/%d pending=%s/%d/%d/%d drained=%s/%d/%d/%d escalation=%s consumed=%d policy=%d persisted=%d receipt=%s recorded=%d request=%s/%s/%d/%d/%d command=%s/%s/%s commandConsumed=%d outcome=%s/%s/%d/%d/%d queuedOutcome=%s/%s pendingOutcome=%s/%s drainedOutcome=%s/%s handling=%s handlingConsumed=%d requestPolicy=%d requestPersisted=%d commandPolicy=%d commandPersisted=%d outcomePolicy=%d outcomePersisted=%d handlingPolicy=%d handlingPersisted=%d\n",
                     statusName (normalStopQueuedBlacklistCandidate.failureKind),
                     normalStopQueuedBlacklistCandidate.candidate ? 1 : 0,
                     normalStopQueuedBlacklistCandidate.crashCandidate ? 1 : 0,
                     normalStopQueuedBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (normalStopPendingBlacklistCandidate.failureKind),
                     normalStopPendingBlacklistCandidate.candidate ? 1 : 0,
                     normalStopPendingBlacklistCandidate.crashCandidate ? 1 : 0,
                     normalStopPendingBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (normalStopDrainedBlacklistCandidate.failureKind),
                     normalStopDrainedBlacklistCandidate.candidate ? 1 : 0,
                     normalStopDrainedBlacklistCandidate.crashCandidate ? 1 : 0,
                     normalStopDrainedBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (normalStopBlacklistEscalationResult.status),
                     normalStopBlacklistEscalationResult.pendingCandidateConsumed ? 1 : 0,
                     normalStopBlacklistEscalationResult.blacklistPolicyApplied ? 1 : 0,
                     normalStopBlacklistEscalationResult.blacklistStatePersisted ? 1 : 0,
                     statusName (normalStopDeferredBlacklistEscalationStatus.status),
                     normalStopDeferredBlacklistEscalationStatus.escalationRecorded ? 1 : 0,
                     statusName (normalStopBlacklistPolicyDecisionRequest.status),
                     statusName (normalStopBlacklistPolicyDecisionRequest.failureKind),
                     normalStopBlacklistPolicyDecisionRequest.crashCandidate ? 1 : 0,
                     normalStopBlacklistPolicyDecisionRequest.watchdogTimeoutCandidate ? 1 : 0,
                     normalStopBlacklistPolicyDecisionRequest.controlThreadPolicyDecisionRequested ? 1 : 0,
                     statusName (normalStopBlacklistPolicyDecisionCommandResult.status),
                     statusName (normalStopBlacklistPolicyDecisionCommandResult.command.command),
                     statusName (normalStopBlacklistPolicyDecisionCommandResult.command.failureKind),
                     normalStopBlacklistPolicyDecisionCommandResult.pendingRequestConsumed ? 1 : 0,
                     statusName (normalStopBlacklistPolicyDecisionOutcome.status),
                     statusName (normalStopBlacklistPolicyDecisionOutcome.failureKind),
                     normalStopBlacklistPolicyDecisionOutcome.crashCandidate ? 1 : 0,
                     normalStopBlacklistPolicyDecisionOutcome.watchdogTimeoutCandidate ? 1 : 0,
                     normalStopBlacklistPolicyDecisionOutcome.controlThreadPolicyDecisionInspected ? 1 : 0,
                     statusName (normalStopQueuedBlacklistPolicyDecisionOutcome.status),
                     statusName (normalStopQueuedBlacklistPolicyDecisionOutcome.failureKind),
                     statusName (normalStopPendingBlacklistPolicyDecisionOutcome.status),
                     statusName (normalStopPendingBlacklistPolicyDecisionOutcome.failureKind),
                     statusName (normalStopDrainedBlacklistPolicyDecisionOutcome.status),
                     statusName (normalStopDrainedBlacklistPolicyDecisionOutcome.failureKind),
                     statusName (normalStopBlacklistPolicyDecisionOutcomeHandlingResult.status),
                     normalStopBlacklistPolicyDecisionOutcomeHandlingResult.pendingOutcomeConsumed ? 1 : 0,
                     normalStopBlacklistPolicyDecisionRequest.blacklistPolicyApplied ? 1 : 0,
                     normalStopBlacklistPolicyDecisionRequest.blacklistStatePersisted ? 1 : 0,
                     normalStopBlacklistPolicyDecisionCommandResult.blacklistPolicyApplied ? 1 : 0,
                     normalStopBlacklistPolicyDecisionCommandResult.blacklistStatePersisted ? 1 : 0,
                     normalStopBlacklistPolicyDecisionOutcome.blacklistPolicyApplied ? 1 : 0,
                     normalStopBlacklistPolicyDecisionOutcome.blacklistStatePersisted ? 1 : 0,
                     normalStopBlacklistPolicyDecisionOutcomeHandlingResult.blacklistPolicyApplied ? 1 : 0,
                     normalStopBlacklistPolicyDecisionOutcomeHandlingResult.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    const auto normalStopAction = coordinator.failureActionRequest();
    if (normalStopAction.action != yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::none
        || normalStopAction.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none
        || normalStopAction.bypassRequested
        || normalStopAction.recompileRequested)
    {
        std::printf ("FAIL: plugin host coordinator normal stop action request is wrong: action=%s failure=%s bypass=%d recompile=%d\n",
                     statusName (normalStopAction.action),
                     statusName (normalStopAction.failureKind),
                     normalStopAction.bypassRequested ? 1 : 0,
                     normalStopAction.recompileRequested ? 1 : 0);
        return 2;
    }

    const auto normalStopQueuedAction = coordinator.queueFailureActionRequestForCurrentFailure();
    const auto normalStopPendingAction = coordinator.pendingFailureActionRequest();
    const auto normalStopDrainedAction = coordinator.drainPendingFailureActionRequest();
    if (! requestMatches (normalStopQueuedAction, normalStopAction)
        || ! requestMatches (normalStopPendingAction, normalStopAction)
        || ! requestMatches (normalStopDrainedAction, normalStopAction))
    {
        std::printf ("FAIL: plugin host coordinator normal stop pending action should remain empty: queued=%s/%s pending=%s/%s drained=%s/%s\n",
                     statusName (normalStopQueuedAction.action),
                     statusName (normalStopQueuedAction.failureKind),
                     statusName (normalStopPendingAction.action),
                     statusName (normalStopPendingAction.failureKind),
                     statusName (normalStopDrainedAction.action),
                     statusName (normalStopDrainedAction.failureKind));
        return 2;
    }

    const auto normalStopCommandResult = coordinator.drainPendingFailureActionRequestToControlCommand();
    if (normalStopCommandResult.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::noAction
        || normalStopCommandResult.drainedRequest.action != yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::none
        || normalStopCommandResult.command.command != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandKind::none
        || normalStopCommandResult.pendingRequestConsumed
        || normalStopCommandResult.graphRecompileExecuted)
    {
        std::printf ("FAIL: plugin host coordinator normal stop control command should remain empty: status=%s drained=%s/%s command=%s/%s consumed=%d executed=%d\n",
                     statusName (normalStopCommandResult.status),
                     statusName (normalStopCommandResult.drainedRequest.action),
                     statusName (normalStopCommandResult.drainedRequest.failureKind),
                     statusName (normalStopCommandResult.command.command),
                     statusName (normalStopCommandResult.command.failureKind),
                     normalStopCommandResult.pendingRequestConsumed ? 1 : 0,
                     normalStopCommandResult.graphRecompileExecuted ? 1 : 0);
        return 2;
    }

    const auto normalStopDeferredStatus =
        deferredReceiptCoordinator.recordDeferredGraphChangeCommandResult (normalStopCommandResult);
    if (normalStopDeferredStatus.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::noAction
        || normalStopDeferredStatus.lastResult.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::noAction
        || normalStopDeferredStatus.commandRecorded
        || normalStopDeferredStatus.graphRecompileExecuted)
    {
        std::printf ("FAIL: plugin host coordinator normal stop should not record a deferred graph-change command: status=%s last=%s recorded=%d executed=%d\n",
                     statusName (normalStopDeferredStatus.status),
                     statusName (normalStopDeferredStatus.lastResult.status),
                     normalStopDeferredStatus.commandRecorded ? 1 : 0,
                     normalStopDeferredStatus.graphRecompileExecuted ? 1 : 0);
        return 2;
    }

    yesdaw::plugin_host::PluginHostCoordinator watchdogCoordinator;
    const auto watchdog = watchdogCoordinator.launchAndExpectWatchdogTimeout (workerExecutable);

    if (watchdog.status != yesdaw::plugin_host::PluginHostCoordinator::WatchdogStatus::timeoutKilled
        || ! watchdog.readySeen
        || ! watchdog.watchdogTimedOut
        || ! watchdog.killRequested
        || ! watchdog.connectionLostSeen)
    {
        std::printf ("FAIL: plugin host coordinator watchdog timeout failed: status=%s ready=%d timedOut=%d kill=%d connectionLost=%d\n",
                     statusName (watchdog.status),
                     watchdog.readySeen ? 1 : 0,
                     watchdog.watchdogTimedOut ? 1 : 0,
                     watchdog.killRequested ? 1 : 0,
                     watchdog.connectionLostSeen ? 1 : 0);
        return 2;
    }

    const auto watchdogStatus = watchdogCoordinator.status();

    if (watchdogStatus.state != yesdaw::plugin_host::PluginHostCoordinator::ChildState::stopped
        || watchdogStatus.handshakeStatus != yesdaw::plugin_host::PluginHostCoordinator::HandshakeStatus::echoTimeout
        || watchdogStatus.stopStatus != yesdaw::plugin_host::PluginHostCoordinator::StopStatus::notStarted
        || watchdogStatus.watchdogStatus != yesdaw::plugin_host::PluginHostCoordinator::WatchdogStatus::timeoutKilled
        || watchdogStatus.crashStatus != yesdaw::plugin_host::PluginHostCoordinator::CrashStatus::notStarted
        || watchdogStatus.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout
        || ! watchdogStatus.launchAttempted
        || ! watchdogStatus.readySeen
        || watchdogStatus.probeEchoed
        || watchdogStatus.stopRequested
        || ! watchdogStatus.watchdogTimedOut
        || ! watchdogStatus.watchdogKillRequested
        || watchdogStatus.crashObservationRequested
        || ! watchdogStatus.connectionLostSeen)
    {
        std::printf ("FAIL: plugin host coordinator watchdog status is wrong: state=%s handshake=%s stop=%s watchdog=%s crash=%s failure=%s launch=%d ready=%d echo=%d stopRequested=%d watchdogTimeout=%d watchdogKill=%d crashObservation=%d connectionLost=%d\n",
                     statusName (watchdogStatus.state),
                     statusName (watchdogStatus.handshakeStatus),
                     statusName (watchdogStatus.stopStatus),
                     statusName (watchdogStatus.watchdogStatus),
                     statusName (watchdogStatus.crashStatus),
                     statusName (watchdogStatus.failureKind),
                     watchdogStatus.launchAttempted ? 1 : 0,
                     watchdogStatus.readySeen ? 1 : 0,
                     watchdogStatus.probeEchoed ? 1 : 0,
                     watchdogStatus.stopRequested ? 1 : 0,
                     watchdogStatus.watchdogTimedOut ? 1 : 0,
                     watchdogStatus.watchdogKillRequested ? 1 : 0,
                     watchdogStatus.crashObservationRequested ? 1 : 0,
                     watchdogStatus.connectionLostSeen ? 1 : 0);
        return 2;
    }

    const auto watchdogFailure = watchdogCoordinator.hostFailureReport();
    if (watchdogFailure.kind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout
        || ! watchdogFailure.connectionLostSeen
        || ! watchdogFailure.watchdogTimedOut
        || ! watchdogFailure.watchdogKillRequested)
    {
        std::printf ("FAIL: plugin host coordinator watchdog failure report is wrong: failure=%s connectionLost=%d watchdogTimeout=%d watchdogKill=%d\n",
                     statusName (watchdogFailure.kind),
                     watchdogFailure.connectionLostSeen ? 1 : 0,
                     watchdogFailure.watchdogTimedOut ? 1 : 0,
                     watchdogFailure.watchdogKillRequested ? 1 : 0);
        return 2;
    }

    const auto watchdogBlacklistCandidate = watchdogCoordinator.blacklistCandidateStatus();
    if (! blacklistCandidateMatches (
            watchdogBlacklistCandidate,
            { yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout, true, false, true }))
    {
        std::printf ("FAIL: plugin host coordinator watchdog blacklist-candidate status is wrong: failure=%s candidate=%d crash=%d watchdog=%d\n",
                     statusName (watchdogBlacklistCandidate.failureKind),
                     watchdogBlacklistCandidate.candidate ? 1 : 0,
                     watchdogBlacklistCandidate.crashCandidate ? 1 : 0,
                     watchdogBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0);
        return 2;
    }

    const auto queuedWatchdogBlacklistCandidate = watchdogCoordinator.queueBlacklistCandidateForCurrentFailure();
    const auto pendingWatchdogBlacklistCandidate = watchdogCoordinator.pendingBlacklistCandidateStatus();
    const auto drainedWatchdogBlacklistCandidate = watchdogCoordinator.drainPendingBlacklistCandidateStatus();
    const auto afterWatchdogBlacklistCandidateDrain = watchdogCoordinator.pendingBlacklistCandidateStatus();
    const auto queuedWatchdogBlacklistEscalationCandidate =
        watchdogCoordinator.queueBlacklistCandidateForCurrentFailure();
    const auto watchdogBlacklistEscalationResult =
        watchdogCoordinator.drainPendingBlacklistCandidateToControlEscalation();
    const auto afterWatchdogBlacklistEscalationDrain = watchdogCoordinator.pendingBlacklistCandidateStatus();
    if (! blacklistCandidateMatches (queuedWatchdogBlacklistCandidate, watchdogBlacklistCandidate)
        || ! blacklistCandidateMatches (pendingWatchdogBlacklistCandidate, watchdogBlacklistCandidate)
        || ! blacklistCandidateMatches (drainedWatchdogBlacklistCandidate, watchdogBlacklistCandidate)
        || ! blacklistCandidateMatches (afterWatchdogBlacklistCandidateDrain, {})
        || ! blacklistCandidateMatches (queuedWatchdogBlacklistEscalationCandidate, watchdogBlacklistCandidate)
        || ! blacklistEscalationResultMatches (
            watchdogBlacklistEscalationResult,
            { yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalationStatus::escalationReady,
              watchdogBlacklistCandidate,
              { yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout, false, true, true },
              true,
              false,
              false })
        || ! blacklistCandidateMatches (afterWatchdogBlacklistEscalationDrain, {}))
    {
        std::printf ("FAIL: plugin host coordinator watchdog pending blacklist candidate escalation is wrong: queued=%s/%d/%d/%d pending=%s/%d/%d/%d drained=%s/%d/%d/%d after=%s/%d/%d/%d escalationQueued=%s/%d/%d/%d escalation=%s drained=%s/%d/%d/%d escalated=%s/%d/%d/%d consumed=%d policy=%d persisted=%d afterEscalation=%s/%d/%d/%d\n",
                     statusName (queuedWatchdogBlacklistCandidate.failureKind),
                     queuedWatchdogBlacklistCandidate.candidate ? 1 : 0,
                     queuedWatchdogBlacklistCandidate.crashCandidate ? 1 : 0,
                     queuedWatchdogBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (pendingWatchdogBlacklistCandidate.failureKind),
                     pendingWatchdogBlacklistCandidate.candidate ? 1 : 0,
                     pendingWatchdogBlacklistCandidate.crashCandidate ? 1 : 0,
                     pendingWatchdogBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (drainedWatchdogBlacklistCandidate.failureKind),
                     drainedWatchdogBlacklistCandidate.candidate ? 1 : 0,
                     drainedWatchdogBlacklistCandidate.crashCandidate ? 1 : 0,
                     drainedWatchdogBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (afterWatchdogBlacklistCandidateDrain.failureKind),
                     afterWatchdogBlacklistCandidateDrain.candidate ? 1 : 0,
                     afterWatchdogBlacklistCandidateDrain.crashCandidate ? 1 : 0,
                     afterWatchdogBlacklistCandidateDrain.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (queuedWatchdogBlacklistEscalationCandidate.failureKind),
                     queuedWatchdogBlacklistEscalationCandidate.candidate ? 1 : 0,
                     queuedWatchdogBlacklistEscalationCandidate.crashCandidate ? 1 : 0,
                     queuedWatchdogBlacklistEscalationCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (watchdogBlacklistEscalationResult.status),
                     statusName (watchdogBlacklistEscalationResult.drainedCandidate.failureKind),
                     watchdogBlacklistEscalationResult.drainedCandidate.candidate ? 1 : 0,
                     watchdogBlacklistEscalationResult.drainedCandidate.crashCandidate ? 1 : 0,
                     watchdogBlacklistEscalationResult.drainedCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (watchdogBlacklistEscalationResult.escalation.failureKind),
                     watchdogBlacklistEscalationResult.escalation.crashCandidate ? 1 : 0,
                     watchdogBlacklistEscalationResult.escalation.watchdogTimeoutCandidate ? 1 : 0,
                     watchdogBlacklistEscalationResult.escalation.controlThreadEscalationRequested ? 1 : 0,
                     watchdogBlacklistEscalationResult.pendingCandidateConsumed ? 1 : 0,
                     watchdogBlacklistEscalationResult.blacklistPolicyApplied ? 1 : 0,
                     watchdogBlacklistEscalationResult.blacklistStatePersisted ? 1 : 0,
                     statusName (afterWatchdogBlacklistEscalationDrain.failureKind),
                     afterWatchdogBlacklistEscalationDrain.candidate ? 1 : 0,
                     afterWatchdogBlacklistEscalationDrain.crashCandidate ? 1 : 0,
                     afterWatchdogBlacklistEscalationDrain.watchdogTimeoutCandidate ? 1 : 0);
        return 2;
    }

    const auto watchdogDeferredBlacklistEscalationStatus =
        blacklistReceiptCoordinator.recordDeferredBlacklistEscalationResult (watchdogBlacklistEscalationResult);
    const auto watchdogDeferredBlacklistEscalationInspection =
        blacklistReceiptCoordinator.deferredBlacklistEscalationStatus();
    if (watchdogDeferredBlacklistEscalationStatus.status
            != yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalationStatus::escalationReady
        || ! blacklistEscalationResultMatches (watchdogDeferredBlacklistEscalationStatus.lastResult,
                                               watchdogBlacklistEscalationResult)
        || ! watchdogDeferredBlacklistEscalationStatus.escalationRecorded
        || watchdogDeferredBlacklistEscalationStatus.blacklistPolicyApplied
        || watchdogDeferredBlacklistEscalationStatus.blacklistStatePersisted
        || ! blacklistEscalationResultMatches (watchdogDeferredBlacklistEscalationInspection.lastResult,
                                               watchdogBlacklistEscalationResult)
        || ! watchdogDeferredBlacklistEscalationInspection.escalationRecorded
        || watchdogDeferredBlacklistEscalationInspection.blacklistPolicyApplied
        || watchdogDeferredBlacklistEscalationInspection.blacklistStatePersisted)
    {
        std::printf ("FAIL: plugin host coordinator watchdog deferred blacklist escalation receipt/status is wrong: status=%s inspected=%s recorded=%d inspectedRecorded=%d policy=%d persisted=%d inspectedPolicy=%d inspectedPersisted=%d\n",
                     statusName (watchdogDeferredBlacklistEscalationStatus.status),
                     statusName (watchdogDeferredBlacklistEscalationInspection.status),
                     watchdogDeferredBlacklistEscalationStatus.escalationRecorded ? 1 : 0,
                     watchdogDeferredBlacklistEscalationInspection.escalationRecorded ? 1 : 0,
                     watchdogDeferredBlacklistEscalationStatus.blacklistPolicyApplied ? 1 : 0,
                     watchdogDeferredBlacklistEscalationStatus.blacklistStatePersisted ? 1 : 0,
                     watchdogDeferredBlacklistEscalationInspection.blacklistPolicyApplied ? 1 : 0,
                     watchdogDeferredBlacklistEscalationInspection.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    const auto watchdogBlacklistPolicyDecisionRequest =
        blacklistReceiptCoordinator.blacklistPolicyDecisionRequest();
    const auto queuedWatchdogBlacklistPolicyDecisionRequest =
        blacklistReceiptCoordinator.queueBlacklistPolicyDecisionRequestForDeferredEscalation();
    const auto pendingWatchdogBlacklistPolicyDecisionRequest =
        blacklistReceiptCoordinator.pendingBlacklistPolicyDecisionRequest();
    const auto drainedWatchdogBlacklistPolicyDecisionRequest =
        blacklistReceiptCoordinator.drainPendingBlacklistPolicyDecisionRequest();
    const auto afterWatchdogDrainBlacklistPolicyDecisionRequest =
        blacklistReceiptCoordinator.drainPendingBlacklistPolicyDecisionRequest();
    const auto commandQueuedWatchdogBlacklistPolicyDecisionRequest =
        blacklistReceiptCoordinator.queueBlacklistPolicyDecisionRequestForDeferredEscalation();
    const auto watchdogBlacklistPolicyDecisionCommandResult =
        blacklistReceiptCoordinator.drainPendingBlacklistPolicyDecisionRequestToControlCommand();
    const auto afterWatchdogCommandDrainBlacklistPolicyDecisionRequest =
        blacklistReceiptCoordinator.pendingBlacklistPolicyDecisionRequest();
    const auto afterWatchdogCommandSecondDrainBlacklistPolicyDecisionCommandResult =
        blacklistReceiptCoordinator.drainPendingBlacklistPolicyDecisionRequestToControlCommand();
    if (! blacklistPolicyDecisionRequestMatches (
            watchdogBlacklistPolicyDecisionRequest,
            { yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionRequestStatus::requestReady,
              yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout,
              false,
              true,
              true,
              false,
              false }))
    {
        std::printf ("FAIL: plugin host coordinator watchdog blacklist policy-decision request is wrong: request=%s/%s/%d/%d/%d policy=%d persisted=%d\n",
                     statusName (watchdogBlacklistPolicyDecisionRequest.status),
                     statusName (watchdogBlacklistPolicyDecisionRequest.failureKind),
                     watchdogBlacklistPolicyDecisionRequest.crashCandidate ? 1 : 0,
                     watchdogBlacklistPolicyDecisionRequest.watchdogTimeoutCandidate ? 1 : 0,
                     watchdogBlacklistPolicyDecisionRequest.controlThreadPolicyDecisionRequested ? 1 : 0,
                     watchdogBlacklistPolicyDecisionRequest.blacklistPolicyApplied ? 1 : 0,
                     watchdogBlacklistPolicyDecisionRequest.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    if (! blacklistPolicyDecisionRequestMatches (queuedWatchdogBlacklistPolicyDecisionRequest,
                                                 watchdogBlacklistPolicyDecisionRequest)
        || ! blacklistPolicyDecisionRequestMatches (pendingWatchdogBlacklistPolicyDecisionRequest,
                                                    watchdogBlacklistPolicyDecisionRequest)
        || ! blacklistPolicyDecisionRequestMatches (drainedWatchdogBlacklistPolicyDecisionRequest,
                                                    watchdogBlacklistPolicyDecisionRequest)
        || ! blacklistPolicyDecisionRequestMatches (afterWatchdogDrainBlacklistPolicyDecisionRequest, {}))
    {
        std::printf ("FAIL: plugin host coordinator watchdog pending blacklist policy-decision request queue/drain is wrong: queued=%s/%s/%d/%d pending=%s/%s drained=%s/%s afterDrain=%s/%s policy=%d persisted=%d\n",
                     statusName (queuedWatchdogBlacklistPolicyDecisionRequest.status),
                     statusName (queuedWatchdogBlacklistPolicyDecisionRequest.failureKind),
                     queuedWatchdogBlacklistPolicyDecisionRequest.crashCandidate ? 1 : 0,
                     queuedWatchdogBlacklistPolicyDecisionRequest.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (pendingWatchdogBlacklistPolicyDecisionRequest.status),
                     statusName (pendingWatchdogBlacklistPolicyDecisionRequest.failureKind),
                     statusName (drainedWatchdogBlacklistPolicyDecisionRequest.status),
                     statusName (drainedWatchdogBlacklistPolicyDecisionRequest.failureKind),
                     statusName (afterWatchdogDrainBlacklistPolicyDecisionRequest.status),
                     statusName (afterWatchdogDrainBlacklistPolicyDecisionRequest.failureKind),
                     drainedWatchdogBlacklistPolicyDecisionRequest.blacklistPolicyApplied ? 1 : 0,
                     drainedWatchdogBlacklistPolicyDecisionRequest.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    if (! blacklistPolicyDecisionRequestMatches (commandQueuedWatchdogBlacklistPolicyDecisionRequest,
                                                 watchdogBlacklistPolicyDecisionRequest)
        || watchdogBlacklistPolicyDecisionCommandResult.status
            != yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionCommandStatus::commandReady
        || ! blacklistPolicyDecisionRequestMatches (watchdogBlacklistPolicyDecisionCommandResult.drainedRequest,
                                                    watchdogBlacklistPolicyDecisionRequest)
        || ! blacklistPolicyDecisionCommandMatches (
            watchdogBlacklistPolicyDecisionCommandResult.command,
            { yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionCommandKind::requestPolicyDecision,
              yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout,
              false,
              true,
              true })
        || ! watchdogBlacklistPolicyDecisionCommandResult.pendingRequestConsumed
        || watchdogBlacklistPolicyDecisionCommandResult.blacklistPolicyApplied
        || watchdogBlacklistPolicyDecisionCommandResult.blacklistStatePersisted
        || ! blacklistPolicyDecisionRequestMatches (afterWatchdogCommandDrainBlacklistPolicyDecisionRequest, {})
        || ! blacklistPolicyDecisionCommandResultMatches (
            afterWatchdogCommandSecondDrainBlacklistPolicyDecisionCommandResult, {}))
    {
        std::printf ("FAIL: plugin host coordinator watchdog blacklist policy-decision control command is wrong: queued=%s/%s/%d/%d status=%s drained=%s/%s/%d/%d command=%s/%s/%d/%d/%d consumed=%d afterPending=%s/%s afterSecond=%s/%s policy=%d persisted=%d\n",
                     statusName (commandQueuedWatchdogBlacklistPolicyDecisionRequest.status),
                     statusName (commandQueuedWatchdogBlacklistPolicyDecisionRequest.failureKind),
                     commandQueuedWatchdogBlacklistPolicyDecisionRequest.crashCandidate ? 1 : 0,
                     commandQueuedWatchdogBlacklistPolicyDecisionRequest.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (watchdogBlacklistPolicyDecisionCommandResult.status),
                     statusName (watchdogBlacklistPolicyDecisionCommandResult.drainedRequest.status),
                     statusName (watchdogBlacklistPolicyDecisionCommandResult.drainedRequest.failureKind),
                     watchdogBlacklistPolicyDecisionCommandResult.drainedRequest.crashCandidate ? 1 : 0,
                     watchdogBlacklistPolicyDecisionCommandResult.drainedRequest.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (watchdogBlacklistPolicyDecisionCommandResult.command.command),
                     statusName (watchdogBlacklistPolicyDecisionCommandResult.command.failureKind),
                     watchdogBlacklistPolicyDecisionCommandResult.command.crashCandidate ? 1 : 0,
                     watchdogBlacklistPolicyDecisionCommandResult.command.watchdogTimeoutCandidate ? 1 : 0,
                     watchdogBlacklistPolicyDecisionCommandResult.command.controlThreadPolicyDecisionRequested ? 1 : 0,
                     watchdogBlacklistPolicyDecisionCommandResult.pendingRequestConsumed ? 1 : 0,
                     statusName (afterWatchdogCommandDrainBlacklistPolicyDecisionRequest.status),
                     statusName (afterWatchdogCommandDrainBlacklistPolicyDecisionRequest.failureKind),
                     statusName (afterWatchdogCommandSecondDrainBlacklistPolicyDecisionCommandResult.status),
                     statusName (afterWatchdogCommandSecondDrainBlacklistPolicyDecisionCommandResult.command.failureKind),
                     watchdogBlacklistPolicyDecisionCommandResult.blacklistPolicyApplied ? 1 : 0,
                     watchdogBlacklistPolicyDecisionCommandResult.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    const auto watchdogDeferredBlacklistPolicyDecisionCommandStatus =
        blacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistPolicyDecisionCommandResult (
            watchdogBlacklistPolicyDecisionCommandResult);
    const auto watchdogDeferredBlacklistPolicyDecisionCommandInspection =
        blacklistPolicyCommandReceiptCoordinator.deferredBlacklistPolicyDecisionCommandStatus();
    const auto watchdogBlacklistPolicyDecisionOutcome =
        blacklistPolicyCommandReceiptCoordinator.blacklistPolicyDecisionOutcomeStatus();
    const auto queuedWatchdogBlacklistPolicyDecisionOutcome =
        blacklistPolicyCommandReceiptCoordinator.queueBlacklistPolicyDecisionOutcomeForDeferredCommand();
    const auto pendingWatchdogBlacklistPolicyDecisionOutcome =
        blacklistPolicyCommandReceiptCoordinator.pendingBlacklistPolicyDecisionOutcomeStatus();
    const auto drainedWatchdogBlacklistPolicyDecisionOutcome =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistPolicyDecisionOutcomeStatus();
    const auto afterWatchdogDrainBlacklistPolicyDecisionOutcome =
        blacklistPolicyCommandReceiptCoordinator.pendingBlacklistPolicyDecisionOutcomeStatus();
    if (watchdogDeferredBlacklistPolicyDecisionCommandStatus.status
            != yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionCommandStatus::commandReady
        || ! blacklistPolicyDecisionCommandResultMatches (
            watchdogDeferredBlacklistPolicyDecisionCommandStatus.lastResult,
            watchdogBlacklistPolicyDecisionCommandResult)
        || ! watchdogDeferredBlacklistPolicyDecisionCommandStatus.commandRecorded
        || watchdogDeferredBlacklistPolicyDecisionCommandStatus.blacklistPolicyApplied
        || watchdogDeferredBlacklistPolicyDecisionCommandStatus.blacklistStatePersisted
        || ! blacklistPolicyDecisionCommandResultMatches (
            watchdogDeferredBlacklistPolicyDecisionCommandInspection.lastResult,
            watchdogBlacklistPolicyDecisionCommandResult)
        || ! watchdogDeferredBlacklistPolicyDecisionCommandInspection.commandRecorded
        || watchdogDeferredBlacklistPolicyDecisionCommandInspection.blacklistPolicyApplied
        || watchdogDeferredBlacklistPolicyDecisionCommandInspection.blacklistStatePersisted
        || ! blacklistPolicyDecisionOutcomeMatches (
            watchdogBlacklistPolicyDecisionOutcome,
            { yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionOutcomeStatus::outcomeReady,
              yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout,
              false,
              true,
              true,
              false,
              false })
        || ! blacklistPolicyDecisionOutcomeMatches (
            queuedWatchdogBlacklistPolicyDecisionOutcome, watchdogBlacklistPolicyDecisionOutcome)
        || ! blacklistPolicyDecisionOutcomeMatches (
            pendingWatchdogBlacklistPolicyDecisionOutcome, watchdogBlacklistPolicyDecisionOutcome)
        || ! blacklistPolicyDecisionOutcomeMatches (
            drainedWatchdogBlacklistPolicyDecisionOutcome, watchdogBlacklistPolicyDecisionOutcome)
        || ! blacklistPolicyDecisionOutcomeMatches (afterWatchdogDrainBlacklistPolicyDecisionOutcome, {}))
    {
        std::printf ("FAIL: plugin host coordinator watchdog deferred blacklist policy-decision command receipt/status/outcome queue is wrong: status=%s inspected=%s recorded=%d inspectedRecorded=%d policy=%d persisted=%d inspectedPolicy=%d inspectedPersisted=%d outcome=%s/%s/%d/%d/%d queued=%s/%s pending=%s/%s drained=%s/%s afterDrain=%s/%s outcomePolicy=%d outcomePersisted=%d\n",
                     statusName (watchdogDeferredBlacklistPolicyDecisionCommandStatus.status),
                     statusName (watchdogDeferredBlacklistPolicyDecisionCommandInspection.status),
                     watchdogDeferredBlacklistPolicyDecisionCommandStatus.commandRecorded ? 1 : 0,
                     watchdogDeferredBlacklistPolicyDecisionCommandInspection.commandRecorded ? 1 : 0,
                     watchdogDeferredBlacklistPolicyDecisionCommandStatus.blacklistPolicyApplied ? 1 : 0,
                     watchdogDeferredBlacklistPolicyDecisionCommandStatus.blacklistStatePersisted ? 1 : 0,
                     watchdogDeferredBlacklistPolicyDecisionCommandInspection.blacklistPolicyApplied ? 1 : 0,
                     watchdogDeferredBlacklistPolicyDecisionCommandInspection.blacklistStatePersisted ? 1 : 0,
                     statusName (watchdogBlacklistPolicyDecisionOutcome.status),
                     statusName (watchdogBlacklistPolicyDecisionOutcome.failureKind),
                     watchdogBlacklistPolicyDecisionOutcome.crashCandidate ? 1 : 0,
                     watchdogBlacklistPolicyDecisionOutcome.watchdogTimeoutCandidate ? 1 : 0,
                     watchdogBlacklistPolicyDecisionOutcome.controlThreadPolicyDecisionInspected ? 1 : 0,
                     statusName (queuedWatchdogBlacklistPolicyDecisionOutcome.status),
                     statusName (queuedWatchdogBlacklistPolicyDecisionOutcome.failureKind),
                     statusName (pendingWatchdogBlacklistPolicyDecisionOutcome.status),
                     statusName (pendingWatchdogBlacklistPolicyDecisionOutcome.failureKind),
                     statusName (drainedWatchdogBlacklistPolicyDecisionOutcome.status),
                     statusName (drainedWatchdogBlacklistPolicyDecisionOutcome.failureKind),
                     statusName (afterWatchdogDrainBlacklistPolicyDecisionOutcome.status),
                     statusName (afterWatchdogDrainBlacklistPolicyDecisionOutcome.failureKind),
                     watchdogBlacklistPolicyDecisionOutcome.blacklistPolicyApplied ? 1 : 0,
                     watchdogBlacklistPolicyDecisionOutcome.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    const auto alreadyDrainedWatchdogBlacklistPolicyDecisionOutcomeHandlingResult =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistPolicyDecisionOutcomeToControlHandling();
    const auto requeuedWatchdogBlacklistPolicyDecisionOutcome =
        blacklistPolicyCommandReceiptCoordinator.queueBlacklistPolicyDecisionOutcomeForDeferredCommand();
    const auto watchdogBlacklistPolicyDecisionOutcomeHandlingResult =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistPolicyDecisionOutcomeToControlHandling();
    const auto afterWatchdogHandlingBlacklistPolicyDecisionOutcome =
        blacklistPolicyCommandReceiptCoordinator.pendingBlacklistPolicyDecisionOutcomeStatus();
    const auto afterWatchdogHandlingSecondResult =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistPolicyDecisionOutcomeToControlHandling();
    if (! blacklistPolicyDecisionOutcomeHandlingResultMatches (
            alreadyDrainedWatchdogBlacklistPolicyDecisionOutcomeHandlingResult, {})
        || ! blacklistPolicyDecisionOutcomeMatches (
            requeuedWatchdogBlacklistPolicyDecisionOutcome, watchdogBlacklistPolicyDecisionOutcome)
        || ! blacklistPolicyDecisionOutcomeHandlingResultMatches (
            watchdogBlacklistPolicyDecisionOutcomeHandlingResult,
            { yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionOutcomeHandlingStatus::handlingReady,
              watchdogBlacklistPolicyDecisionOutcome,
              { yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout,
                false,
                true,
                true },
              true,
              false,
              false })
        || ! blacklistPolicyDecisionOutcomeMatches (afterWatchdogHandlingBlacklistPolicyDecisionOutcome, {})
        || ! blacklistPolicyDecisionOutcomeHandlingResultMatches (afterWatchdogHandlingSecondResult, {}))
    {
        std::printf ("FAIL: plugin host coordinator watchdog blacklist policy-decision outcome handling shell is wrong: alreadyDrained=%s/%d requeued=%s/%s handling=%s drained=%s/%s/%d/%d/%d handled=%s/%d/%d/%d consumed=%d after=%s/%s second=%s/%d policy=%d persisted=%d\n",
                     statusName (alreadyDrainedWatchdogBlacklistPolicyDecisionOutcomeHandlingResult.status),
                     alreadyDrainedWatchdogBlacklistPolicyDecisionOutcomeHandlingResult.pendingOutcomeConsumed ? 1 : 0,
                     statusName (requeuedWatchdogBlacklistPolicyDecisionOutcome.status),
                     statusName (requeuedWatchdogBlacklistPolicyDecisionOutcome.failureKind),
                     statusName (watchdogBlacklistPolicyDecisionOutcomeHandlingResult.status),
                     statusName (watchdogBlacklistPolicyDecisionOutcomeHandlingResult.drainedOutcome.status),
                     statusName (watchdogBlacklistPolicyDecisionOutcomeHandlingResult.drainedOutcome.failureKind),
                     watchdogBlacklistPolicyDecisionOutcomeHandlingResult.drainedOutcome.crashCandidate ? 1 : 0,
                     watchdogBlacklistPolicyDecisionOutcomeHandlingResult.drainedOutcome.watchdogTimeoutCandidate ? 1 : 0,
                     watchdogBlacklistPolicyDecisionOutcomeHandlingResult.drainedOutcome.controlThreadPolicyDecisionInspected ? 1 : 0,
                     statusName (watchdogBlacklistPolicyDecisionOutcomeHandlingResult.handling.failureKind),
                     watchdogBlacklistPolicyDecisionOutcomeHandlingResult.handling.crashCandidate ? 1 : 0,
                     watchdogBlacklistPolicyDecisionOutcomeHandlingResult.handling.watchdogTimeoutCandidate ? 1 : 0,
                     watchdogBlacklistPolicyDecisionOutcomeHandlingResult.handling.controlThreadBlacklistHandlingRequested ? 1 : 0,
                     watchdogBlacklistPolicyDecisionOutcomeHandlingResult.pendingOutcomeConsumed ? 1 : 0,
                     statusName (afterWatchdogHandlingBlacklistPolicyDecisionOutcome.status),
                     statusName (afterWatchdogHandlingBlacklistPolicyDecisionOutcome.failureKind),
                     statusName (afterWatchdogHandlingSecondResult.status),
                     afterWatchdogHandlingSecondResult.pendingOutcomeConsumed ? 1 : 0,
                     watchdogBlacklistPolicyDecisionOutcomeHandlingResult.blacklistPolicyApplied ? 1 : 0,
                     watchdogBlacklistPolicyDecisionOutcomeHandlingResult.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    const auto watchdogDeferredBlacklistPolicyDecisionOutcomeHandlingStatus =
        blacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistPolicyDecisionOutcomeHandlingResult (
            watchdogBlacklistPolicyDecisionOutcomeHandlingResult);
    const auto watchdogDeferredBlacklistPolicyDecisionOutcomeHandlingInspection =
        blacklistPolicyCommandReceiptCoordinator.deferredBlacklistPolicyDecisionOutcomeHandlingStatus();
    const auto watchdogBlacklistHandlingRequest =
        blacklistPolicyCommandReceiptCoordinator.blacklistHandlingRequest();
    const auto queuedWatchdogBlacklistHandlingRequest =
        blacklistPolicyCommandReceiptCoordinator.queueBlacklistHandlingRequestForDeferredOutcomeHandling();
    const auto pendingWatchdogBlacklistHandlingRequest =
        blacklistPolicyCommandReceiptCoordinator.pendingBlacklistHandlingRequest();
    const auto drainedWatchdogBlacklistHandlingRequest =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistHandlingRequest();
    const auto afterDrainedWatchdogBlacklistHandlingRequest =
        blacklistPolicyCommandReceiptCoordinator.pendingBlacklistHandlingRequest();
    const auto afterSecondDrainedWatchdogBlacklistHandlingRequest =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistHandlingRequest();
    const auto commandQueuedWatchdogBlacklistHandlingRequest =
        blacklistPolicyCommandReceiptCoordinator.queueBlacklistHandlingRequestForDeferredOutcomeHandling();
    const auto watchdogBlacklistHandlingCommandResult =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistHandlingRequestToControlCommand();
    const auto afterWatchdogCommandDrainBlacklistHandlingRequest =
        blacklistPolicyCommandReceiptCoordinator.pendingBlacklistHandlingRequest();
    const auto afterWatchdogCommandSecondDrainBlacklistHandlingCommandResult =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistHandlingRequestToControlCommand();
    const auto watchdogDeferredBlacklistHandlingCommandStatus =
        blacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistHandlingCommandResult (
            watchdogBlacklistHandlingCommandResult);
    const auto watchdogDeferredBlacklistHandlingCommandInspection =
        blacklistPolicyCommandReceiptCoordinator.deferredBlacklistHandlingCommandStatus();
    const auto watchdogBlacklistHandlingOutcome =
        blacklistPolicyCommandReceiptCoordinator.blacklistHandlingOutcomeStatus();
    const auto queuedWatchdogBlacklistHandlingOutcome =
        blacklistPolicyCommandReceiptCoordinator.queueBlacklistHandlingOutcomeForDeferredCommand();
    const auto pendingWatchdogBlacklistHandlingOutcome =
        blacklistPolicyCommandReceiptCoordinator.pendingBlacklistHandlingOutcomeStatus();
    const auto drainedWatchdogBlacklistHandlingOutcome =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistHandlingOutcomeStatus();
    const auto afterDrainedWatchdogBlacklistHandlingOutcome =
        blacklistPolicyCommandReceiptCoordinator.pendingBlacklistHandlingOutcomeStatus();
    const auto afterSecondDrainedWatchdogBlacklistHandlingOutcome =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistHandlingOutcomeStatus();
    const auto handlingQueuedWatchdogBlacklistHandlingOutcome =
        blacklistPolicyCommandReceiptCoordinator.queueBlacklistHandlingOutcomeForDeferredCommand();
    const auto watchdogBlacklistHandlingOutcomeHandlingResult =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistHandlingOutcomeToControlHandling();
    const auto afterWatchdogHandlingDrainBlacklistHandlingOutcome =
        blacklistPolicyCommandReceiptCoordinator.pendingBlacklistHandlingOutcomeStatus();
    const auto afterWatchdogHandlingSecondBlacklistHandlingOutcomeHandlingResult =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistHandlingOutcomeToControlHandling();
    const auto afterWatchdogHandlingDeferredBlacklistHandlingCommandStatus =
        blacklistPolicyCommandReceiptCoordinator.deferredBlacklistHandlingCommandStatus();
    const auto watchdogDeferredBlacklistHandlingOutcomeHandlingStatus =
        blacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistHandlingOutcomeHandlingResult (
            watchdogBlacklistHandlingOutcomeHandlingResult);
    const auto watchdogDeferredBlacklistHandlingOutcomeHandlingInspection =
        blacklistPolicyCommandReceiptCoordinator.deferredBlacklistHandlingOutcomeHandlingStatus();
    const auto afterWatchdogHandlingReceiptDeferredBlacklistHandlingCommandStatus =
        blacklistPolicyCommandReceiptCoordinator.deferredBlacklistHandlingCommandStatus();
    const auto acknowledgedWatchdogDeferredBlacklistHandlingOutcomeHandlingStatus =
        blacklistPolicyCommandReceiptCoordinator.acknowledgeDeferredBlacklistHandlingOutcomeHandlingStatus();
    const auto afterAcknowledgedWatchdogDeferredBlacklistHandlingOutcomeHandlingStatus =
        blacklistPolicyCommandReceiptCoordinator.deferredBlacklistHandlingOutcomeHandlingStatus();
    const auto afterAcknowledgedWatchdogOutcomeHandlingDeferredBlacklistHandlingCommandStatus =
        blacklistPolicyCommandReceiptCoordinator.deferredBlacklistHandlingCommandStatus();
    const auto acknowledgedWatchdogDeferredBlacklistHandlingCommandStatus =
        blacklistPolicyCommandReceiptCoordinator.acknowledgeDeferredBlacklistHandlingCommandStatus();
    const auto afterAcknowledgedWatchdogDeferredBlacklistHandlingCommandStatus =
        blacklistPolicyCommandReceiptCoordinator.deferredBlacklistHandlingCommandStatus();
    const auto afterAcknowledgedWatchdogBlacklistHandlingOutcome =
        blacklistPolicyCommandReceiptCoordinator.blacklistHandlingOutcomeStatus();
    const auto afterAcknowledgedQueuedWatchdogBlacklistHandlingOutcome =
        blacklistPolicyCommandReceiptCoordinator.queueBlacklistHandlingOutcomeForDeferredCommand();
    const auto acknowledgedWatchdogDeferredBlacklistPolicyDecisionOutcomeHandlingStatus =
        blacklistPolicyCommandReceiptCoordinator.acknowledgeDeferredBlacklistPolicyDecisionOutcomeHandlingStatus();
    const auto afterAcknowledgedWatchdogDeferredBlacklistPolicyDecisionOutcomeHandlingStatus =
        blacklistPolicyCommandReceiptCoordinator.deferredBlacklistPolicyDecisionOutcomeHandlingStatus();
    const auto afterAcknowledgedWatchdogBlacklistHandlingRequest =
        blacklistPolicyCommandReceiptCoordinator.blacklistHandlingRequest();
    const auto afterAcknowledgedQueuedWatchdogBlacklistHandlingRequest =
        blacklistPolicyCommandReceiptCoordinator.queueBlacklistHandlingRequestForDeferredOutcomeHandling();
    const auto afterAcknowledgedWatchdogBlacklistHandlingCommandResult =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistHandlingRequestToControlCommand();
    if (watchdogDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.status
            != yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionOutcomeHandlingStatus::handlingReady
        || ! blacklistPolicyDecisionOutcomeHandlingResultMatches (
            watchdogDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.lastResult,
            watchdogBlacklistPolicyDecisionOutcomeHandlingResult)
        || ! watchdogDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.handlingRecorded
        || watchdogDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.blacklistPolicyApplied
        || watchdogDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.blacklistStatePersisted
        || ! blacklistPolicyDecisionOutcomeHandlingResultMatches (
            watchdogDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.lastResult,
            watchdogBlacklistPolicyDecisionOutcomeHandlingResult)
        || ! watchdogDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.handlingRecorded
        || watchdogDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.blacklistPolicyApplied
        || watchdogDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.blacklistStatePersisted
        || ! deferredBlacklistPolicyDecisionOutcomeHandlingStatusMatches (
            acknowledgedWatchdogDeferredBlacklistPolicyDecisionOutcomeHandlingStatus, {})
        || ! deferredBlacklistPolicyDecisionOutcomeHandlingStatusMatches (
            afterAcknowledgedWatchdogDeferredBlacklistPolicyDecisionOutcomeHandlingStatus, {})
        || ! blacklistHandlingRequestMatches (
            watchdogBlacklistHandlingRequest,
            { yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingRequestStatus::requestReady,
              yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout,
              false,
              true,
              true,
              false,
              false })
        || ! blacklistHandlingRequestMatches (afterAcknowledgedWatchdogBlacklistHandlingRequest, {}))
    {
        std::printf ("FAIL: plugin host coordinator watchdog deferred blacklist policy-decision outcome handling receipt/status/acknowledge/request is wrong: status=%s inspected=%s recorded=%d inspectedRecorded=%d policy=%d persisted=%d inspectedPolicy=%d inspectedPersisted=%d request=%s/%s/%d/%d/%d requestPolicy=%d requestPersisted=%d ack=%s ackRecorded=%d ackPolicy=%d ackPersisted=%d afterAck=%s afterAckRecorded=%d afterAckPolicy=%d afterAckPersisted=%d afterAckRequest=%s/%s/%d/%d/%d afterAckRequestPolicy=%d afterAckRequestPersisted=%d handling=%s/%s\n",
                     statusName (watchdogDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.status),
                     statusName (watchdogDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.status),
                     watchdogDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.handlingRecorded ? 1 : 0,
                     watchdogDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.handlingRecorded ? 1 : 0,
                     watchdogDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.blacklistPolicyApplied ? 1 : 0,
                     watchdogDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.blacklistStatePersisted ? 1 : 0,
                     watchdogDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.blacklistPolicyApplied ? 1 : 0,
                     watchdogDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.blacklistStatePersisted ? 1 : 0,
                     statusName (watchdogBlacklistHandlingRequest.status),
                     statusName (watchdogBlacklistHandlingRequest.failureKind),
                     watchdogBlacklistHandlingRequest.crashCandidate ? 1 : 0,
                     watchdogBlacklistHandlingRequest.watchdogTimeoutCandidate ? 1 : 0,
                     watchdogBlacklistHandlingRequest.controlThreadBlacklistHandlingRequested ? 1 : 0,
                     watchdogBlacklistHandlingRequest.blacklistPolicyApplied ? 1 : 0,
                     watchdogBlacklistHandlingRequest.blacklistStatePersisted ? 1 : 0,
                     statusName (acknowledgedWatchdogDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.status),
                     acknowledgedWatchdogDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.handlingRecorded ? 1 : 0,
                     acknowledgedWatchdogDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.blacklistPolicyApplied ? 1 : 0,
                     acknowledgedWatchdogDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.blacklistStatePersisted ? 1 : 0,
                     statusName (afterAcknowledgedWatchdogDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.status),
                     afterAcknowledgedWatchdogDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.handlingRecorded ? 1 : 0,
                     afterAcknowledgedWatchdogDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.blacklistPolicyApplied ? 1 : 0,
                     afterAcknowledgedWatchdogDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.blacklistStatePersisted ? 1 : 0,
                     statusName (afterAcknowledgedWatchdogBlacklistHandlingRequest.status),
                     statusName (afterAcknowledgedWatchdogBlacklistHandlingRequest.failureKind),
                     afterAcknowledgedWatchdogBlacklistHandlingRequest.crashCandidate ? 1 : 0,
                     afterAcknowledgedWatchdogBlacklistHandlingRequest.watchdogTimeoutCandidate ? 1 : 0,
                     afterAcknowledgedWatchdogBlacklistHandlingRequest.controlThreadBlacklistHandlingRequested ? 1 : 0,
                     afterAcknowledgedWatchdogBlacklistHandlingRequest.blacklistPolicyApplied ? 1 : 0,
                     afterAcknowledgedWatchdogBlacklistHandlingRequest.blacklistStatePersisted ? 1 : 0,
                     statusName (watchdogDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.lastResult.status),
                     statusName (watchdogDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.lastResult.handling.failureKind));
        return 2;
    }

    if (! blacklistHandlingRequestMatches (queuedWatchdogBlacklistHandlingRequest,
                                           watchdogBlacklistHandlingRequest)
        || ! blacklistHandlingRequestMatches (pendingWatchdogBlacklistHandlingRequest,
                                              watchdogBlacklistHandlingRequest)
        || ! blacklistHandlingRequestMatches (drainedWatchdogBlacklistHandlingRequest,
                                              watchdogBlacklistHandlingRequest)
        || ! blacklistHandlingRequestMatches (afterDrainedWatchdogBlacklistHandlingRequest, {})
        || ! blacklistHandlingRequestMatches (afterSecondDrainedWatchdogBlacklistHandlingRequest, {})
        || ! blacklistHandlingRequestMatches (commandQueuedWatchdogBlacklistHandlingRequest,
                                              watchdogBlacklistHandlingRequest)
        || ! blacklistHandlingCommandResultMatches (
            watchdogBlacklistHandlingCommandResult,
            { yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingCommandStatus::commandReady,
              watchdogBlacklistHandlingRequest,
              { yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingCommandKind::handleBlacklistRequest,
                yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout,
                false,
                true,
                true },
              true,
              false,
              false })
        || ! blacklistHandlingRequestMatches (afterWatchdogCommandDrainBlacklistHandlingRequest, {})
        || ! blacklistHandlingCommandResultMatches (
            afterWatchdogCommandSecondDrainBlacklistHandlingCommandResult, {})
        || ! deferredBlacklistHandlingCommandStatusMatches (
            watchdogDeferredBlacklistHandlingCommandStatus,
            { yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingCommandStatus::commandReady,
              watchdogBlacklistHandlingCommandResult,
              true,
              false,
              false })
        || ! deferredBlacklistHandlingCommandStatusMatches (
            watchdogDeferredBlacklistHandlingCommandInspection,
            watchdogDeferredBlacklistHandlingCommandStatus)
        || ! blacklistHandlingOutcomeMatches (
            watchdogBlacklistHandlingOutcome,
            { yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingOutcomeStatus::outcomeReady,
              yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout,
              false,
              true,
              true,
              false,
              false })
        || ! blacklistHandlingOutcomeMatches (
            queuedWatchdogBlacklistHandlingOutcome, watchdogBlacklistHandlingOutcome)
        || ! blacklistHandlingOutcomeMatches (
            pendingWatchdogBlacklistHandlingOutcome, watchdogBlacklistHandlingOutcome)
        || ! blacklistHandlingOutcomeMatches (
            drainedWatchdogBlacklistHandlingOutcome, watchdogBlacklistHandlingOutcome)
        || ! blacklistHandlingOutcomeMatches (afterDrainedWatchdogBlacklistHandlingOutcome, {})
        || ! blacklistHandlingOutcomeMatches (afterSecondDrainedWatchdogBlacklistHandlingOutcome, {})
        || ! blacklistHandlingOutcomeMatches (
            handlingQueuedWatchdogBlacklistHandlingOutcome, watchdogBlacklistHandlingOutcome)
        || ! blacklistHandlingOutcomeHandlingResultMatches (
            watchdogBlacklistHandlingOutcomeHandlingResult,
            { yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingOutcomeHandlingStatus::handlingReady,
              watchdogBlacklistHandlingOutcome,
              { yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout,
                false,
                true,
                true },
              true,
              false,
              false })
        || ! blacklistHandlingOutcomeMatches (afterWatchdogHandlingDrainBlacklistHandlingOutcome, {})
        || ! blacklistHandlingOutcomeHandlingResultMatches (
            afterWatchdogHandlingSecondBlacklistHandlingOutcomeHandlingResult, {})
        || ! deferredBlacklistHandlingCommandStatusMatches (
            afterWatchdogHandlingDeferredBlacklistHandlingCommandStatus,
            watchdogDeferredBlacklistHandlingCommandStatus)
        || ! deferredBlacklistHandlingOutcomeHandlingStatusMatches (
            watchdogDeferredBlacklistHandlingOutcomeHandlingStatus,
            { yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingOutcomeHandlingStatus::handlingReady,
              watchdogBlacklistHandlingOutcomeHandlingResult,
              true,
              false,
              false })
        || ! deferredBlacklistHandlingOutcomeHandlingStatusMatches (
            watchdogDeferredBlacklistHandlingOutcomeHandlingInspection,
            watchdogDeferredBlacklistHandlingOutcomeHandlingStatus)
        || ! deferredBlacklistHandlingCommandStatusMatches (
            afterWatchdogHandlingReceiptDeferredBlacklistHandlingCommandStatus,
            watchdogDeferredBlacklistHandlingCommandStatus)
        || ! deferredBlacklistHandlingOutcomeHandlingStatusMatches (
            acknowledgedWatchdogDeferredBlacklistHandlingOutcomeHandlingStatus, {})
        || ! deferredBlacklistHandlingOutcomeHandlingStatusMatches (
            afterAcknowledgedWatchdogDeferredBlacklistHandlingOutcomeHandlingStatus, {})
        || ! deferredBlacklistHandlingCommandStatusMatches (
            afterAcknowledgedWatchdogOutcomeHandlingDeferredBlacklistHandlingCommandStatus,
            watchdogDeferredBlacklistHandlingCommandStatus)
        || ! deferredBlacklistHandlingCommandStatusMatches (
            acknowledgedWatchdogDeferredBlacklistHandlingCommandStatus, {})
        || ! deferredBlacklistHandlingCommandStatusMatches (
            afterAcknowledgedWatchdogDeferredBlacklistHandlingCommandStatus, {})
        || ! blacklistHandlingOutcomeMatches (afterAcknowledgedWatchdogBlacklistHandlingOutcome, {})
        || ! blacklistHandlingOutcomeMatches (afterAcknowledgedQueuedWatchdogBlacklistHandlingOutcome, {})
        || ! blacklistHandlingRequestMatches (afterAcknowledgedQueuedWatchdogBlacklistHandlingRequest, {})
        || ! blacklistHandlingCommandResultMatches (afterAcknowledgedWatchdogBlacklistHandlingCommandResult, {}))
    {
        std::printf ("FAIL: plugin host coordinator watchdog pending blacklist-handling request command receipt/status/outcome queue/handling shell is wrong: request=%s/%s queued=%s/%s pending=%s/%s drained=%s/%s after=%s/%s second=%s/%s commandQueued=%s/%s command=%s/%s/%s consumed=%d policy=%d persisted=%d afterCommand=%s/%s secondCommand=%s/%s receipt=%s receiptRecorded=%d receiptCommand=%s/%s inspected=%s inspectedRecorded=%d inspectedCommand=%s/%s outcome=%s/%s/%d/%d/%d queuedOutcome=%s/%s pendingOutcome=%s/%s drainedOutcome=%s/%s afterDrainedOutcome=%s/%s secondDrainedOutcome=%s/%s handlingQueued=%s/%s handling=%s/%s/%d handlingConsumed=%d afterHandling=%s/%s secondHandling=%s afterHandlingReceipt=%s/%d outcomePolicy=%d outcomePersisted=%d handlingPolicy=%d handlingPersisted=%d receiptAck=%s receiptAckRecorded=%d afterReceiptAck=%s afterReceiptAckRecorded=%d afterOutcome=%s/%s/%d/%d/%d afterAckQueued=%s/%s afterAckCommand=%s/%s\n",
                     statusName (watchdogBlacklistHandlingRequest.status),
                     statusName (watchdogBlacklistHandlingRequest.failureKind),
                     statusName (queuedWatchdogBlacklistHandlingRequest.status),
                     statusName (queuedWatchdogBlacklistHandlingRequest.failureKind),
                     statusName (pendingWatchdogBlacklistHandlingRequest.status),
                     statusName (pendingWatchdogBlacklistHandlingRequest.failureKind),
                     statusName (drainedWatchdogBlacklistHandlingRequest.status),
                     statusName (drainedWatchdogBlacklistHandlingRequest.failureKind),
                     statusName (afterDrainedWatchdogBlacklistHandlingRequest.status),
                     statusName (afterDrainedWatchdogBlacklistHandlingRequest.failureKind),
                     statusName (afterSecondDrainedWatchdogBlacklistHandlingRequest.status),
                     statusName (afterSecondDrainedWatchdogBlacklistHandlingRequest.failureKind),
                     statusName (commandQueuedWatchdogBlacklistHandlingRequest.status),
                     statusName (commandQueuedWatchdogBlacklistHandlingRequest.failureKind),
                     statusName (watchdogBlacklistHandlingCommandResult.status),
                     statusName (watchdogBlacklistHandlingCommandResult.command.command),
                     statusName (watchdogBlacklistHandlingCommandResult.command.failureKind),
                     watchdogBlacklistHandlingCommandResult.pendingRequestConsumed ? 1 : 0,
                     watchdogBlacklistHandlingCommandResult.blacklistPolicyApplied ? 1 : 0,
                     watchdogBlacklistHandlingCommandResult.blacklistStatePersisted ? 1 : 0,
                     statusName (afterWatchdogCommandDrainBlacklistHandlingRequest.status),
                     statusName (afterWatchdogCommandDrainBlacklistHandlingRequest.failureKind),
                     statusName (afterWatchdogCommandSecondDrainBlacklistHandlingCommandResult.status),
                     statusName (afterWatchdogCommandSecondDrainBlacklistHandlingCommandResult.command.failureKind),
                     statusName (watchdogDeferredBlacklistHandlingCommandStatus.status),
                     watchdogDeferredBlacklistHandlingCommandStatus.commandRecorded ? 1 : 0,
                     statusName (watchdogDeferredBlacklistHandlingCommandStatus.lastResult.command.command),
                     statusName (watchdogDeferredBlacklistHandlingCommandStatus.lastResult.command.failureKind),
                     statusName (watchdogDeferredBlacklistHandlingCommandInspection.status),
                     watchdogDeferredBlacklistHandlingCommandInspection.commandRecorded ? 1 : 0,
                     statusName (watchdogDeferredBlacklistHandlingCommandInspection.lastResult.command.command),
                     statusName (watchdogDeferredBlacklistHandlingCommandInspection.lastResult.command.failureKind),
                     statusName (watchdogBlacklistHandlingOutcome.status),
                     statusName (watchdogBlacklistHandlingOutcome.failureKind),
                     watchdogBlacklistHandlingOutcome.crashCandidate ? 1 : 0,
                     watchdogBlacklistHandlingOutcome.watchdogTimeoutCandidate ? 1 : 0,
                     watchdogBlacklistHandlingOutcome.controlThreadBlacklistHandlingInspected ? 1 : 0,
                     statusName (queuedWatchdogBlacklistHandlingOutcome.status),
                     statusName (queuedWatchdogBlacklistHandlingOutcome.failureKind),
                     statusName (pendingWatchdogBlacklistHandlingOutcome.status),
                     statusName (pendingWatchdogBlacklistHandlingOutcome.failureKind),
                     statusName (drainedWatchdogBlacklistHandlingOutcome.status),
                     statusName (drainedWatchdogBlacklistHandlingOutcome.failureKind),
                     statusName (afterDrainedWatchdogBlacklistHandlingOutcome.status),
                     statusName (afterDrainedWatchdogBlacklistHandlingOutcome.failureKind),
                     statusName (afterSecondDrainedWatchdogBlacklistHandlingOutcome.status),
                     statusName (afterSecondDrainedWatchdogBlacklistHandlingOutcome.failureKind),
                     statusName (handlingQueuedWatchdogBlacklistHandlingOutcome.status),
                     statusName (handlingQueuedWatchdogBlacklistHandlingOutcome.failureKind),
                     statusName (watchdogBlacklistHandlingOutcomeHandlingResult.status),
                     statusName (watchdogBlacklistHandlingOutcomeHandlingResult.handling.failureKind),
                     watchdogBlacklistHandlingOutcomeHandlingResult.handling.controlThreadBlacklistHandlingRequested ? 1 : 0,
                     watchdogBlacklistHandlingOutcomeHandlingResult.pendingOutcomeConsumed ? 1 : 0,
                     statusName (afterWatchdogHandlingDrainBlacklistHandlingOutcome.status),
                     statusName (afterWatchdogHandlingDrainBlacklistHandlingOutcome.failureKind),
                     statusName (afterWatchdogHandlingSecondBlacklistHandlingOutcomeHandlingResult.status),
                     statusName (afterWatchdogHandlingDeferredBlacklistHandlingCommandStatus.status),
                     afterWatchdogHandlingDeferredBlacklistHandlingCommandStatus.commandRecorded ? 1 : 0,
                     watchdogBlacklistHandlingOutcome.blacklistPolicyApplied ? 1 : 0,
                     watchdogBlacklistHandlingOutcome.blacklistStatePersisted ? 1 : 0,
                     watchdogBlacklistHandlingOutcomeHandlingResult.blacklistPolicyApplied ? 1 : 0,
                     watchdogBlacklistHandlingOutcomeHandlingResult.blacklistStatePersisted ? 1 : 0,
                     statusName (acknowledgedWatchdogDeferredBlacklistHandlingCommandStatus.status),
                     acknowledgedWatchdogDeferredBlacklistHandlingCommandStatus.commandRecorded ? 1 : 0,
                     statusName (afterAcknowledgedWatchdogDeferredBlacklistHandlingCommandStatus.status),
                     afterAcknowledgedWatchdogDeferredBlacklistHandlingCommandStatus.commandRecorded ? 1 : 0,
                     statusName (afterAcknowledgedWatchdogBlacklistHandlingOutcome.status),
                     statusName (afterAcknowledgedWatchdogBlacklistHandlingOutcome.failureKind),
                     afterAcknowledgedWatchdogBlacklistHandlingOutcome.crashCandidate ? 1 : 0,
                     afterAcknowledgedWatchdogBlacklistHandlingOutcome.watchdogTimeoutCandidate ? 1 : 0,
                     afterAcknowledgedWatchdogBlacklistHandlingOutcome.controlThreadBlacklistHandlingInspected ? 1 : 0,
                     statusName (afterAcknowledgedQueuedWatchdogBlacklistHandlingRequest.status),
                     statusName (afterAcknowledgedQueuedWatchdogBlacklistHandlingRequest.failureKind),
                     statusName (afterAcknowledgedWatchdogBlacklistHandlingCommandResult.status),
                     statusName (afterAcknowledgedWatchdogBlacklistHandlingCommandResult.command.failureKind));
        return 2;
    }

    const auto acknowledgedWatchdogDeferredBlacklistPolicyDecisionCommandStatus =
        blacklistPolicyCommandReceiptCoordinator.acknowledgeDeferredBlacklistPolicyDecisionCommandStatus();
    const auto afterAcknowledgedWatchdogDeferredBlacklistPolicyDecisionCommandStatus =
        blacklistPolicyCommandReceiptCoordinator.deferredBlacklistPolicyDecisionCommandStatus();
    const auto afterAcknowledgedWatchdogBlacklistPolicyDecisionOutcome =
        blacklistPolicyCommandReceiptCoordinator.blacklistPolicyDecisionOutcomeStatus();
    const auto afterAcknowledgedQueuedWatchdogBlacklistPolicyDecisionOutcome =
        blacklistPolicyCommandReceiptCoordinator.queueBlacklistPolicyDecisionOutcomeForDeferredCommand();
    const auto afterAcknowledgedWatchdogBlacklistPolicyDecisionOutcomeHandlingResult =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistPolicyDecisionOutcomeToControlHandling();
    if (! deferredBlacklistPolicyDecisionCommandStatusMatches (
            acknowledgedWatchdogDeferredBlacklistPolicyDecisionCommandStatus, {})
        || ! deferredBlacklistPolicyDecisionCommandStatusMatches (
            afterAcknowledgedWatchdogDeferredBlacklistPolicyDecisionCommandStatus, {})
        || ! blacklistPolicyDecisionOutcomeMatches (afterAcknowledgedWatchdogBlacklistPolicyDecisionOutcome, {})
        || ! blacklistPolicyDecisionOutcomeMatches (
            afterAcknowledgedQueuedWatchdogBlacklistPolicyDecisionOutcome, {})
        || ! blacklistPolicyDecisionOutcomeHandlingResultMatches (
            afterAcknowledgedWatchdogBlacklistPolicyDecisionOutcomeHandlingResult, {}))
    {
        std::printf ("FAIL: plugin host coordinator watchdog deferred blacklist policy-decision command acknowledge/clear status/outcome is wrong: status=%s inspected=%s recorded=%d inspectedRecorded=%d policy=%d persisted=%d inspectedPolicy=%d inspectedPersisted=%d outcome=%s/%s/%d/%d/%d queuedOutcome=%s/%s handling=%s/%d outcomePolicy=%d outcomePersisted=%d handlingPolicy=%d handlingPersisted=%d\n",
                     statusName (acknowledgedWatchdogDeferredBlacklistPolicyDecisionCommandStatus.status),
                     statusName (afterAcknowledgedWatchdogDeferredBlacklistPolicyDecisionCommandStatus.status),
                     acknowledgedWatchdogDeferredBlacklistPolicyDecisionCommandStatus.commandRecorded ? 1 : 0,
                     afterAcknowledgedWatchdogDeferredBlacklistPolicyDecisionCommandStatus.commandRecorded ? 1 : 0,
                     acknowledgedWatchdogDeferredBlacklistPolicyDecisionCommandStatus.blacklistPolicyApplied ? 1 : 0,
                     acknowledgedWatchdogDeferredBlacklistPolicyDecisionCommandStatus.blacklistStatePersisted ? 1 : 0,
                     afterAcknowledgedWatchdogDeferredBlacklistPolicyDecisionCommandStatus.blacklistPolicyApplied ? 1 : 0,
                     afterAcknowledgedWatchdogDeferredBlacklistPolicyDecisionCommandStatus.blacklistStatePersisted ? 1 : 0,
                     statusName (afterAcknowledgedWatchdogBlacklistPolicyDecisionOutcome.status),
                     statusName (afterAcknowledgedWatchdogBlacklistPolicyDecisionOutcome.failureKind),
                     afterAcknowledgedWatchdogBlacklistPolicyDecisionOutcome.crashCandidate ? 1 : 0,
                     afterAcknowledgedWatchdogBlacklistPolicyDecisionOutcome.watchdogTimeoutCandidate ? 1 : 0,
                     afterAcknowledgedWatchdogBlacklistPolicyDecisionOutcome.controlThreadPolicyDecisionInspected ? 1 : 0,
                     statusName (afterAcknowledgedQueuedWatchdogBlacklistPolicyDecisionOutcome.status),
                     statusName (afterAcknowledgedQueuedWatchdogBlacklistPolicyDecisionOutcome.failureKind),
                     statusName (afterAcknowledgedWatchdogBlacklistPolicyDecisionOutcomeHandlingResult.status),
                     afterAcknowledgedWatchdogBlacklistPolicyDecisionOutcomeHandlingResult.pendingOutcomeConsumed ? 1 : 0,
                     afterAcknowledgedWatchdogBlacklistPolicyDecisionOutcome.blacklistPolicyApplied ? 1 : 0,
                     afterAcknowledgedWatchdogBlacklistPolicyDecisionOutcome.blacklistStatePersisted ? 1 : 0,
                     afterAcknowledgedWatchdogBlacklistPolicyDecisionOutcomeHandlingResult.blacklistPolicyApplied ? 1 : 0,
                     afterAcknowledgedWatchdogBlacklistPolicyDecisionOutcomeHandlingResult.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    yesdaw::plugin_host::PluginHostCoordinator invalidBlacklistReceiptCoordinator;
    yesdaw::plugin_host::PluginHostCoordinator invalidBlacklistPolicyCommandReceiptCoordinator;
    auto unconsumedBlacklistEscalationResult = watchdogBlacklistEscalationResult;
    unconsumedBlacklistEscalationResult.pendingCandidateConsumed = false;
    auto policyAppliedBlacklistEscalationResult = watchdogBlacklistEscalationResult;
    policyAppliedBlacklistEscalationResult.blacklistPolicyApplied = true;
    auto persistedBlacklistEscalationResult = watchdogBlacklistEscalationResult;
    persistedBlacklistEscalationResult.blacklistStatePersisted = true;
    auto missingControlRequestBlacklistEscalationResult = watchdogBlacklistEscalationResult;
    missingControlRequestBlacklistEscalationResult.escalation.controlThreadEscalationRequested = false;
    auto mismatchedBlacklistEscalationResult = watchdogBlacklistEscalationResult;
    mismatchedBlacklistEscalationResult.escalation.failureKind =
        yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash;
    const auto unconsumedDeferredBlacklistEscalationStatus =
        invalidBlacklistReceiptCoordinator.recordDeferredBlacklistEscalationResult (unconsumedBlacklistEscalationResult);
    const auto policyAppliedDeferredBlacklistEscalationStatus =
        invalidBlacklistReceiptCoordinator.recordDeferredBlacklistEscalationResult (policyAppliedBlacklistEscalationResult);
    const auto persistedDeferredBlacklistEscalationStatus =
        invalidBlacklistReceiptCoordinator.recordDeferredBlacklistEscalationResult (persistedBlacklistEscalationResult);
    const auto missingControlRequestDeferredBlacklistEscalationStatus =
        invalidBlacklistReceiptCoordinator.recordDeferredBlacklistEscalationResult (missingControlRequestBlacklistEscalationResult);
    const auto mismatchedDeferredBlacklistEscalationStatus =
        invalidBlacklistReceiptCoordinator.recordDeferredBlacklistEscalationResult (mismatchedBlacklistEscalationResult);
    const auto invalidBlacklistPolicyDecisionRequest =
        invalidBlacklistReceiptCoordinator.blacklistPolicyDecisionRequest();
    const auto invalidQueuedBlacklistPolicyDecisionRequest =
        invalidBlacklistReceiptCoordinator.queueBlacklistPolicyDecisionRequestForDeferredEscalation();
    const auto invalidDrainedBlacklistPolicyDecisionRequest =
        invalidBlacklistReceiptCoordinator.drainPendingBlacklistPolicyDecisionRequest();
    const auto invalidBlacklistPolicyDecisionCommandResult =
        invalidBlacklistReceiptCoordinator.drainPendingBlacklistPolicyDecisionRequestToControlCommand();
    auto unconsumedBlacklistPolicyDecisionCommandResult = watchdogBlacklistPolicyDecisionCommandResult;
    unconsumedBlacklistPolicyDecisionCommandResult.pendingRequestConsumed = false;
    auto policyAppliedBlacklistPolicyDecisionCommandResult = watchdogBlacklistPolicyDecisionCommandResult;
    policyAppliedBlacklistPolicyDecisionCommandResult.blacklistPolicyApplied = true;
    auto persistedBlacklistPolicyDecisionCommandResult = watchdogBlacklistPolicyDecisionCommandResult;
    persistedBlacklistPolicyDecisionCommandResult.blacklistStatePersisted = true;
    auto missingControlPolicyDecisionCommandResult = watchdogBlacklistPolicyDecisionCommandResult;
    missingControlPolicyDecisionCommandResult.drainedRequest.controlThreadPolicyDecisionRequested = false;
    auto mismatchedPolicyDecisionCommandResult = watchdogBlacklistPolicyDecisionCommandResult;
    mismatchedPolicyDecisionCommandResult.command.failureKind =
        yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash;
    const auto unconsumedDeferredBlacklistPolicyDecisionCommandStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistPolicyDecisionCommandResult (
            unconsumedBlacklistPolicyDecisionCommandResult);
    const auto policyAppliedDeferredBlacklistPolicyDecisionCommandStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistPolicyDecisionCommandResult (
            policyAppliedBlacklistPolicyDecisionCommandResult);
    const auto persistedDeferredBlacklistPolicyDecisionCommandStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistPolicyDecisionCommandResult (
            persistedBlacklistPolicyDecisionCommandResult);
    const auto missingControlDeferredBlacklistPolicyDecisionCommandStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistPolicyDecisionCommandResult (
            missingControlPolicyDecisionCommandResult);
    const auto mismatchedDeferredBlacklistPolicyDecisionCommandStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistPolicyDecisionCommandResult (
            mismatchedPolicyDecisionCommandResult);
    const auto alreadyDrainedDeferredBlacklistPolicyDecisionCommandStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistPolicyDecisionCommandResult (
            afterWatchdogCommandSecondDrainBlacklistPolicyDecisionCommandResult);
    const auto invalidBlacklistPolicyDecisionOutcome =
        invalidBlacklistPolicyCommandReceiptCoordinator.blacklistPolicyDecisionOutcomeStatus();
    const auto invalidQueuedBlacklistPolicyDecisionOutcome =
        invalidBlacklistPolicyCommandReceiptCoordinator.queueBlacklistPolicyDecisionOutcomeForDeferredCommand();
    const auto invalidPendingBlacklistPolicyDecisionOutcome =
        invalidBlacklistPolicyCommandReceiptCoordinator.pendingBlacklistPolicyDecisionOutcomeStatus();
    const auto invalidDrainedBlacklistPolicyDecisionOutcome =
        invalidBlacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistPolicyDecisionOutcomeStatus();
    const auto invalidBlacklistPolicyDecisionOutcomeHandlingResult =
        invalidBlacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistPolicyDecisionOutcomeToControlHandling();
    auto unconsumedBlacklistPolicyDecisionOutcomeHandlingResult =
        watchdogBlacklistPolicyDecisionOutcomeHandlingResult;
    unconsumedBlacklistPolicyDecisionOutcomeHandlingResult.pendingOutcomeConsumed = false;
    auto policyAppliedBlacklistPolicyDecisionOutcomeHandlingResult =
        watchdogBlacklistPolicyDecisionOutcomeHandlingResult;
    policyAppliedBlacklistPolicyDecisionOutcomeHandlingResult.blacklistPolicyApplied = true;
    auto persistedBlacklistPolicyDecisionOutcomeHandlingResult =
        watchdogBlacklistPolicyDecisionOutcomeHandlingResult;
    persistedBlacklistPolicyDecisionOutcomeHandlingResult.blacklistStatePersisted = true;
    auto missingControlBlacklistPolicyDecisionOutcomeHandlingResult =
        watchdogBlacklistPolicyDecisionOutcomeHandlingResult;
    missingControlBlacklistPolicyDecisionOutcomeHandlingResult.handling.controlThreadBlacklistHandlingRequested = false;
    auto mismatchedBlacklistPolicyDecisionOutcomeHandlingResult =
        watchdogBlacklistPolicyDecisionOutcomeHandlingResult;
    mismatchedBlacklistPolicyDecisionOutcomeHandlingResult.handling.failureKind =
        yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash;
    const auto invalidDeferredBlacklistPolicyDecisionOutcomeHandlingStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.deferredBlacklistPolicyDecisionOutcomeHandlingStatus();
    const auto noActionDeferredBlacklistPolicyDecisionOutcomeHandlingStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistPolicyDecisionOutcomeHandlingResult (
            invalidBlacklistPolicyDecisionOutcomeHandlingResult);
    const auto unconsumedDeferredBlacklistPolicyDecisionOutcomeHandlingStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistPolicyDecisionOutcomeHandlingResult (
            unconsumedBlacklistPolicyDecisionOutcomeHandlingResult);
    const auto policyAppliedDeferredBlacklistPolicyDecisionOutcomeHandlingStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistPolicyDecisionOutcomeHandlingResult (
            policyAppliedBlacklistPolicyDecisionOutcomeHandlingResult);
    const auto persistedDeferredBlacklistPolicyDecisionOutcomeHandlingStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistPolicyDecisionOutcomeHandlingResult (
            persistedBlacklistPolicyDecisionOutcomeHandlingResult);
    const auto missingControlDeferredBlacklistPolicyDecisionOutcomeHandlingStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistPolicyDecisionOutcomeHandlingResult (
            missingControlBlacklistPolicyDecisionOutcomeHandlingResult);
    const auto mismatchedDeferredBlacklistPolicyDecisionOutcomeHandlingStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistPolicyDecisionOutcomeHandlingResult (
            mismatchedBlacklistPolicyDecisionOutcomeHandlingResult);
    const auto invalidBlacklistHandlingRequest =
        invalidBlacklistPolicyCommandReceiptCoordinator.blacklistHandlingRequest();
    const auto invalidBlacklistHandlingCommandResult =
        invalidBlacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistHandlingRequestToControlCommand();
    auto unconsumedBlacklistHandlingCommandResult = watchdogBlacklistHandlingCommandResult;
    unconsumedBlacklistHandlingCommandResult.pendingRequestConsumed = false;
    auto policyAppliedBlacklistHandlingCommandResult = watchdogBlacklistHandlingCommandResult;
    policyAppliedBlacklistHandlingCommandResult.blacklistPolicyApplied = true;
    auto persistedBlacklistHandlingCommandResult = watchdogBlacklistHandlingCommandResult;
    persistedBlacklistHandlingCommandResult.blacklistStatePersisted = true;
    auto missingControlBlacklistHandlingCommandResult = watchdogBlacklistHandlingCommandResult;
    missingControlBlacklistHandlingCommandResult.drainedRequest.controlThreadBlacklistHandlingRequested = false;
    auto mismatchedBlacklistHandlingCommandResult = watchdogBlacklistHandlingCommandResult;
    mismatchedBlacklistHandlingCommandResult.command.failureKind =
        yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash;
    const auto invalidDeferredBlacklistHandlingCommandStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.deferredBlacklistHandlingCommandStatus();
    const auto noActionDeferredBlacklistHandlingCommandStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistHandlingCommandResult (
            invalidBlacklistHandlingCommandResult);
    const auto unconsumedDeferredBlacklistHandlingCommandStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistHandlingCommandResult (
            unconsumedBlacklistHandlingCommandResult);
    const auto policyAppliedDeferredBlacklistHandlingCommandStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistHandlingCommandResult (
            policyAppliedBlacklistHandlingCommandResult);
    const auto persistedDeferredBlacklistHandlingCommandStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistHandlingCommandResult (
            persistedBlacklistHandlingCommandResult);
    const auto missingControlDeferredBlacklistHandlingCommandStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistHandlingCommandResult (
            missingControlBlacklistHandlingCommandResult);
    const auto mismatchedDeferredBlacklistHandlingCommandStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistHandlingCommandResult (
            mismatchedBlacklistHandlingCommandResult);
    const auto alreadyDrainedDeferredBlacklistHandlingCommandStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistHandlingCommandResult (
            afterWatchdogCommandSecondDrainBlacklistHandlingCommandResult);
    const auto invalidBlacklistHandlingOutcome =
        invalidBlacklistPolicyCommandReceiptCoordinator.blacklistHandlingOutcomeStatus();
    const auto invalidQueuedBlacklistHandlingOutcome =
        invalidBlacklistPolicyCommandReceiptCoordinator.queueBlacklistHandlingOutcomeForDeferredCommand();
    const auto invalidPendingBlacklistHandlingOutcome =
        invalidBlacklistPolicyCommandReceiptCoordinator.pendingBlacklistHandlingOutcomeStatus();
    const auto invalidDrainedBlacklistHandlingOutcome =
        invalidBlacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistHandlingOutcomeStatus();
    const auto invalidBlacklistHandlingOutcomeHandlingResult =
        invalidBlacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistHandlingOutcomeToControlHandling();
    auto unconsumedBlacklistHandlingOutcomeHandlingResult =
        watchdogBlacklistHandlingOutcomeHandlingResult;
    unconsumedBlacklistHandlingOutcomeHandlingResult.pendingOutcomeConsumed = false;
    auto policyAppliedBlacklistHandlingOutcomeHandlingResult =
        watchdogBlacklistHandlingOutcomeHandlingResult;
    policyAppliedBlacklistHandlingOutcomeHandlingResult.blacklistPolicyApplied = true;
    auto persistedBlacklistHandlingOutcomeHandlingResult =
        watchdogBlacklistHandlingOutcomeHandlingResult;
    persistedBlacklistHandlingOutcomeHandlingResult.blacklistStatePersisted = true;
    auto missingControlBlacklistHandlingOutcomeHandlingResult =
        watchdogBlacklistHandlingOutcomeHandlingResult;
    missingControlBlacklistHandlingOutcomeHandlingResult.handling.controlThreadBlacklistHandlingRequested =
        false;
    auto mismatchedBlacklistHandlingOutcomeHandlingResult =
        watchdogBlacklistHandlingOutcomeHandlingResult;
    mismatchedBlacklistHandlingOutcomeHandlingResult.handling.failureKind =
        yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash;
    const auto invalidDeferredBlacklistHandlingOutcomeHandlingStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.deferredBlacklistHandlingOutcomeHandlingStatus();
    const auto noActionDeferredBlacklistHandlingOutcomeHandlingStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistHandlingOutcomeHandlingResult (
            invalidBlacklistHandlingOutcomeHandlingResult);
    const auto unconsumedDeferredBlacklistHandlingOutcomeHandlingStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistHandlingOutcomeHandlingResult (
            unconsumedBlacklistHandlingOutcomeHandlingResult);
    const auto policyAppliedDeferredBlacklistHandlingOutcomeHandlingStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistHandlingOutcomeHandlingResult (
            policyAppliedBlacklistHandlingOutcomeHandlingResult);
    const auto persistedDeferredBlacklistHandlingOutcomeHandlingStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistHandlingOutcomeHandlingResult (
            persistedBlacklistHandlingOutcomeHandlingResult);
    const auto missingControlDeferredBlacklistHandlingOutcomeHandlingStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistHandlingOutcomeHandlingResult (
            missingControlBlacklistHandlingOutcomeHandlingResult);
    const auto mismatchedDeferredBlacklistHandlingOutcomeHandlingStatus =
        invalidBlacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistHandlingOutcomeHandlingResult (
            mismatchedBlacklistHandlingOutcomeHandlingResult);
    if (! deferredBlacklistEscalationStatusMatches (unconsumedDeferredBlacklistEscalationStatus, {})
        || ! deferredBlacklistEscalationStatusMatches (policyAppliedDeferredBlacklistEscalationStatus, {})
        || ! deferredBlacklistEscalationStatusMatches (persistedDeferredBlacklistEscalationStatus, {})
        || ! deferredBlacklistEscalationStatusMatches (missingControlRequestDeferredBlacklistEscalationStatus, {})
        || ! deferredBlacklistEscalationStatusMatches (mismatchedDeferredBlacklistEscalationStatus, {})
        || ! blacklistPolicyDecisionRequestMatches (invalidBlacklistPolicyDecisionRequest, {})
        || ! blacklistPolicyDecisionRequestMatches (invalidQueuedBlacklistPolicyDecisionRequest, {})
        || ! blacklistPolicyDecisionRequestMatches (invalidDrainedBlacklistPolicyDecisionRequest, {})
        || ! blacklistPolicyDecisionCommandResultMatches (invalidBlacklistPolicyDecisionCommandResult, {})
        || ! deferredBlacklistPolicyDecisionCommandStatusMatches (
            unconsumedDeferredBlacklistPolicyDecisionCommandStatus, {})
        || ! deferredBlacklistPolicyDecisionCommandStatusMatches (
            policyAppliedDeferredBlacklistPolicyDecisionCommandStatus, {})
        || ! deferredBlacklistPolicyDecisionCommandStatusMatches (
            persistedDeferredBlacklistPolicyDecisionCommandStatus, {})
        || ! deferredBlacklistPolicyDecisionCommandStatusMatches (
            missingControlDeferredBlacklistPolicyDecisionCommandStatus, {})
        || ! deferredBlacklistPolicyDecisionCommandStatusMatches (
            mismatchedDeferredBlacklistPolicyDecisionCommandStatus, {})
        || ! deferredBlacklistPolicyDecisionCommandStatusMatches (
            alreadyDrainedDeferredBlacklistPolicyDecisionCommandStatus, {})
        || ! blacklistPolicyDecisionOutcomeMatches (invalidBlacklistPolicyDecisionOutcome, {})
        || ! blacklistPolicyDecisionOutcomeMatches (invalidQueuedBlacklistPolicyDecisionOutcome, {})
        || ! blacklistPolicyDecisionOutcomeMatches (invalidPendingBlacklistPolicyDecisionOutcome, {})
        || ! blacklistPolicyDecisionOutcomeMatches (invalidDrainedBlacklistPolicyDecisionOutcome, {})
        || ! blacklistPolicyDecisionOutcomeHandlingResultMatches (
            invalidBlacklistPolicyDecisionOutcomeHandlingResult, {})
        || ! deferredBlacklistPolicyDecisionOutcomeHandlingStatusMatches (
            invalidDeferredBlacklistPolicyDecisionOutcomeHandlingStatus, {})
        || ! deferredBlacklistPolicyDecisionOutcomeHandlingStatusMatches (
            noActionDeferredBlacklistPolicyDecisionOutcomeHandlingStatus, {})
        || ! deferredBlacklistPolicyDecisionOutcomeHandlingStatusMatches (
            unconsumedDeferredBlacklistPolicyDecisionOutcomeHandlingStatus, {})
        || ! deferredBlacklistPolicyDecisionOutcomeHandlingStatusMatches (
            policyAppliedDeferredBlacklistPolicyDecisionOutcomeHandlingStatus, {})
        || ! deferredBlacklistPolicyDecisionOutcomeHandlingStatusMatches (
            persistedDeferredBlacklistPolicyDecisionOutcomeHandlingStatus, {})
        || ! deferredBlacklistPolicyDecisionOutcomeHandlingStatusMatches (
            missingControlDeferredBlacklistPolicyDecisionOutcomeHandlingStatus, {})
        || ! deferredBlacklistPolicyDecisionOutcomeHandlingStatusMatches (
            mismatchedDeferredBlacklistPolicyDecisionOutcomeHandlingStatus, {})
        || ! blacklistHandlingRequestMatches (invalidBlacklistHandlingRequest, {})
        || ! blacklistHandlingCommandResultMatches (invalidBlacklistHandlingCommandResult, {})
        || ! deferredBlacklistHandlingCommandStatusMatches (
            invalidDeferredBlacklistHandlingCommandStatus, {})
        || ! deferredBlacklistHandlingCommandStatusMatches (
            noActionDeferredBlacklistHandlingCommandStatus, {})
        || ! deferredBlacklistHandlingCommandStatusMatches (
            unconsumedDeferredBlacklistHandlingCommandStatus, {})
        || ! deferredBlacklistHandlingCommandStatusMatches (
            policyAppliedDeferredBlacklistHandlingCommandStatus, {})
        || ! deferredBlacklistHandlingCommandStatusMatches (
            persistedDeferredBlacklistHandlingCommandStatus, {})
        || ! deferredBlacklistHandlingCommandStatusMatches (
            missingControlDeferredBlacklistHandlingCommandStatus, {})
        || ! deferredBlacklistHandlingCommandStatusMatches (
            mismatchedDeferredBlacklistHandlingCommandStatus, {})
        || ! deferredBlacklistHandlingCommandStatusMatches (
            alreadyDrainedDeferredBlacklistHandlingCommandStatus, {})
        || ! blacklistHandlingOutcomeMatches (invalidBlacklistHandlingOutcome, {})
        || ! blacklistHandlingOutcomeMatches (invalidQueuedBlacklistHandlingOutcome, {})
        || ! blacklistHandlingOutcomeMatches (invalidPendingBlacklistHandlingOutcome, {})
        || ! blacklistHandlingOutcomeMatches (invalidDrainedBlacklistHandlingOutcome, {})
        || ! blacklistHandlingOutcomeHandlingResultMatches (
            invalidBlacklistHandlingOutcomeHandlingResult, {})
        || ! deferredBlacklistHandlingOutcomeHandlingStatusMatches (
            invalidDeferredBlacklistHandlingOutcomeHandlingStatus, {})
        || ! deferredBlacklistHandlingOutcomeHandlingStatusMatches (
            noActionDeferredBlacklistHandlingOutcomeHandlingStatus, {})
        || ! deferredBlacklistHandlingOutcomeHandlingStatusMatches (
            unconsumedDeferredBlacklistHandlingOutcomeHandlingStatus, {})
        || ! deferredBlacklistHandlingOutcomeHandlingStatusMatches (
            policyAppliedDeferredBlacklistHandlingOutcomeHandlingStatus, {})
        || ! deferredBlacklistHandlingOutcomeHandlingStatusMatches (
            persistedDeferredBlacklistHandlingOutcomeHandlingStatus, {})
        || ! deferredBlacklistHandlingOutcomeHandlingStatusMatches (
            missingControlDeferredBlacklistHandlingOutcomeHandlingStatus, {})
        || ! deferredBlacklistHandlingOutcomeHandlingStatusMatches (
            mismatchedDeferredBlacklistHandlingOutcomeHandlingStatus, {}))
    {
        std::printf ("FAIL: plugin host coordinator accepted invalid deferred blacklist escalation result/policy request command/outcome/handling receipt/request/command receipt/outcome queue/handling: unconsumed=%s policy=%s persisted=%s missingControl=%s mismatched=%s request=%s/%s/%d/%d/%d queued=%s/%s drained=%s/%s command=%s/%s/%s consumed=%d commandReceiptUnconsumed=%s commandReceiptPolicy=%s commandReceiptPersisted=%s commandReceiptMissingControl=%s commandReceiptMismatched=%s commandReceiptAlreadyDrained=%s outcome=%s/%s/%d/%d/%d queuedOutcome=%s/%s pendingOutcome=%s/%s drainedOutcome=%s/%s handling=%s handlingConsumed=%d handlingReceiptInitial=%s handlingReceiptNoAction=%s handlingReceiptUnconsumed=%s handlingReceiptPolicy=%s handlingReceiptPersisted=%s handlingReceiptMissingControl=%s handlingReceiptMismatched=%s blacklistRequest=%s/%s/%d/%d/%d handlingCommand=%s/%s/%s consumed=%d handlingCommandReceiptInitial=%s handlingCommandReceiptNoAction=%s handlingCommandReceiptUnconsumed=%s handlingCommandReceiptPolicy=%s handlingCommandReceiptPersisted=%s handlingCommandReceiptMissingControl=%s handlingCommandReceiptMismatched=%s handlingCommandReceiptAlreadyDrained=%s handlingOutcome=%s/%s queuedHandlingOutcome=%s/%s pendingHandlingOutcome=%s/%s drainedHandlingOutcome=%s/%s handlingResult=%s/%d requestPolicy=%d requestPersisted=%d commandPolicy=%d commandPersisted=%d outcomePolicy=%d outcomePersisted=%d handlingPolicy=%d handlingPersisted=%d blacklistRequestPolicy=%d blacklistRequestPersisted=%d handlingCommandPolicy=%d handlingCommandPersisted=%d handlingResultPolicy=%d handlingResultPersisted=%d\n",
                     statusName (unconsumedDeferredBlacklistEscalationStatus.status),
                     statusName (policyAppliedDeferredBlacklistEscalationStatus.status),
                     statusName (persistedDeferredBlacklistEscalationStatus.status),
                     statusName (missingControlRequestDeferredBlacklistEscalationStatus.status),
                     statusName (mismatchedDeferredBlacklistEscalationStatus.status),
                     statusName (invalidBlacklistPolicyDecisionRequest.status),
                     statusName (invalidBlacklistPolicyDecisionRequest.failureKind),
                     invalidBlacklistPolicyDecisionRequest.crashCandidate ? 1 : 0,
                     invalidBlacklistPolicyDecisionRequest.watchdogTimeoutCandidate ? 1 : 0,
                     invalidBlacklistPolicyDecisionRequest.controlThreadPolicyDecisionRequested ? 1 : 0,
                     statusName (invalidQueuedBlacklistPolicyDecisionRequest.status),
                     statusName (invalidQueuedBlacklistPolicyDecisionRequest.failureKind),
                     statusName (invalidDrainedBlacklistPolicyDecisionRequest.status),
                     statusName (invalidDrainedBlacklistPolicyDecisionRequest.failureKind),
                     statusName (invalidBlacklistPolicyDecisionCommandResult.status),
                     statusName (invalidBlacklistPolicyDecisionCommandResult.command.command),
                     statusName (invalidBlacklistPolicyDecisionCommandResult.command.failureKind),
                     invalidBlacklistPolicyDecisionCommandResult.pendingRequestConsumed ? 1 : 0,
                     statusName (unconsumedDeferredBlacklistPolicyDecisionCommandStatus.status),
                     statusName (policyAppliedDeferredBlacklistPolicyDecisionCommandStatus.status),
                     statusName (persistedDeferredBlacklistPolicyDecisionCommandStatus.status),
                     statusName (missingControlDeferredBlacklistPolicyDecisionCommandStatus.status),
                     statusName (mismatchedDeferredBlacklistPolicyDecisionCommandStatus.status),
                     statusName (alreadyDrainedDeferredBlacklistPolicyDecisionCommandStatus.status),
                     statusName (invalidBlacklistPolicyDecisionOutcome.status),
                     statusName (invalidBlacklistPolicyDecisionOutcome.failureKind),
                     invalidBlacklistPolicyDecisionOutcome.crashCandidate ? 1 : 0,
                     invalidBlacklistPolicyDecisionOutcome.watchdogTimeoutCandidate ? 1 : 0,
                     invalidBlacklistPolicyDecisionOutcome.controlThreadPolicyDecisionInspected ? 1 : 0,
                     statusName (invalidQueuedBlacklistPolicyDecisionOutcome.status),
                     statusName (invalidQueuedBlacklistPolicyDecisionOutcome.failureKind),
                     statusName (invalidPendingBlacklistPolicyDecisionOutcome.status),
                     statusName (invalidPendingBlacklistPolicyDecisionOutcome.failureKind),
                     statusName (invalidDrainedBlacklistPolicyDecisionOutcome.status),
                     statusName (invalidDrainedBlacklistPolicyDecisionOutcome.failureKind),
                     statusName (invalidBlacklistPolicyDecisionOutcomeHandlingResult.status),
                     invalidBlacklistPolicyDecisionOutcomeHandlingResult.pendingOutcomeConsumed ? 1 : 0,
                     statusName (invalidDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.status),
                     statusName (noActionDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.status),
                     statusName (unconsumedDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.status),
                     statusName (policyAppliedDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.status),
                     statusName (persistedDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.status),
                     statusName (missingControlDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.status),
                     statusName (mismatchedDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.status),
                     statusName (invalidBlacklistHandlingRequest.status),
                     statusName (invalidBlacklistHandlingRequest.failureKind),
                     invalidBlacklistHandlingRequest.crashCandidate ? 1 : 0,
                     invalidBlacklistHandlingRequest.watchdogTimeoutCandidate ? 1 : 0,
                     invalidBlacklistHandlingRequest.controlThreadBlacklistHandlingRequested ? 1 : 0,
                     statusName (invalidBlacklistHandlingCommandResult.status),
                     statusName (invalidBlacklistHandlingCommandResult.command.command),
                     statusName (invalidBlacklistHandlingCommandResult.command.failureKind),
                     invalidBlacklistHandlingCommandResult.pendingRequestConsumed ? 1 : 0,
                     statusName (invalidDeferredBlacklistHandlingCommandStatus.status),
                     statusName (noActionDeferredBlacklistHandlingCommandStatus.status),
                     statusName (unconsumedDeferredBlacklistHandlingCommandStatus.status),
                     statusName (policyAppliedDeferredBlacklistHandlingCommandStatus.status),
                     statusName (persistedDeferredBlacklistHandlingCommandStatus.status),
                     statusName (missingControlDeferredBlacklistHandlingCommandStatus.status),
                     statusName (mismatchedDeferredBlacklistHandlingCommandStatus.status),
                     statusName (alreadyDrainedDeferredBlacklistHandlingCommandStatus.status),
                     statusName (invalidBlacklistHandlingOutcome.status),
                     statusName (invalidBlacklistHandlingOutcome.failureKind),
                     statusName (invalidQueuedBlacklistHandlingOutcome.status),
                     statusName (invalidQueuedBlacklistHandlingOutcome.failureKind),
                     statusName (invalidPendingBlacklistHandlingOutcome.status),
                     statusName (invalidPendingBlacklistHandlingOutcome.failureKind),
                     statusName (invalidDrainedBlacklistHandlingOutcome.status),
                     statusName (invalidDrainedBlacklistHandlingOutcome.failureKind),
                     statusName (invalidBlacklistHandlingOutcomeHandlingResult.status),
                     invalidBlacklistHandlingOutcomeHandlingResult.pendingOutcomeConsumed ? 1 : 0,
                     invalidBlacklistPolicyDecisionRequest.blacklistPolicyApplied ? 1 : 0,
                     invalidBlacklistPolicyDecisionRequest.blacklistStatePersisted ? 1 : 0,
                     invalidBlacklistPolicyDecisionCommandResult.blacklistPolicyApplied ? 1 : 0,
                     invalidBlacklistPolicyDecisionCommandResult.blacklistStatePersisted ? 1 : 0,
                     invalidBlacklistPolicyDecisionOutcome.blacklistPolicyApplied ? 1 : 0,
                     invalidBlacklistPolicyDecisionOutcome.blacklistStatePersisted ? 1 : 0,
                     invalidBlacklistPolicyDecisionOutcomeHandlingResult.blacklistPolicyApplied ? 1 : 0,
                     invalidBlacklistPolicyDecisionOutcomeHandlingResult.blacklistStatePersisted ? 1 : 0,
                     invalidBlacklistHandlingRequest.blacklistPolicyApplied ? 1 : 0,
                     invalidBlacklistHandlingRequest.blacklistStatePersisted ? 1 : 0,
                     invalidBlacklistHandlingCommandResult.blacklistPolicyApplied ? 1 : 0,
                     invalidBlacklistHandlingCommandResult.blacklistStatePersisted ? 1 : 0,
                     invalidBlacklistHandlingOutcomeHandlingResult.blacklistPolicyApplied ? 1 : 0,
                     invalidBlacklistHandlingOutcomeHandlingResult.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    const auto watchdogAction = watchdogCoordinator.failureActionRequest();
    const auto autoQueuedWatchdogAction = watchdogCoordinator.pendingFailureActionRequest();
    if (watchdogAction.action != yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::bypassAndRecompile
        || watchdogAction.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout
        || ! watchdogAction.bypassRequested
        || ! watchdogAction.recompileRequested
        || ! requestMatches (autoQueuedWatchdogAction, watchdogAction))
    {
        std::printf ("FAIL: plugin host coordinator watchdog action request is wrong: action=%s failure=%s bypass=%d recompile=%d autoQueued=%s/%s\n",
                     statusName (watchdogAction.action),
                     statusName (watchdogAction.failureKind),
                     watchdogAction.bypassRequested ? 1 : 0,
                     watchdogAction.recompileRequested ? 1 : 0,
                     statusName (autoQueuedWatchdogAction.action),
                     statusName (autoQueuedWatchdogAction.failureKind));
        return 2;
    }

    const auto queuedWatchdogAction = watchdogCoordinator.queueFailureActionRequestForCurrentFailure();
    const auto pendingWatchdogAction = watchdogCoordinator.pendingFailureActionRequest();
    const auto drainedWatchdogAction = watchdogCoordinator.drainPendingFailureActionRequest();
    const auto afterWatchdogDrainAction = watchdogCoordinator.pendingFailureActionRequest();
    if (! requestMatches (queuedWatchdogAction, watchdogAction)
        || ! requestMatches (pendingWatchdogAction, watchdogAction)
        || ! requestMatches (drainedWatchdogAction, watchdogAction)
        || afterWatchdogDrainAction.action != yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::none
        || afterWatchdogDrainAction.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none
        || afterWatchdogDrainAction.bypassRequested
        || afterWatchdogDrainAction.recompileRequested)
    {
        std::printf ("FAIL: plugin host coordinator watchdog pending action queue/drain is wrong: queued=%s/%s pending=%s/%s drained=%s/%s afterDrain=%s/%s\n",
                     statusName (queuedWatchdogAction.action),
                     statusName (queuedWatchdogAction.failureKind),
                     statusName (pendingWatchdogAction.action),
                     statusName (pendingWatchdogAction.failureKind),
                     statusName (drainedWatchdogAction.action),
                     statusName (drainedWatchdogAction.failureKind),
                     statusName (afterWatchdogDrainAction.action),
                     statusName (afterWatchdogDrainAction.failureKind));
        return 2;
    }

    const auto queuedWatchdogCommandAction = watchdogCoordinator.queueFailureActionRequestForCurrentFailure();
    const auto watchdogCommandResult = watchdogCoordinator.drainPendingFailureActionRequestToControlCommand();
    const auto afterWatchdogCommandDrainAction = watchdogCoordinator.pendingFailureActionRequest();
    if (! requestMatches (queuedWatchdogCommandAction, watchdogAction)
        || watchdogCommandResult.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::commandReady
        || ! requestMatches (watchdogCommandResult.drainedRequest, watchdogAction)
        || ! commandMatches (watchdogCommandResult.command,
                             { yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandKind::bypassAndRecompile,
                               yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout,
                               true,
                               true })
        || ! watchdogCommandResult.pendingRequestConsumed
        || watchdogCommandResult.graphRecompileExecuted
        || afterWatchdogCommandDrainAction.action != yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::none
        || afterWatchdogCommandDrainAction.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none
        || afterWatchdogCommandDrainAction.bypassRequested
        || afterWatchdogCommandDrainAction.recompileRequested)
    {
        std::printf ("FAIL: plugin host coordinator watchdog control command shell is wrong: queued=%s/%s status=%s drained=%s/%s command=%s/%s consumed=%d executed=%d afterDrain=%s/%s\n",
                     statusName (queuedWatchdogCommandAction.action),
                     statusName (queuedWatchdogCommandAction.failureKind),
                     statusName (watchdogCommandResult.status),
                     statusName (watchdogCommandResult.drainedRequest.action),
                     statusName (watchdogCommandResult.drainedRequest.failureKind),
                     statusName (watchdogCommandResult.command.command),
                     statusName (watchdogCommandResult.command.failureKind),
                     watchdogCommandResult.pendingRequestConsumed ? 1 : 0,
                     watchdogCommandResult.graphRecompileExecuted ? 1 : 0,
                     statusName (afterWatchdogCommandDrainAction.action),
                     statusName (afterWatchdogCommandDrainAction.failureKind));
        return 2;
    }

    const auto watchdogDeferredStatus =
        deferredReceiptCoordinator.recordDeferredGraphChangeCommandResult (watchdogCommandResult);
    const auto watchdogDeferredStatusInspection = deferredReceiptCoordinator.deferredGraphChangeCommandStatus();
    if (watchdogDeferredStatus.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::commandReady
        || ! commandResultMatches (watchdogDeferredStatus.lastResult, watchdogCommandResult)
        || ! watchdogDeferredStatus.commandRecorded
        || watchdogDeferredStatus.graphRecompileExecuted
        || ! commandResultMatches (watchdogDeferredStatusInspection.lastResult, watchdogCommandResult)
        || ! watchdogDeferredStatusInspection.commandRecorded
        || watchdogDeferredStatusInspection.graphRecompileExecuted)
    {
        std::printf ("FAIL: plugin host coordinator watchdog deferred command receipt/status is wrong: status=%s inspected=%s recorded=%d inspectedRecorded=%d executed=%d inspectedExecuted=%d\n",
                     statusName (watchdogDeferredStatus.status),
                     statusName (watchdogDeferredStatusInspection.status),
                     watchdogDeferredStatus.commandRecorded ? 1 : 0,
                     watchdogDeferredStatusInspection.commandRecorded ? 1 : 0,
                     watchdogDeferredStatus.graphRecompileExecuted ? 1 : 0,
                     watchdogDeferredStatusInspection.graphRecompileExecuted ? 1 : 0);
        return 2;
    }

    // Short heartbeat so a real child-side crash (the child terminates itself) is detected promptly.
    yesdaw::plugin_host::PluginHostCoordinator crashCoordinator { std::chrono::milliseconds (1500) };
    const auto crash = crashCoordinator.launchAndExpectCrash (workerExecutable);

    if (crash.status != yesdaw::plugin_host::PluginHostCoordinator::CrashStatus::connectionLost
        || ! crash.readySeen
        || ! crash.crashObservationRequested
        || ! crash.connectionLostSeen
        || crash.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash)
    {
        std::printf ("FAIL: plugin host coordinator crash observation failed: status=%s ready=%d observation=%d connectionLost=%d failure=%s\n",
                     statusName (crash.status),
                     crash.readySeen ? 1 : 0,
                     crash.crashObservationRequested ? 1 : 0,
                     crash.connectionLostSeen ? 1 : 0,
                     statusName (crash.failureKind));
        return 2;
    }

    const auto crashStatus = crashCoordinator.status();

    if (crashStatus.state != yesdaw::plugin_host::PluginHostCoordinator::ChildState::lost
        || crashStatus.handshakeStatus != yesdaw::plugin_host::PluginHostCoordinator::HandshakeStatus::notStarted
        || crashStatus.stopStatus != yesdaw::plugin_host::PluginHostCoordinator::StopStatus::notStarted
        || crashStatus.watchdogStatus != yesdaw::plugin_host::PluginHostCoordinator::WatchdogStatus::notStarted
        || crashStatus.crashStatus != yesdaw::plugin_host::PluginHostCoordinator::CrashStatus::connectionLost
        || crashStatus.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash
        || ! crashStatus.launchAttempted
        || ! crashStatus.readySeen
        || crashStatus.probeEchoed
        || crashStatus.stopRequested
        || crashStatus.watchdogTimedOut
        || crashStatus.watchdogKillRequested
        || ! crashStatus.crashObservationRequested
        || ! crashStatus.connectionLostSeen)
    {
        std::printf ("FAIL: plugin host coordinator crash status is wrong: state=%s handshake=%s stop=%s watchdog=%s crash=%s failure=%s launch=%d ready=%d echo=%d stopRequested=%d watchdogTimeout=%d watchdogKill=%d crashObservation=%d connectionLost=%d\n",
                     statusName (crashStatus.state),
                     statusName (crashStatus.handshakeStatus),
                     statusName (crashStatus.stopStatus),
                     statusName (crashStatus.watchdogStatus),
                     statusName (crashStatus.crashStatus),
                     statusName (crashStatus.failureKind),
                     crashStatus.launchAttempted ? 1 : 0,
                     crashStatus.readySeen ? 1 : 0,
                     crashStatus.probeEchoed ? 1 : 0,
                     crashStatus.stopRequested ? 1 : 0,
                     crashStatus.watchdogTimedOut ? 1 : 0,
                     crashStatus.watchdogKillRequested ? 1 : 0,
                     crashStatus.crashObservationRequested ? 1 : 0,
                     crashStatus.connectionLostSeen ? 1 : 0);
        return 2;
    }

    const auto crashFailure = crashCoordinator.hostFailureReport();
    if (crashFailure.kind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash
        || ! crashFailure.connectionLostSeen
        || crashFailure.watchdogTimedOut
        || crashFailure.watchdogKillRequested)
    {
        std::printf ("FAIL: plugin host coordinator crash failure report is wrong: failure=%s connectionLost=%d watchdogTimeout=%d watchdogKill=%d\n",
                     statusName (crashFailure.kind),
                     crashFailure.connectionLostSeen ? 1 : 0,
                     crashFailure.watchdogTimedOut ? 1 : 0,
                     crashFailure.watchdogKillRequested ? 1 : 0);
        return 2;
    }

    const auto crashBlacklistCandidate = crashCoordinator.blacklistCandidateStatus();
    if (! blacklistCandidateMatches (
            crashBlacklistCandidate,
            { yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash, true, true, false })
        || crashBlacklistCandidate.failureKind == watchdogBlacklistCandidate.failureKind)
    {
        std::printf ("FAIL: plugin host coordinator crash blacklist-candidate status is wrong: failure=%s candidate=%d crash=%d watchdog=%d watchdogFailure=%s\n",
                     statusName (crashBlacklistCandidate.failureKind),
                     crashBlacklistCandidate.candidate ? 1 : 0,
                     crashBlacklistCandidate.crashCandidate ? 1 : 0,
                     crashBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (watchdogBlacklistCandidate.failureKind));
        return 2;
    }

    const auto queuedCrashBlacklistCandidate = crashCoordinator.queueBlacklistCandidateForCurrentFailure();
    const auto pendingCrashBlacklistCandidate = crashCoordinator.pendingBlacklistCandidateStatus();
    const auto drainedCrashBlacklistCandidate = crashCoordinator.drainPendingBlacklistCandidateStatus();
    const auto afterCrashBlacklistCandidateDrain = crashCoordinator.pendingBlacklistCandidateStatus();
    const auto queuedCrashBlacklistEscalationCandidate =
        crashCoordinator.queueBlacklistCandidateForCurrentFailure();
    const auto crashBlacklistEscalationResult =
        crashCoordinator.drainPendingBlacklistCandidateToControlEscalation();
    const auto afterCrashBlacklistEscalationDrain = crashCoordinator.pendingBlacklistCandidateStatus();
    if (! blacklistCandidateMatches (queuedCrashBlacklistCandidate, crashBlacklistCandidate)
        || ! blacklistCandidateMatches (pendingCrashBlacklistCandidate, crashBlacklistCandidate)
        || ! blacklistCandidateMatches (drainedCrashBlacklistCandidate, crashBlacklistCandidate)
        || ! blacklistCandidateMatches (afterCrashBlacklistCandidateDrain, {})
        || drainedCrashBlacklistCandidate.failureKind == drainedWatchdogBlacklistCandidate.failureKind
        || ! blacklistCandidateMatches (queuedCrashBlacklistEscalationCandidate, crashBlacklistCandidate)
        || ! blacklistEscalationResultMatches (
            crashBlacklistEscalationResult,
            { yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalationStatus::escalationReady,
              crashBlacklistCandidate,
              { yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash, true, false, true },
              true,
              false,
              false })
        || crashBlacklistEscalationResult.escalation.failureKind
            == watchdogBlacklistEscalationResult.escalation.failureKind
        || ! blacklistCandidateMatches (afterCrashBlacklistEscalationDrain, {}))
    {
        std::printf ("FAIL: plugin host coordinator crash pending blacklist candidate escalation is wrong: queued=%s/%d/%d/%d pending=%s/%d/%d/%d drained=%s/%d/%d/%d after=%s/%d/%d/%d watchdog=%s escalationQueued=%s/%d/%d/%d escalation=%s drained=%s/%d/%d/%d escalated=%s/%d/%d/%d watchdogEscalation=%s consumed=%d policy=%d persisted=%d afterEscalation=%s/%d/%d/%d\n",
                     statusName (queuedCrashBlacklistCandidate.failureKind),
                     queuedCrashBlacklistCandidate.candidate ? 1 : 0,
                     queuedCrashBlacklistCandidate.crashCandidate ? 1 : 0,
                     queuedCrashBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (pendingCrashBlacklistCandidate.failureKind),
                     pendingCrashBlacklistCandidate.candidate ? 1 : 0,
                     pendingCrashBlacklistCandidate.crashCandidate ? 1 : 0,
                     pendingCrashBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (drainedCrashBlacklistCandidate.failureKind),
                     drainedCrashBlacklistCandidate.candidate ? 1 : 0,
                     drainedCrashBlacklistCandidate.crashCandidate ? 1 : 0,
                     drainedCrashBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (afterCrashBlacklistCandidateDrain.failureKind),
                     afterCrashBlacklistCandidateDrain.candidate ? 1 : 0,
                     afterCrashBlacklistCandidateDrain.crashCandidate ? 1 : 0,
                     afterCrashBlacklistCandidateDrain.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (drainedWatchdogBlacklistCandidate.failureKind),
                     statusName (queuedCrashBlacklistEscalationCandidate.failureKind),
                     queuedCrashBlacklistEscalationCandidate.candidate ? 1 : 0,
                     queuedCrashBlacklistEscalationCandidate.crashCandidate ? 1 : 0,
                     queuedCrashBlacklistEscalationCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (crashBlacklistEscalationResult.status),
                     statusName (crashBlacklistEscalationResult.drainedCandidate.failureKind),
                     crashBlacklistEscalationResult.drainedCandidate.candidate ? 1 : 0,
                     crashBlacklistEscalationResult.drainedCandidate.crashCandidate ? 1 : 0,
                     crashBlacklistEscalationResult.drainedCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (crashBlacklistEscalationResult.escalation.failureKind),
                     crashBlacklistEscalationResult.escalation.crashCandidate ? 1 : 0,
                     crashBlacklistEscalationResult.escalation.watchdogTimeoutCandidate ? 1 : 0,
                     crashBlacklistEscalationResult.escalation.controlThreadEscalationRequested ? 1 : 0,
                     statusName (watchdogBlacklistEscalationResult.escalation.failureKind),
                     crashBlacklistEscalationResult.pendingCandidateConsumed ? 1 : 0,
                     crashBlacklistEscalationResult.blacklistPolicyApplied ? 1 : 0,
                     crashBlacklistEscalationResult.blacklistStatePersisted ? 1 : 0,
                     statusName (afterCrashBlacklistEscalationDrain.failureKind),
                     afterCrashBlacklistEscalationDrain.candidate ? 1 : 0,
                     afterCrashBlacklistEscalationDrain.crashCandidate ? 1 : 0,
                     afterCrashBlacklistEscalationDrain.watchdogTimeoutCandidate ? 1 : 0);
        return 2;
    }

    const auto crashDeferredBlacklistEscalationStatus =
        blacklistReceiptCoordinator.recordDeferredBlacklistEscalationResult (crashBlacklistEscalationResult);
    const auto crashDeferredBlacklistEscalationInspection =
        blacklistReceiptCoordinator.deferredBlacklistEscalationStatus();
    if (crashDeferredBlacklistEscalationStatus.status
            != yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalationStatus::escalationReady
        || ! blacklistEscalationResultMatches (crashDeferredBlacklistEscalationStatus.lastResult,
                                               crashBlacklistEscalationResult)
        || ! crashDeferredBlacklistEscalationStatus.escalationRecorded
        || crashDeferredBlacklistEscalationStatus.blacklistPolicyApplied
        || crashDeferredBlacklistEscalationStatus.blacklistStatePersisted
        || ! blacklistEscalationResultMatches (crashDeferredBlacklistEscalationInspection.lastResult,
                                               crashBlacklistEscalationResult)
        || ! crashDeferredBlacklistEscalationInspection.escalationRecorded
        || crashDeferredBlacklistEscalationInspection.blacklistPolicyApplied
        || crashDeferredBlacklistEscalationInspection.blacklistStatePersisted
        || crashDeferredBlacklistEscalationInspection.lastResult.escalation.failureKind
            == watchdogDeferredBlacklistEscalationInspection.lastResult.escalation.failureKind)
    {
        std::printf ("FAIL: plugin host coordinator crash deferred blacklist escalation receipt/status is wrong: status=%s inspected=%s recorded=%d inspectedRecorded=%d policy=%d persisted=%d inspectedPolicy=%d inspectedPersisted=%d watchdogEscalation=%s crashEscalation=%s\n",
                     statusName (crashDeferredBlacklistEscalationStatus.status),
                     statusName (crashDeferredBlacklistEscalationInspection.status),
                     crashDeferredBlacklistEscalationStatus.escalationRecorded ? 1 : 0,
                     crashDeferredBlacklistEscalationInspection.escalationRecorded ? 1 : 0,
                     crashDeferredBlacklistEscalationStatus.blacklistPolicyApplied ? 1 : 0,
                     crashDeferredBlacklistEscalationStatus.blacklistStatePersisted ? 1 : 0,
                     crashDeferredBlacklistEscalationInspection.blacklistPolicyApplied ? 1 : 0,
                     crashDeferredBlacklistEscalationInspection.blacklistStatePersisted ? 1 : 0,
                     statusName (watchdogDeferredBlacklistEscalationInspection.lastResult.escalation.failureKind),
                     statusName (crashDeferredBlacklistEscalationInspection.lastResult.escalation.failureKind));
        return 2;
    }

    const auto crashBlacklistPolicyDecisionRequest =
        blacklistReceiptCoordinator.blacklistPolicyDecisionRequest();
    const auto queuedCrashBlacklistPolicyDecisionRequest =
        blacklistReceiptCoordinator.queueBlacklistPolicyDecisionRequestForDeferredEscalation();
    const auto pendingCrashBlacklistPolicyDecisionRequest =
        blacklistReceiptCoordinator.pendingBlacklistPolicyDecisionRequest();
    const auto drainedCrashBlacklistPolicyDecisionRequest =
        blacklistReceiptCoordinator.drainPendingBlacklistPolicyDecisionRequest();
    const auto afterCrashDrainBlacklistPolicyDecisionRequest =
        blacklistReceiptCoordinator.drainPendingBlacklistPolicyDecisionRequest();
    const auto commandQueuedCrashBlacklistPolicyDecisionRequest =
        blacklistReceiptCoordinator.queueBlacklistPolicyDecisionRequestForDeferredEscalation();
    const auto crashBlacklistPolicyDecisionCommandResult =
        blacklistReceiptCoordinator.drainPendingBlacklistPolicyDecisionRequestToControlCommand();
    const auto afterCrashCommandDrainBlacklistPolicyDecisionRequest =
        blacklistReceiptCoordinator.pendingBlacklistPolicyDecisionRequest();
    const auto afterCrashCommandSecondDrainBlacklistPolicyDecisionCommandResult =
        blacklistReceiptCoordinator.drainPendingBlacklistPolicyDecisionRequestToControlCommand();
    if (! blacklistPolicyDecisionRequestMatches (
            crashBlacklistPolicyDecisionRequest,
            { yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionRequestStatus::requestReady,
              yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash,
              true,
              false,
              true,
              false,
              false })
        || crashBlacklistPolicyDecisionRequest.failureKind == watchdogBlacklistPolicyDecisionRequest.failureKind)
    {
        std::printf ("FAIL: plugin host coordinator crash blacklist policy-decision request is wrong: request=%s/%s/%d/%d/%d policy=%d persisted=%d watchdogRequest=%s\n",
                     statusName (crashBlacklistPolicyDecisionRequest.status),
                     statusName (crashBlacklistPolicyDecisionRequest.failureKind),
                     crashBlacklistPolicyDecisionRequest.crashCandidate ? 1 : 0,
                     crashBlacklistPolicyDecisionRequest.watchdogTimeoutCandidate ? 1 : 0,
                     crashBlacklistPolicyDecisionRequest.controlThreadPolicyDecisionRequested ? 1 : 0,
                     crashBlacklistPolicyDecisionRequest.blacklistPolicyApplied ? 1 : 0,
                     crashBlacklistPolicyDecisionRequest.blacklistStatePersisted ? 1 : 0,
                     statusName (watchdogBlacklistPolicyDecisionRequest.failureKind));
        return 2;
    }

    if (! blacklistPolicyDecisionRequestMatches (queuedCrashBlacklistPolicyDecisionRequest,
                                                 crashBlacklistPolicyDecisionRequest)
        || ! blacklistPolicyDecisionRequestMatches (pendingCrashBlacklistPolicyDecisionRequest,
                                                    crashBlacklistPolicyDecisionRequest)
        || ! blacklistPolicyDecisionRequestMatches (drainedCrashBlacklistPolicyDecisionRequest,
                                                    crashBlacklistPolicyDecisionRequest)
        || ! blacklistPolicyDecisionRequestMatches (afterCrashDrainBlacklistPolicyDecisionRequest, {})
        || drainedCrashBlacklistPolicyDecisionRequest.failureKind
            == drainedWatchdogBlacklistPolicyDecisionRequest.failureKind)
    {
        std::printf ("FAIL: plugin host coordinator crash pending blacklist policy-decision request queue/drain is wrong: queued=%s/%s/%d/%d pending=%s/%s drained=%s/%s watchdogDrained=%s afterDrain=%s/%s policy=%d persisted=%d\n",
                     statusName (queuedCrashBlacklistPolicyDecisionRequest.status),
                     statusName (queuedCrashBlacklistPolicyDecisionRequest.failureKind),
                     queuedCrashBlacklistPolicyDecisionRequest.crashCandidate ? 1 : 0,
                     queuedCrashBlacklistPolicyDecisionRequest.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (pendingCrashBlacklistPolicyDecisionRequest.status),
                     statusName (pendingCrashBlacklistPolicyDecisionRequest.failureKind),
                     statusName (drainedCrashBlacklistPolicyDecisionRequest.status),
                     statusName (drainedCrashBlacklistPolicyDecisionRequest.failureKind),
                     statusName (drainedWatchdogBlacklistPolicyDecisionRequest.failureKind),
                     statusName (afterCrashDrainBlacklistPolicyDecisionRequest.status),
                     statusName (afterCrashDrainBlacklistPolicyDecisionRequest.failureKind),
                     drainedCrashBlacklistPolicyDecisionRequest.blacklistPolicyApplied ? 1 : 0,
                     drainedCrashBlacklistPolicyDecisionRequest.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    if (! blacklistPolicyDecisionRequestMatches (commandQueuedCrashBlacklistPolicyDecisionRequest,
                                                 crashBlacklistPolicyDecisionRequest)
        || crashBlacklistPolicyDecisionCommandResult.status
            != yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionCommandStatus::commandReady
        || ! blacklistPolicyDecisionRequestMatches (crashBlacklistPolicyDecisionCommandResult.drainedRequest,
                                                    crashBlacklistPolicyDecisionRequest)
        || ! blacklistPolicyDecisionCommandMatches (
            crashBlacklistPolicyDecisionCommandResult.command,
            { yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionCommandKind::requestPolicyDecision,
              yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash,
              true,
              false,
              true })
        || ! crashBlacklistPolicyDecisionCommandResult.pendingRequestConsumed
        || crashBlacklistPolicyDecisionCommandResult.blacklistPolicyApplied
        || crashBlacklistPolicyDecisionCommandResult.blacklistStatePersisted
        || ! blacklistPolicyDecisionRequestMatches (afterCrashCommandDrainBlacklistPolicyDecisionRequest, {})
        || ! blacklistPolicyDecisionCommandResultMatches (
            afterCrashCommandSecondDrainBlacklistPolicyDecisionCommandResult, {})
        || crashBlacklistPolicyDecisionCommandResult.command.failureKind
            == watchdogBlacklistPolicyDecisionCommandResult.command.failureKind)
    {
        std::printf ("FAIL: plugin host coordinator crash blacklist policy-decision control command is wrong: queued=%s/%s/%d/%d status=%s drained=%s/%s/%d/%d command=%s/%s/%d/%d/%d watchdogCommand=%s consumed=%d afterPending=%s/%s afterSecond=%s/%s policy=%d persisted=%d\n",
                     statusName (commandQueuedCrashBlacklistPolicyDecisionRequest.status),
                     statusName (commandQueuedCrashBlacklistPolicyDecisionRequest.failureKind),
                     commandQueuedCrashBlacklistPolicyDecisionRequest.crashCandidate ? 1 : 0,
                     commandQueuedCrashBlacklistPolicyDecisionRequest.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (crashBlacklistPolicyDecisionCommandResult.status),
                     statusName (crashBlacklistPolicyDecisionCommandResult.drainedRequest.status),
                     statusName (crashBlacklistPolicyDecisionCommandResult.drainedRequest.failureKind),
                     crashBlacklistPolicyDecisionCommandResult.drainedRequest.crashCandidate ? 1 : 0,
                     crashBlacklistPolicyDecisionCommandResult.drainedRequest.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (crashBlacklistPolicyDecisionCommandResult.command.command),
                     statusName (crashBlacklistPolicyDecisionCommandResult.command.failureKind),
                     crashBlacklistPolicyDecisionCommandResult.command.crashCandidate ? 1 : 0,
                     crashBlacklistPolicyDecisionCommandResult.command.watchdogTimeoutCandidate ? 1 : 0,
                     crashBlacklistPolicyDecisionCommandResult.command.controlThreadPolicyDecisionRequested ? 1 : 0,
                     statusName (watchdogBlacklistPolicyDecisionCommandResult.command.failureKind),
                     crashBlacklistPolicyDecisionCommandResult.pendingRequestConsumed ? 1 : 0,
                     statusName (afterCrashCommandDrainBlacklistPolicyDecisionRequest.status),
                     statusName (afterCrashCommandDrainBlacklistPolicyDecisionRequest.failureKind),
                     statusName (afterCrashCommandSecondDrainBlacklistPolicyDecisionCommandResult.status),
                     statusName (afterCrashCommandSecondDrainBlacklistPolicyDecisionCommandResult.command.failureKind),
                     crashBlacklistPolicyDecisionCommandResult.blacklistPolicyApplied ? 1 : 0,
                     crashBlacklistPolicyDecisionCommandResult.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    const auto crashDeferredBlacklistPolicyDecisionCommandStatus =
        blacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistPolicyDecisionCommandResult (
            crashBlacklistPolicyDecisionCommandResult);
    const auto crashDeferredBlacklistPolicyDecisionCommandInspection =
        blacklistPolicyCommandReceiptCoordinator.deferredBlacklistPolicyDecisionCommandStatus();
    const auto crashBlacklistPolicyDecisionOutcome =
        blacklistPolicyCommandReceiptCoordinator.blacklistPolicyDecisionOutcomeStatus();
    const auto queuedCrashBlacklistPolicyDecisionOutcome =
        blacklistPolicyCommandReceiptCoordinator.queueBlacklistPolicyDecisionOutcomeForDeferredCommand();
    const auto pendingCrashBlacklistPolicyDecisionOutcome =
        blacklistPolicyCommandReceiptCoordinator.pendingBlacklistPolicyDecisionOutcomeStatus();
    const auto drainedCrashBlacklistPolicyDecisionOutcome =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistPolicyDecisionOutcomeStatus();
    const auto afterCrashDrainBlacklistPolicyDecisionOutcome =
        blacklistPolicyCommandReceiptCoordinator.pendingBlacklistPolicyDecisionOutcomeStatus();
    if (crashDeferredBlacklistPolicyDecisionCommandStatus.status
            != yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionCommandStatus::commandReady
        || ! blacklistPolicyDecisionCommandResultMatches (
            crashDeferredBlacklistPolicyDecisionCommandStatus.lastResult,
            crashBlacklistPolicyDecisionCommandResult)
        || ! crashDeferredBlacklistPolicyDecisionCommandStatus.commandRecorded
        || crashDeferredBlacklistPolicyDecisionCommandStatus.blacklistPolicyApplied
        || crashDeferredBlacklistPolicyDecisionCommandStatus.blacklistStatePersisted
        || ! blacklistPolicyDecisionCommandResultMatches (
            crashDeferredBlacklistPolicyDecisionCommandInspection.lastResult,
            crashBlacklistPolicyDecisionCommandResult)
        || ! crashDeferredBlacklistPolicyDecisionCommandInspection.commandRecorded
        || crashDeferredBlacklistPolicyDecisionCommandInspection.blacklistPolicyApplied
        || crashDeferredBlacklistPolicyDecisionCommandInspection.blacklistStatePersisted
        || crashDeferredBlacklistPolicyDecisionCommandInspection.lastResult.command.failureKind
            == watchdogDeferredBlacklistPolicyDecisionCommandInspection.lastResult.command.failureKind
        || ! blacklistPolicyDecisionOutcomeMatches (
            crashBlacklistPolicyDecisionOutcome,
            { yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionOutcomeStatus::outcomeReady,
              yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash,
              true,
              false,
              true,
              false,
              false })
        || crashBlacklistPolicyDecisionOutcome.failureKind == watchdogBlacklistPolicyDecisionOutcome.failureKind
        || ! blacklistPolicyDecisionOutcomeMatches (
            queuedCrashBlacklistPolicyDecisionOutcome, crashBlacklistPolicyDecisionOutcome)
        || ! blacklistPolicyDecisionOutcomeMatches (
            pendingCrashBlacklistPolicyDecisionOutcome, crashBlacklistPolicyDecisionOutcome)
        || ! blacklistPolicyDecisionOutcomeMatches (
            drainedCrashBlacklistPolicyDecisionOutcome, crashBlacklistPolicyDecisionOutcome)
        || ! blacklistPolicyDecisionOutcomeMatches (afterCrashDrainBlacklistPolicyDecisionOutcome, {})
        || drainedCrashBlacklistPolicyDecisionOutcome.failureKind
            == drainedWatchdogBlacklistPolicyDecisionOutcome.failureKind)
    {
        std::printf ("FAIL: plugin host coordinator crash deferred blacklist policy-decision command receipt/status/outcome queue is wrong: status=%s inspected=%s recorded=%d inspectedRecorded=%d policy=%d persisted=%d inspectedPolicy=%d inspectedPersisted=%d watchdogCommand=%s crashCommand=%s outcome=%s/%s/%d/%d/%d watchdogOutcome=%s queued=%s/%s pending=%s/%s drained=%s/%s watchdogDrained=%s afterDrain=%s/%s outcomePolicy=%d outcomePersisted=%d\n",
                     statusName (crashDeferredBlacklistPolicyDecisionCommandStatus.status),
                     statusName (crashDeferredBlacklistPolicyDecisionCommandInspection.status),
                     crashDeferredBlacklistPolicyDecisionCommandStatus.commandRecorded ? 1 : 0,
                     crashDeferredBlacklistPolicyDecisionCommandInspection.commandRecorded ? 1 : 0,
                     crashDeferredBlacklistPolicyDecisionCommandStatus.blacklistPolicyApplied ? 1 : 0,
                     crashDeferredBlacklistPolicyDecisionCommandStatus.blacklistStatePersisted ? 1 : 0,
                     crashDeferredBlacklistPolicyDecisionCommandInspection.blacklistPolicyApplied ? 1 : 0,
                     crashDeferredBlacklistPolicyDecisionCommandInspection.blacklistStatePersisted ? 1 : 0,
                     statusName (watchdogDeferredBlacklistPolicyDecisionCommandInspection.lastResult.command.failureKind),
                     statusName (crashDeferredBlacklistPolicyDecisionCommandInspection.lastResult.command.failureKind),
                     statusName (crashBlacklistPolicyDecisionOutcome.status),
                     statusName (crashBlacklistPolicyDecisionOutcome.failureKind),
                     crashBlacklistPolicyDecisionOutcome.crashCandidate ? 1 : 0,
                     crashBlacklistPolicyDecisionOutcome.watchdogTimeoutCandidate ? 1 : 0,
                     crashBlacklistPolicyDecisionOutcome.controlThreadPolicyDecisionInspected ? 1 : 0,
                     statusName (watchdogBlacklistPolicyDecisionOutcome.failureKind),
                     statusName (queuedCrashBlacklistPolicyDecisionOutcome.status),
                     statusName (queuedCrashBlacklistPolicyDecisionOutcome.failureKind),
                     statusName (pendingCrashBlacklistPolicyDecisionOutcome.status),
                     statusName (pendingCrashBlacklistPolicyDecisionOutcome.failureKind),
                     statusName (drainedCrashBlacklistPolicyDecisionOutcome.status),
                     statusName (drainedCrashBlacklistPolicyDecisionOutcome.failureKind),
                     statusName (drainedWatchdogBlacklistPolicyDecisionOutcome.failureKind),
                     statusName (afterCrashDrainBlacklistPolicyDecisionOutcome.status),
                     statusName (afterCrashDrainBlacklistPolicyDecisionOutcome.failureKind),
                     crashBlacklistPolicyDecisionOutcome.blacklistPolicyApplied ? 1 : 0,
                     crashBlacklistPolicyDecisionOutcome.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    const auto alreadyDrainedCrashBlacklistPolicyDecisionOutcomeHandlingResult =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistPolicyDecisionOutcomeToControlHandling();
    const auto requeuedCrashBlacklistPolicyDecisionOutcome =
        blacklistPolicyCommandReceiptCoordinator.queueBlacklistPolicyDecisionOutcomeForDeferredCommand();
    const auto crashBlacklistPolicyDecisionOutcomeHandlingResult =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistPolicyDecisionOutcomeToControlHandling();
    const auto afterCrashHandlingBlacklistPolicyDecisionOutcome =
        blacklistPolicyCommandReceiptCoordinator.pendingBlacklistPolicyDecisionOutcomeStatus();
    const auto afterCrashHandlingSecondResult =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistPolicyDecisionOutcomeToControlHandling();
    if (! blacklistPolicyDecisionOutcomeHandlingResultMatches (
            alreadyDrainedCrashBlacklistPolicyDecisionOutcomeHandlingResult, {})
        || ! blacklistPolicyDecisionOutcomeMatches (
            requeuedCrashBlacklistPolicyDecisionOutcome, crashBlacklistPolicyDecisionOutcome)
        || ! blacklistPolicyDecisionOutcomeHandlingResultMatches (
            crashBlacklistPolicyDecisionOutcomeHandlingResult,
            { yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionOutcomeHandlingStatus::handlingReady,
              crashBlacklistPolicyDecisionOutcome,
              { yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash,
                true,
                false,
                true },
              true,
              false,
              false })
        || ! blacklistPolicyDecisionOutcomeMatches (afterCrashHandlingBlacklistPolicyDecisionOutcome, {})
        || ! blacklistPolicyDecisionOutcomeHandlingResultMatches (afterCrashHandlingSecondResult, {})
        || crashBlacklistPolicyDecisionOutcomeHandlingResult.drainedOutcome.failureKind
            == watchdogBlacklistPolicyDecisionOutcomeHandlingResult.drainedOutcome.failureKind)
    {
        std::printf ("FAIL: plugin host coordinator crash blacklist policy-decision outcome handling shell is wrong: alreadyDrained=%s/%d requeued=%s/%s handling=%s drained=%s/%s/%d/%d/%d watchdogDrained=%s handled=%s/%d/%d/%d consumed=%d after=%s/%s second=%s/%d policy=%d persisted=%d\n",
                     statusName (alreadyDrainedCrashBlacklistPolicyDecisionOutcomeHandlingResult.status),
                     alreadyDrainedCrashBlacklistPolicyDecisionOutcomeHandlingResult.pendingOutcomeConsumed ? 1 : 0,
                     statusName (requeuedCrashBlacklistPolicyDecisionOutcome.status),
                     statusName (requeuedCrashBlacklistPolicyDecisionOutcome.failureKind),
                     statusName (crashBlacklistPolicyDecisionOutcomeHandlingResult.status),
                     statusName (crashBlacklistPolicyDecisionOutcomeHandlingResult.drainedOutcome.status),
                     statusName (crashBlacklistPolicyDecisionOutcomeHandlingResult.drainedOutcome.failureKind),
                     crashBlacklistPolicyDecisionOutcomeHandlingResult.drainedOutcome.crashCandidate ? 1 : 0,
                     crashBlacklistPolicyDecisionOutcomeHandlingResult.drainedOutcome.watchdogTimeoutCandidate ? 1 : 0,
                     crashBlacklistPolicyDecisionOutcomeHandlingResult.drainedOutcome.controlThreadPolicyDecisionInspected ? 1 : 0,
                     statusName (watchdogBlacklistPolicyDecisionOutcomeHandlingResult.drainedOutcome.failureKind),
                     statusName (crashBlacklistPolicyDecisionOutcomeHandlingResult.handling.failureKind),
                     crashBlacklistPolicyDecisionOutcomeHandlingResult.handling.crashCandidate ? 1 : 0,
                     crashBlacklistPolicyDecisionOutcomeHandlingResult.handling.watchdogTimeoutCandidate ? 1 : 0,
                     crashBlacklistPolicyDecisionOutcomeHandlingResult.handling.controlThreadBlacklistHandlingRequested ? 1 : 0,
                     crashBlacklistPolicyDecisionOutcomeHandlingResult.pendingOutcomeConsumed ? 1 : 0,
                     statusName (afterCrashHandlingBlacklistPolicyDecisionOutcome.status),
                     statusName (afterCrashHandlingBlacklistPolicyDecisionOutcome.failureKind),
                     statusName (afterCrashHandlingSecondResult.status),
                     afterCrashHandlingSecondResult.pendingOutcomeConsumed ? 1 : 0,
                     crashBlacklistPolicyDecisionOutcomeHandlingResult.blacklistPolicyApplied ? 1 : 0,
                     crashBlacklistPolicyDecisionOutcomeHandlingResult.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    const auto crashDeferredBlacklistPolicyDecisionOutcomeHandlingStatus =
        blacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistPolicyDecisionOutcomeHandlingResult (
            crashBlacklistPolicyDecisionOutcomeHandlingResult);
    const auto crashDeferredBlacklistPolicyDecisionOutcomeHandlingInspection =
        blacklistPolicyCommandReceiptCoordinator.deferredBlacklistPolicyDecisionOutcomeHandlingStatus();
    const auto crashBlacklistHandlingRequest =
        blacklistPolicyCommandReceiptCoordinator.blacklistHandlingRequest();
    const auto queuedCrashBlacklistHandlingRequest =
        blacklistPolicyCommandReceiptCoordinator.queueBlacklistHandlingRequestForDeferredOutcomeHandling();
    const auto pendingCrashBlacklistHandlingRequest =
        blacklistPolicyCommandReceiptCoordinator.pendingBlacklistHandlingRequest();
    const auto drainedCrashBlacklistHandlingRequest =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistHandlingRequest();
    const auto afterDrainedCrashBlacklistHandlingRequest =
        blacklistPolicyCommandReceiptCoordinator.pendingBlacklistHandlingRequest();
    const auto afterSecondDrainedCrashBlacklistHandlingRequest =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistHandlingRequest();
    const auto commandQueuedCrashBlacklistHandlingRequest =
        blacklistPolicyCommandReceiptCoordinator.queueBlacklistHandlingRequestForDeferredOutcomeHandling();
    const auto crashBlacklistHandlingCommandResult =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistHandlingRequestToControlCommand();
    const auto afterCrashCommandDrainBlacklistHandlingRequest =
        blacklistPolicyCommandReceiptCoordinator.pendingBlacklistHandlingRequest();
    const auto afterCrashCommandSecondDrainBlacklistHandlingCommandResult =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistHandlingRequestToControlCommand();
    const auto crashDeferredBlacklistHandlingCommandStatus =
        blacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistHandlingCommandResult (
            crashBlacklistHandlingCommandResult);
    const auto crashDeferredBlacklistHandlingCommandInspection =
        blacklistPolicyCommandReceiptCoordinator.deferredBlacklistHandlingCommandStatus();
    const auto crashBlacklistHandlingOutcome =
        blacklistPolicyCommandReceiptCoordinator.blacklistHandlingOutcomeStatus();
    const auto queuedCrashBlacklistHandlingOutcome =
        blacklistPolicyCommandReceiptCoordinator.queueBlacklistHandlingOutcomeForDeferredCommand();
    const auto pendingCrashBlacklistHandlingOutcome =
        blacklistPolicyCommandReceiptCoordinator.pendingBlacklistHandlingOutcomeStatus();
    const auto drainedCrashBlacklistHandlingOutcome =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistHandlingOutcomeStatus();
    const auto afterDrainedCrashBlacklistHandlingOutcome =
        blacklistPolicyCommandReceiptCoordinator.pendingBlacklistHandlingOutcomeStatus();
    const auto afterSecondDrainedCrashBlacklistHandlingOutcome =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistHandlingOutcomeStatus();
    const auto handlingQueuedCrashBlacklistHandlingOutcome =
        blacklistPolicyCommandReceiptCoordinator.queueBlacklistHandlingOutcomeForDeferredCommand();
    const auto crashBlacklistHandlingOutcomeHandlingResult =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistHandlingOutcomeToControlHandling();
    const auto afterCrashHandlingDrainBlacklistHandlingOutcome =
        blacklistPolicyCommandReceiptCoordinator.pendingBlacklistHandlingOutcomeStatus();
    const auto afterCrashHandlingSecondBlacklistHandlingOutcomeHandlingResult =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistHandlingOutcomeToControlHandling();
    const auto afterCrashHandlingDeferredBlacklistHandlingCommandStatus =
        blacklistPolicyCommandReceiptCoordinator.deferredBlacklistHandlingCommandStatus();
    const auto crashDeferredBlacklistHandlingOutcomeHandlingStatus =
        blacklistPolicyCommandReceiptCoordinator.recordDeferredBlacklistHandlingOutcomeHandlingResult (
            crashBlacklistHandlingOutcomeHandlingResult);
    const auto crashDeferredBlacklistHandlingOutcomeHandlingInspection =
        blacklistPolicyCommandReceiptCoordinator.deferredBlacklistHandlingOutcomeHandlingStatus();
    const auto afterCrashHandlingReceiptDeferredBlacklistHandlingCommandStatus =
        blacklistPolicyCommandReceiptCoordinator.deferredBlacklistHandlingCommandStatus();
    const auto acknowledgedCrashDeferredBlacklistHandlingOutcomeHandlingStatus =
        blacklistPolicyCommandReceiptCoordinator.acknowledgeDeferredBlacklistHandlingOutcomeHandlingStatus();
    const auto afterAcknowledgedCrashDeferredBlacklistHandlingOutcomeHandlingStatus =
        blacklistPolicyCommandReceiptCoordinator.deferredBlacklistHandlingOutcomeHandlingStatus();
    const auto afterAcknowledgedCrashOutcomeHandlingDeferredBlacklistHandlingCommandStatus =
        blacklistPolicyCommandReceiptCoordinator.deferredBlacklistHandlingCommandStatus();
    const auto acknowledgedCrashDeferredBlacklistHandlingCommandStatus =
        blacklistPolicyCommandReceiptCoordinator.acknowledgeDeferredBlacklistHandlingCommandStatus();
    const auto afterAcknowledgedCrashDeferredBlacklistHandlingCommandStatus =
        blacklistPolicyCommandReceiptCoordinator.deferredBlacklistHandlingCommandStatus();
    const auto afterAcknowledgedCrashBlacklistHandlingOutcome =
        blacklistPolicyCommandReceiptCoordinator.blacklistHandlingOutcomeStatus();
    const auto afterAcknowledgedQueuedCrashBlacklistHandlingOutcome =
        blacklistPolicyCommandReceiptCoordinator.queueBlacklistHandlingOutcomeForDeferredCommand();
    if (crashDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.status
            != yesdaw::plugin_host::PluginHostCoordinator::BlacklistPolicyDecisionOutcomeHandlingStatus::handlingReady
        || ! blacklistPolicyDecisionOutcomeHandlingResultMatches (
            crashDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.lastResult,
            crashBlacklistPolicyDecisionOutcomeHandlingResult)
        || ! crashDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.handlingRecorded
        || crashDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.blacklistPolicyApplied
        || crashDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.blacklistStatePersisted
        || ! blacklistPolicyDecisionOutcomeHandlingResultMatches (
            crashDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.lastResult,
            crashBlacklistPolicyDecisionOutcomeHandlingResult)
        || ! crashDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.handlingRecorded
        || crashDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.blacklistPolicyApplied
        || crashDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.blacklistStatePersisted
        || crashDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.lastResult.handling.failureKind
            == watchdogDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.lastResult.handling.failureKind
        || ! blacklistHandlingRequestMatches (
            crashBlacklistHandlingRequest,
            { yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingRequestStatus::requestReady,
              yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash,
              true,
              false,
              true,
              false,
              false })
        || crashBlacklistHandlingRequest.failureKind == watchdogBlacklistHandlingRequest.failureKind)
    {
        std::printf ("FAIL: plugin host coordinator crash deferred blacklist policy-decision outcome handling receipt/status/request is wrong: status=%s inspected=%s recorded=%d inspectedRecorded=%d policy=%d persisted=%d inspectedPolicy=%d inspectedPersisted=%d watchdogHandling=%s crashHandling=%s request=%s/%s/%d/%d/%d watchdogRequest=%s requestPolicy=%d requestPersisted=%d\n",
                     statusName (crashDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.status),
                     statusName (crashDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.status),
                     crashDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.handlingRecorded ? 1 : 0,
                     crashDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.handlingRecorded ? 1 : 0,
                     crashDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.blacklistPolicyApplied ? 1 : 0,
                     crashDeferredBlacklistPolicyDecisionOutcomeHandlingStatus.blacklistStatePersisted ? 1 : 0,
                     crashDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.blacklistPolicyApplied ? 1 : 0,
                     crashDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.blacklistStatePersisted ? 1 : 0,
                     statusName (watchdogDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.lastResult.handling.failureKind),
                     statusName (crashDeferredBlacklistPolicyDecisionOutcomeHandlingInspection.lastResult.handling.failureKind),
                     statusName (crashBlacklistHandlingRequest.status),
                     statusName (crashBlacklistHandlingRequest.failureKind),
                     crashBlacklistHandlingRequest.crashCandidate ? 1 : 0,
                     crashBlacklistHandlingRequest.watchdogTimeoutCandidate ? 1 : 0,
                     crashBlacklistHandlingRequest.controlThreadBlacklistHandlingRequested ? 1 : 0,
                     statusName (watchdogBlacklistHandlingRequest.failureKind),
                     crashBlacklistHandlingRequest.blacklistPolicyApplied ? 1 : 0,
                     crashBlacklistHandlingRequest.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    if (! blacklistHandlingRequestMatches (queuedCrashBlacklistHandlingRequest, crashBlacklistHandlingRequest)
        || ! blacklistHandlingRequestMatches (pendingCrashBlacklistHandlingRequest, crashBlacklistHandlingRequest)
        || ! blacklistHandlingRequestMatches (drainedCrashBlacklistHandlingRequest, crashBlacklistHandlingRequest)
        || ! blacklistHandlingRequestMatches (afterDrainedCrashBlacklistHandlingRequest, {})
        || ! blacklistHandlingRequestMatches (afterSecondDrainedCrashBlacklistHandlingRequest, {})
        || ! blacklistHandlingRequestMatches (commandQueuedCrashBlacklistHandlingRequest,
                                              crashBlacklistHandlingRequest)
        || ! blacklistHandlingCommandResultMatches (
            crashBlacklistHandlingCommandResult,
            { yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingCommandStatus::commandReady,
              crashBlacklistHandlingRequest,
              { yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingCommandKind::handleBlacklistRequest,
                yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash,
                true,
                false,
                true },
              true,
              false,
              false })
        || ! blacklistHandlingRequestMatches (afterCrashCommandDrainBlacklistHandlingRequest, {})
        || ! blacklistHandlingCommandResultMatches (
            afterCrashCommandSecondDrainBlacklistHandlingCommandResult, {})
        || ! deferredBlacklistHandlingCommandStatusMatches (
            crashDeferredBlacklistHandlingCommandStatus,
            { yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingCommandStatus::commandReady,
              crashBlacklistHandlingCommandResult,
              true,
              false,
              false })
        || ! deferredBlacklistHandlingCommandStatusMatches (
            crashDeferredBlacklistHandlingCommandInspection,
            crashDeferredBlacklistHandlingCommandStatus)
        || ! blacklistHandlingOutcomeMatches (
            crashBlacklistHandlingOutcome,
            { yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingOutcomeStatus::outcomeReady,
              yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash,
              true,
              false,
              true,
              false,
              false })
        || ! blacklistHandlingOutcomeMatches (
            queuedCrashBlacklistHandlingOutcome, crashBlacklistHandlingOutcome)
        || ! blacklistHandlingOutcomeMatches (
            pendingCrashBlacklistHandlingOutcome, crashBlacklistHandlingOutcome)
        || ! blacklistHandlingOutcomeMatches (
            drainedCrashBlacklistHandlingOutcome, crashBlacklistHandlingOutcome)
        || ! blacklistHandlingOutcomeMatches (afterDrainedCrashBlacklistHandlingOutcome, {})
        || ! blacklistHandlingOutcomeMatches (afterSecondDrainedCrashBlacklistHandlingOutcome, {})
        || ! blacklistHandlingOutcomeMatches (
            handlingQueuedCrashBlacklistHandlingOutcome, crashBlacklistHandlingOutcome)
        || ! blacklistHandlingOutcomeHandlingResultMatches (
            crashBlacklistHandlingOutcomeHandlingResult,
            { yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingOutcomeHandlingStatus::handlingReady,
              crashBlacklistHandlingOutcome,
              { yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash,
                true,
                false,
                true },
              true,
              false,
              false })
        || ! blacklistHandlingOutcomeMatches (afterCrashHandlingDrainBlacklistHandlingOutcome, {})
        || ! blacklistHandlingOutcomeHandlingResultMatches (
            afterCrashHandlingSecondBlacklistHandlingOutcomeHandlingResult, {})
        || ! deferredBlacklistHandlingCommandStatusMatches (
            afterCrashHandlingDeferredBlacklistHandlingCommandStatus,
            crashDeferredBlacklistHandlingCommandStatus)
        || ! deferredBlacklistHandlingOutcomeHandlingStatusMatches (
            crashDeferredBlacklistHandlingOutcomeHandlingStatus,
            { yesdaw::plugin_host::PluginHostCoordinator::BlacklistHandlingOutcomeHandlingStatus::handlingReady,
              crashBlacklistHandlingOutcomeHandlingResult,
              true,
              false,
              false })
        || ! deferredBlacklistHandlingOutcomeHandlingStatusMatches (
            crashDeferredBlacklistHandlingOutcomeHandlingInspection,
            crashDeferredBlacklistHandlingOutcomeHandlingStatus)
        || ! deferredBlacklistHandlingCommandStatusMatches (
            afterCrashHandlingReceiptDeferredBlacklistHandlingCommandStatus,
            crashDeferredBlacklistHandlingCommandStatus)
        || ! deferredBlacklistHandlingOutcomeHandlingStatusMatches (
            acknowledgedCrashDeferredBlacklistHandlingOutcomeHandlingStatus, {})
        || ! deferredBlacklistHandlingOutcomeHandlingStatusMatches (
            afterAcknowledgedCrashDeferredBlacklistHandlingOutcomeHandlingStatus, {})
        || ! deferredBlacklistHandlingCommandStatusMatches (
            afterAcknowledgedCrashOutcomeHandlingDeferredBlacklistHandlingCommandStatus,
            crashDeferredBlacklistHandlingCommandStatus)
        || ! deferredBlacklistHandlingCommandStatusMatches (
            acknowledgedCrashDeferredBlacklistHandlingCommandStatus, {})
        || ! deferredBlacklistHandlingCommandStatusMatches (
            afterAcknowledgedCrashDeferredBlacklistHandlingCommandStatus, {})
        || ! blacklistHandlingOutcomeMatches (afterAcknowledgedCrashBlacklistHandlingOutcome, {})
        || ! blacklistHandlingOutcomeMatches (afterAcknowledgedQueuedCrashBlacklistHandlingOutcome, {})
        || crashDeferredBlacklistHandlingCommandInspection.lastResult.command.failureKind
            == watchdogDeferredBlacklistHandlingCommandInspection.lastResult.command.failureKind
        || crashBlacklistHandlingCommandResult.command.failureKind
            == watchdogBlacklistHandlingCommandResult.command.failureKind
        || crashBlacklistHandlingOutcome.failureKind == watchdogBlacklistHandlingOutcome.failureKind
        || drainedCrashBlacklistHandlingOutcome.failureKind == drainedWatchdogBlacklistHandlingOutcome.failureKind
        || crashBlacklistHandlingOutcomeHandlingResult.handling.failureKind
            == watchdogBlacklistHandlingOutcomeHandlingResult.handling.failureKind
        || crashDeferredBlacklistHandlingOutcomeHandlingInspection.lastResult.handling.failureKind
            == watchdogDeferredBlacklistHandlingOutcomeHandlingInspection.lastResult.handling.failureKind
        || crashBlacklistHandlingOutcomeHandlingResult.drainedOutcome.failureKind
            == watchdogBlacklistHandlingOutcomeHandlingResult.drainedOutcome.failureKind
        || drainedCrashBlacklistHandlingRequest.failureKind == drainedWatchdogBlacklistHandlingRequest.failureKind)
    {
        std::printf ("FAIL: plugin host coordinator crash pending blacklist-handling request command receipt/status/outcome queue/handling shell is wrong: request=%s/%s queued=%s/%s pending=%s/%s drained=%s/%s watchdogDrained=%s after=%s/%s second=%s/%s commandQueued=%s/%s command=%s/%s/%s watchdogCommand=%s consumed=%d policy=%d persisted=%d afterCommand=%s/%s secondCommand=%s/%s receipt=%s receiptRecorded=%d receiptCommand=%s/%s inspected=%s inspectedRecorded=%d inspectedCommand=%s/%s outcome=%s/%s/%d/%d/%d queuedOutcome=%s/%s pendingOutcome=%s/%s drainedOutcome=%s/%s watchdogDrainedOutcome=%s afterDrainedOutcome=%s/%s secondDrainedOutcome=%s/%s watchdogOutcome=%s handlingQueued=%s/%s handling=%s/%s watchdogHandling=%s handlingConsumed=%d afterHandling=%s/%s secondHandling=%s afterHandlingReceipt=%s/%d handlingPolicy=%d handlingPersisted=%d receiptAck=%s receiptAckRecorded=%d afterReceiptAck=%s afterReceiptAckRecorded=%d afterOutcome=%s/%s/%d/%d/%d watchdogReceiptCommand=%s\n",
                     statusName (crashBlacklistHandlingRequest.status),
                     statusName (crashBlacklistHandlingRequest.failureKind),
                     statusName (queuedCrashBlacklistHandlingRequest.status),
                     statusName (queuedCrashBlacklistHandlingRequest.failureKind),
                     statusName (pendingCrashBlacklistHandlingRequest.status),
                     statusName (pendingCrashBlacklistHandlingRequest.failureKind),
                     statusName (drainedCrashBlacklistHandlingRequest.status),
                     statusName (drainedCrashBlacklistHandlingRequest.failureKind),
                     statusName (drainedWatchdogBlacklistHandlingRequest.failureKind),
                     statusName (afterDrainedCrashBlacklistHandlingRequest.status),
                     statusName (afterDrainedCrashBlacklistHandlingRequest.failureKind),
                     statusName (afterSecondDrainedCrashBlacklistHandlingRequest.status),
                     statusName (afterSecondDrainedCrashBlacklistHandlingRequest.failureKind),
                     statusName (commandQueuedCrashBlacklistHandlingRequest.status),
                     statusName (commandQueuedCrashBlacklistHandlingRequest.failureKind),
                     statusName (crashBlacklistHandlingCommandResult.status),
                     statusName (crashBlacklistHandlingCommandResult.command.command),
                     statusName (crashBlacklistHandlingCommandResult.command.failureKind),
                     statusName (watchdogBlacklistHandlingCommandResult.command.failureKind),
                     crashBlacklistHandlingCommandResult.pendingRequestConsumed ? 1 : 0,
                     crashBlacklistHandlingCommandResult.blacklistPolicyApplied ? 1 : 0,
                     crashBlacklistHandlingCommandResult.blacklistStatePersisted ? 1 : 0,
                     statusName (afterCrashCommandDrainBlacklistHandlingRequest.status),
                     statusName (afterCrashCommandDrainBlacklistHandlingRequest.failureKind),
                     statusName (afterCrashCommandSecondDrainBlacklistHandlingCommandResult.status),
                     statusName (afterCrashCommandSecondDrainBlacklistHandlingCommandResult.command.failureKind),
                     statusName (crashDeferredBlacklistHandlingCommandStatus.status),
                     crashDeferredBlacklistHandlingCommandStatus.commandRecorded ? 1 : 0,
                     statusName (crashDeferredBlacklistHandlingCommandStatus.lastResult.command.command),
                     statusName (crashDeferredBlacklistHandlingCommandStatus.lastResult.command.failureKind),
                     statusName (crashDeferredBlacklistHandlingCommandInspection.status),
                     crashDeferredBlacklistHandlingCommandInspection.commandRecorded ? 1 : 0,
                     statusName (crashDeferredBlacklistHandlingCommandInspection.lastResult.command.command),
                     statusName (crashDeferredBlacklistHandlingCommandInspection.lastResult.command.failureKind),
                     statusName (crashBlacklistHandlingOutcome.status),
                     statusName (crashBlacklistHandlingOutcome.failureKind),
                     crashBlacklistHandlingOutcome.crashCandidate ? 1 : 0,
                     crashBlacklistHandlingOutcome.watchdogTimeoutCandidate ? 1 : 0,
                     crashBlacklistHandlingOutcome.controlThreadBlacklistHandlingInspected ? 1 : 0,
                     statusName (queuedCrashBlacklistHandlingOutcome.status),
                     statusName (queuedCrashBlacklistHandlingOutcome.failureKind),
                     statusName (pendingCrashBlacklistHandlingOutcome.status),
                     statusName (pendingCrashBlacklistHandlingOutcome.failureKind),
                     statusName (drainedCrashBlacklistHandlingOutcome.status),
                     statusName (drainedCrashBlacklistHandlingOutcome.failureKind),
                     statusName (drainedWatchdogBlacklistHandlingOutcome.failureKind),
                     statusName (afterDrainedCrashBlacklistHandlingOutcome.status),
                     statusName (afterDrainedCrashBlacklistHandlingOutcome.failureKind),
                     statusName (afterSecondDrainedCrashBlacklistHandlingOutcome.status),
                     statusName (afterSecondDrainedCrashBlacklistHandlingOutcome.failureKind),
                     statusName (watchdogBlacklistHandlingOutcome.failureKind),
                     statusName (handlingQueuedCrashBlacklistHandlingOutcome.status),
                     statusName (handlingQueuedCrashBlacklistHandlingOutcome.failureKind),
                     statusName (crashBlacklistHandlingOutcomeHandlingResult.status),
                     statusName (crashBlacklistHandlingOutcomeHandlingResult.handling.failureKind),
                     statusName (watchdogBlacklistHandlingOutcomeHandlingResult.handling.failureKind),
                     crashBlacklistHandlingOutcomeHandlingResult.pendingOutcomeConsumed ? 1 : 0,
                     statusName (afterCrashHandlingDrainBlacklistHandlingOutcome.status),
                     statusName (afterCrashHandlingDrainBlacklistHandlingOutcome.failureKind),
                     statusName (afterCrashHandlingSecondBlacklistHandlingOutcomeHandlingResult.status),
                     statusName (afterCrashHandlingDeferredBlacklistHandlingCommandStatus.status),
                     afterCrashHandlingDeferredBlacklistHandlingCommandStatus.commandRecorded ? 1 : 0,
                     crashBlacklistHandlingOutcomeHandlingResult.blacklistPolicyApplied ? 1 : 0,
                     crashBlacklistHandlingOutcomeHandlingResult.blacklistStatePersisted ? 1 : 0,
                     statusName (acknowledgedCrashDeferredBlacklistHandlingCommandStatus.status),
                     acknowledgedCrashDeferredBlacklistHandlingCommandStatus.commandRecorded ? 1 : 0,
                     statusName (afterAcknowledgedCrashDeferredBlacklistHandlingCommandStatus.status),
                     afterAcknowledgedCrashDeferredBlacklistHandlingCommandStatus.commandRecorded ? 1 : 0,
                     statusName (afterAcknowledgedCrashBlacklistHandlingOutcome.status),
                     statusName (afterAcknowledgedCrashBlacklistHandlingOutcome.failureKind),
                     afterAcknowledgedCrashBlacklistHandlingOutcome.crashCandidate ? 1 : 0,
                     afterAcknowledgedCrashBlacklistHandlingOutcome.watchdogTimeoutCandidate ? 1 : 0,
                     afterAcknowledgedCrashBlacklistHandlingOutcome.controlThreadBlacklistHandlingInspected ? 1 : 0,
                     statusName (watchdogDeferredBlacklistHandlingCommandInspection.lastResult.command.failureKind));
        return 2;
    }

    const auto acknowledgedCrashDeferredBlacklistPolicyDecisionCommandStatus =
        blacklistPolicyCommandReceiptCoordinator.acknowledgeDeferredBlacklistPolicyDecisionCommandStatus();
    const auto afterAcknowledgedCrashDeferredBlacklistPolicyDecisionCommandStatus =
        blacklistPolicyCommandReceiptCoordinator.deferredBlacklistPolicyDecisionCommandStatus();
    const auto afterAcknowledgedCrashBlacklistPolicyDecisionOutcome =
        blacklistPolicyCommandReceiptCoordinator.blacklistPolicyDecisionOutcomeStatus();
    const auto afterAcknowledgedQueuedCrashBlacklistPolicyDecisionOutcome =
        blacklistPolicyCommandReceiptCoordinator.queueBlacklistPolicyDecisionOutcomeForDeferredCommand();
    const auto afterAcknowledgedCrashBlacklistPolicyDecisionOutcomeHandlingResult =
        blacklistPolicyCommandReceiptCoordinator.drainPendingBlacklistPolicyDecisionOutcomeToControlHandling();
    if (! deferredBlacklistPolicyDecisionCommandStatusMatches (
            acknowledgedCrashDeferredBlacklistPolicyDecisionCommandStatus, {})
        || ! deferredBlacklistPolicyDecisionCommandStatusMatches (
            afterAcknowledgedCrashDeferredBlacklistPolicyDecisionCommandStatus, {})
        || ! blacklistPolicyDecisionOutcomeMatches (afterAcknowledgedCrashBlacklistPolicyDecisionOutcome, {})
        || ! blacklistPolicyDecisionOutcomeMatches (
            afterAcknowledgedQueuedCrashBlacklistPolicyDecisionOutcome, {})
        || ! blacklistPolicyDecisionOutcomeHandlingResultMatches (
            afterAcknowledgedCrashBlacklistPolicyDecisionOutcomeHandlingResult, {}))
    {
        std::printf ("FAIL: plugin host coordinator crash deferred blacklist policy-decision command acknowledge/clear status/outcome is wrong: status=%s inspected=%s recorded=%d inspectedRecorded=%d policy=%d persisted=%d inspectedPolicy=%d inspectedPersisted=%d outcome=%s/%s/%d/%d/%d queuedOutcome=%s/%s handling=%s/%d outcomePolicy=%d outcomePersisted=%d handlingPolicy=%d handlingPersisted=%d\n",
                     statusName (acknowledgedCrashDeferredBlacklistPolicyDecisionCommandStatus.status),
                     statusName (afterAcknowledgedCrashDeferredBlacklistPolicyDecisionCommandStatus.status),
                     acknowledgedCrashDeferredBlacklistPolicyDecisionCommandStatus.commandRecorded ? 1 : 0,
                     afterAcknowledgedCrashDeferredBlacklistPolicyDecisionCommandStatus.commandRecorded ? 1 : 0,
                     acknowledgedCrashDeferredBlacklistPolicyDecisionCommandStatus.blacklistPolicyApplied ? 1 : 0,
                     acknowledgedCrashDeferredBlacklistPolicyDecisionCommandStatus.blacklistStatePersisted ? 1 : 0,
                     afterAcknowledgedCrashDeferredBlacklistPolicyDecisionCommandStatus.blacklistPolicyApplied ? 1 : 0,
                     afterAcknowledgedCrashDeferredBlacklistPolicyDecisionCommandStatus.blacklistStatePersisted ? 1 : 0,
                     statusName (afterAcknowledgedCrashBlacklistPolicyDecisionOutcome.status),
                     statusName (afterAcknowledgedCrashBlacklistPolicyDecisionOutcome.failureKind),
                     afterAcknowledgedCrashBlacklistPolicyDecisionOutcome.crashCandidate ? 1 : 0,
                     afterAcknowledgedCrashBlacklistPolicyDecisionOutcome.watchdogTimeoutCandidate ? 1 : 0,
                     afterAcknowledgedCrashBlacklistPolicyDecisionOutcome.controlThreadPolicyDecisionInspected ? 1 : 0,
                     statusName (afterAcknowledgedQueuedCrashBlacklistPolicyDecisionOutcome.status),
                     statusName (afterAcknowledgedQueuedCrashBlacklistPolicyDecisionOutcome.failureKind),
                     statusName (afterAcknowledgedCrashBlacklistPolicyDecisionOutcomeHandlingResult.status),
                     afterAcknowledgedCrashBlacklistPolicyDecisionOutcomeHandlingResult.pendingOutcomeConsumed ? 1 : 0,
                     afterAcknowledgedCrashBlacklistPolicyDecisionOutcome.blacklistPolicyApplied ? 1 : 0,
                     afterAcknowledgedCrashBlacklistPolicyDecisionOutcome.blacklistStatePersisted ? 1 : 0,
                     afterAcknowledgedCrashBlacklistPolicyDecisionOutcomeHandlingResult.blacklistPolicyApplied ? 1 : 0,
                     afterAcknowledgedCrashBlacklistPolicyDecisionOutcomeHandlingResult.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    const auto acknowledgedDeferredBlacklistEscalationStatus =
        blacklistReceiptCoordinator.acknowledgeDeferredBlacklistEscalationStatus();
    const auto afterAcknowledgedDeferredBlacklistEscalationStatus =
        blacklistReceiptCoordinator.deferredBlacklistEscalationStatus();
    const auto afterAcknowledgedBlacklistPolicyDecisionRequest =
        blacklistReceiptCoordinator.blacklistPolicyDecisionRequest();
    if (! deferredBlacklistEscalationStatusMatches (acknowledgedDeferredBlacklistEscalationStatus, {})
        || ! deferredBlacklistEscalationStatusMatches (afterAcknowledgedDeferredBlacklistEscalationStatus, {})
        || ! blacklistPolicyDecisionRequestMatches (afterAcknowledgedBlacklistPolicyDecisionRequest, {}))
    {
        std::printf ("FAIL: plugin host coordinator deferred blacklist escalation acknowledge/clear status/request is wrong: status=%s inspected=%s recorded=%d inspectedRecorded=%d policy=%d persisted=%d inspectedPolicy=%d inspectedPersisted=%d request=%s/%s/%d/%d/%d requestPolicy=%d requestPersisted=%d\n",
                     statusName (acknowledgedDeferredBlacklistEscalationStatus.status),
                     statusName (afterAcknowledgedDeferredBlacklistEscalationStatus.status),
                     acknowledgedDeferredBlacklistEscalationStatus.escalationRecorded ? 1 : 0,
                     afterAcknowledgedDeferredBlacklistEscalationStatus.escalationRecorded ? 1 : 0,
                     acknowledgedDeferredBlacklistEscalationStatus.blacklistPolicyApplied ? 1 : 0,
                     acknowledgedDeferredBlacklistEscalationStatus.blacklistStatePersisted ? 1 : 0,
                     afterAcknowledgedDeferredBlacklistEscalationStatus.blacklistPolicyApplied ? 1 : 0,
                     afterAcknowledgedDeferredBlacklistEscalationStatus.blacklistStatePersisted ? 1 : 0,
                     statusName (afterAcknowledgedBlacklistPolicyDecisionRequest.status),
                     statusName (afterAcknowledgedBlacklistPolicyDecisionRequest.failureKind),
                     afterAcknowledgedBlacklistPolicyDecisionRequest.crashCandidate ? 1 : 0,
                     afterAcknowledgedBlacklistPolicyDecisionRequest.watchdogTimeoutCandidate ? 1 : 0,
                     afterAcknowledgedBlacklistPolicyDecisionRequest.controlThreadPolicyDecisionRequested ? 1 : 0,
                     afterAcknowledgedBlacklistPolicyDecisionRequest.blacklistPolicyApplied ? 1 : 0,
                     afterAcknowledgedBlacklistPolicyDecisionRequest.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    const auto crashAction = crashCoordinator.failureActionRequest();
    const auto autoQueuedCrashAction = crashCoordinator.pendingFailureActionRequest();
    if (crashAction.action != yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::bypassAndRecompile
        || crashAction.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash
        || ! crashAction.bypassRequested
        || ! crashAction.recompileRequested
        || crashAction.failureKind == watchdogAction.failureKind
        || ! requestMatches (autoQueuedCrashAction, crashAction))
    {
        std::printf ("FAIL: plugin host coordinator crash action request is wrong: action=%s failure=%s bypass=%d recompile=%d watchdogFailure=%s autoQueued=%s/%s\n",
                     statusName (crashAction.action),
                     statusName (crashAction.failureKind),
                     crashAction.bypassRequested ? 1 : 0,
                     crashAction.recompileRequested ? 1 : 0,
                     statusName (watchdogAction.failureKind),
                     statusName (autoQueuedCrashAction.action),
                     statusName (autoQueuedCrashAction.failureKind));
        return 2;
    }

    const auto queuedCrashAction = crashCoordinator.queueFailureActionRequestForCurrentFailure();
    const auto pendingCrashAction = crashCoordinator.pendingFailureActionRequest();
    const auto drainedCrashAction = crashCoordinator.drainPendingFailureActionRequest();
    const auto afterCrashDrainAction = crashCoordinator.pendingFailureActionRequest();
    if (! requestMatches (queuedCrashAction, crashAction)
        || ! requestMatches (pendingCrashAction, crashAction)
        || ! requestMatches (drainedCrashAction, crashAction)
        || afterCrashDrainAction.action != yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::none
        || afterCrashDrainAction.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none
        || afterCrashDrainAction.bypassRequested
        || afterCrashDrainAction.recompileRequested)
    {
        std::printf ("FAIL: plugin host coordinator crash pending action queue/drain is wrong: queued=%s/%s pending=%s/%s drained=%s/%s afterDrain=%s/%s\n",
                     statusName (queuedCrashAction.action),
                     statusName (queuedCrashAction.failureKind),
                     statusName (pendingCrashAction.action),
                     statusName (pendingCrashAction.failureKind),
                     statusName (drainedCrashAction.action),
                     statusName (drainedCrashAction.failureKind),
                     statusName (afterCrashDrainAction.action),
                     statusName (afterCrashDrainAction.failureKind));
        return 2;
    }

    const auto queuedCrashCommandAction = crashCoordinator.queueFailureActionRequestForCurrentFailure();
    const auto crashCommandResult = crashCoordinator.drainPendingFailureActionRequestToControlCommand();
    const auto afterCrashCommandDrainAction = crashCoordinator.pendingFailureActionRequest();
    if (! requestMatches (queuedCrashCommandAction, crashAction)
        || crashCommandResult.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::commandReady
        || ! requestMatches (crashCommandResult.drainedRequest, crashAction)
        || ! commandMatches (crashCommandResult.command,
                             { yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandKind::bypassAndRecompile,
                               yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash,
                               true,
                               true })
        || ! crashCommandResult.pendingRequestConsumed
        || crashCommandResult.graphRecompileExecuted
        || crashCommandResult.command.failureKind == watchdogCommandResult.command.failureKind
        || afterCrashCommandDrainAction.action != yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::none
        || afterCrashCommandDrainAction.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none
        || afterCrashCommandDrainAction.bypassRequested
        || afterCrashCommandDrainAction.recompileRequested)
    {
        std::printf ("FAIL: plugin host coordinator crash control command shell is wrong: queued=%s/%s status=%s drained=%s/%s command=%s/%s watchdogCommand=%s consumed=%d executed=%d afterDrain=%s/%s\n",
                     statusName (queuedCrashCommandAction.action),
                     statusName (queuedCrashCommandAction.failureKind),
                     statusName (crashCommandResult.status),
                     statusName (crashCommandResult.drainedRequest.action),
                     statusName (crashCommandResult.drainedRequest.failureKind),
                     statusName (crashCommandResult.command.command),
                     statusName (crashCommandResult.command.failureKind),
                     statusName (watchdogCommandResult.command.failureKind),
                     crashCommandResult.pendingRequestConsumed ? 1 : 0,
                     crashCommandResult.graphRecompileExecuted ? 1 : 0,
                     statusName (afterCrashCommandDrainAction.action),
                     statusName (afterCrashCommandDrainAction.failureKind));
        return 2;
    }

    const auto crashDeferredStatus =
        deferredReceiptCoordinator.recordDeferredGraphChangeCommandResult (crashCommandResult);
    const auto crashDeferredStatusInspection = deferredReceiptCoordinator.deferredGraphChangeCommandStatus();
    if (crashDeferredStatus.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::commandReady
        || ! commandResultMatches (crashDeferredStatus.lastResult, crashCommandResult)
        || ! crashDeferredStatus.commandRecorded
        || crashDeferredStatus.graphRecompileExecuted
        || ! commandResultMatches (crashDeferredStatusInspection.lastResult, crashCommandResult)
        || ! crashDeferredStatusInspection.commandRecorded
        || crashDeferredStatusInspection.graphRecompileExecuted
        || crashDeferredStatusInspection.lastResult.command.failureKind == watchdogDeferredStatusInspection.lastResult.command.failureKind)
    {
        std::printf ("FAIL: plugin host coordinator crash deferred command receipt/status is wrong: status=%s inspected=%s recorded=%d inspectedRecorded=%d executed=%d inspectedExecuted=%d watchdogCommand=%s crashCommand=%s\n",
                     statusName (crashDeferredStatus.status),
                     statusName (crashDeferredStatusInspection.status),
                     crashDeferredStatus.commandRecorded ? 1 : 0,
                     crashDeferredStatusInspection.commandRecorded ? 1 : 0,
                     crashDeferredStatus.graphRecompileExecuted ? 1 : 0,
                     crashDeferredStatusInspection.graphRecompileExecuted ? 1 : 0,
                     statusName (watchdogDeferredStatusInspection.lastResult.command.failureKind),
                     statusName (crashDeferredStatusInspection.lastResult.command.failureKind));
        return 2;
    }

    const auto acknowledgedDeferredStatus =
        deferredReceiptCoordinator.acknowledgeDeferredGraphChangeCommandStatus();
    const auto afterAcknowledgedDeferredStatus =
        deferredReceiptCoordinator.deferredGraphChangeCommandStatus();
    if (acknowledgedDeferredStatus.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::noAction
        || acknowledgedDeferredStatus.lastResult.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::noAction
        || acknowledgedDeferredStatus.commandRecorded
        || acknowledgedDeferredStatus.graphRecompileExecuted
        || afterAcknowledgedDeferredStatus.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::noAction
        || afterAcknowledgedDeferredStatus.lastResult.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::noAction
        || afterAcknowledgedDeferredStatus.commandRecorded
        || afterAcknowledgedDeferredStatus.graphRecompileExecuted)
    {
        std::printf ("FAIL: plugin host coordinator deferred command acknowledge/clear status is wrong: status=%s inspected=%s recorded=%d inspectedRecorded=%d executed=%d inspectedExecuted=%d\n",
                     statusName (acknowledgedDeferredStatus.status),
                     statusName (afterAcknowledgedDeferredStatus.status),
                     acknowledgedDeferredStatus.commandRecorded ? 1 : 0,
                     afterAcknowledgedDeferredStatus.commandRecorded ? 1 : 0,
                     acknowledgedDeferredStatus.graphRecompileExecuted ? 1 : 0,
                     afterAcknowledgedDeferredStatus.graphRecompileExecuted ? 1 : 0);
        return 2;
    }

    auto executedGraphRecompileResult = crashCommandResult;
    executedGraphRecompileResult.graphRecompileExecuted = true;
    const auto executedGraphRecompileDeferredStatus =
        deferredReceiptCoordinator.recordDeferredGraphChangeCommandResult (executedGraphRecompileResult);
    if (executedGraphRecompileDeferredStatus.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::noAction
        || executedGraphRecompileDeferredStatus.commandRecorded
        || executedGraphRecompileDeferredStatus.graphRecompileExecuted)
    {
        std::printf ("FAIL: plugin host coordinator accepted a deferred command result that claimed graph recompile execution: status=%s recorded=%d executed=%d\n",
                     statusName (executedGraphRecompileDeferredStatus.status),
                     executedGraphRecompileDeferredStatus.commandRecorded ? 1 : 0,
                     executedGraphRecompileDeferredStatus.graphRecompileExecuted ? 1 : 0);
        return 2;
    }

    std::printf ("PASS: plugin host coordinator launched worker, allocated an OS-backed RT-lane shared-memory region, passed its identity over the control lane, observed worker attachment, proved the worker polls the mapped RT lane through the hosted processor path, killed a live child when the running RT-lane output sequence stopped advancing with input backlog, rejected missing/absent RT-lane identities without fallback storage, reported ready/handshake status, stopped worker, refused HostFailureKind::none commands, classified watchdog-timeout vs crash host failures, exposed and queued/drained future blacklist-candidate status, drained future blacklist escalation shells, recorded and acknowledged/cleared deferred blacklist escalation receipt/status without policy or persistence, derived and queued/drained future blacklist policy-decision requests only from valid deferred escalation receipts, drained future control-thread blacklist policy-decision command shells, recorded and acknowledged/cleared deferred blacklist policy-decision command receipt/status without policy or persistence, queued/drained future blacklist policy-decision outcomes and drained them to future control-thread blacklist handling, recorded and acknowledged/cleared deferred blacklist policy-decision outcome handling receipt/status without policy or persistence, derived and queued/drained pending future blacklist-handling requests only from valid deferred outcome-handling receipts without policy or persistence, drained future control-thread blacklist-handling command shells, recorded and acknowledged/cleared deferred blacklist-handling command receipt/status without policy or persistence, queued/drained future blacklist-handling outcomes and drained them to future control-thread blacklist handling, recorded and acknowledged/cleared deferred blacklist-handling outcome handling receipt/status without policy or persistence, requested future bypass/recompile actions, queued/drained pending failure actions, drained future control-thread graph-change command shells, recorded deferred command receipt/status, and acknowledged/cleared it without executing graph recompiles\n");
    return 0;
}
