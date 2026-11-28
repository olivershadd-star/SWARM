#include "PluginEditor.h"

namespace
{
const auto background = juce::Colour (0xff14131e);
const auto ink = juce::Colour (0xfff8f5ff);
const auto subdued = juce::Colour (0xffb5b1c7);
const auto pink = juce::Colour (0xffff74b6);
const auto lime = juce::Colour (0xffc4f469);
const auto lilac = juce::Colour (0xffc4bbff);
const auto panel = juce::Colour (0xff252338);

void text (juce::Graphics& g, const juce::String& value, juce::Rectangle<int> box,
           float size, juce::Colour colour, bool bold = false)
{
    g.setColour (colour);
    g.setFont (juce::Font (juce::FontOptions (size).withStyle (bold ? "Bold" : "Regular")));
    g.drawFittedText (value, box, juce::Justification::centredLeft, 1);
}

void tile (juce::Graphics& g, juce::Rectangle<int> box, juce::Colour fill)
{
    g.setColour (fill);
    g.fillRoundedRectangle (box.toFloat(), 12.0f);
}

void spectrum (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour colour,
               float detune, float width, float motion, float density, float tune)
{
    g.setColour (background.withAlpha (0.75f));
    g.fillRoundedRectangle (area.toFloat(), 8.0f);
    const auto data = area.reduced (14, 10).toFloat();
    const auto mid = data.getCentreX() + data.getWidth() * tune / 96.0f;
    const auto span = data.getWidth() * detune / 180.0f;
    const auto baseline = data.getBottom() - 10.0f;
    g.setColour (colour.withAlpha (0.22f));
    g.drawHorizontalLine (juce::roundToInt (baseline), data.getX(), data.getRight());
    for (int i = -3; i <= 3; ++i)
    {
        const auto pos = static_cast<float> (i) / 3.0f;
        const auto x = mid + span * pos;
        const auto h = (data.getHeight() - 16.0f) * (0.92f - std::abs (pos) * motion * 0.31f);
        const auto weight = i == 0 ? 1.0f : juce::jlimit (0.0f, 1.0f, (density + 1.0f) * 0.5f - static_cast<float> (std::abs (i)));
        g.setColour (colour.withAlpha (weight * (1.0f - std::abs (pos) * width * 0.35f)));
        g.fillRoundedRectangle (x - 2.0f, baseline - h, 4.0f, h, 2.0f);
    }
}

void setParameter (SwarmProcessor& processor, const char* id, float normalized)
{
    auto* parameter = processor.parameters.getParameter (id);
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost (normalized);
    parameter->endChangeGesture();
}
}

void SwarmEditor::Theme::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float position, float start, float end, juce::Slider& dial)
{
    const juce::Rectangle<float> bounds (static_cast<float> (x), static_cast<float> (y),
                                         static_cast<float> (width), static_cast<float> (height));
    const auto centre = bounds.getCentre();
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f - 4.0f;
    if (radius < 4.0f)
        return;

    const auto accent = dial.findColour (juce::Slider::rotarySliderFillColourId);
    const float angle = start + position * (end - start);
    const juce::PathStrokeType stroke (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, start, end, true);
    g.setColour (background);
    g.strokePath (track, stroke);
    if (position > 0.0f)
    {
        juce::Path level;
        level.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, start, angle, true);
        g.setColour (accent);
        g.strokePath (level, stroke);
    }

    const auto face = bounds.withSizeKeepingCentre (radius * 1.5f, radius * 1.5f);
    g.setColour (background.brighter (0.16f));
    g.fillEllipse (face);
    g.setColour (ink.withAlpha (0.18f));
    g.drawEllipse (face, 1.0f);
    g.setColour (accent);
    g.drawLine ({centre.getPointOnCircumference (radius * 0.39f, angle),
                 centre.getPointOnCircumference (radius * 0.64f, angle)}, 2.7f);
    const auto& played = dial.getProperties()["playedPosition"];
    if (! played.isVoid())
    {
        const auto marker = centre.getPointOnCircumference (radius + 0.5f,
            start + static_cast<float> (played) * (end - start));
        g.setColour (ink);
        g.fillEllipse (marker.x - 2.5f, marker.y - 2.5f, 5.0f, 5.0f);
    }
}

class SwarmEditor::Control final : public juce::Component
{
public:
    Control (SwarmProcessor& processor, const char* id, const juce::String& name, juce::Colour colour)
        : key (id)
    {
        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, ink);
        addAndMakeVisible (label);

        const auto configure = [this, &name, colour] (juce::Slider& dial, const juce::String& id)
        {
            dial.setComponentID (id);
            dial.setTitle (name);
            dial.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            dial.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            dial.setColour (juce::Slider::rotarySliderFillColourId, colour);
            dial.setColour (juce::Slider::rotarySliderOutlineColourId, background);
            dial.setColour (juce::Slider::thumbColourId, ink);
            addChildComponent (dial);
        };
        configure (slider, key);
        configure (sceneSlider, "scene-" + key);
        slider.setVisible (true);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.parameters, key, slider);
        const auto range = processor.parameters.getParameter (key)->getNormalisableRange();
        sceneSlider.setNormalisableRange ({
            static_cast<double> (range.start), static_cast<double> (range.end),
            [range] (double, double, double v) { return range.convertFrom0to1 (static_cast<float> (v)); },
            [range] (double, double, double v) { return range.convertTo0to1 (static_cast<float> (v)); },
            [range] (double, double, double v) { return range.snapToLegalValue (static_cast<float> (v)); }
        });
        sceneSlider.onValueChange = [this, &processor]
        {
            processor.setSceneParameter (currentScene, key, static_cast<float> (sceneSlider.getValue()));
            if (auto* editor = findParentComponentOfClass<SwarmEditor>())
                editor->timerCallback();
        };
        addAndMakeVisible (readout);
        readout.setJustificationType (juce::Justification::centred);
        readout.setColour (juce::Label::textColourId, subdued);
        readout.setInterceptsMouseClicks (false, false);
    }

    void update (bool sceneMode, int scene, float edited, float effective)
    {
        slider.setVisible (! sceneMode);
        sceneSlider.setVisible (sceneMode);
        currentScene = scene;
        if (sceneMode)
        {
            sceneSlider.setValue (edited, juce::dontSendNotification);
            const auto marker = sceneSlider.valueToProportionOfLength (effective);
            if (sceneSlider.getProperties().set ("playedPosition", marker))
                sceneSlider.repaint();
            sceneSlider.setTooltip ("Coin : " + format (edited) + "  /  XY : " + format (effective));
        }
        readout.setText (format (sceneMode ? edited : slider.getValue()), juce::dontSendNotification);
    }

    juce::String format (double value) const
    {
        juce::String formatted;
        if (key.startsWith ("detune"))
            formatted = juce::String (value, 1) + " ct";
        else if (key.startsWith ("tune"))
            formatted = juce::String (value, 1) + " st";
        else if (key.startsWith ("density"))
            formatted = juce::String (value, 1) + " / 7";
        else if (key == "lfoRate")
            formatted = juce::String (value, 2) + " Hz";
        else if (key == "delayTime")
            formatted = juce::String (value * 1000.0, 0) + " ms";
        else if (key == "cutoff")
            formatted = value < 1000.0 ? juce::String (value, 0) + " Hz"
                                       : juce::String (value / 1000.0, 2) + " kHz";
        else if (key == "attack" || key == "release")
            formatted = juce::String (value * 1000.0, 0) + " ms";
        else
            formatted = juce::String (value * 100.0, 0) + " %";
        return formatted;
    }

    void resized() override
    {
        auto box = getLocalBounds();
        label.setBounds (box.removeFromTop (24));
        readout.setBounds (box.removeFromBottom (20));
        slider.setBounds (box.reduced (8, 0));
        sceneSlider.setBounds (slider.getBounds());
    }

private:
    juce::String key;
    juce::Label label, readout;
    juce::Slider slider, sceneSlider;
    int currentScene = 0;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class SwarmEditor::MorphPad final : public juce::Component
{
public:
    explicit MorphPad (SwarmProcessor& p) : processor (p)
    {
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
        setTitle ("Morph X/Y");
    }

    void paint (juce::Graphics& g) override
    {
        const auto box = getLocalBounds().toFloat();
        juce::ColourGradient gradient (pink.darker (0.9f), box.getTopLeft(),
                                       lime.darker (1.35f), box.getBottomRight(), false);
        g.setGradientFill (gradient);
        g.fillRoundedRectangle (box, 9.0f);
        const auto plot = area();
        g.setColour (ink.withAlpha (0.5f));
        g.drawRect (plot, 1.0f);
        g.drawHorizontalLine (juce::roundToInt (plot.getCentreY()), plot.getX(), plot.getRight());
        g.drawVerticalLine (juce::roundToInt (plot.getCentreX()), plot.getY(), plot.getBottom());
        const auto px = plot.getX() + x()->getValue() * plot.getWidth();
        const auto py = plot.getY() + y()->getValue() * plot.getHeight();
        g.setColour (isEnabled() ? ink : subdued);
        g.fillEllipse (px - 6.0f, py - 6.0f, 12.0f, 12.0f);
        constexpr const char* names[] { "A", "B", "C", "D" };
        for (int i = 0; i < 4; ++i)
        {
            const auto vx = (i % 2 == 0) ? plot.getX() : plot.getRight();
            const auto vy = (i < 2) ? plot.getY() : plot.getBottom();
            text (g, names[i], {juce::roundToInt (vx) - 6, juce::roundToInt (vy) - 9, 18, 18},
                  11.0f, ink, true);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        x()->beginChangeGesture();
        y()->beginChangeGesture();
        move (e.position);
    }

    void mouseDrag (const juce::MouseEvent& e) override { move (e.position); }
    void mouseUp (const juce::MouseEvent&) override
    {
        x()->endChangeGesture();
        y()->endChangeGesture();
    }

private:
    SwarmProcessor& processor;
    juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (14.0f); }
    juce::RangedAudioParameter* x() const { return processor.parameters.getParameter ("morphX"); }
    juce::RangedAudioParameter* y() const { return processor.parameters.getParameter ("morphY"); }
    void move (juce::Point<float> point)
    {
        const auto bounds = area();
        if (bounds.isEmpty())
            return;
        x()->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (point.x - bounds.getX()) / bounds.getWidth()));
        y()->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (point.y - bounds.getY()) / bounds.getHeight()));
        repaint();
    }
};

SwarmEditor::SwarmEditor (SwarmProcessor& p) : juce::AudioProcessorEditor (&p), processor (p)
{
    theme.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff3d3955));
    theme.setColour (juce::TextButton::textColourOffId, ink);
    theme.setColour (juce::ToggleButton::textColourId, ink);
    theme.setColour (juce::ToggleButton::tickColourId, lime);
    setLookAndFeel (&theme);

    const char* ids[] { "detuneA", "widthA", "motionA", "detuneB", "widthB", "motionB",
                        "mix", "cutoff", "resonance", "drive", "attack", "release", "output",
                        "tuneA", "densityA", "shapeA", "tuneB", "densityB", "shapeB",
                        "lfoRate", "lfoDepth", "delayMix", "delayTime", "delayFeedback" };
    const char* names[] { "Detune", "Largeur", "Mouvement", "Detune", "Largeur", "Mouvement",
                          "Mix A/B", "Cutoff", "Résonance", "Drive", "Attaque", "Relâchement", "Sortie",
                          "Accord", "Densité", "Saw / Pulse", "Accord", "Densité", "Saw / Pulse",
                          "Vitesse", "Profondeur", "Dosage", "Temps", "Feedback" };
    for (size_t i = 0; i < controls.size(); ++i)
    {
        const bool inA = i < 3 || (i >= 13 && i < 16);
        const bool inB = (i >= 3 && i < 6) || (i >= 16 && i < 19);
        controls[i] = std::make_unique<Control> (p, ids[i], juce::String::fromUTF8 (names[i]),
                                                 inA ? pink : (inB ? lime : lilac));
        addAndMakeVisible (*controls[i]);
    }

    pad = std::make_unique<MorphPad> (p);
    addAndMakeVisible (*pad);
    for (size_t i = 0; i < recallButtons.size(); ++i)
    {
        const juce::String letter = juce::String::charToString (static_cast<juce::juce_wchar> ('A' + i));
        auto& recall = recallButtons[i];
        recall.setButtonText (letter);
        recall.setComponentID ("corner-" + letter);
        recall.onClick = [this, i]
        {
            if (processor.parameters.getRawParameterValue ("morphOn")->load() >= 0.5f)
                editedScene = static_cast<int> (i);
            else
                processor.recallScene (static_cast<int> (i));
            timerCallback();
        };
        addAndMakeVisible (recall);
        auto& save = saveButtons[i];
        save.setButtonText ("CAP " + letter);
        save.onClick = [this, i] { processor.captureScene (static_cast<int> (i)); };
        addAndMakeVisible (save);
    }

    addAndMakeVisible (morph);
    morphAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.parameters, "morphOn", morph);
    modeLabel.setColour (juce::Label::textColourId, subdued);
    addAndMakeVisible (modeLabel);

    hearA.onClick = [this] { setParameter (processor, "mix", 0.0f); };
    hearB.onClick = [this] { setParameter (processor, "mix", 1.0f); };
    addAndMakeVisible (hearA);
    addAndMakeVisible (hearB);

    if (p.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        keyboard = std::make_unique<juce::MidiKeyboardComponent> (
            p.auditionKeyboard, juce::MidiKeyboardComponent::horizontalKeyboard);
        keyboard->setAvailableRange (48, 83);
        keyboard->setLowestVisibleKey (48);
        addAndMakeVisible (*keyboard);
        audition.onStateChange = [this] { processor.setAuditionChord (audition.isDown()); };
        addAndMakeVisible (audition);
    }

    setResizable (true, true);
    setResizeLimits (920, keyboard ? 740 : 665, 1480, 940);
    setSize (1060, keyboard ? 770 : 700);
    timerCallback();
    startTimerHz (24);
}

SwarmEditor::~SwarmEditor()
{
    stopTimer();
    if (keyboard != nullptr)
        processor.setAuditionChord (false);
    setLookAndFeel (nullptr);
}

void SwarmEditor::timerCallback()
{
    viewingMorph = processor.parameters.getRawParameterValue ("morphOn")->load() >= 0.5f;
    displayedPatch = processor.getEffectivePatch();
    const auto valuesFor = [] (const anks::Patch& p)
    {
        return std::array<float, 24> {p.detuneA, p.widthA, p.motionA, p.detuneB, p.widthB, p.motionB,
                                     p.mix, p.cutoff, p.resonance, p.drive, p.attack, p.release, p.output,
                                     p.tuneA, p.densityA, p.shapeA, p.tuneB, p.densityB, p.shapeB,
                                     p.lfoRate, p.lfoDepth, p.delayMix, p.delayTime, p.delayFeedback};
    };
    const auto values = valuesFor (displayedPatch);
    const auto edits = viewingMorph ? valuesFor (processor.getScene (editedScene)) : values;
    for (size_t i = 0; i < controls.size(); ++i)
        controls[i]->update (viewingMorph, editedScene, edits[i], values[i]);

    for (size_t i = 0; i < recallButtons.size(); ++i)
    {
        auto& button = recallButtons[i];
        button.setToggleState (viewingMorph && i == static_cast<size_t> (editedScene), juce::dontSendNotification);
        button.setTooltip (viewingMorph ? "Modifier ce coin sans bouger le pad" : "Rappeler ce coin en LIVE");
    }
    for (auto& save : saveButtons)
        save.setEnabled (! viewingMorph);
    hearA.setEnabled (! viewingMorph);
    hearB.setEnabled (! viewingMorph);
    // Let JUCE deliver mouse-up to close an in-progress XY gesture.
    if (! pad->isMouseButtonDown())
        pad->setEnabled (viewingMorph);
    const auto corner = juce::String::charToString (static_cast<juce::juce_wchar> ('A' + editedScene));
    modeLabel.setText (viewingMorph ? "Coin " + corner + "  /  point blanc = XY" : "LIVE / commandes directes",
                       juce::dontSendNotification);
    repaint();
}

void SwarmEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);
    text (g, "SWARM / 01", header.withWidth (260), 35.0f, ink, true);
    text (g, "ANKS LABS", header.withTrimmedLeft (260).withWidth (120), 13.0f, subdued, true);

    auto meter = header.withLeft (header.getRight() - 115).withSizeKeepingCentre (115, 10);
    tile (g, meter, panel);
    const auto level = std::sqrt (juce::jlimit (0.0f, 1.0f, processor.getOutputPeak()));
    tile (g, meter.withWidth (juce::roundToInt (115.0f * level)), lime);

    tile (g, bankA, juce::Colour (0xff512c51));
    tile (g, bankB, juce::Colour (0xff34432c));
    tile (g, morphPanel, panel);
    tile (g, master, panel);
    text (g, "SWARM A", bankA.reduced (16, 12).withHeight (32), 23.0f, pink, true);
    text (g, "SWARM B", bankB.reduced (16, 12).withHeight (32), 23.0f, lime, true);
    text (g, "MORPHER", morphPanel.reduced (14, 10).withHeight (28), 18.0f, lilac, true);
    text (g, "MIX     /     FILTRE     /     AMPLITUDE", master.reduced (14, 7).withHeight (24),
          14.0f, lilac, true);
    auto effectsTitles = master.reduced (14, 0).withTrimmedTop (110).withHeight (18);
    text (g, "LFO FILTRE", effectsTitles.removeFromLeft (effectsTitles.getWidth() * 2 / 5), 13.0f, pink, true);
    text (g, "DELAI STEREO", effectsTitles, 13.0f, lime, true);

    const auto& p = displayedPatch;
    spectrum (g, bankA.reduced (16, 0).withTrimmedTop (55).withTrimmedBottom (188),
              pink, p.detuneA, p.widthA, p.motionA, p.densityA, p.tuneA);
    spectrum (g, bankB.reduced (16, 0).withTrimmedTop (55).withTrimmedBottom (188),
              lime, p.detuneB, p.widthB, p.motionB, p.densityB, p.tuneB);
}

void SwarmEditor::resized()
{
    auto layout = getLocalBounds().reduced (16, 12);
    header = layout.removeFromTop (57);
    if (keyboard != nullptr)
    {
        keyboardRow = layout.removeFromBottom (65);
        auto keys = keyboardRow;
        audition.setBounds (keys.removeFromLeft (135).reduced (0, 12));
        keys.removeFromLeft (12);
        keyboard->setKeyWidth (static_cast<float> (keys.getWidth()) / 21.0f);
        keyboard->setBounds (keys);
        layout.removeFromBottom (10);
    }
    master = layout.removeFromBottom (213);
    layout.removeFromBottom (10);

    const int middleWidth = layout.getWidth() * 31 / 100;
    bankA = layout.removeFromLeft ((layout.getWidth() - middleWidth - 20) / 2);
    layout.removeFromLeft (10);
    morphPanel = layout.removeFromLeft (middleWidth);
    layout.removeFromLeft (10);
    bankB = layout;
    hearA.setBounds (bankA.getRight() - 72, bankA.getY() + 15, 56, 24);
    hearB.setBounds (bankB.getRight() - 72, bankB.getY() + 15, 56, 24);

    const auto row = [this] (juce::Rectangle<int> box, size_t first, size_t count)
    {
        for (size_t i = 0; i < count; ++i)
            controls[first + i]->setBounds (box.removeFromLeft (box.getWidth() / static_cast<int> (count - i)));
    };
    auto a = bankA.reduced (10, 5);
    auto b = bankB.reduced (10, 5);
    row (a.removeFromBottom (88), 13, 3);
    row (b.removeFromBottom (88), 16, 3);
    row (a.removeFromBottom (88), 0, 3);
    row (b.removeFromBottom (88), 3, 3);

    auto sound = master.reduced (10, 4);
    sound.removeFromTop (23);
    row (sound.removeFromTop (84), 6, 7);
    sound.removeFromTop (17);
    row (sound.removeFromLeft (sound.getWidth() * 2 / 5), 19, 2);
    row (sound, 21, 3);

    auto m = morphPanel.reduced (13, 8);
    morph.setBounds (m.removeFromTop (31).removeFromRight (90));
    modeLabel.setBounds (m.removeFromBottom (25));
    auto saves = m.removeFromBottom (27);
    auto recalls = m.removeFromBottom (27);
    for (size_t i = 0; i < recallButtons.size(); ++i)
    {
        const auto remaining = static_cast<int> (recallButtons.size() - i);
        recallButtons[i].setBounds (recalls.removeFromLeft (recalls.getWidth() / remaining).reduced (2, 2));
        saveButtons[i].setBounds (saves.removeFromLeft (saves.getWidth() / remaining).reduced (2, 2));
    }
    pad->setBounds (m.reduced (2, 5));
}
