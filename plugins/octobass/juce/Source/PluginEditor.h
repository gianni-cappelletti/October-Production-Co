#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "LCDDisplay.h"
#include "OctoberLookAndFeel.h"
#include "PluginProcessor.h"

class OctoBassEditor : public juce::AudioProcessorEditor
{
 public:
  static constexpr int kDesignWidth = 740;
  static constexpr int kDesignHeight = 500;

  explicit OctoBassEditor(OctoBassProcessor&);
  ~OctoBassEditor() override;

  void paint(juce::Graphics&) override;
  void resized() override;

 private:
  // Must be declared before all widgets -- LookAndFeel must outlive the components that use it
  OctoberLookAndFeel laf_;

  OctoBassProcessor& audioProcessor;

  // Frequency analysis LCD (placeholder)
  LCDDisplay frequencyLCD_;

  // LOW zone controls
  juce::Label squashLabel_;
  juce::Slider squashSlider_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> squashAttachment_;

  juce::Label compressionModeLabel_;
  juce::Slider compressionModeSlider_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> compressionModeAttachment_;

  juce::Label lowBandLevelLabel_;
  juce::Slider lowBandLevelSlider_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowBandLevelAttachment_;

  juce::ToggleButton lowBandSoloButton_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lowBandSoloAttachment_;

  // CENTER zone controls
  juce::Label crossoverLabel_;
  juce::Slider crossoverSlider_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> crossoverAttachment_;

  juce::Label gateLabel_;
  juce::Slider gateSlider_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gateAttachment_;

  // HIGH zone controls
  juce::Label highInputGainLabel_;
  juce::Slider highInputGainSlider_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highInputGainAttachment_;

  juce::Label highOutputGainLabel_;
  juce::Slider highOutputGainSlider_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highOutputGainAttachment_;

  juce::ToggleButton highBandSoloButton_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> highBandSoloAttachment_;

  juce::Label highBandMixLabel_;
  juce::Slider highBandMixSlider_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highBandMixAttachment_;

  // NAM file loader
  juce::TextButton namLoadButton_;
  juce::TextButton namClearButton_;
  juce::TextButton namPrevButton_;
  juce::TextButton namNextButton_;
  LCDDisplay namLCDDisplay_;

  // IR file loader
  juce::TextButton irLoadButton_;
  juce::TextButton irClearButton_;
  juce::TextButton irPrevButton_;
  juce::TextButton irNextButton_;
  LCDDisplay irLCDDisplay_;

  // File loader helpers
  void namLoadClicked();
  void namClearClicked();
  void namPrevClicked();
  void namNextClicked();
  void irLoadClicked();
  void irClearClicked();
  void irPrevClicked();
  void irNextClicked();
  void cycleNamFile(int direction);
  void cycleIRFile(int direction);
  juce::File getLastBrowsedDirectory() const;
  void updateLastBrowsedDirectory(const juce::File& file);

  juce::File lastBrowsedDirectory_;
  juce::Image logoImage_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OctoBassEditor)
};
