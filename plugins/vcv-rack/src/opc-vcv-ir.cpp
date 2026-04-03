#include "opc-vcv-ir.hpp"

#include <dirent.h>
#include <osdialog.h>
#include <sys/stat.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <vector>

// ---------------------------------------------------------------------------
// Font helpers
// ---------------------------------------------------------------------------

static int ensureFont(NVGcontext* vg, const char* name, const std::string& path)
{
  int id = nvgFindFont(vg, name);
  if (id < 0 && !path.empty())
    id = nvgCreateFont(vg, name, path.c_str());
  return id;
}

// ---------------------------------------------------------------------------
// Drawing helpers — NanoVG translations of JUCE LookAndFeel
// ---------------------------------------------------------------------------

static void drawJuceButton(NVGcontext* vg, float w, float h, const char* label, bool pressed,
                           bool isLoadButton, const std::string& fontPath)
{
  const float cr = 4.0f;

  // Drop shadow (when not pressed)
  if (!pressed)
  {
    NVGpaint shadow =
        nvgBoxGradient(vg, 0.f, 2.f, w, h, cr, 4.f, nvgRGBA(0, 0, 0, 115), nvgRGBA(0, 0, 0, 0));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, -2.f, 0.f, w + 4.f, h + 4.f, cr);
    nvgFillPaint(vg, shadow);
    nvgFill(vg);
  }

  // Gradient fill
  NVGcolor top = pressed ? nvgRGB(0x24, 0x24, 0x24) : nvgRGB(0x38, 0x38, 0x38);
  NVGcolor bot = pressed ? nvgRGB(0x30, 0x30, 0x30) : nvgRGB(0x24, 0x24, 0x24);
  NVGpaint grad = nvgLinearGradient(vg, 0.f, 0.5f, 0.f, h - 0.5f, top, bot);
  nvgBeginPath(vg);
  nvgRoundedRect(vg, 0.5f, 0.5f, w - 1.f, h - 1.f, cr);
  nvgFillPaint(vg, grad);
  nvgFill(vg);

  // Top/left highlight
  nvgStrokeWidth(vg, 1.f);
  nvgStrokeColor(vg, nvgRGB(0x50, 0x50, 0x50));
  nvgBeginPath(vg);
  nvgMoveTo(vg, cr + 0.5f, 1.5f);
  nvgLineTo(vg, w - cr - 0.5f, 1.5f);
  nvgStroke(vg);
  nvgBeginPath(vg);
  nvgMoveTo(vg, 1.5f, cr + 0.5f);
  nvgLineTo(vg, 1.5f, h - cr - 0.5f);
  nvgStroke(vg);

  // Bottom/right shadow
  nvgStrokeColor(vg, nvgRGB(0x14, 0x14, 0x14));
  nvgBeginPath(vg);
  nvgMoveTo(vg, cr + 0.5f, h - 1.5f);
  nvgLineTo(vg, w - cr - 0.5f, h - 1.5f);
  nvgStroke(vg);
  nvgBeginPath(vg);
  nvgMoveTo(vg, w - 1.5f, cr + 0.5f);
  nvgLineTo(vg, w - 1.5f, h - cr - 0.5f);
  nvgStroke(vg);

  // Border
  nvgBeginPath(vg);
  nvgRoundedRect(vg, 0.5f, 0.5f, w - 1.f, h - 1.f, cr);
  nvgStrokeColor(vg, nvgRGB(0x18, 0x18, 0x18));
  nvgStrokeWidth(vg, 1.f);
  nvgStroke(vg);

  const float offset = pressed ? 1.f : 0.f;

  if (isLoadButton)
  {
    // Folder icon (translated from JUCE drawButtonText folder path)
    const float pad = 3.5f;
    float side = std::min(w, h) - pad * 2.f;
    float bx = (w - side) * 0.5f + offset;
    float by = (h - side) * 0.5f + offset;
    float r = std::min(1.5f, side * 0.15f);
    float tabW = side * 0.44f;
    float tabH = side * 0.20f;
    float slopeW = tabH * 0.75f;

    nvgBeginPath(vg);
    nvgMoveTo(vg, bx, by + r);
    nvgQuadTo(vg, bx, by, bx + r, by);
    nvgLineTo(vg, bx + tabW, by);
    nvgBezierTo(vg, bx + tabW + slopeW * 0.5f, by, bx + tabW + slopeW, by + tabH - r,
                bx + tabW + slopeW, by + tabH);
    nvgLineTo(vg, bx + side - r, by + tabH);
    nvgQuadTo(vg, bx + side, by + tabH, bx + side, by + tabH + r);
    nvgLineTo(vg, bx + side, by + side - r);
    nvgQuadTo(vg, bx + side, by + side, bx + side - r, by + side);
    nvgLineTo(vg, bx + r, by + side);
    nvgQuadTo(vg, bx, by + side, bx, by + side - r);
    nvgClosePath(vg);
    nvgFillColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0xff));
    nvgFill(vg);
  }
  else if (label != nullptr)
  {
    int fontId = ensureFont(vg, "CourierPrime", fontPath);
    if (fontId >= 0)
      nvgFontFaceId(vg, fontId);
    nvgFontSize(vg, 11.f);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0xff));
    nvgText(vg, w * 0.5f + offset, h * 0.5f + offset, label, nullptr);
  }
}

static void drawToggleButton(NVGcontext* vg, float w, float h, const char* label, bool isOn,
                             const std::string& fontPath)
{
  const float cr = 4.0f;
  const float ledW = std::max(4.f, w * 0.12f);

  // Drop shadow (when OFF)
  if (!isOn)
  {
    NVGpaint shadow =
        nvgBoxGradient(vg, 0.f, 2.f, w, h, cr, 4.f, nvgRGBA(0, 0, 0, 115), nvgRGBA(0, 0, 0, 0));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, -2.f, 0.f, w + 4.f, h + 4.f, cr);
    nvgFillPaint(vg, shadow);
    nvgFill(vg);
  }

  // Gradient fill
  NVGcolor top, bot;
  if (isOn)
  {
    top = nvgRGB(0x1e, 0x1e, 0x1e);
    bot = nvgRGB(0x2e, 0x2e, 0x2e);
  }
  else
  {
    top = nvgRGB(0x38, 0x38, 0x38);
    bot = nvgRGB(0x24, 0x24, 0x24);
  }
  NVGpaint grad = nvgLinearGradient(vg, 0.f, 0.5f, 0.f, h - 0.5f, top, bot);
  nvgBeginPath(vg);
  nvgRoundedRect(vg, 0.5f, 0.5f, w - 1.f, h - 1.f, cr);
  nvgFillPaint(vg, grad);
  nvgFill(vg);

  // Edge highlights/shadows (inverted when ON)
  NVGcolor hiColor = isOn ? nvgRGB(0x14, 0x14, 0x14) : nvgRGB(0x50, 0x50, 0x50);
  NVGcolor loColor = isOn ? nvgRGB(0x50, 0x50, 0x50) : nvgRGB(0x14, 0x14, 0x14);

  nvgStrokeWidth(vg, 1.f);
  nvgStrokeColor(vg, hiColor);
  nvgBeginPath(vg);
  nvgMoveTo(vg, cr + 0.5f, 1.5f);
  nvgLineTo(vg, w - cr - 0.5f, 1.5f);
  nvgStroke(vg);
  nvgBeginPath(vg);
  nvgMoveTo(vg, 1.5f, cr + 0.5f);
  nvgLineTo(vg, 1.5f, h - cr - 0.5f);
  nvgStroke(vg);

  nvgStrokeColor(vg, loColor);
  nvgBeginPath(vg);
  nvgMoveTo(vg, cr + 0.5f, h - 1.5f);
  nvgLineTo(vg, w - cr - 0.5f, h - 1.5f);
  nvgStroke(vg);
  nvgBeginPath(vg);
  nvgMoveTo(vg, w - 1.5f, cr + 0.5f);
  nvgLineTo(vg, w - 1.5f, h - cr - 0.5f);
  nvgStroke(vg);

  // Border
  nvgBeginPath(vg);
  nvgRoundedRect(vg, 0.5f, 0.5f, w - 1.f, h - 1.f, cr);
  nvgStrokeColor(vg, nvgRGB(0x18, 0x18, 0x18));
  nvgStrokeWidth(vg, 1.f);
  nvgStroke(vg);

  // LED strip on left
  float ledCx = ledW * 0.5f + 0.5f;
  float ledCy = h * 0.5f;
  if (isOn)
  {
    // Glow
    NVGpaint glow = nvgRadialGradient(vg, ledCx, ledCy, 0.f, ledW * 3.f,
                                      nvgRGBA(0xe0, 0x70, 0x30, 72), nvgRGBA(0xe0, 0x70, 0x30, 0));
    nvgBeginPath(vg);
    nvgRect(vg, -ledW, 0.f, ledW * 5.f, h);
    nvgFillPaint(vg, glow);
    nvgFill(vg);

    // Bright strip
    NVGpaint stripGrad =
        nvgRadialGradient(vg, ledCx, ledCy - h * 0.2f, 0.f, h * 0.6f,
                          nvgRGBA(0xff, 0xb0, 0x60, 0xff), nvgRGBA(0xe0, 0x70, 0x30, 0xff));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, 0.5f, 0.5f, ledW, h - 1.f, cr);
    nvgFillPaint(vg, stripGrad);
    nvgFill(vg);
  }
  else
  {
    // Dim LED
    nvgBeginPath(vg);
    nvgRoundedRect(vg, 0.5f, 0.5f, ledW, h - 1.f, cr);
    nvgFillColor(vg, nvgRGBA(0x30, 0x1a, 0x0a, 0xff));
    nvgFill(vg);
  }

  // Text (shifted right past LED strip)
  int fontId = ensureFont(vg, "CourierPrime", fontPath);
  if (fontId >= 0)
    nvgFontFaceId(vg, fontId);
  nvgFontSize(vg, 11.f);
  nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  nvgFillColor(vg, nvgRGBA(0xff, 0xff, 0xff, 230));
  nvgText(vg, (w + ledW) * 0.5f, h * 0.5f, label, nullptr);
}

static void drawLCDBackground(NVGcontext* vg, float x, float y, float w, float h)
{
  const float cr = 3.0f;

  // Orange fill
  nvgBeginPath(vg);
  nvgRoundedRect(vg, x, y, w, h, cr);
  nvgFillColor(vg, nvgRGB(0xF0, 0x88, 0x30));
  nvgFill(vg);

  // Dark border
  nvgBeginPath(vg);
  nvgRoundedRect(vg, x, y, w, h, cr);
  nvgStrokeColor(vg, nvgRGB(0x1a, 0x1a, 0x1a));
  nvgStrokeWidth(vg, 1.f);
  nvgStroke(vg);

  // Top inset shadow
  float ix = x + 1.f;
  float iy = y + 1.f;
  float iw = w - 2.f;
  float ih = h - 2.f;
  NVGpaint topShadow =
      nvgLinearGradient(vg, ix, iy, ix, iy + 5.f, nvgRGBA(0, 0, 0, 56), nvgRGBA(0, 0, 0, 0));
  nvgBeginPath(vg);
  nvgRoundedRect(vg, ix, iy, iw, ih, cr - 1.f);
  nvgFillPaint(vg, topShadow);
  nvgFill(vg);

  // Left inset shadow
  NVGpaint leftShadow =
      nvgLinearGradient(vg, ix, iy, ix + 5.f, iy, nvgRGBA(0, 0, 0, 31), nvgRGBA(0, 0, 0, 0));
  nvgBeginPath(vg);
  nvgRoundedRect(vg, ix, iy, iw, ih, cr - 1.f);
  nvgFillPaint(vg, leftShadow);
  nvgFill(vg);

  // Scan lines
  nvgBeginPath(vg);
  for (float sy = y + 1.f; sy < y + h - 1.f; sy += 2.f)
  {
    nvgMoveTo(vg, x + 1.f, sy);
    nvgLineTo(vg, x + w - 1.f, sy);
  }
  nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 6));
  nvgStrokeWidth(vg, 1.f);
  nvgStroke(vg);
}

static void drawCustomKnob(NVGcontext* vg, float cx, float cy, float radius, float angle,
                           bool isCompact, float startAngle, float endAngle)
{
  const float outerR = radius;
  const float knurlingOuter = radius * 0.68f;
  const float knurlingInner = radius * 0.60f;
  const float capR = radius * 0.60f;
  const int knurlingCount = isCompact ? 28 : 55;

  // Drop shadow
  nvgBeginPath(vg);
  nvgCircle(vg, cx + 0.5f, cy + 2.f, outerR + 1.5f);
  nvgFillColor(vg, nvgRGBA(0, 0, 0, 56));
  nvgFill(vg);

  // Outer skirt with radial gradient (offset center for 3D effect)
  NVGpaint skirt =
      nvgRadialGradient(vg, cx - outerR * 0.3f, cy - outerR * 0.3f, outerR * 0.1f, outerR * 1.2f,
                        nvgRGB(0x2c, 0x2c, 0x2c), nvgRGB(0x0a, 0x0a, 0x0a));
  nvgBeginPath(vg);
  nvgCircle(vg, cx, cy, outerR);
  nvgFillPaint(vg, skirt);
  nvgFill(vg);

  // Rim highlight
  nvgBeginPath(vg);
  nvgCircle(vg, cx, cy, outerR);
  nvgStrokeColor(vg, nvgRGB(0x48, 0x48, 0x48));
  nvgStrokeWidth(vg, 1.f);
  nvgStroke(vg);

  // Knurling band
  nvgStrokeColor(vg, nvgRGB(0x40, 0x40, 0x40));
  nvgStrokeWidth(vg, 1.5f);
  for (int i = 0; i < knurlingCount; ++i)
  {
    float a =
        static_cast<float>(M_PI) * 2.f * static_cast<float>(i) / static_cast<float>(knurlingCount) -
        static_cast<float>(M_PI) * 0.5f;
    float cosA = std::cos(a);
    float sinA = std::sin(a);
    nvgBeginPath(vg);
    nvgMoveTo(vg, cx + knurlingInner * cosA, cy + knurlingInner * sinA);
    nvgLineTo(vg, cx + knurlingOuter * cosA, cy + knurlingOuter * sinA);
    nvgStroke(vg);
  }

  // Silver cap with radial gradient (offset center for convex look)
  NVGpaint cap = nvgRadialGradient(vg, cx - capR * 0.30f, cy - capR * 0.35f, capR * 0.1f,
                                   capR * 1.2f, nvgRGB(0xfa, 0xfa, 0xfa), nvgRGB(0x8c, 0x8c, 0x8c));
  nvgBeginPath(vg);
  nvgCircle(vg, cx, cy, capR);
  nvgFillPaint(vg, cap);
  nvgFill(vg);

  // Cap border
  nvgBeginPath(vg);
  nvgCircle(vg, cx, cy, capR);
  nvgStrokeColor(vg, nvgRGB(0x88, 0x88, 0x88));
  nvgStrokeWidth(vg, 1.f);
  nvgStroke(vg);

  // White indicator line on outer skirt
  float cosA = std::cos(angle - static_cast<float>(M_PI) * 0.5f);
  float sinA = std::sin(angle - static_cast<float>(M_PI) * 0.5f);
  nvgStrokeColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0xff));
  nvgStrokeWidth(vg, 1.5f);
  nvgBeginPath(vg);
  nvgMoveTo(vg, cx + knurlingOuter * cosA, cy + knurlingOuter * sinA);
  nvgLineTo(vg, cx + outerR * 0.90f * cosA, cy + outerR * 0.90f * sinA);
  nvgStroke(vg);

  // External tick marks (large knobs only)
  if (!isCompact)
  {
    const int numTicks = 11;
    const float tickInner = outerR + 2.f;
    const float tickOuter = outerR + 6.f;
    nvgStrokeColor(vg, nvgRGB(0x88, 0x88, 0x88));
    nvgStrokeWidth(vg, 1.5f);
    for (int i = 0; i < numTicks; ++i)
    {
      float tickAngle = startAngle + static_cast<float>(i) / static_cast<float>(numTicks - 1) *
                                         (endAngle - startAngle);
      float tCos = std::cos(tickAngle - static_cast<float>(M_PI) * 0.5f);
      float tSin = std::sin(tickAngle - static_cast<float>(M_PI) * 0.5f);
      nvgBeginPath(vg);
      nvgMoveTo(vg, cx + tickInner * tCos, cy + tickInner * tSin);
      nvgLineTo(vg, cx + tickOuter * tCos, cy + tickOuter * tSin);
      nvgStroke(vg);
    }
  }
}

static void drawComboBoxButton(NVGcontext* vg, float w, float h, const char* label,
                               const std::string& fontPath)
{
  const float cr = 3.0f;

  // Drop shadow
  NVGpaint shadow =
      nvgBoxGradient(vg, 0.f, 2.f, w, h, cr, 4.f, nvgRGBA(0, 0, 0, 115), nvgRGBA(0, 0, 0, 0));
  nvgBeginPath(vg);
  nvgRoundedRect(vg, -2.f, 0.f, w + 4.f, h + 4.f, cr);
  nvgFillPaint(vg, shadow);
  nvgFill(vg);

  // Gradient fill
  NVGpaint grad = nvgLinearGradient(vg, 0.f, 0.5f, 0.f, h - 0.5f, nvgRGB(0x38, 0x38, 0x38),
                                    nvgRGB(0x24, 0x24, 0x24));
  nvgBeginPath(vg);
  nvgRoundedRect(vg, 0.5f, 0.5f, w - 1.f, h - 1.f, cr);
  nvgFillPaint(vg, grad);
  nvgFill(vg);

  // Edge highlights
  nvgStrokeWidth(vg, 1.f);
  nvgStrokeColor(vg, nvgRGB(0x50, 0x50, 0x50));
  nvgBeginPath(vg);
  nvgMoveTo(vg, cr + 0.5f, 1.5f);
  nvgLineTo(vg, w - cr - 0.5f, 1.5f);
  nvgStroke(vg);
  nvgBeginPath(vg);
  nvgMoveTo(vg, 1.5f, cr + 0.5f);
  nvgLineTo(vg, 1.5f, h - cr - 0.5f);
  nvgStroke(vg);

  // Edge shadows
  nvgStrokeColor(vg, nvgRGB(0x14, 0x14, 0x14));
  nvgBeginPath(vg);
  nvgMoveTo(vg, cr + 0.5f, h - 1.5f);
  nvgLineTo(vg, w - cr - 0.5f, h - 1.5f);
  nvgStroke(vg);
  nvgBeginPath(vg);
  nvgMoveTo(vg, w - 1.5f, cr + 0.5f);
  nvgLineTo(vg, w - 1.5f, h - cr - 0.5f);
  nvgStroke(vg);

  // Border
  nvgBeginPath(vg);
  nvgRoundedRect(vg, 0.5f, 0.5f, w - 1.f, h - 1.f, cr);
  nvgStrokeColor(vg, nvgRGB(0x18, 0x18, 0x18));
  nvgStrokeWidth(vg, 1.f);
  nvgStroke(vg);

  // Text (left-aligned with padding)
  int fontId = ensureFont(vg, "CourierPrime", fontPath);
  if (fontId >= 0)
    nvgFontFaceId(vg, fontId);
  nvgFontSize(vg, 11.f);
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  nvgFillColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0xff));
  nvgText(vg, 6.f, h * 0.5f, label, nullptr);

  // Dropdown arrow (chevron on right side)
  const float arrowX = w - 12.f;
  const float arrowCy = h * 0.5f;
  nvgBeginPath(vg);
  nvgMoveTo(vg, arrowX - 4.f, arrowCy - 2.f);
  nvgLineTo(vg, arrowX, arrowCy + 2.f);
  nvgLineTo(vg, arrowX + 4.f, arrowCy - 2.f);
  nvgStrokeColor(vg, nvgRGBA(0xaa, 0xaa, 0xaa, 230));
  nvgStrokeWidth(vg, 2.f);
  nvgStroke(vg);
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

static std::string getParentDir(const std::string& path)
{
  const size_t pos = path.find_last_of("/\\");
  return (pos != std::string::npos) ? path.substr(0, pos) : ".";
}

static std::string getFilename(const std::string& path)
{
  const size_t pos = path.find_last_of("/\\");
  return (pos != std::string::npos) ? path.substr(pos + 1) : path;
}

static bool isAudioExtension(const std::string& name)
{
  const size_t dot = name.rfind('.');
  if (dot == std::string::npos)
    return false;
  std::string ext = name.substr(dot);
  for (char& c : ext)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return ext == ".wav" || ext == ".aiff" || ext == ".aif" || ext == ".flac";
}

// ---------------------------------------------------------------------------
// Custom knob widgets
// ---------------------------------------------------------------------------

struct OpcCustomKnob : app::Knob
{
  std::string fontPath;
  static constexpr float kStartAngle = static_cast<float>(M_PI) * 1.25f;
  static constexpr float kEndAngle = static_cast<float>(M_PI) * 2.75f;

  OpcCustomKnob()
  {
    box.size = mm2px(math::Vec(16.0f, 16.0f));
    minAngle = -0.75f * static_cast<float>(M_PI);
    maxAngle = 0.75f * static_cast<float>(M_PI);
    snap = false;
  }

  void draw(const DrawArgs& args) override
  {
    float radius = std::min(box.size.x, box.size.y) * 0.5f - 8.f;
    float cx = box.size.x * 0.5f;
    float cy = box.size.y * 0.5f;

    engine::ParamQuantity* pq = getParamQuantity();
    float normalized = pq ? pq->getScaledValue() : 0.5f;
    float angle = kStartAngle + normalized * (kEndAngle - kStartAngle);

    drawCustomKnob(args.vg, cx, cy, radius, angle, false, kStartAngle, kEndAngle);
  }
};

struct OpcCustomTrimKnob : app::Knob
{
  static constexpr float kStartAngle = static_cast<float>(M_PI) * 1.25f;
  static constexpr float kEndAngle = static_cast<float>(M_PI) * 2.75f;

  OpcCustomTrimKnob()
  {
    box.size = mm2px(math::Vec(7.0f, 7.0f));
    minAngle = -0.75f * static_cast<float>(M_PI);
    maxAngle = 0.75f * static_cast<float>(M_PI);
    snap = false;
  }

  void draw(const DrawArgs& args) override
  {
    float radius = std::min(box.size.x, box.size.y) * 0.5f - 2.f;
    float cx = box.size.x * 0.5f;
    float cy = box.size.y * 0.5f;

    engine::ParamQuantity* pq = getParamQuantity();
    float normalized = pq ? pq->getScaledValue() : 0.5f;
    float angle = kStartAngle + normalized * (kEndAngle - kStartAngle);

    drawCustomKnob(args.vg, cx, cy, radius, angle, true, kStartAngle, kEndAngle);
  }
};

// ---------------------------------------------------------------------------
// Panel label
// ---------------------------------------------------------------------------

struct OpcPanelLabel : TransparentWidget
{
  std::string text;
  std::string fontPath;
  float fontSize = 10.f;
  NVGcolor color = nvgRGB(0x1a, 0x1a, 0x1a);
  int align = NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE;

  void draw(const DrawArgs& args) override
  {
    int fontId = ensureFont(args.vg, "CourierPrime", fontPath);
    if (fontId >= 0)
      nvgFontFaceId(args.vg, fontId);

    nvgFontSize(args.vg, fontSize);
    nvgTextAlign(args.vg, align);
    nvgFillColor(args.vg, color);
    nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.5f, text.c_str(), nullptr);
  }
};

// ---------------------------------------------------------------------------
// IR file display (LCD style)
// ---------------------------------------------------------------------------

struct IrFileDisplay : OpaqueWidget
{
  OpcVcvIr* module = nullptr;
  bool isIR2;
  std::string lcdFontPath;
  std::string text;

  IrFileDisplay(bool ir2 = false) : isIR2(ir2) { text = "<No IR selected>"; }

  void draw(const DrawArgs& args) override
  {
    drawLCDBackground(args.vg, 0.f, 0.f, box.size.x, box.size.y);

    int fontId = ensureFont(args.vg, "PressStart2P", lcdFontPath);
    if (fontId >= 0)
      nvgFontFaceId(args.vg, fontId);
    nvgFontSize(args.vg, 7.f);
    nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, nvgRGB(0x1c, 0x1c, 0x30));
    nvgText(args.vg, 4.f, box.size.y * 0.5f, text.c_str(), nullptr);
  }

  void onButton(const widget::Widget::ButtonEvent& e) override
  {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module != nullptr)
    {
      e.consume(this);
      openFileDialog();
    }
    else if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT && module != nullptr)
    {
      e.consume(this);
      ui::Menu* menu = createMenu();
      menu->addChild(new MenuSeparator);
      menu->addChild(createMenuItem("Clear", "",
                                    [this]()
                                    {
                                      if (isIR2)
                                        module->clearIR2();
                                      else
                                        module->clearIR1();
                                    }));
    }
  }

  void openFileDialog()
  {
    osdialog_filters* filters = osdialog_filters_parse("Audio files:wav,aiff,flac;All files:*");
    char* path = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, filters);

    if (path != nullptr)
    {
      if (isIR2)
        module->loadIR2(std::string(path));
      else
        module->loadIR(std::string(path));
      updateDisplayText();
      free(path);
    }

    osdialog_filters_free(filters);
  }

  void updateDisplayText()
  {
    if (module == nullptr)
    {
      text = "<No IR selected>";
      return;
    }

    std::string& path = isIR2 ? module->loaded_file_path2_ : module->loaded_file_path_;
    if (path.empty())
    {
      text = "<No IR selected>";
    }
    else
    {
      const size_t pos = path.find_last_of("/\\");
      std::string filename = (pos != std::string::npos) ? path.substr(pos + 1) : path;
      if (filename.length() > 24)
        filename = filename.substr(0, 21) + "...";
      text = filename;
    }
  }

  void step() override
  {
    updateDisplayText();
    OpaqueWidget::step();
  }
};

// ---------------------------------------------------------------------------
// Button widgets
// ---------------------------------------------------------------------------

struct OpcIrEnableButton : app::Switch
{
  std::string label;
  std::string fontPath;

  void draw(const DrawArgs& args) override
  {
    engine::ParamQuantity* pq = getParamQuantity();
    const bool lit = pq ? pq->getValue() > 0.5f : false;
    drawToggleButton(args.vg, box.size.x, box.size.y, label.c_str(), lit, fontPath);
  }
};

struct OpcDetectModeButton : app::Switch
{
  std::string fontPath;

  void draw(const DrawArgs& args) override
  {
    engine::ParamQuantity* pq = getParamQuantity();
    const bool isPeak = pq ? pq->getValue() < 0.5f : true;
    drawComboBoxButton(args.vg, box.size.x, box.size.y, isPeak ? "PEAK" : "RMS", fontPath);
  }
};

struct OpcIrLoadButton : OpaqueWidget
{
  OpcVcvIr* module = nullptr;
  bool isIR2 = false;
  std::string fontPath;
  bool pressed_ = false;

  void draw(const DrawArgs& args) override
  {
    drawJuceButton(args.vg, box.size.x, box.size.y, nullptr, pressed_, true, fontPath);
  }

  void onDragStart(const DragStartEvent& e) override
  {
    pressed_ = true;
    OpaqueWidget::onDragStart(e);
  }

  void onDragEnd(const DragEndEvent& e) override
  {
    pressed_ = false;
    OpaqueWidget::onDragEnd(e);
  }

  void onButton(const ButtonEvent& e) override
  {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module != nullptr)
    {
      e.consume(this);
      openFileDialog();
    }
  }

 private:
  void openFileDialog()
  {
    osdialog_filters* filters = osdialog_filters_parse("Audio files:wav,aiff,flac;All files:*");
    char* path = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, filters);
    if (path != nullptr)
    {
      if (isIR2)
        module->loadIR2(std::string(path));
      else
        module->loadIR(std::string(path));
      free(path);
    }
    osdialog_filters_free(filters);
  }
};

struct OpcIrClearButton : OpaqueWidget
{
  OpcVcvIr* module = nullptr;
  bool isIR2 = false;
  std::string fontPath;
  bool pressed_ = false;

  void draw(const DrawArgs& args) override
  {
    drawJuceButton(args.vg, box.size.x, box.size.y, "CLR", pressed_, false, fontPath);
  }

  void onDragStart(const DragStartEvent& e) override
  {
    pressed_ = true;
    OpaqueWidget::onDragStart(e);
  }

  void onDragEnd(const DragEndEvent& e) override
  {
    pressed_ = false;
    OpaqueWidget::onDragEnd(e);
  }

  void onButton(const ButtonEvent& e) override
  {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module != nullptr)
    {
      e.consume(this);
      if (isIR2)
        module->clearIR2();
      else
        module->clearIR1();
    }
  }
};

template <int Direction>
struct OpcIrNavButton : OpaqueWidget
{
  OpcVcvIr* module = nullptr;
  bool isIR2 = false;
  std::string fontPath;
  bool pressed_ = false;

  void draw(const DrawArgs& args) override
  {
    const bool hasIr = module != nullptr &&
                       !(isIR2 ? module->loaded_file_path2_ : module->loaded_file_path_).empty();
    nvgGlobalAlpha(args.vg, hasIr ? 1.f : 0.4f);
    drawJuceButton(args.vg, box.size.x, box.size.y, Direction < 0 ? "<" : ">", pressed_, false,
                   fontPath);
    nvgGlobalAlpha(args.vg, 1.f);
  }

  void onDragStart(const DragStartEvent& e) override
  {
    pressed_ = true;
    OpaqueWidget::onDragStart(e);
  }

  void onDragEnd(const DragEndEvent& e) override
  {
    pressed_ = false;
    OpaqueWidget::onDragEnd(e);
  }

  void onButton(const ButtonEvent& e) override
  {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module != nullptr)
    {
      e.consume(this);
      loadAdjacentFile();
    }
  }

 private:
  void loadAdjacentFile()
  {
    const std::string& currentPath = isIR2 ? module->loaded_file_path2_ : module->loaded_file_path_;
    if (currentPath.empty())
      return;

    const std::string dir = getParentDir(currentPath);
    const std::string currentName = getFilename(currentPath);

    DIR* dp = opendir(dir.c_str());
    if (dp == nullptr)
    {
      WARN("OpcIrNavButton: could not open directory %s", dir.c_str());
      return;
    }

    std::vector<std::string> entries;
    struct dirent* ep = nullptr;
    while ((ep = readdir(dp)) != nullptr)
    {
      std::string name(ep->d_name);
      if (isAudioExtension(name))
        entries.push_back(name);
    }
    closedir(dp);

    if (entries.empty())
    {
      WARN("OpcIrNavButton: no audio files found in %s", dir.c_str());
      return;
    }

    std::sort(entries.begin(), entries.end());

    const auto it = std::find(entries.begin(), entries.end(), currentName);
    if (it == entries.end())
    {
      WARN("OpcIrNavButton: current file %s not found in directory listing", currentName.c_str());
      return;
    }

    const int idx = static_cast<int>(std::distance(entries.begin(), it));
    const int count = static_cast<int>(entries.size());
    const int newIdx = ((idx + Direction) % count + count) % count;

    const std::string newPath = dir + "/" + entries[static_cast<size_t>(newIdx)];
    INFO("OpcIrNavButton: loading adjacent file %s", newPath.c_str());
    if (isIR2)
      module->loadIR2(newPath);
    else
      module->loadIR(newPath);
  }
};

using OpcIrPrevButton = OpcIrNavButton<-1>;
using OpcIrNextButton = OpcIrNavButton<1>;

// ---------------------------------------------------------------------------
// Meter display (LCD style with discrete segments)
// ---------------------------------------------------------------------------

struct OpcMeterDisplay : OpaqueWidget
{
  OpcVcvIr* module = nullptr;
  std::string lcdFontPath;

  void draw(const DrawArgs& args) override
  {
    const float w = box.size.x;
    const float h = box.size.y;

    drawLCDBackground(args.vg, 0.f, 0.f, w, h);

    const float levelDb = module ? module->currentInputLevelDb_.load() : -60.f;
    const float blend = module ? module->currentBlend_.load() : 0.f;

    const float padH = 4.f;
    const float padV = 4.f;
    const float labelH = 8.f;
    const float gapLB = 2.f;
    const float barH = (h - padV * 2.f - labelH * 2.f - gapLB * 2.f - 4.f) * 0.5f;

    float cy = padV;

    // INPUT label
    int fontId = ensureFont(args.vg, "PressStart2P", lcdFontPath);
    if (fontId >= 0)
      nvgFontFaceId(args.vg, fontId);
    nvgFontSize(args.vg, 7.f);
    nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, nvgRGB(0x1c, 0x1c, 0x30));
    nvgText(args.vg, padH, cy + labelH * 0.5f, "INPUT", nullptr);
    cy += labelH + gapLB;

    drawMeterBar(args.vg, padH, cy, w - padH * 2.f, barH, levelDb, false);

    // Check for threshold markers
    if (module != nullptr)
    {
      float dynMode =
          module->params[static_cast<int>(OpcVcvIr::ParamId::DynamicModeParam)].getValue();
      if (dynMode > 0.5f)
      {
        float threshold =
            module->params[static_cast<int>(OpcVcvIr::ParamId::ThresholdParam)].getValue();
        float rangeDb =
            module->params[static_cast<int>(OpcVcvIr::ParamId::RangeDbParam)].getValue();
        float barX = padH;
        float barW = w - padH * 2.f;
        drawThresholdMarker(args.vg, barX, cy, barW, barH, threshold);
        drawThresholdMarker(args.vg, barX, cy, barW, barH, threshold + rangeDb);
      }
    }

    cy += barH + 4.f;

    // BLEND label
    nvgFillColor(args.vg, nvgRGB(0x1c, 0x1c, 0x30));
    nvgText(args.vg, padH, cy + labelH * 0.5f, "BLEND", nullptr);
    cy += labelH + gapLB;

    drawMeterBar(args.vg, padH, cy, w - padH * 2.f, barH, blend, true);
  }

 private:
  static const int kSegments = 19;
  static constexpr float kDbMin = -60.f;
  static constexpr float kDbMax = 0.f;

  void drawMeterBar(NVGcontext* vg, float x, float y, float w, float h, float value,
                    bool isCenterBalanced) const
  {
    // Bar outline
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, 2.f);
    nvgStrokeColor(vg, nvgRGB(0x1a, 0x1a, 0x1a));
    nvgStrokeWidth(vg, 1.f);
    nvgStroke(vg);

    const float innerX = x + 2.f;
    const float innerY = y + 2.f;
    const float innerW = w - 4.f;
    const float innerH = h - 4.f;
    const float segGap = 2.f;
    const float segW =
        (innerW - static_cast<float>(kSegments - 1) * segGap) / static_cast<float>(kSegments);

    float normValue;
    if (isCenterBalanced)
    {
      normValue = (value + 1.f) * 0.5f;
    }
    else
    {
      normValue = (value - kDbMin) / (kDbMax - kDbMin);
    }
    normValue = std::max(0.f, std::min(1.f, normValue));

    for (int i = 0; i < kSegments; ++i)
    {
      float sx = innerX + static_cast<float>(i) * (segW + segGap);

      bool isLit = false;
      if (isCenterBalanced)
      {
        int centre = kSegments / 2;
        float normPos = normValue * static_cast<float>(kSegments);
        isLit = (i == centre) ||
                (normValue < 0.5f && static_cast<float>(i) >= normPos && i < centre) ||
                (normValue > 0.5f && i > centre && static_cast<float>(i) < normPos);
      }
      else
      {
        isLit = static_cast<float>(i) < normValue * static_cast<float>(kSegments);
      }

      if (isLit)
      {
        nvgBeginPath(vg);
        nvgRect(vg, sx, innerY, segW, innerH);
        nvgFillColor(vg, nvgRGB(0x1c, 0x1c, 0x30));
        nvgFill(vg);
      }
    }
  }

  void drawThresholdMarker(NVGcontext* vg, float barX, float barY, float barW, float barH,
                           float thresholdDb) const
  {
    float norm = (thresholdDb - kDbMin) / (kDbMax - kDbMin);
    norm = std::max(0.f, std::min(1.f, norm));
    float mx = barX + 2.f + norm * (barW - 4.f);

    nvgBeginPath(vg);
    nvgRect(vg, mx - 1.f, barY - 3.f, 2.f, barH + 6.f);
    nvgFillColor(vg, nvgRGB(0x1c, 0x1c, 0x30));
    nvgFill(vg);
  }
};

// ---------------------------------------------------------------------------
// Toggle buttons (DYNAMIC, SIDECHAIN)
// ---------------------------------------------------------------------------

struct OpcRetroButton : app::Switch
{
  std::string label;
  std::string fontPath;

  void draw(const DrawArgs& args) override
  {
    engine::ParamQuantity* pq = getParamQuantity();
    const bool lit = pq ? pq->getValue() > 0.5f : false;
    drawToggleButton(args.vg, box.size.x, box.size.y, label.c_str(), lit, fontPath);
  }
};

// ---------------------------------------------------------------------------
// Swap button
// ---------------------------------------------------------------------------

struct OpcSwapButton : OpaqueWidget
{
  OpcVcvIr* module = nullptr;
  std::string fontPath;
  bool pressed_ = false;

  void draw(const DrawArgs& args) override
  {
    drawJuceButton(args.vg, box.size.x, box.size.y, "SWAP", pressed_, false, fontPath);
  }

  void onDragStart(const DragStartEvent& e) override
  {
    pressed_ = true;
    OpaqueWidget::onDragStart(e);
  }

  void onDragEnd(const DragEndEvent& e) override
  {
    pressed_ = false;
    OpaqueWidget::onDragEnd(e);
  }

  void onButton(const ButtonEvent& e) override
  {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module != nullptr)
    {
      e.consume(this);
      module->swapImpulseResponses();
    }
  }
};

// ---------------------------------------------------------------------------
// Output background
// ---------------------------------------------------------------------------

struct OpcOutputBackground : TransparentWidget
{
  void draw(const DrawArgs& args) override
  {
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 3.f);
    nvgFillColor(args.vg, nvgRGB(0x28, 0x28, 0x2c));
    nvgFill(args.vg);
  }
};

// ---------------------------------------------------------------------------
// Main widget
// ---------------------------------------------------------------------------

struct OpcVcvIrWidget final : ModuleWidget
{
  explicit OpcVcvIrWidget(OpcVcvIr* module)
  {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/opc-vcv-ir-panel.svg")));

    const std::string fpCourier =
        asset::plugin(pluginInstance, "res/font/CourierPrime-Regular.ttf");
    const std::string fpLCD = asset::plugin(pluginInstance, "res/font/PressStart2P-Regular.ttf");

    // Screws
    addChild(createWidget<ScrewSilver>(mm2px(Vec(7.62f, 0.5f))));
    addChild(createWidget<ScrewSilver>(mm2px(Vec(195.58f, 0.5f))));
    addChild(createWidget<ScrewSilver>(mm2px(Vec(7.62f, 122.92f))));
    addChild(createWidget<ScrewSilver>(mm2px(Vec(195.58f, 122.92f))));

    // --- Helper lambdas ---

    auto makeLabel = [&](math::Vec pos, math::Vec size, const char* txt, float fs = 10.f)
    {
      auto* lbl = createWidget<OpcPanelLabel>(pos);
      lbl->box.size = size;
      lbl->text = txt;
      lbl->fontPath = fpCourier;
      lbl->fontSize = fs;
      addChild(lbl);
    };

    auto makeLoadBtn = [&](math::Vec center, bool ir2)
    {
      auto* btn = createWidget<OpcIrLoadButton>(center - mm2px(Vec(3.5f, 2.5f)));
      btn->box.size = mm2px(Vec(7.0f, 5.0f));
      btn->module = module;
      btn->isIR2 = ir2;
      btn->fontPath = fpCourier;
      addChild(btn);
    };

    auto makeClearBtn = [&](math::Vec center, bool ir2)
    {
      auto* btn = createWidget<OpcIrClearButton>(center - mm2px(Vec(4.5f, 2.5f)));
      btn->box.size = mm2px(Vec(9.0f, 5.0f));
      btn->module = module;
      btn->isIR2 = ir2;
      btn->fontPath = fpCourier;
      addChild(btn);
    };

    auto makePrevBtn = [&](math::Vec center, bool ir2)
    {
      auto* btn = createWidget<OpcIrPrevButton>(center - mm2px(Vec(2.5f, 2.5f)));
      btn->box.size = mm2px(Vec(5.0f, 5.0f));
      btn->module = module;
      btn->isIR2 = ir2;
      btn->fontPath = fpCourier;
      addChild(btn);
    };

    auto makeNextBtn = [&](math::Vec center, bool ir2)
    {
      auto* btn = createWidget<OpcIrNextButton>(center - mm2px(Vec(2.5f, 2.5f)));
      btn->box.size = mm2px(Vec(5.0f, 5.0f));
      btn->module = module;
      btn->isIR2 = ir2;
      btn->fontPath = fpCourier;
      addChild(btn);
    };

    auto makeEnableBtn = [&](math::Vec center, bool ir2)
    {
      const auto paramId = static_cast<int>(ir2 ? OpcVcvIr::ParamId::IrBEnableParam
                                                : OpcVcvIr::ParamId::IrAEnableParam);
      auto* btn = createParam<OpcIrEnableButton>(center - mm2px(Vec(6.0f, 2.5f)), module, paramId);
      btn->box.size = mm2px(Vec(12.0f, 5.0f));
      btn->label = "ENABLE";
      btn->fontPath = fpCourier;
      addParam(btn);
    };

    // =====================================================================
    // Layout constants
    // Panel: 203.2mm wide x 128.5mm tall
    // Usable vertical: ~3mm to ~126mm (leaving room for screws)
    // =====================================================================

    const float colAStart = 3.f;
    const float colBStart = 104.f;

    // --- IR A row (Y-center = 8.0) ---
    makeLabel(mm2px(Vec(colAStart, 3.0f)), mm2px(Vec(8.0f, 4.0f)), "IR A", 11.f);
    makeLoadBtn(mm2px(Vec(colAStart + 7.f, 8.0f)), false);
    makeClearBtn(mm2px(Vec(colAStart + 16.5f, 8.0f)), false);
    makePrevBtn(mm2px(Vec(colAStart + 24.f, 8.0f)), false);
    makeNextBtn(mm2px(Vec(colAStart + 30.f, 8.0f)), false);
    makeEnableBtn(mm2px(Vec(colAStart + 40.f, 8.0f)), false);
    addParam(createParamCentered<OpcCustomTrimKnob>(
        mm2px(Vec(colAStart + 53.f, 8.0f)), module,
        static_cast<int>(OpcVcvIr::ParamId::IrATrimGainParam)));

    auto* fileDisplayA = createWidget<IrFileDisplay>(mm2px(Vec(colAStart + 57.f, 4.0f)));
    fileDisplayA->box.size = mm2px(Vec(39.0f, 8.0f));
    fileDisplayA->module = module;
    fileDisplayA->isIR2 = false;
    fileDisplayA->lcdFontPath = fpLCD;
    addChild(fileDisplayA);

    // --- IR B row (Y-center = 8.0, right column) ---
    makeLabel(mm2px(Vec(colBStart, 3.0f)), mm2px(Vec(8.0f, 4.0f)), "IR B", 11.f);
    makeLoadBtn(mm2px(Vec(colBStart + 7.f, 8.0f)), true);
    makeClearBtn(mm2px(Vec(colBStart + 16.5f, 8.0f)), true);
    makePrevBtn(mm2px(Vec(colBStart + 24.f, 8.0f)), true);
    makeNextBtn(mm2px(Vec(colBStart + 30.f, 8.0f)), true);
    makeEnableBtn(mm2px(Vec(colBStart + 40.f, 8.0f)), true);
    addParam(createParamCentered<OpcCustomTrimKnob>(
        mm2px(Vec(colBStart + 53.f, 8.0f)), module,
        static_cast<int>(OpcVcvIr::ParamId::IrBTrimGainParam)));

    auto* fileDisplayB = createWidget<IrFileDisplay>(mm2px(Vec(colBStart + 57.f, 4.0f)));
    fileDisplayB->box.size = mm2px(Vec(39.0f, 8.0f));
    fileDisplayB->module = module;
    fileDisplayB->isIR2 = true;
    fileDisplayB->lcdFontPath = fpLCD;
    addChild(fileDisplayB);

    // --- Mode row (Y-center = 17.0) ---
    const float modeY = 17.0f;
    const float modeW = 56.0f;
    const float modeH = 7.0f;
    const float modeX1 = 34.f;
    const float modeX2 = 102.f;
    const float modeX3 = 170.f;

    {
      auto* dynBtn = createParam<OpcRetroButton>(
          mm2px(Vec(modeX1 - modeW * 0.5f, modeY - modeH * 0.5f)), module,
          static_cast<int>(OpcVcvIr::ParamId::DynamicModeParam));
      dynBtn->box.size = mm2px(Vec(modeW, modeH));
      dynBtn->label = "DYNAMIC";
      dynBtn->fontPath = fpCourier;
      addParam(dynBtn);
    }

    {
      auto* swapBtn =
          createWidget<OpcSwapButton>(mm2px(Vec(modeX2 - modeW * 0.5f, modeY - modeH * 0.5f)));
      swapBtn->box.size = mm2px(Vec(modeW, modeH));
      swapBtn->module = module;
      swapBtn->fontPath = fpCourier;
      addChild(swapBtn);
    }

    {
      auto* scBtn = createParam<OpcRetroButton>(
          mm2px(Vec(modeX3 - modeW * 0.5f, modeY - modeH * 0.5f)), module,
          static_cast<int>(OpcVcvIr::ParamId::SidechainEnableParam));
      scBtn->box.size = mm2px(Vec(modeW, modeH));
      scBtn->label = "SIDECHAIN";
      scBtn->fontPath = fpCourier;
      addParam(scBtn);
    }

    // --- Meter display (Y = 23, height 14mm) ---
    {
      auto* meter = createWidget<OpcMeterDisplay>(mm2px(Vec(5.0f, 23.0f)));
      meter->box.size = mm2px(Vec(193.2f, 14.0f));
      meter->module = module;
      meter->lcdFontPath = fpLCD;
      addChild(meter);
    }

    // --- Large knobs: BLEND and OUTPUT ---
    // Knob is 16mm box. Label 4mm above, knob center, then gap.
    // Label Y = 39, knob center Y = 47
    const float blendX = 51.f;
    const float outGainX = 152.f;
    const float largeKnobY = 47.0f;

    makeLabel(mm2px(Vec(blendX - 10.f, 38.5f)), mm2px(Vec(20.0f, 4.0f)), "BLEND", 11.f);
    makeLabel(mm2px(Vec(outGainX - 10.f, 38.5f)), mm2px(Vec(20.0f, 4.0f)), "OUTPUT", 11.f);

    addParam(createParamCentered<OpcCustomKnob>(mm2px(Vec(blendX, largeKnobY)), module,
                                                static_cast<int>(OpcVcvIr::ParamId::BlendParam)));
    addParam(
        createParamCentered<OpcCustomKnob>(mm2px(Vec(outGainX, largeKnobY)), module,
                                           static_cast<int>(OpcVcvIr::ParamId::OutputGainParam)));

    // --- Knob row 1: THRESHOLD, RANGE, KNEE ---
    // Label Y = 57, knob center Y = 65
    const float row1LabelY = 57.0f;
    const float row1Y = 65.0f;
    const float knobX1 = 34.f;
    const float knobX2 = 102.f;
    const float knobX3 = 170.f;

    makeLabel(mm2px(Vec(knobX1 - 12.f, row1LabelY)), mm2px(Vec(24.0f, 4.0f)), "THRESHOLD", 10.f);
    makeLabel(mm2px(Vec(knobX2 - 10.f, row1LabelY)), mm2px(Vec(20.0f, 4.0f)), "RANGE", 10.f);
    makeLabel(mm2px(Vec(knobX3 - 10.f, row1LabelY)), mm2px(Vec(20.0f, 4.0f)), "KNEE", 10.f);

    addParam(createParamCentered<OpcCustomKnob>(
        mm2px(Vec(knobX1, row1Y)), module, static_cast<int>(OpcVcvIr::ParamId::ThresholdParam)));
    addParam(createParamCentered<OpcCustomKnob>(mm2px(Vec(knobX2, row1Y)), module,
                                                static_cast<int>(OpcVcvIr::ParamId::RangeDbParam)));
    addParam(createParamCentered<OpcCustomKnob>(
        mm2px(Vec(knobX3, row1Y)), module, static_cast<int>(OpcVcvIr::ParamId::KneeWidthDbParam)));

    // --- Knob row 2: ATTACK, RELEASE, DETECTION ---
    // Label Y = 75, knob center Y = 83
    const float row2LabelY = 75.0f;
    const float row2Y = 83.0f;

    makeLabel(mm2px(Vec(knobX1 - 10.f, row2LabelY)), mm2px(Vec(20.0f, 4.0f)), "ATTACK", 10.f);
    makeLabel(mm2px(Vec(knobX2 - 10.f, row2LabelY)), mm2px(Vec(20.0f, 4.0f)), "RELEASE", 10.f);
    makeLabel(mm2px(Vec(knobX3 - 12.f, row2LabelY)), mm2px(Vec(24.0f, 4.0f)), "DETECTION", 10.f);

    addParam(createParamCentered<OpcCustomKnob>(
        mm2px(Vec(knobX1, row2Y)), module, static_cast<int>(OpcVcvIr::ParamId::AttackTimeParam)));
    addParam(createParamCentered<OpcCustomKnob>(
        mm2px(Vec(knobX2, row2Y)), module, static_cast<int>(OpcVcvIr::ParamId::ReleaseTimeParam)));

    {
      auto* detectBtn =
          createParam<OpcDetectModeButton>(mm2px(Vec(knobX3 - 12.f, row2Y - 4.f)), module,
                                           static_cast<int>(OpcVcvIr::ParamId::DetectionModeParam));
      detectBtn->box.size = mm2px(Vec(24.0f, 8.0f));
      detectBtn->fontPath = fpCourier;
      addParam(detectBtn);
    }

    // --- CV input row (Y-center = 97.0) ---
    const float cvY = 97.0f;
    const float cvX1 = 25.f;
    const float cvX2 = 68.f;
    const float cvX3 = 111.f;
    const float cvX4 = 154.f;

    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cvX1, cvY)), module,
                                             static_cast<int>(OpcVcvIr::InputId::SidechainIn)));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cvX2, cvY)), module,
                                             static_cast<int>(OpcVcvIr::InputId::ThresholdCvIn)));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cvX3, cvY)), module,
                                             static_cast<int>(OpcVcvIr::InputId::BlendCvIn)));
    addInput(createInputCentered<PJ301MPort>(
        mm2px(Vec(cvX4, cvY)), module, static_cast<int>(OpcVcvIr::InputId::DynamicsEnableCvIn)));

    makeLabel(mm2px(Vec(cvX1 - 6.f, cvY + 5.f)), mm2px(Vec(12.0f, 4.0f)), "SC IN", 9.f);
    makeLabel(mm2px(Vec(cvX2 - 6.f, cvY + 5.f)), mm2px(Vec(12.0f, 4.0f)), "THR CV", 9.f);
    makeLabel(mm2px(Vec(cvX3 - 6.f, cvY + 5.f)), mm2px(Vec(12.0f, 4.0f)), "BLD CV", 9.f);
    makeLabel(mm2px(Vec(cvX4 - 6.f, cvY + 5.f)), mm2px(Vec(12.0f, 4.0f)), "DYN EN", 9.f);

    // --- Audio I/O row (Y-center = 113.0) ---
    const float ioY = 113.0f;
    const float inLX = 34.f;
    const float inRX = 68.f;
    const float outLX = 135.f;
    const float outRX = 170.f;

    {
      auto* outBg = createWidget<OpcOutputBackground>(mm2px(Vec(outLX - 14.f, ioY - 6.f)));
      outBg->box.size = mm2px(Vec(outRX - outLX + 28.f, 12.5f));
      addChild(outBg);
    }

    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(inLX, ioY)), module,
                                             static_cast<int>(OpcVcvIr::InputId::AudioInL)));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(inRX, ioY)), module,
                                             static_cast<int>(OpcVcvIr::InputId::AudioInR)));
    addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(outLX, ioY)), module,
                                               static_cast<int>(OpcVcvIr::OutputId::OutputL)));
    addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(outRX, ioY)), module,
                                               static_cast<int>(OpcVcvIr::OutputId::OutputR)));

    makeLabel(mm2px(Vec(inLX - 6.f, ioY + 5.f)), mm2px(Vec(12.0f, 4.0f)), "IN L", 9.f);
    makeLabel(mm2px(Vec(inRX - 6.f, ioY + 5.f)), mm2px(Vec(12.0f, 4.0f)), "IN R", 9.f);
    makeLabel(mm2px(Vec(outLX - 6.f, ioY + 5.f)), mm2px(Vec(12.0f, 4.0f)), "OUT L", 9.f);
    makeLabel(mm2px(Vec(outRX - 6.f, ioY + 5.f)), mm2px(Vec(12.0f, 4.0f)), "OUT R", 9.f);
  }

  void draw(const DrawArgs& args) override
  {
    ModuleWidget::draw(args);

    NVGcontext* vg = args.vg;
    float w = box.size.x;
    float h = box.size.y;

    // Panel bevel: layered dark inner shadow
    const float cr = 3.0f;
    float alphas[] = {0.28f, 0.14f, 0.06f};
    for (int i = 0; i < 3; ++i)
    {
      float inset = 0.5f + static_cast<float>(i) * 1.0f;
      nvgBeginPath(vg);
      nvgRoundedRect(vg, inset, inset, w - 2.f * inset, h - 2.f * inset, cr);
      nvgStrokeColor(vg, nvgRGBA(0, 0, 0, static_cast<int>(alphas[i] * 255.f)));
      nvgStrokeWidth(vg, 1.f);
      nvgStroke(vg);
    }

    // Top/left highlight
    nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 77));
    nvgStrokeWidth(vg, 1.f);
    nvgBeginPath(vg);
    nvgMoveTo(vg, cr + 1.f, 1.5f);
    nvgLineTo(vg, w - cr - 1.f, 1.5f);
    nvgStroke(vg);
    nvgBeginPath(vg);
    nvgMoveTo(vg, 1.5f, cr + 1.f);
    nvgLineTo(vg, 1.5f, h - cr - 1.f);
    nvgStroke(vg);
  }

  void appendContextMenu(Menu* menu) override
  {
    auto* module = dynamic_cast<OpcVcvIr*>(this->module);
    if (module == nullptr)
      return;

    menu->addChild(new MenuSeparator);

    menu->addChild(createCheckMenuItem(
        "Dynamic Mode", "",
        [module]()
        {
          return module->params[static_cast<int>(OpcVcvIr::ParamId::DynamicModeParam)].getValue() >
                 0.5f;
        },
        [module]()
        {
          auto& p = module->params[static_cast<int>(OpcVcvIr::ParamId::DynamicModeParam)];
          p.setValue(p.getValue() > 0.5f ? 0.f : 1.f);
        }));
    menu->addChild(createCheckMenuItem(
        "Sidechain Enable", "",
        [module]()
        {
          return module->params[static_cast<int>(OpcVcvIr::ParamId::SidechainEnableParam)]
                     .getValue() > 0.5f;
        },
        [module]()
        {
          auto& p = module->params[static_cast<int>(OpcVcvIr::ParamId::SidechainEnableParam)];
          p.setValue(p.getValue() > 0.5f ? 0.f : 1.f);
        }));

    menu->addChild(new MenuSeparator);

    menu->addChild(
        createMenuItem("Swap IR Order", "", [module]() { module->swapImpulseResponses(); }));
  }
};

Model* modelOpcVcvIr = createModel<OpcVcvIr, OpcVcvIrWidget>("opc-vcv-ir");
