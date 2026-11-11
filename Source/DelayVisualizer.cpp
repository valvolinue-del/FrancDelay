#include "DelayVisualizer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace
{
const juce::Colour background = juce::Colour::fromRGB(0x0a, 0x0a, 0x0f);
const juce::Colour panel = juce::Colour::fromRGB(0x12, 0x12, 0x1b);
const juce::Colour gold = juce::Colour::fromRGB(0xc9, 0xa8, 0x4c);
const juce::Colour goldBright = juce::Colour::fromRGB(0xe4, 0xc5, 0x6b);
const juce::Colour champagne = juce::Colour::fromRGB(0xf3, 0xe6, 0xc0);
const juce::Colour ivory = juce::Colour::fromRGB(0xf4, 0xef, 0xe2);

float wrap01(float value)
{
    return value - std::floor(value);
}

juce::Colour channelColour(int channel)
{
    return channel == 0 ? gold : champagne;
}
} // namespace

DelayVisualizer::DelayVisualizer(FrancDelayAudioProcessor& processorToUse)
    : processor(processorToUse)
{
    ghosts.reserve(120);
    setOpaque(true);
    readParameters();
    startTimerHz(60);
}

void DelayVisualizer::readParameters()
{
    delayMs = std::max(1.0f, processor.apvts.getRawParameterValue(ParamIDs::delayTime)->load());
    feedbackAmount = juce::jlimit(0.0f, 0.95f, processor.apvts.getRawParameterValue(ParamIDs::feedback)->load());
    mixAmount = juce::jlimit(0.0f, 1.0f, processor.apvts.getRawParameterValue(ParamIDs::mix)->load());
    pingPong = processor.apvts.getRawParameterValue(ParamIDs::pingPong)->load() >= 0.5f;
}

void DelayVisualizer::advanceBall(int channel, float direction, float delta, float spacing, float lifetime)
{
    spacing = std::max(0.012f, spacing);
    if (distanceSinceSpawn[channel] >= spacing)
        distanceSinceSpawn[channel] = std::fmod(distanceSinceSpawn[channel], spacing);

    const float oldPhase = phase[channel];
    float walked = 0.0f;
    int spawned = 0;

    while (walked + 1.0e-5f < delta && spawned < 12)
    {
        const float room = delta - walked;
        const float untilSpawn = spacing - distanceSinceSpawn[channel];
        if (untilSpawn > room)
        {
            distanceSinceSpawn[channel] += room;
            walked = delta;
            break;
        }

        walked += untilSpawn;
        distanceSinceSpawn[channel] = 0.0f;

        Ghost ghost;
        ghost.phase = wrap01(oldPhase + direction * walked);
        ghost.age = 0.0f;
        ghost.lifetime = std::max(0.05f, lifetime);
        ghost.channel = channel;
        ghost.energy = std::clamp(level[channel] * (2.4f + 1.8f * mixAmount), 0.0f, 1.0f);
        ghosts.push_back(ghost);
        ++spawned;
    }

    if (walked < delta)
        distanceSinceSpawn[channel] = std::fmod(delta - walked, spacing);

    phase[channel] = wrap01(oldPhase + direction * delta);

    const bool crossedSeam = direction >= 0.0f ? phase[channel] < oldPhase : phase[channel] > oldPhase;
    if (delta >= 1.0f || (delta > 0.0f && crossedSeam))
        pulse[channel] = 1.0f;
}

void DelayVisualizer::timerCallback()
{
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    float dt = static_cast<float>((nowMs - lastTickMs) / 1000.0);
    if (lastTickMs <= 0.0 || dt <= 0.0f || dt > 0.1f)
        dt = 1.0f / 60.0f;
    lastTickMs = nowMs;

    readParameters();

    const float period = delayMs * 0.001f;
    const float delta = dt / period;
    const float follow = 1.0f - std::exp(-dt / 0.05f);
    const float pulseDecay = 0.08f + 0.14f * feedbackAmount;

    for (int channel = 0; channel < 2; ++channel)
    {
        const float target = std::max(processor.getInputRms(channel) * 1.7f, processor.getWetEnvelope(channel));
        level[channel] = std::lerp(level[channel], target, follow);
        pulse[channel] *= std::exp(-dt / pulseDecay);
    }

    wavePhase = wrap01(wavePhase + dt * 0.065f);

    const float spacing = juce::jmap(feedbackAmount, 0.0f, 0.95f, 0.18f, 0.028f);
    const float lifetime = period * (0.22f + 1.25f * feedbackAmount);
    advanceBall(0, 1.0f, delta, spacing, lifetime);
    advanceBall(1, pingPong ? -1.0f : 1.0f, delta, spacing, lifetime);

    for (auto& ghost : ghosts)
        ghost.age += dt;

    ghosts.erase(std::remove_if(ghosts.begin(), ghosts.end(),
                                 [](const Ghost& ghost) { return ghost.age >= ghost.lifetime; }),
                 ghosts.end());

    constexpr std::size_t maxGhosts = 120;
    if (ghosts.size() > maxGhosts)
        ghosts.erase(ghosts.begin(), ghosts.begin() + static_cast<std::ptrdiff_t>(ghosts.size() - maxGhosts));

    repaint();
}

juce::Point<float> DelayVisualizer::pointOnWave(float travelPhase, juce::Rectangle<float> area, float amplitude) const
{
    const float wrapped = wrap01(travelPhase);
    const float x = area.getX() + wrapped * area.getWidth();
    const float theta = (wrapped * 1.35f + wavePhase) * (std::numbers::pi_v<float> * 2.0f);
    const float shaped = std::sin(theta) * 0.84f + std::sin(theta * 2.0f + 0.65f) * 0.16f;
    return { x, area.getCentreY() + shaped * amplitude };
}

void DelayVisualizer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(background);

    g.setColour(panel);
    g.fillRoundedRectangle(bounds.reduced(1.0f), 16.0f);
    g.setColour(gold.withAlpha(0.32f));
    g.drawRoundedRectangle(bounds.reduced(1.5f), 16.0f, 1.0f);

    juce::Path clip;
    clip.addRoundedRectangle(bounds.reduced(2.0f), 15.0f);
    g.reduceClipRegion(clip);

    auto waveArea = bounds.reduced(28.0f, 22.0f);
    const float energy = std::clamp((level[0] + level[1]) * 2.2f, 0.0f, 1.0f);
    const float amplitude = waveArea.getHeight() * (0.27f + 0.07f * energy);

    juce::ColourGradient wash(gold.withAlpha(0.10f), waveArea.getCentreX(), waveArea.getCentreY(),
                              gold.withAlpha(0.0f), waveArea.getCentreX(), waveArea.getBottom(), true);
    g.setGradientFill(wash);
    g.fillRoundedRectangle(waveArea.expanded(8.0f, 0.0f), 12.0f);

    g.setColour(gold.withAlpha(0.10f));
    g.drawHorizontalLine(juce::roundToInt(waveArea.getCentreY()), waveArea.getX(), waveArea.getRight());

    juce::Path stroke;
    juce::Path fill;
    const int steps = std::max(48, static_cast<int>(waveArea.getWidth()));
    for (int step = 0; step <= steps; step += 2)
    {
        const float travel = static_cast<float>(step) / static_cast<float>(steps);
        const auto point = pointOnWave(travel, waveArea, amplitude);
        if (step == 0)
        {
            stroke.startNewSubPath(point);
            fill.startNewSubPath(point.x, waveArea.getBottom());
            fill.lineTo(point);
        }
        else
        {
            stroke.lineTo(point);
            fill.lineTo(point);
        }
    }
    fill.lineTo(waveArea.getRight(), waveArea.getBottom());
    fill.closeSubPath();

    g.setColour(gold.withAlpha(0.08f));
    g.fillPath(fill);

    g.setColour(gold.withAlpha(0.16f));
    g.strokePath(stroke, juce::PathStrokeType(7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(gold.withAlpha(0.45f));
    g.strokePath(stroke, juce::PathStrokeType(2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(goldBright.withAlpha(0.95f));
    g.strokePath(stroke, juce::PathStrokeType(1.35f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    for (const auto& ghost : ghosts)
    {
        const float life = 1.0f - juce::jlimit(0.0f, 1.0f, ghost.age / ghost.lifetime);
        const auto point = pointOnWave(ghost.phase, waveArea, amplitude);
        const float radius = (4.5f + ghost.energy * 7.0f) * (0.65f + 0.45f * life);
        const float alpha = life * life * (0.18f + 0.55f * feedbackAmount);
        g.setColour(channelColour(ghost.channel).withAlpha(alpha * 0.35f));
        g.fillEllipse(point.x - radius, point.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(channelColour(ghost.channel).withAlpha(alpha));
        g.drawEllipse(point.x - radius, point.y - radius, radius * 2.0f, radius * 2.0f, 1.3f);
    }

    const juce::Font labelFont(juce::FontOptions(11.0f).withKerningFactor(0.08f));
    g.setFont(labelFont);

    for (int channel = 0; channel < 2; ++channel)
    {
        const auto point = pointOnWave(phase[channel], waveArea, amplitude);
        const float channelEnergy = std::clamp(level[channel] * 3.2f, 0.0f, 1.0f);
        const float radius = 7.0f + channelEnergy * (8.0f + 16.0f * mixAmount) + pulse[channel] * (4.0f + 8.0f * feedbackAmount);
        const float brightness = std::clamp(0.48f + 0.28f * mixAmount + 0.40f * channelEnergy + 0.30f * pulse[channel], 0.35f, 1.0f);
        const auto colour = channelColour(channel);

        if (pulse[channel] > 0.04f)
        {
            const float ripple = radius + (1.0f - pulse[channel]) * 22.0f;
            g.setColour(colour.withAlpha(0.40f * pulse[channel]));
            g.drawEllipse(point.x - ripple, point.y - ripple, ripple * 2.0f, ripple * 2.0f, 1.4f);
        }

        for (int ring = 4; ring >= 1; --ring)
        {
            const float glow = radius + static_cast<float>(ring) * 3.4f;
            g.setColour(colour.withAlpha(0.045f * brightness * static_cast<float>(5 - ring)));
            g.fillEllipse(point.x - glow, point.y - glow, glow * 2.0f, glow * 2.0f);
        }

        g.setColour(colour.withMultipliedBrightness(0.55f + 0.7f * brightness));
        g.fillEllipse(point.x - radius, point.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(juce::Colours::white.withAlpha(0.28f * brightness));
        g.fillEllipse(point.x - radius * 0.32f, point.y - radius * 0.46f, radius * 0.42f, radius * 0.30f);

        float labelY = point.y - radius - 16.0f;
        if (labelY < waveArea.getY())
            labelY = point.y + radius + 2.0f;
        g.setColour(ivory.withAlpha(0.85f));
        g.drawText(channel == 0 ? "L" : "R",
                   juce::Rectangle<float>(point.x - 10.0f, labelY, 20.0f, 14.0f),
                   juce::Justification::centred,
                   false);
    }

    g.setColour(gold.withAlpha(0.72f));
    g.setFont(juce::Font(juce::FontOptions(11.0f).withKerningFactor(0.16f)));
    g.drawText(pingPong ? "PING-PONG" : "STEREO",
               bounds.reduced(18.0f, 12.0f).removeFromTop(16.0f),
               juce::Justification::centredRight,
               false);

    g.setColour(ivory.withAlpha(0.45f));
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.drawText(formatCaption(), bounds.reduced(18.0f, 12.0f).removeFromTop(16.0f), juce::Justification::centredLeft, false);
}

juce::String DelayVisualizer::formatCaption() const
{
    if (delayMs >= 1000.0f)
        return juce::String(delayMs / 1000.0f, 2) + " s";
    if (delayMs >= 100.0f)
        return juce::String(juce::roundToInt(delayMs)) + " ms";
    return juce::String(delayMs, 1) + " ms";
}
