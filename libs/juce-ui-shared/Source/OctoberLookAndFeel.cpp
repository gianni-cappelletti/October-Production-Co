#include "OctoberLookAndFeel.h"

#include <BinaryData.h>

#include "LCDPainting.h"

OctoberLookAndFeel::OctoberLookAndFeel()
{
  cutiveMonoTypeface_ = juce::Typeface::createSystemTypefaceFor(
      BinaryData::CourierPrimeRegular_ttf, BinaryData::CourierPrimeRegular_ttfSize);
  lcdTypeface_ = juce::Typeface::createSystemTypefaceFor(BinaryData::PressStart2PRegular_ttf,
                                                         BinaryData::PressStart2PRegular_ttfSize);
  setDefaultSansSerifTypeface(cutiveMonoTypeface_);
  setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xfff0f0f2));

  setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xffd8d8d8));
  setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffe07030));
  setColour(juce::Slider::thumbColourId, juce::Colour(0xff1a1a1a));
  setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xff1e1a06));
  setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xffF08830));
  setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff1a1a1a));

  setColour(juce::TextButton::buttonColourId, juce::Colour(0xff323232));
  setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff222222));
  setColour(juce::TextButton::textColourOffId, juce::Colours::white);
  setColour(juce::TextButton::textColourOnId, juce::Colours::white);

  setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff323232));
  setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff181818));
  setColour(juce::ComboBox::textColourId, juce::Colours::white);
  setColour(juce::ComboBox::arrowColourId, juce::Colour(0xffaaaaaa));

  setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff2c2c2c));
  setColour(juce::PopupMenu::textColourId, juce::Colours::white);
  setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xffe07030));
  setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(0xff1c1c30));

  setColour(juce::Label::textColourId, juce::Colour(0xff1a1a1a));
}

void OctoberLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float rotaryStartAngle,
                                          float rotaryEndAngle, juce::Slider& /*slider*/)
{
  const bool isCompact = juce::jmin(width, height) < 40;
  const float reducedAmt = isCompact ? 3.0f : 10.0f;

  auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(reducedAmt);
  auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
  auto centreX = bounds.getCentreX();
  auto centreY = bounds.getCentreY();
  auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
  juce::Point<float> centre(centreX, centreY);

  const float outerR = radius;
  const float knurlingOuter = radius * 0.68f;
  const float knurlingInner = radius * 0.60f;
  const float capR = radius * 0.60f;
  const int knurlingCount = isCompact ? 28 : 55;

  // Drop shadow
  g.setColour(juce::Colour(0xff000000).withAlpha(0.22f));
  g.fillEllipse(centreX - outerR + 0.5f, centreY - outerR + 2.0f, outerR * 2.0f + 3.0f,
                outerR * 2.0f + 3.0f);

  // Outer skirt (near-black) with radial gradient
  juce::ColourGradient skirtGradient(
      juce::Colour(0xff2c2c2c), centre.translated(-outerR * 0.3f, -outerR * 0.3f),
      juce::Colour(0xff0a0a0a), centre.translated(outerR * 0.4f, outerR * 0.4f), true);
  g.setGradientFill(skirtGradient);
  g.fillEllipse(centreX - outerR, centreY - outerR, outerR * 2.0f, outerR * 2.0f);

  // Thin rim highlight
  g.setColour(juce::Colour(0xff484848));
  g.drawEllipse(centreX - outerR, centreY - outerR, outerR * 2.0f, outerR * 2.0f, 1.0f);

  // Knurling band: fine radial tick marks
  g.setColour(juce::Colour(0xff404040));
  for (int i = 0; i < knurlingCount; ++i)
  {
    float angle = juce::MathConstants<float>::twoPi * (float)i / (float)knurlingCount -
                  juce::MathConstants<float>::halfPi;
    float cosA = std::cos(angle);
    float sinA = std::sin(angle);
    g.drawLine(centreX + knurlingInner * cosA, centreY + knurlingInner * sinA,
               centreX + knurlingOuter * cosA, centreY + knurlingOuter * sinA, 1.5f);
  }

  // Inner silver cap with convex-mirror radial gradient
  juce::ColourGradient capGradient(
      juce::Colour(0xfffafafa), centre.translated(-capR * 0.30f, -capR * 0.35f),
      juce::Colour(0xff8c8c8c), centre.translated(capR * 0.50f, capR * 0.55f), true);
  g.setGradientFill(capGradient);
  g.fillEllipse(centreX - capR, centreY - capR, capR * 2.0f, capR * 2.0f);

  // Cap border ring
  g.setColour(juce::Colour(0xff888888));
  g.drawEllipse(centreX - capR, centreY - capR, capR * 2.0f, capR * 2.0f, 1.0f);

  if (isCompact)
  {
    // White line on the outer skirt, same as the large knob style
    float cosA = std::cos(toAngle - juce::MathConstants<float>::halfPi);
    float sinA = std::sin(toAngle - juce::MathConstants<float>::halfPi);
    g.setColour(juce::Colours::white);
    g.drawLine(centreX + knurlingOuter * cosA, centreY + knurlingOuter * sinA,
               centreX + outerR * 0.90f * cosA, centreY + outerR * 0.90f * sinA, 1.5f);
  }
  else
  {
    // White indicator line on the outer skirt
    float cosA = std::cos(toAngle - juce::MathConstants<float>::halfPi);
    float sinA = std::sin(toAngle - juce::MathConstants<float>::halfPi);
    g.setColour(juce::Colours::white);
    g.drawLine(centreX + knurlingOuter * cosA, centreY + knurlingOuter * sinA,
               centreX + outerR * 0.90f * cosA, centreY + outerR * 0.90f * sinA, 1.5f);

    // External tick marks around the rotation arc
    const int numTicks = 11;
    const float tickInner = outerR + 3.0f;
    const float tickOuter = outerR + 9.0f;
    g.setColour(juce::Colour(0xff888888));
    for (int i = 0; i < numTicks; ++i)
    {
      float tickAngle =
          rotaryStartAngle + (float)i / (float)(numTicks - 1) * (rotaryEndAngle - rotaryStartAngle);
      float tCosA = std::cos(tickAngle - juce::MathConstants<float>::halfPi);
      float tSinA = std::sin(tickAngle - juce::MathConstants<float>::halfPi);
      g.drawLine(centreX + tickInner * tCosA, centreY + tickInner * tSinA,
                 centreX + tickOuter * tCosA, centreY + tickOuter * tSinA, 1.5f);
    }
  }
}

void OctoberLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                              const juce::Colour& /*backgroundColour*/,
                                              bool /*shouldDrawButtonAsHighlighted*/,
                                              bool shouldDrawButtonAsDown)
{
  auto bounds = button.getLocalBounds().toFloat().reduced(0.5f, 0.5f);
  const auto cornerSize = 4.0f;
  float alpha = button.isEnabled() ? 1.0f : 0.5f;

  if (!shouldDrawButtonAsDown)
  {
    juce::Path shadowPath;
    shadowPath.addRoundedRectangle(bounds, cornerSize);
    juce::DropShadow(juce::Colours::black.withAlpha(0.45f), 4, {0, 2}).drawForPath(g, shadowPath);
  }

  juce::Colour topColour =
      shouldDrawButtonAsDown ? juce::Colour(0xff242424) : juce::Colour(0xff383838);
  juce::Colour bottomColour =
      shouldDrawButtonAsDown ? juce::Colour(0xff303030) : juce::Colour(0xff242424);

  juce::ColourGradient gradient(topColour.withMultipliedAlpha(alpha), bounds.getX(), bounds.getY(),
                                bottomColour.withMultipliedAlpha(alpha), bounds.getX(),
                                bounds.getBottom(), false);
  g.setGradientFill(gradient);
  g.fillRoundedRectangle(bounds, cornerSize);

  g.setColour(juce::Colour(0xff505050).withMultipliedAlpha(alpha));
  g.drawLine(bounds.getX() + cornerSize, bounds.getY() + 1.0f, bounds.getRight() - cornerSize,
             bounds.getY() + 1.0f, 1.0f);
  g.drawLine(bounds.getX() + 1.0f, bounds.getY() + cornerSize, bounds.getX() + 1.0f,
             bounds.getBottom() - cornerSize, 1.0f);

  g.setColour(juce::Colour(0xff141414).withMultipliedAlpha(alpha));
  g.drawLine(bounds.getX() + cornerSize, bounds.getBottom() - 1.0f, bounds.getRight() - cornerSize,
             bounds.getBottom() - 1.0f, 1.0f);
  g.drawLine(bounds.getRight() - 1.0f, bounds.getY() + cornerSize, bounds.getRight() - 1.0f,
             bounds.getBottom() - cornerSize, 1.0f);

  g.setColour(juce::Colour(0xff181818).withMultipliedAlpha(alpha));
  g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
}

void OctoberLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                        bool /*isHighlighted*/, bool isButtonDown)
{
  if (button.getComponentID() == "loadButton")
  {
    const float pad = 6.5f;
    const auto lb = button.getLocalBounds().toFloat();
    const float side = juce::jmin(lb.getWidth(), lb.getHeight()) - pad * 2.0f;
    auto b = juce::Rectangle<float>(side, side).withCentre(lb.getCentre());
    const float offset = isButtonDown ? 1.0f : 0.0f;
    b.translate(offset, offset);

    const float r = juce::jmin(3.0f, b.getHeight() * 0.15f);
    const float tabW = b.getWidth() * 0.44f;
    const float tabH = b.getHeight() * 0.20f;
    const float slopeW = tabH * 0.75f;

    juce::Path folder;
    folder.startNewSubPath(b.getX(), b.getY() + r);
    folder.quadraticTo(b.getX(), b.getY(), b.getX() + r, b.getY());
    folder.lineTo(b.getX() + tabW, b.getY());
    folder.cubicTo(b.getX() + tabW + slopeW * 0.5f, b.getY(), b.getX() + tabW + slopeW,
                   b.getY() + tabH - r, b.getX() + tabW + slopeW, b.getY() + tabH);
    folder.lineTo(b.getRight() - r, b.getY() + tabH);
    folder.quadraticTo(b.getRight(), b.getY() + tabH, b.getRight(), b.getY() + tabH + r);
    folder.lineTo(b.getRight(), b.getBottom() - r);
    folder.quadraticTo(b.getRight(), b.getBottom(), b.getRight() - r, b.getBottom());
    folder.lineTo(b.getX() + r, b.getBottom());
    folder.quadraticTo(b.getX(), b.getBottom(), b.getX(), b.getBottom() - r);
    folder.closeSubPath();

    g.setColour(juce::Colours::white.withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f));
    g.fillPath(folder);
    return;
  }

  auto height = juce::jmin(13.0f, (float)button.getHeight() * 0.6f);
  g.setFont(juce::Font(juce::FontOptions().withTypeface(cutiveMonoTypeface_).withHeight(height)));
  g.setColour(button
                  .findColour(button.getToggleState() ? juce::TextButton::textColourOnId
                                                      : juce::TextButton::textColourOffId)
                  .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f));

  const int offset = isButtonDown ? 1 : 0;
  g.drawFittedText(button.getButtonText(), offset, offset, button.getWidth(), button.getHeight(),
                   juce::Justification::centred, 1);
}

void OctoberLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                          bool /*shouldDrawButtonAsHighlighted*/,
                                          bool /*shouldDrawButtonAsDown*/)
{
  auto bounds = button.getLocalBounds().toFloat().reduced(0.5f, 0.5f);
  const auto cornerSize = 4.0f;
  bool isOn = button.getToggleState();
  float alpha = button.isEnabled() ? 1.0f : 0.5f;

  if (!isOn)
  {
    juce::Path shadowPath;
    shadowPath.addRoundedRectangle(bounds, cornerSize);
    juce::DropShadow(juce::Colours::black.withAlpha(0.45f), 4, {0, 2}).drawForPath(g, shadowPath);
  }

  if (isOn)
  {
    juce::ColourGradient gradient(juce::Colour(0xff1e1e1e).withMultipliedAlpha(alpha),
                                  bounds.getX(), bounds.getY(),
                                  juce::Colour(0xff2e2e2e).withMultipliedAlpha(alpha),
                                  bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(bounds, cornerSize);

    g.setColour(juce::Colour(0xff141414).withMultipliedAlpha(alpha));
    g.drawLine(bounds.getX() + cornerSize, bounds.getY() + 1.0f, bounds.getRight() - cornerSize,
               bounds.getY() + 1.0f, 1.0f);
    g.drawLine(bounds.getX() + 1.0f, bounds.getY() + cornerSize, bounds.getX() + 1.0f,
               bounds.getBottom() - cornerSize, 1.0f);

    g.setColour(juce::Colour(0xff505050).withMultipliedAlpha(alpha));
    g.drawLine(bounds.getX() + cornerSize, bounds.getBottom() - 1.0f,
               bounds.getRight() - cornerSize, bounds.getBottom() - 1.0f, 1.0f);
    g.drawLine(bounds.getRight() - 1.0f, bounds.getY() + cornerSize, bounds.getRight() - 1.0f,
               bounds.getBottom() - cornerSize, 1.0f);
  }
  else
  {
    juce::ColourGradient gradient(juce::Colour(0xff383838).withMultipliedAlpha(alpha),
                                  bounds.getX(), bounds.getY(),
                                  juce::Colour(0xff242424).withMultipliedAlpha(alpha),
                                  bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(bounds, cornerSize);

    g.setColour(juce::Colour(0xff505050).withMultipliedAlpha(alpha));
    g.drawLine(bounds.getX() + cornerSize, bounds.getY() + 1.0f, bounds.getRight() - cornerSize,
               bounds.getY() + 1.0f, 1.0f);
    g.drawLine(bounds.getX() + 1.0f, bounds.getY() + cornerSize, bounds.getX() + 1.0f,
               bounds.getBottom() - cornerSize, 1.0f);

    g.setColour(juce::Colour(0xff141414).withMultipliedAlpha(alpha));
    g.drawLine(bounds.getX() + cornerSize, bounds.getBottom() - 1.0f,
               bounds.getRight() - cornerSize, bounds.getBottom() - 1.0f, 1.0f);
    g.drawLine(bounds.getRight() - 1.0f, bounds.getY() + cornerSize, bounds.getRight() - 1.0f,
               bounds.getBottom() - cornerSize, 1.0f);
  }

  g.setColour(juce::Colour(0xff181818).withMultipliedAlpha(alpha));
  g.drawRoundedRectangle(bounds, cornerSize, 1.0f);

  // LED strip on left
  const float ledWidth = 8.0f;
  auto ledColour = juce::Colour(0xffe07030);
  auto ledRect = bounds.withWidth(ledWidth);

  if (isOn)
  {
    auto ledCentre = ledRect.getCentre();
    const float glowRadius = 18.0f;
    juce::ColourGradient glow(ledColour.withAlpha(0.28f), ledCentre, ledColour.withAlpha(0.0f),
                              ledCentre.translated(glowRadius, 0.0f), true);
    g.setGradientFill(glow);
    g.fillRect(ledRect.expanded(glowRadius));

    juce::ColourGradient stripGlow(juce::Colour(0xffffb060).withMultipliedAlpha(alpha), ledCentre,
                                   ledColour.darker(0.25f).withMultipliedAlpha(alpha),
                                   ledCentre.translated(0.0f, ledRect.getHeight() * 0.5f), true);
    g.setGradientFill(stripGlow);
    g.fillRoundedRectangle(ledRect, cornerSize);
  }
  else
  {
    g.setColour(ledColour.withSaturation(0.25f).withBrightness(0.12f).withMultipliedAlpha(alpha));
    g.fillRoundedRectangle(ledRect, cornerSize);
  }

  g.setColour(juce::Colours::white.withMultipliedAlpha(alpha * 0.9f));
  g.setFont(juce::Font(juce::FontOptions().withTypeface(cutiveMonoTypeface_).withHeight(13.0f)));
  g.drawFittedText(button.getButtonText(),
                   button.getLocalBounds().withTrimmedLeft(static_cast<int>(ledWidth)),
                   juce::Justification::centred, 1);
}

void OctoberLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                                      int /*buttonX*/, int /*buttonY*/, int /*buttonW*/,
                                      int /*buttonH*/, juce::ComboBox& box)
{
  const auto cornerSize = 3.0f;
  juce::Rectangle<float> boxBounds(0.5f, 0.5f, (float)width - 1.0f, (float)height - 1.0f);

  juce::Colour topColour = isButtonDown ? juce::Colour(0xff242424) : juce::Colour(0xff383838);
  juce::Colour bottomColour = isButtonDown ? juce::Colour(0xff303030) : juce::Colour(0xff242424);

  juce::ColourGradient gradient(topColour, boxBounds.getX(), boxBounds.getY(), bottomColour,
                                boxBounds.getX(), boxBounds.getBottom(), false);
  g.setGradientFill(gradient);
  g.fillRoundedRectangle(boxBounds, cornerSize);

  g.setColour(juce::Colour(0xff505050));
  g.drawLine(boxBounds.getX() + cornerSize, boxBounds.getY() + 1.0f,
             boxBounds.getRight() - cornerSize, boxBounds.getY() + 1.0f, 1.0f);
  g.drawLine(boxBounds.getX() + 1.0f, boxBounds.getY() + cornerSize, boxBounds.getX() + 1.0f,
             boxBounds.getBottom() - cornerSize, 1.0f);

  g.setColour(juce::Colour(0xff141414));
  g.drawLine(boxBounds.getX() + cornerSize, boxBounds.getBottom() - 1.0f,
             boxBounds.getRight() - cornerSize, boxBounds.getBottom() - 1.0f, 1.0f);
  g.drawLine(boxBounds.getRight() - 1.0f, boxBounds.getY() + cornerSize,
             boxBounds.getRight() - 1.0f, boxBounds.getBottom() - cornerSize, 1.0f);

  g.setColour(juce::Colour(0xff181818));
  g.drawRoundedRectangle(boxBounds, cornerSize, 1.0f);

  juce::Rectangle<int> arrowZone(width - 28, 0, 18, height);
  juce::Path arrow;
  arrow.startNewSubPath((float)arrowZone.getX() + 2.0f, (float)arrowZone.getCentreY() - 2.0f);
  arrow.lineTo((float)arrowZone.getCentreX(), (float)arrowZone.getCentreY() + 3.0f);
  arrow.lineTo((float)arrowZone.getRight() - 2.0f, (float)arrowZone.getCentreY() - 2.0f);

  g.setColour(
      box.findColour(juce::ComboBox::arrowColourId).withAlpha(box.isEnabled() ? 0.9f : 0.2f));
  g.strokePath(arrow, juce::PathStrokeType(2.0f));
}

void OctoberLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
  if (dynamic_cast<juce::Slider*>(label.getParentComponent()) != nullptr)
  {
    drawLCDBackground(g, label.getLocalBounds().toFloat());

    if (!label.isBeingEdited())
    {
      g.setFont(getLabelFont(label));
      g.setColour(label.findColour(juce::Label::textColourId));
      g.drawFittedText(label.getText(), label.getLocalBounds().reduced(3, 1),
                       label.getJustificationType(), 1, 1.0f);
    }
  }
  else
  {
    LookAndFeel_V4::drawLabel(g, label);
  }
}

juce::Typeface::Ptr OctoberLookAndFeel::getTypefaceForFont(const juce::Font&)
{
  return cutiveMonoTypeface_;
}

juce::Font OctoberLookAndFeel::getLabelFont(juce::Label& label)
{
  auto f = LookAndFeel_V4::getLabelFont(label);
  bool isSliderTextBox = dynamic_cast<juce::Slider*>(label.getParentComponent()) != nullptr;
  if (isSliderTextBox)
    return juce::Font(juce::FontOptions().withTypeface(lcdTypeface_).withHeight(8.0f));
  return juce::Font(
      juce::FontOptions().withTypeface(cutiveMonoTypeface_).withHeight(f.getHeight()));
}
