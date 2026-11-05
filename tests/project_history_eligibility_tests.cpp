// Whole-history eligibility: independent Control-thread feature smoke.
#include "engine/ProjectUndo.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <memory>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace history_allocation_probe {
struct Counts { std::size_t attempts = 0, failures = 0; };
thread_local bool active = false;
thread_local std::size_t failAt = (std::numeric_limits<std::size_t>::max)();
thread_local Counts counts;

void observe()
{
    if (! active) return;
    const auto ordinal = counts.attempts++;
    if (ordinal == failAt)
    {
        ++counts.failures;
        failAt = (std::numeric_limits<std::size_t>::max)();
        throw std::bad_alloc {};
    }
}

class Scope final
{
public:
    explicit Scope (std::size_t failure = (std::numeric_limits<std::size_t>::max)()) noexcept
    {
        counts = {};
        failAt = failure;
        active = true;
    }
    Scope (const Scope&) = delete;
    Scope& operator= (const Scope&) = delete;
    ~Scope() { active = false; }
    Counts finish() noexcept { active = false; return counts; }
};
}

#if defined (YESDAW_HISTORY_WRAP_NEW)
extern "C" void* __real__Znwm (std::size_t);
extern "C" void* __wrap__Znwm (std::size_t bytes)
{
    history_allocation_probe::observe();
    return __real__Znwm (bytes);
}
#else
void* operator new (std::size_t bytes)
{
    history_allocation_probe::observe();
    for (;;)
    {
        if (void* p = std::malloc (bytes == 0 ? 1 : bytes)) return p;
        if (const auto handler = std::get_new_handler()) handler();
        else throw std::bad_alloc {};
    }
}
void operator delete (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
#endif

namespace {
namespace e = yesdaw::engine;
using Stack = e::ProjectUndoStack;
using Token = Stack::HistoryToken;
using Intent = Stack::HistoryNavigation;
using Capture = Stack::HistoryCaptureStatus;
using Navigation = Stack::HistoryNavigationStatus;
using Verb = e::ProjectEditVerb;
using Probe = history_allocation_probe::Scope;
using Counts = history_allocation_probe::Counts;

constexpr e::EntityId id (std::uint64_t n) noexcept
{
    return e::EntityId::fromBigEndianParts (0x484953544f525900ull, n);
}

e::MidiClip midi (std::uint64_t first)
{
    e::MidiClip c;
    c.id = id (first);
    c.trackId = id (4);
    c.timelineStart = 3 * e::kTicksPerQuarter;
    c.timelineLength = e::kTicksPerQuarter;
    e::Note n;
    n.id = id (first + 1);
    n.startTick = 64;
    n.lengthTicks = 256;
    n.key = 67;
    n.pitchNote = 67.25;
    n.normalizedVelocity = 0.625;
    n.portIndex = 2;
    n.channel = 3;
    c.notes = { n };
    return c;
}

e::Project project()
{
    e::Project p;
    p.id = id (1);
    p.sampleRate = { 48000.0 };
    e::Asset a;
    a.id = id (2);
    a.frames = 4096;
    a.sampleRate = { 48000.0 };
    a.channels = 2;
    a.contentHash.bytes[0] = 0x53;
    p.assets = { a };
    e::Track voice, notes;
    voice.id = id (3);
    voice.strip.name = "Voice";
    voice.strip.linearGain = 0.875f;
    voice.strip.pan = -0.125f;
    notes.id = id (4);
    notes.strip.name = "Notes";
    p.tracks = { voice, notes };
    e::Clip first, second;
    first.id = id (5);
    first.assetId = a.id;
    first.trackId = voice.id;
    first.timelineStart = 120;
    first.timelineLength = 512;
    first.srcOffset = 11;
    first.srcLen = 512;
    first.fadeIn = 3;
    first.fadeOut = 5;
    second.id = id (6);
    second.assetId = a.id;
    second.trackId = voice.id;
    second.timelineStart = 2048;
    second.timelineLength = 256;
    second.srcOffset = 800;
    second.srcLen = 256;
    second.gain = 0.75f;
    second.reversed = true;
    p.clips = { first, second };
    p.tempoMap = { { 0, 120.0 }, { 4 * e::kTicksPerQuarter, 132.0 } };
    p.meterMap = { { 0, 4, 4 } };
    p.markers = { { id (20), 2 * e::kTicksPerQuarter, "cue" } };
    p.locatePoints[0] = e::kTicksPerQuarter;
    p.midiClips = { midi (30) };
    e::AutomationLaneData lane;
    lane.id = id (7);
    lane.ownerEntity = voice.id;
    lane.role = e::AutomationTargetRole::TrackFader;
    lane.paramId = e::FaderNode::kGainParameterId;
    lane.points = { { 0, 0.2 }, { 2048, 0.8, e::AutomationCurveType::Hold } };
    p.automationLanes = { lane };
    return p;
}

bool valuesMatch (const e::Project& a, const e::Project& b)
{
    const auto rows = [] (const e::Project& p) {
        return std::tie (p.id, p.sampleRate, p.assets, p.tracks, p.buses, p.clips,
                         p.tempoMap, p.meterMap, p.markers, p.locatePoints, p.midiClips,
                         p.recordingTakes, p.recordingCompSegments, p.automationLanes,
                         p.masterLinearGain, p.masterStrip, p.automationMode,
                         p.automationFollowsClips, p.punchRegion, p.loopRegion, p.scale);
    };
    return rows (a) == rows (b);
}

void projectIs (const e::Project& actual, const e::Project& expected)
{
    CHECK (actual.clips == expected.clips);
    CHECK (actual.tracks == expected.tracks);
    CHECK (actual.midiClips == expected.midiClips);
    CHECK (actual.automationLanes == expected.automationLanes);
    CHECK (valuesMatch (actual, expected));
}

Token token (const Stack& s)
{
    Token result;
    REQUIRE (s.captureHistory (result));
    REQUIRE (result.valid());
    REQUIRE (s.historyMatches (result));
    return result;
}

void historyIs (const Stack& s, std::size_t cursor,
                std::initializer_list<std::pair<Verb, std::size_t>> expected)
{
    const auto view = s.history();
    REQUIRE (view.current == cursor);
    REQUIRE (view.steps.size() == expected.size());
    std::size_t i = 0;
    for (const auto& step : expected)
    {
        CHECK (view.steps[i].verb == step.first);
        CHECK (view.steps[i].entryCount == step.second);
        ++i;
    }
}

e::ProjectEditCommand mix (float gain)
{
    return e::ProjectEditCommand::setTrackMixScalars (id (3), gain, -0.125f, false, false, false);
}

static_assert (noexcept (std::declval<Stack&>().beginTransactionGroup()));
static_assert (noexcept (std::declval<Stack&>().endTransactionGroup()));
static_assert (noexcept (std::declval<Stack&>().beginScalarCoalescing()));
static_assert (noexcept (std::declval<Stack&>().endScalarCoalescing()));
static_assert (noexcept (std::declval<const Stack&>().captureHistory (std::declval<Token&>())));
static_assert (noexcept (std::declval<const Stack&>().historyMatches (std::declval<const Token&>())));
static_assert (noexcept (std::declval<const Stack&>().captureUndo (std::declval<Intent&>())));
static_assert (noexcept (std::declval<const Stack&>().captureRedo (std::declval<Intent&>())));
static_assert (noexcept (std::declval<const Stack&>().captureHistoryJump (0, std::declval<Intent&>())));
static_assert (std::is_nothrow_move_constructible_v<e::Project>);
static_assert (std::is_nothrow_move_assignable_v<e::Project>);
static_assert (std::is_nothrow_move_constructible_v<e::ProjectEditTransaction>);
static_assert (std::is_nothrow_move_assignable_v<e::ProjectEditTransaction>);
static_assert (std::is_nothrow_move_constructible_v<Stack>);
static_assert (std::is_nothrow_move_assignable_v<Stack>);
}

TEST_CASE ("Whole history eligibility binds grouped and coalesced navigation to current state",
           "[project][history-eligibility]")
{
    SECTION ("capture_and_read_stability")
    {
        Stack s;
        Token absent;
        CHECK_FALSE (absent.valid());
        CHECK_FALSE (s.historyMatches (absent));
        const auto empty = token (s);
        const auto again = token (s);
        CHECK (s.historyMatches (empty));
        CHECK (s.historyMatches (again));
        historyIs (s, 0, {});
        CHECK_FALSE (s.canUndo());
        CHECK_FALSE (s.canRedo());
        CHECK (s.nextUndo() == nullptr);
        CHECK (s.nextRedo() == nullptr);
        CHECK (s.historyMatches (empty));
        auto p = project();
        REQUIRE (p.hasValidAssetClipIndirection());
        REQUIRE (s.apply (p, e::ProjectEditCommand::moveClip (id (5), 384)).applied());
        CHECK_FALSE (s.historyMatches (empty));
        const auto edited = token (s);
        historyIs (s, 1, { { Verb::MoveClip, 1 } });
        CHECK (s.nextUndo()->diff.after.front() == p.clips.front());
        CHECK (s.historyMatches (edited));
    }

    SECTION ("same_depth_scalar_coalescing")
    {
        auto p = project();
        Stack s;
        s.beginScalarCoalescing();
        REQUIRE (s.apply (p, mix (0.625f)).applied());
        const auto first = token (s);
        const auto depth = s.undoDepth();
        REQUIRE (s.nextUndo() != nullptr);
        const auto verb = s.nextUndo()->command.verb;
        const auto changed = s.apply (p, mix (0.375f));
        REQUIRE (changed.applied());
        CHECK (changed.coalesced);
        CHECK (s.undoDepth() == depth);
        CHECK (s.nextUndo()->command.verb == verb);
        CHECK_FALSE (s.historyMatches (first));
        Intent blocked;
        CHECK (s.captureUndo (blocked) == Capture::GestureOpen);
        CHECK_FALSE (blocked.valid());
        const auto open = token (s);
        s.endScalarCoalescing();
        CHECK_FALSE (s.historyMatches (open));
        REQUIRE (s.captureUndo (blocked) == Capture::Captured);
        REQUIRE (s.navigate (p, blocked) == Navigation::Applied);
        projectIs (p, project());
        historyIs (s, 0, { { Verb::SetTrackMixScalars, 1 } });
    }

    SECTION ("whole_group_and_nonfinal_diff")
    {
        auto p = project();
        const auto before = p;
        Stack s;
        REQUIRE (s.beginTransactionGroup());
        REQUIRE (s.apply (p, e::ProjectEditCommand::moveClip (id (5), 384)).applied());
        REQUIRE (s.apply (p, e::ProjectEditCommand::setClipGain (id (6), 0.25f)).applied());
        REQUIRE (s.endTransactionGroup());
        auto edited = before;
        edited.clips[0].timelineStart = 384;
        edited.clips[1].gain = 0.25f;
        projectIs (p, edited);
        historyIs (s, 1, { { Verb::SetClipGain, 2 } });
        Intent undo;
        REQUIRE (s.captureUndo (undo) == Capture::Captured);
        const auto current = token (s);
        auto mismatched = p;
        mismatched.clips[0].timelineStart = 385;
        const auto damaged = mismatched;
        CHECK (s.navigate (mismatched, undo) == Navigation::ProjectMismatch);
        projectIs (mismatched, damaged);
        CHECK (s.historyMatches (current));
        historyIs (s, 1, { { Verb::SetClipGain, 2 } });
        REQUIRE (s.navigate (p, undo) == Navigation::Applied);
        projectIs (p, before);
        CHECK_FALSE (s.historyMatches (current));
        CHECK (s.redoDepth() == 2);
        Intent redo;
        REQUIRE (s.captureRedo (redo) == Capture::Captured);
        REQUIRE (s.navigate (p, redo) == Navigation::Applied);
        projectIs (p, edited);
    }

    SECTION ("group_boundaries_and_noops")
    {
        Stack s;
        auto prior = token (s);
        REQUIRE (s.beginTransactionGroup());
        CHECK_FALSE (s.historyMatches (prior));
        prior = token (s);
        CHECK_FALSE (s.beginTransactionGroup());
        CHECK (s.historyMatches (prior));
        REQUIRE (s.endTransactionGroup());
        CHECK_FALSE (s.historyMatches (prior));
        prior = token (s);
        CHECK_FALSE (s.endTransactionGroup());
        CHECK (s.historyMatches (prior));
        std::uint64_t next = (std::numeric_limits<std::uint64_t>::max)() - 1;
        std::uint64_t value = 17;
        REQUIRE (e::detail::takeProjectUndoGroupId (next, value));
        CHECK (value == (std::numeric_limits<std::uint64_t>::max)() - 1);
        CHECK (next == (std::numeric_limits<std::uint64_t>::max)());
        REQUIRE (e::detail::takeProjectUndoGroupId (next, value));
        CHECK (value == (std::numeric_limits<std::uint64_t>::max)());
        CHECK (next == 0);
        CHECK_FALSE (e::detail::takeProjectUndoGroupId (next, value));
        CHECK (next == 0);
        CHECK (value == (std::numeric_limits<std::uint64_t>::max)());
    }

    SECTION ("sealing_redo_and_net_noop")
    {
        auto p = project();
        Stack s;
        REQUIRE (s.apply (p, mix (0.5f)).applied());
        REQUIRE (s.apply (p, e::ProjectEditCommand::setClipGain (id (6), 0.25f)).applied());
        REQUIRE (s.undo (p) == e::ProjectUndoStatus::Applied);
        auto expected = project();
        expected.tracks[0].strip.linearGain = 0.5f;
        projectIs (p, expected);
        auto prior = token (s);
        s.beginScalarCoalescing();
        CHECK_FALSE (s.historyMatches (prior));
        CHECK_FALSE (s.canRedo());
        const auto open = token (s);
        s.beginScalarCoalescing();
        CHECK (s.historyMatches (open));
        s.endScalarCoalescing();
        CHECK_FALSE (s.historyMatches (open));
        projectIs (p, expected);
        historyIs (s, 1, { { Verb::SetTrackMixScalars, 1 }, { Verb::SetClipGain, 1 } });
        prior = token (s);
        s.beginScalarCoalescing();
        REQUIRE (s.apply (p, mix (0.25f)).applied());
        REQUIRE (s.apply (p, mix (0.5f)).applied());
        const auto beforeRemoval = token (s);
        s.endScalarCoalescing();
        CHECK_FALSE (s.historyMatches (prior));
        CHECK_FALSE (s.historyMatches (beforeRemoval));
        projectIs (p, expected);
        historyIs (s, 1, { { Verb::SetTrackMixScalars, 1 }, { Verb::SetClipGain, 1 } });
        const auto closed = token (s);
        s.endScalarCoalescing();
        CHECK (s.historyMatches (closed));
        REQUIRE (s.redo (p) == e::ProjectUndoStatus::Applied);
        expected.clips[1].gain = 0.25f;
        projectIs (p, expected);
        REQUIRE (s.undo (p) == e::ProjectUndoStatus::Applied);
        s.beginScalarCoalescing();
        REQUIRE (s.apply (p, mix (0.125f)).applied());
        s.endScalarCoalescing();
        CHECK_FALSE (s.canRedo());
        REQUIRE (s.undo (p) == e::ProjectUndoStatus::Applied);
        expected.clips[1].gain = 0.75f;
        projectIs (p, expected);
        REQUIRE (s.undo (p) == e::ProjectUndoStatus::Applied);
        projectIs (p, project());
    }

    SECTION ("all_record_entrypoints")
    {
        for (int operation = 0; operation != 4; ++operation)
        {
            INFO (operation);
            auto p = project();
            const auto before = p;
            auto expected = before;
            Stack s;
            const auto prior = token (s);
            e::ProjectEditApplyResult result;
            if (operation == 0)
            {
                result = s.apply (p, e::ProjectEditCommand::renameTrack (id (3), "Spoken"));
                expected.tracks[0].strip.name = "Spoken";
            }
            else if (operation == 1)
            {
                const auto added = midi (40);
                result = s.addMidiClip (p, added);
                expected.midiClips.push_back (added);
            }
            else if (operation == 2)
            {
                auto after = before.clips[0];
                after.timelineStart = 384;
                after.timelineLength = 1024;
                after.timeBase = e::TimeBase::TempoLocked;
                after.fadeIn = 5;
                after.fadeOut = 7;
                result = s.replaceClipTiming (p, before.clips[0], after);
                expected.clips[0] = after;
            }
            else
            {
                const std::vector<e::AutomationBreakpoint> points {
                    { 16, 0.4, e::AutomationCurveType::Bezier }, { 1024, 0.6 }
                };
                result = s.replaceAutomationLanePoints (p, id (7), points);
                expected.automationLanes[0].points = points;
            }
            REQUIRE (result.applied());
            CHECK_FALSE (s.historyMatches (prior));
            projectIs (p, expected);
            REQUIRE (s.undo (p) == e::ProjectUndoStatus::Applied);
            projectIs (p, before);
            REQUIRE (s.redo (p) == e::ProjectUndoStatus::Applied);
            projectIs (p, expected);
        }

        // The empty stack's final allocation is its real first entry request.
        // Measure an equal fresh fixture; never hard-code an ABI allocation index.
        auto measuredProject = project();
        Stack measuredStack;
        const auto measuredEpoch = token (measuredStack);
        const auto command = e::ProjectEditCommand::moveClip (id (5), 384);
        Counts measured;
        e::ProjectEditApplyResult completed;
        {
            Probe probe;
            completed = measuredStack.apply (measuredProject, command);
            measured = probe.finish();
        }
        REQUIRE (completed.applied());
        REQUIRE (measured.attempts > 0);
        CHECK (measured.failures == 0);
        CHECK_FALSE (measuredStack.historyMatches (measuredEpoch));
        auto p = project();
        auto expected = p;
        expected.clips[0].timelineStart = 384;
        Stack s;
        const auto prior = token (s);
        Counts failed;
        bool threw = false;
        {
            Probe probe (measured.attempts - 1);
            try { (void) s.apply (p, command); }
            catch (const std::bad_alloc&) { threw = true; }
            failed = probe.finish();
        }
        INFO (measured.attempts);
        INFO (failed.attempts);
        REQUIRE (threw);
        REQUIRE (failed.failures == 1);
        REQUIRE (failed.attempts == measured.attempts);
        projectIs (p, expected);
        CHECK (s.undoDepth() == 0);
        CHECK (s.redoDepth() == 0);
        CHECK_FALSE (s.historyMatches (prior));
    }

    SECTION ("push_branch_cursor_and_stale_requests")
    {
        auto p = project();
        Stack s;
        REQUIRE (s.apply (p, e::ProjectEditCommand::moveClip (id (5), 384)).applied());
        Intent oldUndo;
        REQUIRE (s.captureUndo (oldUndo) == Capture::Captured);
        REQUIRE (s.apply (p, e::ProjectEditCommand::setClipGain (id (6), 0.25f)).applied());
        const auto beforeStale = p;
        const auto current = token (s);
        Navigation refused = Navigation::Applied;
        Counts noCopy;
        {
            Probe probe (0);
            refused = s.navigate (p, oldUndo);
            noCopy = probe.finish();
        }
        CHECK (refused == Navigation::StaleHistory);
        CHECK (noCopy.attempts == 0);
        CHECK (noCopy.failures == 0);
        CHECK (s.historyMatches (current));
        projectIs (p, beforeStale);
        REQUIRE (s.undo (p) == e::ProjectUndoStatus::Applied);
        Intent oldRedo;
        REQUIRE (s.captureRedo (oldRedo) == Capture::Captured);
        REQUIRE (s.apply (p, e::ProjectEditCommand::renameTrack (id (3), "Edited")).applied());
        const auto branched = p;
        CHECK_FALSE (s.canRedo());
        CHECK (s.navigate (p, oldRedo) == Navigation::StaleHistory);
        projectIs (p, branched);
        const auto beforeRoundTrip = token (s);
        REQUIRE (s.undo (p) == e::ProjectUndoStatus::Applied);
        REQUIRE (s.redo (p) == e::ProjectUndoStatus::Applied);
        projectIs (p, branched);
        CHECK_FALSE (s.historyMatches (beforeRoundTrip));
        historyIs (s, 2, { { Verb::MoveClip, 1 }, { Verb::RenameTrack, 1 } });
    }

    SECTION ("history_jump_atomicity")
    {
        auto p = project();
        const auto before = p;
        Stack s;
        REQUIRE (s.apply (p, e::ProjectEditCommand::moveClip (id (5), 384)).applied());
        REQUIRE (s.apply (p, e::ProjectEditCommand::setClipGain (id (6), 0.25f)).applied());
        REQUIRE (s.apply (p, e::ProjectEditCommand::renameTrack (id (3), "Edited")).applied());
        auto expected = before;
        expected.clips[0].timelineStart = 384;
        expected.clips[1].gain = 0.25f;
        expected.tracks[0].strip.name = "Edited";
        projectIs (p, expected);
        Intent toStart;
        REQUIRE (s.captureHistoryJump (0, toStart) == Capture::Captured);
        const auto current = token (s);
        auto damaged = p;
        damaged.clips[0].timelineStart = 385;
        const auto beforeFailure = damaged;
        CHECK (s.navigate (damaged, toStart) == Navigation::ProjectMismatch);
        projectIs (damaged, beforeFailure);
        CHECK (s.historyMatches (current));
        historyIs (s, 3, { { Verb::MoveClip, 1 }, { Verb::SetClipGain, 1 }, { Verb::RenameTrack, 1 } });
        REQUIRE (s.navigate (p, toStart) == Navigation::Applied);
        projectIs (p, before);
        historyIs (s, 0, { { Verb::MoveClip, 1 }, { Verb::SetClipGain, 1 }, { Verb::RenameTrack, 1 } });
        CHECK (s.navigate (p, toStart) == Navigation::StaleHistory);
        Intent toEnd;
        REQUIRE (s.captureHistoryJump (3, toEnd) == Capture::Captured);
        REQUIRE (s.navigate (p, toEnd) == Navigation::Applied);
        projectIs (p, expected);
        const auto atEnd = token (s);
        Intent same;
        REQUIRE (s.captureHistoryJump (3, same) == Capture::Captured);
        CHECK (s.navigate (p, same) == Navigation::NoChange);
        CHECK (s.historyMatches (atEnd));
    }

    SECTION ("invalid_empty_and_nochange")
    {
        auto p = project();
        const auto before = p;
        Stack s;
        const auto current = token (s);
        Intent request;
        CHECK (s.captureUndo (request) == Capture::NoStep);
        CHECK_FALSE (request.valid());
        CHECK (s.captureRedo (request) == Capture::NoStep);
        CHECK_FALSE (request.valid());
        CHECK (s.captureHistoryJump ((std::numeric_limits<std::size_t>::max)(), request) == Capture::InvalidTarget);
        CHECK_FALSE (request.valid());
        CHECK (s.navigate (p, request) == Navigation::InvalidIntent);
        CHECK (s.undo (p) == e::ProjectUndoStatus::NothingToUndo);
        CHECK (s.redo (p) == e::ProjectUndoStatus::NothingToRedo);
        CHECK_FALSE (s.apply (p, e::ProjectEditCommand::setClipGain (id (6), -1.0f)).applied());
        CHECK (s.historyMatches (current));
        projectIs (p, before);
        REQUIRE (s.captureHistoryJump (0, request) == Capture::Captured);
        CHECK (s.navigate (p, request) == Navigation::NoChange);
        CHECK (s.historyMatches (current));
        CHECK (s.captureUndo (request) == Capture::NoStep);
        CHECK_FALSE (request.valid());
        REQUIRE (s.apply (p, e::ProjectEditCommand::moveClip (id (5), 384)).applied());
        const auto edited = token (s);
        auto mismatch = p;
        mismatch.clips[0].timelineStart = 385;
        const auto mismatchBefore = mismatch;
        CHECK (s.undo (mismatch) == e::ProjectUndoStatus::ProjectMismatch);
        projectIs (mismatch, mismatchBefore);
        CHECK (s.historyMatches (edited));
        REQUIRE (s.undo (p) == e::ProjectUndoStatus::Applied);
        const auto undone = token (s);
        mismatch = p;
        mismatch.clips[0].timelineStart = 121;
        const auto redoMismatch = mismatch;
        CHECK (s.redo (mismatch) == e::ProjectUndoStatus::ProjectMismatch);
        projectIs (mismatch, redoMismatch);
        CHECK (s.historyMatches (undone));
    }

    SECTION ("copy_forks_and_assignment")
    {
        auto originalProject = project();
        Stack original;
        REQUIRE (original.apply (originalProject, e::ProjectEditCommand::moveClip (id (5), 384)).applied());
        const auto originalToken = token (original);
        Stack left = original;
        Stack right = original;
        CHECK (original.historyMatches (originalToken));
        CHECK_FALSE (left.historyMatches (originalToken));
        CHECK_FALSE (right.historyMatches (originalToken));
        const auto leftInitial = token (left);
        const auto rightInitial = token (right);
        CHECK_FALSE (left.historyMatches (rightInitial));
        CHECK_FALSE (right.historyMatches (leftInitial));
        auto leftProject = originalProject;
        auto rightProject = originalProject;
        REQUIRE (left.apply (leftProject, e::ProjectEditCommand::setClipGain (id (6), 0.25f)).applied());
        REQUIRE (right.apply (rightProject, e::ProjectEditCommand::setClipGain (id (6), 0.5f)).applied());
        CHECK (left.undoDepth() == right.undoDepth());
        CHECK (left.nextUndo()->command.verb == right.nextUndo()->command.verb);
        const auto leftEdited = token (left);
        const auto rightEdited = token (right);
        CHECK_FALSE (left.historyMatches (rightEdited));
        CHECK_FALSE (right.historyMatches (leftEdited));
        CHECK (original.historyMatches (originalToken));

        Stack saved = original;
        const auto savedToken = token (saved);
        REQUIRE (original.apply (originalProject, e::ProjectEditCommand::setClipGain (id (6), 0.5f)).applied());
        CHECK_FALSE (original.historyMatches (originalToken));
        original = saved;
        CHECK_FALSE (original.historyMatches (originalToken));
        CHECK_FALSE (original.historyMatches (savedToken));
        CHECK (saved.historyMatches (savedToken));
        historyIs (original, 1, { { Verb::MoveClip, 1 } });
        const auto assigned = token (original);
        Stack* alias = std::addressof (original);
        original = *alias;
        CHECK (original.historyMatches (assigned));
        auto restoredProject = project();
        restoredProject.clips[0].timelineStart = 384;
        REQUIRE (original.undo (restoredProject) == e::ProjectUndoStatus::Applied);
        projectIs (restoredProject, project());

        auto scalarProject = project();
        Stack scalar;
        scalar.beginScalarCoalescing();
        REQUIRE (scalar.apply (scalarProject, mix (0.5f)).applied());
        const auto scalarToken = token (scalar);
        Stack scalarCopy = scalar;
        Intent blocked;
        CHECK (scalarCopy.captureUndo (blocked) == Capture::GestureOpen);
        auto copyProject = scalarProject;
        const auto copiedEdit = scalarCopy.apply (copyProject, mix (0.25f));
        REQUIRE (copiedEdit.applied());
        REQUIRE (copiedEdit.coalesced);
        scalarCopy.endScalarCoalescing();
        REQUIRE (scalarCopy.undo (copyProject) == e::ProjectUndoStatus::Applied);
        projectIs (copyProject, project());
        CHECK (scalar.historyMatches (scalarToken));
    }

    SECTION ("copy_failure_ordering")
    {
        auto donorProject = project();
        Stack donor;
        REQUIRE (donor.apply (donorProject, e::ProjectEditCommand::moveClip (id (5), 384)).applied());
        REQUIRE (donor.apply (donorProject, e::ProjectEditCommand::setClipGain (id (6), 0.25f)).applied());
        const auto donorToken = token (donor);
        auto receivingProject = project();
        Stack receiving;
        REQUIRE (receiving.apply (receivingProject, e::ProjectEditCommand::renameTrack (id (3), "Receiver")).applied());
        const auto receivingToken = token (receiving);
        const auto before = receivingProject;
        Counts observed;
        bool threw = false;
        {
            Probe probe (0);
            try { receiving = donor; }
            catch (const std::bad_alloc&) { threw = true; }
            observed = probe.finish();
        }
        REQUIRE (threw);
        REQUIRE (observed.failures == 1);
        CHECK (observed.attempts == 1);
        CHECK (receiving.historyMatches (receivingToken));
        CHECK (donor.historyMatches (donorToken));
        projectIs (receivingProject, before);
        historyIs (receiving, 1, { { Verb::RenameTrack, 1 } });
        historyIs (donor, 2, { { Verb::MoveClip, 1 }, { Verb::SetClipGain, 1 } });
        REQUIRE (receiving.undo (receivingProject) == e::ProjectUndoStatus::Applied);
        projectIs (receivingProject, project());
    }

    SECTION ("moves_reset_and_owner_ABA")
    {
        auto p = project();
        Stack source;
        REQUIRE (source.beginTransactionGroup());
        REQUIRE (source.apply (p, e::ProjectEditCommand::moveClip (id (5), 384)).applied());
        const auto fromSource = token (source);
        Stack moved (std::move (source));
        CHECK_FALSE (source.historyMatches (fromSource));
        CHECK_FALSE (moved.historyMatches (fromSource));
        CHECK (source.undoDepth() == 0);
        CHECK (source.redoDepth() == 0);
        CHECK_FALSE (source.transactionGroupOpen());
        CHECK (moved.transactionGroupOpen());
        REQUIRE (moved.endTransactionGroup());
        const auto fromMoved = token (moved);
        Stack receiving;
        const auto receivingOld = token (receiving);
        receiving = std::move (moved);
        CHECK_FALSE (receiving.historyMatches (receivingOld));
        CHECK_FALSE (receiving.historyMatches (fromMoved));
        CHECK_FALSE (moved.historyMatches (fromMoved));
        CHECK (moved.undoDepth() == 0);
        CHECK (moved.redoDepth() == 0);
        CHECK_FALSE (moved.transactionGroupOpen());
        const auto assigned = token (receiving);
        Stack* alias = std::addressof (receiving);
        receiving = std::move (*alias);
        CHECK (receiving.historyMatches (assigned));
        Intent undo;
        REQUIRE (receiving.captureUndo (undo) == Capture::Captured);
        REQUIRE (receiving.navigate (p, undo) == Navigation::Applied);
        projectIs (p, project());
        const auto beforeReset = token (receiving);
        receiving = {};
        CHECK_FALSE (receiving.historyMatches (beforeReset));
        historyIs (receiving, 0, {});
        const auto empty = token (receiving);
        receiving = Stack {};
        CHECK_FALSE (receiving.historyMatches (empty));

        const auto movedFrom = token (moved);
        moved.endScalarCoalescing();
        CHECK (moved.historyMatches (movedFrom));
        auto freshProject = project();
        REQUIRE (moved.beginTransactionGroup());
        REQUIRE (moved.apply (freshProject, e::ProjectEditCommand::setClipGain (id (6), 0.5f)).applied());
        REQUIRE (moved.endTransactionGroup());
        REQUIRE (moved.undo (freshProject) == e::ProjectUndoStatus::Applied);
        projectIs (freshProject, project());

        alignas (Stack) std::array<std::byte, sizeof (Stack)> storage {};
        auto destroy = [] (Stack* s) { std::destroy_at (s); };
        auto* address = reinterpret_cast<Stack*> (storage.data());
        std::unique_ptr<Stack, decltype (destroy)> inPlace (std::construct_at (address), destroy);
        const auto heldAcrossDestruction = token (*inPlace);
        inPlace.reset();
        inPlace.reset (std::construct_at (address));
        CHECK (inPlace.get() == address);
        CHECK_FALSE (inPlace->historyMatches (heldAcrossDestruction));
        const auto rebuilt = token (*inPlace);
        CHECK (inPlace->historyMatches (rebuilt));
        CHECK_FALSE (inPlace->historyMatches (heldAcrossDestruction));
    }

    SECTION ("capture_allocation_refusal")
    {
        Stack previousOwner;
        const auto retained = token (previousOwner);
        Token out = retained;
        Stack s;
        Counts failed;
        bool captured = true;
        {
            Probe probe (0);
            captured = s.captureHistory (out);
            failed = probe.finish();
        }
        REQUIRE (failed.failures == 1);
        CHECK (failed.attempts == 1);
        CHECK_FALSE (captured);
        CHECK_FALSE (out.valid());
        CHECK_FALSE (s.historyMatches (out));
        CHECK (previousOwner.historyMatches (retained));
        historyIs (s, 0, {});
        auto p = project();
        REQUIRE (s.apply (p, e::ProjectEditCommand::moveClip (id (5), 384)).applied());
        const auto recovered = token (s);
        Counts reused;
        {
            Probe probe (0);
            captured = s.captureHistory (out);
            reused = probe.finish();
        }
        CHECK (captured);
        CHECK (reused.attempts == 0);
        CHECK (reused.failures == 0);
        CHECK (s.historyMatches (recovered));
        CHECK (s.historyMatches (out));

        auto otherProject = project();
        Stack other;
        REQUIRE (other.apply (otherProject, e::ProjectEditCommand::moveClip (id (5), 384)).applied());
        Intent navigation;
        Capture status = Capture::Captured;
        {
            Probe probe (0);
            status = other.captureUndo (navigation);
            failed = probe.finish();
        }
        CHECK (status == Capture::IdentityUnavailable);
        CHECK_FALSE (navigation.valid());
        REQUIRE (failed.failures == 1);
        CHECK (failed.attempts == 1);
        historyIs (other, 1, { { Verb::MoveClip, 1 } });
        REQUIRE (other.undo (otherProject) == e::ProjectUndoStatus::Applied);
        projectIs (otherProject, project());
    }

    SECTION ("navigation_staging_resource_failure")
    {
        auto p = project();
        Stack s;
        REQUIRE (s.apply (p, e::ProjectEditCommand::moveClip (id (5), 384)).applied());
        const auto before = p;
        const auto current = token (s);
        Intent undo;
        REQUIRE (s.captureUndo (undo) == Capture::Captured);
        Counts failed;
        Navigation result = Navigation::Applied;
        {
            Probe probe (0);
            result = s.navigate (p, undo);
            failed = probe.finish();
        }
        CHECK (result == Navigation::ResourceUnavailable);
        REQUIRE (failed.failures == 1);
        CHECK (failed.attempts == 1);
        CHECK (s.historyMatches (current));
        projectIs (p, before);
        historyIs (s, 1, { { Verb::MoveClip, 1 } });
        REQUIRE (s.undo (p) == e::ProjectUndoStatus::Applied);
        projectIs (p, project());
    }

    SECTION ("noexcept_and_no_new_edit_allocation")
    {
        Stack s;
        const auto old = token (s);
        bool began = false, ended = false, stale = false;
        Counts boundary;
        {
            Probe probe;
            began = s.beginTransactionGroup();
            ended = s.endTransactionGroup();
            s.beginScalarCoalescing();
            s.beginScalarCoalescing();
            s.endScalarCoalescing();
            s.endScalarCoalescing();
            stale = ! s.historyMatches (old);
            boundary = probe.finish();
        }
        CHECK (began);
        CHECK (ended);
        CHECK (stale);
        CHECK (boundary.attempts == 0);
        CHECK (boundary.failures == 0);

        auto p = project();
        REQUIRE (s.beginTransactionGroup());
        REQUIRE (s.apply (p, e::ProjectEditCommand::moveClip (id (5), 384)).applied());
        REQUIRE (s.apply (p, e::ProjectEditCommand::setClipGain (id (6), 0.25f)).applied());
        REQUIRE (s.endTransactionGroup());
        REQUIRE (s.apply (p, e::ProjectEditCommand::renameTrack (id (3), "Edited")).applied());
        const auto current = token (s);
        Intent undo, redo, jump, invalid;
        Capture a {}, b {}, c {}, d {};
        Counts folded;
        {
            Probe probe;
            a = s.captureUndo (undo);
            b = s.captureRedo (redo);
            c = s.captureHistoryJump (0, jump);
            d = s.captureHistoryJump (3, invalid);
            folded = probe.finish();
        }
        CHECK (a == Capture::Captured);
        CHECK (b == Capture::NoStep);
        CHECK (c == Capture::Captured);
        CHECK (d == Capture::InvalidTarget);
        CHECK (undo.valid());
        CHECK_FALSE (redo.valid());
        CHECK (jump.valid());
        CHECK_FALSE (invalid.valid());
        CHECK (folded.attempts == 0);
        CHECK (folded.failures == 0);
        CHECK (s.historyMatches (current));
        historyIs (s, 2, { { Verb::SetClipGain, 2 }, { Verb::RenameTrack, 1 } });
    }

    SECTION ("ordinary_history_roundtrip")
    {
        auto p = project();
        const auto before = p;
        auto expected = before;
        Stack s;
        REQUIRE (s.beginTransactionGroup());
        REQUIRE (s.apply (p, e::ProjectEditCommand::moveClip (id (5), 384)).applied());
        REQUIRE (s.apply (p, e::ProjectEditCommand::setClipGain (id (6), 0.25f)).applied());
        REQUIRE (s.endTransactionGroup());
        expected.clips[0].timelineStart = 384;
        expected.clips[1].gain = 0.25f;
        s.beginScalarCoalescing();
        REQUIRE (s.apply (p, mix (0.5f)).applied());
        const auto scalarEdit = s.apply (p, mix (0.375f));
        REQUIRE (scalarEdit.applied());
        REQUIRE (scalarEdit.coalesced);
        s.endScalarCoalescing();
        expected.tracks[0].strip.linearGain = 0.375f;
        const auto added = midi (40);
        REQUIRE (s.addMidiClip (p, added).applied());
        expected.midiClips.push_back (added);
        const std::vector<e::AutomationBreakpoint> points { { 32, 0.3 }, { 512, 0.7 } };
        REQUIRE (s.replaceAutomationLanePoints (p, id (7), points).applied());
        expected.automationLanes[0].points = points;
        const auto complete = expected;
        projectIs (p, complete);
        CHECK (s.undoDepth() == 5);
        historyIs (s, 4, { { Verb::SetClipGain, 2 }, { Verb::SetTrackMixScalars, 1 },
                          { Verb::AddMidiClip, 1 }, { Verb::ReplaceAutomationLanePoints, 1 } });
        REQUIRE (s.undo (p) == e::ProjectUndoStatus::Applied);
        expected.automationLanes = before.automationLanes;
        projectIs (p, expected);
        REQUIRE (s.undo (p) == e::ProjectUndoStatus::Applied);
        expected.midiClips = before.midiClips;
        projectIs (p, expected);
        Intent undo;
        REQUIRE (s.captureUndo (undo) == Capture::Captured);
        REQUIRE (s.navigate (p, undo) == Navigation::Applied);
        expected.tracks = before.tracks;
        projectIs (p, expected);
        Intent redo;
        REQUIRE (s.captureRedo (redo) == Capture::Captured);
        REQUIRE (s.navigate (p, redo) == Navigation::Applied);
        expected.tracks[0].strip.linearGain = 0.375f;
        projectIs (p, expected);
        Intent jump;
        REQUIRE (s.captureHistoryJump (0, jump) == Capture::Captured);
        REQUIRE (s.navigate (p, jump) == Navigation::Applied);
        projectIs (p, before);
        REQUIRE (s.captureHistoryJump (4, jump) == Capture::Captured);
        REQUIRE (s.navigate (p, jump) == Navigation::Applied);
        projectIs (p, complete);
        historyIs (s, 4, { { Verb::SetClipGain, 2 }, { Verb::SetTrackMixScalars, 1 },
                          { Verb::AddMidiClip, 1 }, { Verb::ReplaceAutomationLanePoints, 1 } });
    }
}
