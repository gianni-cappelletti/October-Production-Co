#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cmath>

#include "LCDPainting.h"

class LCDSpectrumDisplay : public juce::Component
{
 public:
  static constexpr int kNumBands = 24;
  static constexpr float kMinDb = -80.0f;
  static constexpr float kMaxDb = 0.0f;

  LCDSpectrumDisplay() { bandLevelsDb_.fill(kMinDb); }

  void setTypeface(juce::Typeface::Ptr tf)
  {
    typeface_ = tf;
    repaint();
  }

  void setBandLevels(const float* levelsDb, int count)
  {
    int n = std::min(count, kNumBands);
    for (int i = 0; i < n; ++i)
      bandLevelsDb_[static_cast<size_t>(i)] = levelsDb[i];
    repaint();
  }

  void setCrossoverNormPosition(float normPos)
  {
    crossoverNormPos_ = normPos;
    repaint();
  }

  // Maps a frequency to a normalized 0-1 position across the bar area.
  // Band 0 (<50Hz) maps to the left, bands 1-22 (50Hz-6.3kHz) log-spaced, band 23 (>6.3kHz) right.
  static float freqToNormX(float freqHz)
  {
    float displayBandPos;
    if (freqHz <= 50.0f)
      displayBandPos = 0.0f;
    else if (freqHz >= 6300.0f)
      displayBandPos = static_cast<float>(kNumBands - 1);
    else
    {
      constexpr float kLog50 = 5.643856f;     // log2(50)
      constexpr float kLog6300 = 12.621488f;  // log2(6300)
      float t = (std::log2(freqHz) - kLog50) / (kLog6300 - kLog50);
      displayBandPos = 1.0f + t * 21.0f;
    }
    return (displayBandPos + 0.5f) / static_cast<float>(kNumBands);
  }

  void paint(juce::Graphics& g) override
  {
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    drawLCDBackground(g, bounds);

    auto content = getLocalBounds().reduced(kPad);
    content.removeFromLeft(kYAxisW);
    content.removeFromTop(kTopPad);
    auto xAxisArea = content.removeFromBottom(kXAxisH);
    auto barArea = content;

    juce::Font labelFont =
        typeface_ != nullptr
            ? juce::Font(juce::FontOptions().withTypeface(typeface_).withHeight(kLabelFontH))
            : juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), kLabelFontH,
                                           juce::Font::plain));

    float barAreaTop = static_cast<float>(barArea.getY());
    float barAreaHeight = static_cast<float>(barArea.getHeight());
    float barAreaLeft = static_cast<float>(barArea.getX());
    float barAreaWidth = static_cast<float>(barArea.getWidth());

    // Grid lines (no labels)
    for (int db : kYAxisDbValues)
    {
      float normY = static_cast<float>(db - kMinDb) / static_cast<float>(kMaxDb - kMinDb);
      float y = barAreaTop + barAreaHeight * (1.0f - normY);

      g.setColour(juce::Colour(0xff1c1c30).withAlpha(0.12f));
      g.drawHorizontalLine(static_cast<int>(y), barAreaLeft, barAreaLeft + barAreaWidth);
    }

    // Border lines around the graph area
    g.setColour(juce::Colour(0xff1c1c30));
    g.drawVerticalLine(static_cast<int>(barAreaLeft), barAreaTop, barAreaTop + barAreaHeight);
    g.drawVerticalLine(static_cast<int>(barAreaLeft + barAreaWidth), barAreaTop,
                       barAreaTop + barAreaHeight);
    g.drawHorizontalLine(static_cast<int>(barAreaTop), barAreaLeft, barAreaLeft + barAreaWidth);
    g.drawHorizontalLine(static_cast<int>(barAreaTop + barAreaHeight), barAreaLeft,
                         barAreaLeft + barAreaWidth);

    // Bar dimensions
    float barSlotWidth = barAreaWidth / static_cast<float>(kNumBands);
    float barWidth = barSlotWidth - static_cast<float>(kBarGap);

    // Draw bars (faint backdrop for EQ overlay)
    g.setColour(juce::Colour(0xff1c1c30).withAlpha(0.25f));
    for (int i = 0; i < kNumBands; ++i)
    {
      float normLevel = (bandLevelsDb_[static_cast<size_t>(i)] - kMinDb) / (kMaxDb - kMinDb);
      normLevel = juce::jlimit(0.0f, 1.0f, normLevel);

      float barH = barAreaHeight * normLevel;
      float barX = barAreaLeft + static_cast<float>(i) * barSlotWidth;
      float barY = barAreaTop + barAreaHeight - barH;

      if (barH > 0.5f)
        g.fillRect(barX, barY, barWidth, barH);
    }

    // X-axis frequency labels
    g.setFont(labelFont);
    g.setColour(juce::Colour(0xff1c1c30));
    for (const auto& label : kXAxisLabels)
    {
      float centerX =
          barAreaLeft + static_cast<float>(label.bandIndex) * barSlotWidth + barWidth / 2.0f;
      auto labelRect = juce::Rectangle<float>(centerX - 15.0f, static_cast<float>(xAxisArea.getY()),
                                              30.0f, static_cast<float>(xAxisArea.getHeight()));
      g.drawText(label.text, labelRect.toNearestInt(), juce::Justification::centred, false);
    }

    // Crossover frequency marker (full LCD height)
    float crossoverX = barAreaLeft + crossoverNormPos_ * barAreaWidth;
    float markerTop = static_cast<float>(getLocalBounds().getY() + kPad);
    float markerHeight = static_cast<float>(getLocalBounds().getHeight() - 2 * kPad);
    g.setColour(juce::Colour(0xff1c1c30));
    g.fillRect(crossoverX - kMarkerW / 2.0f, markerTop, kMarkerW, markerHeight);

    // Scan lines for LCD authenticity
    auto scanBounds = getLocalBounds();
    g.setColour(juce::Colour(0xff000000).withAlpha(0.025f));
    for (int sy = scanBounds.getY(); sy < scanBounds.getBottom(); sy += 2)
      g.drawHorizontalLine(sy, static_cast<float>(scanBounds.getX()),
                           static_cast<float>(scanBounds.getRight()));
  }

  std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
  {
    return std::make_unique<juce::AccessibilityHandler>(*this, juce::AccessibilityRole::group);
  }

  struct XAxisLabel
  {
    int bandIndex;
    const char* text;
  };

  static constexpr int kPad = 6;
  static constexpr int kTopPad = 6;
  static constexpr int kYAxisW = 4;
  static constexpr int kXAxisH = 14;
  static constexpr int kBarGap = 2;
  static constexpr float kLabelFontH = 7.0f;
  static constexpr float kMarkerW = 3.0f;

 private:
  static constexpr std::array<int, 5> kYAxisDbValues = {{0, -20, -40, -60, -80}};

  static constexpr std::array<XAxisLabel, 7> kXAxisLabels = {
      {XAxisLabel{1, "50"}, XAxisLabel{4, "100"}, XAxisLabel{8, "250"}, XAxisLabel{11, "500"},
       XAxisLabel{14, "1k"}, XAxisLabel{17, "2k"}, XAxisLabel{21, "5k"}}};

  std::array<float, kNumBands> bandLevelsDb_{};
  float crossoverNormPos_ = 0.0f;
  juce::Typeface::Ptr typeface_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LCDSpectrumDisplay)
};
