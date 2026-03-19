#include "PluginEditor.h"

#include <BinaryData.h>

#include "PluginProcessor.h"

void LCDMeterPanel::paint(juce::Graphics& g)
{
  static constexpr int kPadV = 15;
  static constexpr int kPadH = 8;
  static constexpr int kLabelH = 10;
  static constexpr int kGapLB = 3;
  static constexpr int kBarH = 24;
  static constexpr int kGapBM = 16;

  auto bounds = getLocalBounds().toFloat().reduced(1.0f);

  g.setColour(juce::Colour(0xffF08830));
  g.fillRoundedRectangle(bounds, 3.0f);
  g.setColour(juce::Colour(0xff1a1a1a));
  g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

  auto inner = bounds.reduced(1.0f);
  juce::ColourGradient topShadow(juce::Colour(0xff000000).withAlpha(0.22f), inner.getX(),
                                 inner.getY(), juce::Colour(0xff000000).withAlpha(0.0f),
                                 inner.getX(), inner.getY() + 5.0f, false);
  g.setGradientFill(topShadow);
  g.fillRoundedRectangle(inner, 2.0f);
  juce::ColourGradient leftShadow(juce::Colour(0xff000000).withAlpha(0.12f), inner.getX(),
                                  inner.getY(), juce::Colour(0xff000000).withAlpha(0.0f),
                                  inner.getX() + 5.0f, inner.getY(), false);
  g.setGradientFill(leftShadow);
  g.fillRoundedRectangle(inner, 2.0f);

  if (typeface_ != nullptr)
    g.setFont(juce::Font(juce::FontOptions().withTypeface(typeface_).withHeight(8.0f)));
  else
    g.setFont(juce::Font(
        juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 8.0f, juce::Font::plain)));

  g.setColour(juce::Colour(0xff1c1c30));

  auto content = getLocalBounds().reduced(kPadH, kPadV);
  int cx = content.getX();
  int cw = content.getWidth();
  int cy = content.getY();

  juce::Rectangle<int> inputLabelArea(cx, cy, cw, kLabelH);
  g.drawText("INPUT", inputLabelArea, juce::Justification::centredLeft, true);
  cy += kLabelH + kGapLB;

  juce::Rectangle<int> inputBarArea(cx, cy, cw, kBarH);
  g.setColour(juce::Colour(0xff1a1a1a));
  g.drawRoundedRectangle(inputBarArea.toFloat().reduced(0.5f), 3.0f, 1.0f);

  float normInput = juce::jlimit(0.0f, 1.0f, (inputValue_ + 96.0f) / 96.0f);
  float normLow = juce::jlimit(0.0f, 1.0f, (lowThreshold_ + 96.0f) / 96.0f);
  float normHigh = juce::jlimit(0.0f, 1.0f, (highThreshold_ + 96.0f) / 96.0f);
  paintMeter(g, inputBarArea, normInput, false, showThresholds_, normLow, normHigh);
  cy += kBarH + kGapBM;

  g.setColour(juce::Colour(0xff1c1c30));
  juce::Rectangle<int> blendLabelArea(cx, cy, cw, kLabelH);
  g.drawText("BLEND", blendLabelArea, juce::Justification::centredLeft, true);
  cy += kLabelH + kGapLB;

  juce::Rectangle<int> blendBarArea(cx, cy, cw, kBarH);
  g.setColour(juce::Colour(0xff1a1a1a));
  g.drawRoundedRectangle(blendBarArea.toFloat().reduced(0.5f), 3.0f, 1.0f);

  float normBlend = juce::jlimit(0.0f, 1.0f, (blendValue_ + 1.0f) / 2.0f);
  paintMeter(g, blendBarArea, normBlend, true, false, 0.0f, 0.0f);

  auto scanBounds = getLocalBounds();
  g.setColour(juce::Colour(0xff000000).withAlpha(0.025f));
  for (int sy = scanBounds.getY(); sy < scanBounds.getBottom(); sy += 2)
    g.drawHorizontalLine(sy, static_cast<float>(scanBounds.getX()),
                         static_cast<float>(scanBounds.getRight()));
}

void LCDMeterPanel::paintMeter(juce::Graphics& g, juce::Rectangle<int> barArea, float normValue,
                               bool centreBalanced, bool showThresholds, float normLow,
                               float normHigh) const
{
  auto innerArea = barArea.reduced(3, 3);
  float totalGapWidth = static_cast<float>((numSegments_ - 1) * segmentGap_);
  float segW =
      (static_cast<float>(innerArea.getWidth()) - totalGapWidth) / static_cast<float>(numSegments_);
  float segH = static_cast<float>(innerArea.getHeight());

  for (int i = 0; i < numSegments_; ++i)
  {
    float x = static_cast<float>(innerArea.getX()) +
              static_cast<float>(i) * (segW + static_cast<float>(segmentGap_));
    juce::Rectangle<float> seg(x, static_cast<float>(innerArea.getY()), segW, segH);

    bool isLit = false;
    if (centreBalanced)
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
      g.setColour(juce::Colour(0xff1c1c30));
      g.fillRect(seg);
    }
  }

  if (showThresholds)
  {
    auto drawMarker = [&](float normT)
    {
      float markerX =
          static_cast<float>(innerArea.getX()) + normT * static_cast<float>(innerArea.getWidth());
      g.setColour(juce::Colour(0xff1c1c30));
      g.fillRect(juce::Rectangle<float>(markerX - 2.0f, static_cast<float>(barArea.getY()) - 4.0f,
                                        4.0f, static_cast<float>(barArea.getHeight()) + 8.0f));
    };
    drawMarker(normLow);
    drawMarker(normHigh);
  }
}

static void drawScrew(juce::Graphics& g, float cx, float cy)
{
  // Dimensions and colors from ScrewSilver.svg (VCV Rack ComponentLibrary, 15x15px).
  // All three gradients run top-to-bottom (light -> dark) per the SVG gradientTransform.
  {
    juce::ColourGradient grad(juce::Colour(0xffABA9A9), cx, cy - 6.5f, juce::Colour(0xff8F8F8F), cx,
                              cy + 6.5f, false);
    g.setGradientFill(grad);
    g.fillEllipse(cx - 6.5f, cy - 6.5f, 13.0f, 13.0f);
  }
  {
    juce::ColourGradient grad(juce::Colour(0xffF5F5F5), cx, cy - 5.81f, juce::Colour(0xffC2C2C2),
                              cx, cy + 5.81f, false);
    g.setGradientFill(grad);
    g.fillEllipse(cx - 5.81f, cy - 5.81f, 11.62f, 11.62f);
  }
  {
    juce::ColourGradient grad(juce::Colour(0xffEBEBEB), cx, cy - 5.36f, juce::Colour(0xffCCCCCC),
                              cx, cy + 5.36f, false);
    g.setGradientFill(grad);
    g.fillEllipse(cx - 5.36f, cy - 5.36f, 10.72f, 10.72f);
  }

  // Cross slot: two overlapping rects forming a plus, ~7.09px arm length, ~1.88px arm width
  const float armLen = 7.09f;
  const float armW = 1.88f;
  g.setColour(juce::Colour(0xff8C8C8C));
  g.fillRect(cx - armW * 0.5f, cy - armLen * 0.5f, armW, armLen);
  g.fillRect(cx - armLen * 0.5f, cy - armW * 0.5f, armLen, armW);
}

static void setupRotarySlider(juce::Slider& s, int textBoxWidth = 90)
{
  s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, textBoxWidth, 22);
  s.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                        juce::MathConstants<float>::pi * 2.75f, true);
}

OctobIREditor::OctobIREditor(OctobIRProcessor& p) : AudioProcessorEditor(&p), audioProcessor(p)
{
  setLookAndFeel(&laf_);

  if (auto typeface = laf_.getLCDTypeface())
  {
    ir1LCDDisplay_.setTypeface(typeface);
    ir2LCDDisplay_.setTypeface(typeface);
    meterPanel_.setTypeface(typeface);
  }

  addAndMakeVisible(loadButton1_);
  loadButton1_.setPaintingIsUnclipped(true);
  loadButton1_.setButtonText("LOAD");
  loadButton1_.onClick = [this] { loadButton1Clicked(); };

  addAndMakeVisible(clearButton1_);
  clearButton1_.setPaintingIsUnclipped(true);
  clearButton1_.setButtonText("CLEAR");
  clearButton1_.onClick = [this] { clearButton1Clicked(); };

  addAndMakeVisible(prevButton1_);
  prevButton1_.setPaintingIsUnclipped(true);
  prevButton1_.setButtonText("<");
  prevButton1_.onClick = [this] { prevButton1Clicked(); };

  addAndMakeVisible(nextButton1_);
  nextButton1_.setPaintingIsUnclipped(true);
  nextButton1_.setButtonText(">");
  nextButton1_.onClick = [this] { nextButton1Clicked(); };

  addAndMakeVisible(ir1LCDDisplay_);
  ir1LCDDisplay_.setTextColour(juce::Colour(0xff1c1c30));
  ir1LCDDisplay_.setText(audioProcessor.getCurrentIR1Path().isEmpty()
                             ? "No IR loaded"
                             : juce::File(audioProcessor.getCurrentIR1Path()).getFileName());

  addAndMakeVisible(ir1EnableButton_);
  ir1EnableButton_.setPaintingIsUnclipped(true);
  ir1EnableButton_.setButtonText("ENABLE");
  ir1EnableAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
      audioProcessor.getAPVTS(), "irAEnable", ir1EnableButton_);

  addAndMakeVisible(loadButton2_);
  loadButton2_.setPaintingIsUnclipped(true);
  loadButton2_.setButtonText("LOAD");
  loadButton2_.onClick = [this] { loadButton2Clicked(); };

  addAndMakeVisible(clearButton2_);
  clearButton2_.setPaintingIsUnclipped(true);
  clearButton2_.setButtonText("CLEAR");
  clearButton2_.onClick = [this] { clearButton2Clicked(); };

  addAndMakeVisible(prevButton2_);
  prevButton2_.setPaintingIsUnclipped(true);
  prevButton2_.setButtonText("<");
  prevButton2_.onClick = [this] { prevButton2Clicked(); };

  addAndMakeVisible(nextButton2_);
  nextButton2_.setPaintingIsUnclipped(true);
  nextButton2_.setButtonText(">");
  nextButton2_.onClick = [this] { nextButton2Clicked(); };

  addAndMakeVisible(ir2LCDDisplay_);
  ir2LCDDisplay_.setTextColour(juce::Colour(0xff1c1c30));
  ir2LCDDisplay_.setText(audioProcessor.getCurrentIR2Path().isEmpty()
                             ? "No IR loaded"
                             : juce::File(audioProcessor.getCurrentIR2Path()).getFileName());

  addAndMakeVisible(ir2EnableButton_);
  ir2EnableButton_.setPaintingIsUnclipped(true);
  ir2EnableButton_.setButtonText("ENABLE");
  ir2EnableAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
      audioProcessor.getAPVTS(), "irBEnable", ir2EnableButton_);

  addAndMakeVisible(meterPanel_);

  addAndMakeVisible(dynamicModeButton_);
  dynamicModeButton_.setPaintingIsUnclipped(true);
  dynamicModeButton_.setButtonText("DYNAMIC");
  dynamicModeAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
      audioProcessor.getAPVTS(), "dynamicMode", dynamicModeButton_);

  addAndMakeVisible(sidechainEnableButton_);
  sidechainEnableButton_.setPaintingIsUnclipped(true);
  sidechainEnableButton_.setButtonText("SIDECHAIN");
  sidechainEnableAttachment_ =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          audioProcessor.getAPVTS(), "sidechainEnable", sidechainEnableButton_);

  addAndMakeVisible(swapIROrderButton_);
  swapIROrderButton_.setPaintingIsUnclipped(true);
  swapIROrderButton_.setButtonText("SWAP");
  swapIROrderButton_.onClick = [this] { swapIROrderClicked(); };

  addAndMakeVisible(blendLabel_);
  blendLabel_.setText("BLEND", juce::dontSendNotification);
  blendLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(blendSlider_);
  setupRotarySlider(blendSlider_);
  blendAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      audioProcessor.getAPVTS(), "blend", blendSlider_);

  addAndMakeVisible(outputGainLabel_);
  outputGainLabel_.setText("OUTPUT", juce::dontSendNotification);
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

  logoImage_ =
      juce::ImageCache::getFromMemory(BinaryData::OctoberLogo_png, BinaryData::OctoberLogo_pngSize);

  startTimerHz(30);
  setSize(520, 694);
}

OctobIREditor::~OctobIREditor()
{
  stopTimer();
  setLookAndFeel(nullptr);
}

void OctobIREditor::paint(juce::Graphics& g)
{
  g.fillAll(juce::Colour(0xfff0f0f2));

  // Inner bevel: simulate a recessed metal panel
  auto b = getLocalBounds().toFloat();
  const float cornerR = 3.0f;

  // Layered dark inner shadow along all edges
  g.setColour(juce::Colour(0xff000000).withAlpha(0.28f));
  g.drawRoundedRectangle(b.reduced(0.5f), cornerR, 1.0f);
  g.setColour(juce::Colour(0xff000000).withAlpha(0.14f));
  g.drawRoundedRectangle(b.reduced(1.5f), cornerR, 1.0f);
  g.setColour(juce::Colour(0xff000000).withAlpha(0.06f));
  g.drawRoundedRectangle(b.reduced(2.5f), cornerR, 1.0f);

  // Subtle top and left highlight to reinforce the recessed-panel look
  auto inner = b.reduced(1.0f);
  g.setColour(juce::Colour(0xffffffff).withAlpha(0.30f));
  g.drawLine(inner.getX() + cornerR, inner.getY() + 0.5f, inner.getRight() - cornerR,
             inner.getY() + 0.5f, 1.0f);
  g.drawLine(inner.getX() + 0.5f, inner.getY() + cornerR, inner.getX() + 0.5f,
             inner.getBottom() - cornerR, 1.0f);

  const float w = static_cast<float>(getWidth());
  const float h = static_cast<float>(getHeight());
  const float screwCyTop = 8.0f + 7.5f;
  const float screwCyBottom = h - 8.0f - 7.5f;
  const float screwCx1 = w / 4.0f;
  const float screwCx2 = w * 3.0f / 4.0f;

  drawScrew(g, screwCx1, screwCyTop);
  drawScrew(g, screwCx2, screwCyTop);
  drawScrew(g, screwCx1, screwCyBottom);
  drawScrew(g, screwCx2, screwCyBottom);

  g.setColour(juce::Colour(0xff888888));
  g.setFont(juce::Font(juce::FontOptions().withHeight(8.0f)));

  auto versionArea = juce::Rectangle<float>(32.0f, screwCyBottom - 5.0f, screwCx1 - 40.0f, 10.0f);
  g.drawText("v" JucePlugin_VersionString, versionArea.toNearestInt(),
             juce::Justification::centredLeft, false);

  if (logoImage_.isValid())
  {
    const float imgAspect =
        static_cast<float>(logoImage_.getWidth()) / static_cast<float>(logoImage_.getHeight());
    const float logoH = 40.0f;
    const float logoW = logoH * imgAspect;
    const float logoX = (screwCx2 + w) * 0.5f - logoW * 0.5f - 4.0f;
    const float logoY = screwCyBottom - logoH * 0.5f - 12.0f;
    g.drawImage(logoImage_, (int)logoX, (int)logoY, (int)logoW, (int)logoH, 0, 0,
                logoImage_.getWidth(), logoImage_.getHeight());
  }
}

void OctobIREditor::resized()
{
  auto bounds = getLocalBounds().reduced(15);
  bounds.removeFromTop(16);
  bounds.removeFromBottom(28);

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
  meterPanel_.setBounds(bounds.removeFromTop(120));

  // --- Large Rotary Knobs (110px) ---
  bounds.removeFromTop(10);
  auto largeKnobRow = bounds.removeFromTop(110);
  auto largeHalfW = largeKnobRow.getWidth() / 2;
  {
    auto col = largeKnobRow.removeFromLeft(largeHalfW);
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
  meterPanel_.setInputValue(audioProcessor.getCurrentInputLevel());
  meterPanel_.setBlendValue(audioProcessor.getCurrentBlend());

  bool dynamicMode = audioProcessor.getAPVTS().getRawParameterValue("dynamicMode")->load() > 0.5f;
  meterPanel_.setShowThresholds(dynamicMode);

  if (dynamicMode)
  {
    float threshold = audioProcessor.getAPVTS().getRawParameterValue("threshold")->load();
    float rangeDb = audioProcessor.getAPVTS().getRawParameterValue("rangeDb")->load();
    meterPanel_.setThresholdMarkers(threshold, threshold + rangeDb);
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
