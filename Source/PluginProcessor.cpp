#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
using Patch = anks::Patch;
constexpr size_t originalPatchFieldCount = 13;
constexpr std::array<const char*, 24> patchIds {
    "detuneA", "widthA", "motionA", "detuneB", "widthB", "motionB",
    "mix", "cutoff", "resonance", "drive", "attack", "release", "output",
    "tuneA", "densityA", "shapeA", "tuneB", "densityB", "shapeB",
    "lfoRate", "lfoDepth", "delayMix", "delayTime", "delayFeedback"
};
constexpr std::array<float Patch::*, patchIds.size()> patchFields {
    &Patch::detuneA, &Patch::widthA, &Patch::motionA,
    &Patch::detuneB, &Patch::widthB, &Patch::motionB,
    &Patch::mix, &Patch::cutoff, &Patch::resonance,
    &Patch::drive, &Patch::attack, &Patch::release, &Patch::output,
    &Patch::tuneA, &Patch::densityA, &Patch::shapeA, &Patch::tuneB, &Patch::densityB, &Patch::shapeB,
    &Patch::lfoRate, &Patch::lfoDepth, &Patch::delayMix, &Patch::delayTime, &Patch::delayFeedback
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
    p[0].mix = 0.24f;
    p[1].mix = 0.55f;
    p[2].mix = 0.72f; p[2].attack = 0.35f; p[2].release = 1.5f;
    p[3].mix = 0.63f; p[3].resonance = 0.48f;

    p[0].densityA = 3.0f; p[0].densityB = 1.0f;
    p[0].tuneB = -12.0f; p[0].shapeB = 0.24f;
    p[1].tuneB = 12.0f; p[1].delayMix = 0.21f;
    p[2].shapeA = 0.56f; p[2].shapeB = 0.73f;
    p[2].lfoDepth = 0.52f; p[2].lfoRate = 0.33f; p[2].delayMix = 0.30f;
    p[2].delayTime = 0.44f; p[2].delayFeedback = 0.53f;
    p[3].shapeA = 0.9f; p[3].shapeB = 0.36f; p[3].densityB = 4.0f;
    p[3].lfoRate = 4.2f; p[3].lfoDepth = 0.64f; p[3].delayMix = 0.36f;
    return p;
}
}

void SwarmProcessor::AtomicScene::store (const anks::Patch& p) noexcept
{
    ++generation;
    for (size_t i = 0; i < data.size(); ++i)
        data[i].store (p.*patchFields[i]);
    ++generation;
}

anks::Patch SwarmProcessor::AtomicScene::load() const noexcept
{
    anks::Patch p;
    for (size_t i = 0; i < data.size(); ++i)
        p.*patchFields[i] = data[i].load();
    return p;
}

bool SwarmProcessor::AtomicScene::tryLoad (Patch& destination) const noexcept
{
    const auto before = generation.load();
    if ((before & 1u) != 0)
        return false;
    const auto candidate = load();
    if (before != generation.load())
        return false;
    destination = candidate;
    return true;
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
    add ("tuneA", "A / Tune", -12.0f, 12.0f, 0.0f);
    add ("densityA", "A / Density", 1.0f, 7.0f, 7.0f);
    add ("shapeA", "A / Shape", 0.0f, 1.0f, 0.0f);
    add ("tuneB", "B / Tune", -12.0f, 12.0f, 0.0f);
    add ("densityB", "B / Density", 1.0f, 7.0f, 7.0f);
    add ("shapeB", "B / Shape", 0.0f, 1.0f, 0.0f);
    add ("lfoRate", "Filter LFO Rate", 0.05f, 12.0f, 0.4f);
    add ("lfoDepth", "Filter LFO Depth", 0.0f, 1.0f, 0.0f);
    add ("delayMix", "Delay Mix", 0.0f, 0.75f, 0.0f);
    add ("delayTime", "Delay Time", 0.04f, 0.8f, 0.28f);
    add ("delayFeedback", "Delay Feedback", 0.0f, 0.78f, 0.32f);
    return layout;
}

SwarmProcessor::SwarmProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "ANKSParameters", makeParameterLayout())
{
    const auto defaults = factoryScenes();
    audioScenes = defaults;
    for (size_t i = 0; i < scenes.size(); ++i) scenes[i].store (defaults[i]);
    auditionKeyboard.addListener (this);
}

SwarmProcessor::~SwarmProcessor()
{
    auditionKeyboard.removeListener (this);
}

void SwarmProcessor::handleNoteOn (juce::MidiKeyboardState*, int, int note, float velocity)
{
    if (note >= firstKey && note <= lastKey)
        keyboardVelocity[static_cast<size_t> (note - firstKey)].store (velocity, std::memory_order_relaxed);
}

void SwarmProcessor::handleNoteOff (juce::MidiKeyboardState* state, int channel, int note, float)
{
    handleNoteOn (state, channel, note, 0.0f);
}

void SwarmProcessor::prepareToPlay (double sampleRate, int)
{
    auditionKeyboard.reset();
    for (auto& velocity : keyboardVelocity)
        velocity.store (0.0f, std::memory_order_relaxed);
    keyboardPlaying.fill (false);
    auditionChord.store (false, std::memory_order_relaxed);
    chordPlaying = false;
    outputPeak.store (0.0f, std::memory_order_relaxed);
    engine.setPatch (readLivePatch());
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
    for (size_t i = 0; i < patchIds.size(); ++i)
        p.*patchFields[i] = read (patchIds[i]);
    return p;
}

void SwarmProcessor::captureScene (int sceneIndex)
{
    if (! juce::isPositiveAndBelow (sceneIndex, static_cast<int> (scenes.size())))
        return;
    const std::lock_guard<std::mutex> guard (sceneWriteMutex);
    scenes[static_cast<size_t> (sceneIndex)].store (readLivePatch());
}

anks::Patch SwarmProcessor::getScene (int sceneIndex) const
{
    const std::lock_guard<std::mutex> guard (sceneWriteMutex);
    if (juce::isPositiveAndBelow (sceneIndex, static_cast<int> (scenes.size())))
        return scenes[static_cast<size_t> (sceneIndex)].load();
    return {};
}

void SwarmProcessor::setSceneParameter (int sceneIndex, const juce::String& id, float value)
{
    if (! juce::isPositiveAndBelow (sceneIndex, static_cast<int> (scenes.size())) || ! std::isfinite (value))
        return;
    for (size_t i = 0; i < patchIds.size(); ++i)
    {
        if (id != patchIds[i])
            continue;
        const auto& range = parameters.getParameter (id)->getNormalisableRange();
        const std::lock_guard<std::mutex> guard (sceneWriteMutex);
        auto& scene = scenes[static_cast<size_t> (sceneIndex)];
        auto patch = scene.load();
        patch.*patchFields[i] = range.snapToLegalValue (value);
        scene.store (patch);
        return;
    }
}

void SwarmProcessor::recallScene (int sceneIndex)
{
    const auto patch = getScene (sceneIndex);
    const auto set = [this] (const char* id, float value)
    {
        auto* parameter = parameters.getParameter (id);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        parameter->endChangeGesture();
    };
    set ("morphOn", 0.0f);
    for (size_t i = 0; i < patchIds.size(); ++i)
        set (patchIds[i], patch.*patchFields[i]);
}

anks::Patch SwarmProcessor::getEffectivePatch() const noexcept
{
    const auto read = [this] (const char* id) { return parameters.getRawParameterValue (id)->load(); };
    if (read ("morphOn") < 0.5f)
        return readLivePatch();

    std::array<Patch, 4> corners = factoryScenes();
    for (size_t i = 0; i < corners.size(); ++i)
        scenes[i].tryLoad (corners[i]);
    return anks::interpolateScenes (corners, read ("morphX"), read ("morphY"));
}

void SwarmProcessor::setAuditionChord (bool playing) noexcept
{
    auditionChord.store (playing, std::memory_order_relaxed);
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
        for (size_t i = 0; i < scenes.size(); ++i)
            scenes[i].tryLoad (audioScenes[i]);
        target = anks::interpolateScenes (audioScenes, read ("morphX"), read ("morphY"));
    }
    engine.setPatch (target);
    if (wrapperType == wrapperType_Standalone)
    {
        for (size_t i = 0; i < keyboardVelocity.size(); ++i)
        {
            const float velocity = keyboardVelocity[i].load (std::memory_order_relaxed);
            if ((velocity > 0.0f) != keyboardPlaying[i])
            {
                keyboardPlaying[i] = velocity > 0.0f;
                engine.noteOn (firstKey + static_cast<int> (i), velocity);
            }
        }
        const bool requested = auditionChord.load (std::memory_order_relaxed);
        if (requested != chordPlaying)
        {
            chordPlaying = requested;
            for (const int note : {36, 40, 43, 47})
                if (chordPlaying)
                    engine.noteOn (note, 0.78f);
                else
                    engine.noteOff (note);
        }
    }

    float peak = 0.0f;
    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);
    auto event = midi.begin();
    const auto end = midi.end();
    for (int sample = 0; sample < numSamples; ++sample)
    {
        while (event != end && (*event).samplePosition <= sample)
        {
            const auto message = (*event).getMessage();
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
        peak = std::max ({peak, std::abs (left[sample]), std::abs (right[sample])});
    }
    const auto previousPeak = outputPeak.load (std::memory_order_relaxed);
    outputPeak.store (std::max (peak, previousPeak * 0.97f), std::memory_order_relaxed);
}

void SwarmProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    auto state = parameters.copyState();
    juce::ValueTree sceneState { "SwarmScenes" };
    for (int s = 0; s < 4; ++s)
    {
        juce::ValueTree child { "Scene" };
        child.setProperty ("index", s, nullptr);
        const auto p = getScene (s);
        for (size_t i = 0; i < patchFields.size(); ++i)
            child.setProperty ("p" + juce::String (static_cast<int> (i)), p.*patchFields[i], nullptr);
        sceneState.addChild (child, -1, nullptr);
    }
    state.addChild (sceneState, -1, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destination);
}

void SwarmProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto restored = juce::ValueTree::fromXml (*xml);
        if (! restored.isValid() || ! restored.hasType (parameters.state.getType())) return;
        const auto scenesNode = restored.getChildWithName ("SwarmScenes");
        if (scenesNode.isValid())
        {
            const std::lock_guard<std::mutex> guard (sceneWriteMutex);
            for (int childIndex = 0; childIndex < scenesNode.getNumChildren(); ++childIndex)
            {
                const auto child = scenesNode.getChild (childIndex);
                const int slot = static_cast<int> (child.getProperty ("index", -1));
                if (slot < 0 || slot >= 4) continue;
                Patch patch;
                for (size_t i = 0; i < patchFields.size(); ++i)
                {
                    const auto key = "p" + juce::String (static_cast<int> (i));
                    if (child.hasProperty (key))
                    {
                        const auto value = static_cast<float> (child.getProperty (key));
                        const auto& range = parameters.getParameter (patchIds[i])->getNormalisableRange();
                        if (std::isfinite (value))
                            patch.*patchFields[i] = std::clamp (value, range.start, range.end);
                    }
                }
                scenes[static_cast<size_t> (slot)].store (patch);
            }
        }
        // Keep only APVTS data in the parameter tree. Scene values are managed atomically.
        if (scenesNode.isValid()) restored.removeChild (scenesNode, nullptr);
        for (size_t i = originalPatchFieldCount; i < patchIds.size(); ++i)
        {
            const auto* id = patchIds[i];
            if (restored.getChildWithProperty ("id", id).isValid())
                continue;
            const auto* parameter = parameters.getParameter (id);
            juce::ValueTree entry { "PARAM" };
            entry.setProperty ("id", id, nullptr);
            entry.setProperty ("value", parameter->convertFrom0to1 (parameter->getDefaultValue()), nullptr);
            restored.addChild (entry, -1, nullptr);
        }
        parameters.replaceState (restored);
    }
}

juce::AudioProcessorEditor* SwarmProcessor::createEditor()
{
    return new SwarmEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SwarmProcessor(); }
