#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

inline void drawPanelScrew(juce::Graphics& g, float cx, float cy)
{
  {
    juce::ColourGradient grad(juce::Colour(0xffABA9A9), cx, cy - 6.5f, juce::Colour(0xff8F8F8F), cx,
                              cy + 6.5f, false);
    g.setGradientFill(grad);
    g.fillEllipse(cx - 6.5f, cy - 6.5f, 13.0f, 13.0f);
  }
  {
    juce::ColourGradient grad(juce::Colour(0xffF5F5F5), cx, cy - 5.81f, juce::Colour(0xffC2C2C2),
                              cx, cy + 5.81f, false);
    g.setGradientFill(grad);
    g.fillEllipse(cx - 5.81f, cy - 5.81f, 11.62f, 11.62f);
  }
  {
    juce::ColourGradient grad(juce::Colour(0xffEBEBEB), cx, cy - 5.36f, juce::Colour(0xffCCCCCC),
                              cx, cy + 5.36f, false);
    g.setGradientFill(grad);
    g.fillEllipse(cx - 5.36f, cy - 5.36f, 10.72f, 10.72f);
  }

  const float armLen = 7.09f;
  const float armW = 1.88f;
  g.setColour(juce::Colour(0xff8C8C8C));
  g.fillRect(cx - armW * 0.5f, cy - armLen * 0.5f, armW, armLen);
  g.fillRect(cx - armLen * 0.5f, cy - armW * 0.5f, armLen, armW);
}
