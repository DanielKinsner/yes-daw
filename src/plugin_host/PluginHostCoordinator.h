#pragma once

#include "engine/GraphBuilder.h"
#include "engine/Runtime.h"
#include "engine/plugin/RtLaneRing.h"
#include "plugin_host/PluginHostProtocol.h"

#include <juce_events/juce_events.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace yesdaw::plugin_host {

class PluginHostCoordinator final : private juce::ChildProcessCoordinator
{
public:
    enum class HandshakeStatus
    {
        notStarted,
        success,
        launchFailed,
        readyTimeout,
        probeSendFailed,
        echoTimeout,
        connectionLost
    };

    enum class StopStatus
    {
        notStarted,
        stopped,
        stopTimeout
    };

    enum class WatchdogStatus
    {
        notStarted,
        launchFailed,
        readyTimeout,
        probeSendFailed,
        timeoutKilled,
        killObservationTimeout,
        connectionLost,
        unexpectedResponse
    };

    enum class CrashStatus
    {
        notStarted,
        launchFailed,
        readyTimeout,
        connectionLost,
        observationTimeout
    };

    enum class HostFailureKind
    {
        none,
        crash,
        watchdogTimeout
    };

    enum class FailureActionKind
    {
        none,
        bypassAndRecompile
    };

    enum class GraphChangeCommandKind
    {
        none,
        bypassAndRecompile
    };

    enum class GraphChangeCommandStatus
    {
        noAction,
        commandReady
    };

    enum class GraphRecompileStatus
    {
        noAction,
        compileFailed,
        missingPlaceholder,
        publishFailed,
        graphPublished
    };

    enum class BlacklistEscalationStatus
    {
        noAction,
        escalationReady
    };

    enum class BlacklistPolicyDecisionRequestStatus
    {
        noAction,
        requestReady
    };

    enum class BlacklistPolicyDecisionCommandKind
    {
        none,
        requestPolicyDecision
    };

    enum class BlacklistPolicyDecisionCommandStatus
    {
        noAction,
        commandReady
    };

    enum class BlacklistPolicyDecisionOutcomeStatus
    {
        noAction,
        outcomeReady
    };

    enum class BlacklistPolicyDecisionOutcomeHandlingStatus
    {
        noAction,
        handlingReady
    };

    enum class BlacklistHandlingRequestStatus
    {
        noAction,
        requestReady
    };

    enum class BlacklistHandlingCommandKind
    {
        none,
        handleBlacklistRequest
    };

    enum class BlacklistHandlingCommandStatus
    {
        noAction,
        commandReady
    };

    enum class BlacklistHandlingOutcomeStatus
    {
        noAction,
        outcomeReady
    };

    enum class BlacklistHandlingOutcomeHandlingStatus
    {
        noAction,
        handlingReady
    };

    enum class ChildState
    {
        idle,
        launching,
        ready,
        handshaking,
        running,
        stopping,
        stopped,
        lost
    };

    enum class RtLaneLoadStatus
    {
        notStarted,
        launchFailed,
        readyTimeout,
        allocationFailed,
        messageSendFailed,
        replyTimeout,
        connectionLost,
        workerRejected,
        success
    };

    enum class PluginStateRoundTripStatus
    {
        notStarted,
        rtLaneLoadFailed,
        pullSendFailed,
        pullReplyTimeout,
        pullRejected,
        corruptPushSendFailed,
        corruptPushReplyTimeout,
        corruptPushNotRejected,
        pushSendFailed,
        pushReplyTimeout,
        pushRejected,
        connectionLost,
        success
    };

    struct HandshakeResult
    {
        HandshakeStatus status { HandshakeStatus::notStarted };
        bool readySeen { false };
        bool probeEchoed { false };
    };

    struct StopResult
    {
        StopStatus status { StopStatus::notStarted };
        bool connectionLostSeen { false };
    };

    struct WatchdogResult
    {
        WatchdogStatus status { WatchdogStatus::notStarted };
        bool readySeen { false };
        bool watchdogTimedOut { false };
        bool killRequested { false };
        bool connectionLostSeen { false };
        bool runningRtLaneBacklogSeen { false };
        bool runningRtLaneOutputProgressSeen { false };
        std::uint64_t runningRtLaneInputSeq { 0 };
        std::uint64_t runningRtLaneOutputSeq { 0 };
    };

    struct CrashResult
    {
        CrashStatus status { CrashStatus::notStarted };
        bool readySeen { false };
        bool crashObservationRequested { false };
        bool connectionLostSeen { false };
        HostFailureKind failureKind { HostFailureKind::none };
    };

    struct HostFailureReport
    {
        HostFailureKind kind { HostFailureKind::none };
        bool connectionLostSeen { false };
        bool watchdogTimedOut { false };
        bool watchdogKillRequested { false };
    };

    struct BlacklistCandidateStatus
    {
        HostFailureKind failureKind { HostFailureKind::none };
        bool candidate { false };
        bool crashCandidate { false };
        bool watchdogTimeoutCandidate { false };
    };

    struct BlacklistEscalation
    {
        HostFailureKind failureKind { HostFailureKind::none };
        bool crashCandidate { false };
        bool watchdogTimeoutCandidate { false };
        bool controlThreadEscalationRequested { false };
    };

    struct FailureActionRequest
    {
        FailureActionKind action { FailureActionKind::none };
        HostFailureKind failureKind { HostFailureKind::none };
        bool bypassRequested { false };
        bool recompileRequested { false };
    };

    struct GraphChangeCommand
    {
        GraphChangeCommandKind command { GraphChangeCommandKind::none };
        HostFailureKind failureKind { HostFailureKind::none };
        bool bypassRequested { false };
        bool recompileRequested { false };
    };

    struct GraphChangeCommandResult
    {
        GraphChangeCommandStatus status { GraphChangeCommandStatus::noAction };
        FailureActionRequest drainedRequest;
        GraphChangeCommand command;
        bool pendingRequestConsumed { false };
        bool graphRecompileExecuted { false };
    };

    struct DeferredGraphChangeCommandStatus
    {
        GraphChangeCommandStatus status { GraphChangeCommandStatus::noAction };
        GraphChangeCommandResult lastResult;
        bool commandRecorded { false };
        bool graphRecompileExecuted { false };
    };

    struct PlaceholderGraphRecompileResult
    {
        GraphRecompileStatus status { GraphRecompileStatus::noAction };
        GraphChangeCommandResult commandResult;
        yesdaw::engine::NodeId offenderNodeId { 0 };
        yesdaw::engine::GraphBuildError::Code buildError {
            yesdaw::engine::GraphBuildError::Code::None
        };
        bool pendingRequestConsumed { false };
        bool placeholderCompiled { false };
        bool orderedPublishAccepted { false };
        bool graphRecompileExecuted { false };
    };

    struct BlacklistEscalationResult
    {
        BlacklistEscalationStatus status { BlacklistEscalationStatus::noAction };
        BlacklistCandidateStatus drainedCandidate;
        BlacklistEscalation escalation;
        bool pendingCandidateConsumed { false };
        bool blacklistPolicyApplied { false };
        bool blacklistStatePersisted { false };
    };

    struct DeferredBlacklistEscalationStatus
    {
        BlacklistEscalationStatus status { BlacklistEscalationStatus::noAction };
        BlacklistEscalationResult lastResult;
        bool escalationRecorded { false };
        bool blacklistPolicyApplied { false };
        bool blacklistStatePersisted { false };
    };

    struct BlacklistPolicyDecisionRequest
    {
        BlacklistPolicyDecisionRequestStatus status { BlacklistPolicyDecisionRequestStatus::noAction };
        HostFailureKind failureKind { HostFailureKind::none };
        bool crashCandidate { false };
        bool watchdogTimeoutCandidate { false };
        bool controlThreadPolicyDecisionRequested { false };
        bool blacklistPolicyApplied { false };
        bool blacklistStatePersisted { false };
    };

    struct BlacklistPolicyDecisionCommand
    {
        BlacklistPolicyDecisionCommandKind command { BlacklistPolicyDecisionCommandKind::none };
        HostFailureKind failureKind { HostFailureKind::none };
        bool crashCandidate { false };
        bool watchdogTimeoutCandidate { false };
        bool controlThreadPolicyDecisionRequested { false };
    };

    struct BlacklistPolicyDecisionCommandResult
    {
        BlacklistPolicyDecisionCommandStatus status { BlacklistPolicyDecisionCommandStatus::noAction };
        BlacklistPolicyDecisionRequest drainedRequest;
        BlacklistPolicyDecisionCommand command;
        bool pendingRequestConsumed { false };
        bool blacklistPolicyApplied { false };
        bool blacklistStatePersisted { false };
    };

    struct DeferredBlacklistPolicyDecisionCommandStatus
    {
        BlacklistPolicyDecisionCommandStatus status { BlacklistPolicyDecisionCommandStatus::noAction };
        BlacklistPolicyDecisionCommandResult lastResult;
        bool commandRecorded { false };
        bool blacklistPolicyApplied { false };
        bool blacklistStatePersisted { false };
    };

    struct BlacklistPolicyDecisionOutcome
    {
        BlacklistPolicyDecisionOutcomeStatus status { BlacklistPolicyDecisionOutcomeStatus::noAction };
        HostFailureKind failureKind { HostFailureKind::none };
        bool crashCandidate { false };
        bool watchdogTimeoutCandidate { false };
        bool controlThreadPolicyDecisionInspected { false };
        bool blacklistPolicyApplied { false };
        bool blacklistStatePersisted { false };
    };

    struct BlacklistPolicyDecisionOutcomeHandling
    {
        HostFailureKind failureKind { HostFailureKind::none };
        bool crashCandidate { false };
        bool watchdogTimeoutCandidate { false };
        bool controlThreadBlacklistHandlingRequested { false };
    };

    struct BlacklistPolicyDecisionOutcomeHandlingResult
    {
        BlacklistPolicyDecisionOutcomeHandlingStatus status {
            BlacklistPolicyDecisionOutcomeHandlingStatus::noAction
        };
        BlacklistPolicyDecisionOutcome drainedOutcome;
        BlacklistPolicyDecisionOutcomeHandling handling;
        bool pendingOutcomeConsumed { false };
        bool blacklistPolicyApplied { false };
        bool blacklistStatePersisted { false };
    };

    struct DeferredBlacklistPolicyDecisionOutcomeHandlingStatus
    {
        BlacklistPolicyDecisionOutcomeHandlingStatus status {
            BlacklistPolicyDecisionOutcomeHandlingStatus::noAction
        };
        BlacklistPolicyDecisionOutcomeHandlingResult lastResult;
        bool handlingRecorded { false };
        bool blacklistPolicyApplied { false };
        bool blacklistStatePersisted { false };
    };

    struct BlacklistHandlingRequest
    {
        BlacklistHandlingRequestStatus status { BlacklistHandlingRequestStatus::noAction };
        HostFailureKind failureKind { HostFailureKind::none };
        bool crashCandidate { false };
        bool watchdogTimeoutCandidate { false };
        bool controlThreadBlacklistHandlingRequested { false };
        bool blacklistPolicyApplied { false };
        bool blacklistStatePersisted { false };
    };

    struct BlacklistHandlingCommand
    {
        BlacklistHandlingCommandKind command { BlacklistHandlingCommandKind::none };
        HostFailureKind failureKind { HostFailureKind::none };
        bool crashCandidate { false };
        bool watchdogTimeoutCandidate { false };
        bool controlThreadBlacklistHandlingRequested { false };
    };

    struct BlacklistHandlingCommandResult
    {
        BlacklistHandlingCommandStatus status { BlacklistHandlingCommandStatus::noAction };
        BlacklistHandlingRequest drainedRequest;
        BlacklistHandlingCommand command;
        bool pendingRequestConsumed { false };
        bool blacklistPolicyApplied { false };
        bool blacklistStatePersisted { false };
    };

    struct DeferredBlacklistHandlingCommandStatus
    {
        BlacklistHandlingCommandStatus status { BlacklistHandlingCommandStatus::noAction };
        BlacklistHandlingCommandResult lastResult;
        bool commandRecorded { false };
        bool blacklistPolicyApplied { false };
        bool blacklistStatePersisted { false };
    };

    struct BlacklistHandlingOutcome
    {
        BlacklistHandlingOutcomeStatus status { BlacklistHandlingOutcomeStatus::noAction };
        HostFailureKind failureKind { HostFailureKind::none };
        bool crashCandidate { false };
        bool watchdogTimeoutCandidate { false };
        bool controlThreadBlacklistHandlingInspected { false };
        bool blacklistPolicyApplied { false };
        bool blacklistStatePersisted { false };
    };

    struct BlacklistHandlingOutcomeHandling
    {
        HostFailureKind failureKind { HostFailureKind::none };
        bool crashCandidate { false };
        bool watchdogTimeoutCandidate { false };
        bool controlThreadBlacklistHandlingRequested { false };
    };

    struct BlacklistHandlingOutcomeHandlingResult
    {
        BlacklistHandlingOutcomeHandlingStatus status {
            BlacklistHandlingOutcomeHandlingStatus::noAction
        };
        BlacklistHandlingOutcome drainedOutcome;
        BlacklistHandlingOutcomeHandling handling;
        bool pendingOutcomeConsumed { false };
        bool blacklistPolicyApplied { false };
        bool blacklistStatePersisted { false };
    };

    struct DeferredBlacklistHandlingOutcomeHandlingStatus
    {
        BlacklistHandlingOutcomeHandlingStatus status {
            BlacklistHandlingOutcomeHandlingStatus::noAction
        };
        BlacklistHandlingOutcomeHandlingResult lastResult;
        bool handlingRecorded { false };
        bool blacklistPolicyApplied { false };
        bool blacklistStatePersisted { false };
    };

    struct ChildStatus
    {
        ChildState state { ChildState::idle };
        HandshakeStatus handshakeStatus { HandshakeStatus::notStarted };
        StopStatus stopStatus { StopStatus::notStarted };
        WatchdogStatus watchdogStatus { WatchdogStatus::notStarted };
        CrashStatus crashStatus { CrashStatus::notStarted };
        HostFailureKind failureKind { HostFailureKind::none };
        bool launchAttempted { false };
        bool readySeen { false };
        bool probeEchoed { false };
        bool stopRequested { false };
        bool watchdogTimedOut { false };
        bool watchdogKillRequested { false };
        bool crashObservationRequested { false };
        bool connectionLostSeen { false };
    };

    struct RtLaneLoadIdentity
    {
        std::string sharedMemoryName;
        RtLaneLoadConfig config;
    };

    struct RtLaneLoadResult
    {
        RtLaneLoadStatus status { RtLaneLoadStatus::notStarted };
        RtLaneLoadIdentity identity;
        RtLaneLoadReplyStatus workerReplyStatus { RtLaneLoadReplyStatus::none };
        yesdaw::engine::RtLaneAttachFailure workerAttachFailure {
            yesdaw::engine::RtLaneAttachFailure::None
        };
        int workerAttachSystemError { 0 };
        bool readySeen { false };
        bool loadMessageSent { false };
        bool loadReplySeen { false };
        bool coordinatorAllocated { false };
        bool coordinatorUsesOsSharedMemory { false };
        bool workerAccepted { false };
    };

    struct PluginStateRoundTripResult
    {
        PluginStateRoundTripStatus status { PluginStateRoundTripStatus::notStarted };
        RtLaneLoadResult loadResult;
        PluginStateReplyStatus pullReplyStatus { PluginStateReplyStatus::none };
        PluginStateReplyStatus corruptPushReplyStatus { PluginStateReplyStatus::none };
        PluginStateReplyStatus pushReplyStatus { PluginStateReplyStatus::none };
        std::uint32_t chunkLength { 0 };
        std::uint32_t crc32 { 0 };
        std::uint32_t restoredChunkLength { 0 };
        std::uint32_t restoredCrc32 { 0 };
        bool readySeen { false };
        bool pullRequestSent { false };
        bool pullReplySeen { false };
        bool corruptPushRequestSent { false };
        bool corruptPushReplySeen { false };
        bool pushRequestSent { false };
        bool pushReplySeen { false };
        bool pulledChunkValidated { false };
        bool corruptCrcRejected { false };
        bool pushedChunkAccepted { false };
    };

    struct RtLaneProgress
    {
        std::uint64_t inputSeq { 0 };
        std::uint64_t outputSeq { 0 };
    };

    explicit PluginHostCoordinator (std::chrono::milliseconds timeout = std::chrono::milliseconds (4000),
                                    std::chrono::milliseconds watchdogTimeout = std::chrono::milliseconds (250))
        : timeout_ (timeout),
          watchdogTimeout_ (watchdogTimeout)
    {
    }

    ~PluginHostCoordinator() override
    {
        stop();
    }

    HandshakeResult launchAndHandshake (const juce::File& workerExecutable)
    {
        stop();
        resetState();

        {
            std::lock_guard<std::mutex> lock (mutex_);
            launchAttempted_ = true;
            childState_ = ChildState::launching;
        }

        if (! launchWorkerProcess (workerExecutable, kWorkerCommandLineId, static_cast<int> (timeout_.count())))
            return result (HandshakeStatus::launchFailed);

        if (! waitFor ([this] { return readySeen_ || connectionLost_; }))
            return result (HandshakeStatus::readyTimeout);

        if (connectionLost())
            return result (HandshakeStatus::connectionLost);

        {
            std::lock_guard<std::mutex> lock (mutex_);
            childState_ = ChildState::handshaking;
        }

        if (! sendMessageToWorker (makeMessage (kHandshakeProbeMessage)))
            return result (HandshakeStatus::probeSendFailed);

        if (! waitFor ([this] { return probeEchoed_ || connectionLost_; }))
            return result (HandshakeStatus::echoTimeout);

        if (connectionLost())
            return result (HandshakeStatus::connectionLost);

        return result (HandshakeStatus::success);
    }

    RtLaneLoadResult launchAndLoadRtLane (const juce::File& workerExecutable,
                                          yesdaw::engine::RtLaneConfig config)
    {
        const std::string sharedMemoryName = yesdaw::engine::RtLaneRing::makeUniqueSharedMemoryName();
        RtLaneLoadIdentity identity { sharedMemoryName, rtLaneLoadConfigFor (config) };

        auto ownerEndpoint = std::make_unique<yesdaw::engine::RtLaneRing>();
        try
        {
            ownerEndpoint->prepareSharedMemory (config, sharedMemoryName);
        }
        catch (...)
        {
            return rtLaneLoadResult (RtLaneLoadStatus::allocationFailed, identity, false, false);
        }

        if (! ownerEndpoint->usesOsSharedMemory())
            return rtLaneLoadResult (RtLaneLoadStatus::allocationFailed, identity, false, false);

        return launchAndSendRtLaneLoadIdentityInternal (workerExecutable, std::move (identity),
                                                       std::move (ownerEndpoint));
    }

    PluginStateRoundTripResult launchAndRoundTripSyntheticPluginState (
        const juce::File& workerExecutable,
        yesdaw::engine::RtLaneConfig config)
    {
        const RtLaneLoadResult load = launchAndLoadRtLane (workerExecutable, config);
        PluginStateReplyMessage corruptPushReply;

        auto finish = [&] (PluginStateRoundTripStatus status)
        {
            std::lock_guard<std::mutex> lock (mutex_);
            const bool pulledChunkValidated = pluginStatePullReplySeen_
                && lastPluginStatePullReply_.status == PluginStateReplyStatus::pulled
                && pluginStateReplyCrcMatches (lastPluginStatePullReply_);
            const bool pushedChunkAccepted = pluginStatePushReplySeen_
                && lastPluginStatePushReply_.status == PluginStateReplyStatus::restored
                && lastPluginStatePushReply_.stateAccepted != 0u
                && lastPluginStatePushReply_.chunkLength == lastPluginStatePullReply_.chunkLength
                && lastPluginStatePushReply_.crc32 == lastPluginStatePullReply_.crc32;

            return PluginStateRoundTripResult {
                status,
                load,
                lastPluginStatePullReply_.status,
                corruptPushReply.status,
                lastPluginStatePushReply_.status,
                lastPluginStatePullReply_.chunkLength,
                lastPluginStatePullReply_.crc32,
                lastPluginStatePushReply_.chunkLength,
                lastPluginStatePushReply_.crc32,
                readySeen_,
                pluginStatePullRequestSent_,
                pluginStatePullReplySeen_,
                pluginStateCorruptPushRequestSent_,
                corruptPushReply.status != PluginStateReplyStatus::none,
                pluginStatePushRequestSent_,
                pluginStatePushReplySeen_,
                pulledChunkValidated,
                corruptPushReply.status == PluginStateReplyStatus::rejectedCrcMismatch,
                pushedChunkAccepted
            };
        };

        if (load.status != RtLaneLoadStatus::success)
            return finish (PluginStateRoundTripStatus::rtLaneLoadFailed);

        {
            std::lock_guard<std::mutex> lock (mutex_);
            pluginStatePullRequestSent_ = false;
            pluginStatePullReplySeen_ = false;
            pluginStateCorruptPushRequestSent_ = false;
            pluginStatePushRequestSent_ = false;
            pluginStatePushReplySeen_ = false;
            lastPluginStatePullReply_ = {};
            lastPluginStatePushReply_ = {};
        }

        const PluginStateRequestMessage pullRequest = makePluginStatePullRequestMessage();
        if (! sendMessageToWorker (makeMessage (pullRequest)))
            return finish (PluginStateRoundTripStatus::pullSendFailed);

        {
            std::lock_guard<std::mutex> lock (mutex_);
            pluginStatePullRequestSent_ = true;
        }

        if (! waitFor ([this] { return pluginStatePullReplySeen_ || connectionLost_; }))
            return finish (PluginStateRoundTripStatus::pullReplyTimeout);

        if (connectionLost())
            return finish (PluginStateRoundTripStatus::connectionLost);

        PluginStateReplyMessage pullReply;
        {
            std::lock_guard<std::mutex> lock (mutex_);
            pullReply = lastPluginStatePullReply_;
        }

        if (pullReply.status != PluginStateReplyStatus::pulled || ! pluginStateReplyCrcMatches (pullReply))
            return finish (PluginStateRoundTripStatus::pullRejected);

        PluginStateRequestMessage corruptPushRequest =
            makePluginStatePushRequestMessage (pluginStateChunkBytes (pullReply), pullReply.crc32 ^ 0x00000001u);

        {
            std::lock_guard<std::mutex> lock (mutex_);
            pluginStateCorruptPushRequestSent_ = true;
            pluginStatePushRequestSent_ = true;
            pluginStatePushReplySeen_ = false;
            lastPluginStatePushReply_ = {};
        }

        if (! sendMessageToWorker (makeMessage (corruptPushRequest)))
            return finish (PluginStateRoundTripStatus::corruptPushSendFailed);

        if (! waitFor ([this] { return pluginStatePushReplySeen_ || connectionLost_; }))
            return finish (PluginStateRoundTripStatus::corruptPushReplyTimeout);

        if (connectionLost())
            return finish (PluginStateRoundTripStatus::connectionLost);

        {
            std::lock_guard<std::mutex> lock (mutex_);
            corruptPushReply = lastPluginStatePushReply_;
        }

        if (corruptPushReply.status != PluginStateReplyStatus::rejectedCrcMismatch)
            return finish (PluginStateRoundTripStatus::corruptPushNotRejected);

        PluginStateRequestMessage pushRequest =
            makePluginStatePushRequestMessage (pluginStateChunkBytes (pullReply), pullReply.crc32);

        {
            std::lock_guard<std::mutex> lock (mutex_);
            pluginStatePushRequestSent_ = true;
            pluginStatePushReplySeen_ = false;
            lastPluginStatePushReply_ = {};
        }

        if (! sendMessageToWorker (makeMessage (pushRequest)))
            return finish (PluginStateRoundTripStatus::pushSendFailed);

        if (! waitFor ([this] { return pluginStatePushReplySeen_ || connectionLost_; }))
            return finish (PluginStateRoundTripStatus::pushReplyTimeout);

        if (connectionLost())
            return finish (PluginStateRoundTripStatus::connectionLost);

        PluginStateReplyMessage pushReply;
        {
            std::lock_guard<std::mutex> lock (mutex_);
            pushReply = lastPluginStatePushReply_;
        }

        if (pushReply.status != PluginStateReplyStatus::restored
            || pushReply.stateAccepted == 0u
            || pushReply.chunkLength != pullReply.chunkLength
            || pushReply.crc32 != pullReply.crc32)
            return finish (PluginStateRoundTripStatus::pushRejected);

        return finish (PluginStateRoundTripStatus::success);
    }

    RtLaneLoadResult launchAndSendRtLaneLoadIdentity (const juce::File& workerExecutable,
                                                      RtLaneLoadIdentity identity)
    {
        return launchAndSendRtLaneLoadIdentityInternal (workerExecutable, std::move (identity), nullptr);
    }

    WatchdogResult launchAndExpectWatchdogTimeout (const juce::File& workerExecutable)
    {
        stop();
        resetState();

        {
            std::lock_guard<std::mutex> lock (mutex_);
            launchAttempted_ = true;
            childState_ = ChildState::launching;
        }

        if (! launchWorkerProcess (workerExecutable, kWorkerCommandLineId, static_cast<int> (timeout_.count())))
            return watchdogResult (WatchdogStatus::launchFailed);

        if (! waitFor ([this] { return readySeen_ || connectionLost_; }))
            return watchdogResult (WatchdogStatus::readyTimeout);

        if (connectionLost())
            return watchdogResult (WatchdogStatus::connectionLost);

        {
            std::lock_guard<std::mutex> lock (mutex_);
            childState_ = ChildState::handshaking;
        }

        if (! sendMessageToWorker (makeMessage (kWatchdogProbeMessage)))
            return watchdogResult (WatchdogStatus::probeSendFailed);

        if (waitFor (watchdogTimeout_, [this] { return probeEchoed_ || connectionLost_; }))
        {
            if (connectionLost())
                return watchdogResult (WatchdogStatus::connectionLost);

            return watchdogResult (WatchdogStatus::unexpectedResponse);
        }

        {
            std::lock_guard<std::mutex> lock (mutex_);
            handshakeStatus_ = HandshakeStatus::echoTimeout;
            watchdogTimedOut_ = true;
            watchdogKillRequested_ = true;
            childState_ = ChildState::stopping;
        }

        killWorkerProcess();

        if (! waitFor ([this] { return connectionLost_; }))
            return watchdogResult (WatchdogStatus::killObservationTimeout);

        return watchdogResult (WatchdogStatus::timeoutKilled);
    }

    WatchdogResult launchAndExpectRunningWatchdogTimeout (const juce::File& workerExecutable,
                                                          yesdaw::engine::RtLaneConfig config)
    {
        const RtLaneLoadResult load = launchAndLoadRtLane (workerExecutable, config);
        if (load.status == RtLaneLoadStatus::launchFailed)
            return watchdogResult (WatchdogStatus::launchFailed);
        if (load.status == RtLaneLoadStatus::readyTimeout)
            return watchdogResult (WatchdogStatus::readyTimeout);
        if (load.status == RtLaneLoadStatus::connectionLost)
            return watchdogResult (WatchdogStatus::connectionLost);
        if (load.status != RtLaneLoadStatus::success)
            return watchdogResult (WatchdogStatus::unexpectedResponse);

        const int channels = std::max (1, config.channels);
        const int frames = std::max (1, config.maxBlockSize);
        std::vector<float> input (static_cast<std::size_t> (channels) * static_cast<std::size_t> (frames), 0.0f);
        std::vector<float> output (input.size(), 0.0f);
        std::vector<float*> inputChannels (static_cast<std::size_t> (channels));
        std::vector<float*> outputChannels (static_cast<std::size_t> (channels));
        for (int channel = 0; channel < channels; ++channel)
        {
            inputChannels[static_cast<std::size_t> (channel)] =
                input.data() + static_cast<std::size_t> (channel) * static_cast<std::size_t> (frames);
            outputChannels[static_cast<std::size_t> (channel)] =
                output.data() + static_cast<std::size_t> (channel) * static_cast<std::size_t> (frames);
        }

        RtLaneProgress progress = activeRtLaneProgress();
        std::uint64_t lastOutputSeq = progress.outputSeq;
        bool outputProgressSeen = false;
        bool backlogSeen = false;

        publishActiveRtLaneWatchdogBlock (inputChannels.data(), channels, frames,
                                          outputChannels.data(), channels);

        const auto liveChildProgressDeadline = std::chrono::steady_clock::now() + timeout_;
        while (std::chrono::steady_clock::now() < liveChildProgressDeadline)
        {
            progress = activeRtLaneProgress();
            if (progress.outputSeq > lastOutputSeq)
            {
                lastOutputSeq = progress.outputSeq;
                outputProgressSeen = true;
                break;
            }

            if (connectionLost())
                return watchdogResult (WatchdogStatus::connectionLost);

            std::this_thread::sleep_for (std::chrono::milliseconds (1));
        }

        if (! outputProgressSeen)
        {
            WatchdogResult noProgress = watchdogResult (WatchdogStatus::unexpectedResponse);
            noProgress.runningRtLaneBacklogSeen = progress.inputSeq > progress.outputSeq;
            noProgress.runningRtLaneOutputProgressSeen = false;
            noProgress.runningRtLaneInputSeq = progress.inputSeq;
            noProgress.runningRtLaneOutputSeq = progress.outputSeq;
            return noProgress;
        }

        if (! sendMessageToWorker (makeMessage (kRunningWatchdogRtLaneHangMessage)))
            return watchdogResult (WatchdogStatus::probeSendFailed);

        if (! waitFor ([this] { return runningRtLaneHangAckSeen_ || connectionLost_; }))
            return watchdogResult (WatchdogStatus::unexpectedResponse);

        if (connectionLost())
            return watchdogResult (WatchdogStatus::connectionLost);

        auto lastProgressAt = std::chrono::steady_clock::now();
        const auto deadline = lastProgressAt + timeout_;
        while (std::chrono::steady_clock::now() < deadline)
        {
            progress = activeRtLaneProgress();
            if (progress.inputSeq <= progress.outputSeq)
            {
                publishActiveRtLaneWatchdogBlock (inputChannels.data(), channels, frames,
                                                  outputChannels.data(), channels);
                progress = activeRtLaneProgress();
            }

            if (progress.outputSeq > lastOutputSeq)
            {
                lastOutputSeq = progress.outputSeq;
                outputProgressSeen = true;
                lastProgressAt = std::chrono::steady_clock::now();
            }

            if (progress.inputSeq > progress.outputSeq)
                backlogSeen = true;

            if (backlogSeen && std::chrono::steady_clock::now() - lastProgressAt >= watchdogTimeout_)
                break;

            std::this_thread::sleep_for (std::chrono::milliseconds (1));
        }

        progress = activeRtLaneProgress();
        if (! backlogSeen || progress.inputSeq <= progress.outputSeq)
        {
            WatchdogResult noTimeout = watchdogResult (WatchdogStatus::unexpectedResponse);
            noTimeout.runningRtLaneBacklogSeen = backlogSeen;
            noTimeout.runningRtLaneOutputProgressSeen = outputProgressSeen;
            noTimeout.runningRtLaneInputSeq = progress.inputSeq;
            noTimeout.runningRtLaneOutputSeq = progress.outputSeq;
            return noTimeout;
        }

        {
            std::lock_guard<std::mutex> lock (mutex_);
            watchdogTimedOut_ = true;
            watchdogKillRequested_ = true;
            childState_ = ChildState::stopping;
        }

        killWorkerProcess();

        if (! waitFor ([this] { return connectionLost_; }))
        {
            WatchdogResult failed = watchdogResult (WatchdogStatus::killObservationTimeout);
            failed.runningRtLaneBacklogSeen = backlogSeen;
            failed.runningRtLaneOutputProgressSeen = outputProgressSeen;
            failed.runningRtLaneInputSeq = progress.inputSeq;
            failed.runningRtLaneOutputSeq = progress.outputSeq;
            return failed;
        }

        WatchdogResult killed = watchdogResult (WatchdogStatus::timeoutKilled);
        killed.runningRtLaneBacklogSeen = backlogSeen;
        killed.runningRtLaneOutputProgressSeen = outputProgressSeen;
        killed.runningRtLaneInputSeq = progress.inputSeq;
        killed.runningRtLaneOutputSeq = progress.outputSeq;
        return killed;
    }

    CrashResult launchAndExpectCrash (const juce::File& workerExecutable)
    {
        stop();
        resetState();

        {
            std::lock_guard<std::mutex> lock (mutex_);
            launchAttempted_ = true;
            childState_ = ChildState::launching;
        }

        if (! launchWorkerProcess (workerExecutable, kWorkerCommandLineId, static_cast<int> (timeout_.count())))
            return crashResult (CrashStatus::launchFailed);

        if (! waitFor ([this] { return readySeen_ || connectionLost_; }))
            return crashResult (CrashStatus::readyTimeout);

        if (connectionLost())
            return crashResult (CrashStatus::connectionLost);

        {
            std::lock_guard<std::mutex> lock (mutex_);
            childState_ = ChildState::stopping;
            crashObservationRequested_ = true;
        }

        // Tell the child to crash on cue: it terminates ITSELF (std::abort), and we never kill it.
        // The lost connection we then observe is a genuine child-side fault — the "real crash, not a
        // self-label" the close-out plan demanded (finding K). stopRequested_/watchdogKillRequested_
        // stay false, so handleConnectionLost classifies this as HostFailureKind::crash. If the child
        // somehow fails to die, we fall back to a kill so the gate fails loudly rather than hanging.
        if (! sendMessageToWorker (makeMessage (kChildCrashCommandMessage)))
        {
            killWorkerProcess();
            return crashResult (CrashStatus::observationTimeout);
        }

        // A child that vanishes on its own is detected by the coordinator's heartbeat, not instantly the
        // way a parent-side kill is — so allow up to a couple of heartbeat windows (timeout_) for the
        // lost connection to surface before giving up. If it never does, kill as a backstop so the gate
        // fails loudly rather than hanging.
        if (! waitFor (timeout_ * 2 + std::chrono::milliseconds (2000), [this] { return connectionLost_; }))
        {
            killWorkerProcess();
            return crashResult (CrashStatus::observationTimeout);
        }

        return crashResult (CrashStatus::connectionLost);
    }

    StopResult requestStopAndWait()
    {
        {
            std::lock_guard<std::mutex> lock (mutex_);
            stopRequested_ = true;
            childState_ = ChildState::stopping;
        }

        killWorkerProcess();

        if (! waitFor ([this] { return connectionLost_; }))
            return stopResult (StopStatus::stopTimeout);

        return stopResult (StopStatus::stopped);
    }

    void stop()
    {
        killWorkerProcess();
    }

    ChildStatus status() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return { childState_, handshakeStatus_, stopStatus_, watchdogStatus_, crashStatus_, failureKind_,
                 launchAttempted_, readySeen_, probeEchoed_, stopRequested_, watchdogTimedOut_,
                 watchdogKillRequested_, crashObservationRequested_, connectionLost_ };
    }

    RtLaneLoadIdentity activeRtLaneLoadIdentity() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return activeRtLaneLoadIdentity_;
    }

    bool activeRtLaneUsesOsSharedMemory() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return activeRtLane_ != nullptr && activeRtLane_->usesOsSharedMemory();
    }

    HostFailureReport hostFailureReport() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return hostFailureReportLocked();
    }

    FailureActionRequest failureActionRequest() const
    {
        return failureActionRequestFor (hostFailureReport());
    }

    BlacklistCandidateStatus blacklistCandidateStatus() const
    {
        return blacklistCandidateStatusFor (hostFailureReport());
    }

    FailureActionRequest queueFailureActionRequest (FailureActionRequest request)
    {
        std::lock_guard<std::mutex> lock (mutex_);
        pendingFailureAction_ = canCreateGraphChangeCommand (request) ? request : FailureActionRequest {};
        return pendingFailureAction_;
    }

    FailureActionRequest queueFailureActionRequestForCurrentFailure()
    {
        return queueFailureActionRequest (failureActionRequest());
    }

    FailureActionRequest pendingFailureActionRequest() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return pendingFailureAction_;
    }

    FailureActionRequest drainPendingFailureActionRequest()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        const auto request = pendingFailureAction_;
        pendingFailureAction_ = {};
        return request;
    }

    GraphChangeCommandResult drainPendingFailureActionRequestToControlCommand()
    {
        const auto request = drainPendingFailureActionRequest();
        if (! canCreateGraphChangeCommand (request))
            return {};

        return { GraphChangeCommandStatus::commandReady,
                 request,
                 { GraphChangeCommandKind::bypassAndRecompile,
                   request.failureKind,
                   request.bypassRequested,
                   request.recompileRequested },
                 true,
                 false };
    }

    PlaceholderGraphRecompileResult executePendingFailureActionRequestToPlaceholderGraph (
        yesdaw::engine::Runtime& runtime,
        yesdaw::engine::GraphBuilder::Inputs inputs,
        yesdaw::engine::NodeId offenderNodeId)
    {
        PlaceholderGraphRecompileResult result;
        result.commandResult = drainPendingFailureActionRequestToControlCommand();
        result.offenderNodeId = offenderNodeId;
        result.pendingRequestConsumed = result.commandResult.pendingRequestConsumed;

        if (result.commandResult.status != GraphChangeCommandStatus::commandReady)
            return result;

        yesdaw::engine::GraphBuildError buildError;
        std::unique_ptr<yesdaw::engine::CompiledGraph> graph =
            yesdaw::engine::GraphBuilder::build (std::move (inputs), &buildError);
        result.buildError = buildError.code();

        if (graph == nullptr)
        {
            result.status = GraphRecompileStatus::compileFailed;
            return result;
        }

        result.placeholderCompiled = compiledGraphHasPlaceholderFor (*graph, offenderNodeId);
        if (! result.placeholderCompiled)
        {
            result.status = GraphRecompileStatus::missingPlaceholder;
            return result;
        }

        result.orderedPublishAccepted = runtime.publish (std::move (graph));
        if (! result.orderedPublishAccepted)
        {
            result.status = GraphRecompileStatus::publishFailed;
            return result;
        }

        result.status = GraphRecompileStatus::graphPublished;
        result.graphRecompileExecuted = true;
        result.commandResult.graphRecompileExecuted = true;
        return result;
    }

    DeferredGraphChangeCommandStatus recordDeferredGraphChangeCommandResult (GraphChangeCommandResult result)
    {
        std::lock_guard<std::mutex> lock (mutex_);
        if (canRecordGraphChangeCommandResult (result))
        {
            lastDeferredGraphChangeCommandResult_ = result;
            deferredGraphChangeCommandRecorded_ = true;
        }
        else
        {
            lastDeferredGraphChangeCommandResult_ = {};
            deferredGraphChangeCommandRecorded_ = false;
        }

        return deferredGraphChangeCommandStatusLocked();
    }

    DeferredGraphChangeCommandStatus deferredGraphChangeCommandStatus() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return deferredGraphChangeCommandStatusLocked();
    }

    DeferredGraphChangeCommandStatus acknowledgeDeferredGraphChangeCommandStatus()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        lastDeferredGraphChangeCommandResult_ = {};
        deferredGraphChangeCommandRecorded_ = false;
        return deferredGraphChangeCommandStatusLocked();
    }

    BlacklistCandidateStatus queueBlacklistCandidate (BlacklistCandidateStatus status)
    {
        std::lock_guard<std::mutex> lock (mutex_);
        pendingBlacklistCandidate_ = canQueueBlacklistCandidate (status) ? status : BlacklistCandidateStatus {};
        return pendingBlacklistCandidate_;
    }

    BlacklistCandidateStatus queueBlacklistCandidateForCurrentFailure()
    {
        return queueBlacklistCandidate (blacklistCandidateStatus());
    }

    BlacklistCandidateStatus pendingBlacklistCandidateStatus() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return pendingBlacklistCandidate_;
    }

    BlacklistCandidateStatus drainPendingBlacklistCandidateStatus()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        const auto status = pendingBlacklistCandidate_;
        pendingBlacklistCandidate_ = {};
        return status;
    }

    BlacklistEscalationResult drainPendingBlacklistCandidateToControlEscalation()
    {
        const auto candidate = drainPendingBlacklistCandidateStatus();
        if (! canCreateBlacklistEscalation (candidate))
            return {};

        return { BlacklistEscalationStatus::escalationReady,
                 candidate,
                 { candidate.failureKind,
                   candidate.crashCandidate,
                   candidate.watchdogTimeoutCandidate,
                   true },
                 true,
                 false,
                 false };
    }

    DeferredBlacklistEscalationStatus recordDeferredBlacklistEscalationResult (BlacklistEscalationResult result)
    {
        std::lock_guard<std::mutex> lock (mutex_);
        if (canRecordBlacklistEscalationResult (result))
        {
            lastDeferredBlacklistEscalationResult_ = result;
            deferredBlacklistEscalationRecorded_ = true;
        }
        else
        {
            lastDeferredBlacklistEscalationResult_ = {};
            deferredBlacklistEscalationRecorded_ = false;
        }

        return deferredBlacklistEscalationStatusLocked();
    }

    DeferredBlacklistEscalationStatus deferredBlacklistEscalationStatus() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return deferredBlacklistEscalationStatusLocked();
    }

    DeferredBlacklistEscalationStatus acknowledgeDeferredBlacklistEscalationStatus()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        lastDeferredBlacklistEscalationResult_ = {};
        deferredBlacklistEscalationRecorded_ = false;
        return deferredBlacklistEscalationStatusLocked();
    }

    BlacklistPolicyDecisionRequest blacklistPolicyDecisionRequest() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return blacklistPolicyDecisionRequestFor (deferredBlacklistEscalationStatusLocked());
    }

    BlacklistPolicyDecisionRequest queueBlacklistPolicyDecisionRequestForDeferredEscalation()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        pendingBlacklistPolicyDecisionRequest_ =
            blacklistPolicyDecisionRequestFor (deferredBlacklistEscalationStatusLocked());
        return pendingBlacklistPolicyDecisionRequest_;
    }

    BlacklistPolicyDecisionRequest pendingBlacklistPolicyDecisionRequest() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return pendingBlacklistPolicyDecisionRequest_;
    }

    BlacklistPolicyDecisionRequest drainPendingBlacklistPolicyDecisionRequest()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        const auto request = pendingBlacklistPolicyDecisionRequest_;
        pendingBlacklistPolicyDecisionRequest_ = {};
        return request;
    }

    BlacklistPolicyDecisionCommandResult drainPendingBlacklistPolicyDecisionRequestToControlCommand()
    {
        const auto request = drainPendingBlacklistPolicyDecisionRequest();
        if (! canCreateBlacklistPolicyDecisionCommand (request))
            return {};

        return { BlacklistPolicyDecisionCommandStatus::commandReady,
                 request,
                 { BlacklistPolicyDecisionCommandKind::requestPolicyDecision,
                   request.failureKind,
                   request.crashCandidate,
                   request.watchdogTimeoutCandidate,
                   request.controlThreadPolicyDecisionRequested },
                 true,
                 false,
                 false };
    }

    DeferredBlacklistPolicyDecisionCommandStatus recordDeferredBlacklistPolicyDecisionCommandResult (
        BlacklistPolicyDecisionCommandResult result)
    {
        std::lock_guard<std::mutex> lock (mutex_);
        if (canRecordBlacklistPolicyDecisionCommandResult (result))
        {
            lastDeferredBlacklistPolicyDecisionCommandResult_ = result;
            deferredBlacklistPolicyDecisionCommandRecorded_ = true;
        }
        else
        {
            lastDeferredBlacklistPolicyDecisionCommandResult_ = {};
            deferredBlacklistPolicyDecisionCommandRecorded_ = false;
        }

        return deferredBlacklistPolicyDecisionCommandStatusLocked();
    }

    DeferredBlacklistPolicyDecisionCommandStatus deferredBlacklistPolicyDecisionCommandStatus() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return deferredBlacklistPolicyDecisionCommandStatusLocked();
    }

    DeferredBlacklistPolicyDecisionCommandStatus acknowledgeDeferredBlacklistPolicyDecisionCommandStatus()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        lastDeferredBlacklistPolicyDecisionCommandResult_ = {};
        deferredBlacklistPolicyDecisionCommandRecorded_ = false;
        return deferredBlacklistPolicyDecisionCommandStatusLocked();
    }

    BlacklistPolicyDecisionOutcome blacklistPolicyDecisionOutcomeStatus() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return blacklistPolicyDecisionOutcomeFor (deferredBlacklistPolicyDecisionCommandStatusLocked());
    }

    BlacklistPolicyDecisionOutcome queueBlacklistPolicyDecisionOutcomeForDeferredCommand()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        pendingBlacklistPolicyDecisionOutcome_ =
            blacklistPolicyDecisionOutcomeFor (deferredBlacklistPolicyDecisionCommandStatusLocked());
        return pendingBlacklistPolicyDecisionOutcome_;
    }

    BlacklistPolicyDecisionOutcome pendingBlacklistPolicyDecisionOutcomeStatus() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return pendingBlacklistPolicyDecisionOutcome_;
    }

    BlacklistPolicyDecisionOutcome drainPendingBlacklistPolicyDecisionOutcomeStatus()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        const auto outcome = pendingBlacklistPolicyDecisionOutcome_;
        pendingBlacklistPolicyDecisionOutcome_ = {};
        return outcome;
    }

    BlacklistPolicyDecisionOutcomeHandlingResult drainPendingBlacklistPolicyDecisionOutcomeToControlHandling()
    {
        const auto outcome = drainPendingBlacklistPolicyDecisionOutcomeStatus();
        if (! canCreateBlacklistPolicyDecisionOutcomeHandling (outcome))
            return {};

        return { BlacklistPolicyDecisionOutcomeHandlingStatus::handlingReady,
                 outcome,
                 { outcome.failureKind,
                   outcome.crashCandidate,
                   outcome.watchdogTimeoutCandidate,
                   true },
                 true,
                 false,
                 false };
    }

    DeferredBlacklistPolicyDecisionOutcomeHandlingStatus recordDeferredBlacklistPolicyDecisionOutcomeHandlingResult (
        BlacklistPolicyDecisionOutcomeHandlingResult result)
    {
        std::lock_guard<std::mutex> lock (mutex_);
        if (canRecordBlacklistPolicyDecisionOutcomeHandlingResult (result))
        {
            lastDeferredBlacklistPolicyDecisionOutcomeHandlingResult_ = result;
            deferredBlacklistPolicyDecisionOutcomeHandlingRecorded_ = true;
        }
        else
        {
            lastDeferredBlacklistPolicyDecisionOutcomeHandlingResult_ = {};
            deferredBlacklistPolicyDecisionOutcomeHandlingRecorded_ = false;
        }

        return deferredBlacklistPolicyDecisionOutcomeHandlingStatusLocked();
    }

    DeferredBlacklistPolicyDecisionOutcomeHandlingStatus deferredBlacklistPolicyDecisionOutcomeHandlingStatus() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return deferredBlacklistPolicyDecisionOutcomeHandlingStatusLocked();
    }

    DeferredBlacklistPolicyDecisionOutcomeHandlingStatus
    acknowledgeDeferredBlacklistPolicyDecisionOutcomeHandlingStatus()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        lastDeferredBlacklistPolicyDecisionOutcomeHandlingResult_ = {};
        deferredBlacklistPolicyDecisionOutcomeHandlingRecorded_ = false;
        return deferredBlacklistPolicyDecisionOutcomeHandlingStatusLocked();
    }

    BlacklistHandlingRequest blacklistHandlingRequest() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return blacklistHandlingRequestFor (deferredBlacklistPolicyDecisionOutcomeHandlingStatusLocked());
    }

    BlacklistHandlingRequest queueBlacklistHandlingRequestForDeferredOutcomeHandling()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        pendingBlacklistHandlingRequest_ =
            blacklistHandlingRequestFor (deferredBlacklistPolicyDecisionOutcomeHandlingStatusLocked());
        return pendingBlacklistHandlingRequest_;
    }

    BlacklistHandlingRequest pendingBlacklistHandlingRequest() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return pendingBlacklistHandlingRequest_;
    }

    BlacklistHandlingRequest drainPendingBlacklistHandlingRequest()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        const auto request = pendingBlacklistHandlingRequest_;
        pendingBlacklistHandlingRequest_ = {};
        return request;
    }

    BlacklistHandlingCommandResult drainPendingBlacklistHandlingRequestToControlCommand()
    {
        const auto request = drainPendingBlacklistHandlingRequest();
        if (! canCreateBlacklistHandlingCommand (request))
            return {};

        return { BlacklistHandlingCommandStatus::commandReady,
                 request,
                 { BlacklistHandlingCommandKind::handleBlacklistRequest,
                   request.failureKind,
                   request.crashCandidate,
                   request.watchdogTimeoutCandidate,
                   request.controlThreadBlacklistHandlingRequested },
                 true,
                 false,
                 false };
    }

    DeferredBlacklistHandlingCommandStatus recordDeferredBlacklistHandlingCommandResult (
        BlacklistHandlingCommandResult result)
    {
        std::lock_guard<std::mutex> lock (mutex_);
        if (canRecordBlacklistHandlingCommandResult (result))
        {
            lastDeferredBlacklistHandlingCommandResult_ = result;
            deferredBlacklistHandlingCommandRecorded_ = true;
        }
        else
        {
            lastDeferredBlacklistHandlingCommandResult_ = {};
            deferredBlacklistHandlingCommandRecorded_ = false;
        }

        return deferredBlacklistHandlingCommandStatusLocked();
    }

    DeferredBlacklistHandlingCommandStatus deferredBlacklistHandlingCommandStatus() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return deferredBlacklistHandlingCommandStatusLocked();
    }

    DeferredBlacklistHandlingCommandStatus acknowledgeDeferredBlacklistHandlingCommandStatus()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        lastDeferredBlacklistHandlingCommandResult_ = {};
        deferredBlacklistHandlingCommandRecorded_ = false;
        return deferredBlacklistHandlingCommandStatusLocked();
    }

    BlacklistHandlingOutcome blacklistHandlingOutcomeStatus() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return blacklistHandlingOutcomeFor (deferredBlacklistHandlingCommandStatusLocked());
    }

    BlacklistHandlingOutcome queueBlacklistHandlingOutcomeForDeferredCommand()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        pendingBlacklistHandlingOutcome_ =
            blacklistHandlingOutcomeFor (deferredBlacklistHandlingCommandStatusLocked());
        return pendingBlacklistHandlingOutcome_;
    }

    BlacklistHandlingOutcome pendingBlacklistHandlingOutcomeStatus() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return pendingBlacklistHandlingOutcome_;
    }

    BlacklistHandlingOutcome drainPendingBlacklistHandlingOutcomeStatus()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        const auto outcome = pendingBlacklistHandlingOutcome_;
        pendingBlacklistHandlingOutcome_ = {};
        return outcome;
    }

    BlacklistHandlingOutcomeHandlingResult drainPendingBlacklistHandlingOutcomeToControlHandling()
    {
        const auto outcome = drainPendingBlacklistHandlingOutcomeStatus();
        if (! canCreateBlacklistHandlingOutcomeHandling (outcome))
            return {};

        return { BlacklistHandlingOutcomeHandlingStatus::handlingReady,
                 outcome,
                 { outcome.failureKind,
                   outcome.crashCandidate,
                   outcome.watchdogTimeoutCandidate,
                   outcome.controlThreadBlacklistHandlingInspected },
                 true,
                 false,
                 false };
    }

    DeferredBlacklistHandlingOutcomeHandlingStatus recordDeferredBlacklistHandlingOutcomeHandlingResult (
        BlacklistHandlingOutcomeHandlingResult result)
    {
        std::lock_guard<std::mutex> lock (mutex_);
        if (canRecordBlacklistHandlingOutcomeHandlingResult (result))
        {
            lastDeferredBlacklistHandlingOutcomeHandlingResult_ = result;
            deferredBlacklistHandlingOutcomeHandlingRecorded_ = true;
        }
        else
        {
            lastDeferredBlacklistHandlingOutcomeHandlingResult_ = {};
            deferredBlacklistHandlingOutcomeHandlingRecorded_ = false;
        }

        return deferredBlacklistHandlingOutcomeHandlingStatusLocked();
    }

    DeferredBlacklistHandlingOutcomeHandlingStatus deferredBlacklistHandlingOutcomeHandlingStatus() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return deferredBlacklistHandlingOutcomeHandlingStatusLocked();
    }

    DeferredBlacklistHandlingOutcomeHandlingStatus acknowledgeDeferredBlacklistHandlingOutcomeHandlingStatus()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        lastDeferredBlacklistHandlingOutcomeHandlingResult_ = {};
        deferredBlacklistHandlingOutcomeHandlingRecorded_ = false;
        return deferredBlacklistHandlingOutcomeHandlingStatusLocked();
    }

    static FailureActionRequest failureActionRequestFor (HostFailureReport report) noexcept
    {
        if (report.kind == HostFailureKind::none)
            return {};

        return { FailureActionKind::bypassAndRecompile, report.kind, true, true };
    }

    static BlacklistCandidateStatus blacklistCandidateStatusFor (HostFailureReport report) noexcept
    {
        if (report.kind == HostFailureKind::crash)
            return { report.kind, true, true, false };

        if (report.kind == HostFailureKind::watchdogTimeout)
            return { report.kind, true, false, true };

        return {};
    }

    void handleMessageFromWorker (const juce::MemoryBlock& message) override
    {
        {
            std::lock_guard<std::mutex> lock (mutex_);

            RtLaneLoadReplyMessage rtLaneLoadReply;
            if (copyRtLaneLoadReplyMessage (message.getData(), message.getSize(), rtLaneLoadReply))
            {
                rtLaneLoadReplySeen_ = true;
                lastRtLaneLoadReplyStatus_ = rtLaneLoadReply.status;
                lastRtLaneLoadAttachFailure_ =
                    static_cast<yesdaw::engine::RtLaneAttachFailure> (rtLaneLoadReply.attachFailure);
                lastRtLaneLoadAttachSystemError_ = rtLaneLoadReply.attachSystemError;
                if (rtLaneLoadReply.status == RtLaneLoadReplyStatus::accepted)
                    childState_ = ChildState::running;
            }
            else
            {
                PluginStateReplyMessage pluginStateReply;
                if (copyPluginStateReplyMessage (message.getData(), message.getSize(), pluginStateReply))
                {
                    if (pluginStatePushRequestSent_)
                    {
                        lastPluginStatePushReply_ = pluginStateReply;
                        pluginStatePushReplySeen_ = true;
                    }
                    else
                    {
                        lastPluginStatePullReply_ = pluginStateReply;
                        pluginStatePullReplySeen_ = true;
                    }
                }
                else if (messageMatches (message, kWorkerReadyMessage))
                {
                    readySeen_ = true;
                    childState_ = ChildState::ready;
                }
                else if (messageMatches (message, kHandshakeProbeMessage))
                {
                    probeEchoed_ = true;
                    childState_ = ChildState::running;
                }
                else if (messageMatches (message, kRunningWatchdogRtLaneHangAckMessage))
                {
                    runningRtLaneHangAckSeen_ = true;
                }
            }
        }

        cv_.notify_all();
    }

    void handleConnectionLost() override
    {
        {
            std::lock_guard<std::mutex> lock (mutex_);
            connectionLost_ = true;
            if (watchdogKillRequested_)
                failureKind_ = HostFailureKind::watchdogTimeout;
            else if (! stopRequested_)
                failureKind_ = HostFailureKind::crash;

            if (failureKind_ != HostFailureKind::none)
                pendingFailureAction_ = failureActionRequestFor (hostFailureReportLocked());

            childState_ = stopRequested_ ? ChildState::stopped : ChildState::lost;
        }

        cv_.notify_all();
    }

private:
    static juce::MemoryBlock makeMessage (const char* text)
    {
        return juce::MemoryBlock (text, std::strlen (text) + 1);
    }

    static juce::MemoryBlock makeMessage (const RtLaneLoadMessage& message)
    {
        return juce::MemoryBlock (&message, sizeof (message));
    }

    static juce::MemoryBlock makeMessage (const PluginStateRequestMessage& message)
    {
        return juce::MemoryBlock (&message, sizeof (message));
    }

    static RtLaneLoadConfig rtLaneLoadConfigFor (yesdaw::engine::RtLaneConfig config) noexcept
    {
        return { static_cast<std::uint32_t> (std::max (1, config.channels)),
                 static_cast<std::uint32_t> (std::max (1, config.maxBlockSize)),
                 config.maxEventsPerBlock,
                 config.lastGoodHoldBlocks,
                 config.bypassAfterMisses };
    }

    RtLaneProgress activeRtLaneProgress() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        if (activeRtLane_ == nullptr)
            return {};

        return { activeRtLane_->inputSeq(), activeRtLane_->outputSeq() };
    }

    void publishActiveRtLaneWatchdogBlock (float* const* inputChannels, int inputChannelCount,
                                           int frames,
                                           float* const* outputChannels, int outputChannelCount)
    {
        std::lock_guard<std::mutex> lock (mutex_);
        if (activeRtLane_ != nullptr)
            (void) activeRtLane_->exchangeBlock (inputChannels, inputChannelCount, frames, {},
                                                 outputChannels, outputChannelCount);
    }

    RtLaneLoadResult launchAndSendRtLaneLoadIdentityInternal (
        const juce::File& workerExecutable,
        RtLaneLoadIdentity identity,
        std::unique_ptr<yesdaw::engine::RtLaneRing> ownerEndpoint)
    {
        const bool coordinatorAllocated = ownerEndpoint != nullptr;
        const bool coordinatorUsesOsSharedMemory = ownerEndpoint != nullptr && ownerEndpoint->usesOsSharedMemory();

        stop();
        resetState();

        {
            std::lock_guard<std::mutex> lock (mutex_);
            launchAttempted_ = true;
            childState_ = ChildState::launching;
            pendingRtLaneLoadIdentity_ = identity;
        }

        if (! launchWorkerProcess (workerExecutable, kWorkerCommandLineId, static_cast<int> (timeout_.count())))
            return rtLaneLoadResult (RtLaneLoadStatus::launchFailed, identity,
                                     coordinatorAllocated, coordinatorUsesOsSharedMemory);

        if (! waitFor ([this] { return readySeen_ || connectionLost_; }))
            return rtLaneLoadResult (RtLaneLoadStatus::readyTimeout, identity,
                                     coordinatorAllocated, coordinatorUsesOsSharedMemory);

        if (connectionLost())
            return rtLaneLoadResult (RtLaneLoadStatus::connectionLost, identity,
                                     coordinatorAllocated, coordinatorUsesOsSharedMemory);

        {
            std::lock_guard<std::mutex> lock (mutex_);
            childState_ = ChildState::handshaking;
        }

        const RtLaneLoadMessage loadMessage = makeRtLaneLoadMessage (identity.sharedMemoryName, identity.config);
        if (! sendMessageToWorker (makeMessage (loadMessage)))
            return rtLaneLoadResult (RtLaneLoadStatus::messageSendFailed, identity,
                                     coordinatorAllocated, coordinatorUsesOsSharedMemory);

        {
            std::lock_guard<std::mutex> lock (mutex_);
            rtLaneLoadMessageSent_ = true;
        }

        if (! waitFor ([this] { return rtLaneLoadReplySeen_ || connectionLost_; }))
            return rtLaneLoadResult (RtLaneLoadStatus::replyTimeout, identity,
                                     coordinatorAllocated, coordinatorUsesOsSharedMemory);

        if (connectionLost())
            return rtLaneLoadResult (RtLaneLoadStatus::connectionLost, identity,
                                     coordinatorAllocated, coordinatorUsesOsSharedMemory);

        bool workerAccepted = false;
        {
            std::lock_guard<std::mutex> lock (mutex_);
            workerAccepted = lastRtLaneLoadReplyStatus_ == RtLaneLoadReplyStatus::accepted;
            if (workerAccepted && ownerEndpoint != nullptr)
            {
                activeRtLane_ = std::move (ownerEndpoint);
                activeRtLaneLoadIdentity_ = identity;
            }
        }

        return rtLaneLoadResult (workerAccepted ? RtLaneLoadStatus::success : RtLaneLoadStatus::workerRejected,
                                 identity, coordinatorAllocated, coordinatorUsesOsSharedMemory);
    }

    RtLaneLoadResult rtLaneLoadResult (RtLaneLoadStatus status,
                                       RtLaneLoadIdentity identity,
                                       bool coordinatorAllocated,
                                       bool coordinatorUsesOsSharedMemory) const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return { status,
                 std::move (identity),
                 lastRtLaneLoadReplyStatus_,
                 lastRtLaneLoadAttachFailure_,
                 lastRtLaneLoadAttachSystemError_,
                 readySeen_,
                 rtLaneLoadMessageSent_,
                 rtLaneLoadReplySeen_,
                 coordinatorAllocated,
                 coordinatorUsesOsSharedMemory,
                 lastRtLaneLoadReplyStatus_ == RtLaneLoadReplyStatus::accepted };
    }

    static bool messageMatches (const juce::MemoryBlock& message, const char* text) noexcept
    {
        const auto expectedSize = std::strlen (text) + 1;
        return message.getSize() == expectedSize
            && std::memcmp (message.getData(), text, expectedSize) == 0;
    }

    static bool canCreateGraphChangeCommand (FailureActionRequest request) noexcept
    {
        return request.action == FailureActionKind::bypassAndRecompile
            && request.failureKind != HostFailureKind::none;
    }

    static bool compiledGraphHasPlaceholderFor (const yesdaw::engine::CompiledGraph& graph,
                                                yesdaw::engine::NodeId offenderNodeId) noexcept
    {
        for (const yesdaw::engine::CompiledNode& node : graph.debugCompiledNodes())
            if (node.id == offenderNodeId && node.kind == yesdaw::engine::CompiledNodeKind::Placeholder)
                return true;

        return false;
    }

    static bool canRecordGraphChangeCommandResult (GraphChangeCommandResult result) noexcept
    {
        return result.status == GraphChangeCommandStatus::commandReady
            && result.pendingRequestConsumed
            && ! result.graphRecompileExecuted
            && canCreateGraphChangeCommand (result.drainedRequest)
            && result.command.command == GraphChangeCommandKind::bypassAndRecompile
            && result.command.failureKind == result.drainedRequest.failureKind
            && result.command.bypassRequested == result.drainedRequest.bypassRequested
            && result.command.recompileRequested == result.drainedRequest.recompileRequested;
    }

    static bool canQueueBlacklistCandidate (BlacklistCandidateStatus status) noexcept
    {
        if (! status.candidate)
            return false;

        return blacklistCandidateMatches (status, blacklistCandidateStatusFor ({ status.failureKind, true, false, false }));
    }

    static bool canCreateBlacklistEscalation (BlacklistCandidateStatus status) noexcept
    {
        return canQueueBlacklistCandidate (status);
    }

    static bool canRecordBlacklistEscalationResult (BlacklistEscalationResult result) noexcept
    {
        return result.status == BlacklistEscalationStatus::escalationReady
            && result.pendingCandidateConsumed
            && ! result.blacklistPolicyApplied
            && ! result.blacklistStatePersisted
            && canCreateBlacklistEscalation (result.drainedCandidate)
            && result.escalation.controlThreadEscalationRequested
            && result.escalation.failureKind == result.drainedCandidate.failureKind
            && result.escalation.crashCandidate == result.drainedCandidate.crashCandidate
            && result.escalation.watchdogTimeoutCandidate == result.drainedCandidate.watchdogTimeoutCandidate;
    }

    static BlacklistPolicyDecisionRequest blacklistPolicyDecisionRequestFor (
        DeferredBlacklistEscalationStatus status) noexcept
    {
        if (! canCreateBlacklistPolicyDecisionRequest (status))
            return {};

        const auto escalation = status.lastResult.escalation;
        return { BlacklistPolicyDecisionRequestStatus::requestReady,
                 escalation.failureKind,
                 escalation.crashCandidate,
                 escalation.watchdogTimeoutCandidate,
                 true,
                 false,
                 false };
    }

    static bool canCreateBlacklistPolicyDecisionRequest (DeferredBlacklistEscalationStatus status) noexcept
    {
        return status.status == BlacklistEscalationStatus::escalationReady
            && status.escalationRecorded
            && ! status.blacklistPolicyApplied
            && ! status.blacklistStatePersisted
            && canRecordBlacklistEscalationResult (status.lastResult);
    }

    static bool canCreateBlacklistPolicyDecisionCommand (BlacklistPolicyDecisionRequest request) noexcept
    {
        if (request.status != BlacklistPolicyDecisionRequestStatus::requestReady
            || request.failureKind == HostFailureKind::none
            || ! request.controlThreadPolicyDecisionRequested
            || request.blacklistPolicyApplied
            || request.blacklistStatePersisted)
            return false;

        if (request.failureKind == HostFailureKind::crash)
            return request.crashCandidate && ! request.watchdogTimeoutCandidate;

        if (request.failureKind == HostFailureKind::watchdogTimeout)
            return ! request.crashCandidate && request.watchdogTimeoutCandidate;

        return false;
    }

    static bool canRecordBlacklistPolicyDecisionCommandResult (
        BlacklistPolicyDecisionCommandResult result) noexcept
    {
        return result.status == BlacklistPolicyDecisionCommandStatus::commandReady
            && result.pendingRequestConsumed
            && ! result.blacklistPolicyApplied
            && ! result.blacklistStatePersisted
            && canCreateBlacklistPolicyDecisionCommand (result.drainedRequest)
            && result.command.command == BlacklistPolicyDecisionCommandKind::requestPolicyDecision
            && result.command.failureKind == result.drainedRequest.failureKind
            && result.command.crashCandidate == result.drainedRequest.crashCandidate
            && result.command.watchdogTimeoutCandidate == result.drainedRequest.watchdogTimeoutCandidate
            && result.command.controlThreadPolicyDecisionRequested
                == result.drainedRequest.controlThreadPolicyDecisionRequested;
    }

    static BlacklistPolicyDecisionOutcome blacklistPolicyDecisionOutcomeFor (
        DeferredBlacklistPolicyDecisionCommandStatus status) noexcept
    {
        if (! canCreateBlacklistPolicyDecisionOutcome (status))
            return {};

        const auto command = status.lastResult.command;
        return { BlacklistPolicyDecisionOutcomeStatus::outcomeReady,
                 command.failureKind,
                 command.crashCandidate,
                 command.watchdogTimeoutCandidate,
                 true,
                 false,
                 false };
    }

    static bool canCreateBlacklistPolicyDecisionOutcome (
        DeferredBlacklistPolicyDecisionCommandStatus status) noexcept
    {
        return status.status == BlacklistPolicyDecisionCommandStatus::commandReady
            && status.commandRecorded
            && ! status.blacklistPolicyApplied
            && ! status.blacklistStatePersisted
            && canRecordBlacklistPolicyDecisionCommandResult (status.lastResult);
    }

    static bool canCreateBlacklistPolicyDecisionOutcomeHandling (
        BlacklistPolicyDecisionOutcome outcome) noexcept
    {
        if (outcome.status != BlacklistPolicyDecisionOutcomeStatus::outcomeReady
            || outcome.failureKind == HostFailureKind::none
            || ! outcome.controlThreadPolicyDecisionInspected
            || outcome.blacklistPolicyApplied
            || outcome.blacklistStatePersisted)
            return false;

        if (outcome.failureKind == HostFailureKind::crash)
            return outcome.crashCandidate && ! outcome.watchdogTimeoutCandidate;

        if (outcome.failureKind == HostFailureKind::watchdogTimeout)
            return ! outcome.crashCandidate && outcome.watchdogTimeoutCandidate;

        return false;
    }

    static bool canRecordBlacklistPolicyDecisionOutcomeHandlingResult (
        BlacklistPolicyDecisionOutcomeHandlingResult result) noexcept
    {
        return result.status == BlacklistPolicyDecisionOutcomeHandlingStatus::handlingReady
            && result.pendingOutcomeConsumed
            && ! result.blacklistPolicyApplied
            && ! result.blacklistStatePersisted
            && canCreateBlacklistPolicyDecisionOutcomeHandling (result.drainedOutcome)
            && result.handling.controlThreadBlacklistHandlingRequested
            && result.handling.failureKind == result.drainedOutcome.failureKind
            && result.handling.crashCandidate == result.drainedOutcome.crashCandidate
            && result.handling.watchdogTimeoutCandidate == result.drainedOutcome.watchdogTimeoutCandidate;
    }

    static BlacklistHandlingRequest blacklistHandlingRequestFor (
        DeferredBlacklistPolicyDecisionOutcomeHandlingStatus status) noexcept
    {
        if (! canCreateBlacklistHandlingRequest (status))
            return {};

        const auto handling = status.lastResult.handling;
        return { BlacklistHandlingRequestStatus::requestReady,
                 handling.failureKind,
                 handling.crashCandidate,
                 handling.watchdogTimeoutCandidate,
                 handling.controlThreadBlacklistHandlingRequested,
                 false,
                 false };
    }

    static bool canCreateBlacklistHandlingRequest (
        DeferredBlacklistPolicyDecisionOutcomeHandlingStatus status) noexcept
    {
        return status.status == BlacklistPolicyDecisionOutcomeHandlingStatus::handlingReady
            && status.handlingRecorded
            && ! status.blacklistPolicyApplied
            && ! status.blacklistStatePersisted
            && canRecordBlacklistPolicyDecisionOutcomeHandlingResult (status.lastResult);
    }

    static bool canCreateBlacklistHandlingCommand (BlacklistHandlingRequest request) noexcept
    {
        if (request.status != BlacklistHandlingRequestStatus::requestReady
            || request.failureKind == HostFailureKind::none
            || ! request.controlThreadBlacklistHandlingRequested
            || request.blacklistPolicyApplied
            || request.blacklistStatePersisted)
            return false;

        if (request.failureKind == HostFailureKind::crash)
            return request.crashCandidate && ! request.watchdogTimeoutCandidate;

        if (request.failureKind == HostFailureKind::watchdogTimeout)
            return ! request.crashCandidate && request.watchdogTimeoutCandidate;

        return false;
    }

    static bool canRecordBlacklistHandlingCommandResult (BlacklistHandlingCommandResult result) noexcept
    {
        return result.status == BlacklistHandlingCommandStatus::commandReady
            && result.pendingRequestConsumed
            && ! result.blacklistPolicyApplied
            && ! result.blacklistStatePersisted
            && canCreateBlacklistHandlingCommand (result.drainedRequest)
            && result.command.command == BlacklistHandlingCommandKind::handleBlacklistRequest
            && result.command.failureKind == result.drainedRequest.failureKind
            && result.command.crashCandidate == result.drainedRequest.crashCandidate
            && result.command.watchdogTimeoutCandidate == result.drainedRequest.watchdogTimeoutCandidate
            && result.command.controlThreadBlacklistHandlingRequested
                == result.drainedRequest.controlThreadBlacklistHandlingRequested;
    }

    static BlacklistHandlingOutcome blacklistHandlingOutcomeFor (
        DeferredBlacklistHandlingCommandStatus status) noexcept
    {
        if (! canCreateBlacklistHandlingOutcome (status))
            return {};

        const auto command = status.lastResult.command;
        return { BlacklistHandlingOutcomeStatus::outcomeReady,
                 command.failureKind,
                 command.crashCandidate,
                 command.watchdogTimeoutCandidate,
                 true,
                 false,
                 false };
    }

    static bool canCreateBlacklistHandlingOutcome (
        DeferredBlacklistHandlingCommandStatus status) noexcept
    {
        return status.status == BlacklistHandlingCommandStatus::commandReady
            && status.commandRecorded
            && ! status.blacklistPolicyApplied
            && ! status.blacklistStatePersisted
            && canRecordBlacklistHandlingCommandResult (status.lastResult);
    }

    static bool canCreateBlacklistHandlingOutcomeHandling (BlacklistHandlingOutcome outcome) noexcept
    {
        if (outcome.status != BlacklistHandlingOutcomeStatus::outcomeReady
            || outcome.failureKind == HostFailureKind::none
            || ! outcome.controlThreadBlacklistHandlingInspected
            || outcome.blacklistPolicyApplied
            || outcome.blacklistStatePersisted)
            return false;

        if (outcome.failureKind == HostFailureKind::crash)
            return outcome.crashCandidate && ! outcome.watchdogTimeoutCandidate;

        if (outcome.failureKind == HostFailureKind::watchdogTimeout)
            return ! outcome.crashCandidate && outcome.watchdogTimeoutCandidate;

        return false;
    }

    static bool canRecordBlacklistHandlingOutcomeHandlingResult (
        BlacklistHandlingOutcomeHandlingResult result) noexcept
    {
        return result.status == BlacklistHandlingOutcomeHandlingStatus::handlingReady
            && result.pendingOutcomeConsumed
            && ! result.blacklistPolicyApplied
            && ! result.blacklistStatePersisted
            && canCreateBlacklistHandlingOutcomeHandling (result.drainedOutcome)
            && result.handling.controlThreadBlacklistHandlingRequested
            && result.handling.failureKind == result.drainedOutcome.failureKind
            && result.handling.crashCandidate == result.drainedOutcome.crashCandidate
            && result.handling.watchdogTimeoutCandidate == result.drainedOutcome.watchdogTimeoutCandidate;
    }

    static bool blacklistCandidateMatches (BlacklistCandidateStatus actual,
                                           BlacklistCandidateStatus expected) noexcept
    {
        return actual.failureKind == expected.failureKind
            && actual.candidate == expected.candidate
            && actual.crashCandidate == expected.crashCandidate
            && actual.watchdogTimeoutCandidate == expected.watchdogTimeoutCandidate;
    }

    template <typename Predicate>
    bool waitFor (Predicate&& predicate)
    {
        return waitFor (timeout_, std::forward<Predicate> (predicate));
    }

    template <typename Predicate>
    bool waitFor (std::chrono::milliseconds timeout, Predicate&& predicate)
    {
        std::unique_lock<std::mutex> lock (mutex_);
        return cv_.wait_for (lock, timeout, std::forward<Predicate> (predicate));
    }

    void resetState()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        readySeen_ = false;
        probeEchoed_ = false;
        connectionLost_ = false;
        launchAttempted_ = false;
        stopRequested_ = false;
        watchdogTimedOut_ = false;
        watchdogKillRequested_ = false;
        crashObservationRequested_ = false;
        childState_ = ChildState::idle;
        handshakeStatus_ = HandshakeStatus::notStarted;
        stopStatus_ = StopStatus::notStarted;
        watchdogStatus_ = WatchdogStatus::notStarted;
        crashStatus_ = CrashStatus::notStarted;
        failureKind_ = HostFailureKind::none;
        pendingFailureAction_ = {};
        pendingBlacklistCandidate_ = {};
        pendingBlacklistPolicyDecisionRequest_ = {};
        pendingBlacklistPolicyDecisionOutcome_ = {};
        pendingBlacklistHandlingRequest_ = {};
        pendingBlacklistHandlingOutcome_ = {};
        lastDeferredGraphChangeCommandResult_ = {};
        lastDeferredBlacklistEscalationResult_ = {};
        lastDeferredBlacklistPolicyDecisionCommandResult_ = {};
        lastDeferredBlacklistPolicyDecisionOutcomeHandlingResult_ = {};
        lastDeferredBlacklistHandlingCommandResult_ = {};
        lastDeferredBlacklistHandlingOutcomeHandlingResult_ = {};
        deferredGraphChangeCommandRecorded_ = false;
        deferredBlacklistEscalationRecorded_ = false;
        deferredBlacklistPolicyDecisionCommandRecorded_ = false;
        deferredBlacklistPolicyDecisionOutcomeHandlingRecorded_ = false;
        deferredBlacklistHandlingCommandRecorded_ = false;
        deferredBlacklistHandlingOutcomeHandlingRecorded_ = false;
        activeRtLane_.reset();
        activeRtLaneLoadIdentity_ = {};
        pendingRtLaneLoadIdentity_ = {};
        rtLaneLoadMessageSent_ = false;
        rtLaneLoadReplySeen_ = false;
        runningRtLaneHangAckSeen_ = false;
        lastRtLaneLoadReplyStatus_ = RtLaneLoadReplyStatus::none;
        lastRtLaneLoadAttachFailure_ = yesdaw::engine::RtLaneAttachFailure::None;
        lastRtLaneLoadAttachSystemError_ = 0;
        pluginStatePullRequestSent_ = false;
        pluginStatePullReplySeen_ = false;
        pluginStateCorruptPushRequestSent_ = false;
        pluginStatePushRequestSent_ = false;
        pluginStatePushReplySeen_ = false;
        lastPluginStatePullReply_ = {};
        lastPluginStatePushReply_ = {};
    }

    bool connectionLost() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return connectionLost_;
    }

    HandshakeResult result (HandshakeStatus status)
    {
        std::lock_guard<std::mutex> lock (mutex_);
        handshakeStatus_ = status;
        if (status == HandshakeStatus::success)
            childState_ = ChildState::running;
        return { status, readySeen_, probeEchoed_ };
    }

    StopResult stopResult (StopStatus status)
    {
        std::lock_guard<std::mutex> lock (mutex_);
        stopStatus_ = status;
        return { status, connectionLost_ };
    }

    WatchdogResult watchdogResult (WatchdogStatus status)
    {
        std::lock_guard<std::mutex> lock (mutex_);
        watchdogStatus_ = status;
        if (status == WatchdogStatus::timeoutKilled)
            childState_ = ChildState::stopped;
        return { status, readySeen_, watchdogTimedOut_, watchdogKillRequested_, connectionLost_ };
    }

    CrashResult crashResult (CrashStatus status)
    {
        std::lock_guard<std::mutex> lock (mutex_);
        crashStatus_ = status;
        return { status, readySeen_, crashObservationRequested_, connectionLost_, failureKind_ };
    }

    HostFailureReport hostFailureReportLocked() const
    {
        return { failureKind_, connectionLost_, watchdogTimedOut_, watchdogKillRequested_ };
    }

    DeferredGraphChangeCommandStatus deferredGraphChangeCommandStatusLocked() const
    {
        return { deferredGraphChangeCommandRecorded_ ? lastDeferredGraphChangeCommandResult_.status
                                                     : GraphChangeCommandStatus::noAction,
                 lastDeferredGraphChangeCommandResult_,
                 deferredGraphChangeCommandRecorded_,
                 lastDeferredGraphChangeCommandResult_.graphRecompileExecuted };
    }

    DeferredBlacklistEscalationStatus deferredBlacklistEscalationStatusLocked() const
    {
        return { deferredBlacklistEscalationRecorded_ ? lastDeferredBlacklistEscalationResult_.status
                                                      : BlacklistEscalationStatus::noAction,
                 lastDeferredBlacklistEscalationResult_,
                 deferredBlacklistEscalationRecorded_,
                 lastDeferredBlacklistEscalationResult_.blacklistPolicyApplied,
                 lastDeferredBlacklistEscalationResult_.blacklistStatePersisted };
    }

    DeferredBlacklistPolicyDecisionCommandStatus deferredBlacklistPolicyDecisionCommandStatusLocked() const
    {
        return { deferredBlacklistPolicyDecisionCommandRecorded_
                     ? lastDeferredBlacklistPolicyDecisionCommandResult_.status
                     : BlacklistPolicyDecisionCommandStatus::noAction,
                 lastDeferredBlacklistPolicyDecisionCommandResult_,
                 deferredBlacklistPolicyDecisionCommandRecorded_,
                 lastDeferredBlacklistPolicyDecisionCommandResult_.blacklistPolicyApplied,
                 lastDeferredBlacklistPolicyDecisionCommandResult_.blacklistStatePersisted };
    }

    DeferredBlacklistPolicyDecisionOutcomeHandlingStatus
    deferredBlacklistPolicyDecisionOutcomeHandlingStatusLocked() const
    {
        return { deferredBlacklistPolicyDecisionOutcomeHandlingRecorded_
                     ? lastDeferredBlacklistPolicyDecisionOutcomeHandlingResult_.status
                     : BlacklistPolicyDecisionOutcomeHandlingStatus::noAction,
                 lastDeferredBlacklistPolicyDecisionOutcomeHandlingResult_,
                 deferredBlacklistPolicyDecisionOutcomeHandlingRecorded_,
                 lastDeferredBlacklistPolicyDecisionOutcomeHandlingResult_.blacklistPolicyApplied,
                 lastDeferredBlacklistPolicyDecisionOutcomeHandlingResult_.blacklistStatePersisted };
    }

    DeferredBlacklistHandlingCommandStatus deferredBlacklistHandlingCommandStatusLocked() const
    {
        return { deferredBlacklistHandlingCommandRecorded_
                     ? lastDeferredBlacklistHandlingCommandResult_.status
                     : BlacklistHandlingCommandStatus::noAction,
                 lastDeferredBlacklistHandlingCommandResult_,
                 deferredBlacklistHandlingCommandRecorded_,
                 lastDeferredBlacklistHandlingCommandResult_.blacklistPolicyApplied,
                 lastDeferredBlacklistHandlingCommandResult_.blacklistStatePersisted };
    }

    DeferredBlacklistHandlingOutcomeHandlingStatus
    deferredBlacklistHandlingOutcomeHandlingStatusLocked() const
    {
        return { deferredBlacklistHandlingOutcomeHandlingRecorded_
                     ? lastDeferredBlacklistHandlingOutcomeHandlingResult_.status
                     : BlacklistHandlingOutcomeHandlingStatus::noAction,
                 lastDeferredBlacklistHandlingOutcomeHandlingResult_,
                 deferredBlacklistHandlingOutcomeHandlingRecorded_,
                 lastDeferredBlacklistHandlingOutcomeHandlingResult_.blacklistPolicyApplied,
                 lastDeferredBlacklistHandlingOutcomeHandlingResult_.blacklistStatePersisted };
    }

    const std::chrono::milliseconds timeout_;
    const std::chrono::milliseconds watchdogTimeout_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    ChildState childState_ { ChildState::idle };
    HandshakeStatus handshakeStatus_ { HandshakeStatus::notStarted };
    StopStatus stopStatus_ { StopStatus::notStarted };
    WatchdogStatus watchdogStatus_ { WatchdogStatus::notStarted };
    CrashStatus crashStatus_ { CrashStatus::notStarted };
    HostFailureKind failureKind_ { HostFailureKind::none };
    FailureActionRequest pendingFailureAction_;
    BlacklistCandidateStatus pendingBlacklistCandidate_;
    BlacklistPolicyDecisionRequest pendingBlacklistPolicyDecisionRequest_;
    BlacklistPolicyDecisionOutcome pendingBlacklistPolicyDecisionOutcome_;
    BlacklistHandlingRequest pendingBlacklistHandlingRequest_;
    BlacklistHandlingOutcome pendingBlacklistHandlingOutcome_;
    GraphChangeCommandResult lastDeferredGraphChangeCommandResult_;
    BlacklistEscalationResult lastDeferredBlacklistEscalationResult_;
    BlacklistPolicyDecisionCommandResult lastDeferredBlacklistPolicyDecisionCommandResult_;
    BlacklistPolicyDecisionOutcomeHandlingResult lastDeferredBlacklistPolicyDecisionOutcomeHandlingResult_;
    BlacklistHandlingCommandResult lastDeferredBlacklistHandlingCommandResult_;
    BlacklistHandlingOutcomeHandlingResult lastDeferredBlacklistHandlingOutcomeHandlingResult_;
    std::unique_ptr<yesdaw::engine::RtLaneRing> activeRtLane_;
    RtLaneLoadIdentity activeRtLaneLoadIdentity_;
    RtLaneLoadIdentity pendingRtLaneLoadIdentity_;
    bool launchAttempted_ { false };
    bool readySeen_ { false };
    bool probeEchoed_ { false };
    bool stopRequested_ { false };
    bool watchdogTimedOut_ { false };
    bool watchdogKillRequested_ { false };
    bool crashObservationRequested_ { false };
    bool connectionLost_ { false };
    bool deferredGraphChangeCommandRecorded_ { false };
    bool deferredBlacklistEscalationRecorded_ { false };
    bool deferredBlacklistPolicyDecisionCommandRecorded_ { false };
    bool deferredBlacklistPolicyDecisionOutcomeHandlingRecorded_ { false };
    bool deferredBlacklistHandlingCommandRecorded_ { false };
    bool deferredBlacklistHandlingOutcomeHandlingRecorded_ { false };
    bool rtLaneLoadMessageSent_ { false };
    bool rtLaneLoadReplySeen_ { false };
    bool runningRtLaneHangAckSeen_ { false };
    bool pluginStatePullRequestSent_ { false };
    bool pluginStatePullReplySeen_ { false };
    bool pluginStateCorruptPushRequestSent_ { false };
    bool pluginStatePushRequestSent_ { false };
    bool pluginStatePushReplySeen_ { false };
    RtLaneLoadReplyStatus lastRtLaneLoadReplyStatus_ { RtLaneLoadReplyStatus::none };
    yesdaw::engine::RtLaneAttachFailure lastRtLaneLoadAttachFailure_ {
        yesdaw::engine::RtLaneAttachFailure::None
    };
    int lastRtLaneLoadAttachSystemError_ { 0 };
    PluginStateReplyMessage lastPluginStatePullReply_;
    PluginStateReplyMessage lastPluginStatePushReply_;
};

} // namespace yesdaw::plugin_host
