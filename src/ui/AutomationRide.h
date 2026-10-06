// YES DAW - G4.6 / ADR-0052: the ride laws (message thread; no JUCE, no engine state).
//
// A ride turns control moves made while the transport rolls into automation passes. What starts and ends a
// pass is the project's automation mode:
//   Touch - a target writes from its touch to its release.
//   Latch - from its touch to stop; after the release it holds its last value.
//   Write - every target armed at play (the selected strip's laned targets, at their current values) and every
//           target touched during the pass writes from then to stop; a released target holds its last value.
// A loop wrap (the playhead tick runs backwards) ends every pass as written so far; the targets still writing
// (held, latched or Write) carry on in the next cycle as new passes.
// Density: at most one sample per 64-frame control interval (ADR-0039); a sample is kept when it moved by at
// least 1/512 of full scale from the last kept one or 250 ms passed; a pass always ends on its last value.

#pragma once

#include "engine/Project.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace yesdaw::ui {

struct AutomationRideTarget
{
    engine::EntityId owner {};
    engine::AutomationTargetRole role = engine::AutomationTargetRole::TrackFader;
    std::uint32_t paramId = 0;

    friend bool operator== (const AutomationRideTarget&, const AutomationRideTarget&) = default;
};

struct AutomationRideSample
{
    engine::Tick tick = 0;
    double value = 0.0;

    friend bool operator== (const AutomationRideSample&, const AutomationRideSample&) = default;
};

// Where the playhead is: the musical tick a breakpoint stores and the frame the density rule measures.
struct AutomationRidePosition
{
    engine::Tick tick = 0;
    std::int64_t frame = 0;
};

// One closed pass: the samples one target wrote, in rising ticks (at least one).
struct AutomationRidePass
{
    AutomationRideTarget target {};
    std::vector<AutomationRideSample> samples {};
};

class AutomationRideRecorder
{
public:
    static constexpr std::int64_t kControlIntervalFrames = 64;
    static constexpr double kMinStep = 1.0 / 512.0;
    static constexpr double kMaxGapSeconds = 0.25;

    // A transport run begins: the mode decides what a release does; nothing is writing yet.
    void start (engine::AutomationMode mode, double sampleRateHz)
    {
        mode_ = mode;
        sampleRateHz_ = sampleRateHz > 0.0 ? sampleRateHz : 48'000.0;
        writers_.clear();
    }

    [[nodiscard]] engine::AutomationMode mode() const noexcept { return mode_; }
    [[nodiscard]] bool writing() const noexcept { return ! writers_.empty(); }
    [[nodiscard]] bool isWriting (const AutomationRideTarget& target) const noexcept { return find (target) != nullptr; }

    [[nodiscard]] std::vector<AutomationRideTarget> writingTargets() const
    {
        std::vector<AutomationRideTarget> targets;
        for (const Writer& writer : writers_)
            targets.push_back (writer.target);
        return targets;
    }

    // The value a writing target holds now (what its live post should play), or `fallback`.
    [[nodiscard]] double latestValue (const AutomationRideTarget& target, double fallback) const noexcept
    {
        const Writer* const writer = find (target);
        return writer != nullptr ? writer->latest.value : fallback;
    }

    // How many samples the target's open pass keeps (the state probe's mid-ride view).
    [[nodiscard]] std::size_t keptSampleCount (const AutomationRideTarget& target) const noexcept
    {
        const Writer* const writer = find (target);
        return writer != nullptr ? writer->kept.size() : 0u;
    }

    // Write: a target writes `value` from here to stop until it is touched.
    [[nodiscard]] std::vector<AutomationRidePass> arm (const AutomationRideTarget& target, AutomationRidePosition at, double value)
    {
        std::vector<AutomationRidePass> closed = wrapIfBackwards (at);
        if (find (target) == nullptr)
        {
            writers_.push_back ({ target, false });
            sample (writers_.back(), at, value);
        }
        return closed;
    }

    // A press or a move of a control.
    [[nodiscard]] std::vector<AutomationRidePass> touch (const AutomationRideTarget& target, AutomationRidePosition at, double value)
    {
        std::vector<AutomationRidePass> closed = wrapIfBackwards (at);
        Writer* writer = find (target);
        if (writer == nullptr)
        {
            writers_.push_back ({ target, true });
            writer = &writers_.back();
        }
        writer->pressed = true;
        sample (*writer, at, value);
        return closed;
    }

    // The release. Touch: the pass closes. Latch and Write: the target holds its last value until stop.
    [[nodiscard]] std::vector<AutomationRidePass> release (const AutomationRideTarget& target, AutomationRidePosition at, double value)
    {
        std::vector<AutomationRidePass> closed = wrapIfBackwards (at);
        for (std::size_t i = 0; i < writers_.size(); ++i)
        {
            Writer& writer = writers_[i];
            if (! (writer.target == target))
                continue;

            sample (writer, at, value);
            writer.pressed = false;
            if (mode_ != engine::AutomationMode::Latch && mode_ != engine::AutomationMode::Write)
            {
                closed.push_back (close (writer));
                writers_.erase (writers_.begin() + static_cast<std::ptrdiff_t> (i));
            }
            break;
        }
        return closed;
    }

    // A UI tick while the transport rolls: every writing target samples the value it holds (the 250 ms
    // rule keeps a held value on the lane); a backwards tick (a loop wrap) first closes every pass.
    [[nodiscard]] std::vector<AutomationRidePass> advance (AutomationRidePosition at)
    {
        std::vector<AutomationRidePass> closed = wrapIfBackwards (at);
        for (Writer& writer : writers_)
            sample (writer, at, writer.latest.value);
        return closed;
    }

    // The transport stopped with the playhead last at `at`: every pass writes on to there and closes.
    [[nodiscard]] std::vector<AutomationRidePass> stop (AutomationRidePosition at)
    {
        std::vector<AutomationRidePass> closed = wrapIfBackwards (at);
        for (Writer& writer : writers_)
        {
            sample (writer, at, writer.latest.value);
            closed.push_back (close (writer));
        }
        writers_.clear();
        return closed;
    }

private:
    struct Writer
    {
        AutomationRideTarget target {};
        bool pressed = false;
        std::vector<AutomationRideSample> kept {};
        std::int64_t lastKeptFrame = 0;
        AutomationRideSample latest {};    // the newest sample seen, kept or not
        bool latestKept = true;
    };

    [[nodiscard]] Writer* find (const AutomationRideTarget& target) noexcept
    {
        for (Writer& writer : writers_)
            if (writer.target == target)
                return &writer;
        return nullptr;
    }

    [[nodiscard]] const Writer* find (const AutomationRideTarget& target) const noexcept
    {
        for (const Writer& writer : writers_)
            if (writer.target == target)
                return &writer;
        return nullptr;
    }

    void sample (Writer& writer, AutomationRidePosition at, double value)
    {
        if (! std::isfinite (value) || at.tick < 0)
            return;

        if (writer.kept.empty())
        {
            writer.kept.push_back ({ at.tick, value });
            writer.lastKeptFrame = at.frame;
            writer.latest = writer.kept.back();
            writer.latestKept = true;
            return;
        }

        if (at.tick <= writer.kept.back().tick)
        {
            // The same tick again: the last value wins.
            if (at.tick == writer.kept.back().tick)
            {
                writer.kept.back().value = value;
                writer.latest = writer.kept.back();
                writer.latestKept = true;
            }
            return;
        }

        writer.latest = { at.tick, value };
        const std::int64_t sinceKept = at.frame - writer.lastKeptFrame;
        const bool moved = std::abs (value - writer.kept.back().value) >= kMinStep;
        const bool gap = static_cast<double> (sinceKept) >= kMaxGapSeconds * sampleRateHz_;
        if (sinceKept >= kControlIntervalFrames && (moved || gap))
        {
            writer.kept.push_back (writer.latest);
            writer.lastKeptFrame = at.frame;
            writer.latestKept = true;
        }
        else
        {
            writer.latestKept = false;
        }
    }

    // A pass ends on its last value.
    [[nodiscard]] static AutomationRidePass close (const Writer& writer)
    {
        AutomationRidePass pass { writer.target, writer.kept };
        if (! writer.latestKept && (pass.samples.empty() || writer.latest.tick > pass.samples.back().tick))
            pass.samples.push_back (writer.latest);
        return pass;
    }

    // A playhead behind any writer's newest sample is a loop wrap: every pass closes as written so far and
    // the writers still writing restart here with the value they hold.
    [[nodiscard]] std::vector<AutomationRidePass> wrapIfBackwards (AutomationRidePosition at)
    {
        std::vector<AutomationRidePass> closed;
        bool wrapped = false;
        for (const Writer& writer : writers_)
            wrapped = wrapped || at.tick < writer.latest.tick;
        if (! wrapped)
            return closed;

        for (Writer& writer : writers_)
        {
            closed.push_back (close (writer));
            const double holding = writer.latest.value;
            writer.kept.clear();
            writer.latestKept = true;
            sample (writer, at, holding);
        }
        return closed;
    }

    engine::AutomationMode mode_ = engine::AutomationMode::Touch;
    double sampleRateHz_ = 48'000.0;
    std::vector<Writer> writers_;
};

} // namespace yesdaw::ui
