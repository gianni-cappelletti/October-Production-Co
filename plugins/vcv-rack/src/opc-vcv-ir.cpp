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
    // Glow (radial, equal spread in all directions)
    float glowR = ledW * 2.5f;
    NVGpaint glow = nvgRadialGradient(vg, ledCx, ledCy, 0.f, glowR, nvgRGBA(0xe0, 0x70, 0x30, 72),
                                      nvgRGBA(0xe0, 0x70, 0x30, 0));
    nvgBeginPath(vg);
    nvgRect(vg, ledCx - glowR, ledCy - glowR, glowR * 2.f, glowR * 2.f);
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

static void openIRFileDialog(OpcVcvIr* module, bool isIR2)
{
  osdialog_filters* filters = osdialog_filters_parse("Audio files:wav,aiff,aif,flac;All files:*");
  if (filters == nullptr)
  {
    WARN("openIRFileDialog: failed to parse file dialog filters");
    return;
  }

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
    openIRFileDialog(module, isIR2);
    updateDisplayText();
  }

  void updateDisplayText()
  {
    if (module == nullptr)
    {
      text = "<No IR selected>";
      return;
    }

    std::string path = module->getLoadedFilePath(isIR2);
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

struct OpcToggleButton : app::Switch
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
  void openFileDialog() { openIRFileDialog(module, isIR2); }
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
    const bool hasIr = module != nullptr && !module->getLoadedFilePath(isIR2).empty();
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
    const std::string currentPath = module->getLoadedFilePath(isIR2);
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

    const float levelDb =
        module ? module->currentInputLevelDb_.load(std::memory_order_relaxed) : -60.f;
    const float blend = module ? module->currentBlend_.load(std::memory_order_relaxed) : 0.f;

    const float padH = 4.f;
    const float padTop = 8.f;
    const float padBot = 8.f;
    const float labelH = 8.f;
    const float gapLB = 2.f;
    const float barH = (h - padTop - padBot - labelH * 2.f - gapLB * 2.f - 4.f) * 0.5f;

    float cy = padTop;

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

    // Screws (5.08mm inset from each edge, flush top/bottom)
    const float screwSize = 5.08f;
    const float screwInset = 5.08f;
    const float panelW = 203.2f;
    const float panelH = 128.5f;
    addChild(createWidget<ScrewSilver>(mm2px(Vec(screwInset, 0.f))));
    addChild(createWidget<ScrewSilver>(mm2px(Vec(panelW - screwInset - screwSize, 0.f))));
    addChild(createWidget<ScrewSilver>(mm2px(Vec(screwInset, panelH - screwSize))));
    addChild(
        createWidget<ScrewSilver>(mm2px(Vec(panelW - screwInset - screwSize, panelH - screwSize))));

    // --- Helper lambdas ---

    auto makeLabel = [&](math::Vec pos, math::Vec size, const char* txt, float fs = 10.f,
                         NVGcolor col = nvgRGB(0x1a, 0x1a, 0x1a))
    {
      auto* lbl = createWidget<OpcPanelLabel>(pos);
      lbl->box.size = size;
      lbl->text = txt;
      lbl->fontPath = fpCourier;
      lbl->fontSize = fs;
      lbl->color = col;
      addChild(lbl);
    };

    auto makeLoadBtn = [&](math::Vec center, bool ir2)
    {
      auto* btn = createWidget<OpcIrLoadButton>(center - mm2px(Vec(5.0f, 3.0f)));
      btn->box.size = mm2px(Vec(10.0f, 6.0f));
      btn->module = module;
      btn->isIR2 = ir2;
      btn->fontPath = fpCourier;
      addChild(btn);
    };

    auto makeClearBtn = [&](math::Vec center, bool ir2)
    {
      auto* btn = createWidget<OpcIrClearButton>(center - mm2px(Vec(6.0f, 3.0f)));
      btn->box.size = mm2px(Vec(12.0f, 6.0f));
      btn->module = module;
      btn->isIR2 = ir2;
      btn->fontPath = fpCourier;
      addChild(btn);
    };

    auto makePrevBtn = [&](math::Vec center, bool ir2)
    {
      auto* btn = createWidget<OpcIrPrevButton>(center - mm2px(Vec(3.5f, 3.0f)));
      btn->box.size = mm2px(Vec(7.0f, 6.0f));
      btn->module = module;
      btn->isIR2 = ir2;
      btn->fontPath = fpCourier;
      addChild(btn);
    };

    auto makeNextBtn = [&](math::Vec center, bool ir2)
    {
      auto* btn = createWidget<OpcIrNextButton>(center - mm2px(Vec(3.5f, 3.0f)));
      btn->box.size = mm2px(Vec(7.0f, 6.0f));
      btn->module = module;
      btn->isIR2 = ir2;
      btn->fontPath = fpCourier;
      addChild(btn);
    };

    auto makeEnableBtn = [&](math::Vec center, bool ir2)
    {
      const auto paramId = static_cast<int>(ir2 ? OpcVcvIr::ParamId::IrBEnableParam
                                                : OpcVcvIr::ParamId::IrAEnableParam);
      auto* btn = createParam<OpcToggleButton>(center - mm2px(Vec(10.0f, 3.0f)), module, paramId);
      btn->box.size = mm2px(Vec(20.0f, 6.0f));
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

    // --- IR A controls row (Y-center = 10.0) ---
    makeLoadBtn(mm2px(Vec(colAStart + 7.f, 10.0f)), false);
    makeClearBtn(mm2px(Vec(colAStart + 20.f, 10.0f)), false);
    makePrevBtn(mm2px(Vec(colAStart + 31.f, 10.0f)), false);
    makeNextBtn(mm2px(Vec(colAStart + 40.f, 10.0f)), false);
    makeEnableBtn(mm2px(Vec(colAStart + 55.f, 10.0f)), false);
    addParam(createParamCentered<OpcCustomTrimKnob>(
        mm2px(Vec(colAStart + 70.f, 10.0f)), module,
        static_cast<int>(OpcVcvIr::ParamId::IrATrimGainParam)));

    // --- IR B controls row (Y-center = 10.0, right column) ---
    makeLoadBtn(mm2px(Vec(colBStart + 7.f, 10.0f)), true);
    makeClearBtn(mm2px(Vec(colBStart + 20.f, 10.0f)), true);
    makePrevBtn(mm2px(Vec(colBStart + 31.f, 10.0f)), true);
    makeNextBtn(mm2px(Vec(colBStart + 40.f, 10.0f)), true);
    makeEnableBtn(mm2px(Vec(colBStart + 55.f, 10.0f)), true);
    addParam(createParamCentered<OpcCustomTrimKnob>(
        mm2px(Vec(colBStart + 70.f, 10.0f)), module,
        static_cast<int>(OpcVcvIr::ParamId::IrBTrimGainParam)));

    // --- IR file display row (Y = 16.0, spans full width of each half) ---
    auto* fileDisplayA = createWidget<IrFileDisplay>(mm2px(Vec(colAStart, 16.0f)));
    fileDisplayA->box.size = mm2px(Vec(97.0f, 8.0f));
    fileDisplayA->module = module;
    fileDisplayA->isIR2 = false;
    fileDisplayA->lcdFontPath = fpLCD;
    addChild(fileDisplayA);

    auto* fileDisplayB = createWidget<IrFileDisplay>(mm2px(Vec(colBStart, 16.0f)));
    fileDisplayB->box.size = mm2px(Vec(96.2f, 8.0f));
    fileDisplayB->module = module;
    fileDisplayB->isIR2 = true;
    fileDisplayB->lcdFontPath = fpLCD;
    addChild(fileDisplayB);

    // --- Mode row (Y-center = 31.0) ---
    const float modeY = 31.0f;
    const float modeW = 56.0f;
    const float modeH = 7.0f;
    const float modeX1 = 34.f;
    const float modeX2 = 102.f;
    const float modeX3 = 170.f;

    {
      auto* dynBtn = createParam<OpcToggleButton>(
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
      auto* scBtn = createParam<OpcToggleButton>(
          mm2px(Vec(modeX3 - modeW * 0.5f, modeY - modeH * 0.5f)), module,
          static_cast<int>(OpcVcvIr::ParamId::SidechainEnableParam));
      scBtn->box.size = mm2px(Vec(modeW, modeH));
      scBtn->label = "SIDECHAIN";
      scBtn->fontPath = fpCourier;
      addParam(scBtn);
    }

    // --- Meter display (Y = 38, height 18mm) ---
    {
      auto* meter = createWidget<OpcMeterDisplay>(mm2px(Vec(5.0f, 38.0f)));
      meter->box.size = mm2px(Vec(193.2f, 20.0f));
      meter->module = module;
      meter->lcdFontPath = fpLCD;
      addChild(meter);
    }

    // --- Knob row 1: BLEND, THRESHOLD, RANGE, KNEE, OUTPUT (5 across) ---
    const float row1LabelY = 62.0f;
    const float row1Y = 75.0f;
    const float kr1X1 = 28.f;   // BLEND
    const float kr1X2 = 68.f;   // THRESHOLD
    const float kr1X3 = 102.f;  // RANGE
    const float kr1X4 = 136.f;  // KNEE
    const float kr1X5 = 176.f;  // OUTPUT

    makeLabel(mm2px(Vec(kr1X1 - 10.f, row1LabelY)), mm2px(Vec(20.0f, 4.0f)), "BLEND", 10.f);
    makeLabel(mm2px(Vec(kr1X2 - 12.f, row1LabelY)), mm2px(Vec(24.0f, 4.0f)), "THRESHOLD", 10.f);
    makeLabel(mm2px(Vec(kr1X3 - 10.f, row1LabelY)), mm2px(Vec(20.0f, 4.0f)), "RANGE", 10.f);
    makeLabel(mm2px(Vec(kr1X4 - 10.f, row1LabelY)), mm2px(Vec(20.0f, 4.0f)), "KNEE", 10.f);
    makeLabel(mm2px(Vec(kr1X5 - 10.f, row1LabelY)), mm2px(Vec(20.0f, 4.0f)), "OUTPUT", 10.f);

    addParam(createParamCentered<OpcCustomKnob>(mm2px(Vec(kr1X1, row1Y)), module,
                                                static_cast<int>(OpcVcvIr::ParamId::BlendParam)));
    addParam(createParamCentered<OpcCustomKnob>(
        mm2px(Vec(kr1X2, row1Y)), module, static_cast<int>(OpcVcvIr::ParamId::ThresholdParam)));
    addParam(createParamCentered<OpcCustomKnob>(mm2px(Vec(kr1X3, row1Y)), module,
                                                static_cast<int>(OpcVcvIr::ParamId::RangeDbParam)));
    addParam(createParamCentered<OpcCustomKnob>(
        mm2px(Vec(kr1X4, row1Y)), module, static_cast<int>(OpcVcvIr::ParamId::KneeWidthDbParam)));
    addParam(createParamCentered<OpcCustomKnob>(
        mm2px(Vec(kr1X5, row1Y)), module, static_cast<int>(OpcVcvIr::ParamId::OutputGainParam)));

    // --- Knob row 2: ATTACK, RELEASE, DETECTION (3 across) ---
    const float row2LabelY = 84.0f;
    const float row2Y = 97.0f;
    const float kr2X1 = 52.f;   // ATTACK
    const float kr2X2 = 102.f;  // RELEASE
    const float kr2X3 = 152.f;  // DETECTION

    makeLabel(mm2px(Vec(kr2X1 - 10.f, row2LabelY)), mm2px(Vec(20.0f, 4.0f)), "ATTACK", 10.f);
    makeLabel(mm2px(Vec(kr2X2 - 10.f, row2LabelY)), mm2px(Vec(20.0f, 4.0f)), "RELEASE", 10.f);
    makeLabel(mm2px(Vec(kr2X3 - 12.f, row2LabelY)), mm2px(Vec(24.0f, 4.0f)), "DETECTION", 10.f);

    addParam(createParamCentered<OpcCustomKnob>(
        mm2px(Vec(kr2X1, row2Y)), module, static_cast<int>(OpcVcvIr::ParamId::AttackTimeParam)));
    addParam(createParamCentered<OpcCustomKnob>(
        mm2px(Vec(kr2X2, row2Y)), module, static_cast<int>(OpcVcvIr::ParamId::ReleaseTimeParam)));

    {
      auto* detectBtn =
          createParam<OpcDetectModeButton>(mm2px(Vec(kr2X3 - 12.f, row2Y - 4.f)), module,
                                           static_cast<int>(OpcVcvIr::ParamId::DetectionModeParam));
      detectBtn->box.size = mm2px(Vec(24.0f, 8.0f));
      detectBtn->fontPath = fpCourier;
      addParam(detectBtn);
    }

    // --- Port row: IN L/R (left), CV (middle), OUT L/R (right) ---
    const float portY = 112.0f;
    const float portLabelY = portY + 5.f;
    const float pInL = 16.f;    // IN L/MONO
    const float pInR = 34.f;    // IN RIGHT
    const float pCV1 = 70.f;    // SC IN
    const float pCV2 = 94.f;    // THR CV
    const float pCV3 = 118.f;   // BLD CV
    const float pCV4 = 142.f;   // DYN EN
    const float pOutL = 172.f;  // OUT L/MONO
    const float pOutR = 190.f;  // OUT RIGHT

    {
      auto* outBg = createWidget<OpcOutputBackground>(mm2px(Vec(pOutL - 10.f, portY - 6.f)));
      outBg->box.size = mm2px(Vec(pOutR - pOutL + 20.f, 16.0f));
      addChild(outBg);
    }

    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(pInL, portY)), module,
                                             static_cast<int>(OpcVcvIr::InputId::AudioInL)));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(pInR, portY)), module,
                                             static_cast<int>(OpcVcvIr::InputId::AudioInR)));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(pCV1, portY)), module,
                                             static_cast<int>(OpcVcvIr::InputId::SidechainIn)));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(pCV2, portY)), module,
                                             static_cast<int>(OpcVcvIr::InputId::ThresholdCvIn)));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(pCV3, portY)), module,
                                             static_cast<int>(OpcVcvIr::InputId::BlendCvIn)));
    addInput(createInputCentered<PJ301MPort>(
        mm2px(Vec(pCV4, portY)), module, static_cast<int>(OpcVcvIr::InputId::DynamicsEnableCvIn)));
    addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(pOutL, portY)), module,
                                               static_cast<int>(OpcVcvIr::OutputId::OutputL)));
    addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(pOutR, portY)), module,
                                               static_cast<int>(OpcVcvIr::OutputId::OutputR)));

    makeLabel(mm2px(Vec(pInL - 7.f, portLabelY)), mm2px(Vec(14.0f, 4.0f)), "L/MONO", 10.f);
    makeLabel(mm2px(Vec(pInR - 7.f, portLabelY)), mm2px(Vec(14.0f, 4.0f)), "RIGHT", 10.f);
    makeLabel(mm2px(Vec(pCV1 - 10.f, portLabelY)), mm2px(Vec(20.0f, 4.0f)), "SIDECHAIN", 10.f);
    makeLabel(mm2px(Vec(pCV2 - 10.f, portLabelY)), mm2px(Vec(20.0f, 4.0f)), "THRESHOLD", 10.f);
    makeLabel(mm2px(Vec(pCV3 - 10.f, portLabelY)), mm2px(Vec(20.0f, 4.0f)), "BLEND", 10.f);
    makeLabel(mm2px(Vec(pCV4 - 10.f, portLabelY)), mm2px(Vec(20.0f, 4.0f)), "DYNAMIC", 10.f);
    const NVGcolor lightLabelCol = nvgRGB(0xd8, 0xd4, 0xd0);
    makeLabel(mm2px(Vec(pOutL - 7.f, portLabelY)), mm2px(Vec(14.0f, 4.0f)), "L/MONO", 10.f,
              lightLabelCol);
    makeLabel(mm2px(Vec(pOutR - 7.f, portLabelY)), mm2px(Vec(14.0f, 4.0f)), "RIGHT", 10.f,
              lightLabelCol);
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
