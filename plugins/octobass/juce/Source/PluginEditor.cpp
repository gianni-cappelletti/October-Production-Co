#include "PluginEditor.h"

#include <BinaryData.h>

static void drawScrew(juce::Graphics& g, float cx, float cy)
{
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
static constexpr float kLineShift = 80.0f;
static constexpr float kLeftLineTopX = 288.0f;
static constexpr float kRightLineTopX = 428.0f;

static float lineXAtY(float topX, float y)
{
  return topX - kLineShift * (y / static_cast<float>(OctoBassEditor::kDesignHeight));
}

OctoBassEditor::OctoBassEditor(OctoBassProcessor& p) : AudioProcessorEditor(&p), audioProcessor(p)
{
  setLookAndFeel(&laf_);

  if (auto typeface = laf_.getLCDTypeface())
  {
    frequencyLCD_.setTypeface(typeface);
    namLCDDisplay_.setTypeface(typeface);
    irLCDDisplay_.setTypeface(typeface);
  }

  // Frequency analysis placeholder
  addAndMakeVisible(frequencyLCD_);
  frequencyLCD_.setTextColour(juce::Colour(0xff1c1c30));
  frequencyLCD_.setText("");

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
  setupRotarySlider(highInputGainSlider_);
  highInputGainAttachment_ =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          audioProcessor.getAPVTS(), "highInputGain", highInputGainSlider_);

  // --- HIGH zone: High Output Gain ---
  addAndMakeVisible(highOutputGainLabel_);
  highOutputGainLabel_.setText("OUTPUT", juce::dontSendNotification);
  highOutputGainLabel_.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(highOutputGainSlider_);
  setupRotarySlider(highOutputGainSlider_);
  highOutputGainAttachment_ =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          audioProcessor.getAPVTS(), "highOutputGain", highOutputGainSlider_);

  // --- HIGH zone: High Band Solo ---
  addAndMakeVisible(highBandSoloButton_);
  highBandSoloButton_.setPaintingIsUnclipped(true);
  highBandSoloButton_.setButtonText("S");
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

  // --- NAM file loader ---
  addAndMakeVisible(namLoadButton_);
  namLoadButton_.setPaintingIsUnclipped(true);
  namLoadButton_.setButtonText("");
  namLoadButton_.setTitle("Load NAM Model");
  namLoadButton_.setComponentID("loadButton");
  namLoadButton_.onClick = [this] { namLoadClicked(); };

  addAndMakeVisible(namClearButton_);
  namClearButton_.setPaintingIsUnclipped(true);
  namClearButton_.setButtonText("CLEAR");
  namClearButton_.onClick = [this] { namClearClicked(); };

  addAndMakeVisible(namPrevButton_);
  namPrevButton_.setPaintingIsUnclipped(true);
  namPrevButton_.setButtonText("<");
  namPrevButton_.setTitle("Previous NAM Model");
  namPrevButton_.onClick = [this] { namPrevClicked(); };

  addAndMakeVisible(namNextButton_);
  namNextButton_.setPaintingIsUnclipped(true);
  namNextButton_.setButtonText(">");
  namNextButton_.setTitle("Next NAM Model");
  namNextButton_.onClick = [this] { namNextClicked(); };

  addAndMakeVisible(namLCDDisplay_);
  namLCDDisplay_.setTextColour(juce::Colour(0xff1c1c30));
  namLCDDisplay_.setOnClick([this] { namLoadClicked(); });
  namLCDDisplay_.setText(audioProcessor.getCurrentNamModelPath().isEmpty()
                             ? "No NAM loaded"
                             : juce::File(audioProcessor.getCurrentNamModelPath()).getFileName());

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
  irClearButton_.onClick = [this] { irClearClicked(); };

  addAndMakeVisible(irPrevButton_);
  irPrevButton_.setPaintingIsUnclipped(true);
  irPrevButton_.setButtonText("<");
  irPrevButton_.setTitle("Previous IR");
  irPrevButton_.onClick = [this] { irPrevClicked(); };

  addAndMakeVisible(irNextButton_);
  irNextButton_.setPaintingIsUnclipped(true);
  irNextButton_.setButtonText(">");
  irNextButton_.setTitle("Next IR");
  irNextButton_.onClick = [this] { irNextClicked(); };

  addAndMakeVisible(irLCDDisplay_);
  irLCDDisplay_.setTextColour(juce::Colour(0xff1c1c30));
  irLCDDisplay_.setOnClick([this] { irLoadClicked(); });
  irLCDDisplay_.setText(audioProcessor.getCurrentIRPath().isEmpty()
                            ? "No IR loaded"
                            : juce::File(audioProcessor.getCurrentIRPath()).getFileName());

  logoImage_ =
      juce::ImageCache::getFromMemory(BinaryData::OctoberLogo_png, BinaryData::OctoberLogo_pngSize);

  setResizable(true, true);
  getConstrainer()->setFixedAspectRatio(static_cast<double>(kDesignWidth) /
                                        static_cast<double>(kDesignHeight));
  setSize(kDesignWidth, kDesignHeight);
  setResizeLimits(kDesignWidth * 3 / 4, kDesignHeight * 3 / 4, kDesignWidth * 3 / 2,
                  kDesignHeight * 3 / 2);
}

OctoBassEditor::~OctoBassEditor()
{
  setLookAndFeel(nullptr);
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

  drawScrew(g, screwCx1, screwCyTop);
  drawScrew(g, screwCx2, screwCyTop);
  drawScrew(g, screwCx1, screwCyBottom);
  drawScrew(g, screwCx2, screwCyBottom);

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
  const int lcdH = 134;
  const int lcdBottom = lcdY + lcdH;

  // Knob rows at 1/3 and 2/3 of space between LCD bottom and plugin bottom, with tweaks
  const int controlSpace = kDesignHeight - lcdBottom;
  const int row1CenterY = lcdBottom + controlSpace / 3 - 30;
  const int row2CenterY = lcdBottom + 2 * controlSpace / 3 + 10;

  // Full knob row: label(16) + slider-with-textbox(90) = 106
  const int labelH = 16;
  const int fullKnobH = 90;
  const int fullRowH = labelH + fullKnobH;
  const int row1Y = row1CenterY - fullRowH / 2;
  const int row2Y = row2CenterY - fullRowH / 2;

  // Trim knob: label(14) + knob(28) = 42
  const int trimLabelH = 14;
  const int trimKnobSize = 28;

  // Zone X boundaries
  const int leftX = pad;
  const int leftW = 220;
  const int rightX = pad + leftW + 140;
  const int rightW = kDesignWidth - pad - rightX;

  // === LCD (full width) ===
  frequencyLCD_.setBounds(pad, lcdY, kDesignWidth - 2 * pad, lcdH);

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
  // Layout: Input knob (left), Mix/Solo stacked (center), Output knob (right-aligned with LCD edge)
  const int knobColW = 120;
  const int outputRightEdge = kDesignWidth - pad;
  const int outputColX = outputRightEdge - knobColW;
  const int inputColX = rightX + 10;
  const int midColX = inputColX + knobColW;
  const int midColW = outputColX - midColX;

  // Row 1: Input + Output full rotary knobs + Mix/Solo stacked between them
  {
    auto area = juce::Rectangle<int>(inputColX, row1Y, knobColW, fullRowH);
    highInputGainLabel_.setBounds(area.removeFromTop(labelH));
    highInputGainSlider_.setBounds(area.withSizeKeepingCentre(86, fullKnobH));
  }
  {
    auto area = juce::Rectangle<int>(outputColX, row1Y, knobColW, fullRowH);
    highOutputGainLabel_.setBounds(area.removeFromTop(labelH));
    highOutputGainSlider_.setBounds(area.withSizeKeepingCentre(86, fullKnobH));
  }

  // Mix on top, Solo beneath, stacked vertically between Input and Output
  {
    const int stackCenterX = midColX + midColW / 2;
    const int stackTopY = row1Y + labelH + 4;

    auto mixArea = juce::Rectangle<int>(stackCenterX - 40, stackTopY, 80, trimLabelH + trimKnobSize);
    highBandMixLabel_.setBounds(mixArea.removeFromTop(trimLabelH));
    highBandMixSlider_.setBounds(mixArea.withSizeKeepingCentre(trimKnobSize, trimKnobSize));

    const int soloY = stackTopY + trimLabelH + trimKnobSize + 4;
    auto soloArea = juce::Rectangle<int>(stackCenterX - 20, soloY, 40, 28);
    highBandSoloButton_.setBounds(soloArea);
  }

  // File loaders: fill space from row 1 bottom to row 2 bottom (aligned with left/center row 2)
  const int row1Bottom = row1Y + fullRowH;
  const int row2Bottom = row2Y + fullRowH;
  const int fileStartY = row1Bottom + 6;
  const int fileEndY = row2Bottom;
  const int fileSpace = fileEndY - fileStartY;

  // Distribute space: buttonH + gap + lcdH per loader, plus gap between loaders
  const int buttonH = 28;
  const int lcdH2 = 32;
  const int innerGap = 2;
  const int singleLoaderH = buttonH + innerGap + lcdH2;
  const int loaderGap = fileSpace - 2 * singleLoaderH;

  // NAM file loader
  {
    const int namY = fileStartY;
    auto namSection = juce::Rectangle<int>(rightX, namY, rightW, singleLoaderH);
    auto namButtonRow = namSection.removeFromTop(buttonH);
    namLoadButton_.setBounds(namButtonRow.removeFromLeft(36).reduced(2));
    namClearButton_.setBounds(namButtonRow.removeFromLeft(48).reduced(2));
    namPrevButton_.setBounds(namButtonRow.removeFromLeft(28).reduced(2));
    namNextButton_.setBounds(namButtonRow.removeFromLeft(28).reduced(2));
    namSection.removeFromTop(innerGap);
    namLCDDisplay_.setBounds(namSection.reduced(2, 0));
  }

  // IR file loader
  {
    const int irY = fileStartY + singleLoaderH + loaderGap;
    auto irSection = juce::Rectangle<int>(rightX, irY, rightW, singleLoaderH);
    auto irButtonRow = irSection.removeFromTop(buttonH);
    irLoadButton_.setBounds(irButtonRow.removeFromLeft(36).reduced(2));
    irClearButton_.setBounds(irButtonRow.removeFromLeft(48).reduced(2));
    irPrevButton_.setBounds(irButtonRow.removeFromLeft(28).reduced(2));
    irNextButton_.setBounds(irButtonRow.removeFromLeft(28).reduced(2));
    irSection.removeFromTop(innerGap);
    irLCDDisplay_.setBounds(irSection.reduced(2, 0));
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

// --- NAM file loader handlers ---

void OctoBassEditor::namLoadClicked()
{
  auto chooser = std::make_shared<juce::FileChooser>("Select NAM model file",
                                                     getLastBrowsedDirectory(), "*.nam");

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
          bool success = audioProcessor.loadNamModel(file.getFullPathName(), error);
          if (success)
          {
            namLCDDisplay_.setText(file.getFileName());
          }
          else
          {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                   "Failed to Load NAM Model", error, "OK");
            namLCDDisplay_.setText("Failed to load NAM");
          }
        }
      });
}

void OctoBassEditor::namClearClicked()
{
  audioProcessor.clearNamModel();
  namLCDDisplay_.setText("No NAM loaded");
}

void OctoBassEditor::namPrevClicked()
{
  cycleNamFile(-1);
}

void OctoBassEditor::namNextClicked()
{
  cycleNamFile(1);
}

void OctoBassEditor::cycleNamFile(int direction)
{
  juce::String currentPath = audioProcessor.getCurrentNamModelPath();
  if (currentPath.isEmpty())
    return;

  juce::File currentFile(currentPath);
  if (!currentFile.existsAsFile())
    return;

  juce::File directory = currentFile.getParentDirectory();
  juce::Array<juce::File> namFiles;

  for (const auto& entry : juce::RangedDirectoryIterator(
           directory, false, "*.nam", juce::File::findFiles | juce::File::ignoreHiddenFiles))
  {
    namFiles.add(entry.getFile());
  }

  if (namFiles.isEmpty())
    return;

  namFiles.sort();

  int currentIndex = namFiles.indexOf(currentFile);
  if (currentIndex < 0)
    return;

  int newIndex = currentIndex + direction;
  if (newIndex < 0)
    newIndex = namFiles.size() - 1;
  else if (newIndex >= namFiles.size())
    newIndex = 0;

  juce::File newFile = namFiles[newIndex];
  juce::String error;
  bool success = audioProcessor.loadNamModel(newFile.getFullPathName(), error);
  if (success)
  {
    namLCDDisplay_.setText(newFile.getFileName());
  }
  else
  {
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                           "Failed to Load NAM Model", error, "OK");
    namLCDDisplay_.setText("Failed to load NAM");
  }
}

// --- IR file loader handlers ---

void OctoBassEditor::irLoadClicked()
{
  auto chooser = std::make_shared<juce::FileChooser>("Select impulse response WAV file",
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
          bool success = audioProcessor.loadImpulseResponse(file.getFullPathName(), error);
          if (success)
          {
            irLCDDisplay_.setText(file.getFileName());
          }
          else
          {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                   "Failed to Load IR", error, "OK");
            irLCDDisplay_.setText("Failed to load IR");
          }
        }
      });
}

void OctoBassEditor::irClearClicked()
{
  audioProcessor.clearImpulseResponse();
  irLCDDisplay_.setText("No IR loaded");
}

void OctoBassEditor::irPrevClicked()
{
  cycleIRFile(-1);
}

void OctoBassEditor::irNextClicked()
{
  cycleIRFile(1);
}

void OctoBassEditor::cycleIRFile(int direction)
{
  juce::String currentPath = audioProcessor.getCurrentIRPath();
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
  bool success = audioProcessor.loadImpulseResponse(newFile.getFullPathName(), error);
  if (success)
  {
    irLCDDisplay_.setText(newFile.getFileName());
  }
  else
  {
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Failed to Load IR",
                                           error, "OK");
    irLCDDisplay_.setText("Failed to load IR");
  }
}

// --- Directory helpers ---

juce::File OctoBassEditor::getLastBrowsedDirectory() const
{
  if (lastBrowsedDirectory_.exists() && lastBrowsedDirectory_.isDirectory())
    return lastBrowsedDirectory_;

  juce::String namPath = audioProcessor.getCurrentNamModelPath();
  if (namPath.isNotEmpty())
  {
    juce::File namFile(namPath);
    if (namFile.existsAsFile())
      return namFile.getParentDirectory();
  }

  juce::String irPath = audioProcessor.getCurrentIRPath();
  if (irPath.isNotEmpty())
  {
    juce::File irFile(irPath);
    if (irFile.existsAsFile())
      return irFile.getParentDirectory();
  }

  return juce::File::getSpecialLocation(juce::File::userHomeDirectory);
}

void OctoBassEditor::updateLastBrowsedDirectory(const juce::File& file)
{
  if (file.existsAsFile())
    lastBrowsedDirectory_ = file.getParentDirectory();
  else if (file.isDirectory())
    lastBrowsedDirectory_ = file;
}
