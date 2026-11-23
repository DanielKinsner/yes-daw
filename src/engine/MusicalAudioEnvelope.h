// Offline preparation: apply the Clip's native fade curves once after stretching.
#pragma once

#include "engine/ClipEnvelope.h"
#include "engine/MusicalAudioWindow.h"

#include <algorithm>
#include <cmath>
#include <span>

namespace yesdaw::engine {

enum class MusicalEnvelopeStatus { Ok, InvalidInput, InversePrecision, NonFiniteSample };

// Clock is the checked complete project map from the preparation request. Working samples are
// private until this succeeds. Gain remains on the scheduled Clip and is never multiplied here.
inline MusicalEnvelopeStatus applyMusicalAudioEnvelope (const Clip& clip, const MusicalAudioWindow& window,
                                                       const CompiledTempoMap& clock, unsigned channels,
                                                       std::span<float> samples)
{
    using Status = MusicalEnvelopeStatus;
    if (!window.valid() || clip.timeBase != TimeBase::TempoLocked || clip.timelineStart != window.startTick()
        || clip.timelineLength != window.lengthTicks() || clip.fadeIn < 0 || clip.fadeOut < 0
        || channels < 1 || channels > 2 || samples.size() % channels != 0
        || samples.size() / channels != static_cast<std::uint64_t> (window.frameCount()))
        return Status::InvalidInput;
    const bool faded = clip.fadeIn != 0 || clip.fadeOut != 0;
    double startFrame = 0.0;
    if (faded && (clock.frameForMusicalTime ({ clip.timelineStart, 0.0 }, startFrame) != TimeLookupStatus::Ok
        || startFrame >= 0x1p63 || static_cast<std::int64_t> (std::floor (startFrame)) != window.startFrame()))
        return Status::InversePrecision;

    for (std::int64_t j = 0; j < window.frameCount(); ++j)
    {
        float envelope = 1.0f;
        if (faded)
        {
            const auto absolute = window.startFrame() + j;
            const double frame = static_cast<double> (absolute);
            MusicalTime t;
            double roundTrip = 0.0;
            if (frame >= 0x1p63 || static_cast<std::int64_t> (frame) != absolute
                || clock.musicalTimeForFrame (frame, t) != TimeLookupStatus::Ok
                || clock.frameForMusicalTime (t, roundTrip) != TimeLookupStatus::Ok
                || std::abs (roundTrip - frame) > 1.0 / 1024.0)
                return Status::InversePrecision;
            if (t.tick < clip.timelineStart)
            {
                if (j != 0 || !(frame < startFrame && startFrame - frame < 1.0))
                    return Status::InversePrecision;
                t = { clip.timelineStart, 0.0 };
            }
            const auto whole = t.tick - clip.timelineStart;
            if (whole >= clip.timelineLength) return Status::InversePrecision;
            if (whole < clip.fadeIn)
                envelope = detail::fadeShapeGain (clip.fadeInShape, clip.fadeInCurve,
                    (static_cast<double> (whole) + t.frac) / static_cast<double> (clip.fadeIn));
            const auto outStart = std::max<Tick> (0, clip.timelineLength - clip.fadeOut);
            if (clip.fadeOut > 0 && whole >= outStart)
                envelope = std::min (envelope, detail::fadeShapeGain (clip.fadeOutShape, clip.fadeOutCurve,
                    1.0 - (static_cast<double> (whole - outStart) + t.frac) / static_cast<double> (clip.fadeOut)));
        }
        const auto base = static_cast<std::size_t> (j) * channels;
        for (unsigned channel = 0; channel < channels; ++channel)
        {
            float& sample = samples[base + channel];
            if (!std::isfinite (sample)) return Status::NonFiniteSample;
            sample *= envelope;
        }
    }
    return Status::Ok;
}

} // namespace yesdaw::engine
