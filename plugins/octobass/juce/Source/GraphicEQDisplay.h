#pragma once

#include "LCDSpectrumDisplay.h"

#include <octobass-core/GraphicEQ.hpp>

#include <array>
#include <cmath>
#include <functional>

class GraphicEQDisplay : public juce::Component
{
 public:
  static constexpr int kNumEQBands = octob::kGraphicEQNumBands;
  static constexpr float kMinGainDb = -12.0f;
  static constexpr float kMaxGainDb = 12.0f;

  GraphicEQDisplay()
  {
    eqGainsDb_.fill(0.0f);
    addAndMakeVisible(spectrumDisplay_);
    spectrumDisplay_.setInterceptsMouseClicks(false, false);
  }

  void setTypeface(juce::Typeface::Ptr tf)
  {
    typeface_ = tf;
    spectrumDisplay_.setTypeface(tf);
  }

  void setBandLevels(const float* levelsDb, int count)
  {
    spectrumDisplay_.setBandLevels(levelsDb, count);
  }

  void setCrossoverNormPosition(float normPos)
  {
    spectrumDisplay_.setCrossoverNormPosition(normPos);
  }

  void setSampleRate(double sr) { sampleRate_ = sr; }

  void setEQBandGain(int band, float gainDb)
  {
    if (band < 0 || band >= kNumEQBands)
      return;

    gainDb = juce::jlimit(kMinGainDb, kMaxGainDb, gainDb);

    if (std::fabs(eqGainsDb_[static_cast<size_t>(band)] - gainDb) > 0.001f)
    {
      eqGainsDb_[static_cast<size_t>(band)] = gainDb;
      repaint();
    }
  }

  float getEQBandGain(int band) const
  {
    if (band < 0 || band >= kNumEQBands)
      return 0.0f;
    return eqGainsDb_[static_cast<size_t>(band)];
  }

  std::function<void(int band, float gainDb)> onBandGainChanged;

  void resized() override { spectrumDisplay_.setBounds(getLocalBounds()); }

  void paint(juce::Graphics&) override {}

  void paintOverChildren(juce::Graphics& g) override
  {
    auto barArea = getBarArea();
    float areaTop = static_cast<float>(barArea.getY());
    float areaH = static_cast<float>(barArea.getHeight());
    float areaLeft = static_cast<float>(barArea.getX());
    float areaW = static_cast<float>(barArea.getWidth());

    // Highlight the hovered EQ band region
    if (hoverBand_ >= 0)
    {
      float leftX = eqBandLeftEdge(hoverBand_, areaLeft, areaW);
      float rightX = eqBandRightEdge(hoverBand_, areaLeft, areaW);
      g.setColour(juce::Colour(0xff1c1c30).withAlpha(0.08f));
      g.fillRect(leftX, areaTop, rightX - leftX, areaH);
    }

    // Center line at 0dB gain
    float centerY = gainToY(0.0f, areaTop, areaH);
    g.setColour(juce::Colour(0xff1c1c30).withAlpha(0.3f));
    g.drawHorizontalLine(static_cast<int>(centerY), areaLeft, areaLeft + areaW);

    // Check if any band is active
    bool anyActive = false;
    for (int i = 0; i < kNumEQBands; ++i)
    {
      if (std::fabs(eqGainsDb_[static_cast<size_t>(i)]) >= 0.05f)
      {
        anyActive = true;
        break;
      }
    }

    // Draw actual magnitude response curve
    juce::Path eqPath;
    constexpr int kCurvePoints = 200;

    for (int p = 0; p <= kCurvePoints; ++p)
    {
      float t = static_cast<float>(p) / static_cast<float>(kCurvePoints);
      float x = areaLeft + t * areaW;

      float responseDb = 0.0f;
      if (anyActive)
      {
        float freq = normXToFreq(t);
        responseDb = octob::GraphicEQ::computeMagnitudeResponseDb(eqGainsDb_.data(), freq,
                                                                  sampleRate_);
        responseDb = juce::jlimit(kMinGainDb, kMaxGainDb, responseDb);
      }

      float y = gainToY(responseDb, areaTop, areaH);

      if (p == 0)
        eqPath.startNewSubPath(x, y);
      else
        eqPath.lineTo(x, y);
    }

    g.setColour(juce::Colour(0xff1c1c30));
    g.strokePath(eqPath, juce::PathStrokeType(2.0f));

    // Draw points only on bands with non-zero gain
    for (int i = 0; i < kNumEQBands; ++i)
    {
      if (std::fabs(eqGainsDb_[static_cast<size_t>(i)]) < 0.05f)
        continue;

      float normX = eqBandNormX(i);
      float x = areaLeft + normX * areaW;

      float freq = octob::GraphicEQ::kCenterFreqs[static_cast<size_t>(i)];
      float responseDb = octob::GraphicEQ::computeMagnitudeResponseDb(eqGainsDb_.data(), freq,
                                                                      sampleRate_);
      responseDb = juce::jlimit(kMinGainDb, kMaxGainDb, responseDb);
      float y = gainToY(responseDb, areaTop, areaH);

      float radius = (i == dragBand_) ? 5.0f : 4.0f;
      g.setColour(juce::Colour(0xff1c1c30));
      g.fillEllipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f);
    }

    // dB tooltip for hovered or dragged band
    int tooltipBand = (dragBand_ >= 0) ? dragBand_ : hoverBand_;
    if (tooltipBand >= 0)
    {
      float gain = eqGainsDb_[static_cast<size_t>(tooltipBand)];
      juce::String text = ((gain >= 0.0f) ? "+" : "") + juce::String(gain, 1) + " dB";

      constexpr float kTooltipFontH = 9.0f;
      constexpr float kTooltipPadX = 4.0f;
      constexpr float kTooltipPadY = 2.0f;
      constexpr float kTooltipCorner = 3.0f;

      juce::Font tooltipFont =
          typeface_ != nullptr
              ? juce::Font(juce::FontOptions().withTypeface(typeface_).withHeight(kTooltipFontH))
              : juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),
                                             kTooltipFontH, juce::Font::plain));
      g.setFont(tooltipFont);

      juce::GlyphArrangement glyphs;
      glyphs.addLineOfText(tooltipFont, text, 0.0f, 0.0f);
      float textW = glyphs.getBoundingBox(0, glyphs.getNumGlyphs(), true).getWidth();
      float boxW = textW + kTooltipPadX * 2.0f;
      float boxH = kTooltipFontH + kTooltipPadY * 2.0f;

      float bandNorm = eqBandNormX(tooltipBand);
      float ptX = areaLeft + bandNorm * areaW;
      float freq = octob::GraphicEQ::kCenterFreqs[static_cast<size_t>(tooltipBand)];
      float ptResponseDb = anyActive
                               ? octob::GraphicEQ::computeMagnitudeResponseDb(
                                     eqGainsDb_.data(), freq, sampleRate_)
                               : 0.0f;
      ptResponseDb = juce::jlimit(kMinGainDb, kMaxGainDb, ptResponseDb);
      float ptY = gainToY(ptResponseDb, areaTop, areaH);

      float boxX = ptX + 6.0f;
      float boxY = ptY - boxH - 4.0f;

      // Flip to left side if near right edge
      if (boxX + boxW > areaLeft + areaW)
        boxX = ptX - boxW - 6.0f;
      // Flip below if near top edge
      if (boxY < areaTop)
        boxY = ptY + 6.0f;

      auto boxRect = juce::Rectangle<float>(boxX, boxY, boxW, boxH);
      g.setColour(juce::Colour(0xff1c1c30));
      g.fillRoundedRectangle(boxRect, kTooltipCorner);

      g.setColour(juce::Colour(0xffF08830));
      g.drawText(text, boxRect.toNearestInt(), juce::Justification::centred, false);
    }
  }

  void mouseMove(const juce::MouseEvent& e) override
  {
    int band = nearestBandAtX(e.position.x);
    if (band != hoverBand_)
    {
      hoverBand_ = band;
      setMouseCursor(hoverBand_ >= 0 ? juce::MouseCursor::UpDownResizeCursor
                                     : juce::MouseCursor::NormalCursor);
      repaint();
    }
  }

  void mouseDown(const juce::MouseEvent& e) override
  {
    dragBand_ = nearestBandAtX(e.position.x);
    if (dragBand_ >= 0)
    {
      dragStartGain_ = eqGainsDb_[static_cast<size_t>(dragBand_)];
      dragStartY_ = e.position.y;
    }
  }

  void mouseDrag(const juce::MouseEvent& e) override
  {
    if (dragBand_ < 0)
      return;

    auto barArea = getBarArea();
    float barAreaHeight = static_cast<float>(barArea.getHeight());

    float gainRange = kMaxGainDb - kMinGainDb;
    float deltaY = dragStartY_ - e.position.y;
    float deltaGain = (deltaY / barAreaHeight) * gainRange;

    float newGain = juce::jlimit(kMinGainDb, kMaxGainDb, dragStartGain_ + deltaGain);

    eqGainsDb_[static_cast<size_t>(dragBand_)] = newGain;
    if (onBandGainChanged)
      onBandGainChanged(dragBand_, newGain);
    repaint();
  }

  void mouseUp(const juce::MouseEvent&) override { dragBand_ = -1; }

  void mouseDoubleClick(const juce::MouseEvent& e) override
  {
    int band = nearestBandAtX(e.position.x);
    if (band >= 0)
    {
      eqGainsDb_[static_cast<size_t>(band)] = 0.0f;
      if (onBandGainChanged)
        onBandGainChanged(band, 0.0f);
      repaint();
    }
  }

  void mouseExit(const juce::MouseEvent&) override
  {
    if (hoverBand_ >= 0)
    {
      hoverBand_ = -1;
      setMouseCursor(juce::MouseCursor::NormalCursor);
      repaint();
    }
  }

  LCDSpectrumDisplay& getSpectrumDisplay() { return spectrumDisplay_; }

 private:
  LCDSpectrumDisplay spectrumDisplay_;
  std::array<float, kNumEQBands> eqGainsDb_{};
  double sampleRate_ = 44100.0;
  juce::Typeface::Ptr typeface_;

  int dragBand_ = -1;
  float dragStartGain_ = 0.0f;
  float dragStartY_ = 0.0f;
  int hoverBand_ = -1;

  juce::Rectangle<int> getBarArea() const
  {
    auto content = getLocalBounds().reduced(LCDSpectrumDisplay::kPad);
    content.removeFromLeft(LCDSpectrumDisplay::kYAxisW);
    content.removeFromTop(LCDSpectrumDisplay::kTopPad);
    content.removeFromBottom(LCDSpectrumDisplay::kXAxisH);
    return content;
  }

  float gainToY(float gainDb, float barAreaTop, float barAreaHeight) const
  {
    float normalized = (gainDb - kMinGainDb) / (kMaxGainDb - kMinGainDb);
    return barAreaTop + barAreaHeight * (1.0f - normalized);
  }

  // Normalized [0,1] x position for an EQ band on the spectrum display
  static float eqBandNormX(int band)
  {
    return LCDSpectrumDisplay::freqToNormX(
        octob::GraphicEQ::kCenterFreqs[static_cast<size_t>(band)]);
  }

  // Left edge of an EQ band's highlight region (midpoint to previous band)
  float eqBandLeftEdge(int band, float areaLeft, float areaW) const
  {
    if (band == 0)
      return areaLeft;
    float mid = (eqBandNormX(band - 1) + eqBandNormX(band)) * 0.5f;
    return areaLeft + mid * areaW;
  }

  // Right edge of an EQ band's highlight region (midpoint to next band)
  float eqBandRightEdge(int band, float areaLeft, float areaW) const
  {
    if (band == kNumEQBands - 1)
      return areaLeft + areaW;
    float mid = (eqBandNormX(band) + eqBandNormX(band + 1)) * 0.5f;
    return areaLeft + mid * areaW;
  }

  // Invert the spectrum display's frequency mapping for the magnitude curve
  static float normXToFreq(float normX)
  {
    constexpr float kLog50 = 5.643856f;
    constexpr float kLog6300 = 12.621488f;
    constexpr int kDisplayBands = 24;

    float bandPos = normX * static_cast<float>(kDisplayBands) - 0.5f;
    float t = (bandPos - 1.0f) / 21.0f;
    return std::pow(2.0f, kLog50 + t * (kLog6300 - kLog50));
  }

  // Find the nearest EQ band to a given x pixel position
  int nearestBandAtX(float x) const
  {
    auto barArea = getBarArea();
    float areaLeft = static_cast<float>(barArea.getX());
    float areaW = static_cast<float>(barArea.getWidth());
    float normX = (x - areaLeft) / areaW;

    if (normX < -0.02f || normX > 1.02f)
      return -1;

    int nearest = 0;
    float minDist = std::fabs(normX - eqBandNormX(0));
    for (int i = 1; i < kNumEQBands; ++i)
    {
      float dist = std::fabs(normX - eqBandNormX(i));
      if (dist < minDist)
      {
        minDist = dist;
        nearest = i;
      }
    }
    return nearest;
  }

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GraphicEQDisplay)
};
