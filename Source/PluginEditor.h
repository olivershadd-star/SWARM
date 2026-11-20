#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>
#include <memory>
#include <vector>

class SwarmEditor final : public juce::AudioProcessorEditor
{
public:
    explicit SwarmEditor (SwarmProcessor&);
    ~SwarmEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class SwarmLookAndFeel;
    class Knob;
    class MorphPad;
    SwarmProcessor& processor;
    std::unique_ptr<SwarmLookAndFeel> theme;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::unique_ptr<MorphPad> pad;
    std::array<juce::TextButton, 4> captureButtons;
    juce::ToggleButton morphToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> morphAttachment;
    juce::Label status;

    juce::Rectangle<int> leftPanel, rightPanel, morphPanel, bottomPanel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwarmEditor)
};
