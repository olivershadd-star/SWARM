#include "../Source/DSP/SwarmEngine.h"
#include <array>
#include <cmath>
#include <iostream>

int main()
{
    for (const int sampleRate : {8000, 44100, 48000, 96000})
    {
        anks::SwarmEngine synth;
        synth.prepare (sampleRate);
        anks::Patch patch;
        patch.detuneA = 60.0f; patch.detuneB = 60.0f;
        patch.widthA = patch.widthB = patch.motionA = patch.motionB = 1.0f;
        patch.cutoff = 18000.0f; patch.resonance = 1.0f;
        patch.drive = 1.0f; patch.output = 1.0f;
        patch.attack = 0.005f; patch.release = 0.03f;
        synth.setPatch (patch);
        for (int n = 0; n < 8; ++n) synth.noteOn (100 + n, 1.0f);
        synth.setPitchBend (16383);
        synth.setSustainPedal (true);
        float peak = 0.0f;
        for (int i = 0; i < sampleRate / 2; ++i)
        {
            if (i == sampleRate / 4)
            {
                for (int n = 0; n < 8; ++n) synth.noteOff (100 + n);
            }
            float l = 0.0f, r = 0.0f;
            synth.renderSample (l, r);
            if (! (std::isfinite (l) && std::isfinite (r)))
            {
                std::cerr << "DSP check failed at line " << __LINE__ << '\n';
                return 1;
            }
            peak = std::max ({peak, std::abs (l), std::abs (r)});
        }
        synth.setSustainPedal (false);
        if (! (peak > 0.0f && peak <= 1.0001f))
        {
            std::cerr << "DSP check failed at line " << __LINE__ << '\n';
            return 1;
        }
        std::cout << sampleRate << " Hz : stress OK (peak=" << peak << ")\n";
    }
}
