#include "../Source/PluginProcessor.h"
#include <cmath>
#include <iostream>
#include <vector>

namespace
{
constexpr int blockSize = 256;
constexpr double sampleRate = 48000.0;

juce::Slider* findSlider (juce::Component& parent, const juce::String& id)
{
    if (parent.getComponentID() == id)
        return dynamic_cast<juce::Slider*> (&parent);

    for (auto* child : parent.getChildren())
        if (auto* slider = findSlider (*child, id))
            return slider;

    return nullptr;
}

void setParameter (SwarmProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.parameters.getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

std::vector<float> render (SwarmProcessor& processor)
{
    processor.prepareToPlay (sampleRate, blockSize);
    juce::AudioBuffer<float> audio (2, blockSize);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 52, 0.85f), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 59, 0.8f), 0);
    std::vector<float> result;
    for (int block = 0; block < 180; ++block)
    {
        if (block == 100)
            for (const int note : {52, 59})
                midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
        processor.processBlock (audio, midi);
        midi.clear();
        for (int channel = 0; channel < 2; ++channel)
            result.insert (result.end(), audio.getReadPointer (channel),
                           audio.getReadPointer (channel) + blockSize);
    }
    return result;
}

double rms (const std::vector<float>& signal, const std::vector<float>* comparison = nullptr)
{
    double sum = 0.0;
    for (size_t i = 0; i < signal.size(); ++i)
    {
        const auto sample = signal[i] - (comparison != nullptr ? (*comparison)[i] : 0.0f);
        sum += static_cast<double> (sample) * sample;
    }
    return std::sqrt (sum / static_cast<double> (signal.size()));
}

bool checkControl (const char* id)
{
    SwarmProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    auto* slider = findSlider (*editor, id);
    if (slider == nullptr)
        return false;
    const juce::String name (id);
    if (name.endsWith ("A") || name.endsWith ("B"))
        setParameter (processor, "mix", name.endsWith ("A") ? 0.0f : 1.0f);

    slider->setValue (slider->getMinimum(), juce::sendNotificationSync);
    const auto lowParam = processor.parameters.getRawParameterValue (id)->load();
    const auto low = render (processor);
    slider->setValue (slider->getMaximum(), juce::sendNotificationSync);
    const auto highParam = processor.parameters.getRawParameterValue (id)->load();
    const auto high = render (processor);

    const auto relativeDifference = rms (low, &high) / std::max (rms (low), rms (high));
    std::cout << "LIVE " << id << " " << lowParam << " -> "
              << highParam << ", relative signal difference " << relativeDifference << '\n';
    const bool response = lowParam < highParam && std::isfinite (relativeDifference)
                          && relativeDifference > 0.02;
    return response && (name != "output" || rms (low) < 0.0001);
}
}

int main()
{
    const juce::ScopedJuceInitialiser_GUI juce;
    bool passed = true;
    for (const auto* id : {"detuneA", "widthA", "motionA", "detuneB", "widthB", "motionB",
                           "mix", "cutoff", "resonance", "drive", "attack", "release", "output"})
    {
        passed = checkControl (id) && passed;
    }
    SwarmProcessor morph;
    setParameter (morph, "morphOn", 1.0f);
    setParameter (morph, "morphX", 0.0f);
    setParameter (morph, "morphY", 0.0f);
    const auto top = render (morph);
    setParameter (morph, "morphY", 1.0f);
    const auto bottom = render (morph);
    const auto scene = morph.getEffectivePatch();
    passed = passed && rms (top, &bottom) > 0.02 && std::abs (scene.cutoff - morph.getScene (2).cutoff) < 0.1f;
    morph.recallScene (2);
    passed = passed && morph.parameters.getRawParameterValue ("morphOn")->load() < 0.5f
             && std::abs (morph.getEffectivePatch().cutoff - scene.cutoff) < 0.1f;
    juce::MemoryBlock state;
    morph.getStateInformation (state);
    SwarmProcessor reopened;
    reopened.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    passed = passed && std::abs (reopened.getEffectivePatch().cutoff - scene.cutoff) < 0.1f;

    juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Standalone);
    SwarmProcessor standalone;
    juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Undefined);
    std::unique_ptr<juce::AudioProcessorEditor> standaloneEditor (standalone.createEditor());
    standalone.prepareToPlay (sampleRate, blockSize);
    bool keyboardVisible = false;
    for (auto* component : standaloneEditor->getChildren())
        if (dynamic_cast<juce::MidiKeyboardComponent*> (component) != nullptr)
            keyboardVisible = component->isVisible() && ! component->getBounds().isEmpty();

    juce::AudioBuffer<float> audio (2, blockSize);
    juce::MidiBuffer midi;
    standalone.processBlock (audio, midi);
    const bool silentWithoutInput = audio.getMagnitude (0, blockSize) == 0.0f;
    standalone.auditionKeyboard.noteOn (1, 60, 0.85f);
    for (int block = 0; block < 16; ++block)
    {
        midi.clear();
        standalone.processBlock (audio, midi);
    }
    const auto keyboardPeak = audio.getMagnitude (0, blockSize);
    std::cout << "Standalone keyboard signal peak: " << keyboardPeak << '\n';
    standalone.prepareToPlay (sampleRate, blockSize);
    standalone.setAuditionChord (true);
    for (int block = 0; block < 16; ++block)
        standalone.processBlock (audio, midi);
    const auto chordPeak = audio.getMagnitude (0, blockSize);
    std::cout << "Standalone chord signal peak: " << chordPeak << '\n';
    return passed && keyboardVisible && silentWithoutInput && keyboardPeak > 0.01f
           && chordPeak > 0.01f ? 0 : 1;
}
