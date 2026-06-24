// YES DAW — H0 spike DSP: a pure, testable sine tone generator.
//
// THROWAWAY spike DSP (H0), not the engine. Its one job: be the audio the device round-trip plays,
// while being trivially testable WITHOUT a device or a display. So it has ZERO JUCE / I/O / device
// dependencies — just <cmath>. That lets a headless CI test golden-compare its output and assert its
// pitch/level, instead of a human listening. The real engine's Node contract arrives at H1.
//
// It also shapes like RT-safe code on purpose (no allocation, no locks, no I/O in the hot path) — a
// habit worth forming before the engine exists, where it becomes a hard rule (ADR-0002).

#pragma once

#include <algorithm>
#include <cmath>

namespace yesdaw::dsp {

// A sine oscillator with a short linear fade-in/out envelope. The fade "tames the spike" so starting
// or stopping the tone isn't a jumpscare on real speakers. Everything here is deterministic and
// allocation-free.
class SineSource
{
public:
    static constexpr double kDefaultFrequencyHz = 440.0;
    static constexpr float  kDefaultAmplitude   = 0.10f;  // ~ -20 dBFS, gentle by default
    static constexpr double kDefaultFadeSeconds = 0.05;   // 50 ms ramp

    // Reset and arm the generator for a given sample rate. After prepare(), the envelope ramps UP
    // from silence (a clean fade-in). Call this from the audio prepare step, never mid-block.
    void prepare (double sampleRate)
    {
        sampleRate_ = (sampleRate > 0.0) ? sampleRate : 48000.0;
        phase_      = 0.0;
        updatePhaseDelta();

        const double fadeSamples = kDefaultFadeSeconds * sampleRate_;
        gainStep_   = (fadeSamples > 0.0) ? (float) (1.0 / fadeSamples) : 1.0f;
        gain_       = 0.0f;   // start silent...
        targetGain_ = 1.0f;   // ...and ramp up
    }

    void setFrequency (double hz) { frequencyHz_ = hz; updatePhaseDelta(); }
    void setAmplitude (float a)   { amplitude_ = a; }

    // Begin a fade-out to silence (clean stop) / restart the fade-in (clean start).
    void noteOff() { targetGain_ = 0.0f; }
    void noteOn()  { targetGain_ = 1.0f; }

    bool isSilent() const { return gain_ <= 0.0f && targetGain_ <= 0.0f; }

    // Produce one mono sample and advance. The envelope moves one step toward its target BEFORE the
    // sample is taken, so sample 0 is exactly 0 (silence × sin(0)).
    float nextSample()
    {
        if (gain_ < targetGain_)      gain_ = std::min (targetGain_, gain_ + gainStep_);
        else if (gain_ > targetGain_) gain_ = std::max (targetGain_, gain_ - gainStep_);

        const float s = amplitude_ * gain_ * (float) std::sin (phase_);

        // Keep phase in [0, 2pi) so sin() stays precise. Wrap BOTH ways: a negative frequency
        // (or a future downward sweep through 0) would otherwise drift unboundedly negative.
        phase_ += phaseDelta_;
        while (phase_ >= kTwoPi) phase_ -= kTwoPi;
        while (phase_ < 0.0)     phase_ += kTwoPi;
        return s;
    }

    // Fill numFrames of mono audio. Caller copies to however many output channels it has.
    void processMono (float* dst, int numFrames)
    {
        for (int i = 0; i < numFrames; ++i)
            dst[i] = nextSample();
    }

private:
    static constexpr double kTwoPi = 6.283185307179586476925286766559;

    void updatePhaseDelta() { phaseDelta_ = kTwoPi * frequencyHz_ / sampleRate_; }

    double sampleRate_  = 48000.0;
    double frequencyHz_ = kDefaultFrequencyHz;
    double phase_       = 0.0;
    double phaseDelta_  = 0.0;
    float  amplitude_   = kDefaultAmplitude;
    float  gain_        = 0.0f;
    float  targetGain_  = 1.0f;
    float  gainStep_    = 1.0f;
};

} // namespace yesdaw::dsp
