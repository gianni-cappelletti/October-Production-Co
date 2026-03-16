#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class LCDDisplay : public juce::Component
{
 public:
  LCDDisplay() { setOpaque(false); }

  void setText(const juce::String& text)
  {
    text_ = text;
    repaint();
  }

  void setTextColour(juce::Colour colour)
  {
    textColour_ = colour;
    repaint();
  }

  void setTypeface(juce::Typeface::Ptr typeface)
  {
    typeface_ = typeface;
    repaint();
  }

  void paint(juce::Graphics& g) override
  {
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);

    g.setColour(juce::Colour(0xffF08830));
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

    if (typeface_ != nullptr)
      g.setFont(juce::Font(juce::FontOptions().withTypeface(typeface_).withHeight(8.0f)));
    else
      g.setFont(juce::Font(
          juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 8.0f, juce::Font::plain)));

    g.setColour(textColour_.withAlpha(isEnabled() ? 1.0f : 0.4f));
    g.drawFittedText(text_, getLocalBounds().reduced(6, 4), juce::Justification::centredLeft, 1,
                     0.9f);

    auto scanBounds = getLocalBounds();
    g.setColour(juce::Colour(0xff000000).withAlpha(0.025f));
    for (int sy = scanBounds.getY(); sy < scanBounds.getBottom(); sy += 2)
      g.drawHorizontalLine(sy, (float)scanBounds.getX(), (float)scanBounds.getRight());
  }

 private:
  juce::String text_ = "No IR loaded";
  juce::Colour textColour_ = juce::Colour(0xff1c1c30);
  juce::Typeface::Ptr typeface_;
};
