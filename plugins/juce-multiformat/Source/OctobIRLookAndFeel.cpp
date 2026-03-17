#include "OctobIRLookAndFeel.h"

#include <BinaryData.h>

OctobIRLookAndFeel::OctobIRLookAndFeel()
{
  cutiveMonoTypeface_ = juce::Typeface::createSystemTypefaceFor(
      BinaryData::CourierPrimeRegular_ttf, BinaryData::CourierPrimeRegular_ttfSize);
  lcdTypeface_ = juce::Typeface::createSystemTypefaceFor(BinaryData::PressStart2PRegular_ttf,
                                                         BinaryData::PressStart2PRegular_ttfSize);
  setDefaultSansSerifTypeface(cutiveMonoTypeface_);
  setColour(juce::ResizableWindow::backgroundColourId, juce::Colours::white);

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
  setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

  setColour(juce::Label::textColourId, juce::Colour(0xff1a1a1a));
}

void OctobIRLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float rotaryStartAngle,
                                          float rotaryEndAngle, juce::Slider& /*slider*/)
{
  auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(10.0f);
  auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
  auto centreX = bounds.getCentreX();
  auto centreY = bounds.getCentreY();
  auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
  auto capRadius = radius * 0.72f;

  // Drop shadow
  g.setColour(juce::Colour(0xff000000).withAlpha(0.18f));
  g.fillEllipse(centreX - capRadius + 0.5f, centreY - capRadius + 2.0f, capRadius * 2.0f + 3.0f,
                capRadius * 2.0f + 3.0f);

  // Knob body with radial gradient (light gray)
  juce::Point<float> centre(centreX, centreY);
  juce::ColourGradient bodyGradient(
      juce::Colour(0xffe8e8e8), centre.translated(-capRadius * 0.35f, -capRadius * 0.35f),
      juce::Colour(0xffc0c0c0), centre.translated(capRadius * 0.55f, capRadius * 0.55f), true);
  g.setGradientFill(bodyGradient);
  g.fillEllipse(centreX - capRadius, centreY - capRadius, capRadius * 2.0f, capRadius * 2.0f);

  // Bevel ring: outer shadow, inner highlight
  g.setColour(juce::Colour(0xffb0b0b0));
  g.drawEllipse(centreX - capRadius, centreY - capRadius, capRadius * 2.0f, capRadius * 2.0f, 2.0f);
  auto innerRad = capRadius - 1.5f;
  g.setColour(juce::Colour(0xffffffff));
  g.drawEllipse(centreX - innerRad, centreY - innerRad, innerRad * 2.0f, innerRad * 2.0f, 1.0f);

  // Tick marks
  auto tickRadius = radius - 2.0f;
  const int numTicks = 11;
  for (int i = 0; i < numTicks; ++i)
  {
    float tickAngle =
        rotaryStartAngle + (float)i / (float)(numTicks - 1) * (rotaryEndAngle - rotaryStartAngle);
    bool isMajor = (i == 0 || i == 5 || i == 10);
    float tickLen = isMajor ? 5.0f : 3.0f;

    float cosA = std::cos(tickAngle - juce::MathConstants<float>::halfPi);
    float sinA = std::sin(tickAngle - juce::MathConstants<float>::halfPi);

    float x1 = centreX + (tickRadius - tickLen) * cosA;
    float y1 = centreY + (tickRadius - tickLen) * sinA;
    float x2 = centreX + tickRadius * cosA;
    float y2 = centreY + tickRadius * sinA;

    g.setColour(juce::Colour(0xff888888));
    g.drawLine(x1, y1, x2, y2, 1.5f);
  }

  // Indicator line with orange colour and dot at outer tip
  {
    float cosA = std::cos(toAngle - juce::MathConstants<float>::halfPi);
    float sinA = std::sin(toAngle - juce::MathConstants<float>::halfPi);

    float lx1 = centreX + 0.15f * capRadius * cosA;
    float ly1 = centreY + 0.15f * capRadius * sinA;
    float lx2 = centreX + 0.85f * capRadius * cosA;
    float ly2 = centreY + 0.85f * capRadius * sinA;

    g.setColour(juce::Colour(0xffe07030));
    g.drawLine(lx1, ly1, lx2, ly2, 2.5f);
    g.fillEllipse(lx2 - 2.0f, ly2 - 2.0f, 4.0f, 4.0f);
  }

  // Center cap
  auto centerCapRadius = capRadius * 0.18f;
  g.setColour(juce::Colour(0xffd8d8d8));
  g.fillEllipse(centreX - centerCapRadius, centreY - centerCapRadius, centerCapRadius * 2.0f,
                centerCapRadius * 2.0f);
  g.setColour(juce::Colour(0xffaaaaaa));
  g.drawEllipse(centreX - centerCapRadius, centreY - centerCapRadius, centerCapRadius * 2.0f,
                centerCapRadius * 2.0f, 1.0f);
}

void OctobIRLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
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
                                          bool /*shouldDrawButtonAsHighlighted*/,
                                          bool /*shouldDrawButtonAsDown*/)
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
        isOn ? juce::Colour(0xffffb060) : ledColour.withSaturation(0.3f).withBrightness(0.08f);
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
    float alpha = button.isEnabled() ? 1.0f : 0.5f;

    if (!isOn)
    {
      juce::Path shadowPath;
      shadowPath.addRoundedRectangle(bounds, cornerSize);
      juce::DropShadow(juce::Colours::black.withAlpha(0.45f), 4, {0, 2}).drawForPath(g, shadowPath);
    }

    if (isOn)
    {
      // Pushed-in: reversed gradient (darker top, lighter bottom)
      juce::ColourGradient gradient(juce::Colour(0xff1e1e1e).withMultipliedAlpha(alpha),
                                    bounds.getX(), bounds.getY(),
                                    juce::Colour(0xff2e2e2e).withMultipliedAlpha(alpha),
                                    bounds.getX(), bounds.getBottom(), false);
      g.setGradientFill(gradient);
      g.fillRoundedRectangle(bounds, cornerSize);

      // Inset shadow on top and left
      g.setColour(juce::Colour(0xff141414).withMultipliedAlpha(alpha));
      g.drawLine(bounds.getX() + cornerSize, bounds.getY() + 1.0f, bounds.getRight() - cornerSize,
                 bounds.getY() + 1.0f, 1.0f);
      g.drawLine(bounds.getX() + 1.0f, bounds.getY() + cornerSize, bounds.getX() + 1.0f,
                 bounds.getBottom() - cornerSize, 1.0f);

      // Highlight on bottom and right
      g.setColour(juce::Colour(0xff505050).withMultipliedAlpha(alpha));
      g.drawLine(bounds.getX() + cornerSize, bounds.getBottom() - 1.0f,
                 bounds.getRight() - cornerSize, bounds.getBottom() - 1.0f, 1.0f);
      g.drawLine(bounds.getRight() - 1.0f, bounds.getY() + cornerSize, bounds.getRight() - 1.0f,
                 bounds.getBottom() - cornerSize, 1.0f);
    }
    else
    {
      // Raised: matches TextButton / SWAP button style
      juce::ColourGradient gradient(juce::Colour(0xff383838).withMultipliedAlpha(alpha),
                                    bounds.getX(), bounds.getY(),
                                    juce::Colour(0xff242424).withMultipliedAlpha(alpha),
                                    bounds.getX(), bounds.getBottom(), false);
      g.setGradientFill(gradient);
      g.fillRoundedRectangle(bounds, cornerSize);

      // Highlight on top and left
      g.setColour(juce::Colour(0xff505050).withMultipliedAlpha(alpha));
      g.drawLine(bounds.getX() + cornerSize, bounds.getY() + 1.0f, bounds.getRight() - cornerSize,
                 bounds.getY() + 1.0f, 1.0f);
      g.drawLine(bounds.getX() + 1.0f, bounds.getY() + cornerSize, bounds.getX() + 1.0f,
                 bounds.getBottom() - cornerSize, 1.0f);

      // Shadow on bottom and right
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
    g.drawFittedText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred,
                     1);
  }
}

void OctobIRLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
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

void OctobIRLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
  if (dynamic_cast<juce::Slider*>(label.getParentComponent()) != nullptr)
  {
    auto bounds = label.getLocalBounds().toFloat();
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

juce::Typeface::Ptr OctobIRLookAndFeel::getTypefaceForFont(const juce::Font&)
{
  return cutiveMonoTypeface_;
}

juce::Font OctobIRLookAndFeel::getLabelFont(juce::Label& label)
{
  auto f = LookAndFeel_V4::getLabelFont(label);
  bool isSliderTextBox = dynamic_cast<juce::Slider*>(label.getParentComponent()) != nullptr;
  if (isSliderTextBox)
    return juce::Font(juce::FontOptions().withTypeface(lcdTypeface_).withHeight(8.0f));
  return juce::Font(
      juce::FontOptions().withTypeface(cutiveMonoTypeface_).withHeight(f.getHeight()));
}
