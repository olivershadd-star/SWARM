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

void drawSwarmArt (juce::Graphics& g, juce::Rectangle<int> rect, juce::Colour colour, bool flipped)
{
    const float x0 = static_cast<float> (rect.getX() + 25);
    const float x1 = static_cast<float> (rect.getRight() - 25);
    const float y0 = static_cast<float> (rect.getY() + 114);
    const float width = x1 - x0;
    for (int line = 0; line < 9; ++line)
    {
        juce::Path shape;
        const float t = static_cast<float> (line - 4);
        for (int i = 0; i <= 150; ++i)
        {
            const float u = static_cast<float> (i) / 150.0f;
            const float envelope = std::sin (u * static_cast<float> (juce::MathConstants<double>::pi));
            const float wobble = std::sin ((u * (flipped ? 12.0f : 8.0f) + t * 0.19f) * 6.2831853f) * envelope;
            const float x = x0 + u * width;
            const float y = y0 + t * (flipped ? 5.0f : 4.2f) + wobble * (6.5f + std::abs (t) * 1.2f);
            if (i == 0) shape.startNewSubPath (x, y);
            else shape.lineTo (x, y);
        }
        g.setColour (colour.withAlpha (0.29f + 0.06f * (4.0f - std::abs (t))));
        g.strokePath (shape, juce::PathStrokeType (1.35f));
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
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 19);
        slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
        slider.setColour (juce::Slider::textBoxTextColourId, white);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setPopupDisplayEnabled (true, true, this);
        addAndMakeVisible (slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, id, slider);
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

        constexpr const char* labels[] = {"A", "B", "C", "D"};
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
        enabled->setValueNotifyingHost (1.0f);
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
        b.setButtonText ("CAP " + juce::String ("ABCD").substring (i, i + 1));
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff36394d));
        b.setColour (juce::TextButton::textColourOffId, white);
        b.onClick = [this, i]
        {
            processor.captureScene (i);
            status.setText ("SCENE " + juce::String ("ABCD").substring (i, i + 1) + " SAVED",
                            juce::dontSendNotification);
        };
        addAndMakeVisible (b);
    }
    morphToggle.setButtonText ("MORPH  ON / OFF");
    morphToggle.setClickingTogglesState (true);
    morphToggle.setColour (juce::ToggleButton::textColourId, white);
    morphToggle.setColour (juce::ToggleButton::tickColourId, lime);
    addAndMakeVisible (morphToggle);
    morphAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
        (processor.parameters, "morphOn", morphToggle);
    status.setText ("LIVE: edit  /  CAP: save  /  MORPH: blend", juce::dontSendNotification);
    status.setJustificationType (juce::Justification::centred);
    status.setColour (juce::Label::textColourId, muted);
    status.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (status);

    setResizable (true, true);
    setResizeLimits (1000, 670, 1480, 850);
    setSize (1120, 705);
}

SwarmEditor::~SwarmEditor()
{
    morphAttachment.reset();
    pad.reset();
    knobs.clear();
}

void SwarmEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);
    juce::ColourGradient header (juce::Colour (0xff222336), 0.0f, 0.0f,
                                 background, static_cast<float> (getWidth()), 126.0f, false);
    g.setGradientFill (header);
    g.fillRect (0, 0, getWidth(), 112);

    g.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
    g.setColour (pink);
    g.drawText ("ANKS / LABS", 27, 14, 230, 19, juce::Justification::centredLeft);
    g.setColour (white);
    g.setFont (juce::Font (juce::FontOptions (37.0f).withStyle ("Bold")));
    g.drawText ("SWARM", 25, 35, 235, 56, juce::Justification::centredLeft);
    g.setColour (lime);
    g.drawText ("/ 01", 249, 35, 120, 56, juce::Justification::centredLeft);
    g.setColour (muted);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText ("DUAL SAW SYNTHESIS  •  FOUR-CORNER MORPHING  •  VST3 / STANDALONE",
                398, 48, getWidth() - 430, 35, juce::Justification::centredRight);

    drawPanel (g, leftPanel, pink);
    drawPanel (g, rightPanel, lime);
    drawPanel (g, morphPanel, blue);
    drawPanel (g, bottomPanel, blue);

    g.setFont (juce::Font (juce::FontOptions (18.0f).withStyle ("Bold")));
    g.setColour (pink);
    g.drawText ("SWARM  A", leftPanel.reduced (24).removeFromTop (58), juce::Justification::centredLeft);
    g.setColour (lime);
    g.drawText ("SWARM  B", rightPanel.reduced (24).removeFromTop (58), juce::Justification::centredLeft);
    g.setColour (white);
    g.drawText ("MORPH / XY", morphPanel.reduced (24).removeFromTop (48), juce::Justification::centredLeft);

    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.setColour (muted);
    g.drawText ("7 OSCILLATORS / PINK", leftPanel.getX() + 24, leftPanel.getY() + 52,
                leftPanel.getWidth() - 45, 16, juce::Justification::centredLeft);
    g.drawText ("7 OSCILLATORS / GREEN", rightPanel.getX() + 24, rightPanel.getY() + 52,
                rightPanel.getWidth() - 45, 16, juce::Justification::centredLeft);
    drawSwarmArt (g, leftPanel, pink, false);
    drawSwarmArt (g, rightPanel, lime, true);

    g.setFont (juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")));
    g.setColour (white);
    g.drawText ("MASTER / SHAPE", bottomPanel.getX() + 22, bottomPanel.getY() + 4, 300, 19,
                juce::Justification::centredLeft);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.setColour (muted);
    g.drawText ("V0.1  •  ORIGINAL ANKS LABS DESIGN  •  NO AUDIO FROM KICK",
                22, getHeight() - 22, getWidth() - 44, 14, juce::Justification::centredRight);
}

void SwarmEditor::resized()
{
    const int margin = 22, gap = 14;
    const int centreWidth = 330;
    const int sideWidth = (getWidth() - 2 * margin - 2 * gap - centreWidth) / 2;
    const int top = 119, panelHeight = getHeight() - 295;
    leftPanel = {margin, top, sideWidth, panelHeight};
    morphPanel = {margin + sideWidth + gap, top, centreWidth, panelHeight};
    rightPanel = {morphPanel.getRight() + gap, top, sideWidth, panelHeight};
    bottomPanel = {margin, top + panelHeight + 15, getWidth() - 2 * margin, 136};

    for (int i = 0; i < 3; ++i)
    {
        const auto cw = (sideWidth - 38) / 3;
        knobs[static_cast<size_t> (i)]->setBounds (leftPanel.getX() + 19 + i * cw,
                                                   leftPanel.getBottom() - 137, cw, 112);
        knobs[static_cast<size_t> (i + 3)]->setBounds (rightPanel.getX() + 19 + i * cw,
                                                       rightPanel.getBottom() - 137, cw, 112);
    }
    pad->setBounds (morphPanel.getX() + 27, morphPanel.getY() + 50,
                    morphPanel.getWidth() - 54, panelHeight - 164);
    const int buttonW = (centreWidth - 62) / 4;
    for (int i = 0; i < 4; ++i)
        captureButtons[static_cast<size_t> (i)].setBounds (morphPanel.getX() + 24 + i * (buttonW + 5),
                                                          morphPanel.getBottom() - 99, buttonW, 31);
    morphToggle.setBounds (morphPanel.getX() + 86, morphPanel.getBottom() - 62, 205, 27);
    status.setBounds (morphPanel.getX() + 8, morphPanel.getBottom() - 35, centreWidth - 16, 22);

    const int bottomKnobWidth = (bottomPanel.getWidth() - 26) / 7;
    for (int i = 6; i < 13; ++i)
        knobs[static_cast<size_t> (i)]->setBounds (bottomPanel.getX() + 13 + (i - 6) * bottomKnobWidth,
                                                   bottomPanel.getY() + 23, bottomKnobWidth, 105);
}
