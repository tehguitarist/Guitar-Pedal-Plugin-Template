#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

/**
 * PLACEHOLDER processor — just enough to make the UI (PluginEditor / PedalFace) build, load in a
 * DAW, and pass audio through unchanged. It declares the canonical APVTS parameter set the UI binds
 * to and exposes the input/output peak meters the editor reads; it does NO circuit modelling.
 *
 * Replace the guts during the DSP build sequence (CLAUDE.md): drop in the per-channel WDF chain,
 * oversampling, tapers and calibration. Keep the parameter IDs stable so saved sessions and the UI
 * bindings don't break (architecture.md). The `hq` / `trim_link` params are optional — the editor
 * shows their toggles only when they exist — so remove them here if your pedal doesn't want them.
 */
class PedalAudioProcessor : public juce::AudioProcessor
{
public:
    PedalAudioProcessor();
    ~PedalAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "<Pedal>"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int sizeInBytes) override;

    // Peak meters read by the editor's timer (DAW domain, per channel). Written on the audio thread.
    float getInputLevel(int ch) const  { return inputLevel[(size_t) juce::jlimit(0, 1, ch)].load(); }
    float getOutputLevel(int ch) const { return outputLevel[(size_t) juce::jlimit(0, 1, ch)].load(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::array<std::atomic<float>, 2> inputLevel { 0.0f, 0.0f };
    std::array<std::atomic<float>, 2> outputLevel { 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PedalAudioProcessor)
};
