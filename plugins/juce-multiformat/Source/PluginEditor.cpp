#include "PluginEditor.h"

#include "OctobIRLookAndFeel.h"
#include "PluginProcessor.h"

LCDBargraph::LCDBargraph(const juce::String& name, float minValue, float maxValue,
                         bool centreBalanced)
    : name_(name), minValue_(minValue), maxValue_(maxValue), centreBalanced_(centreBalanced)
{
}

void LCDBargraph::setValue(float value)
{
  currentValue_ = value;
  repaint();
}

void LCDBargraph::setThresholdMarkers(float low, float high)
{
  lowThreshold_ = low;
  highThreshold_ = high;
}

void LCDBargraph::paint(juce::Graphics& g)
{
  auto bounds = getLocalBounds();

  auto labelBounds = bounds.removeFromTop(18);
  g.setColour(juce::Colour(0xff1a1a1a));
  if (auto* laf = dynamic_cast<OctobIRLookAndFeel*>(&getLookAndFeel()))
    g.setFont(
        juce::Font(juce::FontOptions().withTypeface(laf->getMainTypeface()).withHeight(12.0f)));
  else
    g.setFont(juce::FontOptions(12.0f));
  g.drawText(name_, labelBounds, juce::Justification::centredLeft, true);

  bounds.removeFromTop(4);
  auto segmentArea = bounds.removeFromTop(18);

  g.setColour(juce::Colour(0xffF08830));
  g.fillRect(segmentArea);
  g.setColour(juce::Colour(0xff1a1a1a));
  g.drawRect(segmentArea, 1);

  auto innerArea = segmentArea.reduced(1, 1);
  float normValue = (currentValue_ - minValue_) / (maxValue_ - minValue_);
  normValue = std::max(0.0f, std::min(1.0f, normValue));

  float totalGapWidth = static_cast<float>((numSegments_ - 1) * segmentGap_);
  float segW =
      (static_cast<float>(innerArea.getWidth()) - totalGapWidth) / static_cast<float>(numSegments_);
  float segH = static_cast<float>(innerArea.getHeight());

  for (int i = 0; i < numSegments_; ++i)
  {
    float x = static_cast<float>(innerArea.getX()) +
              static_cast<float>(i) * (segW + static_cast<float>(segmentGap_));
    juce::Rectangle<float> seg(x, static_cast<float>(innerArea.getY()), segW, segH);

    bool isClipZone =
        !centreBalanced_ && (static_cast<float>(i) / static_cast<float>(numSegments_)) >= 0.85f;

    bool isLit = false;
    if (centreBalanced_)
    {
      int centre = numSegments_ / 2;
      float normPos = normValue * static_cast<float>(numSegments_);
      if (normValue < 0.5f)
        isLit = (static_cast<float>(i) >= normPos && i < centre);
      else
        isLit = (i >= centre && static_cast<float>(i) < normPos);
    }
    else
    {
      isLit = static_cast<float>(i) < normValue * static_cast<float>(numSegments_);
    }

    if (isLit)
    {
      g.setColour(juce::Colour(0xff1e1a06));
      g.fillRect(seg);
    }
  }

  auto innerF = innerArea.toFloat();
  juce::ColourGradient topShadow(juce::Colour(0xff000000).withAlpha(0.22f), innerF.getX(),
                                 innerF.getY(), juce::Colour(0xff000000).withAlpha(0.0f),
                                 innerF.getX(), innerF.getY() + 5.0f, false);
  g.setGradientFill(topShadow);
  g.fillRect(innerF);
  juce::ColourGradient leftShadow(juce::Colour(0xff000000).withAlpha(0.12f), innerF.getX(),
                                  innerF.getY(), juce::Colour(0xff000000).withAlpha(0.0f),
                                  innerF.getX() + 5.0f, innerF.getY(), false);
  g.setGradientFill(leftShadow);
  g.fillRect(innerF);

  if (showThresholds_)
  {
    auto drawMarker = [&](float threshold)
    {
      float normT = (threshold - minValue_) / (maxValue_ - minValue_);
      normT = std::max(0.0f, std::min(1.0f, normT));
      float x =
          static_cast<float>(innerArea.getX()) + normT * static_cast<float>(innerArea.getWidth());
      g.setColour(juce::Colour(0xffffffff).withAlpha(0.8f));
      g.drawLine(x, static_cast<float>(segmentArea.getY()), x,
                 static_cast<float>(segmentArea.getBottom()), 3.0f);
    };
    drawMarker(lowThreshold_);
    drawMarker(highThreshold_);
  }
}

static void setupRotarySlider(juce::Slider& s, int textBoxWidth = 90)
{
  s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, textBoxWidth, 22);
  s.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                        juce::MathConstants<float>::pi * 2.75f, true);
}

OctobIREditor::OctobIREditor(OctobIRProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      inputLevelMeter_("INPUT", -96.0f, 0.0f, false),
      blendMeter_("BLEND", -1.0f, 1.0f, true)
{
  setLookAndFeel(&laf_);

  if (auto typeface = laf_.getLCDTypeface())
  {
    ir1LCDDisplay_.setTypeface(typeface);
    ir2LCDDisplay_.setTypeface(typeface);
  }

  addAndMakeVisible(loadButton1_);
  loadButton1_.setButtonText("LOAD");
  loadButton1_.onClick = [this] { loadButton1Clicked(); };

  addAndMakeVisible(clearButton1_);
  clearButton1_.setButtonText("CLEAR");
  clearButton1_.onClick = [this] { clearButton1Clicked(); };

  addAndMakeVisible(prevButton1_);
  prevButton1_.setButtonText("<");
  prevButton1_.onClick = [this] { prevButton1Clicked(); };

  addAndMakeVisible(nextButton1_);
  nextButton1_.setButtonText(">");
  nextButton1_.onClick = [this] { nextButton1Clicked(); };

  addAndMakeVisible(ir1LCDDisplay_);
  ir1LCDDisplay_.setTextColour(juce::Colour(0xff1e1a06));
  ir1LCDDisplay_.setText(audioProcessor.getCurrentIR1Path().isEmpty()
                             ? "No IR loaded"
                             : juce::File(audioProcessor.getCurrentIR1Path()).getFileName());

  addAndMakeVisible(ir1EnableButton_);
  ir1EnableButton_.setButtonText("ENABLE");
  ir1EnableButton_.getProperties().set("isLED", true);
  ir1EnableButton_.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffe07030));
  ir1EnableAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
      audioProcessor.getAPVTS(), "irAEnable", ir1EnableButton_);

  addAndMakeVisible(loadButton2_);
  loadButton2_.setButtonText("LOAD");
  loadButton2_.onClick = [this] { loadButton2Clicked(); };

  addAndMakeVisible(clearButton2_);
  clearButton2_.setButtonText("CLEAR");
  clearButton2_.onClick = [this] { clearButton2Clicked(); };

  addAndMakeVisible(prevButton2_);
  prevButton2_.setButtonText("<");
  prevButton2_.onClick = [this] { prevButton2Clicked(); };

  addAndMakeVisible(nextButton2_);
  nextButton2_.setButtonText(">");
  nextButton2_.onClick = [this] { nextButton2Clicked(); };

  addAndMakeVisible(ir2LCDDisplay_);
  ir2LCDDisplay_.setTextColour(juce::Colour(0xff1e1a06));
  ir2LCDDisplay_.setText(audioProcessor.getCurrentIR2Path().isEmpty()
                             ? "No IR loaded"
                             : juce::File(audioProcessor.getCurrentIR2Path()).getFileName());

  addAndMakeVisible(ir2EnableButton_);
  ir2EnableButton_.setButtonText("ENABLE");
  ir2EnableButton_.getProperties().set("isLED", true);
  ir2EnableButton_.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffe07030));
  ir2EnableAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
      audioProcessor.getAPVTS(), "irBEnable", ir2EnableButton_);

  addAndMakeVisible(inputLevelMeter_);
  addAndMakeVisible(blendMeter_);

  addAndMakeVisible(dynamicModeButton_);
  dynamicModeButton_.setButtonText("DYNAMIC");
  dynamicModeAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
      audioProcessor.getAPVTS(), "dynamicMode", dynamicModeButton_);

  addAndMakeVisible(sidechainEnableButton_);
  sidechainEnableButton_.setButtonText("SIDECHAIN");
  sidechainEnableAttachment_ =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          audioProcessor.getAPVTS(), "sidechainEnable", sidechainEnableButton_);

  addAndMakeVisible(swapIROrderButton_);
  swapIROrderButton_.setButtonText("SWAP");
  swapIROrderButton_.onClick = [this] { swapIROrderClicked(); };

  addAndMakeVisible(blendLabel_);
  blendLabel_.setText("STATIC BLEND", juce::dontSendNotification);
  blendLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(blendSlider_);
  setupRotarySlider(blendSlider_);
  blendAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      audioProcessor.getAPVTS(), "blend", blendSlider_);

  addAndMakeVisible(outputGainLabel_);
  outputGainLabel_.setText("OUTPUT GAIN", juce::dontSendNotification);
  outputGainLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(outputGainSlider_);
  setupRotarySlider(outputGainSlider_);
  outputGainAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      audioProcessor.getAPVTS(), "outputGain", outputGainSlider_);

  addAndMakeVisible(thresholdLabel_);
  thresholdLabel_.setText("THRESHOLD", juce::dontSendNotification);
  thresholdLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(thresholdSlider_);
  setupRotarySlider(thresholdSlider_, 80);
  thresholdAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      audioProcessor.getAPVTS(), "threshold", thresholdSlider_);

  addAndMakeVisible(rangeDbLabel_);
  rangeDbLabel_.setText("RANGE", juce::dontSendNotification);
  rangeDbLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(rangeDbSlider_);
  setupRotarySlider(rangeDbSlider_, 80);
  rangeDbAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      audioProcessor.getAPVTS(), "rangeDb", rangeDbSlider_);

  addAndMakeVisible(kneeWidthDbLabel_);
  kneeWidthDbLabel_.setText("KNEE", juce::dontSendNotification);
  kneeWidthDbLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(kneeWidthDbSlider_);
  setupRotarySlider(kneeWidthDbSlider_, 80);
  kneeWidthDbAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      audioProcessor.getAPVTS(), "kneeWidthDb", kneeWidthDbSlider_);

  addAndMakeVisible(attackTimeLabel_);
  attackTimeLabel_.setText("ATTACK", juce::dontSendNotification);
  attackTimeLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(attackTimeSlider_);
  setupRotarySlider(attackTimeSlider_, 80);
  attackTimeAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      audioProcessor.getAPVTS(), "attackTime", attackTimeSlider_);

  addAndMakeVisible(releaseTimeLabel_);
  releaseTimeLabel_.setText("RELEASE", juce::dontSendNotification);
  releaseTimeLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(releaseTimeSlider_);
  setupRotarySlider(releaseTimeSlider_, 80);
  releaseTimeAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      audioProcessor.getAPVTS(), "releaseTime", releaseTimeSlider_);

  addAndMakeVisible(detectionModeLabel_);
  detectionModeLabel_.setText("DETECTION", juce::dontSendNotification);
  detectionModeLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(detectionModeCombo_);
  detectionModeCombo_.addItem("PEAK", 1);
  detectionModeCombo_.addItem("RMS", 2);
  detectionModeAttachment_ =
      std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
          audioProcessor.getAPVTS(), "detectionMode", detectionModeCombo_);

  startTimerHz(30);
  setSize(520, 630);
}

OctobIREditor::~OctobIREditor()
{
  stopTimer();
  setLookAndFeel(nullptr);
}

void OctobIREditor::paint(juce::Graphics& g)
{
  g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void OctobIREditor::resized()
{
  auto bounds = getLocalBounds().reduced(15);
  bounds.removeFromTop(10);

  // --- IR Loading Section (110px) ---
  auto irSection = bounds.removeFromTop(110);

  auto irButtonRow = irSection.removeFromTop(30);
  auto halfW = irButtonRow.getWidth() / 2;

  {
    auto col = irButtonRow.removeFromLeft(halfW);
    loadButton1_.setBounds(col.removeFromLeft(55).reduced(2));
    clearButton1_.setBounds(col.removeFromLeft(48).reduced(2));
    prevButton1_.setBounds(col.removeFromLeft(28).reduced(2));
    nextButton1_.setBounds(col.removeFromLeft(28).reduced(2));
    ir1EnableButton_.setBounds(col.reduced(2));
  }
  {
    auto col = irButtonRow;
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
  auto modeColW = modeRow.getWidth() / 3;
  dynamicModeButton_.setBounds(modeRow.removeFromLeft(modeColW).reduced(2));
  swapIROrderButton_.setBounds(modeRow.removeFromLeft(modeColW).reduced(2));
  sidechainEnableButton_.setBounds(modeRow.reduced(2));

  // --- Meters (96px) ---
  bounds.removeFromTop(10);
  auto inputMeterBounds = bounds.removeFromTop(40);
  bounds.removeFromTop(6);
  auto blendMeterBounds = bounds.removeFromTop(40);
  inputLevelMeter_.setBounds(inputMeterBounds);
  blendMeter_.setBounds(blendMeterBounds);

  // --- Large Rotary Knobs (110px) ---
  bounds.removeFromTop(10);
  auto largeKnobRow = bounds.removeFromTop(110);
  halfW = largeKnobRow.getWidth() / 2;
  {
    auto col = largeKnobRow.removeFromLeft(halfW);
    blendLabel_.setBounds(col.removeFromTop(18));
    blendSlider_.setBounds(col.withSizeKeepingCentre(86, 90));
  }
  {
    auto col = largeKnobRow;
    outputGainLabel_.setBounds(col.removeFromTop(18));
    outputGainSlider_.setBounds(col.withSizeKeepingCentre(86, 90));
  }

  // --- Small Rotary Knobs Row 1: Threshold, Range, Knee (110px) ---
  bounds.removeFromTop(20);
  auto smallRow1 = bounds.removeFromTop(110);
  auto colW = smallRow1.getWidth() / 3;
  {
    auto col = smallRow1.removeFromLeft(colW);
    thresholdLabel_.setBounds(col.removeFromTop(16));
    thresholdSlider_.setBounds(col.withSizeKeepingCentre(86, 90));
  }
  {
    auto col = smallRow1.removeFromLeft(colW);
    rangeDbLabel_.setBounds(col.removeFromTop(16));
    rangeDbSlider_.setBounds(col.withSizeKeepingCentre(86, 90));
  }
  {
    auto col = smallRow1;
    kneeWidthDbLabel_.setBounds(col.removeFromTop(16));
    kneeWidthDbSlider_.setBounds(col.withSizeKeepingCentre(86, 90));
  }

  // --- Small Rotary Knobs Row 2: Attack, Release + Detection ComboBox (110px) ---
  bounds.removeFromTop(20);
  auto smallRow2 = bounds.removeFromTop(110);
  colW = smallRow2.getWidth() / 3;
  {
    auto col = smallRow2.removeFromLeft(colW);
    attackTimeLabel_.setBounds(col.removeFromTop(16));
    attackTimeSlider_.setBounds(col.withSizeKeepingCentre(86, 90));
  }
  {
    auto col = smallRow2.removeFromLeft(colW);
    releaseTimeLabel_.setBounds(col.removeFromTop(16));
    releaseTimeSlider_.setBounds(col.withSizeKeepingCentre(86, 90));
  }
  {
    auto col = smallRow2;
    detectionModeLabel_.setBounds(col.removeFromTop(16));
    detectionModeCombo_.setBounds(col.withSizeKeepingCentre(130, 28));
  }
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

  if (dynamicMode)
  {
    float threshold = audioProcessor.getAPVTS().getRawParameterValue("threshold")->load();
    float rangeDb = audioProcessor.getAPVTS().getRawParameterValue("rangeDb")->load();
    inputLevelMeter_.setThresholdMarkers(threshold, threshold + rangeDb);
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
