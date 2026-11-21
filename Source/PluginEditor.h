#pragma once

#include "PluginProcessor.h"

class SwarmEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SwarmEditor (SwarmProcessor&);
    ~SwarmEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Theme final : public juce::LookAndFeel_V4
    {
        void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    };
    class Control;
    class MorphPad;
    SwarmProcessor& processor;
    Theme theme;
    std::array<std::unique_ptr<Control>, 13> controls;
    std::unique_ptr<MorphPad> pad;
    std::array<juce::TextButton, 4> recallButtons, saveButtons;
    juce::TextButton hearA { "A seul" }, hearB { "B seul" };
    juce::ToggleButton morph { "MORPH" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> morphAttachment;
    juce::TextButton audition { "Jouer un accord" };
    std::unique_ptr<juce::MidiKeyboardComponent> keyboard;
    juce::Label modeLabel;
    juce::Rectangle<int> header, bankA, bankB, morphPanel, master, keyboardRow;
    bool viewingMorph = false;

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwarmEditor)
};
