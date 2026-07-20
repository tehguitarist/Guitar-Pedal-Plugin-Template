#include "PedalFace.h"

#include <cmath>
#include <functional>

using namespace juce;

namespace
{
// Per-knob tooltip formatter: turns the raw 0..1 parameter value into the control's real-world
// unit, to two decimal places (ui.md "Tooltips"). Swap these for your pedal — e.g. a tone control
// might read out its cutoff in Hz (run the value through the same taper the DSP uses), a drive as a
// 0-10 dial number, a mix as a percentage. Placeholders here: Gain/Volume as a 0-10 dial, Tone as %.
String fmtDial(double v01)    { return String(v01 * 10.0, 2); }            // "0.00" .. "10.00"
String fmtPercent(double v01) { return String(v01 * 100.0, 2) + " %"; }    // "0.00 %" .. "100.00 %"
} // namespace

PedalFace::PedalFace(AudioProcessorValueTreeState& apvts) : state(apvts)
{
    // ---- Pot knobs -----------------------------------------------------------------------------
    auto setupKnob = [this](Slider& s, Label& lab, const String& text, const char* paramId,
                            std::function<String(double)> fmt) {
        s.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
        s.setRotaryParameters(MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true); // 270°, gap at bottom
        s.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);  // value shown as a tooltip, not JUCE's text box
        addAndMakeVisible(s);
        sliderAttachments.push_back(std::make_unique<SliderParameterAttachment>(*state.getParameter(paramId), s));
        // Update the drag tooltip to the real-world value (2 dp). Needs a TooltipWindow somewhere
        // in the hierarchy — the editor owns one. (Alternatively use s.setPopupDisplayEnabled(...).)
        s.onValueChange = [&s, fmt] { s.setTooltip(fmt(s.getValue())); };
        s.setTooltip(fmt(s.getValue()));

        lab.setText(text, dontSendNotification);
        lab.setJustificationType(Justification::centred);
        lab.setColour(Label::textColourId, Colour(PedalLookAndFeel::cLabelText));
        addAndMakeVisible(lab);
    };
    setupKnob(gainKnob,   gainLabel,   "GAIN",   "gain",   fmtDial);
    setupKnob(toneKnob,   toneLabel,   "TONE",   "tone",   fmtPercent);
    setupKnob(volumeKnob, volumeLabel, "VOLUME", "volume", fmtDial);

    // ---- 3-position mode switch, bound to the "mode" AudioParameterChoice -----------------------
    // Two-way binding: the ParameterAttachment drives the switch when the host/automation changes
    // the param; the switch's onChange writes back as a complete gesture.
    modeSwitch.setLabels("I", "II", "III");   // placeholder positions — rename per pedal
    addAndMakeVisible(modeSwitch);
    modeAttachment = std::make_unique<ParameterAttachment>(
        *state.getParameter("mode"),
        [this](float v) { modeSwitch.setPosition((int) std::lround(v)); });
    modeSwitch.onChange = [this](int pos) { modeAttachment->setValueAsCompleteGesture((float) pos); };
    modeAttachment->sendInitialUpdate();

    // ---- Status LED + bypass footswitch --------------------------------------------------------
    addAndMakeVisible(led);

    bypassSwitch.setComponentID("bypass");       // -> PedalLookAndFeel footswitch art / vector
    bypassSwitch.setClickingTogglesState(true);
    addAndMakeVisible(bypassSwitch);
    bypassAttachment = std::make_unique<ButtonParameterAttachment>(*state.getParameter("bypass"), bypassSwitch);

    bypassLabel.setText("BYPASS", dontSendNotification);
    bypassLabel.setJustificationType(Justification::centred);
    bypassLabel.setColour(Label::textColourId, Colour(PedalLookAndFeel::cBypassLabel));
    addAndMakeVisible(bypassLabel);

    // ---- Logo ----------------------------------------------------------------------------------
    logoLabel.setText("<PEDAL NAME>", dontSendNotification);   // <-- rename per pedal
    logoLabel.setJustificationType(Justification::centred);
    logoLabel.setColour(Label::textColourId, Colour(PedalLookAndFeel::cLabelText).withAlpha(0.85f));
    addAndMakeVisible(logoLabel);

    updateLED();
}

void PedalFace::paint(Graphics& g)
{
    // Delegate to the LookAndFeel: texture art if embedded (Assets.h), else the procedural
    // mottled background. Keeps the face's look in one place and consistent with the palette.
    if (auto* plf = dynamic_cast<PedalLookAndFeel*>(&getLookAndFeel()))
        plf->paintPedalBackground(g, getLocalBounds());
    else
        g.fillAll(Colour(PedalLookAndFeel::cPedalFace));
}

void PedalFace::resized()
{
    const float W = (float) getWidth(), H = (float) getHeight();
    const float knobD = jmin(W * 0.20f, H * 0.28f);
    const float labH  = jmax(10.0f, H * 0.06f);

    auto place = [](Component& c, float cx, float cy, float w, float h) {
        c.setBounds(roundToInt(cx - w * 0.5f), roundToInt(cy - h * 0.5f), roundToInt(w), roundToInt(h));
    };
    auto placeLabelUnder = [&](Label& l, float cx, float cyKnob, float knob) {
        place(l, cx, cyKnob + knob * 0.72f, knob * 1.8f, labH);
    };

    // Logo across the top.
    place(logoLabel, W * 0.5f, H * 0.12f, W * 0.9f, jmax(14.0f, H * 0.12f));

    // Knob row: GAIN / TONE / VOLUME.
    const float knobY = H * 0.42f;
    const float gx = W * 0.25f, tx = W * 0.5f, vx = W * 0.75f;
    place(gainKnob,   gx, knobY, knobD, knobD);
    place(toneKnob,   tx, knobY, knobD, knobD);
    place(volumeKnob, vx, knobY, knobD, knobD);
    placeLabelUnder(gainLabel,   gx, knobY, knobD);
    placeLabelUnder(toneLabel,   tx, knobY, knobD);
    placeLabelUnder(volumeLabel, vx, knobY, knobD);

    // Bottom row: mode switch (left), LED (centre), footswitch (right).
    const float bottomY = H * 0.76f;
    place(modeSwitch, W * 0.20f, bottomY, W * 0.16f, H * 0.30f);

    const float ledD = jmin(W, H) * 0.07f;
    place(led, W * 0.5f, bottomY - ledD * 0.4f, ledD, ledD);

    const float fsD = jmin(W * 0.20f, H * 0.32f);
    const float fsX = W * 0.78f, fsY = bottomY;
    place(bypassSwitch, fsX, fsY, fsD, fsD);
    place(bypassLabel,  fsX, fsY + fsD * 0.62f, fsD * 1.7f, labH);
}

void PedalFace::refresh(float sc)
{
    scale = sc;
    auto bold = [](float sz) { return Font(FontOptions(jmax(8.0f, sz), Font::bold)); };
    for (auto* l : { &gainLabel, &toneLabel, &volumeLabel })
        l->setFont(bold(11.0f * sc).withExtraKerningFactor(0.10f));
    bypassLabel.setFont(bold(8.0f * sc).withExtraKerningFactor(0.20f));
    logoLabel.setFont(bold(18.0f * sc).withExtraKerningFactor(0.15f));
}

void PedalFace::updateLED()
{
    // LED on = active (not bypassed). Read the param directly so it reflects host recall
    // immediately, before any audio has run (ui.md "Metering & threading").
    const auto* p = state.getRawParameterValue("bypass");
    led.setOn(p == nullptr || p->load() < 0.5f);
}
