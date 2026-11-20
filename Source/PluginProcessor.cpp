#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
using Patch = anks::Patch;
constexpr std::array<float Patch::*, 13> patchFields {
    &Patch::detuneA, &Patch::widthA, &Patch::motionA,
    &Patch::detuneB, &Patch::widthB, &Patch::motionB,
    &Patch::mix, &Patch::cutoff, &Patch::resonance,
    &Patch::drive, &Patch::attack, &Patch::release, &Patch::output
};

std::array<Patch, 4> factoryScenes()
{
    std::array<Patch, 4> p {};
    p[0].detuneA = 10.0f; p[0].detuneB = 21.0f;
    p[0].widthA = 0.38f; p[0].widthB = 0.64f; p[0].motionA = 0.10f; p[0].motionB = 0.12f;
    p[0].cutoff = 11500.0f; p[0].drive = 0.04f;

    p[1].detuneA = 40.0f; p[1].detuneB = 32.0f;
    p[1].widthA = 0.96f; p[1].widthB = 0.83f; p[1].motionA = 0.30f; p[1].motionB = 0.42f;
    p[1].cutoff = 13500.0f; p[1].drive = 0.08f;

    p[2].detuneA = 18.0f; p[2].detuneB = 57.0f;
    p[2].widthA = 0.57f; p[2].widthB = 0.92f; p[2].motionA = 0.68f; p[2].motionB = 0.76f;
    p[2].cutoff = 2700.0f; p[2].resonance = 0.28f; p[2].drive = 0.33f;

    p[3].detuneA = 55.0f; p[3].detuneB = 52.0f;
    p[3].widthA = 0.99f; p[3].widthB = 0.99f; p[3].motionA = 0.92f; p[3].motionB = 0.94f;
    p[3].cutoff = 4200.0f; p[3].resonance = 0.38f; p[3].drive = 0.51f;
    return p;
}
}

void SwarmProcessor::AtomicScene::store (const anks::Patch& p) noexcept
{
    for (size_t i = 0; i < data.size(); ++i)
        data[i].store (p.*patchFields[i], std::memory_order_relaxed);
}

anks::Patch SwarmProcessor::AtomicScene::load() const noexcept
{
    anks::Patch p;
    for (size_t i = 0; i < data.size(); ++i)
        p.*patchFields[i] = data[i].load (std::memory_order_relaxed);
    return p;
}

juce::AudioProcessorValueTreeState::ParameterLayout SwarmProcessor::makeParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto add = [&layout] (const char* id, const char* label, float low, float high, float initial, float centre = 0.0f)
    {
        juce::NormalisableRange<float> range { low, high };
        if (centre > low && centre < high) range.setSkewForCentre (centre);
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {id, 1}, label, range, initial));
    };
    add ("detuneA",  "A / Detune",   0.0f, 60.0f, 15.0f);
    add ("widthA",   "A / Width",    0.0f, 1.0f,  0.65f);
    add ("motionA",  "A / Motion",   0.0f, 1.0f,  0.17f);
    add ("detuneB",  "B / Detune",   0.0f, 60.0f, 29.0f);
    add ("widthB",   "B / Width",    0.0f, 1.0f,  0.83f);
    add ("motionB",  "B / Motion",   0.0f, 1.0f,  0.30f);
    add ("mix",      "Swarm Mix",    0.0f, 1.0f,  0.42f);
    add ("cutoff",   "Cutoff",     100.0f, 18000.0f, 9500.0f, 1700.0f);
    add ("resonance","Resonance",   0.0f, 1.0f,  0.12f);
    add ("drive",    "Drive",       0.0f, 1.0f,  0.10f);
    add ("attack",   "Attack",    0.005f, 3.0f,  0.015f, 0.15f);
    add ("release",  "Release",    0.03f, 8.0f,  0.70f, 0.65f);
    add ("output",   "Output",      0.0f, 1.0f,  0.78f);
    add ("morphX",   "Morph X",     0.0f, 1.0f,  0.5f);
    add ("morphY",   "Morph Y",     0.0f, 1.0f,  0.5f);
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID {"morphOn", 1}, "Morph Enable", false));
    return layout;
}

SwarmProcessor::SwarmProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "ANKSParameters", makeParameterLayout())
{
    const auto defaults = factoryScenes();
    for (size_t i = 0; i < scenes.size(); ++i) scenes[i].store (defaults[i]);
}

void SwarmProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    engine.setVoiceLimit (8);
}

void SwarmProcessor::releaseResources() { engine.allNotesOff(); }

bool SwarmProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet().isDisabled()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

anks::Patch SwarmProcessor::readLivePatch() const noexcept
{
    const auto read = [this] (const char* id) { return parameters.getRawParameterValue (id)->load (std::memory_order_relaxed); };
    anks::Patch p;
    p.detuneA = read ("detuneA"); p.widthA = read ("widthA"); p.motionA = read ("motionA");
    p.detuneB = read ("detuneB"); p.widthB = read ("widthB"); p.motionB = read ("motionB");
    p.mix = read ("mix"); p.cutoff = read ("cutoff"); p.resonance = read ("resonance");
    p.drive = read ("drive"); p.attack = read ("attack"); p.release = read ("release");
    p.output = read ("output");
    return p;
}

void SwarmProcessor::captureScene (int sceneIndex) noexcept
{
    if (sceneIndex >= 0 && sceneIndex < static_cast<int> (scenes.size()))
        scenes[static_cast<size_t> (sceneIndex)].store (readLivePatch());
}

anks::Patch SwarmProcessor::getScene (int sceneIndex) const noexcept
{
    if (sceneIndex >= 0 && sceneIndex < static_cast<int> (scenes.size()))
        return scenes[static_cast<size_t> (sceneIndex)].load();
    return {};
}

void SwarmProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0) return;

    const auto read = [this] (const char* id) { return parameters.getRawParameterValue (id)->load (std::memory_order_relaxed); };
    anks::Patch target = readLivePatch();
    if (read ("morphOn") >= 0.5f)
    {
        const std::array<anks::Patch, 4> sceneValues = {
            scenes[0].load(), scenes[1].load(), scenes[2].load(), scenes[3].load()
        };
        target = anks::interpolateScenes (sceneValues, read ("morphX"), read ("morphY"));
    }
    engine.setPatch (target);

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);
    auto event = midi.begin();
    const auto end = midi.end();
    for (int sample = 0; sample < numSamples; ++sample)
    {
        while (event != end && event->samplePosition <= sample)
        {
            const auto message = event->getMessage();
            if (message.isNoteOn())
                engine.noteOn (message.getNoteNumber(), message.getVelocity() / 127.0f);
            else if (message.isNoteOff())
                engine.noteOff (message.getNoteNumber());
            else if (message.isPitchWheel())
                engine.setPitchBend (message.getPitchWheelValue());
            else if (message.isController() && message.getControllerNumber() == 64)
                engine.setSustainPedal (message.getControllerValue() >= 64);
            else if (message.isAllNotesOff() || message.isAllSoundOff())
                engine.allNotesOff();
            ++event;
        }
        engine.renderSample (left[sample], right[sample]);
    }
}

void SwarmProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    auto state = parameters.copyState();
    juce::ValueTree sceneState { "SwarmScenes" };
    for (int s = 0; s < 4; ++s)
    {
        juce::ValueTree child { "Scene" };
        child.setProperty ("index", s, nullptr);
        const auto p = scenes[static_cast<size_t> (s)].load();
        for (size_t i = 0; i < patchFields.size(); ++i)
            child.setProperty ("p" + juce::String (static_cast<int> (i)), p.*patchFields[i], nullptr);
        sceneState.addChild (child, -1, nullptr);
    }
    state.addChild (sceneState, -1, nullptr);
    if (auto xml = state.createXml()) juce::copyXmlToBinary (*xml, destination);
}

void SwarmProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = juce::getXmlFromBinary (data, size))
    {
        auto restored = juce::ValueTree::fromXml (*xml);
        if (! restored.isValid() || ! restored.hasType (parameters.state.getType())) return;
        const auto scenesNode = restored.getChildWithName ("SwarmScenes");
        if (scenesNode.isValid())
        {
            for (int childIndex = 0; childIndex < scenesNode.getNumChildren(); ++childIndex)
            {
                const auto child = scenesNode.getChild (childIndex);
                const int slot = static_cast<int> (child.getProperty ("index", -1));
                if (slot < 0 || slot >= 4) continue;
                auto patch = scenes[static_cast<size_t> (slot)].load();
                for (size_t i = 0; i < patchFields.size(); ++i)
                {
                    const auto key = "p" + juce::String (static_cast<int> (i));
                    if (child.hasProperty (key)) patch.*patchFields[i] = static_cast<float> (child.getProperty (key));
                }
                scenes[static_cast<size_t> (slot)].store (patch);
            }
        }
        // Keep only APVTS data in the parameter tree. Scene values are managed atomically.
        if (scenesNode.isValid()) restored.removeChild (scenesNode, nullptr);
        parameters.replaceState (restored);
    }
}

juce::AudioProcessorEditor* SwarmProcessor::createEditor()
{
    return new SwarmEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SwarmProcessor(); }
