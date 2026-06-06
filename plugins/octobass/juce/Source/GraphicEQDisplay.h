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

  GraphicEQDisplay();

  void setTypeface(juce::Typeface::Ptr tf)
  {
    typeface_ = tf;
    spectrumDisplay_.setTypeface(tf);
  }

  void setBandLevels(const float* levelsDb, int count)
  {
    spectrumDisplay_.setBandLevels(levelsDb, count);
  }

  void setSpectrumDbRange(float minDb, float maxDb) { spectrumDisplay_.setDbRange(minDb, maxDb); }

  void setCrossoverNormPosition(float normPos)
  {
    spectrumDisplay_.setCrossoverNormPosition(normPos);
  }

  void setSampleRate(double sr) { sampleRate_ = sr; }

  void setNode(int slot, bool active, float freqHz, float gainDb);
  void setLowCut(bool active, float freqHz);
  void setHighCut(bool active, float freqHz);

  bool getNodeActive(int slot) const
  {
    return slot >= 0 && slot < kNumNodes && nodes_[static_cast<size_t>(slot)].active;
  }

  float getNodeFrequency(int slot) const
  {
    if (slot < 0 || slot >= kNumNodes)
      return octob::DefaultGraphicEQFreqHz;
    return nodes_[static_cast<size_t>(slot)].freqHz;
  }

  float getNodeGain(int slot) const
  {
    if (slot < 0 || slot >= kNumNodes)
      return 0.0f;
    return nodes_[static_cast<size_t>(slot)].gainDb;
  }

  bool getLowCutActive() const { return lowCut_.active; }
  float getLowCutFrequency() const { return lowCut_.freqHz; }
  bool getHighCutActive() const { return highCut_.active; }
  float getHighCutFrequency() const { return highCut_.freqHz; }

  std::function<void(int slot, bool active, float freqHz, float gainDb)> onNodeChanged;
  std::function<void(bool active, float freqHz)> onLowCutChanged;
  std::function<void(bool active, float freqHz)> onHighCutChanged;

  // Mouse gesture bracketing so the editor can begin/end host automation
  // gestures around drags; fired before the first and after the last
  // parameter-changing callback of a gesture
  std::function<void(int handle)> onDragStart;
  std::function<void(int handle)> onDragEnd;

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
  void paintOverChildren(juce::Graphics& g) override;

  void mouseMove(const juce::MouseEvent& e) override;
  void mouseDown(const juce::MouseEvent& e) override;
  void mouseDrag(const juce::MouseEvent& e) override;
  void mouseUp(const juce::MouseEvent& e) override;
  void mouseDoubleClick(const juce::MouseEvent& e) override;
  void mouseExit(const juce::MouseEvent& e) override;

  LCDSpectrumDisplay& getSpectrumDisplay() { return spectrumDisplay_; }

 private:
  LCDSpectrumDisplay spectrumDisplay_;
  std::array<octob::GraphicEQNode, kNumNodes> nodes_{};
  octob::GraphicEQCut lowCut_{true, kMinFreqHz};
  octob::GraphicEQCut highCut_{true, kMaxFreqHz};
  double sampleRate_ = 44100.0;
  juce::Typeface::Ptr typeface_;

  int dragHandle_ = kNoHandle;
  int hoverHandle_ = kNoHandle;
  int justCreatedHandle_ = kNoHandle;
  juce::Point<float> dragStartPos_;
  float dragStartGainDb_ = 0.0f;
  bool ghostVisible_ = false;
  juce::Point<float> ghostPos_;

  juce::Rectangle<int> getBarArea() const;
  float gainToY(float gainDb, float barAreaTop, float barAreaHeight) const;
  float yToGain(float y, float barAreaTop, float barAreaHeight) const;
  bool anyFilterActive() const;
  float responseAtFreq(float freqHz) const;
  bool handleActive(int handle) const;
  float handleFrequency(int handle) const;
  juce::Point<float> handlePixel(int handle, juce::Rectangle<int> barArea) const;
  int handleAtPixel(juce::Point<float> p) const;
  void updateGhost(juce::Point<float> p);
  int firstFreeSlot() const;
  void endDrag();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GraphicEQDisplay)
};
