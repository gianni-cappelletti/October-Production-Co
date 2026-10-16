#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <octobass-core/NamProcessor.hpp>
#include <octobass-core/Types.hpp>

class NamCalibrationPanel : public juce::Component
{
 public:
  NamCalibrationPanel(juce::AudioProcessorValueTreeState& apvts, octob::NamSlot slot);

  // Greys out the controls whose metadata the loaded model lacks
  void setCapabilities(const octob::NamModelMetadata& metadata);

  std::function<void()> onDismiss;

  void paint(juce::Graphics&) override;
  void resized() override;
  void mouseDown(const juce::MouseEvent&) override;
  bool keyPressed(const juce::KeyPress&) override;

 private:
  juce::Rectangle<int> panelBounds_;

  juce::Label titleLabel_;
  juce::TextButton closeButton_;

  juce::ToggleButton calibrateInputToggle_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> calibrateInputAttachment_;

  juce::Label inputLevelLabel_;
  juce::Slider inputLevelSlider_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputLevelAttachment_;

  juce::Label outputModeLabel_;
  juce::ComboBox outputModeCombo_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> outputModeAttachment_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NamCalibrationPanel)
};
