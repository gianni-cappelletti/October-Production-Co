#include "PluginEditor.h"

#include "PluginProcessor.h"

VerticalMeter::VerticalMeter(const juce::String& name, float minValue, float maxValue)
    : name_(name), minValue_(minValue), maxValue_(maxValue)
{
  startTimerHz(30);
}

void VerticalMeter::setValue(float value)
{
  currentValue_ = value;
}

void VerticalMeter::setThresholdMarkers(float low, float high)
{
  lowThreshold_ = low;
  highThreshold_ = high;
}

void VerticalMeter::setBlendRangeMarkers(float min, float max)
{
  minBlend_ = min;
  maxBlend_ = max;
}

void VerticalMeter::paint(juce::Graphics& g)
{
  auto bounds = getLocalBounds();
  int labelHeight = 20;
  auto meterBounds = bounds.removeFromBottom(bounds.getHeight() - labelHeight);

  g.setColour(juce::Colour(0xffe8e8e8));
  g.fillRect(meterBounds);

  g.setColour(juce::Colour(0xffcccccc));
  g.drawRect(meterBounds, 1);

  float normalizedValue = (currentValue_ - minValue_) / (maxValue_ - minValue_);
  normalizedValue = std::max(0.0f, std::min(1.0f, normalizedValue));

  float ledDiameter = (static_cast<float>(meterBounds.getWidth()) - 8.0f) / 2.0f;
  float totalHeight = numLEDs_ * ledDiameter + (numLEDs_ - 1) * ledSpacing_;
  float startY = meterBounds.getY() + (meterBounds.getHeight() - totalHeight) / 2.0f;
  float centerX = meterBounds.getX() + meterBounds.getWidth() / 2.0f;

  for (int i = 0; i < numLEDs_; ++i)
  {
    int reversedIndex = numLEDs_ - 1 - i;
    float ledY = startY + i * (ledDiameter + ledSpacing_);
    auto ledBounds =
        juce::Rectangle<float>(centerX - ledDiameter / 2.0f, ledY, ledDiameter, ledDiameter);

    float ledThreshold = static_cast<float>(reversedIndex) / static_cast<float>(numLEDs_);
    bool isLit = normalizedValue >= ledThreshold;

    juce::Colour ledColor;
    if (name_ == "Blend")
    {
      ledColor = getBlendLEDColor(reversedIndex, isLit);
    }
    else
    {
      ledColor = getLEDColor(reversedIndex, isLit);
    }

    g.setColour(ledColor);
    g.fillEllipse(ledBounds);

    if (isLit)
    {
      g.setColour(ledColor.withAlpha(0.25f));
      g.fillEllipse(ledBounds.expanded(2.0f));
    }

    g.setColour(juce::Colour(0xffaaaaaa));
    g.drawEllipse(ledBounds, 1.0f);
  }

  g.setColour(juce::Colour(0xff1a1a1a));
  g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
  g.drawText(name_, bounds.removeFromTop(labelHeight), juce::Justification::centred, true);
}

juce::Colour VerticalMeter::getLEDColor(int ledIndex, bool isLit) const
{
  float position = static_cast<float>(ledIndex) / static_cast<float>(numLEDs_ - 1);

  if (position < 0.67f)
  {
    return isLit ? juce::Colour(0xffe07030) : juce::Colour(0xffd8c8b8);
  }
  else if (position < 0.83f)
  {
    return isLit ? juce::Colour(0xffc85820) : juce::Colour(0xffd8c8b8);
  }
  else
  {
    return isLit ? juce::Colour(0xffb04010) : juce::Colour(0xffd8c8b8);
  }
}

juce::Colour VerticalMeter::getBlendLEDColor(int /*ledIndex*/, bool isLit) const
{
  return isLit ? juce::Colour(0xffe07030) : juce::Colour(0xffd8c8b8);
}

void VerticalMeter::timerCallback()
{
  repaint();
}

static void setupRotarySlider(juce::Slider& s, int textBoxWidth = 70)
{
  s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, textBoxWidth, 18);
  s.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                        juce::MathConstants<float>::pi * 2.75f, true);
}

OctobIREditor::OctobIREditor(OctobIRProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      inputLevelMeter_("Input Level", -96.0f, 0.0f),
      blendMeter_("Blend", -1.0f, 1.0f)
{
  setLookAndFeel(&laf_);

  addAndMakeVisible(ir1TitleLabel_);
  ir1TitleLabel_.setText("IR A (-1.0)", juce::dontSendNotification);
  ir1TitleLabel_.setJustificationType(juce::Justification::centredLeft);
  ir1TitleLabel_.setFont(juce::FontOptions(14.0f, juce::Font::bold));

  addAndMakeVisible(loadButton1_);
  loadButton1_.setButtonText("Load");
  loadButton1_.onClick = [this] { loadButton1Clicked(); };

  addAndMakeVisible(clearButton1_);
  clearButton1_.setButtonText("Clear");
  clearButton1_.onClick = [this] { clearButton1Clicked(); };

  addAndMakeVisible(prevButton1_);
  prevButton1_.setButtonText("<");
  prevButton1_.onClick = [this] { prevButton1Clicked(); };

  addAndMakeVisible(nextButton1_);
  nextButton1_.setButtonText(">");
  nextButton1_.onClick = [this] { nextButton1Clicked(); };

  addAndMakeVisible(ir1LCDDisplay_);
  ir1LCDDisplay_.setTextColour(juce::Colour(0xffe07030));
  ir1LCDDisplay_.setText(audioProcessor.getCurrentIR1Path().isEmpty()
                             ? "No IR loaded"
                             : audioProcessor.getCurrentIR1Path());

  addAndMakeVisible(ir1EnableButton_);
  ir1EnableButton_.setButtonText("Enable A");
  ir1EnableButton_.getProperties().set("isLED", true);
  ir1EnableButton_.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffe07030));
  ir1EnableAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
      audioProcessor.getAPVTS(), "irAEnable", ir1EnableButton_);

  addAndMakeVisible(ir2TitleLabel_);
  ir2TitleLabel_.setText("IR B (+1.0)", juce::dontSendNotification);
  ir2TitleLabel_.setJustificationType(juce::Justification::centredLeft);
  ir2TitleLabel_.setFont(juce::FontOptions(14.0f, juce::Font::bold));

  addAndMakeVisible(loadButton2_);
  loadButton2_.setButtonText("Load");
  loadButton2_.onClick = [this] { loadButton2Clicked(); };

  addAndMakeVisible(clearButton2_);
  clearButton2_.setButtonText("Clear");
  clearButton2_.onClick = [this] { clearButton2Clicked(); };

  addAndMakeVisible(prevButton2_);
  prevButton2_.setButtonText("<");
  prevButton2_.onClick = [this] { prevButton2Clicked(); };

  addAndMakeVisible(nextButton2_);
  nextButton2_.setButtonText(">");
  nextButton2_.onClick = [this] { nextButton2Clicked(); };

  addAndMakeVisible(ir2LCDDisplay_);
  ir2LCDDisplay_.setTextColour(juce::Colour(0xffe07030));
  ir2LCDDisplay_.setText(audioProcessor.getCurrentIR2Path().isEmpty()
                             ? "No IR loaded"
                             : audioProcessor.getCurrentIR2Path());

  addAndMakeVisible(ir2EnableButton_);
  ir2EnableButton_.setButtonText("Enable B");
  ir2EnableButton_.getProperties().set("isLED", true);
  ir2EnableButton_.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffe07030));
  ir2EnableAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
      audioProcessor.getAPVTS(), "irBEnable", ir2EnableButton_);

  addAndMakeVisible(inputLevelMeter_);
  addAndMakeVisible(blendMeter_);

  addAndMakeVisible(dynamicModeButton_);
  dynamicModeButton_.setButtonText("Dynamic Mode");
  dynamicModeAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
      audioProcessor.getAPVTS(), "dynamicMode", dynamicModeButton_);

  addAndMakeVisible(sidechainEnableButton_);
  sidechainEnableButton_.setButtonText("Sidechain Enable");
  sidechainEnableAttachment_ =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          audioProcessor.getAPVTS(), "sidechainEnable", sidechainEnableButton_);

  addAndMakeVisible(swapIROrderButton_);
  swapIROrderButton_.setButtonText("Swap IR Order");
  swapIROrderButton_.onClick = [this] { swapIROrderClicked(); };

  addAndMakeVisible(blendLabel_);
  blendLabel_.setText("Static Blend", juce::dontSendNotification);
  blendLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(blendSlider_);
  setupRotarySlider(blendSlider_);
  blendAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      audioProcessor.getAPVTS(), "blend", blendSlider_);

  addAndMakeVisible(outputGainLabel_);
  outputGainLabel_.setText("Output Gain", juce::dontSendNotification);
  outputGainLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(outputGainSlider_);
  setupRotarySlider(outputGainSlider_);
  outputGainAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      audioProcessor.getAPVTS(), "outputGain", outputGainSlider_);

  addAndMakeVisible(thresholdLabel_);
  thresholdLabel_.setText("Threshold", juce::dontSendNotification);
  thresholdLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(thresholdSlider_);
  setupRotarySlider(thresholdSlider_, 60);
  thresholdAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      audioProcessor.getAPVTS(), "threshold", thresholdSlider_);

  addAndMakeVisible(rangeDbLabel_);
  rangeDbLabel_.setText("Range", juce::dontSendNotification);
  rangeDbLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(rangeDbSlider_);
  setupRotarySlider(rangeDbSlider_, 60);
  rangeDbAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      audioProcessor.getAPVTS(), "rangeDb", rangeDbSlider_);

  addAndMakeVisible(kneeWidthDbLabel_);
  kneeWidthDbLabel_.setText("Knee", juce::dontSendNotification);
  kneeWidthDbLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(kneeWidthDbSlider_);
  setupRotarySlider(kneeWidthDbSlider_, 60);
  kneeWidthDbAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      audioProcessor.getAPVTS(), "kneeWidthDb", kneeWidthDbSlider_);

  addAndMakeVisible(attackTimeLabel_);
  attackTimeLabel_.setText("Attack", juce::dontSendNotification);
  attackTimeLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(attackTimeSlider_);
  setupRotarySlider(attackTimeSlider_, 60);
  attackTimeAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      audioProcessor.getAPVTS(), "attackTime", attackTimeSlider_);

  addAndMakeVisible(releaseTimeLabel_);
  releaseTimeLabel_.setText("Release", juce::dontSendNotification);
  releaseTimeLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(releaseTimeSlider_);
  setupRotarySlider(releaseTimeSlider_, 60);
  releaseTimeAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      audioProcessor.getAPVTS(), "releaseTime", releaseTimeSlider_);

  addAndMakeVisible(detectionModeLabel_);
  detectionModeLabel_.setText("Detection", juce::dontSendNotification);
  detectionModeLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(detectionModeCombo_);
  detectionModeCombo_.addItem("Peak", 1);
  detectionModeCombo_.addItem("RMS", 2);
  detectionModeAttachment_ =
      std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
          audioProcessor.getAPVTS(), "detectionMode", detectionModeCombo_);

  addAndMakeVisible(latencyLabel_);
  latencyLabel_.setJustificationType(juce::Justification::centred);
  latencyLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff8c4010));
  updateLatencyDisplay();

  startTimerHz(30);
  setSize(700, 760);
}

OctobIREditor::~OctobIREditor()
{
  stopTimer();
  setLookAndFeel(nullptr);
}

void OctobIREditor::paint(juce::Graphics& g)
{
  g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

  g.setColour(juce::Colour(0xffe07030));
  g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
  g.drawText("OctobIR", getLocalBounds().removeFromTop(50), juce::Justification::centred, true);
}

void OctobIREditor::resized()
{
  auto bounds = getLocalBounds().reduced(15);
  bounds.removeFromTop(50);

  // --- IR Loading Section (110px) ---
  auto irSection = bounds.removeFromTop(110);

  auto irButtonRow = irSection.removeFromTop(30);
  auto halfW = irButtonRow.getWidth() / 2;

  {
    auto col = irButtonRow.removeFromLeft(halfW);
    ir1TitleLabel_.setBounds(col.removeFromLeft(55));
    loadButton1_.setBounds(col.removeFromLeft(55).reduced(2));
    clearButton1_.setBounds(col.removeFromLeft(48).reduced(2));
    prevButton1_.setBounds(col.removeFromLeft(28).reduced(2));
    nextButton1_.setBounds(col.removeFromLeft(28).reduced(2));
    ir1EnableButton_.setBounds(col.reduced(2));
  }
  {
    auto col = irButtonRow;
    ir2TitleLabel_.setBounds(col.removeFromLeft(55));
    loadButton2_.setBounds(col.removeFromLeft(55).reduced(2));
    clearButton2_.setBounds(col.removeFromLeft(48).reduced(2));
    prevButton2_.setBounds(col.removeFromLeft(28).reduced(2));
    nextButton2_.setBounds(col.removeFromLeft(28).reduced(2));
    ir2EnableButton_.setBounds(col.reduced(2));
  }

  irSection.removeFromTop(5);
  auto irLcdRow = irSection.removeFromTop(35);
  halfW = irLcdRow.getWidth() / 2;
  ir1LCDDisplay_.setBounds(irLcdRow.removeFromLeft(halfW).reduced(3, 2));
  ir2LCDDisplay_.setBounds(irLcdRow.reduced(3, 2));

  irSection.removeFromTop(10);
  auto modeRow = irSection.removeFromTop(30);
  dynamicModeButton_.setBounds(modeRow.removeFromLeft(140).reduced(2));
  modeRow.removeFromLeft(5);
  sidechainEnableButton_.setBounds(modeRow.removeFromLeft(150).reduced(2));
  modeRow.removeFromLeft(5);
  swapIROrderButton_.setBounds(modeRow.removeFromLeft(120).reduced(2));

  // --- Meters (205px) ---
  bounds.removeFromTop(10);
  auto meterRow = bounds.removeFromTop(205);
  auto metersSection = meterRow.withSizeKeepingCentre(140, meterRow.getHeight());
  inputLevelMeter_.setBounds(metersSection.removeFromLeft(60));
  metersSection.removeFromLeft(20);
  blendMeter_.setBounds(metersSection.removeFromLeft(60));

  // --- Large Rotary Knobs (130px) ---
  bounds.removeFromTop(10);
  auto largeKnobRow = bounds.removeFromTop(130);
  halfW = largeKnobRow.getWidth() / 2;
  {
    auto col = largeKnobRow.removeFromLeft(halfW);
    blendLabel_.setBounds(col.removeFromTop(18));
    blendSlider_.setBounds(col.withSizeKeepingCentre(100, 112));
  }
  {
    auto col = largeKnobRow;
    outputGainLabel_.setBounds(col.removeFromTop(18));
    outputGainSlider_.setBounds(col.withSizeKeepingCentre(100, 112));
  }

  // --- Small Rotary Knobs Row 1: Threshold, Range, Knee (90px) ---
  bounds.removeFromTop(5);
  auto smallRow1 = bounds.removeFromTop(90);
  auto colW = smallRow1.getWidth() / 3;
  {
    auto col = smallRow1.removeFromLeft(colW);
    thresholdLabel_.setBounds(col.removeFromTop(16));
    thresholdSlider_.setBounds(col.withSizeKeepingCentre(72, 74));
  }
  {
    auto col = smallRow1.removeFromLeft(colW);
    rangeDbLabel_.setBounds(col.removeFromTop(16));
    rangeDbSlider_.setBounds(col.withSizeKeepingCentre(72, 74));
  }
  {
    auto col = smallRow1;
    kneeWidthDbLabel_.setBounds(col.removeFromTop(16));
    kneeWidthDbSlider_.setBounds(col.withSizeKeepingCentre(72, 74));
  }

  // --- Small Rotary Knobs Row 2: Attack, Release + Detection ComboBox (90px) ---
  auto smallRow2 = bounds.removeFromTop(90);
  colW = smallRow2.getWidth() / 3;
  {
    auto col = smallRow2.removeFromLeft(colW);
    attackTimeLabel_.setBounds(col.removeFromTop(16));
    attackTimeSlider_.setBounds(col.withSizeKeepingCentre(72, 74));
  }
  {
    auto col = smallRow2.removeFromLeft(colW);
    releaseTimeLabel_.setBounds(col.removeFromTop(16));
    releaseTimeSlider_.setBounds(col.withSizeKeepingCentre(72, 74));
  }
  {
    auto col = smallRow2;
    detectionModeLabel_.setBounds(col.removeFromTop(16));
    detectionModeCombo_.setBounds(col.withSizeKeepingCentre(130, 28));
  }

  // --- Latency Label ---
  bounds.removeFromTop(5);
  latencyLabel_.setBounds(bounds.removeFromTop(25));
}

void OctobIREditor::timerCallback()
{
  updateMeters();
}

void OctobIREditor::updateMeters()
{
  inputLevelMeter_.setValue(audioProcessor.getCurrentInputLevel());
  blendMeter_.setValue(audioProcessor.getCurrentBlend());

  bool dynamicMode = audioProcessor.getAPVTS().getRawParameterValue("dynamicMode")->load() > 0.5f;

  inputLevelMeter_.setShowThresholds(dynamicMode);
  blendMeter_.setShowBlendRange(dynamicMode);

  if (dynamicMode)
  {
    float threshold = audioProcessor.getAPVTS().getRawParameterValue("threshold")->load();
    float rangeDb = audioProcessor.getAPVTS().getRawParameterValue("rangeDb")->load();
    inputLevelMeter_.setThresholdMarkers(threshold, threshold + rangeDb);
    blendMeter_.setBlendRangeMarkers(-1.0f, 1.0f);
  }
}

void OctobIREditor::loadButton1Clicked()
{
  auto chooser = std::make_shared<juce::FileChooser>("Select impulse response WAV file for IR 1",
                                                     getLastBrowsedDirectory(), "*.wav");

  auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

  chooser->launchAsync(
      flags,
      [this, chooser](const juce::FileChooser& fc)
      {
        auto file = fc.getResult();
        if (file.existsAsFile())
        {
          updateLastBrowsedDirectory(file);
          juce::String error;
          bool success = audioProcessor.loadImpulseResponse1(file.getFullPathName(), error);
          if (success)
          {
            ir1LCDDisplay_.setText(file.getFileName());
            updateLatencyDisplay();
          }
          else
          {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                   "Failed to Load IR 1", error, "OK");
            ir1LCDDisplay_.setText("Failed to load IR");
          }
        }
      });
}

void OctobIREditor::clearButton1Clicked()
{
  audioProcessor.clearImpulseResponse1();
  ir1LCDDisplay_.setText("No IR loaded");
  updateLatencyDisplay();
}

void OctobIREditor::loadButton2Clicked()
{
  auto chooser = std::make_shared<juce::FileChooser>("Select impulse response WAV file for IR 2",
                                                     getLastBrowsedDirectory(), "*.wav");

  auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

  chooser->launchAsync(
      flags,
      [this, chooser](const juce::FileChooser& fc)
      {
        auto file = fc.getResult();
        if (file.existsAsFile())
        {
          updateLastBrowsedDirectory(file);
          juce::String error;
          bool success = audioProcessor.loadImpulseResponse2(file.getFullPathName(), error);
          if (success)
          {
            ir2LCDDisplay_.setText(file.getFileName());
          }
          else
          {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                   "Failed to Load IR 2", error, "OK");
            ir2LCDDisplay_.setText("Failed to load IR");
          }
        }
      });
}

void OctobIREditor::clearButton2Clicked()
{
  audioProcessor.clearImpulseResponse2();
  ir2LCDDisplay_.setText("No IR loaded");
}

void OctobIREditor::swapIROrderClicked()
{
  auto& apvts = audioProcessor.getAPVTS();

  float currentBlend = apvts.getRawParameterValue("blend")->load();
  float currentLowBlend = apvts.getRawParameterValue("lowBlend")->load();
  float currentHighBlend = apvts.getRawParameterValue("highBlend")->load();

  if (auto* blendParam = apvts.getParameter("blend"))
  {
    blendParam->setValueNotifyingHost(blendParam->convertTo0to1(-currentBlend));
  }

  if (auto* lowParam = apvts.getParameter("lowBlend"))
  {
    lowParam->setValueNotifyingHost(lowParam->convertTo0to1(currentHighBlend));
  }

  if (auto* highParam = apvts.getParameter("highBlend"))
  {
    highParam->setValueNotifyingHost(highParam->convertTo0to1(currentLowBlend));
  }
}

void OctobIREditor::updateLatencyDisplay()
{
  int latency = audioProcessor.getLatencySamples();
  double sampleRate = audioProcessor.getSampleRate();
  if (sampleRate > 0)
  {
    double latencyMs = (latency / sampleRate) * 1000.0;
    latencyLabel_.setText(juce::String("Latency: ") + juce::String(latency) + " samples (" +
                              juce::String(latencyMs, 2) + " ms)",
                          juce::dontSendNotification);
  }
  else
  {
    latencyLabel_.setText(juce::String("Latency: ") + juce::String(latency) + " samples",
                          juce::dontSendNotification);
  }
}

void OctobIREditor::prevButton1Clicked()
{
  cycleIRFile(1, -1);
}

void OctobIREditor::nextButton1Clicked()
{
  cycleIRFile(1, 1);
}

void OctobIREditor::prevButton2Clicked()
{
  cycleIRFile(2, -1);
}

void OctobIREditor::nextButton2Clicked()
{
  cycleIRFile(2, 1);
}

void OctobIREditor::cycleIRFile(int irIndex, int direction)
{
  juce::String currentPath =
      irIndex == 1 ? audioProcessor.getCurrentIR1Path() : audioProcessor.getCurrentIR2Path();

  if (currentPath.isEmpty())
    return;

  juce::File currentFile(currentPath);
  if (!currentFile.existsAsFile())
    return;

  juce::File directory = currentFile.getParentDirectory();
  juce::Array<juce::File> wavFiles;

  for (const auto& entry : juce::RangedDirectoryIterator(
           directory, false, "*.wav", juce::File::findFiles | juce::File::ignoreHiddenFiles))
  {
    wavFiles.add(entry.getFile());
  }

  if (wavFiles.isEmpty())
    return;

  wavFiles.sort();

  int currentIndex = wavFiles.indexOf(currentFile);
  if (currentIndex < 0)
    return;

  int newIndex = currentIndex + direction;
  if (newIndex < 0)
    newIndex = wavFiles.size() - 1;
  else if (newIndex >= wavFiles.size())
    newIndex = 0;

  juce::File newFile = wavFiles[newIndex];
  juce::String error;
  bool success;

  if (irIndex == 1)
  {
    success = audioProcessor.loadImpulseResponse1(newFile.getFullPathName(), error);
    if (success)
    {
      ir1LCDDisplay_.setText(newFile.getFileName());
      updateLatencyDisplay();
    }
    else
    {
      juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Failed to Load IR 1",
                                             error, "OK");
      ir1LCDDisplay_.setText("Failed to load IR");
    }
  }
  else
  {
    success = audioProcessor.loadImpulseResponse2(newFile.getFullPathName(), error);
    if (success)
    {
      ir2LCDDisplay_.setText(newFile.getFileName());
    }
    else
    {
      juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Failed to Load IR 2",
                                             error, "OK");
      ir2LCDDisplay_.setText("Failed to load IR");
    }
  }
}

juce::File OctobIREditor::getLastBrowsedDirectory() const
{
  if (lastBrowsedDirectory_.exists() && lastBrowsedDirectory_.isDirectory())
    return lastBrowsedDirectory_;

  juce::String ir1Path = audioProcessor.getCurrentIR1Path();
  if (ir1Path.isNotEmpty())
  {
    juce::File ir1File(ir1Path);
    if (ir1File.existsAsFile())
      return ir1File.getParentDirectory();
  }

  juce::String ir2Path = audioProcessor.getCurrentIR2Path();
  if (ir2Path.isNotEmpty())
  {
    juce::File ir2File(ir2Path);
    if (ir2File.existsAsFile())
      return ir2File.getParentDirectory();
  }

  return juce::File::getSpecialLocation(juce::File::userHomeDirectory);
}

void OctobIREditor::updateLastBrowsedDirectory(const juce::File& file)
{
  if (file.existsAsFile())
    lastBrowsedDirectory_ = file.getParentDirectory();
}
