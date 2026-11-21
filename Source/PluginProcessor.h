#pragma once
#include <JuceHeader.h>
#include "DSP/SwarmEngine.h"
#include <array>
#include <atomic>
#include <mutex>

class SwarmProcessor final : public juce::AudioProcessor, private juce::MidiKeyboardState::Listener
{
public:
    SwarmProcessor();
    ~SwarmProcessor() override;

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
    double getTailLengthSeconds() const override { return 32.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "User Patch"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState parameters;
    void captureScene (int sceneIndex);
    void recallScene (int sceneIndex);
    anks::Patch getScene (int sceneIndex) const;
    anks::Patch getEffectivePatch() const noexcept;
    juce::MidiKeyboardState auditionKeyboard;
    void setAuditionChord (bool playing) noexcept;
    float getOutputPeak() const noexcept { return outputPeak.load (std::memory_order_relaxed); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameterLayout();
    anks::Patch readLivePatch() const noexcept;
    static constexpr int patchFieldCount = 24;
    struct AtomicScene
    {
        std::array<std::atomic<float>, patchFieldCount> data {};
        std::atomic<unsigned> generation { 0 };
        void store (const anks::Patch& p) noexcept;
        anks::Patch load() const noexcept;
        bool tryLoad (anks::Patch& destination) const noexcept;
    };
    std::array<AtomicScene, 4> scenes {};
    mutable std::mutex sceneWriteMutex;
    std::array<anks::Patch, 4> audioScenes {};
    std::atomic<bool> auditionChord { false };
    std::atomic<float> outputPeak { 0.0f };
    bool chordPlaying = false;
    static constexpr int firstKey = 48;
    static constexpr int lastKey = 83;
    std::array<std::atomic<float>, lastKey - firstKey + 1> keyboardVelocity {};
    std::array<bool, lastKey - firstKey + 1> keyboardPlaying {};
    void handleNoteOn (juce::MidiKeyboardState*, int, int note, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int, int note, float) override;
    anks::SwarmEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwarmProcessor)
};
