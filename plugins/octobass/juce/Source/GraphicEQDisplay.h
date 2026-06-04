#pragma once

#include <array>
#include <cmath>
#include <functional>
#include <octobass-core/GraphicEQ.hpp>

#include "LCDSpectrumDisplay.h"

class GraphicEQDisplay : public juce::Component
{
 public:
  static constexpr int kNumNodes = octob::kGraphicEQNumNodes;
  static constexpr float kMinGainDb = octob::MinGraphicEQGainDb;
  static constexpr float kMaxGainDb = octob::MaxGraphicEQGainDb;
  static constexpr float kMinFreqHz = octob::MinGraphicEQFreqHz;
  static constexpr float kMaxFreqHz = octob::MaxGraphicEQFreqHz;
  static constexpr float kHitRadiusPx = 8.0f;
  static constexpr float kGhostShowRadiusPx = 12.0f;

  // Drag/hover handles: peak nodes are 0..kNumNodes-1, cuts use the slots above
  static constexpr int kNoHandle = -1;
  static constexpr int kLowCutHandle = kNumNodes;
  static constexpr int kHighCutHandle = kNumNodes + 1;

  // Clicks within the leftmost/rightmost spectrum bar create the low/high cut
  static constexpr float kCutZoneNormWidth =
      1.0f / static_cast<float>(LCDSpectrumDisplay::kNumBands);

  GraphicEQDisplay()
  {
    nodeFreqHz_.fill(octob::DefaultGraphicEQFreqHz);
    nodeGainDb_.fill(0.0f);
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

  void setNode(int slot, bool active, float freqHz, float gainDb)
  {
    if (slot < 0 || slot >= kNumNodes)
      return;

    auto idx = static_cast<size_t>(slot);
    freqHz = juce::jlimit(kMinFreqHz, kMaxFreqHz, freqHz);
    gainDb = juce::jlimit(kMinGainDb, kMaxGainDb, gainDb);

    if (nodeActive_[idx] != active || std::fabs(nodeFreqHz_[idx] - freqHz) > 0.01f ||
        std::fabs(nodeGainDb_[idx] - gainDb) > 0.001f)
    {
      nodeActive_[idx] = active;
      nodeFreqHz_[idx] = freqHz;
      nodeGainDb_[idx] = gainDb;
      repaint();
    }
  }

  void setLowCut(bool active, float freqHz)
  {
    freqHz = juce::jlimit(kMinFreqHz, kMaxFreqHz, freqHz);
    if (lowCutActive_ != active || std::fabs(lowCutFreqHz_ - freqHz) > 0.01f)
    {
      lowCutActive_ = active;
      lowCutFreqHz_ = freqHz;
      repaint();
    }
  }

  void setHighCut(bool active, float freqHz)
  {
    freqHz = juce::jlimit(kMinFreqHz, kMaxFreqHz, freqHz);
    if (highCutActive_ != active || std::fabs(highCutFreqHz_ - freqHz) > 0.01f)
    {
      highCutActive_ = active;
      highCutFreqHz_ = freqHz;
      repaint();
    }
  }

  bool getNodeActive(int slot) const
  {
    return slot >= 0 && slot < kNumNodes && nodeActive_[static_cast<size_t>(slot)];
  }

  float getNodeFrequency(int slot) const
  {
    if (slot < 0 || slot >= kNumNodes)
      return octob::DefaultGraphicEQFreqHz;
    return nodeFreqHz_[static_cast<size_t>(slot)];
  }

  float getNodeGain(int slot) const
  {
    if (slot < 0 || slot >= kNumNodes)
      return 0.0f;
    return nodeGainDb_[static_cast<size_t>(slot)];
  }

  bool getLowCutActive() const { return lowCutActive_; }
  float getLowCutFrequency() const { return lowCutFreqHz_; }
  bool getHighCutActive() const { return highCutActive_; }
  float getHighCutFrequency() const { return highCutFreqHz_; }

  std::function<void(int slot, bool active, float freqHz, float gainDb)> onNodeChanged;
  std::function<void(bool active, float freqHz)> onLowCutChanged;
  std::function<void(bool active, float freqHz)> onHighCutChanged;

  // Log-spaced mapping between [kMinFreqHz, kMaxFreqHz] and normalized [0, 1].
  // Static and pure so coordinate logic is unit-testable without a Component.
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

  static bool isInLowCutZone(float normX) { return normX <= kCutZoneNormWidth; }
  static bool isInHighCutZone(float normX) { return normX >= 1.0f - kCutZoneNormWidth; }

  // Nearest active node within hit radius of (px, py), or -1. Pure for testability.
  static int nearestNodeIndex(const bool* active, const float* xs, const float* ys, int count,
                              float px, float py, float radiusPx)
  {
    int nearest = -1;
    float minDistSq = radiusPx * radiusPx;
    for (int i = 0; i < count; ++i)
    {
      if (!active[i])
        continue;

      float dx = xs[i] - px;
      float dy = ys[i] - py;
      float distSq = dx * dx + dy * dy;
      if (distSq <= minDistSq)
      {
        minDistSq = distSq;
        nearest = i;
      }
    }
    return nearest;
  }

  void resized() override { spectrumDisplay_.setBounds(getLocalBounds()); }

  void paint(juce::Graphics&) override {}

  void paintOverChildren(juce::Graphics& g) override
  {
    auto barArea = getBarArea();
    float areaTop = static_cast<float>(barArea.getY());
    float areaH = static_cast<float>(barArea.getHeight());
    float areaLeft = static_cast<float>(barArea.getX());
    float areaW = static_cast<float>(barArea.getWidth());

    // Center line at 0dB gain
    float centerY = gainToY(0.0f, areaTop, areaH);
    g.setColour(juce::Colour(0xff1c1c30).withAlpha(0.3f));
    g.drawHorizontalLine(static_cast<int>(centerY), areaLeft, areaLeft + areaW);

    bool anyActive = anyFilterActive();

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
        responseDb = juce::jlimit(kMinGainDb, kMaxGainDb, responseAtFreq(normXToFreq(t)));
      }

      float y = gainToY(responseDb, areaTop, areaH);

      if (p == 0)
        eqPath.startNewSubPath(x, y);
      else
        eqPath.lineTo(x, y);
    }

    g.setColour(juce::Colour(0xff1c1c30));
    g.strokePath(eqPath, juce::PathStrokeType(2.0f));

    // Ghost dot: hover hint on the curve where a node would be created
    if (ghostVisible_ && dragHandle_ == kNoHandle && hoverHandle_ == kNoHandle)
    {
      constexpr float kGhostRadius = 4.0f;
      g.setColour(juce::Colour(0xff1c1c30).withAlpha(0.35f));
      g.fillEllipse(ghostPos_.x - kGhostRadius, ghostPos_.y - kGhostRadius, kGhostRadius * 2.0f,
                    kGhostRadius * 2.0f);
    }

    // Draw a dot for each active node and cut handle
    for (int handle = 0; handle < kNumNodes + 2; ++handle)
    {
      if (!handleActive(handle))
        continue;

      auto pt = handlePixel(handle, barArea);
      float radius = (handle == dragHandle_ || handle == hoverHandle_) ? 5.0f : 4.0f;
      g.setColour(juce::Colour(0xff1c1c30));
      g.fillEllipse(pt.x - radius, pt.y - radius, radius * 2.0f, radius * 2.0f);
    }

    // Tooltip for the hovered or dragged handle
    int tooltipHandle = (dragHandle_ != kNoHandle) ? dragHandle_ : hoverHandle_;
    if (tooltipHandle != kNoHandle && handleActive(tooltipHandle))
    {
      juce::String text;
      if (tooltipHandle == kLowCutHandle)
        text = "LO CUT";
      else if (tooltipHandle == kHighCutHandle)
        text = "HI CUT";
      else
      {
        float gain = nodeGainDb_[static_cast<size_t>(tooltipHandle)];
        text = ((gain >= 0.0f) ? "+" : "") + juce::String(gain, 1) + " dB";
      }

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

      auto pt = handlePixel(tooltipHandle, barArea);
      float boxX = pt.x + 6.0f;
      float boxY = pt.y - boxH - 4.0f;

      // Flip to left side if near right edge
      if (boxX + boxW > areaLeft + areaW)
        boxX = pt.x - boxW - 6.0f;
      // Flip below if near top edge
      if (boxY < areaTop)
        boxY = pt.y + 6.0f;

      auto boxRect = juce::Rectangle<float>(boxX, boxY, boxW, boxH);
      g.setColour(juce::Colour(0xff1c1c30));
      g.fillRoundedRectangle(boxRect, kTooltipCorner);

      g.setColour(juce::Colour(0xffF08830));
      g.drawText(text, boxRect.toNearestInt(), juce::Justification::centred, false);
    }
  }

  void mouseMove(const juce::MouseEvent& e) override
  {
    int handle = handleAtPixel(e.position);
    bool changed = (handle != hoverHandle_);
    hoverHandle_ = handle;

    bool ghostWasVisible = ghostVisible_;
    updateGhost(e.position);
    changed = changed || ghostWasVisible != ghostVisible_ || ghostVisible_;

    setMouseCursor(hoverHandle_ != kNoHandle ? juce::MouseCursor::UpDownResizeCursor
                                             : juce::MouseCursor::NormalCursor);
    if (changed)
      repaint();
  }

  void mouseDown(const juce::MouseEvent& e) override
  {
    dragHandle_ = handleAtPixel(e.position);
    ghostVisible_ = false;

    if (dragHandle_ == kNoHandle)
    {
      auto barArea = getBarArea();
      float normX = (e.position.x - static_cast<float>(barArea.getX())) /
                    static_cast<float>(barArea.getWidth());
      float freqHz = normXToFreq(normX);

      if (isInLowCutZone(normX))
      {
        lowCutActive_ = true;
        lowCutFreqHz_ = freqHz;
        dragHandle_ = kLowCutHandle;
        if (onLowCutChanged)
          onLowCutChanged(true, freqHz);
        repaint();
      }
      else if (isInHighCutZone(normX))
      {
        highCutActive_ = true;
        highCutFreqHz_ = freqHz;
        dragHandle_ = kHighCutHandle;
        if (onHighCutChanged)
          onHighCutChanged(true, freqHz);
        repaint();
      }
      else
      {
        int slot = firstFreeSlot();
        if (slot < 0)
        {
          DBG("All " + juce::String(kNumNodes) + " EQ node slots in use, ignoring click");
          return;
        }

        float gainDb = yToGain(e.position.y, static_cast<float>(barArea.getY()),
                               static_cast<float>(barArea.getHeight()));

        auto idx = static_cast<size_t>(slot);
        nodeActive_[idx] = true;
        nodeFreqHz_[idx] = freqHz;
        nodeGainDb_[idx] = gainDb;
        dragHandle_ = slot;

        if (onNodeChanged)
          onNodeChanged(slot, true, freqHz, gainDb);
        repaint();
      }
    }

    dragStartPos_ = e.position;
    if (dragHandle_ >= 0 && dragHandle_ < kNumNodes)
      dragStartGainDb_ = nodeGainDb_[static_cast<size_t>(dragHandle_)];
  }

  void mouseDrag(const juce::MouseEvent& e) override
  {
    if (dragHandle_ == kNoHandle)
      return;

    auto barArea = getBarArea();
    float normX = (e.position.x - static_cast<float>(barArea.getX())) /
                  static_cast<float>(barArea.getWidth());
    float newFreq = normXToFreq(normX);

    if (dragHandle_ == kLowCutHandle)
    {
      lowCutFreqHz_ = newFreq;
      if (onLowCutChanged)
        onLowCutChanged(true, newFreq);
    }
    else if (dragHandle_ == kHighCutHandle)
    {
      highCutFreqHz_ = newFreq;
      if (onHighCutChanged)
        onHighCutChanged(true, newFreq);
    }
    else
    {
      float areaH = static_cast<float>(barArea.getHeight());
      float deltaY = dragStartPos_.y - e.position.y;
      float deltaGain = (deltaY / areaH) * (kMaxGainDb - kMinGainDb);
      float newGain = juce::jlimit(kMinGainDb, kMaxGainDb, dragStartGainDb_ + deltaGain);

      auto idx = static_cast<size_t>(dragHandle_);
      nodeFreqHz_[idx] = newFreq;
      nodeGainDb_[idx] = newGain;

      if (onNodeChanged)
        onNodeChanged(dragHandle_, true, newFreq, newGain);
    }
    repaint();
  }

  void mouseUp(const juce::MouseEvent&) override { dragHandle_ = kNoHandle; }

  void mouseDoubleClick(const juce::MouseEvent& e) override
  {
    int handle = handleAtPixel(e.position);
    if (handle == kNoHandle)
      return;

    if (handle == kLowCutHandle)
    {
      lowCutActive_ = false;
      if (onLowCutChanged)
        onLowCutChanged(false, lowCutFreqHz_);
    }
    else if (handle == kHighCutHandle)
    {
      highCutActive_ = false;
      if (onHighCutChanged)
        onHighCutChanged(false, highCutFreqHz_);
    }
    else
    {
      auto idx = static_cast<size_t>(handle);
      nodeActive_[idx] = false;
      if (onNodeChanged)
        onNodeChanged(handle, false, nodeFreqHz_[idx], 0.0f);
    }

    dragHandle_ = kNoHandle;
    hoverHandle_ = kNoHandle;
    repaint();
  }

  void mouseExit(const juce::MouseEvent&) override
  {
    if (hoverHandle_ != kNoHandle || ghostVisible_)
    {
      hoverHandle_ = kNoHandle;
      ghostVisible_ = false;
      setMouseCursor(juce::MouseCursor::NormalCursor);
      repaint();
    }
  }

  LCDSpectrumDisplay& getSpectrumDisplay() { return spectrumDisplay_; }

 private:
  LCDSpectrumDisplay spectrumDisplay_;
  std::array<bool, kNumNodes> nodeActive_{};
  std::array<float, kNumNodes> nodeFreqHz_{};
  std::array<float, kNumNodes> nodeGainDb_{};
  bool lowCutActive_ = true;
  float lowCutFreqHz_ = kMinFreqHz;
  bool highCutActive_ = true;
  float highCutFreqHz_ = kMaxFreqHz;
  double sampleRate_ = 44100.0;
  juce::Typeface::Ptr typeface_;

  int dragHandle_ = kNoHandle;
  int hoverHandle_ = kNoHandle;
  juce::Point<float> dragStartPos_;
  float dragStartGainDb_ = 0.0f;
  bool ghostVisible_ = false;
  juce::Point<float> ghostPos_;

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

  float yToGain(float y, float barAreaTop, float barAreaHeight) const
  {
    float normalized = 1.0f - (y - barAreaTop) / barAreaHeight;
    return juce::jlimit(kMinGainDb, kMaxGainDb,
                        kMinGainDb + normalized * (kMaxGainDb - kMinGainDb));
  }

  bool anyFilterActive() const
  {
    if (lowCutActive_ || highCutActive_)
      return true;

    for (int i = 0; i < kNumNodes; ++i)
    {
      if (nodeActive_[static_cast<size_t>(i)] &&
          std::fabs(nodeGainDb_[static_cast<size_t>(i)]) >= 0.05f)
        return true;
    }
    return false;
  }

  float responseAtFreq(float freqHz) const
  {
    return octob::GraphicEQ::computeMagnitudeResponseDb(
        nodeActive_.data(), nodeFreqHz_.data(), nodeGainDb_.data(), kNumNodes, lowCutActive_,
        lowCutFreqHz_, highCutActive_, highCutFreqHz_, freqHz, sampleRate_);
  }

  bool handleActive(int handle) const
  {
    if (handle == kLowCutHandle)
      return lowCutActive_;
    if (handle == kHighCutHandle)
      return highCutActive_;
    return getNodeActive(handle);
  }

  float handleFrequency(int handle) const
  {
    if (handle == kLowCutHandle)
      return lowCutFreqHz_;
    if (handle == kHighCutHandle)
      return highCutFreqHz_;
    return getNodeFrequency(handle);
  }

  // Pixel position of a handle's dot: x from its frequency, y from the combined
  // response at that frequency so the dot rides the drawn curve
  juce::Point<float> handlePixel(int handle, juce::Rectangle<int> barArea) const
  {
    float freqHz = handleFrequency(handle);
    float x = static_cast<float>(barArea.getX()) +
              freqToNormX(freqHz) * static_cast<float>(barArea.getWidth());
    float responseDb = juce::jlimit(kMinGainDb, kMaxGainDb, responseAtFreq(freqHz));
    float y = gainToY(responseDb, static_cast<float>(barArea.getY()),
                      static_cast<float>(barArea.getHeight()));
    return {x, y};
  }

  int handleAtPixel(juce::Point<float> p) const
  {
    constexpr int kNumHandles = kNumNodes + 2;
    auto barArea = getBarArea();
    std::array<bool, kNumHandles> active{};
    std::array<float, kNumHandles> xs{};
    std::array<float, kNumHandles> ys{};
    for (int handle = 0; handle < kNumHandles; ++handle)
    {
      auto idx = static_cast<size_t>(handle);
      active[idx] = handleActive(handle);
      if (!active[idx])
        continue;
      auto pt = handlePixel(handle, barArea);
      xs[idx] = pt.x;
      ys[idx] = pt.y;
    }
    return nearestNodeIndex(active.data(), xs.data(), ys.data(), kNumHandles, p.x, p.y,
                            kHitRadiusPx);
  }

  // Show a ghost dot on the response curve when the cursor is near the line,
  // hinting that clicking will create a node there
  void updateGhost(juce::Point<float> p)
  {
    auto barArea = getBarArea();
    float normX =
        (p.x - static_cast<float>(barArea.getX())) / static_cast<float>(barArea.getWidth());

    if (hoverHandle_ != kNoHandle || normX < 0.0f || normX > 1.0f)
    {
      ghostVisible_ = false;
      return;
    }

    float responseDb = juce::jlimit(kMinGainDb, kMaxGainDb, responseAtFreq(normXToFreq(normX)));
    float curveY = gainToY(responseDb, static_cast<float>(barArea.getY()),
                           static_cast<float>(barArea.getHeight()));

    ghostVisible_ = std::fabs(p.y - curveY) <= kGhostShowRadiusPx;
    ghostPos_ = {p.x, curveY};
  }

  int firstFreeSlot() const
  {
    for (int i = 0; i < kNumNodes; ++i)
    {
      if (!nodeActive_[static_cast<size_t>(i)])
        return i;
    }
    return -1;
  }

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GraphicEQDisplay)
};
