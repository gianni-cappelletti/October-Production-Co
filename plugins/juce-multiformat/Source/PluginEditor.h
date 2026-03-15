#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "LCDDisplay.h"
#include "OctobIRLookAndFeel.h"
#include "PluginProcessor.h"

class LCDBargraph : public juce::Component
{
 public:
  LCDBargraph(const juce::String& name, float minValue, float maxValue,
              bool centreBalanced = false);
  void setValue(float value);
  void setThresholdMarkers(float low, float high);
  void setShowThresholds(bool show)
  {
    showThresholds_ = show;
    repaint();
  }
  void paint(juce::Graphics& g) override;

 private:
  juce::String name_;
  float minValue_;
  float maxValue_;
  float currentValue_ = 0.0f;
  float lowThreshold_ = 0.0f;
  float highThreshold_ = 0.0f;
  bool showThresholds_ = false;
  bool centreBalanced_;

  static constexpr int numSegments_ = 20;
  static constexpr int segmentGap_ = 2;
};

class OctobIREditor : public juce::AudioProcessorEditor, private juce::Timer
{
 public:
  explicit OctobIREditor(OctobIRProcessor&);
  ~OctobIREditor() override;

  void paint(juce::Graphics&) override;
  void resized() override;

 private:
  void timerCallback() override;

  // Must be declared before all widgets — LookAndFeel must outlive the components that use it
  OctobIRLookAndFeel laf_;

  OctobIRProcessor& audioProcessor;

  juce::TextButton loadButton1_;
  juce::TextButton clearButton1_;
  juce::TextButton prevButton1_;
  juce::TextButton nextButton1_;
  LCDDisplay ir1LCDDisplay_;
  juce::ToggleButton ir1EnableButton_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ir1EnableAttachment_;

  juce::TextButton loadButton2_;
  juce::TextButton clearButton2_;
  juce::TextButton prevButton2_;
  juce::TextButton nextButton2_;
  LCDDisplay ir2LCDDisplay_;
  juce::ToggleButton ir2EnableButton_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ir2EnableAttachment_;

  LCDBargraph inputLevelMeter_;
  LCDBargraph blendMeter_;

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

  juce::Label latencyLabel_;

  void loadButton1Clicked();
  void loadButton2Clicked();
  void clearButton1Clicked();
  void clearButton2Clicked();
  void prevButton1Clicked();
  void nextButton1Clicked();
  void prevButton2Clicked();
  void nextButton2Clicked();
  void swapIROrderClicked();
  void updateLatencyDisplay();
  void updateMeters();
  void cycleIRFile(int irIndex, int direction);
  juce::File getLastBrowsedDirectory() const;
  void updateLastBrowsedDirectory(const juce::File& file);

  juce::File lastBrowsedDirectory_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OctobIREditor)
};
