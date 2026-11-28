#include "../Source/PluginProcessor.h"
#include <cmath>
#include <iostream>
#include <vector>

namespace
{
constexpr int blockSize = 256;
constexpr double sampleRate = 48000.0;

template <typename ComponentType = juce::Slider>
ComponentType* findControl (juce::Component& parent, const juce::String& id)
{
    if (parent.getComponentID() == id)
        return dynamic_cast<ComponentType*> (&parent);

    for (auto* child : parent.getChildren())
        if (auto* slider = findControl<ComponentType> (*child, id))
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
    auto* slider = findControl (*editor, id);
    if (slider == nullptr)
        return false;
    const juce::String name (id);
    if (name.endsWith ("A") || name.endsWith ("B"))
        setParameter (processor, "mix", name.endsWith ("A") ? 0.0f : 1.0f);

    if (name == "lfoRate")
        setParameter (processor, "lfoDepth", 0.85f);
    if (name == "delayTime" || name == "delayFeedback")
        setParameter (processor, "delayMix", 0.65f);

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
                           "mix", "cutoff", "resonance", "drive", "attack", "release", "output",
                           "tuneA", "densityA", "shapeA", "tuneB", "densityB", "shapeB",
                           "lfoRate", "lfoDepth", "delayMix", "delayTime", "delayFeedback"})
    {
        passed = checkControl (id) && passed;
    }
    {
        SwarmProcessor editing;
        setParameter (editing, "morphOn", 1.0f);
        setParameter (editing, "morphX", 0.25f);
        setParameter (editing, "morphY", 0.0f);
        std::unique_ptr<juce::AudioProcessorEditor> editor (editing.createEditor());
        auto* cornerB = findControl<juce::TextButton> (*editor, "corner-B");
        auto* cutoff = findControl (*editor, "scene-cutoff");
        if (cornerB == nullptr || cutoff == nullptr || ! cutoff->isVisible())
            return 1;

        const auto liveCutoff = editing.parameters.getRawParameterValue ("cutoff")->load();
        const auto cutoffA = editing.getScene (0).cutoff;
        const auto initial = editing.getEffectivePatch().cutoff;
        const auto before = render (editing);
        cornerB->onClick();
        passed = passed && std::abs (cutoff->getValue() - editing.getScene (1).cutoff) < 0.1f
                 && std::abs (editing.getEffectivePatch().cutoff - initial) < 0.1f;

        cutoff->setValue (100.0, juce::sendNotificationSync);
        const auto effective = editing.getEffectivePatch().cutoff;
        const auto expected = std::pow (cutoffA, 0.75f) * std::pow (100.0f, 0.25f);
        const auto after = render (editing);
        juce::MemoryBlock saved;
        editing.getStateInformation (saved);
        SwarmProcessor restoredEdit;
        restoredEdit.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
        passed = passed && std::abs (effective - expected) < 0.1f && rms (before, &after) > 0.001
                 && std::abs (editing.getScene (0).cutoff - cutoffA) < 0.1f
                 && std::abs (editing.getScene (1).cutoff - 100.0f) < 0.1f
                 && std::abs (editing.parameters.getRawParameterValue ("cutoff")->load() - liveCutoff) < 0.1f
                 && editing.parameters.getRawParameterValue ("morphOn")->load() > 0.5f
                 && std::abs (editing.parameters.getRawParameterValue ("morphX")->load() - 0.25f) < 0.0001f
                 && std::abs (editing.parameters.getRawParameterValue ("morphY")->load()) < 0.0001f
                 && std::abs (restoredEdit.getEffectivePatch().cutoff - effective) < 0.1f;
        std::cout << "Scene B edit at fixed XY: " << initial << " -> " << effective << " Hz\n";
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
    setParameter (morph, "delayMix", 0.5f);
    morph.captureScene (2);
    morph.getStateInformation (state);
    SwarmProcessor reopened;
    reopened.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    passed = passed && std::abs (reopened.getEffectivePatch().cutoff - scene.cutoff) < 0.1f
             && std::abs (reopened.getScene (2).delayMix - 0.5f) < 0.001f;

    const auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), static_cast<int> (state.getSize()));
    auto oldState = juce::ValueTree::fromXml (*xml);
    oldState.removeChild (oldState.getChildWithProperty ("id", "delayMix"), nullptr);
    for (auto sceneNode : oldState.getChildWithName ("SwarmScenes"))
        sceneNode.removeProperty ("p21", nullptr);
    juce::MemoryBlock oldBinary;
    const auto oldXml = oldState.createXml();
    juce::AudioProcessor::copyXmlToBinary (*oldXml, oldBinary);
    reopened.setStateInformation (oldBinary.getData(), static_cast<int> (oldBinary.getSize()));
    passed = passed && reopened.getEffectivePatch().delayMix < 0.001f
             && reopened.getScene (2).delayMix < 0.001f;

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
