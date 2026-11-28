#include "../Source/DSP/SwarmEngine.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

static void writeLE16 (std::ofstream& f, uint16_t value)
{
    const char b[] = {static_cast<char> (value & 255), static_cast<char> ((value >> 8) & 255)};
    f.write (b, 2);
}
static void writeLE32 (std::ofstream& f, uint32_t value)
{
    writeLE16 (f, static_cast<uint16_t> (value & 65535));
    writeLE16 (f, static_cast<uint16_t> ((value >> 16) & 65535));
}
static void saveWav (const char* filename, const std::vector<float>& stereo, int sampleRate)
{
    std::ofstream f (filename, std::ios::binary);
    if (! f) { std::cerr << "Cannot create WAV: " << filename << '\n'; return; }
    const uint32_t bytes = static_cast<uint32_t> (stereo.size() * 2);
    f.write ("RIFF", 4); writeLE32 (f, 36 + bytes); f.write ("WAVEfmt ", 8);
    writeLE32 (f, 16); writeLE16 (f, 1); writeLE16 (f, 2);
    writeLE32 (f, static_cast<uint32_t> (sampleRate));
    writeLE32 (f, static_cast<uint32_t> (sampleRate * 4));
    writeLE16 (f, 4); writeLE16 (f, 16);
    f.write ("data", 4); writeLE32 (f, bytes);
    for (auto sample : stereo)
    {
        const auto clamped = std::clamp (sample, -1.0f, 1.0f);
        const int16_t v = static_cast<int16_t> (std::lround (clamped * 32767));
        writeLE16 (f, static_cast<uint16_t> (v));
    }
}
int main (int argc, char** argv)
{
    constexpr int sr = 48000;
    {
        anks::SwarmEngine reference, fading;
        anks::Patch patch;
        reference.prepare (sr);
        fading.prepare (sr);
        reference.noteOn (57, 0.85f);
        fading.noteOn (57, 0.85f);
        float refL = 0.0f, refR = 0.0f, fadeL = 0.0f, fadeR = 0.0f;
        for (int i = 0; i < sr / 4; ++i)
        {
            reference.renderSample (refL, refR);
            fading.renderSample (fadeL, fadeR);
        }
        patch.output = 0.0f;
        fading.setPatch (patch);
        for (int i = 0; i < 576; ++i) // 12 ms at 48 kHz, three time constants.
        {
            reference.renderSample (refL, refR);
            fading.renderSample (fadeL, fadeR);
        }
        const float referenceMagnitude = std::max (std::abs (refL), std::abs (refR));
        const float fadedMagnitude = std::max (std::abs (fadeL), std::abs (fadeR));
        if (referenceMagnitude < 1.0e-6f || std::abs (fadedMagnitude / referenceMagnitude - std::exp (-3.0f)) > 0.001f)
        {
            std::cerr << "Output smoothing did not reach 95% in about 12 ms\n";
            return 1;
        }
    }
    anks::SwarmEngine engine;
    engine.prepare (sr);
    anks::Patch a, b, c, d;
    b.detuneA = 43.0f; b.motionA = 0.7f; b.cutoff = 7000.0f;
    c.detuneB = 58.0f; c.motionB = 0.8f; c.drive = 0.45f;
    d.detuneA = 56.0f; d.detuneB = 51.0f; d.cutoff = 3600.0f; d.drive = 0.52f;
    const std::array<anks::Patch, 4> scenes = {a, b, c, d};
    const auto atA = anks::interpolateScenes (scenes, 0.0f, 0.0f);
    const auto atD = anks::interpolateScenes (scenes, 1.0f, 1.0f);
    if (! (atA.detuneA == a.detuneA && atD.detuneA == d.detuneA))
    {
        std::cerr << "DSP check failed at line " << __LINE__ << '\n';
        return 1;
    }
    const auto center = anks::interpolateScenes (scenes, 0.5f, 0.5f);
    if (! (std::abs (center.detuneA - (a.detuneA+b.detuneA+c.detuneA+d.detuneA)*0.25f) < 0.0001f))
    {
        std::cerr << "DSP check failed at line " << __LINE__ << '\n';
        return 1;
    }
    engine.setPatch (a);
    engine.noteOn (48, 0.94f);
    engine.noteOn (55, 0.78f);
    engine.noteOn (60, 0.83f);
    std::vector<float> samples;
    samples.reserve (sr * 4 * 2);
    float peak = 0.0f, energy = 0.0f;
    for (int i = 0; i < sr * 4; ++i)
    {
        if (i == sr) engine.setPatch (anks::interpolateScenes (scenes, 0.8f, 0.25f));
        if (i == sr * 2) engine.setPatch (anks::interpolateScenes (scenes, 0.8f, 0.9f));
        if (i == sr * 3) { engine.noteOff (48); engine.noteOff (55); engine.noteOff (60); }
        float l = 0, r = 0;
        engine.renderSample (l, r);
        if (! (std::isfinite (l) && std::isfinite (r)))
        {
            std::cerr << "DSP check failed at line " << __LINE__ << '\n';
            return 1;
        }
        samples.push_back (l); samples.push_back (r);
        peak = std::max ({peak, std::abs (l), std::abs (r)});
        energy += l*l + r*r;
    }
    std::cout << "DSP tests passed; peak=" << peak << ", RMS=" << std::sqrt (energy / static_cast<float> (samples.size()))
              << ", voices=" << engine.getActiveVoiceCount() << '\n';
    if (! (peak > 0.001f && peak <= 1.0f && energy > 1.0f))
    {
        std::cerr << "DSP check failed at line " << __LINE__ << '\n';
        return 1;
    }
    if (argc > 1) { saveWav (argv[1], samples, sr); std::cout << "WAV rendered: " << argv[1] << '\n'; }
}
