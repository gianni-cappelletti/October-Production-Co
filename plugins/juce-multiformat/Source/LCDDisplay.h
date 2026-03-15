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

    g.setColour(juce::Colour(0xff0a0a0a));
    g.fillRoundedRectangle(bounds, 3.0f);

    g.setColour(juce::Colours::black.withAlpha(0.9f));
    g.drawRoundedRectangle(bounds, 3.0f, 1.5f);

    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawRoundedRectangle(bounds.reduced(1.5f), 2.0f, 1.0f);

    // Phosphor glow pass: draw text slightly larger at low alpha before the primary pass
    auto glowFont = typeface_ != nullptr
                        ? juce::Font(juce::FontOptions().withTypeface(typeface_).withHeight(12.5f))
                        : juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),
                                                       12.5f, juce::Font::plain));
    g.setFont(glowFont);
    g.setColour(textColour_.withAlpha((isEnabled() ? 1.0f : 0.4f) * 0.15f));
    g.drawFittedText(text_, getLocalBounds().reduced(6, 4), juce::Justification::centredLeft, 1,
                     0.9f);

    if (typeface_ != nullptr)
      g.setFont(juce::Font(juce::FontOptions().withTypeface(typeface_).withHeight(12.0f)));
    else
      g.setFont(juce::Font(
          juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain)));

    g.setColour(textColour_.withAlpha(isEnabled() ? 1.0f : 0.4f));
    g.drawFittedText(text_, getLocalBounds().reduced(6, 4), juce::Justification::centredLeft, 1,
                     0.9f);

    // Scanline texture
    auto scanBounds = getLocalBounds();
    g.setColour(juce::Colour(0xff000000).withAlpha(0.08f));
    for (int sy = scanBounds.getY(); sy < scanBounds.getBottom(); sy += 2)
      g.drawHorizontalLine(sy, (float)scanBounds.getX(), (float)scanBounds.getRight());
  }

 private:
  juce::String text_ = "No IR loaded";
  juce::Colour textColour_ = juce::Colour(0xff00ff00);
  juce::Typeface::Ptr typeface_;
};
