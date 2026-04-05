#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

#include "LCDPainting.h"

class LCDDisplay : public juce::Component
{
 public:
  LCDDisplay()
  {
    setOpaque(false);
    setTitle(text_);
  }

  void setText(const juce::String& text)
  {
    text_ = text;
    setTitle(text);
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

  void setOnClick(std::function<void()> callback) { onClick_ = std::move(callback); }

  void mouseUp(const juce::MouseEvent& event) override
  {
    if (onClick_ && event.mouseWasClicked())
      onClick_();
  }

  void paint(juce::Graphics& g) override
  {
    drawLCDBackground(g, getLocalBounds().toFloat().reduced(1.0f));

    if (typeface_ != nullptr)
      g.setFont(juce::Font(juce::FontOptions().withTypeface(typeface_).withHeight(9.0f)));
    else
      g.setFont(juce::Font(
          juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain)));

    g.setColour(textColour_.withAlpha(isEnabled() ? 1.0f : 0.4f));
    g.drawFittedText(text_, getLocalBounds().reduced(6, 4), juce::Justification::centredLeft, 1,
                     0.9f);

    auto scanBounds = getLocalBounds();
    g.setColour(juce::Colour(0xff000000).withAlpha(0.025f));
    for (int sy = scanBounds.getY(); sy < scanBounds.getBottom(); sy += 2)
      g.drawHorizontalLine(sy, (float)scanBounds.getX(), (float)scanBounds.getRight());
  }

  std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
  {
    return std::make_unique<juce::AccessibilityHandler>(*this, juce::AccessibilityRole::staticText);
  }

 private:
  juce::String text_ = "No IR loaded";
  juce::Colour textColour_ = juce::Colour(0xff1c1c30);
  juce::Typeface::Ptr typeface_;
  std::function<void()> onClick_;
};
