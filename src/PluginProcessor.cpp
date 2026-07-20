#include "PluginProcessor.h"

#include "PluginEditor.h"

using namespace juce;

namespace
{
const StringArray kOsChoices { "1x", "2x", "4x", "8x" };
} // namespace

AudioProcessorValueTreeState::ParameterLayout PedalAudioProcessor::createParameterLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    // Pot controls: 0..1, taper applied in DSP (architecture.md). Rename/add to suit the pedal.
    layout.add(std::make_unique<AudioParameterFloat>("gain",   "Gain",   NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    layout.add(std::make_unique<AudioParameterFloat>("tone",   "Tone",   NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    layout.add(std::make_unique<AudioParameterFloat>("volume", "Volume", NormalisableRange<float>(0.0f, 1.0f), 0.5f));

    // 3-position mode switch (precomputed topologies in a real build).
    layout.add(std::make_unique<AudioParameterChoice>("mode", "Mode", StringArray { "I", "II", "III" }, 1));

    // Trims, in dB, distinct from the pedal controls.
    layout.add(std::make_unique<AudioParameterFloat>("input_trim",  "Input Trim",  NormalisableRange<float>(-12.0f, 12.0f), 0.0f));
    layout.add(std::make_unique<AudioParameterFloat>("output_trim", "Output Trim", NormalisableRange<float>(-12.0f, 12.0f), 0.0f));
    layout.add(std::make_unique<AudioParameterBool>("trim_link", "Trim Link", false));

    // Oversampling: realtime + offline-render factors.
    layout.add(std::make_unique<AudioParameterChoice>("oversampling",        "Oversampling",        kOsChoices, 2)); // 4x
    layout.add(std::make_unique<AudioParameterChoice>("render_oversampling", "Render Oversampling", kOsChoices, 3)); // 8x

    // Optional quality toggle (dsp.md "HQ / Eco mode"). Remove if your pedal has no such lever.
    layout.add(std::make_unique<AudioParameterBool>("hq", "HQ", true));

    layout.add(std::make_unique<AudioParameterBool>("bypass", "Bypass", false));

    return layout;
}

PedalAudioProcessor::PedalAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", AudioChannelSet::stereo(), true)
                         .withOutput("Output", AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
}

void PedalAudioProcessor::prepareToPlay(double, int)
{
    for (auto& l : inputLevel)  l.store(0.0f);
    for (auto& l : outputLevel) l.store(0.0f);
}

bool PedalAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void PedalAudioProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    const int numCh  = buffer.getNumChannels();
    const int numSmp = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, numSmp);

    const bool bypassed = apvts.getRawParameterValue("bypass")->load() > 0.5f;
    const float inTrim  = Decibels::decibelsToGain(apvts.getRawParameterValue("input_trim")->load());
    const float outTrim = Decibels::decibelsToGain(apvts.getRawParameterValue("output_trim")->load());

    for (int ch = 0; ch < jmin(numCh, 2); ++ch)
    {
        auto* d = buffer.getWritePointer(ch);
        float inPeak = 0.0f, outPeak = 0.0f;

        for (int n = 0; n < numSmp; ++n)
        {
            const float x = d[n] * inTrim;
            inPeak = jmax(inPeak, std::abs(x));

            // PLACEHOLDER: pass-through. Replace with the WDF chain (input -> clip -> tone -> ...).
            const float y = bypassed ? d[n] : x * outTrim;

            d[n] = y;
            outPeak = jmax(outPeak, std::abs(y));
        }

        inputLevel[(size_t) ch].store(inPeak);
        outputLevel[(size_t) ch].store(outPeak);
    }
}

AudioProcessorEditor* PedalAudioProcessor::createEditor()
{
    return new PedalAudioProcessorEditor(*this);
}

void PedalAudioProcessor::getStateInformation(MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void PedalAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        apvts.replaceState(ValueTree::fromXml(*xml));
}

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PedalAudioProcessor();
}
