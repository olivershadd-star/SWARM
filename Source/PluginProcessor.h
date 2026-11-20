#pragma once
#include <JuceHeader.h>
#include "DSP/SwarmEngine.h"
#include <array>
#include <atomic>

class SwarmProcessor final : public juce::AudioProcessor
{
public:
    SwarmProcessor();
    ~SwarmProcessor() override = default;

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ANKS SWARM 01"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "User Patch"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState parameters;
    void captureScene (int sceneIndex) noexcept;
    anks::Patch getScene (int sceneIndex) const noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameterLayout();
    anks::Patch readLivePatch() const noexcept;
    static constexpr int patchFieldCount = 13;
    struct AtomicScene
    {
        std::array<std::atomic<float>, patchFieldCount> data {};
        void store (const anks::Patch& p) noexcept;
        anks::Patch load() const noexcept;
    };
    std::array<AtomicScene, 4> scenes {};
    anks::SwarmEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwarmProcessor)
};
