#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Shared LCD palette: every component drawing on the LCD references these so
// the ink and backlight colours cannot drift apart
inline const juce::Colour kLCDInkColour{0xff1c1c30};
inline const juce::Colour kLCDBacklightColour{0xffF08830};
// Backlight of a switched-off slot: same hue, darker and desaturated, keeping
// kLCDInkColour at 5.0:1 contrast (WCAG 2.1 SC 1.4.3 requires 4.5:1)
inline const juce::Colour kLCDDimBacklightColour{0xffB4825C};

inline void drawLCDBackground(juce::Graphics& g, juce::Rectangle<float> bounds,
                              juce::Colour backlight = kLCDBacklightColour)
{
  g.setColour(backlight);
  g.fillRoundedRectangle(bounds, 3.0f);

  g.setColour(juce::Colour(0xff1a1a1a));
  g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

  auto inner = bounds.reduced(1.0f);
  juce::ColourGradient topShadow(juce::Colour(0xff000000).withAlpha(0.22f), inner.getX(),
                                 inner.getY(), juce::Colour(0xff000000).withAlpha(0.0f),
                                 inner.getX(), inner.getY() + 5.0f, false);
  g.setGradientFill(topShadow);
  g.fillRoundedRectangle(inner, 2.0f);

  juce::ColourGradient leftShadow(juce::Colour(0xff000000).withAlpha(0.12f), inner.getX(),
                                  inner.getY(), juce::Colour(0xff000000).withAlpha(0.0f),
                                  inner.getX() + 5.0f, inner.getY(), false);
  g.setGradientFill(leftShadow);
  g.fillRoundedRectangle(inner, 2.0f);
}
