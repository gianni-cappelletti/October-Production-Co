#include <BinaryData.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "LCDPainting.h"
#include "OctoberLookAndFeel.h"

namespace
{

constexpr float RenderScale = 2.0f;
constexpr float DisplayScale = 1.3f;
constexpr float PixelScale = RenderScale * DisplayScale;
constexpr float PedalWidth = 760.0f;
constexpr float PedalHeight = 395.0f;
constexpr float DividerY = 305.0f;
constexpr float Pad = 15.0f;
constexpr float LineSlope = 86.4f / 540.0f;
constexpr float LeftLineTopX = 265.6f;
constexpr float RightLineTopX = 405.6f;
constexpr float LabelHeight = 16.0f;
constexpr float KnobWidth = 86.0f;
constexpr float KnobHeight = 68.0f;
constexpr float ValueHeight = 22.0f;
constexpr float FullRowHeight = LabelHeight + KnobHeight + ValueHeight;
constexpr float Row1Y = 29.0f;
constexpr float Row2Y = 179.0f;
constexpr float RightZoneX = Pad + 220.0f + 160.0f;
constexpr float RightZoneWidth = PedalWidth - Pad - RightZoneX;
constexpr float LcdHeight = 30.0f;
constexpr float FootswitchWidth = 56.0f;
constexpr float FootswitchHeight = 64.0f;
constexpr float FootswitchSpacing = 180.0f;
constexpr float FootswitchRowCentreX = 380.0f;
constexpr float FootswitchLabelY = DividerY + 6.0f;
constexpr float FootswitchY = FootswitchLabelY + LabelHeight;
constexpr std::array<float, 3> FileLcdY{{167.0f, 213.0f, 259.0f}};
constexpr float FileLcdInset = 2.0f;
constexpr float FileLcdX = RightZoneX + FileLcdInset;
constexpr float FileLcdWidth = RightZoneWidth - 2.0f * FileLcdInset;
constexpr float ButtonMargin = 8.0f;
constexpr int KnobFrames = 65;
constexpr float BadgeWidth = 84.0f;
constexpr float BadgeHeight = 28.0f;
constexpr float BadgeCornerRadius = 4.0f;
constexpr float BadgeTextHeight = 11.0f;
constexpr float BadgeCentreX = Pad + 161.0f;
constexpr float BadgeCentreY = Row2Y + LabelHeight + KnobHeight * 0.5f;
constexpr const char* BadgeText = "OPC";
constexpr const char* PedalClass = "mod-pedal-octobass";
constexpr const char* FilePlaceholder = "-- none --";

const juce::Colour PanelColour{0xfff0f0f2};
const juce::Colour LineColour{0xff1a1a1a};
const juce::Colour LabelColour{0xff1a1a1a};

enum class Widget
{
  Knob,
  ModeKnob,
  Footswitch,
  Bypass,
};

struct Box
{
  float x;
  float y;
  float w;
  float h;
};

struct Control
{
  std::string symbol;
  Widget widget;
  std::string label;
  Box widgetBox;
  Box labelBox;
  bool hasValue;
  float defaultNormalised;
  std::string defaultText;
};

struct FileSlot
{
  std::string uriSuffix;
  Box lcdBox;
};

float lineXAtY(float topX, float y)
{
  return topX - LineSlope * y;
}

Control knob(const std::string& symbol, const std::string& label, float columnX, float columnW,
             float rowY, Widget widget, float defaultNormalised, const std::string& defaultText)
{
  const float knobX = columnX + (columnW - KnobWidth) * 0.5f;
  return {symbol,
          widget,
          label,
          {knobX, rowY + LabelHeight, KnobWidth, KnobHeight},
          {columnX, rowY, columnW, LabelHeight},
          true,
          defaultNormalised,
          defaultText};
}

std::vector<Control> buildControls()
{
  std::vector<Control> controls;
  const float halfLeft = 110.0f;
  controls.push_back(knob("squash", "SQUASH", Pad, halfLeft, Row1Y, Widget::Knob, 0.0f, "0%"));
  controls.push_back(knob("compression_mode", "MODE", Pad + halfLeft, halfLeft, Row1Y,
                          Widget::ModeKnob, 0.0f, "Tight"));
  controls.push_back(
      knob("low_level", "OUTPUT", Pad, halfLeft, Row2Y, Widget::Knob, 24.0f / 36.0f, "0.0 dB"));

  const float centreAreaW = 120.0f;
  const auto centreColumn = [&](float rowY)
  {
    const float centreY = rowY + FullRowHeight * 0.5f;
    const float cx = (lineXAtY(LeftLineTopX, centreY) + lineXAtY(RightLineTopX, centreY)) * 0.5f;
    return cx - centreAreaW * 0.5f;
  };
  controls.push_back(knob("crossover", "CROSSOVER", centreColumn(Row1Y), centreAreaW, Row1Y,
                          Widget::Knob, 200.0f / 750.0f, "250 Hz"));
  controls.push_back(
      knob("gate", "GATE", centreColumn(Row2Y), centreAreaW, Row2Y, Widget::Knob, 0.0f, "Off"));

  const float columnW = RightZoneWidth / 3.0f;
  controls.push_back(
      knob("high_input_gain", "INPUT", RightZoneX, columnW, Row1Y, Widget::Knob, 0.5f, "0.0 dB"));
  controls.push_back(
      knob("high_blend", "MIX", RightZoneX + columnW, columnW, Row1Y, Widget::Knob, 1.0f, "100%"));
  controls.push_back(knob("high_output_gain", "OUTPUT", RightZoneX + 2.0f * columnW, columnW, Row1Y,
                          Widget::Knob, 0.5f, "0.0 dB"));

  const std::array<std::pair<std::string, std::string>, 3> footswitches = {
      {{":bypass", "ON"}, {"nam1_on", "NAM 1"}, {"nam2_on", "NAM 2"}}};
  for (size_t i = 0; i < footswitches.size(); ++i)
  {
    const float centreX = FootswitchRowCentreX + (static_cast<float>(i) - 1.0f) * FootswitchSpacing;
    const auto& [symbol, label] = footswitches[i];
    controls.push_back(
        {symbol,
         symbol == ":bypass" ? Widget::Bypass : Widget::Footswitch,
         label,
         {centreX - FootswitchWidth * 0.5f, FootswitchY, FootswitchWidth, FootswitchHeight},
         {centreX - FootswitchSpacing * 0.5f, FootswitchLabelY, FootswitchSpacing, LabelHeight},
         false,
         1.0f,
         ""});
  }
  return controls;
}

std::vector<FileSlot> buildFileSlots()
{
  const std::array<std::string, 3> parameters = {"nam1Model", "nam2Model", "cabinetIr"};
  std::vector<FileSlot> slots;
  for (size_t i = 0; i < parameters.size(); ++i)
    slots.push_back({parameters[i], {FileLcdX, FileLcdY[i], FileLcdWidth, LcdHeight}});
  return slots;
}

juce::Image makeImage(float w, float h)
{
  return juce::Image(juce::Image::ARGB, juce::roundToInt(w * PixelScale),
                     juce::roundToInt(h * PixelScale), true);
}

void paintComponentInto(juce::Image& image, juce::Component& component, float originX,
                        float originY, float scaleX, float scaleY)
{
  juce::Graphics g(image);
  g.addTransform(juce::AffineTransform::scale(scaleX, scaleY).translated(originX, originY));
  component.paintEntireComponent(g, false);
}

void configureKnob(juce::Slider& slider, Widget widget)
{
  slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
  slider.setRange(0.0, 1.0);
  if (widget == Widget::ModeKnob)
  {
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.5f,
                               juce::MathConstants<float>::pi * 2.5f, true);
    slider.getProperties().set("numTicks", 4);
  }
  else
  {
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                               juce::MathConstants<float>::pi * 2.75f, true);
  }
}

int frameCount(Widget widget)
{
  switch (widget)
  {
    case Widget::Knob:
      return KnobFrames;
    case Widget::ModeKnob:
      return 4;
    case Widget::Footswitch:
    case Widget::Bypass:
      return 2;
  }
  return 1;
}

float frameMargin(Widget widget)
{
  return widget == Widget::Knob || widget == Widget::ModeKnob ? 0.0f : ButtonMargin;
}

void drawFootswitch(juce::Graphics& g, juce::Rectangle<float> bounds, bool lit)
{
  const juce::Colour ledColour(0xffe07030);
  const float ledRadius = 5.0f;
  const juce::Point<float> led(bounds.getCentreX(), bounds.getY() + ledRadius + 2.0f);
  if (lit)
  {
    juce::ColourGradient glow(ledColour.withAlpha(0.35f), led, ledColour.withAlpha(0.0f),
                              led.translated(ledRadius * 3.0f, 0.0f), true);
    g.setGradientFill(glow);
    g.fillEllipse(juce::Rectangle<float>(ledRadius * 6.0f, ledRadius * 6.0f).withCentre(led));
    juce::ColourGradient body(juce::Colour(0xffffb060), led.translated(-1.5f, -1.5f),
                              ledColour.darker(0.25f), led.translated(ledRadius, ledRadius), true);
    g.setGradientFill(body);
  }
  else
  {
    g.setColour(ledColour.withSaturation(0.25f).withBrightness(0.12f));
  }
  const auto ledBounds = juce::Rectangle<float>(ledRadius * 2.0f, ledRadius * 2.0f).withCentre(led);
  g.fillEllipse(ledBounds);
  g.setColour(juce::Colour(0xff1a1a1a));
  g.drawEllipse(ledBounds, 1.0f);

  const float nutRadius = 24.0f;
  const juce::Point<float> centre(bounds.getCentreX(), bounds.getBottom() - nutRadius - 2.0f);
  const auto hexagon = [&centre](float radius)
  {
    juce::Path path;
    for (int i = 0; i < 6; ++i)
    {
      const float angle = juce::MathConstants<float>::pi / 3.0f * static_cast<float>(i);
      const juce::Point<float> vertex(centre.x + radius * std::cos(angle),
                                      centre.y + radius * std::sin(angle));
      if (i == 0)
        path.startNewSubPath(vertex);
      else
        path.lineTo(vertex);
    }
    path.closeSubPath();
    return path;
  };
  const auto circle = [&centre](float radius)
  { return juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre); };

  const auto nut = hexagon(nutRadius);
  g.setColour(juce::Colours::black.withAlpha(0.28f));
  g.fillPath(nut, juce::AffineTransform::translation(1.5f, 2.5f));

  juce::ColourGradient nutFill(juce::Colour(0xffe2e2e2), centre.x - nutRadius, centre.y - nutRadius,
                               juce::Colour(0xff8e8e8e), centre.x + nutRadius, centre.y + nutRadius,
                               false);
  g.setGradientFill(nutFill);
  g.fillPath(nut);

  g.setColour(juce::Colours::white.withAlpha(0.55f));
  g.strokePath(hexagon(nutRadius - 1.6f), juce::PathStrokeType(1.0f));
  g.setColour(juce::Colour(0xff4e4e4e));
  g.strokePath(nut, juce::PathStrokeType(1.2f));

  const float rimRadius = 15.5f;
  g.setColour(juce::Colours::black.withAlpha(0.3f));
  g.fillEllipse(circle(rimRadius + 1.5f).translated(0.0f, 1.0f));
  juce::ColourGradient rim(juce::Colour(0xff5a5a5a), centre.x, centre.y - rimRadius,
                           juce::Colour(0xff2a2a2a), centre.x, centre.y + rimRadius, false);
  g.setGradientFill(rim);
  g.fillEllipse(circle(rimRadius));

  const float capRadius = 13.5f;
  juce::ColourGradient chrome(juce::Colour(0xfff2f2f2), centre.x, centre.y - capRadius,
                              juce::Colour(0xfff6f6f6), centre.x, centre.y + capRadius, false);
  chrome.addColour(0.30, juce::Colour(0xffc4c4c4));
  chrome.addColour(0.52, juce::Colour(0xff8a8a8a));
  chrome.addColour(0.62, juce::Colour(0xffa8a8a8));
  chrome.addColour(0.85, juce::Colour(0xffdedede));
  g.setGradientFill(chrome);
  g.fillEllipse(circle(capRadius));

  g.setColour(juce::Colours::white.withAlpha(0.35f));
  g.fillEllipse(circle(capRadius)
                    .withSizeKeepingCentre(capRadius * 1.3f, capRadius * 0.5f)
                    .translated(0.0f, -capRadius * 0.55f));
  g.setColour(juce::Colour(0xff6a6a6a));
  g.drawEllipse(circle(capRadius), 0.8f);
}

void paintWidgetFrame(OctoberLookAndFeel& lookAndFeel, const Control& control, int frame,
                      juce::Image& image, float originX, float originY, float scaleX, float scaleY)
{
  const auto& box = control.widgetBox;
  const float marginX = frameMargin(control.widget) * scaleX;
  const float marginY = frameMargin(control.widget) * scaleY;
  const int width = juce::roundToInt(box.w);
  const int height = juce::roundToInt(box.h);

  switch (control.widget)
  {
    case Widget::Knob:
    case Widget::ModeKnob:
    {
      juce::Slider slider;
      slider.setLookAndFeel(&lookAndFeel);
      configureKnob(slider, control.widget);
      slider.setBounds(0, 0, width, height);
      slider.setValue(
          static_cast<double>(frame) / static_cast<double>(frameCount(control.widget) - 1),
          juce::dontSendNotification);
      paintComponentInto(image, slider, originX, originY, scaleX, scaleY);
      slider.setLookAndFeel(nullptr);
      break;
    }
    case Widget::Footswitch:
    case Widget::Bypass:
    {
      juce::Graphics g(image);
      g.addTransform(juce::AffineTransform::scale(scaleX, scaleY)
                         .translated(originX + marginX, originY + marginY));
      drawFootswitch(g, {0.0f, 0.0f, box.w, box.h}, frame == 1);
      break;
    }
  }
}

std::string spriteName(Widget widget)
{
  switch (widget)
  {
    case Widget::Knob:
      return "knob.png";
    case Widget::ModeKnob:
      return "knob-mode.png";
    case Widget::Footswitch:
    case Widget::Bypass:
      return "footswitch.png";
  }
  return {};
}

struct FrameSize
{
  float designWidth;
  float designHeight;
  int cssWidth;
  int cssHeight;
};

FrameSize frameSize(const Control& control)
{
  const float margin = frameMargin(control.widget);
  const float designWidth = control.widgetBox.w + margin * 2.0f;
  const float designHeight = control.widgetBox.h + margin * 2.0f;
  return {designWidth, designHeight, juce::roundToInt(designWidth * DisplayScale),
          juce::roundToInt(designHeight * DisplayScale)};
}

juce::Image renderSprite(OctoberLookAndFeel& lookAndFeel, const Control& control)
{
  const auto size = frameSize(control);
  const int frames = frameCount(control.widget);
  const float framePixelWidth = static_cast<float>(size.cssWidth) * RenderScale;
  const float framePixelHeight = static_cast<float>(size.cssHeight) * RenderScale;
  juce::Image image(juce::Image::ARGB, juce::roundToInt(framePixelWidth) * frames,
                    juce::roundToInt(framePixelHeight), true);
  for (int frame = 0; frame < frames; ++frame)
    paintWidgetFrame(lookAndFeel, control, frame, image,
                     static_cast<float>(frame) * framePixelWidth, 0.0f,
                     framePixelWidth / size.designWidth, framePixelHeight / size.designHeight);
  return image;
}

void drawText(juce::Graphics& g, const juce::String& text, const Box& box, const juce::Font& font,
              juce::Colour colour, juce::Justification justification)
{
  g.setColour(colour);
  g.setFont(font);
  g.drawText(text, juce::Rectangle<float>(box.x, box.y, box.w, box.h), justification, false);
}

juce::Rectangle<float> valueBox(const Control& control)
{
  return {control.widgetBox.x, control.widgetBox.y + KnobHeight, KnobWidth, ValueHeight};
}

juce::Image renderBackground(OctoberLookAndFeel& lookAndFeel, const std::vector<Control>& controls,
                             const std::vector<FileSlot>& fileSlots)
{
  auto image = makeImage(PedalWidth, PedalHeight);
  juce::Graphics g(image);
  g.addTransform(juce::AffineTransform::scale(PixelScale));

  const juce::Rectangle<float> bounds(0.0f, 0.0f, PedalWidth, PedalHeight);
  const float cornerRadius = 3.0f;
  g.setColour(PanelColour);
  g.fillRoundedRectangle(bounds, cornerRadius);
  g.setColour(juce::Colours::black.withAlpha(0.28f));
  g.drawRoundedRectangle(bounds.reduced(0.5f), cornerRadius, 1.0f);
  g.setColour(juce::Colours::black.withAlpha(0.14f));
  g.drawRoundedRectangle(bounds.reduced(1.5f), cornerRadius, 1.0f);
  g.setColour(juce::Colours::black.withAlpha(0.06f));
  g.drawRoundedRectangle(bounds.reduced(2.5f), cornerRadius, 1.0f);

  g.setColour(LineColour);
  g.drawLine(LeftLineTopX, 0.0f, lineXAtY(LeftLineTopX, DividerY), DividerY, 3.0f);
  g.drawLine(RightLineTopX, 0.0f, lineXAtY(RightLineTopX, DividerY), DividerY, 3.0f);
  g.drawLine(0.0f, DividerY, PedalWidth, DividerY, 3.0f);

  const juce::Font labelFont(
      juce::FontOptions(lookAndFeel.getTypefaceForFont(juce::Font(juce::FontOptions(15.0f))))
          .withHeight(15.0f));
  for (const auto& control : controls)
  {
    if (!control.label.empty())
      drawText(g, control.label, control.labelBox, labelFont, LabelColour,
               juce::Justification::centred);
    if (control.hasValue)
      drawLCDBackground(g, valueBox(control));
  }

  const auto badge =
      juce::Rectangle<float>(BadgeWidth, BadgeHeight).withCentre({BadgeCentreX, BadgeCentreY});
  g.setColour(LineColour);
  g.fillRoundedRectangle(badge, BadgeCornerRadius);
  drawText(g, BadgeText, {badge.getX(), badge.getY(), badge.getWidth(), badge.getHeight()},
           juce::Font(juce::FontOptions(lookAndFeel.getLCDTypeface()).withHeight(BadgeTextHeight)),
           kLCDBacklightColour, juce::Justification::centred);
  return image;
}

int fileLcdCssWidth()
{
  return juce::roundToInt(FileLcdWidth * DisplayScale);
}

int fileLcdCssHeight()
{
  return juce::roundToInt(LcdHeight * DisplayScale);
}

juce::Image renderFileLcdSprite()
{
  const float framePixelWidth = static_cast<float>(fileLcdCssWidth()) * RenderScale;
  const float framePixelHeight = static_cast<float>(fileLcdCssHeight()) * RenderScale;
  juce::Image image(juce::Image::ARGB, juce::roundToInt(framePixelWidth) * 2,
                    juce::roundToInt(framePixelHeight), true);
  const std::array<juce::Colour, 2> backlights = {kLCDBacklightColour, kLCDDimBacklightColour};
  for (size_t frame = 0; frame < backlights.size(); ++frame)
  {
    juce::Graphics g(image);
    g.addTransform(
        juce::AffineTransform::scale(framePixelWidth / FileLcdWidth, framePixelHeight / LcdHeight)
            .translated(static_cast<float>(frame) * framePixelWidth, 0.0f));
    drawLCDBackground(g, {0.0f, 0.0f, FileLcdWidth, LcdHeight}, backlights[frame]);
  }
  return image;
}

juce::Image renderScreenshot(OctoberLookAndFeel& lookAndFeel, const juce::Image& background,
                             const std::vector<Control>& controls,
                             const std::vector<FileSlot>& fileSlots)
{
  auto image = background.createCopy();
  for (const auto& control : controls)
  {
    const float margin = frameMargin(control.widget);
    const int frames = frameCount(control.widget);
    const int frame = juce::roundToInt(control.defaultNormalised * static_cast<float>(frames - 1));
    paintWidgetFrame(lookAndFeel, control, frame, image,
                     (control.widgetBox.x - margin) * PixelScale,
                     (control.widgetBox.y - margin) * PixelScale, PixelScale, PixelScale);
  }

  juce::Graphics g(image);
  g.addTransform(juce::AffineTransform::scale(PixelScale));
  const juce::Font valueFont(juce::FontOptions(lookAndFeel.getLCDTypeface()).withHeight(9.0f));
  for (const auto& control : controls)
    if (control.hasValue)
    {
      const auto box = valueBox(control);
      drawText(g, control.defaultText, {box.getX(), box.getY(), box.getWidth(), box.getHeight()},
               valueFont, kLCDInkColour, juce::Justification::centred);
    }

  const juce::Font fileFont(juce::FontOptions(lookAndFeel.getLCDTypeface()).withHeight(10.0f));
  for (const auto& slot : fileSlots)
    drawLCDBackground(g, {slot.lcdBox.x, slot.lcdBox.y, slot.lcdBox.w, slot.lcdBox.h});
  for (const auto& slot : fileSlots)
    drawText(g, FilePlaceholder,
             {slot.lcdBox.x + 8.0f, slot.lcdBox.y, slot.lcdBox.w - 16.0f, slot.lcdBox.h}, fileFont,
             kLCDInkColour, juce::Justification::centredLeft);
  return image;
}

std::string px(float value)
{
  return juce::String(value * DisplayScale, 2)
             .trimCharactersAtEnd("0")
             .trimCharactersAtEnd(".")
             .toStdString() +
         "px";
}

std::string cssClassFor(const std::string& symbol)
{
  return symbol == ":bypass" ? "bypass" : juce::String(symbol).replace("_", "-").toStdString();
}

std::string generateStylesheet(const std::vector<Control>& controls,
                               const std::vector<FileSlot>& fileSlots)
{
  const std::string pedal = std::string(".") + PedalClass;
  std::string css;
  css +=
      "@font-face {\n    font-family: 'OPC LCD';\n"
      "    src: url(/resources/PressStart2P-Regular.ttf{{{ns}}});\n}\n\n";
  css += pedal + " {\n    position: absolute;\n    width: " + px(PedalWidth) +
         ";\n    height: " + px(PedalHeight) +
         ";\n    background-image: url(/resources/background.png{{{ns}}});\n"
         "    background-size: " +
         px(PedalWidth) + " " + px(PedalHeight) +
         ";\n    background-repeat: no-repeat;\n    border-radius: " + px(3.0f) + ";\n}\n\n";
  css += pedal +
         " .mod-drag-handle {\n    position: absolute;\n    left: 0;\n    top: 0;\n"
         "    width: 100%;\n    height: 100%;\n    cursor: move;\n    z-index: 1;\n}\n\n";
  css += pedal +
         " .opc-widget {\n    position: absolute;\n    cursor: pointer;\n"
         "    background-repeat: no-repeat;\n    z-index: 20;\n}\n\n";
  css +=
      pedal +
      " .opc-value {\n    position: absolute;\n    font-family: 'OPC LCD', monospace !important;\n"
      "    font-size: " +
      px(9.0f) + ";\n    line-height: " + px(ValueHeight) +
      ";\n    text-align: center;\n    color: #1c1c30;\n    white-space: nowrap;\n"
      "    overflow: hidden;\n    z-index: 20;\n}\n\n";

  for (const auto& control : controls)
  {
    const float margin = frameMargin(control.widget);
    const auto size = frameSize(control);
    const std::string frameW = std::to_string(size.cssWidth) + "px";
    const std::string frameH = std::to_string(size.cssHeight) + "px";
    const std::string stripW = std::to_string(size.cssWidth * frameCount(control.widget)) + "px";
    const int frames = frameCount(control.widget);
    const std::string selector = pedal + " .opc-" + cssClassFor(control.symbol);
    css += selector + " {\n    left: " + px(control.widgetBox.x - margin) +
           ";\n    top: " + px(control.widgetBox.y - margin) + ";\n    width: " + frameW +
           ";\n    height: " + frameH + ";\n    background-image: url(/resources/" +
           spriteName(control.widget) + "{{{ns}}});\n    background-size: " + stripW + " " +
           frameH + ";\n}\n\n";

    if (frames == 2)
    {
      const std::string active = control.widget == Widget::Bypass ? ".off" : ".on";
      const std::string inactive = control.widget == Widget::Bypass ? ".on" : ".off";
      css += selector + inactive + " {\n    background-position: 0 0;\n}\n\n";
      css += selector + active + " {\n    background-position: -" + frameW + " 0;\n}\n\n";
    }

    if (control.hasValue)
    {
      const auto box = valueBox(control);
      css += pedal + " .opc-value-" + cssClassFor(control.symbol) +
             " {\n    left: " + px(box.getX()) + ";\n    top: " + px(box.getY()) +
             ";\n    width: " + px(box.getWidth()) + ";\n    height: " + px(box.getHeight()) +
             ";\n}\n\n";
    }
  }

  for (const auto& slot : fileSlots)
  {
    const auto& box = slot.lcdBox;
    css += pedal + " .opc-file[mod-parameter-uri$=\"#" + slot.uriSuffix +
           "\"] {\n    left: " + px(box.x) + ";\n    top: " + px(box.y) +
           ";\n    width: " + px(box.w) + ";\n    height: " + px(box.h) + ";\n}\n\n";
  }

  css += pedal +
         " .opc-file {\n    position: absolute;\n    z-index: 30;\n"
         "    background-image: url(/resources/lcd.png{{{ns}}});\n    background-size: " +
         std::to_string(fileLcdCssWidth() * 2) + "px " + std::to_string(fileLcdCssHeight()) +
         "px;\n    background-position: 0 0;\n    background-repeat: no-repeat;\n"
         "    font-family: 'OPC LCD', monospace !important;\n    font-size: " +
         px(10.0f) +
         ";\n    color: #1c1c30;\n"
         "    cursor: pointer;\n}\n\n";
  css += pedal + " .opc-file.opc-dim {\n    background-position: -" +
         std::to_string(fileLcdCssWidth()) + "px 0;\n}\n\n";
  css += pedal + " .opc-file.opc-active {\n    z-index: 31;\n}\n\n";
  css += pedal +
         " .opc-file .mod-enumerated-selected {\n    font-family: 'OPC LCD', monospace "
         "!important;\n    padding: 0 " +
         px(8.0f) + ";\n    line-height: " + px(30.0f) +
         ";\n    white-space: nowrap;\n    overflow: hidden;\n"
         "    text-overflow: ellipsis;\n}\n\n";
  css += pedal +
         " .opc-file .mod-enumerated-list {\n    display: none;\n    position: absolute;\n"
         "    top: " +
         px(32.0f) + ";\n    left: 0;\n    width: 100%;\n    max-height: " + px(220.0f) +
         ";\n"
         "    overflow: auto;\n    background: #2c2c2c;\n    border: 1px solid #181818;\n"
         "    color: #ffffff;\n    font-family: 'OPC LCD', monospace !important;\n    font-size: " +
         px(9.0f) + ";\n}\n\n";
  css += pedal + " .opc-file .mod-enumerated-list > div {\n    padding: " + px(6.0f) + " " +
         px(8.0f) + ";\n}\n\n";
  css += pedal +
         " .opc-file .mod-enumerated-list > div:hover {\n    background: #e07030;\n"
         "    color: #1c1c30;\n}\n\n";
  css += pedal +
         " .opc-file .mod-enumerated-list > div.selected {\n    background: #F08830;\n"
         "    color: #1c1c30;\n}\n";
  return css;
}

std::string widgetRoleAttributes(const Control& control)
{
  if (control.widget == Widget::Bypass)
    return "mod-role=\"bypass\"";
  std::string attributes =
      "mod-role=\"input-control-port\" mod-port-symbol=\"" + control.symbol + "\"";
  if (control.widget == Widget::Footswitch)
    attributes += " mod-widget=\"switch\"";
  return attributes;
}

std::string generateTemplate(const std::vector<Control>& controls)
{
  std::string html = std::string("<div class=\"mod-pedal ") + PedalClass + "\">\n";
  html += "    <div mod-role=\"drag-handle\" class=\"mod-drag-handle\"></div>\n";

  for (const auto& control : controls)
  {
    html += "    <div class=\"opc-widget opc-" + cssClassFor(control.symbol) + "\" " +
            widgetRoleAttributes(control) + "></div>\n";
    if (control.hasValue)
      html += "    <div class=\"opc-value opc-value-" + cssClassFor(control.symbol) +
              "\" mod-role=\"input-control-value\" mod-port-symbol=\"" + control.symbol +
              "\"></div>\n";
  }

  html += "    {{#effect.parameters}}\n";
  html += "    {{#path}}\n";
  html +=
      "    <div class=\"mod-enumerated opc-file\" mod-role=\"input-parameter\" "
      "mod-parameter-uri=\"{{uri}}\" mod-widget=\"custom-select-path\">\n";
  html +=
      "        <div class=\"mod-enumerated-selected\" mod-role=\"input-parameter-value\" "
      "mod-parameter-uri=\"{{uri}}\">" +
      std::string(FilePlaceholder) + "</div>\n";
  html += "        <div class=\"mod-enumerated-list\">\n";
  html += "            {{#files}}\n";
  html +=
      "            <div mod-role=\"enumeration-option\" "
      "mod-parameter-value=\"{{fullname}}\">{{basename}}</div>\n";
  html += "            {{/files}}\n";
  html += "        </div>\n";
  html += "    </div>\n";
  html += "    {{/path}}\n";
  html += "    {{/effect.parameters}}\n";

  html += "    <div class=\"mod-pedal-input\">\n";
  html += "        {{#effect.ports.audio.input}}\n";
  html +=
      "        <div class=\"mod-input mod-input-disconnected\" title=\"{{name}}\" "
      "mod-role=\"input-audio-port\" mod-port-symbol=\"{{symbol}}\">\n";
  html += "            <div class=\"mod-pedal-input-image\"></div>\n";
  html += "        </div>\n";
  html += "        {{/effect.ports.audio.input}}\n";
  html += "    </div>\n";
  html += "    <div class=\"mod-pedal-output\">\n";
  html += "        {{#effect.ports.audio.output}}\n";
  html +=
      "        <div class=\"mod-output mod-output-disconnected\" title=\"{{name}}\" "
      "mod-role=\"output-audio-port\" mod-port-symbol=\"{{symbol}}\">\n";
  html += "            <div class=\"mod-pedal-output-image\"></div>\n";
  html += "        </div>\n";
  html += "        {{/effect.ports.audio.output}}\n";
  html += "    </div>\n";
  html += "</div>\n";
  return html;
}

bool writePng(const juce::Image& image, const juce::File& file)
{
  file.deleteFile();
  juce::FileOutputStream stream(file);
  juce::PNGImageFormat png;
  if (!stream.openedOk() || !png.writeImageToStream(image, stream))
  {
    std::fprintf(stderr, "Failed to write %s\n", file.getFullPathName().toRawUTF8());
    return false;
  }
  std::printf("Wrote %s\n", file.getFullPathName().toRawUTF8());
  return true;
}

bool writeText(const std::string& text, const juce::File& file)
{
  if (!file.replaceWithText(juce::String(text), false, false, "\n"))
  {
    std::fprintf(stderr, "Failed to write %s\n", file.getFullPathName().toRawUTF8());
    return false;
  }
  std::printf("Wrote %s\n", file.getFullPathName().toRawUTF8());
  return true;
}

bool writeBinary(const void* data, int size, const juce::File& file)
{
  if (!file.replaceWithData(data, static_cast<size_t>(size)))
  {
    std::fprintf(stderr, "Failed to write %s\n", file.getFullPathName().toRawUTF8());
    return false;
  }
  std::printf("Wrote %s\n", file.getFullPathName().toRawUTF8());
  return true;
}

}  // namespace

int main(int argc, char** argv)
{
  if (argc != 2)
  {
    std::fprintf(stderr, "Usage: %s <modgui output directory>\n", argv[0]);
    return 1;
  }

  const juce::ScopedJuceInitialiser_GUI juceInitialiser;
  const juce::File outputDirectory(juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]));
  if (!outputDirectory.createDirectory())
  {
    std::fprintf(stderr, "Cannot create %s\n", outputDirectory.getFullPathName().toRawUTF8());
    return 1;
  }

  OctoberLookAndFeel lookAndFeel;
  juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);

  const auto controls = buildControls();
  const auto fileSlots = buildFileSlots();
  bool ok = true;

  std::vector<std::string> rendered;
  for (const auto& control : controls)
  {
    if (std::find(rendered.begin(), rendered.end(), spriteName(control.widget)) != rendered.end())
      continue;
    rendered.push_back(spriteName(control.widget));
    ok &= writePng(renderSprite(lookAndFeel, control),
                   outputDirectory.getChildFile(spriteName(control.widget)));
  }

  const auto background = renderBackground(lookAndFeel, controls, fileSlots);
  ok &= writePng(background, outputDirectory.getChildFile("background.png"));
  ok &= writePng(renderFileLcdSprite(), outputDirectory.getChildFile("lcd.png"));

  const auto screenshot = renderScreenshot(lookAndFeel, background, controls, fileSlots);
  ok &= writePng(screenshot.rescaled(juce::roundToInt(PedalWidth * DisplayScale),
                                     juce::roundToInt(PedalHeight * DisplayScale),
                                     juce::Graphics::highResamplingQuality),
                 outputDirectory.getChildFile("screenshot.png"));
  ok &= writePng(screenshot.rescaled(190, juce::roundToInt(190.0f * PedalHeight / PedalWidth),
                                     juce::Graphics::highResamplingQuality),
                 outputDirectory.getChildFile("thumbnail.png"));

  ok &= writeText(generateStylesheet(controls, fileSlots),
                  outputDirectory.getChildFile("stylesheet.css"));
  ok &= writeText(generateTemplate(controls), outputDirectory.getChildFile("icon.html"));
  ok &= writeBinary(BinaryData::PressStart2PRegular_ttf, BinaryData::PressStart2PRegular_ttfSize,
                    outputDirectory.getChildFile("PressStart2P-Regular.ttf"));

  juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
  return ok ? 0 : 1;
}
