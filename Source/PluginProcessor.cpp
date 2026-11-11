#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
juce::String formatDelay(float value, int maximumLength)
{
    juce::String text;
    if (value >= 1000.0f)
        text = juce::String(value / 1000.0f, 2) + " s";
    else if (value >= 100.0f)
        text = juce::String(juce::roundToInt(value)) + " ms";
    else
        text = juce::String(value, 1) + " ms";

    return maximumLength > 0 ? text.substring(0, maximumLength) : text;
}

float parseDelay(const juce::String& text)
{
    const auto trimmed = text.trim().toLowerCase();
    if (trimmed.endsWith("ms"))
        return trimmed.dropLastCharacters(2).trim().getFloatValue();
    if (trimmed.endsWithChar('s'))
        return trimmed.dropLastCharacters(1).trim().getFloatValue() * 1000.0f;
    return trimmed.getFloatValue();
}

juce::String formatPercent(float value, int maximumLength)
{
    const auto text = juce::String(juce::roundToInt(value * 100.0f)) + " %";
    return maximumLength > 0 ? text.substring(0, maximumLength) : text;
}

float parsePercent(const juce::String& text)
{
    const auto trimmed = text.trim().toLowerCase();
    const float number = trimmed.getFloatValue();
    if (trimmed.containsChar('%') || number > 1.0f)
        return number / 100.0f;
    return number;
}

juce::String formatCutoff(float value, int maximumLength)
{
    juce::String text;
    if (value >= 10000.0f)
        text = juce::String(value / 1000.0f, 1) + " kHz";
    else if (value >= 1000.0f)
        text = juce::String(value / 1000.0f, 2) + " kHz";
    else
        text = juce::String(juce::roundToInt(value)) + " Hz";

    return maximumLength > 0 ? text.substring(0, maximumLength) : text;
}

float parseCutoff(const juce::String& text)
{
    const auto trimmed = text.trim().toLowerCase();
    const float number = trimmed.getFloatValue();
    return trimmed.containsChar('k') ? number * 1000.0f : number;
}

float softClip(float sample) noexcept
{
    constexpr float threshold = 0.95f;
    const float amount = std::abs(sample);
    if (amount <= threshold)
        return sample;

    const float shaped = threshold + 0.05f * std::tanh((amount - threshold) / 0.05f);
    return std::copysign(shaped, sample);
}

juce::NormalisableRange<float> logarithmicFrequencyRange()
{
    return {
        20.0f,
        20000.0f,
        [](float start, float end, float proportion)
        {
            return start * std::pow(end / start, proportion);
        },
        [](float start, float end, float value)
        {
            const float clamped = juce::jlimit(start, end, value);
            return std::log(clamped / start) / std::log(end / start);
        },
    };
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout FrancDelayAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::delayTime, 1 },
        "Delay Time",
        juce::NormalisableRange<float>(1.0f, 2000.0f, 0.01f, 0.5f),
        350.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("ms")
            .withStringFromValueFunction(formatDelay)
            .withValueFromStringFunction(parseDelay)));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::feedback, 1 },
        "Feedback",
        juce::NormalisableRange<float>(0.0f, 0.95f, 0.001f),
        0.4f,
        juce::AudioParameterFloatAttributes()
            .withLabel("%")
            .withStringFromValueFunction(formatPercent)
            .withValueFromStringFunction(parsePercent)));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::mix, 1 },
        "Wet/Dry Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.5f,
        juce::AudioParameterFloatAttributes()
            .withLabel("%")
            .withStringFromValueFunction(formatPercent)
            .withValueFromStringFunction(parsePercent)));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::cutoff, 1 },
        "Cutoff",
        logarithmicFrequencyRange(),
        8000.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("Hz")
            .withStringFromValueFunction(formatCutoff)
            .withValueFromStringFunction(parseCutoff)));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParamIDs::pingPong, 1 },
        "Ping-Pong",
        false,
        juce::AudioParameterBoolAttributes().withStringFromValueFunction(
            [](bool enabled, int) { return enabled ? juce::String("On") : juce::String("Off"); })));

    return layout;
}

FrancDelayAudioProcessor::FrancDelayAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "FrancDelay", createParameterLayout())
{
}

void FrancDelayAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    const int maxDelaySamples = static_cast<int>(std::ceil(sampleRate * 2.0))
                              + static_cast<int>(std::ceil(sampleRate * 0.05));

    const juce::dsp::ProcessSpec spec {
        .sampleRate = sampleRate,
        .maximumBlockSize = static_cast<juce::uint32>(std::max(1, samplesPerBlock)),
        .numChannels = 1u,
    };

    delayL.setMaximumDelayInSamples(maxDelaySamples);
    delayR.setMaximumDelayInSamples(maxDelaySamples);
    delayL.prepare(spec);
    delayR.prepare(spec);

    lowpassL.setType(juce::dsp::FirstOrderTPTFilterType::lowpass);
    lowpassR.setType(juce::dsp::FirstOrderTPTFilterType::lowpass);
    lowpassL.prepare(spec);
    lowpassR.prepare(spec);
    lowpassL.reset();
    lowpassR.reset();

    const float initialDelayMs = apvts.getRawParameterValue(ParamIDs::delayTime)->load();
    delaySamplesSmooth.reset(sampleRate, 0.05);
    delaySamplesSmooth.setCurrentAndTargetValue(initialDelayMs * 0.001f * static_cast<float>(sampleRate));

    feedbackSmooth.reset(sampleRate, 0.02);
    feedbackSmooth.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamIDs::feedback)->load());

    mixSmooth.reset(sampleRate, 0.02);
    mixSmooth.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamIDs::mix)->load());

    cutoffSmooth.reset(sampleRate, 0.03);
    cutoffSmooth.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamIDs::cutoff)->load());

    pingSmooth.reset(sampleRate, 0.02);
    pingSmooth.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamIDs::pingPong)->load());

    wetEnvState[0] = 0.0f;
    wetEnvState[1] = 0.0f;
    wetRelease = std::exp(-1.0f / (0.12f * static_cast<float>(sampleRate)));
    lastCutoff = -1.0f;

    floatScratch.setSize(2, std::max(1, samplesPerBlock), false, false, true);
}

void FrancDelayAudioProcessor::releaseResources()
{
    currentSampleRate = 0.0;
    delayL.reset();
    delayR.reset();
    lowpassL.reset();
    lowpassR.reset();
    wetEnvState[0] = 0.0f;
    wetEnvState[1] = 0.0f;
    inputPeak[0].store(0.0f, std::memory_order_relaxed);
    inputPeak[1].store(0.0f, std::memory_order_relaxed);
    inputRms[0].store(0.0f, std::memory_order_relaxed);
    inputRms[1].store(0.0f, std::memory_order_relaxed);
    wetEnvelope[0].store(0.0f, std::memory_order_relaxed);
    wetEnvelope[1].store(0.0f, std::memory_order_relaxed);
    floatScratch.setSize(0, 0);
}

bool FrancDelayAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    const bool inputOk = input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo();
    const bool outputOk = output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();

    if (! inputOk || ! outputOk)
        return false;

    // Stereo in / mono out would throw away a channel. Mono in / stereo out stays,
    // so ping-pong can bloom a mono source into the stereo field.
    if (input == juce::AudioChannelSet::stereo() && output == juce::AudioChannelSet::mono())
        return false;

    return true;
}

void FrancDelayAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numOutput = getTotalNumOutputChannels();

    if (currentSampleRate <= 0.0 || numSamples <= 0)
    {
        buffer.clear();
        return;
    }

    for (int channel = numOutput; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, numSamples);

    const int numInput = getTotalNumInputChannels();
    const float* inL = numInput > 0 ? buffer.getReadPointer(0) : nullptr;
    const float* inR = numInput > 1 ? buffer.getReadPointer(1) : nullptr;
    float* outL = numOutput > 0 ? buffer.getWritePointer(0) : nullptr;
    float* outR = numOutput > 1 ? buffer.getWritePointer(1) : nullptr;

    delaySamplesSmooth.setTargetValue(apvts.getRawParameterValue(ParamIDs::delayTime)->load() * 0.001f
                                       * static_cast<float>(currentSampleRate));
    feedbackSmooth.setTargetValue(apvts.getRawParameterValue(ParamIDs::feedback)->load());
    mixSmooth.setTargetValue(apvts.getRawParameterValue(ParamIDs::mix)->load());
    cutoffSmooth.setTargetValue(apvts.getRawParameterValue(ParamIDs::cutoff)->load());
    pingSmooth.setTargetValue(apvts.getRawParameterValue(ParamIDs::pingPong)->load() >= 0.5f ? 1.0f : 0.0f);

    const float maxDelay = std::max(2.0f, static_cast<float>(delayL.getMaximumDelayInSamples()) - 1.0f);
    const float nyquist = std::max(21.0f, static_cast<float>(currentSampleRate * 0.49));

    double sumSqL = 0.0;
    double sumSqR = 0.0;
    float peakL = 0.0f;
    float peakR = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float xL = inL != nullptr ? inL[sample] : 0.0f;
        const float xR = inR != nullptr ? inR[sample] : xL;

        const float delaySamples = juce::jlimit(2.0f, maxDelay, delaySamplesSmooth.getNextValue());
        const float feedback = feedbackSmooth.getNextValue();
        const float wet = mixSmooth.getNextValue();
        const float dry = 1.0f - wet;
        const float ping = pingSmooth.getNextValue();

        const float cutoff = juce::jlimit(20.0f, nyquist, cutoffSmooth.getNextValue());
        if (std::abs(cutoff - lastCutoff) > 0.25f)
        {
            lowpassL.setCutoffFrequency(cutoff);
            lowpassR.setCutoffFrequency(cutoff);
            lastCutoff = cutoff;
        }

        float delayedL = lowpassL.processSample(0, delayL.popSample(0, delaySamples, true));
        float delayedR = lowpassR.processSample(0, delayR.popSample(0, delaySamples, true));

        // Ping fully cross-feeds the feedback. The smoother avoids a click on toggle.
        const float feedbackL = delayedL + (delayedR - delayedL) * ping;
        const float feedbackR = delayedR + (delayedL - delayedR) * ping;

        // A mono source fades its direct right-channel write so the cross-feed alternates sides.
        const float sendL = xL;
        const float sendR = numInput > 1 ? xR : xR * (1.0f - ping);

        float writeL = softClip(sendL + feedbackL * feedback);
        float writeR = softClip(sendR + feedbackR * feedback);
        if (! std::isfinite(writeL))
            writeL = 0.0f;
        if (! std::isfinite(writeR))
            writeR = 0.0f;

        delayL.pushSample(0, writeL);
        delayR.pushSample(0, writeR);

        const float yL = xL * dry + delayedL * wet;
        const float yR = xR * dry + delayedR * wet;

        if (outL != nullptr && outR != nullptr)
        {
            outL[sample] = yL;
            outR[sample] = yR;
        }
        else if (outL != nullptr)
        {
            outL[sample] = 0.5f * (yL + yR);
        }

        peakL = std::max(peakL, std::abs(xL));
        peakR = std::max(peakR, std::abs(xR));
        sumSqL += static_cast<double>(xL) * static_cast<double>(xL);
        sumSqR += static_cast<double>(xR) * static_cast<double>(xR);

        const float echoL = std::abs(delayedL);
        const float echoR = std::abs(delayedR);
        wetEnvState[0] = echoL > wetEnvState[0] ? echoL : wetEnvState[0] * wetRelease + echoL * (1.0f - wetRelease);
        wetEnvState[1] = echoR > wetEnvState[1] ? echoR : wetEnvState[1] * wetRelease + echoR * (1.0f - wetRelease);
    }

    lowpassL.snapToZero();
    lowpassR.snapToZero();

    const float inverseCount = 1.0f / static_cast<float>(numSamples);
    inputPeak[0].store(peakL, std::memory_order_relaxed);
    inputPeak[1].store(peakR, std::memory_order_relaxed);
    inputRms[0].store(static_cast<float>(std::sqrt(sumSqL * static_cast<double>(inverseCount))), std::memory_order_relaxed);
    inputRms[1].store(static_cast<float>(std::sqrt(sumSqR * static_cast<double>(inverseCount))), std::memory_order_relaxed);
    wetEnvelope[0].store(wetEnvState[0], std::memory_order_relaxed);
    wetEnvelope[1].store(wetEnvState[1], std::memory_order_relaxed);
}

void FrancDelayAudioProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    if (floatScratch.getNumChannels() < numChannels || floatScratch.getNumSamples() < numSamples)
        floatScratch.setSize(std::max(1, numChannels), std::max(1, numSamples), false, false, true);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        const double* source = buffer.getReadPointer(channel);
        float* destination = floatScratch.getWritePointer(channel);
        for (int sample = 0; sample < numSamples; ++sample)
            destination[sample] = static_cast<float>(source[sample]);
    }

    juce::AudioBuffer<float> view(floatScratch.getArrayOfWritePointers(), numChannels, numSamples);
    processBlock(view, midi);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        const float* source = floatScratch.getReadPointer(channel);
        double* destination = buffer.getWritePointer(channel);
        for (int sample = 0; sample < numSamples; ++sample)
            destination[sample] = static_cast<double>(source[sample]);
    }
}

double FrancDelayAudioProcessor::getTailLengthSeconds() const
{
    const float feedback = apvts.getRawParameterValue(ParamIDs::feedback)->load();
    const double delaySec = static_cast<double>(apvts.getRawParameterValue(ParamIDs::delayTime)->load()) * 0.001;
    if (feedback < 0.01f || delaySec <= 0.0)
        return delaySec;

    const double repeats = std::log(0.001) / std::log(static_cast<double>(feedback));
    return juce::jlimit(0.0, 60.0, delaySec * std::max(0.0, repeats));
}

void FrancDelayAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (const auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void FrancDelayAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* FrancDelayAudioProcessor::createEditor()
{
    return new FrancDelayAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FrancDelayAudioProcessor();
}
