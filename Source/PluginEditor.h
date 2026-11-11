#pragma once

#include "DelayVisualizer.h"
#include "PluginProcessor.h"

class FrancLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FrancLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height, float sliderPosProportional,
                          float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;
    void drawCornerResizer(juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;
    juce::Label* createSliderTextBox(juce::Slider&) override;
    juce::Font getLabelFont(juce::Label&) override;
};

class FrancDelayAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit FrancDelayAudioProcessorEditor(FrancDelayAudioProcessor&);
    ~FrancDelayAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void configureKnob(juce::Slider& slider);
    void configureLabel(juce::Label& label, const juce::String& text);

    FrancDelayAudioProcessor& processor;
    FrancLookAndFeel lookAndFeel;
    DelayVisualizer visualizer;
    juce::TooltipWindow tooltipWindow;

    juce::Label delayLabel;
    juce::Label feedbackLabel;
    juce::Label mixLabel;
    juce::Label cutoffLabel;
    juce::Label pingLabel;

    juce::Slider delaySlider;
    juce::Slider feedbackSlider;
    juce::Slider mixSlider;
    juce::Slider cutoffSlider;
    juce::ToggleButton pingButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> feedbackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pingAttachment;

    juce::Rectangle<int> titleBounds;
    juce::Rectangle<int> controlsBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FrancDelayAudioProcessorEditor)
};
