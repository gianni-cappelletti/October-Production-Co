#include "PluginEditor.h"

#include <BinaryData.h>

#include "PanelPainting.h"

static void setupRotarySlider(juce::Slider& s, int textBoxWidth = 90)
{
  s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, textBoxWidth, 22);
  s.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                        juce::MathConstants<float>::pi * 2.75f, true);
}

static void setupTrimSlider(juce::Slider& s)
{
  s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
  s.setPopupDisplayEnabled(true, true, nullptr);
  s.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                        juce::MathConstants<float>::pi * 2.75f, true);
}

// Diagonal line parameters: both lines lean / (upper-right to lower-left)
// x(y) = topX - shift * (y / height)
static constexpr float kLineShift = 86.4f;
static constexpr float kLeftLineTopX = 294.4f;
static constexpr float kRightLineTopX = 434.4f;

static float lineXAtY(float topX, float y)
{
  return topX - kLineShift * (y / static_cast<float>(OctoBassEditor::kDesignHeight));
}

OctoBassEditor::OctoBassEditor(OctoBassProcessor& p) : AudioProcessorEditor(&p), audioProcessor(p)
{
  setLookAndFeel(&laf_);

  if (auto typeface = laf_.getLCDTypeface())
  {
    graphicEQDisplay_.setTypeface(typeface);
    for (auto& controls : namSlots_)
      controls.lcdDisplay.setTypeface(typeface);
    irLCDDisplay_.setTypeface(typeface);
  }

  // Spectrum analyzer + EQ display
  for (int i = 0; i < octob::kGraphicEQNumNodes; ++i)
  {
    auto idx = static_cast<size_t>(i);
    auto slotStr = juce::String(i);
    eqNodeParams_[idx].active = paramHandle("eqNodeActive" + slotStr);
    eqNodeParams_[idx].freq = paramHandle("eqNodeFreq" + slotStr);
    eqNodeParams_[idx].gain = paramHandle("eqNodeGain" + slotStr);
  }
  eqLowCutActiveParam_ = paramHandle("eqLowCutActive");
  eqLowCutFreqParam_ = paramHandle("eqLowCutFreq");
  eqHighCutActiveParam_ = paramHandle("eqHighCutActive");
  eqHighCutFreqParam_ = paramHandle("eqHighCutFreq");
  crossoverParam_ = paramHandle("crossoverFrequency");

  addAndMakeVisible(graphicEQDisplay_);
  graphicEQDisplay_.setSpectrumDbRange(SpectrumAnalyzer::kMinDb, SpectrumAnalyzer::kMaxDb);
  graphicEQDisplay_.onNodeChanged = [this](int slot, bool active, float freqHz, float gainDb)
  {
    if (slot < 0 || slot >= octob::kGraphicEQNumNodes)
      return;
    const auto& node = eqNodeParams_[static_cast<size_t>(slot)];
    if (node.active.param != nullptr)
      node.active.param->setValueNotifyingHost(active ? 1.0f : 0.0f);
    if (node.freq.param != nullptr)
      node.freq.param->setValueNotifyingHost(node.freq.param->convertTo0to1(freqHz));
    if (node.gain.param != nullptr)
      node.gain.param->setValueNotifyingHost(node.gain.param->convertTo0to1(gainDb));
  };
  graphicEQDisplay_.onLowCutChanged = [this](bool active, float freqHz)
  {
    if (eqLowCutActiveParam_.param != nullptr)
      eqLowCutActiveParam_.param->setValueNotifyingHost(active ? 1.0f : 0.0f);
    if (eqLowCutFreqParam_.param != nullptr)
      eqLowCutFreqParam_.param->setValueNotifyingHost(
          eqLowCutFreqParam_.param->convertTo0to1(freqHz));
  };
  graphicEQDisplay_.onHighCutChanged = [this](bool active, float freqHz)
  {
    if (eqHighCutActiveParam_.param != nullptr)
      eqHighCutActiveParam_.param->setValueNotifyingHost(active ? 1.0f : 0.0f);
    if (eqHighCutFreqParam_.param != nullptr)
      eqHighCutFreqParam_.param->setValueNotifyingHost(
          eqHighCutFreqParam_.param->convertTo0to1(freqHz));
  };

  // Bracket drags so hosts record EQ moves as automation gestures
  graphicEQDisplay_.onDragStart = [this](int handle, GraphicEQDisplay::GestureScope scope)
  { forEachHandleParam(handle, scope, &juce::RangedAudioParameter::beginChangeGesture); };
  graphicEQDisplay_.onDragEnd = [this](int handle, GraphicEQDisplay::GestureScope scope)
  { forEachHandleParam(handle, scope, &juce::RangedAudioParameter::endChangeGesture); };

  spectrumAnalyzer_.setSampleRate(audioProcessor.getSampleRate());
  lastSampleRate_ = audioProcessor.getSampleRate();
  startTimerHz(30);

  // --- LOW zone: Squash ---
  addAndMakeVisible(squashLabel_);
  squashLabel_.setText("SQUASH", juce::dontSendNotification);
  squashLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(squashSlider_);
  setupRotarySlider(squashSlider_);
  squashAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      audioProcessor.getAPVTS(), "squash", squashSlider_);

  // --- LOW zone: Compression Mode ---
  addAndMakeVisible(compressionModeLabel_);
  compressionModeLabel_.setText("MODE", juce::dontSendNotification);
  compressionModeLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(compressionModeSlider_);
  setupRotarySlider(compressionModeSlider_);
  compressionModeSlider_.setRotaryParameters(juce::MathConstants<float>::pi * 1.5f,
                                             juce::MathConstants<float>::pi * 2.5f, true);
  compressionModeSlider_.getProperties().set("numTicks", 4);
  compressionModeAttachment_ =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          audioProcessor.getAPVTS(), "compressionMode", compressionModeSlider_);

  compressionModeSlider_.textFromValueFunction = [](double value) -> juce::String
  {
    static const juce::StringArray modes{"Tight", "Smooth", "Punch", "Glue"};
    return modes[juce::jlimit(0, 3, static_cast<int>(std::round(value)))];
  };
  compressionModeSlider_.valueFromTextFunction = [](const juce::String& text) -> double
  {
    static const juce::StringArray modes{"Tight", "Smooth", "Punch", "Glue"};
    int idx = modes.indexOf(text, true);
    return idx >= 0 ? static_cast<double>(idx) : 0.0;
  };
  compressionModeSlider_.updateText();

  // --- LOW zone: Low Band Level ---
  addAndMakeVisible(lowBandLevelLabel_);
  lowBandLevelLabel_.setText("OUTPUT", juce::dontSendNotification);
  lowBandLevelLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(lowBandLevelSlider_);
  setupRotarySlider(lowBandLevelSlider_);
  lowBandLevelAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      audioProcessor.getAPVTS(), "lowBandLevel", lowBandLevelSlider_);

  // --- LOW zone: Low Band Solo ---
  addAndMakeVisible(lowBandSoloButton_);
  lowBandSoloButton_.setPaintingIsUnclipped(true);
  lowBandSoloButton_.setButtonText("S");
  lowBandSoloButton_.setTitle("Low Band Solo");
  lowBandSoloAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
      audioProcessor.getAPVTS(), "lowBandSolo", lowBandSoloButton_);

  // --- CENTER zone: Crossover ---
  addAndMakeVisible(crossoverLabel_);
  crossoverLabel_.setText("CROSSOVER", juce::dontSendNotification);
  crossoverLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(crossoverSlider_);
  setupRotarySlider(crossoverSlider_);
  crossoverAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      audioProcessor.getAPVTS(), "crossoverFrequency", crossoverSlider_);

  // --- CENTER zone: Gate ---
  addAndMakeVisible(gateLabel_);
  gateLabel_.setText("GATE", juce::dontSendNotification);
  gateLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(gateSlider_);
  setupRotarySlider(gateSlider_);
  gateAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      audioProcessor.getAPVTS(), "gateThreshold", gateSlider_);

  // --- HIGH zone: High Input Gain ---
  addAndMakeVisible(highInputGainLabel_);
  highInputGainLabel_.setText("INPUT", juce::dontSendNotification);
  highInputGainLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(highInputGainSlider_);
  setupTrimSlider(highInputGainSlider_);
  highInputGainAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      audioProcessor.getAPVTS(), "highInputGain", highInputGainSlider_);

  // --- HIGH zone: High Output Gain ---
  addAndMakeVisible(highOutputGainLabel_);
  highOutputGainLabel_.setText("OUTPUT", juce::dontSendNotification);
  highOutputGainLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(highOutputGainSlider_);
  setupTrimSlider(highOutputGainSlider_);
  highOutputGainAttachment_ =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          audioProcessor.getAPVTS(), "highOutputGain", highOutputGainSlider_);

  // --- HIGH zone: High Band Solo ---
  addAndMakeVisible(highBandSoloButton_);
  highBandSoloButton_.setPaintingIsUnclipped(true);
  highBandSoloButton_.setButtonText("S");
  highBandSoloButton_.setTitle("High Band Solo");
  highBandSoloAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
      audioProcessor.getAPVTS(), "highBandSolo", highBandSoloButton_);

  // --- HIGH zone: High Band Mix (trim knob) ---
  addAndMakeVisible(highBandMixLabel_);
  highBandMixLabel_.setText("MIX", juce::dontSendNotification);
  highBandMixLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(highBandMixSlider_);
  setupTrimSlider(highBandMixSlider_);
  highBandMixAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      audioProcessor.getAPVTS(), "highBandMix", highBandMixSlider_);

  for (const auto slot : octob::kNamSlots)
    setUpNamSlot(slot);

  // --- IR file loader ---
  addAndMakeVisible(irLoadButton_);
  irLoadButton_.setPaintingIsUnclipped(true);
  irLoadButton_.setButtonText("");
  irLoadButton_.setTitle("Load IR");
  irLoadButton_.setComponentID("loadButton");
  irLoadButton_.onClick = [this] { irLoadClicked(); };

  addAndMakeVisible(irClearButton_);
  irClearButton_.setPaintingIsUnclipped(true);
  irClearButton_.setButtonText("CLEAR");
  irClearButton_.setTitle("Clear IR");
  irClearButton_.onClick = [this] { irClearClicked(); };

  addAndMakeVisible(irPrevButton_);
  irPrevButton_.setPaintingIsUnclipped(true);
  irPrevButton_.setButtonText("<");
  irPrevButton_.setTitle("Previous IR");
  irPrevButton_.onClick = [this] { cycleIRFile(-1); };

  addAndMakeVisible(irNextButton_);
  irNextButton_.setPaintingIsUnclipped(true);
  irNextButton_.setButtonText(">");
  irNextButton_.setTitle("Next IR");
  irNextButton_.onClick = [this] { cycleIRFile(1); };

  addAndMakeVisible(irLCDDisplay_);
  irLCDDisplay_.setTextColour(kLCDInkColour);
  irLCDDisplay_.setOnClick([this] { irLoadClicked(); });
  showIRPath(audioProcessor.getCurrentIRPath());

  logoImage_ =
      juce::ImageCache::getFromMemory(BinaryData::OctoberLogo_png, BinaryData::OctoberLogo_pngSize);

  for (auto& controls : namSlots_)
    addChildComponent(*controls.calibrationPanel);

  setResizable(true, true);
  getConstrainer()->setFixedAspectRatio(static_cast<double>(kDesignWidth) /
                                        static_cast<double>(kDesignHeight));
  setSize(kDesignWidth, kDesignHeight);
  setResizeLimits(kDesignWidth * 3 / 4, kDesignHeight * 3 / 4, kDesignWidth * 3 / 2,
                  kDesignHeight * 3 / 2);
}

OctoBassEditor::~OctoBassEditor()
{
  stopTimer();
  setLookAndFeel(nullptr);
}

void OctoBassEditor::timerCallback()
{
  double currentRate = audioProcessor.getSampleRate();
  if (currentRate > 0.0 && std::abs(currentRate - lastSampleRate_) > 1.0)
  {
    spectrumAnalyzer_.setSampleRate(currentRate);
    graphicEQDisplay_.setSampleRate(currentRate);
    lastSampleRate_ = currentRate;
  }

  spectrumAnalyzer_.processFromFifo(audioProcessor.getSpectrumFifo(),
                                    audioProcessor.getSpectrumFifoBuffer().data());

  graphicEQDisplay_.setBandLevels(spectrumAnalyzer_.getBandLevels().data(),
                                  SpectrumAnalyzer::kNumBands);

  graphicEQDisplay_.setCrossoverNormPosition(
      LCDSpectrumDisplay::freqToNormX(crossoverParam_.value->load()));

  for (int i = 0; i < octob::kGraphicEQNumNodes; ++i)
  {
    const auto& node = eqNodeParams_[static_cast<size_t>(i)];
    graphicEQDisplay_.setNode(i, node.active.value->load() >= 0.5f, node.freq.value->load(),
                              node.gain.value->load());
  }

  graphicEQDisplay_.setLowCut(eqLowCutActiveParam_.value->load() >= 0.5f,
                              eqLowCutFreqParam_.value->load());
  graphicEQDisplay_.setHighCut(eqHighCutActiveParam_.value->load() >= 0.5f,
                               eqHighCutFreqParam_.value->load());

  for (const auto slot : octob::kNamSlots)
  {
    updateNamQualityToggle(slot);
    updateNamCalibrationControls(slot);
  }
  syncLoadedFileDisplays();
}

void OctoBassEditor::setUpNamSlot(octob::NamSlot slot)
{
  auto& controls = namControls(slot);
  const auto slotName = OctoBassProcessor::getNamSlotName(slot);

  addAndMakeVisible(controls.loadButton);
  controls.loadButton.setPaintingIsUnclipped(true);
  controls.loadButton.setButtonText("");
  controls.loadButton.setTitle("Load " + slotName + " Model");
  controls.loadButton.setComponentID("loadButton");
  controls.loadButton.onClick = [this, slot] { namLoadClicked(slot); };

  addAndMakeVisible(controls.clearButton);
  controls.clearButton.setPaintingIsUnclipped(true);
  controls.clearButton.setButtonText("CLEAR");
  controls.clearButton.setTitle("Clear " + slotName + " Model");
  controls.clearButton.onClick = [this, slot] { namClearClicked(slot); };

  addAndMakeVisible(controls.prevButton);
  controls.prevButton.setPaintingIsUnclipped(true);
  controls.prevButton.setButtonText("<");
  controls.prevButton.setTitle("Previous " + slotName + " Model");
  controls.prevButton.onClick = [this, slot] { cycleNamFile(slot, -1); };

  addAndMakeVisible(controls.nextButton);
  controls.nextButton.setPaintingIsUnclipped(true);
  controls.nextButton.setButtonText(">");
  controls.nextButton.setTitle("Next " + slotName + " Model");
  controls.nextButton.onClick = [this, slot] { cycleNamFile(slot, 1); };

  addAndMakeVisible(controls.calibrateButton);
  controls.calibrateButton.setPaintingIsUnclipped(true);
  controls.calibrateButton.setButtonText("CAL");
  controls.calibrateButton.setTitle(slotName + " Calibration Settings");
  controls.calibrateButton.onClick = [&controls]
  {
    controls.calibrationPanel->setVisible(true);
    controls.calibrationPanel->toFront(true);
    controls.calibrationPanel->grabKeyboardFocus();
  };

  const auto& ids = OctoBassProcessor::getNamSlotIds(slot);

  addAndMakeVisible(controls.onSwitch);
  controls.onSwitch.setComponentID("metalToggle");
  controls.onSwitch.setTitle(slotName + " On");
  controls.onSwitch.popupTextProvider = [&controls]() -> juce::String
  { return controls.onSwitch.getToggleState() ? "On" : "Off"; };
  controls.onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
      audioProcessor.getAPVTS(), ids.on, controls.onSwitch);

  addAndMakeVisible(controls.qualityToggle);
  controls.qualityToggle.setComponentID("metalToggle");
  controls.qualityToggle.setTitle(slotName + " Quality");
  controls.qualityToggle.popupTextProvider = [&controls]() -> juce::String
  {
    if (controls.lastQualityLevels <= 1)
      return "No quality options available";
    return controls.qualityToggle.getToggleState() ? "Full" : "Lite";
  };
  controls.qualityAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          audioProcessor.getAPVTS(), ids.quality, controls.qualityToggle);

  addAndMakeVisible(controls.lcdDisplay);
  controls.lcdDisplay.setTextColour(kLCDInkColour);
  controls.lcdDisplay.setOnClick([this, slot] { namLoadClicked(slot); });
  showNamPath(slot, audioProcessor.getCurrentNamModelPath(slot));
  controls.backlightAttachment = std::make_unique<juce::ParameterAttachment>(
      *audioProcessor.getAPVTS().getParameter(ids.on),
      [&controls, slotName](float value)
      {
        const bool on = value >= 0.5f;
        DBG(slotName + (on ? " switched on" : " switched off"));
        controls.lcdDisplay.setBacklit(on);
      });
  controls.backlightAttachment->sendInitialUpdate();

  controls.calibrationPanel =
      std::make_unique<NamCalibrationPanel>(audioProcessor.getAPVTS(), slot);
  controls.calibrationPanel->onDismiss = [&controls]
  { controls.calibrationPanel->setVisible(false); };

  updateNamQualityToggle(slot);
  updateNamCalibrationControls(slot);
}

void OctoBassEditor::updateNamQualityToggle(octob::NamSlot slot)
{
  auto& controls = namControls(slot);
  const int levels = audioProcessor.getNamQualityLevels(slot);
  if (levels == controls.lastQualityLevels)
    return;
  controls.lastQualityLevels = levels;

  controls.qualityToggle.setEnabled(levels > 1);
}

void OctoBassEditor::updateNamCalibrationControls(octob::NamSlot slot)
{
  auto& controls = namControls(slot);
  const bool loaded = audioProcessor.isNamModelLoaded(slot);
  const auto metadata = audioProcessor.getNamModelMetadata(slot);
  if (loaded == controls.lastLoaded && metadata == controls.lastMetadata)
    return;
  controls.lastLoaded = loaded;
  controls.lastMetadata = metadata;

  controls.calibrateButton.setEnabled(loaded);
  controls.calibrationPanel->setCapabilities(metadata);
  if (!loaded && controls.calibrationPanel->isVisible())
  {
    DBG(OctoBassProcessor::getNamSlotName(slot) +
        " model cleared while its calibration panel was open, dismissing it");
    controls.calibrationPanel->setVisible(false);
  }
}

void OctoBassEditor::syncLoadedFileDisplays()
{
  for (const auto slot : octob::kNamSlots)
  {
    const auto path = audioProcessor.getCurrentNamModelPath(slot);
    if (path != namControls(slot).shownPath)
      showNamPath(slot, path);
  }

  if (const auto irPath = audioProcessor.getCurrentIRPath(); irPath != shownIRPath_)
    showIRPath(irPath);
}

OctoBassEditor::ParamHandle OctoBassEditor::paramHandle(const juce::String& paramID) const
{
  auto& apvts = audioProcessor.getAPVTS();
  return {apvts.getParameter(paramID), apvts.getRawParameterValue(paramID)};
}

void OctoBassEditor::forEachHandleParam(int handle, GraphicEQDisplay::GestureScope scope,
                                        void (juce::RangedAudioParameter::*action)())
{
  auto apply = [action](const ParamHandle& handleParam)
  {
    if (handleParam.param != nullptr)
      (handleParam.param->*action)();
  };

  // 'active' only participates in creation/deletion gestures so hosts do not
  // record automation on a parameter a plain move never changes
  const bool includeActive = scope == GraphicEQDisplay::GestureScope::All;

  if (handle >= 0 && handle < octob::kGraphicEQNumNodes)
  {
    const auto& node = eqNodeParams_[static_cast<size_t>(handle)];
    if (includeActive)
      apply(node.active);
    apply(node.freq);
    apply(node.gain);
  }
  else if (handle == GraphicEQDisplay::kLowCutHandle)
  {
    if (includeActive)
      apply(eqLowCutActiveParam_);
    apply(eqLowCutFreqParam_);
  }
  else if (handle == GraphicEQDisplay::kHighCutHandle)
  {
    if (includeActive)
      apply(eqHighCutActiveParam_);
    apply(eqHighCutFreqParam_);
  }
  else
  {
    DBG("Unknown EQ handle " + juce::String(handle) + " in gesture callback");
  }
}

void OctoBassEditor::paint(juce::Graphics& g)
{
  g.fillAll(juce::Colour(0xfff0f0f2));

  auto b = getLocalBounds().toFloat();
  const float cornerR = 3.0f;

  g.setColour(juce::Colour(0xff000000).withAlpha(0.28f));
  g.drawRoundedRectangle(b.reduced(0.5f), cornerR, 1.0f);
  g.setColour(juce::Colour(0xff000000).withAlpha(0.14f));
  g.drawRoundedRectangle(b.reduced(1.5f), cornerR, 1.0f);
  g.setColour(juce::Colour(0xff000000).withAlpha(0.06f));
  g.drawRoundedRectangle(b.reduced(2.5f), cornerR, 1.0f);

  auto inner = b.reduced(1.0f);
  g.setColour(juce::Colour(0xffffffff).withAlpha(0.30f));
  g.drawLine(inner.getX() + cornerR, inner.getY() + 0.5f, inner.getRight() - cornerR,
             inner.getY() + 0.5f, 1.0f);
  g.drawLine(inner.getX() + 0.5f, inner.getY() + cornerR, inner.getX() + 0.5f,
             inner.getBottom() - cornerR, 1.0f);

  const float scale = static_cast<float>(getWidth()) / static_cast<float>(kDesignWidth);
  g.addTransform(juce::AffineTransform::scale(scale));

  const float w = static_cast<float>(kDesignWidth);
  const float h = static_cast<float>(kDesignHeight);

  // Screws at 1/5 and 4/5 from edges
  const float screwCx1 = w / 5.0f;
  const float screwCx2 = w * 4.0f / 5.0f;
  const float screwCyTop = 8.0f + 7.5f;
  const float screwCyBottom = h - 8.0f - 7.5f;

  drawPanelScrew(g, screwCx1, screwCyTop);
  drawPanelScrew(g, screwCx2, screwCyTop);
  drawPanelScrew(g, screwCx1, screwCyBottom);
  drawPanelScrew(g, screwCx2, screwCyBottom);

  // Diagonal zone separator lines -- both lean / (upper-right to lower-left), full height
  g.setColour(juce::Colour(0xff1a1a1a));
  float leftTopX = kLeftLineTopX;
  float leftBotX = leftTopX - kLineShift;
  float rightTopX = kRightLineTopX;
  float rightBotX = rightTopX - kLineShift;
  g.drawLine(leftTopX, 0.0f, leftBotX, h, 3.0f);
  g.drawLine(rightTopX, 0.0f, rightBotX, h, 3.0f);

  // Version text
  g.setColour(juce::Colour(0xff555555));
  g.setFont(juce::Font(juce::FontOptions().withHeight(8.0f)));
  auto versionArea = juce::Rectangle<float>(32.0f, screwCyBottom - 5.0f, screwCx1 - 40.0f, 10.0f);
  g.drawText("v" JucePlugin_VersionString, versionArea.toNearestInt(),
             juce::Justification::centredLeft, false);

  // October logo -- bottom-right near screws (same as octobir)
  if (logoImage_.isValid())
  {
    const float imgAspect =
        static_cast<float>(logoImage_.getWidth()) / static_cast<float>(logoImage_.getHeight());
    const float logoH = 40.0f;
    const float logoW = logoH * imgAspect;
    const float logoX = (screwCx2 + w) * 0.5f - logoW * 0.5f - 4.0f;
    const float logoY = screwCyBottom - logoH * 0.5f - 2.0f;
    g.drawImage(logoImage_, static_cast<int>(logoX), static_cast<int>(logoY),
                static_cast<int>(logoW), static_cast<int>(logoH), 0, 0, logoImage_.getWidth(),
                logoImage_.getHeight());
  }
}

void OctoBassEditor::resized()
{
  const float scale = static_cast<float>(getWidth()) / static_cast<float>(kDesignWidth);

  const int pad = 15;
  const int lcdY = pad + 10;
  const int lcdH = 174;
  const int lcdBottom = lcdY + lcdH;

  // Knob rows at 1/3 and 2/3 of space between LCD bottom and plugin bottom, with tweaks
  const int controlSpace = kDesignHeight - lcdBottom;
  const int row1CenterY = lcdBottom + controlSpace / 3 - 50;
  const int row2CenterY = lcdBottom + 2 * controlSpace / 3 + 10;

  // Full knob row: label(16) + slider-with-textbox(90) = 106
  const int labelH = 16;
  const int fullKnobH = 90;
  const int fullRowH = labelH + fullKnobH;
  const int row1Y = row1CenterY - fullRowH / 2;
  const int row2Y = row2CenterY - fullRowH / 2;

  const int trimKnobSize = 28;

  // Zone X boundaries
  const int leftX = pad;
  const int leftW = 220;
  const int rightX = pad + leftW + 160;
  const int rightW = kDesignWidth - pad - rightX;

  // === LCD (full width) ===
  graphicEQDisplay_.setBounds(pad, lcdY, kDesignWidth - 2 * pad, lcdH);

  // === LEFT ZONE ===
  const int halfLeft = leftW / 2;

  // Row 1: Squash + Mode
  {
    auto area = juce::Rectangle<int>(leftX, row1Y, halfLeft, fullRowH);
    squashLabel_.setBounds(area.removeFromTop(labelH));
    squashSlider_.setBounds(area.withSizeKeepingCentre(86, fullKnobH));
  }
  {
    auto area = juce::Rectangle<int>(leftX + halfLeft, row1Y, halfLeft, fullRowH);
    compressionModeLabel_.setBounds(area.removeFromTop(labelH));
    compressionModeSlider_.setBounds(area.withSizeKeepingCentre(86, fullKnobH));
  }

  // Row 2: Output + Solo (Output aligned under Squash)
  {
    auto area = juce::Rectangle<int>(leftX, row2Y, halfLeft, fullRowH);
    lowBandLevelLabel_.setBounds(area.removeFromTop(labelH));
    lowBandLevelSlider_.setBounds(area.withSizeKeepingCentre(86, fullKnobH));
  }
  {
    auto soloArea = juce::Rectangle<int>(leftX + halfLeft, row2Y, halfLeft, fullRowH);
    lowBandSoloButton_.setBounds(soloArea.withSizeKeepingCentre(36, 28));
  }

  // === CENTER ZONE (centered between diagonal lines at each row's Y) ===
  const int centerKnobAreaW = 120;

  // Crossover: centered between lines at row1CenterY
  {
    float lx = lineXAtY(kLeftLineTopX, static_cast<float>(row1CenterY));
    float rx = lineXAtY(kRightLineTopX, static_cast<float>(row1CenterY));
    int cx = static_cast<int>((lx + rx) / 2.0f);

    auto area = juce::Rectangle<int>(cx - centerKnobAreaW / 2, row1Y, centerKnobAreaW, fullRowH);
    crossoverLabel_.setBounds(area.removeFromTop(labelH));
    crossoverSlider_.setBounds(area.withSizeKeepingCentre(86, fullKnobH));
  }

  // Gate: centered between lines at row2CenterY
  {
    float lx = lineXAtY(kLeftLineTopX, static_cast<float>(row2CenterY));
    float rx = lineXAtY(kRightLineTopX, static_cast<float>(row2CenterY));
    int cx = static_cast<int>((lx + rx) / 2.0f);

    auto area = juce::Rectangle<int>(cx - centerKnobAreaW / 2, row2Y, centerKnobAreaW, fullRowH);
    gateLabel_.setBounds(area.removeFromTop(labelH));
    gateSlider_.setBounds(area.withSizeKeepingCentre(86, fullKnobH));
  }

  // === RIGHT ZONE ===
  const int stripX = rightX + 20;
  const int stripW = kDesignWidth - pad - stripX;
  const int stripColW = stripW / 4;
  const int stripKnobY = row1Y + labelH;
  auto stripColumn = [&](int index)
  {
    return juce::Rectangle<int>(stripX + index * stripColW, row1Y, stripColW,
                                labelH + trimKnobSize);
  };
  auto placeTrim = [&](int index, juce::Label& label, juce::Slider& slider)
  {
    auto area = stripColumn(index);
    label.setBounds(area.removeFromTop(labelH));
    slider.setBounds(area.withSizeKeepingCentre(trimKnobSize, trimKnobSize));
  };
  placeTrim(0, highInputGainLabel_, highInputGainSlider_);
  placeTrim(1, highBandMixLabel_, highBandMixSlider_);
  placeTrim(2, highOutputGainLabel_, highOutputGainSlider_);
  highBandSoloButton_.setBounds(
      stripColumn(3).withY(stripKnobY).withHeight(trimKnobSize).withSizeKeepingCentre(40, 28));
  const int stripBottom = stripKnobY + trimKnobSize;

  const int row2Bottom = row2Y + fullRowH;
  const int loaderButtonRowH = 28;
  const int loaderLcdH = 30;
  const int innerGap = 3;
  const int loaderH = loaderButtonRowH + innerGap + loaderLcdH;
  const int numLoaders = octob::kNumNamSlots + 1;
  const int minStripClearance = 12;
  const int loaderGap = juce::jmin(
      16, (row2Bottom - stripBottom - minStripClearance - numLoaders * loaderH) / (numLoaders - 1));
  const int firstLoaderY = row2Bottom - numLoaders * loaderH - (numLoaders - 1) * loaderGap;

  auto loaderArea = [&](int index)
  {
    return juce::Rectangle<int>(rightX, firstLoaderY + index * (loaderH + loaderGap), rightW,
                                loaderH);
  };
  auto placeButton = [](juce::Rectangle<int>& row, juce::Button& button, int width)
  { button.setBounds(row.removeFromLeft(width).reduced(2, 1)); };

  for (const auto slot : octob::kNamSlots)
  {
    auto& controls = namControls(slot);
    auto section = loaderArea(static_cast<int>(slot));
    auto buttonRow = section.removeFromTop(loaderButtonRowH);
    placeButton(buttonRow, controls.loadButton, 36);
    placeButton(buttonRow, controls.clearButton, 48);
    placeButton(buttonRow, controls.prevButton, 28);
    placeButton(buttonRow, controls.nextButton, 28);
    placeButton(buttonRow, controls.calibrateButton, 44);
    controls.qualityToggle.setBounds(
        buttonRow.removeFromRight(48).withSizeKeepingCentre(44, loaderButtonRowH));
    controls.onSwitch.setBounds(
        buttonRow.removeFromRight(48).withSizeKeepingCentre(44, loaderButtonRowH));
    section.removeFromTop(innerGap);
    controls.lcdDisplay.setBounds(section.reduced(2, 0));

    controls.calibrationPanel->setBounds(0, 0, kDesignWidth, kDesignHeight);
  }

  {
    auto section = loaderArea(octob::kNumNamSlots);
    auto buttonRow = section.removeFromTop(loaderButtonRowH);
    placeButton(buttonRow, irLoadButton_, 36);
    placeButton(buttonRow, irClearButton_, 48);
    placeButton(buttonRow, irPrevButton_, 28);
    placeButton(buttonRow, irNextButton_, 28);
    section.removeFromTop(innerGap);
    irLCDDisplay_.setBounds(section.reduced(2, 0));
  }

  // Apply scale transform to all children
  juce::AffineTransform xform = juce::AffineTransform::scale(scale);
  for (int i = 0; i < getNumChildComponents(); ++i)
  {
    auto* child = getChildComponent(i);
    if (dynamic_cast<juce::ResizableCornerComponent*>(child) == nullptr)
      child->setTransform(xform);
  }
}

// Neighbouring file (wrapping) of the same type in the directory of currentPath,
// or an invalid File when there is nothing to cycle to
static juce::File adjacentFile(const juce::String& currentPath, const juce::String& wildcard,
                               int direction)
{
  if (currentPath.isEmpty())
    return {};

  juce::File currentFile(currentPath);
  if (!currentFile.existsAsFile())
  {
    DBG("Cannot cycle: current file no longer exists: " + currentPath);
    return {};
  }

  juce::Array<juce::File> files;
  for (const auto& entry :
       juce::RangedDirectoryIterator(currentFile.getParentDirectory(), false, wildcard,
                                     juce::File::findFiles | juce::File::ignoreHiddenFiles))
  {
    files.add(entry.getFile());
  }

  if (files.isEmpty())
    return {};

  files.sort();

  int currentIndex = files.indexOf(currentFile);
  if (currentIndex < 0)
    return {};

  return files[(currentIndex + direction + files.size()) % files.size()];
}

void OctoBassEditor::namLoadClicked(octob::NamSlot slot)
{
  const auto fileSlot = OctoBassProcessor::fileSlotFor(slot);
  auto chooser = std::make_shared<juce::FileChooser>(
      "Select " + OctoBassProcessor::getNamSlotName(slot) + " model file",
      audioProcessor.getBrowseStartFolder(fileSlot), "*.nam");

  auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

  chooser->launchAsync(flags,
                       [safeThis = juce::Component::SafePointer(this), chooser, slot,
                        fileSlot](const juce::FileChooser& fc)
                       {
                         if (safeThis == nullptr)
                         {
                           DBG("Editor destroyed while NAM chooser was open, dropping result");
                           return;
                         }
                         auto file = fc.getResult();
                         if (!file.existsAsFile())
                         {
                           DBG("NAM chooser closed without a file");
                           return;
                         }
                         juce::String error;
                         const bool loaded =
                             safeThis->audioProcessor.loadPickedFile(fileSlot, file, error);
                         safeThis->showNamLoadResult(slot, loaded, error);
                       });
}

void OctoBassEditor::namClearClicked(octob::NamSlot slot)
{
  audioProcessor.clearNamModel(slot);
  showNamPath(slot, {});
}

void OctoBassEditor::showNamPath(octob::NamSlot slot, const juce::String& path)
{
  auto& controls = namControls(slot);
  controls.shownPath = path;
  controls.lcdDisplay.setText(path.isEmpty()
                                  ? "No " + OctoBassProcessor::getNamSlotName(slot) + " loaded"
                                  : juce::File(path).getFileName());
}

void OctoBassEditor::showNamLoadResult(octob::NamSlot slot, bool loaded, const juce::String& error)
{
  showNamPath(slot, audioProcessor.getCurrentNamModelPath(slot));
  if (!loaded)
    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::WarningIcon,
        "Failed to Load " + OctoBassProcessor::getNamSlotName(slot) + " Model", error, "OK");
}

void OctoBassEditor::cycleNamFile(octob::NamSlot slot, int direction)
{
  auto next = adjacentFile(audioProcessor.getCurrentNamModelPath(slot), "*.nam", direction);
  if (next == juce::File())
    return;

  juce::String error;
  const bool loaded = audioProcessor.loadNamModel(slot, next.getFullPathName(), error);
  showNamLoadResult(slot, loaded, error);
}

// --- IR file loader handlers ---

void OctoBassEditor::irLoadClicked()
{
  auto chooser = std::make_shared<juce::FileChooser>(
      "Select impulse response WAV file",
      audioProcessor.getBrowseStartFolder(OctoBassProcessor::FileSlot::ImpulseResponse), "*.wav");

  auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

  // SafePointer: some hosts destroy the editor while the OS dialog is open
  chooser->launchAsync(
      flags,
      [safeThis = juce::Component::SafePointer(this), chooser](const juce::FileChooser& fc)
      {
        if (safeThis == nullptr)
        {
          DBG("Editor destroyed while IR chooser was open, dropping result");
          return;
        }
        auto file = fc.getResult();
        if (!file.existsAsFile())
        {
          DBG("IR chooser closed without a file");
          return;
        }
        juce::String error;
        const bool loaded = safeThis->audioProcessor.loadPickedFile(
            OctoBassProcessor::FileSlot::ImpulseResponse, file, error);
        safeThis->showIRLoadResult(loaded, error);
      });
}

void OctoBassEditor::irClearClicked()
{
  audioProcessor.clearImpulseResponse();
  showIRPath({});
}

void OctoBassEditor::showIRPath(const juce::String& path)
{
  shownIRPath_ = path;
  irLCDDisplay_.setText(path.isEmpty() ? "No IR loaded" : juce::File(path).getFileName());
}

void OctoBassEditor::showIRLoadResult(bool loaded, const juce::String& error)
{
  showIRPath(audioProcessor.getCurrentIRPath());
  if (!loaded)
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Failed to Load IR",
                                           error, "OK");
}

void OctoBassEditor::cycleIRFile(int direction)
{
  auto next = adjacentFile(audioProcessor.getCurrentIRPath(), "*.wav", direction);
  if (next == juce::File())
    return;

  juce::String error;
  const bool loaded = audioProcessor.loadImpulseResponse(next.getFullPathName(), error);
  showIRLoadResult(loaded, error);
}
