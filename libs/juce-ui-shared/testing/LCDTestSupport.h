#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

#include "LCDDisplay.h"

namespace lcd_test
{

// WCAG 2.1 relative luminance (https://www.w3.org/TR/WCAG21/#dfn-relative-luminance)
inline double relativeLuminance(juce::Colour colour)
{
  const auto linear = [](juce::uint8 channel)
  {
    const double c = channel / 255.0;
    return c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
  };
  return 0.2126 * linear(colour.getRed()) + 0.7152 * linear(colour.getGreen()) +
         0.0722 * linear(colour.getBlue());
}

// WCAG 2.1 contrast ratio (https://www.w3.org/TR/WCAG21/#dfn-contrast-ratio)
inline double contrastRatio(juce::Colour a, juce::Colour b)
{
  const double la = relativeLuminance(a);
  const double lb = relativeLuminance(b);
  return (juce::jmax(la, lb) + 0.05) / (juce::jmin(la, lb) + 0.05);
}

// Backlight colour as rendered on screen, sampled right of the left-justified
// text and clear of the inset shadows along the top and left edges
inline juce::Colour renderedBacklight(LCDDisplay& display)
{
  const auto snapshot = display.createComponentSnapshot(display.getLocalBounds(), true, 1.0f);
  return snapshot.getPixelAt(snapshot.getWidth() - 4, snapshot.getHeight() / 2);
}

// A switched-off display reads as unlit: clearly darker and less saturated
// than the lit backlight
inline bool looksDimmed(juce::Colour backlight)
{
  return relativeLuminance(backlight) < 0.8 * relativeLuminance(kLCDBacklightColour) &&
         backlight.getSaturation() < kLCDBacklightColour.getSaturation();
}

inline bool looksLit(juce::Colour backlight)
{
  return relativeLuminance(backlight) >= 0.9 * relativeLuminance(kLCDBacklightColour);
}

}  // namespace lcd_test
