// Headless UI snapshot: renders the real plugin editor to a PNG without a display, for visual
// verification of layout/colour/scale during UI development (build.md "UI changes"). Uses JUCE's
// software image renderer (Graphics over an Image) + paintEntireComponent, so it needs no window
// server, screen, or screen-recording permission — works in CI and over SSH.
//
//   cmake --build build --target UISnapshot && ./build/UISnapshot [scale] [out.png]
//
// Registered with add_test() as a finite/smoke check (renders at min + max scale, asserts a
// non-empty image); it does NOT gate on pixels. Verify layout containment (ui.md) at 0.5x and 2.5x.

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginEditor.h"
#include "PluginProcessor.h"

using namespace juce;

static bool renderAt(float scale, const File& out)
{
    PedalAudioProcessor proc;
    proc.prepareToPlay(48000.0, 512);

    std::unique_ptr<AudioProcessorEditor> editor(proc.createEditor());
    if (editor == nullptr)
        return false;

    // The editor derives its scale from width / kBaseW; drive it by sizing the window.
    editor->setSize(roundToInt(560 * scale), roundToInt(360 * scale)); // kBaseW/kBaseH

    Image img(Image::ARGB, editor->getWidth(), editor->getHeight(), true);
    {
        Graphics g(img);
        editor->paintEntireComponent(g, false);
    }
    if (img.getWidth() <= 0 || img.getHeight() <= 0)
        return false;

    out.deleteFile();
    if (auto os = out.createOutputStream())
    {
        PNGImageFormat png;
        png.writeImageToStream(img, *os);
        std::printf("wrote %s (%dx%d @ %.2fx)\n", out.getFullPathName().toRawUTF8(),
                    img.getWidth(), img.getHeight(), scale);
        return true;
    }
    return false;
}

int main(int argc, char* argv[])
{
    const ScopedJuceInitialiser_GUI guiInit;

    if (argc > 1)
    {
        const float scale = jlimit(0.5f, 2.5f, (float) String(argv[1]).getDoubleValue());
        const File out(argc > 2 ? String(argv[2]) : "pedal_ui.png");
        return renderAt(scale, out.getFullPathName().isNotEmpty() ? out : File::getCurrentWorkingDirectory().getChildFile("pedal_ui.png")) ? 0 : 1;
    }

    // No args (ctest): render the layout extremes and fail if either produces no image.
    const auto dir = File::getCurrentWorkingDirectory();
    const bool okMin = renderAt(0.5f, dir.getChildFile("pedal_ui_0.5x.png"));
    const bool okMax = renderAt(2.5f, dir.getChildFile("pedal_ui_2.5x.png"));
    return (okMin && okMax) ? 0 : 1;
}
