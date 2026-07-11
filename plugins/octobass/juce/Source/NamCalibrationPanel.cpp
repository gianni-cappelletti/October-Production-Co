#include "NamCalibrationPanel.h"

namespace
{
constexpr int kPanelWidth = 380;
constexpr int kPanelHeight = 236;
constexpr int kPanelPadding = 20;
constexpr int kRowHeight = 28;
constexpr int kLabelHeight = 18;
constexpr int kRowGap = 10;

// Matches the LookAndFeel's ComboBox palette; white text on this fill is
// ~14:1 contrast (WCAG 2.1 AAA)
const juce::Colour kPanelFill(0xff262626);
const juce::Colour kPanelOutline(0xff181818);
const juce::Colour kTextColour(0xffffffff);
const juce::Colour kAccentColour(0xffe07030);
const juce::Colour kTrackColour(0xff404040);
}  // namespace

NamCalibrationPanel::NamCalibrationPanel(juce::AudioProcessorValueTreeState& apvts)
{
  setWantsKeyboardFocus(true);

  addAndMakeVisible(titleLabel_);
  titleLabel_.setText("NAM CALIBRATION", juce::dontSendNotification);
  titleLabel_.setJustificationType(juce::Justification::centredLeft);
  titleLabel_.setColour(juce::Label::textColourId, kTextColour);

  addAndMakeVisible(closeButton_);
  closeButton_.setButtonText("X");
  closeButton_.setTitle("Close NAM Calibration");
  closeButton_.onClick = [this]
  {
    if (onDismiss)
      onDismiss();
  };

  addAndMakeVisible(calibrateInputToggle_);
  calibrateInputToggle_.setButtonText("CALIBRATE INPUT");
  calibrateInputToggle_.setTitle("Calibrate Input");
  calibrateInputToggle_.setColour(juce::ToggleButton::textColourId, kTextColour);
  calibrateInputToggle_.setColour(juce::ToggleButton::tickColourId, kAccentColour);
  calibrateInputToggle_.setColour(juce::ToggleButton::tickDisabledColourId, kTrackColour);
  calibrateInputAttachment_ =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          apvts, "namCalibrateInput", calibrateInputToggle_);

  addAndMakeVisible(inputLevelLabel_);
  inputLevelLabel_.setText("INPUT LEVEL (dBu)", juce::dontSendNotification);
  inputLevelLabel_.setJustificationType(juce::Justification::centredLeft);
  inputLevelLabel_.setColour(juce::Label::textColourId, kTextColour);

  addAndMakeVisible(inputLevelSlider_);
  inputLevelSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
  inputLevelSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 22);
  inputLevelSlider_.setColour(juce::Slider::trackColourId, kAccentColour);
  inputLevelSlider_.setColour(juce::Slider::backgroundColourId, kTrackColour);
  inputLevelSlider_.setColour(juce::Slider::thumbColourId, kTextColour);
  inputLevelAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      apvts, "namInputCalibrationLevel", inputLevelSlider_);

  addAndMakeVisible(outputModeLabel_);
  outputModeLabel_.setText("OUTPUT MODE", juce::dontSendNotification);
  outputModeLabel_.setJustificationType(juce::Justification::centredLeft);
  outputModeLabel_.setColour(juce::Label::textColourId, kTextColour);

  addAndMakeVisible(outputModeCombo_);
  outputModeCombo_.setTitle("NAM Output Mode");
  // Item IDs are choice index + 1: ComboBoxAttachment maps them onto the
  // Raw/Normalized/Calibrated choice parameter
  outputModeCombo_.addItem("Raw", 1);
  outputModeCombo_.addItem("Normalized", 2);
  outputModeCombo_.addItem("Calibrated", 3);
  outputModeAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
      apvts, "namOutputMode", outputModeCombo_);

  setCapabilities({});
}

void NamCalibrationPanel::setCapabilities(const octob::NamModelMetadata& metadata)
{
  calibrateInputToggle_.setEnabled(metadata.hasInputLevel);
  inputLevelSlider_.setEnabled(metadata.hasInputLevel);
  inputLevelLabel_.setEnabled(metadata.hasInputLevel);

  outputModeCombo_.setItemEnabled(2, metadata.hasLoudness);
  outputModeCombo_.setItemEnabled(3, metadata.hasOutputLevel);
}

void NamCalibrationPanel::paint(juce::Graphics& g)
{
  g.fillAll(juce::Colours::black.withAlpha(0.55f));

  const auto panel = panelBounds_.toFloat();
  g.setColour(kPanelFill);
  g.fillRoundedRectangle(panel, 4.0f);
  g.setColour(kPanelOutline);
  g.drawRoundedRectangle(panel.reduced(0.5f), 4.0f, 1.0f);
}

void NamCalibrationPanel::resized()
{
  panelBounds_ =
      juce::Rectangle<int>(kPanelWidth, kPanelHeight).withCentre(getLocalBounds().getCentre());

  auto content = panelBounds_.reduced(kPanelPadding);

  auto titleRow = content.removeFromTop(kRowHeight);
  closeButton_.setBounds(titleRow.removeFromRight(kRowHeight).reduced(2));
  titleLabel_.setBounds(titleRow);
  content.removeFromTop(kRowGap);

  calibrateInputToggle_.setBounds(content.removeFromTop(kRowHeight));
  content.removeFromTop(kRowGap);

  inputLevelLabel_.setBounds(content.removeFromTop(kLabelHeight));
  inputLevelSlider_.setBounds(content.removeFromTop(kRowHeight));
  content.removeFromTop(kRowGap);

  outputModeLabel_.setBounds(content.removeFromTop(kLabelHeight));
  outputModeCombo_.setBounds(content.removeFromTop(kRowHeight));
}

void NamCalibrationPanel::mouseDown(const juce::MouseEvent& event)
{
  if (!panelBounds_.contains(event.getPosition()) && onDismiss)
    onDismiss();
}

bool NamCalibrationPanel::keyPressed(const juce::KeyPress& key)
{
  if (key == juce::KeyPress::escapeKey && onDismiss)
  {
    onDismiss();
    return true;
  }
  return false;
}
