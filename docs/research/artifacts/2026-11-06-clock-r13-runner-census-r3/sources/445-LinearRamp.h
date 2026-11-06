// YES DAW — LinearRamp: the per-sample linear ramp the Node contract (ADR-0008) requires.
//
// ADR-0008 makes per-Block evaluation a rule: fades, gain, pan, and automation ramp with a smoother or a
// lookup table — never a recomputed std::pow/std::cos in a per-frame read path. The engine is JUCE-free
// (ADR-0002), so this is our own stand-in for juce::SmoothedValue<float, Linear>: one add per frame, no
// allocation, RT-safe.
//
// CRUCIAL PROPERTY: the ramp advances exactly ONCE PER FRAME, never per Block. That is what makes a Node
// using it produce bit-identical output no matter how the host slices process() into Blocks (the ADR-0008
// invariance contract). A ramp that stepped per Block would drift with the Block size.

#pragma once

#include "rt/RtHot.h"

namespace yesdaw::dsp {

class LinearRamp
{
public:
    // Length of a ramp in frames. Clamped to >= 1 so a zero never divides.
    void setRampLength (int samples) noexcept { rampLen_ = samples > 0 ? samples : 1; }

    // Aim at a new target; the value glides there over rampLen frames. A no-op if the target is unchanged,
    // so a Node can call this every Block from an atomic without re-arming a settled ramp.
    void setTarget (float v) noexcept
    {
        if (v == target_)               // unchanged -> keep the current settled/ramping state as-is
            return;

        target_ = v;
        if (rampLen_ <= 1)              // instantaneous
        {
            current_   = v;
            step_      = 0.0f;
            remaining_ = 0;
            return;
        }
        step_      = (target_ - current_) / static_cast<float> (rampLen_);
        remaining_ = rampLen_;
    }

    // Jump to a value immediately (use at prepare()/reset(), never mid-Block on a live signal).
    void snap (float v) noexcept { current_ = target_ = v; step_ = 0.0f; remaining_ = 0; }

    // Advance one frame and return the new value. The hot path.
    float next() noexcept YESDAW_RT_HOT
    {
        if (remaining_ > 0)
        {
            current_ += step_;
            if (--remaining_ == 0)
                current_ = target_;     // land exactly on target (no float drift)
        }
        return current_;
    }

    float current()   const noexcept { return current_; }
    float target()    const noexcept { return target_; }
    bool  isRamping() const noexcept { return remaining_ > 0; }

private:
    float current_   = 0.0f;
    float target_    = 0.0f;
    float step_      = 0.0f;
    int   remaining_ = 0;
    int   rampLen_   = 256;
};

} // namespace yesdaw::dsp
