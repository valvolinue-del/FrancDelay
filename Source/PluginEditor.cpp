#include "PluginEditor.h"

#include <algorithm>

namespace
{
const juce::Colour background = juce::Colour::fromRGB(0x0a, 0x0a, 0x0f);
const juce::Colour panel = juce::Colour::fromRGB(0x12, 0x12, 0x1b);
const juce::Colour gold = juce::Colour::fromRGB(0xc9, 0xa8, 0x4c);
const juce::Colour goldBright = juce::Colour::fromRGB(0xe4, 0xc5, 0x6b);
const juce::Colour ivory = juce::Colour::fromRGB(0xf4, 0xef, 0xe2);
const juce::Colour muted = juce::Colour::fromRGB(0x9a, 0x91, 0x7c);
const juce::Colour track = juce::Colour::fromRGB(0x2a, 0x28, 0x24);

void drawTrackedText(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                     const juce::Font& font, juce::Colour colour, float tracking)
{
    float width = 0.0f;
    for (int index = 0; index < text.length(); ++index)
        width += juce::GlyphArrangement::getStringWidth(font, text.substring(index, index + 1));
    if (text.length() > 1)
        width += tracking * static_cast<float>(text.length() - 1);

    float x = area.getCentreX() - width * 0.5f;
    g.setFont(font);
    g.setColour(colour);
    for (int index = 0; index < text.length(); ++index)
    {
        const auto glyph = text.substring(index, index + 1);
        const float glyphWidth = juce::GlyphArrangement::getStringWidth(font, glyph);
        g.drawText(glyph, juce::Rectangle<float>(x, area.getY(), glyphWidth + tracking + 2.0f, area.getHeight()),
                   juce::Justification::centredLeft, false);
        x += glyphWidth + tracking;
    }
}
} // namespace

FrancLookAndFeel::FrancLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, ivory);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxHighlightColourId, gold.withAlpha(0.28f));
    setColour(juce::Slider::rotarySliderFillColourId, gold);
    setColour(juce::Slider::rotarySliderOutlineColourId, track);
    setColour(juce::Slider::thumbColourId, goldBright);
    setColour(juce::Label::textColourId, gold);
    setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::ResizableWindow::backgroundColourId, background);
    setColour(juce::TextEditor::textColourId, ivory);
    setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromRGB(0x16, 0x16, 0x1c));
    setColour(juce::TextEditor::outlineColourId, gold);
    setColour(juce::TextEditor::focusedOutlineColourId, goldBright);
    setColour(juce::TextEditor::highlightColourId, gold.withAlpha(0.35f));
    setColour(juce::CaretComponent::caretColourId, gold);
    setColour(juce::TooltipWindow::backgroundColourId, juce::Colour::fromRGB(0x16, 0x16, 0x1c));
    setColour(juce::TooltipWindow::textColourId, ivory);
    setColour(juce::TooltipWindow::outlineColourId, gold);
}

void FrancLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float rotaryStartAngle, float rotaryEndAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                         static_cast<float>(width), static_cast<float>(height))
                      .reduced(6.0f);
    const float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const float arcRadius = radius - 5.0f;

    juce::Path trackPath;
    trackPath.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(track);
    g.strokePath(trackPath, juce::PathStrokeType(3.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valuePath;
    valuePath.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);
    g.setColour(gold.withAlpha(0.28f));
    g.strokePath(valuePath, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(gold);
    g.strokePath(valuePath, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float knobRadius = radius * 0.68f;
    juce::ColourGradient body(juce::Colour::fromRGB(0x2c, 0x2a, 0x32), centre.x, centre.y - knobRadius * 0.6f,
                              juce::Colour::fromRGB(0x0c, 0x0c, 0x10), centre.x, centre.y + knobRadius, true);
    g.setGradientFill(body);
    g.fillEllipse(centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);

    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.fillEllipse(centre.x - knobRadius * 0.55f, centre.y - knobRadius * 0.72f, knobRadius * 1.05f, knobRadius * 0.48f);

    g.setColour(gold.withAlpha(0.85f));
    g.drawEllipse(centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f, 1.15f);

    juce::Path pointer;
    const float pointerLength = knobRadius * 0.72f;
    pointer.addRoundedRectangle(-1.15f, -pointerLength, 2.3f, pointerLength * 0.58f, 1.0f);
    g.setColour(goldBright);
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
}

void FrancLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(4.0f, 2.0f);
    auto pillRow = bounds.removeFromTop(std::min(46.0f, bounds.getHeight() * 0.62f));
    auto pill = juce::Rectangle<float>(56.0f, 28.0f).withCentre(pillRow.getCentre());
    const bool on = button.getToggleState();

    g.setColour(on ? gold : track);
    g.fillRoundedRectangle(pill, 14.0f);
    g.setColour(gold.withAlpha(on || highlighted ? 1.0f : 0.45f));
    g.drawRoundedRectangle(pill, 14.0f, highlighted || down ? 1.8f : 1.15f);

    auto thumb = pill.reduced(3.5f).withWidth(21.0f);
    if (on)
        thumb.setX(pill.getRight() - thumb.getWidth() - 3.5f);
    g.setColour(on ? background : gold);
    g.fillEllipse(thumb);

    g.setColour(on ? goldBright : muted);
    g.setFont(juce::Font(juce::FontOptions(12.0f).withKerningFactor(0.08f)));
    g.drawText(on ? "ON" : "OFF", bounds, juce::Justification::centred, false);
}

void FrancLookAndFeel::drawCornerResizer(juce::Graphics& g, int width, int height, bool, bool)
{
    g.setColour(gold.withAlpha(0.55f));
    const float right = static_cast<float>(width);
    const float bottom = static_cast<float>(height);
    for (int index = 0; index < 3; ++index)
    {
        const float offset = static_cast<float>(index * 4 + 4);
        g.drawLine(right - offset - 8.0f, bottom - 3.0f, right - 3.0f, bottom - offset - 8.0f, 1.1f);
    }
}

juce::Label* FrancLookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox(slider);
    label->setFont(juce::Font(juce::FontOptions(13.0f)));
    label->setJustificationType(juce::Justification::centred);
    label->setColour(juce::Label::textColourId, ivory);
    label->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return label;
}

juce::Font FrancLookAndFeel::getLabelFont(juce::Label&)
{
    return juce::Font(juce::FontOptions(13.0f));
}

FrancDelayAudioProcessorEditor::FrancDelayAudioProcessorEditor(FrancDelayAudioProcessor& processorIn)
    : AudioProcessorEditor(&processorIn),
      processor(processorIn),
      visualizer(processorIn),
      tooltipWindow(this, 650)
{
    setLookAndFeel(&lookAndFeel);
    tooltipWindow.setColour(juce::TooltipWindow::backgroundColourId, juce::Colour::fromRGB(0x16, 0x16, 0x1c));
    tooltipWindow.setColour(juce::TooltipWindow::textColourId, ivory);
    tooltipWindow.setColour(juce::TooltipWindow::outlineColourId, gold);

    addAndMakeVisible(visualizer);

    configureLabel(delayLabel, "DELAY");
    configureLabel(feedbackLabel, "FEEDBACK");
    configureLabel(mixLabel, "MIX");
    configureLabel(cutoffLabel, "CUTOFF");
    configureLabel(pingLabel, "PING-PONG");

    configureKnob(delaySlider);
    configureKnob(feedbackSlider);
    configureKnob(mixSlider);
    configureKnob(cutoffSlider);

    delaySlider.setTooltip("Time between echoes.");
    feedbackSlider.setTooltip("How much of each echo is returned to the delay.");
    mixSlider.setTooltip("Balance of the dry signal and the echoes.");
    cutoffSlider.setTooltip("Low-pass cutoff in the feedback path.");

    pingButton.setClickingTogglesState(true);
    pingButton.setTooltip("Cross-feed feedback between the left and right channels.");
    addAndMakeVisible(pingButton);

    delayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, ParamIDs::delayTime, delaySlider);
    feedbackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, ParamIDs::feedback, feedbackSlider);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, ParamIDs::mix, mixSlider);
    cutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, ParamIDs::cutoff, cutoffSlider);
    pingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, ParamIDs::pingPong, pingButton);

    setResizable(true, true);
    setResizeLimits(700, 460, 1600, 1100);
    setSize(800, 500);
}

FrancDelayAudioProcessorEditor::~FrancDelayAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void FrancDelayAudioProcessorEditor::configureKnob(juce::Slider& slider)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 96, 20);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                               juce::MathConstants<float>::pi * 2.8f,
                               true);
    slider.setColour(juce::Slider::textBoxTextColourId, ivory);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(slider);
}

void FrancDelayAudioProcessorEditor::configureLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::Font(juce::FontOptions(11.5f).withKerningFactor(0.12f)));
    label.setColour(juce::Label::textColourId, gold);
    label.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(label);
}

void FrancDelayAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(background);

    juce::ColourGradient glow(gold.withAlpha(0.09f), static_cast<float>(getWidth()) * 0.5f, 28.0f,
                              background, static_cast<float>(getWidth()) * 0.5f, 240.0f, true);
    g.setGradientFill(glow);
    g.fillRect(getLocalBounds());

    if (! titleBounds.isEmpty())
    {
        auto title = titleBounds.toFloat();
        auto rule = title.removeFromTop(16.0f);
        const float centreX = rule.getCentreX();
        g.setColour(gold.withAlpha(0.75f));
        g.drawLine(rule.getX() + 40.0f, rule.getCentreY(), centreX - 14.0f, rule.getCentreY(), 1.0f);
        g.drawLine(centreX + 14.0f, rule.getCentreY(), rule.getRight() - 40.0f, rule.getCentreY(), 1.0f);

        juce::Path diamond;
        const float y = rule.getCentreY();
        diamond.startNewSubPath(centreX, y - 4.0f);
        diamond.lineTo(centreX + 4.0f, y);
        diamond.lineTo(centreX, y + 4.0f);
        diamond.lineTo(centreX - 4.0f, y);
        diamond.closeSubPath();
        g.setColour(gold);
        g.fillPath(diamond);

        const juce::Font titleFont(juce::FontOptions(34.0f, juce::Font::bold).withKerningFactor(0.045f));
        drawTrackedText(g, "FrancDelay", title.removeFromTop(40.0f), titleFont, gold, 1.4f);

        const juce::Font subtitleFont(juce::FontOptions(11.0f).withKerningFactor(0.18f));
        drawTrackedText(g, "STEREO ECHO", title, subtitleFont, muted, 2.2f);
    }

    if (! controlsBounds.isEmpty())
    {
        auto panelBounds = controlsBounds.toFloat();
        g.setColour(panel);
        g.fillRoundedRectangle(panelBounds, 16.0f);
        g.setColour(gold.withAlpha(0.20f));
        g.drawRoundedRectangle(panelBounds.reduced(0.5f), 16.0f, 1.0f);
    }
}

void FrancDelayAudioProcessorEditor::resized()
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    auto area = getLocalBounds().reduced(26, 16);
    titleBounds = area.removeFromTop(74);
    area.removeFromTop(6);
    controlsBounds = area.removeFromBottom(156);
    area.removeFromTop(8);
    visualizer.setBounds(area);

    auto columns = controlsBounds.reduced(8, 8);
    juce::Component* controls[] = { &delaySlider, &feedbackSlider, &mixSlider, &cutoffSlider, &pingButton };
    juce::Label* labels[] = { &delayLabel, &feedbackLabel, &mixLabel, &cutoffLabel, &pingLabel };
    const int count = 5;

    for (int index = 0; index < count; ++index)
    {
        const int remaining = count - index;
        auto column = columns.removeFromLeft(columns.getWidth() / remaining);
        labels[index]->setBounds(column.removeFromTop(18));
        controls[index]->setBounds(column);
    }
}
