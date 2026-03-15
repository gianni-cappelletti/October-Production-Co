#include "OctobIRLookAndFeel.h"

#include <BinaryData.h>

OctobIRLookAndFeel::OctobIRLookAndFeel()
{
  cutiveMonoTypeface_ = juce::Typeface::createSystemTypefaceFor(
      BinaryData::CourierPrimeRegular_ttf, BinaryData::CourierPrimeRegular_ttfSize);
  workbenchTypeface_ = juce::Typeface::createSystemTypefaceFor(
      BinaryData::WorkbenchRegularVariableFont_BLEDSCAN_ttf,
      BinaryData::WorkbenchRegularVariableFont_BLEDSCAN_ttfSize);
  setDefaultSansSerifTypeface(cutiveMonoTypeface_);
  setColour(juce::ResizableWindow::backgroundColourId, juce::Colours::white);

  setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xffd8d8d8));
  setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffe07030));
  setColour(juce::Slider::thumbColourId, juce::Colour(0xff1a1a1a));
  setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xff444444));
  setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xfff0f0f0));
  setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xffcccccc));

  setColour(juce::TextButton::buttonColourId, juce::Colour(0xffe8e8e8));
  setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffd8d8d8));
  setColour(juce::TextButton::textColourOffId, juce::Colour(0xff1a1a1a));
  setColour(juce::TextButton::textColourOnId, juce::Colour(0xff1a1a1a));

  setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xffe8e8e8));
  setColour(juce::ComboBox::outlineColourId, juce::Colour(0xffcccccc));
  setColour(juce::ComboBox::textColourId, juce::Colour(0xff1a1a1a));
  setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff888888));

  setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xfff8f8f8));
  setColour(juce::PopupMenu::textColourId, juce::Colour(0xff1a1a1a));
  setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xffe07030));
  setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

  setColour(juce::Label::textColourId, juce::Colour(0xff1a1a1a));
}

void OctobIRLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float rotaryStartAngle,
                                          float rotaryEndAngle, juce::Slider& slider)
{
  auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(10.0f);
  auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
  auto centreX = bounds.getCentreX();
  auto centreY = bounds.getCentreY();
  auto lineW = juce::jmin(6.0f, radius * 0.4f);
  auto arcRadius = radius - lineW * 0.5f;
  auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

  juce::Path bgArc;
  bgArc.addCentredArc(centreX, centreY, arcRadius, arcRadius, 0.0f, rotaryStartAngle,
                      rotaryEndAngle, true);
  g.setColour(slider.findColour(juce::Slider::rotarySliderOutlineColourId));
  g.strokePath(bgArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));

  if (sliderPos > 0.0f)
  {
    juce::Path valueArc;
    valueArc.addCentredArc(centreX, centreY, arcRadius, arcRadius, 0.0f, rotaryStartAngle, toAngle,
                           true);
    g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
    g.strokePath(valueArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
  }

  auto capRadius = radius * 0.55f;
  juce::Point<float> centre(centreX, centreY);
  juce::ColourGradient capGradient(
      juce::Colour(0xffffffff), centre.translated(-capRadius * 0.3f, -capRadius * 0.3f),
      juce::Colour(0xffc8c8c8), centre.translated(capRadius * 0.5f, capRadius * 0.5f), true);
  g.setGradientFill(capGradient);
  g.fillEllipse(centreX - capRadius, centreY - capRadius, capRadius * 2.0f, capRadius * 2.0f);
  g.setColour(juce::Colour(0xffbbbbbb));
  g.drawEllipse(centreX - capRadius, centreY - capRadius, capRadius * 2.0f, capRadius * 2.0f, 1.0f);

  auto dotRadius = lineW * 0.85f;
  juce::Point<float> dotPt(
      centreX + arcRadius * std::cos(toAngle - juce::MathConstants<float>::halfPi),
      centreY + arcRadius * std::sin(toAngle - juce::MathConstants<float>::halfPi));
  g.setColour(slider.findColour(juce::Slider::thumbColourId));
  g.fillEllipse(juce::Rectangle<float>(dotRadius * 2.0f, dotRadius * 2.0f).withCentre(dotPt));
}

void OctobIRLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                              const juce::Colour& backgroundColour,
                                              bool shouldDrawButtonAsHighlighted,
                                              bool shouldDrawButtonAsDown)
{
  auto bounds = button.getLocalBounds().toFloat().reduced(0.5f, 0.5f);
  const auto cornerSize = 4.0f;

  auto base = backgroundColour.withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f);

  if (shouldDrawButtonAsDown)
    base = base.darker(0.15f);
  else if (shouldDrawButtonAsHighlighted)
    base = base.brighter(0.1f);

  g.setColour(base);
  g.fillRoundedRectangle(bounds, cornerSize);

  g.setColour(juce::Colour(0xff444444));
  g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
}

void OctobIRLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                        bool /*isHighlighted*/, bool isButtonDown)
{
  auto height = juce::jmin(15.0f, (float)button.getHeight() * 0.6f);
  g.setFont(juce::Font(juce::FontOptions().withTypeface(cutiveMonoTypeface_).withHeight(height)));
  g.setColour(button
                  .findColour(button.getToggleState() ? juce::TextButton::textColourOnId
                                                      : juce::TextButton::textColourOffId)
                  .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f));

  const int offset = isButtonDown ? 1 : 0;
  g.drawFittedText(button.getButtonText(), offset, offset, button.getWidth(), button.getHeight(),
                   juce::Justification::centred, 1);
}

void OctobIRLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                          bool shouldDrawButtonAsHighlighted,
                                          bool shouldDrawButtonAsDown)
{
  if (button.getProperties()["isLED"])
  {
    auto bounds = button.getLocalBounds().toFloat();
    auto ledSize = juce::jmin(bounds.getHeight() * 0.65f, 22.0f);
    auto ledBounds = juce::Rectangle<float>(bounds.getX() + 6.0f,
                                            bounds.getCentreY() - ledSize * 0.5f, ledSize, ledSize);
    auto centre = ledBounds.getCentre();
    auto radius = ledSize * 0.5f;
    bool isOn = button.getToggleState();
    auto ledColour = button.findColour(juce::ToggleButton::tickColourId);

    if (isOn)
    {
      auto glowR = radius * 1.8f;
      juce::ColourGradient glow(ledColour.withAlpha(0.5f), centre, ledColour.withAlpha(0.0f),
                                centre.translated(glowR, 0.0f), true);
      g.setGradientFill(glow);
      g.fillEllipse(centre.x - glowR, centre.y - glowR, glowR * 2.0f, glowR * 2.0f);
    }

    auto innerColour =
        isOn ? ledColour.brighter(0.5f) : ledColour.withSaturation(0.3f).withBrightness(0.08f);
    auto outerColour =
        isOn ? ledColour.darker(0.3f) : ledColour.withSaturation(0.2f).withBrightness(0.04f);
    juce::ColourGradient body(innerColour, centre.translated(-radius * 0.3f, -radius * 0.3f),
                              outerColour, centre.translated(radius * 0.7f, radius * 0.7f), true);
    g.setGradientFill(body);
    g.fillEllipse(ledBounds);

    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawEllipse(ledBounds, 1.0f);

    if (button.getButtonText().isNotEmpty())
    {
      g.setColour(findColour(juce::Label::textColourId)
                      .withMultipliedAlpha(button.isEnabled() ? 0.9f : 0.5f));
      g.setFont(
          juce::Font(juce::FontOptions().withTypeface(cutiveMonoTypeface_).withHeight(12.0f)));
      auto textBounds = bounds.withLeft(ledBounds.getRight() + 5.0f);
      g.drawFittedText(button.getButtonText(), textBounds.toNearestInt(),
                       juce::Justification::centredLeft, 1);
    }
  }
  else
  {
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f, 0.5f);
    const auto cornerSize = 4.0f;
    bool isOn = button.getToggleState();

    auto base = findColour(juce::TextButton::buttonColourId)
                    .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f);

    if (isOn)
      base = base.brighter(0.12f);
    if (shouldDrawButtonAsHighlighted)
      base = base.brighter(0.08f);
    if (shouldDrawButtonAsDown)
      base = base.darker(0.12f);

    g.setColour(base);
    g.fillRoundedRectangle(bounds, cornerSize);

    if (isOn)
    {
      g.setColour(juce::Colour(0xffe07030));
      g.fillRoundedRectangle(bounds.withWidth(3.0f), cornerSize);
    }

    g.setColour(juce::Colour(0xff444444));
    g.drawRoundedRectangle(bounds, cornerSize, 1.0f);

    g.setColour(findColour(juce::Label::textColourId)
                    .withMultipliedAlpha(button.isEnabled() ? 0.9f : 0.5f));
    g.setFont(juce::Font(juce::FontOptions().withTypeface(cutiveMonoTypeface_).withHeight(13.0f)));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred,
                     1);
  }
}

void OctobIRLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height,
                                      bool /*isButtonDown*/, int /*buttonX*/, int /*buttonY*/,
                                      int /*buttonW*/, int /*buttonH*/, juce::ComboBox& box)
{
  const auto cornerSize = 3.0f;
  juce::Rectangle<float> boxBounds(0.0f, 0.0f, (float)width, (float)height);

  g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
  g.fillRoundedRectangle(boxBounds, cornerSize);

  g.setColour(box.findColour(juce::ComboBox::outlineColourId));
  g.drawRoundedRectangle(boxBounds.reduced(0.5f, 0.5f), cornerSize, 1.0f);

  juce::Rectangle<int> arrowZone(width - 28, 0, 18, height);
  juce::Path arrow;
  arrow.startNewSubPath((float)arrowZone.getX() + 2.0f, (float)arrowZone.getCentreY() - 2.0f);
  arrow.lineTo((float)arrowZone.getCentreX(), (float)arrowZone.getCentreY() + 3.0f);
  arrow.lineTo((float)arrowZone.getRight() - 2.0f, (float)arrowZone.getCentreY() - 2.0f);

  g.setColour(
      box.findColour(juce::ComboBox::arrowColourId).withAlpha(box.isEnabled() ? 0.9f : 0.2f));
  g.strokePath(arrow, juce::PathStrokeType(2.0f));
}

juce::Typeface::Ptr OctobIRLookAndFeel::getTypefaceForFont(const juce::Font&)
{
  return cutiveMonoTypeface_;
}

juce::Font OctobIRLookAndFeel::getLabelFont(juce::Label& label)
{
  auto f = LookAndFeel_V4::getLabelFont(label);
  return juce::Font(
      juce::FontOptions().withTypeface(cutiveMonoTypeface_).withHeight(f.getHeight()));
}
