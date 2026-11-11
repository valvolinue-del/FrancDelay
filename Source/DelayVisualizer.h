#pragma once

#include "PluginProcessor.h"

#include <vector>

// Message-thread animation of the delay field. It reads parameter atomics and the
// processor's meter atomics; it never locks and never touches the audio thread.
class DelayVisualizer : public juce::Component,
                        private juce::Timer
{
public:
    explicit DelayVisualizer(FrancDelayAudioProcessor& processorToUse);

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;
    void readParameters();
    void advanceBall(int channel, float direction, float delta, float spacing, float lifetime);
    juce::Point<float> pointOnWave(float travelPhase, juce::Rectangle<float> area, float amplitude) const;
    juce::String formatCaption() const;

    struct Ghost
    {
        float phase = 0.0f;
        float age = 0.0f;
        float lifetime = 1.0f;
        int channel = 0;
        float energy = 0.0f;
    };

    FrancDelayAudioProcessor& processor;
    std::vector<Ghost> ghosts;

    float phase[2] { 0.08f, 0.58f };
    float distanceSinceSpawn[2] { 0.0f, 0.0f };
    float pulse[2] { 0.0f, 0.0f };
    float level[2] { 0.0f, 0.0f };

    float delayMs = 350.0f;
    float feedbackAmount = 0.4f;
    float mixAmount = 0.5f;
    bool pingPong = false;
    float wavePhase = 0.0f;
    double lastTickMs = 0.0;
};
