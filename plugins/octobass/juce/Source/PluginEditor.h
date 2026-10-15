#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

#include "GraphicEQDisplay.h"
#include "LCDDisplay.h"
#include "NamCalibrationPanel.h"
#include "OctoberLookAndFeel.h"
#include "PluginProcessor.h"
#include "PopupToggleButton.h"
#include "SpectrumAnalyzer.h"

class OctoBassEditor : public juce::AudioProcessorEditor, private juce::Timer
{
 public:
  static constexpr int kDesignWidth = 760;
  static constexpr int kDesignHeight = 540;

  explicit OctoBassEditor(OctoBassProcessor&);
  ~OctoBassEditor() override;

  void paint(juce::Graphics&) override;
  void resized() override;

 private:
  void timerCallback() override;
  // Must be declared before all widgets -- LookAndFeel must outlive the components that use it
  OctoberLookAndFeel laf_;

  OctoBassProcessor& audioProcessor;

  // Spectrum analyzer + EQ display
  GraphicEQDisplay graphicEQDisplay_;
  SpectrumAnalyzer spectrumAnalyzer_;
  double lastSampleRate_ = 0.0;

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

  // One loader per NAM slot, stacked in signal-flow order above the IR loader
  struct NamSlotControls
  {
    juce::TextButton loadButton;
    juce::TextButton clearButton;
    juce::TextButton prevButton;
    juce::TextButton nextButton;
    juce::TextButton calibrateButton;
    PopupToggleButton onSwitch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
    PopupToggleButton qualityToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> qualityAttachment;
    LCDDisplay lcdDisplay;
    std::unique_ptr<juce::ParameterAttachment> backlightAttachment;
    std::unique_ptr<NamCalibrationPanel> calibrationPanel;

    juce::String shownPath;
    int lastQualityLevels = -1;
    octob::NamModelMetadata lastMetadata;
    bool lastLoaded = false;
  };
  std::array<NamSlotControls, octob::kNumNamSlots> namSlots_;
  NamSlotControls& namControls(octob::NamSlot slot) { return namSlots_[static_cast<size_t>(slot)]; }

  void setUpNamSlot(octob::NamSlot slot);
  void updateNamQualityToggle(octob::NamSlot slot);
  void updateNamCalibrationControls(octob::NamSlot slot);
  void namLoadClicked(octob::NamSlot slot);
  void namClearClicked(octob::NamSlot slot);
  void cycleNamFile(octob::NamSlot slot, int direction);
  void showNamPath(octob::NamSlot slot, const juce::String& path);
  void showNamLoadResult(octob::NamSlot slot, const juce::File& file, bool loaded,
                         const juce::String& error);

  // IR file loader
  juce::TextButton irLoadButton_;
  juce::TextButton irClearButton_;
  juce::TextButton irPrevButton_;
  juce::TextButton irNextButton_;
  LCDDisplay irLCDDisplay_;
  juce::String shownIRPath_;

  void irLoadClicked();
  void irClearClicked();
  void cycleIRFile(int direction);
  void showIRPath(const juce::String& path);
  void showIRLoadResult(const juce::File& file, bool loaded, const juce::String& error);

  // Files can also change outside this editor (host state restore), so the
  // displays follow the processor rather than only this editor's actions
  void syncLoadedFileDisplays();

  // Cached parameter handles so the 30 Hz timer and the EQ display callbacks
  // avoid per-tick string lookups
  struct ParamHandle
  {
    juce::RangedAudioParameter* param = nullptr;
    std::atomic<float>* value = nullptr;
  };
  struct EQNodeParamHandles
  {
    ParamHandle active;
    ParamHandle freq;
    ParamHandle gain;
  };
  ParamHandle paramHandle(const juce::String& paramID) const;
  void forEachHandleParam(int handle, GraphicEQDisplay::GestureScope scope,
                          void (juce::RangedAudioParameter::*action)());

  std::array<EQNodeParamHandles, octob::kGraphicEQNumNodes> eqNodeParams_{};
  ParamHandle eqLowCutActiveParam_;
  ParamHandle eqLowCutFreqParam_;
  ParamHandle eqHighCutActiveParam_;
  ParamHandle eqHighCutFreqParam_;
  ParamHandle crossoverParam_;

  juce::Image logoImage_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OctoBassEditor)
};
