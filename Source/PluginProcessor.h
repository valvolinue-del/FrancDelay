#pragma once

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

namespace ParamIDs
{
inline constexpr auto delayTime = "delayTime";
inline constexpr auto feedback = "feedback";
inline constexpr auto mix = "mix";
inline constexpr auto cutoff = "cutoff";
inline constexpr auto pingPong = "pingPong";
} // namespace ParamIDs

// Stereo delay with a damped feedback path. Meter atomics are written on the
// audio thread and read by the editor without taking a lock.
class FrancDelayAudioProcessor : public juce::AudioProcessor
{
public:
    FrancDelayAudioProcessor();
    ~FrancDelayAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    bool supportsDoublePrecisionProcessing() const override { return true; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    float getInputPeak(int channel) const noexcept
    {
        return inputPeak[static_cast<size_t>(juce::jlimit(0, 1, channel))].load(std::memory_order_relaxed);
    }

    float getInputRms(int channel) const noexcept
    {
        return inputRms[static_cast<size_t>(juce::jlimit(0, 1, channel))].load(std::memory_order_relaxed);
    }

    float getWetEnvelope(int channel) const noexcept
    {
        return wetEnvelope[static_cast<size_t>(juce::jlimit(0, 1, channel))].load(std::memory_order_relaxed);
    }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    using DelayLine = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>;

    DelayLine delayL;
    DelayLine delayR;
    juce::dsp::FirstOrderTPTFilter<float> lowpassL;
    juce::dsp::FirstOrderTPTFilter<float> lowpassR;

    juce::LinearSmoothedValue<float> delaySamplesSmooth;
    juce::LinearSmoothedValue<float> feedbackSmooth;
    juce::LinearSmoothedValue<float> mixSmooth;
    juce::LinearSmoothedValue<float> cutoffSmooth;
    juce::LinearSmoothedValue<float> pingSmooth;

    juce::AudioBuffer<float> floatScratch;

    std::atomic<float> inputPeak[2] { 0.0f, 0.0f };
    std::atomic<float> inputRms[2] { 0.0f, 0.0f };
    std::atomic<float> wetEnvelope[2] { 0.0f, 0.0f };

    float wetEnvState[2] { 0.0f, 0.0f };
    float wetRelease = 0.999f;
    float lastCutoff = -1.0f;
    double currentSampleRate = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FrancDelayAudioProcessor)
};
