#pragma once

// ANKS Labs SWARM / 01 -- JUCE-independent real-time DSP core.
// Copyright (c) 2026 ANKS Labs. Private development prototype.
// No heap allocation or locks in the audio rendering path.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace anks
{
constexpr int maxPolyphony = 8;
constexpr int oscillatorsPerSwarm = 7;
constexpr double pi = 3.1415926535897932384626433832795;

inline float clamp01 (float x) noexcept { return std::clamp (x, 0.0f, 1.0f); }
inline float lerp (float a, float b, float x) noexcept { return a + (b - a) * x; }

struct Patch
{
    float detuneA = 15.0f;   // cents, 0..60
    float widthA = 0.65f;    // stereo width, 0..1
    float motionA = 0.17f;   // 0..1
    float detuneB = 29.0f;
    float widthB = 0.83f;
    float motionB = 0.30f;
    float mix = 0.42f;       // B proportion: 0..1
    float cutoff = 9500.0f;  // Hz, 100..18000
    float resonance = 0.12f; // 0..1
    float drive = 0.10f;     // 0..1
    float attack = 0.015f;   // s, 0.005..3
    float release = 0.70f;   // s, 0.03..8
    float output = 0.78f;    // 0..1
};

// Four-corner interpolation. Filter frequency is interpolated logarithmically,
// all other parameters linearly. Snapshots are not audio buffers.
inline Patch interpolateScenes (const std::array<Patch, 4>& s, float x, float y) noexcept
{
    x = clamp01 (x);
    y = clamp01 (y);
    const float wA = (1.0f - x) * (1.0f - y);
    const float wB = x * (1.0f - y);
    const float wC = (1.0f - x) * y;
    const float wD = x * y;
    const auto blend = [&] (float Patch::* field)
    {
        return wA * s[0].*field + wB * s[1].*field + wC * s[2].*field + wD * s[3].*field;
    };
    Patch p;
    p.detuneA = blend (&Patch::detuneA);
    p.widthA = blend (&Patch::widthA);
    p.motionA = blend (&Patch::motionA);
    p.detuneB = blend (&Patch::detuneB);
    p.widthB = blend (&Patch::widthB);
    p.motionB = blend (&Patch::motionB);
    p.mix = blend (&Patch::mix);
    p.resonance = blend (&Patch::resonance);
    p.drive = blend (&Patch::drive);
    p.attack = blend (&Patch::attack);
    p.release = blend (&Patch::release);
    p.output = blend (&Patch::output);
    p.cutoff = std::exp (wA * std::log (std::max (s[0].cutoff, 10.0f))
                       + wB * std::log (std::max (s[1].cutoff, 10.0f))
                       + wC * std::log (std::max (s[2].cutoff, 10.0f))
                       + wD * std::log (std::max (s[3].cutoff, 10.0f)));
    return p;
}

class SwarmEngine
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = std::max (newSampleRate, 8000.0);
        smoothingAlpha = 1.0f - std::exp (-1.0f / (0.012f * static_cast<float> (sampleRate)));
        allNotesOff();
        ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0.0f;
        filterUpdateCountdown = 0;
        current = target;
        setPitchBend (8192);
    }

    void setPatch (const Patch& p) noexcept { target = p; }
    void setVoiceLimit (int voices) noexcept { voiceLimit = std::clamp (voices, 1, maxPolyphony); }
    int getActiveVoiceCount() const noexcept
    {
        int n = 0;
        for (const auto& v : voicePool)
            if (v.active) ++n;
        return n;
    }

    void noteOn (int midiNote, float velocity) noexcept
    {
        midiNote = std::clamp (midiNote, 0, 127);
        velocity = clamp01 (velocity);
        if (velocity == 0.0f) { noteOff (midiNote); return; }

        int selected = -1;
        for (int i = 0; i < voiceLimit; ++i)
            if (! voicePool[static_cast<size_t> (i)].active) { selected = i; break; }

        if (selected < 0)
        {
            // Steal the quietest voice, preferring notes already releasing.
            float lowest = 10000.0f;
            for (int i = 0; i < voiceLimit; ++i)
            {
                const auto& v = voicePool[static_cast<size_t> (i)];
                const float score = v.envelope + (v.held ? 1.0f : 0.0f);
                if (score < lowest) { lowest = score; selected = i; }
            }
        }

        auto& v = voicePool[static_cast<size_t> (selected)];
        v.active = true;
        v.held = true;
        v.sustained = false;
        v.note = midiNote;
        v.baseFrequency = 440.0f * std::pow (2.0f, (static_cast<float> (midiNote) - 69.0f) / 12.0f);
        v.velocity = velocity;
        v.envelope = 0.0f;
        // Deterministic phases for reproducibility and a repeatable stereo image.
        for (int b = 0; b < 2; ++b)
            for (int o = 0; o < oscillatorsPerSwarm; ++o)
            {
                auto& osc = v.osc[b][o];
                const float seed = static_cast<float> ((midiNote * 17 + selected * 29 + b * 37 + o * 71) % 997) / 997.0f;
                osc.phase = seed;
                osc.motionPhase = seed * 0.73f + static_cast<float> (o) / static_cast<float> (oscillatorsPerSwarm);
                osc.motionPhase -= std::floor (osc.motionPhase);
            }
    }

    void noteOff (int midiNote) noexcept
    {
        for (auto& v : voicePool)
            if (v.active && v.note == midiNote && v.held)
            {
                v.held = false;
                v.sustained = sustainPedal;
            }
    }

    void setSustainPedal (bool down) noexcept
    {
        sustainPedal = down;
        if (! down)
            for (auto& v : voicePool) v.sustained = false;
    }

    void setPitchBend (int value) noexcept
    {
        pitchFactor = std::pow (2.0f, ((std::clamp (value, 0, 16383) - 8192) / 8192.0f) * (2.0f / 12.0f));
    }

    void allNotesOff() noexcept
    {
        for (auto& v : voicePool) v = Voice{};
        sustainPedal = false;
    }

    void renderSample (float& outputLeft, float& outputRight) noexcept
    {
        smoothPatch();
        if (--filterUpdateCountdown <= 0) { updateFilter(); filterUpdateCountdown = 16; }

        float outL = 0.0f, outR = 0.0f;
        const float attackCoef = 1.0f - std::exp (-1.0f / (std::max (0.005f, current.attack) * static_cast<float> (sampleRate)));
        const float releaseCoef = 1.0f - std::exp (-1.0f / (std::max (0.03f, current.release) * static_cast<float> (sampleRate)));

        for (int vi = 0; vi < voiceLimit; ++vi)
        {
            auto& v = voicePool[static_cast<size_t> (vi)];
            if (! v.active) continue;
            const bool isHeld = v.held || v.sustained;
            v.envelope += ((isHeld ? 1.0f : 0.0f) - v.envelope) * (isHeld ? attackCoef : releaseCoef);
            if (! isHeld && v.envelope < 0.00008f) { v.active = false; continue; }

            const float frequency = v.baseFrequency * pitchFactor;
            float aL = 0.0f, aR = 0.0f, bL = 0.0f, bR = 0.0f;
            renderBank (v.osc[0], frequency, current.detuneA, current.widthA, current.motionA, aL, aR, false);
            renderBank (v.osc[1], frequency, current.detuneB, current.widthB, current.motionB, bL, bR, true);
            const float gain = v.envelope * (0.28f + 0.72f * v.velocity);
            outL += (aL * (1.0f - current.mix) + bL * current.mix) * gain;
            outR += (aR * (1.0f - current.mix) + bR * current.mix) * gain;
        }

        // Voice normalization keeps chords usable without drastic level jumps.
        outL *= 0.25f;
        outR *= 0.25f;
        outL = lowpass (outL, 0);
        outR = lowpass (outR, 1);

        const float saturation = 1.0f + 11.0f * current.drive * current.drive;
        const float makeup = 1.0f / std::tanh (saturation);
        outputLeft = std::tanh (outL * saturation) * makeup * current.output;
        outputRight = std::tanh (outR * saturation) * makeup * current.output;
    }

private:
    struct Oscillator { float phase = 0.0f; float motionPhase = 0.0f; };
    struct Voice
    {
        bool active = false, held = false, sustained = false;
        int note = -1;
        float baseFrequency = 440.0f, velocity = 0.0f, envelope = 0.0f;
        Oscillator osc[2][oscillatorsPerSwarm] {};
    };

    static float polyBlep (float t, float dt) noexcept
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0f;
        }
        if (t > 1.0f - dt)
        {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }

    static float triangle (float phase) noexcept
    {
        return 1.0f - 4.0f * std::abs (phase - 0.5f);
    }

    void renderBank (Oscillator (&bank)[oscillatorsPerSwarm], float rootFrequency,
                     float detune, float width, float motion,
                     float& left, float& right, bool invertPan) noexcept
    {
        constexpr float invOscillators = 1.0f / static_cast<float> (oscillatorsPerSwarm);
        for (int i = 0; i < oscillatorsPerSwarm; ++i)
        {
            auto& o = bank[i];
            const float position = (2.0f * static_cast<float> (i) / static_cast<float> (oscillatorsPerSwarm - 1)) - 1.0f;
            const float movement = triangle (o.motionPhase);
            const float cents = detune * position + 7.5f * motion * movement;
            // Second-order Taylor approx of 2^(cents/1200): avoids millions of exp2 calls.
            const float ratio = 1.0f + 0.00057762265f * cents + 0.000000166825f * cents * cents;
            const float delta = std::clamp (rootFrequency * ratio / static_cast<float> (sampleRate), 0.0000001f, 0.48f);
            const float saw = 2.0f * o.phase - 1.0f - polyBlep (o.phase, delta);
            o.phase += delta;
            if (o.phase >= 1.0f) o.phase -= 1.0f;

            const float rate = 0.08f + static_cast<float> (i + 1) * 0.037f + (invertPan ? 0.023f : 0.0f);
            o.motionPhase += rate / static_cast<float> (sampleRate);
            if (o.motionPhase >= 1.0f) o.motionPhase -= 1.0f;

            const float panMotion = triangle (o.motionPhase + (o.motionPhase > 0.63f ? -0.63f : 0.37f));
            const float pan = clamp01 (0.5f + (invertPan ? -1.0f : 1.0f) * position * width * 0.45f
                                         + panMotion * motion * 0.12f);
            const float gain = saw * (1.0f + movement * motion * 0.18f) * invOscillators;
            left += gain * (1.0f - pan);
            right += gain * pan;
        }
    }

    void smoothPatch() noexcept
    {
        const auto sm = [this] (float& a, float b) { a += (b - a) * smoothingAlpha; };
        sm (current.detuneA, target.detuneA);
        sm (current.widthA, target.widthA);
        sm (current.motionA, target.motionA);
        sm (current.detuneB, target.detuneB);
        sm (current.widthB, target.widthB);
        sm (current.motionB, target.motionB);
        sm (current.mix, target.mix);
        // Smoothing logarithmically isn't necessary for small per-sample steps,
        // while the XY interpolation itself is already logarithmic.
        sm (current.cutoff, target.cutoff);
        sm (current.resonance, target.resonance);
        sm (current.drive, target.drive);
        sm (current.attack, target.attack);
        sm (current.release, target.release);
        sm (current.output, target.output);
    }

    void updateFilter() noexcept
    {
        const float fc = std::clamp (current.cutoff, 100.0f,
                                    std::min (18000.0f, static_cast<float> (sampleRate * 0.44)));
        const float g = std::tan (static_cast<float> (pi) * fc / static_cast<float> (sampleRate));
        const float q = 0.55f + clamp01 (current.resonance) * 7.5f;
        const float k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    float lowpass (float sample, int channel) noexcept
    {
        // Topology-preserving SVF, low-pass output. ic values are channel-local.
        const float v3 = sample - ic2[channel];
        const float v1 = a1 * ic1[channel] + a2 * v3;
        const float v2 = ic2[channel] + a2 * ic1[channel] + a3 * v3;
        ic1[channel] = 2.0f * v1 - ic1[channel];
        ic2[channel] = 2.0f * v2 - ic2[channel];
        if (std::abs (ic1[channel]) < 1.0e-20f) ic1[channel] = 0.0f;
        if (std::abs (ic2[channel]) < 1.0e-20f) ic2[channel] = 0.0f;
        return v2;
    }

    std::array<Voice, maxPolyphony> voicePool {};
    int voiceLimit = 8;
    double sampleRate = 48000.0;
    bool sustainPedal = false;
    float pitchFactor = 1.0f;
    Patch current {}, target {};
    float smoothingAlpha = 0.01f;
    float ic1[2] {}, ic2[2] {};
    float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f;
    int filterUpdateCountdown = 0;
};
} // namespace anks
