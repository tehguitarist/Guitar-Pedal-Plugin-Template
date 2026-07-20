#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/PedalFace.h"
#include "ui/PedalLookAndFeel.h"
#include "ui/VUMeter.h"

/**
 * Reusable editor shell: the three-column layout from ui.md — INPUT side panel (left), the
 * pedal-specific PedalFace (centre), OUTPUT side panel (right) — with a full-width oversampling /
 * UI-scale strip beneath. Only PedalFace changes per pedal; everything here is shared.
 *
 * Expects this interface on the processor (see architecture.md / docs/ui-peripheral-spec.md):
 *   - `juce::AudioProcessorValueTreeState apvts` with params: input_trim, output_trim,
 *     oversampling, render_oversampling, bypass, gain, tone, volume, mode, and OPTIONALLY
 *     hq + trim_link (guarded below — the strip drops the toggle if the param is absent),
 *   - `float getInputLevel(int channel)` / `float getOutputLevel(int channel)` (post-/pre-trim
 *     peak, DAW domain), backed by std::atomic<float> written on the audio thread.
 *
 * Rename `PedalAudioProcessor` to your processor class if it differs.
 */
class PedalAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PedalAudioProcessorEditor(PedalAudioProcessor&);
    ~PedalAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;         // ~33 fps: VU ballistics + LED
    void refreshFonts(float sc);           // re-set every label font (fonts must scale with the window)
    void showScaleMenu();
    void saveDefaultScale();               // cross-session default via ApplicationProperties

    // Base (1x) window size. The pedal face is the centre; side panels + OS strip are fixed-width
    // in px, so all extra window width goes to the face. Tune to suit your face's aspect.
    static constexpr int kBaseW = 560;
    static constexpr int kBaseH = 360;

    PedalAudioProcessor& audioProcessor;
    PedalLookAndFeel     lnf;
    juce::TooltipWindow  tooltipWindow { this };   // enables the knob/trim value tooltips
    juce::ApplicationProperties appProps;
    float currentScale { 1.0f };

    // Debounced cross-session scale save: restarted on every resized(), fires ~500 ms after the
    // last one so a corner-drag coalesces into a single disk write (ui-peripheral-spec.md).
    struct DebouncedSave : juce::Timer {
        std::function<void()> action;
        void timerCallback() override { if (action) action(); stopTimer(); }
    } scaleSaveDebounce;

    // ---- Side panels: Input (left) / Output (right) --------------------------------------------
    juce::Label  inputSectionLabel, outputSectionLabel;
    juce::Slider inputTrim, outputTrim;
    juce::Label  inputTrimSub, outputTrimSub;      // "TRIM"
    juce::Label  inputTrimValue, outputTrimValue;  // always-visible dB readout, e.g. "+3.00 dB"
    VUMeter      inputVU, outputVU;
    std::unique_ptr<juce::SliderParameterAttachment> inputTrimAttach, outputTrimAttach;
    float vuInDecay { 0.0f }, vuOutDecay { 0.0f };

    // ---- Oversampling / scale strip ------------------------------------------------------------
    juce::Label      osLabel, osLiveLabel, osRenderLabel, osSizeLabel, versionLabel;
    juce::ComboBox   osRealtimeBox, osRenderBox;
    juce::TextButton hqButton, trimLinkButton;     // optional toggles (componentID "os")
    juce::TextButton scaleBtn;                      // componentID "os-selector"
    std::unique_ptr<juce::ComboBoxParameterAttachment> osRealtimeAttach, osRenderAttach;
    std::unique_ptr<juce::ButtonParameterAttachment>   hqAttach, trimLinkAttach;

    std::unique_ptr<PedalFace> pedalFace;
    juce::Rectangle<int> pedalFaceArea;
    juce::Rectangle<int> osStripArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PedalAudioProcessorEditor)
};
