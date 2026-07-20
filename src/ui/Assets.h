#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "BinaryData.h"

/**
 * Cached accessors for the embedded UI art (assets/ui/, processed from the ui/ originals via
 * tools/process_ui_assets.sh and embedded by juce_add_binary_data(PedalAssets ...) in
 * CMakeLists.txt). juce::ImageCache deduplicates repeated calls, so these are cheap from paint().
 *
 * This is the GENERIC single-channel placeholder set. To reskin the pedal:
 *   1. drop your art into ui/ (see ui/ui-replacements.md for the rules — noon-centred knobs,
 *      alpha, rotation-safe, don't stretch),
 *   2. keep the output names below (knob.png, trim_knob.png, led_on/off.png, footswitch_up/down.png,
 *      switch_up/mid/down.png, texture.jpg) OR rename in all three places
 *      (process_ui_assets.sh, the juce_add_binary_data list, and the accessors here),
 *   3. re-run tools/process_ui_assets.sh and rebuild.
 *
 * Every consumer draws the image only `if (img.isValid())` and otherwise falls back to a
 * procedural (vector) render in PedalLookAndFeel — so the plugin still draws sensibly if an asset
 * is ever missing, and the vector look remains available as a zero-asset alternative.
 *
 * NOTE: any translation unit that includes this header must link the PedalAssets binary-data
 * target (the plugin does; UI test/snapshot exes must add it too — see CMakeLists.txt).
 */
namespace PedalAssets
{
inline juce::Image knob()           { return juce::ImageCache::getFromMemory (BinaryData::knob_png,            BinaryData::knob_pngSize); }
inline juce::Image trimKnob()       { return juce::ImageCache::getFromMemory (BinaryData::trim_knob_png,       BinaryData::trim_knob_pngSize); }
inline juce::Image ledOn()          { return juce::ImageCache::getFromMemory (BinaryData::led_on_png,          BinaryData::led_on_pngSize); }
inline juce::Image ledOff()         { return juce::ImageCache::getFromMemory (BinaryData::led_off_png,         BinaryData::led_off_pngSize); }
inline juce::Image footswitchUp()   { return juce::ImageCache::getFromMemory (BinaryData::footswitch_up_png,   BinaryData::footswitch_up_pngSize); }
inline juce::Image footswitchDown() { return juce::ImageCache::getFromMemory (BinaryData::footswitch_down_png, BinaryData::footswitch_down_pngSize); }
inline juce::Image switchUp()       { return juce::ImageCache::getFromMemory (BinaryData::switch_up_png,       BinaryData::switch_up_pngSize); }
inline juce::Image switchMid()      { return juce::ImageCache::getFromMemory (BinaryData::switch_mid_png,      BinaryData::switch_mid_pngSize); }
inline juce::Image switchDown()     { return juce::ImageCache::getFromMemory (BinaryData::switch_down_png,     BinaryData::switch_down_pngSize); }
inline juce::Image texture()        { return juce::ImageCache::getFromMemory (BinaryData::texture_jpg,         BinaryData::texture_jpgSize); }

/** Per-pixel brightness/contrast grade (straight-alpha maths; alpha untouched). `contrast` and
 *  `brightness` are multipliers around 1.0 (e.g. 1.1 = +10% contrast, 0.9 = -10% brightness).
 *  Use to tune source art to the pedal-face palette without re-exporting the PNGs.
 */
inline juce::Image adjustBrightnessContrast (const juce::Image& src, float contrast, float brightness)
{
    if (! src.isValid())
        return src;

    juce::Image result (juce::Image::ARGB, src.getWidth(), src.getHeight(), true);
    const juce::Image::BitmapData srcData (src, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData dstData (result, juce::Image::BitmapData::writeOnly);

    const auto grade = [contrast, brightness] (float c) {
        c = (c - 0.5f) * contrast + 0.5f;
        return juce::jlimit (0.0f, 1.0f, c * brightness);
    };

    for (int y = 0; y < src.getHeight(); ++y)
        for (int x = 0; x < src.getWidth(); ++x)
        {
            const auto p = srcData.getPixelColour (x, y);
            dstData.setPixelColour (x, y,
                juce::Colour::fromFloatRGBA (grade (p.getFloatRed()), grade (p.getFloatGreen()),
                                             grade (p.getFloatBlue()), p.getFloatAlpha()));
        }
    return result;
}

// Graded variants (computed once, cached). Tune the multipliers per pedal; 1.0/1.0 = untouched.
inline const juce::Image& knobGraded()
{
    static const juce::Image img = adjustBrightnessContrast (knob(), 1.0f, 1.0f);
    return img;
}
inline const juce::Image& textureGraded()
{
    static const juce::Image img = adjustBrightnessContrast (texture(), 1.0f, 1.0f);
    return img;
}
} // namespace PedalAssets
