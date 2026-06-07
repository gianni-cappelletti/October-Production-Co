#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cmath>

#include "LCDPainting.h"

class LCDSpectrumDisplay : public juce::Component
{
 public:
  static constexpr int kNumBands = 24;
  static constexpr float kDefaultMinDb = -80.0f;
  static constexpr float kDefaultMaxDb = 0.0f;

  LCDSpectrumDisplay() { bandLevelsDb_.fill(minDb_); }

  // Level axis range; levels outside it clamp to the bar area edges
  void setDbRange(float minDb, float maxDb)
  {
    if (minDb >= maxDb)
    {
      DBG("Ignoring invalid dB range: min " + juce::String(minDb) + " >= max " +
          juce::String(maxDb));
      return;
    }
    minDb_ = minDb;
    maxDb_ = maxDb;
    repaint();
  }

  float getMinDb() const { return minDb_; }
  float getMaxDb() const { return maxDb_; }

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
    if (juce::exactlyEqual(crossoverNormPos_, normPos))
      return;
    crossoverNormPos_ = normPos;
    repaint();
  }

  // Frequency axis range; must match the analyzer's log-spaced band edges and
  // the EQ node mapping so the spectrum, labels, and EQ curve all read true
  static constexpr float kMinFreqHz = 20.0f;
  static constexpr float kMaxFreqHz = 20000.0f;

  // Maps a frequency to its normalized 0-1 position on the log axis
  static float freqToNormX(float freqHz)
  {
    float logMin = std::log2(kMinFreqHz);
    float logMax = std::log2(kMaxFreqHz);
    float t =
        (std::log2(juce::jlimit(kMinFreqHz, kMaxFreqHz, freqHz)) - logMin) / (logMax - logMin);
    return juce::jlimit(0.0f, 1.0f, t);
  }

  static float normXToFreq(float normX)
  {
    float logMin = std::log2(kMinFreqHz);
    float logMax = std::log2(kMaxFreqHz);
    return std::pow(2.0f, logMin + juce::jlimit(0.0f, 1.0f, normX) * (logMax - logMin));
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

    // Grid lines every kGridStepDb from the top of the range (no labels)
    g.setColour(kLCDInkColour.withAlpha(0.12f));
    for (float db = maxDb_; db >= minDb_; db -= kGridStepDb)
    {
      float normY = (db - minDb_) / (maxDb_ - minDb_);
      float y = barAreaTop + barAreaHeight * (1.0f - normY);
      g.drawHorizontalLine(static_cast<int>(y), barAreaLeft, barAreaLeft + barAreaWidth);
    }

    // Border lines around the graph area
    g.setColour(kLCDInkColour);
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
    g.setColour(kLCDInkColour.withAlpha(0.25f));
    for (int i = 0; i < kNumBands; ++i)
    {
      float normLevel = (bandLevelsDb_[static_cast<size_t>(i)] - minDb_) / (maxDb_ - minDb_);
      normLevel = juce::jlimit(0.0f, 1.0f, normLevel);

      float barH = barAreaHeight * normLevel;
      float barX = barAreaLeft + static_cast<float>(i) * barSlotWidth;
      float barY = barAreaTop + barAreaHeight - barH;

      if (barH > 0.5f)
        g.fillRect(barX, barY, barWidth, barH);
    }

    // X-axis frequency labels, positioned on the log axis so they read true
    g.setFont(labelFont);
    g.setColour(kLCDInkColour);
    for (const auto& label : kXAxisLabels)
    {
      float centerX = barAreaLeft + freqToNormX(label.freqHz) * barAreaWidth;
      auto labelRect = juce::Rectangle<float>(centerX - 15.0f, static_cast<float>(xAxisArea.getY()),
                                              30.0f, static_cast<float>(xAxisArea.getHeight()));
      g.drawText(label.text, labelRect.toNearestInt(), juce::Justification::centred, false);
    }

    // Crossover frequency marker (full LCD height)
    float crossoverX = barAreaLeft + crossoverNormPos_ * barAreaWidth;
    float markerTop = static_cast<float>(getLocalBounds().getY() + kPad);
    float markerHeight = static_cast<float>(getLocalBounds().getHeight() - 2 * kPad);
    g.setColour(kLCDInkColour);
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
    float freqHz;
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
  static constexpr float kGridStepDb = 20.0f;

  static constexpr std::array<XAxisLabel, 8> kXAxisLabels = {
      {XAxisLabel{50.0f, "50"}, XAxisLabel{100.0f, "100"}, XAxisLabel{250.0f, "250"},
       XAxisLabel{500.0f, "500"}, XAxisLabel{1000.0f, "1k"}, XAxisLabel{2000.0f, "2k"},
       XAxisLabel{5000.0f, "5k"}, XAxisLabel{10000.0f, "10k"}}};

  std::array<float, kNumBands> bandLevelsDb_{};
  float minDb_ = kDefaultMinDb;
  float maxDb_ = kDefaultMaxDb;
  float crossoverNormPos_ = 0.0f;
  juce::Typeface::Ptr typeface_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LCDSpectrumDisplay)
};
