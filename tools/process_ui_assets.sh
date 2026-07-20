#!/bin/bash
# Resizes/compresses the user-supplied UI art in ui/ into assets/ui/ for BinaryData embedding.
# Originals in ui/ are untouched. Re-run after editing any source art, then rebuild so
# juce_add_binary_data(PedalAssets ...) re-embeds them (see CMakeLists.txt).
#
# This template ships a *generic single-channel* placeholder set. Replace the source images in
# ui/ with your pedal's art (keep the same output names below, or edit both this script and the
# juce_add_binary_data() list + Assets.h accessors to match). See ui/ui-replacements.md for the
# art guidelines (noon-centred knobs, alpha, rotation-safe, 2x resolution, don't stretch).
#
# Requires: ImageMagick (magick), pngquant, optipng. On macOS: `brew install imagemagick pngquant optipng`.
set -euo pipefail
cd "$(dirname "$0")/.."

SRC=ui
DST=assets/ui
mkdir -p "$DST"

# Resize a PNG (preserving alpha) to fit size×size, strip metadata, then lossless-ish compress.
resize_png() {
    local src="$1" dst="$2" size="$3"
    magick "$SRC/$src" -resize "${size}x${size}" -strip "$DST/$dst"
    pngquant --force --skip-if-larger --quality=80-100 --speed 1 -o "$DST/$dst" "$DST/$dst" || true
    optipng -quiet -o2 "$DST/$dst"
}

# --- Knobs (square, alpha, drawn at "noon" — rotated in code by the LookAndFeel) ---------------
resize_png "bakelite_knob.png" knob.png       256   # <PEDAL> main pot knob (Gain / Tone / Volume)
resize_png "vol_trim.png"      trim_knob.png   256   # Input/Output peripheral halo trim cap

# --- Status LED (glow baked into the art) ------------------------------------------------------
resize_png "red_led_on.png"    led_on.png      128   # active (not bypassed)
resize_png "red_led_off.png"   led_off.png     128   # bypassed

# --- Bypass footswitch (up = released, down = momentary press animation) ------------------------
resize_png "Footswitch_up.png"   footswitch_up.png    200
resize_png "footswitch_down.png" footswitch_down.png  200

# --- 3-position mode switch (one image per discrete position) ----------------------------------
resize_png "switch_up.png"   switch_up.png    128
resize_png "switch_Mid.png"  switch_mid.png   128
resize_png "switch_down.png" switch_down.png  128

# --- Background texture — no alpha; JPEG (smooth gradient compresses very well lossily) ---------
magick "$SRC/lr_v2_texture.png" -resize 1536x -strip -quality 88 "$DST/texture.jpg"

echo "--- processed sizes ---"
ls -la "$DST"
