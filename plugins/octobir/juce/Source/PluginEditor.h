#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "LCDDisplay.h"
#include "OctoberLookAndFeel.h"
#include "PluginProcessor.h"

class LCDMeterPanel : public juce::Component
{
 public:
  void setTypeface(juce::Typeface::Ptr tf)
  {
    typeface_ = tf;
    repaint();
  }
  void setInputValue(float v)
  {
    inputValue_ = v;
    updateAccessibleTitle();
    repaint();
  }
  void setBlendValue(float v)
  {
    blendValue_ = v;
    updateAccessibleTitle();
    repaint();
  }
  void setShowThresholds(bool show)
  {
    showThresholds_ = show;
    repaint();
  }
  void setThresholdMarkers(float lo, float hi)
  {
    lowThreshold_ = lo;
    highThreshold_ = hi;
    repaint();
  }
  void paint(juce::Graphics& g) override;

  std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
  {
    return std::make_unique<juce::AccessibilityHandler>(*this, juce::AccessibilityRole::group);
  }

 private:
  void updateAccessibleTitle()
  {
    setTitle("Input: " + juce::String(inputValue_, 1) +
             " dB, Blend: " + juce::String(blendValue_, 2));
  }
  float inputValue_ = 0.0f;
  float blendValue_ = 0.0f;
  float lowThreshold_ = 0.0f;
  float highThreshold_ = 0.0f;
  bool showThresholds_ = false;
  juce::Typeface::Ptr typeface_;

  static constexpr int numSegments_ = 19;
  static constexpr int segmentGap_ = 2;

  void paintMeter(juce::Graphics& g, juce::Rectangle<int> barArea, float normValue,
                  bool centreBalanced, bool showThresholds, float normLow, float normHigh) const;
};

class OctobIREditor : public juce::AudioProcessorEditor, private juce::Timer
{
 public:
  static constexpr int kDesignWidth = 580;
  static constexpr int kDesignHeight = 694;

  explicit OctobIREditor(OctobIRProcessor&);
  ~OctobIREditor() override;

  void paint(juce::Graphics&) override;
  void resized() override;

 private:
  void timerCallback() override;

  // Must be declared before all widgets — LookAndFeel must outlive the components that use it
  OctoberLookAndFeel laf_;

  OctobIRProcessor& audioProcessor;

  juce::TextButton loadButton1_;
  juce::TextButton clearButton1_;
  juce::TextButton prevButton1_;
  juce::TextButton nextButton1_;
  LCDDisplay ir1LCDDisplay_;
  juce::ToggleButton ir1EnableButton_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ir1EnableAttachment_;

  juce::Slider irATrimSlider_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> irATrimAttachment_;

  juce::TextButton loadButton2_;
  juce::TextButton clearButton2_;
  juce::TextButton prevButton2_;
  juce::TextButton nextButton2_;
  LCDDisplay ir2LCDDisplay_;
  juce::ToggleButton ir2EnableButton_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ir2EnableAttachment_;

  juce::Slider irBTrimSlider_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> irBTrimAttachment_;

  LCDMeterPanel meterPanel_;

  juce::ToggleButton dynamicModeButton_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> dynamicModeAttachment_;

  juce::ToggleButton sidechainEnableButton_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> sidechainEnableAttachment_;

  juce::TextButton swapIROrderButton_;

  juce::Label blendLabel_;
  juce::Slider blendSlider_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> blendAttachment_;

  juce::Label thresholdLabel_;
  juce::Slider thresholdSlider_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> thresholdAttachment_;

  juce::Label rangeDbLabel_;
  juce::Slider rangeDbSlider_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rangeDbAttachment_;

  juce::Label kneeWidthDbLabel_;
  juce::Slider kneeWidthDbSlider_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> kneeWidthDbAttachment_;

  juce::Label detectionModeLabel_;
  juce::ComboBox detectionModeCombo_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> detectionModeAttachment_;

  juce::Label attackTimeLabel_;
  juce::Slider attackTimeSlider_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackTimeAttachment_;

  juce::Label releaseTimeLabel_;
  juce::Slider releaseTimeSlider_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseTimeAttachment_;

  juce::Label outputGainLabel_;
  juce::Slider outputGainSlider_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment_;

  void loadButton1Clicked();
  void loadButton2Clicked();
  void clearButton1Clicked();
  void clearButton2Clicked();
  void prevButton1Clicked();
  void nextButton1Clicked();
  void prevButton2Clicked();
  void nextButton2Clicked();
  void swapIROrderClicked();
  void updateMeters();
  void cycleIRFile(int irIndex, int direction);
  juce::File getLastBrowsedDirectory() const;
  void updateLastBrowsedDirectory(const juce::File& file);

  juce::File lastBrowsedDirectory_;
  juce::Image logoImage_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OctobIREditor)
};
