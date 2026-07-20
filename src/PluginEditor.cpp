#include "PluginEditor.h"

#include <cmath>

// JUCE defines this from project(<Pedal> VERSION ...) in a plugin build; the fallback keeps
// non-plugin TUs (UI snapshot exes) compiling. "v" JucePlugin_VersionString is plain adjacent
// string-literal concatenation — no runtime String building.
#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "dev"
#endif

using namespace juce;

namespace
{
const StringArray kOsChoices { "1x", "2x", "4x", "8x" };

constexpr float kScales[] = { 0.50f, 0.75f, 1.00f, 1.25f, 1.50f, 1.75f, 2.00f, 2.25f, 2.50f };
constexpr const char* kScaleLabels[] = { "50%", "75%", "100%", "125%", "150%",
                                         "175%", "200%", "225%", "250%" };

// Trim readout / tooltip: always signed, two decimals, dB unit — "+3.00 dB", "-12.00 dB", "+0.00 dB".
String fmtTrim(double db) { return (db >= 0.0 ? "+" : "") + String(db, 2) + " dB"; }
} // namespace

PedalAudioProcessorEditor::PedalAudioProcessorEditor(PedalAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&lnf);

    // Cross-session default scale via ApplicationProperties; per-session via APVTS state.
    PropertiesFile::Options opts;
    opts.applicationName     = "<Pedal>";          // <-- your product name
    opts.filenameSuffix      = ".settings";
    opts.folderName          = "<You>";            // <-- your company name
    opts.osxLibrarySubFolder = "Application Support";
    appProps.setStorageParameters(opts);

    if (const auto v = audioProcessor.apvts.state.getProperty("uiScale"); ! v.isVoid())
        currentScale = (float) (double) v;
    else
        currentScale = (float) appProps.getUserSettings()->getDoubleValue("defaultScale", 1.0);
    currentScale = jlimit(0.5f, 2.5f, currentScale);

    // ---- Side panels ---------------------------------------------------------------------------
    auto setupSectionLabel = [this](Label& l, const String& text) {
        l.setText(text, dontSendNotification);
        l.setJustificationType(Justification::centred);
        l.setColour(Label::textColourId, Colour(PedalLookAndFeel::cTrimLabel));
        addAndMakeVisible(l);
    };
    setupSectionLabel(inputSectionLabel, "INPUT");
    setupSectionLabel(outputSectionLabel, "OUTPUT");

    auto setupTrimSub = [this](Label& l) {
        l.setText("TRIM", dontSendNotification);
        l.setJustificationType(Justification::centred);
        l.setColour(Label::textColourId, Colour(PedalLookAndFeel::cTrimLabel).darker(0.25f));
        addAndMakeVisible(l);
    };
    setupTrimSub(inputTrimSub);
    setupTrimSub(outputTrimSub);

    auto setupTrimValue = [this](Label& l) {
        l.setJustificationType(Justification::centred);
        l.setColour(Label::textColourId, Colour(PedalLookAndFeel::cTrimLabel));
        addAndMakeVisible(l);
    };
    setupTrimValue(inputTrimValue);
    setupTrimValue(outputTrimValue);

    const float pi = MathConstants<float>::pi;
    auto setupTrim = [this, pi](Slider& s) {
        s.setComponentID("trim");                                    // -> halo style in the LookAndFeel
        s.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
        s.setRotaryParameters(pi * 1.25f, pi * 2.75f, true);         // 270° sweep, gap at bottom
        s.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
        s.setDoubleClickReturnValue(true, 0.0);                      // double-click -> 0 dB
        addAndMakeVisible(s);
    };
    setupTrim(inputTrim);
    setupTrim(outputTrim);
    inputTrimAttach  = std::make_unique<SliderParameterAttachment>(*audioProcessor.apvts.getParameter("input_trim"), inputTrim);
    outputTrimAttach = std::make_unique<SliderParameterAttachment>(*audioProcessor.apvts.getParameter("output_trim"), outputTrim);

    // Value label + drag tooltip, both to two decimals with sign, updated together (ui.md Tooltips).
    auto bindTrimReadout = [](Slider& s, Label& valueLabel) {
        auto update = [&s, &valueLabel] {
            const auto txt = fmtTrim(s.getValue());
            s.setTooltip(txt);
            valueLabel.setText(txt, dontSendNotification);
        };
        update();
        s.onValueChange = update;
    };
    bindTrimReadout(inputTrim, inputTrimValue);
    bindTrimReadout(outputTrim, outputTrimValue);

    addAndMakeVisible(inputVU);
    addAndMakeVisible(outputVU);

    // ---- Oversampling / scale strip ------------------------------------------------------------
    auto setupOSLabel = [this](Label& l, const String& text, Justification j) {
        l.setText(text, dontSendNotification);
        l.setJustificationType(j);
        l.setColour(Label::textColourId, Colour(PedalLookAndFeel::cOSLabel));
        addAndMakeVisible(l);
    };
    setupOSLabel(osLabel, "OS", Justification::centredLeft);
    setupOSLabel(osLiveLabel, "LIVE", Justification::centredRight);
    setupOSLabel(osRenderLabel, "RENDER", Justification::centredRight);
    setupOSLabel(osSizeLabel, "UI SIZE", Justification::centredRight);

    // Self-updating version stamp — from JucePlugin_VersionString (= CMake project VERSION). Muted,
    // non-interactive; given the leftover strip space in resized().
    versionLabel.setText("v" JucePlugin_VersionString, dontSendNotification);
    versionLabel.setJustificationType(Justification::centred);
    versionLabel.setColour(Label::textColourId, Colour(PedalLookAndFeel::cOSLabel).withAlpha(0.55f));
    versionLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(versionLabel);

    auto setupOSBox = [this](ComboBox& box) {
        box.addItemList(kOsChoices, 1);
        box.setJustificationType(Justification::centred);
        box.setColour(ComboBox::textColourId, Colour(PedalLookAndFeel::cOSBtnActive));
        addAndMakeVisible(box);
    };
    setupOSBox(osRealtimeBox);
    setupOSBox(osRenderBox);
    osRealtimeAttach = std::make_unique<ComboBoxParameterAttachment>(*audioProcessor.apvts.getParameter("oversampling"), osRealtimeBox);
    osRenderAttach   = std::make_unique<ComboBoxParameterAttachment>(*audioProcessor.apvts.getParameter("render_oversampling"), osRenderBox);

    // Optional quality/behaviour toggles — only shown if the pedal actually declares the param
    // (HQ: dsp.md "HQ / Eco mode"; Trim Link: architecture.md "Input/output trim link", whose
    // mirror logic lives in the PROCESSOR — this button only flips the trim_link bool).
    auto setupToggle = [this](TextButton& b, const String& text, const char* paramId,
                              std::unique_ptr<ButtonParameterAttachment>& attach, const String& tip) {
        if (auto* param = audioProcessor.apvts.getParameter(paramId))
        {
            b.setComponentID("os");            // lit-on / dim-off segmented style
            b.setClickingTogglesState(true);
            b.setButtonText(text);
            b.setTooltip(tip);
            addAndMakeVisible(b);
            attach = std::make_unique<ButtonParameterAttachment>(*param, b);
        }
        // else: never made visible — resized() skips any button that isn't visible.
    };
    setupToggle(hqButton, "HQ", "hq", hqAttach,
                "High-quality diode solve (accurate omega). Off = faster, small added distortion floor.");
    setupToggle(trimLinkButton, "TRIM LINK", "trim_link", trimLinkAttach,
                "Link trims: raising input trim lowers output trim by the same dB, so pushing the "
                "circuit harder doesn't change overall loudness.");

    scaleBtn.setComponentID("os-selector");    // styled like the OS combo boxes (spec parity)
    scaleBtn.onClick = [this] { showScaleMenu(); };
    addAndMakeVisible(scaleBtn);

    pedalFace = std::make_unique<PedalFace>(audioProcessor.apvts);
    addAndMakeVisible(*pedalFace);

    setResizable(true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio((double) kBaseW / (double) kBaseH);
        c->setSizeLimits(roundToInt(kBaseW * 0.5f), roundToInt(kBaseH * 0.5f),
                         roundToInt(kBaseW * 2.5f), roundToInt(kBaseH * 2.5f));
    }
    setSize(roundToInt(kBaseW * currentScale), roundToInt(kBaseH * currentScale));

    scaleSaveDebounce.action = [this] { saveDefaultScale(); };
    startTimerHz(33);
}

PedalAudioProcessorEditor::~PedalAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void PedalAudioProcessorEditor::paint(Graphics& g)
{
    g.fillAll(Colour(PedalLookAndFeel::cBackground));

    // (PedalFace paints its own body over pedalFaceArea.)

    // Oversampling strip background.
    g.setColour(Colour(PedalLookAndFeel::cOSBackground));
    g.fillRoundedRectangle(osStripArea.toFloat(), 6.0f);
    g.setColour(Colour(PedalLookAndFeel::cOSBorder));
    g.drawRoundedRectangle(osStripArea.toFloat().reduced(0.5f), 6.0f, 1.0f);
}

void PedalAudioProcessorEditor::refreshFonts(float sc)
{
    auto bold = [](float sz) { return Font(FontOptions(sz, Font::bold)); };
    inputSectionLabel.setFont(bold(8.0f * sc).withExtraKerningFactor(0.20f));
    outputSectionLabel.setFont(bold(8.0f * sc).withExtraKerningFactor(0.20f));
    inputTrimSub.setFont(bold(7.5f * sc).withExtraKerningFactor(0.15f));
    outputTrimSub.setFont(bold(7.5f * sc).withExtraKerningFactor(0.15f));
    inputTrimValue.setFont(bold(8.5f * sc));
    outputTrimValue.setFont(bold(8.5f * sc));
    osLabel.setFont(bold(8.0f * sc));
    osLiveLabel.setFont(bold(7.0f * sc).withExtraKerningFactor(0.10f));
    osRenderLabel.setFont(bold(7.0f * sc).withExtraKerningFactor(0.10f));
    osSizeLabel.setFont(bold(7.0f * sc).withExtraKerningFactor(0.10f));
    versionLabel.setFont(Font(FontOptions(7.0f * sc, Font::plain)).withExtraKerningFactor(0.10f));
}

void PedalAudioProcessorEditor::resized()
{
    currentScale = (float) getWidth() / (float) kBaseW;
    const float sc = currentScale;
    const auto i = [sc](int v) { return roundToInt((float) v * sc); };
    refreshFonts(sc);

    const int W = getWidth(), H = getHeight();
    const int margin = i(10);
    const int panelW = i(74);
    const int osH    = i(24);
    const int faceGap = i(10);
    const int colGap  = i(8);

    const int topY = margin;
    const int topH = H - margin - osH - faceGap - margin;

    osStripArea = Rectangle<int>(margin, H - margin - osH, W - 2 * margin, osH);

    const Rectangle<int> inPanel(margin, topY, panelW, topH);
    const Rectangle<int> outPanel(W - margin - panelW, topY, panelW, topH);
    pedalFaceArea = Rectangle<int>(margin + panelW + colGap, topY,
                                   W - 2 * (margin + panelW + colGap), topH);
    if (pedalFace != nullptr)
    {
        pedalFace->setBounds(pedalFaceArea);
        pedalFace->refresh(sc);
    }

    // Each element is laid out against its own panel column Rectangle (never the full editor) so
    // the halo knob + VU can't spill past the column edge at any scale (ui.md Layout contract).
    auto layoutPanel = [&](Rectangle<int> panel, Label& sec, Slider& knob, Label& sub, Label& val, VUMeter& vu) {
        auto r = panel;
        sec.setBounds(r.removeFromTop(i(14)));
        r.removeFromTop(i(2));
        const int knobD = jmin(i(70), r.getWidth());
        auto knobRow = r.removeFromTop(knobD);
        knob.setBounds(knobRow.withSizeKeepingCentre(knobD, knobD));
        sub.setBounds(r.removeFromTop(i(12)));
        val.setBounds(r.removeFromTop(i(12)));
        r.removeFromTop(i(2));
        const int vuW = jmin(i(34), r.getWidth());
        vu.setBounds(r.withSizeKeepingCentre(vuW, r.getHeight()));
    };
    layoutPanel(inPanel, inputSectionLabel, inputTrim, inputTrimSub, inputTrimValue, inputVU);
    layoutPanel(outPanel, outputSectionLabel, outputTrim, outputTrimSub, outputTrimValue, outputVU);

    // ---- Oversampling strip content (inset from the bg) ----------------------------------------
    auto os = osStripArea.reduced(i(6), 0);
    const int boxVPad = i(2);

    // Left group: OS | LIVE [box] | RENDER [box] | HQ? | TRIM LINK?
    osLabel.setBounds(os.removeFromLeft(i(20)));
    os.removeFromLeft(i(8));
    osLiveLabel.setBounds(os.removeFromLeft(i(26)));
    os.removeFromLeft(i(5));
    osRealtimeBox.setBounds(os.removeFromLeft(i(36)).reduced(0, boxVPad));
    os.removeFromLeft(i(12));
    osRenderLabel.setBounds(os.removeFromLeft(i(40)));
    os.removeFromLeft(i(5));
    osRenderBox.setBounds(os.removeFromLeft(i(36)).reduced(0, boxVPad));
    if (hqButton.isVisible())
    {
        os.removeFromLeft(i(10));
        hqButton.setBounds(os.removeFromLeft(i(26)).reduced(0, boxVPad));
    }
    if (trimLinkButton.isVisible())
    {
        os.removeFromLeft(i(8));
        trimLinkButton.setBounds(os.removeFromLeft(i(62)).reduced(0, boxVPad));
    }

    // Right group: UI SIZE [scale] — laid out from the right.
    scaleBtn.setBounds(os.removeFromRight(i(48)).reduced(0, boxVPad));
    os.removeFromRight(i(5));
    osSizeLabel.setBounds(os.removeFromRight(i(42)));

    // Version fills whatever is left between the two groups (centred).
    versionLabel.setBounds(os);

    scaleBtn.setButtonText(String(roundToInt(currentScale * 100.0f)) + "%");

    // Persistence — both driven from resized() so a corner-drag is remembered like a menu pick:
    audioProcessor.apvts.state.setProperty("uiScale", (double) currentScale, nullptr); // per-session
    scaleSaveDebounce.startTimer(500);                                                  // cross-session (debounced)
}

void PedalAudioProcessorEditor::timerCallback()
{
    constexpr float kNoiseFl = 5.0e-4f; // -66 dBFS silence floor

    float in = jmax(audioProcessor.getInputLevel(0), vuInDecay * 0.90f);
    if (in < kNoiseFl) in = 0.0f;
    vuInDecay = in;
    inputVU.setLevel(in);

    float out = jmax(audioProcessor.getOutputLevel(0), vuOutDecay * 0.90f);
    if (out < kNoiseFl) out = 0.0f;
    vuOutDecay = out;
    outputVU.setLevel(out);

    if (pedalFace != nullptr)
        pedalFace->updateLED();
}

void PedalAudioProcessorEditor::showScaleMenu()
{
    PopupMenu menu;
    for (int n = 0; n < 9; ++n)
        menu.addItem(n + 1, kScaleLabels[n], true, std::abs(currentScale - kScales[n]) < 0.01f);
    menu.addSeparator();
    menu.addItem(100, "Set current scale as default");

    menu.showMenuAsync(PopupMenu::Options().withTargetComponent(&scaleBtn), [this](int r) {
        if (r >= 1 && r <= 9)
            setSize(roundToInt(kBaseW * kScales[r - 1]), roundToInt(kBaseH * kScales[r - 1]));
        else if (r == 100)
            saveDefaultScale();   // immediate (no debounce) for the explicit menu action
    });
}

void PedalAudioProcessorEditor::saveDefaultScale()
{
    appProps.getUserSettings()->setValue("defaultScale", (double) currentScale);
}
