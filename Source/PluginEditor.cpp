#include "PluginEditor.h"
#include <cmath>

namespace
{
const auto background = juce::Colour (0xff10121b);
const auto panel = juce::Colour (0xff1f2130);
const auto edge = juce::Colour (0xff34374a);
const auto pink = juce::Colour (0xffff76b7);
const auto lime = juce::Colour (0xffb8ed72);
const auto blue = juce::Colour (0xff91a5ff);
const auto white = juce::Colour (0xfff2f0f8);
const auto muted = juce::Colour (0xffa5a7bb);

void drawPanel (juce::Graphics& g, juce::Rectangle<int> rect, juce::Colour accent)
{
    auto r = rect.toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (r, 16.0f);
    g.setColour (edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 16.0f, 1.0f);
    g.setColour (accent.withAlpha (0.8f));
    g.fillRoundedRectangle (r.getX() + 20.0f, r.getY() + 1.0f, r.getWidth() - 40.0f, 3.0f, 1.5f);
}

void drawDetune (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour colour, float cents)
{
    const float mid = static_cast<float> (area.getCentreX());
    const float span = static_cast<float> (area.getWidth()) * 0.45f;
    const float spread = span * cents / 60.0f;
    g.setColour (edge);
    g.drawHorizontalLine (area.getBottom() - 2, static_cast<float> (area.getX()),
                          static_cast<float> (area.getRight()));
    for (int i = -3; i <= 3; ++i)
    {
        const float x = mid + spread * static_cast<float> (i) / 3.0f;
        const float height = static_cast<float> (area.getHeight()) * (1.0f - 0.12f * static_cast<float> (std::abs (i)));
        g.setColour (colour.withAlpha (i == 0 ? 1.0f : 0.62f));
        g.fillRoundedRectangle (x - 2.0f, static_cast<float> (area.getBottom()) - height,
                                4.0f, height - 2.0f, 2.0f);
    }
}
}

class SwarmEditor::SwarmLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosition, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        const float radius = std::min (width, height) * 0.5f - 11.0f;
        const auto centre = juce::Point<float> (static_cast<float> (x + width) * 0.5f,
                                               static_cast<float> (y + height) * 0.5f);
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius, radius, 0,
                             rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colour (0xff36394a));
        g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
        juce::Path valueArc;
        valueArc.addCentredArc (centre.x, centre.y, radius, radius, 0,
                               rotaryStartAngle,
                               rotaryStartAngle + sliderPosition * (rotaryEndAngle - rotaryStartAngle), true);
        g.setColour (accent);
        g.strokePath (valueArc, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xff292b3b));
        g.fillEllipse (centre.x - radius + 9.0f, centre.y - radius + 9.0f,
                       radius * 2.0f - 18.0f, radius * 2.0f - 18.0f);
        g.setColour (juce::Colour (0xff4b4e62));
        g.drawEllipse (centre.x - radius + 9.0f, centre.y - radius + 9.0f,
                       radius * 2.0f - 18.0f, radius * 2.0f - 18.0f, 1.0f);
        const float angle = rotaryStartAngle + sliderPosition * (rotaryEndAngle - rotaryStartAngle);
        const auto tip = centre.getPointOnCircumference (radius - 17.0f, angle);
        g.setColour (accent);
        g.drawLine (centre.x, centre.y, tip.x, tip.y, 2.6f);
        g.fillEllipse (centre.x - 2.5f, centre.y - 2.5f, 5.0f, 5.0f);
    }
};

class SwarmEditor::Knob final : public juce::Component
{
public:
    Knob (SwarmLookAndFeel& theme, juce::AudioProcessorValueTreeState& state,
          const juce::String& id, const juce::String& label, juce::Colour colour)
    {
        title.setText (label, juce::dontSendNotification);
        title.setJustificationType (juce::Justification::centred);
        title.setColour (juce::Label::textColourId, muted);
        title.setFont (juce::Font (juce::FontOptions (12.0f)));
        addAndMakeVisible (title);
        slider.setLookAndFeel (&theme);
        slider.setComponentID (id);
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 19);
        slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
        slider.setColour (juce::Slider::textBoxTextColourId, white);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setPopupDisplayEnabled (true, true, this);
        addAndMakeVisible (slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, id, slider);
        const auto formatter = [id] (double value)
        {
            if (id.startsWith ("detune")) return juce::String (value, 1) + " ct";
            if (id == "cutoff") return value < 1000.0 ? juce::String (value, 0) + " Hz"
                                                        : juce::String (value / 1000.0, 2) + " kHz";
            if (id == "attack" || id == "release") return juce::String (value * 1000.0, 0) + " ms";
            return juce::String (value * 100.0, 0) + "%";
        };
        slider.textFromValueFunction = formatter;
        slider.valueFromTextFunction = [id] (const juce::String& text)
        {
            const auto value = text.getDoubleValue();
            if (id.startsWith ("detune")) return value;
            if (id == "cutoff") return text.containsIgnoreCase ("kHz") ? value * 1000.0 : value;
            if (id == "attack" || id == "release") return value / 1000.0;
            return value / 100.0;
        };
        slider.updateText();
    }
    ~Knob() override { attachment.reset(); slider.setLookAndFeel (nullptr); }
    void resized() override
    {
        auto r = getLocalBounds();
        title.setBounds (r.removeFromTop (18));
        slider.setBounds (getLocalBounds().withTrimmedTop (18));
    }
private:
    juce::Slider slider;
    juce::Label title;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class SwarmEditor::MorphPad final : public juce::Component, private juce::Timer
{
public:
    explicit MorphPad (juce::AudioProcessorValueTreeState& state)
      : x (state.getParameter ("morphX")), y (state.getParameter ("morphY")),
        enabled (state.getParameter ("morphOn"))
    {
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getPadArea();
        juce::ColourGradient gradient (pink.withAlpha (0.33f), area.getX(), area.getY(),
                                       lime.withAlpha (0.29f), area.getRight(), area.getBottom(), false);
        gradient.addColour (0.51, juce::Colour (0xff353850));
        g.setGradientFill (gradient);
        g.fillRoundedRectangle (area, 11.0f);
        g.setColour (juce::Colour (0xff555b73));
        g.drawRoundedRectangle (area, 11.0f, 1.0f);
        g.setColour (white.withAlpha (0.14f));
        g.drawVerticalLine (juce::roundToInt (area.getCentreX()), area.getY() + 3, area.getBottom() - 3);
        g.drawHorizontalLine (juce::roundToInt (area.getCentreY()), area.getX() + 3, area.getRight() - 3);

        constexpr const char* labels[] = {"1", "2", "3", "4"};
        for (int i = 0; i < 4; ++i)
        {
            const bool right = (i % 2) == 1;
            const bool bottom = i >= 2;
            const float px = right ? area.getRight() - 20.0f : area.getX() + 20.0f;
            const float py = bottom ? area.getBottom() - 20.0f : area.getY() + 20.0f;
            g.setColour (juce::Colour (0xff292a3a));
            g.fillEllipse (px - 12.0f, py - 12.0f, 24.0f, 24.0f);
            g.setColour (i == 0 || i == 2 ? pink : lime);
            g.drawEllipse (px - 12.0f, py - 12.0f, 24.0f, 24.0f, 1.3f);
            g.setColour (white);
            g.setFont (juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")));
            g.drawText (labels[i], juce::Rectangle<float> (px - 11, py - 11, 22, 22), juce::Justification::centred);
        }

        const float px = area.getX() + x->getValue() * area.getWidth();
        const float py = area.getY() + y->getValue() * area.getHeight();
        const juce::Colour active = enabled->getValue() > 0.5f ? white : muted;
        g.setColour (active.withAlpha (0.16f));
        g.fillEllipse (px - 22, py - 22, 44, 44);
        g.setColour (active);
        g.drawEllipse (px - 12, py - 12, 24, 24, 2.1f);
        g.setColour (juce::Colour (0xff171820));
        g.fillEllipse (px - 6, py - 6, 12, 12);
        g.setColour (active);
        g.fillEllipse (px - 3, py - 3, 6, 6);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        x->beginChangeGesture();
        y->beginChangeGesture();
        setPosition (e.position);
    }
    void mouseDrag (const juce::MouseEvent& e) override { setPosition (e.position); }
    void mouseUp (const juce::MouseEvent&) override
    {
        x->endChangeGesture();
        y->endChangeGesture();
    }
private:
    juce::RangedAudioParameter* x;
    juce::RangedAudioParameter* y;
    juce::RangedAudioParameter* enabled;
    juce::Rectangle<float> getPadArea() const { return getLocalBounds().toFloat().reduced (19.0f, 14.0f); }
    void setPosition (juce::Point<float> pos)
    {
        const auto bounds = getPadArea();
        const float nx = juce::jlimit (0.0f, 1.0f, (pos.x - bounds.getX()) / bounds.getWidth());
        const float ny = juce::jlimit (0.0f, 1.0f, (pos.y - bounds.getY()) / bounds.getHeight());
        x->setValueNotifyingHost (x->convertTo0to1 (nx));
        y->setValueNotifyingHost (y->convertTo0to1 (ny));
        repaint();
    }
    void timerCallback() override { repaint(); }
};

SwarmEditor::SwarmEditor (SwarmProcessor& p)
  : juce::AudioProcessorEditor (&p), processor (p), theme (std::make_unique<SwarmLookAndFeel>())
{
    struct Definition { const char* id; const char* name; juce::Colour colour; };
    const std::array<Definition, 13> defs {{
        {"detuneA", "DETUNE", pink}, {"widthA", "SPREAD", pink}, {"motionA", "MOTION", pink},
        {"detuneB", "DETUNE", lime}, {"widthB", "SPREAD", lime}, {"motionB", "MOTION", lime},
        {"mix", "MIX A / B", blue}, {"cutoff", "CUTOFF", blue}, {"resonance", "RESONANCE", blue},
        {"drive", "DRIVE", blue}, {"attack", "ATTACK", blue}, {"release", "RELEASE", blue},
        {"output", "OUTPUT", blue}
    }};
    for (const auto& definition : defs)
    {
        auto knob = std::make_unique<Knob> (*theme, processor.parameters,
                                           definition.id, definition.name, definition.colour);
        addAndMakeVisible (*knob);
        knobs.push_back (std::move (knob));
    }

    pad = std::make_unique<MorphPad> (processor.parameters);
    addAndMakeVisible (*pad);
    for (int i = 0; i < 4; ++i)
    {
        auto& b = captureButtons[static_cast<size_t> (i)];
        b.setButtonText ("Save " + juce::String (i + 1));
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff36394d));
        b.setColour (juce::TextButton::textColourOffId, white);
        b.onClick = [this, i]
        {
            processor.captureScene (i);
            status.setText ("Scene " + juce::String (i + 1) + " saved",
                            juce::dontSendNotification);
        };
        addAndMakeVisible (b);
    }
    morphToggle.setButtonText ("Enable morph");
    morphToggle.setClickingTogglesState (true);
    morphToggle.setColour (juce::ToggleButton::textColourId, white);
    morphToggle.setColour (juce::ToggleButton::tickColourId, lime);
    addAndMakeVisible (morphToggle);
    morphAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
        (processor.parameters, "morphOn", morphToggle);
    status.setJustificationType (juce::Justification::centred);
    status.setColour (juce::Label::textColourId, muted);
    status.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (status);

    auto* mix = processor.parameters.getParameter ("mix");
    listenA.onClick = [mix]
    {
        mix->beginChangeGesture();
        mix->setValueNotifyingHost (0.0f);
        mix->endChangeGesture();
    };
    listenB.onClick = [mix]
    {
        mix->beginChangeGesture();
        mix->setValueNotifyingHost (1.0f);
        mix->endChangeGesture();
    };
    addAndMakeVisible (listenA);
    addAndMakeVisible (listenB);

    if (processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        keyboard = std::make_unique<juce::MidiKeyboardComponent> (
            processor.auditionKeyboard, juce::MidiKeyboardComponent::horizontalKeyboard);
        keyboard->setAvailableRange (48, 83);
        keyboard->setLowestVisibleKey (48);
        keyboard->setWantsKeyboardFocus (true);
        addAndMakeVisible (*keyboard);
    }

    setResizable (true, true);
    setResizeLimits (870, keyboard != nullptr ? 575 : 515, 1480, 850);
    setSize (1000, keyboard != nullptr ? 640 : 580);
    timerCallback();
    startTimerHz (24);
}

SwarmEditor::~SwarmEditor()
{
    stopTimer();
    morphAttachment.reset();
    pad.reset();
    knobs.clear();
}

void SwarmEditor::timerCallback()
{
    const bool enabled = processor.parameters.getRawParameterValue ("morphOn")->load() >= 0.5f;
    if (displayedMorphMode != enabled)
    {
        displayedMorphMode = enabled;
        status.setText (enabled ? "Scenes shape the live controls"
                                : "Live controls active",
                        juce::dontSendNotification);
    }
    repaint();
}

void SwarmEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);
    g.setColour (white);
    g.setFont (juce::Font (juce::FontOptions (32.0f).withStyle ("Bold")));
    g.drawText ("SWARM / 01", 18, 9, 280, 45, juce::Justification::centredLeft);

    drawPanel (g, leftPanel, pink);
    drawPanel (g, rightPanel, lime);
    drawPanel (g, morphPanel, blue);
    drawPanel (g, bottomPanel, blue);

    const auto drawTitle = [&g] (const char* title, juce::Rectangle<int> bounds, juce::Colour colour)
    {
        g.setColour (colour);
        g.setFont (juce::Font (juce::FontOptions (17.0f).withStyle ("Bold")));
        g.drawText (title, bounds, juce::Justification::centredLeft);
    };
    drawTitle ("SWARM A", leftPanel.reduced (16, 9).removeFromTop (31), pink);
    drawTitle ("SWARM B", rightPanel.reduced (16, 9).removeFromTop (31), lime);
    drawTitle ("SCENE MORPH", morphPanel.reduced (16, 9).removeFromTop (31), blue);
    drawTitle ("MIX / FILTER / AMP", bottomPanel.reduced (16, 5).removeFromTop (24), white);

    drawDetune (g, leftPanel.reduced (24, 0).withTrimmedTop (62).withHeight (75),
                pink, processor.parameters.getRawParameterValue ("detuneA")->load());
    drawDetune (g, rightPanel.reduced (24, 0).withTrimmedTop (62).withHeight (75),
                lime, processor.parameters.getRawParameterValue ("detuneB")->load());
}

void SwarmEditor::resized()
{
    auto bounds = getLocalBounds().reduced (16, 10);
    bounds.removeFromTop (50);
    if (keyboard != nullptr)
    {
        auto keys = bounds.removeFromBottom (56);
        keyboard->setKeyWidth (static_cast<float> (keys.getWidth()) / 21.0f);
        keyboard->setBounds (keys);
        bounds.removeFromBottom (10);
    }
    bottomPanel = bounds.removeFromBottom (126);
    bounds.removeFromBottom (10);
    const int gap = 10;
    const int centreWidth = bounds.getWidth() * 34 / 100;
    const int leftWidth = (bounds.getWidth() - centreWidth - 2 * gap) / 2;
    leftPanel = bounds.removeFromLeft (leftWidth);
    bounds.removeFromLeft (gap);
    morphPanel = bounds.removeFromLeft (centreWidth);
    bounds.removeFromLeft (gap);
    rightPanel = bounds;

    listenA.setBounds (leftPanel.getRight() - 81, leftPanel.getY() + 14, 66, 26);
    listenB.setBounds (rightPanel.getRight() - 81, rightPanel.getY() + 14, 66, 26);
    const auto layoutKnobs = [this] (juce::Rectangle<int> row, int first, int count)
    {
        for (int i = 0; i < count; ++i)
            knobs[static_cast<size_t> (first + i)]->setBounds (row.removeFromLeft (row.getWidth() / (count - i)));
    };
    layoutKnobs (leftPanel.reduced (12, 8).removeFromBottom (104), 0, 3);
    layoutKnobs (rightPanel.reduced (12, 8).removeFromBottom (104), 3, 3);
    layoutKnobs (bottomPanel.reduced (10, 5).removeFromBottom (94), 6, 7);

    auto morph = morphPanel.reduced (14, 8);
    morph.removeFromTop (32);
    status.setBounds (morph.removeFromBottom (18));
    morphToggle.setBounds (morph.removeFromBottom (26).withSizeKeepingCentre (130, 26));
    auto captures = morph.removeFromBottom (29);
    for (auto& button : captureButtons)
    {
        const int remaining = static_cast<int> (captureButtons.data() + captureButtons.size() - &button);
        button.setBounds (captures.removeFromLeft (captures.getWidth() / remaining).reduced (2, 1));
    }
    pad->setBounds (morph);
}
