#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class OctobIRLookAndFeel : public juce::LookAndFeel_V4
{
 public:
  OctobIRLookAndFeel();

  void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                        float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

  void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                            bool shouldDrawButtonAsHighlighted,
                            bool shouldDrawButtonAsDown) override;

  void drawButtonText(juce::Graphics&, juce::TextButton&, bool shouldDrawButtonAsHighlighted,
                      bool shouldDrawButtonAsDown) override;

  void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

  void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown, int buttonX,
                    int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

  void drawLabel(juce::Graphics&, juce::Label&) override;

  juce::Typeface::Ptr getTypefaceForFont(const juce::Font&) override;
  juce::Font getLabelFont(juce::Label&) override;
  juce::Typeface::Ptr getMainTypeface() const { return cutiveMonoTypeface_; }
  juce::Typeface::Ptr getLCDTypeface() const { return lcdTypeface_; }

 private:
  juce::Typeface::Ptr cutiveMonoTypeface_;
  juce::Typeface::Ptr lcdTypeface_;
};
